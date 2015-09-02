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
#include "llvm/Analysis/NaCl/PNaClAllowedIntrinsics.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeDefs.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GVMaterializer.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/OperandTraits.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/ValueHandle.h"
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
    return DenseMapInfo<std::pair<int,
                                  std::pair<Type*,
                                            Value*> > >::getHashValue(Tuple);
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
public:
  NaClBitcodeReaderValueList() {}
  ~NaClBitcodeReaderValueList() {}

  // vector compatibility methods
  size_t size() const { return ValuePtrs.size(); }
  void resize(size_t N) { ValuePtrs.resize(N); }
  void push_back(Value *V) {
    ValuePtrs.push_back(V);
  }

  void clear() {
    ValuePtrs.clear();
  }

  Value *operator[](size_t i) const {
    assert(i < ValuePtrs.size());
    return ValuePtrs[i];
  }

  Value *back() const { return ValuePtrs.back(); }
  void pop_back() { ValuePtrs.pop_back(); }
  bool empty() const { return ValuePtrs.empty(); }
  void shrinkTo(size_t N) {
    assert(N <= size() && "Invalid shrinkTo request!");
    ValuePtrs.resize(N);
  }

  // Declares the type of the forward-referenced value Idx.  Returns
  // true if an error occurred.  It is an error if Idx's type has
  // already been declared.
  bool createValueFwdRef(NaClBcIndexSize_t Idx, Type *Ty);

  // Gets the forward reference value for Idx.
  Value *getValueFwdRef(NaClBcIndexSize_t Idx);

  // Assigns V to value index Idx.
  void AssignValue(Value *V, NaClBcIndexSize_t Idx);

  // Assigns Idx to the given value, overwriting the existing entry
  // and possibly modifying the type of the entry.
  void OverwriteValue(Value *V, NaClBcIndexSize_t Idx);
};


class NaClBitcodeReader : public GVMaterializer {
  NaClBitcodeHeader Header;  // Header fields of the PNaCl bitcode file.
  LLVMContext &Context;
  Module *TheModule;
  // If non-null, stream to write verbose errors to.
  raw_ostream *Verbose;
  PNaClAllowedIntrinsics AllowedIntrinsics;
  std::unique_ptr<MemoryBuffer> Buffer;
  std::unique_ptr<NaClBitstreamReader> StreamFile;
  NaClBitstreamCursor Stream;
  StreamingMemoryObject *LazyStreamer;
  uint64_t NextUnreadBit;
  bool SeenValueSymbolTable;
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
  // before function bodies are processed. This keeps track of whether
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

  static const std::error_category &BitcodeErrorCategory();

public:

  /// Types of errors reported.
  enum ErrorType {
    CouldNotFindFunctionInStream // Unable to find function in bitcode stream.
    = 1,                         // Note: Error types must not be zero!
    InsufficientFunctionProtos,
    InvalidBitstream,         // Error in bitstream format.
    InvalidBlock,             // Invalid block found in bitcode.
    InvalidConstantReference, // Bad constant reference.
    InvalidDataAfterModule,   // Invalid data after module.
    InvalidInstructionWithNoBB,  // No basic block for instruction.
    InvalidMultipleBlocks,    // Multiple blocks for a kind of block that should
                              // have only one.
    InvalidRecord,            // Record doesn't have expected size or structure.
    InvalidSkippedBlock,      // Unable to skip unknown block in bitcode file.
    InvalidType,              // Invalid type in record.
    InvalidTypeForValue,      // Type of value incorrect.
    InvalidValue,             // Invalid value in record.
    MalformedBlock            // Unable to advance over block.
  };

  explicit NaClBitcodeReader(MemoryBuffer *buffer, LLVMContext &C,
                             raw_ostream *Verbose,
                             bool AcceptSupportedOnly)
      : Context(C), TheModule(nullptr), Verbose(Verbose), AllowedIntrinsics(&C),
        Buffer(buffer),
        LazyStreamer(nullptr), NextUnreadBit(0), SeenValueSymbolTable(false),
        ValueList(),
        SeenFirstFunctionBody(false),
        AcceptSupportedBitcodeOnly(AcceptSupportedOnly),
        IntPtrType(IntegerType::get(C, PNaClIntPtrTypeBitSize)) {
  }
  explicit NaClBitcodeReader(StreamingMemoryObject *streamer,
                             LLVMContext &C,
                             raw_ostream *Verbose,
                             bool AcceptSupportedOnly)
      : Context(C), TheModule(nullptr), Verbose(Verbose), AllowedIntrinsics(&C),
        Buffer(nullptr),
        LazyStreamer(streamer), NextUnreadBit(0), SeenValueSymbolTable(false),
        ValueList(),
        SeenFirstFunctionBody(false),
        AcceptSupportedBitcodeOnly(AcceptSupportedOnly),
        IntPtrType(IntegerType::get(C, PNaClIntPtrTypeBitSize)) {
  }
  ~NaClBitcodeReader() override {
    FreeState();
  }

  void FreeState();

  bool isDematerializable(const GlobalValue *GV) const override;
  std::error_code materialize(GlobalValue *GV) override;
  std::error_code MaterializeModule(Module *M) override;
  std::vector<StructType *> getIdentifiedStructTypes() const override;
  void Dematerialize(GlobalValue *GV) override;
  void releaseBuffer();

  std::error_code Error(ErrorType E) const {
    return std::error_code(E, BitcodeErrorCategory());
  }

  /// Generates the corresponding verbose Message, then generates error.
  std::error_code Error(ErrorType E, const std::string &Message) const;

  /// @brief Main interface to parsing a bitcode buffer.
  /// @returns true if an error occurred.
  std::error_code ParseBitcodeInto(Module *M);

  /// Convert alignment exponent (i.e. power of two (or zero)) to the
  /// corresponding alignment to use. If alignment is too large, it generates
  /// an error message and returns corresponding error code.
  std::error_code getAlignmentValue(uint64_t Exponent, unsigned &Alignment);

  // GVMaterializer interface. It's a no-op for PNaCl bitcode, which has no
  // metadata.
  std::error_code materializeMetadata() override { return std::error_code(); };

  // GVMaterializer interface. Causes debug info to be stripped from the module
  // on materialization. It's a no-op for PNaCl bitcode, which has no metadata.
  void setStripDebugInfo() override {};

  // Returns the value associated with ID.  The value must already exist.
  Value *getFnValueByID(NaClBcIndexSize_t ID) {
    return ValueList.getValueFwdRef(ID);
  }

private:
  // Returns false if Header is acceptable.
  bool AcceptHeader() const {
    return !(Header.IsSupported() ||
             (!AcceptSupportedBitcodeOnly && Header.IsReadable()));
  }
  uint32_t GetPNaClVersion() const {
    return Header.GetPNaClVersion();
  }
  Type *getTypeByID(NaClBcIndexSize_t ID);
  BasicBlock *getBasicBlock(NaClBcIndexSize_t ID) const {
    if (ID >= FunctionBBs.size()) return 0; // Invalid ID
    return FunctionBBs[ID].BB;
  }

  /// \brief Read a value out of the specified record from slot '*Slot'.
  /// Increment *Slot past the number of slots used by the value in the record.
  /// Return true if there is an error.
  bool popValue(const SmallVector<uint64_t, 64> &Record, size_t *Slot,
                NaClBcIndexSize_t InstNum, Value **ResVal) {
    if (*Slot == Record.size()) return true;
    // ValNo is encoded relative to the InstNum.
    NaClBcIndexSize_t ValNo = InstNum -
        static_cast<NaClRelBcIndexSize_t>(Record[(*Slot)++]);
    *ResVal = getFnValueByID(ValNo);
    return *ResVal == 0;
  }

  /// getValue -- Version of getValue that returns ResVal directly,
  /// or 0 if there is an error.
  Value *getValue(const SmallVector<uint64_t, 64> &Record, size_t Slot,
                  NaClBcIndexSize_t InstNum) {
    if (Slot == Record.size()) return 0;
    // ValNo is encoded relative to the InstNum.
    NaClBcIndexSize_t ValNo = InstNum -
        static_cast<NaClRelBcIndexSize_t>(Record[Slot]);
    return getFnValueByID(ValNo);
  }

  /// getValueSigned -- Like getValue, but decodes signed VBRs.
  Value *getValueSigned(const SmallVector<uint64_t, 64> &Record, size_t Slot,
                        NaClBcIndexSize_t InstNum) {
    if (Slot == Record.size()) return 0;
    // ValNo is encoded relative to the InstNum.
    NaClBcIndexSize_t ValNo = InstNum -
        static_cast<NaClRelBcIndexSize_t>(
            NaClDecodeSignRotatedValue(Record[Slot]));
    return getFnValueByID(ValNo);
  }

  /// \brief Create an (elided) cast instruction for basic block
  /// BBIndex.  Op is the type of cast.  V is the value to cast.  CT
  /// is the type to convert V to.  DeferInsertion defines whether the
  /// generated conversion should also be installed into basic block
  /// BBIndex.  Note: For PHI nodes, we don't insert when created
  /// (i.e. DeferInsertion=true), since they must be inserted at the end
  /// of the corresponding incoming basic block.
  CastInst *CreateCast(NaClBcIndexSize_t BBIndex, Instruction::CastOps Op,
                       Type *CT, Value *V, bool DeferInsertion = false);

  /// \brief Add instructions to cast Op to the given type T into
  /// block BBIndex.  Follows rules for pointer conversion as defined
  /// in llvm/lib/Transforms/NaCl/ReplacePtrsWithInts.cpp.
  ///
  /// Returns 0 if unable to generate conversion value (also generates
  /// an appropriate error message and calls Error).
  Value *ConvertOpToType(Value *Op, Type *T, NaClBcIndexSize_t BBIndex);

  /// \brief If Op is a scalar value, this is a nop.  If Op is a
  /// pointer value, a PtrToInt instruction is inserted (in BBIndex)
  /// to convert Op to an integer.  For defaults on DeferInsertion,
  /// see comments for method CreateCast.
  Value *ConvertOpToScalar(Value *Op, NaClBcIndexSize_t BBIndex,
                           bool DeferInsertion = false);

  /// \brief Install instruction I into basic block BB.
  std::error_code InstallInstruction(BasicBlock *BB, Instruction *I);

  FunctionType *AddPointerTypesToIntrinsicType(StringRef Name,
                                               FunctionType *FTy);
  void AddPointerTypesToIntrinsicParams();
  std::error_code ParseModule(bool Resume);
  std::error_code ParseTypeTable();
  std::error_code ParseTypeTableBody();
  std::error_code ParseGlobalVars();
  std::error_code ParseValueSymbolTable();
  std::error_code ParseConstants();
  std::error_code RememberAndSkipFunctionBody();
  std::error_code ParseFunctionBody(Function *F);
  std::error_code GlobalCleanup();
  std::error_code InitStream();
  std::error_code InitStreamFromBuffer();
  std::error_code InitLazyStream();
  std::error_code FindFunctionInStream(
      Function *F,
      DenseMap<Function*, uint64_t>::iterator DeferredFunctionInfoIterator);
};

} // End llvm namespace

#endif
