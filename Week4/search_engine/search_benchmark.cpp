#include "search.h"
#include "../chess.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================
// Suppress search.cpp debug output during benchmarking
// ============================================================

class NullBuffer : public std::streambuf
{
protected:
    int overflow(int c) override
    {
        return c;
    }
};

// ============================================================
// Benchmark position
// ============================================================

struct BenchmarkPosition
{
    std::string fen;
    std::string referenceMove;
    std::string category;
};

// ============================================================
// Simple CSV parser
// ============================================================

static std::vector<std::string>
splitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string cur;
    bool inQuotes = false;

    for (char c : line)
    {
        if (c == '"')
        {
            inQuotes = !inQuotes;
        }
        else if (c == ',' && !inQuotes)
        {
            fields.push_back(cur);
            cur.clear();
        }
        else
        {
            cur += c;
        }
    }

    fields.push_back(cur);
    return fields;
}

// ============================================================
// Load benchmark CSV
// ============================================================

static bool loadBenchmark(
    const std::string& path,
    std::vector<BenchmarkPosition>& out)
{
    std::ifstream file(path);

    if (!file)
        return false;

    std::string line;

    if (!std::getline(file, line))
        return false;

    auto header = splitCsvLine(line);

    int fenCol = -1;
    int moveCol = -1;
    int categoryCol = -1;

    for (int i = 0;
         i < static_cast<int>(header.size());
         ++i)
    {
        if (header[i] == "fen")
            fenCol = i;

        if (header[i] == "playing")
            moveCol = i;

        if (header[i] == "category")
            categoryCol = i;
    }

    if (fenCol < 0 || moveCol < 0)
        return false;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        auto fields = splitCsvLine(line);

        if (fenCol >= static_cast<int>(fields.size()) ||
            moveCol >= static_cast<int>(fields.size()))
        {
            continue;
        }

        BenchmarkPosition p;

        p.fen = fields[fenCol];
        p.referenceMove = fields[moveCol];

        if (categoryCol >= 0 &&
            categoryCol < static_cast<int>(fields.size()))
        {
            p.category = fields[categoryCol];
        }
        else
        {
            p.category = "unknown";
        }

        out.push_back(std::move(p));
    }

    return true;
}

// ============================================================
// Convert chess::Move to UCI notation
// ============================================================

static std::string moveToUci(const chess::Move& move)
{
    if (move == chess::Move::NO_MOVE)
        return "0000";

    std::string s;
    s.reserve(5);

    auto squareName = [](int sq)
    {
        std::string r(2, 'a');

        r[0] =
            static_cast<char>(
                'a' + (sq % 8)
            );

        r[1] =
            static_cast<char>(
                '1' + (sq / 8)
            );

        return r;
    };

    s += squareName(move.from().index());
    s += squareName(move.to().index());

    if (move.typeOf() == chess::Move::PROMOTION)
    {
        switch (
            static_cast<int>(
                move.promotionType()
            )
        )
        {
        case static_cast<int>(
            chess::PieceType::KNIGHT):
            s += 'n';
            break;

        case static_cast<int>(
            chess::PieceType::BISHOP):
            s += 'b';
            break;

        case static_cast<int>(
            chess::PieceType::ROOK):
            s += 'r';
            break;

        case static_cast<int>(
            chess::PieceType::QUEEN):
            s += 'q';
            break;

        default:
            break;
        }
    }

    return s;
}

// ============================================================
// Category index
// ============================================================

static int categoryIndex(const std::string& c)
{
    if (c == "endgame")
        return 0;

    if (c == "middlegame")
        return 1;

    if (c == "other")
        return 2;

    return 3;
}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv)
{
    const std::string csvPath =
        argc > 1
            ? argv[1]
            : "benchmark_500.csv";

    const int depth =
        argc > 2
            ? std::max(
                  1,
                  std::stoi(argv[2])
              )
            : 8;

    std::vector<BenchmarkPosition> tests;

    if (!loadBenchmark(csvPath, tests))
    {
        std::cerr
            << "Could not load benchmark file: "
            << csvPath
            << '\n';

        return 1;
    }

    std::cout
        << "Loaded "
        << tests.size()
        << " benchmark positions\n";

    std::cout
        << "Depth: "
        << depth
        << "\n\n";

    // --------------------------------------------------------
    // Overall statistics
    // --------------------------------------------------------

    std::size_t moveMatches = 0;
    std::size_t moveMismatches = 0;

    std::uint64_t totalNodes = 0;
    std::uint64_t totalMs = 0;

    std::uint64_t totalLmrReductions = 0;
    std::uint64_t totalLmrResearches = 0;

    // --------------------------------------------------------
    // Category statistics
    // --------------------------------------------------------

    std::uint64_t categoryNodes[4]{};
    std::uint64_t categoryMs[4]{};
    std::size_t categoryCount[4]{};

    const char* categoryNames[] =
    {
        "endgame",
        "middlegame",
        "other",
        "unknown"
    };

    // --------------------------------------------------------
    // Run positions
    // --------------------------------------------------------

    for (std::size_t i = 0;
         i < tests.size();
         ++i)
    {
        const auto& test = tests[i];

        chess::Board board;

        board.setFen(test.fen);

        search::SearchStats stats;

        search::SearchLimits limits;
        limits.infinite = true;

        const auto start =
            std::chrono::steady_clock::now();

        // Suppress search.cpp's verbose debug output.
        NullBuffer nullBuffer;

        std::streambuf* oldCout =
            std::cout.rdbuf(&nullBuffer);

        chess::Move best =
            search::findBestMove(
                board,
                depth,
                stats,
                limits
            );

        std::cout.rdbuf(oldCout);

        const auto elapsed =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(
                std::chrono::steady_clock::now()
                - start
            ).count();

        const std::string bestUci =
            moveToUci(best);

        const bool matches =
            bestUci == test.referenceMove;

        if (matches)
            ++moveMatches;
        else
            ++moveMismatches;

        totalNodes += stats.nodes;

        totalMs +=
            static_cast<std::uint64_t>(
                elapsed
            );

        totalLmrReductions +=
            stats.lmrReductions;

        totalLmrResearches +=
            stats.lmrResearches;

        const int ci =
            categoryIndex(test.category);

        categoryNodes[ci] += stats.nodes;

        categoryMs[ci] +=
            static_cast<std::uint64_t>(
                elapsed
            );

        categoryCount[ci]++;

        // ----------------------------------------------------
        // Print only every 10th position and every mismatch
        // ----------------------------------------------------

        if ((i + 1) % 10 == 0 ||
            !matches)
        {
            std::cout
                << '['
                << (i + 1)
                << '/'
                << tests.size()
                << "] "
                << test.category
                << " | engine="
                << bestUci
                << " | reference="
                << test.referenceMove
                << " | "
                << (matches
                        ? "MATCH"
                        : "DIFF")
                << " | nodes="
                << stats.nodes
                << " | ms="
                << elapsed
                << '\n';
        }
    }

    // ========================================================
    // Summary
    // ========================================================

    const double avgNodes =
        tests.empty()
            ? 0.0
            : static_cast<double>(
                  totalNodes
              ) /
              tests.size();

    const double avgMs =
        tests.empty()
            ? 0.0
            : static_cast<double>(
                  totalMs
              ) /
              tests.size();

    const double overallNps =
        totalMs == 0
            ? 0.0
            : static_cast<double>(
                  totalNodes
              ) *
              1000.0 /
              static_cast<double>(
                  totalMs
              );

    const double matchRate =
        tests.empty()
            ? 0.0
            : 100.0 *
              static_cast<double>(
                  moveMatches
              ) /
              static_cast<double>(
                  tests.size()
              );

    std::cout
        << "\n========================================\n";

    std::cout
        << "           BENCHMARK SUMMARY\n";

    std::cout
        << "========================================\n";

    std::cout
        << "Positions: "
        << tests.size()
        << '\n';

    std::cout
        << "Depth: "
        << depth
        << '\n';

    std::cout
        << "Move matches: "
        << moveMatches
        << '\n';

    std::cout
        << "Move mismatches: "
        << moveMismatches
        << '\n';

    std::cout
        << std::fixed
        << std::setprecision(2);

    std::cout
        << "Match rate: "
        << matchRate
        << "%\n";

    std::cout
        << "Average nodes/position: "
        << avgNodes
        << '\n';

    std::cout
        << "Average ms/position: "
        << avgMs
        << '\n';

    std::cout
        << "Overall NPS: "
        << overallNps
        << '\n';

    std::cout
        << "LMR reductions: "
        << totalLmrReductions
        << '\n';

    std::cout
        << "LMR researches: "
        << totalLmrResearches
        << '\n';

    std::cout
        << "\n---------- Categories ----------\n";

    for (int i = 0; i < 4; ++i)
    {
        if (categoryCount[i] == 0)
            continue;

        const double avgCatNodes =
            static_cast<double>(
                categoryNodes[i]
            ) /
            categoryCount[i];

        const double avgCatMs =
            static_cast<double>(
                categoryMs[i]
            ) /
            categoryCount[i];

        std::cout
            << categoryNames[i]
            << " | count="
            << categoryCount[i]
            << " | avg nodes="
            << avgCatNodes
            << " | avg ms="
            << avgCatMs
            << '\n';
    }

    std::cout << "========================================\n";

    return 0;
}