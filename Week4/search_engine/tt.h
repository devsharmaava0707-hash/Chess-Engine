#pragma once

#include "../chess.hpp"
#include <cstdint>
#include <vector>

namespace search::tt
{   

    constexpr int MATE_SCORE = 31000;
    constexpr int MATE_BOUND = 30000;

    int valueToTT(int score, int ply);
    int valueFromTT(int score, int ply);
    enum class Bound : uint8_t
    {
        EXACT,
        LOWERBOUND,
        UPPERBOUND
    };

    struct Entry
    {
        uint64_t key = 0;
        int16_t score = 0;
        int8_t depth = -1;

        Bound bound = Bound::EXACT;

        chess::Move bestMove = chess::Move::NO_MOVE;
    };

    class Table
    {
    public:
        explicit Table(size_t megabytes = 16);

        void clear();

        Entry* probe(uint64_t key);

        void store(uint64_t key,
                   int depth,
                   int score,
                   Bound bound,
                   chess::Move bestMove);

    private:
        std::vector<Entry> entries;
    };
}