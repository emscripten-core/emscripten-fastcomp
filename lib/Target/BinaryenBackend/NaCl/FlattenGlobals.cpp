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
//      ptrtoint (TYPE* @GLOBAL to i32)
//      add (i32 ptrtoint (TYPE* @GLOBAL to i32), i32 ADDEND)
//
//    We use ptrtoint+add rather than bitcast+getelementptr because
//    the constructor for getelementptr ConstantExprs performs
//    constant folding which introduces more complex getelementptrs,
//    and it is hard to check that they follow a normal form.
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

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {

  // Defines a (non-constant) handle that records a use of a
  // constant. Used to make sure a relocation, within flattened global
  // variable initializers, does not get destroyed when method
  // removeDeadConstantUsers gets called. For simplicity, rather than
  // defining a new (non-constant) construct, we use a return
  // instruction as the handle.
  typedef ReturnInst RelocUserType;

  // Define map from a relocation, appearing in the flattened global variable
  // initializers, to it's corresponding use handle.
  typedef DenseMap<Constant*, RelocUserType*> RelocMapType;

  // Define the list to hold the list of global variables being flattened.
  struct FlattenedGlobal;
  typedef std::vector<FlattenedGlobal*> FlattenedGlobalsVectorType;

  // Returns the corresponding relocation, for the given user handle.
  Constant *getRelocUseConstant(RelocUserType *RelocUser) {
    return cast<Constant>(RelocUser->getReturnValue());
  }

  // The state associated with flattening globals of a module.
  struct FlattenGlobalsState {
    /// The module being flattened.
    Module &M;
    /// The data layout to be used.
    DataLayout DL;
    /// The relocations (within the original global variable initializers)
    /// that must be kept.
    RelocMapType RelocMap;
    /// The list of global variables that are being flattened.
    FlattenedGlobalsVectorType FlattenedGlobalsVector;
    /// True if the module was modified during the "flatten globals" pass.
    bool Modified;
    /// The type model of a byte.
    Type *ByteType;
    /// The type model of the integer pointer type.
    Type *IntPtrType;
    /// The size of the pointer type.
    unsigned PtrSize;

    explicit FlattenGlobalsState(Module &M)
        : M(M), DL(&M), RelocMap(),
          Modified(false),
          ByteType(Type::getInt8Ty(M.getContext())),
          IntPtrType(DL.getIntPtrType(M.getContext())),
          PtrSize(DL.getPointerSize())
    {}

    ~FlattenGlobalsState() {
      // Remove added user handles.
      for (RelocMapType::iterator
               I = RelocMap.begin(), E = RelocMap.end(); I != E; ++I) {
        delete I->second;
      }
      // Remove flatteners for global varaibles.
      DeleteContainerPointers(FlattenedGlobalsVector);
    }

    /// Collect Global variables whose initializers should be
    /// flattened.  Creates corresponding flattened initializers (if
    /// applicable), and creates uninitialized replacement global
    /// variables.
    void flattenGlobalsWithInitializers();

    /// Remove initializers from original global variables, and
    /// then remove the portions of the initializers that are
    /// no longer used.
    void removeDeadInitializerConstants();

    // Replace the original global variables with their flattened
    // global variable counterparts.
    void replaceGlobalsWithFlattenedGlobals();

    // Builds and installs initializers for flattened global
    // variables, based on the flattened initializers of the
    // corresponding original global variables.
    void installFlattenedGlobalInitializers();

    // Returns the user handle associated with the reloc, so that it
    // won't be deleted during the flattening process.
    RelocUserType *getRelocUserHandle(Constant *Reloc) {
      RelocUserType *RelocUser = RelocMap[Reloc];
      if (RelocUser == NULL) {
        RelocUser = ReturnInst::Create(M.getContext(), Reloc);
        RelocMap[Reloc] = RelocUser;
      }
      return RelocUser;
    }
  };

  // A FlattenedConstant represents a global variable initializer that
  // has been flattened and may be converted into the normal form.
  class FlattenedConstant {
    FlattenGlobalsState &State;

    // A flattened global variable initializer is represented as:
    // 1) an array of bytes;
    unsigned BufSize;
    uint8_t *Buf;
    // XXX EMSCRIPTEN: There used to be a BufEnd here. No more.

    // 2) an array of relocations.
    class Reloc {
    private:
      unsigned RelOffset;  // Offset at which the relocation is to be applied.
      RelocUserType *RelocUser;
   public:

      unsigned getRelOffset() const { return RelOffset; }
      Constant *getRelocUse() const { return getRelocUseConstant(RelocUser); }
      Reloc(FlattenGlobalsState &State, unsigned RelOffset, Constant *NewVal)
          : RelOffset(RelOffset), RelocUser(State.getRelocUserHandle(NewVal)) {}

      explicit Reloc(const Reloc &R)
          : RelOffset(R.RelOffset), RelocUser(R.RelocUser) {}

      void operator=(const Reloc &R) {
        RelOffset = R.RelOffset;
        RelocUser = R.RelocUser;
      }
    };
    typedef SmallVector<Reloc, 10> RelocArray;
    RelocArray Relocs;

    const DataLayout &getDataLayout() const { return State.DL; }

    Module &getModule() const { return State.M; }

    Type *getIntPtrType() const { return State.IntPtrType; }

    Type *getByteType() const { return State.ByteType; }

    unsigned getPtrSize() const { return State.PtrSize; }

    void putAtDest(Constant *Value, uint8_t *Dest);

    Constant *dataSlice(unsigned StartPos, unsigned EndPos) const {
      return ConstantDataArray::get(
          getModule().getContext(),
          ArrayRef<uint8_t>(Buf + StartPos, Buf + EndPos));
    }

    Type *dataSliceType(unsigned StartPos, unsigned EndPos) const {
      return ArrayType::get(getByteType(), EndPos - StartPos);
    }

  public:
    FlattenedConstant(FlattenGlobalsState &State, Constant *Value):
        State(State),
        BufSize(getDataLayout().getTypeAllocSize(Value->getType())),
        Buf(new uint8_t[BufSize]) {
      memset(Buf, 0, BufSize);
      putAtDest(Value, Buf);
    }

    ~FlattenedConstant() {
      delete[] Buf;
    }

    // Returns the corresponding flattened initializer.
    Constant *getAsNormalFormConstant() const;

    // Returns the type of the corresponding flattened initializer;
    Type *getAsNormalFormType() const;

  };

  // Structure used to flatten a global variable.
  struct FlattenedGlobal {
    // The state of the flatten globals pass.
    FlattenGlobalsState &State;
    // The global variable to flatten.
    GlobalVariable *Global;
    // The replacement global variable, if known.
    GlobalVariable *NewGlobal;
    // True if Global has an initializer.
    bool HasInitializer;
    // The flattened initializer, if the initializer would not just be
    // filled with zeroes.
    FlattenedConstant *FlatConst;
    // The type of GlobalType, when used in an initializer.
    Type *GlobalType;
    // The size of the initializer.
    uint64_t Size;
  public:
    FlattenedGlobal(FlattenGlobalsState &State, GlobalVariable *Global)
        : State(State),
          Global(Global),
          NewGlobal(NULL),
          HasInitializer(Global->hasInitializer()),
          FlatConst(NULL),
          GlobalType(Global->getType()->getPointerElementType()),
          Size(GlobalType->isSized()
               ? getDataLayout().getTypeAllocSize(GlobalType) : 0) {
      Type *NewType = NULL;
      if (HasInitializer) {
        if (Global->getInitializer()->isNullValue()) {
          // Special case of NullValue. As an optimization, for large
          // BSS variables, avoid allocating a buffer that would only be filled
          // with zeros.
          NewType = ArrayType::get(getByteType(), Size);
        } else {
          FlatConst = new FlattenedConstant(State, Global->getInitializer());
          NewType = FlatConst->getAsNormalFormType();
        }
      } else {
        NewType = ArrayType::get(getByteType(), Size);
      }
      NewGlobal = new GlobalVariable(getModule(), NewType,
                                     Global->isConstant(),
                                     Global->getLinkage(),
                                     NULL, "", Global,
                                     Global->getThreadLocalMode());
      NewGlobal->copyAttributesFrom(Global);
      if (NewGlobal->getAlignment() == 0 && GlobalType->isSized())
        NewGlobal->setAlignment(getDataLayout().
                                getPrefTypeAlignment(GlobalType));
      NewGlobal->setExternallyInitialized(Global->isExternallyInitialized());
      NewGlobal->takeName(Global);
    }

    ~FlattenedGlobal() {
      delete FlatConst;
    }

    const DataLayout &getDataLayout() const { return State.DL; }

    Module &getModule() const { return State.M; }

    Type *getByteType() const { return State.ByteType; }

    // Removes the original initializer from the global variable to be
    // flattened, if applicable.
    void removeOriginalInitializer() {
      if (HasInitializer) Global->setInitializer(NULL);
    }

    // Replaces the original global variable with the corresponding
    // flattened global variable.
    void replaceGlobalWithFlattenedGlobal() {
      Global->replaceAllUsesWith(
          ConstantExpr::getBitCast(NewGlobal, Global->getType()));
      Global->eraseFromParent();
    }

    // Installs flattened initializers to the corresponding flattened
    // global variable.
    void installFlattenedInitializer() {
      if (HasInitializer) {
        Constant *NewInit = NULL;
        if (FlatConst == NULL) {
          // Special case of NullValue.
          NewInit = ConstantAggregateZero::get(ArrayType::get(getByteType(),
                                                              Size));
        } else {
          NewInit = FlatConst->getAsNormalFormConstant();
        }
        NewGlobal->setInitializer(NewInit);
      }
    }
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

static void ExpandConstant(const DataLayout *DL, Constant *Val,
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

void FlattenedConstant::putAtDest(Constant *Val, uint8_t *Dest) {
  uint64_t ValSize = getDataLayout().getTypeAllocSize(Val->getType());
  assert(Dest + ValSize <= Buf + BufSize);
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
  } else if (isa<ConstantArray>(Val) || isa<ConstantDataVector>(Val) ||
             isa<ConstantVector>(Val)) {
    uint64_t ElementSize = getDataLayout().getTypeAllocSize(
        Val->getType()->getSequentialElementType());
    for (unsigned I = 0; I < Val->getNumOperands(); ++I) {
      putAtDest(cast<Constant>(Val->getOperand(I)), Dest + ElementSize * I);
    }
  } else if (ConstantStruct *CS = dyn_cast<ConstantStruct>(Val)) {
    const StructLayout *Layout = getDataLayout().getStructLayout(CS->getType());
    for (unsigned I = 0; I < CS->getNumOperands(); ++I) {
      putAtDest(CS->getOperand(I), Dest + Layout->getElementOffset(I));
    }
  } else {
    Constant *GV;
    uint64_t Offset;
    ExpandConstant(&getDataLayout(), Val, &GV, &Offset);
    if (GV) {
      Constant *NewVal = ConstantExpr::getPtrToInt(GV, getIntPtrType());
      if (Offset) {
        // For simplicity, require addends to be 32-bit.
        if ((int64_t) Offset != (int32_t) (uint32_t) Offset) {
          errs() << "Not handled: " << *Val << "\n";
          report_fatal_error(
              "FlattenGlobals: Offset does not fit into 32 bits");
        }
        NewVal = ConstantExpr::getAdd(
            NewVal, ConstantInt::get(getIntPtrType(), Offset,
                                     /* isSigned= */ true));
      }
      Reloc NewRel(State, Dest - Buf, NewVal);
      Relocs.push_back(NewRel);
    } else {
      memcpy(Dest, &Offset, ValSize);
    }
  }
}

Constant *FlattenedConstant::getAsNormalFormConstant() const {
  // Return a single SimpleElement.
  if (Relocs.size() == 0)
    return dataSlice(0, BufSize);
  if (Relocs.size() == 1 && BufSize == getPtrSize()) {
    assert(Relocs[0].getRelOffset() == 0);
    return Relocs[0].getRelocUse();
  }

  // Return a CompoundElement.
  SmallVector<Constant *, 10> Elements;
  unsigned PrevPos = 0;
  for (RelocArray::const_iterator Rel = Relocs.begin(), E = Relocs.end();
       Rel != E; ++Rel) {
    if (Rel->getRelOffset() > PrevPos)
      Elements.push_back(dataSlice(PrevPos, Rel->getRelOffset()));
    Elements.push_back(Rel->getRelocUse());
    PrevPos = Rel->getRelOffset() + getPtrSize();
  }
  if (PrevPos < BufSize)
    Elements.push_back(dataSlice(PrevPos, BufSize));
  return ConstantStruct::getAnon(getModule().getContext(), Elements, true);
}

Type *FlattenedConstant::getAsNormalFormType() const {
  // Return a single element type.
  if (Relocs.size() == 0)
    return dataSliceType(0, BufSize);
  if (Relocs.size() == 1 && BufSize == getPtrSize()) {
    assert(Relocs[0].getRelOffset() == 0);
    return Relocs[0].getRelocUse()->getType();
  }

  // Return a compound type.
  SmallVector<Type *, 10> Elements;
  unsigned PrevPos = 0;
  for (RelocArray::const_iterator Rel = Relocs.begin(), E = Relocs.end();
       Rel != E; ++Rel) {
    if (Rel->getRelOffset() > PrevPos)
      Elements.push_back(dataSliceType(PrevPos, Rel->getRelOffset()));
    Elements.push_back(Rel->getRelocUse()->getType());
    PrevPos = Rel->getRelOffset() + getPtrSize();
  }
  if (PrevPos < BufSize)
    Elements.push_back(dataSliceType(PrevPos, BufSize));
  return StructType::get(getModule().getContext(), Elements, true);
}

char FlattenGlobals::ID = 0;
INITIALIZE_PASS(FlattenGlobals, "flatten-globals",
                "Flatten global variable initializers into byte arrays",
                false, false)

void FlattenGlobalsState::flattenGlobalsWithInitializers() {
  for (Module::global_iterator I = M.global_begin(), E = M.global_end();
       I != E;) {
    GlobalVariable *Global = &*I++;
    // Variables with "appending" linkage must always be arrays and so
    // cannot be normalized, so leave them alone.
    if (Global->hasAppendingLinkage())
      continue;
    Modified = true;
    FlattenedGlobalsVector.push_back(new FlattenedGlobal(*this, Global));
  }
}

void FlattenGlobalsState::removeDeadInitializerConstants() {
  // Detach original initializers.
  for (FlattenedGlobalsVectorType::iterator
           I = FlattenedGlobalsVector.begin(), E = FlattenedGlobalsVector.end();
       I != E; ++I) {
    (*I)->removeOriginalInitializer();
  }
  // Do cleanup of old initializers.
  for (RelocMapType::iterator I = RelocMap.begin(), E = RelocMap.end();
       I != E; ++I) {
    getRelocUseConstant(I->second)->removeDeadConstantUsers();
  }

}

void FlattenGlobalsState::replaceGlobalsWithFlattenedGlobals() {
  for (FlattenedGlobalsVectorType::iterator
           I = FlattenedGlobalsVector.begin(), E = FlattenedGlobalsVector.end();
       I != E; ++I) {
    (*I)->replaceGlobalWithFlattenedGlobal();
  }
}

void FlattenGlobalsState::installFlattenedGlobalInitializers() {
  for (FlattenedGlobalsVectorType::iterator
           I = FlattenedGlobalsVector.begin(), E = FlattenedGlobalsVector.end();
       I != E; ++I) {
    (*I)->installFlattenedInitializer();
  }
}

bool FlattenGlobals::runOnModule(Module &M) {
  FlattenGlobalsState State(M);
  State.flattenGlobalsWithInitializers();
  State.removeDeadInitializerConstants();
  State.replaceGlobalsWithFlattenedGlobals();
  State.installFlattenedGlobalInitializers();
  return State.Modified;
}

ModulePass *llvm::createFlattenGlobalsPass() {
  return new FlattenGlobals();
}
