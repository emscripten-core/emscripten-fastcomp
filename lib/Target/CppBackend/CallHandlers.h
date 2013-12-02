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
  Type *RT = CI->getType();
  std::string text = Name + "(";
  if (NumArgs == -1) NumArgs = CI->getNumOperands();
  for (int i = 0; i < NumArgs; i++) {
    text += getValueAsCastStr(CI->getArgOperand(i)); // FIXME: differentiate ffi calls
    if (i < NumArgs - 1) text += ", ";
  }
  text += ")";
  if (!RT->isVoidTy()) {
    text = getAssign(getCppName(CI), RT) + getCast(text, RT);
  }
  return text;
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

#define DEF_REDIRECT_HANDLER_i(name, to) \
DEF_CALL_HANDLER(name, { \
  Declares.insert(#to); \
  return CH___default__(CI, "_" #to) + "|0"; \
})

DEF_REDIRECT_HANDLER_i(putc, fputc);

// Setups

void setupCallHandlers() {
  CallHandlers = new CallHandlerMap;
  #define SETUP_CALL_HANDLER(Ident) \
    (*CallHandlers)[std::string("_") + #Ident] = &CppWriter::CH_##Ident;

  SETUP_CALL_HANDLER(__default__);
  SETUP_CALL_HANDLER(llvm_nacl_atomic_store_i32);
  SETUP_CALL_HANDLER(llvm_memcpy_p0i8_p0i8_i32);
  SETUP_CALL_HANDLER(llvm_memset_p0i8_i32);
  SETUP_CALL_HANDLER(llvm_memmove_p0i8_p0i8_i32);
  SETUP_CALL_HANDLER(putc);
}

std::string handleCall(const CallInst *CI) {
  std::string Name = getCppName(CI->getCalledValue());
  unsigned NumArgs = CI->getNumArgOperands();
  CallHandlerMap::iterator CH = CallHandlers->find(Name);
  if (CH == CallHandlers->end()) CH = CallHandlers->find("___default__");
  return (this->*(CH->second))(CI, Name, NumArgs);
}

