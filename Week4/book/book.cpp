#include "book.h"
#include "zobrist_keys.hpp"
#include<fstream> // for stream or bascially to read files 
#include<algorithm>
#include<random>
using namespace chess;
/*
in my random postioon selector i have a issue it chooses mose frequent postiotion only not all position so yeah keep that in mind
*/
namespace book
{
    namespace
    {
        int pieceCode(Piece p)
        {
            int code = 0;

            switch (p.type().internal())
            {
                case PieceType::underlying::PAWN:
                    code = 0;
                    break;

                case PieceType::underlying::KNIGHT:
                    code = 2;
                    break;

                case PieceType::underlying::BISHOP:
                    code = 4;
                    break;

                case PieceType::underlying::ROOK:
                    code = 6;
                    break;

                case PieceType::underlying::QUEEN:
                    code = 8;
                    break;

                case PieceType::underlying::KING:
                    code = 10;
                    break;

                default:
                    return -1;
            }

            if (p.color() == Color::WHITE)
                code += 1;

            return code;
        }
    }

    uint64_t polyglotKey(const Board& board)
    {
        const auto& R = zobrist::POLYGLOT_RANDOM64;
        uint64_t key = 0;

        for (int sq = 0; sq < 64; ++sq)
        {
            Piece p = board.at(static_cast<Square>(sq));
            int code = pieceCode(p);

            if (code >= 0)
            {
                key ^= R[64 * code + sq];
            }
        }
        auto cr = board.castlingRights();

        if (cr.has(Color::WHITE, Board::CastlingRights::Side::KING_SIDE)) key ^= R[768];
        if (cr.has(Color::WHITE, Board::CastlingRights::Side::QUEEN_SIDE)) key ^= R[769];
        if (cr.has(Color::BLACK, Board::CastlingRights::Side::KING_SIDE)) key ^= R[770];
        if (cr.has(Color::BLACK, Board::CastlingRights::Side::QUEEN_SIDE)) key ^= R[771];

        Square epSq = board.enpassantSq();
        if (epSq != Square::NO_SQ){
            int file = epSq.index() % 8;
            key ^= R[772 + file];
        }
        if (board.sideToMove() == Color::WHITE) key ^= R[780];
        return key;
    }
    bool PolyglotBook::load(const std::string& path)
    {
        std::ifstream f(path, std::ios::binary);

        if (!f.is_open())
            return false;

        entries_.clear();

        uint8_t buf[16];

        while (f.read(reinterpret_cast<char*>(buf), 16))
        {
            PolyglotEntry e{};

            e.key = (uint64_t(buf[0]) << 56) |
                    (uint64_t(buf[1]) << 48) |
                    (uint64_t(buf[2]) << 40) |
                    (uint64_t(buf[3]) << 32) |
                    (uint64_t(buf[4]) << 24) |
                    (uint64_t(buf[5]) << 16) |
                    (uint64_t(buf[6]) << 8)  |
                    uint64_t(buf[7]);

            e.move = (uint16_t(buf[8]) << 8) |
                    uint16_t(buf[9]);

            e.weight = (uint16_t(buf[10]) << 8) |
                        uint16_t(buf[11]);

            entries_.push_back(e);
        }

        std::sort(entries_.begin(), entries_.end(),
            [](const PolyglotEntry& a, const PolyglotEntry& b)
            {
                return a.key < b.key;
            });

    return !entries_.empty();
    }
    Move decodePolyglotMove(const Board& board, uint16_t packed)
    {
    int toFile   = packed & 0x7;
    int toRank   = (packed >> 3) & 0x7;
    int fromFile = (packed >> 6) & 0x7;
    int fromRank = (packed >> 9) & 0x7;
    int promo    = (packed >> 12) & 0x7;

    Square from = Square(fromRank * 8 + fromFile);
    Square to   = Square(toRank * 8 + toFile);

    Movelist legal;
    movegen::legalmoves(legal, board);

    for (const auto& m : legal)
    {
        if (m.from() == from && m.to() == to)
        {
            if (promo == 0)
                return m;

            PieceType promoType;

            switch (promo)
            {
                case 1:
                    promoType = PieceType::KNIGHT;
                    break;

                case 2:
                    promoType = PieceType::BISHOP;
                    break;

                case 3:
                    promoType = PieceType::ROOK;
                    break;

                case 4:
                    promoType = PieceType::QUEEN;
                    break;

                default:
                    return Move::NO_MOVE;
            }

            if (m.typeOf() == Move::PROMOTION &&
                m.promotionType() == promoType)
            {
                return m;
            }
        }
    }

    for (const auto& m : legal)
    {
        if (m.typeOf() != Move::CASTLING || m.from() != from)
            continue;

        if (
            (m.to() == Square::SQ_H1 && to == Square::SQ_G1) ||
            (m.to() == Square::SQ_A1 && to == Square::SQ_C1) ||
            (m.to() == Square::SQ_H8 && to == Square::SQ_G8) ||
            (m.to() == Square::SQ_A8 && to == Square::SQ_C8)
        )
        {
            return m;
        }
    }

    return Move::NO_MOVE;
    }
    Move PolyglotBook::probe(const Board& board) const
{
    if (entries_.empty())
        return Move::NO_MOVE;

    uint64_t key = polyglotKey(board);

    auto lo = std::lower_bound(
        entries_.begin(),
        entries_.end(),
        key,
        [](const PolyglotEntry& e, uint64_t k)
        {
            return e.key < k;
        }
    );

    if (lo == entries_.end() || lo->key != key)
        return Move::NO_MOVE;

    auto hi = lo;

    while (hi != entries_.end() && hi->key == key)
        ++hi;

    // 1. Find highest weight.
    uint16_t maxWeight = 0;

    for (auto it = lo; it != hi; ++it)
    {
        if (it->weight > maxWeight)
            maxWeight = it->weight;
    }

    // 2. Collect all legal moves having that weight.
    std::vector<Move> bestMoves;

    for (auto it = lo; it != hi; ++it)
    {
        if (it->weight != maxWeight)
            continue;

        Move move = decodePolyglotMove(board, it->move);

        if (move != Move::NO_MOVE)
            bestMoves.push_back(move);
    }

    if (bestMoves.empty())
        return Move::NO_MOVE;

    // 3. Randomly choose between equally weighted best moves.
    static std::mt19937 rng(std::random_device{}());

    std::uniform_int_distribution<size_t> dist(
        0,
        bestMoves.size() - 1
    );

    return bestMoves[dist(rng)];
}
}
