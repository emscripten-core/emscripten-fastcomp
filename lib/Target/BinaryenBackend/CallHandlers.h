// Call handlers: flexible map of call targets to arbitrary handling code
//
// Each handler needs DEF_CALL_HANDLER and SETUP_CALL_HANDLER
//
// Call handlers emit the code that the call will be replaced by. If that
// emitted code contains calls, it must add the targets to Declares,
// which are reported as declared but not implemented symbols, so that
// JS linking brings them in.

typedef std::string (BinaryenWriter::*CallHandler)(const Instruction*, std::string Name, int NumArgs);
typedef std::map<std::string, CallHandler> CallHandlerMap;
CallHandlerMap CallHandlers;

// Definitions

unsigned getNumArgOperands(const Instruction *I) {
  return ImmutableCallSite(I).arg_size();
}

const Value *getActuallyCalledValue(const Instruction *I) {
  const Value *CV = ImmutableCallSite(I).getCalledValue();

  // if the called value is a bitcast of a function, then we just call it directly, properly
  // for example, extern void x() in C will turn into void x(...) in LLVM IR, then the IR bitcasts
  // it to the proper form right before the call. this both causes an unnecessary indirect
  // call, and it is done with the wrong type. TODO: don't even put it into the function table
  if (const Function *F = dyn_cast<const Function>(CV->stripPointerCasts())) {
    CV = F;
  }
  return CV;
}

// We can't and shouldn't try to invoke an LLVM intrinsic which we overload with a call hander -
// it would end up in a function table, which makes no sense.
bool canInvoke(const Value *V) {
  const Function *F = dyn_cast<const Function>(V);
  if (F && F->isDeclaration() && F->isIntrinsic()) {
    auto Intrin = F->getIntrinsicID();
    if (Intrin == Intrinsic::memcpy || Intrin == Intrinsic::memset || Intrin == Intrinsic::memmove) {
      return false;
    }
  }
  return true;
}

#define DEF_CALL_HANDLER(Ident, Code) \
  std::string CH_##Ident(const Instruction *CI, std::string Name, int NumArgs=-1) { Code }

DEF_CALL_HANDLER(__default__, {
  if (!CI) return ""; // we are just called from a handler that was called from getFunctionIndex, only to ensure the handler was run at least once
  const Value *CV = getActuallyCalledValue(CI);
  bool NeedCasts = true;
  FunctionType *FT;
  bool Invoke = false;
  bool Emulated = false;
  if (InvokeState == 1) {
    InvokeState = 2;
    Invoke = canInvoke(CV);
  }
  std::string Sig;
  bool IsMath = Name.find("Math_") == 0;
  bool ForcedNumArgs = NumArgs != -1;
  if (!ForcedNumArgs) NumArgs = getNumArgOperands(CI);

  const Function *F = dyn_cast<const Function>(CV);
  if (F) {
    NeedCasts = F->isDeclaration(); // if ffi call, need casts
    if (IsMath && !NeedCasts) {
      // this was renamed to a math function, but the actual function is implemented, presumably from libc; use that
      IsMath = false;
      Name = getSimpleName(F);
    }
    FT = F->getFunctionType();
  } else {
    FT = dyn_cast<FunctionType>(dyn_cast<PointerType>(CV->getType())->getElementType());
    if (isAbsolute(CV->stripPointerCasts())) {
      Name = "abort /* segfault, call an absolute addr */ ";
    } else {
      // function pointer call
      ensureFunctionTable(FT);
      if (!Invoke) {
        Sig = getFunctionSignature(FT);
        if (!EmulatedFunctionPointers) {
          Name = std::string("FUNCTION_TABLE_") + Sig + "[" + Name + " & #FM_" + Sig + "#]";
          NeedCasts = false; // function table call, so stays in asm module
        } else {
          Name = std::string(Relocatable ? "mftCall_" : "ftCall_") + Sig + "(" + getCast(Name, Type::getInt32Ty(CI->getContext()));
          if (NumArgs > 0) Name += ',';
          Emulated = true;
        }
      }
    }
  }

  if (!FT->isVarArg() && !ForcedNumArgs) {
    int TypeNumArgs = FT->getNumParams();
    if (TypeNumArgs != NumArgs) {
      if (EmscriptenAssertions) prettyWarning() << "unexpected number of arguments " << utostr(NumArgs) << " in call to '" << F->getName() << "', should be " << utostr(TypeNumArgs) << "\n";
      if (NumArgs > TypeNumArgs) NumArgs = TypeNumArgs; // lop off the extra params that will not be used and just break validation
    }
    if (EmscriptenAssertions) {
      for (int i = 0; i < std::min(TypeNumArgs, NumArgs); i++) {
        Type *TypeType = FT->getParamType(i);
        Type *ActualType = CI->getOperand(i)->getType();
        if (getFunctionSignatureLetter(TypeType) != getFunctionSignatureLetter(ActualType)) {
          prettyWarning() << "unexpected argument type " << *ActualType << " at index " << utostr(i) << " in call to '" << F->getName() << "', should be " << *TypeType << "\n";
        }
      }
    }
  }
  if (EmscriptenAssertions) {
    Type *TypeType = FT->getReturnType();
    Type *ActualType = CI->getType();
    if (getFunctionSignatureLetter(TypeType) != getFunctionSignatureLetter(ActualType)) {
      prettyWarning() << "unexpected return type " << *ActualType << " in call to '" << F->getName() << "', should be " << *TypeType << "\n";
    }
  }

  if (Invoke) {
    Sig = getFunctionSignature(FT);
    Name = "invoke_" + Sig;
    NeedCasts = true;
  }
  std::string text = Name;
  if (!Emulated) text += "(";
  if (Invoke) {
    // add first param
    if (F) {
      text += relocateFunctionPointer(utostr(getFunctionIndex(F))); // convert to function pointer
    } else {
      text += getValueAsCastStr(CV); // already a function pointer
    }
    if (NumArgs > 0) text += ",";
  }
  // this is an ffi call if we need casts, and it is not a special Math_ builtin
  bool FFI = NeedCasts;
  if (FFI && IsMath) {
    if (Name == "Math_ceil" || Name == "Math_floor" || Name == "Math_min" || Name == "Math_max" || Name == "Math_sqrt" || Name == "Math_abs") {
      // This special Math builtin is optimizable with all types, including floats, so can treat it as non-ffi
      FFI = false;
    }
  }
  unsigned FFI_OUT = FFI ? ASM_FFI_OUT : 0;
  for (int i = 0; i < NumArgs; i++) {
    if (!NeedCasts) {
      text += getValueAsStr(CI->getOperand(i));
    } else {
      text += getValueAsCastParenStr(CI->getOperand(i), ASM_NONSPECIFIC | FFI_OUT);
    }
    if (i < NumArgs - 1) text += ",";
  }
  text += ")";
  // handle return value
  Type *InstRT = CI->getType();
  Type *ActualRT = FT->getReturnType();
  if (!InstRT->isVoidTy() && ActualRT->isVoidTy()) {
    // the function we are calling was cast to something returning a value, but it really
    // does not return a value
    getAssignIfNeeded(CI); // ensure the variable is defined, but do not emit it here
                           // it should have 0 uses, but just to be safe
  } else if (!ActualRT->isVoidTy()) {
    unsigned FFI_IN = FFI ? ASM_FFI_IN : 0;
    text = getAssignIfNeeded(CI) + "(" + getCast(text, ActualRT, ASM_NONSPECIFIC | FFI_IN) + ")";
  }
  return text;
})

// exceptions support
DEF_CALL_HANDLER(emscripten_preinvoke, {
  // InvokeState is normally 0 here, but might be otherwise if a block was split apart TODO: add a function attribute for this
  InvokeState = 1;
  return "__THREW__ = 0";
})
DEF_CALL_HANDLER(emscripten_postinvoke, {
  // InvokeState is normally 2 here, but can be 1 if the call in between was optimized out, or 0 if a block was split apart
  InvokeState = 0;
  return getAssign(CI) + "__THREW__; __THREW__ = 0";
})
DEF_CALL_HANDLER(emscripten_landingpad, {
  unsigned Num = getNumArgOperands(CI);
  std::string target = "__cxa_find_matching_catch_" + utostr(Num);
  Declares.insert(target);
  std::string Ret = getAssign(CI) + "_" + target + "(";
  for (unsigned i = 1; i < Num-1; i++) { // ignore personality and cleanup XXX - we probably should not be doing that!
    if (i > 1) Ret += ",";
    Ret += getValueAsCastStr(CI->getOperand(i));
  }
  Ret += ")|0";
  return Ret;
})
DEF_CALL_HANDLER(emscripten_resume, {
  Declares.insert("__resumeException");
  return "___resumeException(" + getValueAsCastStr(CI->getOperand(0)) + ")";
})

std::string getTempRet0() {
  return Relocatable ? "(getTempRet0() | 0)" : "tempRet0";
}

std::string setTempRet0(std::string Value) {
  return Relocatable ? "setTempRet0((" + Value + ") | 0)" : "tempRet0 = (" + Value + ")";
}

// setjmp support

DEF_CALL_HANDLER(emscripten_prep_setjmp, {
  return getAdHocAssign("_setjmpTableSize", Type::getInt32Ty(CI->getContext())) + "4;" +
         getAdHocAssign("_setjmpTable", Type::getInt32Ty(CI->getContext())) + "_malloc(40) | 0;" +
         "HEAP32[_setjmpTable>>2]=0";
})
DEF_CALL_HANDLER(emscripten_cleanup_setjmp, {
  return "_free(_setjmpTable|0)";
})
DEF_CALL_HANDLER(emscripten_setjmp, {
  // env, label, table
  Declares.insert("saveSetjmp");
  return "_setjmpTable = _saveSetjmp(" + getValueAsStr(CI->getOperand(0)) + "," + getValueAsStr(CI->getOperand(1)) + ",_setjmpTable|0,_setjmpTableSize|0)|0;_setjmpTableSize = " + getTempRet0();
})
DEF_CALL_HANDLER(emscripten_longjmp, {
  Declares.insert("longjmp");
  return CH___default__(CI, "_longjmp");
})
DEF_CALL_HANDLER(emscripten_check_longjmp, {
  std::string Threw = getValueAsStr(CI->getOperand(0));
  std::string Target = getSimpleName(CI);
  std::string Assign = getAssign(CI);
  return "if (((" + Threw + "|0) != 0) & ((threwValue|0) != 0)) { " +
           Assign + "_testSetjmp(HEAP32[" + Threw + ">>2]|0, _setjmpTable|0, _setjmpTableSize|0)|0; " +
           "if ((" + Target + "|0) == 0) { _longjmp(" + Threw + "|0, threwValue|0); } " + // rethrow
           setTempRet0("threwValue") + "; " +
         "} else { " + Assign + "-1; }";
})
DEF_CALL_HANDLER(emscripten_get_longjmp_result, {
  std::string Threw = getValueAsStr(CI->getOperand(0));
  return getAssign(CI) + getTempRet0();
})

// supporting async functions, see `<emscripten>/src/library_async.js` for detail.
DEF_CALL_HANDLER(emscripten_alloc_async_context, {
  Declares.insert("emscripten_alloc_async_context");
  // insert sp as the 2nd parameter
  return getAssign(CI) + "_emscripten_alloc_async_context(" + getValueAsStr(CI->getOperand(0)) + ",sp)|0";
})
DEF_CALL_HANDLER(emscripten_check_async, {
  return getAssign(CI) + "___async";
})
// prevent unwinding the stack
// preserve the return value of the return inst
DEF_CALL_HANDLER(emscripten_do_not_unwind, {
  return "sp = STACKTOP";
})
// prevent unwinding the async stack
DEF_CALL_HANDLER(emscripten_do_not_unwind_async, {
  return "___async_unwind = 0";
})
DEF_CALL_HANDLER(emscripten_get_async_return_value_addr, {
  return getAssign(CI) + "___async_retval";
})

// emscripten instrinsics
DEF_CALL_HANDLER(emscripten_debugger, {
  CantValidate = "emscripten_debugger is used";
  return "debugger";
})
DEF_CALL_HANDLER(llvm_debugtrap, {
  CantValidate = "llvm.debugtrap is used";
  return "debugger";
})

// i64 support

DEF_CALL_HANDLER(getHigh32, {
  return getAssign(CI) + getTempRet0();
})
DEF_CALL_HANDLER(setHigh32, {
  return setTempRet0(getValueAsStr(CI->getOperand(0)));
})
// XXX float handling here is not optimal
#define TO_I(low, high) \
DEF_CALL_HANDLER(low, { \
  std::string Input = getValueAsStr(CI->getOperand(0)); \
  if (PreciseF32 && CI->getOperand(0)->getType()->isFloatTy()) Input = "+" + Input; \
  return getAssign(CI) + "(~~" + Input + ")>>>0"; \
}) \
DEF_CALL_HANDLER(high, { \
  std::string Input = getValueAsStr(CI->getOperand(0)); \
  if (PreciseF32 && CI->getOperand(0)->getType()->isFloatTy()) Input = "+" + Input; \
  return getAssign(CI) + "+Math_abs(" + Input + ") >= +1 ? " + Input + " > +0 ? (~~+Math_min(+Math_floor(" + Input + " / +4294967296), +4294967295)) >>> 0 : ~~+Math_ceil((" + Input + " - +(~~" + Input + " >>> 0)) / +4294967296) >>> 0 : 0"; \
})
TO_I(FtoILow, FtoIHigh);
TO_I(DtoILow, DtoIHigh);
DEF_CALL_HANDLER(BDtoILow, {
  return "HEAPF64[tempDoublePtr>>3] = " + getValueAsStr(CI->getOperand(0)) + ";" + getAssign(CI) + "HEAP32[tempDoublePtr>>2]|0";
})
DEF_CALL_HANDLER(BDtoIHigh, {
  return getAssign(CI) + "HEAP32[tempDoublePtr+4>>2]|0";
})
DEF_CALL_HANDLER(SItoF, {
  std::string Ret = "(+" + getValueAsCastParenStr(CI->getOperand(0), ASM_UNSIGNED) + ") + " +
                                       "(+4294967296*(+" + getValueAsCastParenStr(CI->getOperand(1), ASM_SIGNED) +   "))";
  if (PreciseF32 && CI->getType()->isFloatTy()) {
    Ret = "Math_fround(" + Ret + ")";
  }
  return getAssign(CI) + Ret;
})
DEF_CALL_HANDLER(UItoF, {
  std::string Ret = "(+" + getValueAsCastParenStr(CI->getOperand(0), ASM_UNSIGNED) + ") + " +
                                       "(+4294967296*(+" + getValueAsCastParenStr(CI->getOperand(1), ASM_UNSIGNED) + "))";
  if (PreciseF32 && CI->getType()->isFloatTy()) {
    Ret = "Math_fround(" + Ret + ")";
  }
  return getAssign(CI) + Ret;
})
DEF_CALL_HANDLER(SItoD, {
  return getAssign(CI) + "(+" + getValueAsCastParenStr(CI->getOperand(0), ASM_UNSIGNED) + ") + " +
                                       "(+4294967296*(+" + getValueAsCastParenStr(CI->getOperand(1), ASM_SIGNED) +   "))";
})
DEF_CALL_HANDLER(UItoD, {
  return getAssign(CI) + "(+" + getValueAsCastParenStr(CI->getOperand(0), ASM_UNSIGNED) + ") + " +
                                       "(+4294967296*(+" + getValueAsCastParenStr(CI->getOperand(1), ASM_UNSIGNED) + "))";
})
DEF_CALL_HANDLER(BItoD, {
  return "HEAP32[tempDoublePtr>>2] = " +   getValueAsStr(CI->getOperand(0)) + ";" +
         "HEAP32[tempDoublePtr+4>>2] = " + getValueAsStr(CI->getOperand(1)) + ";" +
         getAssign(CI) + "+HEAPF64[tempDoublePtr>>3]";
})

// misc

DEF_CALL_HANDLER(llvm_nacl_atomic_store_i32, {
  return "HEAP32[" + getValueAsStr(CI->getOperand(0)) + ">>2]=" + getValueAsStr(CI->getOperand(1));
})

#define CMPXCHG_HANDLER(name, HeapName) \
DEF_CALL_HANDLER(name, { \
  const Value *P = CI->getOperand(0); \
  if (EnablePthreads) { \
    return getAssign(CI) + "(Atomics_compareExchange(" HeapName ", " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ", " + getValueAsStr(CI->getOperand(2)) + ")|0)"; \
  } else { \
    return getLoad(CI, P, CI->getType(), 0) + ';' + \
             "if ((" + getCast(getSimpleName(CI), CI->getType()) + ") == " + getValueAsCastParenStr(CI->getOperand(1)) + ") " + \
                getStore(CI, P, CI->getType(), getValueAsStr(CI->getOperand(2)), 0); \
  } \
})

CMPXCHG_HANDLER(llvm_nacl_atomic_cmpxchg_i8, "HEAP8");
CMPXCHG_HANDLER(llvm_nacl_atomic_cmpxchg_i16, "HEAP16");
CMPXCHG_HANDLER(llvm_nacl_atomic_cmpxchg_i32, "HEAP32");

#define UNROLL_LOOP_MAX 8
#define WRITE_LOOP_MAX 128

DEF_CALL_HANDLER(llvm_memcpy_p0i8_p0i8_i32, {
  if (CI) {
    ConstantInt *AlignInt = dyn_cast<ConstantInt>(CI->getOperand(3));
    if (AlignInt) {
      ConstantInt *LenInt = dyn_cast<ConstantInt>(CI->getOperand(2));
      if (LenInt) {
        // we can emit inline code for this
        unsigned Len = LenInt->getZExtValue();
        if (Len <= WRITE_LOOP_MAX) {
          unsigned Align = AlignInt->getZExtValue();
          if (Align > 4) Align = 4;
          else if (Align == 0) Align = 1; // align 0 means 1 in memcpy and memset (unlike other places where it means 'default/4')
          if (Align == 1 && Len > 1 && WarnOnUnaligned) {
            errs() << "emcc: warning: unaligned memcpy in  " << CI->getParent()->getParent()->getName() << ":" << *CI << " (compiler's fault?)\n";
          }
          unsigned Pos = 0;
          std::string Ret;
          std::string Dest = getValueAsStr(CI->getOperand(0));
          std::string Src = getValueAsStr(CI->getOperand(1));
          while (Len > 0) {
            // handle as much as we can in the current alignment
            unsigned CurrLen = Align*(Len/Align);
            unsigned Factor = CurrLen/Align;
            if (Factor <= UNROLL_LOOP_MAX) {
              // unroll
              for (unsigned Offset = 0; Offset < CurrLen; Offset += Align) {
                unsigned PosOffset = Pos + Offset;
                std::string Add = PosOffset == 0 ? "" : ("+" + utostr(PosOffset));
                Ret += ";" + getHeapAccess(Dest + Add, Align) + "=" + getHeapAccess(Src + Add, Align) + "|0";
              }
            } else {
              // emit a loop
              UsedVars["dest"] = UsedVars["src"] = UsedVars["stop"] = Type::getInt32Ty(TheModule->getContext());
              std::string Add = Pos == 0 ? "" : ("+" + utostr(Pos) + "|0");
              Ret += "dest=" + Dest + Add + "; src=" + Src + Add + "; stop=dest+" + utostr(CurrLen) + "|0; do { " + getHeapAccess("dest", Align) + "=" + getHeapAccess("src", Align) + "|0; dest=dest+" + utostr(Align) + "|0; src=src+" + utostr(Align) + "|0; } while ((dest|0) < (stop|0))";
            }
            Pos += CurrLen;
            Len -= CurrLen;
            Align /= 2;
          }
          return Ret;
        }
      }
    }
  }
  Declares.insert("memcpy");
  return CH___default__(CI, "_memcpy", 3) + "|0";
})

DEF_CALL_HANDLER(llvm_memset_p0i8_i32, {
  if (CI) {
    ConstantInt *AlignInt = dyn_cast<ConstantInt>(CI->getOperand(3));
    if (AlignInt) {
      ConstantInt *LenInt = dyn_cast<ConstantInt>(CI->getOperand(2));
      if (LenInt) {
        ConstantInt *ValInt = dyn_cast<ConstantInt>(CI->getOperand(1));
        if (ValInt) {
          // we can emit inline code for this
          unsigned Len = LenInt->getZExtValue();
          if (Len <= WRITE_LOOP_MAX) {
            unsigned Align = AlignInt->getZExtValue();
            unsigned Val = ValInt->getZExtValue();
            if (Align > 4) Align = 4;
            else if (Align == 0) Align = 1; // align 0 means 1 in memcpy and memset (unlike other places where it means 'default/4')
            if (Align == 1 && Len > 1 && WarnOnUnaligned) {
              errs() << "emcc: warning: unaligned memcpy in  " << CI->getParent()->getParent()->getName() << ":" << *CI << " (compiler's fault?)\n";
            }
            unsigned Pos = 0;
            std::string Ret;
            std::string Dest = getValueAsStr(CI->getOperand(0));
            while (Len > 0) {
              // handle as much as we can in the current alignment
              unsigned CurrLen = Align*(Len/Align);
              unsigned FullVal = 0;
              for (unsigned i = 0; i < Align; i++) {
                FullVal <<= 8;
                FullVal |= Val;
              }
              unsigned Factor = CurrLen/Align;
              if (Factor <= UNROLL_LOOP_MAX) {
                // unroll
                for (unsigned Offset = 0; Offset < CurrLen; Offset += Align) {
                  unsigned PosOffset = Pos + Offset;
                  std::string Add = PosOffset == 0 ? "" : ("+" + utostr(PosOffset));
                  Ret += ";" + getHeapAccess(Dest + Add, Align) + "=" + utostr(FullVal) + "|0";
                }
              } else {
                // emit a loop
                UsedVars["dest"] = UsedVars["stop"] = Type::getInt32Ty(TheModule->getContext());
                std::string Add = Pos == 0 ? "" : ("+" + utostr(Pos) + "|0");
                Ret += "dest=" + Dest + Add + "; stop=dest+" + utostr(CurrLen) + "|0; do { " + getHeapAccess("dest", Align) + "=" + utostr(FullVal) + "|0; dest=dest+" + utostr(Align) + "|0; } while ((dest|0) < (stop|0))";
              }
              Pos += CurrLen;
              Len -= CurrLen;
              Align /= 2;
            }
            return Ret;
          }
        }
      }
    }
  }
  Declares.insert("memset");
  return CH___default__(CI, "_memset", 3) + "|0";
})

DEF_CALL_HANDLER(llvm_memmove_p0i8_p0i8_i32, {
  Declares.insert("memmove");
  return CH___default__(CI, "_memmove", 3) + "|0";
})

DEF_CALL_HANDLER(llvm_expect_i32, {
  return getAssign(CI) + getValueAsStr(CI->getOperand(0));
})
DEF_CALL_HANDLER(llvm_expect_i1, {
  return getAssign(CI) + getValueAsStr(CI->getOperand(0));
})

DEF_CALL_HANDLER(llvm_dbg_declare, {
  if (!EnableCyberDWARF || !EnableCyberDWARFIntrinsics)
    return "";

  auto VariableOffset = "0";
  auto AssignedValue = cast<MetadataAsValue>(CI->getOperand(0))->getMetadata();
  auto const LocalVariableMD = cast<MetadataAsValue>(CI->getOperand(1))->getMetadata();
  auto const LocalVariableDI = cast<DILocalVariable>(LocalVariableMD);
  auto const LocalVariableType = LocalVariableDI->getRawType();
  auto const DwarfOp = cast<MetadataAsValue>(CI->getOperand(2))->getMetadata();
  std::string LocalVariableName = LocalVariableDI->getName().str();

  auto VarMD = utostr(getIDForMetadata(LocalVariableType))
  + "," + VariableOffset + "," + utostr(getIDForMetadata(DwarfOp))
  + ",\"" + LocalVariableName + "\"";


  if (auto const *ValAsAssign = dyn_cast<LocalAsMetadata>(AssignedValue)) {
    Declares.insert("metadata_llvm_dbg_value_local");
    auto LocalVarName = getSimpleName(ValAsAssign->getValue()->stripPointerCasts());
    return "_metadata_llvm_dbg_value_local(" + LocalVarName + "," + VarMD + ")";
  } else if (auto const *ValAsAssign = dyn_cast<ConstantAsMetadata>(AssignedValue)) {
    Declares.insert("metadata_llvm_dbg_value_constant");
    return "_metadata_llvm_dbg_value_constant(\"" + getValueAsStr(ValAsAssign->getValue())
    + "," + VarMD + ")";
  }

  return "";
})

DEF_CALL_HANDLER(llvm_dbg_value, {
  if (!EnableCyberDWARF || !EnableCyberDWARFIntrinsics)
    return "";

  auto VariableOffset = getValueAsStr(CI->getOperand(1));
  auto AssignedValue = cast<MetadataAsValue>(CI->getOperand(0))->getMetadata();
  auto const LocalVariableMD = cast<MetadataAsValue>(CI->getOperand(1))->getMetadata();
  auto const LocalVariableDI = cast<DILocalVariable>(LocalVariableMD);
  auto const LocalVariableType = LocalVariableDI->getRawType();
  auto const DwarfOp = cast<MetadataAsValue>(CI->getOperand(2))->getMetadata();
  std::string LocalVariableName = LocalVariableDI->getName().str();

  auto VarMD = utostr(getIDForMetadata(LocalVariableType))
               + "," + VariableOffset + "," + utostr(getIDForMetadata(DwarfOp))
               + ",\"" + LocalVariableName + "\"";

  if (auto const *ValAsAssign = dyn_cast<LocalAsMetadata>(AssignedValue)) {
    Declares.insert("metadata_llvm_dbg_value_local");
    auto LocalVarName = getSimpleName(ValAsAssign->getValue()->stripPointerCasts());
    return "_metadata_llvm_dbg_value_local(" + LocalVarName + "," + VarMD + ")";
  } else if (auto const *ValAsAssign = dyn_cast<ConstantAsMetadata>(AssignedValue)) {
    Declares.insert("metadata_llvm_dbg_value_constant");
    return "_metadata_llvm_dbg_value_constant(\"" + getValueAsStr(ValAsAssign->getValue())
    + "," + VarMD + ")";
  }

  return "";
})

DEF_CALL_HANDLER(llvm_lifetime_start, {
  return "";
})

DEF_CALL_HANDLER(llvm_lifetime_end, {
  return "";
})

DEF_CALL_HANDLER(llvm_invariant_start, {
  return "";
})

DEF_CALL_HANDLER(llvm_invariant_end, {
  return "";
})

DEF_CALL_HANDLER(llvm_prefetch, {
  return "";
})

DEF_CALL_HANDLER(llvm_objectsize_i32_p0i8, {
  return  getAssign(CI) + ((cast<ConstantInt>(CI->getOperand(1)))->getZExtValue() == 0 ? "-1" : "0");
})

DEF_CALL_HANDLER(llvm_flt_rounds, {
  // FLT_ROUNDS helper. We don't support setting the rounding mode dynamically,
  // so it's always round-to-nearest (1).
  return getAssign(CI) + "1";
})

DEF_CALL_HANDLER(bitshift64Lshr, {
  Declares.insert("bitshift64Lshr");
  return CH___default__(CI, "_bitshift64Lshr", 3);
})

DEF_CALL_HANDLER(bitshift64Ashr, {
  Declares.insert("bitshift64Ashr");
  return CH___default__(CI, "_bitshift64Ashr", 3);
})

DEF_CALL_HANDLER(bitshift64Shl, {
  Declares.insert("bitshift64Shl");
  return CH___default__(CI, "_bitshift64Shl", 3);
})

DEF_CALL_HANDLER(llvm_ctlz_i32, {
  return CH___default__(CI, "Math_clz32", 1);
})

DEF_CALL_HANDLER(llvm_cttz_i32, {
  Declares.insert("llvm_cttz_i32");
  return CH___default__(CI, "_llvm_cttz_i32", 1);
})

// EM_ASM support

std::string handleAsmConst(const Instruction *CI) {
  unsigned Num = getNumArgOperands(CI);
  std::string Sig;
  Sig += getFunctionSignatureLetter(CI->getType());
  for (unsigned i = 1; i < Num; i++) {
    Sig += getFunctionSignatureLetter(CI->getOperand(i)->getType());
  }
  std::string func = "emscripten_asm_const_" + Sig;
  std::string ret = "_" + func + "(" + utostr(getAsmConstId(CI->getOperand(0), Sig));
  for (unsigned i = 1; i < Num; i++) {
    ret += ", " + getValueAsCastParenStr(CI->getOperand(i), ASM_NONSPECIFIC);
  }
  return ret + ")";
}

DEF_CALL_HANDLER(emscripten_asm_const, {
  Declares.insert("emscripten_asm_const");
  return handleAsmConst(CI);
})
DEF_CALL_HANDLER(emscripten_asm_const_int, {
  Declares.insert("emscripten_asm_const_int");
  return getAssign(CI) + getCast(handleAsmConst(CI), Type::getInt32Ty(CI->getContext()));
})
DEF_CALL_HANDLER(emscripten_asm_const_double, {
  Declares.insert("emscripten_asm_const_double");
  return getAssign(CI) + getCast(handleAsmConst(CI), Type::getDoubleTy(CI->getContext()));
})

DEF_CALL_HANDLER(emscripten_atomic_exchange_u8, {
  return getAssign(CI) + "(Atomics_exchange(HEAP8, " + getValueAsStr(CI->getOperand(0)) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_exchange_u16, {
  return getAssign(CI) + "(Atomics_exchange(HEAP16, " + getShiftedPtr(CI->getOperand(0), 2) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_exchange_u32, {
  return getAssign(CI) + "(Atomics_exchange(HEAP32, " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})

DEF_CALL_HANDLER(emscripten_atomic_cas_u8, {
  return getAssign(CI) + "(Atomics_compareExchange(HEAP8, " + getValueAsStr(CI->getOperand(0)) + ", " + getValueAsStr(CI->getOperand(1)) + ", " + getValueAsStr(CI->getOperand(2)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_cas_u16, {
  return getAssign(CI) + "(Atomics_compareExchange(HEAP16, " + getShiftedPtr(CI->getOperand(0), 2) + ", " + getValueAsStr(CI->getOperand(1)) + ", " + getValueAsStr(CI->getOperand(2)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_cas_u32, {
  return getAssign(CI) + "(Atomics_compareExchange(HEAP32, " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ", " + getValueAsStr(CI->getOperand(2)) + ")|0)";
})

DEF_CALL_HANDLER(emscripten_atomic_load_u8, {
  return getAssign(CI) + "(Atomics_load(HEAP8, " + getValueAsStr(CI->getOperand(0)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_load_u16, {
  return getAssign(CI) + "(Atomics_load(HEAP16, " + getShiftedPtr(CI->getOperand(0), 2) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_load_u32, {
  return getAssign(CI) + "(Atomics_load(HEAP32, " + getShiftedPtr(CI->getOperand(0), 4) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_load_f32, {
  // TODO: If https://bugzilla.mozilla.org/show_bug.cgi?id=1131613 is implemented, we could use the commented out version. Until then,
  // we must emulate manually.
  Declares.insert("_Atomics_load_f32_emulated");
  return getAssign(CI) + (PreciseF32 ? "Math_fround(" : "+") + "__Atomics_load_f32_emulated(" + getShiftedPtr(CI->getOperand(0), 4) + (PreciseF32 ? "))" : ")");
//  return getAssign(CI) + "Atomics_load(HEAPF32, " + getShiftedPtr(CI->getOperand(0), 4) + ")";
})
DEF_CALL_HANDLER(emscripten_atomic_load_f64, {
  // TODO: If https://bugzilla.mozilla.org/show_bug.cgi?id=1131624 is implemented, we could use the commented out version. Until then,
  // we must emulate manually.
  Declares.insert("emscripten_atomic_load_f64");
  return getAssign(CI) + "+_emscripten_atomic_load_f64(" + getShiftedPtr(CI->getOperand(0), 8) + ")";
//  return getAssign(CI) + "Atomics_load(HEAPF64, " + getShiftedPtr(CI->getOperand(0), 8) + ")";
})

DEF_CALL_HANDLER(emscripten_atomic_store_u8, {
  return getAssign(CI) + "(Atomics_store(HEAP8, " + getValueAsStr(CI->getOperand(0)) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_store_u16, {
  return getAssign(CI) + "(Atomics_store(HEAP16, " + getShiftedPtr(CI->getOperand(0), 2) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_store_u32, {
  return getAssign(CI) + "(Atomics_store(HEAP32, " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_store_f32, {
  // TODO: If https://bugzilla.mozilla.org/show_bug.cgi?id=1131613 is implemented, we could use the commented out version. Until then,
  // we must emulate manually.
  Declares.insert("emscripten_atomic_store_f32");
  return getAssign(CI) + "_emscripten_atomic_store_f32(" + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ")";
//  return getAssign(CI) + "Atomics_store(HEAPF32, " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ")";
})
DEF_CALL_HANDLER(emscripten_atomic_store_f64, {
  // TODO: If https://bugzilla.mozilla.org/show_bug.cgi?id=1131624 is implemented, we could use the commented out version. Until then,
  // we must emulate manually.
  Declares.insert("emscripten_atomic_store_f64");
  return getAssign(CI) + "+_emscripten_atomic_store_f64(" + getShiftedPtr(CI->getOperand(0), 8) + ", " + getValueAsStr(CI->getOperand(1)) + ")";
//  return getAssign(CI) + "Atomics_store(HEAPF64, " + getShiftedPtr(CI->getOperand(0), 8) + ", " + getValueAsStr(CI->getOperand(1)) + ")";
})

DEF_CALL_HANDLER(emscripten_atomic_add_u8, {
  return getAssign(CI) + "(Atomics_add(HEAP8, " + getValueAsStr(CI->getOperand(0)) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_add_u16, {
  return getAssign(CI) + "(Atomics_add(HEAP16, " + getShiftedPtr(CI->getOperand(0), 2) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_add_u32, {
  return getAssign(CI) + "(Atomics_add(HEAP32, " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})

DEF_CALL_HANDLER(emscripten_atomic_sub_u8, {
  return getAssign(CI) + "(Atomics_sub(HEAP8, " + getValueAsStr(CI->getOperand(0)) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_sub_u16, {
  return getAssign(CI) + "(Atomics_sub(HEAP16, " + getShiftedPtr(CI->getOperand(0), 2) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_sub_u32, {
  return getAssign(CI) + "(Atomics_sub(HEAP32, " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})

DEF_CALL_HANDLER(emscripten_atomic_and_u8, {
  return getAssign(CI) + "(Atomics_and(HEAP8, " + getValueAsStr(CI->getOperand(0)) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_and_u16, {
  return getAssign(CI) + "(Atomics_and(HEAP16, " + getShiftedPtr(CI->getOperand(0), 2) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_and_u32, {
  return getAssign(CI) + "(Atomics_and(HEAP32, " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})

DEF_CALL_HANDLER(emscripten_atomic_or_u8, {
  return getAssign(CI) + "(Atomics_or(HEAP8, " + getValueAsStr(CI->getOperand(0)) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_or_u16, {
  return getAssign(CI) + "(Atomics_or(HEAP16, " + getShiftedPtr(CI->getOperand(0), 2) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_or_u32, {
  return getAssign(CI) + "(Atomics_or(HEAP32, " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})

DEF_CALL_HANDLER(emscripten_atomic_xor_u8, {
  return getAssign(CI) + "(Atomics_xor(HEAP8, " + getValueAsStr(CI->getOperand(0)) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_xor_u16, {
  return getAssign(CI) + "(Atomics_xor(HEAP16, " + getShiftedPtr(CI->getOperand(0), 2) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})
DEF_CALL_HANDLER(emscripten_atomic_xor_u32, {
  return getAssign(CI) + "(Atomics_xor(HEAP32, " + getShiftedPtr(CI->getOperand(0), 4) + ", " + getValueAsStr(CI->getOperand(1)) + ")|0)";
})

#define DEF_BUILTIN_HANDLER(name, to) \
DEF_CALL_HANDLER(name, { \
  return CH___default__(CI, #to); \
})

#define DEF_MAYBE_BUILTIN_HANDLER(name, to) \
DEF_CALL_HANDLER(name, { \
  if (!WebAssembly) return CH___default__(CI, #to); \
  return CH___default__(CI, "_" #name); \
})

// Various simple redirects for our js libc, see library.js and LibraryManager.load
DEF_BUILTIN_HANDLER(abs, Math_abs);
DEF_BUILTIN_HANDLER(labs, Math_abs);
DEF_MAYBE_BUILTIN_HANDLER(cos, Math_cos);
DEF_MAYBE_BUILTIN_HANDLER(cosf, Math_cos);
DEF_MAYBE_BUILTIN_HANDLER(cosl, Math_cos);
DEF_MAYBE_BUILTIN_HANDLER(sin, Math_sin);
DEF_MAYBE_BUILTIN_HANDLER(sinf, Math_sin);
DEF_MAYBE_BUILTIN_HANDLER(sinl, Math_sin);
DEF_MAYBE_BUILTIN_HANDLER(tan, Math_tan);
DEF_MAYBE_BUILTIN_HANDLER(tanf, Math_tan);
DEF_MAYBE_BUILTIN_HANDLER(tanl, Math_tan);
DEF_MAYBE_BUILTIN_HANDLER(acos, Math_acos);
DEF_MAYBE_BUILTIN_HANDLER(acosf, Math_acos);
DEF_MAYBE_BUILTIN_HANDLER(acosl, Math_acos);
DEF_MAYBE_BUILTIN_HANDLER(asin, Math_asin);
DEF_MAYBE_BUILTIN_HANDLER(asinf, Math_asin);
DEF_MAYBE_BUILTIN_HANDLER(asinl, Math_asin);
DEF_MAYBE_BUILTIN_HANDLER(atan, Math_atan);
DEF_MAYBE_BUILTIN_HANDLER(atanf, Math_atan);
DEF_MAYBE_BUILTIN_HANDLER(atanl, Math_atan);
DEF_MAYBE_BUILTIN_HANDLER(atan2, Math_atan2);
DEF_MAYBE_BUILTIN_HANDLER(atan2f, Math_atan2);
DEF_MAYBE_BUILTIN_HANDLER(atan2l, Math_atan2);
DEF_MAYBE_BUILTIN_HANDLER(exp, Math_exp);
DEF_MAYBE_BUILTIN_HANDLER(expf, Math_exp);
DEF_MAYBE_BUILTIN_HANDLER(expl, Math_exp);
DEF_MAYBE_BUILTIN_HANDLER(log, Math_log);
DEF_MAYBE_BUILTIN_HANDLER(logf, Math_log);
DEF_MAYBE_BUILTIN_HANDLER(logl, Math_log);
DEF_BUILTIN_HANDLER(sqrt, Math_sqrt);
DEF_BUILTIN_HANDLER(sqrtf, Math_sqrt);
DEF_BUILTIN_HANDLER(sqrtl, Math_sqrt);
DEF_BUILTIN_HANDLER(fabs, Math_abs);
DEF_BUILTIN_HANDLER(fabsf, Math_abs);
DEF_BUILTIN_HANDLER(fabsl, Math_abs);
DEF_BUILTIN_HANDLER(llvm_fabs_f32, Math_abs);
DEF_BUILTIN_HANDLER(llvm_fabs_f64, Math_abs);
DEF_BUILTIN_HANDLER(ceil, Math_ceil);
DEF_BUILTIN_HANDLER(ceilf, Math_ceil);
DEF_BUILTIN_HANDLER(ceill, Math_ceil);
DEF_BUILTIN_HANDLER(floor, Math_floor);
DEF_BUILTIN_HANDLER(floorf, Math_floor);
DEF_BUILTIN_HANDLER(floorl, Math_floor);
DEF_MAYBE_BUILTIN_HANDLER(pow, Math_pow);
DEF_MAYBE_BUILTIN_HANDLER(powf, Math_pow);
DEF_MAYBE_BUILTIN_HANDLER(powl, Math_pow);
DEF_BUILTIN_HANDLER(llvm_sqrt_f32, Math_sqrt);
DEF_BUILTIN_HANDLER(llvm_sqrt_f64, Math_sqrt);
DEF_BUILTIN_HANDLER(llvm_pow_f32, Math_pow); // XXX these will be slow in wasm, but need to link in libc before getting here, or stop
DEF_BUILTIN_HANDLER(llvm_pow_f64, Math_pow); //     LLVM from creating these intrinsics

DEF_CALL_HANDLER(llvm_powi_f32, {
  return getAssign(CI) + getParenCast("Math_pow(" + getValueAsCastStr(CI->getOperand(0)) + ", " + getCast(getValueAsCastStr(CI->getOperand(1)), CI->getOperand(0)->getType()) + ")", CI->getType());
})
DEF_CALL_HANDLER(llvm_powi_f64, {
  return getAssign(CI) + getParenCast("Math_pow(" + getValueAsCastStr(CI->getOperand(0)) + ", " + getCast(getValueAsCastStr(CI->getOperand(1)), CI->getOperand(0)->getType()) + ")", CI->getType());
})

DEF_BUILTIN_HANDLER(llvm_log_f32, Math_log);
DEF_BUILTIN_HANDLER(llvm_log_f64, Math_log);
DEF_BUILTIN_HANDLER(llvm_exp_f32, Math_exp);
DEF_BUILTIN_HANDLER(llvm_exp_f64, Math_exp);

// Setups

void setupCallHandlers() {
  assert(CallHandlers.empty());
  #define SETUP_CALL_HANDLER(Ident) \
    CallHandlers["_" #Ident] = &BinaryenWriter::CH_##Ident;

  SETUP_CALL_HANDLER(__default__);
  SETUP_CALL_HANDLER(emscripten_preinvoke);
  SETUP_CALL_HANDLER(emscripten_postinvoke);
  SETUP_CALL_HANDLER(emscripten_landingpad);
  SETUP_CALL_HANDLER(emscripten_resume);
  SETUP_CALL_HANDLER(emscripten_prep_setjmp);
  SETUP_CALL_HANDLER(emscripten_cleanup_setjmp);
  SETUP_CALL_HANDLER(emscripten_setjmp);
  SETUP_CALL_HANDLER(emscripten_longjmp);
  SETUP_CALL_HANDLER(emscripten_check_longjmp);
  SETUP_CALL_HANDLER(emscripten_get_longjmp_result);
  SETUP_CALL_HANDLER(emscripten_alloc_async_context);
  SETUP_CALL_HANDLER(emscripten_check_async);
  SETUP_CALL_HANDLER(emscripten_do_not_unwind);
  SETUP_CALL_HANDLER(emscripten_do_not_unwind_async);
  SETUP_CALL_HANDLER(emscripten_get_async_return_value_addr);
  SETUP_CALL_HANDLER(emscripten_debugger);
  SETUP_CALL_HANDLER(llvm_debugtrap);
  SETUP_CALL_HANDLER(getHigh32);
  SETUP_CALL_HANDLER(setHigh32);
  SETUP_CALL_HANDLER(FtoILow);
  SETUP_CALL_HANDLER(FtoIHigh);
  SETUP_CALL_HANDLER(DtoILow);
  SETUP_CALL_HANDLER(DtoIHigh);
  SETUP_CALL_HANDLER(BDtoILow);
  SETUP_CALL_HANDLER(BDtoIHigh);
  SETUP_CALL_HANDLER(SItoF);
  SETUP_CALL_HANDLER(UItoF);
  SETUP_CALL_HANDLER(SItoD);
  SETUP_CALL_HANDLER(UItoD);
  SETUP_CALL_HANDLER(BItoD);
  SETUP_CALL_HANDLER(llvm_nacl_atomic_store_i32);
  SETUP_CALL_HANDLER(llvm_nacl_atomic_cmpxchg_i8);
  SETUP_CALL_HANDLER(llvm_nacl_atomic_cmpxchg_i16);
  SETUP_CALL_HANDLER(llvm_nacl_atomic_cmpxchg_i32);
  SETUP_CALL_HANDLER(llvm_memcpy_p0i8_p0i8_i32);
  SETUP_CALL_HANDLER(llvm_memset_p0i8_i32);
  SETUP_CALL_HANDLER(llvm_memmove_p0i8_p0i8_i32);
  SETUP_CALL_HANDLER(llvm_expect_i32);
  SETUP_CALL_HANDLER(llvm_expect_i1);
  SETUP_CALL_HANDLER(llvm_dbg_declare);
  SETUP_CALL_HANDLER(llvm_dbg_value);
  SETUP_CALL_HANDLER(llvm_lifetime_start);
  SETUP_CALL_HANDLER(llvm_lifetime_end);
  SETUP_CALL_HANDLER(llvm_invariant_start);
  SETUP_CALL_HANDLER(llvm_invariant_end);
  SETUP_CALL_HANDLER(llvm_prefetch);
  SETUP_CALL_HANDLER(llvm_objectsize_i32_p0i8);
  SETUP_CALL_HANDLER(llvm_flt_rounds);
  SETUP_CALL_HANDLER(bitshift64Lshr);
  SETUP_CALL_HANDLER(bitshift64Ashr);
  SETUP_CALL_HANDLER(bitshift64Shl);
  SETUP_CALL_HANDLER(llvm_ctlz_i32);
  SETUP_CALL_HANDLER(llvm_cttz_i32);

  SETUP_CALL_HANDLER(abs);
  SETUP_CALL_HANDLER(labs);
  SETUP_CALL_HANDLER(cos);
  SETUP_CALL_HANDLER(cosf);
  SETUP_CALL_HANDLER(cosl);
  SETUP_CALL_HANDLER(sin);
  SETUP_CALL_HANDLER(sinf);
  SETUP_CALL_HANDLER(sinl);
  SETUP_CALL_HANDLER(tan);
  SETUP_CALL_HANDLER(tanf);
  SETUP_CALL_HANDLER(tanl);
  SETUP_CALL_HANDLER(acos);
  SETUP_CALL_HANDLER(acosf);
  SETUP_CALL_HANDLER(acosl);
  SETUP_CALL_HANDLER(asin);
  SETUP_CALL_HANDLER(asinf);
  SETUP_CALL_HANDLER(asinl);
  SETUP_CALL_HANDLER(atan);
  SETUP_CALL_HANDLER(atanf);
  SETUP_CALL_HANDLER(atanl);
  SETUP_CALL_HANDLER(atan2);
  SETUP_CALL_HANDLER(atan2f);
  SETUP_CALL_HANDLER(atan2l);
  SETUP_CALL_HANDLER(exp);
  SETUP_CALL_HANDLER(expf);
  SETUP_CALL_HANDLER(expl);
  SETUP_CALL_HANDLER(log);
  SETUP_CALL_HANDLER(logf);
  SETUP_CALL_HANDLER(logl);
  SETUP_CALL_HANDLER(sqrt);
  SETUP_CALL_HANDLER(sqrtf);
  SETUP_CALL_HANDLER(sqrtl);
  SETUP_CALL_HANDLER(fabs);
  SETUP_CALL_HANDLER(fabsf);
  SETUP_CALL_HANDLER(fabsl);
  SETUP_CALL_HANDLER(llvm_fabs_f32);
  SETUP_CALL_HANDLER(llvm_fabs_f64);
  SETUP_CALL_HANDLER(ceil);
  SETUP_CALL_HANDLER(ceilf);
  SETUP_CALL_HANDLER(ceill);
  SETUP_CALL_HANDLER(floor);
  SETUP_CALL_HANDLER(floorf);
  SETUP_CALL_HANDLER(floorl);
  SETUP_CALL_HANDLER(pow);
  SETUP_CALL_HANDLER(powf);
  SETUP_CALL_HANDLER(powl);
  SETUP_CALL_HANDLER(llvm_sqrt_f32);
  SETUP_CALL_HANDLER(llvm_sqrt_f64);
  SETUP_CALL_HANDLER(llvm_pow_f32);
  SETUP_CALL_HANDLER(llvm_pow_f64);
  SETUP_CALL_HANDLER(llvm_powi_f32);
  SETUP_CALL_HANDLER(llvm_powi_f64);
  SETUP_CALL_HANDLER(llvm_log_f32);
  SETUP_CALL_HANDLER(llvm_log_f64);
  SETUP_CALL_HANDLER(llvm_exp_f32);
  SETUP_CALL_HANDLER(llvm_exp_f64);
}

std::string handleCall(const Instruction *CI) {
  const Value *CV = getActuallyCalledValue(CI);
  if (const InlineAsm* IA = dyn_cast<const InlineAsm>(CV)) {
    if (IA->hasSideEffects() && IA->getAsmString() == "") {
      return "/* asm() memory 'barrier' */";
    } else {
      errs() << "In function " << CI->getParent()->getParent()->getName() << "()\n";
      errs() << *IA << "\n";
      report_fatal_error("asm() with non-empty content not supported, use EM_ASM() (see emscripten.h)");
    }
  }

  // Get the name to call this function by. If it's a direct call, meaning
  // which know which Function we're calling, avoid calling getValueAsStr, as
  // we don't need to use a function index.
  const std::string &Name = isa<Function>(CV) ? getSimpleName(CV) : getValueAsStr(CV);

  CallHandlerMap::iterator CH = CallHandlers.find("___default__");
  if (isa<Function>(CV)) {
    CallHandlerMap::iterator Custom = CallHandlers.find(Name);
    if (Custom != CallHandlers.end()) CH = Custom;
  }
  return (this->*(CH->second))(CI, Name, -1);
}
