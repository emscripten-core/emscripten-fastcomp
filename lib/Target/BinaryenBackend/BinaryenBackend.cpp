//===-- BinaryenBackend.cpp - Library for converting LLVM code to Binaryen       -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements compiling of LLVM IR, which is assumed to have been
// simplified using the PNaCl passes, and other necessary
// transformations, into WebAssembly using Binaryen, suitable for passing
// to emscripten for final processing.
//
//===----------------------------------------------------------------------===//

#include "BinaryenTargetMachine.h"
#include "MCTargetDesc/BinaryenBackendMCTargetDesc.h"
#include "AllocaManager.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Config/config.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Scalar.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set> // TODO: unordered_set?
#include <sstream>
using namespace llvm;

#include <OptPasses.h>

#include "Binaryen/binaryen-c.h"

// TODO: remove?
#ifdef NDEBUG
#undef assert
#define assert(x) { if (!(x)) report_fatal_error(#x); }
#endif

raw_ostream &prettyWarning() {
  errs().changeColor(raw_ostream::YELLOW);
  errs() << "warning:";
  errs().resetColor();
  errs() << " ";
  return errs();
}

static cl::opt<bool>
PreciseF32("emscripten-precise-f32",
           cl::desc("Enables Math.fround usage to implement precise float32 semantics and performance (see emscripten PRECISE_F32 option)"),
           cl::init(false));

static cl::opt<bool>
EnablePthreads("emscripten-enable-pthreads",
           cl::desc("Enables compilation targeting JavaScript Shared Array Buffer and Atomics API to implement support for pthreads-based multithreading"),
           cl::init(false));

static cl::opt<bool>
WarnOnUnaligned("emscripten-warn-unaligned",
                cl::desc("Warns about unaligned loads and stores (which can negatively affect performance)"),
                cl::init(false));

static cl::opt<bool>
WarnOnNoncanonicalNans("emscripten-warn-noncanonical-nans",
                cl::desc("Warns about detected noncanonical bit patterns in NaNs that will not be preserved in the generated output (this can cause code to run wrong if the exact bits were important)"),
                cl::init(true));

static cl::opt<int>
ReservedFunctionPointers("emscripten-reserved-function-pointers",
                         cl::desc("Number of reserved slots in function tables for functions to be added at runtime (see emscripten RESERVED_FUNCTION_POINTERS option)"),
                         cl::init(0));

static cl::opt<bool>
EmulatedFunctionPointers("emscripten-emulated-function-pointers",
                         cl::desc("Emulate function pointers, avoiding asm.js function tables (see emscripten EMULATED_FUNCTION_POINTERS option)"),
                         cl::init(false));

static cl::opt<int>
EmscriptenAssertions("emscripten-assertions",
                     cl::desc("Additional JS-specific assertions (see emscripten ASSERTIONS)"),
                     cl::init(0));

static cl::opt<bool>
NoAliasingFunctionPointers("emscripten-no-aliasing-function-pointers",
                           cl::desc("Forces function pointers to not alias (this is more correct, but rarely needed, and has the cost of much larger function tables; it is useful for debugging though; see emscripten ALIASING_FUNCTION_POINTERS option)"),
                           cl::init(false));

static cl::opt<int>
GlobalBase("emscripten-global-base",
           cl::desc("Where global variables start out in memory (see emscripten GLOBAL_BASE option)"),
           cl::init(8));

static cl::opt<bool>
Relocatable("emscripten-relocatable",
            cl::desc("Whether to emit relocatable code (see emscripten RELOCATABLE option)"),
            cl::init(false));

static cl::opt<bool>
EnableSjLjEH("enable-pnacl-sjlj-eh",
             cl::desc("Enable use of SJLJ-based C++ exception handling "
                      "as part of the pnacl-abi-simplify passes"),
             cl::init(false));

static cl::opt<bool>
EnableEmCxxExceptions("enable-emscripten-cxx-exceptions",
                      cl::desc("Enables C++ exceptions in emscripten"),
                      cl::init(false));

static cl::opt<bool>
EnableEmAsyncify("emscripten-asyncify",
                 cl::desc("Enable asyncify transformation (see emscripten ASYNCIFY option)"),
                 cl::init(false));

static cl::opt<bool>
NoExitRuntime("emscripten-no-exit-runtime",
              cl::desc("Generate code which assumes the runtime is never exited (so atexit etc. is unneeded; see emscripten NO_EXIT_RUNTIME setting)"),
              cl::init(false));

static cl::opt<bool>

EnableCyberDWARF("enable-cyberdwarf",
                 cl::desc("Include CyberDWARF debug information"),
                 cl::init(false));

static cl::opt<bool>
EnableCyberDWARFIntrinsics("enable-debug-intrinsics",
                           cl::desc("Include debug intrinsics in generated output"),
                           cl::init(false));

static cl::opt<bool>
WebAssembly("emscripten-wasm",
            cl::desc("Generate asm.js which will later be compiled to WebAssembly (see emscripten BINARYEN setting)"),
            cl::init(false));


extern "C" void LLVMInitializeBinaryenBackendTarget() {
  // Register the target.
  RegisterTargetMachine<BinaryenTargetMachine> X(TheBinaryenBackendTarget);
}

namespace {
  #define ASM_SIGNED 0
  #define ASM_UNSIGNED 1
  #define ASM_NONSPECIFIC 2 // nonspecific means to not differentiate ints. |0 for all, regardless of size and sign
  #define ASM_FFI_IN 4 // FFI return values are limited to things that work in ffis
  #define ASM_FFI_OUT 8 // params to FFIs are limited to things that work in ffis
  #define ASM_MUST_CAST 16 // this value must be explicitly cast (or be an integer constant)
  #define ASM_FORCE_FLOAT_AS_INTBITS 32 // if the value is a float, it should be returned as an integer representing the float bits (or NaN canonicalization will eat them away). This flag cannot be used with ASM_UNSIGNED set.
  typedef unsigned AsmCast;

  typedef std::map<const Value*,std::string> ValueMap;
  typedef std::set<std::string> NameSet;
  typedef std::set<int> IntSet;
  typedef std::vector<unsigned char> HeapData;
  typedef std::map<int, HeapData> HeapDataMap;
  typedef std::vector<int> AlignedHeapStartMap;
  struct Address {
    unsigned Offset, Alignment;
    bool ZeroInit;
    Address() {}
    Address(unsigned Offset, unsigned Alignment, bool ZeroInit) : Offset(Offset), Alignment(Alignment), ZeroInit(ZeroInit) {}
  };
  typedef std::map<std::string, Type *> VarMap;
  typedef std::map<std::string, Address> GlobalAddressMap;
  typedef std::vector<std::string> FunctionTable;
  typedef std::map<std::string, FunctionTable> FunctionTableMap;
  typedef std::map<std::string, std::string> StringMap;
  typedef std::map<std::string, unsigned> NameIntMap;
  typedef std::map<unsigned, IntSet> IntIntSetMap;
  typedef std::map<const BasicBlock*, unsigned> BlockIndexMap;
  typedef std::map<const Function*, BlockIndexMap> BlockAddressMap;
  typedef std::map<const BasicBlock*, RelooperBlockRef> LLVMToRelooperMap;
  struct AsmConstInfo {
    int Id;
    std::set<std::string> Sigs;
  };

  /// BinaryenWriter - This class is the main chunk of code that converts an LLVM
  /// module to JavaScript.
  class BinaryenWriter : public ModulePass {
    raw_pwrite_stream &Out;
    Module *TheModule;
    BinaryenModuleRef Wasm;
    unsigned UniqueNum;
    unsigned NextFunctionIndex; // used with NoAliasingFunctionPointers
    ValueMap ValueNames;
    VarMap UsedVars;
    AllocaManager Allocas;
    HeapDataMap GlobalDataMap;
    std::vector<int> ZeroInitSizes; // alignment => used offset in the zeroinit zone
    AlignedHeapStartMap AlignedHeapStarts, ZeroInitStarts;
    GlobalAddressMap GlobalAddresses;
    NameSet Externals; // vars
    NameSet Declares; // funcs
    StringMap Redirects; // library function redirects actually used, needed for wrapper funcs in tables
    std::vector<std::string> PostSets;
    NameIntMap NamedGlobals; // globals that we export as metadata to JS, so it can access them by name
    std::map<std::string, unsigned> IndexedFunctions; // name -> index
    FunctionTableMap FunctionTables; // sig => list of functions
    std::vector<std::string> GlobalInitializers;
    std::vector<std::string> Exports; // additional exports
    StringMap Aliases;
    BlockAddressMap BlockAddresses;
    std::map<std::string, AsmConstInfo> AsmConsts; // code => { index, list of seen sigs }
    NameSet FuncRelocatableExterns; // which externals are accessed in this function; we load them once at the beginning (avoids a potential call in a heap access, and might be faster)

    struct {
      // 0 is reserved for void type
      unsigned MetadataNum = 1;
      std::map<Metadata *, unsigned> IndexedMetadata;
      std::map<unsigned, std::string> VtableOffsets;
      std::ostringstream TypeDebugData;
      std::ostringstream TypeNameMap;
      std::ostringstream FunctionMembers;
    } cyberDWARFData;

    std::string CantValidate;
    int InvokeState; // cycles between 0, 1 after preInvoke, 2 after call, 0 again after postInvoke. hackish, no argument there.
    CodeGenOpt::Level OptLevel;
    const DataLayout *DL;
    bool StackBumped;
    int GlobalBasePadding;
    int MaxGlobalAlign;
    int StaticBump;
    const Instruction* CurrInstruction;

    #include "CallHandlers.h"

  public:
    static char ID;
    BinaryenWriter(raw_pwrite_stream &o, CodeGenOpt::Level OptLevel)
      : ModulePass(ID), Out(o), UniqueNum(0), NextFunctionIndex(0), CantValidate(""),
        InvokeState(0),
        OptLevel(OptLevel), StackBumped(false), GlobalBasePadding(0), MaxGlobalAlign(0),
        CurrInstruction(nullptr) {}

    const char *getPassName() const override { return "JavaScript backend"; }

    bool runOnModule(Module &M) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
      ModulePass::getAnalysisUsage(AU);
    }

    void printProgram(const std::string& fname, const std::string& modName );
    void printModule(const std::string& fname, const std::string& modName );
    void printFunction(const Function *F);

    LLVM_ATTRIBUTE_NORETURN void error(const std::string& msg);

    raw_pwrite_stream& nl(raw_pwrite_stream &Out, int delta = 0);

  private:
    void printCommaSeparated(const HeapData v);

    // parsing of constants has two phases: calculate, and then emit
    void parseConstant(const std::string& name, const Constant* CV, int Alignment, bool calculate);

    #define DEFAULT_MEM_ALIGN 8

    #define STACK_ALIGN 16
    #define STACK_ALIGN_BITS 128

    unsigned stackAlign(unsigned x) {
      return RoundUpToAlignment(x, STACK_ALIGN);
    }
    std::string stackAlignStr(std::string x) {
      return "((" + x + "+" + utostr(STACK_ALIGN-1) + ")&-" + utostr(STACK_ALIGN) + ")";
    }

    void ensureAligned(int Alignment, HeapData* GlobalData) {
      assert(isPowerOf2_32(Alignment) && Alignment > 0);
      while (GlobalData->size() & (Alignment-1)) GlobalData->push_back(0);
    }
    void ensureAligned(int Alignment, HeapData& GlobalData) {
      assert(isPowerOf2_32(Alignment) && Alignment > 0);
      while (GlobalData.size() & (Alignment-1)) GlobalData.push_back(0);
    }

    HeapData *allocateAddress(const std::string& Name, unsigned Alignment) {
      assert(isPowerOf2_32(Alignment) && Alignment > 0);
      HeapData* GlobalData = &GlobalDataMap[Alignment];
      ensureAligned(Alignment, GlobalData);
      GlobalAddresses[Name] = Address(GlobalData->size(), Alignment*8, false);
      return GlobalData;
    }

    void allocateZeroInitAddress(const std::string& Name, unsigned Alignment, unsigned Size) {
      assert(isPowerOf2_32(Alignment) && Alignment > 0);
      while (ZeroInitSizes.size() <= Alignment) ZeroInitSizes.push_back(0);
      GlobalAddresses[Name] = Address(ZeroInitSizes[Alignment], Alignment*8, true);
      ZeroInitSizes[Alignment] += Size;
      while (ZeroInitSizes[Alignment] & (Alignment-1)) ZeroInitSizes[Alignment]++;
    }

    // return the absolute offset of a global
    unsigned getGlobalAddress(const std::string &s) {
      GlobalAddressMap::const_iterator I = GlobalAddresses.find(s);
      if (I == GlobalAddresses.end()) {
        report_fatal_error("cannot find global address " + Twine(s));
      }
      Address a = I->second;
      int Alignment = a.Alignment/8;
      assert(AlignedHeapStarts.size() > (unsigned)Alignment);
      int Ret = a.Offset + (a.ZeroInit ? ZeroInitStarts[Alignment] : AlignedHeapStarts[Alignment]);
      assert(Alignment < (int)(a.ZeroInit ? ZeroInitStarts.size() : AlignedHeapStarts.size()));
      assert(Ret % Alignment == 0);
      return Ret;
    }
    // returns the internal offset inside the proper block: GlobalData8, 32, 64
    unsigned getRelativeGlobalAddress(const std::string &s) {
      GlobalAddressMap::const_iterator I = GlobalAddresses.find(s);
      if (I == GlobalAddresses.end()) {
        report_fatal_error("cannot find global address " + Twine(s));
      }
      Address a = I->second;
      return a.Offset;
    }
    char getFunctionSignatureLetter(Type *T) {
      if (T->isVoidTy()) return 'v';
      else if (T->isFloatingPointTy()) {
        if (PreciseF32 && T->isFloatTy()) {
          return 'f';
        } else {
          return 'd';
        }
      } else if (isa<VectorType>(T)) {
        llvm_unreachable("vector type");
      } else {
        return 'i';
      }
    }
    std::string getFunctionSignature(const FunctionType *F) {
      std::string Ret;
      Ret += getFunctionSignatureLetter(F->getReturnType());
      for (FunctionType::param_iterator AI = F->param_begin(),
             AE = F->param_end(); AI != AE; ++AI) {
        Ret += getFunctionSignatureLetter(*AI);
      }
      return Ret;
    }
    FunctionTable& ensureFunctionTable(const FunctionType *FT) {
      FunctionTable &Table = FunctionTables[getFunctionSignature(FT)];
      unsigned MinSize = ReservedFunctionPointers ? 2*(ReservedFunctionPointers+1) : 1; // each reserved slot must be 2-aligned
      while (Table.size() < MinSize) Table.push_back("0");
      return Table;
    }
    unsigned getFunctionIndex(const Function *F) {
      const std::string &Name = getSimpleName(F);
      if (IndexedFunctions.find(Name) != IndexedFunctions.end()) return IndexedFunctions[Name];
      std::string Sig = getFunctionSignature(F->getFunctionType());
      FunctionTable& Table = ensureFunctionTable(F->getFunctionType());
      if (NoAliasingFunctionPointers) {
        while (Table.size() < NextFunctionIndex) Table.push_back("0");
      }
      // XXX this is wrong, it's always 1. but, that's fine in the ARM-like ABI
      // we have which allows unaligned func the one risk is if someone forces a
      // function to be aligned, and relies on that. Could do F->getAlignment()
      // instead.
      unsigned Alignment = 1;
      while (Table.size() % Alignment) Table.push_back("0");
      unsigned Index = Table.size();
      Table.push_back(Name);
      IndexedFunctions[Name] = Index;
      if (NoAliasingFunctionPointers) {
        NextFunctionIndex = Index+1;
      }

      // invoke the callHandler for this, if there is one. the function may only be indexed but never called directly, and we may need to do things in the handler
      CallHandlerMap::const_iterator CH = CallHandlers.find(Name);
      if (CH != CallHandlers.end()) {
        (this->*(CH->second))(nullptr, Name, -1);
      }

      return Index;
    }

    unsigned getBlockAddress(const Function *F, const BasicBlock *BB) {
      BlockIndexMap& Blocks = BlockAddresses[F];
      if (Blocks.find(BB) == Blocks.end()) {
        Blocks[BB] = Blocks.size(); // block addresses start from 0
      }
      return Blocks[BB];
    }

    unsigned getBlockAddress(const BlockAddress *BA) {
      return getBlockAddress(BA->getFunction(), BA->getBasicBlock());
    }

    const Value *resolveFully(const Value *V) {
      bool More = true;
      while (More) {
        More = false;
        if (const GlobalAlias *GA = dyn_cast<GlobalAlias>(V)) {
          V = GA->getAliasee();
          More = true;
        }
        if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(V)) {
          V = CE->getOperand(0); // ignore bitcasts
          More = true;
        }
      }
      return V;
    }

    std::string relocateFunctionPointer(std::string FP) {
      return Relocatable ? "(fb + (" + FP + ") | 0)" : FP;
    }

    std::string relocateGlobal(std::string G) {
      return Relocatable ? "(gb + (" + G + ") | 0)" : G;
    }

    unsigned getIDForMetadata(Metadata *MD) {
      if (cyberDWARFData.IndexedMetadata.find(MD) == cyberDWARFData.IndexedMetadata.end()) {
        cyberDWARFData.IndexedMetadata[MD] = cyberDWARFData.MetadataNum++;
      }
      return cyberDWARFData.IndexedMetadata[MD];
    }

    // Return a constant we are about to write into a global as a numeric offset. If the
    // value is not known at compile time, emit a postSet to that location.
    unsigned getConstAsOffset(const Value *V, unsigned AbsoluteTarget) {
      V = resolveFully(V);
      if (const Function *F = dyn_cast<const Function>(V)) {
        if (Relocatable) {
          PostSets.push_back("\n HEAP32[" + relocateGlobal(utostr(AbsoluteTarget)) + " >> 2] = " + relocateFunctionPointer(utostr(getFunctionIndex(F))) + ';');
          return 0; // emit zero in there for now, until the postSet
        }
        return getFunctionIndex(F);
      } else if (const BlockAddress *BA = dyn_cast<const BlockAddress>(V)) {
        return getBlockAddress(BA);
      } else {
        if (const GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
          if (!GV->hasInitializer()) {
            // We don't have a constant to emit here, so we must emit a postSet
            // All postsets are of external values, so they are pointers, hence 32-bit
            std::string Name = getOpName(V);
            Externals.insert(Name);
            if (Relocatable) {
              PostSets.push_back("\n temp = g$" + Name + "() | 0;"); // we access linked externs through calls, and must do so to a temp for heap growth validation
              // see later down about adding to an offset
              std::string access = "HEAP32[" + relocateGlobal(utostr(AbsoluteTarget)) + " >> 2]";
              PostSets.push_back("\n " + access + " = (" + access + " | 0) + temp;");
            } else {
              PostSets.push_back("\n HEAP32[" + relocateGlobal(utostr(AbsoluteTarget)) + " >> 2] = " + Name + ';');
            }
            return 0; // emit zero in there for now, until the postSet
          } else if (Relocatable) {
            // this is one of our globals, but we must relocate it. we return zero, but the caller may store
            // an added offset, which we read at postSet time; in other words, we just add to that offset
            std::string access = "HEAP32[" + relocateGlobal(utostr(AbsoluteTarget)) + " >> 2]";
            PostSets.push_back("\n " + access + " = (" + access + " | 0) + " + relocateGlobal(utostr(getGlobalAddress(V->getName().str()))) + ';');
            return 0; // emit zero in there for now, until the postSet
          }
        }
        assert(!Relocatable);
        return getGlobalAddress(V->getName().str());
      }
    }

    // Transform the string input into emscripten_asm_const_*(str, args1, arg2)
    // into an id. We emit a map of id => string contents, and emscripten
    // wraps it up so that calling that id calls that function.
    unsigned getAsmConstId(const Value *V, std::string Sig) {
      V = resolveFully(V);
      const Constant *CI = cast<GlobalVariable>(V)->getInitializer();
      std::string code;
      if (isa<ConstantAggregateZero>(CI)) {
        code = " ";
      } else {
        const ConstantDataSequential *CDS = cast<ConstantDataSequential>(CI);
        code = CDS->getAsString();
        // replace newlines quotes with escaped newlines
        size_t curr = 0;
        while ((curr = code.find("\\n", curr)) != std::string::npos) {
          code = code.replace(curr, 2, "\\\\n");
          curr += 3; // skip this one
        }
        // replace double quotes with escaped single quotes
        curr = 0;
        while ((curr = code.find('"', curr)) != std::string::npos) {
          if (curr == 0 || code[curr-1] != '\\') {
            code = code.replace(curr, 1, "\\" "\"");
            curr += 2; // skip this one
          } else { // already escaped, escape the slash as well
            code = code.replace(curr, 1, "\\" "\\" "\"");
            curr += 3; // skip this one
          }
        }
      }
      unsigned Id;
      if (AsmConsts.count(code) > 0) {
        auto& Info = AsmConsts[code];
        Id = Info.Id;
        Info.Sigs.insert(Sig);
      } else {
        AsmConstInfo Info;
        Info.Id = Id = AsmConsts.size();
        Info.Sigs.insert(Sig);
        AsmConsts[code] = Info;
      }
      return Id;
    }

    // Test whether the given value is known to be an absolute value or one we turn into an absolute value
    bool isAbsolute(const Value *P) {
      if (const IntToPtrInst *ITP = dyn_cast<IntToPtrInst>(P)) {
        return isa<ConstantInt>(ITP->getOperand(0));
      }
      if (isa<ConstantPointerNull>(P) || isa<UndefValue>(P)) {
        return true;
      }
      return false;
    }

    std::string ensureCast(std::string S, Type *T, AsmCast sign) {
      if (sign & ASM_MUST_CAST) return getCast(S, T);
      return S;
    }

    static void emitDebugInfo(raw_ostream& Code, const Instruction *I) {
      auto &Loc = I->getDebugLoc();
      if (Loc) {
        unsigned Line = Loc.getLine();
        auto *Scope = cast_or_null<DIScope>(Loc.getScope());
        if (Scope) {
          StringRef File = Scope->getFilename();
          if (Line > 0)
            Code << " //@line " << utostr(Line) << " \"" << (File.size() > 0 ? File.str() : "?") << "\"";
        }
      }
    }

    std::string ftostr(const ConstantFP *CFP, AsmCast sign) {
      const APFloat &flt = CFP->getValueAPF();

      // Emscripten has its own spellings for infinity and NaN.
      if (flt.getCategory() == APFloat::fcInfinity) return ensureCast(flt.isNegative() ? "-inf" : "inf", CFP->getType(), sign);
      else if (flt.getCategory() == APFloat::fcNaN) {
        APInt i = flt.bitcastToAPInt();
        if ((i.getBitWidth() == 32 && i != APInt(32, 0x7FC00000)) || (i.getBitWidth() == 64 && i != APInt(64, 0x7FF8000000000000ULL))) {
          // If we reach here, things have already gone bad, and JS engine NaN canonicalization will kill the bits in the float. However can't make
          // this a build error in order to not break people's existing code, so issue a warning instead.
          if (WarnOnNoncanonicalNans) {
            errs() << "emcc: warning: cannot represent a NaN literal '" << CFP << "' with custom bit pattern in NaN-canonicalizing JS engines (e.g. Firefox and Safari) without erasing bits!\n";
            if (CurrInstruction) {
              errs() << "  in " << *CurrInstruction << " in " << CurrInstruction->getParent()->getParent()->getName() << "() ";
              emitDebugInfo(errs(), CurrInstruction);
              errs() << '\n';
            }
          }
        }
        return ensureCast("nan", CFP->getType(), sign);
      }

      // Request 9 or 17 digits, aka FLT_DECIMAL_DIG or DBL_DECIMAL_DIG (our
      // long double is the the same as our double), to avoid rounding errors.
      SmallString<29> Str;
      flt.toString(Str, PreciseF32 && CFP->getType()->isFloatTy() ? 9 : 17);

      // asm.js considers literals to be floating-point literals when they contain a
      // dot, however our output may be processed by UglifyJS, which doesn't
      // currently preserve dots in all cases. Mark floating-point literals with
      // unary plus to force them to floating-point.
      if (APFloat(flt).roundToIntegral(APFloat::rmNearestTiesToEven) == APFloat::opOK) {
        return '+' + Str.str().str();
      }

      return Str.str().str();
    }

    std::string getPtrLoad(const Value* Ptr);

    /// Given a pointer to memory, returns the HEAP object and index to that object that is used to access that memory.
    /// @param Ptr [in] The heap object.
    /// @param HeapName [out] Receives the name of the HEAP object used to perform the memory acess.
    /// @return The index to the heap HeapName for the memory access.
    std::string getHeapNameAndIndex(const Value *Ptr, const char **HeapName);

    // Like getHeapNameAndIndex(), but uses the given memory operation size and whether it is an Integer instead of the type of Ptr.
    std::string getHeapNameAndIndex(const Value *Ptr, const char **HeapName, unsigned Bytes, bool Integer);

    /// Like getHeapNameAndIndex(), but for global variables only.
    std::string getHeapNameAndIndexToGlobal(const GlobalVariable *GV, unsigned Bytes, bool Integer, const char **HeapName);

    /// Like getHeapNameAndIndex(), but for pointers represented in string expression form.
    static std::string getHeapNameAndIndexToPtr(const std::string& Ptr, unsigned Bytes, bool Integer, const char **HeapName);

    std::string getShiftedPtr(const Value *Ptr, unsigned Bytes);

    /// Returns a string expression for accessing the given memory address.
    std::string getPtrUse(const Value* Ptr);

    /// Like getPtrUse(), but for pointers represented in string expression form.
    static std::string getHeapAccess(const std::string& Name, unsigned Bytes, bool Integer=true);

    std::string getConstant(const Constant*, AsmCast sign=ASM_SIGNED);
    std::string getValueAsStr(const Value*, AsmCast sign=ASM_SIGNED);
    std::string getValueAsCastStr(const Value*, AsmCast sign=ASM_SIGNED);
    std::string getValueAsParenStr(const Value*);
    std::string getValueAsCastParenStr(const Value*, AsmCast sign=ASM_SIGNED);

    const std::string &getSimpleName(const Value* val);

    std::string getPhiCode(const BasicBlock *From, const BasicBlock *To);

    void printAttributes(const AttributeSet &PAL, const std::string &name);
    void printType(Type* Ty);
    void printTypes(const Module* M);

    std::string getAdHocAssign(const StringRef &, Type *);
    std::string getAssign(const Instruction *I);
    std::string getAssignIfNeeded(const Value *V);
    std::string getCast(const StringRef &, Type *, AsmCast sign=ASM_SIGNED);
    std::string getParenCast(const StringRef &, Type *, AsmCast sign=ASM_SIGNED);
    std::string getDoubleToInt(const StringRef &);
    std::string getIMul(const Value *, const Value *);
    std::string getLoad(const Instruction *I, const Value *P, Type *T, unsigned Alignment, char sep=';');
    std::string getStore(const Instruction *I, const Value *P, Type *T, const std::string& VS, unsigned Alignment, char sep=';');
    std::string getStackBump(unsigned Size);
    std::string getStackBump(const std::string &Size);

    void addBlock(const BasicBlock *BB, RelooperRef R, LLVMToRelooperMap& LLVMToRelooper);
    void printFunctionBody(const Function *F);
    BinaryenExpressionRef generateExpression(const User *I);

    // debug information
    std::string generateDebugRecordForVar(Metadata *MD);
    void buildCyberDWARFData();

    std::string getOpName(const Value*);

    void processConstants();

    // nativization

    typedef std::set<const Value*> NativizedVarsMap;
    NativizedVarsMap NativizedVars;

    void calculateNativizedVars(const Function *F);

    // special analyses

    bool canReloop(const Function *F);

    // main entry point

    void printModuleBody();
  };
} // end anonymous namespace.

raw_pwrite_stream &BinaryenWriter::nl(raw_pwrite_stream &Out, int delta) {
  Out << '\n';
  return Out;
}

static inline char halfCharToHex(unsigned char half) {
  assert(half <= 15);
  if (half <= 9) {
    return '0' + half;
  } else {
    return 'A' + half - 10;
  }
}

static inline void sanitizeGlobal(std::string& str) {
  // Global names are prefixed with "_" to prevent them from colliding with
  // names of things in normal JS.
  str = "_" + str;

  // functions and globals should already be in C-style format,
  // in addition to . for llvm intrinsics and possibly $ and so forth.
  // There is a risk of collisions here, we just lower all these
  // invalid characters to _, but this should not happen in practice.
  // TODO: in debug mode, check for such collisions.
  size_t OriginalSize = str.size();
  for (size_t i = 1; i < OriginalSize; ++i) {
    unsigned char c = str[i];
    if (!isalnum(c) && c != '_') str[i] = '_';
  }
}

static inline void sanitizeLocal(std::string& str) {
  // Local names are prefixed with "$" to prevent them from colliding with
  // global names.
  str = "$" + str;

  // We need to convert every string that is not a valid JS identifier into
  // a valid one, without collisions - we cannot turn "x.a" into "x_a" while
  // also leaving "x_a" as is, for example.
  //
  // We leave valid characters 0-9a-zA-Z and _ unchanged. Anything else
  // we replace with $ and append a hex representation of that value,
  // so for example x.a turns into x$a2e, x..a turns into x$$a2e2e.
  //
  // As an optimization, we replace . with $ without appending anything,
  // unless there is another illegal character. The reason is that . is
  // a common illegal character, and we want to avoid resizing strings
  // for perf reasons, and we If we do see we need to append something, then
  // for . we just append Z (one character, instead of the hex code).
  //

  size_t OriginalSize = str.size();
  int Queued = 0;
  for (size_t i = 1; i < OriginalSize; ++i) {
    unsigned char c = str[i];
    if (!isalnum(c) && c != '_') {
      str[i] = '$';
      if (c == '.') {
        Queued++;
      } else {
        size_t s = str.size();
        str.resize(s+2+Queued);
        for (int i = 0; i < Queued; i++) {
          str[s++] = 'Z';
        }
        Queued = 0;
        str[s] = halfCharToHex(c >> 4);
        str[s+1] = halfCharToHex(c & 0xf);
      }
    }
  }
}

static inline std::string ensureFloat(const std::string &S, Type *T) {
  if (PreciseF32 && T->isFloatTy()) {
    return "Math_fround(" + S + ')';
  }
  return S;
}

static inline std::string ensureFloat(const std::string &value, bool wrap) {
  if (wrap) {
    return "Math_fround(" + value + ')';
  }
  return value;
}

void BinaryenWriter::error(const std::string& msg) {
  report_fatal_error(msg);
}

std::string BinaryenWriter::getPhiCode(const BasicBlock *From, const BasicBlock *To) {
  // FIXME this is all quite inefficient, and also done once per incoming to each phi

  // Find the phis, and generate assignments and dependencies
  std::set<std::string> PhiVars;
  for (BasicBlock::const_iterator I = To->begin(), E = To->end();
       I != E; ++I) {
    const PHINode* P = dyn_cast<PHINode>(I);
    if (!P) break;
    PhiVars.insert(getSimpleName(P));
  }
  typedef std::map<std::string, std::string> StringMap;
  StringMap assigns; // variable -> assign statement
  std::map<std::string, const Value*> values; // variable -> Value
  StringMap deps; // variable -> dependency
  StringMap undeps; // reverse: dependency -> variable
  for (BasicBlock::const_iterator I = To->begin(), E = To->end();
       I != E; ++I) {
    const PHINode* P = dyn_cast<PHINode>(I);
    if (!P) break;
    int index = P->getBasicBlockIndex(From);
    if (index < 0) continue;
    // we found it
    const std::string &name = getSimpleName(P);
    assigns[name] = getAssign(P);
    // Get the operand, and strip pointer casts, since normal expression
    // translation also strips pointer casts, and we want to see the same
    // thing so that we can detect any resulting dependencies.
    const Value *V = P->getIncomingValue(index)->stripPointerCasts();
    values[name] = V;
    std::string vname = getValueAsStr(V);
    if (const Instruction *VI = dyn_cast<const Instruction>(V)) {
      if (VI->getParent() == To && PhiVars.find(vname) != PhiVars.end()) {
        deps[name] = vname;
        undeps[vname] = name;
      }
    }
  }
  // Emit assignments+values, taking into account dependencies, and breaking cycles
  std::string pre = "", post = "";
  while (assigns.size() > 0) {
    bool emitted = false;
    for (StringMap::iterator I = assigns.begin(); I != assigns.end();) {
      StringMap::iterator last = I;
      std::string curr = last->first;
      const Value *V = values[curr];
      std::string CV = getValueAsStr(V);
      I++; // advance now, as we may erase
      // if we have no dependencies, or we found none to emit and are at the end (so there is a cycle), emit
      StringMap::const_iterator dep = deps.find(curr);
      if (dep == deps.end() || (!emitted && I == assigns.end())) {
        if (dep != deps.end()) {
          // break a cycle
          std::string depString = dep->second;
          std::string temp = curr + "$phi";
          pre += getAdHocAssign(temp, V->getType()) + CV + ';';
          CV = temp;
          deps.erase(curr);
          undeps.erase(depString);
        }
        post += assigns[curr] + CV + ';';
        assigns.erase(last);
        emitted = true;
      }
    }
  }
  return pre + post;
}

const std::string &BinaryenWriter::getSimpleName(const Value* val) {
  ValueMap::const_iterator I = ValueNames.find(val);
  if (I != ValueNames.end() && I->first == val)
    return I->second;

  // If this is an alloca we've replaced with another, use the other name.
  if (const AllocaInst *AI = dyn_cast<AllocaInst>(val)) {
    if (AI->isStaticAlloca()) {
      const AllocaInst *Rep = Allocas.getRepresentative(AI);
      if (Rep != AI) {
        return getSimpleName(Rep);
      }
    }
  }

  std::string name;
  if (val->hasName()) {
    name = val->getName().str();
  } else {
    name = utostr(UniqueNum++);
  }

  if (isa<Constant>(val)) {
    sanitizeGlobal(name);
  } else {
    sanitizeLocal(name);
  }

  return ValueNames[val] = name;
}

std::string BinaryenWriter::getAdHocAssign(const StringRef &s, Type *t) {
  UsedVars[s] = t;
  return (s + " = ").str();
}

std::string BinaryenWriter::getAssign(const Instruction *I) {
  return getAdHocAssign(getSimpleName(I), I->getType());
}

std::string BinaryenWriter::getAssignIfNeeded(const Value *V) {
  if (const Instruction *I = dyn_cast<Instruction>(V)) {
    if (!I->use_empty()) return getAssign(I);
  }
  return std::string();
}

std::string BinaryenWriter::getCast(const StringRef &s, Type *t, AsmCast sign) {
  switch (t->getTypeID()) {
    default: {
      errs() << *t << "\n";
      assert(false && "Unsupported type");
    }
    case Type::FloatTyID: {
      if (PreciseF32 && !(sign & ASM_FFI_OUT)) {
        if (sign & ASM_FFI_IN) {
          return ("Math_fround(+(" + s + "))").str();
        } else {
          return ("Math_fround(" + s + ")").str();
        }
      }
      // otherwise fall through to double
    }
    case Type::DoubleTyID: return ("+" + s).str();
    case Type::IntegerTyID: {
      // fall through to the end for nonspecific
      switch (t->getIntegerBitWidth()) {
        case 1:  if (!(sign & ASM_NONSPECIFIC)) return sign == ASM_UNSIGNED ? (s + "&1").str()     : (s + "<<31>>31").str();
        case 8:  if (!(sign & ASM_NONSPECIFIC)) return sign == ASM_UNSIGNED ? (s + "&255").str()   : (s + "<<24>>24").str();
        case 16: if (!(sign & ASM_NONSPECIFIC)) return sign == ASM_UNSIGNED ? (s + "&65535").str() : (s + "<<16>>16").str();
        case 32: return (sign == ASM_SIGNED || (sign & ASM_NONSPECIFIC) ? s + "|0" : s + ">>>0").str();
        default: llvm_unreachable("Unsupported integer cast bitwidth");
      }
    }
    case Type::PointerTyID:
      return (sign == ASM_SIGNED || (sign & ASM_NONSPECIFIC) ? s + "|0" : s + ">>>0").str();
  }
}

std::string BinaryenWriter::getParenCast(const StringRef &s, Type *t, AsmCast sign) {
  return getCast(("(" + s + ")").str(), t, sign);
}

std::string BinaryenWriter::getDoubleToInt(const StringRef &s) {
  return ("~~(" + s + ")").str();
}

std::string BinaryenWriter::getIMul(const Value *V1, const Value *V2) {
  const ConstantInt *CI = nullptr;
  const Value *Other = nullptr;
  if ((CI = dyn_cast<ConstantInt>(V1))) {
    Other = V2;
  } else if ((CI = dyn_cast<ConstantInt>(V2))) {
    Other = V1;
  }
  // we ignore optimizing the case of multiplying two constants - optimizer would have removed those
  if (CI) {
    std::string OtherStr = getValueAsStr(Other);
    unsigned C = CI->getZExtValue();
    if (C == 0) return "0";
    if (C == 1) return OtherStr;
    unsigned Orig = C, Shifts = 0;
    while (C) {
      if ((C & 1) && (C != 1)) break; // not power of 2
      C >>= 1;
      Shifts++;
      if (C == 0) return OtherStr + "<<" + utostr(Shifts-1); // power of 2, emit shift
    }
    if (Orig < (1<<20)) return "(" + OtherStr + "*" + utostr(Orig) + ")|0"; // small enough, avoid imul
  }
  return "Math_imul(" + getValueAsStr(V1) + ", " + getValueAsStr(V2) + ")|0"; // unknown or too large, emit imul
}

static inline const char *getHeapName(int Bytes, int Integer)
{
  switch (Bytes) {
    default: llvm_unreachable("Unsupported type");
    case 8: return "HEAPF64";
    case 4: return Integer ? "HEAP32" : "HEAPF32";
    case 2: return "HEAP16";
    case 1: return "HEAP8";
  }
}

static inline int getHeapShift(int Bytes)
{
  switch (Bytes) {
    default: llvm_unreachable("Unsupported type");
    case 8: return 3;
    case 4: return 2;
    case 2: return 1;
    case 1: return 0;
  }
}

static inline const char *getHeapShiftStr(int Bytes)
{
  switch (Bytes) {
    default: llvm_unreachable("Unsupported type");
    case 8: return ">>3";
    case 4: return ">>2";
    case 2: return ">>1";
    case 1: return ">>0";
  }
}

std::string BinaryenWriter::getHeapNameAndIndexToGlobal(const GlobalVariable *GV, unsigned Bytes, bool Integer, const char **HeapName)
{
  unsigned Addr = getGlobalAddress(GV->getName().str());
  *HeapName = getHeapName(Bytes, Integer);
  if (!Relocatable) {
    return utostr(Addr >> getHeapShift(Bytes));
  } else {
    return relocateGlobal(utostr(Addr)) + getHeapShiftStr(Bytes);
  }
}

std::string BinaryenWriter::getHeapNameAndIndexToPtr(const std::string& Ptr, unsigned Bytes, bool Integer, const char **HeapName)
{
  *HeapName = getHeapName(Bytes, Integer);
  return Ptr + getHeapShiftStr(Bytes);
}

std::string BinaryenWriter::getHeapNameAndIndex(const Value *Ptr, const char **HeapName, unsigned Bytes, bool Integer)
{
  const GlobalVariable *GV;
  if ((GV = dyn_cast<GlobalVariable>(Ptr->stripPointerCasts())) && GV->hasInitializer()) {
    // Note that we use the type of the pointer, as it might be a bitcast of the underlying global. We need the right type.
    return getHeapNameAndIndexToGlobal(GV, Bytes, Integer, HeapName);
  } else {
    return getHeapNameAndIndexToPtr(getValueAsStr(Ptr), Bytes, Integer, HeapName);
  }
}

std::string BinaryenWriter::getHeapNameAndIndex(const Value *Ptr, const char **HeapName)
{
  Type *t = cast<PointerType>(Ptr->getType())->getElementType();
  return getHeapNameAndIndex(Ptr, HeapName, DL->getTypeAllocSize(t), t->isIntegerTy() || t->isPointerTy());
}

static const char *heapNameToAtomicTypeName(const char *HeapName)
{
  if (!strcmp(HeapName, "HEAPF32")) return "f32";
  if (!strcmp(HeapName, "HEAPF64")) return "f64";
  return "";
}

std::string BinaryenWriter::getLoad(const Instruction *I, const Value *P, Type *T, unsigned Alignment, char sep) {
  std::string Assign = getAssign(I);
  unsigned Bytes = DL->getTypeAllocSize(T);
  std::string text;
  if (Bytes <= Alignment || Alignment == 0) {
    if (EnablePthreads && cast<LoadInst>(I)->isVolatile()) {
      const char *HeapName;
      std::string Index = getHeapNameAndIndex(P, &HeapName);
      if (!strcmp(HeapName, "HEAPF32") || !strcmp(HeapName, "HEAPF64")) {
        bool fround = PreciseF32 && !strcmp(HeapName, "HEAPF32");
        // TODO: If https://bugzilla.mozilla.org/show_bug.cgi?id=1131613 and https://bugzilla.mozilla.org/show_bug.cgi?id=1131624 are
        // implemented, we could remove the emulation, but until then we must emulate manually.
        text = Assign + (fround ? "Math_fround(" : "+") + "_emscripten_atomic_load_" + heapNameToAtomicTypeName(HeapName) + "(" + getValueAsStr(P) + (fround ? "))" : ")");
      } else {
        text = Assign + "(Atomics_load(" + HeapName + ',' + Index + ")|0)";
      }
    } else {
      text = Assign + getPtrLoad(P);
    }
    if (isAbsolute(P)) {
      // loads from an absolute constants are either intentional segfaults (int x = *((int*)0)), or code problems
      text += "; abort() /* segfault, load from absolute addr */";
    }
  } else {
    // unaligned in some manner

    if (EnablePthreads && cast<LoadInst>(I)->isVolatile()) {
      errs() << "emcc: warning: unable to implement unaligned volatile load as atomic in " << I->getParent()->getParent()->getName() << ":" << *I << " | ";
      emitDebugInfo(errs(), I);
      errs() << "\n";
    }

    if (WarnOnUnaligned) {
      errs() << "emcc: warning: unaligned load in  " << I->getParent()->getParent()->getName() << ":" << *I << " | ";
      emitDebugInfo(errs(), I);
      errs() << "\n";
    }
    std::string PS = getValueAsStr(P);
    switch (Bytes) {
      case 8: {
        switch (Alignment) {
          case 4: {
            text = "HEAP32[tempDoublePtr>>2]=HEAP32[" + PS + ">>2]" + sep +
                    "HEAP32[tempDoublePtr+4>>2]=HEAP32[" + PS + "+4>>2]";
            break;
          }
          case 2: {
            text = "HEAP16[tempDoublePtr>>1]=HEAP16[" + PS + ">>1]" + sep +
                   "HEAP16[tempDoublePtr+2>>1]=HEAP16[" + PS + "+2>>1]" + sep +
                   "HEAP16[tempDoublePtr+4>>1]=HEAP16[" + PS + "+4>>1]" + sep +
                   "HEAP16[tempDoublePtr+6>>1]=HEAP16[" + PS + "+6>>1]";
            break;
          }
          case 1: {
            text = "HEAP8[tempDoublePtr>>0]=HEAP8[" + PS + ">>0]" + sep +
                   "HEAP8[tempDoublePtr+1>>0]=HEAP8[" + PS + "+1>>0]" + sep +
                   "HEAP8[tempDoublePtr+2>>0]=HEAP8[" + PS + "+2>>0]" + sep +
                   "HEAP8[tempDoublePtr+3>>0]=HEAP8[" + PS + "+3>>0]" + sep +
                   "HEAP8[tempDoublePtr+4>>0]=HEAP8[" + PS + "+4>>0]" + sep +
                   "HEAP8[tempDoublePtr+5>>0]=HEAP8[" + PS + "+5>>0]" + sep +
                   "HEAP8[tempDoublePtr+6>>0]=HEAP8[" + PS + "+6>>0]" + sep +
                   "HEAP8[tempDoublePtr+7>>0]=HEAP8[" + PS + "+7>>0]";
            break;
          }
          default: assert(0 && "bad 8 store");
        }
        text += sep + Assign + "+HEAPF64[tempDoublePtr>>3]";
        break;
      }
      case 4: {
        if (T->isIntegerTy() || T->isPointerTy()) {
          switch (Alignment) {
            case 2: {
              text = Assign + "HEAPU16[" + PS + ">>1]|" +
                             "(HEAPU16[" + PS + "+2>>1]<<16)";
              break;
            }
            case 1: {
              text = Assign + "HEAPU8[" + PS + ">>0]|" +
                             "(HEAPU8[" + PS + "+1>>0]<<8)|" +
                             "(HEAPU8[" + PS + "+2>>0]<<16)|" +
                             "(HEAPU8[" + PS + "+3>>0]<<24)";
              break;
            }
            default: assert(0 && "bad 4i store");
          }
        } else { // float
          assert(T->isFloatingPointTy());
          switch (Alignment) {
            case 2: {
              text = "HEAP16[tempDoublePtr>>1]=HEAP16[" + PS + ">>1]" + sep +
                     "HEAP16[tempDoublePtr+2>>1]=HEAP16[" + PS + "+2>>1]";
              break;
            }
            case 1: {
              text = "HEAP8[tempDoublePtr>>0]=HEAP8[" + PS + ">>0]" + sep +
                     "HEAP8[tempDoublePtr+1>>0]=HEAP8[" + PS + "+1>>0]" + sep +
                     "HEAP8[tempDoublePtr+2>>0]=HEAP8[" + PS + "+2>>0]" + sep +
                     "HEAP8[tempDoublePtr+3>>0]=HEAP8[" + PS + "+3>>0]";
              break;
            }
            default: assert(0 && "bad 4f store");
          }
          text += sep + Assign + getCast("HEAPF32[tempDoublePtr>>2]", Type::getFloatTy(TheModule->getContext()));
        }
        break;
      }
      case 2: {
        text = Assign + "HEAPU8[" + PS + ">>0]|" +
                       "(HEAPU8[" + PS + "+1>>0]<<8)";
        break;
      }
      default: assert(0 && "bad store");
    }
  }
  return text;
}

std::string BinaryenWriter::getStore(const Instruction *I, const Value *P, Type *T, const std::string& VS, unsigned Alignment, char sep) {
  assert(sep == ';'); // FIXME when we need that
  unsigned Bytes = DL->getTypeAllocSize(T);
  std::string text;
  if (Bytes <= Alignment || Alignment == 0) {
    if (EnablePthreads && cast<StoreInst>(I)->isVolatile()) {
      const char *HeapName;
      std::string Index = getHeapNameAndIndex(P, &HeapName);
      if (!strcmp(HeapName, "HEAPF32") || !strcmp(HeapName, "HEAPF64")) {
        // TODO: If https://bugzilla.mozilla.org/show_bug.cgi?id=1131613 and https://bugzilla.mozilla.org/show_bug.cgi?id=1131624 are
        // implemented, we could remove the emulation, but until then we must emulate manually.
        text = std::string("_emscripten_atomic_store_") + heapNameToAtomicTypeName(HeapName) + "(" + getValueAsStr(P) + ',' + VS + ')';
        if (PreciseF32 && !strcmp(HeapName, "HEAPF32"))
          text = "Math_fround(" + text + ")";
        else
          text = "+" + text;
      } else {
        text = std::string("Atomics_store(") + HeapName + ',' + Index + ',' + VS + ")|0";
      }
    } else {
      text = getPtrUse(P) + " = " + VS;
    }
    if (Alignment == 536870912) text += "; abort() /* segfault */";
  } else {
    // unaligned in some manner

    if (EnablePthreads && cast<StoreInst>(I)->isVolatile()) {
      errs() << "emcc: warning: unable to implement unaligned volatile store as atomic in " << I->getParent()->getParent()->getName() << ":" << *I << " | ";
      emitDebugInfo(errs(), I);
      errs() << "\n";
    }

    if (WarnOnUnaligned) {
      errs() << "emcc: warning: unaligned store in " << I->getParent()->getParent()->getName() << ":" << *I << " | ";
      emitDebugInfo(errs(), I);
      errs() << "\n";
    }
    std::string PS = getValueAsStr(P);
    switch (Bytes) {
      case 8: {
        text = "HEAPF64[tempDoublePtr>>3]=" + VS + ';';
        switch (Alignment) {
          case 4: {
            text += "HEAP32[" + PS + ">>2]=HEAP32[tempDoublePtr>>2];" +
                    "HEAP32[" + PS + "+4>>2]=HEAP32[tempDoublePtr+4>>2]";
            break;
          }
          case 2: {
            text += "HEAP16[" + PS + ">>1]=HEAP16[tempDoublePtr>>1];" +
                    "HEAP16[" + PS + "+2>>1]=HEAP16[tempDoublePtr+2>>1];" +
                    "HEAP16[" + PS + "+4>>1]=HEAP16[tempDoublePtr+4>>1];" +
                    "HEAP16[" + PS + "+6>>1]=HEAP16[tempDoublePtr+6>>1]";
            break;
          }
          case 1: {
            text += "HEAP8[" + PS + ">>0]=HEAP8[tempDoublePtr>>0];" +
                    "HEAP8[" + PS + "+1>>0]=HEAP8[tempDoublePtr+1>>0];" +
                    "HEAP8[" + PS + "+2>>0]=HEAP8[tempDoublePtr+2>>0];" +
                    "HEAP8[" + PS + "+3>>0]=HEAP8[tempDoublePtr+3>>0];" +
                    "HEAP8[" + PS + "+4>>0]=HEAP8[tempDoublePtr+4>>0];" +
                    "HEAP8[" + PS + "+5>>0]=HEAP8[tempDoublePtr+5>>0];" +
                    "HEAP8[" + PS + "+6>>0]=HEAP8[tempDoublePtr+6>>0];" +
                    "HEAP8[" + PS + "+7>>0]=HEAP8[tempDoublePtr+7>>0]";
            break;
          }
          default: assert(0 && "bad 8 store");
        }
        break;
      }
      case 4: {
        if (T->isIntegerTy() || T->isPointerTy()) {
          switch (Alignment) {
            case 2: {
              text = "HEAP16[" + PS + ">>1]=" + VS + "&65535;" +
                     "HEAP16[" + PS + "+2>>1]=" + VS + ">>>16";
              break;
            }
            case 1: {
              text = "HEAP8[" + PS + ">>0]=" + VS + "&255;" +
                     "HEAP8[" + PS + "+1>>0]=(" + VS + ">>8)&255;" +
                     "HEAP8[" + PS + "+2>>0]=(" + VS + ">>16)&255;" +
                     "HEAP8[" + PS + "+3>>0]=" + VS + ">>24";
              break;
            }
            default: assert(0 && "bad 4i store");
          }
        } else { // float
          assert(T->isFloatingPointTy());
          text = "HEAPF32[tempDoublePtr>>2]=" + VS + ';';
          switch (Alignment) {
            case 2: {
              text += "HEAP16[" + PS + ">>1]=HEAP16[tempDoublePtr>>1];" +
                      "HEAP16[" + PS + "+2>>1]=HEAP16[tempDoublePtr+2>>1]";
              break;
            }
            case 1: {
              text += "HEAP8[" + PS + ">>0]=HEAP8[tempDoublePtr>>0];" +
                      "HEAP8[" + PS + "+1>>0]=HEAP8[tempDoublePtr+1>>0];" +
                      "HEAP8[" + PS + "+2>>0]=HEAP8[tempDoublePtr+2>>0];" +
                      "HEAP8[" + PS + "+3>>0]=HEAP8[tempDoublePtr+3>>0]";
              break;
            }
            default: assert(0 && "bad 4f store");
          }
        }
        break;
      }
      case 2: {
        text = "HEAP8[" + PS + ">>0]=" + VS + "&255;" +
               "HEAP8[" + PS + "+1>>0]=" + VS + ">>8";
        break;
      }
      default: assert(0 && "bad store");
    }
  }
  return text;
}

std::string BinaryenWriter::getStackBump(unsigned Size) {
  return getStackBump(utostr(Size));
}

std::string BinaryenWriter::getStackBump(const std::string &Size) {
  std::string ret = "STACKTOP = STACKTOP + " + Size + "|0;";
  if (EmscriptenAssertions) {
    ret += " if ((STACKTOP|0) >= (STACK_MAX|0)) abort();";
  }
  return ret;
}

std::string BinaryenWriter::getOpName(const Value* V) { // TODO: remove this
  return getSimpleName(V);
}

std::string BinaryenWriter::getPtrLoad(const Value* Ptr) {
  Type *t = cast<PointerType>(Ptr->getType())->getElementType();
  return getCast(getPtrUse(Ptr), t, ASM_NONSPECIFIC);
}

std::string BinaryenWriter::getHeapAccess(const std::string& Name, unsigned Bytes, bool Integer) {
  const char *HeapName = 0;
  std::string Index = getHeapNameAndIndexToPtr(Name, Bytes, Integer, &HeapName);
  return std::string(HeapName) + '[' + Index + ']';
}

std::string BinaryenWriter::getShiftedPtr(const Value *Ptr, unsigned Bytes) {
  const char *HeapName = 0; // unused
  return getHeapNameAndIndex(Ptr, &HeapName, Bytes, true /* Integer; doesn't matter */);
}

std::string BinaryenWriter::getPtrUse(const Value* Ptr) {
  const char *HeapName = 0;
  std::string Index = getHeapNameAndIndex(Ptr, &HeapName);
  return std::string(HeapName) + '[' + Index + ']';
}

std::string BinaryenWriter::getConstant(const Constant* CV, AsmCast sign) {
  if (isa<ConstantPointerNull>(CV)) return "0";

  if (const Function *F = dyn_cast<Function>(CV)) {
    return relocateFunctionPointer(utostr(getFunctionIndex(F)));
  }

  if (const GlobalValue *GV = dyn_cast<GlobalValue>(CV)) {
    if (GV->isDeclaration()) {
      std::string Name = getOpName(GV);
      Externals.insert(Name);
      if (Relocatable) {
        // we access linked externs through calls, which we load at the beginning of basic blocks
        FuncRelocatableExterns.insert(Name);
        Name = "t$" + Name;
        UsedVars[Name] = Type::getInt32Ty(CV->getContext());
      }
      return Name;
    }
    if (const GlobalAlias *GA = dyn_cast<GlobalAlias>(CV)) {
      // Since we don't currently support linking of our output, we don't need
      // to worry about weak or other kinds of aliases.
      return getConstant(GA->getAliasee()->stripPointerCasts(), sign);
    }
    return relocateGlobal(utostr(getGlobalAddress(GV->getName().str())));
  }

  if (const ConstantFP *CFP = dyn_cast<ConstantFP>(CV)) {
    if (!(sign & ASM_FORCE_FLOAT_AS_INTBITS)) {
      std::string S = ftostr(CFP, sign);
      if (PreciseF32 && CV->getType()->isFloatTy() && !(sign & ASM_FFI_OUT)) {
        S = "Math_fround(" + S + ")";
      }
      return S;
    } else {
      const APFloat &flt = CFP->getValueAPF();
      APInt i = flt.bitcastToAPInt();
      assert(!(sign & ASM_UNSIGNED));
      if (i.getBitWidth() == 32) return itostr((int)(uint32_t)*i.getRawData());
      else return itostr(*i.getRawData());
    }
  } else if (const ConstantInt *CI = dyn_cast<ConstantInt>(CV)) {
    if (sign != ASM_UNSIGNED && CI->getValue().getBitWidth() == 1) {
      sign = ASM_UNSIGNED; // bools must always be unsigned: either 0 or 1
    }
    return CI->getValue().toString(10, sign != ASM_UNSIGNED);
  } else if (isa<UndefValue>(CV)) {
    std::string S;
    if (isa<VectorType>(CV->getType())) {
      llvm_unreachable("vector type");
    } else {
      S = CV->getType()->isFloatingPointTy() ? "+0" : "0"; // XXX refactor this
      if (PreciseF32 && CV->getType()->isFloatTy() && !(sign & ASM_FFI_OUT)) {
        S = "Math_fround(" + S + ")";
      }
    }
    return S;
  } else if (isa<ConstantAggregateZero>(CV)) {
    if (isa<VectorType>(CV->getType())) {
      llvm_unreachable("vector type");
    } else {
      // something like [0 x i8*] zeroinitializer, which clang can emit for landingpads
      return "0";
    }
  } else if (isa<ConstantDataVector>(CV)) {
    llvm_unreachable("vector type");
  } else if (isa<ConstantVector>(CV)) {
    llvm_unreachable("vector type");
  } else if (const ConstantArray *CA = dyn_cast<const ConstantArray>(CV)) {
    // handle things like [i8* bitcast (<{ i32, i32, i32 }>* @_ZTISt9bad_alloc to i8*)] which clang can emit for landingpads
    assert(CA->getNumOperands() == 1);
    CV = CA->getOperand(0);
    const ConstantExpr *CE = cast<ConstantExpr>(CV);
    CV = CE->getOperand(0); // ignore bitcast
    return getConstant(CV);
  } else if (const BlockAddress *BA = dyn_cast<const BlockAddress>(CV)) {
    return utostr(getBlockAddress(BA));
  } else if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(CV)) {
    std::string Code;
    raw_string_ostream CodeStream(Code);
    CodeStream << '(';
    generateExpression(CE, CodeStream); // FIXME
    CodeStream << ')';
    return CodeStream.str();
  } else {
    CV->dump();
    llvm_unreachable("Unsupported constant kind");
  }
}

std::string BinaryenWriter::getValueAsStr(const Value* V, AsmCast sign) {
  // Skip past no-op bitcasts and zero-index geps.
  V = V->stripPointerCasts();

  if (const Constant *CV = dyn_cast<Constant>(V)) {
    return getConstant(CV, sign);
  } else {
    return getSimpleName(V);
  }
}

std::string BinaryenWriter::getValueAsCastStr(const Value* V, AsmCast sign) {
  // Skip past no-op bitcasts and zero-index geps.
  V = V->stripPointerCasts();

  if (isa<ConstantInt>(V) || isa<ConstantFP>(V)) {
    return getConstant(cast<Constant>(V), sign);
  } else {
    return getCast(getValueAsStr(V), V->getType(), sign);
  }
}

std::string BinaryenWriter::getValueAsParenStr(const Value* V) {
  // Skip past no-op bitcasts and zero-index geps.
  V = V->stripPointerCasts();

  if (const Constant *CV = dyn_cast<Constant>(V)) {
    return getConstant(CV);
  } else {
    return "(" + getValueAsStr(V) + ")";
  }
}

std::string BinaryenWriter::getValueAsCastParenStr(const Value* V, AsmCast sign) {
  // Skip past no-op bitcasts and zero-index geps.
  V = V->stripPointerCasts();

  if (isa<ConstantInt>(V) || isa<ConstantFP>(V) || isa<UndefValue>(V)) {
    return getConstant(cast<Constant>(V), sign);
  } else {
    return "(" + getCast(getValueAsStr(V), V->getType(), sign) + ")";
  }
}

static const Value *getElement(const Value *V, unsigned i) {
    if (const InsertElementInst *II = dyn_cast<InsertElementInst>(V)) {
        if (ConstantInt *CI = dyn_cast<ConstantInt>(II->getOperand(2))) {
            if (CI->equalsInt(i))
                return II->getOperand(1);
        }
        return getElement(II->getOperand(0), i);
    }
    return nullptr;
}

static uint64_t LSBMask(unsigned numBits) {
  return numBits >= 64 ? 0xFFFFFFFFFFFFFFFFULL : (1ULL << numBits) - 1;
}

// Given a string which contains a printed base address, print a new string
// which contains that address plus the given offset.
static std::string AddOffset(const std::string &base, int32_t Offset) {
  if (base.empty())
    return itostr(Offset);

  if (Offset == 0)
    return base;

  return "((" + base + ") + " + itostr(Offset) + "|0)";
}

// Generate code for and operator, either an Instruction or a ConstantExpr.
BinaryenExpressionRef BinaryenWriter::generateExpression(const User *I) {
  // To avoid emiting code and variables for the no-op pointer bitcasts
  // and all-zero-index geps that LLVM needs to satisfy its type system, we
  // call stripPointerCasts() on all values before translating them. This
  // includes bitcasts whose only use is lifetime marker intrinsics.
  assert(I == I->stripPointerCasts());

  Type *T = I->getType();
  if (T->isIntegerTy() && T->getIntegerBitWidth() > 32) {
    errs() << *I << "\n";
    report_fatal_error("legalization problem");
  }

  BinaryenExpressionRef Ret = nullptr;

  switch (Operator::getOpcode(I)) {
  default: {
    I->dump();
    error("Invalid instruction in BinaryenWriter::generateExpression");
    break;
  }
  case Instruction::Ret: {
    const ReturnInst* ret =  cast<ReturnInst>(I);
    const Value *RV = ret->getReturnValue();
    auto Ret = BinaryenReturn(Wasm, RV != nullptr ? getValueAsWasm(RV) : nullptr);
    if (StackBumped) {
      BinaryenExpressionRef children[] = {
        SetSTACKTOP(getLocal("sp")),
        Ret
      };
      Ret = BinaryenBlock(Wasm, nullptr, children, 2);
    }
    break;
  }
  case Instruction::Br:
  case Instruction::IndirectBr:
  case Instruction::Switch: return; // handled while relooping
  case Instruction::Unreachable: {
    // Typically there should be an abort right before these, so we don't emit any code // TODO: when ASSERTIONS are on, emit abort(0)
    Code << "// unreachable";
    break;
  }
  case Instruction::Add:
  case Instruction::FAdd:
  case Instruction::Sub:
  case Instruction::FSub:
  case Instruction::Mul:
  case Instruction::FMul:
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::FDiv:
  case Instruction::URem:
  case Instruction::SRem:
  case Instruction::FRem:
  case Instruction::And:
  case Instruction::Or:
  case Instruction::Xor:
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:{
    Code << getAssignIfNeeded(I);
    unsigned opcode = Operator::getOpcode(I);
    switch (opcode) {
      case Instruction::Add:  Code << getParenCast(
                                        getValueAsParenStr(I->getOperand(0)) +
                                        " + " +
                                        getValueAsParenStr(I->getOperand(1)),
                                        I->getType()
                                      ); break;
      case Instruction::Sub:  Code << getParenCast(
                                        getValueAsParenStr(I->getOperand(0)) +
                                        " - " +
                                        getValueAsParenStr(I->getOperand(1)),
                                        I->getType()
                                      ); break;
      case Instruction::Mul:  Code << getIMul(I->getOperand(0), I->getOperand(1)); break;
      case Instruction::UDiv:
      case Instruction::SDiv:
      case Instruction::URem:
      case Instruction::SRem: Code << "(" <<
                                      getValueAsCastParenStr(I->getOperand(0), (opcode == Instruction::SDiv || opcode == Instruction::SRem) ? ASM_SIGNED : ASM_UNSIGNED) <<
                                      ((opcode == Instruction::UDiv || opcode == Instruction::SDiv) ? " / " : " % ") <<
                                      getValueAsCastParenStr(I->getOperand(1), (opcode == Instruction::SDiv || opcode == Instruction::SRem) ? ASM_SIGNED : ASM_UNSIGNED) <<
                                      ")&-1"; break;
      case Instruction::And:  Code << getValueAsStr(I->getOperand(0)) << " & " <<   getValueAsStr(I->getOperand(1)); break;
      case Instruction::Or:   Code << getValueAsStr(I->getOperand(0)) << " | " <<   getValueAsStr(I->getOperand(1)); break;
      case Instruction::Xor:  Code << getValueAsStr(I->getOperand(0)) << " ^ " <<   getValueAsStr(I->getOperand(1)); break;
      case Instruction::Shl:  {
        std::string Shifted = getValueAsStr(I->getOperand(0)) + " << " +  getValueAsStr(I->getOperand(1));
        if (I->getType()->getIntegerBitWidth() < 32) {
          Shifted = getParenCast(Shifted, I->getType(), ASM_UNSIGNED); // remove bits that are shifted beyond the size of this value
        }
        Code << Shifted;
        break;
      }
      case Instruction::AShr:
      case Instruction::LShr: {
        std::string Input = getValueAsStr(I->getOperand(0));
        if (I->getType()->getIntegerBitWidth() < 32) {
          Input = '(' + getCast(Input, I->getType(), opcode == Instruction::AShr ? ASM_SIGNED : ASM_UNSIGNED) + ')'; // fill in high bits, as shift needs those and is done in 32-bit
        }
        Code << Input << (opcode == Instruction::AShr ? " >> " : " >>> ") <<  getValueAsStr(I->getOperand(1));
        break;
      }

      case Instruction::FAdd: Code << ensureFloat(getValueAsStr(I->getOperand(0)) + " + " + getValueAsStr(I->getOperand(1)), I->getType()); break;
      case Instruction::FMul: Code << ensureFloat(getValueAsStr(I->getOperand(0)) + " * " + getValueAsStr(I->getOperand(1)), I->getType()); break;
      case Instruction::FDiv: Code << ensureFloat(getValueAsStr(I->getOperand(0)) + " / " + getValueAsStr(I->getOperand(1)), I->getType()); break;
      case Instruction::FRem: Code << ensureFloat(getValueAsStr(I->getOperand(0)) + " % " + getValueAsStr(I->getOperand(1)), I->getType()); break;
      case Instruction::FSub:
        // LLVM represents an fneg(x) as -0.0 - x.
        if (BinaryOperator::isFNeg(I)) {
          Code << ensureFloat("-" + getValueAsStr(BinaryOperator::getFNegArgument(I)), I->getType());
        } else {
          Code << ensureFloat(getValueAsStr(I->getOperand(0)) + " - " + getValueAsStr(I->getOperand(1)), I->getType());
        }
        break;
      default: error("bad binary opcode"); break;
    }
    break;
  }
  case Instruction::FCmp: {
    unsigned predicate = isa<ConstantExpr>(I) ?
                         (unsigned)cast<ConstantExpr>(I)->getPredicate() :
                         (unsigned)cast<FCmpInst>(I)->getPredicate();
    Code << getAssignIfNeeded(I);
    switch (predicate) {
      // Comparisons which are simple JS operators.
      case FCmpInst::FCMP_OEQ:   Code << getValueAsStr(I->getOperand(0)) << " == " << getValueAsStr(I->getOperand(1)); break;
      case FCmpInst::FCMP_UNE:   Code << getValueAsStr(I->getOperand(0)) << " != " << getValueAsStr(I->getOperand(1)); break;
      case FCmpInst::FCMP_OGT:   Code << getValueAsStr(I->getOperand(0)) << " > "  << getValueAsStr(I->getOperand(1)); break;
      case FCmpInst::FCMP_OGE:   Code << getValueAsStr(I->getOperand(0)) << " >= " << getValueAsStr(I->getOperand(1)); break;
      case FCmpInst::FCMP_OLT:   Code << getValueAsStr(I->getOperand(0)) << " < "  << getValueAsStr(I->getOperand(1)); break;
      case FCmpInst::FCMP_OLE:   Code << getValueAsStr(I->getOperand(0)) << " <= " << getValueAsStr(I->getOperand(1)); break;

      // Comparisons which are inverses of JS operators.
      case FCmpInst::FCMP_UGT:
        Code << "!(" << getValueAsStr(I->getOperand(0)) << " <= " << getValueAsStr(I->getOperand(1)) << ")";
        break;
      case FCmpInst::FCMP_UGE:
        Code << "!(" << getValueAsStr(I->getOperand(0)) << " < "  << getValueAsStr(I->getOperand(1)) << ")";
        break;
      case FCmpInst::FCMP_ULT:
        Code << "!(" << getValueAsStr(I->getOperand(0)) << " >= " << getValueAsStr(I->getOperand(1)) << ")";
        break;
      case FCmpInst::FCMP_ULE:
        Code << "!(" << getValueAsStr(I->getOperand(0)) << " > "  << getValueAsStr(I->getOperand(1)) << ")";
        break;

      // Comparisons which require explicit NaN checks.
      case FCmpInst::FCMP_UEQ:
        Code << "(" << getValueAsStr(I->getOperand(0)) << " != " << getValueAsStr(I->getOperand(0)) << ") | " <<
                "(" << getValueAsStr(I->getOperand(1)) << " != " << getValueAsStr(I->getOperand(1)) << ") |" <<
                "(" << getValueAsStr(I->getOperand(0)) << " == " << getValueAsStr(I->getOperand(1)) << ")";
        break;
      case FCmpInst::FCMP_ONE:
        Code << "(" << getValueAsStr(I->getOperand(0)) << " == " << getValueAsStr(I->getOperand(0)) << ") & " <<
                "(" << getValueAsStr(I->getOperand(1)) << " == " << getValueAsStr(I->getOperand(1)) << ") &" <<
                "(" << getValueAsStr(I->getOperand(0)) << " != " << getValueAsStr(I->getOperand(1)) << ")";
        break;

      // Simple NaN checks.
      case FCmpInst::FCMP_ORD:   Code << "(" << getValueAsStr(I->getOperand(0)) << " == " << getValueAsStr(I->getOperand(0)) << ") & " <<
                                         "(" << getValueAsStr(I->getOperand(1)) << " == " << getValueAsStr(I->getOperand(1)) << ")"; break;
      case FCmpInst::FCMP_UNO:   Code << "(" << getValueAsStr(I->getOperand(0)) << " != " << getValueAsStr(I->getOperand(0)) << ") | " <<
                                         "(" << getValueAsStr(I->getOperand(1)) << " != " << getValueAsStr(I->getOperand(1)) << ")"; break;

      // Simple constants.
      case FCmpInst::FCMP_FALSE: Code << "0"; break;
      case FCmpInst::FCMP_TRUE : Code << "1"; break;

      default: error("bad fcmp"); break;
    }
    break;
  }
  case Instruction::ICmp: {
    auto predicate = isa<ConstantExpr>(I) ?
                     (CmpInst::Predicate)cast<ConstantExpr>(I)->getPredicate() :
                     cast<ICmpInst>(I)->getPredicate();
    AsmCast sign = CmpInst::isUnsigned(predicate) ? ASM_UNSIGNED : ASM_SIGNED;
    Code << getAssignIfNeeded(I) << "(" <<
      getValueAsCastStr(I->getOperand(0), sign) <<
    ")";
    switch (predicate) {
    case ICmpInst::ICMP_EQ:  Code << "==";  break;
    case ICmpInst::ICMP_NE:  Code << "!=";  break;
    case ICmpInst::ICMP_ULE: Code << "<="; break;
    case ICmpInst::ICMP_SLE: Code << "<="; break;
    case ICmpInst::ICMP_UGE: Code << ">="; break;
    case ICmpInst::ICMP_SGE: Code << ">="; break;
    case ICmpInst::ICMP_ULT: Code << "<"; break;
    case ICmpInst::ICMP_SLT: Code << "<"; break;
    case ICmpInst::ICMP_UGT: Code << ">"; break;
    case ICmpInst::ICMP_SGT: Code << ">"; break;
    default: llvm_unreachable("Invalid ICmp predicate");
    }
    Code << "(" <<
      getValueAsCastStr(I->getOperand(1), sign) <<
    ")";
    break;
  }
  case Instruction::Alloca: {
    const AllocaInst* AI = cast<AllocaInst>(I);

    // We've done an alloca, so we'll have bumped the stack and will
    // need to restore it.
    // Yes, we shouldn't have to bump it for nativized vars, however
    // they are included in the frame offset, so the restore is still
    // needed until that is fixed.
    StackBumped = true;

    if (NativizedVars.count(AI)) {
      // nativized stack variable, we just need a 'var' definition
      UsedVars[getSimpleName(AI)] = AI->getType()->getElementType();
      return;
    }

    // Fixed-size entry-block allocations are allocated all at once in the
    // function prologue.
    if (AI->isStaticAlloca()) {
      uint64_t Offset;
      if (Allocas.getFrameOffset(AI, &Offset)) {
        Code << getAssign(AI);
        if (Allocas.getMaxAlignment() <= STACK_ALIGN) {
          Code << "sp";
        } else {
          Code << "sp_a"; // aligned base of stack is different, use that
        }
        if (Offset != 0) {
          Code << " + " << Offset << "|0";
        }
        break;
      }
      // Otherwise, this alloca is being represented by another alloca, so
      // there's nothing to print.
      return;
    }

    assert(AI->getAlignment() <= STACK_ALIGN); // TODO

    Type *T = AI->getAllocatedType();
    std::string Size;
    uint64_t BaseSize = DL->getTypeAllocSize(T);
    const Value *AS = AI->getArraySize();
    if (const ConstantInt *CI = dyn_cast<ConstantInt>(AS)) {
      Size = Twine(stackAlign(BaseSize * CI->getZExtValue())).str();
    } else {
      Size = stackAlignStr("((" + utostr(BaseSize) + '*' + getValueAsStr(AS) + ")|0)");
    }
    Code << getAssign(AI) << "STACKTOP; " << getStackBump(Size);
    break;
  }
  case Instruction::Load: {
    const LoadInst *LI = cast<LoadInst>(I);
    const Value *P = LI->getPointerOperand();
    unsigned Alignment = LI->getAlignment();
    if (NativizedVars.count(P)) {
      Code << getAssign(LI) << getValueAsStr(P);
    } else {
      Code << getLoad(LI, P, LI->getType(), Alignment);
    }
    break;
  }
  case Instruction::Store: {
    const StoreInst *SI = cast<StoreInst>(I);
    const Value *P = SI->getPointerOperand();
    const Value *V = SI->getValueOperand();
    unsigned Alignment = SI->getAlignment();
    std::string VS = getValueAsStr(V);
    if (NativizedVars.count(P)) {
      Code << getValueAsStr(P) << " = " << VS;
    } else {
      Code << getStore(SI, P, V->getType(), VS, Alignment);
    }

    Type *T = V->getType();
    if (T->isIntegerTy() && T->getIntegerBitWidth() > 32) {
      errs() << *I << "\n";
      report_fatal_error("legalization problem");
    }
    break;
  }
  case Instruction::GetElementPtr: {
    Code << getAssignIfNeeded(I);
    const GEPOperator *GEP = cast<GEPOperator>(I);
    gep_type_iterator GTI = gep_type_begin(GEP);
    int32_t ConstantOffset = 0;
    std::string text;

    // If the base is an initialized global variable, the address is just an
    // integer constant, so we can fold it into the ConstantOffset directly.
    const Value *Ptr = GEP->getPointerOperand()->stripPointerCasts();
    if (isa<GlobalVariable>(Ptr) && cast<GlobalVariable>(Ptr)->hasInitializer() && !Relocatable) {
      ConstantOffset = getGlobalAddress(Ptr->getName().str());
    } else {
      text = getValueAsParenStr(Ptr);
    }

    GetElementPtrInst::const_op_iterator I = GEP->op_begin();
    I++;
    for (GetElementPtrInst::const_op_iterator E = GEP->op_end();
       I != E; ++I) {
      const Value *Index = *I;
      if (StructType *STy = dyn_cast<StructType>(*GTI++)) {
        // For a struct, add the member offset.
        unsigned FieldNo = cast<ConstantInt>(Index)->getZExtValue();
        uint32_t Offset = DL->getStructLayout(STy)->getElementOffset(FieldNo);
        ConstantOffset = (uint32_t)ConstantOffset + Offset;
      } else {
        // For an array, add the element offset, explicitly scaled.
        uint32_t ElementSize = DL->getTypeAllocSize(*GTI);
        if (const ConstantInt *CI = dyn_cast<ConstantInt>(Index)) {
          // The index is constant. Add it to the accumulating offset.
          ConstantOffset = (uint32_t)ConstantOffset + (uint32_t)CI->getSExtValue() * ElementSize;
        } else {
          // The index is non-constant. To avoid reassociating, which increases
          // the risk of slow wraparounds, add the accumulated offset first.
          text = AddOffset(text, ConstantOffset);
          ConstantOffset = 0;

          // Now add the scaled dynamic index.
          std::string Mul = getIMul(Index, ConstantInt::get(Type::getInt32Ty(GEP->getContext()), ElementSize));
          text = text.empty() ? Mul : ("(" + text + " + (" + Mul + ")|0)");
        }
      }
    }
    // Add in the final accumulated offset.
    Code << AddOffset(text, ConstantOffset);
    break;
  }
  case Instruction::PHI: {
    // handled separately - we push them back into the relooper branchings
    return;
  }
  case Instruction::PtrToInt:
  case Instruction::IntToPtr:
    Code << getAssignIfNeeded(I) << getValueAsStr(I->getOperand(0));
    break;
  case Instruction::Trunc:
  case Instruction::ZExt:
  case Instruction::SExt:
  case Instruction::FPTrunc:
  case Instruction::FPExt:
  case Instruction::FPToUI:
  case Instruction::FPToSI:
  case Instruction::UIToFP:
  case Instruction::SIToFP: {
    Code << getAssignIfNeeded(I);
    switch (Operator::getOpcode(I)) {
    case Instruction::Trunc: {
      //unsigned inBits = V->getType()->getIntegerBitWidth();
      unsigned outBits = I->getType()->getIntegerBitWidth();
      Code << getValueAsStr(I->getOperand(0)) << "&" << utostr(LSBMask(outBits));
      break;
    }
    case Instruction::SExt: {
      std::string bits = utostr(32 - I->getOperand(0)->getType()->getIntegerBitWidth());
      Code << getValueAsStr(I->getOperand(0)) << " << " << bits << " >> " << bits;
      break;
    }
    case Instruction::ZExt: {
      Code << getValueAsCastStr(I->getOperand(0), ASM_UNSIGNED);
      break;
    }
    case Instruction::FPExt: {
      if (PreciseF32) {
        Code << "+" << getValueAsStr(I->getOperand(0)); break;
      } else {
        Code << getValueAsStr(I->getOperand(0)); break;
      }
      break;
    }
    case Instruction::FPTrunc: {
      Code << ensureFloat(getValueAsStr(I->getOperand(0)), I->getType());
      break;
    }
    case Instruction::SIToFP:   Code << '(' << getCast(getValueAsCastParenStr(I->getOperand(0), ASM_SIGNED),   I->getType()) << ')'; break;
    case Instruction::UIToFP:   Code << '(' << getCast(getValueAsCastParenStr(I->getOperand(0), ASM_UNSIGNED), I->getType()) << ')'; break;
    case Instruction::FPToSI:   Code << '(' << getDoubleToInt(getValueAsParenStr(I->getOperand(0))) << ')'; break;
    case Instruction::FPToUI:   Code << '(' << getCast(getDoubleToInt(getValueAsParenStr(I->getOperand(0))), I->getType(), ASM_UNSIGNED) << ')'; break;
    case Instruction::PtrToInt: Code << '(' << getValueAsStr(I->getOperand(0)) << ')'; break;
    case Instruction::IntToPtr: Code << '(' << getValueAsStr(I->getOperand(0)) << ')'; break;
    default: llvm_unreachable("Unreachable");
    }
    break;
  }
  case Instruction::BitCast: {
    Code << getAssignIfNeeded(I);
    // Most bitcasts are no-ops for us. However, the exception is int to float and float to int
    Type *InType = I->getOperand(0)->getType();
    Type *OutType = I->getType();
    std::string V = getValueAsStr(I->getOperand(0));
    if (InType->isIntegerTy() && OutType->isFloatingPointTy()) {
      assert(InType->getIntegerBitWidth() == 32);
      Code << "(HEAP32[tempDoublePtr>>2]=" << V << "," << getCast("HEAPF32[tempDoublePtr>>2]", Type::getFloatTy(TheModule->getContext())) << ")";
    } else if (OutType->isIntegerTy() && InType->isFloatingPointTy()) {
      assert(OutType->getIntegerBitWidth() == 32);
      Code << "(HEAPF32[tempDoublePtr>>2]=" << V << "," "HEAP32[tempDoublePtr>>2]|0)";
    } else {
      Code << V;
    }
    break;
  }
  case Instruction::Call: {
    const CallInst *CI = cast<CallInst>(I);
    std::string Call = handleCall(CI);
    if (Call.empty()) return;
    Code << Call;
    break;
  }
  case Instruction::Select: {
    Code << getAssignIfNeeded(I) << getValueAsStr(I->getOperand(0)) << " ? " <<
                                    getValueAsStr(I->getOperand(1)) << " : " <<
                                    getValueAsStr(I->getOperand(2));
    break;
  }
  case Instruction::AtomicRMW: {
    const AtomicRMWInst *rmwi = cast<AtomicRMWInst>(I);
    const Value *P = rmwi->getOperand(0);
    const Value *V = rmwi->getOperand(1);
    std::string VS = getValueAsStr(V);

    if (EnablePthreads) {
      std::string Assign = getAssign(rmwi);
      std::string text;
      const char *HeapName;
      std::string Index = getHeapNameAndIndex(P, &HeapName);
      const char *atomicFunc = 0;
      switch (rmwi->getOperation()) {
        case AtomicRMWInst::Xchg: atomicFunc = "exchange"; break;
        case AtomicRMWInst::Add:  atomicFunc = "add"; break;
        case AtomicRMWInst::Sub:  atomicFunc = "sub"; break;
        case AtomicRMWInst::And:  atomicFunc = "and"; break;
        case AtomicRMWInst::Or:   atomicFunc = "or"; break;
        case AtomicRMWInst::Xor:  atomicFunc = "xor"; break;
        case AtomicRMWInst::Nand: // TODO
        case AtomicRMWInst::Max:
        case AtomicRMWInst::Min:
        case AtomicRMWInst::UMax:
        case AtomicRMWInst::UMin:
        case AtomicRMWInst::BAD_BINOP: llvm_unreachable("Bad atomic operation");
      }
      if (!strcmp(HeapName, "HEAPF32") || !strcmp(HeapName, "HEAPF64")) {
        // TODO: If https://bugzilla.mozilla.org/show_bug.cgi?id=1131613 and https://bugzilla.mozilla.org/show_bug.cgi?id=1131624 are
        // implemented, we could remove the emulation, but until then we must emulate manually.
        bool fround = PreciseF32 && !strcmp(HeapName, "HEAPF32");
        Code << Assign << (fround ? "Math_fround(" : "+") << "_emscripten_atomic_" << atomicFunc << "_" << heapNameToAtomicTypeName(HeapName) << "(" << getValueAsStr(P) << ", " << VS << (fround ? "))" : ")"); break;

      // TODO: Remove the following two lines once https://bugzilla.mozilla.org/show_bug.cgi?id=1141986 is implemented!
      } else if (rmwi->getOperation() == AtomicRMWInst::Xchg && !strcmp(HeapName, "HEAP32")) {
        Code << Assign << "_emscripten_atomic_exchange_u32(" << getValueAsStr(P) << ", " << VS << ")|0"; break;

      } else {
        Code << Assign << "(Atomics_" << atomicFunc << "(" << HeapName << ", " << Index << ", " << VS << ")|0)"; break;
      }
    } else {
      Code << getLoad(rmwi, P, I->getType(), 0) << ';';
      // Most bitcasts are no-ops for us. However, the exception is int to float and float to int
      switch (rmwi->getOperation()) {
        case AtomicRMWInst::Xchg: Code << getStore(rmwi, P, I->getType(), VS, 0); break;
        case AtomicRMWInst::Add:  Code << getStore(rmwi, P, I->getType(), "((" + getSimpleName(I) + '+' + VS + ")|0)", 0); break;
        case AtomicRMWInst::Sub:  Code << getStore(rmwi, P, I->getType(), "((" + getSimpleName(I) + '-' + VS + ")|0)", 0); break;
        case AtomicRMWInst::And:  Code << getStore(rmwi, P, I->getType(), "(" + getSimpleName(I) + '&' + VS + ")", 0); break;
        case AtomicRMWInst::Nand: Code << getStore(rmwi, P, I->getType(), "(~(" + getSimpleName(I) + '&' + VS + "))", 0); break;
        case AtomicRMWInst::Or:   Code << getStore(rmwi, P, I->getType(), "(" + getSimpleName(I) + '|' + VS + ")", 0); break;
        case AtomicRMWInst::Xor:  Code << getStore(rmwi, P, I->getType(), "(" + getSimpleName(I) + '^' + VS + ")", 0); break;
        case AtomicRMWInst::Max:
        case AtomicRMWInst::Min:
        case AtomicRMWInst::UMax:
        case AtomicRMWInst::UMin:
        case AtomicRMWInst::BAD_BINOP: llvm_unreachable("Bad atomic operation");
      }
    }
    break;
  }
  case Instruction::Fence:
    if (EnablePthreads) Code << "(Atomics_add(HEAP32, 0, 0)|0) /* fence */";
    else Code << "/* fence */";
    break;
  }

  assert(Ret);

  if (const Instruction *Inst = dyn_cast<Instruction>(I)) {
    Code << ';';
    // append debug info
    emitDebugInfo(Code, Inst);
    Code << '\n';
  }

  return Ret;
}

// Checks whether to use a condition variable. We do so for switches and for indirectbrs
static const Value *considerConditionVar(const Instruction *I) {
  if (const IndirectBrInst *IB = dyn_cast<const IndirectBrInst>(I)) {
    return IB->getAddress();
  }
  const SwitchInst *SI = dyn_cast<SwitchInst>(I);
  if (!SI) return nullptr;
  // otherwise, we trust LLVM switches. if they were too big or sparse, the switch expansion pass should have fixed that
  return SI->getCondition();
}

void BinaryenWriter::addBlock(const BasicBlock *BB, RelooperRef R, LLVMToRelooperMap& LLVMToRelooper) {
  std::vector<BinaryenExpressionRef> Code;
  for (BasicBlock::const_iterator II = BB->begin(), E = BB->end();
       II != E; ++II) {
    auto I = &*II;
    if (I->stripPointerCasts() == I) {
      CurrInstruction = I;
      Code.push_back(generateExpression(I));
    }
  }
  CurrInstruction = nullptr;
  // TODO: switches const Value* Condition = considerConditionVar(BB->getTerminator());
  LLVMToRelooper[BB] = RelooperAddBlock(R, Code);
}

void BinaryenWriter::printFunctionBody(const Function *F) {
  assert(!F->isDeclaration());

  // Prepare relooper
  Relooper::MakeOutputBuffer(1024*1024);
  Relooper R;
  //if (!canReloop(F)) R.SetEmulate(true);
  if (F->getAttributes().hasAttribute(AttributeSet::FunctionIndex, Attribute::MinSize) ||
      F->getAttributes().hasAttribute(AttributeSet::FunctionIndex, Attribute::OptimizeForSize)) {
    R.SetMinSize(true);
  }
  R.SetAsmJSMode(1);
  Block *Entry = nullptr;
  LLVMToRelooperMap LLVMToRelooper;

  // Create relooper blocks with their contents. TODO: We could optimize
  // indirectbr by emitting indexed blocks first, so their indexes
  // match up with the label index.
  for (Function::const_iterator I = F->begin(), BE = F->end();
       I != BE; ++I) {
    auto BI = &*I;
    InvokeState = 0; // each basic block begins in state 0; the previous may not have cleared it, if e.g. it had a throw in the middle and the rest of it was decapitated
    addBlock(BI, R, LLVMToRelooper);
    if (!Entry) Entry = LLVMToRelooper[BI];
  }
  assert(Entry);

  // Create branchings
  for (Function::const_iterator I = F->begin(), BE = F->end();
       I != BE; ++I) {
    auto BI = &*I;
    const TerminatorInst *TI = BI->getTerminator();
    switch (TI->getOpcode()) {
      default: {
        report_fatal_error("invalid branch instr " + Twine(TI->getOpcodeName()));
        break;
      }
      case Instruction::Br: {
        const BranchInst* br = cast<BranchInst>(TI);
        if (br->getNumOperands() == 3) {
          BasicBlock *S0 = br->getSuccessor(0);
          BasicBlock *S1 = br->getSuccessor(1);
          std::string P0 = getPhiCode(&*BI, S0);
          std::string P1 = getPhiCode(&*BI, S1);
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S0], getValueAsStr(TI->getOperand(0)).c_str(), P0.size() > 0 ? P0.c_str() : nullptr);
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S1], nullptr,                                     P1.size() > 0 ? P1.c_str() : nullptr);
        } else if (br->getNumOperands() == 1) {
          BasicBlock *S = br->getSuccessor(0);
          std::string P = getPhiCode(&*BI, S);
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S], nullptr, P.size() > 0 ? P.c_str() : nullptr);
        } else {
          error("Branch with 2 operands?");
        }
        break;
      }
      case Instruction::IndirectBr: {
        const IndirectBrInst* br = cast<IndirectBrInst>(TI);
        unsigned Num = br->getNumDestinations();
        std::set<const BasicBlock*> Seen; // sadly llvm allows the same block to appear multiple times
        bool SetDefault = false; // pick the first and make it the default, llvm gives no reasonable default here
        for (unsigned i = 0; i < Num; i++) {
          const BasicBlock *S = br->getDestination(i);
          if (Seen.find(S) != Seen.end()) continue;
          Seen.insert(S);
          std::string P = getPhiCode(&*BI, S);
          std::string Target;
          if (!SetDefault) {
            SetDefault = true;
          } else {
            Target = "case " + utostr(getBlockAddress(F, S)) + ": ";
          }
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S], Target.size() > 0 ? Target.c_str() : nullptr, P.size() > 0 ? P.c_str() : nullptr);
        }
        break;
      }
      case Instruction::Switch: {
        const SwitchInst* SI = cast<SwitchInst>(TI);
        bool UseSwitch = !!considerConditionVar(SI);
        BasicBlock *DD = SI->getDefaultDest();
        std::string P = getPhiCode(&*BI, DD);
        LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*DD], nullptr, P.size() > 0 ? P.c_str() : nullptr);
        typedef std::map<const BasicBlock*, std::string> BlockCondMap;
        BlockCondMap BlocksToConditions;
        for (SwitchInst::ConstCaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i) {
          const BasicBlock *BB = i.getCaseSuccessor();
          std::string Curr = i.getCaseValue()->getValue().toString(10, true);
          std::string Condition;
          if (UseSwitch) {
            Condition = "case " + Curr + ": ";
          } else {
            Condition = "(" + getValueAsCastParenStr(SI->getCondition()) + " == " + Curr + ")";
          }
          BlocksToConditions[BB] = Condition + (!UseSwitch && BlocksToConditions[BB].size() > 0 ? " | " : "") + BlocksToConditions[BB];
        }
        std::set<const BasicBlock *> alreadyProcessed;
        for (SwitchInst::ConstCaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i) {
          const BasicBlock *BB = i.getCaseSuccessor();
          if (!alreadyProcessed.insert(BB).second) continue;
          if (BB == DD) continue; // ok to eliminate this, default dest will get there anyhow
          std::string P = getPhiCode(&*BI, BB);
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*BB], BlocksToConditions[BB].c_str(), P.size() > 0 ? P.c_str() : nullptr);
        }
        break;
      }
      case Instruction::Ret:
      case Instruction::Unreachable: break;
    }
  }

  // Calculate relooping and print
  R.Calculate(Entry);
  R.Render();

  // Emit local variables
  UsedVars["sp"] = Type::getInt32Ty(F->getContext());
  unsigned MaxAlignment = Allocas.getMaxAlignment();
  if (MaxAlignment > STACK_ALIGN) {
    UsedVars["sp_a"] = Type::getInt32Ty(F->getContext());
  }
  UsedVars["label"] = Type::getInt32Ty(F->getContext());
  if (!UsedVars.empty()) {
    unsigned Count = 0;
    for (VarMap::const_iterator VI = UsedVars.begin(); VI != UsedVars.end(); ++VI) {
      if (Count == 20) {
        Out << ";\n";
        Count = 0;
      }
      if (Count == 0) Out << " var ";
      if (Count > 0) {
        Out << ", ";
      }
      Count++;
      Out << VI->first << " = ";
      switch (VI->second->getTypeID()) {
        default:
          llvm_unreachable("unsupported variable initializer type");
        case Type::PointerTyID:
        case Type::IntegerTyID:
          Out << "0";
          break;
        case Type::FloatTyID:
          if (PreciseF32) {
            Out << "Math_fround(0)";
            break;
          }
          // otherwise fall through to double
        case Type::DoubleTyID:
          Out << "+0";
          break;
      }
    }
    Out << ";";
    nl(Out);
  }

  // Emit stack entry
  Out << " " << getAdHocAssign("sp", Type::getInt32Ty(F->getContext())) << "STACKTOP;";
  if (uint64_t FrameSize = Allocas.getFrameSize()) {
    if (MaxAlignment > STACK_ALIGN) {
      // We must align this entire stack frame to something higher than the default
      Out << "\n ";
      Out << "sp_a = STACKTOP = (STACKTOP + " << utostr(MaxAlignment-1) << ")&-" << utostr(MaxAlignment) << ";";
    }
    Out << "\n ";
    Out << getStackBump(FrameSize);
  }

  // Emit extern loads, if we have any
  if (Relocatable) {
    if (FuncRelocatableExterns.size() > 0) {
      for (auto& RE : FuncRelocatableExterns) {
        std::string Temp = "t$" + RE;
        std::string Call = "g$" + RE;
        Out << Temp + " = " + Call + "() | 0;\n";
      }
      FuncRelocatableExterns.clear();
    }
  }

  // Emit (relooped) code
  char *buffer = Relooper::GetOutputBuffer();
  nl(Out) << buffer;

  // Ensure a final return if necessary
  Type *RT = F->getFunctionType()->getReturnType();
  if (!RT->isVoidTy()) {
    char *LastCurly = strrchr(buffer, '}');
    if (!LastCurly) LastCurly = buffer;
    char *FinalReturn = strstr(LastCurly, "return ");
    if (!FinalReturn) {
      Out << " return " << getParenCast(getConstant(UndefValue::get(RT)), RT, ASM_NONSPECIFIC) << ";\n";
    }
  }

  if (Relocatable) {
    if (!F->hasInternalLinkage()) {
      Exports.push_back(getSimpleName(F));
    }
  }
}

void BinaryenWriter::processConstants() {
  // Ensure a name for each global
  for (Module::global_iterator I = TheModule->global_begin(),
         E = TheModule->global_end(); I != E; ++I) {
    if (I->hasInitializer()) {
      if (!I->hasName()) {
        // ensure a unique name
        static int id = 1;
        std::string newName;
        while (1) {
          newName = std::string("glb_") + utostr(id);
          if (!TheModule->getGlobalVariable("glb_" + utostr(id))) break;
          id++;
          assert(id != 0);
        }
        I->setName(Twine(newName));
      }
    }
  }
  // First, calculate the address of each constant
  for (Module::const_global_iterator I = TheModule->global_begin(),
         E = TheModule->global_end(); I != E; ++I) {
    if (I->hasInitializer()) {
      parseConstant(I->getName().str(), I->getInitializer(), I->getAlignment(), true);
    }
  }
  // Calculate MaxGlobalAlign, adjust final paddings, and adjust GlobalBasePadding
  assert(MaxGlobalAlign == 0);
  for (auto& GI : GlobalDataMap) {
    int Alignment = GI.first;
    if (Alignment > MaxGlobalAlign) MaxGlobalAlign = Alignment;
    ensureAligned(Alignment, &GlobalDataMap[Alignment]);
  }
  if (int(ZeroInitSizes.size()-1) > MaxGlobalAlign) MaxGlobalAlign = ZeroInitSizes.size()-1; // highest index in ZeroInitSizes is the largest zero-init alignment
  if (!Relocatable && MaxGlobalAlign > 0) {
    while ((GlobalBase+GlobalBasePadding) % MaxGlobalAlign != 0) GlobalBasePadding++;
  }
  while (AlignedHeapStarts.size() <= (unsigned)MaxGlobalAlign) AlignedHeapStarts.push_back(0);
  while (ZeroInitStarts.size() <= (unsigned)MaxGlobalAlign) ZeroInitStarts.push_back(0);
  for (auto& GI : GlobalDataMap) {
    int Alignment = GI.first;
    int Curr = GlobalBase + GlobalBasePadding;
    for (auto& GI : GlobalDataMap) { // bigger alignments show up first, smaller later
      if (GI.first > Alignment) {
        Curr += GI.second.size();
      }
    }
    AlignedHeapStarts[Alignment] = Curr;
  }

  unsigned ZeroInitStart = GlobalBase + GlobalBasePadding;
  for (auto& GI : GlobalDataMap) {
    ZeroInitStart += GI.second.size();
  }
  if (!ZeroInitSizes.empty()) {
    while (ZeroInitStart & (MaxGlobalAlign-1)) ZeroInitStart++; // fully align zero init area
    for (int Alignment = ZeroInitSizes.size() - 1; Alignment > 0; Alignment--) {
      if (ZeroInitSizes[Alignment] == 0) continue;
      assert((ZeroInitStart & (Alignment-1)) == 0);
      ZeroInitStarts[Alignment] = ZeroInitStart;
      ZeroInitStart += ZeroInitSizes[Alignment];
    }
  }
  StaticBump = ZeroInitStart; // total size of all the data section

  // Second, allocate their contents
  for (Module::const_global_iterator I = TheModule->global_begin(),
         E = TheModule->global_end(); I != E; ++I) {
    if (I->hasInitializer()) {
      parseConstant(I->getName().str(), I->getInitializer(), I->getAlignment(), false);
    }
  }
  if (Relocatable) {
    for (Module::const_global_iterator II = TheModule->global_begin(),
           E = TheModule->global_end(); II != E; ++II) {
      auto I = &*II;
      if (I->hasInitializer() && !I->hasInternalLinkage()) {
        std::string Name = I->getName().str();
        if (GlobalAddresses.find(Name) != GlobalAddresses.end()) {
          std::string SimpleName = getSimpleName(I).substr(1);
          if (Name == SimpleName) { // don't export things that have weird internal names, that C can't dlsym anyhow
            NamedGlobals[Name] = getGlobalAddress(Name);
          }
        }
      }
    }
  }
}

void BinaryenWriter::printFunction(const Function *F) {
  ValueNames.clear();

  // Prepare and analyze function

  UsedVars.clear();
  UniqueNum = 0;

  // When optimizing, the regular optimizer (mem2reg, SROA, GVN, and others)
  // will have already taken all the opportunities for nativization.
  if (OptLevel == CodeGenOpt::None)
    calculateNativizedVars(F);

  // Do alloca coloring at -O1 and higher.
  Allocas.analyze(*F, *DL, OptLevel != CodeGenOpt::None);

  // Emit the function

  std::string Name = F->getName();
  sanitizeGlobal(Name);
  Out << "function " << Name << "(";
  for (Function::const_arg_iterator AI = F->arg_begin(), AE = F->arg_end();
       AI != AE; ++AI) {
    if (AI != F->arg_begin()) Out << ",";
    Out << getSimpleName(&*AI);
  }
  Out << ") {";
  nl(Out);
  for (Function::const_arg_iterator II = F->arg_begin(), AE = F->arg_end();
       II != AE; ++II) {
    auto AI = &*II;
    std::string name = getSimpleName(AI);
    Out << " " << name << " = " << getCast(name, AI->getType(), ASM_NONSPECIFIC) << ";";
    nl(Out);
  }
  printFunctionBody(F);
  Out << "}";
  nl(Out);

  Allocas.clear();
  StackBumped = false;
}

void BinaryenWriter::printModuleBody() {
  processConstants();

  if (Relocatable) {
    for (Module::const_alias_iterator I = TheModule->alias_begin(), E = TheModule->alias_end();
         I != E; ++I) {
      if (const GlobalAlias *GA = dyn_cast<GlobalAlias>(I)) {
        const Value* Target = resolveFully(GA);
        Aliases[getSimpleName(GA)] = getSimpleName(Target);
      }
    }
  }

  // Emit function bodies.
  nl(Out) << "// EMSCRIPTEN_START_FUNCTIONS"; nl(Out);
  for (Module::const_iterator II = TheModule->begin(), E = TheModule->end();
       II != E; ++II) {
    auto I = &*II;
    if (!I->isDeclaration()) printFunction(I);
  }
  // Emit postSets, split up into smaller functions to avoid one massive one that is slow to compile (more likely to occur in dynamic linking, as more postsets)
  {
    const int CHUNK = 100;
    int i = 0;
    int chunk = 0;
    int num = PostSets.size();
    do {
      if (chunk == 0) {
        Out << "function runPostSets() {\n";
      } else {
        Out << "function runPostSets" << chunk << "() {\n";
      }
      if (Relocatable) Out << " var temp = 0;\n"; // need a temp var for relocation calls, for proper validation in heap growth
      int j = i + CHUNK;
      if (j > num) j = num;
      while (i < j) {
        Out << PostSets[i] << "\n";
        i++;
      }
      // call the next chunk, if there is one
      chunk++;
      if (i < num) {
        Out << " runPostSets" << chunk << "();\n";
      }
      Out << "}\n";
    } while (i < num);
    PostSets.clear();
  }
  Out << "// EMSCRIPTEN_END_FUNCTIONS\n\n";

  if (EnablePthreads) {
    Out << "if (!ENVIRONMENT_IS_PTHREAD) {\n";
  }
  Out << "/* memory initializer */ allocate([";
  if (MaxGlobalAlign > 0) {
    bool First = true;
    for (int i = 0; i < GlobalBasePadding; i++) {
      if (First) {
        First = false;
      } else {
        Out << ",";
      }
      Out << "0";
    }
    int Curr = MaxGlobalAlign;
    while (Curr > 0) {
      if (GlobalDataMap.find(Curr) == GlobalDataMap.end()) {
        Curr = Curr/2;
        continue;
      }
      HeapData* GlobalData = &GlobalDataMap[Curr];
      if (GlobalData->size() > 0) {
        if (First) {
          First = false;
        } else {
          Out << ",";
        }
        printCommaSeparated(*GlobalData);
      }
      Curr = Curr/2;
    }
  }
  Out << "], \"i8\", ALLOC_NONE, Runtime.GLOBAL_BASE);\n";
  if (EnablePthreads) {
    Out << "}\n";
  }
  // Emit metadata for emcc driver
  Out << "\n\n// EMSCRIPTEN_METADATA\n";
  Out << "{\n";

  Out << "\"staticBump\": " << StaticBump << ",\n";

  Out << "\"declares\": [";
  bool first = true;
  for (Module::const_iterator I = TheModule->begin(), E = TheModule->end();
       I != E; ++I) {
    if (I->isDeclaration() && !I->use_empty()) {
      // Ignore intrinsics that are always no-ops or expanded into other code
      // which doesn't require the intrinsic function itself to be declared.
      if (I->isIntrinsic()) {
        switch (I->getIntrinsicID()) {
        default: break;
        case Intrinsic::dbg_declare:
        case Intrinsic::dbg_value:
        case Intrinsic::lifetime_start:
        case Intrinsic::lifetime_end:
        case Intrinsic::invariant_start:
        case Intrinsic::invariant_end:
        case Intrinsic::prefetch:
        case Intrinsic::memcpy:
        case Intrinsic::memset:
        case Intrinsic::memmove:
        case Intrinsic::expect:
        case Intrinsic::flt_rounds:
          continue;
        }
      }
      // Do not report methods implemented in a call handler, unless
      // they are accessed by a function pointer (in which case, we
      // need the expected name to be available TODO: optimize
      // that out, call handlers can declare their "function table
      // name").
      std::string fullName = std::string("_") + I->getName().str();
      if (CallHandlers.count(fullName) > 0) {
        if (IndexedFunctions.find(fullName) == IndexedFunctions.end()) {
          continue;
        }
      }

      if (first) {
        first = false;
      } else {
        Out << ", ";
      }
      Out << "\"" << I->getName() << "\"";
    }
  }
  for (NameSet::const_iterator I = Declares.begin(), E = Declares.end();
       I != E; ++I) {
    if (first) {
      first = false;
    } else {
      Out << ", ";
    }
    Out << "\"" << *I << "\"";
  }
  Out << "],";

  Out << "\"redirects\": {";
  first = true;
  for (StringMap::const_iterator I = Redirects.begin(), E = Redirects.end();
       I != E; ++I) {
    if (first) {
      first = false;
    } else {
      Out << ", ";
    }
    Out << "\"_" << I->first << "\": \"" << I->second << "\"";
  }
  Out << "},";

  Out << "\"externs\": [";
  first = true;
  for (NameSet::const_iterator I = Externals.begin(), E = Externals.end();
       I != E; ++I) {
    if (first) {
      first = false;
    } else {
      Out << ", ";
    }
    Out << "\"" << *I << "\"";
  }
  Out << "],";

  Out << "\"implementedFunctions\": [";
  first = true;
  for (Module::const_iterator I = TheModule->begin(), E = TheModule->end();
       I != E; ++I) {
    if (!I->isDeclaration()) {
      if (first) {
        first = false;
      } else {
        Out << ", ";
      }
      std::string name = I->getName();
      sanitizeGlobal(name);
      Out << "\"" << name << '"';
    }
  }
  Out << "],";

  Out << "\"tables\": {";
  unsigned Num = FunctionTables.size();
  for (FunctionTableMap::iterator I = FunctionTables.begin(), E = FunctionTables.end(); I != E; ++I) {
    Out << "  \"" << I->first << "\": \"var FUNCTION_TABLE_" << I->first << " = [";
    FunctionTable &Table = I->second;
    // ensure power of two
    unsigned Size = 1;
    while (Size < Table.size()) Size <<= 1;
    while (Table.size() < Size) Table.push_back("0");
    for (unsigned i = 0; i < Table.size(); i++) {
      Out << Table[i];
      if (i < Table.size()-1) Out << ",";
    }
    Out << "];\"";
    if (--Num > 0) Out << ",";
    Out << "\n";
  }
  Out << "},";

  Out << "\"initializers\": [";
  first = true;
  for (unsigned i = 0; i < GlobalInitializers.size(); i++) {
    if (first) {
      first = false;
    } else {
      Out << ", ";
    }
    Out << "\"" << GlobalInitializers[i] << "\"";
  }
  Out << "],";

  Out << "\"exports\": [";
  first = true;
  for (unsigned i = 0; i < Exports.size(); i++) {
    if (first) {
      first = false;
    } else {
      Out << ", ";
    }
    Out << "\"" << Exports[i] << "\"";
  }
  Out << "],";

  Out << "\"aliases\": {";
  first = true;
  for (StringMap::const_iterator I = Aliases.begin(), E = Aliases.end();
       I != E; ++I) {
    if (first) {
      first = false;
    } else {
      Out << ", ";
    }
    Out << "\"" << I->first << "\": \"" << I->second << "\"";
  }
  Out << "},";

  Out << "\"cantValidate\": \"" << CantValidate << "\",";

  Out << "\"maxGlobalAlign\": " << utostr(MaxGlobalAlign) << ",";

  Out << "\"namedGlobals\": {";
  first = true;
  for (NameIntMap::const_iterator I = NamedGlobals.begin(), E = NamedGlobals.end(); I != E; ++I) {
    if (first) {
      first = false;
    } else {
      Out << ", ";
    }
    Out << "\"" << I->first << "\": \"" << utostr(I->second) << "\"";
  }
  Out << "},";

  Out << "\"asmConsts\": {";
  first = true;
  for (auto& I : AsmConsts) {
    if (first) {
      first = false;
    } else {
      Out << ", ";
    }
    Out << "\"" << utostr(I.second.Id) << "\": [\"" << I.first.c_str() << "\", [";
    auto& Sigs = I.second.Sigs;
    bool innerFirst = true;
    for (auto& Sig : Sigs) {
      if (innerFirst) {
        innerFirst = false;
      } else {
        Out << ", ";
      }
      Out << "\"" << Sig << "\"";
    }
    Out << "]]";
  }
  Out << "}";

  if (EnableCyberDWARF) {
    Out << ",\"cyberdwarf_data\": {\n";
    Out << "\"types\": {";

    // Remove trailing comma
    std::string TDD = cyberDWARFData.TypeDebugData.str().substr(0, cyberDWARFData.TypeDebugData.str().length() - 1);
    // One Windows, paths can have \ separators
    std::replace(TDD.begin(), TDD.end(), '\\', '/');
    Out << TDD << "}, \"type_name_map\": {";

    std::string TNM = cyberDWARFData.TypeNameMap.str().substr(0, cyberDWARFData.TypeNameMap.str().length() - 1);
    std::replace(TNM.begin(), TNM.end(), '\\', '/');
    Out << TNM << "}, \"functions\": {";

    std::string FM = cyberDWARFData.FunctionMembers.str().substr(0, cyberDWARFData.FunctionMembers.str().length() - 1);
    std::replace(FM.begin(), FM.end(), '\\', '/');
    Out << FM << "}, \"vtable_offsets\": {";
    bool first_elem = true;
    for (auto VTO: cyberDWARFData.VtableOffsets) {
      if (!first_elem) {
        Out << ",";
      }
      Out << "\"" << VTO.first << "\":\"" << VTO.second << "\"";
      first_elem = false;
    }
    Out << "}\n}";
  }

  Out << "\n}\n";
}

void BinaryenWriter::parseConstant(const std::string& name, const Constant* CV, int Alignment, bool calculate) {
  if (isa<GlobalValue>(CV))
    return;
  if (Alignment == 0) Alignment = DEFAULT_MEM_ALIGN;
  //errs() << "parsing constant " << name << " : " << Alignment << "\n";
  // TODO: we repeat some work in both calculate and emit phases here
  // FIXME: use the proper optimal alignments
  if (const ConstantDataSequential *CDS =
         dyn_cast<ConstantDataSequential>(CV)) {
    assert(CDS->isString());
    if (calculate) {
      HeapData *GlobalData = allocateAddress(name, Alignment);
      StringRef Str = CDS->getAsString();
      ensureAligned(Alignment, GlobalData);
      for (unsigned int i = 0; i < Str.size(); i++) {
        GlobalData->push_back(Str.data()[i]);
      }
    }
  } else if (const ConstantFP *CFP = dyn_cast<ConstantFP>(CV)) {
    APFloat APF = CFP->getValueAPF();
    if (CFP->getType() == Type::getFloatTy(CFP->getContext())) {
      if (calculate) {
        HeapData *GlobalData = allocateAddress(name, Alignment);
        union flt { float f; unsigned char b[sizeof(float)]; } flt;
        flt.f = APF.convertToFloat();
        ensureAligned(Alignment, GlobalData);
        for (unsigned i = 0; i < sizeof(float); ++i) {
          GlobalData->push_back(flt.b[i]);
        }
      }
    } else if (CFP->getType() == Type::getDoubleTy(CFP->getContext())) {
      if (calculate) {
        HeapData *GlobalData = allocateAddress(name, Alignment);
        union dbl { double d; unsigned char b[sizeof(double)]; } dbl;
        dbl.d = APF.convertToDouble();
        ensureAligned(Alignment, GlobalData);
        for (unsigned i = 0; i < sizeof(double); ++i) {
          GlobalData->push_back(dbl.b[i]);
        }
      }
    } else {
      assert(false && "Unsupported floating-point type");
    }
  } else if (const ConstantInt *CI = dyn_cast<ConstantInt>(CV)) {
    if (calculate) {
      union { uint64_t i; unsigned char b[sizeof(uint64_t)]; } integer;
      integer.i = *CI->getValue().getRawData();
      unsigned BitWidth = 64; // CI->getValue().getBitWidth();
      assert(BitWidth == 32 || BitWidth == 64);
      HeapData *GlobalData = allocateAddress(name, Alignment);
      // assuming compiler is little endian
      ensureAligned(Alignment, GlobalData);
      for (unsigned i = 0; i < BitWidth / 8; ++i) {
        GlobalData->push_back(integer.b[i]);
      }
    }
  } else if (isa<ConstantPointerNull>(CV)) {
    assert(false && "Unlowered ConstantPointerNull");
  } else if (isa<ConstantAggregateZero>(CV)) {
    if (calculate) {
      unsigned Bytes = DL->getTypeStoreSize(CV->getType());
      allocateZeroInitAddress(name, Alignment, Bytes);
    }
  } else if (const ConstantArray *CA = dyn_cast<ConstantArray>(CV)) {
    if (calculate) {
      for (Constant::const_user_iterator UI = CV->user_begin(), UE = CV->user_end(); UI != UE; ++UI) {
        if ((*UI)->getName() == "llvm.used") {
          // export the kept-alives
          for (unsigned i = 0; i < CA->getNumOperands(); i++) {
            const Constant *C = CA->getOperand(i);
            if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
              C = CE->getOperand(0); // ignore bitcasts
            }
            if (isa<Function>(C)) Exports.push_back(getSimpleName(C));
          }
        } else if ((*UI)->getName() == "llvm.global.annotations") {
          // llvm.global.annotations can be ignored.
        } else {
          llvm_unreachable("Unexpected constant array");
        }
        break; // we assume one use here
      }
    }
  } else if (const ConstantStruct *CS = dyn_cast<ConstantStruct>(CV)) {
    if (name == "__init_array_start") {
      // this is the global static initializer
      if (calculate) {
        unsigned Num = CS->getNumOperands();
        for (unsigned i = 0; i < Num; i++) {
          const Value* C = CS->getOperand(i);
          if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
            C = CE->getOperand(0); // ignore bitcasts
          }
          GlobalInitializers.push_back(getSimpleName(C));
        }
      }
    } else if (calculate) {
      HeapData *GlobalData = allocateAddress(name, Alignment);
      unsigned Bytes = DL->getTypeStoreSize(CV->getType());
      ensureAligned(Alignment, GlobalData);
      for (unsigned i = 0; i < Bytes; ++i) {
        GlobalData->push_back(0);
      }
    } else {
      // Per the PNaCl abi, this must be a packed struct of a very specific type
      // https://chromium.googlesource.com/native_client/pnacl-llvm/+/7287c45c13dc887cebe3db6abfa2f1080186bb97/lib/Transforms/NaCl/FlattenGlobals.cpp
      assert(CS->getType()->isPacked());
      // This is the only constant where we cannot just emit everything during the first phase, 'calculate', as we may refer to other globals
      unsigned Num = CS->getNumOperands();
      unsigned Offset = getRelativeGlobalAddress(name);
      unsigned OffsetStart = Offset;
      unsigned Absolute = getGlobalAddress(name);

      // VTable for the object
      if (name.compare(0, 4, "_ZTV") == 0) {
        cyberDWARFData.VtableOffsets[Absolute] = name;
      }

      for (unsigned i = 0; i < Num; i++) {
        const Constant* C = CS->getOperand(i);
        if (isa<ConstantAggregateZero>(C)) {
          unsigned Bytes = DL->getTypeStoreSize(C->getType());
          Offset += Bytes; // zeros, so just skip
        } else if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
          const Value *V = CE->getOperand(0);
          unsigned Data = 0;
          if (CE->getOpcode() == Instruction::PtrToInt) {
            Data = getConstAsOffset(V, Absolute + Offset - OffsetStart);
          } else if (CE->getOpcode() == Instruction::Add) {
            V = cast<ConstantExpr>(V)->getOperand(0);
            Data = getConstAsOffset(V, Absolute + Offset - OffsetStart);
            ConstantInt *CI = cast<ConstantInt>(CE->getOperand(1));
            Data += *CI->getValue().getRawData();
          } else {
            CE->dump();
            llvm_unreachable("Unexpected constant expr kind");
          }
          union { unsigned i; unsigned char b[sizeof(unsigned)]; } integer;
          integer.i = Data;
          HeapData& GlobalData = GlobalDataMap[Alignment];
          assert(Offset+4 <= GlobalData.size());
          ensureAligned(Alignment, GlobalData);
          for (unsigned i = 0; i < 4; ++i) {
            GlobalData[Offset++] = integer.b[i];
          }
        } else if (const ConstantDataSequential *CDS = dyn_cast<ConstantDataSequential>(C)) {
          assert(CDS->isString());
          StringRef Str = CDS->getAsString();
          HeapData& GlobalData = GlobalDataMap[Alignment];
          assert(Offset+Str.size() <= GlobalData.size());
          ensureAligned(Alignment, GlobalData);
          for (unsigned int i = 0; i < Str.size(); i++) {
            GlobalData[Offset++] = Str.data()[i];
          }
        } else {
          C->dump();
          llvm_unreachable("Unexpected constant kind");
        }
      }
    }
  } else if (isa<ConstantVector>(CV)) {
    assert(false && "Unlowered ConstantVector");
  } else if (isa<BlockAddress>(CV)) {
    assert(false && "Unlowered BlockAddress");
  } else if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(CV)) {
    if (name == "__init_array_start") {
      // this is the global static initializer
      if (calculate) {
        const Value *V = CE->getOperand(0);
        GlobalInitializers.push_back(getSimpleName(V));
        // is the func
      }
    } else if (name == "__fini_array_start") {
      // nothing to do
    } else {
      // a global equal to a ptrtoint of some function, so a 32-bit integer for us
      if (calculate) {
        HeapData *GlobalData = allocateAddress(name, Alignment);
        ensureAligned(Alignment, GlobalData);
        for (unsigned i = 0; i < 4; ++i) {
          GlobalData->push_back(0);
        }
      } else {
        unsigned Data = 0;

        // Deconstruct lowered getelementptrs.
        if (CE->getOpcode() == Instruction::Add) {
          Data = cast<ConstantInt>(CE->getOperand(1))->getZExtValue();
          CE = cast<ConstantExpr>(CE->getOperand(0));
        }
        const Value *V = CE;
        if (CE->getOpcode() == Instruction::PtrToInt) {
          V = CE->getOperand(0);
        }

        // Deconstruct getelementptrs.
        int64_t BaseOffset;
        V = GetPointerBaseWithConstantOffset(V, BaseOffset, *DL);
        Data += (uint64_t)BaseOffset;

        Data += getConstAsOffset(V, getGlobalAddress(name));
        union { unsigned i; unsigned char b[sizeof(unsigned)]; } integer;
        integer.i = Data;
        unsigned Offset = getRelativeGlobalAddress(name);
        HeapData& GlobalData = GlobalDataMap[Alignment];
        assert(Offset+4 <= GlobalData.size());
        ensureAligned(Alignment, GlobalData);
        for (unsigned i = 0; i < 4; ++i) {
          GlobalData[Offset++] = integer.b[i];
        }
      }
    }
  } else if (isa<UndefValue>(CV)) {
    assert(false && "Unlowered UndefValue");
  } else {
    CV->dump();
    assert(false && "Unsupported constant kind");
  }
}

std::string BinaryenWriter::generateDebugRecordForVar(Metadata *MD) {
  // void shows up as nullptr for Metadata
  if (!MD) {
    cyberDWARFData.IndexedMetadata[0] = 0;
    return "\"0\"";
  }
  if (cyberDWARFData.IndexedMetadata.find(MD) == cyberDWARFData.IndexedMetadata.end()) {
    cyberDWARFData.IndexedMetadata[MD] = cyberDWARFData.MetadataNum++;
  }
  else {
    return "\"" + utostr(cyberDWARFData.IndexedMetadata[MD]) + "\"";
  }

  std::string VarIDForJSON = "\"" + utostr(cyberDWARFData.IndexedMetadata[MD]) + "\"";

  if (DIBasicType *BT = dyn_cast<DIBasicType>(MD)) {
    cyberDWARFData.TypeDebugData << VarIDForJSON << ":"
    << "[0,\""
    << BT->getName().str()
    << "\","
    << BT->getEncoding()
    << ","
    << BT->getOffsetInBits()
    << ","
    << BT->getSizeInBits()
    << "],";
  }
  else if (MDString *MDS = dyn_cast<MDString>(MD)) {
    cyberDWARFData.TypeDebugData << VarIDForJSON << ":"
    << "[10,\"" << MDS->getString().str() << "\"],";
  }
  else if (DIDerivedType *DT = dyn_cast<DIDerivedType>(MD)) {
    if (DT->getRawBaseType() && isa<MDString>(DT->getRawBaseType())) {
      auto MDS = cast<MDString>(DT->getRawBaseType());
      cyberDWARFData.TypeDebugData << VarIDForJSON << ":"
      << "[1, \""
      << DT->getName().str()
      << "\","
      << DT->getTag()
      << ",\""
      << MDS->getString().str()
      << "\","
      << DT->getOffsetInBits()
      << ","
      << DT->getSizeInBits() << "],";
    }
    else {
      if (cyberDWARFData.IndexedMetadata.find(DT->getRawBaseType()) == cyberDWARFData.IndexedMetadata.end()) {
        generateDebugRecordForVar(DT->getRawBaseType());
      }

    cyberDWARFData.TypeDebugData << VarIDForJSON << ":"
        << "[1, \""
        << DT->getName().str()
        << "\","
        << DT->getTag()
        << ","
        << cyberDWARFData.IndexedMetadata[DT->getRawBaseType()]
        << ","
        << DT->getOffsetInBits()
        << ","
        << DT->getSizeInBits() << "],";
    }
  }
  else if (DICompositeType *CT = dyn_cast<DICompositeType>(MD)) {

    if (CT->getIdentifier().str() != "") {
      if (CT->isForwardDecl()) {
        cyberDWARFData.TypeNameMap << "\"" << "fd_" << CT->getIdentifier().str() << "\":" << VarIDForJSON << ",";
      } else {
        cyberDWARFData.TypeNameMap << "\"" << CT->getIdentifier().str() << "\":" << VarIDForJSON << ",";
      }
    }

    // Pull in debug info for any used elements before emitting ours
    for (auto e : CT->getElements()) {
      generateDebugRecordForVar(e);
    }

    // Build our base type, if we have one (arrays)
    if (cyberDWARFData.IndexedMetadata.find(CT->getRawBaseType()) == cyberDWARFData.IndexedMetadata.end()) {
      generateDebugRecordForVar(CT->getRawBaseType());
    }

    cyberDWARFData.TypeDebugData << VarIDForJSON << ":"
      << "[2, \""
      << CT->getName().str()
      << "\","
      << CT->getTag()
      << ","
      << cyberDWARFData.IndexedMetadata[CT->getRawBaseType()]
      << ","
      << CT->getOffsetInBits()
      << ","
      << CT->getSizeInBits()
      << ",\""
      << CT->getIdentifier().str()
      << "\",[";

    bool first_elem = true;
    for (auto e : CT->getElements()) {
      auto *vx = dyn_cast<DIType>(e);
      if ((vx && vx->isStaticMember()) || isa<DISubroutineType>(e))
        continue;
      if (!first_elem) {
        cyberDWARFData.TypeDebugData << ",";
      }
      first_elem = false;
      cyberDWARFData.TypeDebugData << generateDebugRecordForVar(e);
    }

    cyberDWARFData.TypeDebugData << "]],";

  }
  else if (DISubroutineType *ST = dyn_cast<DISubroutineType>(MD)) {
    cyberDWARFData.TypeDebugData << VarIDForJSON << ":"
    << "[3," << ST->getTag() << "],";
  }
  else if (DISubrange *SR = dyn_cast<DISubrange>(MD)) {
    cyberDWARFData.TypeDebugData << VarIDForJSON << ":"
    << "[4," << SR->getCount() << "],";
  }
  else if (DISubprogram *SP = dyn_cast<DISubprogram>(MD)) {
    cyberDWARFData.TypeDebugData << VarIDForJSON << ":"
    << "[5,\"" << SP->getName().str() << "\"],";
  }
  else if (DIEnumerator *E = dyn_cast<DIEnumerator>(MD)) {
    cyberDWARFData.TypeDebugData << VarIDForJSON << ":"
    << "[6,\"" << E->getName().str() << "\"," << E->getValue() << "],";
  }
  else {
    //MD->dump();
  }

  return VarIDForJSON;
}

void BinaryenWriter::buildCyberDWARFData() {
  for (auto &F : TheModule->functions()) {
    auto MD = F.getMetadata("dbg");
    if (MD) {
      auto *SP = cast<DISubprogram>(MD);

      if (SP->getLinkageName() != "") {
        cyberDWARFData.FunctionMembers << "\"" << SP->getLinkageName().str() << "\":{";
      }
      else {
        cyberDWARFData.FunctionMembers << "\"" << SP->getName().str() << "\":{";
      }
      bool first_elem = true;
      for (auto V : SP->getVariables()) {
        auto RT = V->getRawType();
        if (!first_elem) {
          cyberDWARFData.FunctionMembers << ",";
        }
        first_elem = false;
        cyberDWARFData.FunctionMembers << "\"" << V->getName().str() << "\":" << generateDebugRecordForVar(RT);
      }
      cyberDWARFData.FunctionMembers << "},";
    }
  }

  // Need to dump any types under each compilation unit's retained types
  auto CUs = TheModule->getNamedMetadata("llvm.dbg.cu");

  for (auto CUi : CUs->operands()) {
    auto CU = cast<DICompileUnit>(CUi);
    auto RT = CU->getRetainedTypes();
    for (auto RTi : RT) {
      generateDebugRecordForVar(RTi);
    }
  }
}

// nativization

void BinaryenWriter::calculateNativizedVars(const Function *F) {
  NativizedVars.clear();

  for (Function::const_iterator I = F->begin(), BE = F->end(); I != BE; ++I) {
    auto BI = &*I;
    for (BasicBlock::const_iterator II = BI->begin(), E = BI->end(); II != E; ++II) {
      const Instruction *I = &*II;
      if (const AllocaInst *AI = dyn_cast<const AllocaInst>(I)) {
        if (AI->getAllocatedType()->isVectorTy()) continue; // we do not nativize vectors, we rely on the LLVM optimizer to avoid load/stores on them
        if (AI->getAllocatedType()->isAggregateType()) continue; // we do not nativize aggregates either
        // this is on the stack. if its address is never used nor escaped, we can nativize it
        bool Fail = false;
        for (Instruction::const_user_iterator UI = I->user_begin(), UE = I->user_end(); UI != UE && !Fail; ++UI) {
          const Instruction *U = dyn_cast<Instruction>(*UI);
          if (!U) { Fail = true; break; } // not an instruction, not cool
          switch (U->getOpcode()) {
            case Instruction::Load: break; // load is cool
            case Instruction::Store: {
              if (U->getOperand(0) == I) Fail = true; // store *of* it is not cool; store *to* it is fine
              break;
            }
            default: { Fail = true; break; } // anything that is "not" "cool", is "not cool"
          }
        }
        if (!Fail) NativizedVars.insert(I);
      }
    }
  }
}

// special analyses

bool BinaryenWriter::canReloop(const Function *F) {
  return true;
}

// main entry

void BinaryenWriter::printCommaSeparated(const HeapData data) {
  for (HeapData::const_iterator I = data.begin();
       I != data.end(); ++I) {
    if (I != data.begin()) {
      Out << ",";
    }
    Out << (int)*I;
  }
}

void BinaryenWriter::printProgram(const std::string& fname,
                             const std::string& mName) {
  printModule(fname,mName);
}

void BinaryenWriter::printModule(const std::string& fname,
                            const std::string& mName) {
  printModuleBody();
}

bool BinaryenWriter::runOnModule(Module &M) {
  TheModule = &M;
  DL = &M.getDataLayout();

  // sanity checks on options
  assert(Relocatable ? GlobalBase == 0 : true);
  assert(Relocatable ? EmulatedFunctionPointers : true);

  Wasm = BinaryenModuleCreate();

  // Build debug data first, so that inline metadata can reuse the indicies
  if (EnableCyberDWARF)
    buildCyberDWARFData();

  setupCallHandlers();

  printProgram("", "");

  abort(); // TODO: write out binary

  BinaryenModuleDispose(Wasm);

  return false;
}

char BinaryenWriter::ID = 0;

class CheckTriple : public ModulePass {
public:
  static char ID;
  CheckTriple() : ModulePass(ID) {}
  bool runOnModule(Module &M) override {
    if (M.getTargetTriple() != "asmjs-unknown-emscripten") {
      prettyWarning() << "incorrect target triple '" << M.getTargetTriple() << "' (did you use emcc/em++ on all source files and not clang directly?)\n";
    }
    return false;
  }
};

char CheckTriple::ID;

Pass *createCheckTriplePass() {
  return new CheckTriple();
}

//===----------------------------------------------------------------------===//
//                       External Interface declaration
//===----------------------------------------------------------------------===//

bool BinaryenTargetMachine::addPassesToEmitFile(
      PassManagerBase &PM, raw_pwrite_stream &Out, CodeGenFileType FileType,
      bool DisableVerify, AnalysisID StartBefore,
      AnalysisID StartAfter, AnalysisID StopAfter,
      MachineFunctionInitializer *MFInitializer) {
  assert(FileType == TargetMachine::CGFT_AssemblyFile);

  PM.add(createCheckTriplePass());

  if (NoExitRuntime) {
    PM.add(createNoExitRuntimePass());
    // removing atexits opens up globalopt/globaldce opportunities
    PM.add(createGlobalOptimizerPass());
    PM.add(createGlobalDCEPass());
  }

  // PNaCl legalization
  {
    PM.add(createStripDanglingDISubprogramsPass());
    if (EnableSjLjEH) {
      // This comes before ExpandTls because it introduces references to
      // a TLS variable, __pnacl_eh_stack.  This comes before
      // InternalizePass because it assumes various variables (including
      // __pnacl_eh_stack) have not been internalized yet.
      PM.add(createPNaClSjLjEHPass());
    } else if (EnableEmCxxExceptions) {
      PM.add(createLowerEmExceptionsPass());
    } else {
      // LowerInvoke prevents use of C++ exception handling by removing
      // references to BasicBlocks which handle exceptions.
      PM.add(createLowerInvokePass());
    }
    // Run CFG simplification passes for a few reasons:
    // (1) Landingpad blocks can be made unreachable by LowerInvoke
    // when EnableSjLjEH is not enabled, so clean those up to ensure
    // there are no landingpad instructions in the stable ABI.
    // (2) Unreachable blocks can have strange properties like self-referencing
    // instructions, so remove them.
    PM.add(createCFGSimplificationPass());

    PM.add(createLowerEmSetjmpPass());

    // Expand out computed gotos (indirectbr and blockaddresses) into switches.
    PM.add(createExpandIndirectBrPass());

    // ExpandStructRegs must be run after ExpandVarArgs so that struct-typed
    // "va_arg" instructions have been removed.
    PM.add(createExpandVarArgsPass());

    // Convert struct reg function params to struct* byval. This needs to be
    // before ExpandStructRegs so it has a chance to rewrite aggregates from
    // function arguments and returns into something ExpandStructRegs can expand.
    PM.add(createSimplifyStructRegSignaturesPass());

    // TODO(mtrofin) Remove the following and only run it as a post-opt pass once
    //               the following bug is fixed.
    // https://code.google.com/p/nativeclient/issues/detail?id=3857
    PM.add(createExpandStructRegsPass());

    PM.add(createExpandCtorsPass());

    if (EnableEmAsyncify)
      PM.add(createLowerEmAsyncifyPass());

    // ExpandStructRegs must be run after ExpandArithWithOverflow to expand out
    // the insertvalue instructions that ExpandArithWithOverflow introduces.
    PM.add(createExpandArithWithOverflowPass());

    // We place ExpandByVal after optimization passes because some byval
    // arguments can be expanded away by the ArgPromotion pass.  Leaving
    // in "byval" during optimization also allows some dead stores to be
    // eliminated, because "byval" is a stronger constraint than what
    // ExpandByVal expands it to.
    PM.add(createExpandByValPass());

    PM.add(createPromoteI1OpsPass());

    // We should not place arbitrary passes after ExpandConstantExpr
    // because they might reintroduce ConstantExprs.
    PM.add(createExpandConstantExprPass());
    // The following pass inserts GEPs, it must precede ExpandGetElementPtr. It
    // also creates vector loads and stores, the subsequent pass cleans them up to
    // fix their alignment.
    PM.add(createConstantInsertExtractElementIndexPass());

    // Optimization passes and ExpandByVal introduce
    // memset/memcpy/memmove intrinsics with a 64-bit size argument.
    // This pass converts those arguments to 32-bit.
    PM.add(createCanonicalizeMemIntrinsicsPass());

    // ConstantMerge cleans up after passes such as GlobalizeConstantVectors. It
    // must run before the FlattenGlobals pass because FlattenGlobals loses
    // information that otherwise helps ConstantMerge do a good job.
    PM.add(createConstantMergePass());
    // FlattenGlobals introduces ConstantExpr bitcasts of globals which
    // are expanded out later. ReplacePtrsWithInts also creates some
    // ConstantExprs, and it locally creates an ExpandConstantExprPass
    // to clean both of these up.
    PM.add(createFlattenGlobalsPass());

    // The type legalization passes (ExpandLargeIntegers and PromoteIntegers) do
    // not handle constexprs and create GEPs, so they go between those passes.
    PM.add(createExpandLargeIntegersPass());
    PM.add(createPromoteIntegersPass());
    // Rewrite atomic and volatile instructions with intrinsic calls.
    PM.add(createRewriteAtomicsPass());

    PM.add(createSimplifyAllocasPass());

    // The atomic cmpxchg instruction returns a struct, and is rewritten to an
    // intrinsic as a post-opt pass, we therefore need to expand struct regs.
    PM.add(createExpandStructRegsPass());

    // Eliminate simple dead code that the post-opt passes could have created.
    PM.add(createDeadCodeEliminationPass());
  }
  // end PNaCl legalization

  PM.add(createExpandInsertExtractElementPass());
  PM.add(createExpandI64Pass());

  CodeGenOpt::Level OptLevel = getOptLevel();

  // When optimizing, there shouldn't be any opportunities for SimplifyAllocas
  // because the regular optimizer should have taken them all (GVN, and possibly
  // also SROA).
  if (OptLevel == CodeGenOpt::None)
    PM.add(createEmscriptenSimplifyAllocasPass());

  PM.add(createEmscriptenRemoveLLVMAssumePass());
  PM.add(createEmscriptenExpandBigSwitchesPass());

  PM.add(new BinaryenWriter(Out, OptLevel));

  return false;
}
