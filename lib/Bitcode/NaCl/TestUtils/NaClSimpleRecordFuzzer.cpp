//===- NaClSimpleRecordFuzzer.cpp - Simple fuzzer of bitcode --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a simple fuzzer for a list of PNaCl bitcode records.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClFuzz.h"
#include "llvm/Support/Format.h"

#include <set>

using namespace llvm;
using namespace naclfuzz;

namespace {

// Counts the number of times each value in a range [0..N) is used (based
// on the number of calls to method increment()).
class DistCounter {
  DistCounter(const DistCounter&) = delete;
  void operator=(const DistCounter&) = delete;
public:
  explicit DistCounter(size_t DistSize)
      : Dist(DistSize, 0), Total(0) {}

  // Increments the count for the given value
  size_t increment(size_t Value) {
    ++Dist[Value];
    ++Total;
    return Value;
  }

  // Returns the end of the range being checked.
  size_t size() const {
    return Dist.size();
  }

  // Returns the number of times value was incremented.
  size_t operator[](size_t Value) const {
    return Dist[Value];
  }

  // Retrns the number of times any value in the distribution was
  // incremented.
  size_t getTotal() const {
    return Total;
  }
private:
  // The number of times each value was incremented.
  std::vector<size_t> Dist;
  // The total number of times any value was incremented.
  size_t Total;
};

// Model weights when randomly choosing values.
typedef unsigned WeightType;

// Associates weights with values. Used to generate weighted
// distributions (see class WeightedDistribution below).
template<typename T>
struct WeightedValue {
  T Value;
  WeightType Weight;
};

// Weighted distribution for a set of values in [0..DistSize).
template<typename T>
class WeightedDistribution {
  WeightedDistribution(const WeightedDistribution&) = delete;
  void operator=(const WeightedDistribution&) = delete;
public:
  typedef const WeightedValue<T> *const_iterator;

  WeightedDistribution(const WeightedValue<T> Dist[],
                       size_t DistSize,
                       RandomNumberGenerator &Generator)
      : Dist(Dist), DistSize(DistSize), TotalWeight(0), Generator(Generator) {
    for (size_t i = 0; i < DistSize; ++i)
      TotalWeight += Dist[i].Weight;
  }

  virtual ~WeightedDistribution() {}

  /// Returns the number of weighted values in the distribution.
  size_t size() const {
    return DistSize;
  }

  /// Returns const iterator at beginning of weighted distribution.
  const_iterator begin() const {
    return Dist;
  }

  /// Returns const iterator at end of weighted distribution.
  const_iterator end() const {
    return Dist + DistSize;
  }

  /// Randomly chooses a weighted value in the distribution, based on
  /// the weights of the distrubution.
  virtual const WeightedValue<T> &choose() {
    return Dist[chooseIndex()];
  }

  /// Returns the total weight associated with the distribution.
  size_t getTotalWeight() const {
    return TotalWeight;
  }

protected:
  // The weighted values defining the distribution.
  const WeightedValue<T> *Dist;
  // The number of weighed values.
  size_t DistSize;
  // The total weight of the distribution (sum of weights in weighted values).
  size_t TotalWeight;
  // The random number generator to use.
  RandomNumberGenerator &Generator;

  // Randomly chooses an index of a weighed value in the distribution,
  // based on the weights of the distribution.
  size_t chooseIndex() {
    /// TODO(kschimpf) Speed this up?
    WeightType WeightedSum = Generator.chooseInRange(TotalWeight);
    assert(WeightedSum < TotalWeight);
    for (size_t Choice = 0; Choice < DistSize; ++Choice) {
      WeightType NextWeight = Dist[Choice].Weight;
      if (WeightedSum < NextWeight)
        return Choice;
      WeightedSum -= NextWeight;
    }
    llvm_unreachable("no index for WeightedDistribution.chooseIndex()");
  }
};

// Defines a range [min..max].
template<typename T>
struct RangeType {
  T min;
  T max;
};

// Weighted distribution of a set of value ranges.
template<typename T>
class WeightedRangeDistribution : public WeightedDistribution<RangeType<T>> {
  WeightedRangeDistribution(const WeightedRangeDistribution<T>&) = delete;
  void operator=(const WeightedRangeDistribution<T>&) = delete;
public:
  WeightedRangeDistribution(const WeightedValue<RangeType<T>> Dist[],
                            size_t DistSize,
                            RandomNumberGenerator &Generator)
      : WeightedDistribution<RangeType<T>>(Dist, DistSize, Generator) {}

  // Choose a value from one of the value ranges.
  T chooseValue() {
    const RangeType<T> Range = this->choose().Value;
    return Range.min +
        this->Generator.chooseInRange(Range.max - Range.min + 1);
  }
};

/// Weighted distribution with a counter, capturing the distribution
/// of random choices for each weighted value.
template<typename T>
class CountedWeightedDistribution : public WeightedDistribution<T> {
  CountedWeightedDistribution(const CountedWeightedDistribution&) = delete;
  void operator=(const CountedWeightedDistribution&) = delete;
 public:
  CountedWeightedDistribution(const WeightedValue<T> Dist[],
                              size_t DistSize,
                              RandomNumberGenerator &Generator)
      : WeightedDistribution<T>(Dist, DistSize, Generator), Counter(DistSize) {}

  const WeightedValue<T> &choose() final {
    return this->Dist[Counter.increment(
        WeightedDistribution<T>::chooseIndex())];
  }

  /// Returns the number of times the Index-th weighted value was
  /// chosen.
  size_t getChooseCount(size_t Index) const {
    return Counter[Index];
  }

  /// Returns the total number of times method choose was called.
  size_t getTotalChooseCount() const {
    return Counter.getTotal();
  }

private:
  DistCounter Counter;
};

#define ARRAY(array) array, array_lengthof(array)

// Weighted distribution used to select edit actions.
const WeightedValue<RecordFuzzer::EditAction> ActionDist[] = {
  {RecordFuzzer::InsertRecord, 3},
  {RecordFuzzer::MutateRecord, 5},
  {RecordFuzzer::RemoveRecord, 1},
  {RecordFuzzer::ReplaceRecord, 1},
  {RecordFuzzer::SwapRecord, 1}
};

// Type of values in bitcode records.
typedef uint64_t ValueType;

// Weighted ranges for non-negative values in records.
const WeightedValue<RangeType<ValueType>> PosValueDist[] = {
  {{0, 6}, 100},
  {{7, 20}, 50},
  {{21, 40}, 10},
  {{41, 100}, 2},
  {{101, 4096}, 1}
};

// Distribution used to decide when use negative values in records.
WeightedValue<bool> NegValueDist[] = {
  {true, 1},   // i.e. make value negative.
  {false, 100} // i.e. leave value positive.
};

// Range distribution for records sizes (must be greater than 0).
const WeightedValue<RangeType<size_t>> RecordSizeDist[] = {
  {{1, 3}, 1000},
  {{4, 7},  100},
  {{7, 100},  1}
};

// Defines valid record codes.
typedef unsigned RecordCodeType;

// Special code to signify adding random other record codes.
static const RecordCodeType OtherRecordCode = 575757575;

// List of record codes we can generate. The weights are based
// on record counts in pnacl-llc.pexe, using how many thousand of
// each record code appeared (or 1 if less than 1 thousand).
WeightedValue<RecordCodeType> RecordCodeDist[] = {
  {naclbitc::BLOCKINFO_CODE_SETBID, 1},
  {naclbitc::MODULE_CODE_VERSION, 1},
  {naclbitc::MODULE_CODE_FUNCTION, 7},
  {naclbitc::TYPE_CODE_NUMENTRY, 1},
  {naclbitc::TYPE_CODE_VOID, 1},
  {naclbitc::TYPE_CODE_FLOAT, 1},
  {naclbitc::TYPE_CODE_DOUBLE, 1},
  {naclbitc::TYPE_CODE_INTEGER, 1},
  {naclbitc::TYPE_CODE_VECTOR, 1},
  {naclbitc::TYPE_CODE_FUNCTION, 1},
  {naclbitc::VST_CODE_ENTRY, 1},
  {naclbitc::VST_CODE_BBENTRY, 1},
  {naclbitc::CST_CODE_SETTYPE, 15},
  {naclbitc::CST_CODE_UNDEF, 1},
  {naclbitc::CST_CODE_INTEGER, 115},
  {naclbitc::CST_CODE_FLOAT, 1},
  {naclbitc::GLOBALVAR_VAR, 14},
  {naclbitc::GLOBALVAR_COMPOUND, 1},
  {naclbitc::GLOBALVAR_ZEROFILL, 2},
  {naclbitc::GLOBALVAR_DATA, 18},
  {naclbitc::GLOBALVAR_RELOC, 20},
  {naclbitc::GLOBALVAR_COUNT, 1},
  {naclbitc::FUNC_CODE_DECLAREBLOCKS, 6},
  {naclbitc::FUNC_CODE_INST_BINOP, 402},
  {naclbitc::FUNC_CODE_INST_CAST, 61},
  {naclbitc::FUNC_CODE_INST_EXTRACTELT, 1},
  {naclbitc::FUNC_CODE_INST_INSERTELT, 1},
  {naclbitc::FUNC_CODE_INST_RET, 7},
  {naclbitc::FUNC_CODE_INST_BR, 223},
  {naclbitc::FUNC_CODE_INST_SWITCH, 7},
  {naclbitc::FUNC_CODE_INST_UNREACHABLE, 1},
  {naclbitc::FUNC_CODE_INST_PHI, 84},
  {naclbitc::FUNC_CODE_INST_ALLOCA, 34},
  {naclbitc::FUNC_CODE_INST_LOAD, 225},
  {naclbitc::FUNC_CODE_INST_STORE, 461},
  {naclbitc::FUNC_CODE_INST_CMP2, 140},
  {naclbitc::FUNC_CODE_INST_VSELECT, 10},
  {naclbitc::FUNC_CODE_INST_CALL, 80},
  {naclbitc::FUNC_CODE_INST_FORWARDTYPEREF, 36},
  {naclbitc::FUNC_CODE_INST_CALL_INDIRECT, 5},
  {naclbitc::BLK_CODE_ENTER, 1},
  {naclbitc::BLK_CODE_EXIT, 1},
  {naclbitc::BLK_CODE_DEFINE_ABBREV, 1},
  {OtherRecordCode, 1}
};

// *Warning* The current implementation does not work on empty bitcode
// record lists.
class SimpleRecordFuzzer : public RecordFuzzer {
public:
  SimpleRecordFuzzer(NaClMungedBitcode &Bitcode,
                     RandomNumberGenerator &Generator)
      : RecordFuzzer(Bitcode, Generator),
        RecordCounter(Bitcode.getBaseRecords().size()),
        ActionWeight(ARRAY(ActionDist), Generator),
        RecordSizeWeight(ARRAY(RecordSizeDist), Generator),
        PosValueWeight(ARRAY(PosValueDist), Generator),
        NegValueWeight(ARRAY(NegValueDist), Generator),
        RecordCodeWeight(ARRAY(RecordCodeDist), Generator) {

    assert(!Bitcode.getBaseRecords().empty()
           && "Can't fuzz empty list of records");

    for (const auto &RecordCode : RecordCodeWeight)
      UsedRecordCodes.insert(RecordCode.Value);
  }

  ~SimpleRecordFuzzer() final;

  bool fuzz(unsigned Count, unsigned Base) final;

  void showRecordDistribution(raw_ostream &Out) const final;

  void showEditDistribution(raw_ostream &Out) const final;

private:
  // Count how many edits are applied to each record in the bitcode.
  DistCounter RecordCounter;
  // Distribution used to randomly choose edit actions.
  CountedWeightedDistribution<RecordFuzzer::EditAction> ActionWeight;
  // Distribution used to randomly choose the size of created records.
  WeightedRangeDistribution<size_t> RecordSizeWeight;
  // Distribution used to randomly choose positive values for records.
  WeightedRangeDistribution<ValueType> PosValueWeight;
  // Distribution of value sign used to randomly choose value for records.
  WeightedDistribution<bool> NegValueWeight;
  // Distribution used to choose record codes for records.
  WeightedDistribution<RecordCodeType> RecordCodeWeight;
  // Filter to make sure the "other" choice for record codes will not
  // choose any other record code in RecordCodeWeight.
  std::set<size_t> UsedRecordCodes;

  // Randomly choose an edit action.
  RecordFuzzer::EditAction chooseAction() {
    return ActionWeight.choose().Value;
  }

  // Randomly choose a record index from the list of records to edit.
  size_t chooseRecordIndex() {
    return RecordCounter.increment(
        Generator.chooseInRange(Bitcode.getBaseRecords().size()));
  }

  // Randomly choose a record code.
  RecordCodeType chooseRecordCode() {
    RecordCodeType Code = RecordCodeWeight.choose().Value;
    if (Code != OtherRecordCode)
      return Code;
    Code = Generator.chooseInRange(UINT_MAX);
    while (UsedRecordCodes.count(Code)) // don't use predefined values.
      ++Code;
    return Code;
  }

  // Randomly choose a positive value for use in a record.
  ValueType choosePositiveValue() {
    return PosValueWeight.chooseValue();
  }

  // Randomly choose a positive/negative value for use in a record.
  ValueType chooseValue() {
    ValueType Value = choosePositiveValue();
    if (NegValueWeight.choose().Value) {
      // Use two's complement negation.
      Value = ~Value + 1;
    }
    return Value;
  }

  // Randomly fill in a record with record values.
  void chooseRecordValues(NaClBitcodeAbbrevRecord &Record) {
    Record.Abbrev = naclbitc::UNABBREV_RECORD;
    Record.Code = chooseRecordCode();
    Record.Values.clear();
    size_t NumValues = RecordSizeWeight.chooseValue();
    for (size_t i = 0; i < NumValues; ++i) {
      Record.Values.push_back(chooseValue());
    }
  }

  // Apply the given edit action to a random record.
  void applyAction(EditAction Action);

  // Randomly mutate a record.
  void mutateRecord(NaClBitcodeAbbrevRecord &Record) {
    // TODO(kschimpf) Do something smarter than just changing a value
    // in the record.
    size_t Index = Generator.chooseInRange(Record.Values.size() + 1);
    if (Index == 0)
      Record.Code = chooseRecordCode();
    else
      Record.Values[Index - 1] = chooseValue();
  }
};

SimpleRecordFuzzer::~SimpleRecordFuzzer() {}

bool SimpleRecordFuzzer::fuzz(unsigned Count, unsigned Base) {
  // TODO(kschimpf): Add some randomness in the number of actions selected.
  clear();
  size_t NumRecords = Bitcode.getBaseRecords().size();
  size_t NumActions = NumRecords * Count / Base;
  if (NumActions == 0)
    NumActions = 1;
  for (size_t i = 0; i < NumActions; ++i)
    applyAction(chooseAction());
  return true;
}

void SimpleRecordFuzzer::applyAction(EditAction Action) {
  size_t Index = chooseRecordIndex();
  switch(Action) {
  case InsertRecord: {
    NaClBitcodeAbbrevRecord Record;
    chooseRecordValues(Record);
    if (Generator.chooseInRange(2))
      Bitcode.addBefore(Index, Record);
    else
      Bitcode.addAfter(Index, Record);
    return;
  }
  case RemoveRecord:
    Bitcode.remove(Index);
    return;
  case ReplaceRecord: {
    NaClBitcodeAbbrevRecord Record;
    chooseRecordValues(Record);
    Bitcode.replace(Index, Record);
    return;
  }
  case MutateRecord: {
    NaClBitcodeAbbrevRecord Copy(*Bitcode.getBaseRecords()[Index]);
    mutateRecord(Copy);
    Bitcode.replace(Index, Copy);
    return;
  }
  case SwapRecord: {
    size_t Index2 = chooseRecordIndex();
    Bitcode.replace(Index, *Bitcode.getBaseRecords()[Index2]);
    Bitcode.replace(Index2, *Bitcode.getBaseRecords()[Index]);
    return;
  }
  }
}

// Returns corresponding percentage defined by Count/Total, in a form
// that can be printed to a raw_ostream.
format_object<float> percentage(size_t Count, size_t Total) {
  float percent = Total == 0.0 ? 0.0 : 100.0 * Count / Total;
  return format("%1.0f", nearbyintf(percent));
}

void SimpleRecordFuzzer::showRecordDistribution(raw_ostream &Out) const {
  Out << "Edit Record Distribution (Total: " << RecordCounter.getTotal()
      << "):\n";
  size_t Total = RecordCounter.getTotal();
  for (size_t i = 0; i < Bitcode.getBaseRecords().size(); ++i) {
    size_t Count = RecordCounter[i];
    Out << "  " << format("%zd", i) << ": "
        << Count << " (" << percentage(Count, Total) << "%)\n";
  }
}

void SimpleRecordFuzzer::showEditDistribution(raw_ostream &Out) const {
  size_t TotalWeight = ActionWeight.getTotalWeight();
  size_t TotalCount = ActionWeight.getTotalChooseCount();
  Out << "Edit Action Distribution(Total: " << TotalCount << "):\n";
  size_t ActionIndex = 0;
  for (const auto &Action : ActionWeight) {
    size_t ActionCount = ActionWeight.getChooseCount(ActionIndex);
    Out << "  " << actionName(Action.Value) <<  " - Wanted: "
        << percentage(Action.Weight, TotalWeight)  << "%, Applied: "
        << ActionCount  << " (" << percentage(ActionCount, TotalCount)
        << "%)\n";
    ++ActionIndex;
  }
}

} // end of anonymous namespace

namespace naclfuzz {

RecordFuzzer *RecordFuzzer::createSimpleRecordFuzzer(
    NaClMungedBitcode &Bitcode,
    RandomNumberGenerator &Generator) {
  return new SimpleRecordFuzzer(Bitcode, Generator);
}

} // end of namespace naclfuzz
