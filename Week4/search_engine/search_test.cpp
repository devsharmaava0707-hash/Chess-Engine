
#include "search.h"
#include "../chess.hpp"
#include <iostream>

int main()
{
    std::cout << "LMR OFF BASELINE TEST\n";

    chess::Board board;
    search::SearchStats stats;

    search::SearchLimits limits;
    limits.infinite = true;

    chess::Move best =
        search::findBestMove(
            board,
            10,
            stats,
            limits
        );

    std::cout << "\nFinal best move:\n";

    std::cout << "From: "
              << best.from().index()
              << '\n';

    std::cout << "To:   "
              << best.to().index()
              << '\n';

    std::cout << "Total nodes: "
              << stats.nodes
              << '\n';

    return 0;
}