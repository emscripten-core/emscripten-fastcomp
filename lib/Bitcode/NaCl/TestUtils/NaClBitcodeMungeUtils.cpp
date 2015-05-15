//===--- Bitcode/NaCl/TestUtils/NaClBitcodeMungeUtils.cpp - Munge Bitcode -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Munge bitcode records utility class NaClMungedBitcode.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeMungeUtils.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Format.h"

using namespace llvm;

namespace {

// \brief Extracts an uint64_t value from the array of values.
//
// \param Values The array of values to extract from.
// \param ValuesSize The length of Values.
// \param Terminator Denotes the end of a bitcode record.
// \param [in/out] Index The index within Values to extract the
// integer.  Updates Index to point to the next value after
// extraction.
uint64_t readValue(const uint64_t Values[], size_t ValuesSize,
                   uint64_t Terminator, size_t &Index) {
  if (Index < ValuesSize && Values[Index] != Terminator)
    return Values[Index++];
  std::string Buffer;
  raw_string_ostream StrBuf(Buffer);
  StrBuf << "Value expected at index " << Index;
  report_fatal_error(StrBuf.str());
}

// \brief Extracts value of Type from the array of values. Parameters
// are the same as for readValue.
template <class Type>
Type readAsType(const uint64_t Values[], size_t ValuesSize, uint64_t Terminator,
                size_t &Index) {
  uint64_t Value = readValue(Values, ValuesSize, Terminator, Index);
  Type ValueAsType = static_cast<Type>(Value);
  if (Value == ValueAsType)
    return ValueAsType;
  std::string Buffer;
  raw_string_ostream StrBuf(Buffer);
  StrBuf << "Out of range value " << Value << " at index " << (Index - 1);
  report_fatal_error(StrBuf.str());
}

// \brief Extracts an edit action from the array of values. Parameters
// are the same as for readValue.
NaClMungedBitcode::EditAction readEditAction(const uint64_t Values[],
                                             size_t ValuesSize,
                                             uint64_t Terminator,
                                             size_t &Index) {
  uint64_t Value = readValue(Values, ValuesSize, Terminator, Index);
  if (Value <= NaClMungedBitcode::Replace)
    return static_cast<NaClMungedBitcode::EditAction>(Value);
  std::string Buffer;
  raw_string_ostream StrBuf(Buffer);
  StrBuf << "Edit action expected at index " << Index << ". Found: " << Value;
  report_fatal_error(StrBuf.str());
}

} // end of anonymous namespace

namespace llvm {

void readNaClBitcodeRecordList(NaClBitcodeRecordList &RecordList,
                               const uint64_t Records[],
                               size_t RecordsSize,
                               uint64_t RecordTerminator) {
  for (size_t Index = 0; Index < RecordsSize;) {
    std::unique_ptr<NaClBitcodeAbbrevRecord>
        Rcd(new NaClBitcodeAbbrevRecord());
    Rcd->read(Records, RecordsSize, RecordTerminator, Index);
    RecordList.push_back(std::move(Rcd));
  }
}

} // end of namespace llvm

void NaClBitcodeAbbrevRecord::print(raw_ostream &Out) const {
  NaClBitcodeRecordData::Print(Out << Abbrev << ": ");
}

void NaClBitcodeAbbrevRecord::read(const uint64_t Vals[], size_t ValsSize,
                                   uint64_t Terminator, size_t &Index) {
  Abbrev = readAsType<unsigned>(Vals, ValsSize, Terminator, Index);
  Code = readAsType<unsigned>(Vals, ValsSize, Terminator, Index);
  Values.clear();
  while (Index < ValsSize) {
    uint64_t Value = Vals[Index++];
    if (Value == Terminator)
      break;
    Values.push_back(Value);
  }
}

NaClMungedBitcode::NaClMungedBitcode(const uint64_t Records[],
                                     size_t RecordsSize,
                                     uint64_t RecordTerminator)
    : BaseRecords(new NaClBitcodeRecordList()) {
  readNaClBitcodeRecordList(*BaseRecords, Records, RecordsSize,
                            RecordTerminator);
}

void NaClMungedBitcode::print(raw_ostream &Out) const {
  size_t Indent = 0;
  for (const NaClBitcodeAbbrevRecord &Record : *this) {
    if (Indent && Record.Code == naclbitc::BLK_CODE_EXIT)
      --Indent;
    for (size_t i = 0; i < Indent; ++i) {
      Out << "  ";
    }
    // Blank fill to make abbreviation indices right align, and then
    // print record.
    uint32_t Cutoff = 9999999;
    while (Record.Abbrev <= Cutoff && Cutoff) {
      Out << " ";
      Cutoff /= 10;
    }
    Out << Record << "\n";
    if (Record.Code == naclbitc::BLK_CODE_ENTER)
      ++Indent;
  }
}

NaClMungedBitcode::~NaClMungedBitcode() {
  removeEdits();
}

void NaClMungedBitcode::addBefore(size_t RecordIndex,
                                  NaClBitcodeAbbrevRecord &Record) {
  assert(RecordIndex < BaseRecords->size());
  at(BeforeInsertionsMap, RecordIndex).push_back(copy(Record));
}

void NaClMungedBitcode::addAfter(size_t RecordIndex,
                                 NaClBitcodeAbbrevRecord &Record) {
  assert(RecordIndex < BaseRecords->size());
  at(AfterInsertionsMap, RecordIndex).push_back(copy(Record));
}

void NaClMungedBitcode::remove(size_t RecordIndex) {
  assert(RecordIndex < BaseRecords->size());
  NaClBitcodeAbbrevRecord *&RcdPtr = ReplaceMap[RecordIndex];
  if (RcdPtr != nullptr)
    delete RcdPtr;
  RcdPtr = nullptr;
}

void NaClMungedBitcode::replace(size_t RecordIndex,
                                NaClBitcodeAbbrevRecord &Record) {
  assert(RecordIndex < BaseRecords->size());
  NaClBitcodeAbbrevRecord *&RcdPtr = ReplaceMap[RecordIndex];
  if (RcdPtr != nullptr)
    delete RcdPtr;
  RcdPtr = copy(Record);
}

NaClBitcodeAbbrevRecord *
NaClMungedBitcode::copy(const NaClBitcodeAbbrevRecord &Record) {
  return new NaClBitcodeAbbrevRecord(Record);
}

void NaClMungedBitcode::destroyInsertionsMap(
    NaClMungedBitcode::InsertionsMapType &Map) {
  for (auto Pair : Map) {
    DeleteContainerPointers(*Pair.second);
    delete Pair.second;
  }
  Map.clear();
}

void NaClMungedBitcode::removeEdits() {
  destroyInsertionsMap(BeforeInsertionsMap);
  destroyInsertionsMap(AfterInsertionsMap);
  DeleteContainerSeconds(ReplaceMap);
  ReplaceMap.clear();
}

void NaClMungedBitcode::munge(const uint64_t Munges[], size_t MungesSize,
                              uint64_t Terminator) {
  for (size_t Index = 0; Index < MungesSize;) {
    size_t RecordIndex =
        readAsType<size_t>(Munges, MungesSize, Terminator, Index);
    if (RecordIndex >= BaseRecords->size()) {
      std::string Buffer;
      raw_string_ostream StrBuf(Buffer);
      StrBuf << "Record index " << RecordIndex << " out of range. "
             << "Must be less than " << BaseRecords->size() << "\n";
      report_fatal_error(StrBuf.str());
    }
    switch (readEditAction(Munges, MungesSize, Terminator, Index)) {
    case NaClMungedBitcode::AddBefore: {
      NaClBitcodeAbbrevRecord Record;
      Record.read(Munges, MungesSize, Terminator, Index);
      addBefore(RecordIndex, Record);
      break;
    }
    case NaClMungedBitcode::AddAfter: {
      NaClBitcodeAbbrevRecord Record;
      Record.read(Munges, MungesSize, Terminator, Index);
      addAfter(RecordIndex, Record);
      break;
    }
    case NaClMungedBitcode::Remove: {
      remove(RecordIndex);
      break;
    }
    case NaClMungedBitcode::Replace: {
      NaClBitcodeAbbrevRecord Record;
      Record.read(Munges, MungesSize, Terminator, Index);
      replace(RecordIndex, Record);
      break;
    }
    }
  }
}

NaClMungedBitcodeIter NaClMungedBitcode::begin() const {
  return NaClMungedBitcodeIter::begin(*this);
}

NaClMungedBitcodeIter NaClMungedBitcode::end() const {
  return NaClMungedBitcodeIter::end(*this);
}

bool NaClMungedBitcodeIter::
operator==(const NaClMungedBitcodeIter &Iter) const {
  if (MungedBitcode != Iter.MungedBitcode || Index != Iter.Index ||
      Position != Iter.Position)
    return false;
  // Deal with the fact that InsertionsIter is undefined when
  // at the end of all records.
  if (Index == MungedBitcode->BaseRecords->size())
    return true;
  return InsertionsIter == Iter.InsertionsIter;
}

NaClBitcodeAbbrevRecord &NaClMungedBitcodeIter::operator*() {
  switch (Position) {
  case InBeforeInsertions:
  case InAfterInsertions:
    assert(Index < MungedBitcode->BaseRecords->size() &&
           InsertionsIter != InsertionsIterEnd);
    return **InsertionsIter;
  case AtIndex: {
    NaClMungedBitcode::ReplaceMapType::const_iterator Pos =
        MungedBitcode->ReplaceMap.find(Index);
    if (Pos == MungedBitcode->ReplaceMap.end())
      return *(*MungedBitcode->BaseRecords)[Index];
    assert(Pos->second);
    return *Pos->second;
  }
  }
}

NaClMungedBitcodeIter &NaClMungedBitcodeIter::operator++() {
  switch (Position) {
  case InBeforeInsertions:
  case InAfterInsertions:
    assert(Index < MungedBitcode->BaseRecords->size() &&
           InsertionsIter != InsertionsIterEnd);
    ++InsertionsIter;
    break;
  case AtIndex: {
    Position = InAfterInsertions;
    placeAt(MungedBitcode->AfterInsertionsMap, Index);
    break;
  }
  }
  updatePosition();
  return *this;
}

void NaClMungedBitcodeIter::updatePosition() {
  while (true) {
    switch (Position) {
    case InBeforeInsertions:
      if (Index >= MungedBitcode->BaseRecords->size() ||
          InsertionsIter != InsertionsIterEnd)
        return;
      Position = AtIndex;
      break;
    case AtIndex: {
      NaClMungedBitcode::ReplaceMapType::const_iterator Pos =
          MungedBitcode->ReplaceMap.find(Index);
      // Stop looking if no replacement, or index has a replacement.
      if (Pos == MungedBitcode->ReplaceMap.end() || Pos->second != nullptr)
        return;
      // Base element has been removed.
      Position = InAfterInsertions;
      placeAt(MungedBitcode->AfterInsertionsMap, Index);
      break;
    }
    case InAfterInsertions:
      if (InsertionsIter != InsertionsIterEnd)
        return;
      Position = InBeforeInsertions;
      ++Index;
      placeAt(MungedBitcode->BeforeInsertionsMap, Index);
      break;
    }
  }
}
