// This evaluation is highly inspired by the stockfish evaluation
// this eval is written with the use of https://hxim.github.io/Stockfish-Evaluation-Guide/ this guide 
// so for refrence see this guide

#include <iostream>
#include<algorithm>
#include<cctype>
#include<string>
#include<vector>
#include "eval.hpp"
#include"psqt_tables.hpp"

using namespace std;
struct CombinedScores {
    int mg_material = 0;
    int eg_material = 0;
    int mg_psqt = 0;
    int eg_psqt = 0;
    // int non_pawn =0;
    int mg_mobility = 0;
    int eg_mobility = 0;
    int mg_pawn_structure = 0;
    int eg_pawn_structure = 0;  
    int mg_passed = 0;
    int eg_passed = 0;

    int mg_bishop = 0;
    int eg_bishop = 0;

    int mg_rook = 0;
    int eg_rook = 0;
    int mg_king_safety=0;
    int eg_king_safety=0;

    int non_pawn = 0;
};
static constexpr int MIDGAME_LIMIT = 15258;
static constexpr int ENDGAME_LIMIT = 3915;
static constexpr int PHASE_SCALE = 128;
int piece_value_bonus(chess::PieceType piece, bool mg) //mg=middle game,eg=endgame
{
    int mg_values[5] = {124, 781, 825, 1276, 2538}; // It is stockfish values they use for type of character during mid game
    int eg_values[5] = {206, 854, 915, 1380, 2682}; // It is stockfish values they use for type of character during end game

    int index;
    /*in this function we use internal and underlying what are they 
    class Fruit {
        public:
            enum class kind { APPLE, BANANA, MANGO };   // <- the TYPE, like `underlying`
             kind getKind() const { return k; }           // <- the FUNCTION, like `internal()`
        private:
             kind k;
        };
    here you can see that the "kind" is axtually what "wunderlying" is and getkind() is what internal is 
    when we use switch and case we need enum int like things we cant pass direct class in it so it just call it and get the 
    objects so we use it 
    */
    switch (piece.internal()){ 
        case chess::PieceType::underlying::PAWN: 
            index = 0;
            break;
        case chess::PieceType::underlying::KNIGHT:
            index = 1;
            break;
        case chess::PieceType::underlying::BISHOP:
            index = 2;
            break;
        case chess::PieceType::underlying::ROOK:
            index = 3;
            break;
        case chess::PieceType::underlying::QUEEN:
            index = 4;
            break;

        default:
            return 0;
    }
    if (mg)
        return mg_values[index];

    return eg_values[index];
}

int piece_value_mg(const chess::Board& board, chess::Square square){
    chess::PieceType piece = board.at<chess::PieceType>(square);
    return piece_value_bonus(piece, true);
}
int piece_value_eg(const chess::Board& board, chess::Square square) // given what type of input a board and a square of board
{
    chess::PieceType piece = board.at<chess::PieceType>(square); // Board, tell me what piece is currently sitting on this square.

    return piece_value_bonus(piece, false);
}
int psqt_bonus( const chess::Board& board, chess::Square square, bool mg)
{
    chess::PieceType piece = board.at<chess::PieceType>(square);

    int x = square.file();
    int y = square.rank();

    chess::Color color = board.at(square).color();

    // Flip rank for Black
    int rank;

    if (color == chess::Color::WHITE)
        rank = 7 - y;
    else
        rank = y;

    int piece_index;

    switch (piece.internal())
    {
        case chess::PieceType::underlying::PAWN:
            return mg
                ? pawn_psqt_mg[rank][x]
                : pawn_psqt_eg[rank][x];

        case chess::PieceType::underlying::KNIGHT:
            piece_index = 0;
            break;

        case chess::PieceType::underlying::BISHOP:
            piece_index = 1;
            break;

        case chess::PieceType::underlying::ROOK:
            piece_index = 2;
            break;

        case chess::PieceType::underlying::QUEEN:
            piece_index = 3;
            break;

        case chess::PieceType::underlying::KING:
            piece_index = 4;
            break;

        default:
            return 0;
    }

    int file = min(x, 7 - x);

    if (mg)
        return psqt_mg[piece_index][rank][file];

    return psqt_eg[piece_index][rank][file];
}
// ============================================================
// PAWN STRUCTURE EVALUATION
//
// Based on the Stockfish Evaluation Guide:
// Isolated
// Opposed
// Phalanx
// Supported
// Backward
// Doubled
// Connected
// Connected bonus
// Weak unopposed pawn
// Weak lever
//
// This is WHITE-perspective evaluation. The final score is
// White pawn-structure score - Black pawn-structure score.
// ============================================================


// ------------------------------------------------------------
// Isolated pawn
//
// A pawn is isolated if there is no friendly pawn on either
// adjacent file.
// ------------------------------------------------------------
static bool pawn_is_isolated(const chess::Board& board,
                             chess::Color color,
                             chess::Square square)
{
    if (board.at<chess::PieceType>(square) !=
        chess::PieceType::PAWN)
        return false;

    if (board.at(square).color() != color)
        return false;

    int file = square.file();

    for (int rank = 0; rank < 8; ++rank)
    {
        if (file > 0)
        {
            chess::Square s(
                (rank * 8) + (file - 1)
            );

            if (board.at<chess::PieceType>(s) ==
                    chess::PieceType::PAWN &&
                board.at(s).color() == color)
            {
                return false;
            }
        }

        if (file < 7)
        {
            chess::Square s(
                (rank * 8) + (file + 1)
            );

            if (board.at<chess::PieceType>(s) ==
                    chess::PieceType::PAWN &&
                board.at(s).color() == color)
            {
                return false;
            }
        }
    }

    return true;
}


// ------------------------------------------------------------
// Opposed pawn
//
// Guide definition:
// opponent has a pawn somewhere in front of our pawn
// on the same file.
// ------------------------------------------------------------
static bool pawn_is_opposed(const chess::Board& board,
                            chess::Color color,
                            chess::Square square)
{
    if (board.at<chess::PieceType>(square) !=
        chess::PieceType::PAWN)
        return false;

    if (board.at(square).color() != color)
        return false;

    int file = square.file();
    int rank = square.rank();

    if (color == chess::Color::WHITE)
    {
        for (int y = 0; y < rank; ++y)
        {
            chess::Square s(y * 8 + file);

            if (board.at<chess::PieceType>(s) ==
                    chess::PieceType::PAWN &&
                board.at(s).color() ==
                    chess::Color::BLACK)
            {
                return true;
            }
        }
    }
    else
    {
        for (int y = rank + 1; y < 8; ++y)
        {
            chess::Square s(y * 8 + file);

            if (board.at<chess::PieceType>(s) ==
                    chess::PieceType::PAWN &&
                board.at(s).color() ==
                    chess::Color::WHITE)
            {
                return true;
            }
        }
    }

    return false;
}


// ------------------------------------------------------------
// Phalanx
//
// Friendly pawn on adjacent file and same rank.
// ------------------------------------------------------------
static bool pawn_is_phalanx(const chess::Board& board,
                            chess::Color color,
                            chess::Square square)
{
    if (board.at<chess::PieceType>(square) !=
        chess::PieceType::PAWN)
        return false;

    if (board.at(square).color() != color)
        return false;

    int file = square.file();
    int rank = square.rank();

    if (file > 0)
    {
        chess::Square s(rank * 8 + file - 1);

        if (board.at<chess::PieceType>(s) ==
                chess::PieceType::PAWN &&
            board.at(s).color() == color)
            return true;
    }

    if (file < 7)
    {
        chess::Square s(rank * 8 + file + 1);

        if (board.at<chess::PieceType>(s) ==
                chess::PieceType::PAWN &&
            board.at(s).color() == color)
            return true;
    }

    return false;
}


// ------------------------------------------------------------
// Supported
//
// Guide definition:
// number of friendly pawns one rank behind on adjacent files.
// ------------------------------------------------------------
static int pawn_supported(const chess::Board& board,
                          chess::Color color,
                          chess::Square square)
{
    if (board.at<chess::PieceType>(square) !=
        chess::PieceType::PAWN)
        return 0;

    if (board.at(square).color() != color)
        return 0;

    int file = square.file();
    int rank = square.rank();

    // White pawns move toward decreasing board rank.
    // Black pawns move toward increasing board rank.
    int behind =
        color == chess::Color::WHITE
            ? rank + 1
            : rank - 1;

    if (behind < 0 || behind >= 8)
        return 0;

    int result = 0;

    if (file > 0)
    {
        chess::Square s(behind * 8 + file - 1);

        if (board.at<chess::PieceType>(s) ==
                chess::PieceType::PAWN &&
            board.at(s).color() == color)
        {
            result++;
        }
    }

    if (file < 7)
    {
        chess::Square s(behind * 8 + file + 1);

        if (board.at<chess::PieceType>(s) ==
                chess::PieceType::PAWN &&
            board.at(s).color() == color)
        {
            result++;
        }
    }

    return result;
}


// ------------------------------------------------------------
// Backward pawn
//
// Based directly on the Evaluation Guide:
//
// A pawn is backward when it is behind all friendly pawns
// on adjacent files and cannot safely advance.
//
// We use the guide's structural definition.
// ------------------------------------------------------------
static bool pawn_is_backward(const chess::Board& board,
                             chess::Color color,
                             chess::Square square)
{
    if (board.at<chess::PieceType>(square) !=
        chess::PieceType::PAWN)
        return false;

    if (board.at(square).color() != color)
        return false;

    int file = square.file();
    int rank = square.rank();

    // Check whether there is a friendly pawn ahead on either
    // adjacent file.
    if (color == chess::Color::WHITE)
    {
        for (int y = rank; y < 8; ++y)
        {
            if (file > 0)
            {
                chess::Square s(y * 8 + file - 1);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() == color)
                    return false;
            }

            if (file < 7)
            {
                chess::Square s(y * 8 + file + 1);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() == color)
                    return false;
            }
        }

        // Enemy pawn two squares in front diagonally,
        // or enemy pawn directly one square in front.
        int y1 = rank - 2;
        int y2 = rank - 1;

        if (y1 >= 0)
        {
            if (file > 0)
            {
                chess::Square s(y1 * 8 + file - 1);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() ==
                        chess::Color::BLACK)
                    return true;
            }

            if (file < 7)
            {
                chess::Square s(y1 * 8 + file + 1);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() ==
                        chess::Color::BLACK)
                    return true;
            }
        }

        if (y2 >= 0)
        {
            chess::Square s(y2 * 8 + file);

            if (board.at<chess::PieceType>(s) ==
                    chess::PieceType::PAWN &&
                board.at(s).color() ==
                    chess::Color::BLACK)
                return true;
        }
    }
    else
    {
        for (int y = rank; y >= 0; --y)
        {
            if (file > 0)
            {
                chess::Square s(y * 8 + file - 1);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() == color)
                    return false;
            }

            if (file < 7)
            {
                chess::Square s(y * 8 + file + 1);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() == color)
                    return false;
            }
        }

        int y1 = rank + 2;
        int y2 = rank + 1;

        if (y1 < 8)
        {
            if (file > 0)
            {
                chess::Square s(y1 * 8 + file - 1);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() ==
                        chess::Color::WHITE)
                    return true;
            }

            if (file < 7)
            {
                chess::Square s(y1 * 8 + file + 1);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() ==
                        chess::Color::WHITE)
                    return true;
            }
        }

        if (y2 < 8)
        {
            chess::Square s(y2 * 8 + file);

            if (board.at<chess::PieceType>(s) ==
                    chess::PieceType::PAWN &&
                board.at(s).color() ==
                    chess::Color::WHITE)
                return true;
        }
    }

    return false;
}


// ------------------------------------------------------------
// Doubled pawn
//
// The guide uses a more specific definition:
//
// The pawn has a friendly pawn directly behind it and is not
// supported by a pawn on either adjacent file behind it.
// ------------------------------------------------------------
static bool pawn_is_doubled(const chess::Board& board,
                            chess::Color color,
                            chess::Square square)
{
    if (board.at<chess::PieceType>(square) !=
        chess::PieceType::PAWN)
        return false;

    if (board.at(square).color() != color)
        return false;

    int file = square.file();
    int rank = square.rank();

    int behind =
        color == chess::Color::WHITE
            ? rank + 1
            : rank - 1;

    if (behind < 0 || behind >= 8)
        return false;

    // Directly behind.
    chess::Square direct(behind * 8 + file);

    if (board.at<chess::PieceType>(direct) !=
            chess::PieceType::PAWN ||
        board.at(direct).color() != color)
        return false;

    // Supported from behind by adjacent pawn?
    if (file > 0)
    {
        chess::Square s(behind * 8 + file - 1);

        if (board.at<chess::PieceType>(s) ==
                chess::PieceType::PAWN &&
            board.at(s).color() == color)
            return false;
    }

    if (file < 7)
    {
        chess::Square s(behind * 8 + file + 1);

        if (board.at<chess::PieceType>(s) ==
                chess::PieceType::PAWN &&
            board.at(s).color() == color)
            return false;
    }

    return true;
}


// ------------------------------------------------------------
// Connected pawn
//
// Guide definition:
// connected = supported OR phalanx.
// ------------------------------------------------------------
static bool pawn_is_connected(const chess::Board& board,
                              chess::Color color,
                              chess::Square square)
{
    return pawn_supported(
               board,
               color,
               square
           ) > 0
        || pawn_is_phalanx(
               board,
               color,
               square
           );
}


// ------------------------------------------------------------
// Weak unopposed pawn
//
// Guide:
// if pawn is not opposed and is isolated or backward.
// ------------------------------------------------------------
static bool pawn_is_weak_unopposed(const chess::Board& board,
                                   chess::Color color,
                                   chess::Square square)
{
    if (pawn_is_opposed(board, color, square))
        return false;

    return pawn_is_isolated(board, color, square)
        || pawn_is_backward(board, color, square);
}


// ------------------------------------------------------------
// Weak lever
//
// Guide definition:
// pawn attacked twice by enemy pawns and not supported.
// ------------------------------------------------------------
static bool pawn_is_weak_lever(const chess::Board& board,
                               chess::Color color,
                               chess::Square square)
{
    if (board.at<chess::PieceType>(square) !=
        chess::PieceType::PAWN)
        return false;

    if (board.at(square).color() != color)
        return false;

    int file = square.file();
    int rank = square.rank();

    int enemy_attack_rank =
        color == chess::Color::WHITE
            ? rank + 1
            : rank - 1;

    if (enemy_attack_rank < 0 ||
        enemy_attack_rank >= 8)
        return false;

    bool left = false;
    bool right = false;

    if (file > 0)
    {
        chess::Square s(
            enemy_attack_rank * 8 + file - 1
        );

        left =
            board.at<chess::PieceType>(s) ==
                chess::PieceType::PAWN &&
            board.at(s).color() != color &&
            board.at(s).color() !=
                chess::Color::NONE;
    }

    if (file < 7)
    {
        chess::Square s(
            enemy_attack_rank * 8 + file + 1
        );

        right =
            board.at<chess::PieceType>(s) ==
                chess::PieceType::PAWN &&
            board.at(s).color() != color &&
            board.at(s).color() !=
                chess::Color::NONE;
    }

    if (!left || !right)
        return false;

    return pawn_supported(
               board,
               color,
               square
           ) == 0;
}


// ------------------------------------------------------------
// Connected bonus
//
// This is the exact connected-bonus formula from the guide:
//
// seed = [0,7,8,12,29,48,86]
//
// bonus = seed[rank-1] * (2 + phalanx - opposed)
//         + 21 * supported
// ------------------------------------------------------------
static int pawn_connected_bonus(const chess::Board& board,
                                chess::Color color,
                                chess::Square square)
{
    if (!pawn_is_connected(board, color, square))
        return 0;

    static constexpr int seed[7] =
    {
        0, 7, 8, 12, 29, 48, 86
    };

    int rank;

    if (color == chess::Color::WHITE)
        rank = square.rank() + 1;
    else
        rank = 8 - square.rank();

    if (rank < 2 || rank > 7)
        return 0;

    int opposed =
        pawn_is_opposed(
            board,
            color,
            square
        ) ? 1 : 0;

    int phalanx =
        pawn_is_phalanx(
            board,
            color,
            square
        ) ? 1 : 0;

    int supported =
        pawn_supported(
            board,
            color,
            square
        );

    return seed[rank - 1] *
               (2 + phalanx - opposed)
         + 21 * supported;
}


// ------------------------------------------------------------
// Pawn structure score.
//
// Conservative MG/EG values are used here. The structural
// definitions follow the Evaluation Guide, while the weights
// are kept modest because we will benchmark/tune the complete
// evaluator after adding the remaining features.
// ------------------------------------------------------------
static int pawn_structure_score(
    const chess::Board& board,
    chess::Color color,
    bool mg)
{
    int score = 0;

    // MG / EG penalties.
    const int isolated =
        mg ? -11 : -5;

    const int backward =
        mg ? -8 : -7;

    const int doubled =
        mg ? -11 : -2;

    const int weak_unopposed =
        mg ? -13 : -5;

    const int weak_lever =
        mg ? -8 : -4;

    for (int i = 0; i < 64; ++i)
    {
        chess::Square square(i);

        if (board.at<chess::PieceType>(square) !=
            chess::PieceType::PAWN)
            continue;

        if (board.at(square).color() != color)
            continue;

        if (pawn_is_isolated(board, color, square))
            score += isolated;

        if (pawn_is_backward(board, color, square))
            score += backward;

        if (pawn_is_doubled(board, color, square))
            score += doubled;

        if (pawn_is_weak_unopposed(board, color, square))
            score += weak_unopposed;

        if (pawn_is_weak_lever(board, color, square))
            score += weak_lever;

        score += pawn_connected_bonus(
            board,
            color,
            square
        );
    }

    return score;
}


// ------------------------------------------------------------
// Final pawn structure contribution.
//
// White score - Black score.
// ------------------------------------------------------------
int pawn_structure_mg(const chess::Board& board)
{
    return pawn_structure_score(
               board,
               chess::Color::WHITE,
               true
           )
         - pawn_structure_score(
               board,
               chess::Color::BLACK,
               true
           );
}

int pawn_structure_eg(const chess::Board& board)
{
    return pawn_structure_score(
               board,
               chess::Color::WHITE,
               false
           )
         - pawn_structure_score(
               board,
               chess::Color::BLACK,
               false
           );
}
// ============================================================
// PASSED PAWNS
// ============================================================

static bool is_passed_pawn(const chess::Board& board,
                           chess::Color color,
                           chess::Square sq)
{
    if (board.at<chess::PieceType>(sq) != chess::PieceType::PAWN)
        return false;

    if (board.at(sq).color() != color)
        return false;

    int file = sq.file();
    int rank = sq.rank();

    chess::Color enemy =
        (color == chess::Color::WHITE)
            ? chess::Color::BLACK
            : chess::Color::WHITE;

    // Look in front of the pawn on the same file and
    // adjacent files.
    for (int f = std::max(0, file - 1);
         f <= std::min(7, file + 1);
         ++f)
    {
        if (color == chess::Color::WHITE)
        {
            for (int r = rank + 1; r < 8; ++r)
            {
                chess::Square s(r * 8 + f);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() == enemy)
                {
                    return false;
                }
            }
        }
        else
        {
            for (int r = rank - 1; r >= 0; --r)
            {
                chess::Square s(r * 8 + f);

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() == enemy)
                {
                    return false;
                }
            }
        }
    }

    return true;
}


static int relative_pawn_rank(chess::Color color,
                              chess::Square sq)
{
    if (color == chess::Color::WHITE)
        return sq.rank() + 1;

    return 8 - sq.rank();
}


static bool pawn_is_blocked(const chess::Board& board,
                            chess::Color color,
                            chess::Square sq)
{
    int next_rank =
        color == chess::Color::WHITE
            ? sq.rank() + 1
            : sq.rank() - 1;

    if (next_rank < 0 || next_rank >= 8)
        return true;

    chess::Square front(
        next_rank * 8 + sq.file()
    );

    return board.at(front).color() !=
           chess::Color::NONE;
}


static int king_distance(const chess::Board& board,
                         chess::Color color,
                         chess::Square sq)
{
    chess::Square king = board.kingSq(color);

    int dx = std::abs(
        king.file() - sq.file()
    );

    int dy = std::abs(
        king.rank() - sq.rank()
    );

    return std::max(dx, dy);
}


static int passed_pawn_score(const chess::Board& board,
                             chess::Color color,
                             bool mg)
{
    int score = 0;

    chess::Color enemy =
        (color == chess::Color::WHITE)
            ? chess::Color::BLACK
            : chess::Color::WHITE;

    for (int i = 0; i < 64; ++i)
    {
        chess::Square sq(i);

        if (!is_passed_pawn(board, color, sq))
            continue;

        int r = relative_pawn_rank(color, sq);

        // Don't evaluate the promotion square itself.
        if (r >= 8)
            continue;

        // ----------------------------------------------------
        // Base passed-pawn bonus.
        //
        // Based on the classical Stockfish passed-pawn formula:
        //
        // rr = r * (r - 1)
        // MG = 20 * rr
        // EG = 10 * (rr + r + 1)
        // ----------------------------------------------------
        int rr = r * (r - 1);

        int bonus;

        if (mg)
            bonus = 20 * rr;
        else
            bonus = 10 * (rr + r + 1);

        // ----------------------------------------------------
        // Blocked passed pawn.
        //
        // A blocked passer is less dangerous.
        // ----------------------------------------------------
        if (pawn_is_blocked(board, color, sq))
        {
            if (mg)
                bonus /= 2;
            else
                bonus = (bonus * 2) / 3;
        }

        // ----------------------------------------------------
        // Friendly pawn support.
        // ----------------------------------------------------
        int file = sq.file();
        int rank = sq.rank();

        int support = 0;

        int behind_rank =
            color == chess::Color::WHITE
                ? rank - 1
                : rank + 1;

        if (behind_rank >= 0 &&
            behind_rank < 8)
        {
            if (file > 0)
            {
                chess::Square s(
                    behind_rank * 8 + file - 1
                );

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() == color)
                {
                    support++;
                }
            }

            if (file < 7)
            {
                chess::Square s(
                    behind_rank * 8 + file + 1
                );

                if (board.at<chess::PieceType>(s) ==
                        chess::PieceType::PAWN &&
                    board.at(s).color() == color)
                {
                    support++;
                }
            }
        }

        if (support)
        {
            if (mg)
                bonus += 10 * r * support;
            else
                bonus += 12 * r * support;
        }

        // ----------------------------------------------------
        // King support.
        // ----------------------------------------------------
        int friendly_king_distance =
            king_distance(board, color, sq);

        int enemy_king_distance =
            king_distance(board, enemy, sq);

        if (friendly_king_distance <= 2)
        {
            bonus += mg ? 8 * r : 12 * r;
        }

        if (enemy_king_distance <= 2)
        {
            bonus -= mg ? 6 * r : 10 * r;
        }

        // ----------------------------------------------------
        // Rook pawn special handling.
        //
        // Rook pawns are generally less valuable when the
        // opponent has heavy pieces.
        // ----------------------------------------------------
        if (file == 0 || file == 7)
        {
            int enemy_npm =
                non_pawn_material(
                    const_cast<chess::Board&>(board),
                    enemy
                );

            if (enemy_npm >= piece_value_bonus(
                    chess::PieceType::ROOK,
                    true))
            {
                bonus = (bonus * 3) / 4;
            }
            else if (enemy_npm <= piece_value_bonus(
                    chess::PieceType::KNIGHT,
                    true))
            {
                bonus = (bonus * 5) / 4;
            }
        }

        score += bonus;
    }

    return score;
}


int passed_pawn_mg(const chess::Board& board)
{
    return passed_pawn_score(
               board,
               chess::Color::WHITE,
               true
           )
         - passed_pawn_score(
               board,
               chess::Color::BLACK,
               true
           );
}


int passed_pawn_eg(const chess::Board& board)
{
    return passed_pawn_score(
               board,
               chess::Color::WHITE,
               false
           )
         - passed_pawn_score(
               board,
               chess::Color::BLACK,
               false
           );
}



// ============================================================
// BISHOP EVALUATION
// ============================================================

static int bishop_pawn_color_penalty(
    const chess::Board& board,
    chess::Color color,
    chess::Square bishop_sq)
{
    int penalty = 0;

    int bishop_color =
        (bishop_sq.file() + bishop_sq.rank()) & 1;

    for (int i = 0; i < 64; ++i)
    {
        chess::Square sq(i);

        if (board.at<chess::PieceType>(sq) !=
            chess::PieceType::PAWN)
            continue;

        if (board.at(sq).color() != color)
            continue;

        int pawn_color =
            (sq.file() + sq.rank()) & 1;

        if (pawn_color == bishop_color)
            penalty++;
    }

    return penalty;
}


static bool bishop_has_open_diagonal(
    const chess::Board& board,
    chess::Square sq)
{
    chess::Bitboard attacks =
        chess::attacks::bishop(
            sq,
            board.occ()
        );

    return attacks.count() >= 7;
}


static int bishop_score(const chess::Board& board,
                        chess::Color color,
                        bool mg)
{
    int score = 0;

    int bishop_count_side =
        bishop_count(
            board,
            color
        );

    // --------------------------------------------------------
    // Bishop pair.
    //
    // Classical Stockfish-style bishop pair bonus.
    // --------------------------------------------------------
    if (bishop_count_side >= 2)
    {
        score += mg ? 30 : 40;
    }

    for (int i = 0; i < 64; ++i)
    {
        chess::Square sq(i);

        if (board.at<chess::PieceType>(sq) !=
            chess::PieceType::BISHOP)
            continue;

        if (board.at(sq).color() != color)
            continue;

        // ----------------------------------------------------
        // Bad bishop.
        //
        // Penalize a bishop whose own pawns occupy many
        // squares of its color.
        // ----------------------------------------------------
        int same_color_pawns =
            bishop_pawn_color_penalty(
                board,
                color,
                sq
            );

        score -=
            mg
                ? same_color_pawns * 4
                : same_color_pawns * 2;

        // ----------------------------------------------------
        // Open diagonal bonus.
        // ----------------------------------------------------
        if (bishop_has_open_diagonal(board, sq))
        {
            score += mg ? 6 : 10;
        }

        // ----------------------------------------------------
        // Bishop on long diagonal.
        // ----------------------------------------------------
        int file = sq.file();
        int rank = sq.rank();

        if ((file == 0 || file == 7) &&
            (rank == 0 || rank == 7))
        {
            score -= mg ? 4 : 2;
        }
    }

    return score;
}


int bishop_eval_mg(const chess::Board& board)
{
    return bishop_score(
               board,
               chess::Color::WHITE,
               true
           )
         - bishop_score(
               board,
               chess::Color::BLACK,
               true
           );
}


int bishop_eval_eg(const chess::Board& board)
{
    return bishop_score(
               board,
               chess::Color::WHITE,
               false
           )
         - bishop_score(
               board,
               chess::Color::BLACK,
               false
           );
}



// ============================================================
// ROOK ACTIVITY
// ============================================================

static bool has_pawn_on_file(
    const chess::Board& board,
    chess::Color color,
    int file)
{
    for (int rank = 0; rank < 8; ++rank)
    {
        chess::Square sq(
            rank * 8 + file
        );

        if (board.at<chess::PieceType>(sq) ==
                chess::PieceType::PAWN &&
            board.at(sq).color() == color)
        {
            return true;
        }
    }

    return false;
}


static bool rook_file_is_open(
    const chess::Board& board,
    int file)
{
    return !has_pawn_on_file(
               board,
               chess::Color::WHITE,
               file
           )
        && !has_pawn_on_file(
               board,
               chess::Color::BLACK,
               file
           );
}


static bool rook_file_is_half_open(
    const chess::Board& board,
    chess::Color color,
    int file)
{
    return !has_pawn_on_file(
               board,
               color,
               file
           )
        && has_pawn_on_file(
               board,
               ~color,
               file
           );
}


static bool rook_on_seventh(
    const chess::Board& board,
    chess::Color color,
    chess::Square sq)
{
    int relative_rank =
        color == chess::Color::WHITE
            ? sq.rank() + 1
            : 8 - sq.rank();

    if (relative_rank != 7)
        return false;

    chess::Square enemy_king =
        board.kingSq(~color);

    int enemy_king_relative_rank =
        color == chess::Color::WHITE
            ? enemy_king.rank() + 1
            : 8 - enemy_king.rank();

    return enemy_king_relative_rank == 8;
}


static bool rooks_connected(
    const chess::Board& board,
    chess::Color color)
{
    chess::Square rooks[2];
    int count = 0;

    for (int i = 0; i < 64; ++i)
    {
        chess::Square sq(i);

        if (board.at<chess::PieceType>(sq) ==
                chess::PieceType::ROOK &&
            board.at(sq).color() == color)
        {
            if (count < 2)
                rooks[count] = sq;

            count++;
        }
    }

    if (count < 2)
        return false;

    int dx =
        rooks[0].file() -
        rooks[1].file();

    int dy =
        rooks[0].rank() -
        rooks[1].rank();

    // Same file.
    if (dx == 0)
    {
        int step =
            dy > 0 ? -1 : 1;

        for (int r =
                 rooks[0].rank() + step;
             r != static_cast<int>(rooks[1].rank());
             r += step)
        {
            chess::Square sq(
                r * 8 + rooks[0].file()
            );

            if (board.at(sq).color() !=
                chess::Color::NONE)
            {
                return false;
            }
        }

        return true;
    }

    // Same rank.
    if (dy == 0)
    {
        int step =
            dx > 0 ? -1 : 1;

        for (int f = static_cast<int>(rooks[0].file()) + step;
     f != static_cast<int>(rooks[1].file());
     f += step)
{
    chess::Square sq(
        static_cast<int>(rooks[0].rank()) * 8 + f
    );

    if (board.at(sq).color() != chess::Color::NONE)
    {
        return false;
    }
}

        return true;
    }

    return false;
}


static int rook_score(const chess::Board& board,
                      chess::Color color,
                      bool mg)
{
    int score = 0;

    for (int i = 0; i < 64; ++i)
    {
        chess::Square sq(i);

        if (board.at<chess::PieceType>(sq) !=
            chess::PieceType::ROOK)
            continue;

        if (board.at(sq).color() != color)
            continue;

        int file = sq.file();

        // ----------------------------------------------------
        // Open file.
        //
        // Classical Stockfish values are roughly:
        // MG +43 / EG +21
        // ----------------------------------------------------
        if (rook_file_is_open(board, file))
        {
            score += mg ? 30 : 16;
        }
        else if (rook_file_is_half_open(
                     board,
                     color,
                     file))
        {
            score += mg ? 15 : 8;
        }

        // ----------------------------------------------------
        // Rook on seventh rank attacking the enemy king.
        // ----------------------------------------------------
        if (rook_on_seventh(board, color, sq))
        {
            score += mg ? 35 : 60;
        }

        // ----------------------------------------------------
        // Connected rooks.
        // ----------------------------------------------------
        if (rooks_connected(board, color))
        {
            score += mg ? 8 : 12;
        }

        // ----------------------------------------------------
        // Rook behind a passed pawn.
        // ----------------------------------------------------
        bool useful_pawn = false;

        for (int p = 0; p < 64; ++p)
        {
            chess::Square pawn_sq(p);

            if (!is_passed_pawn(
                    board,
                    color,
                    pawn_sq))
                continue;
                if (static_cast<int>(pawn_sq.file()) != file)
    continue;

if (color == chess::Color::WHITE &&
    static_cast<int>(sq.rank()) < static_cast<int>(pawn_sq.rank()))
{
    useful_pawn = true;
}

if (color == chess::Color::BLACK &&
    static_cast<int>(sq.rank()) > static_cast<int>(pawn_sq.rank()))
{
    useful_pawn = true;
}
        }

        if (useful_pawn)
        {
            score += mg ? 8 : 20;
        }
    }

    return score;
}


int rook_eval_mg(const chess::Board& board)
{
    return rook_score(
               board,
               chess::Color::WHITE,
               true
           )
         - rook_score(
               board,
               chess::Color::BLACK,
               true
           );
}


int rook_eval_eg(const chess::Board& board)
{
    return rook_score(
               board,
               chess::Color::WHITE,
               false
           )
         - rook_score(
               board,
               chess::Color::BLACK,
               false
           );
}
// ============================================================
// KING SAFETY EVALUATION
//
// Components:
//
// 1. Pawn shelter
// 2. Enemy pawn storm
// 3. King ring
// 4. Enemy attackers in king ring
// 5. Attacker weights
// 6. Weak/undefended king-ring squares
// 7. Safe attacking pressure
//
// This is White-positive:
//     White king safety - Black king safety
//
// King safety is primarily a middlegame concept.
// ============================================================


// ------------------------------------------------------------
// Count pawns of a color on a particular file.
// ------------------------------------------------------------
static int pawns_on_file(
    const chess::Board& board,
    chess::Color color,
    int file)
{
    int count = 0;

    for (int rank = 0; rank < 8; ++rank)
    {
//         chess::Square sq{
//     chess::File(f),
//     chess::Rank(actual_rank)
// }; 
        chess::Square sq(rank * 8 + file);

        if (board.at<chess::PieceType>(sq) ==
                chess::PieceType::PAWN &&
            board.at(sq).color() == color)
        {
            ++count;
        }
    }

    return count;
}


// ------------------------------------------------------------
// Return the number of friendly pawns around the king.
//
// We inspect the king's file and adjacent files.
//
// Pawns on the 2nd and 3rd ranks are particularly useful
// for king shelter.
// ------------------------------------------------------------
static int king_shelter_strength(
    const chess::Board& board,
    chess::Color color)
{
    chess::Square king =
        board.kingSq(color);

    int file =
        static_cast<int>(king.file());

    int score = 0;

    for (int f = std::max(0, file - 1);
         f <= std::min(7, file + 1);
         ++f)
    {
        bool found = false;

        // First three ranks from the home side.
        for (int r = 0; r < 3; ++r)
        {
            int actual_rank;

            if (color == chess::Color::WHITE)
                actual_rank = r;
            else
                actual_rank = 7 - r;
            
            
            // chess::Square sq(x * 8 + y);
            chess::Square sq(actual_rank * 8 + f);

            if (board.at<chess::PieceType>(sq) ==
                    chess::PieceType::PAWN &&
                board.at(sq).color() == color)
            {
                found = true;

                // Pawn on home rank is useful but less valuable.
                if (r == 0)
                    score += 4;

                // Pawn one rank forward gives stronger shelter.
                else if (r == 1)
                    score += 8;

                // Advanced shelter pawn.
                else
                    score += 5;

                break;
            }
        }

        // Missing pawn = hole in shelter.
        if (!found)
            score -= 7;
    }

    return score;
}


// ------------------------------------------------------------
// Enemy pawn storm.
//
// Enemy pawns advancing toward our king are dangerous.
//
// The closer the enemy pawn is to our king's side,
// the larger the penalty.
// ------------------------------------------------------------
static int king_pawn_storm(
    const chess::Board& board,
    chess::Color defending_color)
{
    chess::Color enemy = ~defending_color;

    chess::Square king =
        board.kingSq(defending_color);

    int king_file =
        static_cast<int>(king.file());

    int score = 0;

    chess::Bitboard pawns =
        board.pieces(
            chess::PieceType::PAWN,
            enemy
        );

    while (!pawns.empty())
    {
        chess::Square pawn(pawns.pop());

        int pawn_file =
            static_cast<int>(pawn.file());

        int file_distance =
            std::abs(pawn_file - king_file);

        // Only pawns near the king's files matter much.
        if (file_distance > 2)
            continue;

        int relative_rank;

        if (enemy == chess::Color::WHITE)
        {
            relative_rank =
                static_cast<int>(pawn.rank()) + 1;
        }
        else
        {
            relative_rank =
                8 - static_cast<int>(pawn.rank());
        }

        // Enemy pawn is more dangerous as it advances.
        int danger = 0;

        if (relative_rank >= 5)
            danger = 18;
        else if (relative_rank == 4)
            danger = 13;
        else if (relative_rank == 3)
            danger = 8;
        else
            danger = 3;

        if (file_distance == 0)
            danger += 8;
        else if (file_distance == 1)
            danger += 4;

        score += danger;
    }

    return score;
}


// ------------------------------------------------------------
// King ring.
//
// The Stockfish-style king ring consists of squares around
// the king plus the three squares two ranks in front.
//
// We construct it explicitly.
// ------------------------------------------------------------
static chess::Bitboard king_ring(
    const chess::Board& board,
    chess::Color color)
{
    chess::Square king =
        board.kingSq(color);

    int kx =
        static_cast<int>(king.file());

    int ky =
        static_cast<int>(king.rank());

    chess::Bitboard ring;

    // Adjacent squares.
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            if (dx == 0 && dy == 0)
                continue;

            int x = kx + dx;
            int y = ky + dy;

            if (x >= 0 && x < 8 &&
                y >= 0 && y < 8)
            {
                chess::Square sq(x * 8 + y);

                ring.set(sq.index());
            }
        }
    }

    // Three squares two ranks in front of the king.
    int forward =
        color == chess::Color::WHITE
            ? ky + 2
            : ky - 2;

    if (forward >= 0 && forward < 8)
    {
        for (int x = kx - 1;
             x <= kx + 1;
             ++x)
        {
            if (x < 0 || x >= 8)
                continue;

            // chess::Square sq(x * 8 + y);
            chess::Square sq(forward * 8 + x);

            ring.set(sq.index());
        }
    }

    return ring;
}


// ------------------------------------------------------------
// Count attacks made by one piece onto the king ring.
//
// The values correspond roughly to the classical Stockfish
// attacker weights:
//
// Knight = 2
// Bishop = 2
// Rook   = 3
// Queen  = 5
// ------------------------------------------------------------
static int king_ring_attacks_from_piece(
    const chess::Board& board,
    chess::Color attacker,
    chess::Square source,
    chess::Bitboard ring)
{
    chess::PieceType type =
        board.at<chess::PieceType>(source);

    chess::Bitboard attacks;

    chess::Bitboard occupancy =
        board.occ();

    switch (type.internal())
    {
        case chess::PieceType::underlying::KNIGHT:
            attacks =
                chess::attacks::knight(source);
            break;

        case chess::PieceType::underlying::BISHOP:
            attacks =
                chess::attacks::bishop(
                    source,
                    occupancy
                );
            break;

        case chess::PieceType::underlying::ROOK:
            attacks =
                chess::attacks::rook(
                    source,
                    occupancy
                );
            break;

        case chess::PieceType::underlying::QUEEN:
            attacks =
                chess::attacks::queen(
                    source,
                    occupancy
                );
            break;

        case chess::PieceType::underlying::PAWN:
            attacks =
                chess::attacks::pawn(
                    attacker,
                    source
                );
            break;

        case chess::PieceType::underlying::KING:
            attacks =
                chess::attacks::king(source);
            break;

        default:
            return 0;
    }

    int attack_count =
        (attacks & ring).count();

    int weight = 0;

    switch (type.internal())
    {
        case chess::PieceType::underlying::PAWN:
            weight = 1;
            break;

        case chess::PieceType::underlying::KNIGHT:
            weight = 2;
            break;

        case chess::PieceType::underlying::BISHOP:
            weight = 2;
            break;

        case chess::PieceType::underlying::ROOK:
            weight = 3;
            break;

        case chess::PieceType::underlying::QUEEN:
            weight = 5;
            break;

        case chess::PieceType::underlying::KING:
            weight = 1;
            break;

        default:
            weight = 0;
    }

    return attack_count * weight;
}


// ------------------------------------------------------------
// Total attacker weight against a king.
// ------------------------------------------------------------
static int king_attack_weight(
    const chess::Board& board,
    chess::Color defending_color)
{
    chess::Color attacker =
        ~defending_color;

    chess::Bitboard ring =
        king_ring(
            board,
            defending_color
        );

    int total_weight = 0;

    chess::Bitboard pieces =
    board.pieces(chess::PieceType::PAWN,   attacker) |
    board.pieces(chess::PieceType::KNIGHT, attacker) |
    board.pieces(chess::PieceType::BISHOP, attacker) |
    board.pieces(chess::PieceType::ROOK,   attacker) |
    board.pieces(chess::PieceType::QUEEN,  attacker) |
    board.pieces(chess::PieceType::KING,   attacker);

    while (!pieces.empty())
    {
        chess::Square source(
            pieces.pop()
        );

        chess::PieceType type =
            board.at<chess::PieceType>(source);

        // Pawns contribute to king attack pressure,
        // but we don't count the enemy king itself.
        if (type == chess::PieceType::KING)
            continue;

        total_weight +=
            king_ring_attacks_from_piece(
                board,
                attacker,
                source,
                ring
            );
    }

    return total_weight;
}


// ------------------------------------------------------------
// Number of enemy attacks on the king ring.
// ------------------------------------------------------------
static int king_attack_count(
    const chess::Board& board,
    chess::Color defending_color)
{
    chess::Color attacker =
        ~defending_color;

    chess::Bitboard ring =
        king_ring(
            board,
            defending_color
        );

    int count = 0;

        chess::Bitboard pieces =
    board.pieces(chess::PieceType::PAWN,   attacker) |
    board.pieces(chess::PieceType::KNIGHT, attacker) |
    board.pieces(chess::PieceType::BISHOP, attacker) |
    board.pieces(chess::PieceType::ROOK,   attacker) |
    board.pieces(chess::PieceType::QUEEN,  attacker) |
    board.pieces(chess::PieceType::KING,   attacker);

    while (!pieces.empty())
    {
        chess::Square source(
            pieces.pop()
        );

        chess::PieceType type =
            board.at<chess::PieceType>(source);

        if (type == chess::PieceType::KING)
            continue;

        chess::Bitboard attacks;

        switch (type.internal())
        {
            case chess::PieceType::underlying::PAWN:
                attacks =
                    chess::attacks::pawn(
                        attacker,
                        source
                    );
                break;

            case chess::PieceType::underlying::KNIGHT:
                attacks =
                    chess::attacks::knight(source);
                break;

            case chess::PieceType::underlying::BISHOP:
                attacks =
                    chess::attacks::bishop(
                        source,
                        board.occ()
                    );
                break;

            case chess::PieceType::underlying::ROOK:
                attacks =
                    chess::attacks::rook(
                        source,
                        board.occ()
                    );
                break;

            case chess::PieceType::underlying::QUEEN:
                attacks =
                    chess::attacks::queen(
                        source,
                        board.occ()
                    );
                break;

            default:
                continue;
        }

        count +=
            (attacks & ring).count();
    }

    return count;
}


// ------------------------------------------------------------
// Count undefended squares in the king ring.
//
// An enemy attack on an undefended square is more dangerous
// than an attack on a heavily defended square.
// ------------------------------------------------------------
static int king_weak_squares(
    const chess::Board& board,
    chess::Color defending_color)
{
    chess::Color attacker =
        ~defending_color;

    chess::Bitboard ring =
        king_ring(
            board,
            defending_color
        );

    int count = 0;

    for (int i = 0; i < 64; ++i)
    {
        if (!ring.check(i))
            continue;

        chess::Square sq(i);

        // Our own occupied square is still part of the ring,
        // but only count it if attacked by the enemy.
        chess::Bitboard enemy_attackers =
            chess::attacks::attackers(
                board,
                attacker,
                sq
            );

        if (!enemy_attackers)
            continue;

        chess::Bitboard our_defenders =
            chess::attacks::attackers(
                board,
                defending_color,
                sq
            );

        if (!our_defenders)
            ++count;
    }

    return count;
}


// ------------------------------------------------------------
// King centralization / exposure.
//
// In the middlegame, a king away from its normal shelter is
// generally more exposed.
//
// In the endgame this is reversed, so we keep only a small
// positive endgame contribution for centralization.
// ------------------------------------------------------------
static int king_exposure(
    const chess::Board& board,
    chess::Color color)
{
    chess::Square king =
        board.kingSq(color);

    int file =
        static_cast<int>(king.file());

    int rank =
        static_cast<int>(king.rank());

    int relative_rank =
        color == chess::Color::WHITE
            ? rank + 1
            : 8 - rank;

    int score = 0;

    // Moving the king away from its home corner/file is
    // normally more dangerous while queens/rooks remain.
    int file_distance =
        std::abs(file - 3);

    score += file_distance * 2;

    if (relative_rank >= 4)
        score += (relative_rank - 3) * 4;

    return score;
}


// ------------------------------------------------------------
// Complete king safety score for one side.
//
// Positive result = good king safety.
// ------------------------------------------------------------
static int king_safety_side_mg(
    const chess::Board& board,
    chess::Color color)
{
    int shelter =
        king_shelter_strength(
            board,
            color
        );

    int storm =
        king_pawn_storm(
            board,
            color
        );

    int attacks =
        king_attack_count(
            board,
            color
        );

    int attack_weight =
        king_attack_weight(
            board,
            color
        );

    int weak =
        king_weak_squares(
            board,
            color
        );

    int exposure =
        king_exposure(
            board,
            color
        );

    int score = 0;

    // --------------------------------------------------------
    // Shelter.
    // --------------------------------------------------------
    score += shelter * 5;

    // --------------------------------------------------------
    // Enemy pawn storm.
    // --------------------------------------------------------
    score -= storm * 3;

    // --------------------------------------------------------
    // King-ring attacks.
    // Only meaningful when there are enough attackers.
    // --------------------------------------------------------
    if (attacks >= 2)
    {
        score -= attack_weight * 6;
        score -= attacks * 5;
        score -= weak * 8;
    }
    else if (attacks == 1)
    {
        score -= attack_weight * 2;
        score -= weak * 3;
    }

    // --------------------------------------------------------
    // Exposed king.
    // --------------------------------------------------------
    score -= exposure * 3;

    return score;
}


// ------------------------------------------------------------
// Endgame king safety.
//
// King safety largely disappears in the endgame.
//
// Instead, give a small bonus for centralization.
// ------------------------------------------------------------
static int king_safety_side_eg(
    const chess::Board& board,
    chess::Color color)
{
    chess::Square king =
        board.kingSq(color);

    int file =
        static_cast<int>(king.file());

    int rank =
        static_cast<int>(king.rank());

    int distance =
        std::max(
            std::abs(file - 3),
            std::abs(rank - 3)
        );

    // More central = better in the endgame.
    return (4 - std::min(distance, 4)) * 3;
}


// ------------------------------------------------------------
// Final White-positive MG king safety.
// ------------------------------------------------------------
int king_safety_mg(
    const chess::Board& board)
{
    return
        king_safety_side_mg(
            board,
            chess::Color::WHITE
        )
        -
        king_safety_side_mg(
            board,
            chess::Color::BLACK
        );
}


// ------------------------------------------------------------
// Final White-positive EG king safety.
// ------------------------------------------------------------
int king_safety_eg(
    const chess::Board& board)
{
    return
        king_safety_side_eg(
            board,
            chess::Color::WHITE
        )
        -
        king_safety_side_eg(
            board,
            chess::Color::BLACK
        );
}

static CombinedScores compute_all_scores(const chess::Board& board) {
    CombinedScores scores;

    for (int i = 0; i < 64; ++i) {
        chess::Square square(i);
        chess::PieceType piece = board.at<chess::PieceType>(square);
        chess::Color color = board.at(square).color();

        // Skip empty squares
        if (color == chess::Color::NONE)
            continue;

        int mg_val = piece_value_bonus(piece, true);
        int eg_val = piece_value_bonus(piece, false);
        int psqt_mg = psqt_bonus(board, square, true);
        int psqt_eg = psqt_bonus(board, square, false);

        int sign = (color == chess::Color::WHITE) ? 1 : -1;

        scores.mg_material += sign * mg_val;
        scores.eg_material += sign * eg_val;
        scores.mg_psqt += sign * psqt_mg;
        scores.eg_psqt += sign * psqt_eg;

        // Non-pawn material (absolute MG values)
        switch (piece.internal()) {
            case chess::PieceType::underlying::KNIGHT:
            case chess::PieceType::underlying::BISHOP:
            case chess::PieceType::underlying::ROOK:
            case chess::PieceType::underlying::QUEEN:
                scores.non_pawn += mg_val;
                break;
            default:
                break;
        }
    }
    scores.mg_mobility = mobility_total_mg(board);
    scores.eg_mobility = mobility_total_eg(board);
    scores.mg_pawn_structure = pawn_structure_mg(board);
    scores.eg_pawn_structure = pawn_structure_eg(board);
    scores.mg_passed =passed_pawn_mg(board);

    scores.eg_passed =passed_pawn_eg(board);

    scores.mg_bishop = bishop_eval_mg(board);

    scores.eg_bishop =bishop_eval_eg(board);

    scores.mg_rook =rook_eval_mg(board);

    scores.eg_rook =rook_eval_eg(board);
    scores.mg_king_safety =
    king_safety_mg(board);

scores.eg_king_safety =
    king_safety_eg(board);
    return scores;
}
int middlegame_score(const chess::Board& board) {
    CombinedScores scores = compute_all_scores(board);
    return scores.mg_material + scores.mg_psqt + scores.mg_mobility+scores.mg_pawn_structure+scores.mg_bishop+scores.mg_passed+scores.mg_rook+ scores.mg_king_safety;;
}

int endgame_score(const chess::Board& board) {
    CombinedScores scores = compute_all_scores(board);
    return scores.eg_material + scores.eg_psqt + scores.eg_mobility+scores.eg_pawn_structure+scores.eg_bishop+scores.eg_passed+scores.eg_rook+ scores.eg_king_safety;;
}
int phase(chess::Board& board) {
    CombinedScores scores = compute_all_scores(board);
    int npm = scores.non_pawn;
    npm = max(ENDGAME_LIMIT, min(npm, MIDGAME_LIMIT));
    return ((npm - ENDGAME_LIMIT) * PHASE_SCALE) / (MIDGAME_LIMIT - ENDGAME_LIMIT);
}
int pawn_count(chess::Board& board, chess::Color color)
{
    int count = 0;

    for (int i = 0; i < 64; i++)
    {
        chess::Square square(i);

        if (board.at(square).color() == color &&
            board.at<chess::PieceType>(square).internal() ==
            chess::PieceType::underlying::PAWN)
        {
            count++;
        }
    }

    return count;
}
int queen_count(const chess::Board& board, chess::Color color)
{
    int count = 0;

    for (int i = 0; i < 64; i++)
    {
        chess::Square square(i);

        if (board.at(square).color() == color &&
            board.at<chess::PieceType>(square).internal() ==
            chess::PieceType::underlying::QUEEN)
        {
            count++;
        }
    }

    return count;
}
int bishop_count(const chess::Board& board, chess::Color color)
{
    int count = 0;

    for (int i = 0; i < 64; i++)
    {
        chess::Square square(i);

        if (board.at(square).color() == color &&
            board.at<chess::PieceType>(square).internal() ==
            chess::PieceType::underlying::BISHOP)
        {
            count++;
        }
    }

    return count;
}
int knight_count(const chess::Board& board, chess::Color color)
{
    int count = 0;

    for (int i = 0; i < 64; i++)
    {
        chess::Square square(i);

        if (board.at(square).color() == color &&
            board.at<chess::PieceType>(square).internal() ==
            chess::PieceType::underlying::KNIGHT)
        {
            count++;
        }
    }

    return count;
}
bool opposite_bishops(const chess::Board& board)
{
    if (bishop_count(board, chess::Color::WHITE) != 1)
        return false;

    if (bishop_count(board, chess::Color::BLACK) != 1)
        return false;

    int white_color = -1;
    int black_color = -1;

    for (int i = 0; i < 64; i++)
    {
        chess::Square square(i);

        auto piece = board.at<chess::PieceType>(square);
        auto color = board.at(square).color();

        if (piece.internal() == chess::PieceType::underlying::BISHOP)
        {
            int square_color = (square.file() + square.rank()) % 2;

            if (color == chess::Color::WHITE)
                white_color = square_color;
            else if (color == chess::Color::BLACK)
                black_color = square_color;
        }
    }

    return white_color != black_color;
}
int piece_count(const chess::Board& board, chess::Color color)
{
    int count = 0;

    for (int i = 0; i < 64; i++)
    {
        chess::Square square(i);

        auto piece = board.at<chess::PieceType>(square);
        auto piece_color = board.at(square).color();

        if (piece_color == color)
        {
            count++;
        }
    }

    return count;
}



// ============================================================
// Helpers needed by Stockfish Evaluation Guide: Scale factor
// ============================================================

// Guide's piece_count(pos) with no square:
// sum 1 for every occupied square.
int piece_count(const chess::Board& board)
{
    int count = 0;

    for (int i = 0; i < 64; i++)
    {
        chess::Square square(i);

        auto piece = board.at<chess::PieceType>(square);
        chess::Color piece_color = board.at(square).color();

        if (piece_color == chess::Color::WHITE ||piece_color == chess::Color::BLACK){
            count++;
        }
    }

    return count;
}

// Same non-pawn-material function, but restricted to one color.
// This is what scale_factor needs for pos_w and pos_b.
int non_pawn_material(chess::Board& board, chess::Color color)
{
    int score = 0;

    for (int i = 0; i < 64; i++)
    {
        chess::Square square(i);

        if (board.at(square).color() != color)
            continue;

        chess::PieceType piece = board.at<chess::PieceType>(square);

        switch (piece.internal())
        {
            case chess::PieceType::underlying::KNIGHT:
            case chess::PieceType::underlying::BISHOP:
            case chess::PieceType::underlying::ROOK:
            case chess::PieceType::underlying::QUEEN:
                score += piece_value_bonus(piece, true);
                break;

            default:
                break;
        }
    }

    return score;
}


// ------------------------------------------------------------
// Convert a board square into the coordinate system used by
// the Evaluation Guide.
//
// In the guide, y = 0 is rank 8 and y = 7 is rank 1.
// For the black "colorflip" perspective, ranks are flipped.
// ------------------------------------------------------------
char guide_board_at(chess::Board& board,
                    chess::Color perspective,
                    int x,
                    int y)
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8)
        return '.';

    int actual_rank;

    if (perspective == chess::Color::WHITE)
        actual_rank = 7 - y;
    else
        actual_rank = y;

    chess::Square square(actual_rank * 8 + x);

    chess::PieceType piece = board.at<chess::PieceType>(square);

    if (board.at(square).color() == chess::Color::NONE)
        return '.';

    chess::Color actual_color = board.at(square).color();

    char c;

    switch (piece.internal())
    {
        case chess::PieceType::underlying::PAWN:   c = 'P'; break;
        case chess::PieceType::underlying::KNIGHT: c = 'N'; break;
        case chess::PieceType::underlying::BISHOP: c = 'B'; break;
        case chess::PieceType::underlying::ROOK:   c = 'R'; break;
        case chess::PieceType::underlying::QUEEN:  c = 'Q'; break;
        case chess::PieceType::underlying::KING:   c = 'K'; break;
        default: return '.';
    }

    // colorflip() changes the winning/losing colors.
    // Uppercase means the perspective side in the guide.
    if (actual_color != perspective)
        c = static_cast<char>(tolower(c));

    return c;
}


// ------------------------------------------------------------
// Supported
//
// Exact guide logic:
// if the square is not a white pawn -> 0
// otherwise count friendly pawns one rank behind on adjacent files.
// ------------------------------------------------------------
int supported(chess::Board& board,
              chess::Color perspective,
              int x,
              int y)
{
    if (guide_board_at(board, perspective, x, y) != 'P')
        return 0;

    int result = 0;

    if (guide_board_at(board, perspective, x - 1, y + 1) == 'P')
        result++;

    if (guide_board_at(board, perspective, x + 1, y + 1) == 'P')
        result++;

    return result;
}


// ------------------------------------------------------------
// Candidate passed
//
// This is a direct C++ translation of the guide's candidate_passed()
// logic. It is evaluated from one side's point of view.
// ------------------------------------------------------------
int candidate_passed(chess::Board& board, chess::Color perspective)
{
    int count = 0;

    for (int actual = 0; actual < 64; actual++)
    {
        chess::Square square(actual);

        if (board.at<chess::PieceType>(square).internal() !=
            chess::PieceType::underlying::PAWN)
            continue;

        if (board.at(square).color() != perspective)
            continue;

        int x = square.file();
        int actual_rank = square.rank();

        // Convert to the guide's coordinates.
        int y = (perspective == chess::Color::WHITE)
                    ? 7 - actual_rank
                    : actual_rank;

        int ty1 = 8;
        int ty2 = 8;
        int oy  = 8;

        // The guide code contains oy, although it is not subsequently used.
        (void)oy;

        bool same_file_pawn_ahead = false;

        for (int yy = y - 1; yy >= 0; yy--)
        {
            if (guide_board_at(board, perspective, x, yy) == 'P')
            {
                same_file_pawn_ahead = true;
                break;
            }

            if (guide_board_at(board, perspective, x, yy) == 'p')
                ty1 = yy;

            if (guide_board_at(board, perspective, x - 1, yy) == 'p' ||
                guide_board_at(board, perspective, x + 1, yy) == 'p')
                ty2 = yy;
        }

        if (same_file_pawn_ahead)
            continue;

        if (ty1 == 8 && ty2 >= y - 1)
        {
            count++;
            continue;
        }

        if (ty2 < y - 2 || ty1 < y - 1)
            continue;

        if (ty2 >= y && ty1 == y - 1 && y < 4)
        {
            if (guide_board_at(board, perspective, x - 1, y + 1) == 'P' &&
                guide_board_at(board, perspective, x - 1, y) != 'p' &&
                guide_board_at(board, perspective, x - 2, y - 1) != 'p')
            {
                count++;
                continue;
            }

            if (guide_board_at(board, perspective, x + 1, y + 1) == 'P' &&
                guide_board_at(board, perspective, x + 1, y) != 'p' &&
                guide_board_at(board, perspective, x + 2, y - 1) != 'p')
            {
                count++;
                continue;
            }
        }

        if (guide_board_at(board, perspective, x, y - 1) == 'p')
            continue;

        int lever =
            (guide_board_at(board, perspective, x - 1, y - 1) == 'p' ? 1 : 0) +
            (guide_board_at(board, perspective, x + 1, y - 1) == 'p' ? 1 : 0);

        int leverpush =
            (guide_board_at(board, perspective, x - 1, y - 2) == 'p' ? 1 : 0) +
            (guide_board_at(board, perspective, x + 1, y - 2) == 'p' ? 1 : 0);

        int phalanx =
            (guide_board_at(board, perspective, x - 1, y) == 'P' ? 1 : 0) +
            (guide_board_at(board, perspective, x + 1, y) == 'P' ? 1 : 0);

        if (lever - supported(board, perspective, x, y) > 1)
            continue;

        if (leverpush - phalanx > 0)
            continue;

        if (lever > 0 && leverpush > 0)
            continue;

        count++;
    }

    return count;
}


// ============================================================
// Scale factor
//
// Exact values/conditions from the Stockfish Evaluation Guide.
// ============================================================
int scale_factor(chess::Board& board, int eg)
{
    // Guide:
    // pos_w = eg > 0 ? pos : colorflip(pos)
    // pos_b = eg > 0 ? colorflip(pos) : pos
    chess::Color winning_side =
        (eg > 0) ? chess::Color::WHITE : chess::Color::BLACK;

    chess::Color losing_side =
        (winning_side == chess::Color::WHITE)
            ? chess::Color::BLACK
            : chess::Color::WHITE;

    int sf = 64;

    int pc_w = pawn_count(board, winning_side);
    int pc_b = pawn_count(board, losing_side);

    int qc_w = queen_count(board, winning_side);
    int qc_b = queen_count(board, losing_side);

    int bc_w = bishop_count(board, winning_side);
    int bc_b = bishop_count(board, losing_side);

    int nc_w = knight_count(board, winning_side);
    int nc_b = knight_count(board, losing_side);

    int npm_w = non_pawn_material(board, winning_side);
    int npm_b = non_pawn_material(board, losing_side);

    int bishopValueMg = 825;
    int rookValueMg = 1276;

    // Exact guide condition.
    if (pc_w == 0 && npm_w - npm_b <= bishopValueMg)
    {
        sf = npm_w < rookValueMg
                 ? 0
                 : (npm_b <= bishopValueMg ? 4 : 14);
    }

    if (sf == 64)
    {
        bool ob = opposite_bishops(board);

        if (ob &&
            npm_w == bishopValueMg &&
            npm_b == bishopValueMg)
        {
            sf = 22 + 4 * candidate_passed(board, winning_side);
        }
        else if (ob)
        {
            sf = 22 + 3 * piece_count(board);
        }
        else
        {
            if (npm_w == rookValueMg &&
                npm_b == rookValueMg &&
                pc_w - pc_b <= 1)
            {
                int pawnking_b = 0;
                int pcw_flank[2] = {0, 0};

                for (int x = 0; x < 8; x++)
                {
                    for (int y = 0; y < 8; y++)
                    {
                        if (guide_board_at(board, winning_side, x, y) == 'P')
                            pcw_flank[(x < 4) ? 1 : 0] = 1;

                        if (guide_board_at(board, losing_side, x, y) == 'K')
                        {
                            for (int ix = -1; ix <= 1; ix++)
                            {
                                for (int iy = -1; iy <= 1; iy++)
                                {
                                    if (guide_board_at(board,
                                                       losing_side,
                                                       x + ix,
                                                       y + iy) == 'P')
                                    {
                                        pawnking_b = 1;
                                    }
                                }
                            }
                        }
                    }
                }

                if (pcw_flank[0] != pcw_flank[1] && pawnking_b)
                    return 36;
            }

            if (qc_w + qc_b == 1)
            {
                sf = 37 + 3 *
                    ((qc_w == 1)
                         ? (bc_b + nc_b)
                         : (bc_w + nc_w));
            }
            else
            {
                sf = min(sf, 36 + 7 * pc_w);
            }
        }
    }

    return sf;
}


// ============================================================
// Tempo
// Exact guide value: +28 for White to move, -28 for Black to move.
// ============================================================
int tempo(chess::Board& board)
{
    return board.sideToMove() == chess::Color::WHITE ? 28 : -28;
}


// ============================================================
// Rule 50
// Exact guide returns the first move counter.
// chess.hpp exposes this as halfMoveClock().
// ============================================================
int rule50(chess::Board& board)
{
    return static_cast<int>(board.halfMoveClock());
}
// ============================================================
// MOBILITY EVALUATION
// Based on the Stockfish Evaluation Guide mobility definitions.
// ============================================================

// ------------------------------------------------------------
// Return pin direction of a piece relative to its own king.
//
// 0 = not pinned
// 1 = horizontal
// 2 = diagonal a1-h8
// 3 = vertical
// 4 = diagonal a8-h1
// ------------------------------------------------------------
static int mobility_pin_direction(const chess::Board& board,
                                  chess::Color color,
                                  chess::Square piece_square)
{
    chess::Square king = board.kingSq(color);

    int kx = king.file();
    int ky = king.rank();

    int px = piece_square.file();
    int py = piece_square.rank();

    int dx = px - kx;
    int dy = py - ky;

    int sx = 0;
    int sy = 0;

    // Vertical
    if (dx == 0 && dy != 0)
    {
        sx = 0;
        sy = (dy > 0 ? 1 : -1);
    }
    // Horizontal
    else if (dy == 0 && dx != 0)
    {
        sx = (dx > 0 ? 1 : -1);
        sy = 0;
    }
    // Diagonal
    else if (std::abs(dx) == std::abs(dy) && dx != 0)
    {
        sx = (dx > 0 ? 1 : -1);
        sy = (dy > 0 ? 1 : -1);
    }
    else
    {
        return 0;
    }

    // The potentially pinned piece must be the first piece
    // between the king and the enemy slider.
    int x = kx + sx;
    int y = ky + sy;

    while (x != px || y != py)
    {
        chess::Square sq(x * 8 + y);

        if (board.at(sq).color() != chess::Color::NONE)
            return 0;

        x += sx;
        y += sy;
    }

    // Search beyond the piece for the enemy pinning piece.
    x = px + sx;
    y = py + sy;

    while (x >= 0 && x < 8 && y >= 0 && y < 8)
    {
        chess::Square sq(x * 8 + y);

        chess::Piece p = board.at(sq);

        if (p.color() != chess::Color::NONE)
        {
            if (p.color() == ~color)
            {
                auto pt = p.type();

                bool orthogonal = (sx == 0 || sy == 0);
                bool diagonal = (sx != 0 && sy != 0);

                if (orthogonal &&
                    (pt == chess::PieceType::ROOK ||
                     pt == chess::PieceType::QUEEN))
                {
                    return sx == 0 ? 3 : 1;
                }

                if (diagonal &&
                    (pt == chess::PieceType::BISHOP ||
                     pt == chess::PieceType::QUEEN))
                {
                    return (sx == sy) ? 2 : 4;
                }
            }

            return 0;
        }

        x += sx;
        y += sy;
    }

    return 0;
}


// ------------------------------------------------------------
// Direction from source square to target square.
// Uses same numbering as mobility_pin_direction().
// ------------------------------------------------------------
static int mobility_ray_direction(chess::Square source,
                                  chess::Square target)
{
    int dx = target.file() - source.file();
    int dy = target.rank() - source.rank();

    if (dx == 0 && dy != 0)
        return 3;

    if (dy == 0 && dx != 0)
        return 1;

    if (std::abs(dx) == std::abs(dy) && dx != 0)
    {
        return (dx > 0) == (dy > 0) ? 2 : 4;
    }

    return 0;
}


// ------------------------------------------------------------
// Find pieces blocking a line between our king and an enemy
// rook/bishop/queen.
//
// These squares are excluded from the mobility area.
// ------------------------------------------------------------
static chess::Bitboard mobility_king_blockers(
    const chess::Board& board,
    chess::Color color)
{
    chess::Bitboard result;

    chess::Square king = board.kingSq(color);

    const int dirs[8][2] =
    {
        { 1, 0},
        {-1, 0},
        { 0, 1},
        { 0,-1},

        { 1, 1},
        { 1,-1},
        {-1, 1},
        {-1,-1}
    };

    for (const auto& d : dirs)
    {
        int x = king.file() + d[0];
        int y = king.rank() + d[1];

        chess::Square blocker;
        bool found_blocker = false;

        // Find first occupied square from king.
        while (x >= 0 && x < 8 && y >= 0 && y < 8)
        {
            chess::Square sq(x * 8 + y);
            if (board.at(sq).color() != chess::Color::NONE)
            {
                blocker = sq;
                found_blocker = true;
                break;
            }

            x += d[0];
            y += d[1];
        }

        // First piece must be ours.
        if (!found_blocker ||
            board.at(blocker).color() != color)
        {
            continue;
        }

        // Search beyond that piece.
        x = blocker.file() + d[0];
        y = blocker.rank() + d[1];

        while (x >= 0 && x < 8 && y >= 0 && y < 8)
        {
            chess::Square sq(x * 8 + y);
            if (board.at(sq).color() != chess::Color::NONE)
            {
                chess::Piece p = board.at(sq);
                auto pt = p.type();

                bool orthogonal =
                    (d[0] == 0 || d[1] == 0);

                bool diagonal =
                    (d[0] != 0 && d[1] != 0);

                bool is_slider = false;

                if (p.color() == ~color)
                {
                    if (orthogonal &&
                        (pt == chess::PieceType::ROOK ||
                         pt == chess::PieceType::QUEEN))
                    {
                        is_slider = true;
                    }

                    if (diagonal &&
                        (pt == chess::PieceType::BISHOP ||
                         pt == chess::PieceType::QUEEN))
                    {
                        is_slider = true;
                    }
                }

                if (is_slider)
                    result.set(blocker.index());

                break;
            }

            x += d[0];
            y += d[1];
        }
    }

    return result;
}


// ------------------------------------------------------------
// All squares attacked by enemy pawns.
// ------------------------------------------------------------
static chess::Bitboard enemy_pawn_attacks(
    const chess::Board& board,
    chess::Color enemy)
{
    chess::Bitboard result;

    chess::Bitboard pawns =
        board.pieces(chess::PieceType::PAWN, enemy);

    while (!pawns.empty())
    {
        chess::Square s(pawns.pop());

        result |= chess::attacks::pawn(enemy, s);
    }

    return result;
}


// ------------------------------------------------------------
// Mobility area.
//
// Guide rules:
//
// 1. Exclude squares protected by enemy pawns.
// 2. Exclude our king.
// 3. Exclude our queen.
// 4. Exclude our pawns on ranks 2 and 3.
// 5. Exclude our blocked pawns.
// 6. Exclude blockers for our king.
// ------------------------------------------------------------
static chess::Bitboard mobility_area(
    const chess::Board& board,
    chess::Color color)
{
    chess::Bitboard area(~0ULL);

    // Own king and queen are not part of mobility area.
    area &= ~board.pieces(
        chess::PieceType::KING,
        color
    );

    area &= ~board.pieces(
        chess::PieceType::QUEEN,
        color
    );

    // Squares protected by enemy pawns are excluded.
    area &= ~enemy_pawn_attacks(board, ~color);

    // Pieces blocking our king are excluded.
    area &= ~mobility_king_blockers(board, color);

    // Examine our pawns.
    chess::Bitboard pawns =
        board.pieces(chess::PieceType::PAWN, color);

    while (!pawns.empty())
    {
        chess::Square s(pawns.pop());

        int relative_rank =
            color == chess::Color::WHITE
                ? s.rank() + 1
                : 8 - s.rank();

        bool blocked = false;

        int forward_rank =
            color == chess::Color::WHITE
                ? s.rank() + 1
                : s.rank() - 1;

        if (forward_rank >= 0 &&
            forward_rank < 8)
        {
            chess::Square front{
                chess::File(s.file()),
                chess::Rank(forward_rank)
            };

            blocked =
                board.at(front).color() !=
                chess::Color::NONE;
        }

        // Pawns on ranks 2 and 3 are excluded.
        // Blocked pawns are also excluded.
        if (relative_rank < 4 || blocked)
            area.clear(s.index());
    }

    return area;
}


// ------------------------------------------------------------
// Knight attack.
// ------------------------------------------------------------
static bool mobility_knight_attacks(
    const chess::Board& board,
    chess::Color color,
    chess::Square source,
    chess::Square target)
{
    chess::Bitboard target_bb =
        chess::Bitboard::fromSquare(target);

    if (!(chess::attacks::knight(source) & target_bb))
        return false;

    // A pinned knight cannot move without exposing its king.
    if (mobility_pin_direction(
            board,
            color,
            source) != 0)
    {
        return false;
    }

    return true;
}


// ------------------------------------------------------------
// Bishop x-ray attack.
//
// Queens are removed from occupancy so bishops can see through
// queens, matching the Evaluation Guide definition.
// ------------------------------------------------------------
static bool mobility_bishop_attacks(
    const chess::Board& board,
    chess::Color color,
    chess::Square source,
    chess::Square target)
{
    chess::Bitboard occupied =
        board.pieces(chess::PieceType::PAWN) |
        board.pieces(chess::PieceType::KNIGHT) |
        board.pieces(chess::PieceType::BISHOP) |
        board.pieces(chess::PieceType::ROOK) |
        board.pieces(chess::PieceType::KING);

    chess::Bitboard target_bb =
        chess::Bitboard::fromSquare(target);

    if (!(chess::attacks::bishop(source, occupied) &
          target_bb))
    {
        return false;
    }

    int pin =
        mobility_pin_direction(
            board,
            color,
            source
        );

    // A pinned bishop may only move along the pin line.
    if (pin != 0 &&
        mobility_ray_direction(source, target) != pin)
    {
        return false;
    }

    return true;
}


// ------------------------------------------------------------
// Rook x-ray attack.
//
// Queens and our own rooks are removed from occupancy so that
// rook x-ray attacks can continue through them.
// ------------------------------------------------------------
static bool mobility_rook_attacks(
    const chess::Board& board,
    chess::Color color,
    chess::Square source,
    chess::Square target)
{
    chess::Bitboard occupied =
        board.pieces(chess::PieceType::PAWN) |
        board.pieces(chess::PieceType::KNIGHT) |
        board.pieces(chess::PieceType::BISHOP) |
        board.pieces(chess::PieceType::ROOK) |
        board.pieces(chess::PieceType::KING);

    occupied &= ~board.pieces(
        chess::PieceType::QUEEN
    );

    occupied &= ~board.pieces(
        chess::PieceType::ROOK,
        color
    );

    chess::Bitboard target_bb =
        chess::Bitboard::fromSquare(target);

    if (!(chess::attacks::rook(source, occupied) &
          target_bb))
    {
        return false;
    }

    int pin =
        mobility_pin_direction(
            board,
            color,
            source
        );

    // A pinned rook may only move along the pin line.
    if (pin != 0 &&
        mobility_ray_direction(source, target) != pin)
    {
        return false;
    }

    return true;
}


// ------------------------------------------------------------
// Queen attack.
// ------------------------------------------------------------
static bool mobility_queen_attacks(
    const chess::Board& board,
    chess::Color color,
    chess::Square source,
    chess::Square target)
{
    chess::Bitboard target_bb =
        chess::Bitboard::fromSquare(target);

    if (!(chess::attacks::queen(source, board.occ()) &
          target_bb))
    {
        return false;
    }

    int pin =
        mobility_pin_direction(
            board,
            color,
            source
        );

    if (pin != 0 &&
        mobility_ray_direction(source, target) != pin)
    {
        return false;
    }

    // Guide rule:
    // Queen squares defended by an opponent knight,
    // bishop or rook are ignored.
    if (board.at(target).type() ==
            chess::PieceType::QUEEN &&
        board.at(target).color() == ~color)
    {
        chess::Bitboard defenders =
            chess::attacks::attackers(
                board,
                ~color,
                target
            );

        chess::Bitboard minor_or_rook =
            board.pieces(
                chess::PieceType::KNIGHT,
                ~color
            )
            |
            board.pieces(
                chess::PieceType::BISHOP,
                ~color
            )
            |
            board.pieces(
                chess::PieceType::ROOK,
                ~color
            );

        if (defenders & minor_or_rook)
            return false;
    }

    return true;
}


// ------------------------------------------------------------
// Mobility count for one piece.
// ------------------------------------------------------------
static int mobility_count(
    const chess::Board& board,
    chess::Color color,
    chess::Square source)
{
    chess::PieceType type =
        board.at<chess::PieceType>(source);

    if (type != chess::PieceType::KNIGHT &&
        type != chess::PieceType::BISHOP &&
        type != chess::PieceType::ROOK &&
        type != chess::PieceType::QUEEN)
    {
        return 0;
    }

    chess::Bitboard area =
        mobility_area(board, color);

    int count = 0;

    for (int i = 0; i < 64; ++i)
    {
        if (!area.check(i))
            continue;

        chess::Square target(i);

        bool attacked = false;

        if (type == chess::PieceType::KNIGHT)
        {
            attacked =
                mobility_knight_attacks(
                    board,
                    color,
                    source,
                    target
                );
        }
        else if (type == chess::PieceType::BISHOP)
        {
            attacked =
                mobility_bishop_attacks(
                    board,
                    color,
                    source,
                    target
                );
        }
        else if (type == chess::PieceType::ROOK)
        {
            attacked =
                mobility_rook_attacks(
                    board,
                    color,
                    source,
                    target
                );
        }
        else
        {
            attacked =
                mobility_queen_attacks(
                    board,
                    color,
                    source,
                    target
                );
        }

        if (!attacked)
            continue;

        // Guide rule:
        // Minor pieces don't count squares occupied by
        // our own queen.
        if ((type == chess::PieceType::KNIGHT ||
             type == chess::PieceType::BISHOP) &&
            board.at(target).type() ==
                chess::PieceType::QUEEN &&
            board.at(target).color() == color)
        {
            continue;
        }

        ++count;
    }

    return count;
}


// ------------------------------------------------------------
// Mobility MG bonus tables from the Evaluation Guide.
// ------------------------------------------------------------
static int mobility_bonus_mg(
    chess::PieceType type,
    int mobility)
{
    static const int knight[] =
    {
        -62, -53, -12, -4,
          3,  13,  22, 28, 33
    };

    static const int bishop[] =
    {
        -48, -20, 16, 26, 38,
         51,  55, 63, 63, 68,
         81,  81, 91, 98
    };

    static const int rook[] =
    {
        -60, -20, 2, 3, 3,
         11, 22, 31, 40, 40,
         41, 48, 57, 57, 62
    };

    static const int queen[] =
    {
        -30, -12, -8, -9, 20,
         23,  23, 35, 38, 53,
         64,  65, 65, 66, 67,
         67,  72, 72, 77, 79,
         93, 108,108,108,110,
        114, 114,116
    };

    switch (type.internal())
    {
        case chess::PieceType::underlying::KNIGHT:
            return knight[std::min(mobility, 8)];

        case chess::PieceType::underlying::BISHOP:
            return bishop[std::min(mobility, 13)];

        case chess::PieceType::underlying::ROOK:
            return rook[std::min(mobility, 14)];

        case chess::PieceType::underlying::QUEEN:
            return queen[std::min(mobility, 26)];

        default:
            return 0;
    }
}


// ------------------------------------------------------------
// Mobility EG bonus tables from the Evaluation Guide.
// ------------------------------------------------------------
static int mobility_bonus_eg(
    chess::PieceType type,
    int mobility)
{
    static const int knight[] =
    {
        -81, -56, -31, -16,
           5,  11,  17,  20, 25
    };

    static const int bishop[] =
    {
        -59, -23, -3, 13, 24,
         42,  54, 57, 65, 73,
         78,  86, 88, 97
    };

    static const int rook[] =
    {
        -78, -17, 23, 39, 70,
         99, 103,121,134,139,
        158,164,168,169,172
    };

    static const int queen[] =
    {
        -48,-30,-7,19,40,
         55,59,75,78,96,
         96,100,121,127,131,
        133,136,141,147,150,
        151,168,168,171,182,
        182,192,219
    };

    switch (type.internal())
    {
        case chess::PieceType::underlying::KNIGHT:
            return knight[std::min(mobility, 8)];

        case chess::PieceType::underlying::BISHOP:
            return bishop[std::min(mobility, 13)];

        case chess::PieceType::underlying::ROOK:
            return rook[std::min(mobility, 14)];

        case chess::PieceType::underlying::QUEEN:
            return queen[std::min(mobility, 27)];

        default:
            return 0;
    }
}


// ------------------------------------------------------------
// Mobility score for one color.
// ------------------------------------------------------------
static int mobility_score(
    const chess::Board& board,
    chess::Color color,
    bool mg)
{
    int score = 0;

    for (int i = 0; i < 64; ++i)
    {
        chess::Square square(i);

        if (board.at(square).color() != color)
            continue;

        chess::PieceType type =
            board.at<chess::PieceType>(square);

        if (type != chess::PieceType::KNIGHT &&
            type != chess::PieceType::BISHOP &&
            type != chess::PieceType::ROOK &&
            type != chess::PieceType::QUEEN)
        {
            continue;
        }

        int mobility =
            mobility_count(
                board,
                color,
                square
            );

        score +=
            mg
                ? mobility_bonus_mg(type, mobility)
                : mobility_bonus_eg(type, mobility);
    }

    return score;
}


// ------------------------------------------------------------
// White-only values, matching the guide's orientation.
// ------------------------------------------------------------
int mobility_mg(const chess::Board& board)
{
    return mobility_score(
        board,
        chess::Color::WHITE,
        true
    );
}

int mobility_eg(const chess::Board& board)
{
    return mobility_score(
        board,
        chess::Color::WHITE,
        false
    );
}


// ------------------------------------------------------------
// Final White - Black mobility contribution.
// This is what your evaluator should add.
// ------------------------------------------------------------
int mobility_total_mg(const chess::Board& board)
{
    return mobility_score(
               board,
               chess::Color::WHITE,
               true
           )
         - mobility_score(
               board,
               chess::Color::BLACK,
               true
           );
}

int mobility_total_eg(const chess::Board& board)
{
    return mobility_score(
               board,
               chess::Color::WHITE,
               false
           )
         - mobility_score(
               board,
               chess::Color::BLACK,
               false
           );
}

// ============================================================
// Complete Main Evaluation
//
// This follows the guide's main_evaluation() order:
// 1. MG score
// 2. EG score
// 3. phase
// 4. scale EG
// 5. tapered interpolation
// 6. round to multiple of 16
// 7. tempo
// 8. Rule-50 scaling
// 9. mobility check
// ============================================================
int evaluate(chess::Board& board)
{
    int mg = middlegame_score(board);
    int eg = endgame_score(board);
    int p  = phase(board);

    eg = eg * scale_factor(board, eg) / 64;

    int value =
        (mg * p + (eg * (128 - p))) / 128;

    // main_evaluation(pos) is called with one argument,
    // so the guide performs this rounding.
    value = (value / 16) * 16;

    value += tempo(board);

    value = value * (100 - rule50(board)) / 100;

    return value;
}


// ============================================================
// Test
// ============================================================
