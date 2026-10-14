#pragma once

#include "../chess.hpp"

namespace search::ordering
{
   void scoreMoves(
    const chess::Board& board,
    chess::Movelist& moves,
    const chess::Move killers[2],
    const chess::Move& counterMove,
    const int history[64][64],
    const int continuationHistory[12][64][12][64],
    const int continuationHistory2[12][64][12][64],
    const int continuationHistory4[12][64][12][64],
    const int captureHistory[12][6][64],
    int previousPiece,
    int previousTo,
    int previous2Piece,
    int previous2To,
    int previous4Piece,
    int previous4To,
    const chess::Move& ttMove,
    int scores[256]
);

void pickNextMove(
    chess::Movelist& moves,
    int scores[256],
    int index
);
    int lateMoveReduction(
    int depth,
    int moveIndex,
    bool isPV,
    bool improving,
    bool cutNode,
    bool isCapture,
    bool isBadCapture,
    bool isPromotion,
    bool inCheck,
    bool givesCheck,
    bool isKillerOrCounter,
    bool ttMoveIsCapture,
    int historyScore,
    int continuationHistoryScore
);
}