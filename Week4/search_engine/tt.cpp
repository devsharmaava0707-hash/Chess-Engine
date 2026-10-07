#include "tt.h"
#include<algorithm>
namespace search::tt
{
    void Table::newSearch()
{
    ++currentGeneration;

    if (currentGeneration == 0)
        ++currentGeneration;
}

uint8_t Table::generation() const
{
    return currentGeneration;
}
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
//     Table::Table(size_t megabytes)
// {
//     size_t bytes =
//         megabytes * 1024ULL * 1024ULL;

//     size_t count =
//         bytes / sizeof(Entry);

//     if (count == 0)
//         count = 1;

//     // Round down to a power of two so that
//     // key & (count - 1) can be used instead
//     // of the much slower modulo operation.
//     size_t powerOfTwo = 1;

//     while ((powerOfTwo << 1) <= count)
//         powerOfTwo <<= 1;

//     entries.resize(powerOfTwo);
// }
    Table::Table(size_t megabytes)
{
    resize(megabytes);
}

    void Table::resize(size_t megabytes)
{
    size_t bytes =
        megabytes * 1024ULL * 1024ULL;

    size_t count =
        bytes / sizeof(Entry);

    if (count == 0)
        count = 1;

    size_t powerOfTwo = 1;

    while ((powerOfTwo << 1) <= count)
        powerOfTwo <<= 1;

    entries.assign(powerOfTwo, Entry{});
}

    // void Table::clear()
    // {
    //     for (auto& entry : entries)
    //     {
    //         entry = Entry{};
    //     }
    // }

    int Table::hashfull() const
    {
        constexpr size_t SAMPLE_SIZE = 1000;

        size_t sampleCount =
            std::min(SAMPLE_SIZE, entries.size());

        if (sampleCount == 0)
            return 0;

        size_t filled = 0;

        for (size_t i = 0; i < sampleCount; ++i)
        {
            if (entries[i].key != 0 &&
                entries[i].generation == currentGeneration)
            {
                ++filled;
            }
        }

        return static_cast<int>(
            filled * 1000 / sampleCount
        );
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
            entries[key & (entries.size()-1)];

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
    Entry& e = entries[key & (entries.size() - 1)];

    const bool same = (e.key == key);

    if (!same &&
        e.key &&
        e.generation == currentGeneration &&
        e.depth > depth)
    {
        return;
    }

    if (same &&
        e.generation == currentGeneration &&
        depth < e.depth &&
        bound != Bound::EXACT)
    {
        if (e.bestMove == chess::Move::NO_MOVE)
            e.bestMove = bestMove;

        return;
    }

    if (bestMove != chess::Move::NO_MOVE || !same)
        e.bestMove = bestMove;

    e.key = key;
    e.score = static_cast<int16_t>(score);
    e.depth = static_cast<int8_t>(depth);
    e.bound = bound;
    e.generation = currentGeneration;
}
}