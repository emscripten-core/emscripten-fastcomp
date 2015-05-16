//===- llvm/unittest/Bitcode/NaClBitReaderTest.cpp - Tests for BitReader --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallString.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MemoryBuffer.h"
#include "gtest/gtest.h"

namespace llvm {
namespace {

static std::unique_ptr<Module> makeLLVMModule() {
  std::unique_ptr<Module> Mod(new Module("test-mem", getGlobalContext()));

  FunctionType* FuncTy =
    FunctionType::get(Type::getVoidTy(Mod->getContext()), false);
  Function* Func = Function::Create(FuncTy,GlobalValue::ExternalLinkage,
                                    "func", Mod.get());

  BasicBlock* Entry = BasicBlock::Create(Mod->getContext(), "entry", Func);
  new UnreachableInst(Mod->getContext(), Entry);

  BasicBlock* BB = BasicBlock::Create(Mod->getContext(), "bb", Func);
  new UnreachableInst(Mod->getContext(), BB);

  return Mod;
}

static void writeModuleToBuffer(SmallVectorImpl<char> &Buffer) {
  std::unique_ptr<Module> Mod = makeLLVMModule();
  raw_svector_ostream OS(Buffer);
  NaClWriteBitcodeToFile(Mod.get(), OS);
}

// Check that we can parse a good bitcode file.
TEST(NaClBitReaderTest, MaterializeSimpleModule) {
  SmallString<1024> Mem;
  writeModuleToBuffer(Mem);
  std::unique_ptr<MemoryBuffer> Buffer = MemoryBuffer::getMemBuffer(Mem.str(), "test", false);
  ErrorOr<Module *> ModuleOrErr =
      getNaClLazyBitcodeModule(std::move(Buffer), getGlobalContext());
  EXPECT_EQ(true, bool(ModuleOrErr));
  // Do something with the module just to make sure it was built.
  std::unique_ptr<Module> M(ModuleOrErr.get());
  EXPECT_NE((Module *)nullptr, M.get());
  M->getFunction("func")->materialize();
  EXPECT_FALSE(verifyModule(*M, &dbgs()));
}

// Test that we catch bad stuff at the end of a bitcode file.
TEST(NaClBitReaderTest, BadDataAfterModule) {
  SmallString<1024> Mem;
  writeModuleToBuffer(Mem);
  Mem.append("more"); // Length must be divisible by 4!
  std::unique_ptr<MemoryBuffer> Buffer = MemoryBuffer::getMemBuffer(Mem.str(), "test", false);
  ErrorOr<Module *> ModuleOrErr =
      getNaClLazyBitcodeModule(std::move(Buffer), getGlobalContext());
  EXPECT_EQ(false, bool(ModuleOrErr));
  std::string BadMessage("Invalid data after module");
  EXPECT_EQ(BadMessage, ModuleOrErr.getError().message());
}

}
}
