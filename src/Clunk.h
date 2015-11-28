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
struct Node;

//----------------------------------------------------------------------------
class Clunk : public senjo::ChessEngine
{
public:
  Clunk();
  virtual ~Clunk();

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
  void ClearStopFlags();
  void Initialize();
  void PonderHit();
  void PrintBoard() const;
  void Quit();
  void ResetStatsTotals();
  void SetDebug(const bool flag);
  void ShowStatsTotals() const;
  void Stop(const StopReason);
  void GetStats(int* depth,
                int* seldepth = NULL,
                uint64_t* nodes = NULL,
                uint64_t* qnodes = NULL,
                uint64_t* msecs = NULL,
                int* movenum = NULL,
                char* move = NULL,
                const size_t movelen = 0) const;

protected:
  uint64_t MyPerft(const int depth);
  uint64_t MyQPerft(const int depth);
  std::string MyGo(const int depth,
                   const int movestogo = 0,
                   const uint64_t movetime = 0,
                   const uint64_t wtime = 0, const uint64_t winc = 0,
                   const uint64_t btime = 0, const uint64_t binc = 0,
                   std::string* ponder = NULL);

private:
  Node* root;
};

} // namespace clunk

#endif // CLUNK_H
