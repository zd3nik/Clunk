//----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//----------------------------------------------------------------------------

#ifndef CLUNK_HASHTABLE_H
#define CLUNK_HASHTABLE_H

#include "senjo/Platform.h"
#include "Types.h"
#include "Move.h"

namespace clunk
{

//----------------------------------------------------------------------------
extern const uint64_t _HASH[14][128];

//----------------------------------------------------------------------------
struct HashEntry
{
public:
  enum Flag {
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

  //--------------------------------------------------------------------------
  int GetPrimaryFlag() const {
    return (flags & HashEntry::PrimaryMask);
  }

  //--------------------------------------------------------------------------
  bool HasExtendedFlag() const {
    return (flags & HashEntry::Extended);
  }

  //--------------------------------------------------------------------------
  bool HasPvFlag() const {
    return (flags & HashEntry::FromPV);
  }

  uint64_t positionKey;
  uint32_t moveBits;
  int16_t  score;
  uint8_t  depth;
  uint8_t  flags;
};

//----------------------------------------------------------------------------
class TranspositionTable
{
public:
  //--------------------------------------------------------------------------
  TranspositionTable()
    : keyMask(0ULL),
      entries(NULL)
  { }

  //--------------------------------------------------------------------------
  ~TranspositionTable() {
    delete[] entries;
    entries = NULL;
    keyMask = 0ULL;
  }

  //--------------------------------------------------------------------------
  bool Resize(const size_t mbytes) {
    delete[] entries;
    entries = NULL;
    keyMask = 0ULL;

    if (!mbytes) {
      return true;
    }

    // convert mbytes to bytes
    const size_t bytes = (mbytes * 1024 * 1024);

    // how many hash entries can we fit into the requested number of bytes?
    const size_t count = (bytes / sizeof(HashEntry));

    // get high bit of 'count + 1'
    // for example, if 'count + 1' in binary is: 100110101
    //                    the high bit would be: 100000000
    // NOTE: there are faster ways to do this on modern processors
    size_t highBit = 1;
    for (size_t tmp = ((count + 1) >> 1); tmp; tmp >>= 1) {
      highBit <<= 1;
    }

    // if highBit is 0 we've shifted beyond size_t bit count (e.g. too big!)
    if (!highBit) {
      return false;
    }

    // highBit is the number of entries we'll store
    // highBit - 1 is the bit mask we use to map position keys to a table slot
    // example highBit in binary: 100000000
    //                      mask: 011111111
    keyMask = (highBit - 1);
    if (!keyMask) {
      return false;
    }

    // allocate it
    if (!(entries = new HashEntry[keyMask + 1])) {
      return false;
    }

    // initialize it
    Clear();
    return true;
  }

  //--------------------------------------------------------------------------
  void Clear() {
    ResetCounters();
    if (entries) {
      memset(entries, 0, (sizeof(HashEntry) * (keyMask + 1)));
    }
  }

  //--------------------------------------------------------------------------
  HashEntry* Probe(const uint64_t key) {
    if (key && entries) {
      HashEntry* entry = (entries + (key & keyMask));
      if (entry->positionKey == key) {
        _hits++;
        return entry;
      }
    }
    return NULL;
  }

  //--------------------------------------------------------------------------
  void Store(const uint64_t key,
             const Move& bestmove,
             const int depth,
             const int primaryFlag,
             const int otherFlags)
  {
    assert(bestmove.IsValid());
    assert(abs(bestmove.GetScore()) < Infinity);
    assert((depth >= 0) && (depth < 256));
    assert((primaryFlag == HashEntry::LowerBound) ||
           (primaryFlag == HashEntry::UpperBound) ||
           (primaryFlag == HashEntry::ExactScore));
    assert(!(otherFlags & ~HashEntry::OtherMask));

    if (key && entries) { // TODO && (key != tt->key or depth >= tt->depth)
      HashEntry* entry   = (entries + (key & keyMask));
      entry->positionKey = key;
      entry->moveBits    = bestmove.Bits();
      entry->score       = static_cast<int16_t>(bestmove.GetScore());
      entry->depth       = static_cast<uint8_t>(depth);
      entry->flags       = static_cast<uint8_t>(primaryFlag | otherFlags);
      _stores++;
    }
  }

  //--------------------------------------------------------------------------
  void StoreCheckmate(const uint64_t key) {
    if (key && entries) {
      HashEntry* entry   = (entries + (key & keyMask));
      entry->positionKey = key;
      entry->moveBits    = 0;
      entry->score       = Infinity;
      entry->depth       = 0;
      entry->flags       = HashEntry::Checkmate;
      _checkmates++;
    }
  }

  //--------------------------------------------------------------------------
  void StoreStalemate(const uint64_t key) {
    if (key && entries) {
      HashEntry* entry   = (entries + (key & keyMask));
      entry->positionKey = key;
      entry->moveBits    = 0;
      entry->score       = 0;
      entry->depth       = 0;
      entry->flags       = HashEntry::Stalemate;
      _stalemates++;
    }
  }

  //--------------------------------------------------------------------------
  void ResetCounters() {
    _stores = 0;
    _hits = 0;
    _checkmates = 0;
    _stalemates = 0;
  }

  //--------------------------------------------------------------------------
  uint64_t GetStores() const {
    return _stores;
  }

  //--------------------------------------------------------------------------
  uint64_t GetHits() const {
    return _hits;
  }

  //--------------------------------------------------------------------------
  uint64_t GetCheckmates() const {
    return _checkmates;
  }

  //--------------------------------------------------------------------------
  uint64_t GetStalemates() const {
    return _stalemates;
  }

private:
  static uint64_t _stores;
  static uint64_t _hits;
  static uint64_t _checkmates;
  static uint64_t _stalemates;

  size_t     keyMask;
  HashEntry* entries;
};

} // namespace clunk

#endif // CLUNK_HASHTABLE_H
