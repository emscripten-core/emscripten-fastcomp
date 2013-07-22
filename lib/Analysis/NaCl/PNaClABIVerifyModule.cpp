//===- PNaClABIVerifyModule.cpp - Verify PNaCl ABI rules ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Verify module-level PNaCl ABI requirements (specifically those that do not
// require looking at the function bodies)
//
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "PNaClABITypeChecker.h"
using namespace llvm;

namespace llvm {
cl::opt<bool>
PNaClABIAllowDebugMetadata("pnaclabi-allow-debug-metadata",
  cl::desc("Allow debug metadata during PNaCl ABI verification."),
  cl::init(false));

}

static cl::opt<bool>
PNaClABIAllowDevIntrinsics("pnaclabi-allow-dev-intrinsics",
  cl::desc("Allow dev LLVM intrinsics during PNaCl ABI verification."),
  cl::init(false));

namespace {
// This pass should not touch function bodies, to stay streaming-friendly
class PNaClABIVerifyModule : public ModulePass {
 public:
  static char ID;
  PNaClABIVerifyModule() :
      ModulePass(ID),
      Reporter(new PNaClABIErrorReporter),
      ReporterIsOwned(true) {
    initializePNaClABIVerifyModulePass(*PassRegistry::getPassRegistry());
  }
  explicit PNaClABIVerifyModule(PNaClABIErrorReporter *Reporter_,
                                bool StreamingMode) :
      ModulePass(ID),
      Reporter(Reporter_),
      ReporterIsOwned(false),
      StreamingMode(StreamingMode) {
    initializePNaClABIVerifyModulePass(*PassRegistry::getPassRegistry());
  }
  ~PNaClABIVerifyModule() {
    if (ReporterIsOwned)
      delete Reporter;
  }
  bool runOnModule(Module &M);
  virtual void print(raw_ostream &O, const Module *M) const;
 private:
  void checkGlobalValueCommon(const GlobalValue *GV);
  bool isWhitelistedMetadata(const NamedMDNode *MD);

  /// Returns whether \p GV is an allowed external symbol in stable bitcode.
  bool isWhitelistedExternal(const GlobalValue *GV);

  void checkGlobalIsFlattened(const GlobalVariable *GV);
  PNaClABIErrorReporter *Reporter;
  bool ReporterIsOwned;
  bool StreamingMode;
};

class AllowedIntrinsics {
  LLVMContext *Context;
  // Maps from an allowed intrinsic's name to its type.
  StringMap<FunctionType *> Mapping;

  // Tys is an array of type parameters for the intrinsic.  This
  // defaults to an empty array.
  void addIntrinsic(Intrinsic::ID ID,
                    ArrayRef<Type *> Tys = ArrayRef<Type*>()) {
    Mapping[Intrinsic::getName(ID, Tys)] =
        Intrinsic::getType(*Context, ID, Tys);
  }
public:
  AllowedIntrinsics(LLVMContext *Context);
  bool isAllowed(const Function *Func);
};

static const char *linkageName(GlobalValue::LinkageTypes LT) {
  // This logic is taken from PrintLinkage in lib/VMCore/AsmWriter.cpp
  switch (LT) {
    case GlobalValue::ExternalLinkage: return "external";
    case GlobalValue::PrivateLinkage:       return "private ";
    case GlobalValue::LinkerPrivateLinkage: return "linker_private ";
    case GlobalValue::LinkerPrivateWeakLinkage: return "linker_private_weak ";
    case GlobalValue::InternalLinkage:      return "internal ";
    case GlobalValue::LinkOnceAnyLinkage:   return "linkonce ";
    case GlobalValue::LinkOnceODRLinkage:   return "linkonce_odr ";
    case GlobalValue::LinkOnceODRAutoHideLinkage:
      return "linkonce_odr_auto_hide ";
    case GlobalValue::WeakAnyLinkage:       return "weak ";
    case GlobalValue::WeakODRLinkage:       return "weak_odr ";
    case GlobalValue::CommonLinkage:        return "common ";
    case GlobalValue::AppendingLinkage:     return "appending ";
    case GlobalValue::DLLImportLinkage:     return "dllimport ";
    case GlobalValue::DLLExportLinkage:     return "dllexport ";
    case GlobalValue::ExternalWeakLinkage:  return "extern_weak ";
    case GlobalValue::AvailableExternallyLinkage:
      return "available_externally ";
    default:
      return "unknown";
  }
}

} // end anonymous namespace

// Check linkage type and section attributes, which are the same for
// GlobalVariables and Functions.
void PNaClABIVerifyModule::checkGlobalValueCommon(const GlobalValue *GV) {
  assert(!isa<GlobalAlias>(GV));
  const char *GVTypeName = isa<GlobalVariable>(GV) ?
      "Variable " : "Function ";
  switch (GV->getLinkage()) {
    case GlobalValue::ExternalLinkage:
      if (!isWhitelistedExternal(GV)) {
        Reporter->addError()
          << GV->getName()
          << " is not a valid external symbol (disallowed)\n";
      }
      break;
    case GlobalValue::InternalLinkage:
      break;
    default:
      Reporter->addError() << GVTypeName << GV->getName()
                           << " has disallowed linkage type: "
                           << linkageName(GV->getLinkage()) << "\n";
  }
  if (GV->getVisibility() != GlobalValue::DefaultVisibility) {
    std::string Text = "unknown";
    if (GV->getVisibility() == GlobalValue::HiddenVisibility) {
      Text = "hidden";
    } else if (GV->getVisibility() == GlobalValue::ProtectedVisibility) {
      Text = "protected";
    }
    Reporter->addError() << GVTypeName << GV->getName()
                         << " has disallowed visibility: " << Text << "\n";
  }
  if (GV->hasSection()) {
    Reporter->addError() << GVTypeName << GV->getName() <<
        " has disallowed \"section\" attribute\n";
  }
  if (GV->getType()->getAddressSpace() != 0) {
    Reporter->addError() << GVTypeName << GV->getName()
                         << " has addrspace attribute (disallowed)\n";
  }
  // The "unnamed_addr" attribute can be used to merge duplicate
  // definitions, but that should be done by user-toolchain
  // optimization passes, not by the PNaCl translator.
  if (GV->hasUnnamedAddr()) {
    Reporter->addError() << GVTypeName << GV->getName()
                         << " has disallowed \"unnamed_addr\" attribute\n";
  }
}

AllowedIntrinsics::AllowedIntrinsics(LLVMContext *Context) : Context(Context) {
  Type *I8Ptr = Type::getInt8PtrTy(*Context);
  Type *I8 = Type::getInt8Ty(*Context);
  Type *I16 = Type::getInt16Ty(*Context);
  Type *I32 = Type::getInt32Ty(*Context);
  Type *I64 = Type::getInt64Ty(*Context);
  Type *Float = Type::getFloatTy(*Context);
  Type *Double = Type::getDoubleTy(*Context);

  // We accept bswap for a limited set of types (i16, i32, i64).  The
  // various backends are able to generate instructions to implement
  // the intrinsic.  Also, i16 and i64 are easy to implement as along
  // as there is a way to do i32.
  addIntrinsic(Intrinsic::bswap, I16);
  addIntrinsic(Intrinsic::bswap, I32);
  addIntrinsic(Intrinsic::bswap, I64);

  // We accept cttz, ctlz, and ctpop for a limited set of types (i32, i64).
  addIntrinsic(Intrinsic::ctlz, I32);
  addIntrinsic(Intrinsic::ctlz, I64);
  addIntrinsic(Intrinsic::cttz, I32);
  addIntrinsic(Intrinsic::cttz, I64);
  addIntrinsic(Intrinsic::ctpop, I32);
  addIntrinsic(Intrinsic::ctpop, I64);

  addIntrinsic(Intrinsic::nacl_read_tp);
  addIntrinsic(Intrinsic::nacl_longjmp);
  addIntrinsic(Intrinsic::nacl_setjmp);

  // For native sqrt instructions. Must guarantee when x < -0.0, sqrt(x) = NaN.
  addIntrinsic(Intrinsic::sqrt, Float);
  addIntrinsic(Intrinsic::sqrt, Double);

  Type *AtomicTypes[] = { I8, I16, I32, I64 };
  for (size_t T = 0, E = array_lengthof(AtomicTypes); T != E; ++T) {
    addIntrinsic(Intrinsic::nacl_atomic_load, AtomicTypes[T]);
    addIntrinsic(Intrinsic::nacl_atomic_store, AtomicTypes[T]);
    addIntrinsic(Intrinsic::nacl_atomic_rmw, AtomicTypes[T]);
    addIntrinsic(Intrinsic::nacl_atomic_cmpxchg, AtomicTypes[T]);
  }
  addIntrinsic(Intrinsic::nacl_atomic_fence);

  // Stack save and restore are used to support C99 VLAs.
  addIntrinsic(Intrinsic::stacksave);
  addIntrinsic(Intrinsic::stackrestore);

  addIntrinsic(Intrinsic::trap);

  // We only allow the variants of memcpy/memmove/memset with an i32
  // "len" argument, not an i64 argument.
  Type *MemcpyTypes[] = { I8Ptr, I8Ptr, I32 };
  addIntrinsic(Intrinsic::memcpy, MemcpyTypes);
  addIntrinsic(Intrinsic::memmove, MemcpyTypes);
  Type *MemsetTypes[] = { I8Ptr, I32 };
  addIntrinsic(Intrinsic::memset, MemsetTypes);
}

bool AllowedIntrinsics::isAllowed(const Function *Func) {
  // Keep 3 categories of intrinsics for now.
  // (1) Allowed always, provided the exact name and type match.
  // (2) Never allowed.
  // (3) "Dev": intrinsics in the development or prototype stage,
  // or private intrinsics used for building special programs.
  // (4) Debug info intrinsics.
  //
  // Please keep these sorted or grouped in a sensible way, within
  // each category.

  // (1) Allowed always, provided the exact name and type match.
  if (Mapping.count(Func->getName()) == 1)
    return Func->getFunctionType() == Mapping[Func->getName()];

  switch (Func->getIntrinsicID()) {
    // Disallow by default.
    default: return false;

    // (2) Known to be never allowed.
    case Intrinsic::not_intrinsic:
    // Trampolines depend on a target-specific-sized/aligned buffer.
    case Intrinsic::adjust_trampoline:
    case Intrinsic::init_trampoline:
    // CXX exception handling is not stable.
    case Intrinsic::eh_dwarf_cfa:
    case Intrinsic::eh_return_i32:
    case Intrinsic::eh_return_i64:
    case Intrinsic::eh_sjlj_callsite:
    case Intrinsic::eh_sjlj_functioncontext:
    case Intrinsic::eh_sjlj_longjmp:
    case Intrinsic::eh_sjlj_lsda:
    case Intrinsic::eh_sjlj_setjmp:
    case Intrinsic::eh_typeid_for:
    case Intrinsic::eh_unwind_init:
    // We do not want to expose addresses to the user.
    case Intrinsic::frameaddress:
    case Intrinsic::returnaddress:
    // Not supporting stack protectors.
    case Intrinsic::stackprotector:
    // Var-args handling is done w/out intrinsics.
    case Intrinsic::vacopy:
    case Intrinsic::vaend:
    case Intrinsic::vastart:
    // Disallow the *_with_overflow intrinsics because they return
    // struct types.  All of them can be introduced by passing -ftrapv
    // to Clang, which we do not support for now.  umul_with_overflow
    // and uadd_with_overflow are introduced by Clang for C++'s new[],
    // but ExpandArithWithOverflow expands out this use.
    case Intrinsic::sadd_with_overflow:
    case Intrinsic::ssub_with_overflow:
    case Intrinsic::uadd_with_overflow:
    case Intrinsic::usub_with_overflow:
    case Intrinsic::smul_with_overflow:
    case Intrinsic::umul_with_overflow:
    // Disallow lifetime.start/end because the semantics of what
    // arguments they accept are not very well defined, and because it
    // would be better to do merging of stack slots in the user
    // toolchain than in the PNaCl translator.
    // See https://code.google.com/p/nativeclient/issues/detail?id=3443
    case Intrinsic::lifetime_end:
    case Intrinsic::lifetime_start:
    case Intrinsic::invariant_end:
    case Intrinsic::invariant_start:
    // Some transcendental functions not needed yet.
    case Intrinsic::cos:
    case Intrinsic::exp:
    case Intrinsic::exp2:
    case Intrinsic::log:
    case Intrinsic::log2:
    case Intrinsic::log10:
    case Intrinsic::pow:
    case Intrinsic::powi:
    case Intrinsic::sin:
    // We run -lower-expect to convert Intrinsic::expect into branch weights
    // and consume in the middle-end. The backend just ignores llvm.expect.
    case Intrinsic::expect:
    // For FLT_ROUNDS macro from float.h. It works for ARM and X86
    // (but not MIPS). Also, wait until we add a set_flt_rounds intrinsic
    // before we bless this.
    case Intrinsic::flt_rounds:
      return false;

    // (3) Dev intrinsics.
    case Intrinsic::nacl_target_arch: // Used by translator self-build.
      return PNaClABIAllowDevIntrinsics;

    // (4) Debug info intrinsics.
    case Intrinsic::dbg_declare:
    case Intrinsic::dbg_value:
      return PNaClABIAllowDebugMetadata;
  }
}

bool PNaClABIVerifyModule::isWhitelistedMetadata(const NamedMDNode *MD) {
  return MD->getName().startswith("llvm.dbg.") && PNaClABIAllowDebugMetadata;
}

bool PNaClABIVerifyModule::isWhitelistedExternal(const GlobalValue *GV) {
  if (const Function *Func = dyn_cast<const Function>(GV)) {
    if (Func->getName().equals("_start") || Func->isIntrinsic()) {
      return true;
    }
  }
  return false;
}

static bool isPtrToIntOfGlobal(const Constant *C) {
  if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
    return CE->getOpcode() == Instruction::PtrToInt &&
           isa<GlobalValue>(CE->getOperand(0));
  }
  return false;
}

// This checks for part of the normal form produced by FlattenGlobals.
static bool isSimpleElement(const Constant *C) {
  // A SimpleElement is one of the following:
  // 1) An i8 array literal or zeroinitializer:
  //      [SIZE x i8] c"DATA"
  //      [SIZE x i8] zeroinitializer
  if (ArrayType *Ty = dyn_cast<ArrayType>(C->getType())) {
    return Ty->getElementType()->isIntegerTy(8) &&
           (isa<ConstantAggregateZero>(C) ||
            isa<ConstantDataSequential>(C));
  }
  // 2) A reference to a GlobalValue (a function or global variable)
  //    with an optional byte offset added to it (the addend).
  if (C->getType()->isIntegerTy(32)) {
    const ConstantExpr *CE = dyn_cast<ConstantExpr>(C);
    if (!CE)
      return false;
    // Without addend:  ptrtoint (TYPE* @GLOBAL to i32)
    if (isPtrToIntOfGlobal(CE))
      return true;
    // With addend:  add (i32 ptrtoint (TYPE* @GLOBAL to i32), i32 ADDEND)
    if (CE->getOpcode() == Instruction::Add &&
        isPtrToIntOfGlobal(CE->getOperand(0)) &&
        isa<ConstantInt>(CE->getOperand(1)))
      return true;
  }
  return false;
}

// This checks for part of the normal form produced by FlattenGlobals.
static bool isCompoundElement(const Constant *C) {
  const ConstantStruct *CS = dyn_cast<ConstantStruct>(C);
  if (!CS || !CS->getType()->isPacked() || CS->getType()->hasName() ||
      CS->getNumOperands() <= 1)
    return false;
  for (unsigned I = 0; I < CS->getNumOperands(); ++I) {
    if (!isSimpleElement(CS->getOperand(I)))
      return false;
  }
  return true;
}

static std::string getAttributesAsString(AttributeSet Attrs) {
  std::string AttrsAsString;
  for (unsigned Slot = 0; Slot < Attrs.getNumSlots(); ++Slot) {
    for (AttributeSet::iterator Attr = Attrs.begin(Slot),
           E = Attrs.end(Slot); Attr != E; ++Attr) {
      AttrsAsString += " ";
      AttrsAsString += Attr->getAsString();
    }
  }
  return AttrsAsString;
}

// This checks that the GlobalVariable has the normal form produced by
// the FlattenGlobals pass.
void PNaClABIVerifyModule::checkGlobalIsFlattened(const GlobalVariable *GV) {
  if (!GV->hasInitializer()) {
    Reporter->addError() << "Global variable " << GV->getName()
                         << " has no initializer (disallowed)\n";
    return;
  }
  const Constant *InitVal = GV->getInitializer();
  if (isSimpleElement(InitVal) || isCompoundElement(InitVal))
    return;
  Reporter->addError() << "Global variable " << GV->getName()
                       << " has non-flattened initializer (disallowed): "
                       << *InitVal << "\n";
}

bool PNaClABIVerifyModule::runOnModule(Module &M) {
  AllowedIntrinsics Intrinsics(&M.getContext());

  if (!M.getModuleInlineAsm().empty()) {
    Reporter->addError() <<
        "Module contains disallowed top-level inline assembly\n";
  }

  for (Module::const_global_iterator MI = M.global_begin(), ME = M.global_end();
       MI != ME; ++MI) {
    checkGlobalIsFlattened(MI);
    checkGlobalValueCommon(MI);

    if (MI->isThreadLocal()) {
      Reporter->addError() << "Variable " << MI->getName() <<
          " has disallowed \"thread_local\" attribute\n";
    }
    if (MI->isExternallyInitialized()) {
      Reporter->addError() << "Variable " << MI->getName() <<
          " has disallowed \"externally_initialized\" attribute\n";
    }
  }

  // No aliases allowed for now.
  for (Module::alias_iterator MI = M.alias_begin(),
           E = M.alias_end(); MI != E; ++MI) {
    Reporter->addError() << "Variable " << MI->getName() <<
        " is an alias (disallowed)\n";
  }

  for (Module::const_iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
    if (MI->isIntrinsic()) {
      // Check intrinsics.
      if (!Intrinsics.isAllowed(MI)) {
        Reporter->addError() << "Function " << MI->getName()
                             << " is a disallowed LLVM intrinsic\n";
      }
    } else {
      // Check types of functions and their arguments.  Not necessary
      // for intrinsics, whose types are fixed anyway, and which have
      // argument types that we disallow such as i8.
      if (!PNaClABITypeChecker::isValidFunctionType(MI->getFunctionType())) {
        Reporter->addError() << "Function " << MI->getName()
            << " has disallowed type: "
            << PNaClABITypeChecker::getTypeName(MI->getFunctionType())
            << "\n";
      }
      // This check is disabled in streaming mode because it would
      // reject a function that is defined but not read in yet.
      // Unfortunately this means we simply don't check this property
      // when translating a pexe in the browser.
      // TODO(mseaborn): Enforce this property in the bitcode reader.
      if (!StreamingMode && MI->isDeclaration()) {
        Reporter->addError() << "Function " << MI->getName()
                             << " is declared but not defined (disallowed)\n";
      }
      if (!MI->getAttributes().isEmpty()) {
        Reporter->addError()
            << "Function " << MI->getName() << " has disallowed attributes:"
            << getAttributesAsString(MI->getAttributes()) << "\n";
      }
      if (MI->getCallingConv() != CallingConv::C) {
        Reporter->addError()
            << "Function " << MI->getName()
            << " has disallowed calling convention: "
            << MI->getCallingConv() << "\n";
      }
    }

    checkGlobalValueCommon(MI);

    if (MI->hasGC()) {
      Reporter->addError() << "Function " << MI->getName() <<
          " has disallowed \"gc\" attribute\n";
    }
    // Knowledge of what function alignments are useful is
    // architecture-specific and sandbox-specific, so PNaCl pexes
    // should not be able to specify function alignment.
    if (MI->getAlignment() != 0) {
      Reporter->addError() << "Function " << MI->getName() <<
          " has disallowed \"align\" attribute\n";
    }
  }

  // Check named metadata nodes
  for (Module::const_named_metadata_iterator I = M.named_metadata_begin(),
           E = M.named_metadata_end(); I != E; ++I) {
    if (!isWhitelistedMetadata(I)) {
      Reporter->addError() << "Named metadata node " << I->getName()
                           << " is disallowed\n";
    }
  }

  Reporter->checkForFatalErrors();
  return false;
}

// This method exists so that the passes can easily be run with opt -analyze.
// In this case the default constructor is used and we want to reset the error
// messages after each print (this is more of an issue for the FunctionPass
// than the ModulePass)
void PNaClABIVerifyModule::print(llvm::raw_ostream &O, const Module *M) const {
  Reporter->printErrors(O);
  Reporter->reset();
}

char PNaClABIVerifyModule::ID = 0;
INITIALIZE_PASS(PNaClABIVerifyModule, "verify-pnaclabi-module",
                "Verify module for PNaCl", false, true)

ModulePass *llvm::createPNaClABIVerifyModulePass(
    PNaClABIErrorReporter *Reporter, bool StreamingMode) {
  return new PNaClABIVerifyModule(Reporter, StreamingMode);
}
