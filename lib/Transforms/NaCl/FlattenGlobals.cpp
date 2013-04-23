//===- FlattenGlobals.cpp - Flatten global variable initializers-----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass converts initializers for global variables into a
// flattened normal form which removes nested struct types and
// simplifies ConstantExprs.
//
// In this normal form, an initializer is either a SimpleElement or a
// CompoundElement.
//
// A SimpleElement is one of the following:
//
// 1) An i8 array literal or zeroinitializer:
//
//      [SIZE x i8] c"DATA"
//      [SIZE x i8] zeroinitializer
//
// 2) A reference to a GlobalValue (a function or global variable)
//    with an optional 32-bit byte offset added to it (the addend):
//
//      i8* bitcast (TYPE* @GLOBAL to i8*)
//      getelementptr (i8* bitcast (TYPE* @GLOBAL to i8*), i32 0, i32 ADDEND)
//
//    Note that if @GLOBAL is an i8 array, LLVM will write the latter as:
//
//      getelementptr ([SIZE x i8]* @GLOBAL, i32 0, i32 ADDEND)
//
//    For completeness, the pass also allows a BlockAddress as well as
//    a GlobalValue here, although BlockAddresses are currently not
//    allowed in the PNaCl ABI, so this should not be considered part
//    of the normal form.
//
// A CompoundElement is a unnamed, packed struct containing only
// SimpleElements.
//
// Limitations:
//
// LLVM IR allows ConstantExprs that calculate the difference between
// two globals' addresses.  FlattenGlobals rejects these because Clang
// does not generate these and because ELF does not support such
// relocations in general.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  // A FlattenedConstant represents a global variable initializer that
  // has been flattened and may be converted into the normal form.
  class FlattenedConstant {
    LLVMContext *Context;
    Type *I8Ptr;
    unsigned PtrSize;

    // A flattened global variable initializer is represented as:
    // 1) an array of bytes;
    unsigned BufSize;
    uint8_t *Buf;
    uint8_t *BufEnd;

    // 2) an array of relocations.
    struct Reloc {
      unsigned RelOffset;  // Offset at which the relocation is to be applied.
      Constant *GlobalRef;
    };
    typedef SmallVector<Reloc, 10> RelocArray;
    RelocArray Relocs;

    void putAtDest(DataLayout *DL, Constant *Value, uint8_t *Dest);

    Constant *dataSlice(unsigned StartPos, unsigned EndPos) {
      return ConstantDataArray::get(
          *Context, ArrayRef<uint8_t>(Buf + StartPos, Buf + EndPos));
    }

  public:
    FlattenedConstant(DataLayout *DL, Constant *Value):
        Context(&Value->getContext()) {
      I8Ptr = Type::getInt8Ty(*Context)->getPointerTo();
      PtrSize = DL->getPointerSize();
      BufSize = DL->getTypeAllocSize(Value->getType());
      Buf = new uint8_t[BufSize];
      BufEnd = Buf + BufSize;
      memset(Buf, 0, BufSize);
      putAtDest(DL, Value, Buf);
    }

    ~FlattenedConstant() {
      delete Buf;
    }

    Constant *getAsNormalFormConstant();
  };

  class FlattenGlobals : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    FlattenGlobals() : ModulePass(ID) {
      initializeFlattenGlobalsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

static void ExpandConstant(DataLayout *DL, Constant *Val,
                           Constant **ResultGlobal, uint64_t *ResultOffset) {
  if (isa<GlobalValue>(Val) || isa<BlockAddress>(Val)) {
    *ResultGlobal = Val;
    *ResultOffset = 0;
  } else if (isa<ConstantPointerNull>(Val)) {
    *ResultGlobal = NULL;
    *ResultOffset = 0;
  } else if (ConstantInt *CI = dyn_cast<ConstantInt>(Val)) {
    *ResultGlobal = NULL;
    *ResultOffset = CI->getZExtValue();
  } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(Val)) {
    ExpandConstant(DL, CE->getOperand(0), ResultGlobal, ResultOffset);
    if (CE->getOpcode() == Instruction::GetElementPtr) {
      SmallVector<Value *, 8> Indexes(CE->op_begin() + 1, CE->op_end());
      *ResultOffset += DL->getIndexedOffset(CE->getOperand(0)->getType(),
                                            Indexes);
    } else if (CE->getOpcode() == Instruction::BitCast ||
               CE->getOpcode() == Instruction::IntToPtr) {
      // Nothing more to do.
    } else if (CE->getOpcode() == Instruction::PtrToInt) {
      if (Val->getType()->getIntegerBitWidth() < DL->getPointerSizeInBits()) {
        errs() << "Not handled: " << *CE << "\n";
        report_fatal_error("FlattenGlobals: a ptrtoint that truncates "
                           "a pointer is not allowed");
      }
    } else {
      errs() << "Not handled: " << *CE << "\n";
      report_fatal_error(
          std::string("FlattenGlobals: ConstantExpr opcode not handled: ")
          + CE->getOpcodeName());
    }
  } else {
    errs() << "Not handled: " << *Val << "\n";
    report_fatal_error("FlattenGlobals: Constant type not handled for reloc");
  }
}

void FlattenedConstant::putAtDest(DataLayout *DL, Constant *Val,
                                  uint8_t *Dest) {
  uint64_t ValSize = DL->getTypeAllocSize(Val->getType());
  assert(Dest + ValSize <= BufEnd);
  if (isa<ConstantAggregateZero>(Val) ||
      isa<UndefValue>(Val) ||
      isa<ConstantPointerNull>(Val)) {
    // The buffer is already zero-initialized.
  } else if (ConstantInt *CI = dyn_cast<ConstantInt>(Val)) {
    memcpy(Dest, CI->getValue().getRawData(), ValSize);
  } else if (ConstantFP *CF = dyn_cast<ConstantFP>(Val)) {
    APInt Data = CF->getValueAPF().bitcastToAPInt();
    assert((Data.getBitWidth() + 7) / 8 == ValSize);
    assert(Data.getBitWidth() % 8 == 0);
    memcpy(Dest, Data.getRawData(), ValSize);
  } else if (ConstantDataSequential *CD =
             dyn_cast<ConstantDataSequential>(Val)) {
    // Note that getRawDataValues() assumes the host endianness is the same.
    StringRef Data = CD->getRawDataValues();
    assert(Data.size() == ValSize);
    memcpy(Dest, Data.data(), Data.size());
  } else if (isa<ConstantArray>(Val) || isa<ConstantVector>(Val)) {
    uint64_t ElementSize = DL->getTypeAllocSize(
        Val->getType()->getSequentialElementType());
    for (unsigned I = 0; I < Val->getNumOperands(); ++I) {
      putAtDest(DL, cast<Constant>(Val->getOperand(I)), Dest + ElementSize * I);
    }
  } else if (ConstantStruct *CS = dyn_cast<ConstantStruct>(Val)) {
    const StructLayout *Layout = DL->getStructLayout(CS->getType());
    for (unsigned I = 0; I < CS->getNumOperands(); ++I) {
      putAtDest(DL, CS->getOperand(I), Dest + Layout->getElementOffset(I));
    }
  } else {
    Constant *GV;
    uint64_t Offset;
    ExpandConstant(DL, Val, &GV, &Offset);
    if (GV) {
      Constant *NewVal = ConstantExpr::getBitCast(GV, I8Ptr);
      if (Offset) {
        // For simplicity, require addends to be 32-bit.
        if ((int64_t) Offset != (int32_t) (uint32_t) Offset) {
          errs() << "Not handled: " << *Val << "\n";
          report_fatal_error(
              "FlattenGlobals: Offset does not fit into 32 bits");
        }
        NewVal = ConstantExpr::getGetElementPtr(
            NewVal, ConstantInt::get(*Context, APInt(32, Offset)));
      }
      Reloc NewRel = { Dest - Buf, NewVal };
      Relocs.push_back(NewRel);
    } else {
      memcpy(Dest, &Offset, ValSize);
    }
  }
}

Constant *FlattenedConstant::getAsNormalFormConstant() {
  // Return a single SimpleElement.
  if (Relocs.size() == 0)
    return dataSlice(0, BufSize);
  if (Relocs.size() == 1 && BufSize == PtrSize) {
    assert(Relocs[0].RelOffset == 0);
    return Relocs[0].GlobalRef;
  }

  // Return a CompoundElement.
  SmallVector<Constant *, 10> Elements;
  unsigned PrevPos = 0;
  for (RelocArray::iterator Rel = Relocs.begin(), E = Relocs.end();
       Rel != E; ++Rel) {
    if (Rel->RelOffset > PrevPos)
      Elements.push_back(dataSlice(PrevPos, Rel->RelOffset));
    Elements.push_back(Rel->GlobalRef);
    PrevPos = Rel->RelOffset + PtrSize;
  }
  if (PrevPos < BufSize)
    Elements.push_back(dataSlice(PrevPos, BufSize));
  return ConstantStruct::getAnon(*Context, Elements, true);
}

char FlattenGlobals::ID = 0;
INITIALIZE_PASS(FlattenGlobals, "flatten-globals",
                "Flatten global variable initializers into byte arrays",
                false, false)

bool FlattenGlobals::runOnModule(Module &M) {
  bool Modified = false;
  DataLayout DL(&M);
  Type *I8 = Type::getInt8Ty(M.getContext());

  for (Module::global_iterator I = M.global_begin(), E = M.global_end();
       I != E; ) {
    GlobalVariable *Global = I++;
    // Variables with "appending" linkage must always be arrays and so
    // cannot be normalized, so leave them alone.
    if (Global->hasAppendingLinkage())
      continue;
    Modified = true;

    uint64_t Size = DL.getTypeAllocSize(
        Global->getType()->getPointerElementType());
    Constant *NewInit;
    Type *NewType;
    if (Global->hasInitializer()) {
      if (Global->getInitializer()->isNullValue()) {
        // As optimization, for large BSS variables, avoid allocating
        // a buffer that would only be filled with zeroes.
        NewType = ArrayType::get(I8, Size);
        NewInit = ConstantAggregateZero::get(NewType);
      } else {
        FlattenedConstant Buffer(&DL, Global->getInitializer());
        NewInit = Buffer.getAsNormalFormConstant();
        NewType = NewInit->getType();
      }
    } else {
      NewInit = NULL;
      NewType = ArrayType::get(I8, Size);
    }
    GlobalVariable *NewGlobal = new GlobalVariable(
        M, NewType,
        Global->isConstant(),
        Global->getLinkage(),
        NewInit, "", Global,
        Global->getThreadLocalMode());
    NewGlobal->copyAttributesFrom(Global);
    NewGlobal->setExternallyInitialized(Global->isExternallyInitialized());
    NewGlobal->takeName(Global);
    Global->replaceAllUsesWith(
        ConstantExpr::getBitCast(NewGlobal, Global->getType()));
    Global->eraseFromParent();
  }
  return Modified;

}

ModulePass *llvm::createFlattenGlobalsPass() {
  return new FlattenGlobals();
}
