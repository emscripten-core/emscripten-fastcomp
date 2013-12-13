//===- NaClBitcodeRecordBitsDist.h -----------------------------*- C++ -*-===//
//     Maps distributions of values and corresponding number of
//     bits in PNaCl bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Creates a (nestable) distribution map of values, and the correspdonding
// bits, in PNaCl bitcode records. These distributions are built directly
// on top of NaClBitcodeRecordDist and NaClBitcodeRecordDistElement classes.
// See (included) file NaClBitcodeRecordDist.h for more details on these
// classes, and how you should use them.

#ifndef LLVM_BITCODE_NACL_NACLBITCODERECORDBITSDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODERECORDBITSDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeRecordDist.h"

namespace llvm {

/// Defines the element type of a PNaCl bitcode distribution map when
/// we want to count both the number of instances, and the number of
/// bits used by each record. Also tracks the number to times an
/// abbreviation was used to parse the corresponding record.
class NaClBitcodeRecordBitsDistElement : public NaClBitcodeRecordDistElement {
  NaClBitcodeRecordBitsDistElement(const NaClBitcodeRecordBitsDistElement&)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeRecordBitsDistElement&)
      LLVM_DELETED_FUNCTION;

public:
  // Create an element with no instances.
  NaClBitcodeRecordBitsDistElement(
      NaClBitcodeRecordDist<NaClBitcodeRecordDistElement>* NestedDist)
      : NaClBitcodeRecordDistElement(NestedDist), TotalBits(0), NumAbbrevs(0)
  {}

  virtual ~NaClBitcodeRecordBitsDistElement() {}

  virtual void Add(const NaClBitcodeRecord &Record) {
    NaClBitcodeRecordDistElement::Add(Record);
    TotalBits += Record.GetNumBits();
    if (Record.UsedAnAbbreviation()) {
      ++NumAbbrevs;
    }
  }

  // Returns the total number of bits used to represent all instances
  // of this value.
  uint64_t GetTotalBits() const {
    return TotalBits;
  }

  // Returns the number of times an abbreviation was used to represent
  // the value.
  unsigned GetNumAbbrevs() const {
    return NumAbbrevs;
  }

private:
  // Number of bits used to represent all instances of the value.
  uint64_t TotalBits;
  // Number of times an abbreviation is used for the value.
  unsigned NumAbbrevs;
};

/// Defines a PNaCl bitcode distribution map when we want to count
/// both the number of instances, and the number of bits used by each
/// record.
///
/// ElementType is assumed to be a derived class of
/// NaClBitcodeRecordBitsDistElement.
template<class ElementType>
class NaClBitcodeRecordBitsDist : public NaClBitcodeRecordDist<ElementType> {
  NaClBitcodeRecordBitsDist(const NaClBitcodeRecordBitsDist&)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeRecordBitsDist&)
      LLVM_DELETED_FUNCTION;

public:
  NaClBitcodeRecordBitsDist()
      : NaClBitcodeRecordDist<ElementType>() {}

  virtual ~NaClBitcodeRecordBitsDist() {}

protected:
  virtual void PrintRowStats(raw_ostream &Stream,
                             std::string Indent,
                             NaClBitcodeRecordDistValue Value) const {

    ElementType *Element = this->at(Value);
    Stream << Indent
           << format("%7d %6.2f %9lu ",
                     Element->GetNumInstances(),
                     (double) Element->GetNumInstances()/
                     this->GetTotal()*100.0,
                     (unsigned long) Element->GetTotalBits())
           << format("%9.2f",
                     (double) Element->GetTotalBits()/
                     Element->GetNumInstances());
    if (Element->GetNumAbbrevs())
      Stream << format(" %7.2f  ",
                       (double) Element->GetNumAbbrevs()/
                       Element->GetNumInstances()*100.0);
    else
      Stream << "          ";
  }

  virtual void PrintHeader(raw_ostream &Stream, std::string Indent) const {
    Stream << Indent
           << "  Count %Total    # Bits Bits/Elmt   % Abv  "
           << this->GetValueHeader() << "\n";
  }
};

}

#endif
