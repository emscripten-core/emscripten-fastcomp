// Call handlers: flexible map of call targets to arbitrary handling code
//
// Each handler needs DEF_CALL_HANDLER and SETUP_CALL_HANDLER

typedef std::string (CppWriter::*CallHandler)(const CallInst*, std::string Name, int NumArgs);
typedef std::map<std::string, CallHandler> CallHandlerMap;
CallHandlerMap *CallHandlers;

// Definitions

#define DEF_CALL_HANDLER(Ident, Code) \
  std::string CH_##Ident(const CallInst *CI, std::string Name, int NumArgs=-1) { Code }

DEF_CALL_HANDLER(__default__, {
  const Value *CV = CI->getCalledValue();
  if (!isa<Function>(CV)) {
    // function pointer call
    FunctionType *FT = dyn_cast<FunctionType>(dyn_cast<PointerType>(CV->getType())->getElementType());
    std::string Sig = getFunctionSignature(FT);
    Name = std::string("FUNCTION_TABLE_") + Sig + "[" + Name + " & #FM_" + Sig + "#]";
    ensureFunctionTable(FT);
  }
  std::string text = Name + "(";
  if (NumArgs == -1) NumArgs = CI->getNumOperands()-1; // last operand is the function itself
  for (int i = 0; i < NumArgs; i++) {
    text += getValueAsCastStr(CI->getArgOperand(i), ASM_NONSPECIFIC); // FIXME: differentiate ffi calls
    if (i < NumArgs - 1) text += ", ";
  }
  text += ")";
  Type *RT = CI->getType();
  if (!RT->isVoidTy()) {
    text = getAssign(getCppName(CI), RT) + getCast(text, RT, ASM_NONSPECIFIC);
  }
  return text;
})

DEF_CALL_HANDLER(getHigh32, {
  return getAssign(getCppName(CI), CI->getType()) + "tempRet0";
})

DEF_CALL_HANDLER(setHigh32, {
  return "tempRet0 = " + getValueAsStr(CI->getArgOperand(0));
})

DEF_CALL_HANDLER(FPtoILow, {
  return getAssign(getCppName(CI), CI->getType()) + "(~~" + getValueAsStr(CI->getArgOperand(0)) + ")>>>0";
})

DEF_CALL_HANDLER(FPtoIHigh, {
  std::string Input = getValueAsStr(CI->getArgOperand(0));
  return getAssign(getCppName(CI), CI->getType()) + "Math_abs(" + Input + ") >= 1.0 ? " + Input + " > 0.0 ? (Math_min(+Math_floor(" + Input + " / 4294967296.0), 4294967295.0) | 0) >>> 0 : ~~+Math_ceil((" + Input + " - +(~~" + Input + " >>> 0)) / 4294967296.0) >>> 0 : 0)";
})

DEF_CALL_HANDLER(llvm_nacl_atomic_store_i32, {
  return "HEAP32[" + getValueAsStr(CI->getArgOperand(0)) + ">>2]=" + getValueAsStr(CI->getArgOperand(1));
})

DEF_CALL_HANDLER(llvm_memcpy_p0i8_p0i8_i32, {
  Declares.insert("memcpy");
  return CH___default__(CI, "_memcpy", 3) + "|0";
})

DEF_CALL_HANDLER(llvm_memset_p0i8_i32, {
  Declares.insert("memset");
  return CH___default__(CI, "_memset", 3) + "|0";
})

DEF_CALL_HANDLER(llvm_memmove_p0i8_p0i8_i32, {
  Declares.insert("memmove");
  return CH___default__(CI, "_memmove", 3) + "|0";
})

DEF_CALL_HANDLER(bitshift64Lshr, {
  return CH___default__(CI, "_bitshift64Lshr", 3) + "|0";
})

DEF_CALL_HANDLER(bitshift64Ashr, {
  return CH___default__(CI, "_bitshift64Ashr", 3) + "|0";
})

DEF_CALL_HANDLER(bitshift64Shl, {
  return CH___default__(CI, "_bitshift64Shl", 3) + "|0";
})

#define DEF_REDIRECT_HANDLER(name, to) \
DEF_CALL_HANDLER(name, { \
  /* FIXME: do not redirect if this is implemented and not just a declare! */ \
  Declares.insert(#to); \
  return CH___default__(CI, "_" #to); \
})

// Various simple redirects for our js libc, see library.js
DEF_REDIRECT_HANDLER(putc, fputc);
DEF_REDIRECT_HANDLER(__cxa_atexit, atexit);
DEF_REDIRECT_HANDLER(ntohl, htonl);
DEF_REDIRECT_HANDLER(ntohs, htons);
DEF_REDIRECT_HANDLER(atol, atoi);

// Setups

void setupCallHandlers() {
  CallHandlers = new CallHandlerMap;
  #define SETUP_CALL_HANDLER(Ident) \
    (*CallHandlers)[std::string("_") + #Ident] = &CppWriter::CH_##Ident;

  SETUP_CALL_HANDLER(__default__);
  SETUP_CALL_HANDLER(getHigh32);
  SETUP_CALL_HANDLER(setHigh32);
  SETUP_CALL_HANDLER(FPtoILow);
  SETUP_CALL_HANDLER(FPtoIHigh);
  SETUP_CALL_HANDLER(llvm_nacl_atomic_store_i32);
  SETUP_CALL_HANDLER(llvm_memcpy_p0i8_p0i8_i32);
  SETUP_CALL_HANDLER(llvm_memset_p0i8_i32);
  SETUP_CALL_HANDLER(llvm_memmove_p0i8_p0i8_i32);
  SETUP_CALL_HANDLER(bitshift64Lshr);
  SETUP_CALL_HANDLER(bitshift64Ashr);
  SETUP_CALL_HANDLER(bitshift64Shl);
  SETUP_CALL_HANDLER(putc);
  SETUP_CALL_HANDLER(__cxa_atexit);
  SETUP_CALL_HANDLER(ntohl);
  SETUP_CALL_HANDLER(ntohs);
  SETUP_CALL_HANDLER(atol);
}

std::string handleCall(const CallInst *CI) {
  const Value *CV = CI->getCalledValue();
  std::string Name = getCppName(CV);
  unsigned NumArgs = CI->getNumArgOperands();
  CallHandlerMap::iterator CH = CallHandlers->find("___default__");
  if (isa<Function>(CV)) {
    CallHandlerMap::iterator Custom = CallHandlers->find(Name);
    if (Custom != CallHandlers->end()) CH = Custom;
  }
  return (this->*(CH->second))(CI, Name, NumArgs);
}

