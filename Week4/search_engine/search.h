#pragma once

#include "../chess.hpp"
#include "tt.h"
namespace search
{
    constexpr int INF = 32000;
    constexpr int MATE_SCORE = 31000;
    constexpr int MAX_PLY = 128;
    constexpr int MAX_QPLY = 64;
    struct SearchStats{
        uint64_t nodes = 0;
        chess::Move killers[MAX_PLY][2]{};
        int history[2][64][64]{};
        chess::Move counterMoves[2][64][64]{};
        tt::Table table{16};
        uint64_t ttHits = 0;
        uint64_t ttCutoffs = 0;
    };
    int evaluateForSideToMove(chess::Board& board);
    int quiescence(chess::Board& board,int alpha,int beta,SearchStats& stats,int qply = 0);
    int negamax(chess::Board& board,int depth,int alpha,int beta,SearchStats &stats,chess::Move prevMove);

    chess::Move findBestMove(chess::Board& board,int depth,SearchStats &stats);
}