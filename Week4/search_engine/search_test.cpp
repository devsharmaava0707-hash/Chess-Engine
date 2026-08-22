#include "search.h"
#include "../chess.hpp"
#include <iostream>


int main()
{
    for (int depth = 4; depth <= 4; ++depth)
    {
        chess::Board board;
        search::SearchStats stats;

        chess::Move best =
            search::findBestMove(board, depth, stats);

        std::cout << "Depth: " << depth << '\n';
        std::cout << "Nodes: " << stats.nodes << '\n';

        if (best != chess::Move::NO_MOVE)
        {
            std::cout << "From: "
                      << best.from().index() << '\n';

            std::cout << "To:   "
                      << best.to().index() << '\n';
        }

        std::cout << "----------------\n";
    }

    return 0;
}