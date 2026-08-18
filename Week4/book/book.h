#pragma once // avoid dublicating particular files
#include <vector>
#include "../chess.hpp"
#include <cstdint> // this helps us to take our won type data type like uint64_t
#include<string>
/*
Explanation of each function used here

1) pieceCode:
    What it does:
    It takes one chess piece as input and returns the Polyglot code
    representing that piece and its color to use later.

    How??
    It first checks the type of the piece (pawn, knight, bishop, rook,
    queen, king) and assigns the corresponding Polyglot number.
    Then, if the piece is white, it adds 1 to the number so that every
    piece type has one code for black and one for white.

    Example:
    black pawn   -> 0
    white pawn   -> 1
    black knight -> 2
    white knight -> 3
    ...
    black king   -> 10
    white king   -> 11

    This number is later used with the square number to find the correct
    random value in the Polyglot Zobrist table.


2) polyglotKey:
    What it does:
    It takes a complete chess board and calculates its Polyglot key,
    which is a 64-bit number used to identify that exact chess position
    inside the opening book (.bin file).

    How??
    It goes through all 64 squares of the board.
    For every square, it checks whether there is a piece there.
    If there is a piece, pieceCode() gives its Polyglot code.

    Then it calculates:
        index = 64 * pieceCode + square

    Using this index, it directly takes the corresponding random 64-bit
    number from zobrist_keys.hpp and XORs it into the key.

    After processing all pieces, it also XORs different random numbers
    for:
        - castling rights
        - en-passant information
        - side to move

    The final 64-bit value is the Polyglot key.

    This key is then used to search for the current position in komodo.bin.


3) decodePolyglotMove:
    What it does:
    It takes the 16-bit move stored inside a Polyglot book entry and
    converts it into the chess library's actual Move object.

    How??
    The 16-bit number contains information about:
        - destination file
        - destination rank
        - source file
        - source rank
        - promotion piece

    The function extracts these values using bit operations and creates
    the corresponding source and destination squares.

    It then generates all legal moves for the current board and checks
    which legal move matches the encoded information.

    This is especially useful for correctly handling promotions and
    castling, because Polyglot's move representation can differ from
    the chess library's internal Move representation.

    Finally, it returns the corresponding legal chess::Move.
    If no matching legal move is found, it returns Move::NO_MOVE.


4) PolyglotBook::load:
    What it does:
    It loads the Polyglot opening-book .bin file into memory.

    How??
    It opens the binary file and reads it 16 bytes at a time because
    every Polyglot entry occupies exactly 16 bytes.

    Each entry contains:
        - 8 bytes -> position key
        - 2 bytes -> packed move
        - 2 bytes -> move weight
        - 4 bytes -> learn value (not used by our engine)

    The function converts the bytes from the file into a PolyglotEntry
    object and stores it inside entries_.

    After reading the complete file, the entries are sorted according
    to their key so that we can later use binary search to quickly find
    all entries belonging to a particular position.

    It returns true if the book was successfully loaded and contains
    entries, otherwise false.


5) PolyglotBook::probe:
    What it does:
    It takes the current chess board and returns a book move for that
    position.

    How??
    First, it calculates the Polyglot key of the current board using
    polyglotKey().

    It then uses lower_bound() on the sorted entries_ vector to find
    the first book entry having that key.

    After finding the first matching entry, it continues forward until
    the key changes, giving us all book moves available for this position.

    For each matching entry, the stored move is converted into an actual
    chess::Move using decodePolyglotMove().

    In our implementation, we find the move(s) with the highest weight.
    If multiple legal moves have the same highest weight, we randomly
    choose between those moves.

    If the position does not exist in the opening book, or no legal book
    move can be decoded, the function returns Move::NO_MOVE.
*/
namespace book
{
    uint64_t polyglotKey(const chess::Board& board); // we use uint64_t as it means unsihned integar 64 type 
    chess::Move decodePolyglotMove(const chess::Board& board,uint16_t packed);
    struct PolyglotEntry
    {
        uint64_t key;
        uint16_t move;
        uint16_t weight;
    };
    class PolyglotBook
    {
        public:
        bool load(const std::string& path);
        // size_t size() const { return entries_.size(); }
        chess::Move probe(const chess::Board& board) const;
        private:
        std::vector<PolyglotEntry> entries_;
    };
}
