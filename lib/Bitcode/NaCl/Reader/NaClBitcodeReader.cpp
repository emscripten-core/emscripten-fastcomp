//===- NaClBitcodeReader.cpp ----------------------------------------------===//
//     Internal NaClBitcodeReader implementation
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "NaClBitcodeReader"

#include "NaClBitcodeReader.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/NaCl/PNaClABITypeChecker.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeDecoders.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/IR/AutoUpgrade.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/OperandTraits.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/DataStream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


cl::opt<bool>
llvm::PNaClAllowLocalSymbolTables(
    "allow-local-symbol-tables",
    cl::desc("Allow (function) local symbol tables in PNaCl bitcode files"),
    cl::init(false));

static_assert(sizeof(NaClBcIndexSize_t) <= sizeof(size_t),
              "NaClBcIndexSize_t incorrectly defined");

void NaClBitcodeReader::FreeState() {
  std::vector<Type*>().swap(TypeList);
  ValueList.clear();

  std::vector<Function*>().swap(FunctionsWithBodies);
  DeferredFunctionInfo.clear();
}

//===----------------------------------------------------------------------===//
//  Helper functions to implement forward reference resolution, etc.
//===----------------------------------------------------------------------===//

/// ConvertToString - Convert a string from a record into an std::string, return
/// true on failure.
template<typename StrTy>
static bool ConvertToString(ArrayRef<uint64_t> Record, size_t Idx,
                            StrTy &Result) {
  if (Idx > Record.size())
    return true;

  for (size_t i = Idx, e = Record.size(); i != e; ++i)
    Result += (char)Record[i];
  return false;
}

void NaClBitcodeReaderValueList::AssignValue(Value *V, NaClBcIndexSize_t Idx) {
  assert(V);
  if (Idx == size()) {
    push_back(V);
    return;
  }

  if (Idx >= size())
    resize(Idx+1);

  WeakVH &OldV = ValuePtrs[Idx];
  if (OldV == 0) {
    OldV = V;
    return;
  }

  // If there was a forward reference to this value, replace it.
  Value *PrevVal = OldV;
  OldV->replaceAllUsesWith(V);
  delete PrevVal;
}

void NaClBitcodeReaderValueList::OverwriteValue(Value *V,
                                                NaClBcIndexSize_t Idx) {
  ValuePtrs[Idx] = V;
}

Value *NaClBitcodeReaderValueList::getValueFwdRef(NaClBcIndexSize_t Idx) {
  if (Idx >= size())
    return 0;

  if (Value *V = ValuePtrs[Idx])
    return V;

  return 0;
}

bool NaClBitcodeReaderValueList::createValueFwdRef(NaClBcIndexSize_t Idx,
                                                   Type *Ty) {
  if (Idx >= size())
    resize(Idx + 1);

  // Return an error if this a duplicate definition of Idx.
  if (ValuePtrs[Idx])
    return true;

  // No type specified, must be invalid reference.
  if (Ty == 0)
    return true;

  // Create a placeholder, which will later be RAUW'd.
  ValuePtrs[Idx] = new Argument(Ty);
  return false;
}

namespace {
class NaClBitcodeErrorCategoryType : public std::error_category {
  const char *name() const LLVM_NOEXCEPT override {
    return "pnacl.bitcode";
  }
  std::string message(int IndexError) const override {
    switch(static_cast<NaClBitcodeReader::ErrorType>(IndexError)) {
    case NaClBitcodeReader::CouldNotFindFunctionInStream:
      return "Unable to find function in bitcode stream.";
    case NaClBitcodeReader::InsufficientFunctionProtos:
      return "Insufficient function protos";
    case NaClBitcodeReader::InvalidBitstream:
      return "Error in bitstream format";
    case NaClBitcodeReader::InvalidBlock:
      return "Invalid block found in bitcode file";
    case NaClBitcodeReader::InvalidConstantReference:
      return "Bad constant reference";
    case NaClBitcodeReader::InvalidDataAfterModule:
      return "Invalid data after module";
    case NaClBitcodeReader::InvalidInstructionWithNoBB:
      return "No basic block for instruction";
    case NaClBitcodeReader::InvalidMultipleBlocks:
      return "Multiple blocks for a kind of block that should have only one";
    case NaClBitcodeReader::InvalidRecord:
      return "Record doesn't have expected size or structure";
    case NaClBitcodeReader::InvalidSkippedBlock:
      return "Unable to skip unknown block in bitcode file";
    case NaClBitcodeReader::InvalidType:
      return "Invalid type in record";
    case NaClBitcodeReader::InvalidTypeForValue:
      return "Type of value in record incorrect";
    case NaClBitcodeReader::InvalidValue:
      return "Invalid value in record";
    case NaClBitcodeReader::MalformedBlock:
      return "Malformed block. Unable to advance over block";
    }
    llvm_unreachable("Unknown error type!");
  }
};
} // end of anonymous namespace.

const std::error_category &NaClBitcodeReader::BitcodeErrorCategory() {
  static NaClBitcodeErrorCategoryType ErrCat;
  return ErrCat;
}

Type *NaClBitcodeReader::getTypeByID(NaClBcIndexSize_t ID) {
  // The type table size is always specified correctly.
  if (ID >= TypeList.size())
    return 0;

  if (Type *Ty = TypeList[ID])
    return Ty;

  // If we have a forward reference, the only possible case is when it is to a
  // named struct.  Just create a placeholder for now.
  return TypeList[ID] = StructType::create(Context);
}


//===----------------------------------------------------------------------===//
//  Functions for parsing blocks from the bitcode file
//===----------------------------------------------------------------------===//


namespace {

static const unsigned MaxAlignmentExponent = 29;
static_assert(
    (1u << MaxAlignmentExponent) == Value::MaximumAlignment,
    "Inconsistency between Value.MaxAlignment and PNaCl alignment limit");
}

std::error_code NaClBitcodeReader::Error(ErrorType E,
                                         const std::string &Message) const {
  if (Verbose) {
    naclbitc::ErrorAt(*Verbose, naclbitc::Error, Stream.GetCurrentBitNo())
        << Message << "\n";
  }
  return Error(E);
}

std::error_code NaClBitcodeReader::getAlignmentValue(
    uint64_t Exponent, unsigned &Alignment) {
  if (Exponent > MaxAlignmentExponent + 1) {
    std::string Buffer;
    raw_string_ostream StrBuf(Buffer);
    StrBuf << "Alignment can't be greater than 2**" << MaxAlignmentExponent
           << ". Found: 2**" << (Exponent - 1);
    return Error(InvalidValue, StrBuf.str());
  }
  Alignment = (1 << static_cast<unsigned>(Exponent)) >> 1;
  return std::error_code();
}

std::error_code NaClBitcodeReader::ParseTypeTable() {
  DEBUG(dbgs() << "-> ParseTypeTable\n");
  if (Stream.EnterSubBlock(naclbitc::TYPE_BLOCK_ID_NEW))
    return Error(InvalidRecord, "Malformed block record");

  std::error_code result = ParseTypeTableBody();
  if (!result)
    DEBUG(dbgs() << "<- ParseTypeTable\n");
  return result;
}

std::error_code NaClBitcodeReader::ParseTypeTableBody() {
  if (!TypeList.empty())
    return Error(InvalidMultipleBlocks, "Multiple TYPE_BLOCKs found!");

  SmallVector<uint64_t, 64> Record;
  NaClBcIndexSize_t NumRecords = 0;
  NaClBcIndexSize_t ExpectedNumRecords = 0;

  // Read all the records for this type table.
  while (1) {
    NaClBitstreamEntry Entry = Stream.advance(0, nullptr);

    switch (Entry.Kind) {
    case NaClBitstreamEntry::SubBlock:
      return Error(InvalidBlock, "Invalid block found in the types block");
    case NaClBitstreamEntry::Error:
      return Error(MalformedBlock, "Malformed types block");
    case NaClBitstreamEntry::EndBlock:
      if (NumRecords != ExpectedNumRecords)
        return Error(MalformedBlock,
                     "Invalid forward reference in the types block");
      return std::error_code();
    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a record.
    Record.clear();
    Type *ResultTy = 0;
    unsigned TypeCode = Stream.readRecord(Entry.ID, Record);
    switch (TypeCode) {
    default: {
      std::string Message;
      raw_string_ostream StrM(Message);
      StrM << "Unknown type code in type table: " << TypeCode;
      StrM.flush();
      return Error(InvalidValue, Message);
    }

    case naclbitc::TYPE_CODE_NUMENTRY: { // TYPE_CODE_NUMENTRY: [numentries]
      // TYPE_CODE_NUMENTRY contains a count of the number of types in the
      // type list.  This allows us to reserve space.
      if (Record.size() != 1)
        return Error(InvalidRecord, "Invalid TYPE_CODE_NUMENTRY record");
      uint64_t Size = Record[0];
      if (Size > NaClBcIndexSize_t_Max)
        return Error(InvalidValue, "Size too big in TYPE_CODE_NUMENTRY record");
      // The code double checks that Expected size and the actual size
      // at the end of the block. To reduce allocations we preallocate
      // the space.
      //
      // However, if the number is large, we suspect that the number
      // is (possibly) incorrect. In that case, we preallocate a
      // smaller space.
      TypeList.resize(std::min(Size, (uint64_t) 1000000));
      ExpectedNumRecords = Size;
      // No type was defined, skip the checks that follow the switch.
      continue;
    }
    case naclbitc::TYPE_CODE_VOID: // VOID
      if (Record.size() != 0)
        return Error(InvalidRecord, "Invalid TYPE_CODE_VOID record");
      ResultTy = Type::getVoidTy(Context);
      break;

    case naclbitc::TYPE_CODE_FLOAT: // FLOAT
      if (Record.size() != 0)
        return Error(InvalidRecord, "Invalid TYPE_CODE_FLOAT record");
      ResultTy = Type::getFloatTy(Context);
      break;

    case naclbitc::TYPE_CODE_DOUBLE: // DOUBLE
      if (Record.size() != 0)
        return Error(InvalidRecord, "Invalid TYPE_CODE_DOUBLE record");
      ResultTy = Type::getDoubleTy(Context);
      break;

    case naclbitc::TYPE_CODE_INTEGER: // INTEGER: [width]
      if (Record.size() != 1)
        return Error(InvalidRecord, "Invalid TYPE_CODE_INTEGER record");
      // TODO(kschimpf): Should we check if Record[0] is in range?
      ResultTy = IntegerType::get(Context, Record[0]);
      break;

    case naclbitc::TYPE_CODE_FUNCTION: {
      // FUNCTION: [vararg, retty, paramty x N]
      if (Record.size() < 2)
        return Error(InvalidRecord, "Invalid TYPE_CODE_FUNCTION record");
      SmallVector<Type *, 8> ArgTys;
      for (size_t i = 2, e = Record.size(); i != e; ++i) {
        if (Type *T = getTypeByID(Record[i]))
          ArgTys.push_back(T);
        else
          break;
      }

      ResultTy = getTypeByID(Record[1]);
      if (ResultTy == 0 || ArgTys.size() < Record.size() - 2)
        return Error(InvalidType, "invalid type in function type");

      ResultTy = FunctionType::get(ResultTy, ArgTys, Record[0]);
      break;
    }
    case naclbitc::TYPE_CODE_VECTOR: { // VECTOR: [numelts, eltty]
      if (Record.size() != 2)
        return Error(InvalidRecord, "Invalid VECTOR type record");
      if ((ResultTy = getTypeByID(Record[1])))
        ResultTy = VectorType::get(ResultTy, Record[0]);
      else
        return Error(InvalidType, "invalid type in vector type");
      break;
    }
    }

    if (NumRecords >= ExpectedNumRecords)
      return Error(MalformedBlock, "invalid TYPE table");
    assert(ResultTy && "Didn't read a type?");
    assert(TypeList[NumRecords] == 0 && "Already read type?");
    if (NumRecords == NaClBcIndexSize_t_Max)
      return Error(InvalidRecord, "Exceeded type index limit");
    TypeList[NumRecords++] = ResultTy;
  }
  return std::error_code();
}

namespace {

// Class to process globals in two passes. In the first pass, build
// the corresponding global variables with no initializers. In the
// second pass, add initializers. The purpose of putting off
// initializers is to make sure that we don't need to generate
// placeholders for relocation records, and the corresponding cost
// of duplicating initializers when these placeholders are replaced.
class ParseGlobalsHandler {
  ParseGlobalsHandler(const ParseGlobalsHandler &H) = delete;
  void operator=(const ParseGlobalsHandler &H) = delete;

  NaClBitcodeReader &Reader;
  NaClBitcodeReaderValueList &ValueList;
  NaClBitstreamCursor &Stream;
  LLVMContext &Context;
  Module *TheModule;

  // Holds read data record.
  SmallVector<uint64_t, 64> Record;
  // True when processing a global variable. Stays true until all records
  // are processed, and the global variable is created.
  bool ProcessingGlobal;
  // The number of initializers needed for the global variable.
  size_t VarInitializersNeeded;
  NaClBcIndexSize_t FirstValueNo;
  // The index of the next global variable.
  NaClBcIndexSize_t NextValueNo;
  // The number of expected global variable definitions.
  NaClBcIndexSize_t NumGlobals;
  // The bit to go back to to generate initializers.
  uint64_t StartBit;

  void InitPass() {
    Stream.JumpToBit(StartBit);
    ProcessingGlobal = false;
    VarInitializersNeeded = 0;
    NextValueNo = FirstValueNo;
  }

public:
  ParseGlobalsHandler(NaClBitcodeReader &Reader,
                      NaClBitcodeReaderValueList &ValueList,
                      NaClBitstreamCursor &Stream,
                      LLVMContext &Context,
                      Module *TheModule)
      : Reader(Reader),
        ValueList(ValueList),
        Stream(Stream),
        Context(Context),
        TheModule(TheModule),
        FirstValueNo(ValueList.size()),
        NumGlobals(0),
        StartBit(Stream.GetCurrentBitNo()) {}

  std::error_code GenerateGlobalVarsPass() {
    InitPass();

    // The type for the initializer of the global variable.
    SmallVector<Type*, 10> VarType;
    // The alignment value defined for the global variable.
    unsigned VarAlignment = 0;
    // True if the variable is read-only.
    bool VarIsConstant = false;

    // Read all records to build global variables without initializers.
    while (1) {
      NaClBitstreamEntry Entry =
        Stream.advance(NaClBitstreamCursor::AF_DontPopBlockAtEnd, nullptr);
      switch (Entry.Kind) {
      case NaClBitstreamEntry::SubBlock:
	return Reader.Error(NaClBitcodeReader::InvalidBlock,
			    "Invalid block in the global vars block");
      case NaClBitstreamEntry::Error:
        return Reader.Error(NaClBitcodeReader::MalformedBlock,
                            "Error in the global vars block");
      case NaClBitstreamEntry::EndBlock:
        if (ProcessingGlobal || NumGlobals != (NextValueNo - FirstValueNo))
          return Reader.Error(NaClBitcodeReader::MalformedBlock,
                              "Error in the global vars block");
        return std::error_code();
      case NaClBitstreamEntry::Record:
        // The interesting case.
        break;
      }

      // Read a record.
      Record.clear();
      unsigned Bitcode = Stream.readRecord(Entry.ID, Record);
      switch (Bitcode) {
      default:
        return Reader.Error(NaClBitcodeReader::InvalidValue,
                            "Unknown global variable entry");
      case naclbitc::GLOBALVAR_VAR:
        // Start the definition of a global variable.
        if (ProcessingGlobal || Record.size() != 2)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Bad GLOBALVAR_VAR record");
        ProcessingGlobal = true;
        if (std::error_code EC =
            Reader.getAlignmentValue(Record[0], VarAlignment))
          return EC;
        VarIsConstant = Record[1] != 0;
        // Assume (by default) there is a single initializer.
        VarInitializersNeeded = 1;
        break;
      case naclbitc::GLOBALVAR_COMPOUND: {
        // Global variable has multiple initializers. Changes the
        // default number of initializers to the given value in
        // Record[0].
        if (!ProcessingGlobal || !VarType.empty() ||
            VarInitializersNeeded != 1 || Record.size() != 1)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Bad GLOBALVAR_COMPOUND record");
        uint64_t Size = Record[0];
        // Check that ILP32 assumption met.
        if (Size > MaxNaClGlobalVarInits)
          return Reader.Error(NaClBitcodeReader::InvalidValue,
                              "Size too big in GLOBALVAR_COMPOUND record");
        VarInitializersNeeded = static_cast<size_t>(Size);
        break;
      }
      case naclbitc::GLOBALVAR_ZEROFILL: {
        // Define a type that defines a sequence of zero-filled bytes.
        if (!ProcessingGlobal || Record.size() != 1)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Bad GLOBALVAR_ZEROFILL record");
        VarType.push_back(ArrayType::get(
            Type::getInt8Ty(Context), Record[0]));
        break;
      }
      case naclbitc::GLOBALVAR_DATA: {
        // Defines a type defined by a sequence of byte values.
        if (!ProcessingGlobal || Record.size() < 1)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Bad GLOBALVAR_DATA record");
        VarType.push_back(ArrayType::get(
            Type::getInt8Ty(Context), Record.size()));
        break;
      }
      case naclbitc::GLOBALVAR_RELOC: {
        // Define a relocation initializer type.
        if (!ProcessingGlobal || Record.size() < 1 || Record.size() > 2)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Bad GLOBALVAR_RELOC record");
        VarType.push_back(IntegerType::get(Context, 32));
        break;
      }
      case naclbitc::GLOBALVAR_COUNT:
        if (Record.size() != 1 || NumGlobals != 0)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Invalid global count record");
        if (Record[0] > NaClBcIndexSize_t_Max)
          return Reader.Error(NaClBitcodeReader::InvalidValue,
                              "Size too big in global count record");
        NumGlobals = static_cast<NaClBcIndexSize_t>(Record[0]);
        break;
      }

      // If more initializers needed for global variable, continue processing.
      if (!ProcessingGlobal || VarType.size() < VarInitializersNeeded)
        continue;

      Type *Ty = 0;
      switch (VarType.size()) {
      case 0:
        return Reader.Error(
            NaClBitcodeReader::InvalidRecord,
            "No initializer for global variable in global vars block");
      case 1:
        Ty = VarType[0];
        break;
      default:
        Ty = StructType::get(Context, VarType, true);
        break;
      }
      GlobalVariable *GV = new GlobalVariable(
          *TheModule, Ty, VarIsConstant,
          GlobalValue::InternalLinkage, NULL, "");
      GV->setAlignment(VarAlignment);
      ValueList.AssignValue(GV, NextValueNo);
      ++NextValueNo;
      ProcessingGlobal = false;
      VarAlignment = 0;
      VarIsConstant = false;
      VarInitializersNeeded = 0;
      VarType.clear();
    }
    return std::error_code();
  }

  std::error_code GenerateGlobalVarInitsPass() {
    InitPass();
    // The initializer for the global variable.
    SmallVector<Constant *, 10> VarInit;

    while (1) {
      NaClBitstreamEntry Entry =
        Stream.advance(NaClBitstreamCursor::AF_DontAutoprocessAbbrevs, nullptr);
      switch (Entry.Kind) {
      case NaClBitstreamEntry::SubBlock:
	return Reader.Error(NaClBitcodeReader::InvalidBlock,
			    "Invalid block in the global vars block");
      case NaClBitstreamEntry::Error:
        return Reader.Error(NaClBitcodeReader::MalformedBlock,
                            "Error in the global vars block");
      case NaClBitstreamEntry::EndBlock:
        if (ProcessingGlobal || NumGlobals != (NextValueNo - FirstValueNo))
          return Reader.Error(NaClBitcodeReader::MalformedBlock,
                              "Error in the global vars block");
        return std::error_code();
      case NaClBitstreamEntry::Record:
        if (Entry.ID == naclbitc::DEFINE_ABBREV) {
          Stream.SkipAbbrevRecord();
          continue;
        }
        // The interesting case.
        break;
      }

      // Read a record.
      Record.clear();
      unsigned Bitcode = Stream.readRecord(Entry.ID, Record);
      switch (Bitcode) {
      default:
        return Reader.Error(NaClBitcodeReader::InvalidValue,
                            "Unknown global variable entry 2");
      case naclbitc::GLOBALVAR_VAR:
        // Start the definition of a global variable.
        ProcessingGlobal = true;
        // Assume (by default) there is a single initializer.
        VarInitializersNeeded = 1;
        break;
      case naclbitc::GLOBALVAR_COMPOUND:
        // Global variable has multiple initializers. Changes the
        // default number of initializers to the given value in
        // Record[0].
        if (!ProcessingGlobal || !VarInit.empty() ||
            VarInitializersNeeded != 1 || Record.size() != 1)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Bad GLOBALVAR_COMPOUND record");
        // Note: We assume VarInitializersNeeded size was checked
        // in GenerateGlobalVarsPass.
        VarInitializersNeeded = static_cast<size_t>(Record[0]);
        break;
      case naclbitc::GLOBALVAR_ZEROFILL: {
        // Define an initializer that defines a sequence of zero-filled bytes.
        if (!ProcessingGlobal || Record.size() != 1)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Bad GLOBALVAR_ZEROFILL record");
        Type *Ty = ArrayType::get(Type::getInt8Ty(Context),
                                  Record[0]);
        Constant *Zero = ConstantAggregateZero::get(Ty);
        VarInit.push_back(Zero);
        break;
      }
      case naclbitc::GLOBALVAR_DATA: {
        // Defines an initializer defined by a sequence of byte values.
        if (!ProcessingGlobal || Record.size() < 1)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Bad GLOBALVAR_DATA record");
        size_t Size = Record.size();
        uint8_t *Buf = new uint8_t[Size];
        assert(Buf);
        for (size_t i = 0; i < Size; ++i)
          Buf[i] = Record[i];
        Constant *Init = ConstantDataArray::get(
            Context, ArrayRef<uint8_t>(Buf, Buf + Size));
        VarInit.push_back(Init);
        delete[] Buf;
        break;
      }
      case naclbitc::GLOBALVAR_RELOC: {
        // Define a relocation initializer.
        if (!ProcessingGlobal || Record.size() < 1 || Record.size() > 2)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Bad GLOBALVAR_RELOC record");
        Constant *BaseVal = cast<Constant>(Reader.getFnValueByID(Record[0]));
        Type *IntPtrType = IntegerType::get(Context, 32);
        Constant *Val = ConstantExpr::getPtrToInt(BaseVal, IntPtrType);
        if (Record.size() == 2) {
          uint64_t Addend = Record[1];
          // Note: PNaCl is ILP32, so Addend must be uint32_t.
          if (Addend > std::numeric_limits<uint32_t>::max())
            return Reader.Error(NaClBitcodeReader::InvalidValue,
                                "Addend of GLOBALVAR_RELOC record too big");
          Val = ConstantExpr::getAdd(Val, ConstantInt::get(IntPtrType,
                                                           Addend));
        }
        VarInit.push_back(Val);
        break;
      }
      case naclbitc::GLOBALVAR_COUNT:
        if (Record.size() != 1)
          return Reader.Error(NaClBitcodeReader::InvalidRecord,
                              "Invalid global count record");
        // Note: NumGlobals should have been set in GenerateGlobalVarsPass.
        // Fail if methods are called in wrong order.
        assert(NumGlobals == Record[0]);
        break;
      }

      // If more initializers needed for global variable, continue processing.
      if (!ProcessingGlobal || VarInit.size() < VarInitializersNeeded)
        continue;

      Constant *Init = 0;
      switch (VarInit.size()) {
      case 0:
        return Reader.Error(NaClBitcodeReader::InvalidRecord,
            "No initializer for global variable in global vars block");
      case 1:
        Init = VarInit[0];
        break;
      default:
        Init = ConstantStruct::getAnon(Context, VarInit, true);
        break;
      }
      cast<GlobalVariable>(ValueList[NextValueNo])->setInitializer(Init);
      if (NextValueNo == NaClBcIndexSize_t_Max)
        return Reader.Error(NaClBitcodeReader::InvalidRecord,
                            "Exceeded value index limit");
      ++NextValueNo;
      ProcessingGlobal = false;
      VarInitializersNeeded = 0;
      VarInit.clear();
    }
    return std::error_code();
  }
};

} // End anonymous namespace.

std::error_code NaClBitcodeReader::ParseGlobalVars() {
  if (Stream.EnterSubBlock(naclbitc::GLOBALVAR_BLOCK_ID))
    return Error(InvalidRecord, "Malformed block record");

  ParseGlobalsHandler PassHandler(*this, ValueList, Stream, Context, TheModule);
  if (std::error_code EC = PassHandler.GenerateGlobalVarsPass())
    return EC;
  return PassHandler.GenerateGlobalVarInitsPass();
}

std::error_code NaClBitcodeReader::ParseValueSymbolTable() {
  DEBUG(dbgs() << "-> ParseValueSymbolTable\n");
  if (Stream.EnterSubBlock(naclbitc::VALUE_SYMTAB_BLOCK_ID))
    return Error(InvalidRecord, "Malformed block record");

  SmallVector<uint64_t, 64> Record;

  // Read all the records for this value table.
  SmallString<128> ValueName;
  while (1) {
    NaClBitstreamEntry Entry = Stream.advance(0, nullptr);

    switch (Entry.Kind) {
    case NaClBitstreamEntry::SubBlock:
      return Error(InvalidBlock,
                   "Invalid block in the value symbol table block");
    case NaClBitstreamEntry::Error:
      return Error(MalformedBlock, "malformed value symbol table block");
    case NaClBitstreamEntry::EndBlock:
      DEBUG(dbgs() << "<- ParseValueSymbolTable\n");
      return std::error_code();
    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a record.
    Record.clear();
    switch (Stream.readRecord(Entry.ID, Record)) {
    default:  // Default behavior: unknown type.
      break;
    case naclbitc::VST_CODE_ENTRY: {  // VST_ENTRY: [valueid, namechar x N]
      if (ConvertToString(Record, 1, ValueName))
        return Error(InvalidRecord, "Invalid VST_ENTRY record");
      Value *V = getFnValueByID(Record[0]);
      if (V == nullptr)
        return Error(InvalidValue, "Invalid Value ID in VST_ENTRY record");

      V->setName(StringRef(ValueName.data(), ValueName.size()));
      ValueName.clear();
      break;
    }
    case naclbitc::VST_CODE_BBENTRY: {
      if (ConvertToString(Record, 1, ValueName))
        return Error(InvalidRecord, "Invalid VST_BBENTRY record");
      BasicBlock *BB = getBasicBlock(Record[0]);
      if (BB == 0)
        return Error(InvalidValue, "Invalid BB ID in VST_BBENTRY record");

      BB->setName(StringRef(ValueName.data(), ValueName.size()));
      ValueName.clear();
      break;
    }
    }
  }
}

std::error_code NaClBitcodeReader::ParseConstants() {
  DEBUG(dbgs() << "-> ParseConstants\n");
  if (Stream.EnterSubBlock(naclbitc::CONSTANTS_BLOCK_ID))
    return Error(InvalidRecord, "Malformed block record");

  SmallVector<uint64_t, 64> Record;

  // Read all the records for this value table.
  Type *CurTy = Type::getInt32Ty(Context);
  NaClBcIndexSize_t NextCstNo = ValueList.size();
  while (1) {
    NaClBitstreamEntry Entry = Stream.advance(0, nullptr);

    switch (Entry.Kind) {
    case NaClBitstreamEntry::SubBlock:
      return Error(InvalidBlock, "Invalid block in function constants block");
    case NaClBitstreamEntry::Error:
      return Error(MalformedBlock, "malformed function constants block");
    case NaClBitstreamEntry::EndBlock:
      if (NextCstNo != ValueList.size())
        return Error(InvalidConstantReference,
                     "Invalid constant reference!");
      DEBUG(dbgs() << "<- ParseConstants\n");
      return std::error_code();
    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a record.
    Record.clear();
    Value *V = 0;
    unsigned BitCode = Stream.readRecord(Entry.ID, Record);
    switch (BitCode) {
    default: {
      std::string Message;
      raw_string_ostream StrM(Message);
      StrM << "Invalid Constant code: " << BitCode;
      StrM.flush();
      return Error(InvalidValue, Message);
    }
    case naclbitc::CST_CODE_UNDEF:     // UNDEF
      V = UndefValue::get(CurTy);
      break;
    case naclbitc::CST_CODE_SETTYPE:   // SETTYPE: [typeid]
      if (Record.empty())
        return Error(NaClBitcodeReader::InvalidRecord,
                     "Malformed CST_SETTYPE record");
      if (Record[0] >= TypeList.size())
        return Error(NaClBitcodeReader::InvalidType,
                     "Invalid Type ID in CST_SETTYPE record");
      CurTy = TypeList[Record[0]];
      continue;  // Skip the ValueList manipulation.
    case naclbitc::CST_CODE_INTEGER:   // INTEGER: [intval]
      if (!CurTy->isIntegerTy() || Record.empty())
        return Error(InvalidRecord, "Invalid CST_INTEGER record");
      V = ConstantInt::get(CurTy, NaClDecodeSignRotatedValue(Record[0]));
      break;
    case naclbitc::CST_CODE_FLOAT: {    // FLOAT: [fpval]
      if (Record.empty())
        return Error(NaClBitcodeReader::InvalidRecord, "Invalid FLOAT record");
      if (CurTy->isFloatTy())
        V = ConstantFP::get(Context, APFloat(APFloat::IEEEsingle,
                                             APInt(32, (uint32_t)Record[0])));
      else if (CurTy->isDoubleTy())
        V = ConstantFP::get(Context, APFloat(APFloat::IEEEdouble,
                                             APInt(64, Record[0])));
      else
        return Error(NaClBitcodeReader::InvalidRecord,
                     "Unknown type for FLOAT record");
      break;
    }
    }

    ValueList.AssignValue(V, NextCstNo);
    ++NextCstNo;
  }
  return std::error_code();
}

/// RememberAndSkipFunctionBody - When we see the block for a function body,
/// remember where it is and then skip it.  This lets us lazily deserialize the
/// functions.
std::error_code NaClBitcodeReader::RememberAndSkipFunctionBody() {
  DEBUG(dbgs() << "-> RememberAndSkipFunctionBody\n");
  // Get the function we are talking about.
  if (FunctionsWithBodies.empty())
    return Error(InsufficientFunctionProtos,
                 "Insufficient function protos");

  Function *Fn = FunctionsWithBodies.back();
  FunctionsWithBodies.pop_back();

  // Save the current stream state.
  uint64_t CurBit = Stream.GetCurrentBitNo();
  DeferredFunctionInfo[Fn] = CurBit;

  // Skip over the function block for now.
  if (Stream.SkipBlock())
    return Error(InvalidSkippedBlock, "Unable to skip function block.");
  DEBUG(dbgs() << "<- RememberAndSkipFunctionBody\n");
  return std::error_code();
}

std::error_code NaClBitcodeReader::GlobalCleanup() {
  // Look for intrinsic functions which need to be upgraded at some point
  for (Module::iterator FI = TheModule->begin(), FE = TheModule->end();
       FI != FE; ++FI) {
    Function *NewFn;
    if (UpgradeIntrinsicFunction(FI, NewFn))
      UpgradedIntrinsics.push_back(std::make_pair(FI, NewFn));
  }

  // Look for global variables which need to be renamed.
  for (Module::global_iterator
         GI = TheModule->global_begin(), GE = TheModule->global_end();
       GI != GE; ++GI)
    UpgradeGlobalVariable(GI);
  return std::error_code();
}

FunctionType *NaClBitcodeReader::AddPointerTypesToIntrinsicType(
    StringRef Name, FunctionType *FTy) {
  FunctionType *IntrinsicTy = AllowedIntrinsics.getIntrinsicType(Name);
  if (IntrinsicTy == 0) return FTy;

  Type *IReturnTy = IntrinsicTy->getReturnType();
  Type *FReturnTy = FTy->getReturnType();

  if (!PNaClABITypeChecker::IsPointerEquivType(IReturnTy, FReturnTy)) {
    std::string Buffer;
    raw_string_ostream StrBuf(Buffer);
    StrBuf << "Intrinsic return type mismatch for " << Name << ": "
           << *IReturnTy << " and " << *FReturnTy;
    report_fatal_error(StrBuf.str());
  }
  if (FTy->getNumParams() != IntrinsicTy->getNumParams()) {
    std::string Buffer;
    raw_string_ostream StrBuf(Buffer);
    StrBuf << "Intrinsic type mistmatch for " << Name << ": "
           << *FTy << " and " << *IntrinsicTy;
    report_fatal_error(StrBuf.str());
  }
  for (size_t i = 0; i < FTy->getNumParams(); ++i) {
    Type *IargTy = IntrinsicTy->getParamType(i);
    Type *FargTy = FTy->getParamType(i);
    if (!PNaClABITypeChecker::IsPointerEquivType(IargTy, FargTy)) {
      std::string Buffer;
      raw_string_ostream StrBuf(Buffer);
      StrBuf << "Intrinsic type mismatch for argument " << i << " in "
             << Name << ": " << *IargTy << " and " << *FargTy;
      report_fatal_error(StrBuf.str());
    }
  }
  return IntrinsicTy;
}

void NaClBitcodeReader::AddPointerTypesToIntrinsicParams() {
  for (size_t Index = 0, E = ValueList.size(); Index < E; ++Index) {
    if (Function *Func = dyn_cast<Function>(ValueList[Index])) {
      if (Func->isIntrinsic()) {
        FunctionType *FTy = Func->getFunctionType();
        FunctionType *ITy = AddPointerTypesToIntrinsicType(
            Func->getName(), FTy);
        if (ITy == FTy) continue;
        Function *NewIntrinsic = Function::Create(
            ITy, GlobalValue::ExternalLinkage, "", TheModule);
        NewIntrinsic->takeName(Func);
        ValueList.OverwriteValue(NewIntrinsic, Index);
        Func->eraseFromParent();
      }
    }
  }
}

std::error_code NaClBitcodeReader::ParseModule(bool Resume) {
  DEBUG(dbgs() << "-> ParseModule\n");
  if (Resume)
    Stream.JumpToBit(NextUnreadBit);
  else if (Stream.EnterSubBlock(naclbitc::MODULE_BLOCK_ID))
    return Error(InvalidRecord, "Malformed block record");

  SmallVector<uint64_t, 64> Record;

  // Read all the records for this module.
  while (1) {
    NaClBitstreamEntry Entry = Stream.advance(0, nullptr);

    switch (Entry.Kind) {
    case NaClBitstreamEntry::Error:
      return Error(MalformedBlock, "malformed module block");
    case NaClBitstreamEntry::EndBlock:
      DEBUG(dbgs() << "<- ParseModule\n");
      if (std::error_code EC = GlobalCleanup())
        return EC;
      if (!Stream.AtEndOfStream())
        return Error(InvalidDataAfterModule, "Invalid data after module");
      return std::error_code();
    case NaClBitstreamEntry::SubBlock:
      switch (Entry.ID) {
      default: {
        std::string Message;
        raw_string_ostream StrM(Message);
        StrM << "Unknown block ID: " << Entry.ID;
        return Error(InvalidRecord, StrM.str());
      }
      case naclbitc::BLOCKINFO_BLOCK_ID:
        if (Stream.ReadBlockInfoBlock(0))
          return Error(MalformedBlock, "Malformed BlockInfoBlock");
        break;
      case naclbitc::TYPE_BLOCK_ID_NEW:
        if (std::error_code EC = ParseTypeTable())
          return EC;
        break;
      case naclbitc::GLOBALVAR_BLOCK_ID:
        if (std::error_code EC = ParseGlobalVars())
          return EC;
        break;
      case naclbitc::VALUE_SYMTAB_BLOCK_ID:
        if (std::error_code EC = ParseValueSymbolTable())
          return EC;
        SeenValueSymbolTable = true;
        // Now that we know the names of the intrinsics, we can add
        // pointer types to the intrinsic declarations' types.
        AddPointerTypesToIntrinsicParams();
        break;
      case naclbitc::FUNCTION_BLOCK_ID:
        // If this is the first function body we've seen, reverse the
        // FunctionsWithBodies list.
        if (!SeenFirstFunctionBody) {
          std::reverse(FunctionsWithBodies.begin(), FunctionsWithBodies.end());
          if (std::error_code EC = GlobalCleanup())
            return EC;
          SeenFirstFunctionBody = true;
        }

        if (std::error_code EC = RememberAndSkipFunctionBody())
          return EC;

        // For streaming bitcode, suspend parsing when we reach the function
        // bodies. Subsequent materialization calls will resume it when
        // necessary. For streaming, the function bodies must be at the end of
        // the bitcode. If the bitcode file is old, the symbol table will be
        // at the end instead and will not have been seen yet. In this case,
        // just finish the parse now.
        if (LazyStreamer && SeenValueSymbolTable) {
          NextUnreadBit = Stream.GetCurrentBitNo();
          DEBUG(dbgs() << "<- ParseModule\n");
          return std::error_code();
        }
        break;
      }
      continue;

    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a record.
    unsigned Selector = Stream.readRecord(Entry.ID, Record);
    switch (Selector) {
    default: {
      std::string Message;
      raw_string_ostream StrM(Message);
      StrM << "Invalid MODULE_CODE: " << Selector;
      StrM.flush();
      return Error(InvalidValue, Message);
    }
    case naclbitc::MODULE_CODE_VERSION: {  // VERSION: [version#]
      if (Record.size() < 1)
        return Error(InvalidRecord, "Malformed MODULE_CODE_VERSION");
      // Only version #1 is supported for PNaCl. Version #0 is not supported.
      uint64_t module_version = Record[0];
      if (module_version != 1)
        return Error(InvalidValue, "Unknown bitstream version!");
      break;
    }
    // FUNCTION:  [type, callingconv, isproto, linkage]
    case naclbitc::MODULE_CODE_FUNCTION: {
      if (Record.size() < 4)
        return Error(InvalidRecord, "Invalid MODULE_CODE_FUNCTION record");
      Type *Ty = getTypeByID(Record[0]);
      if (!Ty)
        return Error(InvalidType, "Invalid MODULE_CODE_FUNCTION record");
      FunctionType *FTy = dyn_cast<FunctionType>(Ty);
      if (!FTy)
        return Error(InvalidType,
                     "Function not declared with a function type!");

      Function *Func = Function::Create(FTy, GlobalValue::ExternalLinkage,
                                        "", TheModule);

      CallingConv::ID CallingConv;
      if (!naclbitc::DecodeCallingConv(Record[1], CallingConv))
        return Error(InvalidValue,
                     "PNaCl bitcode contains invalid calling conventions.");
      Func->setCallingConv(CallingConv);
      bool isProto = Record[2];
      GlobalValue::LinkageTypes Linkage;
      if (!naclbitc::DecodeLinkage(Record[3], Linkage))
        return Error(InvalidValue, "Unknown linkage type");
      Func->setLinkage(Linkage);
      ValueList.push_back(Func);

      // If this is a function with a body, remember the prototype we are
      // creating now, so that we can match up the body with them later.
      if (!isProto) {
        Func->setIsMaterializable(true);
        FunctionsWithBodies.push_back(Func);
        if (LazyStreamer) DeferredFunctionInfo[Func] = 0;
      }
      break;
    }
    }
    Record.clear();
  }
  return std::error_code();
}

const char *llvm::PNaClDataLayout =
    "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-"
    "f32:32:32-f64:64:64-p:32:32:32-v128:32:32";

std::error_code NaClBitcodeReader::ParseBitcodeInto(Module *M) {
  TheModule = 0;

  // PNaCl does not support different DataLayouts in pexes, so we
  // implicitly set the DataLayout to the following default.
  //
  // This is not usually needed by the backend, but it might be used
  // by IR passes that the PNaCl translator runs.  We set this in the
  // reader rather than in pnacl-llc so that 'opt' will also use the
  // correct DataLayout if it is run on a pexe.
  M->setDataLayout(PNaClDataLayout);

  if (std::error_code EC = InitStream())
    return EC;

  // We expect a number of well-defined blocks, though we don't necessarily
  // need to understand them all.
  while (1) {
    if (Stream.AtEndOfStream())
      return std::error_code();

    NaClBitstreamEntry Entry =
        Stream.advance(NaClBitstreamCursor::AF_DontAutoprocessAbbrevs, nullptr);

    switch (Entry.Kind) {
    case NaClBitstreamEntry::Error:
      return Error(MalformedBlock, "malformed module file");
    case NaClBitstreamEntry::EndBlock:
      return std::error_code();

    case NaClBitstreamEntry::SubBlock:
      switch (Entry.ID) {
      case naclbitc::MODULE_BLOCK_ID:
        // Reject multiple MODULE_BLOCK's in a single bitstream.
        if (TheModule)
          return Error(InvalidMultipleBlocks,
                       "Multiple MODULE_BLOCKs in same stream");
        TheModule = M;
        if (std::error_code EC = ParseModule(false))
          return EC;
        if (LazyStreamer)
          return std::error_code();
        break;
      default:
        return Error(InvalidBlock, "Invalid top-level block found.");
        break;
      }
      continue;
    case NaClBitstreamEntry::Record:
      // There should be no records in the top-level of blocks.
      return Error(InvalidRecord, "Invalid record at top-level");
    }
  }
}

// Returns true if error occured installing I into BB.
std::error_code NaClBitcodeReader::InstallInstruction(
    BasicBlock *BB, Instruction *I) {
  // Add instruction to end of current BB.  If there is no current BB, reject
  // this file.
  if (BB == 0) {
    delete I;
    return Error(InvalidInstructionWithNoBB,
                 "Instruction with no BB, can't install");
  }
  BB->getInstList().push_back(I);
  return std::error_code();
}

CastInst *
NaClBitcodeReader::CreateCast(NaClBcIndexSize_t BBIndex,
                              Instruction::CastOps Op,
                              Type *CT, Value *V, bool DeferInsertion) {
  if (BBIndex >= FunctionBBs.size())
    report_fatal_error("CreateCast on unknown basic block");
  BasicBlockInfo &BBInfo = FunctionBBs[BBIndex];
  NaClBitcodeReaderCast ModeledCast(Op, CT, V);
  CastInst *Cast = BBInfo.CastMap[ModeledCast];
  if (Cast == NULL) {
    Cast = CastInst::Create(Op, V, CT);
    BBInfo.CastMap[ModeledCast] = Cast;
    if (DeferInsertion) {
      BBInfo.PhiCasts.push_back(Cast);
    }
  }
  if (!DeferInsertion && Cast->getParent() == 0) {
    InstallInstruction(BBInfo.BB, Cast);
  }
  return Cast;
}

Value *NaClBitcodeReader::ConvertOpToScalar(Value *Op,
                                            NaClBcIndexSize_t BBIndex,
                                            bool DeferInsertion) {
  if (Op->getType()->isPointerTy()) {
    return CreateCast(BBIndex, Instruction::PtrToInt, IntPtrType, Op,
                      DeferInsertion);
  }
  return Op;
}

Value *NaClBitcodeReader::ConvertOpToType(Value *Op, Type *T,
                                          NaClBcIndexSize_t BBIndex) {
  Type *OpTy = Op->getType();
  if (OpTy == T) return Op;

  if (OpTy->isPointerTy()) {
    if (T == IntPtrType) {
      return ConvertOpToScalar(Op, BBIndex);
    } else {
      return CreateCast(BBIndex, Instruction::BitCast, T, Op);
    }
  } else if (OpTy == IntPtrType) {
    return CreateCast(BBIndex, Instruction::IntToPtr, T, Op);
  }

  std::string Message;
  raw_string_ostream StrM(Message);
  StrM << "Can't convert " << *Op << " to type " << *T << "\n";
  report_fatal_error(StrM.str());
}

/// ParseFunctionBody - Lazily parse the specified function body block.
std::error_code NaClBitcodeReader::ParseFunctionBody(Function *F) {
  DEBUG(dbgs() << "-> ParseFunctionBody\n");
  if (Stream.EnterSubBlock(naclbitc::FUNCTION_BLOCK_ID))
    return Error(InvalidRecord, "Malformed block record");

  NaClBcIndexSize_t ModuleValueListSize = ValueList.size();

  // Add all the function arguments to the value table.
  for(Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E; ++I)
    ValueList.push_back(I);

  NaClBcIndexSize_t NextValueNo = ValueList.size();
  BasicBlock *CurBB = 0;
  NaClBcIndexSize_t CurBBNo = 0;

  // Read all the records.
  SmallVector<uint64_t, 64> Record;
  while (1) {
    NaClBitstreamEntry Entry = Stream.advance(0, nullptr);

    switch (Entry.Kind) {
    case NaClBitstreamEntry::Error:
      return Error(MalformedBlock, "Bitcode error in function block");
    case NaClBitstreamEntry::EndBlock:
      goto OutOfRecordLoop;

    case NaClBitstreamEntry::SubBlock:
      switch (Entry.ID) {
      default:
        return Error(InvalidBlock, "Invalid block in function block");
        break;
      case naclbitc::CONSTANTS_BLOCK_ID:
        if (std::error_code EC = ParseConstants())
          return EC;
        NextValueNo = ValueList.size();
        break;
      case naclbitc::VALUE_SYMTAB_BLOCK_ID:
        if (PNaClAllowLocalSymbolTables) {
          if (std::error_code EC = ParseValueSymbolTable())
            return EC;
        } else {
          return Error(InvalidRecord, "Local value symbol tables not allowed");
        }
        break;
      }
      continue;

    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a record.
    Record.clear();
    Instruction *I = 0;
    unsigned BitCode = Stream.readRecord(Entry.ID, Record);
    switch (BitCode) {
    default: {// Default behavior: reject
      std::string Message;
      raw_string_ostream StrM(Message);
      StrM << "Unknown instruction record: <" << BitCode;
      for (size_t I = 0, E = Record.size(); I != E; ++I) {
        StrM << " " << Record[I];
      }
      StrM << ">";
      return Error(InvalidRecord, StrM.str());
    }

    case naclbitc::FUNC_CODE_DECLAREBLOCKS:     // DECLAREBLOCKS: [nblocks]
      if (Record.size() != 1 || Record[0] == 0)
        return Error(InvalidRecord, "Invalid DECLAREBLOCKS record");
      // Create all the basic blocks for the function.
      // TODO(kschimpf): Figure out how to handle size values that
      // are too large.
      FunctionBBs.resize(Record[0]);
      for (size_t i = 0, e = FunctionBBs.size(); i != e; ++i) {
        BasicBlockInfo &BBInfo = FunctionBBs[i];
        BBInfo.BB = BasicBlock::Create(Context, "", F);
      }
      CurBB = FunctionBBs.at(0).BB;
      continue;

    case naclbitc::FUNC_CODE_INST_BINOP: {
      // BINOP: [opval, opval, opcode[, flags]]
      // Note: Only old PNaCl bitcode files may contain flags. If
      // they are found, we ignore them.
      size_t OpNum = 0;
      Value *LHS, *RHS;
      if (popValue(Record, &OpNum, NextValueNo, &LHS) ||
          popValue(Record, &OpNum, NextValueNo, &RHS) ||
          OpNum+1 > Record.size())
        return Error(InvalidRecord, "Invalid BINOP record");

      LHS = ConvertOpToScalar(LHS, CurBBNo);
      RHS = ConvertOpToScalar(RHS, CurBBNo);

      Instruction::BinaryOps Opc;
      if (!naclbitc::DecodeBinaryOpcode(Record[OpNum++], LHS->getType(), Opc))
        return Error(InvalidValue, "Invalid binary opcode in BINOP record");
      I = BinaryOperator::Create(Opc, LHS, RHS);
      break;
    }
    case naclbitc::FUNC_CODE_INST_CAST: {    // CAST: [opval, destty, castopc]
      size_t OpNum = 0;
      Value *Op;
      if (popValue(Record, &OpNum, NextValueNo, &Op) ||
          OpNum+2 != Record.size())
        return Error(InvalidRecord, "Invalid CAST record: bad record size");

      Type *ResTy = getTypeByID(Record[OpNum]);
      if (ResTy == 0)
        return Error(InvalidType, "Invalid CAST record: bad type ID");
      Instruction::CastOps Opc;
      if (!naclbitc::DecodeCastOpcode(Record[OpNum+1], Opc)) {
        return Error(InvalidValue, "Invalid CAST record: bad opcode");
      }

      // If a ptrtoint cast was elided on the argument of the cast,
      // add it back. Note: The casts allowed here should match the
      // casts in NaClValueEnumerator::ExpectsScalarValue.
      switch (Opc) {
      case Instruction::Trunc:
      case Instruction::ZExt:
      case Instruction::SExt:
      case Instruction::UIToFP:
      case Instruction::SIToFP:
        Op = ConvertOpToScalar(Op, CurBBNo);
        break;
      default:
        break;
      }

      I = CastInst::Create(Opc, Op, ResTy);
      break;
    }

    case naclbitc::FUNC_CODE_INST_VSELECT: {// VSELECT: [opval, opval, pred]
      // new form of select
      // handles select i1 or select [N x i1]
      size_t OpNum = 0;
      Value *TrueVal, *FalseVal, *Cond;
      if (popValue(Record, &OpNum, NextValueNo, &TrueVal) ||
          popValue(Record, &OpNum, NextValueNo, &FalseVal) ||
          popValue(Record, &OpNum, NextValueNo, &Cond) ||
          OpNum != Record.size())
        return Error(InvalidRecord, "Invalid SELECT record");

      TrueVal = ConvertOpToScalar(TrueVal, CurBBNo);
      FalseVal = ConvertOpToScalar(FalseVal, CurBBNo);

      // select condition can be either i1 or [N x i1]
      if (VectorType* vector_type =
          dyn_cast<VectorType>(Cond->getType())) {
        // expect <n x i1>
        if (vector_type->getElementType() != Type::getInt1Ty(Context))
          return Error(InvalidTypeForValue,
                       "Invalid SELECT vector condition type");
      } else {
        // expect i1
        if (Cond->getType() != Type::getInt1Ty(Context))
          return Error(InvalidTypeForValue, "Invalid SELECT condition type");
      }

      I = SelectInst::Create(Cond, TrueVal, FalseVal);
      break;
    }

    case naclbitc::FUNC_CODE_INST_EXTRACTELT: { // EXTRACTELT: [opval, opval]
      size_t OpNum = 0;
      Value *Vec, *Idx;
      if (popValue(Record, &OpNum, NextValueNo, &Vec) ||
          popValue(Record, &OpNum, NextValueNo, &Idx) || OpNum != Record.size())
        return Error(InvalidRecord, "Invalid EXTRACTELEMENT record");

      // expect i32
      if (Idx->getType() != Type::getInt32Ty(Context))
        return Error(InvalidTypeForValue, "Invalid EXTRACTELEMENT index type");

      I = ExtractElementInst::Create(Vec, Idx);
      break;
    }

    case naclbitc::FUNC_CODE_INST_INSERTELT: { // INSERTELT: [opval,opval,opval]
      size_t OpNum = 0;
      Value *Vec, *Elt, *Idx;
      if (popValue(Record, &OpNum, NextValueNo, &Vec) ||
          popValue(Record, &OpNum, NextValueNo, &Elt) ||
          popValue(Record, &OpNum, NextValueNo, &Idx) || OpNum != Record.size())
        return Error(InvalidRecord, "Invalid INSERTELEMENT record");

      // expect vector type
      if (!isa<VectorType>(Vec->getType()))
        return Error(InvalidTypeForValue, "Invalid INSERTELEMENT vector type");
      // match vector and element types
      if (cast<VectorType>(Vec->getType())->getElementType() != Elt->getType())
        return Error(InvalidTypeForValue,
                     "Mismatched INSERTELEMENT vector and element type");
      // expect i32
      if (Idx->getType() != Type::getInt32Ty(Context))
        return Error(InvalidTypeForValue, "Invalid INSERTELEMENT index type");

      I = InsertElementInst::Create(Vec, Elt, Idx);
      break;
    }

    case naclbitc::FUNC_CODE_INST_CMP2: { // CMP2: [opval, opval, pred]
      // FCmp/ICmp returning bool or vector of bool

      size_t OpNum = 0;
      Value *LHS, *RHS;
      if (popValue(Record, &OpNum, NextValueNo, &LHS) ||
          popValue(Record, &OpNum, NextValueNo, &RHS) ||
          OpNum+1 != Record.size())
        return Error(InvalidRecord, "Invalid CMP record");

      LHS = ConvertOpToScalar(LHS, CurBBNo);
      RHS = ConvertOpToScalar(RHS, CurBBNo);

      CmpInst::Predicate Predicate;
      if (LHS->getType()->isFPOrFPVectorTy()) {
        if (!naclbitc::DecodeFcmpPredicate(Record[OpNum], Predicate))
          return Error(
              InvalidValue,
              "PNaCl bitcode contains invalid floating comparison predicate");
        I = new FCmpInst(Predicate, LHS, RHS);
      } else {
        if (!naclbitc::DecodeIcmpPredicate(Record[OpNum], Predicate))
          return Error(
              InvalidValue,
              "PNaCl bitcode contains invalid integer comparison predicate");
        I = new ICmpInst(Predicate, LHS, RHS);
      }
      break;
    }

    case naclbitc::FUNC_CODE_INST_RET: // RET: [opval<optional>]
      {
        size_t Size = Record.size();
        if (Size == 0) {
          I = ReturnInst::Create(Context);
          break;
        }

        size_t OpNum = 0;
        Value *Op = NULL;
        if (popValue(Record, &OpNum, NextValueNo, &Op))
          return Error(InvalidRecord, "Invalid RET record");
        if (OpNum != Record.size())
          return Error(InvalidRecord, "Invalid RET record");

        I = ReturnInst::Create(Context, ConvertOpToScalar(Op, CurBBNo));
        break;
      }
    case naclbitc::FUNC_CODE_INST_BR: { // BR: [bb#, bb#, opval] or [bb#]
      if (Record.size() != 1 && Record.size() != 3)
        return Error(InvalidRecord, "Invalid BR record");
      BasicBlock *TrueDest = getBasicBlock(Record[0]);
      if (TrueDest == 0)
        return Error(InvalidRecord, "Invalid BR record");

      if (Record.size() == 1) {
        I = BranchInst::Create(TrueDest);
      }
      else {
        BasicBlock *FalseDest = getBasicBlock(Record[1]);
        Value *Cond = getValue(Record, 2, NextValueNo);
        if (FalseDest == 0 || Cond == 0)
          return Error(InvalidValue, "Invalid BR record");
        if (!Cond->getType()->isIntegerTy(1)) {
          std::string Buffer;
          raw_string_ostream StrBuf(Buffer);
          StrBuf << "Type of branch condition not i1. Found: "
                 << *Cond->getType() << "\n";
          return Error(InvalidValue, StrBuf.str());
        }
        I = BranchInst::Create(TrueDest, FalseDest, Cond);
      }
      break;
    }
    case naclbitc::FUNC_CODE_INST_SWITCH: { // SWITCH: [opty, op0, op1, ...]
      if (Record.size() < 4)
        return Error(InvalidRecord, "Invalid SWITCH record");
      Type *OpTy = getTypeByID(Record[0]);
      unsigned ValueBitWidth = cast<IntegerType>(OpTy)->getBitWidth();
      if (ValueBitWidth > 64)
        return Error(InvalidValue,
                     "Wide integers are not supported in PNaCl bitcode");

      Value *Cond = getValue(Record, 1, NextValueNo);
      BasicBlock *Default = getBasicBlock(Record[2]);
      if (OpTy == 0 || Cond == 0 || Default == 0)
        return Error(InvalidRecord, "Invalid SWITCH record");

      Cond = ConvertOpToScalar(Cond, CurBBNo);
      // TODO(kschimpf): Deal with values that are too large for NumCases.
      size_t NumCases = Record[3];

      SwitchInst *SI = SwitchInst::Create(Cond, Default, NumCases);

      size_t CurIdx = 4;
      for (size_t i = 0; i != NumCases; ++i) {
        // The PNaCl bitcode format has vestigial support for case
        // ranges, but we no longer support reading them because
        // no-one produced them.
        // See https://code.google.com/p/nativeclient/issues/detail?id=3758
        if (CurIdx + 3 >= Record.size())
          return Error(InvalidRecord,
                       "Incomplete case entry in SWITCH record");
        uint64_t NumItems = Record[CurIdx++];
        bool isSingleNumber = Record[CurIdx++];
        if (NumItems != 1 || !isSingleNumber)
          return Error(InvalidRecord,
                       "Case ranges are not supported in PNaCl bitcode");

        APInt CaseValue(ValueBitWidth,
                        NaClDecodeSignRotatedValue(Record[CurIdx++]));
        BasicBlock *DestBB = getBasicBlock(Record[CurIdx++]);
        SI->addCase(ConstantInt::get(Context, CaseValue), DestBB);
      }
      I = SI;
      break;
    }
    case naclbitc::FUNC_CODE_INST_UNREACHABLE: // UNREACHABLE
      I = new UnreachableInst(Context);
      break;
    case naclbitc::FUNC_CODE_INST_PHI: { // PHI: [ty, val0,bb0, ...]
      if (Record.size() < 1 || ((Record.size()-1)&1))
        return Error(InvalidRecord, "Invalid PHI record");
      Type *Ty = getTypeByID(Record[0]);
      if (!Ty) return Error(InvalidType, "Invalid PHI record");

      PHINode *PN = PHINode::Create(Ty, (Record.size()-1)/2);

      for (size_t i = 0, e = Record.size()-1; i != e; i += 2) {
        Value *V;
        // With relative value IDs, it is possible that operands have
        // negative IDs (for forward references).  Use a signed VBR
        // representation to keep the encoding small.
        V = getValueSigned(Record, 1+i, NextValueNo);
        NaClBcIndexSize_t BBIndex = Record[2+i];
        BasicBlock *BB = getBasicBlock(BBIndex);
        if (!V || !BB)
          return Error(InvalidValue, "Invalid PHI record");
        if (Ty == IntPtrType) {
          // Delay installing scalar casts until all instructions of
          // the function are rendered. This guarantees that we insert
          // the conversion just before the incoming edge (or use an
          // existing conversion if already installed).
          V = ConvertOpToScalar(V, BBIndex, /* DeferInsertion = */ true);
        }
        PN->addIncoming(V, BB);
      }
      I = PN;
      break;
    }

    case naclbitc::FUNC_CODE_INST_ALLOCA: { // ALLOCA: [op, align]
      if (Record.size() != 2)
        return Error(InvalidRecord, "Invalid ALLOCA record");
      Value *Size;
      size_t OpNum = 0;
      if (popValue(Record, &OpNum, NextValueNo, &Size))
        return Error(InvalidRecord, "Invalid ALLOCA record");
      unsigned Alignment;
      if (std::error_code EC = getAlignmentValue(Record[1], Alignment))
        return EC;
      I = new AllocaInst(Type::getInt8Ty(Context), Size, Alignment);
      break;
    }
    case naclbitc::FUNC_CODE_INST_LOAD: {
      // LOAD: [op, align, ty]
      size_t OpNum = 0;
      Value *Op;
      if (popValue(Record, &OpNum, NextValueNo, &Op) ||
          Record.size() != 3)
        return Error(InvalidRecord, "Invalid LOAD record");

      // Add pointer cast to op.
      Type *T = getTypeByID(Record[2]);
      if (T == nullptr)
        return Error(InvalidType, "Invalid type for load instruction");
      Op = ConvertOpToType(Op, T->getPointerTo(), CurBBNo);
      if (Op == nullptr)
        return Error(InvalidTypeForValue, "Can't convert cast to type");
      unsigned Alignment;
      if (std::error_code EC = getAlignmentValue(Record[OpNum], Alignment))
        return EC;
      I = new LoadInst(Op, "", false, Alignment);
      break;
    }
    case naclbitc::FUNC_CODE_INST_STORE: {
      // STORE: [ptr, val, align]
      size_t OpNum = 0;
      Value *Val, *Ptr;
      if (popValue(Record, &OpNum, NextValueNo, &Ptr) ||
          popValue(Record, &OpNum, NextValueNo, &Val) ||
          OpNum+1 != Record.size())
        return Error(InvalidRecord, "Invalid STORE record");
      Val = ConvertOpToScalar(Val, CurBBNo);
      Ptr = ConvertOpToType(Ptr, Val->getType()->getPointerTo(), CurBBNo);
      if (Ptr == nullptr)
        return Error(InvalidTypeForValue, "Can't convert cast to type");
      unsigned Alignment;
      if (std::error_code EC = getAlignmentValue(Record[OpNum], Alignment))
       return EC;
      I = new StoreInst(Val, Ptr, false, Alignment);
      break;
    }
    case naclbitc::FUNC_CODE_INST_CALL:
    case naclbitc::FUNC_CODE_INST_CALL_INDIRECT: {
      // CALL: [cc, fnid, arg0, arg1...]
      // CALL_INDIRECT: [cc, fnid, returnty, args...]
      if ((Record.size() < 2) ||
          (BitCode == naclbitc::FUNC_CODE_INST_CALL_INDIRECT &&
           Record.size() < 3))
        return Error(InvalidRecord, "Invalid CALL record");

      unsigned CCInfo = Record[0];

      size_t OpNum = 1;
      Value *Callee;
      if (popValue(Record, &OpNum, NextValueNo, &Callee))
        return Error(InvalidRecord, "Invalid CALL record");

      // Build function type for call.
      FunctionType *FTy = 0;
      Type *ReturnType = 0;
      if (BitCode == naclbitc::FUNC_CODE_INST_CALL_INDIRECT) {
        // Callee type has been elided, add back in.
        ReturnType = getTypeByID(Record[2]);
        ++OpNum;
      } else {
        // Get type signature from callee.
        if (PointerType *OpTy = dyn_cast<PointerType>(Callee->getType())) {
          FTy = dyn_cast<FunctionType>(OpTy->getElementType());
        }
        if (FTy == 0)
          return Error(InvalidType, "Invalid type for CALL record");
      }

      size_t NumParams = Record.size() - OpNum;
      if (FTy && NumParams != FTy->getNumParams())
        return Error(InvalidRecord, "Invalid CALL record");

      // Process call arguments.
      SmallVector<Value*, 6> Args;
      for (size_t Index = 0; Index < NumParams; ++Index) {
        Value *Arg;
        if (popValue(Record, &OpNum, NextValueNo, &Arg)) {
          std::string Buffer;
          raw_string_ostream StrBuf(Buffer);
          StrBuf << "Invalid call argument: Index " << Index;
          return Error(InvalidValue, StrBuf.str());
        }
        if (FTy) {
          // Add a cast, to a pointer type if necessary, in case this
          // is an intrinsic call that takes a pointer argument.
          Arg = ConvertOpToType(Arg, FTy->getParamType(Index), CurBBNo);
        } else {
          Arg = ConvertOpToScalar(Arg, CurBBNo);
        }
        if (Arg == nullptr) {
          std::string Buffer;
          raw_string_ostream StrBuf(Buffer);
          StrBuf << "Unable to cast call argument to parameter type: " << Index;
          return Error(InvalidValue, StrBuf.str());
        }
        Args.push_back(Arg);
      }
      if (FTy == nullptr) {
        // Reconstruct the function type and cast the function pointer
        // to it.
        SmallVector<Type*, 6> ArgTypes;
        for (const auto Arg : Args) {
          ArgTypes.push_back(Arg->getType());
        }
        FTy = FunctionType::get(ReturnType, ArgTypes, false);
        Callee = ConvertOpToType(Callee, FTy->getPointerTo(), CurBBNo);
      }

      // Construct call.
      I = CallInst::Create(Callee, Args);
      CallingConv::ID CallingConv;
      if (!naclbitc::DecodeCallingConv(CCInfo>>1, CallingConv))
        return Error(InvalidValue,
                     "PNaCl bitcode contains invalid calling conventions.");
      cast<CallInst>(I)->setCallingConv(CallingConv);
      cast<CallInst>(I)->setTailCall(CCInfo & 1);
      break;
    }
    case naclbitc::FUNC_CODE_INST_FORWARDTYPEREF:
      // Build corresponding forward reference.
      if (Record.size() != 2 ||
          ValueList.createValueFwdRef(Record[0], getTypeByID(Record[1])))
        return Error(InvalidRecord, "Invalid FORWARDTYPEREF record");
      continue;
    }

    if (std::error_code EC = InstallInstruction(CurBB, I))
      return EC;

    // If this was a terminator instruction, move to the next block.
    if (isa<TerminatorInst>(I)) {
      ++CurBBNo;
      CurBB = getBasicBlock(CurBBNo);
    }

    // Non-void values get registered in the value table for future use.
    if (I && !I->getType()->isVoidTy()) {
      Value *NewVal = I;
      if (NewVal->getType()->isPointerTy() &&
          getFnValueByID(NextValueNo)) {
        // Forward-referenced values cannot have pointer type.
        NewVal = ConvertOpToScalar(NewVal, CurBBNo);
      }
      ValueList.AssignValue(NewVal, NextValueNo++);
    }
  }

OutOfRecordLoop:

  // Add PHI conversions to corresponding incoming block, if not
  // already in the block. Also clear all conversions after fixing
  // PHI conversions.
  for (size_t I = 0, NumBBs = FunctionBBs.size(); I < NumBBs; ++I) {
    BasicBlockInfo &BBInfo = FunctionBBs[I];
    std::vector<CastInst*> &PhiCasts = BBInfo.PhiCasts;
    for (std::vector<CastInst*>::iterator Iter = PhiCasts.begin(),
           IterEnd = PhiCasts.end(); Iter != IterEnd; ++Iter) {
      CastInst *Cast = *Iter;
      if (Cast->getParent() == 0) {
        BasicBlock *BB = BBInfo.BB;
        BB->getInstList().insert(BB->getTerminator(), Cast);
      }
    }
    PhiCasts.clear();
    BBInfo.CastMap.clear();
  }

  // Check the function list for unresolved values.
  if (Argument *A = dyn_cast<Argument>(ValueList.back())) {
    if (A->getParent() == 0) {
      // We found at least one unresolved value.  Nuke them all to avoid leaks.
      for (size_t i = ModuleValueListSize, e = ValueList.size(); i != e; ++i){
        if ((A = dyn_cast<Argument>(ValueList[i])) && A->getParent() == 0) {
          A->replaceAllUsesWith(UndefValue::get(A->getType()));
          delete A;
        }
      }
      return Error(InvalidValue, "Never resolved value found in function!");
    }
  }

  // Trim the value list down to the size it was before we parsed this function.
  ValueList.shrinkTo(ModuleValueListSize);
  FunctionBBs.clear();
  DEBUG(dbgs() << "-> ParseFunctionBody\n");
  return std::error_code();
}

/// FindFunctionInStream - Find the function body in the bitcode stream
std::error_code NaClBitcodeReader::FindFunctionInStream(
    Function *F,
    DenseMap<Function*, uint64_t>::iterator DeferredFunctionInfoIterator) {
  while (DeferredFunctionInfoIterator->second == 0) {
    if (Stream.AtEndOfStream())
      return Error(CouldNotFindFunctionInStream,
                   "Could not find Function in stream");
    // ParseModule will parse the next body in the stream and set its
    // position in the DeferredFunctionInfo map.
    if (std::error_code EC = ParseModule(true))
      return EC;
  }
  return std::error_code();
}

//===----------------------------------------------------------------------===//
// GVMaterializer implementation
//===----------------------------------------------------------------------===//

void NaClBitcodeReader::releaseBuffer() { Buffer.release(); }

std::error_code NaClBitcodeReader::materialize(GlobalValue *GV) {
  Function *F = dyn_cast<Function>(GV);
  // If it's not a function or is already material, ignore the request.
  if (!F || !F->isMaterializable())
    return std::error_code();

  DenseMap<Function*, uint64_t>::iterator DFII = DeferredFunctionInfo.find(F);
  assert(DFII != DeferredFunctionInfo.end() && "Deferred function not found!");
  // If its position is recorded as 0, its body is somewhere in the stream
  // but we haven't seen it yet.
  if (DFII->second == 0) {
    if (std::error_code EC = FindFunctionInStream(F, DFII)) {
      return EC;
    }
  }

  // Move the bit stream to the saved position of the deferred function body.
  Stream.JumpToBit(DFII->second);

  if (std::error_code EC = ParseFunctionBody(F))
    return EC;
  F->setIsMaterializable(false);

  // Upgrade any old intrinsic calls in the function.
  for (UpgradedIntrinsicMap::iterator I = UpgradedIntrinsics.begin(),
       E = UpgradedIntrinsics.end(); I != E; ++I) {
    if (I->first != I->second) {
      for (Value::use_iterator UI = I->first->use_begin(),
           UE = I->first->use_end(); UI != UE; ) {
        if (CallInst* CI = dyn_cast<CallInst>(*UI++))
          UpgradeIntrinsicCall(CI, I->second);
      }
    }
  }

  return std::error_code();
}

bool NaClBitcodeReader::isDematerializable(const GlobalValue *GV) const {
  const Function *F = dyn_cast<Function>(GV);
  if (!F || F->isDeclaration())
    return false;
  return DeferredFunctionInfo.count(const_cast<Function*>(F));
}

void NaClBitcodeReader::Dematerialize(GlobalValue *GV) {
  Function *F = dyn_cast<Function>(GV);
  // If this function isn't dematerializable, this is a noop.
  if (!F || !isDematerializable(F))
    return;

  assert(DeferredFunctionInfo.count(F) && "No info to read function later?");

  // Just forget the function body, we can remat it later.
  F->dropAllReferences();
  F->setIsMaterializable(true);
}


std::error_code NaClBitcodeReader::MaterializeModule(Module *M) {
  assert(M == TheModule &&
         "Can only Materialize the Module this NaClBitcodeReader is attached to.");
  // Iterate over the module, deserializing any functions that are still on
  // disk.
  for (Module::iterator F = TheModule->begin(), E = TheModule->end();
       F != E; ++F) {
    if (F->isMaterializable()) {
      if (std::error_code EC = materialize(F))
        return EC;
    }
  }

  // At this point, if there are any function bodies, the current bit is
  // pointing to the END_BLOCK record after them. Now make sure the rest
  // of the bits in the module have been read.
  if (NextUnreadBit)
    ParseModule(true);

  // Upgrade any intrinsic calls that slipped through (should not happen!) and
  // delete the old functions to clean up. We can't do this unless the entire
  // module is materialized because there could always be another function body
  // with calls to the old function.
  for (std::vector<std::pair<Function*, Function*> >::iterator I =
       UpgradedIntrinsics.begin(), E = UpgradedIntrinsics.end(); I != E; ++I) {
    if (I->first != I->second) {
      for (Value::use_iterator UI = I->first->use_begin(),
           UE = I->first->use_end(); UI != UE; ) {
        if (CallInst* CI = dyn_cast<CallInst>(*UI++))
          UpgradeIntrinsicCall(CI, I->second);
      }
      if (!I->first->use_empty())
        I->first->replaceAllUsesWith(I->second);
      I->first->eraseFromParent();
    }
  }
  std::vector<std::pair<Function*, Function*> >().swap(UpgradedIntrinsics);

  return std::error_code();
}

std::vector<StructType *> NaClBitcodeReader::getIdentifiedStructTypes() const {
  // MERGETODO(dschuff): does this need to contain anything for TypeFinder?
  return std::vector<StructType *>();
}

std::error_code NaClBitcodeReader::InitStream() {
  if (LazyStreamer)
    return InitLazyStream();
  return InitStreamFromBuffer();
}

std::error_code NaClBitcodeReader::InitStreamFromBuffer() {
  const unsigned char *BufPtr = (const unsigned char*)Buffer->getBufferStart();
  const unsigned char *BufEnd = BufPtr+Buffer->getBufferSize();

  if (Buffer->getBufferSize() & 3)
    return Error(InvalidBitstream,
                 "Bitcode stream should be a multiple of 4 bytes in length");

  if (Header.Read(BufPtr, BufEnd))
    return Error(InvalidBitstream, Header.Unsupported());

  if (AcceptHeader())
    return Error(InvalidBitstream, Header.Unsupported());

  StreamFile.reset(new NaClBitstreamReader(BufPtr, BufEnd, Header));
  Stream.init(StreamFile.get());

  return std::error_code();
}

std::error_code NaClBitcodeReader::InitLazyStream() {
  if (Header.Read(LazyStreamer))
    return Error(InvalidBitstream, Header.Unsupported());

  if (AcceptHeader())
    return Error(InvalidBitstream, Header.Unsupported());

  StreamFile.reset(new NaClBitstreamReader(LazyStreamer, Header));
  Stream.init(StreamFile.get());
  return std::error_code();
}

//===----------------------------------------------------------------------===//
// External interface
//===----------------------------------------------------------------------===//

/// \brief Get a lazy one-at-time loading module from bitcode.
///
/// This isn't always used in a lazy context.  In particular, it's also used by
/// \a NaClParseBitcodeFile(). Compared to the upstream LLVM bitcode reader,
/// NaCl does not support BlockAddresses, so it does not need to materialize
/// forward-referenced functions from block address references.
ErrorOr<Module *> llvm::getNaClLazyBitcodeModule(
    std::unique_ptr<MemoryBuffer> &&Buffer, LLVMContext& Context,
    raw_ostream *Verbose, bool AcceptSupportedOnly) {
  Module *M = new Module(Buffer->getBufferIdentifier(), Context);
  NaClBitcodeReader *R =
      new NaClBitcodeReader(Buffer.get(), Context, Verbose, AcceptSupportedOnly);
  M->setMaterializer(R);

  auto cleanupOnError = [&](std::error_code EC) {
    R->releaseBuffer(); // Never take ownership on error.
    delete M;  // Also deletes R.
    return EC;
  };

  if (std::error_code EC = R->ParseBitcodeInto(M))
    return cleanupOnError(EC);

  Buffer.release(); // The BitcodeReader owns it now.
  return M;
}


Module *llvm::getNaClStreamedBitcodeModule(const std::string &name,
                                           StreamingMemoryObject *Streamer,
                                           LLVMContext &Context,
                                           raw_ostream *Verbose,
                                           std::string *ErrMsg,
                                           bool AcceptSupportedOnly) {
  Module *M = new Module(name, Context);
  NaClBitcodeReader *R =
      new NaClBitcodeReader(Streamer, Context, Verbose,
                            AcceptSupportedOnly);
  M->setMaterializer(R);
  if (std::error_code EC = R->ParseBitcodeInto(M)) {
    if (ErrMsg)
      *ErrMsg = EC.message();
    delete M;  // Also deletes R.
    return nullptr;
  }

  return M;
}

ErrorOr<Module *> llvm::NaClParseBitcodeFile(
    MemoryBufferRef Buffer, LLVMContext& Context, raw_ostream *Verbose,
    bool AcceptSupportedOnly){
  std::unique_ptr<MemoryBuffer> Buf = MemoryBuffer::getMemBuffer(Buffer, false);
  ErrorOr<Module *> ModuleOrErr =
      getNaClLazyBitcodeModule(std::move(Buf), Context, Verbose,
                               AcceptSupportedOnly);
  if (!ModuleOrErr)
    return ModuleOrErr;
  Module *M = ModuleOrErr.get();
  // Read in the entire module, and destroy the NaClBitcodeReader.
  if (std::error_code EC = M->materializeAllPermanently()) {
    delete M;
    return EC;
  }

  // TODO: Restore the use-lists to the in-memory state when the bitcode was
  // written.  We must defer until the Module has been fully materialized.

  return M;
}
