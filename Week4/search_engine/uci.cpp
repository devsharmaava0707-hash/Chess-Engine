#include "search.h"
#include "../chess.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <streambuf>
#include <algorithm>
#include <cstdint>

// ============================================================
// Suppress search.cpp debug output.
// UCI protocol must use stdout only.
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
// UCI move formatting
// ============================================================

static std::string moveToUci(const chess::Move& move)
{
    if (move == chess::Move::NO_MOVE)
        return "0000";

    auto squareName = [](int sq)
    {
        std::string s(2, 'a');
        s[0] = static_cast<char>('a' + (sq % 8));
        s[1] = static_cast<char>('1' + (sq / 8));
        return s;
    };

    std::string result =
        squareName(move.from().index()) +
        squareName(move.to().index());

    if (move.typeOf() == chess::Move::PROMOTION)
    {
        switch (static_cast<int>(move.promotionType()))
        {
        case static_cast<int>(chess::PieceType::KNIGHT):
            result += 'n';
            break;

        case static_cast<int>(chess::PieceType::BISHOP):
            result += 'b';
            break;

        case static_cast<int>(chess::PieceType::ROOK):
            result += 'r';
            break;

        case static_cast<int>(chess::PieceType::QUEEN):
            result += 'q';
            break;

        default:
            break;
        }
    }

    return result;
}

// ============================================================
// Find a legal chess::Move matching UCI text.
// ============================================================

static chess::Move parseUciMove(
    chess::Board& board,
    const std::string& text)
{
    chess::Movelist legalMoves;

    chess::movegen::legalmoves(
        legalMoves,
        board
    );

    for (const auto& move : legalMoves)
    {
        if (moveToUci(move) == text)
            return move;
    }

    return chess::Move::NO_MOVE;
}

// ============================================================
// Apply all moves after "moves".
// ============================================================

static void applyMoves(
    chess::Board& board,
    std::istringstream& iss)
{
    std::string token;

    if (!(iss >> token))
        return;

    if (token != "moves")
        return;

    std::string uciMove;

    while (iss >> uciMove)
    {
        chess::Move move =
            parseUciMove(board, uciMove);

        if (move == chess::Move::NO_MOVE)
        {
            std::cerr
                << "info string invalid move "
                << uciMove
                << '\n';
            return;
        }

        board.makeMove(move);
    }
}

// ============================================================
// UCI "position" command.
// Supports:
//
// position startpos
// position startpos moves e2e4 e7e5 ...
//
// position fen <6 fen fields>
// position fen <6 fen fields> moves ...
// ============================================================

static void handlePosition(
    chess::Board& board,
    std::istringstream& iss)
{
    std::string token;

    if (!(iss >> token))
        return;

    if (token == "startpos")
    {
        board = chess::Board();

        applyMoves(board, iss);
        return;
    }

    if (token == "fen")
    {
        std::string fen;
        std::string field;

        // FEN consists of exactly 6 fields.
        for (int i = 0; i < 6; ++i)
        {
            if (!(iss >> field))
                return;

            if (i > 0)
                fen += ' ';

            fen += field;
        }

        board.setFen(fen);

        applyMoves(board, iss);
        return;
    }
}

// ============================================================
// Main UCI loop
// ============================================================

int main()
{
    chess::Board board;

    // UCI clock state.
    std::int64_t whiteTimeMs = 0;
    std::int64_t blackTimeMs = 0;
    std::int64_t whiteIncrementMs = 0;
    std::int64_t blackIncrementMs = 0;
    std::int64_t movesToGo = 0;

    std::string line;

    while (std::getline(std::cin, line))
    {
        std::istringstream iss(line);

        std::string command;

        if (!(iss >> command))
            continue;

        // ----------------------------------------------------
        // uci
        // ----------------------------------------------------

        if (command == "uci")
        {
            std::cout
                << "id name NeuralGambit\n";

            std::cout
                << "id author Dev\n";

            std::cout
                << "uciok\n";

            std::cout.flush();
        }

        // ----------------------------------------------------
        // isready
        // ----------------------------------------------------

        else if (command == "isready")
        {
            std::cout
                << "readyok\n";

            std::cout.flush();
        }

        // ----------------------------------------------------
        // ucinewgame
        // ----------------------------------------------------

        else if (command == "ucinewgame")
        {
            board = chess::Board();

            whiteTimeMs = 0;
            blackTimeMs = 0;
            whiteIncrementMs = 0;
            blackIncrementMs = 0;
            movesToGo = 0;
        }

        // ----------------------------------------------------
        // position
        // ----------------------------------------------------

        else if (command == "position")
        {
            handlePosition(board, iss);
        }

        // ----------------------------------------------------
        // go
        // ----------------------------------------------------

        else if (command == "go")
        {
            search::SearchLimits limits;

            int depth = 64;

            bool hasMoveTime = false;
            bool hasClock = false;

            std::int64_t moveTimeMs = 0;

            std::string token;

            while (iss >> token)
            {
                if (token == "depth")
                {
                    iss >> depth;
                }
                else if (token == "movetime")
                {
                    iss >> moveTimeMs;
                    hasMoveTime = true;
                }
                else if (token == "wtime")
                {
                    iss >> whiteTimeMs;
                    hasClock = true;
                }
                else if (token == "btime")
                {
                    iss >> blackTimeMs;
                    hasClock = true;
                }
                else if (token == "winc")
                {
                    iss >> whiteIncrementMs;
                }
                else if (token == "binc")
                {
                    iss >> blackIncrementMs;
                }
                else if (token == "movestogo")
                {
                    iss >> movesToGo;
                }
                else if (token == "infinite")
                {
                    limits.infinite = true;
                }
            }

            // ------------------------------------------------
            // Set limits understood by your current search.
            //
            // Your SearchLimits already has:
            //   moveTimeMs
            //   myTimeMs
            //   incrementMs
            //   movesToGo
            //   infinite
            //
            // We map UCI's side-specific clock to the current
            // side to move.
            // ------------------------------------------------

            if (hasMoveTime)
            {
                limits.moveTimeMs =
                    static_cast<int>(
                        std::max<std::int64_t>(
                            0,
                            moveTimeMs
                        )
                    );
            }
            else if (hasClock)
            {
                const bool whiteToMove =
                    board.sideToMove() ==
                    chess::Color::WHITE;

                const std::int64_t myTime =
                    whiteToMove
                        ? whiteTimeMs
                        : blackTimeMs;

                const std::int64_t myIncrement =
                    whiteToMove
                        ? whiteIncrementMs
                        : blackIncrementMs;

                limits.myTimeMs =
                    static_cast<int>(
                        std::max<std::int64_t>(
                            0,
                            myTime
                        )
                    );

                limits.incrementMs =
                    static_cast<int>(
                        std::max<std::int64_t>(
                            0,
                            myIncrement
                        )
                    );

                limits.movesToGo =
                    static_cast<int>(
                        std::max<std::int64_t>(
                            0,
                            movesToGo
                        )
                    );
            }

            // ------------------------------------------------
            // Run search.
            //
            // We suppress internal debug output so stdout
            // remains a valid UCI stream.
            // ------------------------------------------------

            search::SearchStats stats;

            NullBuffer nullBuffer;

            std::streambuf* oldCout =
                std::cout.rdbuf(&nullBuffer);

            const chess::Move best =
                search::findBestMove(
                    board,
                    depth,
                    stats,
                    limits
                );

            std::cout.rdbuf(oldCout);

            // ------------------------------------------------
            // UCI bestmove
            // ------------------------------------------------

            std::cout
                << "bestmove "
                << moveToUci(best)
                << '\n';

            std::cout.flush();
        }

        // ----------------------------------------------------
        // stop
        //
        // Your current findBestMove() is synchronous, so this
        // cannot interrupt a search already in progress.
        // It is accepted so GUIs do not get an "unknown
        // command" problem. Real async stop comes later.
        // ----------------------------------------------------

        else if (command == "stop")
        {
            // No action yet.
        }

        // ----------------------------------------------------
        // quit
        // ----------------------------------------------------

        else if (command == "quit")
        {
            break;
        }

        // ----------------------------------------------------
        // debug command for manual testing
        // ----------------------------------------------------

        else if (command == "d")
        {
            std::cerr
                << "FEN: "
                << board.getFen()
                << '\n';
        }
    }

    return 0;
}