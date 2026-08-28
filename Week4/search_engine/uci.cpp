#include "search.h"
#include "../chess.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <streambuf>
#include <vector>

// ============================================================
// NeuralGambit UCI Frontend
//
// Supported:
//   uci
//   isready
//   ucinewgame
//   position startpos [moves ...]
//   position fen <6 FEN fields> [moves ...]
//   go depth N
//   go movetime N
//   go wtime N btime N winc N binc N movestogo N
//   go infinite
//   quit
//   stop
//
// "stop" is accepted but cannot interrupt an already-running
// synchronous findBestMove(). True asynchronous stop requires
// the search layer itself to expose a thread-safe stop flag.
// ============================================================

// ============================================================
// Suppress internal search.cpp stdout.
//
// stdout belongs to UCI.
// Search diagnostics must not corrupt the protocol.
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
// Utility
// ============================================================

static std::string toLower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(c)
            )
        );

    return s;
}

static bool parseInt64(
    const std::string& text,
    std::int64_t& value)
{
    try
    {
        std::size_t used = 0;

        const long long parsed =
            std::stoll(text, &used);

        if (used != text.size())
            return false;

        value =
            static_cast<std::int64_t>(
                parsed
            );

        return true;
    }
    catch (...)
    {
        return false;
    }
}

static int clampToInt(
    std::int64_t value)
{
    if (value < 0)
        return 0;

    if (value >
        static_cast<std::int64_t>(
            INT32_MAX
        ))
    {
        return INT32_MAX;
    }

    return static_cast<int>(value);
}

// ============================================================
// Move -> UCI
// ============================================================

static std::string moveToUci(
    const chess::Move& move)
{
    if (move == chess::Move::NO_MOVE)
        return "0000";

    auto squareName = [](int sq)
    {
        std::string s(2, 'a');

        s[0] = static_cast<char>(
            'a' + (sq % 8)
        );

        s[1] = static_cast<char>(
            '1' + (sq / 8)
        );

        return s;
    };

    std::string result;

    result.reserve(5);

    result +=
        squareName(
            move.from().index()
        );

    result +=
        squareName(
            move.to().index()
        );

    if (move.typeOf() ==
        chess::Move::PROMOTION)
    {
        switch (
            static_cast<int>(
                move.promotionType()
            ))
        {
        case static_cast<int>(
            chess::PieceType::KNIGHT):
            result += 'n';
            break;

        case static_cast<int>(
            chess::PieceType::BISHOP):
            result += 'b';
            break;

        case static_cast<int>(
            chess::PieceType::ROOK):
            result += 'r';
            break;

        case static_cast<int>(
            chess::PieceType::QUEEN):
            result += 'q';
            break;

        default:
            break;
        }
    }

    return result;
}

// ============================================================
// UCI -> legal chess::Move
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
// Apply moves after "moves"
// ============================================================

static bool applyMoves(
    chess::Board& board,
    std::istringstream& iss)
{
    std::string token;

    if (!(iss >> token))
        return true;

    if (toLower(token) != "moves")
        return true;

    std::string moveText;

    while (iss >> moveText)
    {
        chess::Move move =
            parseUciMove(
                board,
                moveText
            );

        if (move == chess::Move::NO_MOVE)
        {
            std::cout
                << "info string invalid move "
                << moveText
                << '\n';

            std::cout.flush();

            return false;
        }

        board.makeMove(move);
    }

    return true;
}

// ============================================================
// position command
// ============================================================

static bool handlePosition(
    chess::Board& board,
    std::istringstream& iss)
{
    std::string mode;

    if (!(iss >> mode))
    {
        std::cout
            << "info string position command missing argument\n";

        std::cout.flush();

        return false;
    }

    mode = toLower(mode);

    // --------------------------------------------------------
    // position startpos [moves ...]
    // --------------------------------------------------------

    if (mode == "startpos")
    {
        board = chess::Board();

        return applyMoves(
            board,
            iss
        );
    }

    // --------------------------------------------------------
    // position fen <6 fields> [moves ...]
    // --------------------------------------------------------

    if (mode == "fen")
    {
        std::string fen;
        std::string field;

        // FEN = exactly six fields:
        //
        // placement
        // side
        // castling
        // en-passant
        // halfmove
        // fullmove

        for (int i = 0; i < 6; ++i)
        {
            if (!(iss >> field))
            {
                std::cout
                    << "info string incomplete FEN\n";

                std::cout.flush();

                return false;
            }

            if (i != 0)
                fen += ' ';

            fen += field;
        }

        try
        {
            board.setFen(fen);
        }
        catch (...)
        {
            std::cout
                << "info string invalid FEN\n";

            std::cout.flush();

            return false;
        }

        return applyMoves(
            board,
            iss
        );
    }

    std::cout
        << "info string unknown position type "
        << mode
        << '\n';

    std::cout.flush();

    return false;
}

// ============================================================
// Main UCI loop
// ============================================================

int main()
{
    chess::Board board;
    search::SearchStats stats;

    // Persisted UCI clock state.
    //
    // These are updated by every go command.
    std::int64_t whiteTimeMs = 0;
    std::int64_t blackTimeMs = 0;
    std::int64_t whiteIncrementMs = 0;
    std::int64_t blackIncrementMs = 0;
    std::int64_t movesToGo = 0;

    bool uciInitialized = false;

    std::string line;

    while (std::getline(
        std::cin,
        line))
    {
        if (line.empty())
            continue;

        std::istringstream iss(line);

        std::string command;

        if (!(iss >> command))
            continue;

        command = toLower(command);

        // ====================================================
        // UCI IDENTIFICATION
        // ====================================================

        if (command == "uci")
        {
            std::cout
                << "id name NeuralGambit\n";

            std::cout
                << "id author Dev\n";

            // We intentionally advertise only capabilities
            // that this current implementation genuinely has.
            std::cout
                << "uciok\n";

            std::cout.flush();

            uciInitialized = true;
        }

        // ====================================================
        // READY
        // ====================================================

        else if (command == "isready")
        {
            std::cout
                << "readyok\n";

            std::cout.flush();
        }

        // ====================================================
        // NEW GAME
        // ====================================================

        else if (command == "ucinewgame")
        {
            board = chess::Board();
            stats = search::SearchStats{};

            whiteTimeMs = 0;
            blackTimeMs = 0;

            whiteIncrementMs = 0;
            blackIncrementMs = 0;

            movesToGo = 0;
        }

        // ====================================================
        // POSITION
        // ====================================================

        else if (command == "position")
        {
            handlePosition(
                board,
                iss
            );
        }

        // ====================================================
        // GO
        // ====================================================

        else if (command == "go")
        {
            search::SearchLimits limits;

            // Default: search until the search implementation's
            // normal stopping condition.
            int depth = 64;

            bool haveMoveTime = false;
            bool haveClock = false;

            std::int64_t moveTimeMs = 0;

            std::string token;

            while (iss >> token)
            {
                token = toLower(token);

                std::string valueText;

                // --------------------------------------------
                // depth
                // --------------------------------------------

                if (token == "depth")
                {
                    if (!(iss >> valueText))
                    {
                        std::cout
                            << "info string missing depth value\n";

                        continue;
                    }

                    std::int64_t value;

                    if (parseInt64(
                            valueText,
                            value))
                    {
                        depth =
                            std::max(
                                1,
                                clampToInt(value)
                            );
                    }
                }

                // --------------------------------------------
                // movetime
                // --------------------------------------------

                else if (token == "movetime")
                {
                    if (!(iss >> valueText))
                        continue;

                    if (parseInt64(
                            valueText,
                            moveTimeMs))
                    {
                        haveMoveTime = true;
                    }
                }

                // --------------------------------------------
                // wtime
                // --------------------------------------------

                else if (token == "wtime")
                {
                    if (!(iss >> valueText))
                        continue;

                    std::int64_t value;

                    if (parseInt64(
                            valueText,
                            value))
                    {
                        whiteTimeMs =
                            std::max<std::int64_t>(
                                0,
                                value
                            );

                        haveClock = true;
                    }
                }

                // --------------------------------------------
                // btime
                // --------------------------------------------

                else if (token == "btime")
                {
                    if (!(iss >> valueText))
                        continue;

                    std::int64_t value;

                    if (parseInt64(
                            valueText,
                            value))
                    {
                        blackTimeMs =
                            std::max<std::int64_t>(
                                0,
                                value
                            );

                        haveClock = true;
                    }
                }

                // --------------------------------------------
                // winc
                // --------------------------------------------

                else if (token == "winc")
                {
                    if (!(iss >> valueText))
                        continue;

                    std::int64_t value;

                    if (parseInt64(
                            valueText,
                            value))
                    {
                        whiteIncrementMs =
                            std::max<std::int64_t>(
                                0,
                                value
                            );
                    }
                }

                // --------------------------------------------
                // binc
                // --------------------------------------------

                else if (token == "binc")
                {
                    if (!(iss >> valueText))
                        continue;

                    std::int64_t value;

                    if (parseInt64(
                            valueText,
                            value))
                    {
                        blackIncrementMs =
                            std::max<std::int64_t>(
                                0,
                                value
                            );
                    }
                }

                // --------------------------------------------
                // movestogo
                // --------------------------------------------

                else if (token == "movestogo")
                {
                    if (!(iss >> valueText))
                        continue;

                    std::int64_t value;

                    if (parseInt64(
                            valueText,
                            value))
                    {
                        movesToGo =
                            std::max<std::int64_t>(
                                0,
                                value
                            );
                    }
                }

                // --------------------------------------------
                // infinite
                // --------------------------------------------

                else if (token == "infinite")
                {
                    limits.infinite = true;
                }

                // --------------------------------------------
                // ponder
                //
                // We accept the keyword so a GUI doesn't treat
                // it as an unknown command, but this search
                // currently has no ponder implementation.
                // --------------------------------------------

                else if (token == "ponder")
                {
                    std::cout
                        << "info string ponder requested; "
                           "ponder mode is not implemented\n";
                }

                // --------------------------------------------
                // unsupported UCI search controls
                // --------------------------------------------

                else if (
                    token == "nodes" ||
                    token == "mate" ||
                    token == "searchmoves")
                {
                    std::cout
                        << "info string unsupported go option "
                        << token
                        << '\n';

                    // searchmoves needs a list parser, so we
                    // deliberately don't pretend to implement it.
                    if (token == "searchmoves")
                    {
                        while (iss >> valueText)
                        {
                            // Consume the rest of the command.
                            // The current search API has no
                            // searchmoves field.
                        }
                    }
                }
            }

            // =================================================
            // Build SearchLimits
            // =================================================

            if (haveMoveTime)
            {
                limits.moveTimeMs =
                    clampToInt(
                        moveTimeMs
                    );
            }
            else if (haveClock)
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
                    clampToInt(
                        myTime
                    );

                limits.incrementMs =
                    clampToInt(
                        myIncrement
                    );

                limits.movesToGo =
                    clampToInt(
                        movesToGo
                    );
            }

            // =================================================
            // Search
            // =================================================

            // search::SearchStats stats;

            NullBuffer nullBuffer;

            std::streambuf* oldCout =
                std::cout.rdbuf(
                    &nullBuffer
                );

            const chess::Move best =
                search::findBestMove(
                    board,
                    depth,
                    stats,
                    limits
                );

            std::cout.rdbuf(
                oldCout
            );

            // =================================================
            // Required UCI response
            // =================================================

            std::cout
                << "bestmove "
                << moveToUci(best)
                << '\n';

            std::cout.flush();
        }

        // ====================================================
        // STOP
        // ====================================================

        else if (command == "stop")
        {
            // IMPORTANT:
            //
            // findBestMove() is synchronous in the current
            // search architecture. Therefore the main UCI
            // thread cannot receive/process "stop" while a
            // search is executing.
            //
            // This command is accepted for protocol
            // compatibility, but true interruption must be
            // implemented in the search layer.
        }

        // ====================================================
        // DEBUG
        // ====================================================

        else if (command == "debug")
        {
            std::string mode;

            if (iss >> mode)
            {
                mode = toLower(mode);

                if (mode == "on" || mode == "off")
                {
                    std::cout
                        << "info string debug "
                        << mode
                        << '\n';

                    std::cout.flush();
                }
            }
        }

        // ====================================================
        // QUIT
        // ====================================================

        else if (command == "quit")
        {
            break;
        }

        // ====================================================
        // Unknown command
        // ====================================================

        else
        {
            if (uciInitialized)
            {
                std::cout
                    << "info string unknown command "
                    << command
                    << '\n';

                std::cout.flush();
            }
        }
    }

    return 0;
}