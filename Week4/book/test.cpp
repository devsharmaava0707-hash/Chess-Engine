#include "book.h"
#include <iostream>

int main()
{
    book::PolyglotBook book;

    bool loaded = book.load("../books/komodo.bin");

    if (!loaded)
    {
        std::cout << "Book failed to load!\n";
        return 1;
    }

    std::cout << "Book loaded successfully!\n";
    // std::cout << "Entries: " << book.size() << '\n';

    chess::Board board;

    chess::Move move = book.probe(board);

    if (move == chess::Move::NO_MOVE)
    {
        std::cout << "No book move found!\n";
    }
    else
    {
        std::cout << "Book move found!\n";
        std::cout << "From square: " << move.from().index() << '\n';
        std::cout << "To square: " << move.to().index() << '\n';
    }

    return 0;
}