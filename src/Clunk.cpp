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
const char* NOT_INITIALIZED = "Engine not initialized";

//-----------------------------------------------------------------------------
struct Piece {
  uint8_t type;
  uint8_t sqr;
};

//-----------------------------------------------------------------------------
enum ScoreType {
  CanWin         = 0,
  Passer         = 14,
  Material       = 16,
  Total          = 18,
  ScoreTypeCount = 20
};

//-----------------------------------------------------------------------------
const char* _SCORE[ScoreTypeCount] = {
  "Can Win     ", "Can Win     ",
  "Pawn        ", "Pawn        ",
  "Knight      ", "Knight      ",
  "Bishop      ", "Bishop      ",
  "Rook        ", "Rook        ",
  "Queen       ", "Queen       ",
  "King        ", "King        ",
  "Passer      ", "Passer      ",
  "Material    ", "Material    ",
  "Total       ", "Total       "
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

//----------------------------------------------------------------------------
const int _PASSER_BONUS[8] = { 0, 4, 8, 16, 32, 56, 88, 0 };
const int _CAN_WIN_BONUS   = 50;

//----------------------------------------------------------------------------
const int _PAWN_SQR[128] = {
   0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
   8,  8,  8, -4, -4,  8,  8,  8,   24, 24, 24, 24, 24, 24, 24, 24,
   0,  0,  0,  0,  0,  0,  0,  0,   16, 16, 16, 16, 16, 16, 16, 16,
   0,  0,  8, 10, 10,  8,  0,  0,    8,  8, 10, 12, 12, 10,  8,  8,
   8,  8, 10, 12, 12, 10,  8,  8,    0,  0,  8, 10, 10,  8,  0,  0,
  16, 16, 16, 16, 16, 16, 16, 16,    0,  0,  0,  0,  0,  0,  0,  0,
  24, 24, 24, 24, 24, 24, 24, 24,    8,  8,  8, -4, -4,  8,  8,  8,
   0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0
};

//----------------------------------------------------------------------------
const int _SQR[128] =  {
 -24,-16,-12, -8, -8,-12,-16,-24,   -4,  8,  8, -8, -8,  8,  8, -4,
 -16,-12, -8,  0,  0, -8,-12,-16,   -4, -8,-12,-12,-12,-12, -8, -4,
 -12, -8,  0,  8,  8,  0, -8,-12,   -8,-12,-16,-16,-16,-16,-12, -8,
  -8,  0,  8, 12, 12,  8,  0, -8,  -12,-16,-24,-24,-24,-24,-16,-12,
  -8,  0,  8, 12, 12,  8,  0, -8,  -12,-16,-24,-24,-24,-24,-16,-12,
 -12, -8,  0,  8,  8,  0, -8,-12,   -8,-12,-16,-16,-16,-16,-12, -8,
 -16,-12, -8,  0,  0, -8,-12,-16,   -4, -8,-12,-12,-12,-12, -8, -4,
 -24,-16,-12, -8, -8,-12,-16,-24,   -4,  8,  8, -8, -8,  8,  8, -4
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
const uint64_t _DIAG_DIRS  = 0xFF00FF0000FF00FFULL;
const uint64_t _CROSS_DIRS = 0x00FF00FFFF00FF00ULL;

//-----------------------------------------------------------------------------
char     _dir[128][128] = {0};
char     _dist[128][128] = {0};
char     _knightDir[128][128] = {0};
char     _kingDir[128] = {0};
Piece*   _board[128] = {0};
Piece    _piece[PieceListSize];
int      _seenIndex = 0;
int      _pcount[12] = {0};
int      _material[2] = {0};
uint64_t _atk[128] = {0};
uint64_t _seen[MaxPlies] = {0};
uint64_t _pawnCaps[128] = {0};
uint64_t _knightMoves[128] = {0};
uint64_t _bishopRook[128] = {0};
uint64_t _queenKing[128] = {0};
uint8_t  _seenFilter[SeenFilterMask + 1] = {0};

#ifndef NDEBUG
uint64_t _seenStack[8000] = {0};
uint64_t* _seenStackTop = _seenStack;
#endif

//-----------------------------------------------------------------------------
Piece* _EMPTY              = _piece; // _piece[0]
const Piece* _KING[2]      = { (_piece + WhiteKingOffset),
                               (_piece + BlackKingOffset) };
const Piece* _FIRST_SLIDER = (_piece + BishopOffset);
const Piece* _FIRST_ROOK   = (_piece + RookOffset);

//-----------------------------------------------------------------------------
int         _stop = 0;
int         _depth = 0;
int         _seldepth = 0;
int         _movenum = 0;
int         _drawScore[2] = {0};
uint64_t    _startTime = 0;
bool        _debug = false;
char        _hist[TwelveBits + 1] = {0};
std::string _currmove;
Stats       _stats;
Stats       _totalStats;

//-----------------------------------------------------------------------------
TranspositionTable<PawnEntry> _pawnTT;
TranspositionTable<HashEntry> _tt;

//-----------------------------------------------------------------------------
void InitSearch(const Color colorToMove, const uint64_t startTime) {
  _startTime = startTime;
  _stop     &= senjo::ChessEngine::FullStop;
  _depth     = 0;
  _seldepth  = 0;
  _movenum   = 0;

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
  memset(_knightDir, 0, sizeof(_knightDir));
  const int knightDirs[8] = {
    KnightMove1, KnightMove2, KnightMove3, KnightMove4,
    KnightMove5, KnightMove6, KnightMove7, KnightMove8
  };
  for (int a = A1; a <= H8; ++a) {
    if (BAD_SQR(a)) {
      a += 7;
      continue;
    }
    for (int i = 0; i < 8; ++i) {
      const int to = (a + knightDirs[i]);
      if (IS_SQUARE(to)) {
        _knightDir[a][to] = 1;
      }
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
          if (((a - b) % 17) == 0) {
            _dir[a][b] = SouthWest;
          }
        }
        else if (y1 < y2) {
          if (((b - a) % 15) == 0) {
            _dir[a][b] = NorthWest;
          }
        }
      }
      else if (x1 < x2) {
        if (y1 > y2) {
          if (((a - b) % 15) == 0) {
            _dir[a][b] = SouthEast;
          }
        }
        else if (y1 < y2) {
          if (((b - a) % 17) == 0) {
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
inline int IsKnightMove(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  return _knightDir[from][to];
}

//-----------------------------------------------------------------------------
inline int ValueOf(const int pc) {
  assert(!pc || IS_CAP(pc));
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
    pc = const_cast<Piece*>(_KING[White]);
    assert(!pc->type & !pc->sqr); // only allow this once
    break;
  case (Black|King):
    pc = const_cast<Piece*>(_KING[Black]);
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
  const int DIRECTION[8] = {
    KnightMove1, KnightMove2, KnightMove3, KnightMove4,
    KnightMove5, KnightMove6, KnightMove7, KnightMove8
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  for (int i = 0; i < 8; ++i) {
    const int to = (from + DIRECTION[i]);
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
  const int DIRECTION[4] = {
    SouthWest, SouthEast, NorthWest, NorthEast
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  for (int i = 0; i < 4; ++i) {
    const int dir = DIRECTION[i];
    int end = (from + dir);
    if (IS_SQUARE(end)) {
      while (IS_SQUARE(end + dir)) end += dir;
      assert(IS_SQUARE(end));
      assert(Direction(from, end) == dir);
      assert(shift <= 24);
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
  const int DIRECTION[4] = {
    South, West, East, North
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  for (int i = 0; i < 4; ++i) {
    const int dir = DIRECTION[i];
    int end = (from + dir);
    if (IS_SQUARE(end)) {
      while (IS_SQUARE(end + dir)) end += dir;
      assert(IS_SQUARE(end));
      assert(Direction(from, end) == dir);
      assert(shift <= 24);
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
  const int DIRECTION[8] = {
    SouthWest, South, SouthEast, West,
    East, NorthWest, North, NorthEast
  };
  uint64_t mvs = 0ULL;
  int shift = 0;
  for (int i = 0; i < 8; ++i) {
    const int dir = DIRECTION[i];
    int end = (from + dir);
    if (IS_SQUARE(end)) {
      while (IS_SQUARE(end + dir)) end += dir;
      assert(IS_SQUARE(end));
      assert(Direction(from, end) == dir);
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
template<Color color>
void ClearKingDirs(const int from) {
  assert(IS_SQUARE(from));
  for (uint64_t mvs = _queenKing[from]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = Direction(from, end);
    assert(IS_DIR(dir));
    for (int to = (from + dir);; to += dir) {
      assert(IS_SQUARE(to));
      assert(Direction(from, to) == dir);
      if (_kingDir[to + (color * 8)]) {
        _kingDir[to + (color * 8)] = 0;
      }
      else {
        break;
      }
      if (to == end) {
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------------
template<Color color>
void SetKingDirs(const int from) {
  assert(IS_SQUARE(from));
  for (uint64_t mvs = _queenKing[from]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = Direction(from, end);
    assert(IS_DIR(dir));
    for (int to = (from + dir);; to += dir) {
      assert(IS_SQUARE(to));
      assert(Direction(from, to) == dir);
      assert(!_kingDir[to + (color * 8)]);
      _kingDir[to + (color * 8)] = -dir;
      if ((to == end) || (_board[to] != _EMPTY)) {
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------------
template<Color color>
void UpdateKingDirs(const int from, const int to) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  assert(from != to);
  assert(_board[to] != _EMPTY);
  int dir = _kingDir[from + (color * 8)];
  if (dir && (_board[from] == _EMPTY)) {
    assert(IS_DIR(dir));
    assert(Direction(from, _KING[color]->sqr) == dir);
    if (Direction(from, to) != dir) {
      for (int sqr = (from - dir); IS_SQUARE(sqr); sqr -= dir) {
        assert(Direction(from, sqr) == -dir);
        _kingDir[sqr + (color * 8)] = dir;
        if (sqr == to) {
          return;
        }
        if (_board[sqr] != _EMPTY) {
          break;
        }
      }
    }
  }
  if ((dir = _kingDir[to + (color * 8)])) {
    assert(IS_DIR(dir));
    assert(Direction(to, _KING[color]->sqr) == dir);
    for (int sqr = (to - dir); IS_SQUARE(sqr) && _kingDir[sqr + (color * 8)];
         sqr -= dir)
    {
      assert(Direction(to, sqr) == -dir);
      _kingDir[sqr + (color * 8)] = 0;
    }
  }
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
  assert(!(_atk[to] & (0xFFULL << DirShift(dir))) ||
         ((_atk[to] & (0xFFULL << DirShift(dir))) ==
          (uint64_t(from + 1) << DirShift(dir))));
  _atk[to] |= (uint64_t(from + 1) << DirShift(dir));
}

//-----------------------------------------------------------------------------
inline void AddSlide(const int from, const int to, const int dir) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  assert(from != to);
  assert(_board[from] >= _FIRST_SLIDER);
  assert(IS_SLIDER(_board[from]->type));
  assert(_board[from]->sqr == from);
  assert(Direction(from, to) == dir);
  assert(!(_atk[from + 8] & (0xFFULL << DirShift(dir))) ||
         ((_atk[from + 8] & (0xFFULL << DirShift(dir))) ==
          (uint64_t(to + 1) << DirShift(dir))));
  _atk[from + 8] |= (uint64_t(to + 1) << DirShift(dir));
}

//-----------------------------------------------------------------------------
inline void SetSlide(const int from, const int to, const int dir) {
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  assert(from != to);
  assert(_board[from] >= _FIRST_SLIDER);
  assert(IS_SLIDER(_board[from]->type));
  assert(_board[from]->sqr == from);
  assert(Direction(from, to) == dir);
  const int shift = DirShift(dir);
  _atk[from + 8] = ((_atk[from + 8] & ~(0xFFULL << shift)) |
      (uint64_t(to + 1) << shift));
}

//-----------------------------------------------------------------------------
inline void ClearAttack(const int to, const int dir) {
  assert(IS_SQUARE(to));
  _atk[to] &= ~(0xFFULL << DirShift(dir));
}

//-----------------------------------------------------------------------------
inline void ClearSlide(const int from, const int dir) {
  assert(IS_SQUARE(from));
  assert(_board[from] >= _FIRST_SLIDER);
  assert(IS_SLIDER(_board[from]->type));
  assert(_board[from]->sqr == from);
  _atk[from + 8] &= ~(0xFFULL << DirShift(dir));
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
        AddSlide(from, to, dir);
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
    ClearSlide(from, dir);
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
  for (uint64_t tmp = _atk[to]; tmp; tmp >>= 8) {
    if (tmp & 0xFF) {
      const int from = static_cast<int>((tmp & 0xFF) - 1);
      assert(IS_SQUARE(from));
      assert(_board[from] >= _FIRST_SLIDER);
      assert(IS_SLIDER(_board[from]->type));
      assert(_board[from]->sqr == from);
      const int dir = Direction(from, to);
      SetSlide(from, to, dir);
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
  for (uint64_t tmp = _atk[to]; tmp; tmp >>= 8) {
    if (tmp & 0xFF) {
      const int from = static_cast<int>((tmp & 0xFF) - 1);
      assert(IS_SQUARE(from));
      assert(_board[from] >= _FIRST_SLIDER);
      assert(IS_SLIDER(_board[from]->type));
      assert(_board[from]->sqr == from);
      const int dir = Direction(from, to);
      assert(IS_DIR(dir));
      int end = to;
      for (int ato = (to + dir); IS_SQUARE(ato); ato += dir) {
        assert(Direction(from, ato) == dir);
        AddAttack(from, (end = ato), dir);
        if (_board[ato] != _EMPTY) {
          break;
        }
      }
      if (end != to) {
        SetSlide(from, end, dir);
      }
    }
  }
}

//-----------------------------------------------------------------------------
template<Color color>
bool AttackedBy(const int sqr) {
  assert(IS_SQUARE(sqr));
  if (_pcount[color]) {
    assert(_pcount[color] > 0);
    for (uint64_t mvs = _atk[sqr]; mvs; mvs >>= 8) {
      if (mvs & 0xFF) {
        assert(IS_SQUARE((mvs & 0xFF) - 1));
        assert(IS_DIR(Direction(((mvs & 0xFF) - 1), sqr)));
        assert(_board[(mvs & 0xFF) - 1] >= _FIRST_SLIDER);
        assert(_board[(mvs & 0xFF) - 1]->sqr == ((mvs & 0xFF) - 1));
        assert(IS_SLIDER(_board[(mvs & 0xFF) - 1]->type));
        if (COLOR(_board[(mvs & 0xFF) - 1]->type) == color) {
          return true;
        }
      }
    }
  }
  if (_pcount[color|Knight]) {
    assert(_pcount[color|Knight] > 0);
    for (uint64_t mvs = _knightMoves[sqr]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      assert(IS_SQUARE((mvs & 0xFF) - 1));
      assert(int((mvs & 0xFF) - 1) != sqr);
      if (_board[(mvs & 0xFF) - 1]->type == (color|Knight)) {
        assert(_board[(mvs & 0xFF) - 1]->sqr == ((mvs & 0xFF) - 1));
        return true;
      }
    }
  }
  for (uint64_t mvs = _queenKing[sqr + 8]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int from = ((mvs & 0xFF) - 1);
    assert(IS_DIR(Direction(sqr, from)));
    if (_board[from] == _KING[color]) {
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
bool VerifyAttacksTo(const int to, const bool do_assert, char kdir[128]) {
  uint64_t atk = 0;
  const bool king = (_board[to]->type >= King);
  const int idx = (8 * COLOR(_board[to]->type));
  if (king) {
    assert(!_kingDir[to + idx]);
  }
  for (uint64_t mvs = _queenKing[to]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = Direction(end, to);
    assert(IS_DIR(dir));
    for (int from = (to - dir);; from -= dir) {
      assert(IS_SQUARE(from));
      assert(Direction(from, to) == dir);
      if (king) {
        kdir[from + idx] = dir;
      }
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
    assert(atk == _atk[to]);
  }
  return (atk == _atk[to]);
}

//-----------------------------------------------------------------------------
bool VerifySlide(uint64_t mvs, const int from, const bool do_assert) {
  uint64_t slide = 0;
  while (mvs) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = Direction(from, end);
    assert(IS_DIR(dir));
    assert(!(slide & (0xFFULL << DirShift(dir))));
    for (int to = (from + dir);; to += dir) {
      assert(IS_SQUARE(to));
      assert(Direction(from, to) == dir);
      if ((to == end) || (_board[to] != _EMPTY)) {
        slide |= (uint64_t(to + 1) << DirShift(dir));
        break;
      }
    }
    assert(slide & (0xFFULL << DirShift(dir)));
    mvs >>= 8;
  }
  if (do_assert) {
    assert(slide == _atk[from + 8]);
  }
  return (slide == _atk[from + 8]);
}

//-----------------------------------------------------------------------------
bool VerifyAttacks(const bool do_assert) {
  char kdir[128] = {0};
  for (int sqr = A1; sqr <= H8; ++sqr) {
    if (BAD_SQR(sqr)) {
      sqr += 7;
    }
    else {
      if (!VerifyAttacksTo(sqr, do_assert, kdir)) {
        return false;
      }
      switch (Black|_board[sqr]->type) {
      case (Black|Bishop):
        if (!VerifySlide(_bishopRook[sqr], sqr, do_assert)) {
          return false;
        }
        break;
      case (Black|Rook):
        if (!VerifySlide(_bishopRook[sqr + 8], sqr, do_assert)) {
          return false;
        }
        break;
      case (Black|Queen):
        if (!VerifySlide(_queenKing[sqr], sqr, do_assert)) {
          return false;
        }
        break;
      }
    }
  }
  if (memcmp(kdir, _kingDir, sizeof(kdir))) {
    if (do_assert) {
      assert(false);
    }
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------
template<Color color>
bool EpPinned(const int from, const int cap) {
  assert(_KING[color]->type == (color|King));
  assert(IS_SQUARE(_KING[color]->sqr));
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(cap));
  assert(from != cap);
  assert(from != _KING[color]->sqr);
  assert(YC(from) == YC(cap));
  assert(YC(from) == (color ? 3 : 4));
  assert(Distance(from, cap) == 1);
  assert(_board[from] != _EMPTY);
  assert(_board[cap] != _EMPTY);
  assert(_board[from]->type == (color|Pawn));
  assert(_board[cap]->type == ((!color)|Pawn));
  if (!_pcount[!color] || (YC(from) != YC(_KING[color]->sqr))) {
    return false;
  }
  int left = std::min<int>(from, cap);
  int right = std::max<int>(from, cap);
  if (_KING[color]->sqr < left) {
    right = ((_atk[right] >> DirShift(West)) & 0xFF);
    assert(!right || (IS_SQUARE(right - 1) && (YC(right - 1) == YC(from))));
    assert(!right || (_board[right - 1] >= _FIRST_SLIDER));
    if (right-- && (_board[right] >= _FIRST_ROOK) &&
        (COLOR(_board[right]->type) != color))
    {
      assert(_board[right]->type >= Rook);
      assert(_board[right]->type < King);
      assert(_board[right]->sqr == right);
      while (--left > _KING[color]->sqr) {
        assert(IS_SQUARE(left));
        assert(Direction(right, left) == West);
        if (_board[left] != _EMPTY) {
          assert(left != _KING[color]->sqr);
          return false;
        }
      }
      assert(left == _KING[color]->sqr);
      return true;
    }
  }
  else {
    assert(_KING[color]->sqr > right);
    left = ((_atk[left] >> DirShift(East)) & 0xFF);
    assert(!left || (IS_SQUARE(left - 1) && (YC(left - 1) == YC(from))));
    assert(!left || (_board[left - 1] >= _FIRST_ROOK));
    if (left-- && (_board[left] >= _FIRST_ROOK) &&
        (COLOR(_board[left]->type) != color))
    {
      assert(_board[left]->type >= Rook);
      assert(_board[left]->type < King);
      assert(_board[left]->sqr == left);
      while (++right < _KING[color]->sqr) {
        assert(IS_SQUARE(right));
        assert(Direction(left, right) == East);
        if (_board[right] != _EMPTY) {
          assert(right != _KING[color]->sqr);
          return false;
        }
      }
      assert(right == _KING[color]->sqr);
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
inline void IncHistory(const Move& move, const int depth) {
  assert(move.IsValid());
  assert((depth >= 0) & (depth <= MaxPlies));
  const int idx = move.TypeToIndex();
  const int val = (_hist[idx] + depth + 2);
  _hist[idx] = static_cast<char>(std::min<int>(val, 16));
}

//-----------------------------------------------------------------------------
inline void DecHistory(const Move& move) {
  const int idx = move.TypeToIndex();
  const int val = (_hist[idx] - 1);
  _hist[idx] = static_cast<char>(std::max<int>(val, -2));
}

//-----------------------------------------------------------------------------
inline double EndGame(const Color color) {
  return (static_cast<double>(StartMaterial-_material[!color])/StartMaterial);
}

//-----------------------------------------------------------------------------
inline double MidGame(const Color color) {
  return (static_cast<double>(_material[!color]) / StartMaterial);
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
  int state;
  int ep;
  int rcount;
  int mcount;
  uint64_t pawnKey;
  uint64_t pieceKey;
  uint64_t positionKey;
  Move lastMove;
  int checks;

  //---------------------------------------------------------------------------
  // updated by Evaluate()
  //---------------------------------------------------------------------------
#ifndef NDEBUG
  int scoreType[ScoreTypeCount];
#endif
  int atkCount[2];
  int atkScore[2];
  int standPat;

  //---------------------------------------------------------------------------
  // updated by move generation and search
  //---------------------------------------------------------------------------
  int moveIndex;
  int moveCount;
  Move moves[MaxMoves];

  //---------------------------------------------------------------------------
  // updated during search
  //---------------------------------------------------------------------------
  int depthChange;
  int pvCount;
  Move killer[2];
//  Move counter[128][128];
  Move pv[MaxPlies];

  //---------------------------------------------------------------------------
  bool InSeenStack() const {
#ifndef NDEBUG
    for (uint64_t* top = _seenStackTop; (top-- > _seenStack); ) {
      if (positionKey == *top) {
        return true;
      }
    }
#endif
    return false;
  }

  //---------------------------------------------------------------------------
  bool HasRepeated() const {
    if ((rcount > 1) & (_seenFilter[positionKey & SeenFilterMask] != 0)) {
      for (int n = rcount, i = _seenIndex; n--;) {
        assert((i >= 0) & (i < MaxPlies));
        i += ((MaxPlies * !i) - 1);
        assert((i >= 0) & (i < MaxPlies));
        if (_seen[i] == positionKey) {
          assert(InSeenStack());
          return true;
        }
      }
    }
    return false;
  }

  //---------------------------------------------------------------------------
  inline bool IsDraw() const {
    return (((state & Draw) | (rcount >= 100)) || HasRepeated());
  }

  //---------------------------------------------------------------------------
  inline void AddKiller(const Move& move, const int depth) {
    if (!move.IsCapOrPromo()) {
      if (move != killer[0]) {
        killer[1] = killer[0];
        killer[0] = move;
      }
      if (!checks) {
        IncHistory(move, depth);
      }
    }
//    if (lastMove) {
//      assert(lastMove.IsValid());
//      counter[lastMove.From()][lastMove.To()] = move;
//    }
  }

  //---------------------------------------------------------------------------
  inline bool IsKiller(const Move& move) const {
    return ((move == killer[0]) | (move == killer[1]) /*|
            (move == counter[lastMove.From()][lastMove.To()])*/);
  }

  //---------------------------------------------------------------------------
  void ClearKillers() {
    killer[0].Clear();
    killer[1].Clear();
//    memset(counter, 0, sizeof(counter));
  }

  //---------------------------------------------------------------------------
  void PrintBoard(const bool do_eval) {
    if (do_eval) {
      Evaluate();
    }
    const int mat = (_material[White] - _material[Black]);
    const int eval = (COLOR(state) ? -standPat : standPat);
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
        out << "  Move Number       : " << ((mcount + 2) / 2);
        break;
      case 5:
        out << "  Reversible Moves  : " << rcount;
        break;
      case 4:
        out << "  Castling Rights   : ";
        if (state & WhiteShort) out << 'K';
        if (state & WhiteLong)  out << 'Q';
        if (state & BlackShort) out << 'k';
        if (state & BlackLong)  out << 'q';
        break;
      case 3:
        if (ep != None) {
          out << "  En Passant Square : " << senjo::Square(ep).ToString();
        }
        break;
      case 2:
        out << "  Material Balance  : " << mat;
        break;
      case 1:
        out << "  Evaluation        : " << eval;
        break;
      case 0:
        out << "  Compensation      : " << (eval - mat);
        break;
      }
      out << '\n';
    }

#ifndef NDEBUG
    if (_debug && do_eval) {
      out << '\n';
      out << "            \tWhite\tBlack\tSum\n";
      for (int type = 0; type < ScoreTypeCount; type += 2) {
        int w = scoreType[White|type];
        int b = scoreType[Black|type];
        int sum = (w - b);
        out << _SCORE[type] << '\t' << w << '\t' << b << '\t' << sum << '\n';
      }
      int w = (scoreType[White|Total] - scoreType[White|Material]);
      int b = (scoreType[Black|Total] - scoreType[Black|Material]);
      int adjust = (eval - (scoreType[White|Total] - scoreType[Black|Total]));
      out << "Compensation\t" << w << '\t' << b << '\t' << (w - b) << '\n';
      out << "Draw Adjustment\t" << adjust << '\n';
    }
#endif
  }

  //---------------------------------------------------------------------------
  std::string GetFEN() const {
    char fen[256] = {0};
    char* p = fen;
    int empty = 0;
    for (int y = 7; y >= 0; --y) {
      for (int x = 0; x < 8; ++x) {
        const int type = _board[SQR(x,y)]->type;
        if (type && empty) {
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
  template<Color color>
  void FindCheckers() {
    assert(COLOR(state) == color);
    assert(_KING[color]);
    assert(_KING[color]->type == (color|King));

    const int sqr = _KING[color]->sqr;
    assert(IS_SQUARE(sqr));
    assert(_board[sqr] == _KING[color]);

    checks = 0;

    if (_pcount[!color]) {
      assert(_pcount[!color] > 0);
      for (uint64_t mvs = _atk[sqr]; mvs; mvs >>= 8) {
        if (mvs & 0xFF) {
          assert(IS_SQUARE((mvs & 0xFF) - 1));
          assert(IS_DIR(Direction(((mvs & 0xFF) - 1), sqr)));
          assert(_board[(mvs & 0xFF) - 1] >= _FIRST_SLIDER);
          assert(_board[(mvs & 0xFF) - 1]->sqr == ((mvs & 0xFF) - 1));
          assert(IS_SLIDER(_board[(mvs & 0xFF) - 1]->type));
          if (COLOR(_board[(mvs & 0xFF) - 1]->type) != color) {
            assert(!(checks & 0xFF00));
            checks = ((checks << 8) | (mvs & 0xFF));
          }
        }
      }
    }

    if (_pcount[(!color)|Knight]) {
      assert(_pcount[(!color)|Knight] > 0);
      for (uint64_t mvs = _knightMoves[sqr]; mvs; mvs >>= 8) {
        assert(mvs & 0xFF);
        assert(IS_SQUARE((mvs & 0xFF) - 1));
        assert(int((mvs & 0xFF) - 1) != sqr);
        if (_board[(mvs & 0xFF) - 1]->type == ((!color)|Knight)) {
          assert(_board[(mvs & 0xFF) - 1]->sqr == ((mvs & 0xFF) - 1));
          assert(!(checks & 0xFF00));
          checks = ((checks << 8) | (mvs & 0xFF));
        }
      }
    }

    for (uint64_t mvs = _queenKing[sqr + 8]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int from = ((mvs & 0xFF) - 1);
      assert(IS_DIR(Direction(sqr, from)));
      if (_board[from] == _KING[!color]) {
        assert(_board[from]->type == ((!color)|King));
        assert(_board[from]->sqr == from);
        assert(!(checks & 0xFF00));
        checks = ((checks << 8) | (mvs & 0xFF));
      }
      else if (_board[from]->type == ((!color)|Pawn)) {
        assert(_board[from]->sqr == from);
        if (color) {
          switch (Direction(from, sqr)) {
          case NorthWest: case NorthEast:
            assert(!(checks & 0xFF00));
            checks = ((checks << 8) | (mvs & 0xFF));
            break;
          }
        }
        else {
          switch (Direction(from, sqr)) {
          case SouthWest: case SouthEast:
            assert(!(checks & 0xFF00));
            checks = ((checks << 8) | (mvs & 0xFF));
            break;
          }
        }
      }
    }
  }

  //---------------------------------------------------------------------------
  inline int GetPinDir(const Color color, const int from) {
    assert(IS_SQUARE(from));
    const int kdir = _kingDir[from + (color * 8)];
    if ((kdir != 0) & (_atk[from] != 0ULL)) {
      const int tmp = ((_atk[from] >> DirShift(kdir)) & 0xFF);
      if (tmp) {
        assert(IS_SQUARE(tmp - 1));
        assert(_board[tmp - 1] >= _FIRST_SLIDER);
        assert(IS_SLIDER(_board[tmp - 1]->type));
        if (COLOR(_board[tmp - 1]->type) != color) {
          return abs(kdir);
        }
      }
    }
    return 0;
  }

  //---------------------------------------------------------------------------
  int DuplicateMoveCount() {
    int dupes = 0;
    Move dupeMoves[MaxMoves];
    for (int i = 0; i < moveCount; ++i) {
      for (int n = (i + 1); n < moveCount; ++n) {
        if (moves[i] == moves[n]) {
          dupeMoves[dupes++] = moves[i];
        }
      }
    }
    if (dupes > 0) {
      PrintBoard(false);
      for (int i = 0; i < dupes; ++i) {
        senjo::Output() << dupeMoves[i].ToString();
      }
    }
    return dupes;
  }

  //---------------------------------------------------------------------------
  inline int GetDiscoverDir(const Color color, const int from) {
    assert(IS_SQUARE(from));
    const int kdir = _kingDir[from + (8 * !color)];
    if ((kdir != 0) & (_atk[from] != 0ULL)) {
      const int tmp = ((_atk[from] >> DirShift(kdir)) & 0xFF);
      if (tmp) {
        assert(IS_SQUARE(tmp - 1));
        assert(_board[tmp - 1] >= _FIRST_SLIDER);
        assert(IS_SLIDER(_board[tmp - 1]->type));
        if (COLOR(_board[tmp - 1]->type) == color) {
          return abs(kdir);
        }
      }
    }
    return 0;
  }

  //---------------------------------------------------------------------------
  inline void AddMove(const MoveType type,
                      const int from, const int to, const int score,
                      const int cap = 0, const int promo = 0)
  {
    assert(moveCount >= 0);
    assert((moveCount + 1) < MaxMoves);
    Move& move = moves[moveCount++];
    move.Set(type, from, to, cap, promo, score);
    move.IncScore(50 * IsKiller(move));
  }

  //---------------------------------------------------------------------------
  template<Color color, bool qsearch>
  void GetPawnMoves(const int from, const int depth) {
    assert(!checks);
    assert(IS_SQUARE(from));
    assert(_board[from] != _EMPTY);
    assert(_board[from]->type == (color|Pawn));
    assert(_board[from]->sqr == from);
    const int pinDir = GetPinDir(color, from);
    int score;
    int to;

    int kdir = 0;
    if (qsearch & (!depth)) {
      kdir = GetDiscoverDir(color, from);
      if (abs(kdir) == North) {
        kdir = 0;
      }
    }

    for (uint64_t mvs = _pawnCaps[from + (color * 8)]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      to = ((mvs & 0xFF) - 1);
      assert(Distance(from, to) == 1);
      assert(IS_DIR(Direction(from, to)));
      if (pinDir && (abs(Direction(from, to)) != pinDir)) {
        continue;
      }
      const int cap = _board[to]->type;
      assert(cap || (_board[to] == _EMPTY));
      if (!cap) {
        if ((ep != None) && (to == ep) &&
            !EpPinned<color>(from, (ep + (color ? North : South))))
        {
          AddMove(PawnCap, from, to, PawnValue);
        }
      }
      else if (COLOR(cap) != color) {
        score = ValueOf(cap);
        if (YC(to) == (color ? 0 : 7)) {
          AddMove(PawnCap, from, to, (score + QueenValue),  cap, (color|Queen));
          if (!qsearch || !depth) {
            AddMove(PawnCap, from, to, (score + RookValue),   cap, (color|Rook));
            AddMove(PawnCap, from, to, (score + BishopValue), cap, (color|Bishop));
            AddMove(PawnCap, from, to, (score + KnightValue), cap, (color|Knight));
          }
        }
        else {
          AddMove(PawnCap, from, to, score, cap);
        }
      }
    }
    if (!qsearch || ((!depth) | (YC(from) == (color ? 1 : 6)))) {
      to = (from + (color ? South : North));
      if (pinDir && (abs(Direction(from, to)) != pinDir)) {
        return;
      }
      if (_board[to] == _EMPTY) {
        if (YC(to) == (color ? 0 : 7)) {
          AddMove(PawnMove, from, to, QueenValue,  0, (color|Queen));
          if (!qsearch || !depth) {
            AddMove(PawnMove, from, to, RookValue,   0, (color|Rook));
            AddMove(PawnMove, from, to, BishopValue, 0, (color|Bishop));
            AddMove(PawnMove, from, to, KnightValue, 0, (color|Knight));
          }
        }
        else {
          if (qsearch) {
            assert(!depth);
            if (kdir |
                ((to + (color ? SouthWest : NorthWest)) == _KING[!color]->sqr) |
                ((to + (color ? SouthEast : NorthEast)) == _KING[!color]->sqr))
            {
              score = (_PAWN_SQR[to + (8 * color)] -
                       _PAWN_SQR[from + (8 * color)] + 25);
              AddMove(PawnMove, from, to, score);
            }
          }
          else {
            score = (_PAWN_SQR[to + (8 * color)] -
                     _PAWN_SQR[from + (8 * color)] + 5);
            AddMove(PawnMove, from, to, score);
          }
          if (YC(from) == (color ? 6 : 1)) {
            to += (color ? South : North);
            assert(IS_SQUARE(to));
            if (_board[to] == _EMPTY) {
              if (qsearch) {
                assert(!depth);
                if (kdir |
                    ((to + (color ? SouthWest : NorthWest)) == _KING[!color]->sqr) |
                    ((to + (color ? SouthEast : NorthEast)) == _KING[!color]->sqr))
                {
                  score = (_PAWN_SQR[to + (8 * color)] -
                           _PAWN_SQR[from + (8 * color)] + 30);
                  AddMove(PawnLung, from, to, score);
                }
              }
              else {
                score = (_PAWN_SQR[to + (8 * color)] -
                         _PAWN_SQR[from + (8 * color)] + 10);
                AddMove(PawnLung, from, to, score);
              }
            }
          }
        }
      }
    }
  }

  //---------------------------------------------------------------------------
  template<bool qsearch>
  void GetKnightMoves(const Color color, const int from, const int depth) {
    assert(!checks);
    assert(IS_SQUARE(from));
    assert(_board[from] != _EMPTY);
    assert(_board[from]->type == (color|Knight));
    assert(_board[from]->sqr == from);
    if (GetPinDir(color, from)) {
      return;
    }

    int kdir = 0;
    if (qsearch && !depth) {
      kdir = GetDiscoverDir(color, from);
    }

    int score;
    for (uint64_t mvs = _knightMoves[from]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(IS_SQUARE(to));
      assert(to != from);
      const int cap = _board[to]->type;
      assert(cap || (_board[to] == _EMPTY));
      if (!cap) {
        if (qsearch) {
          if (!depth && (kdir | IsKnightMove(_KING[!color]->sqr, to))) {
            score = (_SQR[to] - _SQR[from] + 20);
            AddMove(KnightMove, from, to, score);
          }
        }
        else {
          score = (_SQR[to] - _SQR[from]);
          AddMove(KnightMove, from, to, score);
        }
      }
      else if (COLOR(cap) != color) {
        score = (ValueOf(cap) + _SQR[to] - _SQR[from] - (8 * KnightMove));
        AddMove(KnightMove, from, to, score, cap);
      }
    }
  }

  //---------------------------------------------------------------------------
  void GetSliderCaptures(const Color color, const MoveType type,
                         uint64_t mvs, const int from)
  {
    assert(!checks);
    assert(IS_SQUARE(from));
    assert(_board[from] != _EMPTY);
    assert(_board[from]->type == (color|type));
    assert(_board[from]->sqr == from);
    const int pinDir = GetPinDir(color, from);
    int score;
    while (mvs) {
      if (mvs & 0xFF) {
        const int to = ((mvs & 0xFF) - 1);
        const int dir = (Direction(from, to));
        assert(IS_DIR(dir));
        if (!pinDir || (abs(dir) == pinDir)) {
          const int cap = _board[to]->type;
          assert(cap || (_board[to] == _EMPTY));
          if ((cap != 0) & (COLOR(cap) != color)) {
            score = (ValueOf(cap) - Distance(from, to) - (8 * type));
            AddMove(type, from, to, score, cap);
          }
        }
      }
      mvs >>= 8;
    }
  }

  //---------------------------------------------------------------------------
  void GetSliderMoves(const Color color, const MoveType type,
                      uint64_t mvs, const int from)
  {
    assert(!checks);
    assert(IS_SQUARE(from));
    assert(_board[from] != _EMPTY);
    assert(_board[from]->type == (color|type));
    assert(_board[from]->sqr == from);
    const int pinDir = GetPinDir(color, from);
    int score;
    while (mvs) {
      assert(mvs & 0xFF);
      const int end = ((mvs & 0xFF) - 1);
      const int dir = (Direction(from, end));
      assert(IS_DIR(dir));
      if (!pinDir || (abs(dir) == pinDir)) {
        for (int to = (from + dir);; to += dir) {
          assert(IS_SQUARE(to));
          assert(Direction(from, to) == dir);
          const int cap = _board[to]->type;
          assert(cap || (_board[to] == _EMPTY));
          if (!cap) {
            score = (_SQR[to] - _SQR[from]);
            AddMove(type, from, to, score);
          }
          else {
            if (COLOR(cap) != color) {
              score = (ValueOf(cap) - Distance(from, to) - (8 * type));
              AddMove(type, from, to, score, cap);
            }
            break;
          }
          if (to == end) {
            break;
          }
        }
      }
      mvs >>= 8;
    }
  }

  //---------------------------------------------------------------------------
  void GetSliderDiscovers(const Color color, const MoveType type,
                          uint64_t mvs, const int from)
  {
    assert(!checks);
    assert(IS_SQUARE(from));
    assert(_board[from] != _EMPTY);
    assert(_board[from]->type == (color|type));
    assert(_board[from]->sqr == from);
    const int pinDir = GetPinDir(color, from);
    while (mvs) {
      assert(mvs & 0xFF);
      const int end = ((mvs & 0xFF) - 1);
      const int dir = (Direction(from, end));
      assert(IS_DIR(dir));
      if (!pinDir || (abs(dir) == pinDir)) {
        for (int to = (from + dir);; to += dir) {
          assert(IS_SQUARE(to));
          assert(Direction(from, to) == dir);
          const int cap = _board[to]->type;
          assert(cap || (_board[to] == _EMPTY));
          if (!cap) {
            if ((type == BishopMove) ? !IS_DIAG(_kingDir[to + (8 * !color)])
                                     : !IS_CROSS(_kingDir[to + (8 * !color)]))
            {
              const int score = (_SQR[to] - _SQR[from] + 10);
              AddMove(type, from, to, score);
            }
          }
          else {
            break;
          }
          if (to == end) {
            break;
          }
        }
      }
      mvs >>= 8;
    }
  }

  //---------------------------------------------------------------------------
  template<Color color>
  void GetSliderChecks() {
    const int king = _KING[!color]->sqr;
    assert(IS_SQUARE(king));
    assert(_board[king] == _KING[!color]);

    int score;
    int pinDir;
    for (uint64_t mvs = _queenKing[king]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int end = ((mvs & 0xFF) - 1);
      const int dir = Direction(king, end);
      assert(IS_DIR(dir));
      for (int to = (king + dir);; to += dir) {
        assert(IS_SQUARE(to));
        assert(Direction(king, to) == dir);
        for (uint64_t tmp = _atk[to]; tmp; tmp >>= 8) {
          if (tmp & 0xFF) {
            const int from = ((tmp & 0xFF) - 1);
            assert(IS_SQUARE(from));
            assert(_board[from] >= _FIRST_SLIDER);
            assert(IS_SLIDER(_board[from]->type));
            switch (_board[from]->type) {
            case (color|Bishop):
              assert(IS_DIAG(Direction(from, to)));
              if (_board[to] == _EMPTY) {
                if (IS_DIAG(dir)) {
                  pinDir = GetPinDir(color, from);
                  if (!pinDir || (abs(Direction(from, to)) == pinDir)) {
                    score = (_SQR[to] - _SQR[from]);
                    AddMove(BishopMove, from, to, score);
                  }
                }
              }
              else if ((Direction(to, from) == dir) &
                       (_board[to]->type == (color|Rook)))
              {
                GetSliderDiscovers(color, RookMove, _bishopRook[to + 8], to);
              }
              break;
            case (color|Rook):
              assert(IS_CROSS(Direction(from, to)));
              if (_board[to] == _EMPTY) {
                if (IS_CROSS(dir)) {
                  pinDir = GetPinDir(color, from);
                  if (!pinDir || (abs(Direction(from, to)) == pinDir)) {
                    score = (_SQR[to] - _SQR[from]);
                    AddMove(RookMove, from, to, score);
                  }
                }
              }
              else if ((Direction(to, from) == dir) &
                       (_board[to]->type == (color|Bishop)))
              {
                GetSliderDiscovers(color, BishopMove, _bishopRook[to], to);
              }
              break;
            case (color|Queen):
              assert(IS_DIR(Direction(from, to)));
              if (_board[to] == _EMPTY) {
                pinDir = GetPinDir(color, from);
                if (!pinDir || (abs(Direction(from, to)) == pinDir)) {
                  score = (_SQR[to] - _SQR[from]);
                  AddMove(QueenMove, from, to, score);
                }
              }
              else if (Direction(to, from) == dir) {
                switch (_board[to]->type) {
                case (color|Bishop):
                  if (IS_CROSS(dir)) {
                    GetSliderDiscovers(color, BishopMove, _bishopRook[to], to);
                  }
                  break;
                case (color|Rook):
                  if (IS_DIAG(dir)) {
                    GetSliderDiscovers(color, RookMove, _bishopRook[to + 8], to);
                  }
                  break;
                }
              }
              break;
            }
          }
        }
        if ((to == end) | (_board[to] != _EMPTY)) {
          break;
        }
      }
    }
  }

  //---------------------------------------------------------------------------
  template<Color color, bool qsearch>
  void GetKingMoves(const int depth) {
    assert(!checks);
    assert(_KING[color]->type == (color|King));

    const int from = _KING[color]->sqr;
    assert(IS_SQUARE(from));
    assert(_board[from] == _KING[color]);

    int kdir = 0;
    if (qsearch & (!depth)) {
      kdir = GetDiscoverDir(color, from);
    }

    int score;
    for (uint64_t mvs = _queenKing[from + 8]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(Distance(from, to) == 1);
      assert(IS_DIR(Direction(from, to)));
      if (qsearch) {
        const int cap = _board[to]->type;
        assert(cap || (_board[to] == _EMPTY));
        if (cap) {
          if ((COLOR(cap) != color) && !AttackedBy<!color>(to)) {
            score = (ValueOf(cap) + _SQR[to] - _SQR[from] - 80);
            AddMove(KingMove, from, to, score, cap);
          }
        }
        else if (kdir && (kdir != abs(Direction(from, to)))) {
          if (!AttackedBy<!color>(to)) {
            score = (_SQR[to] - _SQR[from] - 10);
            AddMove(KingMove, from, to, score);
          }
        }
      }
      else if (!AttackedBy<!color>(to)) {
        const int cap = _board[to]->type;
        assert(cap || (_board[to] == _EMPTY));
        if (!cap) {
          score = (_SQR[to] - _SQR[from] - 20);
          AddMove(KingMove, from, to, score);
          if ((to == (color ? F8 : F1)) &&
              (state & (color ? BlackShort : WhiteShort)) &&
              (_board[color ? G8 : G1] == _EMPTY) &&
              !AttackedBy<!color>(color ? G8 : G1))
          {
            assert(from == (color ? E8 : E1));
            assert(_board[color ? H8 : H1]->type == (color|Rook));
            AddMove(CastleShort, from, (color ? G8 : G1), 50);
          }
          else if ((to == (color ? D8 : D1)) &&
                   (state & (color ? BlackLong : WhiteLong)) &&
                   (_board[color ? C8 : C1] == _EMPTY) &&
                   (_board[color ? B8 : B1] == _EMPTY) &&
                   !AttackedBy<!color>(color ? C8 : C1))
          {
            assert(from == (color ? E8 : E1));
            assert(_board[color ? A8 : A1]->type == (color|Rook));
            AddMove(CastleLong, from, (color ? C8 : C1), 50);
          }
        }
        else if (COLOR(cap) != color) {
          score = (ValueOf(cap) + _SQR[to] - _SQR[from] - 80);
          AddMove(KingMove, from, to, score, cap);
        }
      }
    }
  }

  //---------------------------------------------------------------------------
  template<Color color>
  void GetKingEscapes() {
    assert(_KING[color]->type == (color|King));

    const int from = _KING[color]->sqr;
    assert(IS_SQUARE(from));
    assert(_board[from] == _KING[color]);

    const int sqr1 = ((checks & 0xFF) - 1);
    const int sqr2 = (((checks >> 8) & 0xFF) - 1);
    assert(sqr1 != sqr2);
    assert(IS_SQUARE(sqr1) & IS_SQUARE(sqr2));
    assert(IS_CAP(_board[sqr1]->type) && (COLOR(_board[sqr1]->type) != color));
    assert(IS_CAP(_board[sqr2]->type) && (COLOR(_board[sqr2]->type) != color));

    const int dir1 = ((_board[sqr1]->type >= Bishop)
                      ? abs(Direction(from, sqr1)) : 0);
    const int dir2 = ((_board[sqr2]->type >= Bishop)
                      ? abs(Direction(from, sqr2)) : 0);
    assert((!dir1 || IS_DIR(dir1)) && (!dir2 || IS_DIR(dir2)));

    for (uint64_t mvs = _queenKing[from + 8]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int to = ((mvs & 0xFF) - 1);
      assert(Distance(from, to) == 1);
      const int dir = abs(Direction(from, to));
      assert(IS_DIR(Direction(from, to)));
      if (((dir == dir1) & (to != sqr1)) | ((dir == dir2) & (to != sqr2))) {
        continue;
      }
      if (!AttackedBy<!color>(to)) {
        const int cap = _board[to]->type;
        assert(cap || (_board[to] == _EMPTY));
        if (!cap) {
          const int score = (_SQR[to] - _SQR[from]);
          AddMove(KingMove, from, to, score);
        }
        else if (COLOR(cap) != color) {
          const int score = (ValueOf(cap) + _SQR[to] - _SQR[from]);
          AddMove(KingMove, from, to, score, cap);
        }
      }
    }
  }

  //---------------------------------------------------------------------------
  template<Color color>
  void GetCheckEvasions() {
    assert(checks);
    assert(!(checks & ~0xFF));

    int from;
    int to = (checks - 1);
    assert(IS_SQUARE(to));

    int cap = _board[to]->type;
    assert(IS_CAP(cap) & (COLOR(cap) != color));
    const bool slider = (cap >= Bishop);

    int score;
    uint64_t mvs;
    if (_pcount[color|Pawn]) {
      assert(_pcount[color|Pawn] > 0);
      if ((cap == ((!color)|Pawn)) & (ep == (to + (color ? South : North)))) {
        if (XC(ep) > 0) {
          from = (ep + (color ? NorthWest : SouthWest));
          if (_board[from]->type == (color|Pawn)) {
            const int pinDir = GetPinDir(color, from);
            if ((!pinDir || (abs(Direction(from, to)) == pinDir)) &&
                !EpPinned<color>(from, to))
            {
              AddMove(PawnCap, from, ep, PawnValue);
            }
          }
        }
        if (XC(ep) < 7) {
          from = (ep + (color ? NorthEast : SouthEast));
          if (_board[from]->type == (color|Pawn)) {
            const int pinDir = GetPinDir(color, from);
            if ((!pinDir || (abs(Direction(from, to)) == pinDir)) &&
                !EpPinned<color>(from, to))
            {
              AddMove(PawnCap, from, ep, PawnValue);
            }
          }
        }
      }
      for (mvs = _pawnCaps[to + (8 * !color)]; mvs; mvs >>= 8) {
        assert(mvs & 0xFF);
        from = ((mvs & 0xFF) - 1);
        assert(IS_SQUARE(from));
        assert(Distance(from, to) == 1);
        assert(IS_DIAG(Direction(to, from)));
        if (_board[from]->type == (color|Pawn)) {
          const int pinDir = GetPinDir(color, from);
          if (!pinDir || (abs(Direction(from, to)) == pinDir)) {
            score = ValueOf(cap);
            if (YC(to) == (color ? 0 : 7)) {
              assert(cap >= Knight);
              AddMove(PawnCap, from, to, (score + QueenValue),  cap, (color|Queen));
              AddMove(PawnCap, from, to, (score + RookValue),   cap, (color|Rook));
              AddMove(PawnCap, from, to, (score + BishopValue), cap, (color|Bishop));
              AddMove(PawnCap, from, to, (score + KnightValue), cap, (color|Knight));
            }
            else {
              AddMove(PawnCap, from, to, score, cap);
            }
          }
        }
      }
    }

    const int king = _KING[color]->sqr;
    assert(IS_SQUARE(king));

    const int dir = Direction(to, king);
    assert(!slider || IS_DIR(dir));

    do {
      assert(IS_SQUARE(to));
      assert(cap == _board[to]->type);

      if (!cap && _pcount[color|Pawn]) {
        from = (to + (color ? North : South));
        if (IS_SQUARE(from)) {
          if (_board[from]->type == (color|Pawn)) {
            const int pinDir = GetPinDir(color, from);
            if (!pinDir || (abs(Direction(from, to)) == pinDir)) {
              if (YC(to) == (color ? 0 : 7)) {
                AddMove(PawnMove, from, to, QueenValue,  0, (color|Queen));
                AddMove(PawnMove, from, to, RookValue,   0, (color|Rook));
                AddMove(PawnMove, from, to, BishopValue, 0, (color|Bishop));
                AddMove(PawnMove, from, to, KnightValue, 0, (color|Knight));
              }
              else {
                score = (_PAWN_SQR[to + (8 * color)] -
                         _PAWN_SQR[from + (8 * color)]);
                AddMove(PawnMove, from, to, score);
              }
            }
          }
          else if (!_board[from]->type && (YC(to) == (color ? 4 : 3))) {
            from += (color ? North : South);
            assert(IS_SQUARE(from));
            if (_board[from]->type == (color|Pawn)) {
              const int pinDir = GetPinDir(color, from);
              if (!pinDir || (abs(Direction(from, to)) == pinDir)) {
                score = (_PAWN_SQR[to + (8 * color)] -
                         _PAWN_SQR[from + (8 * color)]);
                AddMove(PawnLung, from, to, score);
              }
            }
          }
        }
      }

      if (_pcount[color|Knight]) {
        assert(_pcount[color|Knight] > 0);
        for (mvs = _knightMoves[to]; mvs; mvs >>= 8) {
          assert(mvs & 0xFF);
          from = ((mvs & 0xFF) - 1);
          assert(IS_SQUARE(from));
          assert(from != to);
          if ((_board[from]->type == (color|Knight)) &&
              !GetPinDir(color, from))
          {
            score = (ValueOf(cap) + _SQR[to] - _SQR[from] - (8 * KnightMove));
            AddMove(KnightMove, from, to, score, cap);
          }
        }
      }

      if (_pcount[color]) {
        assert(_pcount[color] > 0);
        for (mvs = _atk[to]; mvs; mvs >>= 8) {
          if (mvs & 0xFF) {
            from = ((mvs & 0xFF) - 1);
            assert(IS_SQUARE(from));
            assert(IS_DIR(Direction(from, to)));
            assert(_board[from] >= _FIRST_SLIDER);
            const int type = _board[from]->type;
            assert(IS_SLIDER(type));
            if (COLOR(type) == color) {
              const int pinDir = GetPinDir(color, from);
              if (!pinDir || (abs(Direction(from, to)) == pinDir)) {
                score = (ValueOf(cap) - Distance(from, to) - (8 * (type & ~1)));
                AddMove(static_cast<MoveType>(type & ~1), from, to, score, cap);
              }
            }
          }
        }
      }

      if (cap && (cap < Bishop)) {
        break;
      }
      to += dir;
      cap = 0;
    } while (to != king);
    assert((to == king) || (cap && (cap < Bishop)));

    for (mvs = _queenKing[king + 8]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      to = ((mvs & 0xFF) - 1);
      assert(IS_SQUARE(to));
      assert(Distance(king, to) == 1);
      assert(IS_DIR(Direction(king, to)));
      if ((slider & (to != (checks - 1))) &&
          (abs(Direction(king, to)) == abs(dir)))
      {
        continue;
      }
      if (AttackedBy<!color>(to)) {
        continue;
      }

      cap = _board[to]->type;
      assert(cap || (_board[to] == _EMPTY));
      if (!cap) {
        score = (_SQR[to] - _SQR[king] - 20);
        AddMove(KingMove, king, to, score);
      }
      else if (COLOR(cap) != color) {
        score = (ValueOf(cap) + _SQR[to] - _SQR[king] - 80);
        AddMove(KingMove, king, to, score, cap);
      }
    }
  }

  //---------------------------------------------------------------------------
  template<Color color, bool qsearch>
  void GenerateMoves(const int depth) {
    assert(!_piece[0].type);
    assert(_piece[WhiteKingOffset].type == (White|King));
    assert(_piece[BlackKingOffset].type == (Black|King));
    assert(_KING[White]->type == (White|King));
    assert(_KING[Black]->type == (Black|King));
    assert(IS_SQUARE(_KING[White]->sqr));
    assert(IS_SQUARE(_KING[White]->sqr));
    assert(_board[_KING[White]->sqr] == _KING[White]);
    assert(_board[_KING[Black]->sqr] == _KING[Black]);
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

    assert(!(checks & ~0xFFFF));
    if (checks & 0xFF00) { // double check
      assert(checks & 0xFF);
      GetKingEscapes<color>();
      assert(!DuplicateMoveCount());
      return;
    }

    if (checks & 0xFF) { // single check
      GetCheckEvasions<color>();
      assert(!DuplicateMoveCount());
      return;
    }

    int from;
    for (int i = _pcount[color|Pawn]; i--; ) {
      assert(i >= 0);
      from = _piece[(color ? BlackPawnOffset : PawnOffset) + i].sqr;
      GetPawnMoves<color, qsearch>(from, depth);
    }

    for (int i = _pcount[color|Knight]; i--; ) {
      assert(i >= 0);
      from = _piece[(color ? BlackKnightOffset : KnightOffset) + i].sqr;
      GetKnightMoves<qsearch>(color, from, depth);
    }

    for (int i = _pcount[color|Bishop]; i--; ) {
      assert(i >= 0);
      from = _piece[(color ? BlackBishopOffset : BishopOffset) + i].sqr;
      if (qsearch) {
        GetSliderCaptures(color, BishopMove, _atk[from + 8], from);
      }
      else {
        GetSliderMoves(color, BishopMove, _bishopRook[from], from);
      }
    }

    for (int i = _pcount[color|Rook]; i--; ) {
      assert(i >= 0);
      from = _piece[(color ? BlackRookOffset : RookOffset) + i].sqr;
      if (qsearch) {
        GetSliderCaptures(color, RookMove, _atk[from + 8], from);
      }
      else {
        GetSliderMoves(color, RookMove, _bishopRook[from + 8], from);
      }
    }

    for (int i = _pcount[color|Queen]; i--; ) {
      assert(i >= 0);
      from = _piece[(color ? BlackQueenOffset : QueenOffset) + i].sqr;
      if (qsearch) {
        GetSliderCaptures(color, QueenMove, _atk[from + 8], from);
      }
      else {
        GetSliderMoves(color, QueenMove, _queenKing[from], from);
      }
    }

    if (qsearch && _pcount[color] && !depth) {
      GetSliderChecks<color>();
    }

    GetKingMoves<color, qsearch>(depth);
    assert(!DuplicateMoveCount());
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
        VASSERT(_board[from] == _KING[color]);
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
      VASSERT(moved == (color ? _KING[Black] : _KING[White]));
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
      VASSERT(moved == (color ? _KING[Black] : _KING[White]));
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
    assert(!checks == !AttackedBy<!color>(_KING[color]->sqr));

    _stats.execs++;

    assert((_seenIndex >= 0) & (_seenIndex < MaxPlies));
    _seen[_seenIndex++] = positionKey;
    _seenIndex -= (MaxPlies * (_seenIndex == MaxPlies));
    _seenFilter[positionKey & SeenFilterMask]++;
    assert(_seenFilter[positionKey & SeenFilterMask]);
#ifndef NDEBUG
    assert(_seenStackTop >= _seenStack);
    assert((_seenStackTop - _seenStack) < 8000);
    *_seenStackTop++ = positionKey;
#endif

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
      UpdateKingDirs<White>(from, to);
      UpdateKingDirs<Black>(from, to);
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
      UpdateKingDirs<White>(from, to);
      UpdateKingDirs<Black>(from, to);
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
          if (_atk[sqr]) {
            ExtendAttacks(sqr);
          }
          dest.pawnKey = (pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to] ^
                          _HASH[(!color)|Pawn][sqr]);
          dest.pieceKey = pieceKey;
          UpdateKingDirs<White>(sqr, to);
          UpdateKingDirs<Black>(sqr, to);
        }
      }
      dest.state = ((state ^ 1) & _TOUCH[from] & _TOUCH[to]);
      dest.ep = None;
      dest.rcount = 0;
      dest.mcount = (mcount + 1);
      UpdateKingDirs<White>(from, to);
      UpdateKingDirs<Black>(from, to);
      break;
    case KnightMove:
    case BishopMove:
    case RookMove:
    case QueenMove:
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
      UpdateKingDirs<White>(from, to);
      UpdateKingDirs<Black>(from, to);
      break;
    case KingMove:
      ClearKingDirs<color>(from);
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
      SetKingDirs<color>(to);
      UpdateKingDirs<!color>(from, to);
      break;
    case CastleShort:
      ClearKingDirs<color>(from);
      ClearAttacksFrom((color|Rook), (color ? H8 : H1));
      _board[color ? E8 : E1] = _EMPTY;
      _board[color ? G8 : G1] = moved;
      _board[color ? F8 : F1] = _board[color ? H8 : H1];
      _board[color ? H8 : H1] = _EMPTY;
      moved->sqr = to;
      _board[color ? F8 : F1]->sqr = (color ? F8 : F1);
      SetKingDirs<color>(to);
      if (_kingDir[color ? (E8 + 8) : E1] == West) {
        assert(!_kingDir[color ? (F8 + 8) : F1]);
        _kingDir[color ? (F8 + 8) : F1] = West;
      }
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
      ClearKingDirs<color>(from);
      ClearAttacksFrom((color|Rook), (color ? A8 : A1));
      _board[color ? D8 : D1] = _board[color ? A8 : A1];
      _board[color ? A8 : A1] = _EMPTY;
      _board[color ? C8 : C1] = moved;
      _board[color ? E8 : E1] = _EMPTY;
      moved->sqr = to;
      _board[color ? D8 : D1]->sqr = (color ? D8 : D1);
      SetKingDirs<color>(to);
      if (_kingDir[color ? (E8 + 8) : E1] == East) {
        assert(!_kingDir[color ? (D8 + 8) : D1]);
        _kingDir[color ? (D8 + 8) : D1] = East;
      }
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
                        _HASH[1][dest.ep]);

    dest.lastMove = move;
    dest.standPat = (_material[!color] - _material[color]);
    assert(standPat > (ply - Infinity));

    if (!cap && _atk[to]) {
      TruncateAttacks(to, from);
    }
    if (_atk[from]) {
      ExtendAttacks(from);
    }
    if (_board[to] >= _FIRST_SLIDER) {
      assert(IS_SLIDER(_board[to]->type));
      assert(_board[to]->sqr == to);
      AddAttacksFrom(_board[to]->type, to);
    }

#if 0
    assert(VerifyAttacks(true));
#endif

    dest.FindCheckers<!color>();
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

    _seenIndex += ((MaxPlies * !_seenIndex) - 1);
    assert((_seenIndex >= 0) & (_seenIndex < MaxPlies));
    assert(_seenFilter[positionKey & SeenFilterMask]);
    _seenFilter[positionKey & SeenFilterMask]--;

#ifndef NDEBUG
    assert(_seenStackTop > _seenStack);
    _seenStackTop--;
#endif

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
        if (_atk[from]) {
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
        if (_atk[from]) {
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
        if (_atk[sqr]) {
          TruncateAttacks(sqr, -1);
        }
        AddPiece(((!color)|Pawn), sqr);
        _board[to] = _EMPTY;
        _board[from] = moved;
        moved->sqr = from;
        _material[!color] += PawnValue;
        if (_atk[from]) {
          TruncateAttacks(from, to);
        }
        if (_atk[to]) {
          ExtendAttacks(to);
        }
        if (_atk[sqr]) {
          TruncateAttacks(sqr, from);
        }
        UpdateKingDirs<White>(to, sqr);
        UpdateKingDirs<Black>(to, sqr);
      }
      UpdateKingDirs<White>(to, from);
      UpdateKingDirs<Black>(to, from);
      break;
    case KingMove:
      ClearKingDirs<color>(to);
      _board[from] = moved;
      moved->sqr = from;
      if (cap) {
        AddPiece(cap, to);
        _material[!color] += ValueOf(cap);
      }
      else {
        _board[to] = _EMPTY;
      }
      if (_atk[from]) {
        TruncateAttacks(from, to);
      }
      if (cap >= Bishop) {
        AddAttacksFrom(cap, to);
      }
      else if (!cap && _atk[to]) {
        ExtendAttacks(to);
      }
      SetKingDirs<color>(from);
      UpdateKingDirs<!color>(to, from);
      break;
    case CastleShort:
      assert(moved == (color ? _KING[Black] : _KING[White]));
      assert(from == (color ? E8 : E1));
      assert(to == (color ? G8 : G1));
      assert(!cap);
      assert(!promo);
      assert(_board[color ? E8 : E1] == _EMPTY);
      assert(_board[color ? H8 : H1] == _EMPTY);
      assert(_board[color ? F8 : F1] != _EMPTY);
      assert(_board[color ? G8 : G1] != _EMPTY);
      assert(_board[color ? F8 : F1]->type == (color|Rook));
      ClearKingDirs<color>(to);
      ClearAttacksFrom((color|Rook), (color ? F8 : F1));
      _board[color ? E8 : E1] = moved;
      _board[color ? G8 : G1] = _EMPTY;
      _board[color ? H8 : H1] = _board[color ? F8 : F1];
      _board[color ? F8 : F1] = _EMPTY;
      moved->sqr = from;
      _board[color ? H8 : H1]->sqr = (color ? H8 : H1);
      SetKingDirs<color>(from);
      if (_kingDir[color ? (E8 + 8) : E1] == West) {
        assert(_kingDir[color ? (F8 + 8) : F1] == West);
        _kingDir[color ? (F8 + 8) : F1] = 0;
      }
      if (_atk[from]) {
        TruncateAttacks(from, to);
      }
      AddAttacksFrom((color|Rook), (color ? H8 : H1));
      break;
    case CastleLong:
      assert(moved == (color ? _KING[Black] : _KING[White]));
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
      ClearKingDirs<color>(to);
      ClearAttacksFrom((color|Rook), (color ? D8 : D1));
      _board[color ? A8 : A1] = _board[color ? D8 : D1];
      _board[color ? D8 : D1] = _EMPTY;
      _board[color ? E8 : E1] = moved;
      _board[color ? C8 : C1] = _EMPTY;
      moved->sqr = from;
      _board[color ? A8 : A1]->sqr = (color ? A8 : A1);
      SetKingDirs<color>(from);
      if (_kingDir[color ? (E8 + 8) : E1] == East) {
        assert(_kingDir[color ? (D8 + 8) : D1] == East);
        _kingDir[color ? (D8 + 8) : D1] = 0;
      }
      if (_atk[from]) {
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
      if (_atk[from]) {
        TruncateAttacks(from, to);
      }
      if (cap >= Bishop) {
        AddAttacksFrom(cap, to);
      }
      else if (!cap && _atk[to]) {
        ExtendAttacks(to);
      }
      if (_board[from] >= _FIRST_SLIDER) {
        AddAttacksFrom(_board[from]->type, from);
      }
      UpdateKingDirs<White>(to, from);
      UpdateKingDirs<Black>(to, from);
    }

#if 0
    assert(VerifyAttacks(true));
#endif
  }

  //---------------------------------------------------------------------------
  template<Color color>
  inline void ExecNullMove(Node& dest) const {
    assert(COLOR(state) == color);
    assert(!checks);
    assert(&dest != this);

    _stats.nullMoves++;

    dest.state = (state ^ 1);
    dest.ep = None;
    dest.rcount = rcount;
    dest.mcount = mcount;
    dest.pawnKey = pawnKey;
    dest.pieceKey = pieceKey;
    dest.positionKey = (dest.pawnKey ^
                        dest.pieceKey ^
                        _HASH[0][dest.state & 0x1F] ^
                        _HASH[1][dest.ep]);

    dest.lastMove.Clear();
    dest.standPat = (_material[!color] - _material[color]);
    assert(standPat > (ply - Infinity));
    dest.checks = 0;

#if 0
    assert(VerifyAttacks(true));
#endif
  }

  //---------------------------------------------------------------------------
  template<Color color>
  uint64_t PerftSearch(const int depth) {
    GenerateMoves<color, false>(depth);
    _stats.snodes += moveCount;
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

    GenerateMoves<color, false>(depth);
    _stats.snodes += moveCount;

    std::sort(moves, (moves + moveCount), Move::LexicalCompare);

    uint64_t total = 0;
    if (depth > 1) {
      while (!_stop && (moveIndex < moveCount)) {
        const Move& move = moves[moveIndex++];
        Exec<color>(move, *child);
        const uint64_t count = child->PerftSearch<!color>(depth - 1);
        Undo<color>(move);
        senjo::Output() << move.ToString() << ' ' << count << ' '
                        << move.GetScore();
        total += count;
      }
    }
    else {
      while (!_stop && (moveIndex < moveCount)) {
        const Move& move = moves[moveIndex++];
        assert(move.IsValid());
        senjo::Output() << move.ToString() << " 0 " << move.GetScore();
        total++;
      }
    }
    return total;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  uint64_t QPerftSearch(const int depth) {
    if (depth <= 0) {
      GenerateMoves<color, true>(depth);
      _stats.qnodes += moveCount;
    }
    else {
      GenerateMoves<color, false>(depth);
      _stats.snodes += moveCount;
    }
    if ((!child) | (!moveCount) | (depth < 0)) {
      return (moveCount + 1);
    }

    uint64_t count = 1;
    while (!_stop && (moveIndex < moveCount)) {
      const Move& move = moves[moveIndex++];
      Exec<color>(move, *child);
      count += child->QPerftSearch<!color>(depth - 1);
      Undo<color>(move);
    }
    return count;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  uint64_t QPerftSearchRoot(const int depth) {
    assert(!ply);
    assert(!parent);
    assert(child);

    if (depth <= 0) {
      GenerateMoves<color, true>(depth);
      _stats.qnodes += moveCount;
    }
    else {
      GenerateMoves<color, false>(depth);
      _stats.snodes += moveCount;
    }
    std::sort(moves, (moves + moveCount), Move::LexicalCompare);

    uint64_t total = 0;
    while (!_stop && (moveIndex < moveCount)) {
      const Move& move = moves[moveIndex++];
      Exec<color>(move, *child);
      const uint64_t count = child->QPerftSearch<!color>(depth - 1);
      Undo<color>(move);
      senjo::Output() << move.ToString() << ' ' << count << ' '
                      << move.GetScore();
      total += count;
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
  template<Color color>
  int PawnEval(PawnEntry* entry) {
    assert(entry);
    assert((PawnOffset + 8) == BlackPawnOffset);
    int score = 0;
    int stack[8];
    int* top = stack;
    uint8_t* file = entry->fileInfo[color];

    for (int i = _pcount[color|Pawn]; i--; ) {
      assert(i >= 0);
      const int sqr = _piece[PawnOffset + i + (8 * color)].sqr;
      assert(IS_SQUARE(sqr));
      assert(_board[sqr] == (_piece + PawnOffset + i + (8 * color)));
      assert(_board[sqr]->type == (color|Pawn));
      assert(_board[sqr]->sqr == sqr);

      score += _PAWN_SQR[sqr + (8 * color)];

      // penalty if doubled
      const int x = XC(sqr);
      const int y = YC(sqr);
      assert((x >= 0) && (x < 8));
      assert((y >= 1) && (y < 7));
      const int a = file[x + 1];
      if (a) {
        file[x + 1] = (color ? std::min<int>(a, y) : std::max<int>(a, y));
        score -= 32;
      }
      else {
        file[x + 1] = y;
        *top++ = x;
      }
    }

    while (top-- > stack) {
      const int x = *top;
      assert((x >= 0) & (x < 8));
      int l = (file[x] & 7);
      const int y = (file[x + 1] & 7);
      int r = (file[x + 2] & 7);
      assert((l >= 0) & (l < 7));
      assert((y >= 1) & (y < 7));
      assert((r >= 0) & (r < 7));
      l = (l ? (color ? (l - y) : (y - l)) : -5);
      r = (r ? (color ? (r - y) : (y - r)) : -5);
      if ((l < 0) & (r < 0)) {
        score += ((x < 3) | (x > 4)) ? (l + r) : ((l + r) / 2);
        file[x + 1] |= PawnEntry::Backward;
      }
      else if ((l == 0) | (l == 1) | (r == 0) | (r == 1)) {
        file[x + 1] |= PawnEntry::Supported;
      }
    }

    assert(entry->score[color] == 0);
    entry->score[color] = score;

#ifndef NDEBUG
    scoreType[color|Pawn] = score;
    scoreType[color|Total] += score;
#endif

    return score;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  void FindPassers(PawnEntry* entry) {
    assert(entry);
    uint8_t* me = entry->fileInfo[color];
    const uint8_t* you = entry->fileInfo[!color];

    for (int x = 0; x < 8; ++x) {
      const int y = (me[x + 1] & 7);
      if (!y) {
        continue;
      }

      assert((y >= 1) & (y <= 6));
      assert(IS_SQUARE(SQR(x,y)));
      assert(_board[SQR(x,y)]->type == (color|Pawn));
      assert(_board[SQR(x,y)]->sqr == SQR(x,y));

      // opposing pawn counts
      int op[3] = {0};
      for (int n = 0; n < 3; ++n) {
        for (int i = (you[x + n] & 7); ((i > 0) & (i < 7)); color ? --i : ++i) {
          if (_board[SQR((x + n - 1),i)]->type == ((!color)|Pawn)) {
            op[n] += (color ? (i < y) : (i > y));
          }
        }
      }
      if (op[1]) {
        continue;
      }
      if (op[0] | op[2]) {
        if ((color ? (y > 3) : (y < 4)) | !(me[x + 1] & PawnEntry::Supported)) {
          continue;
        }
        for (int n = 0; n < 3; n += 2) {
          for (int i = (me[x + n] & 7); ((i > 0) & (i < 7)); color ? ++i : --i) {
            if (_board[SQR((x + n - 1),i)]->type == (color|Pawn)) {
              op[n] -= (color ? (i < y) : (i > y));
              break; // be pessimistic, only count the first supporting pawn
            }
          }
        }
        if ((op[0] + op[2]) > 0) {
          continue;
        }
        me[x + 1] |= PawnEntry::Potential;
      }
      me[x + 1] |= PawnEntry::Passed;
    }
  }

  //---------------------------------------------------------------------------
  template<Color color>
  int PasserEval(const uint8_t fileInfo[2][10]) {
    assert(fileInfo);
    const uint8_t* me = fileInfo[color];
    int score = 0;
    for (int x = 0; x < 8; ++x) {
      if (!(me[x + 1] & PawnEntry::Passed)) {
        continue;
      }

      const int y = (me[x + 1] & 7);
      assert((y >= 1) & (y <= 6));
      assert(IS_SQUARE(SQR(x,y)));
      assert(_board[SQR(x,y)]->type == (color|Pawn));
      assert(_board[SQR(x,y)]->sqr == SQR(x,y));

      int bonus = _PASSER_BONUS[color ? (7 - y) : y];

      // reduce bonus if only a potential passer
      if (me[x + 1] & PawnEntry::Potential) {
        bonus /= 2;
      }

      // bonus if it has support
      else if (me[x + 1] & PawnEntry::Supported) {
        bonus = ((bonus * 4) / 3);
      }

      const int from = SQR(x,y);
      assert(IS_SQUARE(from));
      assert(_board[from]->type == (color|Pawn));
      assert(_board[from]->sqr == SQR(x,y));

      // reduce bonus if blocked
      if (_board[from + (color ? South : North)]->type) {
        bonus = ((bonus * 3) / 4);
      }

      // can it outrun the enemy king?
      else if (!(_pcount[!color] + _pcount[(!color)|Knight])) {
        const int tmp = SQR(x,(color ? 0 : 7)); // promotion square
        if ((Distance(from, tmp) + (COLOR(state) != color)) <
            Distance(_KING[!color]->sqr, tmp))
        {
          bonus += 200;
        }
      }

      // increase bonus if path to promotion is completely unblocked
      else {
        bonus += 8;
        for (int sqr = (from + (2 * (color ? South : North))); IS_SQUARE(sqr);
             sqr += (color ? South : North))
        {
          if (_board[sqr]->type) {
            bonus -= 8;
            break;
          }
        }
      }

      score += bonus;
    }

#ifndef NDEBUG
    scoreType[color|Passer] = score;
    scoreType[color|Total] += score;
#endif

    return score;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  int KnightEval(const uint8_t fileInfo[2][10]) {
    assert(fileInfo);
    assert((KnightOffset + 10) == BlackKnightOffset);
    int score = 0;

    // redundant knights are worth slightly less
    if (_pcount[color|Knight] > 1) {
      score -= (16 * (_pcount[color|Knight] - 1));
    }

    // loner knight worth less than a pawn
//    else if ((_pcount[color|Knight] == 1) &
//             !(_pcount[color] + _pcount[color|Pawn]))
//    {
//      score += ((PawnValue / 2) - KnightValue);
//    }

    for (int i = _pcount[color|Knight]; i--; ) {
      assert(i >= 0);
      const int sqr = _piece[KnightOffset + i + (10 * color)].sqr;
      assert(IS_SQUARE(sqr));
      assert(_board[sqr] == (_piece + KnightOffset + i + (10 * color)));
      assert(_board[sqr]->type == (color|Knight));
      assert(_board[sqr]->sqr == sqr);

      const int x = XC(sqr);
      const int y = YC(sqr);
      if (_pcount[(!color)|Pawn]) {
        // bonus if can't be menaced by enemy pawns
        score += (4 * !(fileInfo[!color][x] | fileInfo[!color][x + 2]));

        // bonus if blocking a backward pawn
        score += (4 * (color ? (y < 5) : (y > 2)) *
                  (fileInfo[!color][x + 1] == (y + (color ? South : North))));
      }

      // mobility bonus/penalty
      int mob = 0;
      for (uint64_t mvs = _knightMoves[sqr]; mvs; mvs >>= 8) {
        if (mvs & 0xFF) {
          const int to = ((mvs & 0xFF) - 1);
          assert(IS_SQUARE(to));
          assert(to != sqr);
          mob += ((!_board[to]->type) | (COLOR(_board[to]->type) != color));
        }
      }
      assert((mob >= 0) & (mob <= 8));
      score += (mob - (16 * !mob) - (4 * (mob == 1)));

      // keep knights close to the kings
      score += (8 - (Distance(sqr, _KING[White]->sqr) +
                     Distance(sqr, _KING[Black]->sqr)));
    }

#ifndef NDEBUG
    scoreType[color|Knight] = score;
    scoreType[color|Total] += score;
#endif

    return score;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  int BishopEval(const uint8_t fileInfo[2][10]) {
    assert(fileInfo);
    assert((BishopOffset + 10) == BlackBishopOffset);
    int score = 0;

    // bonus for having bishop pair (increases as pawns come off the board)
    // NOTE: this doesn't verify they are on opposite color squares!
    if (_pcount[color|Bishop] > 1) {
      score += (48 - ((5 * (_pcount[White|Pawn] + _pcount[Black|Pawn])) / 3));
    }

    // loner bishop worth less than a pawn
//    else if ((_pcount[color|Bishop] == 1) &
//             ((_pcount[color]+_pcount[color|Pawn]+_pcount[color|Knight]) == 1))
//    {
//      score += ((PawnValue / 2) - BishopValue);
//    }

    for (int i = _pcount[color|Bishop]; i--; ) {
      assert(i >= 0);
      const int sqr = _piece[BishopOffset + i + (10 * color)].sqr;
      assert(IS_SQUARE(sqr));
      assert(_board[sqr] == (_piece + BishopOffset + i + (10 * color)));
      assert(_board[sqr]->type == (color|Bishop));
      assert(_board[sqr]->sqr == sqr);

      const int x = XC(sqr);
      const int y = YC(sqr);
      if (_pcount[(!color)|Pawn]) {
        // bonus if can't be menaced by enemy pawns
        score += (4 * !(fileInfo[!color][x] | fileInfo[!color][x + 2]));

        // bonus if blocking a backward pawn (adds to previous bonus)
        score += (4 * (color ? (y < 5) : (y > 2)) *
                  (fileInfo[!color][x + 1] == (y + (color ? South : North))));
      }

      // mobility bonus/penalty
      int mob = 0;
      for (uint64_t mvs = _atk[sqr + 8]; mvs; mvs >>= 8) {
        if (mvs & 0xFF) {
          int to = ((mvs & 0xFF) - 1);
          assert(IS_DIAG(Direction(sqr, to)));
          assert(Distance(sqr, to) > 0);
          mob += (Distance(sqr, to) - ((_board[to]->type != 0) &
                                       (COLOR(_board[to]->type) == color)));
        }
      }
      assert((mob >= 0) & (mob <= 13));
      score += ((mob / 2) - (16 * !mob) - (4 * (mob == 1)));

      // bonus for being inline with enemy king
      const int dir = Direction(sqr, _KING[!color]->sqr);
      score += (4 * IS_DIAG(dir));

      // stay close to fiendly king during endgame
      score += static_cast<int>(
            EndGame(color) * (8 - Distance(sqr, _KING[color]->sqr)));
    }

#ifndef NDEBUG
    scoreType[color|Bishop] = score;
    scoreType[color|Total] += score;
#endif

    return score;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  int RookEval(const uint8_t fileInfo[2][10]) {
    assert(fileInfo);
    assert((RookOffset + 10) == BlackRookOffset);
    int score = 0;

    for (int i = _pcount[color|Rook]; i--; ) {
      assert(i >= 0);
      const int sqr = _piece[RookOffset + i + (10 * color)].sqr;
      assert(IS_SQUARE(sqr));
      assert(_board[sqr] == (_piece + RookOffset + i + (10 * color)));
      assert(_board[sqr]->type == (color|Rook));
      assert(_board[sqr]->sqr == sqr);

      int mob = 0;
      for (uint64_t mvs = _atk[sqr + 8]; mvs; mvs >>= 8) {
        if (mvs & 0xFF) {
          int to = ((mvs & 0xFF) - 1);
          assert(IS_CROSS(Direction(sqr, to)));
          assert(Distance(sqr, to) > 0);
          mob += (Distance(sqr, to) - ((_board[to]->type != 0) &
                                       (COLOR(_board[to]->type) == color)));
        }
      }
      assert((mob >= 0) & (mob <= 14));

      const int x = XC(sqr);
      const int y = YC(sqr);
      const uint8_t pawn = (fileInfo[color][x + 1] & 7);
      assert(!pawn || ((pawn >= 1) & (pawn <= 6)));
      assert(!pawn || (_board[SQR(x,pawn)]->type == (color|Pawn)));
      assert(!pawn || (pawn != y));

      // bonus if on open file
      // extra if on 7th, inline with enemy passed/backward pawn
      if (!pawn) {
        score += (mob +
                  (4 * (y == (color ? 1 : 6))) +
                  (4 * !fileInfo[!color][x + 1]) +
                  (8 * !!(fileInfo[!color][x + 1] &
                          (PawnEntry::Backward|PawnEntry::Passed))));
      }

      // bonus for passed pawn support
      else if (fileInfo[color][x + 1] & PawnEntry::Passed) {
        score += (mob + 4 + (4 * (color ? (pawn < y) : (pawn > y))));
      }

      // penalty if stuck behind own pawn after castling
      else if ((mob < 6) & !(state & (color ? BlackCastle : WhiteCastle)) &
               (color ? (y > pawn) : (y < pawn)))
      {
        const int kx = XC(_KING[color]->sqr);
        if (kx >= 4) {
          score -= (20 * (x >= kx));
        }
        else {
          score -= (20 * (x <= kx));
        }

        // extra penalty if trapped
        score -= ((16 * !mob) + (4 * (mob == 1)));
      }

      // partial mobility bonus
      // slight bonus for backward pawn support
      else {
        score += ((mob / 2) +
                  (4 * !!(fileInfo[color][x + 1] & PawnEntry::Backward)));
      }

      // stay close to fiendly king during endgame
      score += static_cast<int>(
            EndGame(color) * (8 - Distance(sqr, _KING[color]->sqr)));
    }

#ifndef NDEBUG
    scoreType[color|Rook] = score;
    scoreType[color|Total] += score;
#endif

    return score;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  int QueenEval() {
    assert((QueenOffset + 10) == BlackQueenOffset);
    int score = 0;

    for (int i = _pcount[color|Queen]; i--; ) {
      assert(i >= 0);
      const int sqr = _piece[QueenOffset + i + (10 * color)].sqr;
      assert(IS_SQUARE(sqr));
      assert(_board[sqr] == (_piece + QueenOffset + i + (10 * color)));
      assert(_board[sqr]->type == (color|Queen));
      assert(_board[sqr]->sqr == sqr);

      // mobility bonus/penalty
      int mob = 0;
      for (uint64_t mvs = _atk[sqr + 8]; mvs; mvs >>= 8) {
        if (mvs & 0xFF) {
          int to = ((mvs & 0xFF) - 1);
          assert(IS_DIR(Direction(sqr, to)));
          assert(Distance(sqr, to) > 0);
          mob += (Distance(sqr, to) - ((_board[to]->type != 0) &
                                       (COLOR(_board[to]->type) == color)));
        }
      }
      assert((mob >= 0) & (mob <= 27));
      score += ((mob / 4) - (16 * !mob) - (4 * (mob == 1)));

      // bonus for being close to enemy king
      score += (16 - (2 * Distance(sqr, _KING[!color]->sqr)));
    }

#ifndef NDEBUG
    scoreType[color|Queen] = score;
    scoreType[color|Total] += score;
#endif

    return score;
  }

  //---------------------------------------------------------------------------
  template<Color color>
  inline void FileEval(int& score, const int y, const int me, const int you) {
    assert((me >= 0) & (me < 7));
    assert((you >= 0) & (you < 7));
    score -= ((12 * !me) + (12 * !you));
    if (me) {
      if (color ? (y < me) : (y > me)) {
        score -= 16;
      }
      else if (y != me) {
        score -= (4 * (color ? (y - me - 2) : (me - y - 2)));
      }
    }
    if (you) {
      score -= (4 * (5 - abs(you - y)));
    }
  }

  //---------------------------------------------------------------------------
  template<Color color>
  int KingEval(const uint8_t fileInfo[2][10]) {
    const int sqr = _KING[color]->sqr;
    assert(IS_SQUARE(sqr));
    assert(_board[sqr]->type == (color|King));
    assert(_board[sqr]->sqr == sqr);
    assert(fileInfo);

    int eg = _SQR[sqr];     // endgame score
    int mg = _SQR[sqr + 8]; // midgame score

    // penalty for open files on side of the board where the king is
    // bonus for pawn shield
    // penalty for pawn storm
    const uint8_t* me  = fileInfo[color];
    const uint8_t* you = fileInfo[!color];
    const int x = XC(sqr);
    const int y = YC(sqr);
    if (x < 2) {
      FileEval<color>(mg, y, (me[1] & 7), (you[1] & 7));
      FileEval<color>(mg, y, (me[2] & 7), (you[2] & 7));
      FileEval<color>(mg, y, (me[3] & 7), (you[3] & 7));
    }
    else if (x > 5) {
      FileEval<color>(mg, y, (me[6] & 7), (you[6] & 7));
      FileEval<color>(mg, y, (me[7] & 7), (you[7] & 7));
      FileEval<color>(mg, y, (me[8] & 7), (you[8] & 7));
    }
    else {
      FileEval<color>(mg, y, (me[3] & 7), (you[3] & 7));
      FileEval<color>(mg, y, (me[4] & 7), (you[4] & 7));
      FileEval<color>(mg, y, (me[5] & 7), (you[5] & 7));
      FileEval<color>(mg, y, (me[6] & 7), (you[6] & 7));
    }

    // TODO midgame scoring
    //      penalty for enemy attacks near the king
    //      penalty increased if friendly pieces far away
    //      penalty for unprotected squares around king

    // TODO endgame scoring
    //      keep close to pawns, especially passers (friend or foe)

    mg = static_cast<int>((EndGame(color) * eg) + (MidGame(color) * mg));

#ifndef NDEBUG
    scoreType[color|King] = mg;
    scoreType[color|Total] += mg;
#endif

    return mg;
  }

  //---------------------------------------------------------------------------
  void Evaluate() {
    assert(!_piece[0].type);
    assert(_piece[WhiteKingOffset].type == (White|King));
    assert(_piece[BlackKingOffset].type == (Black|King));
    assert(_KING[White]->type == (White|King));
    assert(_KING[Black]->type == (Black|King));
    assert(IS_SQUARE(_KING[White]->sqr));
    assert(IS_SQUARE(_KING[White]->sqr));
    assert(_board[_KING[White]->sqr] == _KING[White]);
    assert(_board[_KING[Black]->sqr] == _KING[Black]);
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
    assert(!pawnKey == !(_pcount[White|Pawn]|_pcount[Black|Pawn]));

    const bool whiteCanWin = (_pcount[White|Pawn] |
                             (_pcount[White|Knight] > 2) |
                             (_pcount[White|Bishop] > 1) |
                             ((_pcount[White|Knight] != 0) &
                              (_pcount[White|Bishop] != 0)) |
                              _pcount[White|Rook] |
                              _pcount[White|Queen]);

    const bool blackCanWin = (_pcount[Black|Pawn] |
                             (_pcount[Black|Knight] > 2) |
                             (_pcount[Black|Bishop] > 1) |
                             ((_pcount[Black|Knight] != 0) &
                              (_pcount[Black|Bishop] != 0)) |
                              _pcount[Black|Rook] |
                              _pcount[Black|Queen]);

#ifndef NDEBUG
    memset(scoreType, 0, sizeof(scoreType));
    scoreType[White|CanWin]   = (whiteCanWin * _CAN_WIN_BONUS);
    scoreType[Black|CanWin]   = (blackCanWin * _CAN_WIN_BONUS);
    scoreType[White|Material] = _material[White];
    scoreType[Black|Material] = _material[Black];
    scoreType[White|Total]    = (_material[White] + scoreType[White|CanWin]);
    scoreType[Black|Total]    = (_material[Black] + scoreType[Black|CanWin]);
#endif

    if (!(whiteCanWin | blackCanWin)) {
      state |= Draw;
      standPat = _drawScore[COLOR(state)];
      return;
    }

    atkCount[White] = 0;
    atkCount[Black] = 0;
    atkScore[White] = 0;
    atkScore[Black] = 0;

    int score = (_material[White] - _material[Black] +
                 (whiteCanWin * _CAN_WIN_BONUS) -
                 (blackCanWin * _CAN_WIN_BONUS));

    PawnEntry* entry = _pawnTT.Get(pawnKey);
    assert(entry);
    if (entry->positionKey == pawnKey) {
      _pawnTT.IncHits();
      score += (entry->score[White] - entry->score[Black]);
#ifndef NDEBUG
      int pscore = (score - entry->score[White] + entry->score[Black]);
      PawnEntry tmp;
      memset(&tmp, 0, sizeof(tmp));
      tmp.positionKey = pawnKey;
      if (_pcount[White|Pawn]) pscore += PawnEval<White>(&tmp);
      if (_pcount[Black|Pawn]) pscore -= PawnEval<Black>(&tmp);
      if (_pcount[White|Pawn]) FindPassers<White>(&tmp);
      if (_pcount[Black|Pawn]) FindPassers<Black>(&tmp);
      assert(pscore == score);
      assert(!memcmp(&tmp, entry, sizeof(PawnEntry)));
#endif
    }
    else {
      memset(entry, 0, sizeof(PawnEntry));
      entry->positionKey = pawnKey;
      if (_pcount[White|Pawn]) score += PawnEval<White>(entry);
      if (_pcount[Black|Pawn]) score -= PawnEval<Black>(entry);
      if (_pcount[White|Pawn]) FindPassers<White>(entry);
      if (_pcount[Black|Pawn]) FindPassers<Black>(entry);
    }

    if (_pcount[White|Pawn]) score += PasserEval<White>(entry->fileInfo);
    if (_pcount[Black|Pawn]) score -= PasserEval<Black>(entry->fileInfo);
    score += KnightEval<White>(entry->fileInfo);
    score -= KnightEval<Black>(entry->fileInfo);
    score += BishopEval<White>(entry->fileInfo);
    score -= BishopEval<Black>(entry->fileInfo);
    score += RookEval<White>(entry->fileInfo);
    score -= RookEval<Black>(entry->fileInfo);
    score += QueenEval<White>();
    score -= QueenEval<Black>();
    score += KingEval<White>(entry->fileInfo);
    score -= KingEval<Black>(entry->fileInfo);

    // reduce winning score if "winning" side can't win
    if (((score > 0) & !whiteCanWin) | ((score < 0) & !blackCanWin)) {
      assert(whiteCanWin != blackCanWin);
      score /= 8;
    }

    // reduce winning score if rcount is getting large
    // NOTE: this destabilizes transposition table values
    //       because rcount is not encoded into positionKey
    if ((rcount > 25) & (abs(score) > 8)) {
      score = static_cast<int>(score * (25.0 / rcount));
    }

    standPat = (COLOR(state) ? -score : score);
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
    depthChange = 0;

    if (IsDraw()) {
      return (standPat = _drawScore[color]);
    }

    // mate distance pruning
    int best = (ply - Infinity);
    alpha = std::max<int>(best, alpha);
    beta = std::min<int>((Infinity - ply + 1), beta);
    if ((alpha >= beta) | !child) {
      return alpha;
    }

    // transposition table lookup
    int score;
    HashEntry* entry = _tt.Get(positionKey);
    if (entry->Key() == positionKey) {
      _tt.IncHits();
      switch (entry->GetPrimaryFlag()) {
      case HashEntry::Checkmate: return (ply - Infinity);
      case HashEntry::Stalemate: return (standPat = _drawScore[color]);
      case HashEntry::UpperBound:
        if ((score = entry->Score(ply)) <= alpha) {
          return score;
        }
        break;
      case HashEntry::ExactScore:
        return entry->Score(ply);
      case HashEntry::LowerBound:
        if ((score = entry->Score(ply)) >= beta) {
          return score;
        }
        break;
      default:
        assert(false);
      }
    }

    assert(best <= alpha);
    assert(alpha < beta);
    assert(!pvCount);

    // if not in check allow standPat
    if (!checks) {
      Evaluate();
      if (standPat >= beta) {
        return standPat;
      }
      alpha = std::max<int>(alpha, standPat);
      best = standPat;
      assert(best <= alpha);
      assert(alpha < beta);
    }

    // generate moves
    GenerateMoves<color, true>(depth);
    if (moveCount <= 0) {
      if (checks) {
        entry->SetCheckmate(positionKey);
        _tt.IncCheckmates();
        return (ply - Infinity);
      }
      // don't call _tt.StoreStalemate()!!!
      return standPat;
    }

    // search 'em
    Move* move;
    while ((move = GetNextMove())) {
      assert(move->IsValid());
      assert(best <= alpha);
      assert(alpha < beta);
      _stats.qexecs++;
      Exec<color>(*move, *child);
      score = -child->QSearch<!color>(-beta, -alpha, (depth - 1));
      Undo<color>(*move);
      if (_stop) {
        return beta;
      }
      if (score > best) {
        UpdatePV(*move);
        if (score >= beta) {
          return score;
        }
        alpha = std::max<int>(alpha, score);
        best = score;
      }
      assert(alpha < beta);
    }

    assert(best <= alpha);
    assert(alpha < beta);
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
    assert((type == PV) | ((alpha + 1) == beta));
    assert(cutNode ? lastMove.IsValid() : true);

    _stats.snodes++;
    moveIndex   = 0;
    moveCount   = 0;
    pvCount     = 0;
    depthChange = 0;

    if (IsDraw()) {
      return (standPat = _drawScore[color]);
    }

    // mate distance pruning
    int best = (ply - Infinity);
    alpha = std::max<int>(best, alpha);
    beta = std::min<int>((Infinity - ply + 1), beta);
    if ((alpha >= beta) | !child) {
      return alpha;
    }

    // check extensions
    if (checks && (parent->depthChange <= 0)) {
      _stats.chkExts++;
      depthChange++;
      depth++;
    }

    const bool pvNode = (type == PV);
    int eval = Infinity;
    Move firstMove;

    // transposition table lookup
    HashEntry* entry = _tt.Get(positionKey);
    if (entry->Key() == positionKey) {
      _tt.IncHits();
      switch (entry->GetPrimaryFlag()) {
      case HashEntry::Checkmate: return (ply - Infinity);
      case HashEntry::Stalemate: return (standPat = _drawScore[color]);
      case HashEntry::UpperBound:
        firstMove.Init(entry->MoveBits(), entry->Score(ply));
        assert(ValidateMove<color>(firstMove) == 0);
        if (((!pvNode) | entry->HasPvFlag()) &&
            ((entry->Depth() >= depth) & (firstMove.GetScore() <= alpha)))
        {
          pv[0] = firstMove;
          pvCount = 1;
          return firstMove.GetScore();
        }
        if (entry->Depth() >= (depth - 3)) {
          eval = std::min<int>(eval, firstMove.GetScore());
        }
        break;
      case HashEntry::ExactScore:
        firstMove.Init(entry->MoveBits(), entry->Score(ply));
        assert(ValidateMove<color>(firstMove) == 0);
        assert(entry->HasPvFlag());
        if ((entry->Depth() >= depth) &
            ((!pvNode) |
             (firstMove.GetScore() <= alpha) |
             (firstMove.GetScore() >= beta)))
        {
          pv[0] = firstMove;
          pvCount = 1;
          if (firstMove.GetScore() >= beta) {
            AddKiller(firstMove, depth);
          }
          else if ((!checks) & (firstMove.GetScore() > alpha) &
                   (!firstMove.IsCapOrPromo()))
          {
            IncHistory(firstMove, depth);
          }
          return firstMove.GetScore();
        }
        if (entry->Depth() >= (depth - 3)) {
          eval = std::min<int>(eval, firstMove.GetScore());
        }
        break;
      case HashEntry::LowerBound:
        firstMove.Init(entry->MoveBits(), entry->Score(ply));
        assert(ValidateMove<color>(firstMove) == 0);
        if (((!pvNode) | entry->HasPvFlag()) &&
            ((entry->Depth() >= depth) & (firstMove.GetScore() >= beta)))
        {
          pv[0] = firstMove;
          pvCount = 1;
          AddKiller(firstMove, depth);
          return firstMove.GetScore();
        }
        break;
      default:
        assert(false);
      }
      if (((depthChange <= 0) & (parent->depthChange <= 0)) &&
          entry->HasExtendedFlag())
      {
        _stats.hashExts++;
        depthChange++;
        depth++;
      }
    }

    assert(depthChange >= 0);
    assert(!moveIndex);
    assert(!moveCount);
    assert(!pvCount);

    // forward pruning (the risky stuff)
    if ((cutNode & (!pvNode) & (!depthChange) & (!checks)) &&
        !IsKiller(lastMove) &&
        ((_pcount[color] + _pcount[color|Knight]) > 1) &&
        ((_pcount[color] + _pcount[color|Knight] + _pcount[color|Pawn] > 2)))
    {
      assert((alpha + 1) == beta);
      Evaluate();

      // static null move pruning
      if (depth == 1) {
        eval = std::min<int>(eval, standPat);
        if ((eval >= (beta + (3 * PawnValue))) & (abs(beta) < WinningScore)) {
          _stats.staticNM++;
          depthChange = -1;
          return beta;
        }
      }

      // null move pruning
      else if (eval >= beta) {
        ExecNullMove<color>(*child);
        const int rdepth = (depth - 3 - (depth / 6) - ((eval - 500) >= beta));
        eval = (rdepth > 0)
            ? -child->Search<NonPV, !color>(-beta, -alpha, rdepth, false)
            : -child->QSearch<!color>(-beta, -alpha, 0);
        if (_stop) {
          return beta;
        }
        if (eval >= beta) {
          _stats.nmCutoffs++;
          depthChange = -depth;
          return std::max<int>(standPat, beta); // do not return eval
        }
      }
    }

    assert(!moveIndex);

    // internal iterative deepening
    if ((!firstMove) & (depth > 3)) {
      assert(!moveCount);
      assert(!pvCount);
      _stats.iidCount++;
      eval = Search<NonPV, color>((beta - 1), beta, (depth - 2), false);
      if (_stop | !pvCount) {
        return eval;
      }
      assert(moveCount > 0);
      assert(pvCount > 0);
      assert(pv[0].IsValid());
      firstMove = pv[0];
      moveIndex = 1;
    }

    // make sure firstMove is populated
    else if (!firstMove) {
      assert(!moveCount);
      assert(!pvCount);
      GenerateMoves<color, false>(depth);
      if (moveCount <= 0) {
        if (checks) {
          entry->SetCheckmate(positionKey);
          _tt.IncCheckmates();
          return (ply - Infinity);
        }
        entry->SetStalemate(positionKey);
        _tt.IncStalemates();
        return (standPat = _drawScore[color]);
      }

      firstMove = *GetNextMove();
      assert(moveIndex == 1);
    }

    // single reply extensions
    if ((moveCount == 1) & (depthChange <= 0) & (parent->depthChange <= 0)) {
      _stats.oneReplyExts++;
      depthChange++;
      depth++;
    }

    // search first move with full alpha/beta window
    const int orig_alpha = alpha;
    Exec<color>(firstMove, *child);
    best = (depth > 1)
        ? -child->Search<type, !color>(-beta, -alpha, (depth - 1), !cutNode)
        : -child->QSearch<!color>(-beta, -alpha, 0);
    Undo<color>(firstMove);
    if (_stop) {
      return beta;
    }
    int pvDepth = (depth + std::min<int>(0, child->depthChange));
    UpdatePV(firstMove);
    if (best >= beta) {
      assert(pvDepth == depth);
      AddKiller(firstMove, pvDepth);
      eval = ((abs(best) > MateScore) ? best : beta);
      entry->Set(positionKey, firstMove, eval, ply, pvDepth,
                 HashEntry::LowerBound,
                 (((depthChange > 0) ? HashEntry::Extended : 0) |
                  (pvNode ? HashEntry::FromPV : 0)));
      return best;
    }
    if (best > alpha) {
      assert(pvDepth == depth);
      alpha = best;
    }
    else if (!(checks | firstMove.IsCapOrPromo())) {
      DecHistory(firstMove);
    }

    // generate moves if we haven't done so already
    if (moveCount <= 0) {
      GenerateMoves<color, false>(depth);
      assert(moveCount > 0);
      assert((moveCount == 1) ? (moves[0] == firstMove) : true);
    }

    assert(moveIndex <= 1);
    assert(moveIndex <= moveCount);

    // search remaining moves
    const bool lmrOK = ((!pvNode) & (depth > 2) & (!checks));
    Move* move;
    while ((move = GetNextMove())) {
      assert(move->IsValid());
      if (firstMove == (*move)) {
        continue;
      }

      _stats.lateMoves++;
      Exec<color>(*move, *child);

      // late move reductions
      int d = (depth - 1);
      if (lmrOK) {
        _stats.lmCandidates++;
        if (!(move->IsCapOrPromo() | IsKiller(*move) | child->checks) &&
            (_hist[move->TypeToIndex()] < depth))
        {
          _stats.lmReductions++;
          d -= (1 + (_hist[move->TypeToIndex()] < 0));
        }
      }

      // first search with a null window to quickly see if it improves alpha
      eval = (d > 0)
          ? -child->Search<NonPV, !color>(-(alpha + 1), -alpha, d, true)
          : -child->QSearch<!color>(-(alpha + 1), -alpha, 0);

      // re-search it?
      if ((!_stop) & (eval > alpha)) {
        assert(child->depthChange >= 0);
        _stats.lmAlphaIncs++;
        if (pvNode) {
          assert(d == (depth - 1));
          _stats.lmResearches++;
          eval = (d > 0)
              ? -child->Search<PV, !color>(-beta, -alpha, d, false)
              : -child->QSearch<!color>(-beta, -alpha, 0);
        }
        else if (d < (depth - 1)) {
          assert(child->depthChange >= 0);
          assert((alpha + 1) == beta);
          _stats.lmResearches++;
          d = (depth - 1);
          eval = -child->Search<NonPV, !color>(-beta, -alpha, d, false);
        }
        _stats.lmConfirmed += (eval > alpha);
      }

      assert((depth + child->depthChange) >= 0);
      Undo<color>(*move);
      if (_stop) {
        return beta;
      }
      if (eval > best) {
        best = eval;
        pvDepth = (depth + std::min<int>(0, child->depthChange));
        UpdatePV(*move);
        if (eval >= beta) {
          assert(pvDepth == depth);
          AddKiller(*move, pvDepth);
          eval = ((abs(best) > MateScore) ? best : beta);
          entry->Set(positionKey, *move, eval, ply, pvDepth,
                     HashEntry::LowerBound,
                     (((depthChange > 0) ? HashEntry::Extended : 0) |
                      (pvNode ? HashEntry::FromPV : 0)));
          return best;
        }
      }
      if (eval > alpha) {
        assert(pvDepth == depth);
        alpha = eval;
      }
      else if (!(checks | move->IsCapOrPromo())) {
        DecHistory(*move);
      }
    }

    assert(moveCount > 0);
    assert(best <= alpha);
    assert(alpha < beta);

    if (pvCount > 0) {
      assert(pvDepth >= 0);
      assert(pv[0].IsValid());
      if (!(checks | pv[0].IsCapOrPromo())) {
        IncHistory(pv[0], depth);
      }
      if (best > orig_alpha) {
        assert(pvNode);
        assert(pvDepth == depth);
        entry->Set(positionKey, pv[0], best, ply, pvDepth,
            HashEntry::ExactScore,
            (((depthChange > 0) ? HashEntry::Extended : 0) |
             HashEntry::FromPV));
      }
      else {
        assert(alpha == orig_alpha);
        entry->Set(positionKey, pv[0], alpha, ply, pvDepth,
            HashEntry::UpperBound,
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

    // TODO disable timer

    GenerateMoves<color, false>(depth);
    if (moveCount <= 0) {
      senjo::Output() << "No legal moves";
      return std::string();
    }

    // sort 'em
    std::stable_sort(moves, (moves + moveCount), Move::ScoreCompare);

    // move transposition table move (if any) to front of list
    HashEntry* entry = _tt.Get(positionKey);
    assert(entry);
    if (entry->Key() == positionKey) {
      _tt.IncHits();
      if (moveCount > 1) {
        Move ttMove;
        switch (entry->GetPrimaryFlag()) {
        case HashEntry::Checkmate:
          senjo::Output() << "CHECKMATE";
          return std::string();
        case HashEntry::Stalemate:
          senjo::Output() << "STALEMATE";
          return std::string();
        case HashEntry::UpperBound:
        case HashEntry::ExactScore:
        case HashEntry::LowerBound:
          ttMove.Init(entry->MoveBits(), entry->Score(ply));
          assert(ValidateMove<color>(ttMove) == 0);
          for (int i = 0; i < moveCount; ++i) {
            if (moves[i] == ttMove) {
              ScootMoveToFront(i);
              break;
            }
          }
          break;
        }
      }
    }

    // initial principal variation
    pvCount = 1;
    pv[0] = moves[0];

    // return immediately if we only have one move
    if (moveCount == 1) {
      OutputPV(0);
      return pv[0].ToString();
    }

    Move* move;
    bool  showPV = true;
    int   alpha = -Infinity;
    int   beta;
    int   delta;
    int   score;

    // iterative deepening
    for (int d = 0; !_stop && (d < depth); ++d) {
      // TODO enable timer if d > 0

      _seldepth = _depth = (d + 1);
      delta = (_depth < AspirationDepth) ? HugeDelta : 16;
      beta  = std::min<int>((alpha + delta), +Infinity);
      alpha = std::max<int>((alpha - delta), -Infinity);

      for (moveIndex = 0; !_stop && (moveIndex < moveCount); ++moveIndex) {
        move = (moves + moveIndex);
        assert(ValidateMove<color>(*move) == 0);
        assert(d == (_depth - 1));

        _currmove = move->ToString();
        _movenum  = (moveIndex + 1);

        Exec<color>(*move, *child);
        score = (d > 0)
            ? ((_movenum == 1)
               ? -child->Search<   PV, !color>(-beta, -alpha, d, false)
               : -child->Search<NonPV, !color>(-beta, -alpha, d, true))
            : -child->QSearch<!color>(-beta, -alpha, 0);
        if (_stop) {
          Undo<color>(*move);
          break;
        }
        assert(abs(score) < Infinity);
        move->SetScore(score);

        // re-search it to get real score?
        while ((score >= beta) | ((score <= alpha) & (_movenum == 1))) {
          if (score >= beta) {
            OutputPV(score, 1); // report lowerbound
            beta = std::min<int>(Infinity, (score + delta));
          }
          else if (_movenum == 1) {
            OutputPV(score, -1); // report upperbound
            alpha = std::max<int>(-Infinity, (score - delta));
          }
          else {
            beta = std::min<int>(Infinity, (score + delta));
          }
          // TODO: increase time
          score = (d > 0)
              ? -child->Search<PV, !color>(-beta, -alpha, d, false)
              : -child->QSearch<!color>(-beta, -alpha, 0);
          assert(abs(score) < Infinity);
          if (_stop) {
            break;
          }
          move->SetScore(score);
          delta *= 2;
        }
        Undo<color>(*move);

        assert(abs(score) < Infinity);
        assert(abs(move->GetScore()) < Infinity);

        // do we have a new principal variation?
        if ((_movenum == 1) | (move->GetScore() > alpha)) {
          alpha = move->GetScore();
          UpdatePV(*move);
          OutputPV(alpha);
          showPV = false;
          if (!_stop) {
            entry->Set(positionKey, *move, alpha, ply, _depth,
                       HashEntry::ExactScore,
                       HashEntry::FromPV);
          }
          ScootMoveToFront(moveIndex);
        }

        // set null aspiration window now that we have a principal variation
        delta = (_depth < AspirationDepth) ? HugeDelta : 16;
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
  : root(NULL)
{
}

//-----------------------------------------------------------------------------
Clunk::~Clunk() {
}

//-----------------------------------------------------------------------------
bool Clunk::IsInitialized() const {
  return (root != NULL);
}

//-----------------------------------------------------------------------------
bool Clunk::SetEngineOption(const std::string& /*optionName*/,
                            const std::string& /*optionValue*/)
{
  return false;
}

//-----------------------------------------------------------------------------
bool Clunk::WhiteToMove() const {
  if (!root) {
    senjo::Output() << NOT_INITIALIZED;
    return true;
  }
  return !COLOR(root->state);
}

//-----------------------------------------------------------------------------
const char* Clunk::MakeMove(const char* str) {
  if (!root) {
    senjo::Output() << NOT_INITIALIZED;
    return NULL;
  }
  if (!str ||
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
    root->GenerateMoves<Black, false>(1);
  }
  else {
    root->GenerateMoves<White, false>(1);
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

  _seenIndex = 0;
  memset(_kingDir, 0, sizeof(_kingDir));
  memset(_board, 0, sizeof(_board));
  memset(_piece, 0, sizeof(_piece));
  memset(_pcount, 0, sizeof(_pcount));
  memset(_material, 0, sizeof(_material));
  memset(_atk, 0, sizeof(_atk));
  memset(_seen, 0, sizeof(_seen));
  memset(_seenFilter, 0, sizeof(_seenFilter));

  _board[None] = _EMPTY;

  root->state = 0;
  root->ep = None;
  root->rcount = 0;
  root->mcount = 0;
  root->pawnKey = 0ULL;
  root->pieceKey = 0ULL;
  root->positionKey = 0ULL;
  root->lastMove.Clear();
  root->standPat = 0;
  root->checks = 0;

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

  SetKingDirs<White>(_KING[White]->sqr);
  SetKingDirs<Black>(_KING[Black]->sqr);

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

  p = NextSpace(p);
  p = NextWord(p);
  if (isdigit(*p)) {
    while (*p && isdigit(*p)) {
      root->rcount = ((root->rcount * 10) + (*p++ - '0'));
    }
    p = NextSpace(p);
    p = NextWord(p);
  }

  if (isdigit(*p)) {
    while (*p && isdigit(*p)) {
      root->mcount = ((root->mcount * 10) + (*p++ - '0'));
    }
    if (root->mcount) {
      root->mcount = (((root->mcount - 1) * 2) + COLOR(root->state));
    }
  }

  while (p && (*p == '-')) {
    p = NextSpace(p);
    p = NextWord(p);
  }

  root->positionKey = (root->pawnKey ^
                       root->pieceKey ^
                       _HASH[0][root->state & 0x1F] ^
                       _HASH[1][root->ep]);

  if (COLOR(root->state)) {
    root->FindCheckers<Black>();
  }
  else {
    root->FindCheckers<White>();
  }

  return p;
}

//-----------------------------------------------------------------------------
std::list<senjo::EngineOption> Clunk::GetOptions() const {
  std::list<senjo::EngineOption> opts;
  // TODO add options
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
  return ("1.2." + rev);
}

//-----------------------------------------------------------------------------
std::string Clunk::GetFEN() const {
  if (!root) {
    senjo::Output() << NOT_INITIALIZED;
    return std::string();
  }
  return root->GetFEN();
}

//-----------------------------------------------------------------------------
void Clunk::ClearSearchData() {
  _tt.Clear();
  _pawnTT.Clear();
  memset(_hist, 0, sizeof(_hist));
  for (int i = 0; i < MaxPlies; ++i) {
    _node[i].ClearKillers();
  }
}

//-----------------------------------------------------------------------------
void Clunk::ClearStopFlags() {
  ChessEngine::ClearStopFlags();
  clunk::_stop = 0;
}

//-----------------------------------------------------------------------------
void Clunk::Initialize() {
  InitNodes();
  InitDistDir();
  InitMoveMaps();

  _tt.Resize(512); // TODO make configurable
  _pawnTT.Resize(2); // TODO make configurable

  root = _node;
  SetPosition(_STARTPOS);
}

//-----------------------------------------------------------------------------
void Clunk::PonderHit() {
}

//-----------------------------------------------------------------------------
void Clunk::PrintBoard() const {
  if (root) {
    root->PrintBoard(true);
  }
  else {
    senjo::Output() << NOT_INITIALIZED;
  }
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
void Clunk::SetDebug(const bool flag) {
  ChessEngine::SetDebug(flag);
  clunk::_debug = flag;
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
    *nodes = (_stats.snodes + _stats.qnodes);
  }
  if (qnodes) {
    *qnodes = _stats.qnodes;
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
  if (!root) {
    senjo::Output() << NOT_INITIALIZED;
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
  senjo::Output() << "Nodes " << _stats.snodes << ' '
                  << senjo::Rate((_stats.snodes / 1000), msecs)
                  << " KNodes/sec";

  return count;
}

//-----------------------------------------------------------------------------
uint64_t Clunk::MyQPerft(const int depth) {
  if (!root) {
    senjo::Output() << NOT_INITIALIZED;
    return 0;
  }

  if (_debug) {
    PrintBoard();
    senjo::Output() << GetFEN();
  }

  InitSearch(COLOR(root->state), _startTime);

  const int d = std::min<int>(depth, MaxPlies);
  const uint64_t count = WhiteToMove() ? root->QPerftSearchRoot<White>(d)
                                       : root->QPerftSearchRoot<Black>(d);

  const uint64_t msecs = (senjo::Now() - _startTime);
  assert(count == (_stats.snodes + _stats.qnodes));
  senjo::Output() << "Qperft " << count << ' '
                  << senjo::Rate((count / 1000), msecs) << " KNodes/sec";
  senjo::Output() << "Snodes " << _stats.snodes << ", Qnodes " << _stats.qnodes
                  << " (" << senjo::Percent(_stats.qnodes, count) << "%)";

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
  if (!root) {
    senjo::Output() << NOT_INITIALIZED;
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

  _stats.ttGets   += _tt.Gets();
  _stats.ttHits   += _tt.Hits();
  _stats.ttMates  += _tt.Checkmates();
  _stats.ttStales += _tt.Stalemates();
  _stats.ptGets   += _pawnTT.Gets();
  _stats.ptHits   += _pawnTT.Hits();
  _totalStats += _stats;

  if (_debug) {
    senjo::Output() << "--- Stats";
    _stats.Print();
  }

  return bestmove;
}

} // namespace clunk
