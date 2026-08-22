#include "search.h"
#include<iostream>
#include "../manual_evaluation_chess/eval.hpp"
#include "see.h"
#include "move_ordering.h"
namespace search
{

int evaluateForSideToMove(chess::Board& board)
{
    int score = evaluate(board);

    return board.sideToMove() == chess::Color::WHITE
        ? score
        : -score;
}
int quiescence(chess::Board& board,int alpha,int beta,SearchStats& stats,int qply){
    ++stats.nodes;

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

    if (score >= beta)
        return beta;

    if (score > alpha)
        alpha = score;
    }

    return alpha;
}

    int negamax(chess::Board& board,int depth,int alpha,int beta,SearchStats& stats,int ply,chess:: Move prevMove)
{
    ++stats.nodes;
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

    if (depth <= 0)
        return quiescence(board, alpha, beta, stats);

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

    for (const auto& move : moves)
    {
        bool isCapture =
            move.typeOf() == chess::Move::ENPASSANT ||
            (move.typeOf() != chess::Move::CASTLING &&
             board.at(move.to()) != chess::Piece::NONE);

        bool isQuiet =
            !isCapture &&
            move.typeOf() != chess::Move::PROMOTION;

        board.makeMove(move);

        int score = -negamax(
            board,
            depth - 1,
            -beta,
            -alpha,
            stats,
            ply + 1,move
        );

        board.unmakeMove(move);

        if (score > bestScore) { bestScore = score;bestMove = move;}
            // bestScore = score;bestMove = move;

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

                int side =
                    static_cast<int>(board.sideToMove());

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
    }

    tt::Bound bound = tt::Bound::EXACT;

if (bestScore <= alphaOriginal)
    bound = tt::Bound::UPPERBOUND;
else if (bestScore >= beta)
    bound = tt::Bound::LOWERBOUND;

stats.table.store(
    key,
    depth,
    tt::valueToTT(bestScore, ply),
    bound,
    bestMove
);

return bestScore;
}



chess::Move findBestMove(chess::Board& board, int depth,SearchStats &stats)
{
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    chess::Move bestMove = chess::Move::NO_MOVE;
    int bestScore = -INF;
    int alpha = -INF;
    int beta = INF;

    for (const auto& move : moves)
    {
        board.makeMove(move);

        int score = -negamax(
            board,
            depth - 1,
            -beta,
            -alpha,
            stats,1,chess::Move::NO_MOVE
        );

        board.unmakeMove(move);

        if (score > bestScore)
        {
            bestScore = score;
            bestMove = move;
        }

        if (score > alpha)
            alpha = score;
    }
    std::cout << "Best score: " << bestScore << '\n';
    std::cout << "TT hits: " << stats.ttHits << '\n';
    std::cout << "TT cutoffs: " << stats.ttCutoffs << '\n';

return bestMove;
    return bestMove;
}
}

