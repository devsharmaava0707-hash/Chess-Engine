#pragma once

#include "../chess.hpp"

namespace search::ordering
{
    void orderMoves(
        const chess::Board& board,
        chess::Movelist& moves,
        const chess::Move killers[2],
        const chess::Move& counterMove,
        const int history[64][64],
        const chess::Move& ttMove
    );
}