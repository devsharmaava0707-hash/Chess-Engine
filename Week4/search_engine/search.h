#pragma once
#include <chrono>
#include <cstdint>
#include "../chess.hpp"
#include "tt.h"
namespace search
{
    constexpr int INF = 32000;
    constexpr int MATE_SCORE = 31000;
    constexpr int MAX_PLY = 128;
    constexpr int MAX_QPLY = 64;
    constexpr int NMP_VERIFICATION_MIN_DEPTH = 7;
    struct SearchLimits
{
    int64_t myTimeMs = 0;
    int64_t incrementMs = 0;
    
    int movesToGo = 0;

    int64_t moveTimeMs = 0;

    bool infinite = false;
};
    struct SearchStats{
        uint64_t nodes = 0;
        chess::Move killers[MAX_PLY][2]{};
        int history[2][64][64]{};
        chess::Move counterMoves[2][64][64]{};
        tt::Table table{16};
        uint64_t ttHits = 0;
        uint64_t ttCutoffs = 0;
        uint64_t nullMoveCutoffs = 0;
        uint64_t nullMoveAttempts = 0;
        uint64_t rootTTHits = 0;
        uint64_t aspirationFailLow = 0;
        uint64_t aspirationFailHigh = 0;
        // constexpr int NMP_VERIFICATION_MIN_DEPTH = 7;
        // time related
        std::chrono::steady_clock::time_point startTime;
        int64_t optimalMs = 0;
        int64_t maximumMs = 0;
        bool stop = false;
        bool stable = true;
int previousScore = 0;
chess::Move previousBestMove = chess::Move::NO_MOVE;
    };
    int64_t elapsedMs(const SearchStats& stats);
    bool timeUp(const SearchStats& stats);
    int evaluateForSideToMove(chess::Board& board);
    int quiescence(chess::Board& board,int alpha,int beta,SearchStats& stats,int qply = 0);
    int negamax(chess::Board& board,int depth,int alpha,int beta,SearchStats &stats,chess::Move prevMove);

    chess::Move findBestMove(chess::Board& board,int depth,SearchStats &stats,const SearchLimits & limits);
}