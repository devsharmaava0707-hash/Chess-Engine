#include "move_ordering.h"
#include "see.h"

#include <algorithm>

namespace search::ordering
{
    namespace
    {
        int moveScore(
            const chess::Board& board,
            const chess::Move& move, const chess::Move killers[2],const int history[64][64])
        {
            // Killer 1
            if (move == killers[0])
                return 90000;

            // Killer 2
            if (move == killers[1])
                return 80000;
            bool isCapture =
                move.typeOf() == chess::Move::ENPASSANT ||
                (move.typeOf() != chess::Move::CASTLING &&
                 board.at(move.to()) != chess::Piece::NONE);

            if (!isCapture)
                return 0;

            return 100000 +
                   search::see::evaluate(board, move);
             // Then history for quiet moves.
        return history[move.from().index()][move.to().index()];
        }
    }

    void orderMoves(
        const chess::Board& board,
        chess::Movelist& moves,const chess::Move killers[2],const int history[64][64])
    {
        for (int i = 0;
             i < static_cast<int>(moves.size());
             ++i)
        {
            int bestIndex = i;
            int bestScore = moveScore(board, moves[i],killers,history);

            for (int j = i + 1;
                 j < static_cast<int>(moves.size());
                 ++j)
            {
                int score = moveScore(board, moves[j],killers,history);

                if (score > bestScore)
                {
                    bestScore = score;
                    bestIndex = j;
                }
            }

            if (bestIndex != i)
                std::swap(moves[i], moves[bestIndex]);
        }
    }
}