//----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//----------------------------------------------------------------------------

#ifndef CLUNK_DEFS_H
#define CLUNK_DEFS_H

#include "Types.h"

namespace clunk
{

#define COLOR(x)      static_cast<Color>((x) & 1)
#define BAD_SQR(x)    ((x) & ~0x77)
#define IS_COLOR(x)   (((x) == Black) | ((x) == White))
#define IS_SQUARE(x)  (!BAD_SQR(x))
#define IS_PTYPE(x)   (((x) >= (White|Pawn)) & ((x) <= (Black|King)))
#define IS_PAWN(x)    ((Black|(x)) == (Black|Pawn))
#define IS_KING(x)    ((Black|(x)) == (Black|King))
#define IS_CAP(x)     (((x) >= (White|Pawn)) & ((x) <= (Black|Queen)))
#define IS_PROMO(x)   (((x) >= (White|Knight)) & ((x) <= (Black|Queen)))
#define IS_SLIDER(x)  (((x) >= (White|Bishop)) & (((x) <= (Black|Queen))))
#define IS_MTYPE(x)   (((x) >= PawnMove) & ((x) <= CastleLong))
#define XC(x)         ((x) & 0xF)
#define YC(x)         ((x) / 16)
#define IS_DIR(x)     (IS_DIAG(x) | IS_CROSS(x))
#define IS_DIAG(x)    (((x) == SouthWest) | \
                       ((x) == SouthEast) | \
                       ((x) == NorthWest) | \
                       ((x) == NorthEast))
#define IS_CROSS(x)   (((x) == South) | \
                       ((x) == West) | \
                       ((x) == East) | \
                       ((x) == North))

} // namespace clunk

#endif // CLUNK_DEFS_H

