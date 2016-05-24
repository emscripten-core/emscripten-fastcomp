//===- FixVectorLoadStoreAlignment.cpp - Vector load/store alignment ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Fix vector load/store alignment by:
// - Leaving as-is if the alignment is equal to the vector's element width.
// - Reducing the alignment to vector's element width if it's greater and the
//   current alignment is a factor of the element alignment.
// - Scalarizing if the alignment is smaller than the element-wise alignment.
//
// Volatile vector load/store are handled the same, and can therefore be broken
// up as allowed by C/C++.
//
// TODO(jfb) Atomic accesses cause errors at compile-time. This could be
//           implemented as a call to the C++ runtime, since 128-bit atomics
//           aren't usually lock-free.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
class FixVectorLoadStoreAlignment : public BasicBlockPass {
public:
  static char ID; // Pass identification, replacement for typeid
  FixVectorLoadStoreAlignment() : BasicBlockPass(ID), M(0), DL(0) {
    initializeFixVectorLoadStoreAlignmentPass(*PassRegistry::getPassRegistry());
  }
  using BasicBlockPass::doInitialization;
  bool doInitialization(Module &Mod) override {
    M = &Mod;
    return false; // Unchanged.
  }
  bool runOnBasicBlock(BasicBlock &BB) override;

private:
  typedef SmallVector<Instruction *, 8> Instructions;
  const Module *M;
  const DataLayout *DL;

  /// Some sub-classes of Instruction have a non-virtual function
  /// indicating which operand is the pointer operand. This template
  /// function returns the pointer operand's type, and requires that
  /// InstTy have a getPointerOperand function.
  template <typename InstTy>
  static PointerType *pointerOperandType(const InstTy *I) {
    return cast<PointerType>(I->getPointerOperand()->getType());
  }

  /// Similar to pointerOperandType, this template function checks
  /// whether the pointer operand is a pointer to a vector type.
  template <typename InstTy>
  static bool pointerOperandIsVectorPointer(const Instruction *I) {
    return pointerOperandType(cast<InstTy>(I))->getElementType()->isVectorTy();
  }

  /// Returns true if one of the Instruction's operands is a pointer to
  /// a vector type. This is more general than the above and assumes we
  /// don't know which Instruction type is provided.
  static bool hasVectorPointerOperand(const Instruction *I) {
    for (User::const_op_iterator IB = I->op_begin(), IE = I->op_end(); IB != IE;
         ++IB)
      if (PointerType *PtrTy = dyn_cast<PointerType>((*IB)->getType()))
        if (isa<VectorType>(PtrTy->getElementType()))
          return true;
    return false;
  }

  /// Vectors are expected to be element-aligned. If they are, leave as-is; if
  /// the alignment is too much then narrow the alignment (when possible);
  /// otherwise return false.
  template <typename InstTy>
  static bool tryFixVectorAlignment(const DataLayout *DL, Instruction *I) {
    InstTy *LoadStore = cast<InstTy>(I);
    VectorType *VecTy =
        cast<VectorType>(pointerOperandType(LoadStore)->getElementType());
    Type *ElemTy = VecTy->getElementType();
    uint64_t ElemBitSize = DL->getTypeSizeInBits(ElemTy);
    uint64_t ElemByteSize = ElemBitSize / CHAR_BIT;
    uint64_t CurrentByteAlign = LoadStore->getAlignment();
    bool isABIAligned = CurrentByteAlign == 0;
    uint64_t VecABIByteAlign = DL->getABITypeAlignment(VecTy);
    CurrentByteAlign = isABIAligned ? VecABIByteAlign : CurrentByteAlign;

    if (CHAR_BIT * ElemByteSize != ElemBitSize)
      return false; // Minimum byte-size elements.
    if (MinAlign(ElemByteSize, CurrentByteAlign) == ElemByteSize) {
      // Element-aligned, or compatible over-aligned. Keep element-aligned.
      LoadStore->setAlignment(ElemByteSize);
      return true;
    }
    return false; // Under-aligned.
  }

  void visitVectorLoadStore(BasicBlock &BB, Instructions &Loads,
                            Instructions &Stores) const;
  void scalarizeVectorLoadStore(BasicBlock &BB, const Instructions &Loads,
                                const Instructions &Stores) const;
};
} // anonymous namespace

char FixVectorLoadStoreAlignment::ID = 0;
INITIALIZE_PASS(FixVectorLoadStoreAlignment, "fix-vector-load-store-alignment",
                "Ensure vector load/store have element-size alignment",
                false, false)

void FixVectorLoadStoreAlignment::visitVectorLoadStore(
    BasicBlock &BB, Instructions &Loads, Instructions &Stores) const {
  for (BasicBlock::iterator BBI = BB.begin(), BBE = BB.end(); BBI != BBE;
       ++BBI) {
    Instruction *I = &*BBI;
    // The following list of instructions is based on mayReadOrWriteMemory.
    switch (I->getOpcode()) {
    case Instruction::Load:
      if (pointerOperandIsVectorPointer<LoadInst>(I)) {
        if (cast<LoadInst>(I)->isAtomic())
          report_fatal_error("unhandled: atomic vector store");
        if (!tryFixVectorAlignment<LoadInst>(DL, I))
          Loads.push_back(I);
      }
      break;
    case Instruction::Store:
      if (pointerOperandIsVectorPointer<StoreInst>(I)) {
        if (cast<StoreInst>(I)->isAtomic())
          report_fatal_error("unhandled: atomic vector store");
        if (!tryFixVectorAlignment<StoreInst>(DL, I))
          Stores.push_back(I);
      }
      break;
    case Instruction::Alloca:
    case Instruction::Fence:
    case Instruction::VAArg:
      // Leave these memory operations as-is, even when they deal with
      // vectors.
      break;
    case Instruction::Call:
    case Instruction::Invoke:
      // Call/invoke don't touch memory per-se, leave them as-is.
      break;
    case Instruction::AtomicCmpXchg:
      if (pointerOperandIsVectorPointer<AtomicCmpXchgInst>(I))
        report_fatal_error(
            "unhandled: atomic compare and exchange operation on vector");
      break;
    case Instruction::AtomicRMW:
      if (pointerOperandIsVectorPointer<AtomicRMWInst>(I))
        report_fatal_error("unhandled: atomic RMW operation on vector");
      break;
    default:
      if (I->mayReadOrWriteMemory() && hasVectorPointerOperand(I)) {
        errs() << "Not handled: " << *I << '\n';
        report_fatal_error(
            "unexpected: vector operations which may read/write memory");
      }
      break;
    }
  }
}

void FixVectorLoadStoreAlignment::scalarizeVectorLoadStore(
    BasicBlock &BB, const Instructions &Loads,
    const Instructions &Stores) const {
  for (Instructions::const_iterator IB = Loads.begin(), IE = Loads.end();
       IB != IE; ++IB) {
    LoadInst *VecLoad = cast<LoadInst>(*IB);
    VectorType *LoadedVecTy =
        cast<VectorType>(pointerOperandType(VecLoad)->getElementType());
    Type *ElemTy = LoadedVecTy->getElementType();

    // The base of the vector is as aligned as the vector load (where
    // zero means ABI alignment for the vector), whereas subsequent
    // elements are as aligned as the base+offset can be.
    unsigned BaseAlign = VecLoad->getAlignment()
                             ? VecLoad->getAlignment()
                             : DL->getABITypeAlignment(LoadedVecTy);
    unsigned ElemAllocSize = DL->getTypeAllocSize(ElemTy);

    // Fill in the vector element by element.
    IRBuilder<> IRB(VecLoad);
    Value *Loaded = UndefValue::get(LoadedVecTy);
    Value *Base =
        IRB.CreateBitCast(VecLoad->getPointerOperand(), ElemTy->getPointerTo());

    for (unsigned Elem = 0, NumElems = LoadedVecTy->getNumElements();
         Elem != NumElems; ++Elem) {
      unsigned Align = MinAlign(BaseAlign, ElemAllocSize * Elem);
      Value *GEP = IRB.CreateConstInBoundsGEP1_32(ElemTy, Base, Elem);
      LoadInst *LoadedElem =
          IRB.CreateAlignedLoad(GEP, Align, VecLoad->isVolatile());
      LoadedElem->setSynchScope(VecLoad->getSynchScope());
      Loaded = IRB.CreateInsertElement(
          Loaded, LoadedElem,
          ConstantInt::get(Type::getInt32Ty(M->getContext()), Elem));
    }

    VecLoad->replaceAllUsesWith(Loaded);
    VecLoad->eraseFromParent();
  }

  for (Instructions::const_iterator IB = Stores.begin(), IE = Stores.end();
       IB != IE; ++IB) {
    StoreInst *VecStore = cast<StoreInst>(*IB);
    Value *StoredVec = VecStore->getValueOperand();
    VectorType *StoredVecTy = cast<VectorType>(StoredVec->getType());
    Type *ElemTy = StoredVecTy->getElementType();

    unsigned BaseAlign = VecStore->getAlignment()
                             ? VecStore->getAlignment()
                             : DL->getABITypeAlignment(StoredVecTy);
    unsigned ElemAllocSize = DL->getTypeAllocSize(ElemTy);

    // Fill in the vector element by element.
    IRBuilder<> IRB(VecStore);
    Value *Base = IRB.CreateBitCast(VecStore->getPointerOperand(),
                                    ElemTy->getPointerTo());

    for (unsigned Elem = 0, NumElems = StoredVecTy->getNumElements();
         Elem != NumElems; ++Elem) {
      unsigned Align = MinAlign(BaseAlign, ElemAllocSize * Elem);
      Value *GEP = IRB.CreateConstInBoundsGEP1_32(ElemTy, Base, Elem);
      Value *ElemToStore = IRB.CreateExtractElement(
          StoredVec, ConstantInt::get(Type::getInt32Ty(M->getContext()), Elem));
      StoreInst *StoredElem = IRB.CreateAlignedStore(ElemToStore, GEP, Align,
                                                     VecStore->isVolatile());
      StoredElem->setSynchScope(VecStore->getSynchScope());
    }

    VecStore->eraseFromParent();
  }
}

bool FixVectorLoadStoreAlignment::runOnBasicBlock(BasicBlock &BB) {
  bool Changed = false;
  if (!DL)
    DL = &BB.getParent()->getParent()->getDataLayout();
  Instructions Loads;
  Instructions Stores;
  visitVectorLoadStore(BB, Loads, Stores);
  if (!(Loads.empty() && Stores.empty())) {
    Changed = true;
    scalarizeVectorLoadStore(BB, Loads, Stores);
  }
  return Changed;
}

BasicBlockPass *llvm::createFixVectorLoadStoreAlignmentPass() {
  return new FixVectorLoadStoreAlignment();
}
