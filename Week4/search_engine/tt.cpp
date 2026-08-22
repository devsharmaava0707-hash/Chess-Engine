#include "tt.h"

namespace search::tt
{
    int valueToTT(int score, int ply)
{
    if (score > MATE_BOUND)
        return score + ply;

    if (score < -MATE_BOUND)
        return score - ply;

    return score;
}

int valueFromTT(int score, int ply)
{
    if (score > MATE_BOUND)
        return score - ply;

    if (score < -MATE_BOUND)
        return score + ply;

    return score;
}
    Table::Table(size_t megabytes)
    {
        size_t bytes = megabytes * 1024ULL * 1024ULL;

        size_t count =
            bytes / sizeof(Entry);

        if (count == 0)
            count = 1;

        entries.resize(count);
    }

    void Table::clear()
    {
        for (auto& entry : entries)
        {
            entry = Entry{};
        }
    }

    Entry* Table::probe(uint64_t key)
    {
        Entry& entry =
            entries[key % entries.size()];

        if (entry.key == key)
            return &entry;

        return nullptr;
    }

    void Table::store(uint64_t key,
                  int depth,
                  int score,
                  Bound bound,
                  chess::Move bestMove)
{
    Entry& entry =
        entries[key % entries.size()];

    // Replace if:
    // 1. slot is empty
    // 2. same position is already there
    // 3. new search is at least as deep
    if (entry.depth > depth &&
        entry.key != key)
    {
        return;
    }

    entry.key = key;
    entry.depth = static_cast<int8_t>(depth);
    entry.score = static_cast<int16_t>(score);
    entry.bound = bound;
    entry.bestMove = bestMove;
}
}