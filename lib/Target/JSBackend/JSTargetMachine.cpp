#include "JSTargetMachine.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/PassManager.h"
using namespace llvm;

JSTargetMachine::JSTargetMachine(const Target &T, StringRef Triple,
                                 StringRef CPU, StringRef FS, const TargetOptions &Options,
                                 Reloc::Model RM, CodeModel::Model CM,
                                 CodeGenOpt::Level OL)
  : TargetMachine(T, Triple, CPU, FS, Options),
    DL("e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-"
       "f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128") {
  CodeGenInfo = T.createMCCodeGenInfo(Triple, RM, CM, OL);
}
