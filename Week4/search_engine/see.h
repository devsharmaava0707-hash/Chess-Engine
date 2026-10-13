#pragma once

#include "../chess.hpp"

namespace search::see
{
    int evaluate(const chess::Board& board,const chess::Move& move);
    bool see_ge(
        const chess::Board& board,
        const chess::Move& move,
        int threshold
    );
}