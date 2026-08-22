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
int negamax(chess::Board& board, int depth, int alpha, int beta,SearchStats &stats,int ply)
{
    ++stats.nodes;
    if (depth <= 0)return quiescence(board,alpha,beta,stats);
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    int safePly = std::min(ply, MAX_PLY - 1);
    int side = static_cast<int>(board.sideToMove());
    search::ordering::orderMoves(board, moves,stats.killers[safePly],stats.history[side]);
    if (moves.empty())
    {
        if (board.inCheck())
            return -INF + 1;

        return 0;
    }

    int bestScore = -INF;
    constexpr int MAX_QUIETS = 256;
    chess::Move quietsTried[MAX_QUIETS];
    int quietCount = 0;
    for (const auto& move : moves)
    {
        board.makeMove(move);

        int score = -negamax(
            board,
            depth - 1,
            -beta,
            -alpha,
            stats,ply+1
        );

        board.unmakeMove(move);

        if (score > bestScore)
            bestScore = score;

        if (score > alpha)
            alpha = score;
        if (isQuiet && quietCount < MAX_QUIETS) quietsTried[quietCount++] = move;
        if (alpha >= beta)
        {
            bool isCapture =
                move.typeOf() == chess::Move::ENPASSANT ||
                (move.typeOf() != chess::Move::CASTLING &&
                board.at(move.to()) != chess::Piece::NONE);

            if (!isCapture &&
                move.typeOf() != chess::Move::PROMOTION)
            {
                int safePly = std::min(ply, MAX_PLY - 1);

                stats.killers[safePly][1] =
                stats.killers[safePly][0];
                stats.killers[safePly][0] = move;
                int side = static_cast<int>(board.sideToMove());
                int &h =stats.history[side][move.from().index()][move.to().index()];
                h += depth * depth;
                if (h > 30000)
                h = 30000;
            }

                break;
            }               
    }

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
            stats,1
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
    return bestMove;
}
}

