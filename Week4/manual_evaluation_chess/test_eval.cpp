#include <iostream>
#include "chess.hpp"
#include "eval.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " \"<FEN>\"\n";
        return 1;
    }

    std::string fen = argv[1];
    chess::Board board(fen);
    int score = evaluate(board);
    std::cout << "Score: " << score << " cp\n";
    return 0;
}