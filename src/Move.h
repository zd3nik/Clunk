//-----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//-----------------------------------------------------------------------------

#ifndef CLUNK_MOVE_H
#define CLUNK_MOVE_H

#include "senjo/Square.h"
#include "Defs.h"

namespace clunk
{

class Move {
public:
  //---------------------------------------------------------------------------
  enum Shifts {
    ToShift    = 4,
    FromShift  = 12,
    CapShift   = 20,
    PromoShift = 24
  };

  //---------------------------------------------------------------------------
  static bool LexicalCompare(const Move& a, const Move& b) {
    return (strcmp(a.ToString().c_str(), b.ToString().c_str()) < 0);
  }

  //---------------------------------------------------------------------------
  static bool ScoreCompare(const Move& a, const Move& b) {
    return (a.score > b.score);
  }

  //---------------------------------------------------------------------------
  Move(const uint32_t bits = 0, const int score = 0)
    : bits(bits),
      score(static_cast<int32_t>(score))
  {
    assert(abs(score) <= Infinity);
    assert(!bits || IsValid());
  }

  //---------------------------------------------------------------------------
  Move(const Move& other)
    : bits(other.bits),
      score(other.score)
  {
    assert(abs(score) <= Infinity);
    assert(!bits || IsValid());
  }

  //---------------------------------------------------------------------------
  Move& operator=(const Move& other) {
    assert(abs(other.score) <= Infinity);
    bits = other.bits;
    score = other.score;
    assert(!bits || IsValid());
    return *this;
  }

  //---------------------------------------------------------------------------
  void Clear() {
    bits = 0;
    score = 0;
  }

  //---------------------------------------------------------------------------
  void Init(const uint32_t bits, const int score) {
    assert(abs(score) <= Infinity);
    this->bits = bits;
    this->score = static_cast<int32_t>(score);
    assert(!bits || IsValid());
  }

  //---------------------------------------------------------------------------
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
           (to    <<    ToShift) |
           (from  <<  FromShift) |
           (cap   <<   CapShift) |
           (promo << PromoShift));
    this->score = static_cast<int32_t>(score);
    assert(Type() == type);
    assert(From() == from);
    assert(To() == to);
    assert(Cap() == cap);
    assert(Promo() == promo);
    assert(IsValid());
  }

  //---------------------------------------------------------------------------
  int GetScore() const {
    return score;
  }

  //---------------------------------------------------------------------------
  int& Score() {
    return score;
  }

  //---------------------------------------------------------------------------
  uint32_t Bits() const {
    return bits;
  }

  //---------------------------------------------------------------------------
  MoveType Type() const {
    return static_cast<MoveType>(bits & 0xF);
  }

  //---------------------------------------------------------------------------
  int To() const {
    return static_cast<int>((bits >> ToShift) & 0xFF);
  }

  //---------------------------------------------------------------------------
  int From() const {
    return static_cast<int>((bits >> FromShift) & 0xFF);
  }

  //---------------------------------------------------------------------------
  int Cap() const {
    return static_cast<int>((bits >> CapShift) & 0xF);
  }

  //---------------------------------------------------------------------------
  int Promo() const {
    return static_cast<int>((bits >> PromoShift) & 0xF);
  }

  //---------------------------------------------------------------------------
  int HistoryIndex() const {
    return static_cast<int>(bits & 0xFFF);
  }

  //---------------------------------------------------------------------------
  bool IsCapOrPromo() const {
    return ((bits & 0xFF00000UL) | (Type() == PawnCap));
  }

  //---------------------------------------------------------------------------
  bool operator==(const Move& other) const {
    return (bits == other.bits);
  }

  //---------------------------------------------------------------------------
  bool operator!=(const Move& other) const {
    return (bits != other.bits);
  }

  //---------------------------------------------------------------------------
  void SwapWith(Move& other) {
    const uint32_t tbits  = bits;
    const int32_t  tscore = score;
    bits  = other.bits;
    score = other.score;
    other.bits =  tbits;
    other.score = tscore;
  }

  //---------------------------------------------------------------------------
  std::string ToString() const {
    std::string str;
    if (bits) {
      str = (senjo::Square(From()).ToString() + senjo::Square(To()).ToString());
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

  //---------------------------------------------------------------------------
  bool IsValid() const {
    return (IS_MTYPE(Type()) &
            IS_SQUARE(To()) &
            IS_SQUARE(From()) &
            (To() != From()) &
            ((!Cap()) | IS_CAP(Cap())) &
            ((!Promo()) || IS_PROMO(Promo())) &
            (abs(score) <= Infinity));
  }

private:
  uint32_t bits;
  int32_t score;
};

} // namespace clunk

#endif // CLUNK_MOVE_H
