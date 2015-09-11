//----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//----------------------------------------------------------------------------

#ifndef CLUNK_TYPES_H
#define CLUNK_TYPES_H

namespace clunk
{

//-----------------------------------------------------------------------------
typedef bool Color;

//-----------------------------------------------------------------------------
enum Square {
  A1=0x00,B1,C1,D1,E1,F1,G1,H1, //   0,  1,  2,  3,  4,  5,  6,  7
  A2=0x10,B2,C2,D2,E2,F2,G2,H2, //  16, 17, 18, 19, 20, 21, 22, 23
  A3=0x20,B3,C3,D3,E3,F3,G3,H3, //  32, 33, 34, 35, 36, 37, 38, 39
  A4=0x30,B4,C4,D4,E4,F4,G4,H4, //  48, 49, 50, 51, 52, 53, 54, 55
  A5=0x40,B5,C5,D5,E5,F5,G5,H5, //  64, 65, 66, 67, 68, 69, 70, 71
  A6=0x50,B6,C6,D6,E6,F6,G6,H6, //  80, 81, 82, 83, 84, 85, 86, 87
  A7=0x60,B7,C7,D7,E7,F7,G7,H7, //  96, 97, 98, 99,100,101,102,103
  A8=0x70,B8,C8,D8,E8,F8,G8,H8  // 112,113,114,115,116,117,118,119
};

//-----------------------------------------------------------------------------
enum Direction {
  SouthWest   = -17,
  South       = -16,
  SouthEast   = -15,
  West        =  -1,
  East        =   1,
  NorthWest   =  15,
  North       =  16,
  NorthEast   =  17
};

//-----------------------------------------------------------------------------
enum ColorType {
  White = 0,
  Black = 1
};

//-----------------------------------------------------------------------------
enum NodeType {
  PV,
  NonPV
};

//-----------------------------------------------------------------------------
enum PieceType {
  Pawn    = 2,
  Knight  = 4,
  Bishop  = 6,
  Rook    = 8,
  Queen   = 10,
  King    = 12
};

//-----------------------------------------------------------------------------
enum PieceValue {
  PawnValue   = 100,
  KnightValue = 300,
  BishopValue = 320,
  RookValue   = 500,
  QueenValue  = 975
};

//-----------------------------------------------------------------------------
enum MoveType {
  NoMove,
  PawnMove,
  PawnLung,
  PawnCap,
  KnightMove = Knight,
  BishopMove = Bishop,
  RookMove   = Rook,
  QueenMove  = Queen,
  KingMove   = King,
  CastleShort,
  CastleLong
};

//-----------------------------------------------------------------------------
enum StateBits {
  SideToMove  = 0x01,
  WhiteShort  = 0x02,
  WhiteLong   = 0x04,
  WhiteCastle = (WhiteShort|WhiteLong),
  BlackShort  = 0x08,
  BlackLong   = 0x10,
  BlackCastle = (BlackShort|BlackLong),
  CastleMask  = (WhiteCastle|BlackCastle),
  Draw        = 0x20
};

//-----------------------------------------------------------------------------
enum CheckState {
  Unknown,
  IsInCheck,
  NotInCheck
};

//-----------------------------------------------------------------------------
enum {
  WhiteKingOffset   = 2,
  BlackKingOffset   = 3,
  PawnOffset        = 4,
  BlackPawnOffset   = 12,
  KnightOffset      = 20,
  BlackKnightOffset = 30,
  BishopOffset      = 40,
  BlackBishopOffset = 50,
  RookOffset        = 60,
  BlackRookOffset   = 70,
  QueenOffset       = 80,
  BlackQueenOffset  = 90,
  PieceListSize     = 100,
  MaxPlies          = 100,
  MaxMoves          = 128,
  StartMaterial     = ((8 * PawnValue) + (2 * KnightValue) +
                       (2 * BishopValue) + (2 * RookValue) +  QueenValue),
  WinningScore      = 30000,
  MateScore         = 31000,
  Infinity          = 32000,
  HugeDelta         = 64000
};

} // namespace clunk

#endif // CLUNK_TYPES_H
