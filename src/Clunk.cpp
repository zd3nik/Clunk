//----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//----------------------------------------------------------------------------

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
int                _offset[128] = {0};
Piece              _pc[PieceListSize] = {0,0};
int                _pcount[12] = {0};
int                _material[2] = {0};
uint64_t           _atkd[128] = {0};
uint64_t           _pawnCaps[128] = {0};
uint64_t           _knightMoves[128] = {0};
uint64_t           _bishopRook[128] = {0};
uint64_t           _queenKing[128] = {0};
Clunk              _node[MaxPlies];
std::set<uint64_t> _seen;

//--------------------------------------------------------------------------
int                _stop = 0;
int                _depth = 0;
int                _seldepth = 0;
int                _movenum = 0;
uint64_t           _execs = 0;
uint64_t           _nodes = 0;
uint64_t           _qnodes = 0;
Move               _currmove;
Stats              _stats;
TranspositionTable _tt;

//----------------------------------------------------------------------------
void InitSearch() {
  _stop     = 0;
  _depth    = 0;
  _seldepth = 0;
  _movenum  = 0;
  _execs    = 0;
  _nodes    = 0;
  _qnodes   = 0;

  _currmove.Clear();
  _stats.Clear();
  _tt.ResetCounters();
}

//--------------------------------------------------------------------------
void InitNodes(Clunk* root) {
  for (int i = 0; i < MaxPlies; ++i) {
    _node[i].parent = (i ? &_node[i - 1] : root);
    _node[i].child = (((i + 1) < MaxPlies) ? &_node[i + 1] : NULL);
    _node[i].ply = (i + 1);
  }
}

//--------------------------------------------------------------------------
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
  assert(!pc || ((pc >= Pawn) && (pc < King)));
  return _VALUE_OF[pc];
}

//-----------------------------------------------------------------------------
inline int AddPiece(const int type, const int sqr) {
  assert(IS_SQUARE(sqr));
  int offset;
  switch (type) {
  case (White|Pawn):
    offset = (PawnOffset + _pcount[White|Pawn]++);
    assert(offset < BlackPawnOffset);
    break;
  case (Black|Pawn):
    offset = (BlackPawnOffset + _pcount[Black|Pawn]++);
    assert(offset < KnightOffset);
    break;
  case (White|Knight):
    offset = (KnightOffset + _pcount[White|Knight]++);
    assert(offset < BlackKnightOffset);
    _pcount[White]++;
    break;
  case (Black|Knight):
    offset = (BlackKnightOffset + _pcount[Black|Knight]++);
    assert(offset < BishopOffset);
    _pcount[Black]++;
    break;
  case (White|Bishop):
    offset = (BishopOffset + _pcount[White|Bishop]++);
    assert(offset < BlackBishopOffset);
    _pcount[White]++;
    break;
  case (Black|Bishop):
    offset = (BlackBishopOffset + _pcount[Black|Bishop]++);
    assert(offset < RookOffset);
    _pcount[Black]++;
    break;
  case (White|Rook):
    offset = (RookOffset + _pcount[White|Rook]++);
    assert(offset < BlackRookOffset);
    _pcount[White]++;
    break;
  case (Black|Rook):
    offset = (BlackRookOffset + _pcount[Black|Rook]++);
    assert(offset < QueenOffset);
    _pcount[Black]++;
    break;
  case (White|Queen):
    offset = (QueenOffset + _pcount[White|Queen]++);
    assert(offset < BlackQueenOffset);
    _pcount[White]++;
    break;
  case (Black|Queen):
    offset = (BlackQueenOffset + _pcount[Black|Queen]++);
    assert(offset < PieceListSize);
    _pcount[Black]++;
    break;
  case (White|King):
    offset = WhiteKingOffset;
    break;
  case (Black|King):
    offset = BlackKingOffset;
    break;
  default:
    assert(false);
    offset = 0;
  }
  _pc[offset].type = type;
  _pc[offset].sqr = sqr;
  return offset;
}

//-----------------------------------------------------------------------------
inline void RemovePiece(const int type, const int offset) {
  switch (type) {
  case (White|Pawn):
    assert((_pcount[type] > 0) && (_pcount[type] <= 8));
    assert(offset >= PawnOffset);
    assert(offset < (PawnOffset + _pcount[type]));
    _pc[offset] = _pc[PawnOffset + --_pcount[White|Pawn]];
    break;
  case (Black|Pawn):
    assert((_pcount[type] > 0) && (_pcount[type] <= 8));
    assert(offset >= BlackPawnOffset);
    assert(offset < (BlackPawnOffset + _pcount[type]));
    _pc[offset] = _pc[BlackPawnOffset + --_pcount[Black|Pawn]];
    break;
  case (White|Knight):
    assert((_pcount[type] > 0) && (_pcount[type] <= 10));
    assert(offset >= KnightOffset);
    assert(offset < (KnightOffset + _pcount[type]));
    _pc[offset] = _pc[KnightOffset + --_pcount[White|Knight]];
    --_pcount[White];
    break;
  case (Black|Knight):
    assert((_pcount[type] > 0) && (_pcount[type] <= 10));
    assert(offset >= BlackPawnOffset);
    assert(offset < (BlackPawnOffset + _pcount[type]));
    _pc[offset] = _pc[BlackKnightOffset + --_pcount[Black|Knight]];
    --_pcount[Black];
    break;
  case (White|Bishop):
    assert((_pcount[type] > 0) && (_pcount[type] <= 10));
    assert(offset >= BishopOffset);
    assert(offset < (BishopOffset + _pcount[type]));
    _pc[offset] = _pc[BishopOffset + --_pcount[White|Bishop]];
    --_pcount[White];
    break;
  case (Black|Bishop):
    assert((_pcount[type] > 0) && (_pcount[type] <= 10));
    assert(offset >= BlackBishopOffset);
    assert(offset < (BlackBishopOffset + _pcount[type]));
    _pc[offset] = _pc[BlackBishopOffset + --_pcount[Black|Bishop]];
    --_pcount[Black];
    break;
  case (White|Rook):
    assert((_pcount[type] > 0) && (_pcount[type] <= 10));
    assert(offset >= RookOffset);
    assert(offset < (RookOffset + _pcount[type]));
    _pc[offset] = _pc[RookOffset + --_pcount[White|Rook]];
    --_pcount[White];
    break;
  case (Black|Rook):
    assert((_pcount[type] > 0) && (_pcount[type] <= 10));
    assert(offset >= BlackRookOffset);
    assert(offset < (BlackRookOffset + _pcount[type]));
    _pc[offset] = _pc[BlackRookOffset + --_pcount[Black|Rook]];
    --_pcount[Black];
    break;
  case (White|Queen):
    assert((_pcount[type] > 0) && (_pcount[type] <= 9));
    assert(offset >= QueenOffset);
    assert(offset < (QueenOffset + _pcount[type]));
    _pc[offset] = _pc[QueenOffset + --_pcount[White|Queen]];
    --_pcount[White];
    break;
  case (Black|Queen):
    assert((_pcount[type] > 0) && (_pcount[type] <= 9));
    assert(offset >= BlackQueenOffset);
    assert(offset < (BlackQueenOffset + _pcount[type]));
    _pc[offset] = _pc[BlackQueenOffset + --_pcount[Black|Queen]];
    --_pcount[Black];
    break;
  default:
    assert(false);
  }
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
  assert(_offset[from] >= SliderOffset);
  assert(IS_SLIDER(_pc[_offset[from]].type));
  assert(_pc[_offset[from]].sqr == from);
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
  assert(_offset[from] >= SliderOffset);
  assert(_pc[_offset[from]].type == pc);
  assert(_pc[_offset[from]].sqr == from);
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
      if ((to == end) || _offset[to]) {
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------------
void ClearAttacksFrom(const int pc, const int from) {
  assert(IS_SLIDER(pc));
  assert(IS_SQUARE(from));
  assert(_offset[from] >= SliderOffset);
  assert(_pc[_offset[from]].type == pc);
  assert(_pc[_offset[from]].sqr == from);
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
      if ((to == end) || _offset[to]) {
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
      assert(_offset[from] >= SliderOffset);
      assert(IS_SLIDER(_pc[_offset[from]].type));
      assert(_pc[_offset[from]].sqr == from);
      const int dir = Direction(from, to);
      for (int ato = (to + dir); IS_SQUARE(ato); ato += dir) {
        assert(Direction(from, ato) == dir);
        ClearAttack(ato, dir);
        if ((ato == stop) || _offset[ato]) {
          break;
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
void ExtendAttacks(const int to) {
  assert(IS_SQUARE(to));
  assert(!_offset[to]);
  for (uint64_t tmp = _atkd[to]; tmp; tmp >>= 8) {
    if (tmp & 0xFF) {
      const int from = static_cast<int>((tmp & 0xFF) - 1);
      assert(IS_SQUARE(from));
      assert(_offset[from] >= SliderOffset);
      assert(IS_SLIDER(_pc[_offset[from]].type));
      assert(_pc[_offset[from]].sqr == from);
      const int dir = Direction(from, to);
      assert(IS_DIR(dir));
      for (int ato = (to + dir); IS_SQUARE(ato); ato += dir) {
        assert(Direction(from, ato) == dir);
        AddAttack(from, ato, dir);
        if (_offset[ato]) {
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
      assert(_offset[(mvs & 0xFF) - 1] >= SliderOffset);
      if (COLOR(_pc[_offset[(mvs & 0xFF) - 1]].type) == color) {
        assert(IS_SLIDER(_pc[_offset[(mvs & 0xFF) - 1]].type));
        assert(_pc[_offset[(mvs & 0xFF) - 1]].sqr == int((mvs & 0xFF) - 1));
        assert(_pcount[_pc[_offset[(mvs & 0xFF) - 1]].type] > 0);
        assert(_pcount[color] >= _pcount[_pc[_offset[(mvs & 0xFF) - 1]].type]);
        return true;
      }
    }
  }
  if (_pcount[color|Knight]) {
    assert(_pcount[color] >= _pcount[color|Knight]);
    for (uint64_t mvs = _knightMoves[sqr]; mvs; mvs >>= 8) {
      assert(mvs & 0xFF);
      const int from = ((mvs & 0xFF) - 1);
      assert(IS_SQUARE(from));
      assert(from != sqr);
      if ((_offset[from] >= (color ? BlackKnightOffset : KnightOffset)) &&
          (_offset[from] < (color ? BishopOffset : BlackKnightOffset)))
      {
        assert(_pc[_offset[from]].type == (color|Knight));
        assert(_pc[_offset[from]].sqr == from);
        return true;
      }
    }
  }
  for (uint64_t mvs = _queenKing[sqr + 8]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int from = ((mvs & 0xFF) - 1);
    assert(IS_DIR(Direction(sqr, from)));
    if (_offset[from] == (color ? BlackKingOffset : WhiteKingOffset)) {
      return true;
    }
    if (color) {
      switch (Direction(sqr, from)) {
      case NorthWest: case NorthEast:
        if ((_offset[from] >= BlackPawnOffset) &&
            (_offset[from] < KnightOffset))
        {
          assert(_pc[_offset[from]].type == (Black|Pawn));
          assert(_pc[_offset[from]].sqr == from);
          return true;
        }
      }
    }
    else {
      switch (Direction(sqr, from)) {
      case SouthWest: case SouthEast:
        if ((_offset[from] >= PawnOffset) &&
            (_offset[from] < BlackPawnOffset))
        {
          assert(_pc[_offset[from]].type == (White|Pawn));
          assert(_pc[_offset[from]].sqr == from);
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
      if (_offset[from] >= SliderOffset) {
        const int pc = _pc[_offset[from]].type;
        assert(IS_SLIDER(pc));
        assert(_pc[_offset[from]].sqr == from);
        if ((IS_DIAG(dir) && ((Black|pc) != (Black|Rook))) ||
            (IS_CROSS(dir) && ((Black|pc) != (Black|Bishop))))
        {
          atk |= (uint64_t(from + 1) << DirShift(dir));
        }
        break;
      }
      if ((from == end) || _offset[from]) {
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
void Exec(const Move& move, const Clunk& src, Clunk& dest) {
  assert(COLOR(src.state) == color);
  assert(move.IsValid());
  assert(IS_SQUARE(move.From()));
  assert(IS_SQUARE(move.To()));
  assert(!move.Cap() || IS_CAP(move.Cap()));
  assert(!move.Cap() || COLOR(move.Cap()) != color);
  assert(!move.Promo() || IS_PROMO(move.Promo()));
  assert(!move.Promo() || COLOR(move.Promo()) == color);

  _execs++;

  const int from  = move.From();
  const int to    = move.To();
  const int cap   = move.Cap();
  const int promo = move.Promo();
  const int moved = _offset[from];
  const int pc    = _pc[moved].type;

  assert(moved);
  assert(_pc[moved].sqr == from);
  assert(pc);
  assert(COLOR(pc) == color);

  if (IS_SLIDER(pc)) {
    ClearAttacksFrom(pc, from);
  }
  if (IS_SLIDER(cap)) {
    ClearAttacksFrom(cap, to);
  }

  switch (move.Type()) {
  case PawnMove:
    assert(pc == (color|Pawn));
    assert(!_offset[to]);
    assert(!cap);
    _offset[from] = 0;
    if (promo) {
      assert(IS_PROMO(promo));
      assert(COLOR(promo) == color);
      assert(YC(to) == (color ? 0 : 7));
      RemovePiece((color|Pawn), moved);
      _offset[to] = AddPiece(promo, to);
      _material[color] += (ValueOf(promo) - PawnValue);
      dest.pawnKey = (src.pawnKey ^ _HASH[pc][from]);
      dest.pieceKey = (src.pieceKey ^ _HASH[promo][to]);
    }
    else {
      assert(YC(to) != (color ? 0 : 7));
      _offset[to] = moved;
      _pc[moved].sqr = to;
      dest.pawnKey = (src.pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
      dest.pieceKey = src.pieceKey;
    }
    dest.state = ((src.state ^ 1) & _TOUCH[from] & _TOUCH[to]);
    dest.ep = 0;
    dest.rcount = 0;
    dest.mcount = (src.mcount + 1);
    break;
  case PawnLung:
    assert(pc == (color|Pawn));
    assert(YC(from) == (color ? 6 : 1));
    assert(!_offset[to]);
    assert(!_offset[to + (color ? North : South)]);
    assert(!cap);
    assert(!promo);
    _offset[from] = 0;
    _offset[to] = moved;
    _pc[moved].sqr = to;
    dest.state = ((src.state ^ 1) & _TOUCH[from] & _TOUCH[to]);
    dest.ep = (to + (color ? North : South));
    dest.rcount = 0;
    dest.mcount = (src.mcount + 1);
    dest.pawnKey = (src.pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
    dest.pieceKey = src.pieceKey;
    break;
  case PawnCap:
    assert(pc == (color|Pawn));
    _offset[from] = 0;
    if (promo) {
      assert(IS_PROMO(promo));
      assert(COLOR(promo) == color);
      assert(YC(to) == (color ? 0 : 7));
      assert(cap);
      assert(_offset[to]);
      assert(cap == _pc[_offset[to]].type);
      assert(_pc[_offset[to]].sqr == to);
      RemovePiece((color|Pawn), moved);
      RemovePiece(cap, _offset[to]);
      _offset[to] = AddPiece(promo, to);
      _material[color] += (ValueOf(promo) - PawnValue);
      _material[!color] -= ValueOf(cap);
      dest.pawnKey = (src.pawnKey ^ _HASH[pc][from]);
      dest.pieceKey = (src.pieceKey ^ _HASH[promo][to] ^ _HASH[cap][to]);
    }
    else {
      assert(YC(to) != (color ? 0 : 7));
      if (cap >= Knight) {
        assert(_offset[to]);
        assert(cap == _pc[_offset[to]].type);
        assert(_pc[_offset[to]].sqr == to);
        RemovePiece(cap, _offset[to]);
        _offset[to] = moved;
        _pc[moved].sqr = to;
        _material[!color] -= ValueOf(cap);
        dest.pawnKey = (src.pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
        dest.pieceKey = (src.pieceKey ^ _HASH[cap][to]);
      }
      else if (cap) {
        assert(cap == ((!color)|Pawn));
        assert(_offset[to]);
        assert(cap == _pc[_offset[to]].type);
        assert(_pc[_offset[to]].sqr == to);
        RemovePiece(((!color)|Pawn), _offset[to]);
        _offset[to] = moved;
        _pc[moved].sqr = to;
        _material[!color] -= ValueOf(cap);
        dest.pawnKey = (src.pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to] ^
                        _HASH[cap][to]);
        dest.pieceKey = src.pieceKey;
      }
      else {
        assert(src.ep && (to == src.ep));
        assert(_offset[src.ep + (color ? North : South)]);
        assert(_pc[_offset[src.ep + (color ? North : South)]].type == ((!color)|Pawn));
        RemovePiece(((!color)|Pawn), _offset[src.ep + (color ? North : South)]);
        _offset[to] = moved;
        _pc[moved].sqr = to;
        _material[!color] -= PawnValue;
        if (_atkd[src.ep + (color ? North : South)]) {
          ExtendAttacks(src.ep + (color ? North : South));
        }
        dest.pawnKey = (src.pawnKey ^ _HASH[pc][from] ^ _HASH[pc][to] ^
                        _HASH[(!color)|Pawn][src.ep + (color ? North : South)]);
        dest.pieceKey = src.pieceKey;
      }
    }
    dest.state = ((src.state ^ 1) & _TOUCH[from] & _TOUCH[to]);
    dest.ep = 0;
    dest.rcount = 0;
    dest.mcount = (src.mcount + 1);
    break;
  case KnightMove:
  case BishopMove:
  case RookMove:
  case QueenMove:
    assert(pc == (color|move.Type()));
    assert(cap ? (_offset[to] && (_pc[_offset[to]].type == cap)) : !_offset[to]);
    assert(!promo);
    _offset[from] = 0;
    dest.state = ((src.state ^ 1) & _TOUCH[from] & _TOUCH[to]);
    dest.ep = 0;
    dest.rcount = (cap ? 0 : (src.rcount + 1));
    dest.mcount = (src.mcount + 1);
    if (cap >= Knight) {
      RemovePiece(cap, _offset[to]);
      _material[!color] -= ValueOf(cap);
      dest.pawnKey = src.pawnKey;
      dest.pieceKey = (src.pieceKey ^ _HASH[pc][from] ^ _HASH[pc][to] ^
                       _HASH[cap][to]);
    }
    else if (cap) {
      assert(cap == ((!color)|Pawn));
      RemovePiece(((!color)|Pawn), _offset[to]);
      _material[!color] -= PawnValue;
      dest.pawnKey = (src.pawnKey ^ _HASH[cap][to]);
      dest.pieceKey = (src.pieceKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
    }
    else {
      dest.pawnKey = src.pawnKey;
      dest.pieceKey = (src.pieceKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
    }
    _offset[to] = moved;
    _pc[moved].sqr = to;
    break;
  case KingMove:
    assert(pc == (color|King));
    assert(moved == (color ? BlackKingOffset : WhiteKingOffset));
    assert(cap ? (_offset[to] && (_pc[_offset[to]].type == cap)) : !_offset[to]);
    assert(!promo);
    _offset[from] = 0;
    dest.state = ((src.state ^ 1) & _TOUCH[from] & _TOUCH[to]);
    dest.ep = 0;
    dest.rcount = (cap ? 0 : (src.rcount + 1));
    dest.mcount = (src.mcount + 1);
    if (cap >= Knight) {
      RemovePiece(cap, _offset[to]);
      dest.pawnKey = src.pawnKey;
      dest.pieceKey = (src.pieceKey ^ _HASH[pc][from] ^ _HASH[pc][to] ^
                       _HASH[cap][to]);
    }
    else if (cap) {
      assert(cap == ((!color)|Pawn));
      RemovePiece(((!color)|Pawn), _offset[to]);
      dest.pawnKey = (src.pawnKey ^ _HASH[cap][to]);
      dest.pieceKey = (src.pieceKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
    }
    else {
      dest.pawnKey = src.pawnKey;
      dest.pieceKey = (src.pieceKey ^ _HASH[pc][from] ^ _HASH[pc][to]);
    }
    _offset[to] = moved;
    _pc[moved].sqr = to;
    break;
  case CastleShort:
    assert(from == (color ? E8 : E1));
    assert(to == (color ? G8 : G1));
    assert(!cap);
    assert(!promo);
    assert(_offset[from] == (color ? BlackKingOffset : WhiteKingOffset));
    assert(_offset[color ? H8 : H1]);
    assert(_pc[_offset[color ? H8 : H1]].type == (color|Rook));
    assert(!_offset[color ? F8 : F1]);
    assert(!_offset[color ? G8 : G1]);
    assert(!AttackedBy<!color>(color ? E8 : E1));
    assert(!AttackedBy<!color>(color ? F8 : F1));
    assert(!AttackedBy<!color>(color ? G8 : G1));
    ClearAttacksFrom((color|Rook), (color ? H8 : H1));
    _offset[color ? E8 : E1] = 0;
    _offset[color ? G8 : G1] = moved;
    _offset[color ? F8 : F1] = _offset[color ? H8 : H1];
    _offset[color ? H8 : H1] = 0;
    _pc[moved].sqr = to;
    _pc[_offset[color ? F8 : F1]].sqr = (color ? F8 : F1);
    AddAttacksFrom((color|Rook), (color ? F8 : F1));
    dest.state = ((src.state ^ 1) & ~(color ? BlackCastle : WhiteCastle));
    dest.ep = 0;
    dest.rcount = (src.rcount + 1);
    dest.mcount = (src.mcount + 1);
    dest.pawnKey = src.pawnKey;
    dest.pieceKey = (src.pieceKey ^
                     _HASH[color|Rook][color ? F8 : F1] ^
                     _HASH[color|Rook][color ? H8 : H1] ^
                     _HASH[color|King][color ? E8 : E1] ^
                     _HASH[color|King][color ? G8 : G1]);
    break;
  case CastleLong:
    assert(from == (color ? E8 : E1));
    assert(to == (color ? C8 : C1));
    assert(!cap);
    assert(!promo);
    assert(_offset[from] == (color ? BlackKingOffset : WhiteKingOffset));
    assert(_offset[color ? A8 : A1]);
    assert(_pc[_offset[color ? A8 : A1]].type == (color|Rook));
    assert(!_offset[color ? B8 : B1]);
    assert(!_offset[color ? C8 : C1]);
    assert(!_offset[color ? D8 : D1]);
    assert(!AttackedBy<!color>(color ? C8 : C1));
    assert(!AttackedBy<!color>(color ? D8 : D1));
    assert(!AttackedBy<!color>(color ? E8 : E1));
    ClearAttacksFrom((color|Rook), (color ? A8 : A1));
    _offset[color ? D8 : D1] = _offset[color ? A8 : A1];
    _offset[color ? A8 : A1] = 0;
    _offset[color ? C8 : C1] = moved;
    _offset[color ? E8 : E1] = 0;
    _pc[moved].sqr = to;
    _pc[_offset[color ? D8 : D1]].sqr = (color ? D8 : D1);
    AddAttacksFrom((color|Rook), (color ? D8 : D1));
    dest.state = ((src.state ^ 1) & ~(color ? BlackCastle : WhiteCastle));
    dest.ep = 0;
    dest.rcount = (src.rcount + 1);
    dest.mcount = (src.mcount + 1);
    dest.pawnKey = src.pawnKey;
    dest.pieceKey = (src.pieceKey ^
                     _HASH[color|Rook][color ? A8 : A1] ^
                     _HASH[color|Rook][color ? D8 : D1] ^
                     _HASH[color|King][color ? C8 : C1] ^
                     _HASH[color|King][color ? E8 : E1]);
    break;
  default:
    assert(false);
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
  if (_offset[to] >= SliderOffset) {
    assert(IS_SLIDER(_pc[_offset[to]].type));
    AddAttacksFrom(_pc[_offset[to]].type, to);
  }

  assert(VerifyAttacks(true));
}

//-----------------------------------------------------------------------------
template<Color color>
void Undo(const Move& move, const Clunk& src) {
  assert(COLOR(src.state) == color);
  assert(move.IsValid());
  assert(IS_SQUARE(move.From()));
  assert(IS_SQUARE(move.To()));
  assert(!move.Cap() || IS_CAP(move.Cap()));
  assert(!move.Cap() || COLOR(move.Cap()) != color);
  assert(!move.Promo() || IS_PROMO(move.Promo()));
  assert(!move.Promo() || COLOR(move.Promo()) == color);

  const int from  = move.From();
  const int to    = move.To();
  const int cap   = move.Cap();
  const int promo = move.Promo();
  const int moved = _offset[to];
  const int pc    = _pc[moved].type;

  assert(!_offset[from]);
  assert(moved);
  assert(_pc[moved].sqr == to);
  assert(pc);
  assert(COLOR(pc) == color);

  if (IS_SLIDER(pc)) {
    ClearAttacksFrom(pc, to);
  }

  if (promo) {
    assert(_pc[moved].type == promo);
    RemovePiece(promo, moved);
    _material[color] += (PawnValue - ValueOf(promo));
  }

  switch (move.Type()) {
  case PawnMove:
  case PawnLung:
    assert(!cap);
    _offset[to] = 0;
    _offset[from] = (promo ? AddPiece((color|Pawn), from) : moved);
    _pc[_offset[from]].sqr = from;
    break;
  case PawnCap:
    if (cap) {
      _offset[to] = AddPiece(cap, to);
      _offset[from] = (promo ? AddPiece((color|Pawn), from) : moved);
      _pc[_offset[from]].sqr = from;
    }
    else {
      assert(pc == (color|Pawn));
      assert(src.ep && (to == src.ep));
      const int sqr = (src.ep + (color ? North : South));
      assert(!_offset[sqr]);
      if (_atkd[sqr]) {
        TruncateAttacks(sqr, -1);
      }
      _offset[to] = 0;
      _offset[sqr] = AddPiece(((!color)|Pawn), sqr);
      _offset[from] = moved;
      _pc[moved].sqr = from;
      _material[!color] += PawnValue;
    }
    break;
  case KnightMove:
  case BishopMove:
  case RookMove:
  case QueenMove:
  case KingMove:
    assert(pc == (color|move.Type()));
    assert(!promo);
    _offset[to] = (cap ? AddPiece(cap, to) : 0);
    _offset[from] = moved;
    _pc[moved].sqr = from;
    break;
  case CastleShort:
    assert(from == (color ? E8 : E1));
    assert(to == (color ? G8 : G1));
    assert(pc == (color|King));
    assert(!cap);
    assert(!promo);
    assert(!_offset[color ? E8 : E1]);
    assert(!_offset[color ? H8 : H1]);
    assert(_offset[color ? F8 : F1]);
    assert(_offset[color ? G8 : G1]);
    assert(_pc[_offset[color ? F8 : F1]].type == (color|Rook));
    ClearAttacksFrom((color|Rook), (color ? F8 : F1));
    _offset[color ? E8 : E1] = moved;
    _offset[color ? G8 : G1] = 0;
    _offset[color ? H8 : H1] = _offset[color ? F8 : F1];
    _offset[color ? F8 : F1] = 0;
    _pc[moved].sqr = from;
    _pc[_offset[color ? H8 : H1]].sqr = (color ? H8 : H1);
    AddAttacksFrom((color|Rook), (color ? H8 : H1));
    break;
  case CastleLong:
    assert(from == (color ? E8 : E1));
    assert(to == (color ? C8 : C1));
    assert(pc == (color|King));
    assert(!cap);
    assert(!promo);
    assert(!_offset[color ? E8 : E1]);
    assert(!_offset[color ? A8 : A1]);
    assert(!_offset[color ? B8 : B1]);
    assert(_offset[color ? C8 : C1]);
    assert(_offset[color ? D8 : D1]);
    assert(_pc[_offset[color ? D8 : D1]].type == (color|Rook));
    ClearAttacksFrom((color|Rook), (color ? D8 : D1));
    _offset[color ? A8 : A1] = _offset[color ? D8 : D1];
    _offset[color ? D8 : D1] = 0;
    _offset[color ? E8 : E1] = moved;
    _offset[color ? C8 : C1] = 0;
    _pc[moved].sqr = from;
    _pc[_offset[color ? A8 : A1]].sqr = (color ? A8 : A1);
    AddAttacksFrom((color|Rook), (color ? A8 : A1));
    break;
  default:
    assert(false);
  }

  if (_atkd[from]) {
    TruncateAttacks(from, to);
  }

  if (cap >= Knight) {
    _material[!color] += ValueOf(cap);
    if (cap >= Bishop) {
      AddAttacksFrom(cap, to);
    }
  }
  else if (cap) {
    assert(cap == ((!color)|Pawn));
    _material[!color] += PawnValue;
  }
  else {
    if (_atkd[to]) {
      ExtendAttacks(to);
    }
    if ((move.Type() == PawnCap) && _atkd[src.ep + (color ? North : South)]) {
      TruncateAttacks(src.ep + (color ? North : South), from);
    }
  }

  if (_offset[from] >= SliderOffset) {
    assert(IS_SLIDER(_pc[_offset[from]].type));
    AddAttacksFrom(_pc[_offset[from]].type, from);
  }

  assert(VerifyAttacks(true));
}

//-----------------------------------------------------------------------------
void AddMove(Clunk& node, const Color color, const MoveType type,
             const int from, const int to,
             const int cap = 0, const int promo = 0)
{
  assert(IS_MTYPE(type));
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(to));
  assert(from != to);
  assert(node.moveCount >= 0);
  assert((node.moveCount + 1) < MaxMoves);
  if (_pcount[!color] > _pcount[(!color)|Knight]) {
    if (_pinDir[from] && (_pinDir[from] != abs(Direction(from, to)))) {
      return;
    }
  }
  node.moves[node.moveCount++].Set(type, from, to, cap, promo);
}

//-----------------------------------------------------------------------------
template<Color color>
bool EpPinned(const int from, const int cap) {
  assert((2 + color) == (color ? BlackKingOffset : WhiteKingOffset));
  assert(_pc[2 + color].type == (color|King));
  assert(IS_SQUARE(_pc[2 + color].sqr));
  assert(_offset[_pc[2 + color].sqr] == (2 + color));
  assert(IS_SQUARE(from));
  assert(IS_SQUARE(cap));
  assert(from != cap);
  assert(from != _pc[2 + color].sqr);
  assert(YC(from) == YC(cap));
  assert(YC(from) == (color ? 3 : 4));
  assert(Distance(from, cap) == 1);
  assert(_offset[from]);
  assert(_offset[cap]);
  assert(_pc[_offset[from]].type == (color|Pawn));
  assert(_pc[_offset[cap]].type == ((!color)|Pawn));
  if (!_pcount[!color] || (YC(from) != YC(_pc[2 + color].sqr))) {
    return false;
  }
  int left = std::min<int>(from, cap);
  int right = std::max<int>(from, cap);
  if (_pc[2 + color].sqr < left) {
    right = ((_atkd[right] >> DirShift(West)) & 0xFF);
    assert(!right || (IS_SQUARE(right - 1) && (YC(right - 1) == YC(from))));
    assert(!right || (_offset[right - 1] >= SliderOffset));
    if (right-- && (_offset[right] >= RookOffset) &&
        (COLOR(_pc[_offset[right]].type) != color))
    {
      assert(_pc[_offset[right]].type >= Rook);
      assert(_pc[_offset[right]].type < King);
      assert(_pc[_offset[right]].sqr == right);
      while (--left > _pc[2 + color].sqr) {
        assert(IS_SQUARE(left));
        assert(Direction(right, left) == West);
        if (_offset[left]) {
          assert(_offset[left] != (2 + color));
          return false;
        }
      }
      assert(left == _pc[2 + color].sqr);
      return true;
    }
  }
  else {
    assert(_pc[2 + color].sqr > right);
    left = ((_atkd[left] >> DirShift(East)) & 0xFF);
    assert(!left || (IS_SQUARE(left - 1) && (YC(left - 1) == YC(from))));
    assert(!left || (_offset[left - 1] >= SliderOffset));
    if (left-- && (_offset[left] >= RookOffset) &&
        (COLOR(_pc[_offset[left]].type) != color))
    {
      assert(_pc[_offset[left]].type >= Rook);
      assert(_pc[_offset[left]].type < King);
      assert(_pc[_offset[left]].sqr == left);
      while (++right < _pc[2 + color].sqr) {
        assert(IS_SQUARE(right));
        assert(Direction(left, right) == East);
        if (_offset[right]) {
          assert(_offset[right] != (2 + color));
          return false;
        }
      }
      assert(right == _pc[2 + color].sqr);
      return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------
template<Color color>
int GetCheckEvasions(Clunk& node) {
  assert((2 + color) == (color ? BlackKingOffset : WhiteKingOffset));
  assert(_pc[2 + color].type == (color|King));
  assert(IS_SQUARE(_pc[2 + color].sqr));
  assert(_offset[_pc[2 + color].sqr] == (2 + color));
  int from = _pc[2 + color].sqr;
  int attackers = 0;
  int squareCount = 0;
  int squares[40];
  int xrayCount = 0;
  int xray[4] = {-1,-1,-1,-1};

  if (_pcount[!color] > _pcount[(!color)|Knight]) {
    memset(_pinDir, 0, sizeof(_pinDir));
  }

  for (uint64_t mvs = _knightMoves[from]; mvs; mvs >>=8) {
    assert(mvs & 0xFF);
    const int to = ((mvs & 0xFF) - 1);
    assert(IS_SQUARE(to));
    assert(to != from);
    if ((_offset[to] >= (color ? KnightOffset : BlackKnightOffset)) &&
        (_offset[to] < (color ? BlackKnightOffset : BishopOffset)))
    {
      assert(_pc[_offset[to]].type == ((!color)|Knight));
      assert(_pc[_offset[to]].sqr == to);
      attackers++;
      assert(squareCount < 40);
      squares[squareCount++] = to;
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
      if (_offset[to]) {
        firstPiece = _pc[_offset[to]].type;
        assert(firstPiece > 0);
        break;
      }
      if (to == end) {
        assert(!firstPiece);
        break;
      }
      to += dir;
    }

    assert(newSquareCount > squareCount);
    if (!firstPiece) {
      continue;
    }

    assert(IS_SQUARE(to));
    assert(firstPiece == _pc[_offset[to]].type);
    if (COLOR(firstPiece) == color) {
      if (_atkd[to]) {
        const int afrom = ((_atkd[to] >> DirShift(-dir)) & 0xFF);
        if (afrom) {
          assert(IS_SQUARE(afrom - 1));
          assert(_offset[afrom - 1] >= SliderOffset);
          assert(IS_SLIDER(_pc[_offset[afrom - 1]].type));
          assert(_pc[_offset[afrom - 1]].sqr == (afrom - 1));
          if (COLOR(_pc[_offset[afrom - 1]].type) != color) {
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

  assert(!_pc[0].type);
  if (attackers == 1) {
    // get non-king moves that block or capture the piece giving check
    for (int z = 0; z < squareCount; ++z) {
      const int to = squares[z];
      assert(IS_SQUARE(to));
      const int cap = _pc[_offset[to]].type;
      assert(!cap == !_offset[to]);
      assert(!cap || (_pc[_offset[to]].sqr == to));
      if ((cap == ((!color)|Pawn)) &&
          (node.ep == (to + (color ? South : North))))
      {
        if (XC(node.ep) > 0) {
          from = (node.ep + (color ? NorthWest : SouthWest));
          if ((_pc[_offset[from]].type == (color|Pawn)) &&
              !EpPinned<color>(from, to)) {
            AddMove(node, color, PawnCap, from, node.ep);
          }
        }
        if (XC(node.ep) < 7) {
          from = (node.ep + (color ? NorthEast : SouthEast));
          if ((_pc[_offset[from]].type == (color|Pawn)) &&
              !EpPinned<color>(from, to)) {
            AddMove(node, color, PawnCap, from, node.ep);
          }
        }
      }

      if (_pcount[color|Knight]) {
        for (uint64_t mvs = _knightMoves[to]; mvs; mvs >>= 8) {
          assert(mvs & 0xFF);
          from = ((mvs & 0xFF) - 1);
          assert(IS_SQUARE(from));
          assert(from != to);
          if ((_offset[from] >= (color ? BlackKnightOffset : KnightOffset)) &&
              (_offset[from] < (color ? BishopOffset : BlackKnightOffset)))
          {
            AddMove(node, color, KnightMove, from, to, cap);
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
          switch (_pc[_offset[from]].type) {
          case (color|Pawn):
            if (cap && (color ? (from > to) : (from < to)) &&
                (Distance(from, to) == 1))
            {
              if (YC(to) == (color ? 0 : 7)) {
                AddMove(node, color, PawnCap, from, to, cap, (color|Queen));
                AddMove(node, color, PawnCap, from, to, cap, (color|Rook));
                AddMove(node, color, PawnCap, from, to, cap, (color|Bishop));
                AddMove(node, color, PawnCap, from, to, cap, (color|Knight));
              }
              else {
                AddMove(node, color, PawnCap, from, to, cap);
              }
            }
            break;
          case (color|Bishop):
            AddMove(node, color, BishopMove, from, to, cap);
            break;
          case (color|Queen):
            AddMove(node, color, QueenMove, from, to, cap);
            break;
          }
          if ((from == end) || _offset[from]) {
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
          switch (_pc[_offset[from]].type) {
          case (color|Pawn):
            if (!cap && (dir == (color ? North : South))) {
              if (Distance(from, to) == 1) {
                if (YC(to) == (color ? 0 : 7)) {
                  AddMove(node, color, PawnMove, from, to, 0, (color|Queen));
                  AddMove(node, color, PawnMove, from, to, 0, (color|Rook));
                  AddMove(node, color, PawnMove, from, to, 0, (color|Bishop));
                  AddMove(node, color, PawnMove, from, to, 0, (color|Knight));
                }
                else {
                  AddMove(node, color, PawnMove, from, to);
                }
              }
              else if ((Distance(from, to) == 2) &&
                       (YC(to) == (color ? 4 : 3)))
              {
                AddMove(node, color, PawnLung, from, to);
              }
            }
            break;
          case (color|Rook):
            AddMove(node, color, RookMove, from, to, cap);
            break;
          case (color|Queen):
            AddMove(node, color, QueenMove, from, to, cap);
            break;
          }
          if ((from == end) || _offset[from]) {
            break;
          }
        }
      }
    }
  }

  // get king moves
  from = _pc[2 + color].sqr;
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

    const int cap = _pc[_offset[to]].type;
    assert(!cap == !_offset[to]);
    assert(!cap || (_pc[_offset[to]].sqr == to));
    if (!cap) {
      AddMove(node, color, KingMove, from, to);
    }
    else if (COLOR(cap) != color) {
      AddMove(node, color, KingMove, from, to, cap);
    }
  }
  return attackers;
}

//-----------------------------------------------------------------------------
template<Color color>
void GetPawnMoves(Clunk& node, const int from) {
  assert(IS_SQUARE(from));
  assert(_offset[from]);
  assert(_pc[_offset[from]].type == (color|Pawn));
  assert(_pc[_offset[from]].sqr == from);
  int to;
  for (uint64_t mvs = _pawnCaps[from + (color * 8)]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    to = ((mvs & 0xFF) - 1);
    assert(Distance(from, to) == 1);
    const int cap = _pc[_offset[to]].type;
    assert(!cap == !_offset[to]);
    assert(!cap || (_pc[_offset[to]].sqr == to));
    if (!cap) {
      if (node.ep && (to == node.ep) &&
          !EpPinned<color>(from, (node.ep + (color ? North : South))))
      {
        AddMove(node, color, PawnCap, from, to);
      }
    }
    else if (COLOR(cap) != color) {
      if (YC(to) == (color ? 0 : 7)) {
        AddMove(node, color, PawnCap, from, to, cap, (color|Queen));
        AddMove(node, color, PawnCap, from, to, cap, (color|Rook));
        AddMove(node, color, PawnCap, from, to, cap, (color|Bishop));
        AddMove(node, color, PawnCap, from, to, cap, (color|Knight));
      }
      else {
        AddMove(node, color, PawnCap, from, to, cap);
      }
    }
  }
  to = (from + (color ? South : North));
  assert(IS_SQUARE(to));
  if (!_offset[to]) {
    if (YC(to) == (color ? 0 : 7)) {
      AddMove(node, color, PawnMove, from, to, 0, (color|Queen));
      AddMove(node, color, PawnMove, from, to, 0, (color|Rook));
      AddMove(node, color, PawnMove, from, to, 0, (color|Bishop));
      AddMove(node, color, PawnMove, from, to, 0, (color|Knight));
    }
    else {
      AddMove(node, color, PawnMove, from, to);
      if (YC(from) == (color ? 6 : 1)) {
        to += (color ? South : North);
        assert(IS_SQUARE(to));
        if (!_offset[to]) {
          AddMove(node, color, PawnLung, from, to);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------
void GetKnightMoves(Clunk& node, const Color color, const int from) {
  assert(IS_SQUARE(from));
  assert(_offset[from]);
  assert(_pc[_offset[from]].type == (color|Knight));
  assert(_pc[_offset[from]].sqr == from);
  for (uint64_t mvs = _knightMoves[from]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int to = ((mvs & 0xFF) - 1);
    assert(IS_SQUARE(to));
    assert(to != from);
    const int cap = _pc[_offset[to]].type;
    assert(!cap == !_offset[to]);
    assert(!cap || (_pc[_offset[to]].sqr == to));
    if (!cap) {
      AddMove(node, color, KnightMove, from, to);
    }
    else if (COLOR(cap) != color) {
      AddMove(node, color, KnightMove, from, to, cap);
    }
  }
}

//-----------------------------------------------------------------------------
void GetSliderMoves(Clunk& node, const Color color, const MoveType type,
                    uint64_t mvs, const int from)
{
  assert(IS_SQUARE(from));
  assert(_offset[from]);
  assert(_pc[_offset[from]].type == (color|type));
  assert(_pc[_offset[from]].sqr == from);
  while (mvs) {
    assert(mvs & 0xFF);
    const int end = ((mvs & 0xFF) - 1);
    const int dir = (Direction(from, end));
    assert(IS_DIR(dir));
    for (int to = (from + dir);; to += dir) {
      assert(IS_SQUARE(to));
      assert(Direction(from, to) == dir);
      const int cap = _pc[_offset[to]].type;
      assert(!cap == !_offset[to]);
      assert(!cap || (_pc[_offset[to]].sqr == to));
      if (!cap) {
        AddMove(node, color, type, from, to);
      }
      else {
        if (COLOR(cap) != color) {
          AddMove(node, color, type, from, to, cap);
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

//-----------------------------------------------------------------------------
template<Color color>
void GetKingMoves(Clunk& node, const int from) {
  assert(IS_SQUARE(from));
  assert((2 + color) == (color ? BlackKingOffset : WhiteKingOffset));
  assert(_pc[2 + color].type == (color|King));
  assert(_pc[2 + color].sqr == from);
  assert(!AttackedBy<!color>(from));
  for (uint64_t mvs = _queenKing[from + 8]; mvs; mvs >>= 8) {
    assert(mvs & 0xFF);
    const int to = ((mvs & 0xFF) - 1);
    assert(Distance(from, to) == 1);
    if (!AttackedBy<!color>(to)) {
      const int cap = _pc[_offset[to]].type;
      assert(!cap == !_offset[to]);
      assert(!cap || (_pc[_offset[to]].sqr == to));
      if (!cap) {
        AddMove(node, color, KingMove, from, to);
        if ((to == (color ? F8 : F1)) &&
            (node.state & (color ? BlackShort : WhiteShort)) &&
            !_offset[color ? G8 : G1] &&
            !AttackedBy<!color>(color ? G8 : G1))
        {
          assert(from == (color ? E8 : E1));
          assert(_offset[color ? H8 : H1] == (color|Rook));
          AddMove(node, color, CastleShort, from, (color ? G8 : G1));
        }
        else if ((to == (color ? D8 : D1)) &&
                 (node.state & (color ? BlackLong : WhiteLong)) &&
                 !_offset[color ? C8 : C1] &&
                 !_offset[color ? B8 : B1] &&
                 !AttackedBy<!color>(color ? C8 : C1))
        {
          assert(from == (color ? E8 : E1));
          assert(_offset[color ? A8 : A1] == (color|Rook));
          AddMove(node, color, CastleLong, from, (color ? C8 : C1));
        }
      }
      else if (COLOR(cap) != color) {
        AddMove(node, color, KingMove, from, to, cap);
      }
    }
  }
}

//-----------------------------------------------------------------------------
template<Color color>
void GenerateMoves(Clunk& node) {
  assert(_pc[WhiteKingOffset].type == (White|King));
  assert(_pc[BlackKingOffset].type == (Black|King));
  assert(IS_SQUARE(_pc[WhiteKingOffset].sqr));
  assert(IS_SQUARE(_pc[BlackKingOffset].sqr));
  assert((_pcount[White] >= 0) && (_pcount[White] <= 10));
  assert((_pcount[Black] >= 0) && (_pcount[Black] <= 10));
  assert(_pcount[White] == (_pcount[White|Knight] + _pcount[White|Bishop] +
                            _pcount[White|Rook] + _pcount[White|Queen]));
  assert(_pcount[Black] == (_pcount[Black|Knight] + _pcount[Black|Bishop] +
                            _pcount[Black|Rook] + _pcount[Black|Queen]));
  assert((_pcount[White|Pawn] >= 0) && (_pcount[White|Pawn] <= 8));
  assert((_pcount[Black|Pawn] >= 0) && (_pcount[Black|Pawn] <= 8));
  assert((_pcount[White|Knight] >= 0) && (_pcount[White|Knight] <= 10));
  assert((_pcount[Black|Knight] >= 0) && (_pcount[Black|Knight] <= 10));
  assert((_pcount[White|Bishop] >= 0) && (_pcount[White|Bishop] <= 10));
  assert((_pcount[Black|Bishop] >= 0) && (_pcount[Black|Bishop] <= 10));
  assert((_pcount[White|Rook] >= 0) && (_pcount[White|Rook] <= 10));
  assert((_pcount[Black|Rook] >= 0) && (_pcount[Black|Rook] <= 10));
  assert((_pcount[White|Queen] >= 0) && (_pcount[White|Queen] <= 9));
  assert((_pcount[Black|Queen] >= 0) && (_pcount[Black|Queen] <= 9));

  node.moveCount = node.moveIndex = 0;
  if (GetCheckEvasions<color>(node)) {
    return;
  }

  for (int i = 0; i < _pcount[color|Pawn]; ++i) {
    const int from = _pc[(color ? BlackPawnOffset : PawnOffset) + i].sqr;
    GetPawnMoves<color>(node, from);
  }

  for (int i = 0; i < _pcount[color|Knight]; ++i) {
    const int from = _pc[(color ? BlackKnightOffset : KnightOffset) + i].sqr;
    GetKnightMoves(node, color, from);
  }

  for (int i = 0; i < _pcount[color|Bishop]; ++i) {
    const int from = _pc[(color ? BlackBishopOffset : BishopOffset) + i].sqr;
    GetSliderMoves(node, color, BishopMove, _bishopRook[from], from);
  }

  for (int i = 0; i < _pcount[color|Rook]; ++i) {
    const int from = _pc[(color ? BlackRookOffset : RookOffset) + i].sqr;
    GetSliderMoves(node, color, RookMove, _bishopRook[from + 8], from);
  }

  for (int i = 0; i < _pcount[color|Queen]; ++i) {
    const int from = _pc[(color ? BlackQueenOffset : QueenOffset) + i].sqr;
    GetSliderMoves(node, color, QueenMove, _queenKing[from], from);
  }

  assert((2 + color) == (color ? BlackKingOffset : WhiteKingOffset));
  GetKingMoves<color>(node, _pc[2 + color].sqr);
}

//-----------------------------------------------------------------------------
template<Color color>
uint64_t PerftSearch(Clunk& node, const int depth) {
  GenerateMoves<color>(node);
  if (!node.child || (depth <= 1)) {
    return node.moveCount;
  }

  uint64_t count = 0;
  while (!_stop && (node.moveIndex < node.moveCount)) {
    const Move& move = node.moves[node.moveIndex++];
    Exec<color>(move, node, *node.child);
    count += PerftSearch<!color>(*node.child, (depth - 1));
    Undo<color>(move, node);
  }
  return count;
}

//-----------------------------------------------------------------------------
template<Color color>
uint64_t PerftSearchRoot(Clunk& node, const int depth) {
  assert(!node.ply);
  assert(!node.parent);
  assert(node.child);
  assert(VerifyAttacks(true));

  GenerateMoves<color>(node);
  std::sort(node.moves, (node.moves + node.moveCount), Move::LexicalCompare);

  uint64_t total = 0;
  if (depth > 1) {
    while (!_stop && (node.moveIndex < node.moveCount)) {
      const Move& move = node.moves[node.moveIndex++];
      Exec<color>(move, node, *node.child);
      const uint64_t count = PerftSearch<!color>(*node.child, (depth - 1));
      Undo<color>(move, node);
      senjo::Output() << move.ToString() << ' ' << count;
      total += count;
    }
  }
  else {
    while (!_stop && (node.moveIndex < node.moveCount)) {
      senjo::Output() << node.moves[node.moveIndex++].ToString() << " 1 ";
      total++;
    }
  }
  return total;
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

//----------------------------------------------------------------------------
Clunk::Clunk()
  : parent(NULL),
    child(NULL),
    ply(0)
{
}

//----------------------------------------------------------------------------
Clunk::~Clunk()
{
}

//----------------------------------------------------------------------------
bool Clunk::IsInitialized() const
{
  return (child != NULL);
}

//----------------------------------------------------------------------------
bool Clunk::SetEngineOption(const std::string& /*optionName*/,
                            const std::string& /*optionValue*/)
{
  return false;
}

//----------------------------------------------------------------------------
bool Clunk::WhiteToMove() const
{
  return !COLOR(state);
}

//----------------------------------------------------------------------------
const char* Clunk::MakeMove(const char* str)
{
  if (!str ||
      !IS_X(str[0]) || !IS_Y(str[1]) ||
      !IS_X(str[2]) || !IS_Y(str[3]))
  {
    return NULL;
  }

  assert(!_pc[0].type);
  int from  = SQR(TO_X(str[0]), TO_Y(str[1]));
  int to    = SQR(TO_X(str[2]), TO_Y(str[3]));
  int pc    = _pc[_offset[from]].type;
  int cap   = _pc[_offset[to]].type;
  int promo = 0;

  const Color color = COLOR(state);
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
    GenerateMoves<Black>(*this);
  }
  else {
    GenerateMoves<White>(*this);
  }

  for (; moveIndex < moveCount; ++moveIndex) {
    const Move& move = moves[moveIndex];
    if ((move.From() == from) &&
        (move.To() == to) &&
        (move.Promo() == promo))
    {
      break;
    }
  }
  if (moveIndex >= moveCount) {
    return NULL;
  }

  if (color) {
    Exec<Black>(moves[moveIndex], *this, *this);
  }
  else {
    Exec<White>(moves[moveIndex], *this, *this);
  }

  return p;
}

//----------------------------------------------------------------------------
const char* Clunk::SetPosition(const char* fen)
{
  if (!fen || !*fen) {
    senjo::Output() << "NULL or empty fen string";
    return NULL;
  }

  memset(_offset, 0, sizeof(_offset));
  memset(_pc, 0, sizeof(_pc));
  memset(_pcount, 0, sizeof(_pcount));
  memset(_material, 0, sizeof(_material));
  memset(_atkd, 0, sizeof(_atkd));
  _seen.clear();

  checkState = CheckState::Unknown;
  state = 0;
  ep = 0;
  rcount = 0;
  mcount = 0;
  pawnKey = 0ULL;
  pieceKey = 0ULL;
  positionKey = 0ULL;

  const char* p = fen;
  for (int y = 7; y >= 0; --y) {
    for (int x = 0; x < 8; ++x, ++p) {
      if (!*p) {
        senjo::Output() << "Incomplete board position";
        return NULL;
      }
      else if (isdigit(*p)) {
        x += (*p - '1');
      }
      else {
        const int sqr = SQR(x,y);
        assert(IS_SQUARE(sqr));
        switch (*p) {
        case 'b':
          if (_pcount[Black|Bishop] >= 10) {
            senjo::Output() << "Too many black bishops";
            return NULL;
          }
          _offset[sqr] = AddPiece((Black|Bishop), sqr);
          _material[Black] += BishopValue;
          pieceKey ^= _HASH[Black|Bishop][sqr];
          break;
        case 'B':
          if (_pcount[White|Bishop] >= 10) {
            senjo::Output() << "Too many white bishops";
            return NULL;
          }
          _offset[sqr] = AddPiece((White|Bishop), sqr);
          _material[White] += BishopValue;
          pieceKey ^= _HASH[White|Bishop][sqr];
          break;
        case 'k':
          if (_pc[BlackKingOffset].type) {
            senjo::Output() << "Multiple black kings";
            return NULL;
          }
          _offset[sqr] = AddPiece((Black|King), sqr);
          pieceKey ^= _HASH[Black|King][sqr];
          break;
        case 'K':
          if (_pc[WhiteKingOffset].type) {
            senjo::Output() << "Multiple white kings";
            return NULL;
          }
          _offset[sqr] = AddPiece((White|King), sqr);
          pieceKey ^= _HASH[White|King][sqr];
          break;
        case 'n':
          if (_pcount[Black|Knight] >= 10) {
            senjo::Output() << "Too many black knights";
            return NULL;
          }
          _offset[sqr] = AddPiece((Black|Knight), sqr);
          _material[Black] += KnightValue;
          pieceKey ^= _HASH[Black|Knight][sqr];
          break;
        case 'N':
          if (_pcount[White|Knight] >= 10) {
            senjo::Output() << "Too many white knights";
            return NULL;
          }
          _offset[sqr] = AddPiece((White|Knight), sqr);
          _material[White] += KnightValue;
          pieceKey ^= _HASH[White|Knight][sqr];
          break;
        case 'p':
          if (_pcount[Black|Pawn] >= 8) {
            senjo::Output() << "Too many black pawns";
            return NULL;
          }
          _offset[sqr] = AddPiece((Black|Pawn), sqr);
          _material[Black] += PawnValue;
          pawnKey ^= _HASH[Black|Pawn][sqr];
          break;
        case 'P':
          if (_pcount[White|Pawn] >= 8) {
            senjo::Output() << "Too many white pawns";
            return NULL;
          }
          _offset[sqr] = AddPiece((White|Pawn), sqr);
          _material[White] += PawnValue;
          pawnKey ^= _HASH[White|Pawn][sqr];
          break;
        case 'q':
          if (_pcount[Black|Queen] >= 9) {
            senjo::Output() << "Too many black queens";
            return NULL;
          }
          _offset[sqr] = AddPiece((Black|Queen), sqr);
          _material[Black] += QueenValue;
          pieceKey ^= _HASH[Black|Queen][sqr];
          break;
        case 'Q':
          if (_pcount[White|Queen] >= 9) {
            senjo::Output() << "Too many white queens";
            return NULL;
          }
          _offset[sqr] = AddPiece((White|Queen), sqr);
          _material[White] += QueenValue;
          pieceKey ^= _HASH[White|Queen][sqr];
          break;
        case 'r':
          if (_pcount[Black|Rook] >= 10) {
            senjo::Output() << "Too many black rooks";
            return NULL;
          }
          _offset[sqr] = AddPiece((Black|Rook), sqr);
          _material[Black] += RookValue;
          pieceKey ^= _HASH[Black|Rook][sqr];
          break;
        case 'R':
          if (_pcount[White|Rook] >= 10) {
            senjo::Output() << "Too many hite rooks";
            return NULL;
          }
          _offset[sqr] = AddPiece((White|Rook), sqr);
          _material[White] += RookValue;
          pieceKey ^= _HASH[White|Rook][sqr];
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

  if (_pc[WhiteKingOffset].type != (White|King)) {
    senjo::Output() << "No white king in this position";
    return NULL;
  }
  if (_pc[BlackKingOffset].type != (Black|King)) {
    senjo::Output() << "No black king in this position";
    return NULL;
  }

  for (int sqr = A1; sqr <= H8; ++sqr) {
    if (BAD_SQR(sqr)) {
      sqr += 7;
    }
    else {
      assert(!_offset[sqr] || (_pc[_offset[sqr]].sqr == sqr));
      if (_offset[sqr] >= SliderOffset) {
        AddAttacksFrom(_pc[_offset[sqr]].type, sqr);
      }
    }
  }
  assert(VerifyAttacks(true));

  p = NextSpace(p);
  p = NextWord(p);
  switch (*p) {
  case 'b':
    state |= Black;
    if (AttackedBy<Black>(_pc[WhiteKingOffset].sqr)) {
      senjo::Output() << "The king can be captured in this position";
      return NULL;
    }
    break;
  case 'w':
    state |= White;
    if (AttackedBy<White>(_pc[BlackKingOffset].sqr)) {
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
    case 'k': state |= BlackShort; continue;
    case 'K': state |= WhiteShort; continue;
    case 'q': state |= BlackLong;  continue;
    case 'Q': state |= WhiteLong;  continue;
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
    ep = SQR(x, y);
    if ((y != (COLOR(state) ? 2 : 5)) ||
        _offset[ep] || !_offset[ep + (COLOR(state) ? North : South)] ||
        (_pc[_offset[ep + (COLOR(state) ? North : South)]].type != ((!COLOR(state))|Pawn)))
    {
      senjo::Output() << "Invalid en passant square: "
                      << senjo::Square(ep).ToString();
      return NULL;
    }
  }

  NextSpace(p);
  NextWord(p);
  if (isdigit(*p)) {
    while (*p && isdigit(*p)) {
      rcount = ((rcount * 10) + (*p++ - '0'));
    }
  }

  NextSpace(p);
  NextWord(p);
  if (isdigit(*p)) {
    while (*p && isdigit(*p)) {
      mcount = ((mcount * 10) + (*p++ - '0'));
    }
    if (mcount) {
      mcount = (((mcount - 1) * 2) + COLOR(state));
    }
  }

  NextSpace(p);
  NextWord(p);

  positionKey = (pawnKey ^ pieceKey ^ _HASH[0][state & 0x1F] ^ _HASH[0][ep]);
  return p;
}

//----------------------------------------------------------------------------
std::list<senjo::EngineOption> Clunk::GetOptions() const
{
  std::list<senjo::EngineOption> opts;
  return opts;
}

//----------------------------------------------------------------------------
std::string Clunk::GetAuthorName() const
{
  return "Shawn Chidester";
}

//----------------------------------------------------------------------------
std::string Clunk::GetCountryName() const
{
  return "USA";
}

//----------------------------------------------------------------------------
std::string Clunk::GetEngineName() const
{
  return (sizeof(void*) == 8) ? "Clunk" : "Clunk (32-bit)";
}

//----------------------------------------------------------------------------
std::string Clunk::GetEngineVersion() const
{
  std::string rev = MAKE_XSTR(GIT_REV);
  if (rev.size() > 7) {
    rev = rev.substr(0, 7);
  }
  return ("1.0." + rev);
}

//----------------------------------------------------------------------------
std::string Clunk::GetFEN() const
{
  char fen[256] = {0};
  char* p = fen;
  int empty = 0;

  // piece positions
  for (int y = 7; y >= 0; --y) {
    for (int x = 0; x < 8; ++x) {
      const int sqr = SQR(x,y);
      const int type = _pc[_offset[sqr]].type;
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

  // color to move
  *p++ = ' ';
  *p++ = (COLOR(state) ? 'b' : 'w');

  // castling
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

  // en passant square
  *p++ = ' ';
  if (ep) {
    *p++ = ('a' + XC(ep));
    *p++ = ('1' + YC(ep));
  }
  else {
    *p++ = '-';
  }

  // reversible half-move count and full move count
  snprintf(p, (sizeof(fen) - strlen(fen)), " %d %d",
           rcount, ((mcount + 2) / 2));
  return fen;
}

//----------------------------------------------------------------------------
void Clunk::ClearSearchData()
{
  // TODO
}

//----------------------------------------------------------------------------
void Clunk::Initialize()
{
  InitNodes(this);
  InitDistDir();
  InitMoveMaps();

  child = _node;

  SetPosition(_STARTPOS);
}

//----------------------------------------------------------------------------
void Clunk::PonderHit()
{
}

//----------------------------------------------------------------------------
void Clunk::PrintBoard() const
{
  senjo::Output out;
  out << '\n';
  for (int y = 7; y >= 0; --y) {
    for (int x = 0; x < 8; ++x) {
      const int sqr = SQR(x,y);
      const int type = _pc[_offset[sqr]].type;
      switch (type) {
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
      if (ep) {
        out << "  En Passant Square : " << senjo::Square(ep).ToString();
      }
      break;
    }
    out << '\n';
  }
  out << '\n';
}

//----------------------------------------------------------------------------
void Clunk::Quit() {
  ChessEngine::Quit();
  clunk::_stop |= senjo::ChessEngine::FullStop;
}

//----------------------------------------------------------------------------
void Clunk::Stop(const StopReason reason) {
  ChessEngine::Stop(reason);
  clunk::_stop |= reason;
}

//----------------------------------------------------------------------------
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
    snprintf(move, movelen, "%s", _currmove.ToString().c_str());
  }
}

//----------------------------------------------------------------------------
uint64_t Clunk::MyPerft(const int depth)
{
  if (!child) {
    senjo::Output() << "Engine not initialized";
    return 0;
  }

  if (_debug) {
    PrintBoard();
    senjo::Output() << GetFEN();
  }

  InitSearch();

  const int d = std::min<int>(depth, MaxPlies);
  const uint64_t count = WhiteToMove() ? PerftSearchRoot<White>(*this, d)
                                       : PerftSearchRoot<Black>(*this, d);

  const uint64_t msecs = (senjo::Now() - _startTime);
  senjo::Output() << "Perft " << count << ' '
                  << senjo::Rate((count / 1000), msecs) << " KLeafs/sec";

  return count;
}

//----------------------------------------------------------------------------
std::string Clunk::MyGo(const int depth,
                        const int /*movestogo*/,
                        const uint64_t /*movetime*/,
                        const uint64_t /*wtime*/, const uint64_t /*winc*/,
                        const uint64_t /*btime*/, const uint64_t /*binc*/,
                        std::string* /*ponder*/)
{
  if (!child) {
    senjo::Output() << "Engine not initialized";
    return std::string();
  }

  // TODO

  return std::string();
}

} // namespace clunk
