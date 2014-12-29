//===- AllocateDataSegment.cpp - Create a template for the data segment ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Code sandboxed with MinSFI cannot access the memory containing its data
// segment directly because it is located outside its address subspace. To
// this end, this pass collates all of the global variables in the module
// into an exported global struct named "__sfi_data_segment" and a corresponding
// global integer holding the overall size. The runtime is expected to link
// against these variables and to initialize the memory region of the sandbox
// by copying the data segment template into a fixed address inside the region.
//
// This pass assumes that the base of the memory region of the sandbox is
// aligned to at least 2^29 bytes (=512MB), which is the maximum global variable
// alignment supported by LLVM.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/MinSFI.h"

using namespace llvm;

static const char ExternalSymName_DataSegment[] = "__sfi_data_segment";
static const char ExternalSymName_DataSegmentSize[] = "__sfi_data_segment_size";

static const uint32_t DataSegmentBaseAddress = 0x10000;

namespace {
class AllocateDataSegment : public ModulePass {
 public:
  static char ID;
  AllocateDataSegment() : ModulePass(ID) {
    initializeAllocateDataSegmentPass(*PassRegistry::getPassRegistry());
  }

  virtual bool runOnModule(Module &M);
};
}  // namespace

static inline uint32_t getPadding(uint32_t Offset, const GlobalVariable *GV,
                                  const DataLayout &DL) {
  uint32_t Alignment = DL.getPreferredAlignment(GV);
  if (Alignment <= 1)
    return 0;
  else
    return OffsetToAlignment(Offset, Alignment);
}

bool AllocateDataSegment::runOnModule(Module &M) {
  DataLayout DL(&M);
  Type *I8 = Type::getInt8Ty(M.getContext());
  Type *I32 = Type::getInt32Ty(M.getContext());
  Type *IntPtrType = DL.getIntPtrType(M.getContext());

  // First, we do a pass over the global variables, in which we compute
  // the amount of required padding between them and consequently their
  // addresses relative to the memory base of the sandbox. References to each
  // global are then replaced with direct memory pointers.
  uint32_t VarOffset = 0;
  DenseMap<GlobalVariable*, uint32_t> VarPadding;
  for (Module::global_iterator GV = M.global_begin(), E = M.global_end();
       GV != E; ++GV) {
    assert(GV->hasInitializer());

    uint32_t Padding = getPadding(VarOffset, GV, DL);
    VarPadding[GV] = Padding;
    VarOffset += Padding;

    GV->replaceAllUsesWith(
        ConstantExpr::getIntToPtr(
            ConstantInt::get(IntPtrType,
                             DataSegmentBaseAddress + VarOffset),
            GV->getType()));

    VarOffset += DL.getTypeStoreSize(GV->getType()->getPointerElementType());
  }

  // Using the offsets computed above, we prepare the layout and the contents
  // of the desired data structure. After the type and initializer of each
  // global is copied, the global is not needed any more and it is erased.
  SmallVector<Type*, 10> TemplateLayout;
  SmallVector<Constant*, 10> TemplateData;
  for (Module::global_iterator It = M.global_begin(), E = M.global_end();
       It != E; ) {
    GlobalVariable *GV = It++;

    uint32_t Padding = VarPadding[GV];
    if (Padding > 0) {
      Type *PaddingType = ArrayType::get(I8, Padding);
      TemplateLayout.push_back(PaddingType);
      TemplateData.push_back(ConstantAggregateZero::get(PaddingType));
    }

    TemplateLayout.push_back(GV->getType()->getPointerElementType());
    TemplateData.push_back(GV->getInitializer());

    GV->eraseFromParent();
  }

  // Finally, we create the struct and size global variables.
  StructType *TemplateType =
      StructType::create(M.getContext(), ExternalSymName_DataSegment);
  TemplateType->setBody(TemplateLayout, /*isPacked=*/true);

  Constant *Template = ConstantStruct::get(TemplateType, TemplateData);
  new GlobalVariable(M, Template->getType(), /*isConstant=*/true,
                     GlobalVariable::ExternalLinkage, Template,
                     ExternalSymName_DataSegment);

  Constant *TemplateSize =
      ConstantInt::get(I32, DL.getTypeAllocSize(TemplateType));
  new GlobalVariable(M, TemplateSize->getType(), /*isConstant=*/true,
                     GlobalVariable::ExternalLinkage, TemplateSize,
                     ExternalSymName_DataSegmentSize);

  return true;
}

char AllocateDataSegment::ID = 0;
INITIALIZE_PASS(AllocateDataSegment, "minsfi-allocate-data-segment",
                "Create a template for the data segment", false, false)

ModulePass *llvm::createAllocateDataSegmentPass() {
  return new AllocateDataSegment();
}
