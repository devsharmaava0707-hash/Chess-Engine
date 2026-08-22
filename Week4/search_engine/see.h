#pragma once

#include "../chess.hpp"

namespace search::see
{
    int evaluate(const chess::Board& board,const chess::Move& move);
}