//===- NaClBitcodeMungeUtils.h - Munge bitcode records --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines utility class NaClMungedBitcode to edit a base
// sequence of PNaCl bitcode records. It is intended to be used for
// both unit testing, and for fuzzing bitcode files.
//
// The editing actions are defined in terms of the base sequence of
// bitcode records, and do not actually modify that base sequence. As
// such, all edits are defined relative to a record index in the base
// sequence of bitcode records. Using record indices into the base
// sequence means that succeeding edits need not modify record indices
// based on the effects of preceding edits. Rather, the record index
// remains constant across all editing actions.
//
// There are two steps in creating munged bitcode. The first step
// defines the initial (base) sequence of bitcode records to be
// edited. The second is to apply editing actions to the base sequence
// of records.
//
// Defining the initial sequence of bitcode records is done by
// using NaClMungedBitcode's constructor.
//
// There are four kinds of editing actions:
//
// 1) Add a record before a (base) record index.
// 2) Add a record after a (base) record index.
// 3) Remove a record at a (base) record index.
// 4) Replace a record at a (base) record index.
//
// Each of the editing actions are defined by the methods addBefore(),
// addAfter(), remove(), and replace(). The edited sequence of records
// is defined by an iterator, and is accessible using methods begin()
// and end().
//
// If multiple records are added before/after a record index, the
// order of the added records correspond to the order they were added.
//
// For unit testing, simple array interfaces are provided. The first
// interface defines the initial base sequence of bitcode records. The
// second interface, method munge(), defines a sequence of editing
// actions to apply to the base sequence of records.
//
// These arrays are defined using type uint64_t. A bitcode record is
// defined as a sequence of values of the form:
//
//   AbbrevIndex, RecordCode, Value1, ..., ValueN , Terminator
//
// Terminator is a (user-specified) designated constant used to mark
// the end of bitcode records. A record can contain zero or more
// values.
//
// An editing action is defined as one of the following sequences of
// values:
//
//   RecordIndex, AddBefore, AbbrevIndex, RecordCode, Value, ..., Terminator
//   RecordIndex, AddAfter, AbbrevIndex, RecordCode, Value, ..., Terminator
//   RecordIndex, Remove
//   RecordIndex, Replace, AbbrevIndex, RecordCode, Value, ..., Terminator.
//
// RecordIndex defines where the action applies, relative to the base
// sequence of records. EditAction defines the editing action to
// apply. The remaining fields define an optional record associated
// with the editing action.
//
// ===---------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLBITCODEMUNGEUTILS_H
#define LLVM_BITCODE_NACL_NACLBITCODEMUNGEUTILS_H

#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Support/MemoryBuffer.h"

#include <list>
#include <map>

namespace llvm {

class NaClBitcodeAbbrevRecord; // bitcode record.
class NaClMungedBitcodeIter;   // iterator over edited bitcode records.

/// \brief Defines a list of bitcode records.
typedef std::vector<std::unique_ptr<NaClBitcodeAbbrevRecord>>
    NaClBitcodeRecordList;

// TODO(kschimpf): Modify following two methods to return error_code.

/// \brief Extracts out the records in Records, and puts them into RecordList.
///
/// \brief RecordList[in/out] Record list to read into.
/// \brief Records Array containing data defining records.
/// \brief RecordsSize The size of array Records.
/// \brief RecordTerminator The value used to terminate records.
void readNaClBitcodeRecordList(NaClBitcodeRecordList &RecordList,
                               const uint64_t Records[],
                               size_t RecordsSize,
                               uint64_t RecordTerminator);

/// Read in the list of records from binary bitcode from a memory buffer.
void readNaClBitcodeRecordList(NaClBitcodeRecordList &RecordList,
                               std::unique_ptr<MemoryBuffer> InputBuffer);

/// Read in the list of records from textual bitcode from a memory buffer.
std::error_code readNaClTextBcRecordList(
    NaClBitcodeRecordList &RecordList,
    std::unique_ptr<MemoryBuffer> InputBuffer);

/// Read textual bitcode records from Filename, and fill Buffer with
/// corresponding bitcode. Return error_code describing success of
/// read.  Verbose (if not nullptr) is used to generate more human
/// readable error messages than the text in the returned error
/// message.
std::error_code readNaClRecordTextAndBuildBitcode(
    StringRef Filename, SmallVectorImpl<char> &Buffer,
    raw_ostream *Verbose = nullptr);

/// Write out RecordList (as text) to Buffer. Returns true when
/// successful. Error message are written to ErrStream.
bool writeNaClBitcodeRecordList(NaClBitcodeRecordList &RecordList,
                                SmallVectorImpl<char> &Buffer,
                                raw_ostream &ErrStream);

/// \brief An edited (i.e. munged) list of bitcode records. Edits are
/// always relative to the initial list of records.
class NaClMungedBitcode {
  friend class NaClMungedBitcodeIter;
  NaClMungedBitcode() = delete;
  NaClMungedBitcode(const NaClMungedBitcode &) = delete;
  NaClMungedBitcode &operator=(const NaClMungedBitcode &) = delete;

public:
  /// \brief Iterator over edited records.
  typedef NaClMungedBitcodeIter iterator;

  /// \brief Read in initial list of records from bitcode in a memory buffer.
  explicit NaClMungedBitcode(std::unique_ptr<MemoryBuffer> InputBuffer,
                             bool ReadAsText=false)
      : BaseRecords(new NaClBitcodeRecordList()) {
    if (ReadAsText)
      readNaClTextBcRecordList(*BaseRecords, std::move(InputBuffer));
    else
      readNaClBitcodeRecordList(*BaseRecords, std::move(InputBuffer));
  }

  /// \brief Initialize the list of records to be edited.
  explicit NaClMungedBitcode(std::unique_ptr<NaClBitcodeRecordList> BaseRecords)
      : BaseRecords(std::move(BaseRecords)) {}

  /// \brief Initialize the list of records to be edited using
  /// array specification.
  ///
  /// \brief Records Array containing data defining records.
  /// \brief RecordsSize The size of array Records.
  /// \brief RecordTerminator The value used to terminate records.
  NaClMungedBitcode(const uint64_t Records[], size_t RecordsSize,
                    uint64_t RecordTerminator);

  ~NaClMungedBitcode();

  /// \brief Iterator pointing to first record in edited records.
  NaClMungedBitcodeIter begin() const;

  /// \brief Iterator pointing after last record in edited records.
  NaClMungedBitcodeIter end() const;

  /// \brief Insert Record immediately before the record at RecordIndex.
  ///
  /// \param RecordIndex The index (within BaseRecords) to add before.
  /// \param Record The record to add.
  void addBefore(size_t RecordIndex, NaClBitcodeAbbrevRecord &Record);

  /// \brief Insert Record after the record at RecordIndex (and after
  /// any previously added after records for RecordIndex).
  ///
  /// \param RecordIndex The index (within BaseRecords) to add after.
  /// \param Record The record to add.
  void addAfter(size_t RecordIndex, NaClBitcodeAbbrevRecord &Record);

  /// \brief Remove the record at RecordIndex. Note that because
  /// the record index is based on the base records, and those records
  /// are not modified, this editing action effectively undoes all
  /// previous remove/replace editing actions for this index.
  void remove(size_t RecordIndex);

  /// \brief Replace the record at RecordIndex with Record. Note that
  /// because the record index is based on the base records, and those
  /// records are not modified, this editing action effectively undoes
  /// all previous remove/replace editing actions for this index.
  void replace(size_t RecordIndex, NaClBitcodeAbbrevRecord &Record);

  /// \brief Print out the resulting edited list of records.
  void print(raw_ostream &Out) const;

  /// Defines set of possible write flags.
  struct WriteFlags {
    /// True if error recovery should be applied.
    bool getTryToRecover() const { return TryToRecover; }

    /// Define that error recovery should be applied when writing.
    void setTryToRecover(bool NewValue) {
      TryToRecover = NewValue;
      assert(!(TryToRecover && WriteBadAbbrevIndex));
    }

    /// True if a bad abbreviation index should be written (rather than
    /// trying error recovery) so that bitcode readers can be tested for
    /// this condition.
    bool getWriteBadAbbrevIndex() const { return WriteBadAbbrevIndex; }

    /// Define that the first bad abbreviation index should be written,
    /// and corresponding minimal context added so that the bitcode can
    /// be used to test reading the erroneous written bitcode.
    void setWriteBadAbbrevIndex(bool NewValue) {
      WriteBadAbbrevIndex = NewValue;
      assert(!(TryToRecover && WriteBadAbbrevIndex));
    }

    /// Get the stream to print errors while writing bitcode.
    raw_ostream &getErrStream() const {
      return ErrStream ? *ErrStream : errs();
    }

    /// Set the stream to print errors to.
    void setErrStream(raw_ostream &NewValue) {
      ErrStream = &NewValue;
    }

    void reset() {
      TryToRecover = false;
      WriteBadAbbrevIndex = false;
      ErrStream = nullptr;
    }

  private:
    bool TryToRecover = false;
    bool WriteBadAbbrevIndex = false;
    raw_ostream *ErrStream = nullptr;
  };

  /// Defines the results associated with writing bitcode.
  struct WriteResults {
    /// Number of errors generated.
    size_t NumErrors = 0;
    /// Number of repairs (via error recovery) that were applied.
    size_t NumRepairs = 0;
    /// True if a bad abbreviation index were written.
    bool WroteBadAbbrevIndex = false;
  };

  /// \brief Write out the edited list of bitcode records using
  /// the given buffer.
  ///
  /// \param Buffer The buffer to write into.
  /// \param AddHeader Add header block when true.
  /// \param Flags Write flags to use.
  ///
  /// \return Returns the results of the write.
  WriteResults writeMaybeRepair(
      SmallVectorImpl<char> &Buffer, bool AddHeader,
      const WriteFlags &Flags) const;

  bool write(SmallVectorImpl<char> &Buffer, bool AddHeader,
             const WriteFlags &Flags) const {
    WriteResults Results = writeMaybeRepair(Buffer, AddHeader, Flags);
    return Results.NumErrors == 0
        || (Flags.getTryToRecover() && Results.NumErrors == Results.NumRepairs);
  }

  bool write(SmallVectorImpl<char> &Buffer, bool AddHeader) const {
    WriteFlags Flags;
    return write(Buffer, AddHeader, Flags);
  }

  /// \brief The types of editing actions that can be applied.
  enum EditAction {
    AddBefore, // Insert new record before base record at index.
    AddAfter,  // Insert new record after base record at index.
    Remove,    // Remove record at index.
    Replace    // Replace base record at index with new record.
  };

  /// \brief Apply a set of edits defined in the given array.
  ///
  /// Actions are a sequence of values, followed by a (common)
  /// terminator value. Valid action sequences are:
  ///
  /// RecordIndex AddBefore Abbrev Code Values Terminator
  ///
  /// RecordIndex AddAfter Abbrev Code Values Terminator
  ///
  /// RecordIndex Remove
  ///
  /// RecordIndex Replace Abbrev Code Values Terminator
  ///
  /// \param Munges The array containing the edits to apply.
  /// \param MungesSize The size of Munges.
  /// \param Terminator The value used to terminate records in editing actions.
  void munge(const uint64_t Munges[], size_t MungesSize, uint64_t Terminator);

  /// \brief Removes all editing actions and resets back to the original
  /// set of base records.
  void removeEdits();

  /// Returns the unedited list of bitcode records.
  const NaClBitcodeRecordList &getBaseRecords() const {
    return *BaseRecords;
  }

private:
  typedef std::list<NaClBitcodeAbbrevRecord *> RecordListType;
  typedef std::map<size_t, RecordListType *> InsertionsMapType;
  typedef std::map<size_t, NaClBitcodeAbbrevRecord *> ReplaceMapType;

  /// \brief The list of base records that will be edited.
  std::unique_ptr<NaClBitcodeRecordList> BaseRecords;
  // Holds map from record index to list of records added before
  // the corresponding record in the list of base records.
  InsertionsMapType BeforeInsertionsMap;
  // Holds map from record index to the list of records added after
  // the corresponding record in the list of base records.
  InsertionsMapType AfterInsertionsMap;
  // Holds map from record index to the record that replaces the
  // corresponding record in the list of base records. Note: If the
  // range is the nullptr, it corresponds to a remove instead of a
  // replace.
  ReplaceMapType ReplaceMap;

  // Returns the list of records associated with Index in Map.
  RecordListType &at(InsertionsMapType &Map, size_t Index) {
    InsertionsMapType::iterator Pos = Map.find(Index);
    if (Pos == Map.end()) {
      RecordListType *List = new RecordListType();
      Map.emplace(Index, List);
      return *List;
    }
    return *Pos->second;
  }

  // Creates a (heap allocated) copy of the given record.
  NaClBitcodeAbbrevRecord *copy(const NaClBitcodeAbbrevRecord &Record);

  // Delete nested objects within insertions map.
  void destroyInsertionsMap(NaClMungedBitcode::InsertionsMapType &Map);
};

/// \brief Defines a bitcode record with its associated abbreviation index.
class NaClBitcodeAbbrevRecord : public NaClBitcodeRecordData {
  NaClBitcodeAbbrevRecord &operator=(NaClBitcodeAbbrevRecord &) = delete;

public:
  /// \brief The abbreviation associated with the bitcode record.
  unsigned Abbrev;

  /// \brief Creates a bitcode record.
  ///
  /// \param Abbrev Abbreviation index associated with record.
  /// \param Code The selector code of the record.
  /// \param Values The values associated with the selector code.
  NaClBitcodeAbbrevRecord(unsigned Abbrev, unsigned Code,
                          const NaClRecordVector &Values)
      : NaClBitcodeRecordData(Code, Values), Abbrev(Abbrev) {}

  /// \brief Creates a copy of the given abbreviated bitcode record.
  explicit NaClBitcodeAbbrevRecord(const NaClBitcodeAbbrevRecord &Rcd)
      : NaClBitcodeRecordData(Rcd), Abbrev(Rcd.Abbrev) {}

  /// \brief Creates a default abbreviated bitcode record.
  NaClBitcodeAbbrevRecord() : Abbrev(naclbitc::UNABBREV_RECORD) {}

  ~NaClBitcodeAbbrevRecord() {}

  /// \brief Replaces the contents of the abbreviated bitcode record with
  /// the corresponding contents in the array.
  ///
  /// \param Values The array to get values from.
  /// \param ValuesSize The length of the Values array.
  /// \param Terminator The value defining the end of the record in Values.
  /// \param Index The index of the first value to be used in Values.
  void read(const uint64_t Values[], size_t ValuesSize,
            const uint64_t Terminator, size_t &Index);

  void print(raw_ostream &out) const;
};

inline raw_ostream &operator<<(raw_ostream &Out,
                               const NaClBitcodeAbbrevRecord &Record) {
  Record.print(Out);
  return Out;
}

/// \brief Defines an iterator to walk over elements of an edited
/// record list.
class NaClMungedBitcodeIter {

public:
  /// \brief Returns an iterator pointing to the first record in the
  /// edited list of records.
  static NaClMungedBitcodeIter begin(const NaClMungedBitcode &MungedBitcode) {
    return NaClMungedBitcodeIter(MungedBitcode, 0);
  }

  /// \brief Returns an iterator pointing past the last record in the
  /// edited list of records.
  static NaClMungedBitcodeIter end(const NaClMungedBitcode &MungedBitcode) {
    return NaClMungedBitcodeIter(MungedBitcode,
                                 MungedBitcode.BaseRecords->size());
  }

  bool operator==(const NaClMungedBitcodeIter &Iter) const;

  bool operator!=(const NaClMungedBitcodeIter &Iter) const {
    return !operator==(Iter);
  }

  /// \brief Advances the iterator over one record in the list of
  /// edited records.
  NaClMungedBitcodeIter &operator++();

  /// \brief Returns the bitcode record the iterator is before.
  NaClBitcodeAbbrevRecord &operator*();

private:
  /// \brief Defines position of the iterator, relative to the corresponding
  /// record index in the base list of records.
  enum MungedPosition {
    /// Processing the list of records inserted before the record
    /// index of the base list of records.
    InBeforeInsertions,
    /// Processing the record at the given record index of the base
    /// list of records.
    AtIndex,
    /// Processing the list of records inserted after the record index
    /// of the base list of records.
    InAfterInsertions,
  };

  // The edited list of records to iterate over.
  const NaClMungedBitcode *MungedBitcode;
  // The corresponding index, wrt to the edited list of records, that
  // is being processed.
  size_t Index;
  // The position of the iterator wrt to Index.
  MungedPosition Position;
  // An iterator defining the current position within a
  // BeforeInsertions or AfterInsertions list. When the iterator is
  // not in the corresponding position (i.e. InBeforeInsertions or
  // InAfterInsertions) this iterator is undefined.
  NaClMungedBitcode::RecordListType::const_iterator InsertionsIter;
  // An iterator defining the end position of the corresponding list
  // of records defined by InsertionsIter. Only defined when
  // InsertionsIter is defined.
  NaClMungedBitcode::RecordListType::const_iterator InsertionsIterEnd;
  // Dummy list to initialize InsertionsIter if no such list exists.
  NaClMungedBitcode::RecordListType EmptyList;

  // \brief Defines an iterator at the beginning of the
  // BeforeInsertions list associated with the given Index in
  // MungedBitcode.
  NaClMungedBitcodeIter(const NaClMungedBitcode &MungedBitcode, size_t Index)
      : MungedBitcode(&MungedBitcode), Index(Index),
        Position(InBeforeInsertions), InsertionsIter(), InsertionsIterEnd() {
    placeAt(MungedBitcode.BeforeInsertionsMap, 0);
    updatePosition();
  }

  // \brief Places the corresponding insertions iterator based on the
  // list of records defined at Index for the given insertions Map.
  void placeAt(const NaClMungedBitcode::InsertionsMapType &Map, size_t Index) {
    NaClMungedBitcode::InsertionsMapType::const_iterator Pos = Map.find(Index);
    if (Pos == Map.end()) {
      InsertionsIter = EmptyList.end();
      InsertionsIterEnd = EmptyList.end();
    } else {
      InsertionsIter = Pos->second->begin();
      InsertionsIterEnd = Pos->second->end();
    }
  }

  // \brief Moves the iterator to the position of the next edited
  // record.
  void updatePosition();
};

} // end namespace llvm.

#endif // LLVM_BITCODE_NACL_NACLBITCODEMUNGE_H
