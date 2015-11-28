//===- ExpandTls.cpp - Convert TLS variables to a concrete layout----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass expands out uses of thread-local (TLS) variables into
// more primitive operations.
//
// A reference to the address of a TLS variable is expanded into code
// which gets the current thread's thread pointer using
// @llvm.nacl.read.tp() and adds a fixed offset.
//
// This pass allocates the offsets (relative to the thread pointer)
// that will be used for TLS variables.  It sets up the global
// variables __tls_template_start, __tls_template_end etc. to contain
// a template for initializing TLS variables' values for each thread.
// This is a task normally performed by the linker in ELF systems.
//
//===----------------------------------------------------------------------===//

#include <vector>

#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  struct VarInfo {
    GlobalVariable *TlsVar;
    bool IsBss; // Whether variable is in zero-intialized part of template
    int TemplateIndex;
  };

  class PassState {
  public:
    PassState(Module *M): M(M), DL(M), Offset(0), Alignment(1) {}

    Module *M;
    DataLayout DL;
    uint64_t Offset;
    // 'Alignment' is the maximum variable alignment seen so far, in
    // bytes.  After visiting all TLS variables, this is the overall
    // alignment required for the TLS template.
    uint32_t Alignment;
  };

  class ExpandTls : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    ExpandTls() : ModulePass(ID) {
      initializeExpandTlsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

char ExpandTls::ID = 0;
INITIALIZE_PASS(ExpandTls, "nacl-expand-tls",
                "Expand out TLS variables and fix TLS variable layout",
                false, false)

static void setGlobalVariableValue(Module &M, const char *Name,
                                   Constant *Value) {
  if (GlobalVariable *Var = M.getNamedGlobal(Name)) {
    if (Var->hasInitializer()) {
      report_fatal_error(std::string("Variable ") + Name +
                         " already has an initializer");
    }
    Var->replaceAllUsesWith(ConstantExpr::getBitCast(Value, Var->getType()));
    Var->eraseFromParent();
  }
}

// Insert alignment padding into the TLS template.
static void padToAlignment(PassState *State,
                           std::vector<Type*> *FieldTypes,
                           std::vector<Constant*> *FieldValues,
                           unsigned Alignment) {
  if ((State->Offset & (Alignment - 1)) != 0) {
    unsigned PadSize = Alignment - (State->Offset & (Alignment - 1));
    Type *i8 = Type::getInt8Ty(State->M->getContext());
    Type *PadType = ArrayType::get(i8, PadSize);
    FieldTypes->push_back(PadType);
    if (FieldValues)
      FieldValues->push_back(Constant::getNullValue(PadType));
    State->Offset += PadSize;
  }
  if (State->Alignment < Alignment) {
    State->Alignment = Alignment;
  }
}

static void addVarToTlsTemplate(PassState *State,
                                std::vector<Type*> *FieldTypes,
                                std::vector<Constant*> *FieldValues,
                                GlobalVariable *TlsVar) {
  unsigned Alignment = State->DL.getPreferredAlignment(TlsVar);
  padToAlignment(State, FieldTypes, FieldValues, Alignment);

  FieldTypes->push_back(TlsVar->getType()->getElementType());
  if (FieldValues)
    FieldValues->push_back(TlsVar->getInitializer());
  State->Offset +=
      State->DL.getTypeAllocSize(TlsVar->getType()->getElementType());
}

static StructType *buildTlsTemplate(Module &M, std::vector<VarInfo> *TlsVars) {
  std::vector<Type*> FieldBssTypes;
  std::vector<Type*> FieldInitTypes;
  std::vector<Constant*> FieldInitValues;
  PassState State(&M);

  for (Module::global_iterator GV = M.global_begin();
       GV != M.global_end();
       ++GV) {
    if (GV->isThreadLocal()) {
      if (!GV->hasInitializer()) {
        // Since this is a whole-program transformation, "extern" TLS
        // variables are not allowed at this point.
        report_fatal_error(std::string("TLS variable without an initializer: ")
                           + GV->getName());
      }
      if (!GV->getInitializer()->isNullValue()) {
        addVarToTlsTemplate(&State, &FieldInitTypes,
                            &FieldInitValues, &*GV);
        VarInfo Info;
        Info.TlsVar = &*GV;
        Info.IsBss = false;
        Info.TemplateIndex = FieldInitTypes.size() - 1;
        TlsVars->push_back(Info);
      }
    }
  }
  // Handle zero-initialized TLS variables in a second pass, because
  // these should follow non-zero-initialized TLS variables.
  for (Module::global_iterator GV = M.global_begin();
       GV != M.global_end();
       ++GV) {
    if (GV->isThreadLocal() && GV->getInitializer()->isNullValue()) {
      addVarToTlsTemplate(&State, &FieldBssTypes, NULL, &*GV);
      VarInfo Info;
      Info.TlsVar = &*GV;
      Info.IsBss = true;
      Info.TemplateIndex = FieldBssTypes.size() - 1;
      TlsVars->push_back(Info);
    }
  }
  // Add final alignment padding so that
  //   (struct tls_struct *) __nacl_read_tp() - 1
  // gives the correct, aligned start of the TLS variables given the
  // x86-style layout we are using.  This requires some more bytes to
  // be memset() to zero at runtime.  This wastage doesn't seem
  // important gives that we're not trying to optimize packing by
  // reordering to put similarly-aligned variables together.
  padToAlignment(&State, &FieldBssTypes, NULL, State.Alignment);

  // We create the TLS template structs as "packed" because we insert
  // alignment padding ourselves, and LLVM's implicit insertion of
  // padding would interfere with ours.  tls_bss_template can start at
  // a non-aligned address immediately following the last field in
  // tls_init_template.
  StructType *InitTemplateType =
      StructType::create(M.getContext(), "tls_init_template");
  InitTemplateType->setBody(FieldInitTypes, /*isPacked=*/true);
  StructType *BssTemplateType =
      StructType::create(M.getContext(), "tls_bss_template");
  BssTemplateType->setBody(FieldBssTypes, /*isPacked=*/true);

  StructType *TemplateType = StructType::create(M.getContext(), "tls_struct");
  SmallVector<Type*, 2> TemplateTopFields;
  TemplateTopFields.push_back(InitTemplateType);
  TemplateTopFields.push_back(BssTemplateType);
  TemplateType->setBody(TemplateTopFields, /*isPacked=*/true);
  PointerType *TemplatePtrType = PointerType::get(TemplateType, 0);

  // We define the following symbols, which are the same as those
  // defined by NaCl's original customized binutils linker scripts:
  //   __tls_template_start
  //   __tls_template_tdata_end
  //   __tls_template_end
  // We also define __tls_template_alignment, which was not defined by
  // the original linker scripts.

  const char *StartSymbol = "__tls_template_start";
  Constant *TemplateData = ConstantStruct::get(InitTemplateType,
                                               FieldInitValues);
  GlobalVariable *TemplateDataVar =
      new GlobalVariable(M, InitTemplateType, /*isConstant=*/true,
                         GlobalValue::InternalLinkage, TemplateData);
  setGlobalVariableValue(M, StartSymbol, TemplateDataVar);
  TemplateDataVar->setName(StartSymbol);

  Constant *TdataEnd = ConstantExpr::getGetElementPtr(
      InitTemplateType,
      TemplateDataVar,
      ConstantInt::get(M.getContext(), APInt(32, 1)));
  setGlobalVariableValue(M, "__tls_template_tdata_end", TdataEnd);

  Constant *TotalEnd = ConstantExpr::getGetElementPtr(
      TemplateType,
      ConstantExpr::getBitCast(TemplateDataVar, TemplatePtrType),
      ConstantInt::get(M.getContext(), APInt(32, 1)));
  setGlobalVariableValue(M, "__tls_template_end", TotalEnd);

  const char *AlignmentSymbol = "__tls_template_alignment";
  Type *i32 = Type::getInt32Ty(M.getContext());
  GlobalVariable *AlignmentVar = new GlobalVariable(
      M, i32, /*isConstant=*/true,
      GlobalValue::InternalLinkage,
      ConstantInt::get(M.getContext(), APInt(32, State.Alignment)));
  setGlobalVariableValue(M, AlignmentSymbol, AlignmentVar);
  AlignmentVar->setName(AlignmentSymbol);

  return TemplateType;
}

static void rewriteTlsVars(Module &M, std::vector<VarInfo> *TlsVars,
                           StructType *TemplateType) {
  // Set up the intrinsic that reads the thread pointer.
  Function *ReadTpFunc = Intrinsic::getDeclaration(&M, Intrinsic::nacl_read_tp);

  for (std::vector<VarInfo>::iterator VarInfo = TlsVars->begin();
       VarInfo != TlsVars->end();
       ++VarInfo) {
    GlobalVariable *Var = VarInfo->TlsVar;
    while (Var->hasNUsesOrMore(1)) {
      Use *U = &*Var->use_begin();
      Instruction *InsertPt = PhiSafeInsertPt(U);
      Value *RawThreadPtr = CallInst::Create(ReadTpFunc, "tls_raw", InsertPt);
      Value *TypedThreadPtr = new BitCastInst(
          RawThreadPtr, TemplateType->getPointerTo(), "tls_struct", InsertPt);
      SmallVector<Value*, 3> Indexes;
      // We use -1 because we use the x86-style TLS layout in which
      // the TLS data is stored at addresses below the thread pointer.
      // This is largely because a check in nacl_irt_thread_create()
      // in irt/irt_thread.c requires the thread pointer to be a
      // self-pointer on x86-32.
      // TODO(mseaborn): I intend to remove that check because it is
      // non-portable.  In the mean time, we want PNaCl pexes to work
      // in older Chromium releases when translated to nexes.
      Indexes.push_back(ConstantInt::get(
          M.getContext(), APInt(32, -1)));
      Indexes.push_back(ConstantInt::get(
          M.getContext(), APInt(32, VarInfo->IsBss ? 1 : 0)));
      Indexes.push_back(ConstantInt::get(
          M.getContext(), APInt(32, VarInfo->TemplateIndex)));
      Value *TlsField = GetElementPtrInst::Create(
          TemplateType, TypedThreadPtr, Indexes, "field", InsertPt);
      PhiSafeReplaceUses(U, TlsField);
    }
    VarInfo->TlsVar->eraseFromParent();
  }
}

static void replaceFunction(Module &M, const char *Name, Value *NewFunc) {
  if (Function *Func = M.getFunction(Name)) {
    if (Func->hasLocalLinkage())
      return;
    if (!Func->isDeclaration())
      report_fatal_error(std::string("Function already defined: ") + Name);
    Func->replaceAllUsesWith(NewFunc);
    Func->eraseFromParent();
  }
}

// Provide fixed definitions for NaCl's TLS layout functions,
// __nacl_tp_*().  We adopt the x86-style layout: ExpandTls will
// output a program that uses the x86-style layout wherever it runs.
//
// This overrides the architecture-specific definitions of
// __nacl_tp_*() that PNaCl's native support code makes available to
// non-ABI-stable code.
static void defineTlsLayoutFunctions(Module &M) {
  Type *i32 = Type::getInt32Ty(M.getContext());
  SmallVector<Type*, 1> ArgTypes;
  ArgTypes.push_back(i32);
  FunctionType *FuncType = FunctionType::get(i32, ArgTypes, /*isVarArg=*/false);
  Function *NewFunc;
  BasicBlock *BB;

  // Define the function as follows:
  //   uint32_t __nacl_tp_tdb_offset(uint32_t tdb_size) {
  //     return 0;
  //   }
  // This means the thread pointer points to the TDB.
  NewFunc = Function::Create(FuncType, GlobalValue::InternalLinkage,
                             "nacl_tp_tdb_offset", &M);
  BB = BasicBlock::Create(M.getContext(), "entry", NewFunc);
  ReturnInst::Create(M.getContext(),
                     ConstantInt::get(M.getContext(), APInt(32, 0)), BB);
  replaceFunction(M, "__nacl_tp_tdb_offset", NewFunc);

  // Define the function as follows:
  //   uint32_t __nacl_tp_tls_offset(uint32_t tls_size) {
  //     return -tls_size;
  //   }
  // This means the TLS variables are stored below the thread pointer.
  NewFunc = Function::Create(FuncType, GlobalValue::InternalLinkage,
                             "nacl_tp_tls_offset", &M);
  BB = BasicBlock::Create(M.getContext(), "entry", NewFunc);
  Value *Arg = &*NewFunc->arg_begin();
  Arg->setName("size");
  Value *Result = BinaryOperator::CreateNeg(Arg, "result", BB);
  ReturnInst::Create(M.getContext(), Result, BB);
  replaceFunction(M, "__nacl_tp_tls_offset", NewFunc);
}

bool ExpandTls::runOnModule(Module &M) {
  ModulePass *Pass = createExpandTlsConstantExprPass();
  Pass->runOnModule(M);
  delete Pass;

  std::vector<VarInfo> TlsVars;
  StructType *TemplateType = buildTlsTemplate(M, &TlsVars);
  rewriteTlsVars(M, &TlsVars, TemplateType);

  defineTlsLayoutFunctions(M);

  return true;
}

ModulePass *llvm::createExpandTlsPass() {
  return new ExpandTls();
}
