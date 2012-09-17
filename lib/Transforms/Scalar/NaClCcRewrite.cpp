//===- ConstantProp.cpp - Code to perform Simple Constant Propagation -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements calling convention rewrite for Native Client to ensure
// compatibility between pnacl and gcc generated code when calling
// ppapi interface functions.
//===----------------------------------------------------------------------===//


// Major TODOs:
// * add register constraints to x86-64 rewrite decissions
// * dealing with vararg
//   (We shoulf exclude all var arg functions and calls to them from rewrites)

#define DEBUG_TYPE "naclcc"

#include "llvm/Argument.h"
#include "llvm/Attributes.h"
#include "llvm/Constant.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Transforms/Scalar.h"

#include <vector>

using namespace llvm;

namespace llvm {

cl::opt<bool> FlagEnableCcRewrite(
  "nacl-cc-rewrite",
  cl::desc("enable NaCl CC rewrite"));
}

namespace {

// This represents a rule for rewiriting types
struct TypeRewriteRule {
  const char* src;    // type pattern we are trying to match
  const char* dst;    // replacement type
  const char* name;   // name of the rule for diagnosis
};

// Note: all rules must be well-formed
// * parentheses must match
// * TODO: add verification for this

// Legend:
// s(): struct (also used for unions)
// c:   char (= 8 bit int)  (only allowed for src)
// i:   32 bit int
// l:   64 bit int
// f:   32 bit float
// d:   64 bit float (= double)
// p:   untyped pointer (only allowed for src)
// P(): typed pointer (currently not used, only allowed for src)
// C:   "copy", use src as dst (only allowed for dst and sret)
// F:   generic function type (only allowed for src)


// The X8664 Rewrite rules are also subject to
// register constraints, c.f.: section 3.2.3
// http://www.x86-64.org/documentation/abi.pdf
TypeRewriteRule ByvalRulesX8664[] = {
  {"s(iis(d))", "ll", "PP_Var"},
  {"s(pp)",     "l",  "PP_ArrayOutput"},
  {"s(ppi)",    "li", "PP_CompletionCallback"},
  {0, 0, 0},
};

TypeRewriteRule SretRulesX8664[] = {
  {"s(iis(d))", "s(ll)", "PP_Var"},
  {"s(ff)",     "d",     "PP_FloatPoint"},
  {"s(ii)",     "l",     "PP_Point" },
  {"s(pp)",     "l",     "PP_ArrayOutput"},
  {0, 0, 0},
};

TypeRewriteRule ByvalRulesARM[] = {
  {"s(iis(d))",  "ll",  "PP_Var"},
  {"s(ppi)",     "iii", "PP_CompletionCallback" },
  {"s(pp)",      "ii",  "PP_ArrayOutput"},
  {0, 0, 0},
};

TypeRewriteRule SretRulesARM[] = {
  {"s(ff)",     "C", "PP_FloatPoint"},
  {0, 0, 0},
};

// TODO: Find a better way to determine the architecture
const TypeRewriteRule* GetByvalRewriteRulesForTarget(
  const TargetLowering* tli) {
  if (!FlagEnableCcRewrite) return 0;

  const TargetMachine &m = tli->getTargetMachine();
  const StringRef triple = m.getTargetTriple();

  if (0 == triple.find("x86_64"))  return ByvalRulesX8664;
  if (0 == triple.find("i686")) return 0;
  if (0 == triple.find("armv7a")) return ByvalRulesARM;

  llvm_unreachable("Unknown arch");
  return 0;
}

// TODO: Find a better way to determine the architecture
const TypeRewriteRule* GetSretRewriteRulesForTarget(
  const TargetLowering* tli) {
  if (!FlagEnableCcRewrite) return 0;

  const TargetMachine &m = tli->getTargetMachine();
  const StringRef triple = m.getTargetTriple();

  if (0 == triple.find("x86_64"))  return SretRulesX8664;
  if (0 == triple.find("i686")) return 0;
  if (0 == triple.find("armv7a")) return SretRulesARM;

  llvm_unreachable("Unknown arch");
  return 0;
}

// This class represents the a bitcode rewrite pass which ensures
// that all ppapi interfaces are calling convention compatible
// with gcc. This pass is archtitecture dependent.
struct NaClCcRewrite : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid

  const TypeRewriteRule* SretRewriteRules;
  const TypeRewriteRule* ByvalRewriteRules;

  explicit NaClCcRewrite(const TargetLowering *tli = 0)
    : FunctionPass(ID),
      SretRewriteRules(GetSretRewriteRulesForTarget(tli)),
      ByvalRewriteRules(GetByvalRewriteRulesForTarget(tli)) {
    initializeNaClCcRewritePass(*PassRegistry::getPassRegistry());
  }

  // main pass entry point
  bool runOnFunction(Function &F);

 private:
  void RewriteCallsite(Instruction* call, LLVMContext& C);
  void RewriteFunctionPrologAndEpilog(Function& F);
};

char NaClCcRewrite::ID = 0;

// This is only used for dst side of rules
Type* GetElementaryType(char c, LLVMContext& C) {
  switch (c) {
   case 'i':
    return Type::getInt32Ty(C);
   case 'l':
    return Type::getInt64Ty(C);
   case 'd':
    return Type::getDoubleTy(C);
   case 'f':
    return Type::getFloatTy(C);
   default:
    dbgs() << c << "\n";
    llvm_unreachable("Unknown type specifier");
    return 0;
  }
}

// This is only used for the dst side of a rule
int GetElementaryTypeWidth(char c) {
  switch (c) {
   case 'i':
   case 'f':
    return 4;
   case 'l':
   case 'd':
    return 8;
   default:
    llvm_unreachable("Unknown type specifier");
    return 0;
  }
}

// Check whether a type matches the *src* side pattern of a rewrite rule.
// Note that the pattern parameter is updated during the recursion
bool HasRewriteType(const Type* type, const char*& pattern) {
  switch (*pattern++) {
   case '\0':
    return false;
   case ')':
    return false;
   case 's':   // struct and union are currently no distinguished
    {
      if (*pattern++ != '(')  llvm_unreachable("malformed type pattern");
      if (!type->isStructTy()) return false;
      // check struct members
      const StructType* st = cast<StructType>(type);
      for (StructType::element_iterator it = st->element_begin(),
                                        end = st->element_end();
           it != end;
           ++it) {
        if (!HasRewriteType(*it, pattern)) return false;
      }
      // ensure we reached the end
      int c = *pattern++;
      return c == ')';
    }
    break;
   case 'c':
    return type->isIntegerTy(8);
   case 'i':
    return type->isIntegerTy(32);
   case 'l':
    return type->isIntegerTy(64);
   case 'd':
    return type->isDoubleTy();
   case 'f':
    return type->isFloatTy();
   case 'F':
    return type->isFunctionTy();
   case 'p':  // untyped pointer
    return type->isPointerTy();
   case 'P':  // typed pointer
    {
      if (*pattern++ != '(')  llvm_unreachable("malformed type pattern");
      if (!type->isPointerTy()) return false;
      Type* pointee = dyn_cast<PointerType>(type)->getElementType();
      if (!HasRewriteType(pointee, pattern)) return false;
      int c = *pattern++;
      return c == ')';
    }
   default:
    llvm_unreachable("Unknown type specifier");
    return false;
  }
}

// Match a type against a set of rewrite rules.
// Return the matching rule, if any.
const TypeRewriteRule* MatchRewriteRules(
  const Type* type, const TypeRewriteRule* rules) {
  if (rules == 0) return 0;
  for (; rules->name != 0; ++rules) {
    const char* pattern = rules->src;
    if (HasRewriteType(type, pattern)) return rules;
  }
  return 0;
}

// Same as MatchRewriteRules but "dereference" type first.
const TypeRewriteRule* MatchRewriteRulesPointee(const Type* t,
                                                const TypeRewriteRule* Rules) {
  // sret and byval are both modelled as pointers
  const PointerType* pointer = dyn_cast<PointerType>(t);
  if (pointer == 0) return 0;

  return MatchRewriteRules(pointer->getElementType(), Rules);
}

// Note, the attributes are not part of the type but are stored
// with the CallInst and/or the Function (if any)
Type* CreateFunctionPointerType(Type* result_type,
                                std::vector<Type*>& arguments) {
  FunctionType* ft = FunctionType::get(result_type,
                                       arguments,
                                       false);
  return PointerType::getUnqual(ft);
}

// Determines whether a function body needs a rewrite
bool FunctionNeedsRewrite(const Function* fun,
                          const TypeRewriteRule* ByvalRewriteRules,
                          const TypeRewriteRule* SretRewriteRules) {
  // TODO: can this be detected on indirect callsites as well.
  //       if we skip the rewrite for the function body
  //       we also need to skip it at the callsites
  // if (F.isVarArg()) return false;

  for (Function::const_arg_iterator AI = fun->arg_begin(), AE = fun->arg_end();
       AI != AE;
       ++AI) {
    const Argument& a = *AI;
    const Type* t = a.getType();
    // byval and srets are modelled as pointers (to structs)
    if (!t->isPointerTy()) continue;
    Type* pointee = dyn_cast<PointerType>(t)->getElementType();

    if (ByvalRewriteRules && a.hasByValAttr()) {
      if (0 != MatchRewriteRules(pointee, ByvalRewriteRules)) return true;
    }

    if (SretRewriteRules && a.hasStructRetAttr()) {
      if (0 != MatchRewriteRules(pointee, SretRewriteRules)) return true;
    }
  }
  return false;
}

// Used for sret rewrites to determine the new function result type
Type* GetNewReturnType(Type* type,
                       const TypeRewriteRule* rule,
                       LLVMContext& C) {
  if (std::string("C") == rule->dst) {
    if (!type->isPointerTy()) {
      llvm_unreachable("unexpected return type for C");
    }
    Type* pointee = dyn_cast<PointerType>(type)->getElementType();
    return pointee;
  } else if (std::string("l") == rule->dst ||
             std::string("d") == rule->dst) {
    return GetElementaryType(rule->dst[0], C);
  } else if (rule->dst[0] == 's') {
    const char* cp = rule->dst + 2; // skip 's('
    std::vector<Type*> fields;
    while (*cp != ')') {
      fields.push_back(GetElementaryType(*cp, C));
      ++cp;
    }
    return StructType::get(C, fields, false /* isPacked */);
  } else {
    dbgs() << *type << " " << rule->name << "\n";
    llvm_unreachable("unexpected return type");
    return 0;
  }
}

// Rewrite sret parameter while rewriting a function
Type* RewriteFunctionSret(Function& F,
                          Value* orig_val,
                          const TypeRewriteRule* rule) {
  LLVMContext& C = F.getContext();
  BasicBlock& entry = F.getEntryBlock();
  Instruction* before = &(entry.front());
  Type* old_type = orig_val->getType();
  Type* old_pointee = dyn_cast<PointerType>(old_type)->getElementType();
  Type* new_type = GetNewReturnType(old_type, rule, C);
  // create a temporary to hold the return value as we no longer pass
  // in the pointer
  AllocaInst* tmp_ret = new AllocaInst(old_pointee, "result", before);
  orig_val->replaceAllUsesWith(tmp_ret);
  CastInst* cast_ret = CastInst::CreatePointerCast(
    tmp_ret,
    PointerType::getUnqual(new_type),
    "byval_cast",
    before);
  for (Function::iterator BI = F.begin(), BE = F.end(); BI != BE; ++BI) {
    for (BasicBlock::iterator II = BI->begin(), IE = BI->end();
         II != IE;
         /* see below */) {
      Instruction* inst = II;
      // we do decontructive magic below, so advance the iterator here
      // (this is still a little iffy)
      ++II;
      ReturnInst* ret = dyn_cast<ReturnInst>(inst);
      if (ret) {
        if (ret->getReturnValue() != 0)
          llvm_unreachable("expected a void return");
        // load the return value from temporary
        Value *ret_val = new LoadInst(cast_ret, "load_result", ret);
        // return that loaded value and delete the return instruction
        ReturnInst::Create(C, ret_val, ret);
        ret->eraseFromParent();
      }
    }
  }
  return new_type;
}

// Rewrite one byval function parameter while rewriting a function
void FixFunctionByvalsParameter(Function& F,
                                std::vector<Argument*>& new_arguments,
                                std::vector<Attributes>& new_attributes,
                                Value* byval,
                                const TypeRewriteRule* rule) {
  LLVMContext& C = F.getContext();
  BasicBlock& entry = F.getEntryBlock();
  Instruction* before = &(entry.front());
  Twine prefix =  byval->getName() + "_split";
  Type* t = byval->getType();
  Type* pointee = dyn_cast<PointerType>(t)->getElementType();
  AllocaInst* tmp_param = new AllocaInst(pointee, prefix + "_param", before);
  byval->replaceAllUsesWith(tmp_param);
  // convert byval poiner to char pointer
  Value* base = CastInst::CreatePointerCast(
    tmp_param, PointerType::getInt8PtrTy(C), prefix + "_base", before);

  int width = 0;
  const char* pattern = rule->dst;
  for (int offset = 0; *pattern; ++pattern, offset += width) {
    width = GetElementaryTypeWidth(*pattern);
    Type* t = GetElementaryType(*pattern, C);
    Argument* arg = new Argument(t, prefix, &F);
    Type* pt = PointerType::getUnqual(t);
    // the code below generates something like:
    // <CHAR-PTR> = getelementptr i8* <BASE>, i32 <OFFSET-FROM-BASE>
    // <PTR> = bitcast i8* <CHAR-PTR> to <TYPE>*
    // store <ARG> <TYPE>* <ELEM-PTR>
    ConstantInt* baseOffset = ConstantInt::get(Type::getInt32Ty(C), offset);
    Value *v;
    v = GetElementPtrInst::Create(base, baseOffset, prefix + "_base_add", before);
    v = CastInst::CreatePointerCast(v, pt, prefix + "_cast", before);
    v = new StoreInst(arg, v, before);

    new_arguments.push_back(arg);
    new_attributes.push_back(Attribute::None);
  }
}

// Change function signature to reflect all the rewrites.
// This includes function type/signature and attributes.
void UpdateFunctionSignature(Function &F,
                             Type* new_result_type,
                             std::vector<Argument*>& new_arguments,
                             std::vector<Attributes>& new_attributes) {
  DEBUG(dbgs() << "PHASE PROTOTYPE UPDATE\n");
  if (new_result_type) {
    DEBUG(dbgs() << "NEW RESULT TYPE: " << *new_result_type << "\n");
  }
  // Update function type
  FunctionType* old_fun_type = F.getFunctionType();
  std::vector<Type*> new_types;
  for (size_t i = 0; i < new_arguments.size(); ++i) {
    new_types.push_back(new_arguments[i]->getType());
  }

  FunctionType* new_fun_type = FunctionType::get(
    new_result_type ? new_result_type : old_fun_type->getReturnType(),
    new_types,
    false);
  F.setType(PointerType::getUnqual(new_fun_type));

  Function::ArgumentListType& args = F.getArgumentList();
  DEBUG(dbgs() << "PHASE ARGUMENT ERASE " <<  args.size() << "\n");
  while (args.size()) {
    Argument* arg = args.remove(args.begin());
  }

  DEBUG(dbgs() << "PHASE ARGUMENT REFILL" <<  new_arguments.size()   << "\n");
  for (size_t i = 0; i < new_arguments.size(); ++i) {
    args.push_back(new_arguments[i]);
  }

  DEBUG(dbgs() << "PHASE ATTRIBUTES UPDATE\n");
  // Update function attributes
  std::vector<AttributeWithIndex> new_attributes_vec;
  for (size_t i = 0; i < new_attributes.size(); ++i) {
    Attributes attr = new_attributes[i];
    if (attr) {
      // index 0 is for the return value
      new_attributes_vec.push_back(AttributeWithIndex::get(i + 1, attr));
    }
  }
  if (Attributes attrs = F.getAttributes().getFnAttributes())
    new_attributes_vec.push_back(AttributeWithIndex::get(~0, attrs));

  F.setAttributes(AttrListPtr::get(new_attributes_vec));
}

// Apply byval or sret rewrites to function body.
void NaClCcRewrite::RewriteFunctionPrologAndEpilog(Function& F) {

  DEBUG(dbgs() << "\nFUNCTION-REWRITE\n");

  DEBUG(dbgs() << "FUNCTION BEFORE ");
  DEBUG(dbgs() << F);
  DEBUG(dbgs() << "\n");

  std::vector<Argument*> new_arguments;
  std::vector<Attributes> new_attributes;
  std::vector<Argument*> old_arguments;

  // make copy first as create Argument adds them to the list
  for (Function::arg_iterator ai = F.arg_begin(),
                             end = F.arg_end();
       ai != end;
       ++ai) {
    old_arguments.push_back(ai);
  }

  for (size_t i = 0; i < old_arguments.size(); ++i) {
    Argument* arg = old_arguments[i];
    Type* t = arg->getType();
    // index zero is for return value attributes
    Attributes attr = F.getAttributes().getParamAttributes(i + 1);
    const TypeRewriteRule* rule = 0;
    if (attr & Attribute::ByVal) {
      rule = MatchRewriteRulesPointee(t, ByvalRewriteRules);
    }
    if (rule == 0) {
      new_arguments.push_back(arg);
      new_attributes.push_back(attr);
      continue;
    }
    DEBUG(dbgs() << "REWRITING BYVAL "
          << *t << " arg " << arg->getName() << " " << rule->name << "\n");
    FixFunctionByvalsParameter(F,
                               new_arguments,
                               new_attributes,
                               arg,
                               rule);
  }

  // A non-zero new_result_type indicates an sret rewrite
  Type* new_result_type = 0;
  // only the first arg can be "sret"
  if (new_attributes[0] & Attribute::StructRet) {
    const TypeRewriteRule* sret_rule = MatchRewriteRulesPointee(
      new_arguments[0]->getType(), SretRewriteRules);
    if (sret_rule) {
      Argument* arg = F.getArgumentList().begin();
      DEBUG(dbgs() << "REWRITING SRET "
            << " arg " << arg->getName() << " " << sret_rule->name << "\n");
      new_result_type = RewriteFunctionSret(F, arg, sret_rule);
      new_arguments.erase(new_arguments.begin());
      new_attributes.erase(new_attributes.begin());
    }
  }

  UpdateFunctionSignature(F, new_result_type, new_arguments, new_attributes);

  DEBUG(dbgs() << "FUNCTION AFTER ");
  DEBUG(dbgs() << F);
  DEBUG(dbgs() << "\n");
}

// used for T in {CallInst, InvokeInst}
template<class T> bool CallNeedsRewrite(
  const Instruction* inst,
  const TypeRewriteRule* ByvalRewriteRules,
  const TypeRewriteRule* SretRewriteRules) {

  const T* call = cast<T>(inst);
  // skip non parameter operands at the end
  size_t num_params = call->getNumOperands() -
                      (isa<CallInst>(inst) ? 1 : 3);
  for (size_t i = 0; i <  num_params; ++i) {
    Type* t = call->getOperand(i)->getType();
    // byval and srets are modelled as pointers (to structs)
    if (!t->isPointerTy()) continue;
    Type* pointee = dyn_cast<PointerType>(t)->getElementType();

    //  param zero is for the return value
    if (ByvalRewriteRules && call->paramHasAttr(i + 1, Attribute::ByVal)) {
      if (0 != MatchRewriteRules(pointee, ByvalRewriteRules)) return true;
    }

    if (SretRewriteRules && call->paramHasAttr(i + 1, Attribute::StructRet)) {
      if (0 != MatchRewriteRules(pointee, SretRewriteRules)) return true;
    }
  }

  return false;
}

// This code will load the fields of the byval ptr into scalar variables
// which will then be used as argument when we rewrite the actual call
// instruction.
void PrependCompensationForByvals(std::vector<Value*>& new_operands,
                                   std::vector<Attributes>& new_attributes,
                                   Instruction* call,
                                   Value* byval,
                                   const TypeRewriteRule* rule,
                                   LLVMContext& C) {
  // convert byval poiner to char pointer
  Value* base = CastInst::CreatePointerCast(
    byval, PointerType::getInt8PtrTy(C), "byval_base", call);

  int width = 0;
  const char* pattern = rule->dst;
  for (int offset = 0; *pattern; ++pattern, offset += width) {
    width = GetElementaryTypeWidth(*pattern);
    Type* t = GetElementaryType(*pattern, C);
    Type* pt = PointerType::getUnqual(t);
    // the code below generates something like:
    // <CHAR-PTR> = getelementptr i8* <BASE>, i32 <OFFSET-FROM-BASE>
    // <PTR> = bitcast i8* <CHAR-PTR> to i32*
    // <SCALAR> = load i32* <ELEM-PTR>
    ConstantInt* baseOffset = ConstantInt::get(Type::getInt32Ty(C), offset);
    Value* v;
    v = GetElementPtrInst::Create(base, baseOffset, "byval_base_add", call);
    v = CastInst::CreatePointerCast(v, pt, "byval_cast", call);
    v = new LoadInst(v, "byval_extract", call);

    new_operands.push_back(v);
    new_attributes.push_back(Attribute::None);
  }
}

// Note: this will only be called if we expect a rewrite to occur
void CallsiteFixupSrets(Instruction* call,
                        Value* sret,
                        Type* new_type,
                        const TypeRewriteRule* rule) {
  const char* pattern = rule->dst;
  Instruction* next= call->getNextNode();
  if (next == 0) {
    llvm_unreachable("unexpected missing next instruction");
  }

  if (std::string("C") == pattern) {
    // Note, this may store complex values, e.g. struct values, same code:
    // store %struct.PP_FloatPoint <CALL-RESULT>, %struct.PP_FloatPoint* <SRET-PTR>
    new StoreInst(call, sret, next);
  } else if (pattern[0] == 's' ||
             std::string("l") == pattern ||
             std::string("d") == pattern) {
    Type* pt = PointerType::getUnqual(new_type);
    Value* cast = CastInst::CreatePointerCast(sret, pt, "cast", next);
    new StoreInst(call, cast, next);
  } else {
    dbgs() << rule->name << "\n";
    llvm_unreachable("unexpected return type at fix up");
  }
}

void ExtractOperandsAndAttributesFromCallInst(
  CallInst* call,
  std::vector<Value*>& operands,
  std::vector<Attributes>& attributes) {

  AttrListPtr PAL = call->getAttributes();
  // last operand is: function
  for (size_t i = 0; i <  call->getNumOperands() - 1; ++i) {
    operands.push_back(call->getArgOperand(i));
    // index zero is for return value attributes
    attributes.push_back(PAL.getParamAttributes(i + 1));
  }
}

// Note: this differs from the one above in the loop bounds
void ExtractOperandsAndAttributesFromeInvokeInst(
  InvokeInst* call,
  std::vector<Value*>& operands,
  std::vector<Attributes>& attributes) {
  AttrListPtr PAL = call->getAttributes();
  // last three operands are: function, bb-normal, bb-exception
  for (size_t i = 0; i <  call->getNumOperands() - 3; ++i) {
    operands.push_back(call->getArgOperand(i));
    // index zero is for return value attributes
    attributes.push_back(PAL.getParamAttributes(i + 1));
  }
}


Instruction* ReplaceCallInst(CallInst* call,
                             Type* function_pointer,
                             std::vector<Value*>& new_operands,
                             std::vector<Attributes>& new_attributes) {
  Value* v = CastInst::CreatePointerCast(
    call->getCalledValue(), function_pointer, "fp_cast", call);
  CallInst* new_call = CallInst::Create(v, new_operands, "", call);
  // NOTE: tail calls may be ruled out but byval/sret, should we assert this?
  // TODO: did wid forget to clone anything else?
  new_call->setTailCall(call->isTailCall());
  new_call->setCallingConv(call->getCallingConv());
  for (size_t i = 0; i < new_attributes.size(); ++i) {
    // index zero is for return value attributes
    new_call->addAttribute(i + 1, new_attributes[i]);
  }
  return new_call;
}

Instruction* ReplaceInvokeInst(InvokeInst* call,
                             Type* function_pointer,
                             std::vector<Value*>& new_operands,
                             std::vector<Attributes>& new_attributes) {
  Value* v = CastInst::CreatePointerCast(
    call->getCalledValue(), function_pointer, "fp_cast", call);
  InvokeInst* new_call = InvokeInst::Create(v,
                                            call->getNormalDest(),
                                            call->getUnwindDest(),
                                            new_operands,
                                            "",
                                            call);
  for (size_t i = 0; i < new_attributes.size(); ++i) {
    // index zero is for return value attributes
    new_call->addAttribute(i + 1, new_attributes[i]);
  }
  return new_call;
}


void NaClCcRewrite::RewriteCallsite(Instruction* call, LLVMContext& C) {
  BasicBlock* BB = call->getParent();

  DEBUG(dbgs() << "\nCALLSITE-REWRITE\n");
  DEBUG(dbgs() << "CALLSITE BB BEFORE " << *BB);
  DEBUG(dbgs() << "\n");
  DEBUG(dbgs() << *call << "\n");

  // new_result(_type) is only relevent if an sret is rewritten
  // whish is indicated by sret_rule != 0
  const TypeRewriteRule* sret_rule = 0;
  Type* new_result_type = call->getType();
  Value* new_result = 0;

  std::vector<Value*> old_operands;
  std::vector<Attributes> old_attributes;
  if (isa<CallInst>(call)) {
    ExtractOperandsAndAttributesFromCallInst(
      cast<CallInst>(call), old_operands, old_attributes);
  } else if (isa<InvokeInst>(call)) {
    ExtractOperandsAndAttributesFromeInvokeInst(
      cast<InvokeInst>(call), old_operands, old_attributes);
  } else {
    llvm_unreachable("Unexpected instruction type");
  }

  std::vector<Value*> new_operands;
  std::vector<Attributes> new_attributes;

  for (size_t i = 0; i <  old_operands.size(); ++i) {
    Value *operand = old_operands[i];
    Type* t = operand->getType();
    const TypeRewriteRule* rule = 0;
    if (old_attributes[i] & Attribute::ByVal) {
      rule = MatchRewriteRulesPointee(t, ByvalRewriteRules);
    }
    if (rule == 0) {
      new_operands.push_back(operand);
      new_attributes.push_back(old_attributes[i]);
      continue;
    }

    DEBUG(dbgs() << "REWRITING BYVAL "
          << *t << " arg " << i << " " << rule->name << "\n");
    PrependCompensationForByvals(new_operands,
                                 new_attributes,
                                 call,
                                 operand,
                                 rule,
                                 C);
  }

  // only the first arg can be "sret"
  if (new_attributes[0] & Attribute::StructRet) {
    sret_rule = MatchRewriteRulesPointee(
      new_operands[0]->getType(), SretRewriteRules);
  }

  // we have to patch the call before we can add the sret compensation code
  // because otherwise the type checker complains
  if (sret_rule) {
    new_result_type = GetNewReturnType(new_operands[0]->getType(), sret_rule, C);
    new_result = new_operands[0];
    new_operands.erase(new_operands.begin());
    new_attributes.erase(new_attributes.begin());
  }

  // Note, this code is tricky.
  // Initially we used a much more elaborate scheme introducing
  // new function declarations for direct calls.
  // This simpler scheme, however, works for both direct and
  // indirect calls
  // We transform (here the direct case):
  // call void @result_PP_FloatPoint(%struct.PP_FloatPoint* sret %sret)
  // into
  //  %fp_cast = bitcast void (%struct.PP_FloatPoint*)*
  //                @result_PP_FloatPoint to %struct.PP_FloatPoint ()*
  //  %result = call %struct.PP_FloatPoint %fp_cast()
  //
  std::vector<Type*> new_arg_types;
  for (size_t i = 0; i < new_operands.size(); ++i) {
    new_arg_types.push_back(new_operands[i]->getType());
  }

  DEBUG(dbgs() << "REWRITE CALL INSTRUCTION\n");
  Instruction* new_call = 0;
  if (isa<CallInst>(call)) {
    new_call = ReplaceCallInst(
      cast<CallInst>(call),
      CreateFunctionPointerType(new_result_type, new_arg_types),
      new_operands,
      new_attributes);
  } else if (isa<InvokeInst>(call)) {
    new_call = ReplaceInvokeInst(
      cast<InvokeInst>(call),
      CreateFunctionPointerType(new_result_type, new_arg_types),
      new_operands,
      new_attributes);
  } else {
    llvm_unreachable("Unexpected instruction type");
  }

  // We prepended the new call, now get rid of the old one.
  // If we did not change the return type, there may be consumers
  // of the result which must be redirected.
  if (!sret_rule) {
    call->replaceAllUsesWith(new_call);
  }
  call->eraseFromParent();

  // Add compensation codes for srets if necessary
  if (sret_rule) {
    DEBUG(dbgs() << "REWRITING  SRET " << sret_rule->name << "\n");
    CallsiteFixupSrets(new_call, new_result, new_result_type, sret_rule);
  }

  DEBUG(dbgs() << "CALLSITE BB AFTER" << *BB);
  DEBUG(dbgs() << "\n");
  DEBUG(dbgs() << *new_call << "\n");
}

bool NaClCcRewrite::runOnFunction(Function &F) {
  // No rules - no action
  if (ByvalRewriteRules == 0 && SretRewriteRules == 0) return false;

  bool Changed = false;

  if (FunctionNeedsRewrite(&F, ByvalRewriteRules, SretRewriteRules)) {
    DEBUG(dbgs() << "FUNCTION NEEDS REWRITE " << F.getName() << "\n");
    RewriteFunctionPrologAndEpilog(F);
    Changed = true;
  }

  // Find all the calls and invokes in F and rewrite them if necessary
  for (Function::iterator BI = F.begin(), BE = F.end(); BI != BE; ++BI) {
    for (BasicBlock::iterator II = BI->begin(), IE = BI->end();
         II != IE;
         /* II updated below */) {
      Instruction* inst = II;
      // we do decontructive magic below, so advance the iterator here
      // (this is still a little iffy)
      ++II;

      if (isa<InvokeInst>(inst) || isa<CallInst>(inst))  {
        if (isa<CallInst>(inst) &&
            !CallNeedsRewrite<CallInst>
            (inst, ByvalRewriteRules, SretRewriteRules)) continue;

        if (isa<InvokeInst>(inst) &&
            !CallNeedsRewrite<InvokeInst>
            (inst, ByvalRewriteRules, SretRewriteRules)) continue;

        RewriteCallsite(inst, F.getContext());
        Changed = true;
      }
    }
  }
  return Changed;
}

} // end anonymous namespace


INITIALIZE_PASS(NaClCcRewrite, "naclcc", "NaCl CC Rewriter", false, false)

FunctionPass *llvm::createNaClCcRewritePass(const TargetLowering *tli) {
  return new NaClCcRewrite(tli);
}

