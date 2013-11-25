// Call handlers: flexible map of call targets to arbitrary handling code
//
// Each handler needs DEF_CALL_HANDLER and SETUP_CALL_HANDLER

typedef std::string (CppWriter::*CallHandler)(const CallInst*);
typedef std::map<std::string, CallHandler> CallHandlerMap;
CallHandlerMap *CallHandlers;

// Definitions

#define DEF_CALL_HANDLER(Ident, Code) \
  std::string CH_##Ident(const CallInst *CI) { Code }

DEF_CALL_HANDLER(__default__, {
  const int numArgs = CI->getNumArgOperands();
  Type *RT = CI->getCalledFunction()->getReturnType();
  std::string text = getCppName(CI->getCalledValue()) + "(";
  for (int i = 0; i < numArgs; i++) {
    text += getValueAsCastStr(CI->getArgOperand(i)); // FIXME: differentiate ffi calls
    if (i < numArgs - 1) text += ", ";
  }
  text += ")";
  if (!RT->isVoidTy()) {
    text = getAssign(getCppName(CI), RT) + getCast(text, RT);
  }
  return text + ';';
})

DEF_CALL_HANDLER(llvm_nacl_atomic_store_i32, {
  return "HEAP32[" + getValueAsStr(CI->getArgOperand(0)) + ">>2]=" + getValueAsStr(CI->getArgOperand(1)) + ";";
})


// Setups

void setupCallHandlers() {
  CallHandlers = new CallHandlerMap;
  #define SETUP_CALL_HANDLER(Ident) \
    (*CallHandlers)[std::string("_") + #Ident] = &CppWriter::CH_##Ident;

  SETUP_CALL_HANDLER(__default__);
  SETUP_CALL_HANDLER(llvm_nacl_atomic_store_i32);
}

std::string handleCall(const CallInst *CI) {
  CallHandlerMap::iterator CH = CallHandlers->find(getCppName(CI->getCalledValue()));
  if (CH == CallHandlers->end()) CH = CallHandlers->find("___default__");
  return (this->*(CH->second))(CI);
}

