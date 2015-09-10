//----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//----------------------------------------------------------------------------

#ifndef CLUNK_MOVE_H
#define CLUNK_MOVE_H

#include "senjo/Square.h"
#include "Defs.h"

namespace clunk
{

class Move {
public:
  //--------------------------------------------------------------------------
  enum Shifts {
    FromShift   = 4,  ///< Number of bits the 'from' square is shifted left
    ToShift     = 12, ///< Number of bits the 'to' square is shifted left
    CapShift    = 20, ///< Number of bits the 'cap' value is shifted left
    PromoShift  = 24  ///< Number of bits the 'promo' value is shifted left
  };

  //--------------------------------------------------------------------------
  static bool LexicalCompare(const Move& a, const Move& b) {
    return (strcmp(a.ToString().c_str(), b.ToString().c_str()) < 0);
  }

  //--------------------------------------------------------------------------
  Move(const uint32_t bits = 0, const int score = 0)
    : bits(bits),
      score(static_cast<int32_t>(score))
  {
    assert(abs(score) <= Infinity);
    assert(Validate());
  }

  //--------------------------------------------------------------------------
  Move(const Move& other)
    : bits(other.bits),
      score(other.score)
  {
    assert(abs(score) <= Infinity);
    assert(Validate());
  }

  //--------------------------------------------------------------------------
  Move& operator=(const Move& other) {
    assert(abs(other.score) <= Infinity);
    bits = other.bits;
    score = other.score;
    assert(Validate());
    return *this;
  }

  //--------------------------------------------------------------------------
  void Clear() {
    bits = 0;
    score = 0;
  }

  //--------------------------------------------------------------------------
  void Init(const uint32_t bits, const int score) {
    assert(abs(score) <= Infinity);
    this->bits = bits;
    this->score = static_cast<int32_t>(score);
    assert(Validate());
  }

  //--------------------------------------------------------------------------
  void Set(const MoveType type,
           const int from,
           const int to,
           const int cap = 0,
           const int promo = 0,
           const int score = 0)
  {
    assert(IS_MTYPE(type));
    assert(IS_SQUARE(from));
    assert(IS_SQUARE(to));
    assert(from != to);
    assert(!cap || IS_CAP(cap));
    assert(!promo || IS_PROMO(promo));
    assert(abs(score) <= Infinity);
    bits = (type                 |
           (from  <<  FromShift) |
           (to    <<    ToShift) |
           (cap   <<   CapShift) |
           (promo << PromoShift));
    this->score = static_cast<int32_t>(score);
    assert(Type() == type);
    assert(From() == from);
    assert(To() == to);
    assert(Cap() == cap);
    assert(Promo() == promo);
    assert(Validate());
  }

  //--------------------------------------------------------------------------
  int GetScore() const {
    return score;
  }

  //--------------------------------------------------------------------------
  int& Score() {
    return score;
  }

  //--------------------------------------------------------------------------
  uint32_t Bits() const {
    return bits;
  }

  //--------------------------------------------------------------------------
  MoveType Type() const {
    return static_cast<MoveType>(bits & 0xF);
  }

  //--------------------------------------------------------------------------
  int From() const {
    return static_cast<int>((bits >> FromShift) & 0xFF);
  }

  //--------------------------------------------------------------------------
  int To() const {
    return static_cast<int>((bits >> ToShift) & 0xFF);
  }

  //--------------------------------------------------------------------------
  int Cap() const {
    return static_cast<int>((bits >> CapShift) & 0xF);
  }

  //--------------------------------------------------------------------------
  int Promo() const {
    return static_cast<int>((bits >> PromoShift) & 0xF);
  }

  //--------------------------------------------------------------------------
  senjo::Square FromSquare() const {
    return senjo::Square((bits >> FromShift) & 0xFF);
  }

  //--------------------------------------------------------------------------
  senjo::Square ToSquare() const {
    return senjo::Square((bits >> ToShift) & 0xFF);
  }

  //--------------------------------------------------------------------------
  bool IsValid() const {
    return (Type() && (From() != To()));
  }

  //--------------------------------------------------------------------------
  bool IsCapOrPromo() const {
    return (bits & 0xFF00000UL);
  }

  //--------------------------------------------------------------------------
  bool operator==(const Move& other) const {
    return (bits == other.bits);
  }

  //--------------------------------------------------------------------------
  bool operator!=(const Move& other) const {
    return (bits != other.bits);
  }

  //--------------------------------------------------------------------------
  bool operator<(const Move& other) const {
    return (score < other.score);
  }

  //--------------------------------------------------------------------------
  void SwapWith(Move& other) {
    const uint32_t tbits  = bits;
    const int32_t  tscore = score;
    bits  = other.bits;
    score = other.score;
    other.bits =  tbits;
    other.score = tscore;
  }

  //--------------------------------------------------------------------------
  std::string ToString() const {
    std::string str;
    if (IsValid()) {
      str = (FromSquare().ToString() + ToSquare().ToString());
      switch (Promo()) {
      case (White|Knight): case (Black|Knight): return (str + "n");
      case (White|Bishop): case (Black|Bishop): return (str + "b");
      case (White|Rook):   case (Black|Rook):   return (str + "r");
      case (White|Queen):  case (Black|Queen):  return (str + "q");
      default:
        break;
      }
    }
    return str;
  }

  //--------------------------------------------------------------------------
  bool Validate() const {
    if (bits) {
      assert(IS_MTYPE(Type()));
      assert(IS_SQUARE(From()));
      assert(IS_SQUARE(To()));
      assert(From() != To());
      assert(!Cap() || IS_CAP(Cap()));
      assert(!Promo() || IS_PROMO(Promo()));
      assert(abs(score) <= Infinity);
    }
    return true;
  }

private:
  uint32_t bits;
  int32_t score;
};

} // namespace clunk

#endif // CLUNK_MOVE_H
