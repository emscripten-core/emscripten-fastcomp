//===- NaClBitcodeDist.h ---------------------------------------*- C++ -*-===//
//     Maps distributions of values in PNaCl bitcode files.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Creates a (nestable) distribution map of values in PNaCl bitcode.
// The domain of these maps is the set of record values being
// tracked. The range is the information associated with each block
// and/or record value, including the number of instances of that value. The
// distribution map is nested if the range element contains another
// distribution map.
//
// The goal of distribution maps is to build a (histogram)
// distribution of values in bitcode records and blocks, of a PNaCl
// bitcode file. From appropriately built distribution maps, one can
// infer possible new abbreviations that can be used in the PNaCl
// bitcode file.  Hence, one of the primary goals of distribution maps
// is to support tools pnacl-bcanalyzer and pnacl-bccompress.
//
// Distribution maps are constructed either from NaClBitcodeBlock's or
// NaClBitcodeRecord's (in NaClBitcodeParser.h), but not both. It is
// assumed that only blocks, or records (but not both) are added to a
// distribution map. To add data to the distribution map, one calls
// AddRecord and/or AddBlock. If the distribution map contains record
// values, you must call AddRecord for each record to be put into the
// distribution map. If the distribution map contains block values (i.e.
// block ID's), you must call AddBlock for each block to be put into
// the distribution map.
//
// While it may counterintuitive, one can call both AddRecord and
// AddBlock, for each corresponding record and block processed. The
// reason for this is that an internal flag StorageKind is kept within
// distribution maps.  If the flag value doesn't correspond to the
// type of called add method, no update is done. This behaviour is
// done so that nested distribution maps can be updated via blind
// calls in NaClBitcodeAnalyzer.cpp.
//
// Via inheritance, and overriding the (virtual) AddRecord
// method for a distribution map, we can redirect the add to look up
// the distribution element associated with the block of the record,
// and then update the corresponding record distribution map. In general,
// it only makes sense (for nested distribution maps) to be able to
// redirect record additions. Redirecting blocks within a record (since
// a record is only associated with one block) does not make sense. Hence,
// we have not made AddBlock virtual.
//
// When updating a block distribution map, the domain value is the
// BlockID of the corresponding block being added.
//
// On the other hand, values associated with record distribution maps
// are many possible values (the code, the abbreviation, the values
// etc). To make the API uniform, record distribution maps are updated
// using NaClBitcodeRecords (in NaClBitcodeParser.h). The values from
// the record are defined by the extract method GetValueList, and
// added via method AddRecord.
//
// Distribution maps are implemented using two classes:
//
//  NaClBitcodeDist
//     A generic distribution map.
//
//  NaClBitcodeDistElement
//     The implementation of elements in the range of the distribution
//     map.
//
// The code has been written to put the (virtual) logic of
// distribution maps into derived classes of NaClBitcodeDistElement
// whenever possible. This is intentional, in that it keeps all
// knowledge of how to handle/print elements in one class. However,
// because some distributions have external data that is needed by all
// elements, the virtual methods of class NaClBitcodeDist can be
// overridden, and not delegate to NaClBitcodeDistElement.
//
// To do this, an NaClBitcodeDist requires a "sentinel" (derived)
// instance of NaClBitcodeDistElement. This sentinel is used to define
// behaviour needed by distribution maps.
//
// By having control passed to the (derived) instances of
// NaClBitcodeDistElement, it also makes handling nested distributions
// relatively easy. One just extends the methods AddRecord and/or
// AddBlock to also update the corresponding nested distribution.
//
// The exception to this rule is printing, since header information is
// dependent on properties of any possible nested distribution maps
// (for example, we copy column headers after each nested distribution
// map so that it is easier to read the output). To fix this, we let
// virtual method NaClBitcodeDistElement::GetNestedDistributions
// return the array of nested distribution pointers for the nested
// distributions of that map. The order in the array is the order the
// nested distributions will be printed. Typically, this is
// implemented as a field of the distribution element, and is
// initialized to contain the pointers of all nested distributions in
// the element. This field can then be returned from method
// GetNestedDistributions.
//
// Distribution maps are sortable (via method GetDistribution). The
// purpose of sorting is to find interesting elements. This is done by
// sorting the values in the domain of the distribution map, based on
// the GetImportance method of the range element.
//
// Method GetImportance defines how (potentially) interesting the
// value is in the distribution. "Interesting" is based on the notion
// of how likely will the value show a case where adding an
// abbreviation will shrink the size of the corresponding bitcode
// file. For most distributions, the number of instances associated
// with the value is the best measure.
//
// However, for cases where multiple domain entries are created for
// the same NaClBitcodeRecord (i.e. method GetValueList defines more
// than one value), things are not so simple.

// For example, for some distributions (such as value index distributions)
// the numbers of instances isn't sufficient. In such cases, you may
// have to look at nested distributions to find important cases.
//
// In the case of value index distributions, when the size of the
// records is the same, all value indices have the same number of
// instances.  In this case, "interesting" may be measured in terms of
// the (nested) distribution of the values that can appear at that
// index, and how often each value appears.
//
// The larger the importance, the more interesting the value is
// considered, and sorting is based on moving interesting values to
// the front of the sorted list.
//
// When printing distribution maps, the values are sorted based on
// the importance. By default, importance is based on the number of
// times the value appears in records, putting the most used values
// at the top of the printed list.
//
// Since sorting is expensive, the sorted distribution is built once
// and cached.  This cache is flushed whenever the distribution map is
// updated, so that a new sorted distribuition will be generated.
//
// Printing of distribution maps are stylized, so that virtuals can
// easily fill in the necessary data.
//
// For concrete instances of NaClBitcodeDistElement, the following
// virtual method must be defined:
//
//    CreateElement
//       Creates a new instance of the distribution element, to
//       be put into the corresponding distribution map when a new
//       value is added to the distribution map.
//
// In addition, if the distribution element is based on record values,
// the virtual method GetValueList must be defined, to extract values
// out of the bitcode record.

#ifndef LLVM_BITCODE_NACL_NACLBITCODEDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODEDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <vector>

namespace llvm {

/// The domain type of PNaCl bitcode record distribution maps.
typedef uint64_t NaClBitcodeDistValue;

/// Base class of the range type of PNaCl bitcode record distribution
/// maps.
class NaClBitcodeDistElement;

/// Type defining the list of values extracted from the corresponding
/// bitcode record. Typically, the list size is one. However, there
/// are cases where a record defines more than one value (i.e. value
/// indices). Hence, this type defines the more generic API for
/// values.
typedef std::vector<NaClBitcodeDistValue> ValueListType;

typedef ValueListType::const_iterator ValueListIterator;

/// Defines a PNaCl bitcode record distribution map. The distribution
/// map is a map from a (record) value, to the corresponding data
/// associated with that value. Assumes distributions elements are
/// instances of NaClBitcodeDistElement.
class NaClBitcodeDist {
  NaClBitcodeDist(const NaClBitcodeDist&) = delete;
  void operator=(const NaClBitcodeDist&) = delete;
  friend class NaClBitcodeDistElement;

public:
  /// Define kinds for isa, dyn_cast, etc. support (see
  /// llvm/Support/Casting.h). Only defined for concrete classes.
  enum NaClBitcodeDistKind {
    RD_Dist,                 // class NaClBitcodeDist.
      RD_BlockDist,          // class NaClBitcodeBlockDist.
      RD_BlockDistLast,
      RD_CodeDist,           // class NaClBitcodeCodeDist.
      RD_CodeDistLast,
      RD_AbbrevDist,         // class NaClBitcodeAbbrevDist.
      RD_AbbrevDistLast,
      RD_SubblockDist,       // class NaClBlockSubblockDist.
      RD_SubblockDistLast,
      RD_ValueDist,          // class NaClBitcodeValueDist.
      RD_ValueDistLast,
    RD_DistLast
  };

  NaClBitcodeDistKind getKind() const { return Kind; }

private:
  const NaClBitcodeDistKind Kind;

  static bool classof(const NaClBitcodeDist *Dist) {
    return Dist->getKind() >= RD_Dist && Dist->getKind() < RD_DistLast;
  }

public:
  /// Type defining the mapping used to define the distribution.
  typedef std::map<NaClBitcodeDistValue, NaClBitcodeDistElement*> MappedElement;

  typedef MappedElement::const_iterator const_iterator;

  /// Type defining a pair of values used to sort the
  /// distribution. The first element is defined by method
  /// GetImportance, and the second is the distribution value
  /// associated with that importance.
  typedef std::pair<double, NaClBitcodeDistValue> DistPair;

  /// Type defining the sorted list of (domain) values in the
  /// corresponding distribution map.
  typedef std::vector<DistPair> Distribution;

  /// Defines whether blocks or records are stored in the distribution map.
  /// Used to decide if AddRecord/AddBlock methods should fire.
  enum StorageSelector {
    BlockStorage,
    RecordStorage
  };

  NaClBitcodeDist(StorageSelector StorageKind,
                  const NaClBitcodeDistElement *Sentinel,
                  NaClBitcodeDistKind Kind=RD_Dist)
      : Kind(Kind), StorageKind(StorageKind), Sentinel(Sentinel),
        TableMap(), CachedDistribution(0), Total(0) {
  }

  virtual ~NaClBitcodeDist();

  /// Number of elements in the distribution map.
  size_t size() const {
    return TableMap.size();
  }

  /// Iterator at beginning of distribution map.
  const_iterator begin() const {
    return TableMap.begin();
  }

  /// Iterator at end of distribution map.
  const_iterator end() const {
    return TableMap.end();
  }

  /// Returns true if the distribution map is empty.
  bool empty() const {
    return TableMap.empty();
  }

  /// Returns the element associated with the given distribution
  /// value.  Creates the element if needed.
  inline NaClBitcodeDistElement *GetElement(NaClBitcodeDistValue Value);

  /// Returns the element associated with the given distribution
  /// value.
  NaClBitcodeDistElement *at(NaClBitcodeDistValue Value) const {
    return TableMap.at(Value);
  }

  // Creates a new instance of this element for the given value. Used
  // by class NaClBitcodeDist to create instances. Default method
  // simply dispatches to the CreateElement method of the sentinel.
  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const;

  /// Interrogates the block record, and returns the corresponding
  /// values that are being tracked by the distribution map.  Default
  /// method simply dispatches to the GetValueList of the sentinel.
  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const;

  /// Returns the total number of instances held in the distribution
  /// map.
  unsigned GetTotal() const {
    return Total;
  }

  /// Adds the value(s) in the given bitcode record to the
  /// distribution map.  The value(s) based on method GetValueList.
  /// Note: Default requires that GetStorageKind() == RecordStorage.
  /// Override if you want special handling for nested distributions
  /// in a block distribution map.
  virtual void AddRecord(const NaClBitcodeRecord &Record);

  /// Adds the BlockID of the given bitcode block to the distribution
  /// map, if applicable (based on the value of method UseBlockID).
  /// Note: Requires that GetStorageKind() == BlockStorage.
  virtual void AddBlock(const NaClBitcodeBlock &Block);

  /// Builds the distribution associated with the distribution map.
  /// Warning: The distribution is cached, and hence, only valid while
  /// it's contents is not changed.
  const Distribution *GetDistribution() const {
    if (CachedDistribution == 0) Sort();
    return CachedDistribution;
  }

  /// Prints out the contents of the distribution map to Stream.
  void Print(raw_ostream &Stream, const std::string &Indent) const;

  void Print(raw_ostream &Stream) const {
    std::string Indent;
    Print(Stream, Indent);
  }

protected:
  /// If the distribution is cached, remove it. Should be called
  /// whenever the distribution map is changed.
  void RemoveCachedDistribution() const {
    if (CachedDistribution) {
      delete CachedDistribution;
      CachedDistribution = 0;
    }
  }

  /// Sorts the distribution, based on the importance of each element.
  void Sort() const;

private:
  // Defines whether values in distribution map are from blocks or records.
  const StorageSelector StorageKind;
  // Sentinel element used to do generic operations for distribution.
  const NaClBitcodeDistElement *Sentinel;
  // Map from the distribution value to the corresponding distribution
  // element.
  MappedElement TableMap;
  // Pointer to the cached distribution.
  mutable Distribution *CachedDistribution;
  // The total number of instances in the map.
  unsigned Total;
};

/// Defines the element type of a PNaCl bitcode distribution map.
/// This is the base class for all element types used in
/// NaClBitcodeDist.  By default, only the number of instances
/// of the corresponding distribution values is recorded.
class NaClBitcodeDistElement {
  NaClBitcodeDistElement(const NaClBitcodeDistElement &)
      = delete;
  void operator=(const NaClBitcodeDistElement &)
      = delete;

public:
  /// Define kinds for isa, dyn_cast, etc. support. Only defined
  /// for concrete classes.
  enum NaClBitcodeDistElementKind {
    RDE_Dist,                    // class NaClBitcodeDistElement.
      RDE_AbbrevDist,            // class NaClBitcodeAbbrevDistElement.
      RDE_AbbrevDistLast,
      RDE_BitsDist,              // class NaClBitcodeBitsDistElement.
        RDE_BitsAndAbbrevsDist,  // class NaClBitcodeBitsAndAbbrevsDistElement.
          RDE_CodeDist,          // class NaClBitcodeCodeDistElement.
            RDE_CompressCodeDist, // class NaClCompressCodeDistElement.
            RDE_CompressCodeDistLast,
          RDE_CodeDistLast,
        RDE_BitsAndAbbrevsDistLast,
      RDE_BitsDistLast,
      RDE_BlockDist,             // class NaClBitcodeBlockDistElement.
        RDE_NaClAnalBlockDist,   // class NaClAnalyzerBlockDistElement.
        RDE_NaClAnalBlockDistLast,
        RDE_PNaClCompressBlockDist, // class NaClCompressBlockDistElement.
        RDE_PNaClCompressBlockDistLast,
      RDE_BlockDistLast,
      RDE_SizeDist,              // class NaClBitcodeSizeDistElement.
      RDE_SizeDistLast,
      RDE_SubblockDist,          // class NaClBitcodeSubblockDistElement
      RDE_SubblockDistLast,
      RDE_ValueDist,             // class NaClBitcodeValueDistElement.
      RDE_ValueDistLast,
      RDE_ValueIndexDist,        // class NaClBitcodeValueIndexDistElement.
      RDE_ValueIndexDistLast,
    RDE_DistLast
  };

  NaClBitcodeDistElementKind getKind() const { return Kind; }

  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_Dist && Element->getKind() < RDE_DistLast;
  }

private:
  const NaClBitcodeDistElementKind Kind;

public:

  // Constructor to use in derived classes.
  NaClBitcodeDistElement(
      NaClBitcodeDistElementKind Kind=RDE_Dist)
      : Kind(Kind), NumInstances(0)
  {}

  virtual ~NaClBitcodeDistElement();

  // Adds an instance of the given record to this element.
  virtual void AddRecord(const NaClBitcodeRecord &Record);

  // Adds an instance of the given block to this element.
  virtual void AddBlock(const NaClBitcodeBlock &Block);

  // Returns the number of instances associated with this element.
  unsigned GetNumInstances() const {
    return NumInstances;
  }

  // Creates a new instance of this element for the given value. Used
  // by class NaClBitcodeDist to create instances.
  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const = 0;

  /// Interrogates the block record, and returns the corresponding
  /// values that are being tracked by the distribution map. Must be
  /// defined in derived classes.
  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const;

  // Returns the importance of this element. In many cases, it will be
  // the number of instances associated with it. However, it need not
  // be correlated to the number of instance. Used to sort the
  // distribution map, where values with larger importance appear
  // first. Value is the domain value associated with the element in
  // the distribution map.
  virtual double GetImportance(NaClBitcodeDistValue Value) const;

  /// Returns the title to use when printing the title associated
  /// with instances of this distribution element.
  virtual const char *GetTitle() const;

  /// Prints out the title of the distribution map associated with
  /// instances of this distribution element.
  virtual void PrintTitle(raw_ostream &Stream,
                          const NaClBitcodeDist *Distribution) const;

  /// Returns the header to use when printing the value associated
  /// with instances of this distribution element.
  virtual const char *GetValueHeader() const;

  /// Prints out header for row of statistics associated with instances
  /// of this distribution element.
  virtual void PrintStatsHeader(raw_ostream &Stream) const;

  /// Prints out the header to the printed distribution map associated
  /// with instances of this distribution element.
  void PrintHeader(raw_ostream &Stream) const;

  /// Prints out statistics for the row with the given value.
  virtual void PrintRowStats(raw_ostream &Stream,
                             const NaClBitcodeDist *Distribution) const;

  /// Prints out Value (in a row) to Stream.
  virtual void PrintRowValue(raw_ostream &Stream,
                             NaClBitcodeDistValue Value,
                             const NaClBitcodeDist *Distribution) const;

  /// Prints out a row in the printed distribution map.
  virtual void PrintRow(raw_ostream &Stream,
                        NaClBitcodeDistValue Value,
                        const NaClBitcodeDist *Distribution) const;

  /// Returns a pointer to the list of nested distributions that
  /// should be printed when this element is printed. Return 0 if no
  /// nested distributions should be printed.
  virtual const SmallVectorImpl<NaClBitcodeDist*> *
  GetNestedDistributions() const;

  /// Prints out any nested distributions, if defined for the element.
  /// Returns true if a nested distribution was printed.
  bool PrintNestedDistIfApplicable(
      raw_ostream &Stream, const std::string &Indent) const;

private:
  // The number of instances associated with this element.
  unsigned NumInstances;
};

inline NaClBitcodeDistElement *NaClBitcodeDist::
GetElement(NaClBitcodeDistValue Value) {
  if (TableMap.find(Value) == TableMap.end()) {
    TableMap[Value] = CreateElement(Value);
  }
  return TableMap[Value];
}

}

#endif
