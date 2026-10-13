// #include "see.h"
// #include "search_engine/see.h"
#include "see.h"
#include <array>
#include <iostream>
#include <string>

struct TestStats
{
    long long captures = 0;
    long long comparisons = 0;
    long long mismatches = 0;
};

void testPosition(chess::Board& board, int depth, TestStats& stats)
{
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    for (const auto& move : moves)
    {
        const bool isCapture =
            board.at(move.to()) != chess::Piece::NONE ||
            move.typeOf() == chess::Move::ENPASSANT;

        if (isCapture)
        {
            ++stats.captures;

            const int score =
                search::see::evaluate(board, move);

            const std::array<int, 12> thresholds = {
                -900, -500, -320, -100, -90, -50,
                   0,   50,  100,  320,  500, 900
            };

            for (int threshold : thresholds)
            {
                const bool expected = score >= threshold;
                const bool actual =
                    search::see::see_ge(board, move, threshold);

                ++stats.comparisons;

                if (actual != expected)
                {
                    ++stats.mismatches;

                    if (stats.mismatches <= 20)
                    {
                        std::cout
                            << "Mismatch: score=" << score
                            << ", threshold=" << threshold
                            << ", expected=" << expected
                            << ", actual=" << actual
                            << '\n';
                    }
                }
            }

            // Test the exact score boundary too.
            for (int threshold : {score, score + 1})
            {
                const bool expected = score >= threshold;
                const bool actual =
                    search::see::see_ge(board, move, threshold);

                ++stats.comparisons;

                if (actual != expected)
                {
                    ++stats.mismatches;
                }
            }
        }

        if (depth > 0)
        {
            board.makeMove(move);
            testPosition(board, depth - 1, stats);
            board.unmakeMove(move);
        }
    }
}

int main()
{
    const std::array<std::string, 3> positions = {
        // Starting position
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",

        // Pawn can capture a rook and promote
        "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1",

        // En passant is available
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"
    };

    TestStats stats;

    for (const auto& fen : positions)
    {
        chess::Board board(fen);
        testPosition(board, 3, stats);
    }

    std::cout << "\nCaptures tested: " << stats.captures
              << "\nComparisons: " << stats.comparisons
              << "\nMismatches: " << stats.mismatches
              << '\n';

    return stats.mismatches == 0 ? 0 : 1;
}