// #include "search.h"
// #include "../chess.hpp"
// #include <iostream>

// int main()
// {
//     std::cout << "TEST START" << std::endl;
//     search::SearchLimits limits;
//     limits.myTimeMs = 30000;      // 30 seconds left
//     limits.incrementMs = 500;     // 0.5 sec increment
//     limits.movesToGo = 30;
//     chess::Board board;
//     search::SearchStats stats;

//     chess::Move best =
//         search::findBestMove(board, 20, stats,limits);

//     std::cout << "\nFinal best move:\n";
//     std::cout << "From: " << best.from().index() << '\n';
//     std::cout << "To:   " << best.to().index() << '\n';

//     std::cout << "Total nodes: "
//               << stats.nodes << '\n';

//     std::cout << "Elapsed ms: "
//               << search::elapsedMs(stats) << '\n';

//     std::cout << "Stopped: "
//               << (stats.stop ? "yes" : "no") << '\n';
//     std::cout << "Optimal ms: "
//           << stats.optimalMs << '\n';

// std::cout << "Maximum ms: "
//           << stats.maximumMs << '\n';

//     return 0;
// }
#include "search.h"
#include "../chess.hpp"
#include <iostream>

void printResult(
    const char* testName,
    const search::SearchStats& stats,
    const chess::Move& best)
{
    std::cout << "\n========== "
              << testName
              << " ==========\n";

    std::cout << "Best move: "
              << best.from().index()
              << " -> "
              << best.to().index()
              << '\n';

    std::cout << "Nodes: "
              << stats.nodes
              << '\n';

    std::cout << "Elapsed ms: "
              << search::elapsedMs(stats)
              << '\n';

    std::cout << "Stopped: "
              << (stats.stop ? "yes" : "no")
              << '\n';

    std::cout << "Optimal ms: "
              << stats.optimalMs
              << '\n';

    std::cout << "Maximum ms: "
              << stats.maximumMs
              << '\n';
}

int main()
{
    std::cout << "TIME MANAGEMENT TEST\n";

    // =========================================================
    // TEST 1: Fixed move time
    // =========================================================
    {
        chess::Board board;
        search::SearchStats stats;

        search::SearchLimits limits;
        limits.moveTimeMs = 1000;

        chess::Move best =
            search::findBestMove(
                board,
                20,
                stats,
                limits
            );

        printResult(
            "MOVETIME 1000 ms",
            stats,
            best
        );
    }

    // =========================================================
    // TEST 2: Clock + increment + moves to go
    // =========================================================
    {
        chess::Board board;
        search::SearchStats stats;

        search::SearchLimits limits;
        limits.myTimeMs = 30000;
        limits.incrementMs = 500;
        limits.movesToGo = 30;

        chess::Move best =
            search::findBestMove(
                board,
                20,
                stats,
                limits
            );

        printResult(
            "CLOCK 30s + 500ms INC + 30 MTG",
            stats,
            best
        );
    }

    // =========================================================
    // TEST 3: Infinite
    // =========================================================
    {
        chess::Board board;
        search::SearchStats stats;

        search::SearchLimits limits;
        limits.infinite = true;

        chess::Move best =
            search::findBestMove(
                board,
                5,
                stats,
                limits
            );

        printResult(
            "INFINITE",
            stats,
            best
        );
    }

    return 0;
}