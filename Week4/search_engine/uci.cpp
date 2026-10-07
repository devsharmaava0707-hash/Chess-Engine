#include "search.h"
#include "../chess.hpp"
#include "../arun_eval/eval.h"
#include "../tablebase/tbprobe.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <memory>
#include <cstring>
// ============================================================
// NeuralGambit UCI Frontend (advanced)
//
// Supported:
//   uci
//   isready                     (answered immediately, even mid-search)
//   ucinewgame
//   position startpos [moves ...]
//   position fen <6 FEN fields> [moves ...]
//   go depth N
//   go movetime N
//   go nodes N
//   go wtime N btime N winc N binc N movestogo N
//   go infinite
//   go ponder                   (see note below)
//   ponderhit
//   stop                        (genuinely interrupts a running search)
//   setoption name Hash value N
//   setoption name Threads value N        (accepted; only 1 honored)
//   setoption name Move Overhead value N
//   setoption name Clear Hash
//   setoption name Ponder value <true|false>
//   eval                        (prints the static evaluation of the
//                                current position; non-standard, but
//                                widely supported by engines for
//                                debugging)
//   d                           (prints the FEN of the current
//                                position; non-standard debug aid)
//   debug on|off
//   quit
//
// Search now runs on a dedicated worker thread. "stop" sets an
// atomic flag the search thread checks periodically, so it can
// genuinely interrupt a running search instead of only being
// accepted for protocol compatibility.
//
// Pondering is supported in the simplest correct form: "go ...
// ponder" runs an unbounded search (like "go infinite"), and the
// GUI is expected to send "stop" at the point it would normally
// have stopped a timed search. "ponderhit" is acknowledged but
// does not switch to a dynamically-computed time budget mid-search
// -- this mirrors how many engines implement minimal ponder support.
// ============================================================

namespace
{
    std::mutex stdoutMutex;

    void printLine(const std::string& text)
    {
        std::lock_guard<std::mutex> lock(stdoutMutex);
        std::cout << text << '\n';
        std::cout.flush();
    }

    std::string toLower(std::string s)
    {
        for (char& c : s)
            c = static_cast<char>(
                std::tolower(
                    static_cast<unsigned char>(c)
                )
            );

        return s;
    }

    std::string trim(const std::string& s)
    {
        size_t start = s.find_first_not_of(" \t\r\n");

        if (start == std::string::npos)
            return "";

        size_t end = s.find_last_not_of(" \t\r\n");

        return s.substr(start, end - start + 1);
    }

    bool parseInt64(
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

    int clampToInt(
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

    // ========================================================
    // Move -> UCI
    // ========================================================

    std::string moveToUci(const chess::Move& move)
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

    std::string result;
    result.reserve(5);

    result += squareName(move.from().index());

    if (move.typeOf() == chess::Move::CASTLING)
    {
        // to() is the rook's square in chess.hpp; convert to the
        // king's actual UCI destination square.
        bool kingside = move.to() > move.from();
        int fromRank = move.from().index() / 8;
        int destFile = kingside ? 6 : 2; // g-file or c-file
        int destSq = fromRank * 8 + destFile;
        result += squareName(destSq);
    }
    else
    {
        result += squareName(move.to().index());
    }

    if (move.typeOf() == chess::Move::PROMOTION)
    {
        switch (static_cast<int>(move.promotionType()))
        {
        case static_cast<int>(chess::PieceType::KNIGHT): result += 'n'; break;
        case static_cast<int>(chess::PieceType::BISHOP): result += 'b'; break;
        case static_cast<int>(chess::PieceType::ROOK):   result += 'r'; break;
        case static_cast<int>(chess::PieceType::QUEEN):  result += 'q'; break;
        default: break;
        }
    }

    return result;
}

    // ========================================================
    // UCI -> legal chess::Move
    // ========================================================

    chess::Move parseUciMove(
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

    // ========================================================
    // Apply moves after "moves"
    // ========================================================

    bool applyMoves(
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
                printLine(
                    "info string invalid move " + moveText
                );

                return false;
            }

            board.makeMove(move);
        }

        return true;
    }
    // ========================================================
    // PERFT
    // ========================================================

    std::uint64_t perft(
        chess::Board& board,
        int depth)
    {
        if (depth == 0)
            return 1;

        chess::Movelist moves;

        chess::movegen::legalmoves(
            moves,
            board
        );

        if (depth == 1)
            return static_cast<std::uint64_t>(
                moves.size()
            );

        std::uint64_t nodes = 0;

        for (const auto& move : moves)
        {
            board.makeMove(move);

            nodes += perft(
                board,
                depth - 1
            );

            board.unmakeMove(move);
        }

        return nodes;
    }

    // ========================================================
    // position command
    // ========================================================

    bool handlePosition(
        chess::Board& board,
        std::istringstream& iss)
    {
        std::string mode;

        if (!(iss >> mode))
        {
            printLine("info string position command missing argument");
            return false;
        }

        mode = toLower(mode);

        if (mode == "startpos")
        {
            board = chess::Board();
            return applyMoves(board, iss);
        }

        if (mode == "fen")
        {
            std::string fen;
            std::string field;

            for (int i = 0; i < 6; ++i)
            {
                if (!(iss >> field))
                {
                    printLine("info string incomplete FEN");
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
                printLine("info string invalid FEN");
                return false;
            }

            return applyMoves(board, iss);
        }

        printLine("info string unknown position type " + mode);
        return false;
    }

    // ========================================================
    // info line formatting
    // ========================================================

    void printInfoLine(const search::SearchInfo& info)
    {
        std::ostringstream oss;

        oss << "info depth " << info.depth;

        if (info.seldepth > 0)
            oss << " seldepth " << info.seldepth;

        oss << " score ";

        if (info.isMate)
            oss << "mate " << info.mateIn;
        else
            oss << "cp " << info.score;

        oss << " nodes " << info.nodes
            << " nps " << info.nps
            << " hashfull " << info.hashfull
            << " time " << info.timeMs;

        if (!info.pv.empty())
        {
            oss << " pv";

            for (const auto& move : info.pv)
                oss << ' ' << moveToUci(move);
        }

        printLine(oss.str());
    }

    // ========================================================
    // UCI option declarations (advertised before uciok)
    // ========================================================

    void printOptions(int64_t hashMb, int64_t moveOverheadMs)
    {
        std::ostringstream oss;

        oss << "option name Hash type spin default "
            << hashMb << " min 1 max 4096";
        printLine(oss.str());

        printLine(
            "option name Threads type spin default 1 min 1 max 1"
        );

        std::ostringstream oss2;
        oss2 << "option name Move Overhead type spin default "
             << moveOverheadMs << " min 0 max 5000";
        printLine(oss2.str());

        printLine("option name Clear Hash type button");
        printLine("option name Ponder type check default false");
    }
}

// ============================================================
// Main UCI loop
// ============================================================

int main()
{
    chess::Board board;

    auto statsPtr =
        std::make_unique<search::SearchStats>();

    search::SearchStats& stats =
        *statsPtr;
    const bool tbLoaded =
    tb_init("tablebases");

std::cerr
    << "Fathom: "
    << (tbLoaded ? "loaded" : "failed")
    << ", TB_LARGEST = "
    << TB_LARGEST
    << '\n';
    const std::string bookPath =
    "books\\komodo.bin";
    const bool bookLoaded =
    stats.book.load(bookPath);
    if (bookLoaded)
{
    std::cerr
        << "Polyglot book loaded successfully\n";
}
else
{
    std::cerr
        << "Polyglot book FAILED to load\n";
}
    
    // Persisted UCI clock state.
    std::int64_t whiteTimeMs = 0;
    std::int64_t blackTimeMs = 0;
    std::int64_t whiteIncrementMs = 0;
    std::int64_t blackIncrementMs = 0;
    std::int64_t movesToGo = 0;

    // Persisted engine options.
    std::int64_t hashMb = 16;
    std::int64_t threadsOption = 1;
    std::int64_t moveOverheadMs = 50;
    bool ponderEnabled = false;

    bool uciInitialized = false;

    std::atomic<bool> searching{false};
    std::thread searchThread;

    std::string line;

    while (std::getline(std::cin, line))
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
            printLine("id name NeuralGambit");
            printLine("id author Dev");

            printOptions(hashMb, moveOverheadMs);

            printLine("uciok");

            uciInitialized = true;
        }

        // ====================================================
        // READY -- answered immediately, even mid-search.
        // ====================================================

        else if (command == "isready")
        {
            printLine("readyok");
        }

        // ====================================================
        // NEW GAME
        // ====================================================

        else if (command == "ucinewgame")
        {
            if (searching.load())
            {
                printLine(
                    "info string cannot start a new game while "
                    "searching; send stop first"
                );
            }
            else
            {
                board = chess::Board();

                stats.resetForNewGame();


                whiteTimeMs = 0;
                blackTimeMs = 0;
                whiteIncrementMs = 0;
                blackIncrementMs = 0;
                movesToGo = 0;
            }
        }

        // ====================================================
        // POSITION
        // ====================================================

        else if (command == "position")
        {
            if (searching.load())
            {
                printLine(
                    "info string cannot change position while "
                    "searching; send stop first"
                );
            }
            else
            {
                handlePosition(board, iss);
            }
        }
                // ====================================================
        // PERFT -- move-generation correctness test
        // ====================================================

        else if (command == "perft")
        {
            if (searching.load())
            {
                printLine(
                    "info string cannot run perft while searching"
                );
            }
            else
            {
                std::string valueText;

                if (!(iss >> valueText))
                {
                    printLine(
                        "info string perft requires a depth"
                    );
                    continue;
                }

                std::int64_t value;

                if (!parseInt64(valueText, value) ||
                    value < 0 ||
                    value > 6)
                {
                    printLine(
                        "info string invalid perft depth (0-6)"
                    );
                    continue;
                }

                const int depth =
                    static_cast<int>(value);

                std::uint64_t nodes = 0;

                if (depth == 0)
                {
                    nodes = 1;
                }
                else
                {
                    chess::Movelist rootMoves;

                    chess::movegen::legalmoves(
                        rootMoves,
                        board
                    );

                    for (const auto& move : rootMoves)
                    {
                        board.makeMove(move);

                        const std::uint64_t moveNodes =
                            depth > 1
                                ? perft(board, depth - 1)
                                : 1;

                        board.unmakeMove(move);

                        printLine(
                            "info string " +
                            moveToUci(move) +
                            ": " +
                            std::to_string(moveNodes)
                        );

                        nodes += moveNodes;
                    }
                }

                printLine(
                    "info string perft depth " +
                    std::to_string(depth) +
                    " nodes " +
                    std::to_string(nodes)
                );
            }
        }
        // ====================================================
        // GO
        // ====================================================

        else if (command == "go")
        {
            if (searching.load())
            {
                printLine(
                    "info string search already running; "
                    "send stop first"
                );
            }
            else
            {
                search::SearchLimits limits;

                int depth = 64;

                bool haveMoveTime = false;
                bool haveClock = false;
                bool isPondering = false;

                std::int64_t moveTimeMs = 0;
                std::int64_t nodesLimit = 0;

                std::string token;

                while (iss >> token)
                {
                    token = toLower(token);

                    std::string valueText;

                    if (token == "depth")
                    {
                        if (!(iss >> valueText))
                        {
                            printLine("info string missing depth value");
                            continue;
                        }

                        std::int64_t value;

                        if (parseInt64(valueText, value))
                        {
                            depth = std::max(1, clampToInt(value));
                        }
                    }
                    else if (token == "movetime")
                    {
                        if (!(iss >> valueText))
                            continue;

                        if (parseInt64(valueText, moveTimeMs))
                            haveMoveTime = true;
                    }
                    else if (token == "wtime")
                    {
                        if (!(iss >> valueText))
                            continue;

                        std::int64_t value;

                        if (parseInt64(valueText, value))
                        {
                            whiteTimeMs = std::max<std::int64_t>(0, value);
                            haveClock = true;
                        }
                    }
                    else if (token == "btime")
                    {
                        if (!(iss >> valueText))
                            continue;

                        std::int64_t value;

                        if (parseInt64(valueText, value))
                        {
                            blackTimeMs = std::max<std::int64_t>(0, value);
                            haveClock = true;
                        }
                    }
                    else if (token == "winc")
                    {
                        if (!(iss >> valueText))
                            continue;

                        std::int64_t value;

                        if (parseInt64(valueText, value))
                        {
                            whiteIncrementMs =
                                std::max<std::int64_t>(0, value);
                        }
                    }
                    else if (token == "binc")
                    {
                        if (!(iss >> valueText))
                            continue;

                        std::int64_t value;

                        if (parseInt64(valueText, value))
                        {
                            blackIncrementMs =
                                std::max<std::int64_t>(0, value);
                        }
                    }
                    else if (token == "movestogo")
                    {
                        if (!(iss >> valueText))
                            continue;

                        std::int64_t value;

                        if (parseInt64(valueText, value))
                        {
                            movesToGo = std::max<std::int64_t>(0, value);
                        }
                    }
                    else if (token == "nodes")
                    {
                        if (!(iss >> valueText))
                            continue;

                        parseInt64(valueText, nodesLimit);
                    }
                    else if (token == "infinite")
                    {
                        limits.infinite = true;
                    }
                    else if (token == "ponder")
                    {
                        isPondering = true;
                    }
                    else if (token == "mate")
                    {
                        printLine(
                            "info string unsupported go option mate"
                        );

                        if (iss >> valueText)
                        {
                            // Consume the value; mate search is not
                            // implemented.
                        }
                    }
                    else if (token == "searchmoves")
                    {
                        printLine(
                            "info string unsupported go option searchmoves"
                        );

                        while (iss >> valueText)
                        {
                            // Consume the rest of the command. The
                            // current search API has no searchmoves
                            // field.
                        }
                    }
                }

                if (haveMoveTime)
                {
                    limits.moveTimeMs = clampToInt(moveTimeMs);
                }
                else if (haveClock)
                {
                    const bool whiteToMove =
                        board.sideToMove() == chess::Color::WHITE;

                    const std::int64_t myTime =
                        whiteToMove ? whiteTimeMs : blackTimeMs;

                    const std::int64_t myIncrement =
                        whiteToMove ? whiteIncrementMs : blackIncrementMs;

                    limits.myTimeMs = clampToInt(myTime);
                    limits.incrementMs = clampToInt(myIncrement);
                    limits.movesToGo = clampToInt(movesToGo);
                }

                limits.maxNodes = nodesLimit;
                limits.moveOverheadMs = moveOverheadMs;

                if (isPondering)
                {
                    // The position being searched is hypothetical
                    // until "ponderhit" arrives, so ignore any
                    // clock-derived budget and rely on the GUI
                    // sending "stop" at the right time -- exactly
                    // like "go infinite".
                    limits.infinite = true;
                }

                stats.stop = false;
                stats.onIteration = printInfoLine;

                searching.store(true);

                chess::Board searchBoard = board;

                if (searchThread.joinable())
                    searchThread.join();

                searchThread = std::thread(
                    [&stats, &searching, searchBoard, depth, limits]
                    () mutable
                    {
                        const uint64_t hashBefore = searchBoard.hash();

                        const chess::Move best =
                            search::findBestMove(
                                searchBoard,
                                depth,
                                stats,
                                limits
                            );

                        const uint64_t hashAfter = searchBoard.hash();

                        if (hashBefore != hashAfter)
                        {
                            std::cerr
                                << "SEARCH CORRUPTED BOARD STATE!\n"
                                << "Before hash: " << hashBefore << '\n'
                                << "After hash: " << hashAfter << '\n';
                        }

                        chess::Move finalMove = best;

                        chess::Movelist legalMoves;
                        chess::movegen::legalmoves(
                            legalMoves,
                            searchBoard
                        );

                        bool legal = false;

                        for (const auto& move : legalMoves)
                        {
                            if (move == best)
                            {
                                legal = true;
                                break;
                            }
                        }

                        if (!legal)
                        {
                            std::cerr
                                << "ILLEGAL SEARCH MOVE: "
                                << moveToUci(best) << '\n';

                            if (!legalMoves.empty())
                            {
                                finalMove = legalMoves[0];

                                std::cerr
                                    << "FALLBACK MOVE: "
                                    << moveToUci(finalMove) << '\n';
                            }
                            else
                            {
                                finalMove = chess::Move::NO_MOVE;
                            }
                        }

                        printLine(
                            "bestmove " + moveToUci(finalMove)
                        );

                        searching.store(false);
                    }
                );
            }
        }

        // ====================================================
        // STOP -- genuinely interrupts the running search.
        // ====================================================

        else if (command == "stop")
        {
            if (searching.load())
                stats.stop = true;
        }

        // ====================================================
        // PONDERHIT
        // ====================================================

        else if (command == "ponderhit")
        {
            // The position now being searched is the real one.
            // Our minimal pondering support runs unbounded the
            // whole time (see "go ... ponder" above), so no
            // action is required here beyond acknowledging it.
        }

        // ====================================================
        // SETOPTION
        // ====================================================

        else if (command == "setoption")
        {
            std::string rest;
            std::getline(iss, rest);

            size_t namePos = rest.find("name");

            if (namePos == std::string::npos)
            {
                printLine("info string malformed setoption command");
            }
            else
            {
                size_t valuePos = rest.find(" value ", namePos);

                std::string optName;
                std::string optValue;
                bool hasValue = valuePos != std::string::npos;

                if (hasValue)
                {
                    optName = rest.substr(
                        namePos + 4,
                        valuePos - (namePos + 4)
                    );

                    optValue = rest.substr(valuePos + 7);
                }
                else
                {
                    optName = rest.substr(namePos + 4);
                }

                optName = trim(optName);
                optValue = trim(optValue);

                std::string optNameLower = toLower(optName);

                if (optNameLower == "hash")
                {
                    std::int64_t value;

                    if (hasValue && parseInt64(optValue, value) &&
                        value >= 1)
                    {
                        if (searching.load())
                        {
                            printLine(
                                "info string cannot resize hash "
                                "while searching; send stop first"
                            );
                        }
                        else
                        {
                            hashMb = value;
                            stats.table.resize(
                                static_cast<size_t>(hashMb)
                            );
                        }
                    }
                }
                else if (optNameLower == "threads")
                {
                    std::int64_t value;

                    if (hasValue && parseInt64(optValue, value))
                    {
                        threadsOption = value;

                        if (threadsOption != 1)
                        {
                            printLine(
                                "info string only 1 thread is "
                                "supported; ignoring"
                            );
                        }
                    }
                }
                else if (optNameLower == "move overhead")
                {
                    std::int64_t value;

                    if (hasValue && parseInt64(optValue, value) &&
                        value >= 0)
                    {
                        moveOverheadMs = value;
                    }
                }
                else if (optNameLower == "clear hash")
                {
                    if (searching.load())
                    {
                        printLine(
                            "info string cannot clear hash while "
                            "searching; send stop first"
                        );
                    }
                    else
                    {
                        stats.table.clear();
                    }
                }
                else if (optNameLower == "ponder")
                {
                    ponderEnabled = (toLower(optValue) == "true");
                }
                else
                {
                    printLine(
                        "info string unknown option " + optName
                    );
                }
            }
        }

        // ====================================================
        // EVAL -- static evaluation of the current position.
        // ====================================================

        else if (command == "eval")
        {
            if (searching.load())
            {
                printLine(
                    "info string cannot evaluate while searching; "
                    "send stop first"
                );
            }
            else
            {
                int whiteRelative = eval::evaluate(board);

                int sideToMoveRelative =
                    board.sideToMove() == chess::Color::WHITE
                        ? whiteRelative
                        : -whiteRelative;

                std::ostringstream oss;

                oss << "info string eval "
                    << whiteRelative
                    << " cp (white perspective), "
                    << sideToMoveRelative
                    << " cp (side to move perspective)";

                printLine(oss.str());
            }
        }

        // ====================================================
        // D -- debug display of current position.
        // ====================================================

        else if (command == "d")
        {
            printLine("info string fen " + board.getFen());
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
                    printLine("info string debug " + mode);
                }
            }
        }

        // ====================================================
        // QUIT
        // ====================================================

        else if (command == "quit")
        {
            if (searching.load())
                stats.stop = true;

            if (searchThread.joinable())
                searchThread.join();

            break;
        }

        // ====================================================
        // Unknown command
        // ====================================================

        else
        {
            if (uciInitialized)
            {
                printLine("info string unknown command " + command);
            }
        }
    }

    if (searchThread.joinable())
    {
        stats.stop = true;
        searchThread.join();
    }

    return 0;
}