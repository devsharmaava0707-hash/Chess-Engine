#include "move_ordering.h"
#include "see.h"
#include<cmath>
#include<algorithm>
namespace search::ordering
{
    namespace
    {
        int pieceIndexForMove(
    const chess::Board& board,
    const chess::Move& move)
{
    const chess::Piece piece =
        board.at(move.from());

    const int type =
        static_cast<int>(
            piece.type().internal()
        );

    if (piece.color() == chess::Color::NONE ||
        type < 0 ||
        type >= 6)
    {
        return -1;
    }

    return
        (piece.color() == chess::Color::WHITE ? 0 : 6)
        + type;
}
int capturedTypeForMove(
    const chess::Board& board,
    const chess::Move& move)
{
    if (move.typeOf() == chess::Move::ENPASSANT)
        return static_cast<int>(
            chess::PieceType::PAWN
        );

    const chess::Piece captured =
        board.at(move.to());

    if (captured == chess::Piece::NONE)
        return -1;

    const int type =
        static_cast<int>(
            captured.type().internal()
        );

    if (type < 0 || type >= 6)
        return -1;

    return type;
}
        int moveScore(
    const chess::Board& board,
    const chess::Move& move,
    const chess::Move killers[2],
    const chess::Move& counterMove,
    const int history[64][64],
    const int continuationHistory[12][64][12][64],
    const int captureHistory[12][6][64],
    int previousPiece,
    int previousTo,
    const chess::Move& ttMove)
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
    const int seeScore =
        search::see::evaluate(board, move);

    const int movingPiece =
        pieceIndexForMove(board, move);

    const int capturedType =
        capturedTypeForMove(board, move);

    int captureHistoryScore = 0;

    if (movingPiece >= 0 &&
        movingPiece < 12 &&
        capturedType >= 0 &&
        capturedType < 6)
    {
        captureHistoryScore =
            captureHistory
                [movingPiece]
                [capturedType]
                [move.to().index()];
    }

    if (seeScore >= 0)
    {
        // SEE remains the primary capture signal.
        return 100000 +
               seeScore +
               captureHistoryScore / 16;
    }

    // Losing captures stay below quiet heuristics.
    return -100000 +
           seeScore +
           captureHistoryScore / 16;
}  
                bool isPromotion =
                move.typeOf() == chess::Move::PROMOTION;

            if (isPromotion)
            {
                // Queen promotions are almost always strong;
                // under-promotions are rare/tactical, still worth
                // ranking above ordinary quiets.
                if (move.promotionType() == chess::PieceType::QUEEN)
                    return 95000;

                return 60000;
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
//             int historyScore =
//     history[
//         move.from().index()
//     ][
//         move.to().index()
//     ];

// int continuationScore =
//     continuationHistory[
//         move.to().index()
//     ][
//         move.from().index()
//     ][
//         move.to().index()
//     ];

// return historyScore + continuationScore;
// return history[
//     move.from().index()
// ][
//     move.to().index()
// ];
int historyScore =
    history[
        move.from().index()
    ][
        move.to().index()
    ];

int continuationScore = 0;

if (previousPiece >= 0 &&
    previousPiece < 12 &&
    previousTo >= 0 &&
    previousTo < 64)
{
    const int currentPiece =
        pieceIndexForMove(board, move);

    if (currentPiece >= 0)
    {
        continuationScore =
            continuationHistory
                [previousPiece]
                [previousTo]
                [currentPiece]
                [move.to().index()];
    }
}

return historyScore + continuationScore / 2;
        }
    }

//     void orderMoves(
//     const chess::Board& board,
//     chess::Movelist& moves,
//     const chess::Move killers[2],
//     const chess::Move& counterMove,
//     const int history[64][64],
//     const chess::Move& ttMove)
// {
//     const int n = static_cast<int>(moves.size());

//     std::vector<int> scores(n);
//     for (int i = 0; i < n; ++i)
//     {
//         scores[i] = moveScore(
//             board, moves[i], killers, counterMove, history, ttMove
//         );
//     }

//     std::vector<int> order(n);
//     for (int i = 0; i < n; ++i) order[i] = i;

//     std::stable_sort(order.begin(), order.end(),
//         [&scores](int a, int b) { return scores[a] > scores[b]; });

//     chess::Movelist sorted;
//     for (int i = 0; i < n; ++i)
//         sorted.add(moves[order[i]]);

//     for (int i = 0; i < n; ++i)
//         moves[i] = sorted[i];
// }
    void orderMoves(
    const chess::Board& board,
    chess::Movelist& moves,
    const chess::Move killers[2],
    const chess::Move& counterMove,
    const int history[64][64],
    const int continuationHistory[12][64][12][64],
    const int captureHistory[12][6][64],
    int previousPiece,
    int previousTo,
    const chess::Move& ttMove)
{
    const int n = static_cast<int>(moves.size());

    static thread_local int scores[256];
    static thread_local int order[256];

    for (int i = 0; i < n; ++i)
    {
        scores[i] = moveScore(
    board,
    moves[i],
    killers,
    counterMove,
    history,
    continuationHistory,
    captureHistory,
    previousPiece,
    previousTo,
    ttMove
);
        order[i] = i;
    }

    std::stable_sort(order, order + n,
        [](int a, int b) { return scores[a] > scores[b]; });

    // Apply permutation in place.
    std::vector<bool> placed(n, false);
    for (int i = 0; i < n; ++i)
    {
        if (placed[i] || order[i] == i)
            continue;

        chess::Move temp = moves[i];
        int j = i;

        while (!placed[j])
        {
            placed[j] = true;
            int next = order[j];

            if (next == i)
            {
                moves[j] = temp;
                break;
            }

            moves[j] = moves[next];
            j = next;
        }
    }
}   
    int lateMoveReduction(
    int depth,
    int moveIndex,
    bool isPV,
    bool improving,
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
    if (!improving)
    ++reduction;
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
