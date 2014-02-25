//===-- JSBackend.cpp - Library for converting LLVM code to JS       -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements compiling of LLVM IR, which is assumed to have been
// simplified using the PNaCl passes, i64 legalization, and other necessary
// transformations, into JavaScript in asm.js format, suitable for passing
// to emscripten for final processing.
//
//===----------------------------------------------------------------------===//

#include "JSTargetMachine.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Config/config.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/DebugInfo.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set> // TODO: unordered_set?
using namespace llvm;

#include <OptPasses.h>
#include <Relooper.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#ifdef NDEBUG
#undef assert
#define assert(x) { if (!(x)) report_fatal_error(#x); }
#endif

static cl::opt<bool>
PreciseF32("emscripten-precise-f32",
           cl::desc("Enables Math.fround usage to implement precise float32 semantics and performance (see emscripten PRECISE_F32 option)"),
           cl::init(false));

static cl::opt<bool>
WarnOnUnaligned("emscripten-warn-unaligned",
                cl::desc("Warns about unaligned loads and stores (which can negatively affect performance)"),
                cl::init(false));

static cl::opt<int>
ReservedFunctionPointers("emscripten-reserved-function-pointers",
                         cl::desc("Number of reserved slots in function tables for functions to be added at runtime (see emscripten RESERVED_FUNCTION_POINTERS option)"),
                         cl::init(0));

extern "C" void LLVMInitializeJSBackendTarget() {
  // Register the target.
  RegisterTargetMachine<JSTargetMachine> X(TheJSBackendTarget);
}

namespace {
  #define ASM_SIGNED 0
  #define ASM_UNSIGNED 1
  #define ASM_NONSPECIFIC 2 // nonspecific means to not differentiate ints. |0 for all, regardless of size and sign
  #define ASM_FFI_IN 4 // FFI return values are limited to things that work in ffis
  #define ASM_FFI_OUT 8 // params to FFIs are limited to things that work in ffis
  typedef unsigned AsmCast;

  const char *const SIMDLane = "XYZW";
  const char *const simdLane = "xyzw";

  typedef std::map<const Value*,std::string> ValueMap;
  typedef std::set<std::string> NameSet;
  typedef std::vector<unsigned char> HeapData;
  typedef std::pair<unsigned, unsigned> Address;
  typedef std::map<std::string, Type::TypeID> VarMap;
  typedef std::map<const AllocaInst*, unsigned> AllocaIntMap;
  typedef std::map<std::string, Address> GlobalAddressMap;
  typedef std::vector<std::string> FunctionTable;
  typedef std::map<std::string, FunctionTable> FunctionTableMap;
  typedef std::map<std::string, std::string> StringMap;
  typedef std::map<std::string, unsigned> NameIntMap;
  typedef std::map<const BasicBlock*, unsigned> BlockIndexMap;
  typedef std::map<const Function*, BlockIndexMap> BlockAddressMap;
  typedef std::map<const BasicBlock*, Block*> LLVMToRelooperMap;

  /// JSWriter - This class is the main chunk of code that converts an LLVM
  /// module to JavaScript.
  class JSWriter : public ModulePass {
    formatted_raw_ostream &Out;
    const Module *TheModule;
    unsigned UniqueNum;
    ValueMap ValueNames;
    VarMap UsedVars;
    AllocaIntMap StackAllocs;
    unsigned TotalStackAllocs;
    HeapData GlobalData8;
    HeapData GlobalData32;
    HeapData GlobalData64;
    GlobalAddressMap GlobalAddresses;
    NameSet Externals; // vars
    NameSet Declares; // funcs
    StringMap Redirects; // library function redirects actually used, needed for wrapper funcs in tables
    std::string PostSets;
    NameIntMap NamedGlobals; // globals that we export as metadata to JS, so it can access them by name
    std::map<std::string, unsigned> IndexedFunctions; // name -> index
    FunctionTableMap FunctionTables; // sig => list of functions
    std::vector<std::string> GlobalInitializers;
    std::vector<std::string> Exports; // additional exports
    BlockAddressMap BlockAddresses;

    bool UsesSIMD;
    int InvokeState; // cycles between 0, 1 after preInvoke, 2 after call, 0 again after postInvoke. hackish, no argument there.
    DataLayout *DL;

    #include "CallHandlers.h"

  public:
    static char ID;
    explicit JSWriter(formatted_raw_ostream &o) : ModulePass(ID), Out(o), UniqueNum(0), UsesSIMD(false), InvokeState(0) {}

    virtual const char *getPassName() const { return "JavaScript backend"; }

    virtual bool runOnModule(Module &M);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
      AU.addRequired<DataLayout>();
      ModulePass::getAnalysisUsage(AU);
    }

    void printProgram(const std::string& fname, const std::string& modName );
    void printModule(const std::string& fname, const std::string& modName );
    void printFunction(const Function *F);

    void error(const std::string& msg);
    
    formatted_raw_ostream& nl(formatted_raw_ostream &Out, int delta = 0);
    
  private:
    void printCommaSeparated(const HeapData v);

    // parsing of constants has two phases: calculate, and then emit
    void parseConstant(const std::string& name, const Constant* CV, bool calculate);

    #define MEM_ALIGN 8
    #define MEM_ALIGN_BITS 64

    unsigned memAlign(unsigned x) {
      return x + (x%MEM_ALIGN != 0 ? MEM_ALIGN - x%MEM_ALIGN : 0);
    }
    std::string memAlignStr(std::string x) {
      return "((" + x + "+" + utostr(MEM_ALIGN-1) + ")&-" + utostr(MEM_ALIGN) + ")";
    }

    HeapData *allocateAddress(const std::string& Name, unsigned Bits = MEM_ALIGN_BITS) {
      assert(Bits == 64); // FIXME when we use optimal alignments
      HeapData *GlobalData = NULL;
      switch (Bits) {
        case 8:  GlobalData = &GlobalData8;  break;
        case 32: GlobalData = &GlobalData32; break;
        case 64: GlobalData = &GlobalData64; break;
        default: llvm_unreachable("Unsupported data element size");
      }
      while (GlobalData->size() % (Bits/8) != 0) GlobalData->push_back(0);
      GlobalAddresses[Name] = Address(GlobalData->size(), Bits);
      return GlobalData;
    }

    #define GLOBAL_BASE 8

    // return the absolute offset of a global
    unsigned getGlobalAddress(const std::string &s) {
      GlobalAddressMap::const_iterator I = GlobalAddresses.find(s);
      if (I == GlobalAddresses.end()) {
        report_fatal_error("cannot find global address " + Twine(s));
      }
      Address a = I->second;
      assert(a.second == 64); // FIXME when we use optimal alignments
      unsigned Ret;
      switch (a.second) {
        case 64:
          assert((a.first + GLOBAL_BASE)%8 == 0);
          Ret = a.first + GLOBAL_BASE;
          break;
        case 32:
          assert((a.first + GLOBAL_BASE)%4 == 0);
          Ret = a.first + GLOBAL_BASE + GlobalData64.size();
          break;
        case 8:
          Ret = a.first + GLOBAL_BASE + GlobalData64.size() + GlobalData32.size();
          break;
        default:
          report_fatal_error("bad global address " + Twine(s) + ": "
                             "count=" + Twine(a.first) + " "
                             "elementsize=" + Twine(a.second));
      }
      if (s == "_ZTVN10__cxxabiv119__pointer_type_infoE" ||
          s == "_ZTVN10__cxxabiv117__class_type_infoE" ||
          s == "_ZTVN10__cxxabiv120__si_class_type_infoE" ||
          s == "_ZTIi" ||
          s == "_ZTIj" ||
          s == "_ZTIl" ||
          s == "_ZTIm" ||
          s == "_ZTIx" ||
          s == "_ZTIy" ||
          s == "_ZTIf" ||
          s == "_ZTId" ||
          s == "_ZTIe" ||
          s == "_ZTIc" ||
          s == "_ZTIa" ||
          s == "_ZTIh" ||
          s == "_ZTIs" ||
          s == "_ZTIt") {
        NamedGlobals[s] = Ret;
      }
      return Ret;
    }
    // returns the internal offset inside the proper block: GlobalData8, 32, 64
    unsigned getRelativeGlobalAddress(const std::string &s) {
      GlobalAddressMap::const_iterator I = GlobalAddresses.find(s);
      if (I == GlobalAddresses.end()) {
        report_fatal_error("cannot find global address " + Twine(s));
      }
      Address a = I->second;
      return a.first;
    }
    char getFunctionSignatureLetter(Type *T) {
      if (T->isVoidTy()) return 'v';
      else if (T->isFloatingPointTy()) {
        if (PreciseF32 && T->isFloatTy()) {
          return 'f';
        } else {
          return 'd';
        }
      } else return 'i';
    }
    std::string getFunctionSignature(const FunctionType *F, const std::string *Name=NULL) {
      if (Name) {
        // special-case some function signatures, because of how we emit code for them FIXME this is hackish
        if (*Name == "_llvm_memcpy_p0i8_p0i8_i32"  || *Name == "_memcpy" ||
            *Name == "_llvm_memset_p0i8_i32"       || *Name == "_memset" ||
            *Name == "_llvm_memmove_p0i8_p0i8_i32" || *Name == "_memmove") {
          return "iiii";
        }
      }
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
      const std::string &Name = getJSName(F);
      if (IndexedFunctions.find(Name) != IndexedFunctions.end()) return IndexedFunctions[Name];
      std::string Sig = getFunctionSignature(F->getFunctionType(), &Name);
      FunctionTable& Table = ensureFunctionTable(F->getFunctionType());
      // use alignment info to avoid unnecessary holes. This is not optimal though,
      // (1) depends on order of appearance, and (2) really just need align for &class::method, see test_polymorph
      unsigned Alignment = F->getAlignment() || 1;
      while (Table.size() % Alignment) Table.push_back("0");
      unsigned Index = Table.size();
      Table.push_back(Name);
      IndexedFunctions[Name] = Index;

      // invoke the callHandler for this, if there is one. the function may only be indexed but never called directly, and we may need to do things in the handler
      CallHandlerMap::const_iterator CH = CallHandlers->find(Name);
      if (CH != CallHandlers->end()) {
        (this->*(CH->second))(NULL, Name, -1);
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

    // Return a constant we are about to write into a global as a numeric offset. If the
    // value is not known at compile time, emit a postSet to that location.
    unsigned getConstAsOffset(const Value *V, unsigned AbsoluteTarget) {
      if (const Function *F = dyn_cast<const Function>(V)) {
        return getFunctionIndex(F);
      } else if (const BlockAddress *BA = dyn_cast<const BlockAddress>(V)) {
        return getBlockAddress(BA);
      } else {
        if (const GlobalValue *GV = dyn_cast<GlobalValue>(V)) {
          if (GV->hasExternalLinkage()) {
            // We don't have a constant to emit here, so we must emit a postSet
            // All postsets are of external values, so they are pointers, hence 32-bit
            std::string Name = getOpName(V);
            Externals.insert(Name);
            PostSets += "HEAP32[" + utostr(AbsoluteTarget>>2) + "] = " + Name + ';';
            return 0; // emit zero in there for now, until the postSet
          }
        }
        return getGlobalAddress(V->getName().str());
      }
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

    void checkVectorType(Type *T) {
      VectorType *VT = cast<VectorType>(T);
      assert(VT->getElementType()->getPrimitiveSizeInBits() == 32);
      assert(VT->getNumElements() == 4);
      UsesSIMD = true;
    }

    std::string getPtrLoad(const Value* Ptr);
    std::string getHeapAccess(const std::string& Name, unsigned Bytes, bool Integer=true);
    std::string getPtrUse(const Value* Ptr);
    std::string getConstant(const Constant*, AsmCast sign=ASM_SIGNED);
    std::string getValueAsStr(const Value*, AsmCast sign=ASM_SIGNED);
    std::string getValueAsCastStr(const Value*, AsmCast sign=ASM_SIGNED);
    std::string getValueAsParenStr(const Value*);
    std::string getValueAsCastParenStr(const Value*, AsmCast sign=ASM_SIGNED);

    const std::string &getJSName(const Value* val);

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

    void addBlock(const BasicBlock *BB, Relooper& R, LLVMToRelooperMap& LLVMToRelooper);
    void printFunctionBody(const Function *F);
    bool generateSIMDExpression(const User *I, raw_string_ostream& Code);
    void generateExpression(const User *I, raw_string_ostream& Code);

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

formatted_raw_ostream &JSWriter::nl(formatted_raw_ostream &Out, int delta) {
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
    return "Math_fround(" + S + ")";
  }
  return S;
}

static void emitDebugInfo(raw_ostream& Code, const Instruction *I) {
  if (MDNode *N = I->getMetadata("dbg")) {
    DILocation Loc(N);
    unsigned Line = Loc.getLineNumber();
    StringRef File = Loc.getFilename();
    Code << " //@line " << utostr(Line) << " \"" << (File.size() > 0 ? File.str() : "?") << "\"";
  }
}

void JSWriter::error(const std::string& msg) {
  report_fatal_error(msg);
}

std::string JSWriter::getPhiCode(const BasicBlock *From, const BasicBlock *To) {
  // FIXME this is all quite inefficient, and also done once per incoming to each phi

  // Find the phis, and generate assignments and dependencies
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
    const std::string &name = getJSName(P);
    assigns[name] = getAssign(P);
    const Value *V = P->getIncomingValue(index);
    values[name] = V;
    std::string vname = getValueAsStr(V);
    if (const Instruction *VI = dyn_cast<const Instruction>(V)) {
      if (VI->getParent() == To) {
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
          pre  += getAdHocAssign(temp, V->getType()) + CV + ';';
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

const std::string &JSWriter::getJSName(const Value* val) {
  ValueMap::const_iterator I = ValueNames.find(val);
  if (I != ValueNames.end() && I->first == val)
    return I->second;

  std::string name;
  if (val->hasName()) {
    if (isa<Function>(val) || isa<Constant>(val)) {
      name = std::string("_") + val->getName().str();
      sanitizeGlobal(name);
    } else {
      name = std::string("$") + val->getName().str();
      sanitizeLocal(name);
    }
  } else {
    name = "u$" + utostr(UniqueNum++);
  }
  return ValueNames[val] = name;
}

std::string JSWriter::getAdHocAssign(const StringRef &s, Type *t) {
  UsedVars[s] = t->getTypeID();
  return (s + " = ").str();
}

std::string JSWriter::getAssign(const Instruction *I) {
  return getAdHocAssign(getJSName(I), I->getType());
}

std::string JSWriter::getAssignIfNeeded(const Value *V) {
  if (const Instruction *I = dyn_cast<Instruction>(V))
    return getAssign(I);
  return std::string();
}

std::string JSWriter::getCast(const StringRef &s, Type *t, AsmCast sign) {
  switch (t->getTypeID()) {
    default: {
      // some types we cannot cast, like vectors - ignore
      if (!t->isVectorTy()) { errs() << *t << "\n"; assert(false && "Unsupported type");}
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
        case 1:  if (!(sign & ASM_NONSPECIFIC)) return (s + "&1").str();
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

std::string JSWriter::getParenCast(const StringRef &s, Type *t, AsmCast sign) {
  return getCast(("(" + s + ")").str(), t, sign);
}

std::string JSWriter::getDoubleToInt(const StringRef &s) {
  return ("~~(" + s + ")").str();
}

std::string JSWriter::getIMul(const Value *V1, const Value *V2) {
  const ConstantInt *CI = NULL;
  const Value *Other = NULL;
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

std::string JSWriter::getLoad(const Instruction *I, const Value *P, Type *T, unsigned Alignment, char sep) {
  std::string Assign = getAssign(I);
  unsigned Bytes = DL->getTypeAllocSize(T);
  std::string text;
  if (Bytes <= Alignment || Alignment == 0) {
    text = Assign + getPtrLoad(P);
    if (isAbsolute(P)) {
      // loads from an absolute constants are either intentional segfaults (int x = *((int*)0)), or code problems
      text += "; abort() /* segfault, load from absolute addr */";
    }
  } else {
    // unaligned in some manner
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
            text = "HEAP8[tempDoublePtr]=HEAP8[" + PS + "]" + sep +
                   "HEAP8[tempDoublePtr+1|0]=HEAP8[" + PS + "+1|0]" + sep +
                   "HEAP8[tempDoublePtr+2|0]=HEAP8[" + PS + "+2|0]" + sep +
                   "HEAP8[tempDoublePtr+3|0]=HEAP8[" + PS + "+3|0]" + sep +
                   "HEAP8[tempDoublePtr+4|0]=HEAP8[" + PS + "+4|0]" + sep +
                   "HEAP8[tempDoublePtr+5|0]=HEAP8[" + PS + "+5|0]" + sep +
                   "HEAP8[tempDoublePtr+6|0]=HEAP8[" + PS + "+6|0]" + sep +
                   "HEAP8[tempDoublePtr+7|0]=HEAP8[" + PS + "+7|0]";
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
              text = Assign + "HEAPU8[" + PS + "]|" +
                             "(HEAPU8[" + PS + "+1|0]<<8)|" +
                             "(HEAPU8[" + PS + "+2|0]<<16)|" +
                             "(HEAPU8[" + PS + "+3|0]<<24)";
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
              text = "HEAP8[tempDoublePtr]=HEAP8[" + PS + "]" + sep +
                     "HEAP8[tempDoublePtr+1|0]=HEAP8[" + PS + "+1|0]" + sep +
                     "HEAP8[tempDoublePtr+2|0]=HEAP8[" + PS + "+2|0]" + sep +
                     "HEAP8[tempDoublePtr+3|0]=HEAP8[" + PS + "+3|0]";
              break;
            }
            default: assert(0 && "bad 4f store");
          }
          text += sep + Assign + getCast("HEAPF32[tempDoublePtr>>2]", Type::getFloatTy(TheModule->getContext()));
        }
        break;
      }
      case 2: {
        text = Assign + "HEAPU8[" + PS + "]|" +
                       "(HEAPU8[" + PS + "+1|0]<<8)";
        break;
      }
      default: assert(0 && "bad store");
    }
  }
  return text;
}

std::string JSWriter::getStore(const Instruction *I, const Value *P, Type *T, const std::string& VS, unsigned Alignment, char sep) {
  assert(sep == ';'); // FIXME when we need that
  unsigned Bytes = DL->getTypeAllocSize(T);
  std::string text;
  if (Bytes <= Alignment || Alignment == 0) {
    text = getPtrUse(P) + " = " + VS;
    if (Alignment == 536870912) text += "; abort() /* segfault */";
  } else {
    // unaligned in some manner
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
            text += "HEAP8[" + PS + "]=HEAP8[tempDoublePtr];" +
                    "HEAP8[" + PS + "+1|0]=HEAP8[tempDoublePtr+1|0];" +
                    "HEAP8[" + PS + "+2|0]=HEAP8[tempDoublePtr+2|0];" +
                    "HEAP8[" + PS + "+3|0]=HEAP8[tempDoublePtr+3|0];" +
                    "HEAP8[" + PS + "+4|0]=HEAP8[tempDoublePtr+4|0];" +
                    "HEAP8[" + PS + "+5|0]=HEAP8[tempDoublePtr+5|0];" +
                    "HEAP8[" + PS + "+6|0]=HEAP8[tempDoublePtr+6|0];" +
                    "HEAP8[" + PS + "+7|0]=HEAP8[tempDoublePtr+7|0]";
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
              text = "HEAP8[" + PS + "]=" + VS + "&255;" +
                     "HEAP8[" + PS + "+1|0]=(" + VS + ">>8)&255;" +
                     "HEAP8[" + PS + "+2|0]=(" + VS + ">>16)&255;" +
                     "HEAP8[" + PS + "+3|0]=" + VS + ">>24";
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
              text += "HEAP8[" + PS + "]=HEAP8[tempDoublePtr];" +
                      "HEAP8[" + PS + "+1|0]=HEAP8[tempDoublePtr+1|0];" +
                      "HEAP8[" + PS + "+2|0]=HEAP8[tempDoublePtr+2|0];" +
                      "HEAP8[" + PS + "+3|0]=HEAP8[tempDoublePtr+3|0]";
              break;
            }
            default: assert(0 && "bad 4f store");
          }
        }
        break;
      }
      case 2: {
        text = "HEAP8[" + PS + "]=" + VS + "&255;" +
               "HEAP8[" + PS + "+1|0]=" + VS + ">>8";
        break;
      }
      default: assert(0 && "bad store");
    }
  }
  return text;
}

std::string JSWriter::getOpName(const Value* V) { // TODO: remove this
  return getJSName(V);
}

std::string JSWriter::getPtrLoad(const Value* Ptr) {
  Type *t = cast<PointerType>(Ptr->getType())->getElementType();
  return getCast(getPtrUse(Ptr), t, ASM_NONSPECIFIC);
}

std::string JSWriter::getHeapAccess(const std::string& Name, unsigned Bytes, bool Integer) {
  switch (Bytes) {
  default: llvm_unreachable("Unsupported type");
  case 8: return "HEAPF64[" + Name + ">>3]";
  case 4: {
    if (Integer) {
      return "HEAP32[" + Name + ">>2]";
    } else {
      return "HEAPF32[" + Name + ">>2]";
    }
  }
  case 2: return "HEAP16[" + Name + ">>1]";
  case 1: return "HEAP8[" + Name + "]";
  }
}

std::string JSWriter::getPtrUse(const Value* Ptr) {
  Type *t = cast<PointerType>(Ptr->getType())->getElementType();
  unsigned Bytes = DL->getTypeAllocSize(t);
  if (const GlobalVariable *GV = dyn_cast<GlobalVariable>(Ptr)) {
    std::string text = "";
    unsigned Addr = getGlobalAddress(GV->getName().str());
    switch (Bytes) {
    default: llvm_unreachable("Unsupported type");
    case 8: return "HEAPF64[" + utostr(Addr >> 3) + "]";
    case 4: {
      if (t->isIntegerTy() || t->isPointerTy()) {
        return "HEAP32[" + utostr(Addr >> 2) + "]";
      } else {
        assert(t->isFloatingPointTy());
        return "HEAPF32[" + utostr(Addr >> 2) + "]";
      }
    }
    case 2: return "HEAP16[" + utostr(Addr >> 1) + "]";
    case 1: return "HEAP8[" + utostr(Addr) + "]";
    }
  } else {
    return getHeapAccess(getValueAsStr(Ptr), Bytes, t->isIntegerTy() || t->isPointerTy());
  }
}

static inline std::string ftostr(const ConstantFP *CFP) {
  const APFloat &flt = CFP->getValueAPF();

  // Emscripten has its own spellings for infinity and NaN.
  if (flt.getCategory() == APFloat::fcInfinity) return flt.isNegative() ? "-inf" : "inf";
  else if (flt.getCategory() == APFloat::fcNaN) return "nan";

  // Request 21 digits, aka DECIMAL_DIG, to avoid rounding errors.
  SmallString<29> Str;
  flt.toString(Str, 21);

  // asm.js considers literals to be floating-point literals when they contain a
  // dot, however our output may be processed by UglifyJS, which doesn't
  // currently preserve dots in all cases. Mark floating-point literals with
  // unary plus to force them to floating-point.
  if (APFloat(flt).roundToIntegral(APFloat::rmNearestTiesToEven) == APFloat::opOK) {
    return '+' + Str.str().str();
  }

  return Str.str().str();
}

std::string JSWriter::getConstant(const Constant* CV, AsmCast sign) {
  if (isa<ConstantPointerNull>(CV)) return "0";

  if (const Function *F = dyn_cast<Function>(CV)) {
    return utostr(getFunctionIndex(F));
  }

  if (const GlobalValue *GV = dyn_cast<GlobalValue>(CV)) {
    if (GV->isDeclaration()) {
      std::string Name = getOpName(GV);
      Externals.insert(Name);
      return Name;
    }
    return utostr(getGlobalAddress(GV->getName().str()));
  }

  if (const ConstantFP *CFP = dyn_cast<ConstantFP>(CV)) {
    std::string S = ftostr(CFP);
    if (PreciseF32 && CV->getType()->isFloatTy() && !(sign & ASM_FFI_OUT)) {
      S = "Math_fround(" + S + ")";
    }
    return S;
  } else if (const ConstantInt *CI = dyn_cast<ConstantInt>(CV)) {
    if (sign != ASM_UNSIGNED && CI->getValue().getBitWidth() == 1) {
      sign = ASM_UNSIGNED; // bools must always be unsigned: either 0 or 1
    }
    return CI->getValue().toString(10, sign != ASM_UNSIGNED);
  } else if (isa<UndefValue>(CV)) {
    std::string S = CV->getType()->isFloatingPointTy() ? "+0" : "0"; // XXX refactor this
    if (PreciseF32 && CV->getType()->isFloatTy() && !(sign & ASM_FFI_OUT)) {
      S = "Math_fround(" + S + ")";
    }
    return S;
  } else if (isa<ConstantAggregateZero>(CV)) {
    if (VectorType *VT = dyn_cast<VectorType>(CV->getType())) {
      if (VT->getElementType()->isIntegerTy()) {
        return "int32x4.splat(0)";
      } else {
        return "float32x4.splat(0)";
      }
    } else {
      // something like [0 x i8*] zeroinitializer, which clang can emit for landingpads
      return "0";
    }
  } else if (const ConstantDataVector *DV = dyn_cast<ConstantDataVector>(CV)) {
    const VectorType *VT = cast<VectorType>(CV->getType());
    if (VT->getElementType()->isIntegerTy()) {
      return "int32x4(" + getConstant(DV->getElementAsConstant(0)) + ',' +
                          getConstant(DV->getElementAsConstant(1)) + ',' +
                          getConstant(DV->getElementAsConstant(2)) + ',' +
                          getConstant(DV->getElementAsConstant(3)) + ')';
    } else {
      return "float32x4(" + getConstant(DV->getElementAsConstant(0)) + ',' +
                            getConstant(DV->getElementAsConstant(1)) + ',' +
                            getConstant(DV->getElementAsConstant(2)) + ',' +
                            getConstant(DV->getElementAsConstant(3)) + ')';
    }
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
    generateExpression(CE, CodeStream);
    CodeStream << ')';
    return CodeStream.str();
  } else {
    CV->dump();
    llvm_unreachable("Unsupported constant kind");
  }
}

std::string JSWriter::getValueAsStr(const Value* V, AsmCast sign) {
  if (const Constant *CV = dyn_cast<Constant>(V)) {
    return getConstant(CV, sign);
  } else {
    return getJSName(V);
  }
}

std::string JSWriter::getValueAsCastStr(const Value* V, AsmCast sign) {
  if (isa<ConstantInt>(V) || isa<ConstantFP>(V)) {
    return getConstant(cast<Constant>(V), sign);
  } else {
    return getCast(getValueAsStr(V), V->getType(), sign);
  }
}

std::string JSWriter::getValueAsParenStr(const Value* V) {
  if (const Constant *CV = dyn_cast<Constant>(V)) {
    return getConstant(CV);
  } else {
    return "(" + getValueAsStr(V) + ")";
  }
}

std::string JSWriter::getValueAsCastParenStr(const Value* V, AsmCast sign) {
  if (isa<ConstantInt>(V) || isa<ConstantFP>(V)) {
    return getConstant(cast<Constant>(V), sign);
  } else {
    return "(" + getCast(getValueAsStr(V), V->getType(), sign) + ")";
  }
}

bool JSWriter::generateSIMDExpression(const User *I, raw_string_ostream& Code) {
  VectorType *VT;
  if ((VT = dyn_cast<VectorType>(I->getType()))) {
    // vector-producing instructions
    checkVectorType(VT);

    switch (Operator::getOpcode(I)) {
      default: I->dump(); error("invalid vector instr"); break;
      case Instruction::FAdd: Code << getAssignIfNeeded(I) << "SIMD.float32x4.add(" << getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")"; break;
      case Instruction::FMul: Code << getAssignIfNeeded(I) << "SIMD.float32x4.mul(" << getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")"; break;
      case Instruction::FDiv: Code << getAssignIfNeeded(I) << "SIMD.float32x4.div(" << getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")"; break;
      case Instruction::Add: Code << getAssignIfNeeded(I) << "SIMD.int32x4.add(" << getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")"; break;
      case Instruction::Sub: Code << getAssignIfNeeded(I) << "SIMD.int32x4.sub(" << getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")"; break;
      case Instruction::Mul: Code << getAssignIfNeeded(I) << "SIMD.int32x4.mul(" << getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")"; break;
      case Instruction::And: Code << getAssignIfNeeded(I) << "SIMD.int32x4.and(" << getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")"; break;
      case Instruction::Or:  Code << getAssignIfNeeded(I) << "SIMD.int32x4.or(" <<  getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")"; break;
      case Instruction::Xor: Code << getAssignIfNeeded(I) << "SIMD.int32x4.xor(" << getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")"; break;
      case Instruction::FSub:
        // LLVM represents an fneg(x) as -0.0 - x.
        Code << getAssignIfNeeded(I);
        if (BinaryOperator::isFNeg(I)) {
          Code << "SIMD.float32x4.neg(" << getValueAsStr(BinaryOperator::getFNegArgument(I)) << ")";
        } else {
          Code << "SIMD.float32x4.sub(" << getValueAsStr(I->getOperand(0)) << "," << getValueAsStr(I->getOperand(1)) << ")";
        }
        break;
      case Instruction::BitCast: {
        Code << getAssignIfNeeded(I);
        if (cast<VectorType>(I->getType())->getElementType()->isIntegerTy()) {
          Code << "SIMD.float32x4.bitsToInt32x4(" << getValueAsStr(I->getOperand(0)) << ')';
        } else {
          Code << "SIMD.int32x4.bitsToInt32x4(" << getValueAsStr(I->getOperand(0)) << ')';
        }
        break;
      }
      case Instruction::Load: {
        const LoadInst *LI = cast<LoadInst>(I);
        const Value *P = LI->getPointerOperand();
        std::string PS = getValueAsStr(P);
        Code << getAssignIfNeeded(I);
        if (VT->getElementType()->isIntegerTy()) {
          Code << "int32x4(HEAPU32[" << PS << ">>2],HEAPU32[" << PS << "+4>>2],HEAPU32[" << PS << "+8>>2],HEAPU32[" << PS << "+12>>2])";
        } else {
          Code << "float32x4(HEAPF32[" << PS << ">>2],HEAPF32[" << PS << "+4>>2],HEAPF32[" << PS << "+8>>2],HEAPF32[" << PS << "+12>>2])";
        }
        break;
      }
      case Instruction::InsertElement: {
        const InsertElementInst *III = cast<InsertElementInst>(I);
        const ConstantInt *IndexInt = cast<const ConstantInt>(III->getOperand(2));
        unsigned Index = IndexInt->getZExtValue();
        assert(Index <= 3);
        Code << getAssignIfNeeded(I);
        if (VT->getElementType()->isIntegerTy()) {
          Code << "SIMD.int32x4.with";
        } else {
          Code << "SIMD.float32x4.with";
        }
        Code << SIMDLane[Index];
        Code << "(" << getValueAsStr(III->getOperand(0)) << ',' << getValueAsStr(III->getOperand(1)) << ')';
        break;
      }
      case Instruction::ShuffleVector: {
        Code << getAssignIfNeeded(I);
        if (VT->getElementType()->isIntegerTy()) {
          Code << "int32x4(";
        } else {
          Code << "float32x4(";
        }
        const ShuffleVectorInst *SVI = cast<ShuffleVectorInst>(I);
        std::string A = getValueAsStr(I->getOperand(0));
        std::string B = getValueAsStr(I->getOperand(1));
        for (unsigned int i = 0; i < 4; i++) {
          int Mask = SVI->getMaskValue(i);
          if (Mask < 0) {
            Code << "0";
          } else if (Mask < 4) {
            Code << A << "." << simdLane[Mask];
          } else {
            assert(Mask < 8);
            Code << B << "." << simdLane[Mask-4];
          }
          if (i < 3) Code << ",";
        }
        Code << ')';
        break;
      }
    }
    return true;
  } else {
    // vector-consuming instructions
    if (Operator::getOpcode(I) == Instruction::Store && (VT = dyn_cast<VectorType>(I->getOperand(0)->getType())) && VT->isVectorTy()) {
      checkVectorType(VT);
      const StoreInst *SI = cast<StoreInst>(I);
      const Value *P = SI->getPointerOperand();
      std::string PS = getOpName(P);
      std::string VS = getValueAsStr(SI->getValueOperand());
      Code << getAdHocAssign(PS, P->getType()) << getValueAsStr(P) << ';';
      if (VT->getElementType()->isIntegerTy()) {
        Code << "HEAPU32[" << PS << ">>2]=" << VS << ".x;HEAPU32[" << PS << "+4>>2]=" << VS << ".y;HEAPU32[" << PS << "+8>>2]=" << VS << ".z;HEAPU32[" << PS << "+12>>2]=" << VS << ".w";
      } else {
        Code << "HEAPF32[" << PS << ">>2]=" << VS << ".x;HEAPF32[" << PS << "+4>>2]=" << VS << ".y;HEAPF32[" << PS << "+8>>2]=" << VS << ".z;HEAPF32[" << PS << "+12>>2]=" << VS << ".w";
      }
      return true;
    } else if (Operator::getOpcode(I) == Instruction::ExtractElement) {
      const ExtractElementInst *EEI = cast<ExtractElementInst>(I);
      VT = cast<VectorType>(EEI->getVectorOperand()->getType());
      checkVectorType(VT);
      const ConstantInt *IndexInt = cast<const ConstantInt>(EEI->getIndexOperand());
      unsigned Index = IndexInt->getZExtValue();
      assert(Index <= 3);
      Code << getAssignIfNeeded(I);
      Code << getValueAsStr(EEI->getVectorOperand()) << '.' << simdLane[Index];
      return true;
    }
  }
  return false;
}

static uint64_t LSBMask(unsigned numBits) {
  return numBits >= 64 ? 0xFFFFFFFFFFFFFFFFULL : (1ULL << numBits) - 1;
}

// Generate code for and operator, either an Instruction or a ConstantExpr.
void JSWriter::generateExpression(const User *I, raw_string_ostream& Code) {
  Type *T = I->getType();
  if (T->isIntegerTy() && T->getIntegerBitWidth() > 32) {
    errs() << *I << "\n";
    report_fatal_error("legalization problem");
  }

  if (!generateSIMDExpression(I, Code)) switch (Operator::getOpcode(I)) {
  default: {
    I->dump();
    error("Invalid instruction");
    break;
  }
  case Instruction::Ret: {
    const ReturnInst* ret =  cast<ReturnInst>(I);
    const Value *RV = ret->getReturnValue();
    Code << "STACKTOP = sp;";
    Code << "return";
    if (RV != NULL) {
      Code << " " << getValueAsCastParenStr(RV, ASM_NONSPECIFIC);
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
    Code << getAssignIfNeeded(I);
    switch (cast<FCmpInst>(I)->getPredicate()) {
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
    unsigned predicate = isa<ConstantExpr>(I) ?
                         cast<ConstantExpr>(I)->getPredicate() :
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
    if (NativizedVars.count(I)) {
      // nativized stack variable, we just need a 'var' definition
      UsedVars[getJSName(I)] = cast<PointerType>(I->getType())->getElementType()->getTypeID();
      return;
    }
    const AllocaInst* AI = cast<AllocaInst>(I);
    AllocaIntMap::iterator AIMI = StackAllocs.find(AI);
    if (AIMI != StackAllocs.end()) {
      // fixed-size allocation that is already taken into account in the big initial allocation
      if (AIMI->second) {
        Code << getAssign(AI) << "sp + " << utostr(AIMI->second) << "|0";
      } else {
        Code << getAssign(AI) << "sp";
      }
      break;
    }
    Type *T = AI->getAllocatedType();
    std::string Size;
    uint64_t BaseSize = DL->getTypeAllocSize(T);
    const Value *AS = AI->getArraySize();
    if (const ConstantInt *CI = dyn_cast<ConstantInt>(AS)) {
      Size = Twine(memAlign(BaseSize * CI->getZExtValue())).str();
    } else {
      Size = memAlignStr("((" + utostr(BaseSize) + '*' + getValueAsStr(AS) + ")|0)");
    }
    Code << getAssign(AI) << "STACKTOP; STACKTOP = STACKTOP + " << Size << "|0";
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
    std::string text = getValueAsParenStr(GEP->getPointerOperand());
    for (GetElementPtrInst::const_op_iterator I = llvm::next(GEP->op_begin()),
                                              E = GEP->op_end();
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
          ConstantOffset = (uint32_t)ConstantOffset + (uint32_t)CI->getSExtValue() * ElementSize;
        } else {
          text = "(" + text + " + (" + getIMul(Index, ConstantInt::get(Type::getInt32Ty(GEP->getContext()), ElementSize)) + ")|0)";
        }
      }
    }
    if (ConstantOffset != 0) {
      text = "(" + text + " + " + itostr(ConstantOffset) + "|0)";
    }
    Code << text;
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
    case Instruction::ZExt:     Code << getValueAsCastStr(I->getOperand(0), ASM_UNSIGNED); break;
    case Instruction::FPExt: {
      if (PreciseF32) {
        Code << "+" + getValueAsStr(I->getOperand(0)); break;
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
    case Instruction::UIToFP:   Code << '('<< getCast(getValueAsCastParenStr(I->getOperand(0), ASM_UNSIGNED), I->getType()) << ')'; break;
    case Instruction::FPToSI:   Code << '('<< getDoubleToInt(getValueAsParenStr(I->getOperand(0))) << ')'; break;
    case Instruction::FPToUI:   Code << '('<< getCast(getDoubleToInt(getValueAsParenStr(I->getOperand(0))), I->getType(), ASM_UNSIGNED) << ')'; break;
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
      Code << "(HEAP32[tempDoublePtr>>2]=" << V << "," << getCast("HEAPF32[tempDoublePtr>>2]", Type::getFloatTy(TheModule->getContext())) + ")";
    } else if (OutType->isIntegerTy() && InType->isFloatingPointTy()) {
      assert(OutType->getIntegerBitWidth() == 32);
      Code << "(HEAPF32[tempDoublePtr>>2]=" << V << "," << "HEAP32[tempDoublePtr>>2]|0)";
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
    const SelectInst* SI = cast<SelectInst>(I);
    Code << getAssignIfNeeded(I) << getValueAsStr(SI->getCondition()) << " ? " <<
                                    getValueAsStr(SI->getTrueValue()) << " : " <<
                                    getValueAsStr(SI->getFalseValue());
    break;
  }
  case Instruction::AtomicCmpXchg: {
    const AtomicCmpXchgInst *cxi = cast<AtomicCmpXchgInst>(I);
    const Value *P = I->getOperand(0);
    Code << getLoad(cxi, P, I->getType(), 0) << ';' <<
           "if ((" << getCast(getJSName(I), I->getType()) << ") == " << getValueAsCastParenStr(I->getOperand(1)) << ") " <<
              getStore(cxi, P, I->getType(), getValueAsStr(I->getOperand(2)), 0);
    break;
  }
  case Instruction::AtomicRMW: {
    const AtomicRMWInst *rmwi = cast<AtomicRMWInst>(I);
    const Value *P = rmwi->getOperand(0);
    const Value *V = rmwi->getOperand(1);
    std::string VS = getValueAsStr(V);
    Code << getLoad(rmwi, P, I->getType(), 0) << ';';
    // Most bitcasts are no-ops for us. However, the exception is int to float and float to int
    switch (rmwi->getOperation()) {
      case AtomicRMWInst::Xchg: Code << getStore(rmwi, P, I->getType(), VS, 0); break;
      case AtomicRMWInst::Add:  Code << getStore(rmwi, P, I->getType(), "((" + getJSName(I) + '+' + VS + ")|0)", 0); break;
      case AtomicRMWInst::Sub:  Code << getStore(rmwi, P, I->getType(), "((" + getJSName(I) + '-' + VS + ")|0)", 0); break;
      case AtomicRMWInst::And:  Code << getStore(rmwi, P, I->getType(), "(" + getJSName(I) + '&' + VS + ")", 0); break;
      case AtomicRMWInst::Nand: Code << getStore(rmwi, P, I->getType(), "(~(" + getJSName(I) + '&' + VS + "))", 0); break;
      case AtomicRMWInst::Or:   Code << getStore(rmwi, P, I->getType(), "(" + getJSName(I) + '|' + VS + ")", 0); break;
      case AtomicRMWInst::Xor:  Code << getStore(rmwi, P, I->getType(), "(" + getJSName(I) + '^' + VS + ")", 0); break;
      case AtomicRMWInst::Max:
      case AtomicRMWInst::Min:
      case AtomicRMWInst::UMax:
      case AtomicRMWInst::UMin:
      case AtomicRMWInst::BAD_BINOP: llvm_unreachable("Bad atomic operation");
    }
    break;
  }
  case Instruction::Fence: // no threads, so nothing to do here
    Code << "/* fence */";
    break;
  }

  if (const Instruction *Inst = dyn_cast<Instruction>(I)) {
    Code << ';';
    // append debug info
    emitDebugInfo(Code, Inst);
    Code << '\n';
  }
}

// Checks whether to use a condition variable. We do so for switches and for indirectbrs
static const Value *considerConditionVar(const Instruction *I) {
  if (const IndirectBrInst *IB = dyn_cast<const IndirectBrInst>(I)) {
    return IB->getAddress();
  }
  const SwitchInst *SI = dyn_cast<SwitchInst>(I);
  if (!SI) return NULL;
  // use a switch if the range is not too big or sparse
  int Minn = INT_MAX, Maxx = INT_MIN, Num = 0;
  for (SwitchInst::ConstCaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i) {
    const IntegersSubset CaseVal = i.getCaseValueEx();
    assert(CaseVal.isSingleNumbersOnly());
    std::string Condition = "";
    for (unsigned Index = 0; Index < CaseVal.getNumItems(); Index++) {
      int Curr = CaseVal.getSingleNumber(Index).toConstantInt()->getSExtValue();
      if (Curr < Minn) Minn = Curr;
      if (Curr > Maxx) Maxx = Curr;
    }
    Num++;
  }
  int64_t Range = (int64_t)Maxx - (int64_t)Minn;
  return Num < 5 || Range > 10*1024 || (Range/Num) > 1024 ? NULL : SI->getCondition(); // heuristics
}

void JSWriter::addBlock(const BasicBlock *BB, Relooper& R, LLVMToRelooperMap& LLVMToRelooper) {
  std::string Code;
  raw_string_ostream CodeStream(Code);
  for (BasicBlock::const_iterator I = BB->begin(), E = BB->end();
       I != E; ++I) {
    generateExpression(I, CodeStream);
  }
  CodeStream.flush();
  const Value* Condition = considerConditionVar(BB->getTerminator());
  Block *Curr = new Block(Code.c_str(), Condition ? getValueAsCastStr(Condition).c_str() : NULL);
  LLVMToRelooper[BB] = Curr;
  R.AddBlock(Curr);
}

void JSWriter::printFunctionBody(const Function *F) {
  assert(!F->isDeclaration());

  // Prepare relooper
  Relooper::MakeOutputBuffer(1024*1024);
  Relooper R;
  //if (!canReloop(F)) R.SetEmulate(true);
  R.SetAsmJSMode(1);
  Block *Entry = NULL;
  LLVMToRelooperMap LLVMToRelooper;

  // Create relooper blocks with their contents. TODO: We could optimize
  // indirectbr by emitting indexed blocks first, so their indexes
  // match up with the label index.
  for (Function::const_iterator BI = F->begin(), BE = F->end();
       BI != BE; ++BI) {
    addBlock(BI, R, LLVMToRelooper);
    if (!Entry) Entry = LLVMToRelooper[BI];
  }
  assert(Entry);

  // Create branchings
  for (Function::const_iterator BI = F->begin(), BE = F->end();
       BI != BE; ++BI) {
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
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S0], getValueAsStr(TI->getOperand(0)).c_str(), P0.size() > 0 ? P0.c_str() : NULL);
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S1], NULL,                                     P1.size() > 0 ? P1.c_str() : NULL);
        } else if (br->getNumOperands() == 1) {
          BasicBlock *S = br->getSuccessor(0);
          std::string P = getPhiCode(&*BI, S);
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S], NULL, P.size() > 0 ? P.c_str() : NULL);
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
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S], Target.size() > 0 ? Target.c_str() : NULL, P.size() > 0 ? P.c_str() : NULL);
        }
        break;
      }
      case Instruction::Switch: {
        const SwitchInst* SI = cast<SwitchInst>(TI);
        bool UseSwitch = !!considerConditionVar(SI);
        BasicBlock *DD = SI->getDefaultDest();
        std::string P = getPhiCode(&*BI, DD);
        LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*DD], NULL, P.size() > 0 ? P.c_str() : NULL);
        typedef std::map<const BasicBlock*, std::string> BlockCondMap;
        BlockCondMap BlocksToConditions;
        for (SwitchInst::ConstCaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i) {
          const BasicBlock *BB = i.getCaseSuccessor();
          const IntegersSubset CaseVal = i.getCaseValueEx();
          assert(CaseVal.isSingleNumbersOnly());
          std::string Condition = "";
          for (unsigned Index = 0; Index < CaseVal.getNumItems(); Index++) {
            std::string Curr = CaseVal.getSingleNumber(Index).toConstantInt()->getValue().toString(10, true);
            if (UseSwitch) {
              Condition += "case " + Curr + ": ";
            } else {
              if (Condition.size() > 0) Condition += " | ";
              Condition += "(" + getValueAsCastParenStr(SI->getCondition()) + " == " + Curr + ")";
            }
          }
          BlocksToConditions[BB] = Condition + (!UseSwitch && BlocksToConditions[BB].size() > 0 ? " | " : "") + BlocksToConditions[BB];
        }
        for (BlockCondMap::const_iterator I = BlocksToConditions.begin(), E = BlocksToConditions.end(); I != E; ++I) {
          const BasicBlock *BB = I->first;
          std::string P = getPhiCode(&*BI, BB);
          LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*BB], I->second.c_str(), P.size() > 0 ? P.c_str() : NULL);
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
  UsedVars["sp"] = Type::IntegerTyID;
  UsedVars["label"] = Type::IntegerTyID;
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
      switch (VI->second) {
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
        case Type::VectorTyID:
          Out << "0"; // best we can do for now
          break;
      }
    }
    Out << ";";
    nl(Out);
  }

  // Emit stack entry
  Out << " " << getAdHocAssign("sp", Type::getInt32Ty(F->getContext())) << "STACKTOP;";
  if (TotalStackAllocs) {
    Out << "\n " << "STACKTOP = STACKTOP + " + utostr(TotalStackAllocs) + "|0;";
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
      Out << " return " << getCast("0", RT, ASM_NONSPECIFIC) << ";\n";
    }
  }
}

void JSWriter::processConstants() {
  // First, calculate the address of each constant
  for (Module::const_global_iterator I = TheModule->global_begin(),
         E = TheModule->global_end(); I != E; ++I) {
    if (I->hasInitializer()) {
      parseConstant(I->getName().str(), I->getInitializer(), true);
    }
  }
  // Second, allocate their contents
  for (Module::const_global_iterator I = TheModule->global_begin(),
         E = TheModule->global_end(); I != E; ++I) {
    if (I->hasInitializer()) {
      parseConstant(I->getName().str(), I->getInitializer(), false);
    }
  }
}

void JSWriter::printFunction(const Function *F) {
  ValueNames.clear();

  // Ensure all arguments and locals are named (we assume used values need names, which might be false if the optimizer did not run)
  unsigned Next = 1;
  for (Function::const_arg_iterator AI = F->arg_begin(), AE = F->arg_end();
       AI != AE; ++AI) {
    if (!AI->hasName() && AI->hasNUsesOrMore(1)) {
      ValueNames[AI] = "$" + utostr(Next++);
    }
  }
  for (Function::const_iterator BI = F->begin(), BE = F->end();
       BI != BE; ++BI) {
    for (BasicBlock::const_iterator II = BI->begin(), E = BI->end();
         II != E; ++II) {
      if (!II->hasName() && II->hasNUsesOrMore(1)) {
        ValueNames[II] = "$" + utostr(Next++);
      }
    }
  }

  // Prepare and analyze function

  UsedVars.clear();
  UniqueNum = 0;
  calculateNativizedVars(F);

  StackAllocs.clear();
  TotalStackAllocs = 0;

  for (Function::const_iterator BI = F->begin(), BE = F->end(); BI != BE; ++BI) {
    for (BasicBlock::const_iterator II = BI->begin(), E = BI->end(); II != E; ++II) {
      if (const AllocaInst* AI = dyn_cast<AllocaInst>(II)) {
        Type *T = AI->getAllocatedType();
        const Value *AS = AI->getArraySize();
        unsigned BaseSize = DL->getTypeAllocSize(T);
        if (const ConstantInt *CI = dyn_cast<ConstantInt>(AS)) {
          // TODO: group by alignment to avoid unnecessary padding
          unsigned Size = memAlign(BaseSize * CI->getZExtValue());
          StackAllocs[AI] = TotalStackAllocs;
          TotalStackAllocs += Size;
        }
      } else {
        // stop after the first non-alloca - could alter the stack
        // however, ptrtoints are ok, and the legalizaton passes introduce them
        if (!isa<PtrToIntInst>(II)) break;
      }
    }
    break;
  }

  // Emit the function

  std::string Name = F->getName();
  sanitizeGlobal(Name);
  Out << "function _" << Name << "(";
  for (Function::const_arg_iterator AI = F->arg_begin(), AE = F->arg_end();
       AI != AE; ++AI) {
    if (AI != F->arg_begin()) Out << ",";
    Out << getJSName(AI);
  }
  Out << ") {";
  nl(Out);
  for (Function::const_arg_iterator AI = F->arg_begin(), AE = F->arg_end();
       AI != AE; ++AI) {
    std::string name = getJSName(AI);
    Out << " " << name << " = " << getCast(name, AI->getType(), ASM_NONSPECIFIC) << ";";
    nl(Out);
  }
  printFunctionBody(F);
  Out << "}";
  nl(Out);
}

void JSWriter::printModuleBody() {
  processConstants();

  // Emit function bodies.
  nl(Out) << "// EMSCRIPTEN_START_FUNCTIONS"; nl(Out);
  for (Module::const_iterator I = TheModule->begin(), E = TheModule->end();
       I != E; ++I) {
    // Ignore intrinsics that are always no-ops. We don't emit any code for
    // them, so we don't need to declare them.
    if (I->isIntrinsic()) {
      switch (I->getIntrinsicID()) {
      case Intrinsic::dbg_declare:
      case Intrinsic::dbg_value:
      case Intrinsic::lifetime_start:
      case Intrinsic::lifetime_end:
      case Intrinsic::invariant_start:
      case Intrinsic::invariant_end:
      case Intrinsic::prefetch:
        continue;
      }
    }

    if (!I->isDeclaration()) printFunction(I);
  }
  Out << "function runPostSets() {\n";
  Out << " " << PostSets << "\n";
  Out << "}\n";
  PostSets = "";
  Out << "// EMSCRIPTEN_END_FUNCTIONS\n\n";

  assert(GlobalData32.size() == 0 && GlobalData8.size() == 0); // FIXME when we use optimal constant alignments

  // TODO fix commas
  Out << "/* memory initializer */ allocate([";
  printCommaSeparated(GlobalData64);
  if (GlobalData64.size() > 0 && GlobalData32.size() + GlobalData8.size() > 0) {
    Out << ",";
  }
  printCommaSeparated(GlobalData32);
  if (GlobalData32.size() > 0 && GlobalData8.size() > 0) {
    Out << ",";
  }
  printCommaSeparated(GlobalData8);
  Out << "], \"i8\", ALLOC_NONE, Runtime.GLOBAL_BASE);";

  // Emit metadata for emcc driver
  Out << "\n\n// EMSCRIPTEN_METADATA\n";
  Out << "{\n";

  Out << "\"declares\": [";
  bool first = true;
  for (Module::const_iterator I = TheModule->begin(), E = TheModule->end();
       I != E; ++I) {
    if (I->isDeclaration() && I->hasNUsesOrMore(1)) {
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
      Out << "\"_" << I->getName() << '"';
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

  Out << "\"simd\": ";
  Out << (UsesSIMD ? "1" : "0");
  Out << ",";

  Out << "\"namedGlobals\": {";
  first = true;
  for (NameIntMap::const_iterator I = NamedGlobals.begin(), E = NamedGlobals.end(); I != E; ++I) {
    if (first) {
      first = false;
    } else {
      Out << ", ";
    }
    Out << "\"_" << I->first << "\": \"" << utostr(I->second) << "\"";
  }
  Out << "}";

  Out << "\n}\n";
}

void JSWriter::parseConstant(const std::string& name, const Constant* CV, bool calculate) {
  if (isa<GlobalValue>(CV))
    return;
  //errs() << "parsing constant " << name << "\n";
  // TODO: we repeat some work in both calculate and emit phases here
  // FIXME: use the proper optimal alignments
  if (const ConstantDataSequential *CDS =
         dyn_cast<ConstantDataSequential>(CV)) {
    assert(CDS->isString());
    if (calculate) {
      HeapData *GlobalData = allocateAddress(name);
      StringRef Str = CDS->getAsString();
      for (unsigned int i = 0; i < Str.size(); i++) {
        GlobalData->push_back(Str.data()[i]);
      }
    }
  } else if (const ConstantFP *CFP = dyn_cast<ConstantFP>(CV)) {
    APFloat APF = CFP->getValueAPF();
    if (CFP->getType() == Type::getFloatTy(CFP->getContext())) {
      if (calculate) {
        HeapData *GlobalData = allocateAddress(name);
        union flt { float f; unsigned char b[sizeof(float)]; } flt;
        flt.f = APF.convertToFloat();
        for (unsigned i = 0; i < sizeof(float); ++i) {
          GlobalData->push_back(flt.b[i]);
        }
      }
    } else if (CFP->getType() == Type::getDoubleTy(CFP->getContext())) {
      if (calculate) {
        HeapData *GlobalData = allocateAddress(name);
        union dbl { double d; unsigned char b[sizeof(double)]; } dbl;
        dbl.d = APF.convertToDouble();
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
      HeapData *GlobalData = allocateAddress(name);
      // assuming compiler is little endian
      for (unsigned i = 0; i < BitWidth / 8; ++i) {
        GlobalData->push_back(integer.b[i]);
      }
    }
  } else if (isa<ConstantPointerNull>(CV)) {
    assert(false && "Unlowered ConstantPointerNull");
  } else if (isa<ConstantAggregateZero>(CV)) {
    if (calculate) {
      unsigned Bytes = DL->getTypeStoreSize(CV->getType());
      // FIXME: assume full 64-bit alignment for now
      Bytes = memAlign(Bytes);
      HeapData *GlobalData = allocateAddress(name);
      for (unsigned i = 0; i < Bytes; ++i) {
        GlobalData->push_back(0);
      }
      // FIXME: create a zero section at the end, avoid filling meminit with zeros
    }
  } else if (const ConstantArray *CA = dyn_cast<ConstantArray>(CV)) {
    if (calculate) {
      for (Constant::const_use_iterator UI = CV->use_begin(), UE = CV->use_end(); UI != UE; ++UI) {
        assert((*UI)->getName() == "llvm.used"); // llvm.used is acceptable (and can be ignored)
      }
      // export the kept-alives
      for (unsigned i = 0; i < CA->getNumOperands(); i++) {
        const Constant *C = CA->getOperand(i);
        if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
          C = CE->getOperand(0); // ignore bitcasts
        }
        Exports.push_back(getJSName(C));
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
          GlobalInitializers.push_back(getJSName(C));
        }
      }
    } else if (calculate) {
      HeapData *GlobalData = allocateAddress(name);
      unsigned Bytes = DL->getTypeStoreSize(CV->getType());
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
          assert(Offset+4 <= GlobalData64.size());
          for (unsigned i = 0; i < 4; ++i) {
            GlobalData64[Offset++] = integer.b[i];
          }
        } else if (const ConstantDataSequential *CDS = dyn_cast<ConstantDataSequential>(C)) {
          assert(CDS->isString());
          StringRef Str = CDS->getAsString();
          assert(Offset+Str.size() <= GlobalData64.size());
          for (unsigned int i = 0; i < Str.size(); i++) {
            GlobalData64[Offset++] = Str.data()[i];
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
        GlobalInitializers.push_back(getJSName(V));
        // is the func
      }
    } else if (name == "__fini_array_start") {
      // nothing to do
    } else {
      // a global equal to a ptrtoint of some function, so a 32-bit integer for us
      if (calculate) {
        HeapData *GlobalData = allocateAddress(name);
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
        V = GetPointerBaseWithConstantOffset(V, BaseOffset, DL);
        Data += (uint64_t)BaseOffset;

        Data += getConstAsOffset(V, getGlobalAddress(name));
        union { unsigned i; unsigned char b[sizeof(unsigned)]; } integer;
        integer.i = Data;
        unsigned Offset = getRelativeGlobalAddress(name);
        assert(Offset+4 <= GlobalData64.size());
        for (unsigned i = 0; i < 4; ++i) {
          GlobalData64[Offset++] = integer.b[i];
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

// nativization

void JSWriter::calculateNativizedVars(const Function *F) {
  NativizedVars.clear();

  for (Function::const_iterator BI = F->begin(), BE = F->end(); BI != BE; ++BI) {
    for (BasicBlock::const_iterator II = BI->begin(), E = BI->end(); II != E; ++II) {
      const Instruction *I = &*II;
      if (const AllocaInst *AI = dyn_cast<const AllocaInst>(I)) {
        if (AI->getAllocatedType()->isVectorTy()) continue; // we do not nativize vectors, we rely on the LLVM optimizer to avoid load/stores on them
        if (AI->getAllocatedType()->isAggregateType()) continue; // we do not nativize aggregates either
        // this is on the stack. if its address is never used nor escaped, we can nativize it
        bool Fail = false;
        for (Instruction::const_use_iterator UI = I->use_begin(), UE = I->use_end(); UI != UE && !Fail; ++UI) {
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

bool JSWriter::canReloop(const Function *F) {
  return true;
}

// main entry

void JSWriter::printCommaSeparated(const HeapData data) {
  for (HeapData::const_iterator I = data.begin();
       I != data.end(); ++I) {
    if (I != data.begin()) {
      Out << ",";
    }
    Out << (int)*I;
  }
}

void JSWriter::printProgram(const std::string& fname,
                             const std::string& mName) {
  printModule(fname,mName);
}

void JSWriter::printModule(const std::string& fname,
                            const std::string& mName) {
  printModuleBody();
}

bool JSWriter::runOnModule(Module &M) {
  TheModule = &M;
  DL = &getAnalysis<DataLayout>();

  setupCallHandlers();

  printProgram("", "");

  return false;
}

char JSWriter::ID = 0;

//===----------------------------------------------------------------------===//
//                       External Interface declaration
//===----------------------------------------------------------------------===//

bool JSTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                          formatted_raw_ostream &o,
                                          CodeGenFileType FileType,
                                          bool DisableVerify,
                                          AnalysisID StartAfter,
                                          AnalysisID StopAfter) {
  assert(FileType == TargetMachine::CGFT_AssemblyFile);

  PM.add(createSimplifyAllocasPass());
  PM.add(new JSWriter(o));

  return false;
}

