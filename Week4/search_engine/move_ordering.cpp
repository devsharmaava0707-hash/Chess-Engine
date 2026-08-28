#include "move_ordering.h"
#include "see.h"
#include<cmath>
#include<algorithm>
namespace search::ordering
{
    namespace
    {
        int moveScore(
            const chess::Board& board,
            const chess::Move& move,
            const chess::Move killers[2],
            const chess::Move& counterMove,
            const int history[64][64],const chess::Move& ttMove)
        {
            bool isCapture =
                move.typeOf() == chess::Move::ENPASSANT ||
                (move.typeOf() != chess::Move::CASTLING &&
                 board.at(move.to()) != chess::Piece::NONE);
            if (move == ttMove)return 200000;
            // 1. Captures
            // if (isCapture)
            // {
            //     return 100000 +
            //            search::see::evaluate(board, move);
            // }
            if (isCapture)
{
    int seeScore =
        search::see::evaluate(board, move);

    if (seeScore >= 0)
    {
        // Winning/equal captures stay above killers.
        return 100000 + seeScore;
    }

    // Losing captures go below quiet heuristics.
    return -100000 + seeScore;
}

            // 2. Killer 1
            if (move == killers[0])
                return 90000;

            // 3. Killer 2
            if (move == killers[1])
                return 80000;

            // 4. Countermove
            if (counterMove != chess::Move::NO_MOVE &&
                move == counterMove)
            {
                return 70000;
            }

            // 5. History
            return history[
                move.from().index()
            ][
                move.to().index()
            ];
        }
    }

    void orderMoves(
        const chess::Board& board,
        chess::Movelist& moves,
        const chess::Move killers[2],
        const chess::Move& counterMove,
        const int history[64][64],
        const chess:: Move& ttMove)
    {
        for (int i = 0;
             i < static_cast<int>(moves.size());
             ++i)
        {
            int bestIndex = i;

            int bestScore =
                moveScore(
                    board,
                    moves[i],
                    killers,
                    counterMove,
                    history,
                    ttMove
                );

            for (int j = i + 1;
                 j < static_cast<int>(moves.size());
                 ++j)
            {
                int score =
                    moveScore(
                        board,
                        moves[j],
                        killers,
                        counterMove,
                        history,
                        ttMove
                    );

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
    int lateMoveReduction(
    int depth,
    int moveIndex,
    bool isPV,
    bool isCapture,
    bool isPromotion,
    bool inCheck,
    bool givesCheck,
    bool isKillerOrCounter,
    int historyScore)
{
    if (depth < 2 ||
        moveIndex < 1 ||
        isCapture ||
        isPromotion ||
        inCheck ||
        givesCheck)
    {
        return 0;
    }

    double lr =
        0.4 +
        std::log(static_cast<double>(depth)) *
        std::log(static_cast<double>(moveIndex + 1)) /
        2.0;

    int reduction =
        static_cast<int>(lr);

    if (isPV && reduction > 0)
        --reduction;

    if (isKillerOrCounter)
    {
        if (reduction > 0)
            --reduction;
    }
    else if (historyScore < -4000)
    {
        ++reduction;
    }

    int maxReduction =
        std::max(0, depth - 2);

    return std::max(
        0,
        std::min(reduction, maxReduction)
    );
}
}
