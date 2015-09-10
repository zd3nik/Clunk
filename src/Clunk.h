//----------------------------------------------------------------------------
// Copyright (c) 2015 Shawn Chidester <zd3nik@gmail.com>, All rights reserved
//----------------------------------------------------------------------------

#ifndef CLUNK_H
#define CLUNK_H

#include "senjo/ChessEngine.h"
#include "Move.h"

namespace clunk
{

//----------------------------------------------------------------------------
class Clunk : public senjo::ChessEngine
{
public:
  Clunk();
  virtual ~Clunk();

  //--------------------------------------------------------------------------
  // unchanging
  //--------------------------------------------------------------------------
  Clunk* parent;
  Clunk* child;
  int ply;

  //--------------------------------------------------------------------------
  // updated by Exec()
  //--------------------------------------------------------------------------
  int checkState;
  int state;
  int ep;
  int rcount;
  int mcount;
  uint64_t pawnKey;
  uint64_t pieceKey;
  uint64_t positionKey;

  //--------------------------------------------------------------------------
  // updated by move generation and search
  //--------------------------------------------------------------------------
  int moveIndex;
  int moveCount;
  Move moves[MaxMoves];

  //--------------------------------------------------------------------------
  // senjo::ChessEngine members
  //--------------------------------------------------------------------------
  bool IsInitialized() const;
  bool SetEngineOption(const std::string& name, const std::string& value);
  bool WhiteToMove() const;
  const char* MakeMove(const char* str);
  const char* SetPosition(const char* fen);
  std::list<senjo::EngineOption> GetOptions() const;
  std::string GetAuthorName() const;
  std::string GetCountryName() const;
  std::string GetEngineName() const;
  std::string GetEngineVersion() const;
  std::string GetFEN() const;
  void ClearSearchData();
  void Initialize();
  void PonderHit();
  void PrintBoard() const;
  void Quit();
  void GetStats(int* depth,
                int* seldepth = NULL,
                uint64_t* nodes = NULL,
                uint64_t* qnodes = NULL,
                uint64_t* msecs = NULL,
                int* movenum = NULL,
                char* move = NULL,
                const size_t movelen = 0) const;

protected:
  //--------------------------------------------------------------------------
  // senjo::ChessEngine members
  //--------------------------------------------------------------------------
  uint64_t MyPerft(const int depth);
  std::string MyGo(const int depth,
                   const int movestogo = 0,
                   const uint64_t movetime = 0,
                   const uint64_t wtime = 0, const uint64_t winc = 0,
                   const uint64_t btime = 0, const uint64_t binc = 0,
                   std::string* ponder = NULL);
};

} // namespace clunk

#endif // CLUNK_H

