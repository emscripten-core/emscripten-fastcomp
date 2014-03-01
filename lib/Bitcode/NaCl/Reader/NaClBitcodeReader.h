//===- NaClBitcodeReader.h ------------------------------------*- C++ -*-===//
//     Internal NaClBitcodeReader implementation
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This header defines the NaClBitcodeReader class.
//
//===----------------------------------------------------------------------===//

#ifndef NACL_BITCODE_READER_H
#define NACL_BITCODE_READER_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"
#include "llvm/GVMaterializer.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/OperandTraits.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/ValueHandle.h"
#include <vector>

namespace llvm {
  class MemoryBuffer;
  class LLVMContext;
  class CastInst;

// Models a Cast.  Used to cache casts created in a basic block by the
// PNaCl bitcode reader.
struct NaClBitcodeReaderCast {
  // Fields of the conversion.
  Instruction::CastOps Op;
  Type *Ty;
  Value *Val;

  NaClBitcodeReaderCast(Instruction::CastOps Op, Type *Ty, Value *Val)
    : Op(Op), Ty(Ty), Val(Val) {}
};

// Models the data structure used to hash/compare Casts in a DenseMap.
template<>
struct DenseMapInfo<NaClBitcodeReaderCast> {
public:
  static NaClBitcodeReaderCast getEmptyKey() {
    return NaClBitcodeReaderCast(Instruction::CastOpsEnd,
                                 DenseMapInfo<Type*>::getEmptyKey(),
                                 DenseMapInfo<Value*>::getEmptyKey());
  }
  static NaClBitcodeReaderCast getTombstoneKey() {
    return NaClBitcodeReaderCast(Instruction::CastOpsEnd,
                                 DenseMapInfo<Type*>::getTombstoneKey(),
                                 DenseMapInfo<Value*>::getTombstoneKey());
  }
  static unsigned getHashValue(const NaClBitcodeReaderCast &C) {
    std::pair<int, std::pair<Type*, Value*> > Tuple;
    Tuple.first = C.Op;
    Tuple.second.first = C.Ty;
    Tuple.second.second = C.Val;
    return DenseMapInfo<std::pair<int, std::pair<Type*, Value*> > >::getHashValue(Tuple);
  }
  static bool isEqual(const NaClBitcodeReaderCast &LHS,
                      const NaClBitcodeReaderCast &RHS) {
    return LHS.Op == RHS.Op && LHS.Ty == RHS.Ty && LHS.Val == RHS.Val;
  }
};

//===----------------------------------------------------------------------===//
//                          NaClBitcodeReaderValueList Class
//===----------------------------------------------------------------------===//

class NaClBitcodeReaderValueList {
  std::vector<WeakVH> ValuePtrs;
  LLVMContext &Context;
public:
  NaClBitcodeReaderValueList(LLVMContext &C) : Context(C) {}
  ~NaClBitcodeReaderValueList() {}

  // vector compatibility methods
  unsigned size() const { return ValuePtrs.size(); }
  void resize(unsigned N) { ValuePtrs.resize(N); }
  void push_back(Value *V) {
    ValuePtrs.push_back(V);
  }

  void clear() {
    ValuePtrs.clear();
  }

  Value *operator[](unsigned i) const {
    assert(i < ValuePtrs.size());
    return ValuePtrs[i];
  }

  Value *back() const { return ValuePtrs.back(); }
    void pop_back() { ValuePtrs.pop_back(); }
  bool empty() const { return ValuePtrs.empty(); }
  void shrinkTo(unsigned N) {
    assert(N <= size() && "Invalid shrinkTo request!");
    ValuePtrs.resize(N);
  }

  // Declares the type of the forward-referenced value Idx.  Returns
  // true if an error occurred.  It is an error if Idx's type has
  // already been declared.
  bool createValueFwdRef(unsigned Idx, Type *Ty);

  // Gets the forward reference value for Idx.
  Value *getValueFwdRef(unsigned Idx);

  // Gets the corresponding constant defining the address of the
  // corresponding global variable defined by Idx, if already defined.
  // Otherwise, creates a forward reference for Idx, and returns the
  // placeholder constant for the address of the corresponding global
  // variable defined by Idx.
  Constant *getOrCreateGlobalVarRef(unsigned Idx, Module* M);

  // Assigns Idx to the given value (if new), or assigns V to Idx (if Idx
  // was forward referenced).
  void AssignValue(Value *V, unsigned Idx);

  // Assigns Idx to the given global variable.  If the Idx currently has
  // a forward reference (built by createGlobalVarFwdRef(unsigned Idx)),
  // replaces uses of the global variable forward reference with the
  // value GV.
  void AssignGlobalVar(GlobalVariable *GV, unsigned Idx);

  // Assigns Idx to the given value, overwriting the existing entry
  // and possibly modifying the type of the entry.
  void OverwriteValue(Value *V, unsigned Idx);
};


class NaClBitcodeReader : public GVMaterializer {
  NaClBitcodeHeader Header;  // Header fields of the PNaCl bitcode file.
  LLVMContext &Context;
  Module *TheModule;
  MemoryBuffer *Buffer;
  bool BufferOwned;
  OwningPtr<NaClBitstreamReader> StreamFile;
  NaClBitstreamCursor Stream;
  StreamableMemoryObject *LazyStreamer;
  uint64_t NextUnreadBit;
  bool SeenValueSymbolTable;

  std::string ErrorString;

  std::vector<Type*> TypeList;
  NaClBitcodeReaderValueList ValueList;

  // Holds information about each BasicBlock in the function being read.
  struct BasicBlockInfo {
    // A basic block within the function being modeled.
    BasicBlock *BB;
    // The set of generated conversions.
    DenseMap<NaClBitcodeReaderCast, CastInst*> CastMap;
    // The set of generated conversions that were added for phi nodes,
    // and may need thier parent basic block defined.
    std::vector<CastInst*> PhiCasts;
  };

  /// FunctionBBs - While parsing a function body, this is a list of the basic
  /// blocks for the function.
  std::vector<BasicBlockInfo> FunctionBBs;

  // When reading the module header, this list is populated with functions that
  // have bodies later in the file.
  std::vector<Function*> FunctionsWithBodies;

  // When intrinsic functions are encountered which require upgrading they are
  // stored here with their replacement function.
  typedef std::vector<std::pair<Function*, Function*> > UpgradedIntrinsicMap;
  UpgradedIntrinsicMap UpgradedIntrinsics;

  // Several operations happen after the module header has been read, but
  // before function bodies are processed.  This keeps track of whether
  // we've done this yet.
  bool SeenFirstFunctionBody;

  /// DeferredFunctionInfo - When function bodies are initially scanned, this
  /// map contains info about where to find deferred function body in the
  /// stream.
  DenseMap<Function*, uint64_t> DeferredFunctionInfo;

  /// \brief True if we should only accept supported bitcode format.
  bool AcceptSupportedBitcodeOnly;

  /// \brief Integer type use for PNaCl conversion of pointers.
  Type *IntPtrType;

public:
  explicit NaClBitcodeReader(MemoryBuffer *buffer, LLVMContext &C,
                             bool AcceptSupportedOnly = true)
    : Context(C), TheModule(0), Buffer(buffer), BufferOwned(false),
      LazyStreamer(0), NextUnreadBit(0), SeenValueSymbolTable(false),
      ValueList(C),
      SeenFirstFunctionBody(false),
      AcceptSupportedBitcodeOnly(AcceptSupportedOnly),
      IntPtrType(IntegerType::get(C, PNaClIntPtrTypeBitSize)) {
  }
  explicit NaClBitcodeReader(StreamableMemoryObject *streamer, LLVMContext &C,
                             bool AcceptSupportedOnly = true)
    : Context(C), TheModule(0), Buffer(0), BufferOwned(false),
      LazyStreamer(streamer), NextUnreadBit(0), SeenValueSymbolTable(false),
      ValueList(C),
      SeenFirstFunctionBody(false),
      AcceptSupportedBitcodeOnly(AcceptSupportedOnly),
      IntPtrType(IntegerType::get(C, PNaClIntPtrTypeBitSize)) {
  }
  ~NaClBitcodeReader() {
    FreeState();
  }

  void FreeState();

  /// setBufferOwned - If this is true, the reader will destroy the MemoryBuffer
  /// when the reader is destroyed.
  void setBufferOwned(bool Owned) { BufferOwned = Owned; }

  virtual bool isMaterializable(const GlobalValue *GV) const;
  virtual bool isDematerializable(const GlobalValue *GV) const;
  virtual error_code Materialize(GlobalValue *GV);
  virtual error_code MaterializeModule(Module *M);
  virtual void Dematerialize(GlobalValue *GV);

  bool Error(const std::string &Str) {
    ErrorString = Str;
    return true;
  }
  const std::string &getErrorString() const { return ErrorString; }

  /// @brief Main interface to parsing a bitcode buffer.
  /// @returns true if an error occurred.
  bool ParseBitcodeInto(Module *M);

private:
  // Returns false if Header is acceptable.
  bool AcceptHeader() const {
    return !(Header.IsSupported() ||
             (!AcceptSupportedBitcodeOnly && Header.IsReadable()));
  }
  uint32_t GetPNaClVersion() const {
    return Header.GetPNaClVersion();
  }
  Type *getTypeByID(unsigned ID);
  // Returns the value associated with ID.  The value must already exist,
  // or a forward referenced value created by getOrCreateFnVaueByID.
  Value *getFnValueByID(unsigned ID) {
    return ValueList.getValueFwdRef(ID);
  }
  BasicBlock *getBasicBlock(unsigned ID) const {
    if (ID >= FunctionBBs.size()) return 0; // Invalid ID
    return FunctionBBs[ID].BB;
  }

  /// \brief Read a value out of the specified record from slot '*Slot'.
  /// Increment *Slot past the number of slots used by the value in the record.
  /// Return true if there is an error.
  bool popValue(const SmallVector<uint64_t, 64> &Record, unsigned *Slot,
                unsigned InstNum, Value **ResVal) {
    if (*Slot == Record.size()) return true;
    // ValNo is encoded relative to the InstNum.
    unsigned ValNo = InstNum - (unsigned)Record[(*Slot)++];
    *ResVal = getFnValueByID(ValNo);
    return *ResVal == 0;
  }

  /// getValue -- Version of getValue that returns ResVal directly,
  /// or 0 if there is an error.
  Value *getValue(const SmallVector<uint64_t, 64> &Record, unsigned Slot,
                  unsigned InstNum) {
    if (Slot == Record.size()) return 0;
    // ValNo is encoded relative to the InstNum.
    unsigned ValNo = InstNum - (unsigned)Record[Slot];
    return getFnValueByID(ValNo);
  }

  /// getValueSigned -- Like getValue, but decodes signed VBRs.
  Value *getValueSigned(const SmallVector<uint64_t, 64> &Record, unsigned Slot,
                        unsigned InstNum) {
    if (Slot == Record.size()) return 0;
    // ValNo is encoded relative to the InstNum.
    unsigned ValNo = InstNum -
        (unsigned) NaClDecodeSignRotatedValue(Record[Slot]);
    return getFnValueByID(ValNo);
  }

  /// \brief Create an (elided) cast instruction for basic block
  /// BBIndex.  Op is the type of cast.  V is the value to cast.  CT
  /// is the type to convert V to.  DeferInsertion defines whether the
  /// generated conversion should also be installed into basic block
  /// BBIndex.  Note: For PHI nodes, we don't insert when created
  /// (i.e. DeferInsertion=true), since they must be inserted at the end
  /// of the corresponding incoming basic block.
  CastInst *CreateCast(unsigned BBIndex, Instruction::CastOps Op,
                       Type *CT, Value *V, bool DeferInsertion = false);

  /// \brief Add instructions to cast Op to the given type T into
  /// block BBIndex.  Follows rules for pointer conversion as defined
  /// in llvm/lib/Transforms/NaCl/ReplacePtrsWithInts.cpp.
  ///
  /// Returns 0 if unable to generate conversion value (also generates
  /// an appropriate error message and calls Error).
  Value *ConvertOpToType(Value *Op, Type *T, unsigned BBIndex);

  /// \brief If Op is a scalar value, this is a nop.  If Op is a
  /// pointer value, a PtrToInt instruction is inserted (in BBIndex)
  /// to convert Op to an integer.  For defaults on DeferInsertion,
  /// see comments for method CreateCast.
  Value *ConvertOpToScalar(Value *Op, unsigned BBIndex,
                           bool DeferInsertion = false);

  /// \brief Install instruction I into basic block BB.
  bool InstallInstruction(BasicBlock *BB, Instruction *I);

  FunctionType *AddPointerTypesToIntrinsicType(StringRef Name,
                                               FunctionType *FTy);
  void AddPointerTypesToIntrinsicParams();
  bool ParseModule(bool Resume);
  bool ParseTypeTable();
  bool ParseTypeTableBody();
  bool ParseGlobalVars();
  bool ParseValueSymbolTable();
  bool ParseConstants();
  bool RememberAndSkipFunctionBody();
  bool ParseFunctionBody(Function *F);
  bool GlobalCleanup();
  bool InitStream();
  bool InitStreamFromBuffer();
  bool InitLazyStream();
  bool FindFunctionInStream(Function *F,
         DenseMap<Function*, uint64_t>::iterator DeferredFunctionInfoIterator);
};

} // End llvm namespace

#endif
