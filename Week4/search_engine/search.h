#pragma once
#include <chrono>
#include <cstdint>
#include "../chess.hpp"
#include "tt.h"
#include "../book/book.h"
#include <atomic>
#include <vector>
#include <functional>
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
    int64_t maxNodes = 0;
    int64_t moveOverheadMs = 50;
};
    struct SearchInfo
    {
        int depth = 0;
        int seldepth = 0;
        bool isMate = false;
        int score = 0;
        int mateIn = 0;
        uint64_t nodes = 0;
        int64_t timeMs = 0;
        uint64_t nps = 0;
        int hashfull = 0;
        std::vector<chess::Move> pv;
    };

    using InfoCallback = std::function<void(const SearchInfo&)>;
    struct SearchStats{
        uint64_t nodes = 0;
        chess::Move killers[MAX_PLY][2]{};
        int history[2][64][64]{};
        // int continuationHistory[64][64][64]{};
        chess::Move counterMoves[2][64][64]{};
        tt::Table table{16};
        uint64_t ttHits = 0;
        uint64_t ttCutoffs = 0;
        uint64_t nullMoveCutoffs = 0;
        uint64_t nullMoveAttempts = 0;
        uint64_t rootTTHits = 0;
        uint64_t aspirationFailLow = 0;
        uint64_t aspirationFailHigh = 0;
        
        // bool stop = false;
        std::atomic<bool> stop{false};
        bool stable = true;
        int previousScore = 0;
        chess::Move previousBestMove = chess::Move::NO_MOVE;
        uint64_t lmrReductions = 0;
        uint64_t lmrResearches = 0;
        uint64_t iirReductions = 0;
        uint64_t maxNodes = 0;
        int seldepth = 0;
        InfoCallback onIteration = nullptr;
        // constexpr int NMP_VERIFICATION_MIN_DEPTH = 7;
        // time related
        std::chrono::steady_clock::time_point startTime;
        int completedDepth = 0;
        int64_t optimalMs = 0;
        int64_t maximumMs = 0;
        int64_t softStopMs = 0;
        int64_t hardStopMs = 0;
        int64_t lastIterationMs = 0;
        int64_t lastIterationNodes = 0;
        int64_t previousIterationMs = 0;
        int64_t previousIterationNodes = 0;
        int timeCheckPeriod = 1024;

        // polyglot implementation
        book::PolyglotBook book;

        // extras
        int staticEvalStack[MAX_PLY]{};
        int extensionCap = 0;
    };
    int64_t elapsedMs(const SearchStats& stats);
    bool timeUp(const SearchStats& stats);
    int evaluateForSideToMove(chess::Board& board);
    int quiescence(chess::Board& board,int alpha,int beta,SearchStats& stats,int qply,int rootPly);
    // int negamax(chess::Board& board,int depth,int alpha,int beta,SearchStats &stats,chess::Move prevMove);
    int negamax(chess::Board& board,int depth,int alpha,int beta,SearchStats& stats,int ply,chess::Move prevMove,bool nullMoveAllowe,int extcount);
    std::vector<chess::Move> extractPV(chess::Board board, SearchStats& stats, int maxLength);
    chess::Move findBestMove(chess::Board& board,int depth,SearchStats &stats,const SearchLimits & limits);
}