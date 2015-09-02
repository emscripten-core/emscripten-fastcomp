//===- NaClBitstreamWriter.h - NaCl bitstream writer ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This header defines the BitstreamWriter class.  This class can be used to
// write an arbitrary bitstream, regardless of its contents.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLBITSTREAMWRITER_H
#define LLVM_BITCODE_NACL_NACLBITSTREAMWRITER_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/NaCl/NaClBitCodes.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include <vector>

namespace llvm {

class NaClBitstreamWriter {
  SmallVectorImpl<char> &Out;

  /// CurBit - Always between 0 and 31 inclusive, specifies the next bit to use.
  unsigned CurBit;

  /// CurValue - The current value.  Only bits < CurBit are valid.
  uint32_t CurValue;

  /// CurCodeSize - This is the declared size of code values used for the
  /// current block, in bits.
  NaClBitcodeSelectorAbbrev CurCodeSize;

  /// BlockInfoCurBID - When emitting a BLOCKINFO_BLOCK, this is the currently
  /// selected BLOCK ID.
  unsigned BlockInfoCurBID;

  /// CurAbbrevs - Abbrevs installed at in this block.
  std::vector<NaClBitCodeAbbrev*> CurAbbrevs;

  struct Block {
    const NaClBitcodeSelectorAbbrev PrevCodeSize;
    const unsigned StartSizeWord;
    std::vector<NaClBitCodeAbbrev*> PrevAbbrevs;
    const unsigned AbbreviationIndexLimit;
    Block(const NaClBitcodeSelectorAbbrev& PCS, unsigned SSW,
          unsigned AbbreviationIndexLimit)
        : PrevCodeSize(PCS), StartSizeWord(SSW),
          AbbreviationIndexLimit(AbbreviationIndexLimit) {}
  };

  /// BlockScope - This tracks the current blocks that we have entered.
  std::vector<Block> BlockScope;

  /// BlockInfo - This contains information emitted to BLOCKINFO_BLOCK blocks.
  /// These describe abbreviations that all blocks of the specified ID inherit.
  struct BlockInfo {
    unsigned BlockID;
    std::vector<NaClBitCodeAbbrev*> Abbrevs;
  };
  std::vector<BlockInfo> BlockInfoRecords;

  // True if filler should be added to byte align records.
  bool AlignBitcodeRecords = false;

  /// AbbrevValues - Wrapper class that allows the bitstream writer to
  /// prefix a code to the set of values, associated with a record to
  /// emit, without having to destructively change the contents of
  /// values.
  template<typename uintty>
  struct AbbrevValues {
    AbbrevValues(uintty Code, const SmallVectorImpl<uintty> &Values)
        : Code(Code), Values(Values) {}

    size_t size() const {
      return Values.size() + 1;
    }

    uintty operator[](size_t Index) const {
      return Index == 0 ? Code : Values[Index-1];
    }

  private:
    // The code to use (if not DONT_USE_CODE).
    uintty Code;
    const SmallVectorImpl<uintty> &Values;
  };

public:
  // BackpatchWord - Backpatch a 32-bit word in the output with the specified
  // value.
  void BackpatchWord(unsigned ByteNo, unsigned NewWord) {
    Out[ByteNo++] = (unsigned char)(NewWord >>  0);
    Out[ByteNo++] = (unsigned char)(NewWord >>  8);
    Out[ByteNo++] = (unsigned char)(NewWord >> 16);
    Out[ByteNo  ] = (unsigned char)(NewWord >> 24);
  }

private:
  void WriteByte(unsigned char Value) {
    Out.push_back(Value);
  }

  void WriteWord(unsigned Value) {
    unsigned char Bytes[4] = {
      (unsigned char)(Value >>  0),
      (unsigned char)(Value >>  8),
      (unsigned char)(Value >> 16),
      (unsigned char)(Value >> 24) };
    Out.append(&Bytes[0], &Bytes[4]);
  }

  unsigned GetBufferOffset() const {
    return Out.size();
  }

  unsigned GetWordIndex() const {
    unsigned Offset = GetBufferOffset();
    assert((Offset & 3) == 0 && "Not 32-bit aligned");
    return Offset / 4;
  }

public:
  explicit NaClBitstreamWriter(SmallVectorImpl<char> &O)
      : Out(O), CurBit(0), CurValue(0), CurCodeSize() {}

  ~NaClBitstreamWriter() {
    assert(CurBit == 0 && "Unflushed data remaining");
    assert(BlockScope.empty() && CurAbbrevs.empty() && "Block imbalance");

    // Free the BlockInfoRecords.
    while (!BlockInfoRecords.empty()) {
      BlockInfo &Info = BlockInfoRecords.back();
      // Free blockinfo abbrev info.
      for (unsigned i = 0, e = static_cast<unsigned>(Info.Abbrevs.size());
           i != e; ++i)
        Info.Abbrevs[i]->dropRef();
      BlockInfoRecords.pop_back();
    }
  }

  void initFromHeader(const NaClBitcodeHeader &Header) {
    AlignBitcodeRecords = Header.getAlignBitcodeRecords();
  }

  /// \brief Retrieve the current position in the stream, in bits.
  uint64_t GetCurrentBitNo() const { return GetBufferOffset() * 8 + CurBit; }

  /// \brief Returns the maximum abbreviation index allowed for the
  /// current block.
  size_t getMaxCurAbbrevIndex() const {
    return CurAbbrevs.size() + naclbitc::DEFAULT_MAX_ABBREV;
  }

  //===--------------------------------------------------------------------===//
  // Basic Primitives for emitting bits to the stream.
  //===--------------------------------------------------------------------===//

  // Max Number of bits that can be written using Emit.
  static const unsigned MaxEmitNumBits = 32;

  void Emit(uint32_t Val, unsigned NumBits) {
    assert(NumBits && NumBits <= MaxEmitNumBits && "Invalid value size!");
    assert((Val &
            ~(~0U >> (MaxEmitNumBits-NumBits))) == 0 && "High bits set!");
    CurValue |= Val << CurBit;
    if (CurBit + NumBits < MaxEmitNumBits) {
      CurBit += NumBits;
      return;
    }

    // Add the current word.
    WriteWord(CurValue);

    if (CurBit)
      CurValue = Val >> (MaxEmitNumBits-CurBit);
    else
      CurValue = 0;
    CurBit = (CurBit+NumBits) & (MaxEmitNumBits-1);
  }

  void Emit64(uint64_t Val, unsigned NumBits) {
    while (NumBits > MaxEmitNumBits) {
      Emit((uint32_t)Val, MaxEmitNumBits);
      Val >>= MaxEmitNumBits;
      NumBits -= MaxEmitNumBits;
    }
    Emit((uint32_t)Val, NumBits);
  }

  void flushToByte() {
    unsigned BitsToFlush = (32 - CurBit) % CHAR_BIT;
    if (BitsToFlush)
      Emit(0, BitsToFlush);
  }

  void flushToByteIfAligned() {
    if (AlignBitcodeRecords)
      flushToByte();
  }

  void FlushToWord() {
    if (CurBit) {
      WriteWord(CurValue);
      CurBit = 0;
      CurValue = 0;
    }
  }

  void EmitVBR(uint32_t Val, unsigned NumBits) {
    assert(NumBits <= 32 && "Too many bits to emit!");
    assert(NumBits > 1 && "Too few bits to emit!");
    uint32_t Threshold = 1U << (NumBits-1);

    // Emit the bits with VBR encoding, NumBits-1 bits at a time.
    while (Val >= Threshold) {
      Emit((Val & ((1 << (NumBits-1))-1)) | (1 << (NumBits-1)), NumBits);
      Val >>= NumBits-1;
    }

    Emit(Val, NumBits);
  }

  void EmitVBR64(uint64_t Val, unsigned NumBits) {
    assert(NumBits <= 32 && "Too many bits to emit!");
    assert(NumBits > 1 && "Too few bits to emit!");
    if ((uint32_t)Val == Val)
      return EmitVBR((uint32_t)Val, NumBits);

    uint32_t Threshold = 1U << (NumBits-1);

    // Emit the bits with VBR encoding, NumBits-1 bits at a time.
    while (Val >= Threshold) {
      Emit(((uint32_t)Val & ((1 << (NumBits-1))-1)) |
           (1 << (NumBits-1)), NumBits);
      Val >>= NumBits-1;
    }

    Emit((uint32_t)Val, NumBits);
  }

  /// EmitCode - Emit the specified code.
  void EmitCode(unsigned Val) {
    if (CurCodeSize.IsFixed)
      Emit(Val, CurCodeSize.NumBits);
    else
      EmitVBR(Val, CurCodeSize.NumBits);
  }

  //===--------------------------------------------------------------------===//
  // Block Manipulation
  //===--------------------------------------------------------------------===//

  /// getBlockInfo - If there is block info for the specified ID, return it,
  /// otherwise return null.
  BlockInfo *getBlockInfo(unsigned BlockID) {
    // Common case, the most recent entry matches BlockID.
    if (!BlockInfoRecords.empty() && BlockInfoRecords.back().BlockID == BlockID)
      return &BlockInfoRecords.back();

    for (unsigned i = 0, e = static_cast<unsigned>(BlockInfoRecords.size());
         i != e; ++i)
      if (BlockInfoRecords[i].BlockID == BlockID)
        return &BlockInfoRecords[i];
    return 0;
  }

private:
  // Enter block using CodeLen bits to read the size of the code
  // selector associated with the block.
  void EnterSubblock(unsigned BlockID,
                     const NaClBitcodeSelectorAbbrev& CodeLen,
                     BlockInfo *Info) {
    // Block header:
    //    [ENTER_SUBBLOCK, blockid, newcodelen, <align4bytes>, blocklen]
    EmitCode(naclbitc::ENTER_SUBBLOCK);
    EmitVBR(BlockID, naclbitc::BlockIDWidth);
    assert(CodeLen.IsFixed && "Block codelens must be fixed");
    EmitVBR(CodeLen.NumBits, naclbitc::CodeLenWidth);
    FlushToWord();

    unsigned BlockSizeWordIndex = GetWordIndex();
    NaClBitcodeSelectorAbbrev OldCodeSize(CurCodeSize);

    // Emit a placeholder, which will be replaced when the block is popped.
    Emit(0, naclbitc::BlockSizeWidth);

    CurCodeSize = CodeLen;

    // Push the outer block's abbrev set onto the stack, start out with an
    // empty abbrev set.
    BlockScope.push_back(Block(OldCodeSize, BlockSizeWordIndex,
                               1 << CodeLen.NumBits));
    BlockScope.back().PrevAbbrevs.swap(CurAbbrevs);

    // If there is a blockinfo for this BlockID, add all the predefined abbrevs
    // to the abbrev list.
    if (Info) {
      for (unsigned i = 0, e = static_cast<unsigned>(Info->Abbrevs.size());
           i != e; ++i) {
        CurAbbrevs.push_back(Info->Abbrevs[i]);
        Info->Abbrevs[i]->addRef();
      }
    }
  }

public:
  /// \brief Enter block using CodeLen bits to read the size of the code
  /// selector associated with the block.
  void EnterSubblock(unsigned BlockID,
                     const NaClBitcodeSelectorAbbrev& CodeLen) {
    EnterSubblock(BlockID, CodeLen, getBlockInfo(BlockID));
  }

  /// \brief Enter block, using a code length based on the number of
  /// (global) BlockInfo entries defined for the block. Note: This
  /// should be used only if the block doesn't define any local abbreviations.
  void EnterSubblock(unsigned BlockID) {
    BlockInfo *Info = getBlockInfo(BlockID);
    size_t NumAbbrevs = Info ? Info->Abbrevs.size() : 0;
    NaClBitcodeSelectorAbbrev DefaultCodeLen(
        naclbitc::DEFAULT_MAX_ABBREV+NumAbbrevs);
    EnterSubblock(BlockID, DefaultCodeLen, Info);
  }

  /// \brief Enter block with the given number of abbreviations.
  void EnterSubblock(unsigned BlockID, unsigned NumAbbrev) {
    NaClBitcodeSelectorAbbrev CodeLenAbbrev(NumAbbrev);
    EnterSubblock(BlockID, CodeLenAbbrev);
  }

  void ExitBlock() {
    assert(!BlockScope.empty() && "Block scope imbalance!");

    // Delete all abbrevs.
    for (unsigned i = 0, e = static_cast<unsigned>(CurAbbrevs.size());
         i != e; ++i)
      CurAbbrevs[i]->dropRef();

    const Block &B = BlockScope.back();

    // Block tail:
    //    [END_BLOCK, <align4bytes>]
    EmitCode(naclbitc::END_BLOCK);
    FlushToWord();

    // Compute the size of the block, in words, not counting the size field.
    unsigned SizeInWords = GetWordIndex() - B.StartSizeWord - 1;
    unsigned ByteNo = B.StartSizeWord*4;

    // Update the block size field in the header of this sub-block.
    BackpatchWord(ByteNo, SizeInWords);

    // Restore the inner block's code size and abbrev table.
    CurCodeSize = B.PrevCodeSize;
    BlockScope.back().PrevAbbrevs.swap(CurAbbrevs);
    BlockScope.pop_back();
  }

  //===--------------------------------------------------------------------===//
  // Record Emission
  //===--------------------------------------------------------------------===//

private:
  /// EmitAbbreviatedField - Emit a single scalar field value with the specified
  /// encoding.
  template<typename uintty>
  void EmitAbbreviatedField(const NaClBitCodeAbbrevOp &Op, uintty V) {
    switch (Op.getEncoding()) {
    case NaClBitCodeAbbrevOp::Literal:
      // This is a no-op, since the abbrev specifies the literal to use.
      assert(V == Op.getValue() && "Invalid abbrev for record!");
      break;
    case NaClBitCodeAbbrevOp::Fixed:
      if (Op.getValue())
        Emit((unsigned)V, (unsigned)Op.getValue());
      break;
    case NaClBitCodeAbbrevOp::VBR:
      if (Op.getValue())
        EmitVBR64(V, (unsigned)Op.getValue());
      break;
    case NaClBitCodeAbbrevOp::Array:
      report_fatal_error("Not to be used with array abbreviation op!");
    case NaClBitCodeAbbrevOp::Char6:
      Emit(NaClBitCodeAbbrevOp::EncodeChar6((char)V), 6);
      break;
    }
  }

  /// EmitRecordWithAbbrevImpl - This is the core implementation of the record
  /// emission code.
  template<typename uintty>
  void EmitRecordWithAbbrevImpl(unsigned Abbrev,
                                const AbbrevValues<uintty> &Vals) {
    const NaClBitCodeAbbrev *Abbv = getAbbreviation(Abbrev);
    assert(Abbv && "Abbreviation index is invalid");

    EmitCode(Abbrev);

    unsigned RecordIdx = 0;
    for (unsigned i = 0, e = static_cast<unsigned>(Abbv->getNumOperandInfos());
         i != e; ++i) {
      const NaClBitCodeAbbrevOp &Op = Abbv->getOperandInfo(i);
      if (Op.getEncoding() == NaClBitCodeAbbrevOp::Array) {
        // Array case.
        assert(i+2 == e && "array op not second to last?");
        const NaClBitCodeAbbrevOp &EltEnc = Abbv->getOperandInfo(++i);

        // Emit a vbr6 to indicate the number of elements present.
        EmitVBR(static_cast<uint32_t>(Vals.size()-RecordIdx), 6);

        // Emit each field.
        for (unsigned e = Vals.size(); RecordIdx != e; ++RecordIdx)
          EmitAbbreviatedField(EltEnc, Vals[RecordIdx]);
      } else {
        assert(RecordIdx < Vals.size() && "Invalid abbrev/record");
        EmitAbbreviatedField(Op, Vals[RecordIdx]);
        ++RecordIdx;
      }
    }
    assert(RecordIdx == Vals.size() && "Not all record operands emitted!");
  }

public:

  /// Returns a pointer to the abbreviation currently associated with
  /// the abbreviation index. Returns nullptr if no such abbreviation.
  const NaClBitCodeAbbrev *getAbbreviation(unsigned Index) const {
    if (Index < naclbitc::FIRST_APPLICATION_ABBREV)
      return nullptr;
    if (Index >= BlockScope.back().AbbreviationIndexLimit)
      return nullptr;
    unsigned AbbrevNo = Index - naclbitc::FIRST_APPLICATION_ABBREV;
    if (AbbrevNo >= CurAbbrevs.size())
      return nullptr;
    return CurAbbrevs[AbbrevNo];
  }

  /// EmitRecord - Emit the specified record to the stream, using an abbrev if
  /// we have one to compress the output.
  template<typename uintty>
  void EmitRecord(unsigned Code, const SmallVectorImpl<uintty> &Vals,
                  unsigned Abbrev = 0) {
    if (!Abbrev) {
      // If we don't have an abbrev to use, emit this in its fully unabbreviated
      // form.
      EmitCode(naclbitc::UNABBREV_RECORD);
      EmitVBR(Code, 6);
      EmitVBR(static_cast<uint32_t>(Vals.size()), 6);
      for (unsigned i = 0, e = static_cast<unsigned>(Vals.size()); i != e; ++i)
        EmitVBR64(Vals[i], 6);
      flushToByteIfAligned();
      return;
    }

    // combine code and values, and then emit.
    AbbrevValues<uintty> AbbrevVals(Code, Vals);
    EmitRecordWithAbbrevImpl(Abbrev, AbbrevVals);
    flushToByteIfAligned();
  }

  //===--------------------------------------------------------------------===//
  // Abbrev Emission
  //===--------------------------------------------------------------------===//

private:
  // Emit the abbreviation as a DEFINE_ABBREV record.
  void EncodeAbbrev(NaClBitCodeAbbrev *Abbv) {
    EmitCode(naclbitc::DEFINE_ABBREV);
    EmitVBR(Abbv->getNumOperandInfos(), 5);
    for (unsigned i = 0, e = static_cast<unsigned>(Abbv->getNumOperandInfos());
         i != e; ++i) {
      const NaClBitCodeAbbrevOp &Op = Abbv->getOperandInfo(i);
      bool IsLiteral = Op.isLiteral();
      Emit(IsLiteral, 1);
      if (IsLiteral) {
        EmitVBR64(Op.getValue(), 8);
      } else {
        Emit(Op.getEncoding(), 3);
        if (Op.hasValue())
          EmitVBR64(Op.getValue(), 5);
      }
    }
    flushToByteIfAligned();
  }
public:

  /// EmitAbbrev - This emits an abbreviation to the stream.  Note that this
  /// method takes ownership of the specified abbrev.
  unsigned EmitAbbrev(NaClBitCodeAbbrev *Abbv) {
    assert(Abbv->isValid() && "Can't emit invalid abbreviation!");
    // Emit the abbreviation as a record.
    EncodeAbbrev(Abbv);
    CurAbbrevs.push_back(Abbv);
    return static_cast<unsigned>(CurAbbrevs.size())-1 +
      naclbitc::FIRST_APPLICATION_ABBREV;
  }

  //===--------------------------------------------------------------------===//
  // BlockInfo Block Emission
  //===--------------------------------------------------------------------===//

  /// EnterBlockInfoBlock - Start emitting the BLOCKINFO_BLOCK.
  void EnterBlockInfoBlock() {
    EnterSubblock(naclbitc::BLOCKINFO_BLOCK_ID);
    BlockInfoCurBID = ~0U;
  }
private:
  /// SwitchToBlockID - If we aren't already talking about the specified block
  /// ID, emit a BLOCKINFO_CODE_SETBID record.
  void SwitchToBlockID(unsigned BlockID) {
    if (BlockInfoCurBID == BlockID) return;
    SmallVector<unsigned, 2> V;
    V.push_back(BlockID);
    EmitRecord(naclbitc::BLOCKINFO_CODE_SETBID, V);
    BlockInfoCurBID = BlockID;
  }

  BlockInfo &getOrCreateBlockInfo(unsigned BlockID) {
    if (BlockInfo *BI = getBlockInfo(BlockID))
      return *BI;

    // Otherwise, add a new record.
    BlockInfoRecords.push_back(BlockInfo());
    BlockInfoRecords.back().BlockID = BlockID;
    return BlockInfoRecords.back();
  }

public:

  /// EmitBlockInfoAbbrev - Emit a DEFINE_ABBREV record for the specified
  /// BlockID.
  unsigned EmitBlockInfoAbbrev(unsigned BlockID, NaClBitCodeAbbrev *Abbv) {
    SwitchToBlockID(BlockID);
    EncodeAbbrev(Abbv);

    // Add the abbrev to the specified block record.
    BlockInfo &Info = getOrCreateBlockInfo(BlockID);
    Info.Abbrevs.push_back(Abbv);

    return Info.Abbrevs.size()-1+naclbitc::FIRST_APPLICATION_ABBREV;
  }
};


} // End llvm namespace

#endif
