#include "search.h"
#include<iostream>
#include "../manual_evaluation_chess/eval.hpp"
#include "see.h"
#include "move_ordering.h"
#include<algorithm>
namespace search
{
    struct TimeBudget
{
    int64_t optimalMs = 0;
    int64_t maximumMs = 0;
};
int estimateMovesToGo(int moveNumber)
{
    int mtg = 45 - moveNumber / 2;

    return std::clamp(mtg, 15, 45);
}
TimeBudget computeTimeBudget(
    const SearchLimits& limits,
    int moveNumber)
{
    if (limits.infinite)
    return {0, 0}; 
    // Explicit movetime.
    if (limits.moveTimeMs > 0)
    {
        int64_t usable =
            std::max<int64_t>(1, limits.moveTimeMs - 50);

        return {
            usable,
            usable
        };
    }

    // No clock information.
    if (limits.myTimeMs <= 0)
        return {0, 0};

    int movesToGo = limits.movesToGo;

    if (movesToGo <= 0)
        movesToGo = estimateMovesToGo(moveNumber);

    // Reference-style basic allocation:
    // remaining time / moves to go + increment.
    int64_t optimal =
        limits.myTimeMs / movesToGo +
        limits.incrementMs;

    // Don't plan to spend more than 85% of the remaining clock.
    int64_t hardCap =
        (limits.myTimeMs * 85) / 100;

    optimal = std::min(optimal, hardCap);

    // Keep 50 ms for communication / overhead.
    if (limits.myTimeMs > 50)
        optimal =
            std::min(optimal, limits.myTimeMs - 50);

    optimal = std::max<int64_t>(1, optimal);

    int64_t maximum =
        std::min(optimal * 3, hardCap);

    if (limits.myTimeMs > 50)
        maximum =
            std::min(maximum, limits.myTimeMs - 50);

    maximum =
        std::max(maximum, optimal);

    return {
        optimal,
        maximum
    };
}
int64_t elapsedMs(const SearchStats& stats)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - stats.startTime
    ).count();
}
bool timeUp(const SearchStats& stats)
{
    if (stats.maximumMs <= 0)
        return false;

    return elapsedMs(stats) >= stats.maximumMs;
}
int evaluateForSideToMove(chess::Board& board)
{
    int score = evaluate(board);

    return board.sideToMove() == chess::Color::WHITE
        ? score
        : -score;
}
int quiescence(chess::Board& board,int alpha,int beta,SearchStats& stats,int qply){
    ++stats.nodes;

if ((stats.nodes & 2047) == 0 &&
    timeUp(stats))
{
    stats.stop = true;
}

if (stats.stop)
    return 0;

    // Safety limit for the tactical search.
    if (qply >= MAX_QPLY)
        return evaluateForSideToMove(board);

    const bool inCheck = board.inCheck();

    /*
        If we are not in check, we can stand pat:
        "What if I make no more tactical captures?"
    */
    if (!inCheck)
{
    int standPat = evaluateForSideToMove(board);

    if (standPat >= beta)
        return beta;

    if (standPat > alpha)
        alpha = standPat;

    // Delta pruning
    constexpr int DELTA_MARGIN = 200;

    if (standPat +
        piece_value_bonus(
            chess::PieceType::QUEEN,
            true
        ) +
        DELTA_MARGIN < alpha)
    {
        return alpha;
    }
}

    chess::Movelist moves;

    if (inCheck)
    {
        // In check: we must consider every legal response.
        chess::movegen::legalmoves(moves, board);
    }
    else
    {
        // Not in check: only forcing captures.
        chess::movegen::legalmoves<
            chess::movegen::MoveGenType::CAPTURE
        >(moves, board);
    }

    // Checkmate during qsearch.
    if (moves.empty())
    {
        if (inCheck)
            return -MATE_SCORE + qply;

        return alpha;
    }

    for (const auto& move : moves)
    {
    // In a normal quiescence node, skip captures
    // that SEE says are materially losing.
    //
    // When in check, we must examine every legal move.
    if (!inCheck && search::see::evaluate(board, move) < 0)
        continue;

    board.makeMove(move);

    int score = -quiescence(
        board,
        -beta,
        -alpha,
        stats,
        qply + 1
    );

    board.unmakeMove(move);
    if (stats.stop)
    {
        return 0;
    }
    if (score >= beta)
        return beta;

    if (score > alpha)
        alpha = score;
    }

    return alpha;
}

    int negamax(chess::Board& board,int depth,int alpha,int beta,SearchStats& stats,int ply,chess:: Move prevMove,bool nullMoveAllowed)
{
    ++stats.nodes;

if ((stats.nodes & 2047) == 0 &&
    timeUp(stats))
{
    stats.stop = true;
}

if (stats.stop)
    return 0;
    int alphaOriginal = alpha;
    uint64_t key = board.hash();
    tt::Entry* entry = stats.table.probe(key);

if (entry != nullptr)
{
    ++stats.ttHits;

    if (entry->depth >= depth)
    {
        int ttScore =
            tt::valueFromTT(entry->score, ply);

        if (entry->bound == tt::Bound::EXACT)
        {
            ++stats.ttCutoffs;
            return ttScore;
        }

        if (entry->bound == tt::Bound::LOWERBOUND &&
            ttScore >= beta)
        {
            ++stats.ttCutoffs;
            return ttScore;
        }

        if (entry->bound == tt::Bound::UPPERBOUND &&
            ttScore <= alpha)
        {
            ++stats.ttCutoffs;
            return ttScore;
        }
    }
}

    if (depth <= 0)return quiescence(board, alpha, beta, stats);
    // REF
    bool isPV = (beta - alpha > 1);
    int staticEval = evaluateForSideToMove(board);
    constexpr int RFP_MARGIN_PER_DEPTH = 120;
    int rfpMargin = RFP_MARGIN_PER_DEPTH * depth;
    if (!isPV &&!board.inCheck() &&depth <= 6 &&staticEval - rfpMargin >= beta)
    {
        return staticEval;
    }   
    // Razoring
    if (!isPV &&
    !board.inCheck() &&
    depth <= 3 &&
    staticEval + 200 * depth < alpha)
    {
        return quiescence(board, alpha, beta, stats);
    }  
    // IIR
    // Internal Iterative Reduction
    int searchDepth=depth;
    // if (entry == nullptr &&!board.inCheck() &&depth >= 4)
    // {
    //     searchDepth=depth-1;
    // } 
    if (!isPV &&
    !board.inCheck() &&
    nullMoveAllowed &&
    depth >= 3 &&
     
    staticEval >= beta)
{
    // ++nullMoveAttempts = 0;
     ++stats.nullMoveAttempts;
    chess::Color us = board.sideToMove();

    chess::Bitboard nonPawnPieces =
        board.us(us) &
        ~board.pieces(chess::PieceType::PAWN, us) &
        ~board.pieces(chess::PieceType::KING, us);

    if (nonPawnPieces.count() >= 1)
    {
        int R = (depth >= 6 ? 3 : 2);

        if (staticEval - beta >= 200)
            ++R;

        board.makeNullMove();

        int nullScore = -negamax(
            board,
            depth - 1 - R,
            -beta,
            -beta + 1,
            stats,
            ply + 1,
            chess::Move::NO_MOVE,
            false
        );

        board.unmakeNullMove();

        if (nullScore >= beta)
        {
            // For now, simple cutoff.
             ++stats.nullMoveCutoffs; // temp
             if (depth < NMP_VERIFICATION_MIN_DEPTH)
        return beta;

    int verifyScore = negamax(
        board,
        depth - R,
        alpha,
        beta,
        stats,
        ply,
        prevMove,
        false
    );
    if (verifyScore >= beta)
            return beta;
        }
    }
}
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    if (moves.empty())
{
    if (board.inCheck())
        return -MATE_SCORE + ply;

    return 0;
}

    int safePly = std::min(ply, MAX_PLY - 1);

    int side =
        static_cast<int>(board.sideToMove());
    chess::Move counterMove = chess::Move::NO_MOVE;

if (prevMove != chess::Move::NO_MOVE)
{
    counterMove =
        stats.counterMoves[side]
                          [prevMove.from().index()]
                          [prevMove.to().index()];
}
    chess::Move ttMove = chess::Move::NO_MOVE;

    if (entry != nullptr) ttMove = entry->bestMove;
    search::ordering::orderMoves(
        board,
        moves,
        stats.killers[safePly],
        counterMove,
        stats.history[side],
        ttMove
    );

    int bestScore = -INF;
    chess::Move bestMove = chess::Move::NO_MOVE;
    constexpr int MAX_QUIETS = 256;
    chess::Move quietsTried[MAX_QUIETS];
    int quietCount = 0;
    bool firstMove=true;
    bool inCheck = board.inCheck();
    int moveIndex=0;
    for (const auto& move : moves)
{
    bool isCapture =
        move.typeOf() == chess::Move::ENPASSANT ||
        (move.typeOf() != chess::Move::CASTLING &&
         board.at(move.to()) != chess::Piece::NONE);

    bool isQuiet =
        !isCapture &&
        move.typeOf() != chess::Move::PROMOTION;

    bool isPromotion =
        move.typeOf() == chess::Move::PROMOTION;

    bool isKillerOrCounter =
        move == stats.killers[safePly][0] ||
        move == stats.killers[safePly][1] ||
        (counterMove != chess::Move::NO_MOVE &&
         move == counterMove);

    int historyScore =
        stats.history[side]
                    [move.from().index()]
                    [move.to().index()];

    board.makeMove(move);

    bool givesCheck = board.inCheck();

    int reduction = 0;
    int score;

    if (firstMove)
    {
        score = -negamax(
            board,
            searchDepth - 1,
            -beta,
            -alpha,
            stats,
            ply + 1,
            move,
            true
        );

        firstMove = false;
    }
    else
    {
        reduction =
            search::ordering::lateMoveReduction(
                searchDepth,
                moveIndex,
                isPV,
                isCapture,
                isPromotion,
                inCheck,
                givesCheck,
                isKillerOrCounter,
                historyScore
            );
            if (reduction > 0)
        ++stats.lmrReductions;
        score = -negamax(
            board,
            searchDepth - 1 -reduction ,
            -alpha - 1,
            -alpha,
            stats,
            ply + 1,
            move,
            true
        );

        if (score > alpha &&
            (reduction > 0 || isPV))
        {
            if (reduction > 0)
    ++stats.lmrResearches;
            score = -negamax(
                board,
                searchDepth - 1,
                -beta,
                -alpha,
                stats,
                ply + 1,
                move,
                true
            );
        }
    }

    board.unmakeMove(move);

    if (stats.stop)
        break;

    if (score > bestScore)
    {
        bestScore = score;
        bestMove = move;
    }

    if (score > alpha)
        alpha = score;

    if (isQuiet &&
        alpha < beta &&
        quietCount < MAX_QUIETS)
    {
        quietsTried[quietCount++] = move;
    }

    if (alpha >= beta)
    {
        if (isQuiet)
        {
            int safePly =
                std::min(ply, MAX_PLY - 1);

            stats.killers[safePly][1] =
                stats.killers[safePly][0];

            stats.killers[safePly][0] =
                move;

            int& h =
                stats.history[side]
                            [move.from().index()]
                            [move.to().index()];

            h += depth * depth;

            if (h > 30000)
                h = 30000;

            for (int i = 0; i < quietCount; ++i)
            {
                const chess::Move& qm =
                    quietsTried[i];

                int& hq =
                    stats.history[side]
                                [qm.from().index()]
                                [qm.to().index()];

                hq -= depth * depth / 2;

                if (hq < -30000)
                    hq = -30000;
            }
        }

        break;
    }

    ++moveIndex;
}

    tt::Bound bound = tt::Bound::EXACT;

if (bestScore <= alphaOriginal)
    bound = tt::Bound::UPPERBOUND;
else if (bestScore >= beta)
    bound = tt::Bound::LOWERBOUND;
if (stats.stop)
    return 0;
stats.table.store(
    key,
    depth,
    tt::valueToTT(bestScore, ply),
    bound,
    bestMove
);

return bestScore;
}


// 
chess::Move findBestMove(
    
    chess::Board& board,
    int maxDepth,
    SearchStats& stats,const SearchLimits& limits)
{
    stats.startTime =
    std::chrono::steady_clock::now();

    stats.stop = false;

    TimeBudget budget =
    computeTimeBudget(limits, -1);

stats.optimalMs = budget.optimalMs;
stats.maximumMs = budget.maximumMs;
    chess::Move bestMove = chess::Move::NO_MOVE;
    int bestScore = -INF;

    constexpr int ASPIRATION_WINDOW = 50;

    // Root legal-move check
    chess::Movelist rootMoves;
    chess::movegen::legalmoves(rootMoves, board);

    if (rootMoves.empty())
        return chess::Move::NO_MOVE;

    for (int depth = 1; depth <= maxDepth; ++depth)
    {
        // Generate fresh root move list for this iteration.
        chess::Movelist moves;
        chess::movegen::legalmoves(moves, board);

        // Previous iteration's root TT move.
        uint64_t key = board.hash();

        tt::Entry* entry = stats.table.probe(key);

        chess::Move ttMove = chess::Move::NO_MOVE;

        if (entry != nullptr &&
            entry->bestMove != chess::Move::NO_MOVE)
        {
            ++stats.rootTTHits;
            ttMove = entry->bestMove;
        }

        // Put previous iteration's best move first.
        if (ttMove != chess::Move::NO_MOVE)
        {
            for (int i = 0;
                 i < static_cast<int>(moves.size());
                 ++i)
            {
                if (moves[i] == ttMove)
                {
                    if (i != 0)
                        std::swap(moves[0], moves[i]);

                    break;
                }
            }
        }

        // Depth 1 uses the full window.
        int alpha = -INF;
        int beta = INF;

        if (depth > 1)
        {
            alpha = bestScore - ASPIRATION_WINDOW;
            beta = bestScore + ASPIRATION_WINDOW;
        }

        chess::Move iterationBestMove = chess::Move::NO_MOVE;
        int iterationBestScore = -INF;

        while (true)
        {
            int windowAlpha = alpha;
            int windowBeta = beta;

            iterationBestMove = chess::Move::NO_MOVE;
            iterationBestScore = -INF;
            for (const auto& move : moves)
            {
                if (stats.stop)break;
                board.makeMove(move);

                int score = -negamax(
                    board,
                    depth - 1,
                    -windowBeta,
                    -windowAlpha,
                    stats,
                    1,
                    move,
                    true
                );

                board.unmakeMove(move);
                if (stats.stop){
                    break;
                    }
                if (score > iterationBestScore)
                {
                    iterationBestScore = score;
                    iterationBestMove = move;
                }

                if (score > windowAlpha)
                    windowAlpha = score;
            }
            if (stats.stop)break;

            // Fail-low: score is outside the lower bound.
            if (iterationBestScore <= alpha)
            {
                ++stats.aspirationFailLow;
                alpha = -INF;
                continue;
            }

            // Fail-high: score is outside the upper bound.
            if (iterationBestScore >= beta)
            {
                ++stats.aspirationFailHigh;
                beta = INF;
                continue;
            }

            // Score is inside the aspiration window.
            break;
        }
        if (stats.stop) break;
    
        // aspiration search finishes successfully

bool bestMoveChanged =
    bestMove != chess::Move::NO_MOVE &&
    iterationBestMove != bestMove;

bool scoreDropped =
    depth > 1 &&
    iterationBestScore < bestScore - 50;

stats.stable =
    !bestMoveChanged &&
    !scoreDropped;

bestMove = iterationBestMove;
bestScore = iterationBestScore;
int64_t softLimit = stats.optimalMs;

if (!stats.stable)
{
    softLimit = std::min(
        stats.maximumMs,
        stats.optimalMs * 2
    );
}
std::cout << "Stable: "
          << (stats.stable ? "yes" : "no")
          << '\n';

std::cout << "Soft limit: "
          << softLimit << '\n';

int64_t elapsed = elapsedMs(stats);

if (stats.optimalMs > 0 &&
    elapsed >= softLimit)
{
    break;
}
        // Store root result for the next iteration.
        stats.table.store(
            key,
            depth,
            tt::valueToTT(bestScore, 0),
            tt::Bound::EXACT,
            bestMove
        );

        std::cout << "Depth: " << depth << '\n';
        std::cout << "Score: " << bestScore << '\n';
        std::cout << "Best move: "
                  << bestMove.from().index()
                  << " -> "
                  << bestMove.to().index()
                  << '\n';
        std::cout << "Root TT hits: "
                  << stats.rootTTHits << '\n';
        std::cout << "----------------\n";
        std::cout << "Aspiration fail-low: "
          << stats.aspirationFailLow << '\n';

std::cout << "Aspiration fail-high: "
          << stats.aspirationFailHigh << '\n';
          
          std::cout << "LMR reductions: "
          << stats.lmrReductions << '\n';

std::cout << "LMR researches: "
          << stats.lmrResearches << '\n';
         
    }

    return bestMove;
}
}

