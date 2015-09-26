//-----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//-----------------------------------------------------------------------------

#ifndef CLUNK_HASHTABLE_H
#define CLUNK_HASHTABLE_H

#include "senjo/Platform.h"
#include "Types.h"
#include "Move.h"

namespace clunk
{

//-----------------------------------------------------------------------------
extern const uint64_t _HASH[14][128];

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
struct HashEntry
{
  enum {
    // primary flags
    Checkmate   = 0x01,
    Stalemate   = 0x02,
    UpperBound  = 0x03,
    ExactScore  = 0x04,
    LowerBound  = 0x05,
    PrimaryMask = 0x07,

    // other flags
    Extended    = 0x08,
    FromPV      = 0x10,
    OtherMask   = 0x18
  };

  //---------------------------------------------------------------------------
  int GetPrimaryFlag() const {
    return (flags & HashEntry::PrimaryMask);
  }

  //---------------------------------------------------------------------------
  bool HasExtendedFlag() const {
    return (flags & HashEntry::Extended);
  }

  //---------------------------------------------------------------------------
  bool HasPvFlag() const {
    return (flags & HashEntry::FromPV);
  }

  //---------------------------------------------------------------------------
  void Set(const uint64_t key,
           const Move& bestmove,
           const int draft,
           const int primaryFlag,
           const int otherFlags)
  {
    assert(key);
    assert(bestmove.IsValid());
    assert(abs(bestmove.GetScore()) < Infinity);
    assert((draft >= 0) && (draft < 256));
    assert((primaryFlag == HashEntry::LowerBound) ||
           (primaryFlag == HashEntry::UpperBound) ||
           (primaryFlag == HashEntry::ExactScore));
    assert(!(otherFlags & ~HashEntry::OtherMask));

    positionKey = key;
    moveBits    = bestmove.Bits();
    score       = static_cast<int16_t>(bestmove.GetScore());
    depth       = static_cast<uint8_t>(draft);
    flags       = static_cast<uint8_t>(primaryFlag | otherFlags);
  }

  //---------------------------------------------------------------------------
  void SetCheckmate(const uint64_t key) {
    assert(key);
    positionKey = key;
    moveBits    = 0;
    score       = Infinity;
    depth       = 0;
    flags       = HashEntry::Checkmate;
  }

  //---------------------------------------------------------------------------
  void SetStalemate(const uint64_t key) {
    assert(key);
    positionKey = key;
    moveBits    = 0;
    score       = 0;
    depth       = 0;
    flags       = HashEntry::Stalemate;
  }

  uint64_t positionKey;
  uint32_t moveBits;
  int16_t  score;
  uint8_t  depth;
  uint8_t  flags;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
struct PawnEntry
{
  enum {
    Backward  = 0x10,
    Supported = 0x20,
    Passed    = 0x40
  };

  uint64_t positionKey;
  uint8_t fileInfo[2][10];
  int16_t score[2];
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template<typename EntryType>
class TranspositionTable
{
public:
  //---------------------------------------------------------------------------
  TranspositionTable()
    : keyMask(0ULL),
      entries(NULL)
  {
    Resize(1);
  }

  //---------------------------------------------------------------------------
  ~TranspositionTable() {
    delete[] entries;
    entries = NULL;
    keyMask = 0ULL;
  }

  //---------------------------------------------------------------------------
  void Resize(size_t mbytes) {
    delete[] entries;
    entries = NULL;
    keyMask = 0ULL;

    if (!mbytes) {
      mbytes = 1;
    }

    // convert mbytes to bytes
    const size_t bytes = (mbytes * 1024 * 1024);

    // how many hash entries can we fit into the requested number of bytes?
    const size_t count = (bytes / sizeof(EntryType));

    // get high bit of 'count + 1'
    // for example, if 'count + 1' in binary is: 100110101
    //                    the high bit would be: 100000000
    // NOTE: there are faster ways to do this on modern processors
    size_t highBit = 2;
    for (size_t tmp = ((count + 1) >> 2); tmp; tmp >>= 1) {
      const size_t next = (highBit << 1);
      if (next) {
        highBit = next;
      }
      else {
        break;
      }
    }
    assert(highBit >= 2);

    // highBit is the number of entries we'll store
    // highBit - 1 is the bit mask we use to map position keys to a table slot
    // example highBit in binary: 100000000
    //                      mask: 011111111
    keyMask = (highBit - 1);
    assert(keyMask);

    // allocate as much as we can
    while (!(entries = new EntryType[keyMask + 1])) {
      keyMask >>= 1;
      assert(keyMask);
    }

    // initialize it
    Clear();
  }

  //---------------------------------------------------------------------------
  void Clear() {
    assert(keyMask);
    assert(entries);
    memset(entries, 0, (sizeof(EntryType) * (keyMask + 1)));
    ResetCounters();
  }

  //---------------------------------------------------------------------------
  inline EntryType* Get(const uint64_t key) {
    assert(keyMask);
    assert(entries);
    gets++;
    return (entries + (key & keyMask));
  }

  //---------------------------------------------------------------------------
  void ResetCounters() {
    gets = 0;
    hits = 0;
    checkmates = 0;
    stalemates = 0;
  }

  //---------------------------------------------------------------------------
  void IncHits() {
    hits++;
  }

  //---------------------------------------------------------------------------
  void IncCheckmates() {
    checkmates++;
  }

  //---------------------------------------------------------------------------
  void IncStalemates() {
    stalemates++;
  }

  //---------------------------------------------------------------------------
  uint64_t Gets() const {
    return gets;
  }

  //---------------------------------------------------------------------------
  uint64_t Hits() const {
    return hits;
  }

  //---------------------------------------------------------------------------
  uint64_t Checkmates() const {
    return checkmates;
  }

  //---------------------------------------------------------------------------
  uint64_t Stalemates() const {
    return stalemates;
  }

private:
  uint64_t gets;
  uint64_t hits;
  uint64_t checkmates;
  uint64_t stalemates;

  size_t     keyMask;
  EntryType* entries;
};

} // namespace clunk

#endif // CLUNK_HASHTABLE_H
