//===- NaClBitcodeHeader.cpp ----------------------------------------------===//
//     PNaCl bitcode header reader.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implementation of Bitcode abbrevations.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitCodes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

void NaClBitCodeAbbrevOp::Print(raw_ostream& Stream) const {
  if (isLiteral()) {
    Stream << getLiteralValue();
  } else if (isEncoding()) {
    switch (getEncoding()) {
    case Fixed:
      Stream << "Fixed(" << getEncodingData() << ")";
      break;
    case VBR:
      Stream << "VBR(" << getEncodingData() << ")";
      break;
    case Array:
      Stream << "Array";
      break;
    case Char6:
      Stream << "Char6";
      break;
    case Blob:
      Stream << "Blob";
      break;
    default:
      llvm_unreachable("Unknown bitcode abbreviation operator");
      Stream << "??";  // In case asserts are turned off.
      break;
    }
  } else {
    llvm_unreachable("Unknown bitcode abbreviation operator");
    Stream << "??";  // In case asserts are turned off.
  }
}

static void PrintExpression(raw_ostream &Stream,
                            const NaClBitCodeAbbrev *Abbrev,
                            unsigned &Index) {
  // Bail out early, in case we are incrementally building the
  // expression and the argument is not available yet.
  if (Index >= Abbrev->getNumOperandInfos()) return;

  const NaClBitCodeAbbrevOp &Op = Abbrev->getOperandInfo(Index);
  Op.Print(Stream);
  if (unsigned NumArgs = Op.NumArguments()) {
    Stream << "(";
    for (unsigned i = 0; i < NumArgs; ++i) {
      ++Index;
      if (i > 0) Stream << ",";
      PrintExpression(Stream, Abbrev, Index);
    }
    Stream << ")";
  }
}

void NaClBitCodeAbbrev::Print(raw_ostream &Stream, bool AddNewLine) const {
  Stream << "[";
  for (unsigned i = 0; i < getNumOperandInfos(); ++i) {
    if (i > 0) Stream << ", ";
    PrintExpression(Stream, this, i);
  }
  Stream << "]";
  if (AddNewLine) Stream << "\n";
}

NaClBitCodeAbbrev *NaClBitCodeAbbrev::Simplify() const {
  NaClBitCodeAbbrev *Abbrev = new NaClBitCodeAbbrev();
  for (unsigned i = 0; i < OperandList.size(); ++i) {
    const NaClBitCodeAbbrevOp &Op = OperandList[i];
    // Simplify if possible.  Currently, the only simplification known
    // is to remove unnecessary operands appearing immediately before an
    // array operator. That is, apply the simplification:
    //    Op Array(Op) -> Array(Op)
    assert(!Op.isArrayOp() || i == OperandList.size()-2);
    while (Op.isArrayOp() && !Abbrev->OperandList.empty() &&
           Abbrev->OperandList.back() == OperandList[i+1]) {
      Abbrev->OperandList.pop_back();
    }
    Abbrev->OperandList.push_back(Op);
  }
  return Abbrev;
}

bool NaClBitCodeAbbrev::isValid() const {
  // Verify that an array op appears can only appear if it is the
  // second to last element.
  unsigned NumOperands = getNumOperandInfos();
  for (unsigned i = 0; i < NumOperands; ++i) {
    const NaClBitCodeAbbrevOp &Op = getOperandInfo(i);
    if (Op.isArrayOp() && i + 2 != NumOperands)
      return false;
  }
  return true;
}
