//===-- CPPBackend.cpp - Library for converting LLVM code to C++ code -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the writing of the LLVM IR as a set of C++ calls to the
// LLVM IR interface. The input module is assumed to be verified.
//
//===----------------------------------------------------------------------===//

#include "CPPTargetMachine.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Config/config.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/TargetRegistry.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
using namespace llvm;

#define dump(x, ...) fprintf(stderr, x, __VA_ARGS__)

#include <Relooper.h>

static cl::opt<std::string>
FuncName("cppfname", cl::desc("Specify the name of the generated function"),
         cl::value_desc("function name"));

enum WhatToGenerate {
  GenProgram,
  GenModule,
  GenContents,
  GenFunction,
  GenFunctions,
  GenInline,
  GenVariable,
  GenType
};

static cl::opt<WhatToGenerate> GenerationType("cppgen", cl::Optional,
  cl::desc("Choose what kind of output to generate"),
  cl::init(GenProgram),
  cl::values(
    clEnumValN(GenProgram,  "program",   "Generate a complete program"),
    clEnumValN(GenModule,   "module",    "Generate a module definition"),
    clEnumValN(GenContents, "contents",  "Generate contents of a module"),
    clEnumValN(GenFunction, "function",  "Generate a function definition"),
    clEnumValN(GenFunctions,"functions", "Generate all function definitions"),
    clEnumValN(GenInline,   "inline",    "Generate an inline function"),
    clEnumValN(GenVariable, "variable",  "Generate a variable definition"),
    clEnumValN(GenType,     "type",      "Generate a type definition"),
    clEnumValEnd
  )
);

static cl::opt<std::string> NameToGenerate("cppfor", cl::Optional,
  cl::desc("Specify the name of the thing to generate"),
  cl::init("!bad!"));

extern "C" void LLVMInitializeCppBackendTarget() {
  // Register the target.
  RegisterTargetMachine<CPPTargetMachine> X(TheCppBackendTarget);
}

namespace {
  typedef std::vector<Type*> TypeList;
  typedef std::map<Type*,std::string> TypeMap;
  typedef std::map<const Value*,std::string> ValueMap;
  typedef std::set<std::string> NameSet;
  typedef std::set<Type*> TypeSet;
  typedef std::set<const Value*> ValueSet;
  typedef std::map<std::string, Type::TypeID> VarMap;
  typedef std::map<const Value*,std::string> ForwardRefMap;
  typedef std::vector<unsigned char> HeapData;
  typedef std::pair<unsigned, unsigned> Address;

  /// CppWriter - This class is the main chunk of code that converts an LLVM
  /// module to a C++ translation unit.
  class CppWriter : public ModulePass {
    formatted_raw_ostream &Out;
    const Module *TheModule;
    uint64_t uniqueNum;
    TypeMap TypeNames;
    ValueMap ValueNames;
    NameSet UsedNames;
    TypeSet DefinedTypes;
    ValueSet DefinedValues;
    VarMap UsedVars;
    ForwardRefMap ForwardRefs;
    bool is_inline;
    unsigned indent_level;
    HeapData GlobalData8;
    HeapData GlobalData32;
    HeapData GlobalData64;
    std::map<std::string, Address> GlobalAddresses;

  public:
    static char ID;
    explicit CppWriter(formatted_raw_ostream &o) :
      ModulePass(ID), Out(o), uniqueNum(0), is_inline(false), indent_level(0){
    }

    virtual const char *getPassName() const { return "C++ backend"; }

    bool runOnModule(Module &M);

    void printProgram(const std::string& fname, const std::string& modName );
    void printModule(const std::string& fname, const std::string& modName );
    void printContents(const std::string& fname, const std::string& modName );
    void printFunction(const std::string& fname, const std::string& funcName );
    void printFunctions();
    void printInline(const std::string& fname, const std::string& funcName );
    void printVariable(const std::string& fname, const std::string& varName );
    void printType(const std::string& fname, const std::string& typeName );

    void error(const std::string& msg);

    
    formatted_raw_ostream& nl(formatted_raw_ostream &Out, int delta = 0);
    inline void in() { indent_level++; }
    inline void out() { if (indent_level >0) indent_level--; }
    
  private:
    void printLinkageType(GlobalValue::LinkageTypes LT);
    void printVisibilityType(GlobalValue::VisibilityTypes VisTypes);
    void printThreadLocalMode(GlobalVariable::ThreadLocalMode TLM);
    void printCallingConv(CallingConv::ID cc);
    void printEscapedString(const std::string& str);
    void printCFP(const ConstantFP* CFP);
    void printCommaSeparated(const HeapData v);

    void allocateConstant(const Constant* CV);
    unsigned getGlobalAddress(const std::string &s) {
      Address a = GlobalAddresses[s];
      switch (a.second) {
        case 64:
          return a.first + 8;
        case 32:
          return a.first + 8 + GlobalData64.size();
        case 8:
          return a.first + 8 + GlobalData64.size() + GlobalData32.size();
        default:
          assert(false);
      }
    }
    std::string getPtrLoad(const Value* Ptr);
    std::string getPtrUse(const Value* Ptr);
    std::string getPtr(const Value* Ptr);
    std::string getConstant(const Constant*);
    std::string getValueAsStr(const Value*);
    std::string getValueAsParenStr(const Value*);
    std::string getCppName(Type* val);
    inline void printCppName(Type* val);

    std::string getCppName(const Value* val);
    inline void printCppName(const Value* val);

    void printAttributes(const AttributeSet &PAL, const std::string &name);
    void printType(Type* Ty);
    void printTypes(const Module* M);

    std::string getAssign(const StringRef &, const Type *);
    std::string getCast(const StringRef &, const Type *);
    std::string getParenCast(const StringRef &, const Type *);

    void printConstant(const Constant *CPV);
    void printConstants(const Module* M);

    void printVariableUses(const GlobalVariable *GV);
    void printVariableHead(const GlobalVariable *GV);
    void printVariableBody(const GlobalVariable *GV);

    void printFunctionUses(const Function *F);
    void printFunctionHead(const Function *F);
    void printFunctionBody(const Function *F);
    std::string generateInstruction(const Instruction *I);
    std::string getOpName(const Value*);

    void printModuleBody();

    unsigned stackAlign(unsigned x) {
      return x + (x%4 != 0 ? 4 - x%4 : 0);
    }
  };
} // end anonymous namespace.

formatted_raw_ostream &CppWriter::nl(formatted_raw_ostream &Out, int delta) {
  Out << '\n';
  if (delta >= 0 || indent_level >= unsigned(-delta))
    indent_level += delta;
  Out.indent(indent_level);
  return Out;
}

static inline void sanitize(std::string &str) {
  for (size_t i = 1; i < str.length(); ++i)
    if (!isalnum(str[i]) && str[i] != '_' && str[i] != '$')
      str[i] = '_';
}

static std::string getTypePrefix(Type *Ty) {
  switch (Ty->getTypeID()) {
  case Type::VoidTyID:     return "void_";
  case Type::IntegerTyID:
    return "int" + utostr(cast<IntegerType>(Ty)->getBitWidth()) + "_";
  case Type::FloatTyID:    return "float_";
  case Type::DoubleTyID:   return "double_";
  case Type::LabelTyID:    return "label_";
  case Type::FunctionTyID: return "func_";
  case Type::StructTyID:   return "struct_";
  case Type::ArrayTyID:    return "array_";
  case Type::PointerTyID:  return "ptr_";
  case Type::VectorTyID:   return "packed_";
  default:                 return "other_";
  }
}

void CppWriter::error(const std::string& msg) {
  report_fatal_error(msg);
}

static inline std::string ftostr(const APFloat& V) {
  std::string Buf;
  if (&V.getSemantics() == &APFloat::IEEEdouble) {
    raw_string_ostream(Buf) << V.convertToDouble();
    return Buf;
  } else if (&V.getSemantics() == &APFloat::IEEEsingle) {
    raw_string_ostream(Buf) << (double)V.convertToFloat();
    return Buf;
  }
  return "<unknown format in ftostr>"; // error
}

// printCFP - Print a floating point constant .. very carefully :)
// This makes sure that conversion to/from floating yields the same binary
// result so that we don't lose precision.
void CppWriter::printCFP(const ConstantFP *CFP) {
  bool ignored;
  APFloat APF = APFloat(CFP->getValueAPF());  // copy
  if (CFP->getType() == Type::getFloatTy(CFP->getContext()))
    APF.convert(APFloat::IEEEdouble, APFloat::rmNearestTiesToEven, &ignored);
#if HAVE_PRINTF_A
  char Buffer[100];
  sprintf(Buffer, "%A", APF.convertToDouble());
  if ((!strncmp(Buffer, "0x", 2) ||
       !strncmp(Buffer, "-0x", 3) ||
       !strncmp(Buffer, "+0x", 3)) &&
      APF.bitwiseIsEqual(APFloat(atof(Buffer)))) {
    if (CFP->getType() == Type::getDoubleTy(CFP->getContext()))
      Out << "BitsToDouble(" << Buffer << ")";
    else
      Out << "BitsToFloat((float)" << Buffer << ")";
    Out << ")";
  } else {
#endif
    std::string StrVal = ftostr(CFP->getValueAPF());

    while (StrVal[0] == ' ')
      StrVal.erase(StrVal.begin());

    // Check to make sure that the stringized number is not some string like
    // "Inf" or NaN.  Check that the string matches the "[-+]?[0-9]" regex.
    if (((StrVal[0] >= '0' && StrVal[0] <= '9') ||
         ((StrVal[0] == '-' || StrVal[0] == '+') &&
          (StrVal[1] >= '0' && StrVal[1] <= '9'))) &&
        (CFP->isExactlyValue(atof(StrVal.c_str())))) {
      if (CFP->getType() == Type::getDoubleTy(CFP->getContext()))
        Out <<  StrVal;
      else
        Out << StrVal << "f";
    } else if (CFP->getType() == Type::getDoubleTy(CFP->getContext()))
      Out << "BitsToDouble(0x"
          << utohexstr(CFP->getValueAPF().bitcastToAPInt().getZExtValue())
          << "ULL) /* " << StrVal << " */";
    else
      Out << "BitsToFloat(0x"
          << utohexstr((uint32_t)CFP->getValueAPF().
                                      bitcastToAPInt().getZExtValue())
          << "U) /* " << StrVal << " */";
#if HAVE_PRINTF_A
  }
#endif
}

void CppWriter::printCallingConv(CallingConv::ID cc){
  // Print the calling convention.
  switch (cc) {
  case CallingConv::C:     Out << "CallingConv::C"; break;
  case CallingConv::Fast:  Out << "CallingConv::Fast"; break;
  case CallingConv::Cold:  Out << "CallingConv::Cold"; break;
  case CallingConv::FirstTargetCC: Out << "CallingConv::FirstTargetCC"; break;
  default:                 Out << cc; break;
  }
}

void CppWriter::printLinkageType(GlobalValue::LinkageTypes LT) {
  switch (LT) {
  case GlobalValue::InternalLinkage:
    Out << "GlobalValue::InternalLinkage"; break;
  case GlobalValue::PrivateLinkage:
    Out << "GlobalValue::PrivateLinkage"; break;
  case GlobalValue::LinkerPrivateLinkage:
    Out << "GlobalValue::LinkerPrivateLinkage"; break;
  case GlobalValue::LinkerPrivateWeakLinkage:
    Out << "GlobalValue::LinkerPrivateWeakLinkage"; break;
  case GlobalValue::AvailableExternallyLinkage:
    Out << "GlobalValue::AvailableExternallyLinkage "; break;
  case GlobalValue::LinkOnceAnyLinkage:
    Out << "GlobalValue::LinkOnceAnyLinkage "; break;
  case GlobalValue::LinkOnceODRLinkage:
    Out << "GlobalValue::LinkOnceODRLinkage "; break;
  case GlobalValue::LinkOnceODRAutoHideLinkage:
    Out << "GlobalValue::LinkOnceODRAutoHideLinkage"; break;
  case GlobalValue::WeakAnyLinkage:
    Out << "GlobalValue::WeakAnyLinkage"; break;
  case GlobalValue::WeakODRLinkage:
    Out << "GlobalValue::WeakODRLinkage"; break;
  case GlobalValue::AppendingLinkage:
    Out << "GlobalValue::AppendingLinkage"; break;
  case GlobalValue::ExternalLinkage:
    Out << "GlobalValue::ExternalLinkage"; break;
  case GlobalValue::DLLImportLinkage:
    Out << "GlobalValue::DLLImportLinkage"; break;
  case GlobalValue::DLLExportLinkage:
    Out << "GlobalValue::DLLExportLinkage"; break;
  case GlobalValue::ExternalWeakLinkage:
    Out << "GlobalValue::ExternalWeakLinkage"; break;
  case GlobalValue::CommonLinkage:
    Out << "GlobalValue::CommonLinkage"; break;
  }
}

void CppWriter::printVisibilityType(GlobalValue::VisibilityTypes VisType) {
  switch (VisType) {
  case GlobalValue::DefaultVisibility:
    Out << "GlobalValue::DefaultVisibility";
    break;
  case GlobalValue::HiddenVisibility:
    Out << "GlobalValue::HiddenVisibility";
    break;
  case GlobalValue::ProtectedVisibility:
    Out << "GlobalValue::ProtectedVisibility";
    break;
  }
}

void CppWriter::printThreadLocalMode(GlobalVariable::ThreadLocalMode TLM) {
  switch (TLM) {
    case GlobalVariable::NotThreadLocal:
      Out << "GlobalVariable::NotThreadLocal";
      break;
    case GlobalVariable::GeneralDynamicTLSModel:
      Out << "GlobalVariable::GeneralDynamicTLSModel";
      break;
    case GlobalVariable::LocalDynamicTLSModel:
      Out << "GlobalVariable::LocalDynamicTLSModel";
      break;
    case GlobalVariable::InitialExecTLSModel:
      Out << "GlobalVariable::InitialExecTLSModel";
      break;
    case GlobalVariable::LocalExecTLSModel:
      Out << "GlobalVariable::LocalExecTLSModel";
      break;
  }
}

// printEscapedString - Print each character of the specified string, escaping
// it if it is not printable or if it is an escape char.
void CppWriter::printEscapedString(const std::string &Str) {
  for (unsigned i = 0, e = Str.size(); i != e; ++i) {
    unsigned char C = Str[i];
    if (isprint(C) && C != '"' && C != '\\') {
      Out << C;
    } else {
      Out << "\\x"
          << (char) ((C/16  < 10) ? ( C/16 +'0') : ( C/16 -10+'A'))
          << (char)(((C&15) < 10) ? ((C&15)+'0') : ((C&15)-10+'A'));
    }
  }
}

std::string CppWriter::getCppName(Type* Ty) {
  // First, handle the primitive types .. easy
  if (Ty->isPrimitiveType() || Ty->isIntegerTy()) {
    switch (Ty->getTypeID()) {
    case Type::VoidTyID:   return "Type::getVoidTy(mod->getContext())";
    case Type::IntegerTyID: {
      unsigned BitWidth = cast<IntegerType>(Ty)->getBitWidth();
      return "IntegerType::get(mod->getContext(), " + utostr(BitWidth) + ")";
    }
    case Type::X86_FP80TyID: return "Type::getX86_FP80Ty(mod->getContext())";
    case Type::FloatTyID:    return "Type::getFloatTy(mod->getContext())";
    case Type::DoubleTyID:   return "Type::getDoubleTy(mod->getContext())";
    case Type::LabelTyID:    return "Type::getLabelTy(mod->getContext())";
    case Type::X86_MMXTyID:  return "Type::getX86_MMXTy(mod->getContext())";
    default:
      error("Invalid primitive type");
      break;
    }
    // shouldn't be returned, but make it sensible
    return "Type::getVoidTy(mod->getContext())";
  }

  // Now, see if we've seen the type before and return that
  TypeMap::iterator I = TypeNames.find(Ty);
  if (I != TypeNames.end())
    return I->second;

  // Okay, let's build a new name for this type. Start with a prefix
  const char* prefix = 0;
  switch (Ty->getTypeID()) {
  case Type::FunctionTyID:    prefix = "FuncTy_"; break;
  case Type::StructTyID:      prefix = "StructTy_"; break;
  case Type::ArrayTyID:       prefix = "ArrayTy_"; break;
  case Type::PointerTyID:     prefix = "PointerTy_"; break;
  case Type::VectorTyID:      prefix = "VectorTy_"; break;
  default:                    prefix = "OtherTy_"; break; // prevent breakage
  }

  // See if the type has a name in the symboltable and build accordingly
  std::string name;
  if (StructType *STy = dyn_cast<StructType>(Ty))
    if (STy->hasName())
      name = STy->getName();
  
  if (name.empty())
    name = utostr(uniqueNum++);
  
  name = std::string(prefix) + name;
  sanitize(name);

  // Save the name
  return TypeNames[Ty] = name;
}

void CppWriter::printCppName(Type* Ty) {
  printEscapedString(getCppName(Ty));
}

std::string CppWriter::getCppName(const Value* val) {
  std::string name;
  ValueMap::iterator I = ValueNames.find(val);
  if (I != ValueNames.end() && I->first == val)
    return I->second;

  if (val->hasName()) {
    if (dyn_cast<Function>(val)) {
      name = std::string("_") + val->getName().str();
    } else {
      name = std::string("$") + val->getName().str();
    }
    sanitize(name);
  } else {
    if (const GlobalVariable* GV = dyn_cast<GlobalVariable>(val)) {
      name = std::string("gvar_") +
        getTypePrefix(GV->getType()->getElementType());
    } else if (isa<Function>(val)) {
      name = std::string("func_");
    } else if (const Constant* C = dyn_cast<Constant>(val)) {
      name = std::string("const_") + getTypePrefix(C->getType());
    } else if (const Argument* Arg = dyn_cast<Argument>(val)) {
      if (is_inline) {
        unsigned argNum = std::distance(Arg->getParent()->arg_begin(),
                                        Function::const_arg_iterator(Arg)) + 1;
        name = std::string("arg_") + utostr(argNum);
        NameSet::iterator NI = UsedNames.find(name);
        if (NI != UsedNames.end())
          name += std::string("_") + utostr(uniqueNum++);
        UsedNames.insert(name);
        return ValueNames[val] = name;
      } else {
        name = getTypePrefix(val->getType());
      }
    } else {
      name = getTypePrefix(val->getType());
    }
    name += utostr(uniqueNum++);
    sanitize(name);
    NameSet::iterator NI = UsedNames.find(name);
    if (NI != UsedNames.end())
      name += std::string("_") + utostr(uniqueNum++);
    UsedNames.insert(name);
  }
  return ValueNames[val] = name;
}

void CppWriter::printCppName(const Value* val) {
  printEscapedString(getCppName(val));
}

void CppWriter::printAttributes(const AttributeSet &PAL,
                                const std::string &name) {
  Out << "AttributeSet " << name << "_PAL;";
  nl(Out);
  if (!PAL.isEmpty()) {
    Out << '{'; in(); nl(Out);
    Out << "SmallVector<AttributeSet, 4> Attrs;"; nl(Out);
    Out << "AttributeSet PAS;"; in(); nl(Out);
    for (unsigned i = 0; i < PAL.getNumSlots(); ++i) {
      unsigned index = PAL.getSlotIndex(i);
      AttrBuilder attrs(PAL.getSlotAttributes(i), index);
      Out << "{"; in(); nl(Out);
      Out << "AttrBuilder B;"; nl(Out);

#define HANDLE_ATTR(X)                                                  \
      if (attrs.contains(Attribute::X)) {                               \
        Out << "B.addAttribute(Attribute::" #X ");"; nl(Out);           \
        attrs.removeAttribute(Attribute::X);                            \
      }

      HANDLE_ATTR(SExt);
      HANDLE_ATTR(ZExt);
      HANDLE_ATTR(NoReturn);
      HANDLE_ATTR(InReg);
      HANDLE_ATTR(StructRet);
      HANDLE_ATTR(NoUnwind);
      HANDLE_ATTR(NoAlias);
      HANDLE_ATTR(ByVal);
      HANDLE_ATTR(Nest);
      HANDLE_ATTR(ReadNone);
      HANDLE_ATTR(ReadOnly);
      HANDLE_ATTR(NoInline);
      HANDLE_ATTR(AlwaysInline);
      HANDLE_ATTR(OptimizeForSize);
      HANDLE_ATTR(StackProtect);
      HANDLE_ATTR(StackProtectReq);
      HANDLE_ATTR(StackProtectStrong);
      HANDLE_ATTR(NoCapture);
      HANDLE_ATTR(NoRedZone);
      HANDLE_ATTR(NoImplicitFloat);
      HANDLE_ATTR(Naked);
      HANDLE_ATTR(InlineHint);
      HANDLE_ATTR(ReturnsTwice);
      HANDLE_ATTR(UWTable);
      HANDLE_ATTR(NonLazyBind);
      HANDLE_ATTR(MinSize);
#undef HANDLE_ATTR

      if (attrs.contains(Attribute::StackAlignment)) {
        Out << "B.addStackAlignmentAttr(" << attrs.getStackAlignment()<<')';
        nl(Out);
        attrs.removeAttribute(Attribute::StackAlignment);
      }

      Out << "PAS = AttributeSet::get(mod->getContext(), ";
      if (index == ~0U)
        Out << "~0U,";
      else
        Out << index << "U,";
      Out << " B);"; out(); nl(Out);
      Out << "}"; out(); nl(Out);
      nl(Out);
      Out << "Attrs.push_back(PAS);"; nl(Out);
    }
    Out << name << "_PAL = AttributeSet::get(mod->getContext(), Attrs);";
    nl(Out);
    out(); nl(Out);
    Out << '}'; nl(Out);
  }
}

void CppWriter::printType(Type* Ty) {
  // We don't print definitions for primitive types
  if (Ty->isPrimitiveType() || Ty->isIntegerTy())
    return;

  // If we already defined this type, we don't need to define it again.
  if (DefinedTypes.find(Ty) != DefinedTypes.end())
    return;

  // Everything below needs the name for the type so get it now.
  std::string typeName(getCppName(Ty));

  // Print the type definition
  switch (Ty->getTypeID()) {
  case Type::FunctionTyID:  {
    FunctionType* FT = cast<FunctionType>(Ty);
    Out << "std::vector<Type*>" << typeName << "_args;";
    nl(Out);
    FunctionType::param_iterator PI = FT->param_begin();
    FunctionType::param_iterator PE = FT->param_end();
    for (; PI != PE; ++PI) {
      Type* argTy = static_cast<Type*>(*PI);
      printType(argTy);
      std::string argName(getCppName(argTy));
      Out << typeName << "_args.push_back(" << argName;
      Out << ");";
      nl(Out);
    }
    printType(FT->getReturnType());
    std::string retTypeName(getCppName(FT->getReturnType()));
    Out << "FunctionType* " << typeName << " = FunctionType::get(";
    in(); nl(Out) << "/*Result=*/" << retTypeName;
    Out << ",";
    nl(Out) << "/*Params=*/" << typeName << "_args,";
    nl(Out) << "/*isVarArg=*/" << (FT->isVarArg() ? "true" : "false") << ");";
    out();
    nl(Out);
    break;
  }
  case Type::StructTyID: {
    StructType* ST = cast<StructType>(Ty);
    if (!ST->isLiteral()) {
      Out << "StructType *" << typeName << " = mod->getTypeByName(\"";
      printEscapedString(ST->getName());
      Out << "\");";
      nl(Out);
      Out << "if (!" << typeName << ") {";
      nl(Out);
      Out << typeName << " = ";
      Out << "StructType::create(mod->getContext(), \"";
      printEscapedString(ST->getName());
      Out << "\");";
      nl(Out);
      Out << "}";
      nl(Out);
      // Indicate that this type is now defined.
      DefinedTypes.insert(Ty);
    }

    Out << "std::vector<Type*>" << typeName << "_fields;";
    nl(Out);
    StructType::element_iterator EI = ST->element_begin();
    StructType::element_iterator EE = ST->element_end();
    for (; EI != EE; ++EI) {
      Type* fieldTy = static_cast<Type*>(*EI);
      printType(fieldTy);
      std::string fieldName(getCppName(fieldTy));
      Out << typeName << "_fields.push_back(" << fieldName;
      Out << ");";
      nl(Out);
    }

    if (ST->isLiteral()) {
      Out << "StructType *" << typeName << " = ";
      Out << "StructType::get(" << "mod->getContext(), ";
    } else {
      Out << "if (" << typeName << "->isOpaque()) {";
      nl(Out);
      Out << typeName << "->setBody(";
    }

    Out << typeName << "_fields, /*isPacked=*/"
        << (ST->isPacked() ? "true" : "false") << ");";
    nl(Out);
    if (!ST->isLiteral()) {
      Out << "}";
      nl(Out);
    }
    break;
  }
  case Type::ArrayTyID: {
    ArrayType* AT = cast<ArrayType>(Ty);
    Type* ET = AT->getElementType();
    printType(ET);
    if (DefinedTypes.find(Ty) == DefinedTypes.end()) {
      std::string elemName(getCppName(ET));
      Out << "ArrayType* " << typeName << " = ArrayType::get("
          << elemName
          << ", " << utostr(AT->getNumElements()) << ");";
      nl(Out);
    }
    break;
  }
  case Type::PointerTyID: {
    PointerType* PT = cast<PointerType>(Ty);
    Type* ET = PT->getElementType();
    printType(ET);
    if (DefinedTypes.find(Ty) == DefinedTypes.end()) {
      std::string elemName(getCppName(ET));
      Out << "PointerType* " << typeName << " = PointerType::get("
          << elemName
          << ", " << utostr(PT->getAddressSpace()) << ");";
      nl(Out);
    }
    break;
  }
  case Type::VectorTyID: {
    VectorType* PT = cast<VectorType>(Ty);
    Type* ET = PT->getElementType();
    printType(ET);
    if (DefinedTypes.find(Ty) == DefinedTypes.end()) {
      std::string elemName(getCppName(ET));
      Out << "VectorType* " << typeName << " = VectorType::get("
          << elemName
          << ", " << utostr(PT->getNumElements()) << ");";
      nl(Out);
    }
    break;
  }
  default:
    error("Invalid TypeID");
  }

  // Indicate that this type is now defined.
  DefinedTypes.insert(Ty);

  // Finally, separate the type definition from other with a newline.
  nl(Out);
}

void CppWriter::printTypes(const Module* M) {
  // Add all of the global variables to the value table.
  for (Module::const_global_iterator I = TheModule->global_begin(),
         E = TheModule->global_end(); I != E; ++I) {
    if (I->hasInitializer())
      printType(I->getInitializer()->getType());
    printType(I->getType());
  }

  // Add all the functions to the table
  for (Module::const_iterator FI = TheModule->begin(), FE = TheModule->end();
       FI != FE; ++FI) {
    printType(FI->getReturnType());
    printType(FI->getFunctionType());
    // Add all the function arguments
    for (Function::const_arg_iterator AI = FI->arg_begin(),
           AE = FI->arg_end(); AI != AE; ++AI) {
      printType(AI->getType());
    }

    // Add all of the basic blocks and instructions
    for (Function::const_iterator BB = FI->begin(),
           E = FI->end(); BB != E; ++BB) {
      printType(BB->getType());
      for (BasicBlock::const_iterator I = BB->begin(), E = BB->end(); I!=E;
           ++I) {
        printType(I->getType());
        for (unsigned i = 0; i < I->getNumOperands(); ++i)
          printType(I->getOperand(i)->getType());
      }
    }
  }
}


std::string CppWriter::getAssign(const StringRef &s, const Type *t) {
  UsedVars[s] = t->getTypeID();
  return (s + " = ").str();
}

std::string CppWriter::getCast(const StringRef &s, const Type *t) {
  switch (t->getTypeID()) {
  default:
    assert(false && "Unsupported type");
  case Type::FloatTyID:
    // TODO return ("Math_fround(" + s + ")").str();
  case Type::DoubleTyID:
    return ("+" + s).str();
  case Type::IntegerTyID:
  case Type::PointerTyID:
    return (s + "|0").str();
  }
}

std::string CppWriter::getParenCast(const StringRef &s, const Type *t) {
  return getCast(("(" + s + ")").str(), t);
}

// printConstant - Print out a constant pool entry...
void CppWriter::printConstant(const Constant *CV) {
  // First, if the constant is actually a GlobalValue (variable or function)
  // or its already in the constant list then we've printed it already and we
  // can just return.
  if (isa<GlobalValue>(CV) || ValueNames.find(CV) != ValueNames.end())
    return;

  std::string constName(getCppName(CV));
  std::string typeName(getCppName(CV->getType()));

  //Out << "var " << constName << " = ";

  if (const ConstantInt *CI = dyn_cast<ConstantInt>(CV)) {
    std::string constValue = CI->getValue().toString(10, true);
    Out << constValue << ";";
  } else if (isa<ConstantAggregateZero>(CV)) {
    Out << "ConstantAggregateZero::get(" << typeName << ");";
  } else if (isa<ConstantPointerNull>(CV)) {
    Out << "ConstantPointerNull::get(" << typeName << ");";
  } else if (const ConstantFP *CFP = dyn_cast<ConstantFP>(CV)) {
    printCFP(CFP);
    Out << ";";
  } else if (const ConstantArray *CA = dyn_cast<ConstantArray>(CV)) {
    Out << "std::vector<Constant*> " << constName << "_elems;";
    nl(Out);
    unsigned N = CA->getNumOperands();
    for (unsigned i = 0; i < N; ++i) {
      printConstant(CA->getOperand(i)); // recurse to print operands
      Out << constName << "_elems.push_back("
          << getCppName(CA->getOperand(i)) << ");";
      nl(Out);
    }
    Out << "Constant* " << constName << " = ConstantArray::get("
        << typeName << ", " << constName << "_elems);";
  } else if (const ConstantStruct *CS = dyn_cast<ConstantStruct>(CV)) {
    Out << "std::vector<Constant*> " << constName << "_fields;";
    nl(Out);
    unsigned N = CS->getNumOperands();
    for (unsigned i = 0; i < N; i++) {
      printConstant(CS->getOperand(i));
      Out << constName << "_fields.push_back("
          << getCppName(CS->getOperand(i)) << ");";
      nl(Out);
    }
    Out << "Constant* " << constName << " = ConstantStruct::get("
        << typeName << ", " << constName << "_fields);";
  } else if (const ConstantVector *CVec = dyn_cast<ConstantVector>(CV)) {
    Out << "std::vector<Constant*> " << constName << "_elems;";
    nl(Out);
    unsigned N = CVec->getNumOperands();
    for (unsigned i = 0; i < N; ++i) {
      printConstant(CVec->getOperand(i));
      Out << constName << "_elems.push_back("
          << getCppName(CVec->getOperand(i)) << ");";
      nl(Out);
    }
    Out << "Constant* " << constName << " = ConstantVector::get("
        << typeName << ", " << constName << "_elems);";
  } else if (isa<UndefValue>(CV)) {
    Out << "UndefValue* " << constName << " = UndefValue::get("
        << typeName << ");";
  } else if (const ConstantDataSequential *CDS =
               dyn_cast<ConstantDataSequential>(CV)) {
    if (CDS->isString()) {
      Out << "allocate([";
      StringRef Str = CDS->getAsString();
      for (unsigned int i = 0; i < Str.size(); i++) {
        Out << (unsigned int)(Str.data()[i]);
        if (i < Str.size()-1) Out << ",";
      }
      Out << "], 'i8', ALLOC_STATIC);";
    } else {
      // TODO: Could generate more efficient code generating CDS calls instead.
      Out << "std::vector<Constant*> " << constName << "_elems;";
      nl(Out);
      for (unsigned i = 0; i != CDS->getNumElements(); ++i) {
        Constant *Elt = CDS->getElementAsConstant(i);
        printConstant(Elt);
        Out << constName << "_elems.push_back(" << getCppName(Elt) << ");";
        nl(Out);
      }
      Out << "Constant* " << constName;
      
      if (isa<ArrayType>(CDS->getType()))
        Out << " = ConstantArray::get(";
      else
        Out << " = ConstantVector::get(";
      Out << typeName << ", " << constName << "_elems);";
    }
  } else if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(CV)) {
    if (CE->getOpcode() == Instruction::GetElementPtr) {
      Out << "allocate([";
      for (unsigned i = 0; i < CE->getNumOperands(); ++i ) {
        Out << getCppName(CE->getOperand(i));
        if (i < CE->getNumOperands()-1) Out << ",";
      }
      Out << "], 'i32', ALLOC_STATIC);";
    } else if (CE->isCast()) {
      printConstant(CE->getOperand(0));
      Out << "Constant* " << constName << " = ConstantExpr::getCast(";
      switch (CE->getOpcode()) {
      default: llvm_unreachable("Invalid cast opcode");
      case Instruction::Trunc: Out << "Instruction::Trunc"; break;
      case Instruction::ZExt:  Out << "Instruction::ZExt"; break;
      case Instruction::SExt:  Out << "Instruction::SExt"; break;
      case Instruction::FPTrunc:  Out << "Instruction::FPTrunc"; break;
      case Instruction::FPExt:  Out << "Instruction::FPExt"; break;
      case Instruction::FPToUI:  Out << "Instruction::FPToUI"; break;
      case Instruction::FPToSI:  Out << "Instruction::FPToSI"; break;
      case Instruction::UIToFP:  Out << "Instruction::UIToFP"; break;
      case Instruction::SIToFP:  Out << "Instruction::SIToFP"; break;
      case Instruction::PtrToInt:  Out << "Instruction::PtrToInt"; break;
      case Instruction::IntToPtr:  Out << "Instruction::IntToPtr"; break;
      case Instruction::BitCast:  Out << "Instruction::BitCast"; break;
      }
      Out << ", " << getCppName(CE->getOperand(0)) << ", "
          << getCppName(CE->getType()) << ");";
    } else {
      unsigned N = CE->getNumOperands();
      for (unsigned i = 0; i < N; ++i ) {
        printConstant(CE->getOperand(i));
      }
      Out << "Constant* " << constName << " = ConstantExpr::";
      switch (CE->getOpcode()) {
      case Instruction::Add:    Out << "getAdd(";  break;
      case Instruction::FAdd:   Out << "getFAdd(";  break;
      case Instruction::Sub:    Out << "getSub("; break;
      case Instruction::FSub:   Out << "getFSub("; break;
      case Instruction::Mul:    Out << "getMul("; break;
      case Instruction::FMul:   Out << "getFMul("; break;
      case Instruction::UDiv:   Out << "getUDiv("; break;
      case Instruction::SDiv:   Out << "getSDiv("; break;
      case Instruction::FDiv:   Out << "getFDiv("; break;
      case Instruction::URem:   Out << "getURem("; break;
      case Instruction::SRem:   Out << "getSRem("; break;
      case Instruction::FRem:   Out << "getFRem("; break;
      case Instruction::And:    Out << "getAnd("; break;
      case Instruction::Or:     Out << "getOr("; break;
      case Instruction::Xor:    Out << "getXor("; break;
      case Instruction::ICmp:
        Out << "getICmp(ICmpInst::ICMP_";
        switch (CE->getPredicate()) {
        case ICmpInst::ICMP_EQ:  Out << "EQ"; break;
        case ICmpInst::ICMP_NE:  Out << "NE"; break;
        case ICmpInst::ICMP_SLT: Out << "SLT"; break;
        case ICmpInst::ICMP_ULT: Out << "ULT"; break;
        case ICmpInst::ICMP_SGT: Out << "SGT"; break;
        case ICmpInst::ICMP_UGT: Out << "UGT"; break;
        case ICmpInst::ICMP_SLE: Out << "SLE"; break;
        case ICmpInst::ICMP_ULE: Out << "ULE"; break;
        case ICmpInst::ICMP_SGE: Out << "SGE"; break;
        case ICmpInst::ICMP_UGE: Out << "UGE"; break;
        default: error("Invalid ICmp Predicate");
        }
        break;
      case Instruction::FCmp:
        Out << "getFCmp(FCmpInst::FCMP_";
        switch (CE->getPredicate()) {
        case FCmpInst::FCMP_FALSE: Out << "FALSE"; break;
        case FCmpInst::FCMP_ORD:   Out << "ORD"; break;
        case FCmpInst::FCMP_UNO:   Out << "UNO"; break;
        case FCmpInst::FCMP_OEQ:   Out << "OEQ"; break;
        case FCmpInst::FCMP_UEQ:   Out << "UEQ"; break;
        case FCmpInst::FCMP_ONE:   Out << "ONE"; break;
        case FCmpInst::FCMP_UNE:   Out << "UNE"; break;
        case FCmpInst::FCMP_OLT:   Out << "OLT"; break;
        case FCmpInst::FCMP_ULT:   Out << "ULT"; break;
        case FCmpInst::FCMP_OGT:   Out << "OGT"; break;
        case FCmpInst::FCMP_UGT:   Out << "UGT"; break;
        case FCmpInst::FCMP_OLE:   Out << "OLE"; break;
        case FCmpInst::FCMP_ULE:   Out << "ULE"; break;
        case FCmpInst::FCMP_OGE:   Out << "OGE"; break;
        case FCmpInst::FCMP_UGE:   Out << "UGE"; break;
        case FCmpInst::FCMP_TRUE:  Out << "TRUE"; break;
        default: error("Invalid FCmp Predicate");
        }
        break;
      case Instruction::Shl:     Out << "getShl("; break;
      case Instruction::LShr:    Out << "getLShr("; break;
      case Instruction::AShr:    Out << "getAShr("; break;
      case Instruction::Select:  Out << "getSelect("; break;
      case Instruction::ExtractElement: Out << "getExtractElement("; break;
      case Instruction::InsertElement:  Out << "getInsertElement("; break;
      case Instruction::ShuffleVector:  Out << "getShuffleVector("; break;
      default:
        error("Invalid constant expression");
        break;
      }
      Out << getCppName(CE->getOperand(0));
      for (unsigned i = 1; i < CE->getNumOperands(); ++i)
        Out << ", " << getCppName(CE->getOperand(i));
      Out << ");";
    }
  } else if (const BlockAddress *BA = dyn_cast<BlockAddress>(CV)) {
    Out << "Constant* " << constName << " = ";
    Out << "BlockAddress::get(" << getOpName(BA->getBasicBlock()) << ");";
  } else {
    error("Bad Constant");
    Out << "Constant* " << constName << " = 0; ";
  }
  nl(Out);
}

void CppWriter::printConstants(const Module* M) {
  // Traverse all the global variables looking for constant initializers
  for (Module::const_global_iterator I = TheModule->global_begin(),
         E = TheModule->global_end(); I != E; ++I)
    if (I->hasInitializer()) {
      const Constant *CV = I->getInitializer();
      allocateConstant(CV);
      GlobalAddresses[I->getName().str()] = GlobalAddresses[getCppName(CV)];
    }

  // Traverse the LLVM functions looking for constants
  for (Module::const_iterator FI = TheModule->begin(), FE = TheModule->end();
       FI != FE; ++FI) {
    // Add all of the basic blocks and instructions
    for (Function::const_iterator BB = FI->begin(),
           E = FI->end(); BB != E; ++BB) {
      for (BasicBlock::const_iterator I = BB->begin(), E = BB->end(); I!=E;
           ++I) {
        for (unsigned i = 0; i < I->getNumOperands(); ++i) {
          if (Constant* C = dyn_cast<Constant>(I->getOperand(i))) {
            allocateConstant(C);
          }
        }
      }
    }
  }
}

void CppWriter::printVariableUses(const GlobalVariable *GV) {
}

void CppWriter::printVariableHead(const GlobalVariable *GV) {
  Out << "var ";
  printCppName(GV);
  Out << ";\n";
}

void CppWriter::printVariableBody(const GlobalVariable *GV) {
  if (GV->hasInitializer()) {
    printCppName(GV);
    Out << " = ";
    Out << getCppName(GV->getInitializer()) << ";";
    nl(Out);
  }
}

std::string CppWriter::getOpName(const Value* V) {
  if (!isa<Instruction>(V) || DefinedValues.find(V) != DefinedValues.end())
    return getCppName(V);

  // See if its alread in the map of forward references, if so just return the
  // name we already set up for it
  ForwardRefMap::const_iterator I = ForwardRefs.find(V);
  if (I != ForwardRefs.end())
    return I->second;

  // This is a new forward reference. Generate a unique name for it
  std::string result(std::string("fwdref_") + utostr(uniqueNum++));

  // Yes, this is a hack. An Argument is the smallest instantiable value that
  // we can make as a placeholder for the real value. We'll replace these
  // Argument instances later.
  Out << "Argument* " << result << " = new Argument("
      << getCppName(V->getType()) << ");";
  nl(Out);
  ForwardRefs[V] = result;
  return result;
}

static StringRef ConvertAtomicOrdering(AtomicOrdering Ordering) {
  switch (Ordering) {
    case NotAtomic: return "NotAtomic";
    case Unordered: return "Unordered";
    case Monotonic: return "Monotonic";
    case Acquire: return "Acquire";
    case Release: return "Release";
    case AcquireRelease: return "AcquireRelease";
    case SequentiallyConsistent: return "SequentiallyConsistent";
  }
  llvm_unreachable("Unknown ordering");
}

static StringRef ConvertAtomicSynchScope(SynchronizationScope SynchScope) {
  switch (SynchScope) {
    case SingleThread: return "SingleThread";
    case CrossThread: return "CrossThread";
  }
  llvm_unreachable("Unknown synch scope");
}

std::string CppWriter::getPtrLoad(const Value* Ptr) {
  Type *t = cast<PointerType>(Ptr->getType())->getElementType();
  return getCast(getPtrUse(Ptr), t);
}

std::string CppWriter::getPtrUse(const Value* Ptr) {
  Type *t = cast<PointerType>(Ptr->getType())->getElementType();
  if (const GlobalVariable *GV = dyn_cast<GlobalVariable>(Ptr)) {
    std::string text = "";
    unsigned Addr = getGlobalAddress(GV->getName().str());
    switch (t->getTypeID()) {
    default:
      assert(false && "Unsupported type");
    case Type::DoubleTyID:
      return "HEAPF64[" + utostr(Addr >> 3) + "]";
    case Type::FloatTyID:
      return "HEAPF32[" + utostr(Addr >> 2) + "]";
    case Type::IntegerTyID:
      return "HEAP32[" + utostr(Addr >> 2) + "]";
    }
  } else {
    switch (t->getTypeID()) {
    default:
      assert(false && "Unsupported type");
    case Type::DoubleTyID:
      return "HEAPF64[" + getOpName(Ptr) + ">>3]";
    case Type::FloatTyID:
      return "HEAPF32[" + getOpName(Ptr) + ">>2]";
    case Type::IntegerTyID:
    case Type::PointerTyID:
      return "HEAP32[" + getOpName(Ptr) + ">>2]";
    }
  }
}

std::string CppWriter::getPtr(const Value* Ptr) {
  Type *t = cast<PointerType>(Ptr->getType())->getElementType();
  if (const Constant *CV = dyn_cast<Constant>(Ptr)) {
    std::string text = "";
    unsigned Addr = getGlobalAddress(CV->getName().str());
    switch (t->getTypeID()) {
    default:
      assert(false && "Unsupported type");
    case Type::DoubleTyID:
      return utostr(Addr >> 3);
    case Type::FloatTyID:
      return utostr(Addr >> 2);
    case Type::ArrayTyID:
    case Type::StructTyID:
    case Type::PointerTyID:
    case Type::VectorTyID:
    case Type::IntegerTyID:
      return utostr(Addr >> 2);
    }
  } else {
    return getOpName(Ptr) + "|0";
  }
}

std::string CppWriter::getConstant(const Constant* CV) {
  if (/* const PointerType *Ptr = */ dyn_cast<PointerType>(CV->getType())) {
    return getPtr(CV);
  } else {
    if (const ConstantFP *CFP = dyn_cast<ConstantFP>(CV)) {
      std::string S = ftostr(CFP->getValueAPF());
      S = '+' + S;
//      if (S.find('.') == S.npos) { TODO: do this when necessary, but it is necessary even for 0.0001
      return S;
    } else if (const ConstantInt *CI = dyn_cast<ConstantInt>(CV)) {
      return CI->getValue().toString(10, true);
    } else {
      assert(false);
    }
  }
}

std::string CppWriter::getValueAsStr(const Value* V) {
  if (const Constant *CV = dyn_cast<Constant>(V)) {
    return getConstant(CV);
  } else {
    return getCast(getCppName(V), V->getType());
  }
}

std::string CppWriter::getValueAsParenStr(const Value* V) {
  return "(" + getValueAsStr(V) + ")";
}

// generateInstruction - This member is called for each Instruction in a function.
std::string CppWriter::generateInstruction(const Instruction *I) {
  std::string text = "NYI: " + std::string(I->getOpcodeName());
  std::string bbname = "NO_BBNAME";
  std::string iName(getCppName(I));

  // Before we emit this instruction, we need to take care of generating any
  // forward references. So, we get the names of all the operands in advance
  const unsigned Ops(I->getNumOperands());
  std::string* opNames = new std::string[Ops];
  for (unsigned i = 0; i < Ops; i++)
    opNames[i] = getOpName(I->getOperand(i));

  switch (I->getOpcode()) {
  default:
    error("Invalid instruction");
    break;

  case Instruction::Ret: {
    const ReturnInst* ret =  cast<ReturnInst>(I);
    Value *RV = ret->getReturnValue();
    text = "STACKTOP = sp;";
    text += "return";
    if (RV == NULL) {
      text += ";";
    } else {
      text += " " + getValueAsStr(RV) + ";";
    }
    break;
  }
  case Instruction::Br: {
    text = "";
    break;
  }
  case Instruction::Switch: {
    const SwitchInst *SI = cast<SwitchInst>(I);
    Out << "SwitchInst* " << iName << " = SwitchInst::Create("
        << getOpName(SI->getCondition()) << ", "
        << getOpName(SI->getDefaultDest()) << ", "
        << SI->getNumCases() << ", " << bbname << ");";
    nl(Out);
    for (SwitchInst::ConstCaseIt i = SI->case_begin(), e = SI->case_end();
         i != e; ++i) {
      const IntegersSubset CaseVal = i.getCaseValueEx();
      const BasicBlock *BB = i.getCaseSuccessor();
      Out << iName << "->addCase("
          << getOpName(CaseVal) << ", "
          << getOpName(BB) << ");";
      nl(Out);
    }
    break;
  }
  case Instruction::IndirectBr: {
    const IndirectBrInst *IBI = cast<IndirectBrInst>(I);
    Out << "IndirectBrInst *" << iName << " = IndirectBrInst::Create("
        << opNames[0] << ", " << IBI->getNumDestinations() << ");";
    nl(Out);
    for (unsigned i = 1; i != IBI->getNumOperands(); ++i) {
      Out << iName << "->addDestination(" << opNames[i] << ");";
      nl(Out);
    }
    break;
  }
  case Instruction::Resume: {
    Out << "ResumeInst::Create(mod->getContext(), " << opNames[0]
        << ", " << bbname << ");";
    break;
  }
  case Instruction::Invoke: {
    const InvokeInst* inv = cast<InvokeInst>(I);
    Out << "std::vector<Value*> " << iName << "_params;";
    nl(Out);
    for (unsigned i = 0; i < inv->getNumArgOperands(); ++i) {
      Out << iName << "_params.push_back("
          << getOpName(inv->getArgOperand(i)) << ");";
      nl(Out);
    }
    // FIXME: This shouldn't use magic numbers -3, -2, and -1.
    Out << "InvokeInst *" << iName << " = InvokeInst::Create("
        << getOpName(inv->getCalledFunction()) << ", "
        << getOpName(inv->getNormalDest()) << ", "
        << getOpName(inv->getUnwindDest()) << ", "
        << iName << "_params, \"";
    printEscapedString(inv->getName());
    Out << "\", " << bbname << ");";
    nl(Out) << iName << "->setCallingConv(";
    printCallingConv(inv->getCallingConv());
    Out << ");";
    printAttributes(inv->getAttributes(), iName);
    Out << iName << "->setAttributes(" << iName << "_PAL);";
    nl(Out);
    break;
  }
  case Instruction::Unreachable: {
    Out << "new UnreachableInst("
        << "mod->getContext(), "
        << bbname << ");";
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
    //Out << "BinaryOperator* " << iName << " = BinaryOperator::Create(";
    text = getAssign(iName, Type::getInt32Ty(I->getContext()));
    switch (I->getOpcode()) {
    case Instruction::Add: text += getParenCast(getValueAsParenStr(I->getOperand(0)) + " + " + getValueAsParenStr(I->getOperand(1)), Type::getInt32Ty(I->getContext())) + ";"; break;
    case Instruction::FAdd: Out << "Instruction::FAdd"; break;
    case Instruction::Sub: text += getParenCast(getValueAsParenStr(I->getOperand(0)) + " - " + getValueAsParenStr(I->getOperand(1)), Type::getInt32Ty(I->getContext())) + ";"; break;
    case Instruction::FSub: Out << "Instruction::FSub"; break;
    case Instruction::Mul: Out << "Instruction::Mul"; break;
    case Instruction::FMul: text += getParenCast(getValueAsStr(I->getOperand(0)) + " * " + getValueAsStr(I->getOperand(1)), I->getType()) + ";"; break;
    case Instruction::UDiv:Out << "Instruction::UDiv"; break;
    case Instruction::SDiv:Out << "Instruction::SDiv"; break;
    case Instruction::FDiv:Out << "Instruction::FDiv"; break;
    case Instruction::URem:Out << "Instruction::URem"; break;
    case Instruction::SRem:Out << "Instruction::SRem"; break;
    case Instruction::FRem:Out << "Instruction::FRem"; break;
    case Instruction::And: Out << "Instruction::And"; break;
    case Instruction::Or:  Out << "Instruction::Or";  break;
    case Instruction::Xor: Out << "Instruction::Xor"; break;
    case Instruction::Shl: Out << "Instruction::Shl"; break;
    case Instruction::LShr:Out << "Instruction::LShr"; break;
    case Instruction::AShr:Out << "Instruction::AShr"; break;
    default: Out << "Instruction::BadOpCode"; break;
    }
    //Out << ", " << opNames[0] << ", " << opNames[1] << ", \"";
    //printEscapedString(I->getName());
    //Out << "\", " << bbname << ");";
    break;
  }
  case Instruction::FCmp: {
    Out << "FCmpInst* " << iName << " = new FCmpInst(*" << bbname << ", ";
    switch (cast<FCmpInst>(I)->getPredicate()) {
    case FCmpInst::FCMP_FALSE: Out << "FCmpInst::FCMP_FALSE"; break;
    case FCmpInst::FCMP_OEQ  : Out << "FCmpInst::FCMP_OEQ"; break;
    case FCmpInst::FCMP_OGT  : Out << "FCmpInst::FCMP_OGT"; break;
    case FCmpInst::FCMP_OGE  : Out << "FCmpInst::FCMP_OGE"; break;
    case FCmpInst::FCMP_OLT  : Out << "FCmpInst::FCMP_OLT"; break;
    case FCmpInst::FCMP_OLE  : Out << "FCmpInst::FCMP_OLE"; break;
    case FCmpInst::FCMP_ONE  : Out << "FCmpInst::FCMP_ONE"; break;
    case FCmpInst::FCMP_ORD  : Out << "FCmpInst::FCMP_ORD"; break;
    case FCmpInst::FCMP_UNO  : Out << "FCmpInst::FCMP_UNO"; break;
    case FCmpInst::FCMP_UEQ  : Out << "FCmpInst::FCMP_UEQ"; break;
    case FCmpInst::FCMP_UGT  : Out << "FCmpInst::FCMP_UGT"; break;
    case FCmpInst::FCMP_UGE  : Out << "FCmpInst::FCMP_UGE"; break;
    case FCmpInst::FCMP_ULT  : Out << "FCmpInst::FCMP_ULT"; break;
    case FCmpInst::FCMP_ULE  : Out << "FCmpInst::FCMP_ULE"; break;
    case FCmpInst::FCMP_UNE  : Out << "FCmpInst::FCMP_UNE"; break;
    case FCmpInst::FCMP_TRUE : Out << "FCmpInst::FCMP_TRUE"; break;
    default: Out << "FCmpInst::BAD_ICMP_PREDICATE"; break;
    }
    Out << ", " << opNames[0] << ", " << opNames[1] << ", \"";
    printEscapedString(I->getName());
    Out << "\");";
    break;
  }
  case Instruction::ICmp: {
    text = getAssign(iName, Type::getInt32Ty(I->getContext())) + "(" + getValueAsStr(I->getOperand(0)) + ")";
    switch (cast<ICmpInst>(I)->getPredicate()) {
    case ICmpInst::ICMP_EQ:  text += "==";  break;
    case ICmpInst::ICMP_NE:  text += "!=";  break;
    // TODO: handle signed and unsigned
    case ICmpInst::ICMP_ULE: text += "<="; break;
    case ICmpInst::ICMP_SLE: text += "<="; break;
    case ICmpInst::ICMP_UGE: text += ">="; break;
    case ICmpInst::ICMP_SGE: text += ">="; break;
    case ICmpInst::ICMP_ULT: text += "<"; break;
    case ICmpInst::ICMP_SLT: text += "<"; break;
    case ICmpInst::ICMP_UGT: text += ">"; break;
    case ICmpInst::ICMP_SGT: text += ">"; break;
    default: text += "ICmpInst::BAD_ICMP_PREDICATE"; break;
    }
    text += "(" + getValueAsStr(I->getOperand(1)) + ")";
    break;
  }
  case Instruction::Alloca: {
    const AllocaInst* allocaI = cast<AllocaInst>(I);
    Type *t = allocaI->getAllocatedType();
    unsigned size;
    if (ArrayType *AT = dyn_cast<ArrayType>(t)) {
      size = AT->getElementType()->getScalarSizeInBits()/8 * AT->getNumElements();
    } else {
      size = t->getScalarSizeInBits()/8;
    }
    text = getAssign(iName, Type::getInt32Ty(I->getContext())) + "STACKTOP; STACKTOP = STACKTOP + " + Twine(stackAlign(size)).str() + "|0;";
    break;
  }
  case Instruction::Load: {
    const LoadInst *LI = cast<LoadInst>(I);
    const Value *Ptr = LI->getPointerOperand();
    Type *t = cast<PointerType>(Ptr->getType())->getElementType();
    text = getAssign(iName, t) + getPtrLoad(Ptr) + ";";
    break;
  }
  case Instruction::Store: {
    const StoreInst *SI = cast<StoreInst>(I);
    const Value *P = SI->getPointerOperand();
    const Value *V = SI->getValueOperand();
    std::string VS = getValueAsStr(V);
    if (V->getType()->isDoubleTy() && SI->getAlignment() == 4) {
      // only 4-byte aligned, copy carefully
      std::string PS = getOpName(P);
      text = "HEAPF64[tempDoublePtr>>3]=" + VS + ";HEAPF32[" + PS + ">>2]=HEAPF32[tempDoublePtr>>2];HEAPF32[" + PS + "+4>>2]=HEAPF32[tempDoublePtr+4>>2];";
    } else {
      text = getPtrUse(P) + " = " + VS + ";";
    }
    break;
  }
  case Instruction::GetElementPtr: {
    assert(false && "Unhandled instruction");
    break;
  }
  case Instruction::PHI: {
    const PHINode* phi = cast<PHINode>(I);

    Out << "PHINode* " << iName << " = PHINode::Create("
        << getCppName(phi->getType()) << ", "
        << phi->getNumIncomingValues() << ", \"";
    printEscapedString(phi->getName());
    Out << "\", " << bbname << ");";
    nl(Out);
    for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
      Out << iName << "->addIncoming("
          << opNames[PHINode::getOperandNumForIncomingValue(i)] << ", "
          << getOpName(phi->getIncomingBlock(i)) << ");";
      nl(Out);
    }
    break;
  }
  case Instruction::PtrToInt:
    text = getAssign(iName, Type::getInt32Ty(I->getContext()));
    if (const Constant *CV = dyn_cast<Constant>(I->getOperand(0))) {
      text += utostr(getGlobalAddress(CV->getName().str()));
    } else {
      text += getCast(opNames[0], Type::getInt32Ty(I->getContext()));
    }
    text += ";";
    break;
  case Instruction::IntToPtr:
    text = getAssign(iName, Type::getInt32Ty(I->getContext())) + opNames[0] + ";";
    break;
  case Instruction::Trunc:
  case Instruction::ZExt:
  case Instruction::SExt:
  case Instruction::FPTrunc:
  case Instruction::FPExt:
  case Instruction::FPToUI:
  case Instruction::FPToSI:
  case Instruction::UIToFP:
  case Instruction::SIToFP:
  case Instruction::BitCast: {
    switch (I->getOpcode()) {
    case Instruction::Trunc:    Out << "TruncInst"; break;
    case Instruction::ZExt:     Out << "ZExtInst"; break;
    case Instruction::SExt:     Out << "SExtInst"; break;
    case Instruction::FPTrunc:  Out << "FPTruncInst"; break;
    case Instruction::FPExt:    text = getAssign(iName, Type::getFloatTy(I->getContext())) + opNames[0] + ";"; break;
    case Instruction::FPToUI:   Out << "FPToUIInst"; break;
    case Instruction::FPToSI:   Out << "FPToSIInst"; break;
    case Instruction::UIToFP:   Out << "UIToFPInst"; break;
    case Instruction::PtrToInt: Out << "PtrToIntInst"; break;
    case Instruction::IntToPtr: Out << "IntToPtrInst"; break;
    case Instruction::SIToFP:
    case Instruction::BitCast:  text = getAssign(iName, I->getOperand(0)->getType()) + getValueAsStr(I->getOperand(0)) + ";"; break;
    default: llvm_unreachable("Unreachable");
    }
    break;
  }
  case Instruction::Call: {
    const CallInst* call = cast<CallInst>(I);
    const int numArgs = call->getNumArgOperands();
    Type *RT = call->getCalledFunction()->getReturnType();
    text = opNames[numArgs] + "(";
    for (int i = 0; i < numArgs; i++) {
      Value *Arg = call->getArgOperand(i);
      Type *t = dyn_cast<PointerType>(Arg->getType());
      const GlobalVariable *GV;
      if (t && (GV = dyn_cast<GlobalVariable>(Arg))) {
        text += utostr(getGlobalAddress(GV->getName().str()));
      } else {
        text += getValueAsStr(call->getArgOperand(i));
      }
      if (i < numArgs - 1) text += ", ";
    }
    text += ")";
    if (!RT->isVoidTy()) {
      text = getAssign(iName, RT) + getCast(text, RT);
    }
    text += ";";
    break;
  }
  case Instruction::Select: {
    const SelectInst* sel = cast<SelectInst>(I);
    Out << "SelectInst* " << getCppName(sel) << " = SelectInst::Create(";
    Out << opNames[0] << ", " << opNames[1] << ", " << opNames[2] << ", \"";
    printEscapedString(sel->getName());
    Out << "\", " << bbname << ");";
    break;
  }
  case Instruction::UserOp1:
    /// FALL THROUGH
  case Instruction::UserOp2: {
    /// FIXME: What should be done here?
    break;
  }
  case Instruction::VAArg: {
    const VAArgInst* va = cast<VAArgInst>(I);
    Out << "VAArgInst* " << getCppName(va) << " = new VAArgInst("
        << opNames[0] << ", " << getCppName(va->getType()) << ", \"";
    printEscapedString(va->getName());
    Out << "\", " << bbname << ");";
    break;
  }
  case Instruction::ExtractElement: {
    const ExtractElementInst* eei = cast<ExtractElementInst>(I);
    Out << "ExtractElementInst* " << getCppName(eei)
        << " = new ExtractElementInst(" << opNames[0]
        << ", " << opNames[1] << ", \"";
    printEscapedString(eei->getName());
    Out << "\", " << bbname << ");";
    break;
  }
  case Instruction::InsertElement: {
    const InsertElementInst* iei = cast<InsertElementInst>(I);
    Out << "InsertElementInst* " << getCppName(iei)
        << " = InsertElementInst::Create(" << opNames[0]
        << ", " << opNames[1] << ", " << opNames[2] << ", \"";
    printEscapedString(iei->getName());
    Out << "\", " << bbname << ");";
    break;
  }
  case Instruction::ShuffleVector: {
    const ShuffleVectorInst* svi = cast<ShuffleVectorInst>(I);
    Out << "ShuffleVectorInst* " << getCppName(svi)
        << " = new ShuffleVectorInst(" << opNames[0]
        << ", " << opNames[1] << ", " << opNames[2] << ", \"";
    printEscapedString(svi->getName());
    Out << "\", " << bbname << ");";
    break;
  }
  case Instruction::ExtractValue: {
    const ExtractValueInst *evi = cast<ExtractValueInst>(I);
    Out << "std::vector<unsigned> " << iName << "_indices;";
    nl(Out);
    for (unsigned i = 0; i < evi->getNumIndices(); ++i) {
      Out << iName << "_indices.push_back("
          << evi->idx_begin()[i] << ");";
      nl(Out);
    }
    Out << "ExtractValueInst* " << getCppName(evi)
        << " = ExtractValueInst::Create(" << opNames[0]
        << ", "
        << iName << "_indices, \"";
    printEscapedString(evi->getName());
    Out << "\", " << bbname << ");";
    break;
  }
  case Instruction::InsertValue: {
    const InsertValueInst *ivi = cast<InsertValueInst>(I);
    Out << "std::vector<unsigned> " << iName << "_indices;";
    nl(Out);
    for (unsigned i = 0; i < ivi->getNumIndices(); ++i) {
      Out << iName << "_indices.push_back("
          << ivi->idx_begin()[i] << ");";
      nl(Out);
    }
    Out << "InsertValueInst* " << getCppName(ivi)
        << " = InsertValueInst::Create(" << opNames[0]
        << ", " << opNames[1] << ", "
        << iName << "_indices, \"";
    printEscapedString(ivi->getName());
    Out << "\", " << bbname << ");";
    break;
  }
  case Instruction::Fence: {
    const FenceInst *fi = cast<FenceInst>(I);
    StringRef Ordering = ConvertAtomicOrdering(fi->getOrdering());
    StringRef CrossThread = ConvertAtomicSynchScope(fi->getSynchScope());
    Out << "FenceInst* " << iName
        << " = new FenceInst(mod->getContext(), "
        << Ordering << ", " << CrossThread << ", " << bbname
        << ");";
    break;
  }
  case Instruction::AtomicCmpXchg: {
    const AtomicCmpXchgInst *cxi = cast<AtomicCmpXchgInst>(I);
    StringRef Ordering = ConvertAtomicOrdering(cxi->getOrdering());
    StringRef CrossThread = ConvertAtomicSynchScope(cxi->getSynchScope());
    Out << "AtomicCmpXchgInst* " << iName
        << " = new AtomicCmpXchgInst("
        << opNames[0] << ", " << opNames[1] << ", " << opNames[2] << ", "
        << Ordering << ", " << CrossThread << ", " << bbname
        << ");";
    nl(Out) << iName << "->setName(\"";
    printEscapedString(cxi->getName());
    Out << "\");";
    break;
  }
  case Instruction::AtomicRMW: {
    const AtomicRMWInst *rmwi = cast<AtomicRMWInst>(I);
    StringRef Ordering = ConvertAtomicOrdering(rmwi->getOrdering());
    StringRef CrossThread = ConvertAtomicSynchScope(rmwi->getSynchScope());
    StringRef Operation;
    switch (rmwi->getOperation()) {
      case AtomicRMWInst::Xchg: Operation = "AtomicRMWInst::Xchg"; break;
      case AtomicRMWInst::Add:  Operation = "AtomicRMWInst::Add"; break;
      case AtomicRMWInst::Sub:  Operation = "AtomicRMWInst::Sub"; break;
      case AtomicRMWInst::And:  Operation = "AtomicRMWInst::And"; break;
      case AtomicRMWInst::Nand: Operation = "AtomicRMWInst::Nand"; break;
      case AtomicRMWInst::Or:   Operation = "AtomicRMWInst::Or"; break;
      case AtomicRMWInst::Xor:  Operation = "AtomicRMWInst::Xor"; break;
      case AtomicRMWInst::Max:  Operation = "AtomicRMWInst::Max"; break;
      case AtomicRMWInst::Min:  Operation = "AtomicRMWInst::Min"; break;
      case AtomicRMWInst::UMax: Operation = "AtomicRMWInst::UMax"; break;
      case AtomicRMWInst::UMin: Operation = "AtomicRMWInst::UMin"; break;
      case AtomicRMWInst::BAD_BINOP: llvm_unreachable("Bad atomic operation");
    }
    Out << "AtomicRMWInst* " << iName
        << " = new AtomicRMWInst("
        << Operation << ", "
        << opNames[0] << ", " << opNames[1] << ", "
        << Ordering << ", " << CrossThread << ", " << bbname
        << ");";
    nl(Out) << iName << "->setName(\"";
    printEscapedString(rmwi->getName());
    Out << "\");";
    break;
  }
  }
  DefinedValues.insert(I);
  delete [] opNames;
  return text;
}

// Print out the types, constants and declarations needed by one function
void CppWriter::printFunctionUses(const Function* F) {
  nl(Out) << "// Type Definitions"; nl(Out);
  if (!is_inline) {
    // Print the function's return type
    printType(F->getReturnType());

    // Print the function's function type
    printType(F->getFunctionType());

    // Print the types of each of the function's arguments
    for (Function::const_arg_iterator AI = F->arg_begin(), AE = F->arg_end();
         AI != AE; ++AI) {
      printType(AI->getType());
    }
  }

  // Print type definitions for every type referenced by an instruction and
  // make a note of any global values or constants that are referenced
  SmallPtrSet<GlobalValue*,64> gvs;
  SmallPtrSet<Constant*,64> consts;
  for (Function::const_iterator BB = F->begin(), BE = F->end();
       BB != BE; ++BB){
    for (BasicBlock::const_iterator I = BB->begin(), E = BB->end();
         I != E; ++I) {
      // Print the type of the instruction itself
      printType(I->getType());

      // Print the type of each of the instruction's operands
      for (unsigned i = 0; i < I->getNumOperands(); ++i) {
        Value* operand = I->getOperand(i);
        printType(operand->getType());

        // If the operand references a GVal or Constant, make a note of it
        if (GlobalValue* GV = dyn_cast<GlobalValue>(operand)) {
          gvs.insert(GV);
          if (GenerationType != GenFunction)
            if (GlobalVariable *GVar = dyn_cast<GlobalVariable>(GV))
              if (GVar->hasInitializer())
                consts.insert(GVar->getInitializer());
        } else if (Constant* C = dyn_cast<Constant>(operand)) {
          consts.insert(C);
          for (unsigned j = 0; j < C->getNumOperands(); ++j) {
            // If the operand references a GVal or Constant, make a note of it
            Value* operand = C->getOperand(j);
            printType(operand->getType());
            if (GlobalValue* GV = dyn_cast<GlobalValue>(operand)) {
              gvs.insert(GV);
              if (GenerationType != GenFunction)
                if (GlobalVariable *GVar = dyn_cast<GlobalVariable>(GV))
                  if (GVar->hasInitializer())
                    consts.insert(GVar->getInitializer());
            }
          }
        }
      }
    }
  }

}

void CppWriter::printFunctionHead(const Function* F) {
  nl(Out) << "Function* " << getCppName(F);
  Out << " = mod->getFunction(\"";
  printEscapedString(F->getName());
  Out << "\");";
  nl(Out) << "if (!" << getCppName(F) << ") {";
  nl(Out) << getCppName(F);

  Out<< " = Function::Create(";
  nl(Out,1) << "/*Type=*/" << getCppName(F->getFunctionType()) << ",";
  nl(Out) << "/*Linkage=*/";
  printLinkageType(F->getLinkage());
  Out << ",";
  nl(Out) << "/*Name=*/\"";
  printEscapedString(F->getName());
  Out << "\", mod); " << (F->isDeclaration()? "// (external, no body)" : "");
  nl(Out,-1);
  printCppName(F);
  Out << "->setCallingConv(";
  printCallingConv(F->getCallingConv());
  Out << ");";
  nl(Out);
  if (F->hasSection()) {
    printCppName(F);
    Out << "->setSection(\"" << F->getSection() << "\");";
    nl(Out);
  }
  if (F->getAlignment()) {
    printCppName(F);
    Out << "->setAlignment(" << F->getAlignment() << ");";
    nl(Out);
  }
  if (F->getVisibility() != GlobalValue::DefaultVisibility) {
    printCppName(F);
    Out << "->setVisibility(";
    printVisibilityType(F->getVisibility());
    Out << ");";
    nl(Out);
  }
  if (F->hasGC()) {
    printCppName(F);
    Out << "->setGC(\"" << F->getGC() << "\");";
    nl(Out);
  }
  Out << "}";
  nl(Out);
  printAttributes(F->getAttributes(), getCppName(F));
  printCppName(F);
  Out << "->setAttributes(" << getCppName(F) << "_PAL);";
  nl(Out);
}

void CppWriter::printFunctionBody(const Function *F) {
  assert(!F->isDeclaration());

  // Clear the DefinedValues and ForwardRefs maps because we can't have
  // cross-function forward refs
  ForwardRefs.clear();
  DefinedValues.clear();

  UsedVars.clear();

  // Create all the argument values
  if (!is_inline) {
    for (Function::const_arg_iterator AI = F->arg_begin(), AE = F->arg_end();
         AI != AE; ++AI) {
      if (AI->hasName()) {
        //Out << getCppName(AI) << "->setName(\"";
        //printEscapedString(AI->getName());
        //Out << "\");";
        //nl(Out);
      } else {
      }
    }
  }

  // Prepare relooper TODO: resize buffer as needed
  #define RELOOPER_BUFFER 10*1024*1024
  static char *buffer = new char[RELOOPER_BUFFER];
  Relooper::SetOutputBuffer(buffer, RELOOPER_BUFFER);
  Relooper R;
  Block *Entry = NULL;
  std::map<const BasicBlock*, Block*> LLVMToRelooper;

  // Create relooper blocks with their contents
  for (Function::const_iterator BI = F->begin(), BE = F->end();
       BI != BE; ++BI) {
    std::string contents = "";
    for (BasicBlock::const_iterator I = BI->begin(), E = BI->end();
         I != E; ++I) {
      contents += " " + generateInstruction(I) + "\n";
    }
    Block *Curr = new Block(contents.c_str(), NULL); // TODO: use branch vars so we get switches
    const BasicBlock *BB = &*BI;
    LLVMToRelooper[BB] = Curr;
    R.AddBlock(Curr);
    if (!Entry) Entry = Curr;
  }

  // Create branchings
  for (Function::const_iterator BI = F->begin(), BE = F->end();
       BI != BE; ++BI) {
    const TerminatorInst *TI = BI->getTerminator();
    switch (TI->getOpcode()) {
    default: {
      //error("Invalid branch instruction");
      break;
    }
    case Instruction::Br: {
      const BranchInst* br = cast<BranchInst>(TI);
      if (br->getNumOperands() == 3) {
        BasicBlock *S0 = br->getSuccessor(0);
        BasicBlock *S1 = br->getSuccessor(1);
        LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S0], NULL);
        LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S1],
            getOpName(TI->getOperand(0)).c_str());
      } else if (br->getNumOperands() == 1) {
        BasicBlock *S = br->getSuccessor(0);
        LLVMToRelooper[&*BI]->AddBranchTo(LLVMToRelooper[&*S], NULL);
      } else {
        error("Branch with 2 operands?");
      }
      break;
    }
    }
  }

  // Calculate relooping and print
  R.Calculate(Entry);
  R.Render();

  // Emit local variables
  UsedVars["sp"] = Type::getInt32Ty(F->getContext())->getTypeID();
  if (!UsedVars.empty()) {
    Out << " var ";
    for (VarMap::iterator VI = UsedVars.begin(); VI != UsedVars.end(); ++VI) {
      if (VI != UsedVars.begin()) {
        Out << ", ";
      }
      Out << VI->first << " = ";
      switch (VI->second) {
        default:
          assert(false);
        case Type::PointerTyID:
        case Type::IntegerTyID:
          Out << "0";
          break;
        case Type::FloatTyID:
          // TODO Out << "Math_fround(0)";
        case Type::DoubleTyID:
          Out << "+0"; // FIXME
          break;
      }
    }
    Out << ";";
    nl(Out);
  }

  // Emit stack entry
  Out << " " + getAssign("sp", Type::getInt32Ty(F->getContext())) + "STACKTOP;";

  // Emit (relooped) code
  nl(Out) << buffer;

  // Loop over the ForwardRefs and resolve them now that all instructions
  // are generated.
  if (!ForwardRefs.empty()) {
    nl(Out) << "// Resolve Forward References";
    nl(Out);
  }

  while (!ForwardRefs.empty()) {
    ForwardRefMap::iterator I = ForwardRefs.begin();
    Out << I->second << "->replaceAllUsesWith("
        << getCppName(I->first) << "); delete " << I->second << ";";
    nl(Out);
    ForwardRefs.erase(I);
  }
}

void CppWriter::printInline(const std::string& fname,
                            const std::string& func) {
  const Function* F = TheModule->getFunction(func);
  if (!F) {
    error(std::string("Function '") + func + "' not found in input module");
    return;
  }
  if (F->isDeclaration()) {
    error(std::string("Function '") + func + "' is external!");
    return;
  }
  nl(Out) << "BasicBlock* " << fname << "(Module* mod, Function *"
          << getCppName(F);
  unsigned arg_count = 1;
  for (Function::const_arg_iterator AI = F->arg_begin(), AE = F->arg_end();
       AI != AE; ++AI) {
    Out << ", Value* arg_" << arg_count;
  }
  Out << ") {";
  nl(Out);
  is_inline = true;
  printFunctionUses(F);
  printFunctionBody(F);
  is_inline = false;
  Out << "return " << getCppName(F->begin()) << ";";
  nl(Out) << "}";
  nl(Out);
}

void CppWriter::printModuleBody() {
  // Calculate the constants definitions.
  printConstants(TheModule);

  // Emit function bodies.
  nl(Out) << "// EMSCRIPTEN_START_FUNCTIONS"; nl(Out);
  for (Module::const_iterator I = TheModule->begin(), E = TheModule->end();
       I != E; ++I) {
    if (!I->isDeclaration()) {
      Out << "function _" << I->getName() << "(";
      for (Function::const_arg_iterator AI = I->arg_begin(), AE = I->arg_end();
           AI != AE; ++AI) {
        if (AI != I->arg_begin()) Out << ",";
        Out << getCppName(AI);
      }
      Out << ") {";
      nl(Out);
      for (Function::const_arg_iterator AI = I->arg_begin(), AE = I->arg_end();
           AI != AE; ++AI) {
        std::string name = getCppName(AI);
        Out << " " << name << " = " << getCast(name, AI->getType()) << ";";
        nl(Out);
      }
      printFunctionBody(I);
      Out << "}";
      nl(Out);
    }
  }
  Out << "// EMSCRIPTEN_END_FUNCTIONS\n\n";

  // TODO fix commas
  Out << "/* memory initializer */ allocate([";
  printCommaSeparated(GlobalData64);
  if (GlobalData64.size() > 0 && GlobalData32.size() > 0) {
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
    if (I->isDeclaration()) {
      if (first) {
        first = false;
      } else {
        Out << ", ";
      }
      Out << "\"" + I->getName() + "\"";
    }
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
  Out << "]";

  Out << "\n}\n";
}

#include <iostream>

void CppWriter::allocateConstant(const Constant* CV) {
  if (isa<GlobalValue>(CV))
    return;

  std::string name = getCppName(CV);
  if (const ConstantDataSequential *CDS =
         dyn_cast<ConstantDataSequential>(CV)) {
    if (CDS->isString()) {
      GlobalAddresses[name] = Address(GlobalData8.size(), 8);
      StringRef Str = CDS->getAsString();
      for (unsigned int i = 0; i < Str.size(); i++) {
        GlobalData8.push_back(Str.data()[i]);
      }
    } else {
      assert(false);
    }
  } else if (const ConstantFP *CFP = dyn_cast<ConstantFP>(CV)) {
    APFloat APF = CFP->getValueAPF();
    if (CFP->getType() == Type::getFloatTy(CFP->getContext())) {
      GlobalAddresses[name] = Address(GlobalData32.size(), 32);
      union flt { float f; unsigned char b[sizeof(float)]; } flt;
      flt.f = APF.convertToFloat();
      for (unsigned i = 0; i < sizeof(float); ++i)
        GlobalData32.push_back(flt.b[i]);
    } else if (CFP->getType() == Type::getDoubleTy(CFP->getContext())) {
      GlobalAddresses[name] = Address(GlobalData64.size(), 64);
      union dbl { double d; unsigned char b[sizeof(double)]; } dbl;
      dbl.d = APF.convertToDouble();
      for (unsigned i = 0; i < sizeof(double); ++i)
        GlobalData64.push_back(dbl.b[i]);
    } else {
      assert(false);
    }
  } else if (const ConstantInt *CI = dyn_cast<ConstantInt>(CV)) {
    union { uint64_t i; unsigned char b[sizeof(uint64_t)]; } integer;
    integer.i = *CI->getValue().getRawData();
    unsigned BitWidth = CI->getValue().getBitWidth();
    assert(BitWidth == 32 || BitWidth == 64);
    HeapData *GlobalData = NULL;
    switch (BitWidth) {
      case 32:
        GlobalData = &GlobalData32;
        break;
      case 64:
        GlobalData = &GlobalData64;
        break;
      default:
        assert(false);
    }
    // assuming compiler is little endian
    GlobalAddresses[name] = Address(GlobalData->size(), BitWidth);
    for (unsigned i = 0; i < BitWidth / 8; ++i) {
      GlobalData->push_back(integer.b[i]);
    }
  } else if (/* const ConstantPointerNull *CPN = */ dyn_cast<ConstantPointerNull>(CV)) {
    assert(false);
  } else if (/* const ConstantAggregateZero *CAZ = */ dyn_cast<ConstantAggregateZero>(CV)) {
    printf("Warning: ignoring CAZ\n");
    //Constant *C = CAZ->getSequentialElement();
  } else if (/* const ConstantArray *CA = */ dyn_cast<ConstantArray>(CV)) {
    assert(false);
  } else if (/* const ConstantStruct *CS = */ dyn_cast<ConstantStruct>(CV)) {
    assert(false);
  } else if (/* const ConstantVector *CVec = */ dyn_cast<ConstantVector>(CV)) {
    assert(false);
  } else if (/* const BlockAddress *BA = */ dyn_cast<BlockAddress>(CV)) {
    assert(false);
  } else if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(CV)) {
    if (CE->getOpcode() == Instruction::GetElementPtr) {
        assert(false && "Unhandled CE GEP");
    } else if (CE->isCast()) {
        assert(false && "Unhandled cast");
    } else {
        assert(false);
    }
  } else if (/* const UndefValue *UV = */ dyn_cast<UndefValue>(CV)) {
    assert(false);
  } else {
    std::cout << getCppName(CV) << std::endl;
    assert(false);
  }
}

void CppWriter::printCommaSeparated(const HeapData data) {
  for (HeapData::const_iterator I = data.begin();
       I != data.end(); ++I) {
    if (I != data.begin()) {
      Out << ",";
    }
    Out << (int)*I;
  }
}

void CppWriter::printProgram(const std::string& fname,
                             const std::string& mName) {
  printModule(fname,mName);
}

void CppWriter::printModule(const std::string& fname,
                            const std::string& mName) {
  printModuleBody();
}

void CppWriter::printContents(const std::string& fname,
                              const std::string& mName) {
  Out << "\nModule* " << fname << "(Module *mod) {\n";
  Out << "\nmod->setModuleIdentifier(\"";
  printEscapedString(mName);
  Out << "\");\n";
  printModuleBody();
  Out << "\nreturn mod;\n";
  Out << "\n}\n";
}

void CppWriter::printFunction(const std::string& fname,
                              const std::string& funcName) {
  const Function* F = TheModule->getFunction(funcName);
  if (!F) {
    error(std::string("Function '") + funcName + "' not found in input module");
    return;
  }
  Out << "\nFunction* " << fname << "(Module *mod) {\n";
  printFunctionUses(F);
  printFunctionHead(F);
  printFunctionBody(F);
  Out << "return " << getCppName(F) << ";\n";
  Out << "}\n";
}

void CppWriter::printFunctions() {
  const Module::FunctionListType &funcs = TheModule->getFunctionList();
  Module::const_iterator I  = funcs.begin();
  Module::const_iterator IE = funcs.end();

  for (; I != IE; ++I) {
    const Function &func = *I;
    if (!func.isDeclaration()) {
      std::string name("define_");
      name += func.getName();
      printFunction(name, func.getName());
    }
  }
}

void CppWriter::printVariable(const std::string& fname,
                              const std::string& varName) {
  const GlobalVariable* GV = TheModule->getNamedGlobal(varName);

  if (!GV) {
    error(std::string("Variable '") + varName + "' not found in input module");
    return;
  }
  Out << "\nGlobalVariable* " << fname << "(Module *mod) {\n";
  printVariableUses(GV);
  printVariableHead(GV);
  printVariableBody(GV);
  Out << "return " << getCppName(GV) << ";\n";
  Out << "}\n";
}

void CppWriter::printType(const std::string &fname,
                          const std::string &typeName) {
  Type* Ty = TheModule->getTypeByName(typeName);
  if (!Ty) {
    error(std::string("Type '") + typeName + "' not found in input module");
    return;
  }
  Out << "\nType* " << fname << "(Module *mod) {\n";
  printType(Ty);
  Out << "return " << getCppName(Ty) << ";\n";
  Out << "}\n";
}

bool CppWriter::runOnModule(Module &M) {
  TheModule = &M;

  // Emit a header
  Out << "//========================================\n\n";

  // Get the name of the function we're supposed to generate
  std::string fname = FuncName.getValue();

  // Get the name of the thing we are to generate
  std::string tgtname = NameToGenerate.getValue();
  if (GenerationType == GenModule ||
      GenerationType == GenContents ||
      GenerationType == GenProgram ||
      GenerationType == GenFunctions) {
    if (tgtname == "!bad!") {
      if (M.getModuleIdentifier() == "-")
        tgtname = "<stdin>";
      else
        tgtname = M.getModuleIdentifier();
    }
  } else if (tgtname == "!bad!")
    error("You must use the -for option with -gen-{function,variable,type}");

  switch (WhatToGenerate(GenerationType)) {
   case GenProgram:
    if (fname.empty())
      fname = "makeLLVMModule";
    printProgram(fname,tgtname);
    break;
   case GenModule:
    if (fname.empty())
      fname = "makeLLVMModule";
    printModule(fname,tgtname);
    break;
   case GenContents:
    if (fname.empty())
      fname = "makeLLVMModuleContents";
    printContents(fname,tgtname);
    break;
   case GenFunction:
    if (fname.empty())
      fname = "makeLLVMFunction";
    printFunction(fname,tgtname);
    break;
   case GenFunctions:
    printFunctions();
    break;
   case GenInline:
    if (fname.empty())
      fname = "makeLLVMInline";
    printInline(fname,tgtname);
    break;
   case GenVariable:
    if (fname.empty())
      fname = "makeLLVMVariable";
    printVariable(fname,tgtname);
    break;
   case GenType:
    if (fname.empty())
      fname = "makeLLVMType";
    printType(fname,tgtname);
    break;
  }

  return false;
}

char CppWriter::ID = 0;

//===----------------------------------------------------------------------===//
//                       External Interface declaration
//===----------------------------------------------------------------------===//

bool CPPTargetMachine::addPassesToEmitFile(PassManagerBase &PM,
                                           formatted_raw_ostream &o,
                                           CodeGenFileType FileType,
                                           bool DisableVerify,
                                           AnalysisID StartAfter,
                                           AnalysisID StopAfter) {
  if (FileType != TargetMachine::CGFT_AssemblyFile) return true;
  PM.add(new CppWriter(o));
  return false;
}
