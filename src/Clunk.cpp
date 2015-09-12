//-----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//-----------------------------------------------------------------------------

#include "Clunk.h"
#include "HashTable.h"
#include "Stats.h"
#include "senjo/Output.h"

namespace clunk
{

//-----------------------------------------------------------------------------
struct Piece {
  uint8_t type;
  uint8_t sqr;
};

//-----------------------------------------------------------------------------
const int _VALUE_OF[12] = {
  0,            0,
  PawnValue,    PawnValue,
  KnightValue,  KnightValue,
  BishopValue,  BishopValue,
  RookValue,    RookValue,
  QueenValue,   QueenValue
};

//-----------------------------------------------------------------------------
const int _TOUCH[128] = {
  ~WhiteLong, ~0, ~0, ~0, ~WhiteCastle, ~0, ~0, ~WhiteShort, 0,0,0,0,0,0,0,0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,                            0,0,0,0,0,0,0,0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,                            0,0,0,0,0,0,0,0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,                            0,0,0,0,0,0,0,0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,                            0,0,0,0,0,0,0,0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,                            0,0,0,0,0,0,0,0,
  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,                            0,0,0,0,0,0,0,0,
  ~BlackLong, ~0, ~0, ~0, ~BlackCastle, ~0, ~0, ~BlackShort, 0,0,0,0,0,0,0,0,
};

//-----------------------------------------------------------------------------
const int _DIR_SHIFT[35] = {
   0, // (-17) SouthWest
   8, // (-16) South
  16, // (-15) SouthEast
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  24, // (-1) West
  -1, // ( 0)
  32, // ( 1) East
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  40, // (15) NorthEast
  48, // (16) North
  56  // (17) NorthEast
};

//-----------------------------------------------------------------------------
char               _dir[128][128] = {0};
char               _dist[128][128] = {0};
char               _pinDir[128] = {0};
Piece*             _board[128] = {0};
Piece              _piece[PieceListSize];
int                _pcount[12] = {0};
int                _material[2] = {0};
uint64_t           _atkd[128] = {0};
uint64_t           _pawnCaps[128] = {0};
uint64_t           _knightMoves[128] = {0};
uint64_t           _bishopRook[128] = {0};
uint64_t           _queenKing[128] = {0};
std::set<uint64_t> _seen;

//-----------------------------------------------------------------------------
Piece* _EMPTY              = _piece;
const Piece* _WHITE_KING   = (_piece + WhiteKingOffset);
const Piece* _BLACK_KING   = (_piece + BlackKingOffset);
const Piece* _FIRST_SLIDER = (_piece + BishopOffset);
const Piece* _FIRST_ROOK   = (_piece + RookOffset);

//-----------------------------------------------------------------------------
int                _stop = 0;
int                _depth = 0;
int                _seldepth = 0;
int                _movenum = 0;
int                _drawScore[2] = {0};
uint64_t           _execs = 0;
uint64_t           _nodes = 0;
uint64_t           _qnodes = 0;
uint64_t           _startTime = 0;
char               _hist[0x1000] = {0};
std::string        _currmove;
Stats              _stats;
Stats              _totalStats;
TranspositionTable _tt;

//-----------------------------------------------------------------------------
void InitSearch(const Color colorToMove, const uint64_t startTime) {
  _stop      = 0;
  _depth     = 0;
  _seldepth  = 0;
  _movenum   = 0;
  _execs     = 0;
  _nodes     = 0;
  _qnodes    = 0;
  _startTime = startTime;

  _currmove.clear();
  _stats.Clear();
  _tt.ResetCounters();

  _drawScore[colorToMove] = 0; // TODO -_contempt;
  _drawScore[!colorToMove] = 0; // TODO _contempt;
}

//-----------------------------------------------------------------------------
void InitDistDir() {
  memset(_dir, 0, sizeof(_dir));
  memset(_dist, 0, sizeof(_dist));
  for (int a = A1; a <= H8; ++a) {
    if (BAD_SQR(a)) {
      a += 7;
      continue;
    }
    for (int b = A1; b <= H8; ++b) {
      if (BAD_SQR(b)) {
        b += 7;
        continue;
      }
      const int x1 = XC(a);
      const int y1 = YC(a);
      const int x2 = XC(b);
      const int y2 = YC(b);
      const int d1 = abs(x1 - x2);
      const int d2 = abs(y1 - y2);
      _dist[a][b]  = std::max<int>(d1, d2);
      if (x1 == x2) {
        if (y1 > y2) {
          _dir[a][b] = South;
        }
        else if (y1 < y2) {
          _dir[a][b] = North;
        }
      }
      else if (y1 == y2) {
        if (x1 > x2) {
          _dir[a][b] = West;
        }
        else if (x1 < x2) {
          _dir[a][b] = East;
        }
      }
      else if (x1 > x2) {
        if (y1 > y2) {
          if (((SQR(x1, y1) - SQR(x2, y2)) % 17) == 0) {
            _dir[a][b] = SouthWest;
          }
        }
        else if (y1 < y2) {
          if (((SQR(x2, y2) - SQR(x1, y1)) % 15) == 0) {
            _dir[a][b] = NorthWest;
          }
        }
      }
      else if (x1 < x2) {
        if (y1 > y2) {
          if (((SQR(x1, y1) - SQR(x2, y2)) % 15) == 0) {
            _dir[a][b] = SouthEast;
          }
        }
        else if (y1 < y2) {
          if (((SQR(x2, y2) - SQR(x1, y1)) % 17) == 0) {
            _dir[a][b] = NorthEast;
          }
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
inline int Distance(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  return _dist[from][to];
}

//-----------------------------------------------------------------------------
inline int Direction(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  return _dir[from][to];
}

//-----------------------------------------------------------------------------
inline int ValueOf(const int pc) {
  assert(!pc || ((pc >= Pawn) & (pc < King)));
  return _VALUE_OF[pc];
}

//-----------------------------------------------------------------------------
inline void AddPiece(const int type, const int sqr) {
  assert(IS_SQUARE(sqr));
  Piece* pc;
  switch (type) {
  case (White|Pawn):
    assert(_pcount[White|Pawn] < 8);
    pc = (_piece + PawnOffset + _pcount[White|Pawn]++);
    assert(pc < (_piece + BlackPawnOffset));
    break;
  case (Black|Pawn):
    assert(_pcount[Black|Pawn] < 8);
    pc = (_piece + BlackPawnOffset + _pcount[Black|Pawn]++);
    assert(pc < (_piece + KnightOffset));
    break;
  case (White|Knight):
    assert(_pcount[White|Knight] < 10);
    pc = (_piece + KnightOffset + _pcount[White|Knight]++);
    assert(pc < (_piece + BlackKnightOffset));
    break;
  case (Black|Knight):
    assert(_pcount[Black|Knight] < 10);
    pc = (_piece + BlackKnightOffset + _pcount[Black|Knight]++);
    assert(pc < (_piece + BishopOffset));
    break;
  case (White|Bishop):
    assert(_pcount[White|Bishop] < 10);
    assert(_pcount[White] < 13);
    pc = (_piece + BishopOffset + _pcount[White|Bishop]++);
    assert(pc < (_piece + BlackBishopOffset));
    _pcount[White]++;
    break;
  case (Black|Bishop):
    assert(_pcount[Black|Bishop] < 10);
    assert(_pcount[Black] < 13);
    pc = (_piece + BlackBishopOffset + _pcount[Black|Bishop]++);
    assert(pc < (_piece + RookOffset));
    _pcount[Black]++;
    break;
  case (White|Rook):
    assert(_pcount[White|Rook] < 10);
    assert(_pcount[White] < 13);
    pc = (_piece + RookOffset + _pcount[White|Rook]++);
    assert(pc < (_piece + BlackRookOffset));
    _pcount[White]++;
    break;
  case (Black|Rook):
    assert(_pcount[Black|Rook] < 10);
    assert(_pcount[Black] < 13);
    pc = (_piece + BlackRookOffset + _pcount[Black|Rook]++);
    assert(pc < (_piece + QueenOffset));
    _pcount[Black]++;
    break;
  case (White|Queen):
    assert(_pcount[White|Queen] < 9);
    assert(_pcount[White] < 13);
    pc = (_piece + QueenOffset + _pcount[White|Queen]++);
    assert(pc < (_piece + BlackQueenOffset));
    _pcount[White]++;
    break;
  case (Black|Queen):
    assert(_pcount[Black|Queen] < 9);
    assert(_pcount[Black] < 13);
    pc = (_piece + BlackQueenOffset + _pcount[Black|Queen]++);
    assert(pc < (_piece + PieceListSize));
    _pcount[Black]++;
    break;
  case (White|King):
    pc = const_cast<Piece*>(_WHITE_KING);
    assert(!pc->type & !pc->sqr); // only allow this once
    break;
  case (Black|King):
    pc = const_cast<Piece*>(_BLACK_KING);
    assert(!pc->type & !pc->sqr); // only allow this once
    break;
  default:
    assert(false);
    pc = NULL;
  }
  assert(pc && (pc != _EMPTY));
  pc->type = type;
  pc->sqr = sqr;
  _board[sqr] = pc;
}

//-----------------------------------------------------------------------------
inline void RemovePiece(const int type, const int sqr) {
  assert((type >= Pawn) & (type < King));
  assert(IS_SQUARE(sqr));
  Piece* pc = _board[sqr];
  assert(pc && (pc != _EMPTY));
  assert(pc->type == type);
  assert(pc->sqr == sqr);
  switch (type) {
  case (White|Pawn):
    assert((_pcount[type] > 0) & (_pcount[type] <= 8));
    assert(pc >= (_piece + PawnOffset));
    assert(pc < (_piece + PawnOffset + _pcount[type]));
    *pc = _piece[PawnOffset + --_pcount[White|Pawn]];
    break;
  case (Black|Pawn):
    assert((_pcount[type] > 0) & (_pcount[type] <= 8));
    assert(pc >= (_piece + BlackPawnOffset));
    assert(pc < (_piece + BlackPawnOffset + _pcount[type]));
    *pc = _piece[BlackPawnOffset + --_pcount[Black|Pawn]];
    break;
  case (White|Knight):
    assert((_pcount[type] > 0) & (_pcount[type] <= 10));
    assert(pc >= (_piece + KnightOffset));
    assert(pc < (_piece + KnightOffset + _pcount[type]));
    *pc = _piece[KnightOffset + --_pcount[White|Knight]];
    break;
  case (Black|Knight):
    assert((_pcount[type] > 0) & (_pcount[type] <= 10));
    assert(pc >= (_piece + BlackKnightOffset));
    assert(pc < (_piece + BlackKnightOffset + _pcount[type]));
    *pc = _piece[BlackKnightOffset + --_pcount[Black|Knight]];
    break;
  case (White|Bishop):
    assert((_pcount[type] > 0) & (_pcount[type] <= 10));
    assert(pc >= (_piece + BishopOffset));
    assert(pc < (_piece + BishopOffset + _pcount[type]));
    *pc = _piece[BishopOffset + --_pcount[White|Bishop]];
    --_pcount[White];
    break;
  case (Black|Bishop):
    assert((_pcount[type] > 0) & (_pcount[type] <= 10));
    assert(pc >= (_piece + BlackBishopOffset));
    assert(pc < (_piece + BlackBishopOffset + _pcount[type]));
    *pc = _piece[BlackBishopOffset + --_pcount[Black|Bishop]];
    --_pcount[Black];
    break;
  case (White|Rook):
    assert((_pcount[type] > 0) & (_pcount[type] <= 10));
    assert(pc >= (_piece + RookOffset));
    assert(pc < (_piece + RookOffset + _pcount[type]));
    *pc = _piece[RookOffset + --_pcount[White|Rook]];
    --_pcount[White];
    break;
  case (Black|Rook):
    assert((_pcount[type] > 0) & (_pcount[type] <= 10));
    assert(pc >= (_piece + BlackRookOffset));
    assert(pc < (_piece + BlackRookOffset + _pcount[type]));
    *pc = _piece[BlackRookOffset + --_pcount[Black|Rook]];
    --_pcount[Black];
    break;
  case (White|Queen):
    assert((_pcount[type] > 0) & (_pcount[type] <= 9));
    assert(pc >= (_piece + QueenOffset));
    assert(pc < (_piece + QueenOffset + _pcount[type]));
    *pc = _piece[QueenOffset + --_pcount[White|Queen]];
    --_pcount[White];
    break;
  case (Black|Queen):
    assert((_pcount[type] > 0) & (_pcount[type] <= 9));
    assert(pc >= (_piece + BlackQueenOffset));
    assert(pc < (_piece + BlackQueenOffset + _pcount[type]));
    *pc = _piece[BlackQueenOffset + --_pcount[Black|Queen]];
    --_pcount[Black];
    break;
  default:
    assert(false);
  }
  _board[pc->sqr] = pc;
  _board[sqr] = _EMPTY;
}

//-----------------------------------------------------------------------------
bool VerifyMoveMap(const int from, uint64_t map) {
  assert(IS_SQUARE(from));
  int sqrs[128] = {0};
  int sqr;
  sqrs[from] = 1;
  while (map) {
    if (!(map & 0xFF)) {
      return false;
    }
    sqr = ((map & 0xFF) - 1);
    if (!IS_SQUARE(sqr)) {
      return false;
    }
    if (sqrs[sqr]) {
      return false;
    }
    sqrs[sqr] = 1;
    map >>= 8;
  }
  return true;
}

//-----------------------------------------------------------------------------
template<Color color>
void InitPawnMoves(const int from) {
  assert(IS_SQUARE(from));
  uint64_t mvs = 0ULL;
  int shift = 0;
  int to = (from + (color ? SouthWest : NorthWest));
  if (IS_SQUARE(to)) {
    assert(shift <= 56);
    mvs |= (uint64_t(to + 1) << shift);
    shift += 8;
  }
  to = (from + (color ? SouthEast : NorthEast));
  if (IS_SQUARE(to)) {
    assert(shift <= 56);
    mvs |= (uint64_t(to + 1) << shift);
    shift += 8;
  }
  assert(VerifyMoveMap(from, mvs));
  _pawnCaps[from + (color * 8)] = mvs;
}

//-----------------------------------------------------------------------------
void InitKnightMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[8] = {
    -33, -31, -18, -14,
     14,  18,  31,  33
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  for (int i = 0; i < 8; ++i) {
    const int to = (from + dir[i]);
    if (IS_SQUARE(to)) {
      assert(shift <= 56);
      mvs |= (uint64_t(to + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _knightMoves[from] = mvs;
}

//-----------------------------------------------------------------------------
void InitBishopMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[4] = {
    SouthWest, SouthEast, NorthWest, NorthEast
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  for (int i = 0; i < 4; ++i) {
    int end = (from + dir[i]);
    if (IS_SQUARE(end)) {
      while (IS_SQUARE(end + dir[i])) end += dir[i];
      assert(IS_SQUARE(end));
      assert(Direction(from, end) == dir[i]);
      assert(shift <= 56);
      mvs |= (uint64_t(end + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _bishopRook[from] = mvs;
}

//-----------------------------------------------------------------------------
void InitRookMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[4] = {
    South, West, East, North
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  for (int i = 0; i < 4; ++i) {
    int end = (from + dir[i]);
    if (IS_SQUARE(end)) {
      while (IS_SQUARE(end + dir[i])) end += dir[i];
      assert(IS_SQUARE(end));
      assert(Direction(from, end) == dir[i]);
      assert(shift <= 56);
      mvs |= (uint64_t(end + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _bishopRook[from + 8] = mvs;
}

//-----------------------------------------------------------------------------
void InitQueenMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[8] = {
    SouthWest, South, SouthEast, West,
    East, NorthWest, North, NorthEast
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  for (int i = 0; i < 8; ++i) {
    int end = (from + dir[i]);
    if (IS_SQUARE(end)) {
      while (IS_SQUARE(end + dir[i])) end += dir[i];
      assert(IS_SQUARE(end));
      assert(Direction(from, end) == dir[i]);
      assert(shift <= 56);
      mvs |= (uint64_t(end + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _queenKing[from] = mvs;
}

//-----------------------------------------------------------------------------
void InitKingMoves(const int from) {
  assert(IS_SQUARE(from));
  const int dir[8] = {
    SouthWest, South, SouthEast, West,
    East, NorthWest, North, NorthEast
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  for (int i = 0; i < 8; ++i) {
    const int to = (from + dir[i]);
    if (IS_SQUARE(to)) {
      assert(Direction(from, to) == dir[i]);
      assert(shift <= 56);
      mvs |= (uint64_t(to + 1) << shift);
      shift += 8;
    }
  }
  assert(VerifyMoveMap(from, mvs));
  _queenKing[from + 8] = mvs;
}

//-----------------------------------------------------------------------------
void InitMoveMaps() {
  memset(_pawnCaps, 0, sizeof(_pawnCaps));
  memset(_knightMoves, 0, sizeof(_knightMoves));
  memset(_bishopRook, 0, sizeof(_bishopRook));
  memset(_queenKing, 0, sizeof(_queenKing));
  for (int sqr = A1; sqr <= H8; ++sqr) {
    if (BAD_SQR(sqr)) {
      sqr += 7;
      continue;
    }
    InitPawnMoves<White>(sqr);
    InitPawnMoves<Black>(sqr);
    InitKnightMoves(sqr);
    InitBishopMoves(sqr);
    InitRookMoves(sqr);
    InitQueenMoves(sqr);
    InitKingMoves(sqr);
  }
}

//-----------------------------------------------------------------------------
inline int DirShift(const int dir) {
  assert(IS_DIR(dir));
  assert(_DIR_SHIFT[dir + 17] >= 0);
  assert(_DIR_SHIFT[dir + 17] < 64);
  assert(!(_DIR_SHIFT[dir + 17] % 8));
  return _DIR_SHIFT[dir + 17];
}

//-----------------------------------------------------------------------------
inline void AddAttack(const int from, const int to, const int dir) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  assert(from != to);
  assert(_board[from] >= _FIRST_SLIDER);
  assert(IS_SLIDER(_board[from]->type));
  assert(_board[from]->sqr == from);
  assert(Direction(from, to) == dir);
  assert(!(_atkd[to] & (0xFFULL << DirShift(dir))) ||
         ((_atkd[to] & (0xFFULL << DirShift(dir))) ==
          (uint64_t(from + 1) << DirShift(dir))));
  _atkd[to] |= (uint64_t(from + 1) << DirShift(dir));
}

//-----------------------------------------------------------------------------
inline void ClearAttack(const int to, const int dir) {
  assert(IS_SQUARE(to));
  _atkd[to] &= ~(0xFFULL << DirShift(dir));
}

//-----------------------------------------------------------------------------
void AddAttacksFrom(const int pc, const int from) {
  assert(IS_SLIDER(pc));
  assert(IS_SQUARE(from));
  assert(_board[from] >= _FIRST_SLIDER);
  assert(_board[from]->type == pc);
  assert(_board[from]->sqr == from);
  uint64_t mvs = (pc < Rook)  ? _bishopRook[from] :
                 (pc < Queen) ? _bishopRook[from + 8]
                              : _queenKing[from];
  for (; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = Direction(from, end);
    assert(IS_DIR(dir));
    for (int to = (from + dir);; to += dir) {
      assert(IS_SQUARE(to));
      assert(Direction(from, to) == dir);
      AddAttack(from, to, dir);
      if ((to == end) || (_board[to] != _EMPTY)) {
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------------
void ClearAttacksFrom(const int pc, const int from) {
  assert(IS_SLIDER(pc));
  assert(IS_SQUARE(from));
  assert(_board[from] >= _FIRST_SLIDER);
  assert(_board[from]->type == pc);
  assert(_board[from]->sqr == from);
  uint64_t mvs = (pc < Rook)  ? _bishopRook[from] :
                 (pc < Queen) ? _bishopRook[from + 8]
                              : _queenKing[from];
  for (; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = Direction(from, end);
    assert(IS_DIR(dir));
    for (int to = (from + dir);; to += dir) {
      assert(IS_SQUARE(to));
      assert(Direction(from, to) == dir);
      ClearAttack(to, dir);
      if ((to == end) || (_board[to] != _EMPTY)) {
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------------
void TruncateAttacks(const int to, const int stop) {
  assert(IS_SQUARE(to));
  for (uint64_t tmp = _atkd[to]; tmp; tmp >>= 8) {
    if (tmp & 0xFF) {
      const int from = static_cast<int>((tmp & 0xFF) - 1);
      assert(IS_SQUARE(from));
      assert(_board[from] >= _FIRST_SLIDER);
      assert(IS_SLIDER(_board[from]->type));
      assert(_board[from]->sqr == from);
      const int dir = Direction(from, to);
      for (int ato = (to + dir); IS_SQUARE(ato); ato += dir) {
        assert(Direction(from, ato) == dir);
        ClearAttack(ato, dir);
        if ((ato == stop) || (_board[ato] != _EMPTY)) {
          break;
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
void ExtendAttacks(const int to) {
  assert(IS_SQUARE(to));
  assert(_board[to] == _EMPTY);
  for (uint64_t tmp = _atkd[to]; tmp; tmp >>= 8) {
    if (tmp & 0xFF) {
      const int from = static_cast<int>((tmp & 0xFF) - 1);
      assert(IS_SQUARE(from));
      assert(_board[from] >= _FIRST_SLIDER);
      assert(IS_SLIDER(_board[from]->type));
      assert(_board[from]->sqr == from);
      const int dir = Direction(from, to);
      assert(IS_DIR(dir));
      for (int ato = (to + dir); IS_SQUARE(ato); ato += dir) {
        assert(Direction(from, ato) == dir);
        AddAttack(from, ato, dir);
        if (_board[ato] != _EMPTY) {
          break;
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
template<Color color>
bool AttackedBy(const int sqr) {
  assert(IS_SQUARE(sqr));
  for (uint64_t mvs = _atkd[sqr]; mvs; mvs >>= 8) {
    if (mvs & 0xFF) {
      assert(IS_SQUARE((mvs & 0xFF) - 1));
      assert(IS_DIR(Direction(((mvs & 0xFF) - 1), sqr)));
      assert(_board[(mvs & 0xFF) - 1] >= _FIRST_SLIDER);
      if (COLOR(_board[(mvs & 0xFF) - 1]->type) == color) {
        assert(IS_SLIDER(_board[(mvs & 0xFF) - 1]->type));
        assert(_board[(mvs & 0xFF) - 1]->sqr == ((mvs & 0xFF) - 1));
        return true;
      }
    }
  }
  if (_pcount[color|Knight]) {
    assert(_pcount[color|Knight] > 0);
    for (uint64_t mvs = _knightMoves[sqr]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int from = ((mvs & 0xFF) - 1);
      assert(IS_SQUARE(from));
      assert(from != sqr);
      if (_board[from]->type == (color|Knight)) {
        assert(_board[from]->sqr == from);
        return true;
      }
    }
  }
  for (uint64_t mvs = _queenKing[sqr + 8]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int from = ((mvs & 0xFF) - 1);
    assert(IS_DIR(Direction(sqr, from)));
    if (_board[from] == (color ? _BLACK_KING : _WHITE_KING)) {
      assert(_board[from]->type == (color|King));
      assert(_board[from]->sqr == from);
      return true;
    }
    if (_board[from]->type == (color|Pawn)) {
      assert(_board[from]->sqr == from);
      if (color) {
        switch (Direction(sqr, from)) {
        case NorthWest: case NorthEast:
          return true;
        }
      }
      else {
        switch (Direction(sqr, from)) {
        case SouthWest: case SouthEast:
          return true;
        }
      }
    }
  }
  return false;
}

//-----------------------------------------------------------------------------
bool VerifyAttacksTo(const int to, const bool do_assert) {
  uint64_t atk = 0;
  for (uint64_t mvs = _queenKing[to]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = Direction(end, to);
    assert(IS_DIR(dir));
    for (int from = (to - dir);; from -= dir) {
      assert(IS_SQUARE(from));
      assert(Direction(from, to) == dir);
      if (_board[from] >= _FIRST_SLIDER) {
        const int pc = _board[from]->type;
        assert(IS_SLIDER(pc));
        assert(_board[from]->sqr == from);
        if ((IS_DIAG(dir) && ((Black|pc) != (Black|Rook))) ||
            (IS_CROSS(dir) && ((Black|pc) != (Black|Bishop))))
        {
          atk |= (uint64_t(from + 1) << DirShift(dir));
        }
        break;
      }
      if ((from == end) || (_board[from] != _EMPTY)) {
        break;
      }
    }
  }
  if (do_assert) {
    assert(atk == _atkd[to]);
  }
  return (atk == _atkd[to]);
}

//-----------------------------------------------------------------------------
bool VerifyAttacks(const bool do_assert) {
  for (int sqr = A1; sqr <= H8; ++sqr) {
    if (BAD_SQR(sqr)) {
      sqr += 7;
    }
    else if (!VerifyAttacksTo(sqr, do_assert)) {
      return false;
    }
  }
  return true;
}

//-----------------------------------------------------------------------------
template<Color color>
bool EpPinned(const int from, const int cap) {
  assert((2 + color) == (color ? BlackKingOffset : WhiteKingOffset));
  assert((_piece + 2 + color) == (color ? _BLACK_KING : _WHITE_KING));
  assert(_piece[2 + color].type == (color|King));
  assert(IS_SQUARE(_piece[2 + color].sqr));
  assert(_board[_piece[2 + color].sqr] == (_piece + 2 + color));
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(cap));
  assert(from != cap);
  assert(from != _piece[2 + color].sqr);
  assert(YC(from) == YC(cap));
  assert(YC(from) == (color ? 3 : 4));
  assert(Distance(from, cap) == 1);
  assert(_board[from] != _EMPTY);
  assert(_board[cap] != _EMPTY);
  assert(_board[from]->type == (color|Pawn));
  assert(_board[cap]->type == ((!color)|Pawn));
  if (!_pcount[!color] || (YC(from) != YC(_piece[2 + color].sqr))) {
    return false;
  }
  int left = std::min<int>(from, cap);
  int right = std::max<int>(from, cap);
  if (_piece[2 + color].sqr < left) {
    right = ((_atkd[right] >> DirShift(West)) & 0xFF);
    assert(!right || (IS_SQUARE(right - 1) && (YC(right - 1) == YC(from))));
    assert(!right || (_board[right - 1] >= _FIRST_SLIDER));
    if (right-- && (_board[right] >= _FIRST_ROOK) &&
        (COLOR(_board[right]->type) != color))
    {
      assert(_board[right]->type >= Rook);
      assert(_board[right]->type < King);
      assert(_board[right]->sqr == right);
      while (--left > _piece[2 + color].sqr) {
        assert(IS_SQUARE(left));
        assert(Direction(right, left) == West);
        if (_board[left] != _EMPTY) {
          assert(left != _piece[2 + color].sqr);
          return false;
        }
      }
      assert(left == _piece[2 + color].sqr);
      return true;
    }
  }
  else {
    assert(_piece[2 + color].sqr > right);
    left = ((_atkd[left] >> DirShift(East)) & 0xFF);
    assert(!left || (IS_SQUARE(left - 1) && (YC(left - 1) == YC(from))));
    assert(!left || (_board[left - 1] >= _FIRST_ROOK));
    if (left-- && (_board[left] >= _FIRST_ROOK) &&
        (COLOR(_board[left]->type) != color))
    {
      assert(_board[left]->type >= Rook);
      assert(_board[left]->type < King);
      assert(_board[left]->sqr == left);
      while (++right < _piece[2 + color].sqr) {
        assert(IS_SQUARE(right));
        assert(Direction(left, right) == East);
        if (_board[right] != _EMPTY) {
          assert(right != _piece[2 + color].sqr);
          return false;
        }
      }
      assert(right == _piece[2 + color].sqr);
      return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------
const char* NextWord(const char* p) {
  while (p && *p && isspace(*p)) ++p;
  return p;
}

//-----------------------------------------------------------------------------
const char* NextSpace(const char* p) {
  while (p && *p && !isspace(*p)) ++p;
  return p;
}

//-----------------------------------------------------------------------------
inline int RazorDelta(const int depth) {
  const int x = (64 * depth);
  return (500 + (x * (x / 128)));
}

//-----------------------------------------------------------------------------
inline int FutilityDelta(const int depth) {
  const int x = (200 * depth);
  return std::max<int>(x, (x * (x / 256)));
}

//-----------------------------------------------------------------------------
inline void IncHistory(const Move& move, const bool check, const int depth) {
  assert(move.IsValid());
  assert(depth >= 0);
  if (!check) {
    const int idx = move.HistoryIndex();
    const int val = (_hist[idx] + depth + 2);
    _hist[idx] = static_cast<char>(std::min<int>(val, 40));
  }
}

//-----------------------------------------------------------------------------
inline void DecHistory(const Move& move, const bool check) {
  if (!check) {
    const int idx = move.HistoryIndex();
    const int val = (_hist[idx] - 1);
    _hist[idx] = static_cast<char>(std::max<int>(val, -2));
  }
}

//-----------------------------------------------------------------------------
struct Node
{
  //---------------------------------------------------------------------------
  // unchanging
  //---------------------------------------------------------------------------
  Node* parent;
  Node* child;
  int ply;

  //---------------------------------------------------------------------------
  // updated by Exec()
  //---------------------------------------------------------------------------
  int checkState;
  int state;
  int ep;
  int rcount;
  int mcount;
  uint64_t pawnKey;
  uint64_t pieceKey;
  uint64_t positionKey;

  //---------------------------------------------------------------------------
  // updated by move generation and search
  //---------------------------------------------------------------------------
  int moveIndex;
  int moveCount;
  Move moves[MaxMoves];

  //---------------------------------------------------------------------------
  // updated during search
  //---------------------------------------------------------------------------
  bool pruneOK;
  int standPat;
  int depthChange;
  int pvCount;
  Move lastMove;
  Move killer[2];
  Move pv[MaxPlies];

  //---------------------------------------------------------------------------
  void PrintBoard() const {
    senjo::Output out(senjo::Output::NoPrefix);
    out << '\n';
    for (int y = 7; y >= 0; --y) {
      for (int x = 0; x < 8; ++x) {
        switch (_board[SQR(x,y)]->type) {
        case (White|Pawn):   out << " P"; break;
        case (White|Knight): out << " N"; break;
        case (White|Bishop): out << " B"; break;
        case (White|Rook):   out << " R"; break;
        case (White|Queen):  out << " Q"; break;
        case (White|King):   out << " K"; break;
        case (Black|Pawn):   out << " p"; break;
        case (Black|Knight): out << " n"; break;
        case (Black|Bishop): out << " b"; break;
        case (Black|Rook):   out << " r"; break;
        case (Black|Queen):  out << " q"; break;
        case (Black|King):   out << " k"; break;
        default:
          out << (((x ^ y) & 1) ? " -" : "  ");
        }
      }
      switch (y) {
      case 7:
        out << (COLOR(state) ? "  Black to move" : "  White to move");
        break;
      case 6:
        out << "  Castling Rights   : ";
        if (state & WhiteShort) out << 'K';
        if (state & WhiteLong)  out << 'Q';
        if (state & BlackShort) out << 'k';
        if (state & BlackLong)  out << 'q';
        break;
      case 5:
        if (ep != None) {
          out << "  En Passant Square : " << senjo::Square(ep).ToString();
        }
        break;
      }
      out << '\n';
    }
  }

  //---------------------------------------------------------------------------
  std::string GetFEN() const {
    char fen[256] = {0};
    char* p = fen;
    int empty = 0;
    for (int y = 7; y >= 0; --y) {
      for (int x = 0; x < 8; ++x) {
        const int type = _board[SQR(x,y)]->type;
        if ((type != 0) & (empty != 0)) {
          *p++ = ('0' + empty);
          empty = 0;
        }
        switch (type) {
        case (White|Pawn):   *p++ = 'P'; break;
        case (Black|Pawn):   *p++ = 'p'; break;
        case (White|Knight): *p++ = 'N'; break;
        case (Black|Knight): *p++ = 'n'; break;
        case (White|Bishop): *p++ = 'B'; break;
        case (Black|Bishop): *p++ = 'b'; break;
        case (White|Rook):   *p++ = 'R'; break;
        case (Black|Rook):   *p++ = 'r'; break;
        case (White|Queen):  *p++ = 'Q'; break;
        case (Black|Queen):  *p++ = 'q'; break;
        case (White|King):   *p++ = 'K'; break;
        case (Black|King):   *p++ = 'k'; break;
        default:
          empty++;
        }
      }
      if (empty) {
        *p++ = ('0' + empty);
        empty = 0;
      }
      if (y > 0) {
        *p++ = '/';
      }
    }
    *p++ = ' ';
    *p++ = (COLOR(state) ? 'b' : 'w');
    *p++ = ' ';
    if (state & CastleMask) {
      if (state & WhiteShort) *p++ = 'K';
      if (state & WhiteLong)  *p++ = 'Q';
      if (state & BlackShort) *p++ = 'k';
      if (state & BlackLong)  *p++ = 'q';
    }
    else {
      *p++ = '-';
    }
    *p++ = ' ';
    if (ep != None) {
      *p++ = ('a' + XC(ep));
      *p++ = ('1' + YC(ep));
    }
    else {
      *p++ = '-';
    }
    snprintf(p, (sizeof(fen) - strlen(fen)), " %d %d",
             rcount, ((mcount + 2) / 2));
    return fen;
  }

  //---------------------------------------------------------------------------
  inline void AddMove(const Color color, const MoveType type,
                      const int from, const int to,
                      const int cap = 0, const int promo = 0)
  {
    assert(IS_MTYPE(type));
    assert(IS_SQUARE(from));
    assert(IS_SQUARE(to));
    assert(from != to);
    assert(moveCount >= 0);
    assert((moveCount + 1) < MaxMoves);
    if (_pcount[!color]) {
      assert(_pcount[!color] > 0);
      if (_pinDir[from] && (_pinDir[from] != abs(Direction(from, to)))) {
        return;
      }
    }
    moves[moveCount++].Set(type, from, to, cap, promo);
  }

  //---------------------------------------------------------------------------
  template<Color color>
  int GetCheckEvasions() {
    assert((2 + color) == (color ? BlackKingOffset : WhiteKingOffset));
    assert((_piece + 2 + color) == (color ? _BLACK_KING : _WHITE_KING));
    assert(_piece[2 + color].type == (color|King));
    assert(IS_SQUARE(_piece[2 + color].sqr));
    assert(_board[_piece[2 + color].sqr] == (_piece + 2 + color));
    int from = _piece[2 + color].sqr;
    int attackers = 0;
    int squareCount = 0;
    int squares[40];
    int xrayCount = 0;
    int xray[4] = {-1,-1,-1,-1};

    if (_pcount[!color]) {
      assert(_pcount[!color] > 0);
      memset(_pinDir, 0, sizeof(_pinDir));
    }

    if (_pcount[(!color)|Knight]) {
      assert(_pcount[(!color)|Knight] > 0);
      for (uint64_t mvs = _knightMoves[from]; mvs; mvs >>=8) {
        assert(mvs & 0xFF);
        const int to = ((mvs & 0xFF) - 1);
        assert(IS_SQUARE(to));
        assert(to != from);
        if (_board[to]->type == ((!color)|Knight)) {
          assert(_board[to]->sqr == to);
          attackers++;
          assert(squareCount < 40);
          squares[squareCount++] = to;
        }
      }
    }

    for (uint64_t mvs = _queenKing[from]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int end = ((mvs & 0xFF) - 1);
      const int dir = Direction(from, end);
      assert(IS_DIR(dir));
      int newSquareCount = squareCount;
      int firstPiece = 0;
      int to = (from + dir);
      while (true) {
        assert(IS_SQUARE(to));
        assert(Direction(from, to) == dir);
        assert(newSquareCount < 40);
        squares[newSquareCount++] = to;
        if ((to == end) || (_board[to] != _EMPTY)) {
          firstPiece = _board[to]->type;
          assert(_EMPTY->type == 0);
          break;
        }
        to += dir;
      }

      assert(newSquareCount > squareCount);
      assert(IS_SQUARE(to));
      if (!firstPiece) {
        assert(_board[to] == _EMPTY);
        continue;
      }

      assert(_board[to] != _EMPTY);
      assert(firstPiece == _board[to]->type);
      if (COLOR(firstPiece) == color) {
        if (_atkd[to]) {
          const int afrom = ((_atkd[to] >> DirShift(-dir)) & 0xFF);
          if (afrom) {
            assert(IS_SQUARE(afrom - 1));
            assert(_board[afrom - 1] >= _FIRST_SLIDER);
            assert(IS_SLIDER(_board[afrom - 1]->type));
            assert(_board[afrom - 1]->sqr == (afrom - 1));
            if (COLOR(_board[afrom - 1]->type) != color) {
              _pinDir[to] = abs(dir);
            }
          }
        }
        continue;
      }

      switch (firstPiece) {
      case ((!color)|Pawn):
        if (Distance(from, to) == 1) {
          if (color) {
            switch (dir) {
            case SouthWest: case SouthEast:
              squareCount = newSquareCount;
              attackers++;
            }
          }
          else {
            switch (dir) {
            case NorthWest: case NorthEast:
              squareCount = newSquareCount;
              attackers++;
            }
          }
        }
        break;
      case ((!color)|Bishop):
        switch (dir) {
        case SouthWest: case SouthEast: case NorthWest: case NorthEast:
          if (Distance(from, to) > 1) {
            assert(xrayCount < 4);
            xray[xrayCount++] = (from + dir);
          }
          assert(xrayCount < 4);
          xray[xrayCount++] = (from - dir);
          squareCount = newSquareCount;
          attackers++;
        }
        break;
      case ((!color)|Rook):
        switch (dir) {
        case South: case West: case East: case North:
          if (Distance(from, to) > 1) {
            assert(xrayCount < 4);
            xray[xrayCount++] = (from + dir);
          }
          assert(xrayCount < 4);
          xray[xrayCount++] = (from - dir);
          squareCount = newSquareCount;
          attackers++;
        }
        break;
      case ((!color)|Queen):
        if (Distance(from, to) > 1) {
          assert(xrayCount < 4);
          xray[xrayCount++] = (from + dir);
        }
        assert(xrayCount < 4);
        xray[xrayCount++] = (from - dir);
        squareCount = newSquareCount;
        attackers++;
        break;
      }
    }

    assert(attackers < 3);
    if (!attackers) {
      return 0;
    }

    assert(!_piece[0].type);
    if (attackers == 1) {
      // get non-king moves that block or capture the piece giving check
      for (int z = 0; z < squareCount; ++z) {
        const int to = squares[z];
        assert(IS_SQUARE(to));
        const int cap = _board[to]->type;
        assert(cap || (_board[to] == _EMPTY));
        if ((cap == ((!color)|Pawn)) &&
            (ep == (to + (color ? South : North))))
        {
          if (XC(ep) > 0) {
            from = (ep + (color ? NorthWest : SouthWest));
            if ((_board[from]->type == (color|Pawn)) &&
                !EpPinned<color>(from, to))
            {
              AddMove(color, PawnCap, from, ep);
            }
          }
          if (XC(ep) < 7) {
            from = (ep + (color ? NorthEast : SouthEast));
            if ((_board[from]->type == (color|Pawn)) &&
                !EpPinned<color>(from, to))
            {
              AddMove(color, PawnCap, from, ep);
            }
          }
        }

        if (_pcount[color|Knight]) {
          assert(_pcount[color|Knight] > 0);
          for (uint64_t mvs = _knightMoves[to]; mvs; mvs >>= 8) {
            assert(mvs & 0xFF);
            from = ((mvs & 0xFF) - 1);
            assert(IS_SQUARE(from));
            assert(from != to);
            if (_board[from]->type == (color|Knight)) {
              assert(_board[from]->sqr == from);
              AddMove(color, KnightMove, from, to, cap);
            }
          }
        }

        for (uint64_t mvs = _bishopRook[to]; mvs; mvs >>= 8) {
          assert(mvs & 0xFF);
          const int end = ((mvs & 0xFF) - 1);
          const int dir = Direction(to, end);
          assert(IS_DIAG(dir));
          for (from = (to + dir);; from += dir) {
            assert(IS_SQUARE(from));
            assert(Direction(to, from) == dir);
            switch (_board[from]->type) {
            case (color|Pawn):
              if (cap && (color ? (from > to) : (from < to)) &&
                  (Distance(from, to) == 1))
              {
                if (YC(to) == (color ? 0 : 7)) {
                  AddMove(color, PawnCap, from, to, cap, (color|Queen));
                  AddMove(color, PawnCap, from, to, cap, (color|Rook));
                  AddMove(color, PawnCap, from, to, cap, (color|Bishop));
                  AddMove(color, PawnCap, from, to, cap, (color|Knight));
                }
                else {
                  AddMove(color, PawnCap, from, to, cap);
                }
              }
              break;
            case (color|Bishop):
              AddMove(color, BishopMove, from, to, cap);
              break;
            case (color|Queen):
              AddMove(color, QueenMove, from, to, cap);
              break;
            }
            if ((from == end) || (_board[from] != _EMPTY)) {
              break;
            }
          }
        }

        for (uint64_t mvs = _bishopRook[to + 8]; mvs; mvs >>= 8) {
          assert(mvs & 0xFF);
          const int end = ((mvs & 0xFF) - 1);
          const int dir = Direction(to, end);
          assert(IS_CROSS(dir));
          for (from = (to + dir);; from += dir) {
            assert(IS_SQUARE(from));
            assert(Direction(to, from) == dir);
            switch (_board[from]->type) {
            case (color|Pawn):
              if (!cap && (dir == (color ? North : South))) {
                if (Distance(from, to) == 1) {
                  if (YC(to) == (color ? 0 : 7)) {
                    AddMove(color, PawnMove, from, to, 0, (color|Queen));
                    AddMove(color, PawnMove, from, to, 0, (color|Rook));
                    AddMove(color, PawnMove, from, to, 0, (color|Bishop));
                    AddMove(color, PawnMove, from, to, 0, (color|Knight));
                  }
                  else {
                    AddMove(color, PawnMove, from, to);
                  }
                }
                else if ((Distance(from, to) == 2) &&
                         (YC(to) == (color ? 4 : 3)))
                {
                  AddMove(color, PawnLung, from, to);
                }
              }
              break;
            case (color|Rook):
              AddMove(color, RookMove, from, to, cap);
              break;
            case (color|Queen):
              AddMove(color, QueenMove, from, to, cap);
              break;
            }
            if ((from == end) || (_board[from] != _EMPTY)) {
              break;
            }
          }
        }
      }
    }

    // get king moves
    from = _piece[2 + color].sqr;
    for (uint64_t mvs = _queenKing[from + 8]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(IS_SQUARE(to));
      assert(to != from);
      if ((to == xray[0]) || (to == xray[1]) ||
          (to == xray[2]) || (to == xray[3]) ||
          AttackedBy<!color>(to))
      {
        continue;
      }

      const int cap = _board[to]->type;
      assert(cap || (_board[to] == _EMPTY));
      if (!cap) {
        AddMove(color, KingMove, from, to);
      }
      else if (COLOR(cap) != color) {
        AddMove(color, KingMove, from, to, cap);
      }
    }
    return attackers;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  void GetPawnMoves(const int from) {
    assert(IS_SQUARE(from));
    assert(_board[from] != _EMPTY);
    assert(_board[from]->type == (color|Pawn));
    assert(_board[from]->sqr == from);
    int to;
    for (uint64_t mvs = _pawnCaps[from + (color * 8)]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      to = ((mvs & 0xFF) - 1);
      assert(Distance(from, to) == 1);
      const int cap = _board[to]->type;
      assert(cap || (_board[to] == _EMPTY));
      if (!cap) {
        if ((ep != None) && (to == ep) &&
            !EpPinned<color>(from, (ep + (color ? North : South))))
        {
          AddMove(color, PawnCap, from, to);
        }
      }
      else if (COLOR(cap) != color) {
        if (YC(to) == (color ? 0 : 7)) {
          AddMove(color, PawnCap, from, to, cap, (color|Queen));
          AddMove(color, PawnCap, from, to, cap, (color|Rook));
          AddMove(color, PawnCap, from, to, cap, (color|Bishop));
          AddMove(color, PawnCap, from, to, cap, (color|Knight));
        }
        else {
          AddMove(color, PawnCap, from, to, cap);
        }
      }
    }
    to = (from + (color ? South : North));
    assert(IS_SQUARE(to));
    if (_board[to] == _EMPTY) {
      if (YC(to) == (color ? 0 : 7)) {
        AddMove(color, PawnMove, from, to, 0, (color|Queen));
        AddMove(color, PawnMove, from, to, 0, (color|Rook));
        AddMove(color, PawnMove, from, to, 0, (color|Bishop));
        AddMove(color, PawnMove, from, to, 0, (color|Knight));
      }
      else {
        AddMove(color, PawnMove, from, to);
        if (YC(from) == (color ? 6 : 1)) {
          to += (color ? South : North);
          assert(IS_SQUARE(to));
          if (_board[to] == _EMPTY) {
            AddMove(color, PawnLung, from, to);
          }
        }
      }
    }
  }

  //---------------------------------------------------------------------------
  void GetKnightMoves(const Color color, const int from) {
    assert(IS_SQUARE(from));
    assert(_board[from] != _EMPTY);
    assert(_board[from]->type == (color|Knight));
    assert(_board[from]->sqr == from);
    for (uint64_t mvs = _knightMoves[from]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(IS_SQUARE(to));
      assert(to != from);
      const int cap = _board[to]->type;
      assert(cap || (_board[to] == _EMPTY));
      if (!cap) {
        AddMove(color, KnightMove, from, to);
      }
      else if (COLOR(cap) != color) {
        AddMove(color, KnightMove, from, to, cap);
      }
    }
  }

  //---------------------------------------------------------------------------
  void GetSliderMoves(const Color color, const MoveType type,
                      uint64_t mvs, const int from)
  {
    assert(IS_SQUARE(from));
    assert(_board[from] != _EMPTY);
    assert(_board[from]->type == (color|type));
    assert(_board[from]->sqr == from);
    while (mvs) {
      assert(mvs & 0xFF);
      const int end = ((mvs & 0xFF) - 1);
      const int dir = (Direction(from, end));
      assert(IS_DIR(dir));
      for (int to = (from + dir);; to += dir) {
        assert(IS_SQUARE(to));
        assert(Direction(from, to) == dir);
        const int cap = _board[to]->type;
        assert(cap || (_board[to] == _EMPTY));
        if (!cap) {
          AddMove(color, type, from, to);
        }
        else {
          if (COLOR(cap) != color) {
            AddMove(color, type, from, to, cap);
          }
          break;
        }
        if (to == end) {
          break;
        }
      }
      mvs >>= 8;
    }
  }

  //---------------------------------------------------------------------------
  template<Color color>
  void GetKingMoves(const int from) {
    assert((2 + color) == (color ? BlackKingOffset : WhiteKingOffset));
    assert((_piece + 2 + color) == (color ? _BLACK_KING : _WHITE_KING));
    assert(_piece[2 + color].type == (color|King));
    assert(IS_SQUARE(_piece[2 + color].sqr));
    assert(_board[_piece[2 + color].sqr] == (_piece + 2 + color));
    assert(IS_SQUARE(from));
    assert(!AttackedBy<!color>(from));
    for (uint64_t mvs = _queenKing[from + 8]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(Distance(from, to) == 1);
      if (!AttackedBy<!color>(to)) {
        const int cap = _board[to]->type;
        assert(cap || (_board[to] == _EMPTY));
        if (!cap) {
          AddMove(color, KingMove, from, to);
          if ((to == (color ? F8 : F1)) &&
              (state & (color ? BlackShort : WhiteShort)) &&
              (_board[color ? G8 : G1] == _EMPTY) &&
              !AttackedBy<!color>(color ? G8 : G1))
          {
            assert(from == (color ? E8 : E1));
            assert(_board[color ? H8 : H1]->type == (color|Rook));
            AddMove(color, CastleShort, from, (color ? G8 : G1));
          }
          else if ((to == (color ? D8 : D1)) &&
                   (state & (color ? BlackLong : WhiteLong)) &&
                   (_board[color ? C8 : C1] == _EMPTY) &&
                   (_board[color ? B8 : B1] == _EMPTY) &&
                   !AttackedBy<!color>(color ? C8 : C1))
          {
            assert(from == (color ? E8 : E1));
            assert(_board[color ? A8 : A1]->type == (color|Rook));
            AddMove(color, CastleLong, from, (color ? C8 : C1));
          }
        }
        else if (COLOR(cap) != color) {
          AddMove(color, KingMove, from, to, cap);
        }
      }
    }
  }

  //---------------------------------------------------------------------------
  template<Color color>
  void GenerateMoves() {
    assert((2 + color) == (color ? BlackKingOffset : WhiteKingOffset));
    assert((_piece + WhiteKingOffset) == _WHITE_KING);
    assert((_piece + BlackKingOffset) == _BLACK_KING);
    assert(_WHITE_KING->type == (White|King));
    assert(_BLACK_KING->type == (Black|King));
    assert(IS_SQUARE(_WHITE_KING->sqr));
    assert(IS_SQUARE(_BLACK_KING->sqr));
    assert(_board[_WHITE_KING->sqr] == _WHITE_KING);
    assert(_board[_BLACK_KING->sqr] == _BLACK_KING);
    assert((_pcount[White] >= 0) & (_pcount[White] <= 13));
    assert((_pcount[Black] >= 0) & (_pcount[Black] <= 13));
    assert(_pcount[White] == (_pcount[White|Bishop] + _pcount[White|Rook] +
                              _pcount[White|Queen]));
    assert(_pcount[Black] == (_pcount[Black|Bishop] + _pcount[Black|Rook] +
                              _pcount[Black|Queen]));
    assert((_pcount[White|Pawn] >= 0) & (_pcount[White|Pawn] <= 8));
    assert((_pcount[Black|Pawn] >= 0) & (_pcount[Black|Pawn] <= 8));
    assert((_pcount[White|Knight] >= 0) & (_pcount[White|Knight] <= 10));
    assert((_pcount[Black|Knight] >= 0) & (_pcount[Black|Knight] <= 10));
    assert((_pcount[White|Bishop] >= 0) & (_pcount[White|Bishop] <= 10));
    assert((_pcount[Black|Bishop] >= 0) & (_pcount[Black|Bishop] <= 10));
    assert((_pcount[White|Rook] >= 0) & (_pcount[White|Rook] <= 10));
    assert((_pcount[Black|Rook] >= 0) & (_pcount[Black|Rook] <= 10));
    assert((_pcount[White|Queen] >= 0) & (_pcount[White|Queen] <= 9));
    assert((_pcount[Black|Queen] >= 0) & (_pcount[Black|Queen] <= 9));

    moveCount = moveIndex = 0;
    if (GetCheckEvasions<color>()) {
      return;
    }

    int from;
    for (int i = 0; i < _pcount[color|Pawn]; ++i) {
      from = _piece[(color ? BlackPawnOffset : PawnOffset) + i].sqr;
      GetPawnMoves<color>(from);
    }

    for (int i = 0; i < _pcount[color|Knight]; ++i) {
      from = _piece[(color ? BlackKnightOffset : KnightOffset) + i].sqr;
      GetKnightMoves(color, from);
    }

    for (int i = 0; i < _pcount[color|Bishop]; ++i) {
      from = _piece[(color ? BlackBishopOffset : BishopOffset) + i].sqr;
      GetSliderMoves(color, BishopMove, _bishopRook[from], from);
    }

    for (int i = 0; i < _pcount[color|Rook]; ++i) {
      from = _piece[(color ? BlackRookOffset : RookOffset) + i].sqr;
      GetSliderMoves(color, RookMove, _bishopRook[from + 8], from);
    }

    for (int i = 0; i < _pcount[color|Queen]; ++i) {
      from = _piece[(color ? BlackQueenOffset : QueenOffset) + i].sqr;
      GetSliderMoves(color, QueenMove, _queenKing[from], from);
    }

    from = _piece[2 + color].sqr;
    GetKingMoves<color>(from);
  }

  //---------------------------------------------------------------------------
  inline void ScootMoveToFront(int idx) {
    assert((idx >= 0) & (idx < moveCount));
    while (idx-- > 0) {
      moves[idx].SwapWith(moves[idx + 1]);
    }
  }

  //---------------------------------------------------------------------------
  inline Move* GetNextMove() {
    assert(moveIndex >= 0);
    assert((moveCount >= 0) & (moveCount < MaxMoves));

    if (moveIndex >= moveCount) {
      return NULL;
    }

    Move* best = (moves + moveIndex);
    for (int i = (moveIndex + 1); i < moveCount; ++i) {
      if (moves[i].GetScore() > best->GetScore()) {
        best = (moves + i);
      }
    }
    if (best != (moves + moveIndex)) {
      moves[moveIndex].SwapWith(*best);
    }

    return (moves + moveIndex++);
  }

  //---------------------------------------------------------------------------
  template<Color color>
  int ValidateMove(const Move& move) const {
#ifdef NDEBUG
    int test = 0;
#define VASSERT(x) ++test; if (!(x)) return test;
#else
#define VASSERT(x) if (!(x)) { \
    senjo::Output() << move.ToString(); \
    assert(false); \
  }
#endif

    VASSERT(COLOR(state) == color);
    VASSERT(move.IsValid());
    VASSERT(IS_SQUARE(move.From()));
    VASSERT(IS_SQUARE(move.To()));
    VASSERT(!move.Cap() || IS_CAP(move.Cap()));
    VASSERT(!move.Cap() || (COLOR(move.Cap()) != color));
    VASSERT(!move.Promo() || IS_PROMO(move.Promo()));
    VASSERT(!move.Promo() || (COLOR(move.Promo()) == color));

    const int from  = move.From();
    const int to    = move.To();
    const int cap   = move.Cap();
    const int promo = move.Promo();

    Piece* moved = _board[from];
    VASSERT(moved && (moved != _EMPTY));
    VASSERT(moved->type);
    VASSERT(moved->sqr == from);

    const int pc = moved->type;
    VASSERT(COLOR(pc) == color);

    switch (move.Type()) {
    case PawnMove:
      VASSERT(pc == (color|Pawn));
      VASSERT(_board[to] == _EMPTY);
      VASSERT(!cap);
      VASSERT((YC(from) != 0) & (YC(from) != 7));
      VASSERT(Distance(from, to) == 1);
      VASSERT(Direction(from, to) == (color ? South : North));
      if (promo) {
        VASSERT(IS_PROMO(promo));
        VASSERT(COLOR(promo) == color);
        VASSERT(YC(to) == (color ? 0 : 7));
      }
      else {
        VASSERT(YC(to) != (color ? 0 : 7));
      }
      break;
    case PawnLung:
      VASSERT(pc == (color|Pawn));
      VASSERT(YC(from) == (color ? 6 : 1));
      VASSERT(Distance(from, to) == 2);
      VASSERT(Direction(from, to) == (color ? South : North));
      VASSERT(_board[to] == _EMPTY);
      VASSERT(_board[to + (color ? North : South)] == _EMPTY);
      VASSERT(!cap);
      VASSERT(!promo);
      break;
    case PawnCap:
      VASSERT(pc == (color|Pawn));
      VASSERT(Distance(from, to) == 1);
      if (promo) {
        VASSERT(IS_PROMO(promo));
        VASSERT(COLOR(promo) == color);
        VASSERT(YC(to) == (color ? 0 : 7));
        VASSERT(cap);
        VASSERT(_board[to] != _EMPTY);
        VASSERT(cap == _board[to]->type);
        VASSERT(_board[to]->sqr == to);
      }
      else {
        VASSERT((YC(from) != 0) & (YC(from) != 7));
        VASSERT(YC(to) != (color ? 0 : 7));
        if (cap) {
          VASSERT(_board[to] != _EMPTY);
          VASSERT(cap == _board[to]->type);
          VASSERT(_board[to]->sqr == to);
        }
        else {
          VASSERT(_board[to] == _EMPTY);
          VASSERT((ep != None) && (to == ep));
          const int sqr = (ep + (color ? North : South));
          VASSERT(IS_SQUARE(sqr));
          VASSERT(_board[sqr] != _EMPTY);
          VASSERT(_board[sqr]->type == ((!color)|Pawn));
        }
      }
      break;
    case KnightMove:
    case BishopMove:
    case RookMove:
    case QueenMove:
    case KingMove:
      VASSERT(pc == (color|move.Type()));
      VASSERT(cap ? (_board[to]->type == cap) : (_board[to] == _EMPTY));
      VASSERT(!promo);
      switch (pc) {
      case (color|Knight):
        switch (to - from) {
        case KnightMove1: case KnightMove2: case KnightMove3: case KnightMove4:
        case KnightMove5: case KnightMove6: case KnightMove7: case KnightMove8:
          break;
        default:
          VASSERT(false);
        }
        break;
      case (color|Bishop):
        switch (Direction(from, to)) {
        case SouthWest: case SouthEast: case NorthWest: case NorthEast:
          break;
        default:
          VASSERT(false);
        }
        break;
      case (color|Rook):
        switch (Direction(from, to)) {
        case South: case West: case East: case North:
          break;
        default:
          VASSERT(false);
        }
        break;
      case (color|King):
        VASSERT(Distance(from, to) == 1);
        VASSERT(!AttackedBy<!color>(to));
      case (color|Queen):
        switch (Direction(from, to)) {
        case SouthWest: case South: case SouthEast: case West:
        case East: case NorthWest: case North: case NorthEast:
          break;
        default:
          VASSERT(false);
        }
        break;
      default:
        VASSERT(false);
      }
      break;
    case CastleShort:
      VASSERT(state & (color ? BlackShort : WhiteShort));
      VASSERT(moved == (color ? _BLACK_KING : _WHITE_KING));
      VASSERT(from == (color ? E8 : E1));
      VASSERT(to == (color ? G8 : G1));
      VASSERT(!cap);
      VASSERT(!promo);
      VASSERT(_board[color ? H8 : H1] != _EMPTY);
      VASSERT(_board[color ? H8 : H1]->type == (color|Rook));
      VASSERT(_board[color ? F8 : F1] == _EMPTY);
      VASSERT(_board[color ? G8 : G1] == _EMPTY);
      VASSERT(!AttackedBy<!color>(color ? E8 : E1));
      VASSERT(!AttackedBy<!color>(color ? F8 : F1));
      VASSERT(!AttackedBy<!color>(color ? G8 : G1));
      break;
    case CastleLong:
      VASSERT(state & (color ? BlackLong : WhiteLong));
      VASSERT(moved == (color ? _BLACK_KING : _WHITE_KING));
      VASSERT(from == (color ? E8 : E1));
      VASSERT(to == (color ? C8 : C1));
      VASSERT(!cap);
      VASSERT(!promo);
      VASSERT(_board[color ? A8 : A1] != _EMPTY);
      VASSERT(_board[color ? A8 : A1]->type == (color|Rook));
      VASSERT(_board[color ? B8 : B1] == _EMPTY);
      VASSERT(_board[color ? C8 : C1] == _EMPTY);
      VASSERT(_board[color ? D8 : D1] == _EMPTY);
      VASSERT(!AttackedBy<!color>(color ? C8 : C1));
      VASSERT(!AttackedBy<!color>(color ? D8 : D1));
      VASSERT(!AttackedBy<!color>(color ? E8 : E1));
      break;
    default:
      VASSERT(false);
    }
    return 0;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  void Exec(const Move& move, Node& dest) const {
    assert(ValidateMove<color>(move) == 0);

    _execs++;

    const int from  = move.From();
    const int to    = move.To();
    const int cap   = move.Cap();
    const int promo = move.Promo();

    Piece* moved = _board[from];
    const int pc = moved->type;

    if (IS_SLIDER(pc)) {
      ClearAttacksFrom(pc, from);
    }
    if (IS_SLIDER(cap)) {
      ClearAttacksFrom(cap, to);
    }

    switch (move.Type()) {
    case PawnMove:
      if (promo) {
        RemovePiece((color|Pawn), from);
        AddPiece(promo, to);
        _material[color] += (ValueOf(promo) - PawnValue);
        dest.pawnKey = (pawnKey ^ _HASH[pc][from]);
        dest.pieceKey = (pieceKey ^ _HASH[promo][to]);
      }
      else {
        _board[from] = _EMPTY;
        _board[to] = moved;
        moved->sqr = to;
        dest.pawnKey = (pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
        dest.pieceKey = pieceKey;
      }
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = None;
      dest.rcount = 0;
      dest.mcount = (mcount + 1);
      break;
    case PawnLung:
      _board[from] = _EMPTY;
      _board[to] = moved;
      moved->sqr = to;
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = (to + (color ? North : South));
      dest.rcount = 0;
      dest.mcount = (mcount + 1);
      dest.pawnKey = (pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
      dest.pieceKey = pieceKey;
      break;
    case PawnCap:
      if (promo) {
        RemovePiece((color|Pawn), from);
        RemovePiece(cap, to);
        AddPiece(promo, to);
        _material[color] += (ValueOf(promo) - PawnValue);
        _material[!color] -= ValueOf(cap);
        dest.pawnKey = (pawnKey ^ _HASH[pc][from]);
        dest.pieceKey = (pieceKey ^ _HASH[promo][to] ^ _HASH[cap][to]);
      }
      else {
        _board[from] = _EMPTY;
        if (cap >= Knight) {
          RemovePiece(cap, to);
          _board[to] = moved;
          moved->sqr = to;
          _material[!color] -= ValueOf(cap);
          dest.pawnKey = (pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
          dest.pieceKey = (pieceKey ^ _HASH[cap][to]);
        }
        else if (cap) {
          RemovePiece(((!color)|Pawn), to);
          _board[to] = moved;
          moved->sqr = to;
          _material[!color] -= ValueOf(cap);
          dest.pawnKey = (pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to] ^
                          _HASH[cap][to]);
          dest.pieceKey = pieceKey;
        }
        else {
          const int sqr = (ep + (color ? North : South));
          RemovePiece(((!color)|Pawn), sqr);
          _board[to] = moved;
          moved->sqr = to;
          _material[!color] -= PawnValue;
          if (_atkd[sqr]) {
            ExtendAttacks(sqr);
          }
          dest.pawnKey = (pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to] ^
                          _HASH[(!color)|Pawn][sqr]);
          dest.pieceKey = pieceKey;
        }
      }
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = None;
      dest.rcount = 0;
      dest.mcount = (mcount + 1);
      break;
    case KnightMove:
    case BishopMove:
    case RookMove:
    case QueenMove:
    case KingMove:
      _board[from] = _EMPTY;
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = None;
      dest.rcount = (cap ? 0 : (rcount + 1));
      dest.mcount = (mcount + 1);
      if (cap >= Knight) {
        RemovePiece(cap, to);
        _material[!color] -= ValueOf(cap);
        dest.pawnKey = pawnKey;
        dest.pieceKey = (pieceKey ^ _HASH[pc][from] ^ _HASH[pc][to] ^
                         _HASH[cap][to]);
      }
      else if (cap) {
        RemovePiece(((!color)|Pawn), to);
        _material[!color] -= PawnValue;
        dest.pawnKey = (pawnKey ^ _HASH[cap][to]);
        dest.pieceKey = (pieceKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
      }
      else {
        dest.pawnKey = pawnKey;
        dest.pieceKey = (pieceKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
      }
      _board[to] = moved;
      moved->sqr = to;
      break;
    case CastleShort:
      ClearAttacksFrom((color|Rook), (color ? H8 : H1));
      _board[color ? E8 : E1] = _EMPTY;
      _board[color ? G8 : G1] = moved;
      _board[color ? F8 : F1] = _board[color ? H8 : H1];
      _board[color ? H8 : H1] = _EMPTY;
      moved->sqr = to;
      _board[color ? F8 : F1]->sqr = (color ? F8 : F1);
      AddAttacksFrom((color|Rook), (color ? F8 : F1));
      dest.state = ((state ^ 1) & ~(color ? BlackCastle : WhiteCastle));
      dest.ep = None;
      dest.rcount = (rcount + 1);
      dest.mcount = (mcount + 1);
      dest.pawnKey = pawnKey;
      dest.pieceKey = (pieceKey ^
                       _HASH[color|Rook][color ? F8 : F1] ^
                       _HASH[color|Rook][color ? H8 : H1] ^
                       _HASH[color|King][color ? E8 : E1] ^
                       _HASH[color|King][color ? G8 : G1]);
      break;
    case CastleLong:
      ClearAttacksFrom((color|Rook), (color ? A8 : A1));
      _board[color ? D8 : D1] = _board[color ? A8 : A1];
      _board[color ? A8 : A1] = _EMPTY;
      _board[color ? C8 : C1] = moved;
      _board[color ? E8 : E1] = _EMPTY;
      moved->sqr = to;
      _board[color ? D8 : D1]->sqr = (color ? D8 : D1);
      AddAttacksFrom((color|Rook), (color ? D8 : D1));
      dest.state = ((state ^ 1) & ~(color ? BlackCastle : WhiteCastle));
      dest.ep = None;
      dest.rcount = (rcount + 1);
      dest.mcount = (mcount + 1);
      dest.pawnKey = pawnKey;
      dest.pieceKey = (pieceKey ^
                       _HASH[color|Rook][color ? A8 : A1] ^
                       _HASH[color|Rook][color ? D8 : D1] ^
                       _HASH[color|King][color ? C8 : C1] ^
                       _HASH[color|King][color ? E8 : E1]);
      break;
    }

    dest.positionKey = (dest.pawnKey ^
                        dest.pieceKey ^
                        _HASH[0][dest.state & 0x1F] ^
                        _HASH[0][dest.ep]);

    if (!cap && _atkd[to]) {
      TruncateAttacks(to, from);
    }
    if (_atkd[from]) {
      ExtendAttacks(from);
    }
    if (_board[to] >= _FIRST_SLIDER) {
      assert(IS_SLIDER(_board[to]->type));
      assert(_board[to]->sqr == to);
      AddAttacksFrom(_board[to]->type, to);
    }

//    assert(VerifyAttacks(true));
  }

  //---------------------------------------------------------------------------
  template<Color color>
  void Undo(const Move& move) const {
    assert(COLOR(state) == color);
    assert(move.IsValid());
    assert(IS_SQUARE(move.From()));
    assert(IS_SQUARE(move.To()));
    assert(_board[move.From()] == _EMPTY);
    assert(!move.Cap() || IS_CAP(move.Cap()));
    assert(!move.Cap() || (COLOR(move.Cap()) != color));
    assert(!move.Promo() || IS_PROMO(move.Promo()));
    assert(!move.Promo() || (COLOR(move.Promo()) == color));

    const int from  = move.From();
    const int to    = move.To();
    const int cap   = move.Cap();
    const int promo = move.Promo();

    Piece* moved = _board[to];
    assert(moved && (moved != _EMPTY));
    assert(moved->type);
    assert(COLOR(moved->type) == color);
    assert(moved->sqr == to);

    switch (move.Type()) {
    case PawnCap:
      if (promo) {
        assert(moved->type == promo);
        assert(cap);
        if (promo >= Bishop) {
          ClearAttacksFrom(promo, to);
        }
        RemovePiece(promo, to);
        AddPiece(cap, to);
        AddPiece((color|Pawn), from);
        _material[color] += (PawnValue - ValueOf(promo));
        _material[!color] += ValueOf(cap);
        if (_atkd[from]) {
          TruncateAttacks(from, to);
        }
        if (cap >= Bishop) {
          AddAttacksFrom(cap, to);
        }
      }
      else if (cap) {
        _board[from] = moved;
        moved->sqr = from;
        AddPiece(cap, to);
        _material[!color] += ValueOf(cap);
        if (_atkd[from]) {
          TruncateAttacks(from, to);
        }
        if (cap >= Bishop) {
          AddAttacksFrom(cap, to);
        }
      }
      else {
        assert(moved->type == (color|Pawn));
        assert((ep != None) && (to == ep));
        const int sqr = (ep + (color ? North : South));
        assert(_board[sqr] == _EMPTY);
        if (_atkd[sqr]) {
          TruncateAttacks(sqr, -1);
        }
        AddPiece(((!color)|Pawn), sqr);
        _board[to] = _EMPTY;
        _board[from] = moved;
        moved->sqr = from;
        _material[!color] += PawnValue;
        if (_atkd[from]) {
          TruncateAttacks(from, to);
        }
        if (_atkd[to]) {
          ExtendAttacks(to);
        }
        if (_atkd[sqr]) {
          TruncateAttacks(sqr, from);
        }
      }
      break;
    case CastleShort:
      assert(moved == (color ? _BLACK_KING : _WHITE_KING));
      assert(from == (color ? E8 : E1));
      assert(to == (color ? G8 : G1));
      assert(!cap);
      assert(!promo);
      assert(_board[color ? E8 : E1] == _EMPTY);
      assert(_board[color ? H8 : H1] == _EMPTY);
      assert(_board[color ? F8 : F1] != _EMPTY);
      assert(_board[color ? G8 : G1] != _EMPTY);
      assert(_board[color ? F8 : F1]->type == (color|Rook));
      ClearAttacksFrom((color|Rook), (color ? F8 : F1));
      _board[color ? E8 : E1] = moved;
      _board[color ? G8 : G1] = _EMPTY;
      _board[color ? H8 : H1] = _board[color ? F8 : F1];
      _board[color ? F8 : F1] = _EMPTY;
      moved->sqr = from;
      _board[color ? H8 : H1]->sqr = (color ? H8 : H1);
      if (_atkd[from]) {
        TruncateAttacks(from, to);
      }
      AddAttacksFrom((color|Rook), (color ? H8 : H1));
      break;
    case CastleLong:
      assert(moved == (color ? _BLACK_KING : _WHITE_KING));
      assert(from == (color ? E8 : E1));
      assert(to == (color ? C8 : C1));
      assert(!cap);
      assert(!promo);
      assert(_board[color ? E8 : E1] == _EMPTY);
      assert(_board[color ? A8 : A1] == _EMPTY);
      assert(_board[color ? B8 : B1] == _EMPTY);
      assert(_board[color ? C8 : C1] != _EMPTY);
      assert(_board[color ? D8 : D1] != _EMPTY);
      assert(_board[color ? D8 : D1]->type == (color|Rook));
      ClearAttacksFrom((color|Rook), (color ? D8 : D1));
      _board[color ? A8 : A1] = _board[color ? D8 : D1];
      _board[color ? D8 : D1] = _EMPTY;
      _board[color ? E8 : E1] = moved;
      _board[color ? C8 : C1] = _EMPTY;
      moved->sqr = from;
      _board[color ? A8 : A1]->sqr = (color ? A8 : A1);
      if (_atkd[from]) {
        TruncateAttacks(from, to);
      }
      AddAttacksFrom((color|Rook), (color ? A8 : A1));
      break;
    default:
      if (moved >= _FIRST_SLIDER) {
        ClearAttacksFrom(moved->type, to);
      }
      if (promo) {
        assert(move.Type() == PawnMove);
        assert(moved->type == promo);
        RemovePiece(promo, to);
        AddPiece((color|Pawn), from);
        _material[color] += (PawnValue - ValueOf(promo));
      }
      else {
        _board[from] = moved;
        moved->sqr = from;
      }
      if (cap) {
        AddPiece(cap, to);
        _material[!color] += ValueOf(cap);
      }
      else {
        _board[to] = _EMPTY;
      }
      if (_atkd[from]) {
        TruncateAttacks(from, to);
      }
      if (cap >= Bishop) {
        AddAttacksFrom(cap, to);
      }
      else if (!cap && _atkd[to]) {
        ExtendAttacks(to);
      }
      if (_board[from] >= _FIRST_SLIDER) {
        AddAttacksFrom(_board[from]->type, from);
      }
    }

//    assert(VerifyAttacks(true));
  }

  //---------------------------------------------------------------------------
  template<Color color>
  inline void ExecNullMove(Node& dest) const {
    assert(COLOR(state) == color);
    assert(checkState == NotInCheck);
    assert(&dest != this);

    _stats.nullMoves++;

    dest.lastMove.Clear();
    dest.mcount = mcount;
    dest.rcount = rcount;
    dest.state = (state ^ 1);
    dest.ep = None;
    dest.checkState = CheckState::NotInCheck;
    dest.pieceKey = pieceKey;
    dest.positionKey = (pieceKey ^
                        _HASH[0][dest.state & 0x1F] ^
                        _HASH[0][None]);
  }

  //---------------------------------------------------------------------------
  template<Color color>
  uint64_t PerftSearch(const int depth) {
    GenerateMoves<color>();
    if (!child || (depth <= 1)) {
      return moveCount;
    }

    uint64_t count = 0;
    while (!_stop && (moveIndex < moveCount)) {
      const Move& move = moves[moveIndex++];
      Exec<color>(move, *child);
      count += child->PerftSearch<!color>(depth - 1);
      Undo<color>(move);
    }
    return count;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  uint64_t PerftSearchRoot(const int depth) {
    assert(!ply);
    assert(!parent);
    assert(child);
    assert(VerifyAttacks(true));

    GenerateMoves<color>();
    std::sort(moves, (moves + moveCount), Move::LexicalCompare);

    uint64_t total = 0;
    if (depth > 1) {
      while (!_stop && (moveIndex < moveCount)) {
        const Move& move = moves[moveIndex++];
        Exec<color>(move, *child);
        const uint64_t count = child->PerftSearch<!color>(depth - 1);
        Undo<color>(move);
        senjo::Output() << move.ToString() << ' ' << count;
        total += count;
      }
    }
    else {
      while (!_stop && (moveIndex < moveCount)) {
        senjo::Output() << moves[moveIndex++].ToString() << " 1 ";
        total++;
      }
    }
    return total;
  }

  //---------------------------------------------------------------------------
  inline void UpdatePV(const Move& move) {
    pv[0] = move;
    if (child) {
      if ((pvCount = (child->pvCount + 1)) > 1) {
        assert(pvCount <= MaxPlies);
        memcpy((pv + 1), child->pv, (child->pvCount * sizeof(Move)));
      }
    }
    else {
      pvCount = 1;
    }
  }

  //---------------------------------------------------------------------------
  void OutputPV(const int score, const int bound = 0) const {
    if (pvCount > 0) {
      const uint64_t msecs = (senjo::Now() - _startTime);
      senjo::Output out(senjo::Output::NoPrefix);

      const uint64_t nodes = (_stats.snodes + _stats.qnodes);
      out << "info depth " << _depth
          << " seldepth " << _seldepth
          << " nodes " << nodes
          << " time " << msecs
          << " nps " << static_cast<uint64_t>(senjo::Rate(nodes, msecs));

      if (bound) {
        out << " currmovenumber " << _movenum
            << " currmove " << _currmove;
      }

      if (abs(score) < MateScore) {
        out << " score cp " << score;
      }
      else {
        const int count = (Infinity - abs(score));
        const int mate = ((count + 1) / 2);
        out << " score mate " << ((score < 0) ? -mate : mate);
      }

      if (bound) {
        out << ((bound < 0) ? " upperbound" : " lowerbound");
      }
      else {
        out << " pv";
        for (int i = 0; i < pvCount; ++i) {
          const Move& move = pv[i];
          out << ' ' << move.ToString();
        }
      }
    }
  }

  //---------------------------------------------------------------------------
  inline bool IsDraw() const {
    return ((state & Draw) | (rcount >= 100) | _seen.count(positionKey));
  }

  //---------------------------------------------------------------------------
  template<Color color>
  inline bool InCheck() {
    assert(color == COLOR(state));
    if (checkState == CheckState::Unknown) {
      if (AttackedBy<!color>(_piece[2 + color].sqr)) {
        checkState = IsInCheck;
      }
      else {
        checkState = NotInCheck;
      }
    }
    return (checkState == IsInCheck);
  }

  //---------------------------------------------------------------------------
  inline void AddKiller(const Move& move) {
    if (move != killer[0]) {
      killer[1] = killer[0];
      killer[0] = move;
    }
  }

  //---------------------------------------------------------------------------
  inline bool IsKiller(const Move& move) const {
    return ((move == killer[0]) | (move == killer[1]));
  }

  //--------------------------------------------------------------------------
  template<Color color>
  int QSearch(int alpha, int beta, const int depth) {
    assert(alpha < beta);
    assert(abs(alpha) <= Infinity);
    assert(abs(beta) <= Infinity);
    assert(depth <= 0);

    _stats.qnodes++;
    if (ply > _seldepth) {
      _seldepth = ply;
    }

    pvCount = 0;
    if (IsDraw()) {
      return _drawScore[color];
    }

    // mate distance pruning and standPat beta cutoff
    assert(standPat > (ply - Infinity));
    const bool check = InCheck<color>();
    int best = (check ? (ply - Infinity) : standPat);
    alpha = std::max<int>(best, alpha);
    beta = std::min<int>((Infinity - ply + 1), beta);
    if ((alpha >= beta) | !child) {
      return alpha;
    }

    // transposition table
    Move firstMove;
    HashEntry* entry = _tt.Probe(positionKey);
    if (entry) {
      switch (entry->GetPrimaryFlag()) {
      case HashEntry::Checkmate: return (ply - Infinity);
      case HashEntry::Stalemate: return _drawScore[color];
      case HashEntry::UpperBound:
        firstMove.Init(entry->moveBits, entry->score);
        assert(ValidateMove<color>(firstMove) == 0);
        if (entry->score <= alpha) {
          pv[0] = firstMove;
          pvCount = 1;
          return entry->score;
        }
        break;
      case HashEntry::ExactScore:
        firstMove.Init(entry->moveBits, entry->score);
        assert(ValidateMove<color>(firstMove) == 0);
        pv[0] = firstMove;
        pvCount = 1;
        if ((entry->score >= beta) && !firstMove.IsCapOrPromo()) {
          IncHistory(firstMove, check, entry->depth);
          AddKiller(firstMove);
        }
        return entry->score;
      case HashEntry::LowerBound:
        firstMove.Init(entry->moveBits, entry->score);
        assert(ValidateMove<color>(firstMove) == 0);
        if (entry->score >= beta) {
          pv[0] = firstMove;
          pvCount = 1;
          if (!firstMove.IsCapOrPromo()) {
            IncHistory(firstMove, check, entry->depth);
            AddKiller(firstMove);
          }
          return entry->score;
        }
        if (entry->score > best) {
          best = entry->score;
          if (best > alpha) {
            alpha = best;
          }
        }
        break;
      default:
        assert(false);
      }
    }

    assert(alpha < beta);
    assert(best < beta);
    assert(best <= alpha);

    // search firstMove if we have it
    const int orig_alpha = alpha;
    if (firstMove.Type()) {
      _stats.qexecs++;
      Exec<color>(firstMove, *child);
      if (!check & !firstMove.IsCapOrPromo() & !child->InCheck<!color>()) {
        Undo<color>(firstMove);
      }
      else {
        firstMove.Score() = -child->QSearch<!color>(-beta, -alpha, (depth - 1));
        Undo<color>(firstMove);
        if (_stop) {
          return beta;
        }
        if (firstMove.GetScore() > alpha) {
          alpha = firstMove.GetScore();
        }
        if (firstMove.GetScore() > best) {
          best = firstMove.GetScore();
          UpdatePV(firstMove);
          if (firstMove.GetScore() >= beta) {
            if (!firstMove.IsCapOrPromo()) {
              AddKiller(firstMove);
            }
            if (check) {
              firstMove.Score() = beta;
              _tt.Store(positionKey, firstMove, 0, HashEntry::LowerBound, 0);
            }
            return best;
          }
        }
      }
    }

    // generate moves
    GenerateMoves<color>(); // TODO make QSearchMoveGen(depth)
    if (moveCount <= 0) {
      if (check) {
        assert(!firstMove.IsValid());
        _tt.StoreCheckmate(positionKey);
        return (ply - Infinity);
      }
      // don't call _tt.StoreStalemate()!!!
      assert(best >= standPat);
      assert(best < beta);
      return best;
    }

    // search 'em
    Move* move;
    while ((move = GetNextMove())) {
      if (firstMove == (*move)) {
        assert(firstMove.IsValid());
        continue;
      }

      _stats.qexecs++;
      Exec<color>(*move, *child);
      if (!check & (depth < 0) & !move->Promo() &&
          ((standPat + ValueOf(move->Cap()) + 900) <= alpha) & // TODO adjust delta
          !child->InCheck<!color>())
      {
        _stats.deltaCount++;
        Undo<color>(*move);
        if (_stop) {
          return beta;
        }
        continue;
      }

      move->Score() = -child->QSearch<!color>(-beta, -alpha, (depth - 1));
      Undo<color>(*move);
      if (_stop) {
        return beta;
      }
      if (move->GetScore() > alpha) {
        alpha = move->GetScore();
      }
      if (move->GetScore() > best) {
        best = move->GetScore();
        UpdatePV(*move);
        if (move->GetScore() >= beta) {
          if (!move->IsCapOrPromo()) {
            AddKiller(*move);
          }
          if (check) {
            move->Score() = beta;
            _tt.Store(positionKey, *move, 0, HashEntry::LowerBound, 0);
          }
          return best;
        }
      }
    }

    assert(best <= alpha);
    assert(alpha < beta);

    if (check & (pvCount > 0)) {
      if (alpha > orig_alpha) {
        assert(pv[0].GetScore() == alpha);
        assert(beta > (orig_alpha + 1));
        _tt.Store(positionKey, pv[0], 0, HashEntry::ExactScore,
            HashEntry::FromPV);
      }
      else {
        assert(alpha == orig_alpha);
        assert(pv[0].GetScore() <= alpha);
        pv[0].Score() = alpha;
        _tt.Store(positionKey, pv[0], 0, HashEntry::UpperBound, 0);
      }
    }

    return best;
  }

  //---------------------------------------------------------------------------
  template<NodeType type, Color color>
  int Search(int alpha, int beta, int depth, const bool cutNode) {
    assert(parent);
    assert(ply > 0);
    assert(alpha < beta);
    assert(abs(alpha) <= Infinity);
    assert(abs(beta) <= Infinity);
    assert(depth > 0);
    assert(depthChange <= 1);
    assert((depth + depthChange) > 0);
    assert((type == PV) | ((alpha + 1) == beta));

    _stats.snodes++;
    moveCount = 0;
    pvCount   = 0;

    if (IsDraw()) {
      return _drawScore[color];
    }

    // mate distance pruning
    int best = (ply - Infinity);
    alpha = std::max<int>(best, alpha);
    beta = std::min<int>((Infinity - ply + 1), beta);
    if ((alpha >= beta) | !child) {
      return alpha;
    }

    depth += depthChange;

    // check extensions
    const bool check = InCheck<color>();
    if (check & (depthChange <= 0) & (parent->depthChange <= 0)) {
      _stats.chkExts++;
      depthChange++;
      depth++;
    }

    // transposition table
    const bool pvNode = (type == PV);
    HashEntry* entry = _tt.Probe(positionKey);
    Move firstMove;
    int eval = standPat;
    if (entry) {
      switch (entry->GetPrimaryFlag()) {
      case HashEntry::Checkmate: return (ply - Infinity);
      case HashEntry::Stalemate: return _drawScore[color];
      case HashEntry::UpperBound:
        firstMove.Init(entry->moveBits, entry->score);
        assert(ValidateMove<color>(firstMove) == 0);
        if ((!pvNode | entry->HasPvFlag()) &
            (entry->depth >= depth) & (entry->score <= alpha))
        {
          pv[0] = firstMove;
          pvCount = 1;
          return entry->score;
        }
        if ((entry->depth >= (depth - 3)) & (entry->score < eval)) {
          eval = entry->score;
        }
        break;
      case HashEntry::ExactScore:
        firstMove.Init(entry->moveBits, entry->score);
        assert(entry->HasPvFlag());
        assert(ValidateMove<color>(firstMove) == 0);
        if ((entry->depth >= depth) & ((entry->score <= alpha) |
                                       (entry->score >= beta)))
        {
          pv[0] = firstMove;
          pvCount = 1;
          if ((entry->score >= beta) & !firstMove.IsCapOrPromo()) {
            IncHistory(firstMove, check, entry->depth);
            AddKiller(firstMove);
          }
          return entry->score;
        }
        if (entry->depth >= (depth - 3)) {
          eval = entry->score;
        }
        break;
      case HashEntry::LowerBound:
        firstMove.Init(entry->moveBits, entry->score);
        assert(ValidateMove<color>(firstMove) == 0);
        if ((!pvNode | entry->HasPvFlag()) &
            (entry->depth >= depth) & (entry->score >= beta))
        {
          pv[0] = firstMove;
          pvCount = 1;
          if (!firstMove.IsCapOrPromo()) {
            IncHistory(firstMove, check, entry->depth);
            AddKiller(firstMove);
          }
          return entry->score;
        }
        if ((entry->depth >= (depth - 3)) & (entry->score > eval)) {
          eval = entry->score;
        }
        break;
      default:
        assert(false);
      }
      if (entry->HasExtendedFlag() & (depthChange <= 0) &
          (parent->depthChange <= 0))
      {
        _stats.hashExts++;
        depthChange++;
        depth++;
      }
    }

    // forward pruning prerequisites
    if (pvNode || !(pruneOK & !check & (depthChange <= 0))) {
      goto search_main;
    }

    // razoring (fail low pruning)
    if (((depth < 4) & (alpha < WinningScore) & !parent->InCheck<!color>()) &&
        // TODO && no pawns on 2nd/7th rank
        ((eval + RazorDelta(depth)) <= alpha))
    {
      _stats.rzrCount++;
      if ((depth <= 1) & ((eval + RazorDelta(3 * depth)) <= alpha)) {
        _stats.rzrEarlyOut++;
        return QSearch<color>(alpha, beta, 0);
      }
      const int ralpha = (alpha - RazorDelta(depth));
      const int val = QSearch<color>(ralpha, (ralpha + 1), 0);
      if (_stop) {
        return beta;
      }
      if (val <= ralpha) {
        _stats.rzrCutoffs++;
        return val;
      }
    }

    // null move heuristics prerequisites
    if (((_pcount[color] + _pcount[color|Knight]) < 2) |
        ((_pcount[color] + _pcount[color|Knight] + _pcount[color|Pawn]) < 4))
    {
      goto search_main;
    }

    // futility pruning (static null move pruning)
    if ((cutNode & (depth < 7)) && // TODO try different max depths
        ((eval - FutilityDelta(depth)) >= beta))
    {
      _stats.futility++;
      pvCount = 0;
      return (eval - FutilityDelta(depth));
    }

    int searchDepth;
    if (depth > 1) {
      if (eval >= beta) {
        // null move pruning
        ExecNullMove<color>(*child);
        child->pruneOK = false;
        child->depthChange = 0;
        searchDepth = (depth - 3 - (depth / 6) - ((eval - 400) >= beta));
        eval = (searchDepth > 0)
            ? -child->Search<NonPV, !color>(-beta, -alpha, searchDepth, false)
            : -child->QSearch<!color>(-beta, -alpha, 0);
        if (_stop) {
          return beta;
        }
        if (eval >= beta) {
          // TODO do verification search if depth reduction > 4
          _stats.nmCutoffs++;
          pvCount = 0;
          return (standPat >= beta) ? standPat : beta; // do not return eval
        }
      }
      else if (cutNode & (depth > 2) & !parent->InCheck<!color>() &
               (eval >= -parent->standPat) & !lastMove.IsCapOrPromo() &
               !((lastMove.Type() == PawnMove) &
                 (YC(lastMove.To()) == (color ? 6 : 1))))
      {
        // null move reductions
        _stats.nmrCandidates++;
        ExecNullMove<color>(*child);
        eval = -child->QSearch<!color>(-standPat, (1 - standPat), 0);
        if (_stop) {
          return beta;
        }
        if (eval >= standPat) {
          // last move looks pretty quiet and we're already expecting
          // to be able to refute it (this is a cutNode) so we're betting we
          // can reduce depth and still refute it.  if not, we spend less time
          // determining that the last move is actually pretty good.
          // but this can backfire if:
          // 1) reduced depth fails low here (fails high in parent)
          // 2) parent failes low on re-search at full depth
          // example stats: 4896 nmr candidates, 3254 reduced (66.4624%), 114 backfires (3.50338%)
//          senjo::Output() << parent->moveIndex << ", " << lastMove.ToString()
//                          << ", " << -parent->standPat
//                          << ", " << eval
//                          << ", " << standPat;
//          PrintBoard();
          _stats.nmReductions++;
          depthChange -= (1 + (eval >= -parent->standPat));
          depth -= (1 + (eval >= -parent->standPat));
        }
      }
    }
    pruneOK = false;

    // internal iterative deepening
    if (!firstMove.Type() & (beta < Infinity) &
        ((beta - 1) > -Infinity) & (depth >= (pvNode ? 4 : 6)))
    {
      assert(!pvCount);
      _stats.iidCount++;
      // subtract depthChange because it will be added again at top of Search()
      searchDepth = (depth - depthChange - (pvNode ? 2 : 4));
      eval = Search<NonPV, color>((beta - 1), beta, searchDepth, true);
      if (_stop | !pvCount) {
        return eval;
      }
      assert(pv[0].IsValid());
      firstMove = pv[0];
    }

search_main:

    // make sure firstMove is populated
    if (!firstMove.Type()) {
      GenerateMoves<color>();
      if (moveCount <= 0) {
        if (check) {
          _tt.StoreCheckmate(positionKey);
          return (ply - Infinity);
        }
        _tt.StoreStalemate(positionKey);
        return _drawScore[0];
      }
      firstMove = *GetNextMove();

      // single reply extensions
      if ((moveCount == 1) & (depthChange <= 0) & (parent->depthChange <= 0)) {
        _stats.oneReplyExts++;
        depthChange++;
        depth++;
      }
    }

    // search first move with full alpha/beta window
    assert(depth > 0);
    const int orig_alpha = alpha;
    Exec<color>(firstMove, *child);
    child->pruneOK = true;
    child->depthChange = 0;
    eval = (depth > 1)
        ? -child->Search<type, !color>(-beta, -alpha, (depth - 1), !cutNode)
        : -child->QSearch<!color>(-beta, -alpha, 0);
    assert(!pvNode || (child->depthChange >= 0));
    assert((depth + child->depthChange) >= 0);
    if (_stop) {
      Undo<color>(firstMove);
      return beta;
    }
    if ((eval > alpha) & (child->depthChange < 0)) {
      assert(!pvNode);
      assert(depth > 1);
      child->pruneOK = false;
      child->depthChange = 0;
      eval = -child->Search<type, !color>(-beta, -alpha, (depth - 1), false);
      if (_stop) {
        Undo<color>(firstMove);
        return beta;
      }
    }
    Undo<color>(firstMove);
    int pvDepth = (depth + ((child->depthChange < 0) ? child->depthChange : 0));
    best = eval;
    UpdatePV(firstMove);
    if (eval > alpha) {
      alpha = eval;
    }
    else if (!firstMove.IsCapOrPromo()) {
      DecHistory(firstMove, check);
    }
    if (eval >= beta) {
      if (!firstMove.IsCapOrPromo()) {
        IncHistory(firstMove, check, pvDepth);
        AddKiller(firstMove);
      }
      firstMove.Score() = beta;
      _tt.Store(positionKey, firstMove, pvDepth, HashEntry::LowerBound,
                (((depthChange > 0) ? HashEntry::Extended : 0) |
                 (pvNode ? HashEntry::FromPV : 0)));
      return best;
    }

    // generate moves if we haven't done so already
    if (moveCount <= 0) {
      GenerateMoves<color>();
      assert(moveCount > 0);

      // single reply extensions
      if ((moveCount == 1) & (depthChange <= 0) & (parent->depthChange <= 0)) {
        _stats.oneReplyExts++;
        depthChange++;
        depth++;
      }
    }

    // search remaining moves
    const bool lmr_ok = ((cutNode | !pvNode) & !check & (depth > 2));
    Move* move;
    moveIndex = 0;
    while ((move = GetNextMove())) {
      if (firstMove == (*move)) {
        assert(firstMove.IsValid());
        continue;
      }

      Exec<color>(*move, *child);

      // late move reductions
      _stats.lateMoves++;
      if (lmr_ok) _stats.lmCandidates++;
      if (lmr_ok &
          !move->IsCapOrPromo() &
          !IsKiller(*move) &
          (_hist[move->HistoryIndex()] < 0) &
          (!pvNode || (moveIndex > 7)) &
          !child->InCheck<!color>())
      {
        _stats.lmReductions++;
        child->pruneOK = true;
        child->depthChange = -(1 + (!pvNode &
                                    (-child->standPat <= -parent->standPat)));
      }
      else {
        child->pruneOK = true;
        child->depthChange = 0;
      }

      // first search with a null window to quickly see if it improves alpha
      eval = ((depth + child->depthChange - 1) > 0)
          ? -child->Search<NonPV, !color>(-(alpha + 1), -alpha, (depth - 1), true)
          : -child->QSearch<!color>(-(alpha + 1), -alpha, 0);

      // re-search at full depth?
      if (!_stop && (eval > alpha) && (child->depthChange < 0)) {
        assert(depth > 1);
        _stats.lmResearches++;
        child->pruneOK = false;
        child->depthChange = 0;
        eval = -child->Search<NonPV, !color>(-(alpha + 1), -alpha, (depth - 1), false);
        if (!_stop && (eval > alpha)) {
          _stats.lmConfirmed++;
        }
      }

      // re-search with full window?
      if (!_stop && pvNode && (eval > alpha)) {
        child->pruneOK = false;
        assert(child->depthChange >= 0);
        eval = (depth > 1)
            ? -child->Search<type, !color>(-beta, -alpha, (depth - 1), false)
            : -child->QSearch<!color>(-beta, -alpha, 0);
      }

      Undo<color>(*move);
      if (_stop) {
        return beta;
      }
      if (eval > alpha) {
        alpha = eval;
        _stats.lmAlphaIncs++;
        assert(child->depthChange >= 0);
      }
      else if (!move->IsCapOrPromo()) {
        DecHistory(*move, check);
      }
      if (eval > best) {
        best = eval;
        UpdatePV(*move);
        assert((depth + child->depthChange) >= 0);
        pvDepth = (depth + ((child->depthChange < 0) ? child->depthChange : 0));
        if (eval >= beta) {
          if (!move->IsCapOrPromo()) {
            IncHistory(*move, check, pvDepth);
            AddKiller(*move);
          }
          move->Score() = beta;
          _tt.Store(positionKey, *move, pvDepth, HashEntry::LowerBound,
                    (((depthChange > 0) ? HashEntry::Extended : 0) |
                     (pvNode ? HashEntry::FromPV : 0)));
          return best;
        }
      }
    }

    assert(moveCount > 0);
    assert(best <= alpha);
    assert(alpha < beta);

    if (pvCount > 0) {
      pv[0].Score() = alpha;
      if (alpha > orig_alpha) {
        assert(pvNode);
        assert(pvDepth == depth);
        if (!pv[0].IsCapOrPromo()) {
          IncHistory(pv[0], check, pvDepth);
        }
        _tt.Store(positionKey, pv[0], pvDepth, HashEntry::ExactScore,
            (((depthChange > 0) ? HashEntry::Extended : 0) |
             HashEntry::FromPV));
      }
      else {
        assert(alpha == orig_alpha);
        assert(pvDepth <= depth);
        _tt.Store(positionKey, pv[0], pvDepth, HashEntry::UpperBound,
            (((depthChange > 0) ? HashEntry::Extended : 0) |
             (pvNode ? HashEntry::FromPV : 0)));
      }
    }

    return best;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  std::string SearchRoot(const int depth) {
    assert(!parent);
    assert(child);
    assert(!ply);

    pruneOK = false;
    depthChange = 0;

    GenerateMoves<color>();
    if (moveCount <= 0) {
      senjo::Output() << "No legal moves";
      return std::string();
    }

    // sort 'em
    std::sort(moves, (moves + moveCount), Move::ScoreCompare);

    // move transposition table move (if any) to front of list
    if (moveCount > 1) {
      HashEntry* entry = _tt.Probe(positionKey);
      if (entry) {
        switch (entry->GetPrimaryFlag()) {
        case HashEntry::Checkmate:
        case HashEntry::Stalemate:
          assert(false);
          break;
        case HashEntry::UpperBound:
        case HashEntry::ExactScore:
        case HashEntry::LowerBound: {
          const Move ttMove(entry->moveBits, entry->score);
          assert(ValidateMove<color>(ttMove) == 0);
          for (int i = 0; i < moveCount; ++i) {
            if (moves[i] == ttMove) {
              ScootMoveToFront(i);
              break;
            }
          }
          break;
        }}
      }
    }

    // initial principal variation
    pvCount = 1;
    pv[0] = moves[0];

    // return immediately if we only have one move
    if (moveCount == 1) {
      OutputPV(pv[0].GetScore());
      return pv[0].ToString();
    }

    Move* move;
    bool  newPV = true;
    bool  showPV = true;
    int   alpha;
    int   best = standPat;
    int   beta;
    int   delta;

    // iterative deepening
    for (int d = 0; !_stop && (d < depth); ++d) {
      _seldepth = _depth = (d + 1);

      newPV = true;
      delta = (_depth < 5) ? HugeDelta : 25;
      alpha = std::max<int>((best - delta), -Infinity);
      beta  = std::min<int>((best + delta), +Infinity);

      for (moveIndex = 0; !_stop && (moveIndex < moveCount); ++moveIndex) {
        move      = (moves + moveIndex);
        _currmove = move->ToString();
        _movenum  = (moveIndex + 1);

        child->pruneOK = true;
        child->depthChange = 0;
        Exec<color>(*move, *child);
        move->Score() = (_depth > 1)
            ? ((_movenum == 1)
               ? -child->Search<PV, !color>(-beta, -alpha, (_depth - 1), false)
               : -child->Search<NonPV, !color>(-beta, -alpha, (_depth - 1), true))
            : -child->QSearch<!color>(-beta, -alpha, 0);
        assert(move->GetScore() > -Infinity);
        assert(move->GetScore() < Infinity);
        if (_stop) {
          Undo<color>(*move);
          break;
        }

        // re-search to get real score?
        if ((move->GetScore() >= beta) |
            ((move->GetScore() <= alpha) & (_movenum == 1)))
        {
          int bound[2] = { -Infinity, Infinity };
          newPV = true;
          delta = (_depth < 5) ? HugeDelta : 100;
          do {
            if (move->GetScore() >= beta) {
              OutputPV(move->GetScore(), 1); // report lowerbound
              beta = std::min<int>(Infinity, (move->GetScore() + delta));
              alpha = (move->GetScore() - 1);
            }
            else {
              assert(move->GetScore() <= alpha);
              OutputPV(move->GetScore(), -1); // report upperbound
              if (_movenum == 1) {
                alpha = std::max<int>(-Infinity, (move->GetScore() - delta));
              }
              else {
                alpha = std::max<int>(best, (move->GetScore() - delta));
              }
              beta = (move->GetScore() + 1);
            }
            child->pruneOK = false;
            child->depthChange = 0;
            move->Score() = (_depth > 1)
                ? -child->Search<PV, !color>(-beta, -alpha, (_depth - 1), false)
                : -child->QSearch<!color>(-beta, -alpha, 0);
            assert(move->GetScore() > -Infinity);
            assert(move->GetScore() < Infinity);
            if (_stop) {
              break;
            }
            if ((_movenum > 1) & (move->GetScore() <= best)) {
              newPV = false;
              break;
            }
            if (move->GetScore() > bound[0]) {
              bound[0] = move->GetScore();
            }
            else if (move->GetScore() < bound[1]) {
              bound[1] = move->GetScore();
            }
            else {
              // TODO increase time
              senjo::Output() << "UNSTABLE(" << move->GetScore() << ", "
                              << bound[0] << ", " << bound[1] << ")";
              break;
            }
            if (abs(move->GetScore()) >= 1000) {
              delta = HugeDelta;
            }
          } while ((move->GetScore() <= alpha) | (move->GetScore() >= beta));
        }
        Undo<color>(*move);

        // do we have a new principal variation?
        if (newPV) {
          newPV = false;
          showPV = false;
          UpdatePV(*move);
          if (!_stop &&
              (move->GetScore() > alpha) && (move->GetScore() < beta))
          {
            OutputPV(move->GetScore());
            _tt.Store(positionKey, *move, _depth, HashEntry::ExactScore,
                      HashEntry::FromPV);
          }

          best = alpha = move->GetScore();
          ScootMoveToFront(moveIndex);
        }

        // set null aspiration window now that we have a principal variation
        beta = (alpha + 1);
      }
    }

    if (showPV) {
      OutputPV(pv[0].GetScore());
    }

    return pv[0].ToString();
  }
};

//-----------------------------------------------------------------------------
Node _node[MaxPlies];

//-----------------------------------------------------------------------------
void InitNodes() {
  for (int i = 0; i < MaxPlies; ++i) {
    Node* node = (_node + i);
    node->parent = (i ? (node - 1) : NULL);
    node->child = (((i + 1) < MaxPlies) ? (node + 1) : NULL);
    node->ply = i;
  }
}

//-----------------------------------------------------------------------------
Clunk::Clunk()
  : root(_node),
    initialized(false)
{
}

//-----------------------------------------------------------------------------
Clunk::~Clunk() {
}

//-----------------------------------------------------------------------------
bool Clunk::IsInitialized() const {
  return initialized;
}

//-----------------------------------------------------------------------------
bool Clunk::SetEngineOption(const std::string& /*optionName*/,
                            const std::string& /*optionValue*/)
{
  return false;
}

//-----------------------------------------------------------------------------
bool Clunk::WhiteToMove() const {
  return !COLOR(root->state);
}

//-----------------------------------------------------------------------------
const char* Clunk::MakeMove(const char* str) {
  if (!initialized || !str ||
      !IS_X(str[0]) || !IS_Y(str[1]) ||
      !IS_X(str[2]) || !IS_Y(str[3]))
  {
    return NULL;
  }

  int from  = SQR(TO_X(str[0]), TO_Y(str[1]));
  int to    = SQR(TO_X(str[2]), TO_Y(str[3]));
  int pc    = _board[from]->type;
  int cap   = _board[to]->type;
  int promo = 0;

  assert(_EMPTY->type == 0);
  assert(cap || (_board[to] == _EMPTY));

  const Color color = COLOR(root->state);
  const char* p = (str + 4);
  switch (*p) {
  case 'b': promo = (color|Bishop); p++; break;
  case 'n': promo = (color|Knight); p++; break;
  case 'q': promo = (color|Queen);  p++; break;
  case 'r': promo = (color|Rook);   p++; break;
  default:
    break;
  }

  if ((*p && !isspace(*p)) ||
      !pc ||
      (from == to) ||
      (COLOR(pc) != color) ||
      (cap && (COLOR(cap) == color)) ||
      ((Black|cap) == (Black|King)) ||
      ((Black|promo) == (Black|Pawn)) ||
      ((Black|promo) == (Black|King)) ||
      (promo && (pc != (color|Pawn))))
  {
    return NULL;
  }

  if (color) {
    root->GenerateMoves<Black>();
  }
  else {
    root->GenerateMoves<White>();
  }

  for (; root->moveIndex < root->moveCount; ++root->moveIndex) {
    const Move& move = root->moves[root->moveIndex];
    if ((move.From() == from) &&
        (move.To() == to) &&
        (move.Promo() == promo))
    {
      break;
    }
  }
  if (root->moveIndex >= root->moveCount) {
    return NULL;
  }

  if (color) {
    root->Exec<Black>(root->moves[root->moveIndex], *root);
  }
  else {
    root->Exec<White>(root->moves[root->moveIndex], *root);
  }

  return p;
}

//-----------------------------------------------------------------------------
const char* Clunk::SetPosition(const char* fen) {
  if (!fen || !*fen) {
    senjo::Output() << "NULL or empty fen string";
    return NULL;
  }

  memset(_board, 0, sizeof(_board));
  memset(_piece, 0, sizeof(_piece));
  memset(_pcount, 0, sizeof(_pcount));
  memset(_material, 0, sizeof(_material));
  memset(_atkd, 0, sizeof(_atkd));
  _seen.clear();

  _board[None] = _EMPTY;

  root->checkState = CheckState::Unknown;
  root->state = 0;
  root->ep = None;
  root->rcount = 0;
  root->mcount = 0;
  root->pawnKey = 0ULL;
  root->pieceKey = 0ULL;
  root->positionKey = 0ULL;

  const char* p = fen;
  for (int y = 7; y >= 0; --y) {
    for (int x = 0; x < 8; ++x, ++p) {
      if (!*p) {
        senjo::Output() << "Incomplete board position";
        return NULL;
      }
      else if (isdigit(*p)) {
        if (*p == '0') {
          senjo::Output() << "Invalid empty square count at " << p;
          return NULL;
        }
        _board[SQR(x,y)] = _EMPTY;
        for (int n = (*p - '1'); n; --n) {
          ++x;
          _board[SQR(x,y)] = _EMPTY;
        }
      }
      else {
        const int sqr = SQR(x,y);
        assert(IS_SQUARE(sqr));
        _board[sqr] = _EMPTY;
        switch (*p) {
        case 'b':
          if (_pcount[Black|Bishop] >= 10) {
            senjo::Output() << "Too many black bishops";
            return NULL;
          }
          AddPiece((Black|Bishop), sqr);
          _material[Black] += BishopValue;
          root->pieceKey ^= _HASH[Black|Bishop][sqr];
          break;
        case 'B':
          if (_pcount[White|Bishop] >= 10) {
            senjo::Output() << "Too many white bishops";
            return NULL;
          }
          AddPiece((White|Bishop), sqr);
          _material[White] += BishopValue;
          root->pieceKey ^= _HASH[White|Bishop][sqr];
          break;
        case 'k':
          if (_piece[BlackKingOffset].type) {
            senjo::Output() << "Multiple black kings";
            return NULL;
          }
          AddPiece((Black|King), sqr);
          root->pieceKey ^= _HASH[Black|King][sqr];
          break;
        case 'K':
          if (_piece[WhiteKingOffset].type) {
            senjo::Output() << "Multiple white kings";
            return NULL;
          }
          AddPiece((White|King), sqr);
          root->pieceKey ^= _HASH[White|King][sqr];
          break;
        case 'n':
          if (_pcount[Black|Knight] >= 10) {
            senjo::Output() << "Too many black knights";
            return NULL;
          }
          AddPiece((Black|Knight), sqr);
          _material[Black] += KnightValue;
          root->pieceKey ^= _HASH[Black|Knight][sqr];
          break;
        case 'N':
          if (_pcount[White|Knight] >= 10) {
            senjo::Output() << "Too many white knights";
            return NULL;
          }
          AddPiece((White|Knight), sqr);
          _material[White] += KnightValue;
          root->pieceKey ^= _HASH[White|Knight][sqr];
          break;
        case 'p':
          if (_pcount[Black|Pawn] >= 8) {
            senjo::Output() << "Too many black pawns";
            return NULL;
          }
          AddPiece((Black|Pawn), sqr);
          _material[Black] += PawnValue;
          root->pawnKey ^= _HASH[Black|Pawn][sqr];
          break;
        case 'P':
          if (_pcount[White|Pawn] >= 8) {
            senjo::Output() << "Too many white pawns";
            return NULL;
          }
          AddPiece((White|Pawn), sqr);
          _material[White] += PawnValue;
          root->pawnKey ^= _HASH[White|Pawn][sqr];
          break;
        case 'q':
          if (_pcount[Black|Queen] >= 9) {
            senjo::Output() << "Too many black queens";
            return NULL;
          }
          AddPiece((Black|Queen), sqr);
          _material[Black] += QueenValue;
          root->pieceKey ^= _HASH[Black|Queen][sqr];
          break;
        case 'Q':
          if (_pcount[White|Queen] >= 9) {
            senjo::Output() << "Too many white queens";
            return NULL;
          }
          AddPiece((White|Queen), sqr);
          _material[White] += QueenValue;
          root->pieceKey ^= _HASH[White|Queen][sqr];
          break;
        case 'r':
          if (_pcount[Black|Rook] >= 10) {
            senjo::Output() << "Too many black rooks";
            return NULL;
          }
          AddPiece((Black|Rook), sqr);
          _material[Black] += RookValue;
          root->pieceKey ^= _HASH[Black|Rook][sqr];
          break;
        case 'R':
          if (_pcount[White|Rook] >= 10) {
            senjo::Output() << "Too many hite rooks";
            return NULL;
          }
          AddPiece((White|Rook), sqr);
          _material[White] += RookValue;
          root->pieceKey ^= _HASH[White|Rook][sqr];
          break;
        default:
          senjo::Output() << "Invalid character at " << p;
          return NULL;
        }
      }
    }
    if (y > 0) {
      if (*p == '/') {
        ++p;
      }
      else if (*p) {
        senjo::Output() << "Expected '/' at " << p;
        return NULL;
      }
      else {
        senjo::Output() << "Incomplete board position";
        return NULL;
      }
    }
  }

  if (_piece[WhiteKingOffset].type != (White|King)) {
    senjo::Output() << "No white king in this position";
    return NULL;
  }
  if (_piece[BlackKingOffset].type != (Black|King)) {
    senjo::Output() << "No black king in this position";
    return NULL;
  }

  int from;
  for (int i = 0; i < _pcount[White|Bishop]; ++i) {
    from = _piece[BishopOffset + i].sqr;
    AddAttacksFrom((White|Bishop), from);
  }
  for (int i = 0; i < _pcount[Black|Bishop]; ++i) {
    from = _piece[BlackBishopOffset + i].sqr;
    AddAttacksFrom((Black|Bishop), from);
  }
  for (int i = 0; i < _pcount[White|Rook]; ++i) {
    from = _piece[RookOffset + i].sqr;
    AddAttacksFrom((White|Rook), from);
  }
  for (int i = 0; i < _pcount[Black|Rook]; ++i) {
    from = _piece[BlackRookOffset + i].sqr;
    AddAttacksFrom((Black|Rook), from);
  }
  for (int i = 0; i < _pcount[White|Queen]; ++i) {
    from = _piece[QueenOffset + i].sqr;
    AddAttacksFrom((White|Queen), from);
  }
  for (int i = 0; i < _pcount[Black|Queen]; ++i) {
    from = _piece[BlackQueenOffset + i].sqr;
    AddAttacksFrom((Black|Queen), from);
  }
  assert(VerifyAttacks(true));

  p = NextSpace(p);
  p = NextWord(p);
  switch (*p) {
  case 'b':
    root->state |= Black;
    if (AttackedBy<Black>(_piece[WhiteKingOffset].sqr)) {
      senjo::Output() << "The king can be captured in this position";
      return NULL;
    }
    break;
  case 'w':
    root->state |= White;
    if (AttackedBy<White>(_piece[BlackKingOffset].sqr)) {
      senjo::Output() << "The king can be captured in this position";
      return NULL;
    }
    break;
  case 0:
    senjo::Output() << "Missing side to move (w|b)";
    return NULL;
  default:
    senjo::Output() << "Expected 'w' or 'b' at " << p;
    return NULL;
  }

  p = NextSpace(p);
  p = NextWord(p);
  while (*p && !isspace(*p)) {
    switch (*p++) {
    case '-': break;
    case 'k': root->state |= BlackShort; continue;
    case 'K': root->state |= WhiteShort; continue;
    case 'q': root->state |= BlackLong;  continue;
    case 'Q': root->state |= WhiteLong;  continue;
    default:
      senjo::Output() << "Unexpected castle rights at " << p;
      return NULL;
    }
    break;
  }

  p = NextSpace(p);
  p = NextWord(p);
  if (IS_X(p[0]) && IS_Y(p[1])) {
    const int x = TO_X(*p++);
    const int y = TO_Y(*p++);
    root->ep = SQR(x, y);
    const int sqr = (root->ep + (COLOR(root->state) ? North : South));
    if ((y != (COLOR(root->state) ? 2 : 5)) ||
        (_board[root->ep] != _EMPTY) ||
        (_board[sqr] == _EMPTY) ||
        (_board[sqr]->type != ((!COLOR(root->state))|Pawn)))
    {
      senjo::Output() << "Invalid en passant square: "
                      << senjo::Square(root->ep).ToString();
      return NULL;
    }
  }

  NextSpace(p);
  NextWord(p);
  if (isdigit(*p)) {
    while (*p && isdigit(*p)) {
      root->rcount = ((root->rcount * 10) + (*p++ - '0'));
    }
  }

  NextSpace(p);
  NextWord(p);
  if (isdigit(*p)) {
    while (*p && isdigit(*p)) {
      root->mcount = ((root->mcount * 10) + (*p++ - '0'));
    }
    if (root->mcount) {
      root->mcount = (((root->mcount - 1) * 2) + COLOR(root->state));
    }
  }

  NextSpace(p);
  NextWord(p);

  root->positionKey = (root->pawnKey ^
                       root->pieceKey ^
                       _HASH[0][root->state & 0x1F] ^
                       _HASH[0][root->ep]);
  return p;
}

//-----------------------------------------------------------------------------
std::list<senjo::EngineOption> Clunk::GetOptions() const {
  std::list<senjo::EngineOption> opts;
  return opts;
}

//-----------------------------------------------------------------------------
std::string Clunk::GetAuthorName() const {
  return "Shawn Chidester";
}

//-----------------------------------------------------------------------------
std::string Clunk::GetCountryName() const {
  return "USA";
}

//-----------------------------------------------------------------------------
std::string Clunk::GetEngineName() const {
  return (sizeof(void*) == 8) ? "Clunk" : "Clunk (32-bit)";
}

//-----------------------------------------------------------------------------
std::string Clunk::GetEngineVersion() const {
  std::string rev = MAKE_XSTR(GIT_REV);
  if (rev.size() > 7) {
    rev = rev.substr(0, 7);
  }
  return ("1.0." + rev);
}

//-----------------------------------------------------------------------------
std::string Clunk::GetFEN() const {
  return root->GetFEN();
}

//-----------------------------------------------------------------------------
void Clunk::ClearSearchData() {
  // TODO
}

//-----------------------------------------------------------------------------
void Clunk::Initialize() {
  InitNodes();
  InitDistDir();
  InitMoveMaps();
  SetPosition(_STARTPOS);
  initialized = true;
}

//-----------------------------------------------------------------------------
void Clunk::PonderHit() {
}

//-----------------------------------------------------------------------------
void Clunk::PrintBoard() const {
  root->PrintBoard();
}

//-----------------------------------------------------------------------------
void Clunk::Quit() {
  ChessEngine::Quit();
  clunk::_stop |= senjo::ChessEngine::FullStop;
}

//-----------------------------------------------------------------------------
void Clunk::ResetStatsTotals() {
  _totalStats.Clear();
}

//-----------------------------------------------------------------------------
void Clunk::ShowStatsTotals() const {
  _totalStats.Average().Print();
}

//-----------------------------------------------------------------------------
void Clunk::Stop(const StopReason reason) {
  ChessEngine::Stop(reason);
  clunk::_stop |= reason;
}

//-----------------------------------------------------------------------------
void Clunk::GetStats(int* depth,
                     int* seldepth,
                     uint64_t* nodes,
                     uint64_t* qnodes,
                     uint64_t* msecs,
                     int* movenum,
                     char* move,
                     const size_t movelen) const
{
  if (depth) {
    *depth = _depth;
  }
  if (seldepth) {
    *seldepth = _seldepth;
  }
  if (nodes) {
    *nodes = _execs;
  }
  if (qnodes) {
    *qnodes = _qnodes;
  }
  if (msecs) {
    *msecs = (senjo::Now() - _startTime);
  }
  if (movenum) {
    *movenum = _movenum;
  }
  if (move && movelen) {
    snprintf(move, movelen, "%s", _currmove.c_str());
  }
}

//-----------------------------------------------------------------------------
uint64_t Clunk::MyPerft(const int depth) {
  if (!initialized) {
    senjo::Output() << "Engine not initialized";
    return 0;
  }

  if (_debug) {
    PrintBoard();
    senjo::Output() << GetFEN();
  }

  InitSearch(COLOR(root->state), _startTime);

  const int d = std::min<int>(depth, MaxPlies);
  const uint64_t count = WhiteToMove() ? root->PerftSearchRoot<White>(d)
                                       : root->PerftSearchRoot<Black>(d);

  const uint64_t msecs = (senjo::Now() - _startTime);
  senjo::Output() << "Perft " << count << ' '
                  << senjo::Rate((count / 1000), msecs) << " KLeafs/sec";

  return count;
}

//-----------------------------------------------------------------------------
std::string Clunk::MyGo(const int depth,
                        const int /*movestogo*/,
                        const uint64_t /*movetime*/,
                        const uint64_t /*wtime*/, const uint64_t /*winc*/,
                        const uint64_t /*btime*/, const uint64_t /*binc*/,
                        std::string* /*ponder*/)
{
  if (!initialized) {
    senjo::Output() << "Engine not initialized";
    return std::string();
  }

  if (_debug) {
    PrintBoard();
    senjo::Output() << GetFEN();
  }

  InitSearch(COLOR(root->state), _startTime);

  int d = std::min<int>(depth, MaxPlies);
  if (d <= 0) {
    d = MaxPlies;
  }
  std::string bestmove = (WhiteToMove() ? root->SearchRoot<White>(d)
                                        : root->SearchRoot<Black>(d));

  _totalStats += _stats;
  if (_debug) {
    senjo::Output() << "--- Stats";
    senjo::Output() << _tt.GetStores() << " stores, "
                    << _tt.GetHits() << " hits, "
                    << _tt.GetCheckmates() << " checkmates, "
                    << _tt.GetStalemates() << " stalemates";
    _stats.Print();
  }

  return bestmove;
}

} // namespace clunk
