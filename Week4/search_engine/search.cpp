#include "search.h"
#include<iostream>
// #include "../manual_evaluation_chess/eval.hpp"
#include "../arun_eval/eval.h"
#include "see.h"
#include "../tablebase/tbprobe.h"
#include "move_ordering.h"
#include<algorithm>
namespace search
{
    struct TimeBudget
{
    int64_t optimalMs = 0;
    int64_t maximumMs = 0;
};
int estimateMovesToGo(int moveNumber)
{
    if (moveNumber <= 0)
        return 40;

    int mtg = 45 - moveNumber / 2;

    return std::clamp(
        mtg,
        15,
        45
    );
}
TimeBudget computeTimeBudget(
    const SearchLimits& limits,
    int moveNumber)
{
    constexpr int64_t COMMUNICATION_MARGIN_MS = 50;

    // --------------------------------------------------------
    // Explicit per-move time.
    // --------------------------------------------------------
    // dev sharma
    // if (limits.moveTimeMs > 0)
    // {
    //     const int64_t usable =
    //         std::max<int64_t>(
    //             1,
    //             limits.moveTimeMs -
    //             // COMMUNICATION_MARGIN_MS
    //             limits.moveOverheadMs
    //         );

    //     return {
    //         usable,
    //         usable
    //     };
    // }
    // dev sharma
    if (limits.moveTimeMs > 0)
{
    const int64_t usable =
        std::max<int64_t>(
            1,
            limits.moveTimeMs -
            limits.moveOverheadMs
        );

    const int64_t soft =
        std::max<int64_t>(
            1,
            usable * 80 / 100
        );

    return {
        soft,
        usable
    };
}
    // --------------------------------------------------------
    // Infinite search.
    // --------------------------------------------------------

    if (limits.infinite)
        return {0, 0};

    // --------------------------------------------------------
    // No clock information.
    // --------------------------------------------------------

    if (limits.myTimeMs <= 0)
        return {0, 0};

    const int64_t timeLeft =
        limits.myTimeMs;

    // --------------------------------------------------------
    // Moves remaining.
    // --------------------------------------------------------
    // dev sharma 
    // int movesToGo =
    //     limits.movesToGo;

    // if (movesToGo <= 0)
    //     movesToGo =
    //         estimateMovesToGo(moveNumber); 

    // movesToGo =
    //     std::clamp(
    //         movesToGo,
    //         1,
    //         100
    //     );
    // dev sharma
    int movesToGo =
    limits.movesToGo;

if (movesToGo <= 0)
    movesToGo = 30;

movesToGo =
    std::clamp(
        movesToGo,
        1,
        100
    );
    // --------------------------------------------------------
    // Reserve time for GUI/UCI overhead.
    //
    // More time => slightly larger absolute reserve.
    // --------------------------------------------------------
    // dev sharma
    // int64_t overhead =limits.moveOverheadMs;
    //     // COMMUNICATION_MARGIN_MS; 

    // if (timeLeft >= 10000)
    //     overhead = 75;

    // if (timeLeft >= 30000)
    //     overhead = 100;
    // dev sharma
    int64_t overhead =
    std::max<int64_t>(
        limits.moveOverheadMs,
        COMMUNICATION_MARGIN_MS
    );

if (timeLeft >= 10000)
{
    overhead =
        std::max<int64_t>(overhead, 75);
}

if (timeLeft >= 30000)
{
    overhead =
        std::max<int64_t>(overhead, 100);
}
    const int64_t usable =
        std::max<int64_t>(
            1,
            timeLeft - overhead
        );

    // --------------------------------------------------------
    // Increment is not fully spent every move.
    //
    // Using most, but not all, of the increment reduces
    // long-term drift and still allows the clock to grow.
    // --------------------------------------------------------

    const int64_t effectiveIncrement =
        std::max<int64_t>(
            0,
            (limits.incrementMs * 80) / 100
        );

    // --------------------------------------------------------
    // Base allocation.
    // --------------------------------------------------------
    // dev sharma
    // int64_t base =
    //     usable / movesToGo;

    // base += effectiveIncrement;

    // // --------------------------------------------------------
    // // Give ourselves a little more time when the clock is
    // // comfortable, but never spend too much of the bank.
    // // --------------------------------------------------------

    // int64_t soft =
    //     base;

    // if (timeLeft > 30000)
    //     soft += usable / 20;      // +5%

    // else if (timeLeft > 10000)
    //     soft += usable / 30;      // +3.3%
    // dev sharma
    int64_t soft =
    usable / movesToGo;

soft += effectiveIncrement;

    // --------------------------------------------------------
    // Safety rails.
    // --------------------------------------------------------

    const int64_t reserve =
        std::max<int64_t>(
            overhead,
            100
        );

    const int64_t hardAvailable =
        std::max<int64_t>(
            1,
            timeLeft - reserve
        );

    // Never plan to consume more than 85% of the clock.
    // dev sharma 
    // const int64_t hardCap =
    //     std::max<int64_t>(
    //         1,
    //         (timeLeft * 85) / 100
    //     );
    // dev sharma
    const int64_t hardCap =
    std::max<int64_t>(
        1,
        (timeLeft * 30) / 100
    );
    soft =
        std::min(
            soft,
            hardCap
        );

    soft =
        std::min(
            soft,
            hardAvailable
        );

    soft =
        std::max<int64_t>(
            1,
            soft
        );

    // --------------------------------------------------------
    // Hard limit starts around 2.5x soft allocation.
    //
    // It is still constrained by the 85% rail.
    // --------------------------------------------------------
    // dev sharma
    // int64_t hard =
    //     soft * 5 / 2;

    // hard =
    //     std::min(
    //         hard,
    //         hardCap
    //     );

    // hard =
    //     std::min(
    //         hard,
    //         hardAvailable
    //     );

    // hard =
    //     std::max(
    //         hard,
    //         soft
    //     );
    // dev sharma 
    const int64_t hard =
    std::max<int64_t>(
        soft,
        std::min({ soft * 5, hardCap, hardAvailable })
    );
    return {
        soft,
        hard
    };
}
int64_t elapsedMs(const SearchStats& stats)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - stats.startTime
    ).count();
}
bool timeUp(const SearchStats& stats)
{
    if (stats.maximumMs <= 0)
        return false;

    return elapsedMs(stats) >= stats.maximumMs;
}
int evaluateForSideToMove(chess::Board& board)
{
    // // int score = eval::evaluate(board);
    // int score = eval::evaluate(board);

    // return board.sideToMove() == chess::Color::WHITE
    //     ? score
    //     : -score;
    return eval::evaluate(board);
}
static inline int pieceIndexForMoveSearch(
    const chess::Board& board,
    const chess::Move& move)
{
    const chess::Piece piece = board.at(move.from());

    const int type =
        static_cast<int>(piece.type().internal());

    if (piece.color() == chess::Color::NONE ||
        type < 0 ||
        type >= 6)
    {
        return -1;
    }

    return
        (piece.color() == chess::Color::WHITE ? 0 : 6)
        + type;
}
static inline int capturedTypeForMoveSearch(
    const chess::Board& board,
    const chess::Move& move)
{
    if (move.typeOf() == chess::Move::ENPASSANT)
        return static_cast<int>(
            chess::PieceType::PAWN
        );

    const chess::Piece captured =
        board.at(move.to());

    if (captured == chess::Piece::NONE)
        return -1;

    const int type =
        static_cast<int>(
            captured.type().internal()
        );

    return (type >= 0 && type < 6)
        ? type
        : -1;
}
static int pawnCorrectionIndex(
    uint64_t whitePawns,
    uint64_t blackPawns)
{
    uint64_t key =
        whitePawns ^
        (blackPawns
         + 0x9e3779b97f4a7c15ULL
         + (whitePawns << 6)
         + (whitePawns >> 2));

    key ^= key >> 30;
    key *= 0xbf58476d1ce4e5b9ULL;
    key ^= key >> 27;
    key *= 0x94d049bb133111ebULL;
    key ^= key >> 31;

    return static_cast<int>(
        key & (SearchStats::PAWN_CORRECTION_SIZE - 1)
    );
}

static int getPawnCorrection(
    const chess::Board& board,
    const SearchStats& stats)
{
    const uint64_t whitePawns =
        board.pieces(
            chess::PieceType::PAWN,
            chess::Color::WHITE
        ).getBits();

    const uint64_t blackPawns =
        board.pieces(
            chess::PieceType::PAWN,
            chess::Color::BLACK
        ).getBits();

    const int side =
        static_cast<int>(board.sideToMove());

    const int index =
        pawnCorrectionIndex(whitePawns, blackPawns);

    const auto& entry =
        stats.pawnCorrectionHistory[side][index];

    if (!entry.valid ||
        entry.whitePawns != whitePawns ||
        entry.blackPawns != blackPawns)
    {
        return 0;
    }

    return entry.correction;
}

static void updatePawnCorrection(
    const chess::Board& board,
    SearchStats& stats,
    int residual,
    int depth)
{
    constexpr int CORRECTION_LIMIT = 64;

    const uint64_t whitePawns =
        board.pieces(
            chess::PieceType::PAWN,
            chess::Color::WHITE
        ).getBits();

    const uint64_t blackPawns =
        board.pieces(
            chess::PieceType::PAWN,
            chess::Color::BLACK
        ).getBits();

    const int side =
        static_cast<int>(board.sideToMove());

    const int index =
        pawnCorrectionIndex(whitePawns, blackPawns);

    auto& entry =
        stats.pawnCorrectionHistory[side][index];

    if (!entry.valid ||
        entry.whitePawns != whitePawns ||
        entry.blackPawns != blackPawns)
    {
        entry.whitePawns = whitePawns;
        entry.blackPawns = blackPawns;
        entry.correction = 0;
        entry.valid = true;
    }

    const int target =
        std::clamp(
            residual,
            -CORRECTION_LIMIT,
            CORRECTION_LIMIT
        );

    const int difference =
        target - entry.correction;

    const int weight =
        std::min(depth + 1, 16);

    int adjustment =
        difference * weight / 64;

    if (adjustment == 0 && difference != 0)
        adjustment = difference > 0 ? 1 : -1;

    entry.correction =
        std::clamp(
            entry.correction + adjustment,
            -CORRECTION_LIMIT,
            CORRECTION_LIMIT
        );
}

#if defined(_MSC_VER)
#define SEARCH_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define SEARCH_NOINLINE __attribute__((noinline))
#else
#define SEARCH_NOINLINE
#endif

static constexpr int PROBCUT_MARGIN = 120;
static constexpr int PROBCUT_REDUCTION = 4;
static constexpr int PROBCUT_MATE_BOUND = MATE_SCORE - MAX_PLY;

static SEARCH_NOINLINE bool tryProbCut(
    chess::Board& board,
    int searchDepth,
    int probBeta,
    SearchStats& stats,
    int ply,
    chess::Move prevMove,
    int extCount,
    int staticEval,
    chess::Move ttMove,
    int& cutoffScore,
    chess::Move& cutoffMove)
{
    cutoffScore = 0;
    cutoffMove = chess::Move::NO_MOVE;

    // ProbCut considers captures only.
    chess::Movelist probMoves;

    chess::movegen::legalmoves<
        chess::movegen::MoveGenType::CAPTURE
    >(probMoves, board);

    if (probMoves.empty())
        return false;

    const int side =
        static_cast<int>(board.sideToMove());

    const int safePly =
        std::min(ply, MAX_PLY - 1);

    chess::Move counterMove =
        chess::Move::NO_MOVE;

    if (prevMove != chess::Move::NO_MOVE)
    {
        counterMove =
            stats.counterMoves[side]
                              [prevMove.from().index()]
                              [prevMove.to().index()];
    }

    int previousPiece = -1;
    int previousTo = -1;

    if (stats.moveStack[ply].valid)
    {
        previousPiece = stats.moveStack[ply].piece;
        previousTo = stats.moveStack[ply].to;
    }

    int previous2Piece = -1;
    int previous2To = -1;

    if (ply >= 2 && stats.moveStack[ply - 1].valid)
    {
        previous2Piece = stats.moveStack[ply - 1].piece;
        previous2To = stats.moveStack[ply - 1].to;
    }

    int previous4Piece = -1;
    int previous4To = -1;

    if (ply >= 4 && stats.moveStack[ply - 3].valid)
    {
        previous4Piece = stats.moveStack[ply - 3].piece;
        previous4To = stats.moveStack[ply - 3].to;
    }

    // Fixed-size score array: chess positions have fewer
    // than 256 legal moves.
    int probMoveScores[256];

    search::ordering::scoreMoves(
        board,
        probMoves,
        stats.killers[safePly],
        counterMove,
        stats.history[side],
        stats.continuationHistory,
        stats.continuationHistory2,
        stats.continuationHistory4,
        stats.captureHistory,
        previousPiece,
        previousTo,
        previous2Piece,
        previous2To,
        previous4Piece,
        previous4To,
        ttMove,
        probMoveScores
    );

    for (int moveSlot = 0;
         moveSlot < static_cast<int>(probMoves.size());
         ++moveSlot)
    {
        search::ordering::pickNextMove(
            probMoves,
            probMoveScores,
            moveSlot
        );

        const chess::Move probMove =
            probMoves[moveSlot];

        // Reject captures that cannot plausibly reach the
        // raised ProbCut threshold.
        if (!search::see::see_ge(
                board,
                probMove,
                probBeta - staticEval))
        {
            continue;
        }

        // Clear any stale continuation-history context.
        stats.moveStack[ply + 1] = {};

        const int pcPiece =
            pieceIndexForMoveSearch(board, probMove);

        if (pcPiece >= 0)
        {
            stats.moveStack[ply + 1].piece = pcPiece;
            stats.moveStack[ply + 1].to =
                probMove.to().index();
            stats.moveStack[ply + 1].valid = true;
        }

        board.makeMove(probMove);

        const int probScore = -negamax(
            board,
            searchDepth - PROBCUT_REDUCTION,
            -probBeta,
            -probBeta + 1,
            stats,
            ply + 1,
            probMove,
            true,
            extCount,
            false,
            chess::Move::NO_MOVE,
            false
        );

        board.unmakeMove(probMove);

        if (stats.stop)
            return false;

        if (probScore >= probBeta)
        {
            // Keep the cutoff score conservative.
            cutoffScore =
                probScore - PROBCUT_MARGIN;

            cutoffMove = probMove;
            return true;
        }
    }

    return false;
}

#undef SEARCH_NOINLINE

int quiescence(chess::Board& board,int alpha,int beta,SearchStats& stats,int qply,int rootPly){
    const int ply = rootPly + qply;
    ++stats.nodes;

if ((stats.nodes & (stats.timeCheckPeriod - 1)) == 0)
{
    if (timeUp(stats))
        stats.stop = true;

    if (stats.maxNodes > 0 &&
        stats.nodes >= stats.maxNodes)
    {
        stats.stop = true;
    }
}

if (stats.stop)
    return 0;
if (ply > stats.seldepth)
    stats.seldepth = ply;
// if (board.isRepetition(1))
//     return 0;

// if (board.isHalfMoveDraw())
//     return 0;

// if (board.isInsufficientMaterial())
//     return 0;
    // Safety limit for the tactical search.
    if (qply >= MAX_QPLY)
        return evaluateForSideToMove(board);
// const int qDepth = -qply;
// const int qDepth = -1 - qply;
const int qDepth = 0;
uint64_t key = board.hash();
tt::Entry* entry = stats.table.probe(key);

int alphaOriginal = alpha;
int bestScore = -INF;
int standPat = -INF;

constexpr int DELTA_MARGIN = 200;
if (entry != nullptr &&
    entry->depth >= qDepth)
{
    int ttScore =
        tt::valueFromTT(entry->score, ply);

    if (entry->bound == tt::Bound::EXACT)
        return ttScore;

    if (entry->bound == tt::Bound::LOWERBOUND &&
        ttScore >= beta)
        return ttScore;

    if (entry->bound == tt::Bound::UPPERBOUND &&
        ttScore <= alpha)
        return ttScore;
}


    const bool inCheck = board.inCheck();
    if (inCheck)
{
    if (board.isRepetition(1))
        return 0;

    if (board.isInsufficientMaterial())
        return 0;
}

if (board.isHalfMoveDraw())
    return 0;

    /*
        If we are not in check, we can stand pat:
        "What if I make no more tactical captures?"
    */
    if (!inCheck)
{
     standPat = evaluateForSideToMove(board);
    bestScore = standPat;

    // if (standPat >= beta)
    //     return beta;
    if (standPat >= beta)
{
    if (!stats.stop)
        stats.table.store(
            key,
            qDepth,
            tt::valueToTT(standPat, ply),
            tt::Bound::LOWERBOUND,
            chess::Move::NO_MOVE
        );

    return standPat;
}

    if (standPat > alpha)
        alpha = standPat;

    // Delta pruning
    // constexpr int DELTA_MARGIN = 200;

    // if (standPat +
    //     piece_value_bonus(
    //         chess::PieceType::QUEEN,
    //         true
    //     ) +
    //     DELTA_MARGIN < alpha)
    // if (standPat +
    // eval::materialValue(chess::PieceType::QUEEN) + // for arun evaluation 
    // DELTA_MARGIN < alpha)
    // {
    //     return standPat;
    // }
}

    chess::Movelist moves;

    if (inCheck)
    {
        // In check: we must consider every legal response.
        chess::movegen::legalmoves(moves, board);
    }
    else
{
    // Generate captures, including capture-promotions.
    chess::movegen::legalmoves<
        chess::movegen::MoveGenType::CAPTURE
    >(moves, board);

    // Also include quiet promotions, but not ordinary quiet moves.
    chess::Movelist quietMoves;

    chess::movegen::legalmoves<
        chess::movegen::MoveGenType::QUIET
    >(quietMoves, board);

    for (const auto& move : quietMoves)
    {
        if (move.typeOf() == chess::Move::PROMOTION)
            moves.add(move);
    }
}

    // Checkmate during qsearch.
    if (moves.empty())
    {
        if (inCheck)
            return -MATE_SCORE + ply;

        return alpha;
    }
    const int qSafePly = std::min(ply, MAX_PLY - 1);
const int qSide = static_cast<int>(board.sideToMove());

const chess::Move qTtMove =
    entry != nullptr
        ? entry->bestMove
        : chess::Move::NO_MOVE;

int qMoveScores[256];

search::ordering::scoreMoves(
    board,
    moves,
    stats.killers[qSafePly],
    chess::Move::NO_MOVE,
    stats.history[qSide],
    stats.continuationHistory,
    stats.continuationHistory2,
    stats.continuationHistory4,
    stats.captureHistory,
    -1, -1,
    -1, -1,
    -1, -1,
    qTtMove,
    qMoveScores
);
    for (int qMoveSlot = 0;
     qMoveSlot < static_cast<int>(moves.size());
     ++qMoveSlot)
{
    search::ordering::pickNextMove(
        moves,
        qMoveScores,
        qMoveSlot
    );

    const chess::Move& move = moves[qMoveSlot];
    if (!inCheck &&
    move.typeOf() != chess::Move::PROMOTION)
{
    int victimValue = 0;

    if (move.typeOf() == chess::Move::ENPASSANT)
    {
        victimValue =
            eval::materialValue(chess::PieceType::PAWN);
    }
    else
    {
        const chess::Piece victim = board.at(move.to());

        if (victim != chess::Piece::NONE)
            victimValue = eval::materialValue(victim.type());
    }

    if (standPat + victimValue + DELTA_MARGIN <= alpha)
        continue;
}
    // In a normal quiescence node, skip captures
    // that SEE says are materially losing.
    //
    // When in check, we must examine every legal move.
    if (!inCheck && !search::see::see_ge(board, move, 0))
    continue;

    board.makeMove(move);

    int score = -quiescence(
        board,
        -beta,
        -alpha,
        stats,
        qply + 1,
        rootPly
    );

    board.unmakeMove(move);
    if (stats.stop)
    {
        return 0;
    }
    // if (score >= beta)
    //     return beta;
    bestScore = std::max(bestScore, score);
    if (score >= beta)
{
    if (!stats.stop)
        stats.table.store(
            key,
            qDepth,
            tt::valueToTT(score, ply),
            tt::Bound::LOWERBOUND,
            move
        );

    return score;
}


    if (score > alpha)
        alpha = score;
    }

    // return alpha;
    tt::Bound bound = tt::Bound::EXACT;

if (bestScore <= alphaOriginal)
    bound = tt::Bound::UPPERBOUND;
else if (bestScore >= beta)
    bound = tt::Bound::LOWERBOUND;

if (!stats.stop)
{
    stats.table.store(
        key,
        qDepth,
        tt::valueToTT(bestScore, ply),
        bound,
        chess::Move::NO_MOVE
    );
}

return bestScore;
}


// fathom end moves integration 
static bool tryRootTablebase(
    chess::Board& board,
    chess::Move& tbMove)
{
    tbMove = chess::Move::NO_MOVE;

    // Fathom was not initialized or no Syzygy files were found.
    if (TB_LARGEST == 0)
        return false;

    // Tablebase can only handle positions up to the loaded
    // number of pieces.
    
    if (board.occ().count() >
        static_cast<int>(TB_LARGEST))
    {
        return false;
    }

    // Your Fathom wrapper rejects any position with castling
    // rights, so only probe when all castling rights are gone.
    const auto cr = board.castlingRights();

    const bool hasCastlingRights =
        cr.has(
            chess::Color::WHITE,
            chess::Board::CastlingRights::Side::KING_SIDE
        ) ||
        cr.has(
            chess::Color::WHITE,
            chess::Board::CastlingRights::Side::QUEEN_SIDE
        ) ||
        cr.has(
            chess::Color::BLACK,
            chess::Board::CastlingRights::Side::KING_SIDE
        ) ||
        cr.has(
            chess::Color::BLACK,
            chess::Board::CastlingRights::Side::QUEEN_SIDE
        );

    if (hasCastlingRights)
        return false;

    const uint64_t white =
        board.us(
            chess::Color::WHITE
        ).getBits();

    const uint64_t black =
        board.us(
            chess::Color::BLACK
        ).getBits();

    const uint64_t kings =
        board.pieces(
            chess::PieceType::KING
        ).getBits();

    const uint64_t queens =
        board.pieces(
            chess::PieceType::QUEEN
        ).getBits();

    const uint64_t rooks =
        board.pieces(
            chess::PieceType::ROOK
        ).getBits();

    const uint64_t bishops =
        board.pieces(
            chess::PieceType::BISHOP
        ).getBits();

    const uint64_t knights =
        board.pieces(
            chess::PieceType::KNIGHT
        ).getBits();

    const uint64_t pawns =
        board.pieces(
            chess::PieceType::PAWN
        ).getBits();

    const unsigned rule50 =
        static_cast<unsigned>(
            board.halfMoveClock()
        );

    const unsigned ep =
        board.enpassantSq() ==
            chess::Square::NO_SQ
        ? 0
        : static_cast<unsigned>(
              board.enpassantSq().index()
          );

    const bool whiteToMove =
        board.sideToMove() ==
        chess::Color::WHITE;

    // Required by tb_probe_root().
    unsigned results[TB_MAX_MOVES] = {};

    const unsigned result =
        tb_probe_root(
            white,
            black,
            kings,
            queens,
            rooks,
            bishops,
            knights,
            pawns,
            rule50,
            0,              // no castling rights
            ep,
            whiteToMove,
            results
        );

    if (result == TB_RESULT_FAILED)
        return false;

    const unsigned from =
        TB_GET_FROM(result);

    const unsigned to =
        TB_GET_TO(result);

    const unsigned promotes =
        TB_GET_PROMOTES(result);

    // Convert Fathom's encoded move into YOUR chess::Move.
    // Validate it against the actual legal move list.
    chess::Movelist legalMoves;

    chess::movegen::legalmoves(
        legalMoves,
        board
    );

    for (const auto& move : legalMoves)
    {
        if (move.from().index() !=
            static_cast<int>(from))
        {
            continue;
        }

        if (move.to().index() !=
            static_cast<int>(to))
        {
            continue;
        }

        // Normal move.
        if (promotes == TB_PROMOTES_NONE)
        {
            tbMove = move;
            return true;
        }

        // Promotion.
        if (move.typeOf() !=
            chess::Move::PROMOTION)
        {
            continue;
        }

        chess::PieceType promotionType;

        switch (promotes)
        {
            case TB_PROMOTES_QUEEN:
                promotionType =
                    chess::PieceType::QUEEN;
                break;

            case TB_PROMOTES_ROOK:
                promotionType =
                    chess::PieceType::ROOK;
                break;

            case TB_PROMOTES_BISHOP:
                promotionType =
                    chess::PieceType::BISHOP;
                break;

            case TB_PROMOTES_KNIGHT:
                promotionType =
                    chess::PieceType::KNIGHT;
                break;

            default:
                continue;
        }

        if (move.promotionType() ==
            promotionType)
        {
            tbMove = move;
            return true;
        }
    }

    // Fathom returned a result, but we could not map its
    // suggested move to one of the board's legal chess moves.
    return false;
}

    int negamax(chess::Board& board,int depth,int alpha,int beta,SearchStats& stats,int ply,chess:: Move prevMove,bool nullMoveAllowed,int extCount,bool cutNode,chess::Move excludedMove,bool allowProbCut)
{
   ++stats.nodes;
   if (ply >= MAX_PLY - 1)
    return evaluateForSideToMove(board);
    if (ply + 2 < MAX_PLY)
{
    stats.killers[ply + 2][0] = chess::Move::NO_MOVE;
    stats.killers[ply + 2][1] = chess::Move::NO_MOVE;
}

if ((stats.nodes & (stats.timeCheckPeriod - 1)) == 0)
{
    if (timeUp(stats))
        stats.stop = true;

    if (stats.maxNodes > 0 &&
        stats.nodes >= stats.maxNodes)
    {
        stats.stop = true;
    }
}

if (stats.stop)
    return 0;
if (ply > stats.seldepth)
    stats.seldepth = ply;
// Draw detection.
// Do not apply this at the root.
if (ply > 0)
{
    if (board.isRepetition(1))
        return 0;

    if (board.isHalfMoveDraw())
    {
        // A checkmate takes precedence over the 50/75/100-move draw.
        if (board.inCheck())
        {
            chess::Movelist drawCheckMoves;
            chess::movegen::legalmoves(drawCheckMoves, board);

            if (drawCheckMoves.empty())
                return -MATE_SCORE + ply;
        }

        return 0;
    }

    if (board.isInsufficientMaterial())
        return 0;
}
if (ply > 0)
{
    alpha = std::max(alpha, -MATE_SCORE + ply);
    beta = std::min(beta, MATE_SCORE - ply - 1);

    if (alpha >= beta)
        return alpha;
}
    int alphaOriginal = alpha;
    uint64_t key = board.hash();
    tt::Entry* entry = stats.table.probe(key);
    const bool isPV = (beta - alpha > 1);


if (entry != nullptr &&  excludedMove == chess::Move::NO_MOVE)
{
    ++stats.ttHits;

    if (entry->depth >= depth)
    {
        int ttScore =
            tt::valueFromTT(entry->score, ply);

        if (entry->bound == tt::Bound::EXACT)
            return ttScore;

        if (!isPV &&
            entry->bound == tt::Bound::LOWERBOUND &&
            ttScore >= beta)
            return ttScore;

        if (!isPV &&
            entry->bound == tt::Bound::UPPERBOUND &&
            ttScore <= alpha)
            return ttScore;
    }
}

//     if (depth <= 0)return quiescence(board, alpha, beta, stats,ply);
//     // REF
//     bool isPV = (beta - alpha > 1);
//     int staticEval = evaluateForSideToMove(board);
//     bool improving = false;

// if (!board.inCheck())
// {
//     stats.staticEvalStack[ply] = staticEval;

//     // Compare with the same side's evaluation two plies earlier.
//     // We start at ply 3 because the root itself is not searched through
//     // negamax in the current implementation.
//     if (ply >= 3)
//     {
//         improving =
//             staticEval >= stats.staticEvalStack[ply - 2];
//     }
// }
if (depth <= 0)
    return quiescence(board, alpha, beta, stats, 0,ply);

// REF
const bool inCheck = board.inCheck();



constexpr int EVAL_NONE = INF;

const int rawStaticEval =
    inCheck
        ? 0
        : evaluateForSideToMove(board);

int staticEval = rawStaticEval;

if (!inCheck)
    staticEval += getPawnCorrection(board, stats);

stats.staticEvalStack[ply] =
    inCheck ? EVAL_NONE : staticEval;

bool improving = false;

if (!inCheck && ply >= 2)
{
    const int prev =
        stats.staticEvalStack[ply - 2];

    improving =
        (prev != EVAL_NONE) &&
        staticEval >= prev;
}
    // constexpr int RFP_MARGIN_PER_DEPTH = 120;
    // int rfpMargin = RFP_MARGIN_PER_DEPTH * depth;
    // if (!isPV &&!board.inCheck() &&depth <= 6 &&staticEval - rfpMargin >= beta)
    // {
    //     return staticEval;
    // }  
    constexpr int RFP_MARGIN_PER_DEPTH = 120;
constexpr int RFP_NOT_IMPROVING_PENALTY = 70;

int rfpMargin =
    RFP_MARGIN_PER_DEPTH * depth;

if (!improving)
    rfpMargin += RFP_NOT_IMPROVING_PENALTY;

if (!isPV &&
    !inCheck &&
    depth <= 6 &&
    staticEval - rfpMargin >= beta)
{
    return staticEval;
} 
    // Razoring
    if (!isPV &&
    !inCheck &&
    depth <= 3 &&
    staticEval + 200 * depth < alpha)
    {
        return quiescence(board, alpha, beta, stats,0,ply);
    }  
    // IIR
    // Internal Iterative Reduction
        int searchDepth = depth;

    // ---------------------------------------------------------
    // Internal Iterative Reduction (IIR)
    //
    // No TT move means no good ordering guess at this node.
    // Shrink the depth we actually recurse with instead of
    // paying full price for a poorly-ordered search. The store
    // at the end of this function still writes a TT entry with
    // a best move, so the next visit to this position gets a
    // real move to try first.
    // ---------------------------------------------------------
    if (!inCheck &&
        depth >= 4 &&
        (entry == nullptr || entry->bestMove == chess::Move::NO_MOVE))
    {
        // searchDepth = isPV ? depth - 1 : depth - 2;
        searchDepth = depth - 1;
        ++stats.iirReductions;   // add this counter to SearchStats, like lmrReductions
    } 
    const chess::Color us = board.sideToMove();

const chess::Bitboard nonPawnPieces =
    board.us(us) &
    ~board.pieces(chess::PieceType::PAWN, us) &
    ~board.pieces(chess::PieceType::KING, us);

const bool hasNonPawnMaterial =
    nonPawnPieces.count() > 0;

    constexpr int NULL_MOVE_MATE_BOUND =
    MATE_SCORE - MAX_PLY;

const bool ttUpperBoundBelowBeta =
    entry != nullptr &&
    entry->depth >= depth &&
    entry->bound == tt::Bound::UPPERBOUND &&
    tt::valueFromTT(entry->score, ply) < beta;

    if (!isPV &&
    !inCheck &&
    nullMoveAllowed &&
    depth >= 3 &&
    staticEval >= beta &&
    beta > -NULL_MOVE_MATE_BOUND &&
    beta < NULL_MOVE_MATE_BOUND &&
    !ttUpperBoundBelowBeta)
{
    // ++nullMoveAttempts = 0;
     ++stats.nullMoveAttempts;
    // chess::Color us = board.sideToMove();

    // chess::Bitboard nonPawnPieces =
    //     board.us(us) &
    //     ~board.pieces(chess::PieceType::PAWN, us) &
    //     ~board.pieces(chess::PieceType::KING, us);

    if (hasNonPawnMaterial)
    {
        int R =
    3 +
    depth / 3 +
    std::min((staticEval - beta) / 200, 3);

    //     board.makeNullMove();

    //     int nullScore = -negamax(
    //         board,
    //         depth - 1 - R,
    //         -beta,
    //         -beta + 1,
    //         stats,
    //         ply + 1,
    //         chess::Move::NO_MOVE,
    //         false
    //     );

    //     board.unmakeNullMove();

    //     if (nullScore >= beta)
    //     {
    //         // For now, simple cutoff.
    //          ++stats.nullMoveCutoffs; // temp
    //          if (depth < NMP_VERIFICATION_MIN_DEPTH)
    //     return beta;

    // int verifyScore = negamax(
    //     board,
    //     depth - R,
    //     alpha,
    //     beta,
    //     stats,
    //     ply+1,
    //     prevMove,
    //     false
    // );
    // if (verifyScore >= beta)
    //         return beta;
    //     }
    // }    stats.moveStack[ply + 1] = {};
    stats.moveStack[ply + 1].valid = false;
            board.makeNullMove();

        int nullScore = -negamax(
            board,
            depth - 1 - R,
            -beta,
            -beta + 1,
            stats,
            ply + 1,
            chess::Move::NO_MOVE,
            false,extCount,!cutNode,chess::Move::NO_MOVE,
allowProbCut
        );

        board.unmakeNullMove();

        if (stats.stop)
            return 0;

        if (nullScore >= beta)
        {
            ++stats.nullMoveCutoffs;

            if (depth < NMP_VERIFICATION_MIN_DEPTH)
                return nullScore;
            stats.moveStack[ply + 1].valid = false;
            int verifyScore = negamax(
                board,
                depth - R,
                beta - 1,
                beta,
                stats,
                ply ,
                prevMove,
                false,extCount,false,chess::Move::NO_MOVE,
allowProbCut
            );

            if (stats.stop)
                return 0;

            if (verifyScore >= beta)
                return verifyScore;
        }
    }
}
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    if (moves.empty())
{
    if (inCheck)
        return -MATE_SCORE + ply;

    return 0;
}

    int safePly = std::min(ply, MAX_PLY - 1);

    int side =
        static_cast<int>(board.sideToMove());
    chess::Move counterMove = chess::Move::NO_MOVE;

if (prevMove != chess::Move::NO_MOVE)
{
    counterMove =
        stats.counterMoves[side]
                          [prevMove.from().index()]
                          [prevMove.to().index()];
}
    chess::Move ttMove = chess::Move::NO_MOVE;

    if (entry != nullptr) ttMove = entry->bestMove;
    const bool ttMoveIsCapture =
    ttMove != chess::Move::NO_MOVE &&
    (ttMove.typeOf() == chess::Move::ENPASSANT ||
     (ttMove.typeOf() != chess::Move::CASTLING &&
      board.at(ttMove.to()) != chess::Piece::NONE));

      constexpr int SINGULAR_MATE_BOUND =
    MATE_SCORE - 2 * MAX_PLY;

const int singularTtScore =
    entry != nullptr
        ? tt::valueFromTT(entry->score, ply)
        : 0;

const bool singularCandidate =
    excludedMove == chess::Move::NO_MOVE &&
    !isPV &&
    !inCheck &&
    searchDepth >= 6 &&
    ply < stats.extensionCap &&
    entry != nullptr &&
    ttMove != chess::Move::NO_MOVE &&
    entry->depth >= searchDepth - 3 &&
    (entry->bound == tt::Bound::LOWERBOUND ||
     entry->bound == tt::Bound::EXACT) &&
    singularTtScore > -SINGULAR_MATE_BOUND &&
    singularTtScore < SINGULAR_MATE_BOUND;
    int previousPiece = -1;
int previousTo = -1;

if (stats.moveStack[ply].valid)
{
    previousPiece =
        stats.moveStack[ply].piece;

    previousTo =
        stats.moveStack[ply].to;
}
int previous2Piece = -1;
int previous2To = -1;

if (ply >= 2 && stats.moveStack[ply - 1].valid)
{
    previous2Piece = stats.moveStack[ply - 1].piece;
    previous2To = stats.moveStack[ply - 1].to;
}

int previous4Piece = -1;
int previous4To = -1;

if (ply >= 4 && stats.moveStack[ply - 3].valid)
{
    previous4Piece = stats.moveStack[ply - 3].piece;
    previous4To = stats.moveStack[ply - 3].to;
}
    int moveScores[256];

search::ordering::scoreMoves(
    board,
    moves,
    stats.killers[safePly],
    counterMove,
    stats.history[side],
    stats.continuationHistory,
    stats.continuationHistory2,
    stats.continuationHistory4,
    stats.captureHistory,
    previousPiece,
    previousTo,
    previous2Piece,
    previous2To,
    previous4Piece,
    previous4To,
    ttMove,
    moveScores
);

if (allowProbCut &&
    excludedMove == chess::Move::NO_MOVE &&
    !isPV &&
    !inCheck &&
    hasNonPawnMaterial &&
    searchDepth >= PROBCUT_REDUCTION + 1 &&
    beta > -PROBCUT_MATE_BOUND &&
    beta < PROBCUT_MATE_BOUND - PROBCUT_MARGIN)
{
    const int probBeta =
        beta + PROBCUT_MARGIN;

    // A sufficiently deep TT upper/exact bound below the
    // raised threshold argues against trying ProbCut.
    const bool ttCanRejectProbCut =
        entry != nullptr &&
        entry->depth >= searchDepth - PROBCUT_REDUCTION &&
        (entry->bound == tt::Bound::UPPERBOUND ||
         entry->bound == tt::Bound::EXACT) &&
        tt::valueFromTT(entry->score, ply) < probBeta;

    if (!ttCanRejectProbCut)
    {
        int cutoffScore = 0;
        chess::Move cutoffMove =
            chess::Move::NO_MOVE;

        if (tryProbCut(
                board,
                searchDepth,
                probBeta,
                stats,
                ply,
                prevMove,
                extCount,
                staticEval,
                ttMove,
                cutoffScore,
                cutoffMove))
        {
            stats.table.store(
                key,
                searchDepth - PROBCUT_REDUCTION + 1,
                tt::valueToTT(cutoffScore, ply),
                tt::Bound::LOWERBOUND,
                cutoffMove
            );

            return cutoffScore;
        }
    }
}

    int bestScore = -INF;
    chess::Move bestMove = chess::Move::NO_MOVE;
    constexpr int MAX_QUIETS = 64;
    chess::Move quietsTried[MAX_QUIETS];
    int quietCount = 0;
    struct TriedCapture
{
    int piece;
    int type;
    int to;
};

TriedCapture capturesTried[32];
int captureCount = 0;
    bool firstMove = true;
int moveIndex = 0;
int quietsSearched = 0;

// chess::Color us = board.sideToMove();

// chess::Bitboard nonPawnPieces =
//     board.us(us) &
//     ~board.pieces(chess::PieceType::PAWN, us) &
//     ~board.pieces(chess::PieceType::KING, us);

// bool hasNonPawnMaterial = nonPawnPieces.count() > 0;

constexpr int MATE_BOUND = MATE_SCORE - MAX_PLY;
constexpr int LMP_MOVE_LIMIT[4] = {0,6,10,14};
    for (int moveSlot = 0;
     moveSlot < static_cast<int>(moves.size());
     ++moveSlot)
{
    search::ordering::pickNextMove(
        moves,
        moveScores,
        moveSlot
    );

    const chess::Move& move = moves[moveSlot];
if (move == excludedMove)
    continue;
    ++moveIndex;

    bool isCapture =
        move.typeOf() == chess::Move::ENPASSANT ||
        (move.typeOf() != chess::Move::CASTLING &&
         board.at(move.to()) != chess::Piece::NONE);

    bool isQuiet =
        !isCapture &&
        move.typeOf() != chess::Move::PROMOTION;

    bool isPromotion =
        move.typeOf() == chess::Move::PROMOTION;

    bool isKillerOrCounter =
        move == stats.killers[safePly][0] ||
        move == stats.killers[safePly][1] ||
        (counterMove != chess::Move::NO_MOVE &&
         move == counterMove);

    int historyScore =
        stats.history[side]
                    [move.from().index()]
                    [move.to().index()];
        // const bool quietGivesCheck =
        // board.givesCheck(move) != chess::CheckType::NO_CHECK;
        bool quietGivesCheck = false;
        int singularExtension = 0;

if (singularCandidate && move == ttMove)
{
    stats.moveStack[ply + 1].valid = false;

    const int singularBeta =
        singularTtScore - 2 * searchDepth;

    const int singularDepth =
        std::max(1, (searchDepth - 1) / 2);

    const int singularScore = negamax(
        board,
        singularDepth,
        singularBeta - 1,
        singularBeta,
        stats,
        ply,
        prevMove,
        false,
        extCount,
        false,
        ttMove
    );

    if (stats.stop)
        return 0;

    if (singularScore < singularBeta)
        singularExtension = 1;
}
    // Step 5: Main-search SEE pruning
// if (!isPV &&
//     !inCheck &&
//     depth <= 6 &&
//     isCapture &&
//     !isPromotion &&
//     search::see::evaluate(board, move) < 0)
// {
//     continue;
// }
if (!isPV &&
    !inCheck &&
    hasNonPawnMaterial &&
    bestScore > -MATE_BOUND &&
    depth <= 6 &&
    isCapture &&
    !isPromotion &&
    !search::see::see_ge(board, move, -90 * depth))
{
    continue;
}
constexpr int FUTILITY_MARGIN_PER_DEPTH = 120;
const bool quietPruningCandidate =
    !isPV &&
    !inCheck &&
    hasNonPawnMaterial &&
    isQuiet &&
    !isKillerOrCounter &&
    bestScore > -MATE_BOUND;

const bool futilityMayPrune =
    quietPruningCandidate &&
    depth <= 3 &&
    staticEval + FUTILITY_MARGIN_PER_DEPTH * depth <= alpha;

const bool lmpMayPrune =
    quietPruningCandidate &&
    depth >= 1 &&
    depth <= 3 &&
    quietsSearched >= LMP_MOVE_LIMIT[depth];

if (futilityMayPrune || lmpMayPrune)
{
    quietGivesCheck =
        board.givesCheck(move) != chess::CheckType::NO_CHECK;
}
if (!isPV &&
    !inCheck &&
    hasNonPawnMaterial &&
    depth <= 3 &&
    isQuiet &&
    !isKillerOrCounter &&
    !quietGivesCheck &&
    bestScore > -MATE_BOUND &&
    staticEval + FUTILITY_MARGIN_PER_DEPTH * depth <= alpha)
{
    continue;
}
if (!isPV &&
    !inCheck &&
    hasNonPawnMaterial &&
    isQuiet &&
    !isKillerOrCounter &&
    !quietGivesCheck &&
    bestScore > -MATE_BOUND)
{
    if (depth >= 1 &&
        depth <= 3 &&
        quietsSearched >= LMP_MOVE_LIMIT[depth])
    {
        continue;
    }
}
const bool isBadCapture =
    !firstMove &&
    depth >= 3 &&
    isCapture &&
    !isPromotion &&
    !search::see::see_ge(board, move, 0);
const int currentPiece =
    pieceIndexForMoveSearch(board, move);
int continuationHistoryScore = 0;

if (!isCapture &&
    previousPiece >= 0 &&
    previousPiece < 12 &&
    previousTo >= 0 &&
    previousTo < 64 &&
    currentPiece >= 0)
{
    continuationHistoryScore =
        stats.continuationHistory
            [previousPiece]
            [previousTo]
            [currentPiece]
            [move.to().index()];
}

stats.moveStack[ply + 1].valid = false;

if (currentPiece >= 0)
{
    stats.moveStack[ply + 1].piece = currentPiece;
    stats.moveStack[ply + 1].to = move.to().index();
    stats.moveStack[ply + 1].valid = true;
}
int capPiece = -1;
int capType = -1;

if (isCapture)
{
    capPiece = currentPiece;
    capType =
        capturedTypeForMoveSearch(
            board,
            move
        );
}

board.makeMove(move);


    bool givesCheck = board.inCheck();
// extension add
   int extension = singularExtension;

if (givesCheck &&
    depth >= 2 &&
    ply < stats.extensionCap)
{
    extension = std::max(extension, 1);
}
// // above is extension add
    int reduction = 0;
    int score;
    const bool childCutNode =
    isPV ? !firstMove : !cutNode;

    if (firstMove)
    {
        score = -negamax(
            board,
            searchDepth - 1+extension, // extension add
            -beta,
            -alpha,
            stats,
            ply + 1,
            move,
            true,extCount + extension,childCutNode,chess::Move::NO_MOVE,allowProbCut
        );

        firstMove = false;
    }
    // lmr implementation 
    else
    {
        reduction =
    search::ordering::lateMoveReduction(
        searchDepth,
        moveIndex,
        isPV,
        improving,
        cutNode,
        isCapture,
        isBadCapture,
        isPromotion,
        inCheck,
        givesCheck,
        isKillerOrCounter,
        ttMoveIsCapture,
        isCapture ? 0 : historyScore,
        isCapture ? 0 : continuationHistoryScore
    );
            if (reduction > 0)
        ++stats.lmrReductions;
        score = -negamax(
    board,
    searchDepth - 1 - reduction + extension,
    -alpha - 1,
    -alpha,
    stats,
    ply + 1,
    move,
    true,
    extCount + extension,
    childCutNode,chess::Move::NO_MOVE,
allowProbCut
);

if (score > alpha)
{
    // If the move was reduced, first verify it at full depth
    // with a null window.
    if (reduction > 0)
    {
        ++stats.lmrResearches;

        score = -negamax(
            board,
            searchDepth - 1 + extension,
            -alpha - 1,
            -alpha,
            stats,
            ply + 1,
            move,
            true,
            extCount + extension,
            childCutNode,chess::Move::NO_MOVE,
allowProbCut
        );
    }

    // Only do the expensive full-window search if the
    // full-depth result actually lies inside the window.
    if (score > alpha && score < beta)
    {
        score = -negamax(
            board,
            searchDepth - 1 + extension,
            -beta,
            -alpha,
            stats,
            ply + 1,
            move,
            true,
            extCount + extension,
            childCutNode,chess::Move::NO_MOVE,
allowProbCut
        );
    }
}
    }
    // lmr implementation above 

    // below this is without lmr for evaluation of our engine 
//     else
// {
//     // LMR OFF: pure PVS
//     score = -negamax(
//         board,
//         searchDepth - 1,
//         -alpha - 1,
//         -alpha,
//         stats,
//         ply + 1,
//         move,
//         true
//     );

//     if (score > alpha)
//     {
//         score = -negamax(
//             board,
//             searchDepth - 1,
//             -beta,
//             -alpha,
//             stats,
//             ply + 1,
//             move,
//             true
//         );
//     }
// }
 // above this is without lmr for evaluation of our engine 
    board.unmakeMove(move);
    if (isQuiet) ++quietsSearched;

    if (stats.stop)
        break;

    if (score > bestScore)
    {
        bestScore = score;
        bestMove = move;
    }

    if (score > alpha)
        alpha = score;

    if (isQuiet &&
        alpha < beta &&
        quietCount < MAX_QUIETS)
    {
        quietsTried[quietCount++] = move;
    }
    if (isCapture &&
    capPiece >= 0 &&
    capType >= 0 &&
    alpha < beta &&
    captureCount < 32)
{
    capturesTried[captureCount++] = {
        capPiece,
        capType,
        move.to().index()
    };
}
    
    if (alpha >= beta)
    {
        if (isQuiet)
        {
            int safePly =
                std::min(ply, MAX_PLY - 1);

            if (stats.killers[safePly][0] != move)
{
    stats.killers[safePly][1] =
        stats.killers[safePly][0];

    stats.killers[safePly][0] =
        move;
}
            if (prevMove != chess::Move::NO_MOVE)
{
    stats.counterMoves[side]
                        [prevMove.from().index()]
                        [prevMove.to().index()] =
        move;
}


//            if (prevMove != chess::Move::NO_MOVE)
// {
//     stats.counterMoves[side]
//         [prevMove.from().index()]
//         [prevMove.to().index()] =
//         move;
// }

// int& h =
//     stats.history[side]
//                 [move.from().index()]
//                 [move.to().index()];

// h += depth * depth;

// if (h > 30000)
//     h = 30000;
int& h =
    stats.history[side]
                [move.from().index()]
                [move.to().index()];

constexpr int HISTORY_MAX = 30000;
const int bonus =
    std::min(
        depth * depth * 2,
        2000
    );

h += bonus - h * bonus / HISTORY_MAX;

if (h > HISTORY_MAX)
    h = HISTORY_MAX;

            for (int i = 0; i < quietCount; ++i)
            {
                const chess::Move& qm =
                    quietsTried[i];

                int& hq =
    stats.history[side]
                [qm.from().index()]
                [qm.to().index()];

constexpr int HISTORY_MAX = 30000;
const int malus =
    std::min(depth * depth * 2, 2000);

hq += -malus - hq * malus / HISTORY_MAX;

if (hq < -HISTORY_MAX)
    hq = -HISTORY_MAX;
            }
                        // Continuation history update.
            // Update continuation history at 1, 2, and 4 plies.
constexpr int CONT_HISTORY_MAX = 30000;

const int contDelta =
    std::min(depth * depth * 2, 2000);

auto updateContinuationTable =
    [&](auto& table, int contextPly)
{
    if (contextPly < 0 ||
        contextPly >= MAX_PLY ||
        !stats.moveStack[contextPly].valid ||
        !stats.moveStack[ply + 1].valid)
    {
        return;
    }

    const auto& context = stats.moveStack[contextPly];
    const auto& current = stats.moveStack[ply + 1];

    auto updateCell =
        [&](int piece, int to, int delta)
    {
        int& value =
            table[context.piece]
                 [context.to]
                 [piece]
                 [to];

        const int magnitude =
            delta < 0 ? -delta : delta;

        value +=
            delta -
            value * magnitude / CONT_HISTORY_MAX;

        value = std::clamp(
            value,
            -CONT_HISTORY_MAX,
            CONT_HISTORY_MAX
        );
    };

    // Reward the quiet move that caused the cutoff.
    updateCell(
        current.piece,
        current.to,
        contDelta
    );

    // Penalize earlier quiet moves that failed to cut off.
    for (int i = 0; i < quietCount; ++i)
    {
        const chess::Move& qm = quietsTried[i];

        const int qmPiece =
            pieceIndexForMoveSearch(board, qm);

        if (qmPiece < 0)
            continue;

        updateCell(
            qmPiece,
            qm.to().index(),
            -contDelta
        );
    }
};

updateContinuationTable(
    stats.continuationHistory,
    ply
);

updateContinuationTable(
    stats.continuationHistory2,
    ply - 1
);

updateContinuationTable(
    stats.continuationHistory4,
    ply - 3
);
        }
        {
    auto updateCaptureHistory =
        [&](int piece, int type, int to, int delta)
        {
            int& h =
                stats.captureHistory
                    [piece]
                    [type]
                    [to];

            const int amount =
                delta < 0 ? -delta : delta;

            h += delta -
                 h * amount / 30000;
        };

    if (isCapture &&
        capPiece >= 0 &&
        capType >= 0)
    {
        updateCaptureHistory(
            capPiece,
            capType,
            move.to().index(),
            std::min(
                depth * depth * 2,
                2000
            )
        );
    }

    for (int i = 0;
         i < captureCount;
         ++i)
    {
        updateCaptureHistory(
            capturesTried[i].piece,
            capturesTried[i].type,
            capturesTried[i].to,
            -std::min(
                depth * depth,
                1000
            )
        );
    }
}

        break;
    }

    
}
constexpr int CORRECTION_MATE_BOUND =
    MATE_SCORE - MAX_PLY;

const bool bestIsCapture =
    bestMove != chess::Move::NO_MOVE &&
    (bestMove.typeOf() == chess::Move::ENPASSANT ||
     (bestMove.typeOf() != chess::Move::CASTLING &&
      board.at(bestMove.to()) != chess::Piece::NONE));

const bool lowerBound = bestScore >= beta;
const bool upperBound = bestScore <= alphaOriginal;

if (!stats.stop &&
    excludedMove == chess::Move::NO_MOVE &&
    !inCheck &&
    depth >= 3 &&
    !bestIsCapture &&
    bestScore > -CORRECTION_MATE_BOUND &&
    bestScore < CORRECTION_MATE_BOUND &&
    !(lowerBound && bestScore <= rawStaticEval) &&
    !(upperBound && bestScore >= rawStaticEval))
{
    updatePawnCorrection(
        board,
        stats,
        bestScore - rawStaticEval,
        depth
    );
}
tt::Bound bound = tt::Bound::EXACT;

if (bestScore <= alphaOriginal)
    bound = tt::Bound::UPPERBOUND;
else if (bestScore >= beta)
    bound = tt::Bound::LOWERBOUND;

if (stats.stop)
    return 0;

// chess::Move storedMove =
//     (bestScore > alphaOriginal)
//         ? bestMove
//         : chess::Move::NO_MOVE;
chess::Move storedMove =
    (bestScore > alphaOriginal)
        ? bestMove
        : ttMove;

stats.table.store(
    key,
    searchDepth,
    tt::valueToTT(bestScore, ply),
    bound,
    storedMove
);

return bestScore;
}

std::vector<chess::Move> extractPV(
    chess::Board board,
    SearchStats& stats,
    int maxLength)
{
    std::vector<chess::Move> pv;

    for (int i = 0; i < maxLength; ++i)
    {
        uint64_t key = board.hash();
        tt::Entry* entry = stats.table.probe(key);

        if (entry == nullptr ||
            entry->bestMove == chess::Move::NO_MOVE)
        {
            break;
        }

        chess::Movelist legalMoves;
        chess::movegen::legalmoves(legalMoves, board);

        bool isLegal = false;

        for (const auto& m : legalMoves)
        {
            if (m == entry->bestMove)
            {
                isLegal = true;
                break;
            }
        }

        if (!isLegal)
            break;

        pv.push_back(entry->bestMove);
        board.makeMove(entry->bestMove);

        if (board.isRepetition(1) || board.isHalfMoveDraw())
            break;
    }

    return pv;
}
// 
chess::Move findBestMove(
    
    chess::Board& board,
    int maxDepth,
    SearchStats& stats,const SearchLimits& limits)
{
    stats.startTime =
    std::chrono::steady_clock::now();

    // stats.stop = false;

stats.nodes = 0;
stats.seldepth = 0;

stats.ttHits = 0;
stats.ttCutoffs = 0;
stats.nullMoveCutoffs = 0;
stats.nullMoveAttempts = 0;
stats.rootTTHits = 0;

stats.aspirationFailLow = 0;
stats.aspirationFailHigh = 0;

stats.lmrReductions = 0;
stats.lmrResearches = 0;
stats.iirReductions = 0;
stats.maxNodes =
    limits.maxNodes > 0
        ? static_cast<uint64_t>(limits.maxNodes)
        : 0;
stats.table.newSearch();

// chess::Move bookMove =
//     stats.book.probe(board);

// if (bookMove != chess::Move::NO_MOVE)
// {
//     return bookMove;
// }
if (!limits.infinite)
{
    chess::Move bookMove =
        stats.book.probe(board);

    if (bookMove != chess::Move::NO_MOVE)
    {
        return bookMove;
    }
}
// fathom integration
chess::Move tbMove =
    chess::Move::NO_MOVE;

if (tryRootTablebase(board, tbMove))
{
    return tbMove;
}
    TimeBudget budget =
    computeTimeBudget(
        limits,
        static_cast<int>(
            board.fullMoveNumber()
        )
    );

stats.optimalMs = budget.optimalMs;
stats.maximumMs = budget.maximumMs;
stats.softStopMs =
    stats.optimalMs;

stats.hardStopMs =
    stats.maximumMs;

stats.timeCheckPeriod = 1024;
    // chess::Move bestMove = chess::Move::NO_MOVE;
    // int bestScore = -INF;

    // constexpr int ASPIRATION_WINDOW = 50;

    // // Root legal-move check
    // chess::Movelist rootMoves;
    // chess::movegen::legalmoves(rootMoves, board);

    // if (rootMoves.empty())
    //     return chess::Move::NO_MOVE;
    // Root legal-move check
chess::Movelist rootMoves;
chess::movegen::legalmoves(rootMoves, board);
constexpr int ASPIRATION_WINDOW = 50;
if (rootMoves.empty())
    return chess::Move::NO_MOVE;
if (rootMoves.size() == 1)
    return rootMoves[0]; // dev sharma 
// Always have a legal fallback move.
chess::Move bestMove = rootMoves[0];

int bestScore = -INF;

    for (int depth = 1; depth <= maxDepth; ++depth)
    {
        stats.extensionCap = maxDepth * 2;
        const int64_t iterationStartMs =elapsedMs(stats);
        const uint64_t iterationStartNodes =stats.nodes;
        // Generate fresh root move list for this iteration.
        chess::Movelist moves;
        chess::movegen::legalmoves(moves, board);

        // Previous iteration's root TT move.
        uint64_t key = board.hash();

        tt::Entry* entry = stats.table.probe(key);

        chess::Move ttMove = chess::Move::NO_MOVE;

        if (entry != nullptr &&
            entry->bestMove != chess::Move::NO_MOVE)
        {
            ++stats.rootTTHits;
            ttMove = entry->bestMove;
        }

        // // Put previous iteration's best move first.
        // if (ttMove != chess::Move::NO_MOVE)
        // {
        //     for (int i = 0;
        //          i < static_cast<int>(moves.size()); //risky
        //          ++i)
        //     {
        //         if (moves[i] == ttMove)
        //         {
        //             if (i != 0)
        //                 std::swap(moves[0], moves[i]);

        //             break;
        //         }
        //     }
        // }
      int rootMoveScores[256];

search::ordering::scoreMoves(
    board,
    moves,
    stats.killers[0],
    chess::Move::NO_MOVE,
    stats.history[
        static_cast<int>(board.sideToMove())
    ],
    stats.continuationHistory,
    stats.continuationHistory2,
    stats.continuationHistory4,
    stats.captureHistory,
    -1, -1,
    -1, -1,
    -1, -1,
    ttMove,
    rootMoveScores
);

        // Depth 1 uses the full window.
        // int alpha = -INF; // risky
        // int beta = INF;

        // if (depth > 1)
        // {
        //     alpha = bestScore - ASPIRATION_WINDOW;
        //     beta = bestScore + ASPIRATION_WINDOW;
        // }
        int alpha = -INF;
int beta = INF;
int delta =
    (depth >= 5)
        ? 20
        : ASPIRATION_WINDOW;
if (depth > 1)
{
    

    alpha = bestScore - delta;
    beta = bestScore + delta;
}
        chess::Move iterationBestMove = chess::Move::NO_MOVE;
        int iterationBestScore = -INF;

        while (true)
        {
            int windowAlpha = alpha;
            int windowBeta = beta;

            iterationBestMove = chess::Move::NO_MOVE;
            iterationBestScore = -INF;
            // for (const auto& move : moves)  // risky
            // {
            //     if (stats.stop)break;
            //     board.makeMove(move);

            //     int score = -negamax(
            //         board,
            //         depth - 1,
            //         -windowBeta,
            //         -windowAlpha,
            //         stats,
            //         1,
            //         move,
            //         true
            //     );

            //     board.unmakeMove(move);
            //     if (stats.stop){
            //         break;
            //         }
            //     if (score > iterationBestScore)
            //     {
            //         iterationBestScore = score;
            //         iterationBestMove = move;
            //     }

            //     if (score > windowAlpha)
            //         windowAlpha = score;
            // }
            for (int i = 0; // risky entire loop
     i < static_cast<int>(moves.size());
     ++i)
{
    search::ordering::pickNextMove(
        moves,
        rootMoveScores,
        i
    );
    const chess::Move& move = moves[i];

    if (stats.stop)
        break;

    const int currentPiece =
    pieceIndexForMoveSearch(board, move);

stats.moveStack[1] = {};

if (currentPiece >= 0)
{
    stats.moveStack[1].piece = currentPiece;
    stats.moveStack[1].to = move.to().index();
    stats.moveStack[1].valid = true;
}

board.makeMove(move);
    
    int score;

    if (i == 0)
    {
        // First/root PV move gets the full aspiration window.
        score = -negamax(
            board,
            depth - 1,
            -windowBeta,
            -windowAlpha,
            stats,
            1,
            move,
            true,0,false
        );
    }
    else
    {
        // Later moves get a null-window search first.
        score = -negamax(
            board,
            depth - 1,
            -windowAlpha - 1,
            -windowAlpha,
            stats,
            1,
            move,
            true,0,true
        );

        // It beat alpha, so we need the real score.
        if (score > windowAlpha)
        {
            score = -negamax(
                board,
                depth - 1,
                -windowBeta,
                -windowAlpha,
                stats,
                1,
                move,
                true,0,false
            );
        }
    }

    board.unmakeMove(move);

    if (stats.stop)
        break;

    if (score > iterationBestScore)
    {
        iterationBestScore = score;
        iterationBestMove = move;
    }

    if (score > windowAlpha)
        windowAlpha = score;
}
            if (stats.stop)break;

            // Fail-low: score is outside the lower bound.
            // if (iterationBestScore <= alpha) //risky
            // {
            //     ++stats.aspirationFailLow;
            //     alpha = -INF;
            //     continue;
            // }

            // // Fail-high: score is outside the upper bound.
            // if (iterationBestScore >= beta)
            // {
            //     ++stats.aspirationFailHigh;
            //     beta = INF;
            //     continue;
            // }
//             if (iterationBestScore <= alpha)
// {
//     ++stats.aspirationFailLow;

//     alpha = -INF;  //risky
//     continue;
// }

// if (iterationBestScore >= beta)
// {
//     ++stats.aspirationFailHigh;

//     beta = INF;
//     continue;
// }
            if (iterationBestScore <= alpha)
{
    ++stats.aspirationFailLow;

    delta += delta / 2;

    if (delta > 400)
    {
        alpha = -INF;
        beta = INF;
    }
    else
    {
        beta  = (alpha + beta) / 2;
        alpha = std::max(iterationBestScore - delta, -INF);
    }

    continue;
}

          if (iterationBestScore >= beta)
{
    ++stats.aspirationFailHigh;

    if (iterationBestMove != chess::Move::NO_MOVE)
    {
        for (int k = 0; k < static_cast<int>(moves.size()); ++k)
        {
            if (moves[k] == iterationBestMove)
            {
                rootMoveScores[k] = 2000000000;
                break;
            }
        }
    }

    delta += delta / 2;

    if (delta > 400)
    {
        alpha = -INF;
        beta  = INF;
    }
    else
    {
        beta = std::min(iterationBestScore + delta, INF);
    }

    continue;
}
            // Score is inside the aspiration window.
            break;
        }
        // if (stats.stop) break;
       if (stats.stop)
{
    if (iterationBestMove != chess::Move::NO_MOVE &&
        iterationBestScore > alpha)
    {
        bestMove  = iterationBestMove;
        bestScore = iterationBestScore;
    }
    break;
}
    
        // aspiration search finishes successfully

bool bestMoveChanged =
    bestMove != chess::Move::NO_MOVE &&
    iterationBestMove != bestMove;

bool scoreDropped =
    depth > 1 &&
    iterationBestScore < bestScore - 50;

stats.stable =
    !bestMoveChanged &&
    !scoreDropped;

bestMove = iterationBestMove;
bestScore = iterationBestScore;
const int64_t iterationTime =
    elapsedMs(stats) - iterationStartMs;

const uint64_t iterationNodes =
    stats.nodes - iterationStartNodes;

stats.previousIterationMs =
    stats.lastIterationMs;

stats.previousIterationNodes =
    stats.lastIterationNodes;

stats.lastIterationMs =
    iterationTime;

stats.lastIterationNodes =
    static_cast<int64_t>(iterationNodes);

stats.completedDepth =
    depth;

stats.previousScore = bestScore;

// Store root result BEFORE extractPV().
stats.table.store(
    key,
    depth,
    tt::valueToTT(bestScore, 0),
    tt::Bound::EXACT,
    bestMove
);

int64_t softLimit = stats.optimalMs;

if (!stats.stable)
{
    softLimit =
        std::min(
            stats.maximumMs,
            stats.optimalMs * 2
        );
}

// // If the last iteration was unexpectedly cheap,
// // allow another iteration rather than stopping too early.
// if (stats.completedDepth >= 2 &&
//     stats.lastIterationMs > 0)
// {
//     const int64_t estimatedNextIteration =
//         stats.lastIterationMs * 3 / 2;

//     const int64_t elapsed =
//         elapsedMs(stats);

//     if (elapsed + estimatedNextIteration <
//         stats.maximumMs)
//     {
//         if (softLimit <
//             elapsed + estimatedNextIteration)
//         {
//             softLimit =
//                 std::min(
//                     stats.maximumMs,
//                     elapsed +
//                     estimatedNextIteration
//                 );
//         }
//     }
// }
// std::cerr << "Stable: "
//           << (stats.stable ? "yes" : "no")
//           << '\n';

// std::cerr << "Soft limit: "
//           << softLimit << '\n';

if (stats.onIteration)
{
    SearchInfo info;
    info.depth = depth;
    info.seldepth = stats.seldepth;
    info.nodes = stats.nodes;
    info.timeMs = elapsedMs(stats);
    info.nps = info.timeMs > 0
        ? (info.nodes * 1000ULL) / static_cast<uint64_t>(info.timeMs)
        : info.nodes;
    info.hashfull = stats.table.hashfull();

    if (bestScore > MATE_SCORE - MAX_PLY ||
        bestScore < -(MATE_SCORE - MAX_PLY))
    {
        info.isMate = true;
        int pliesToMate = MATE_SCORE -
            (bestScore > 0 ? bestScore : -bestScore);
        int movesToMate = (pliesToMate + 1) / 2;
        info.mateIn = bestScore > 0 ? movesToMate : -movesToMate;
        info.score = 0;
    }
    else
    {
        info.isMate = false;
        info.score = bestScore;
        info.mateIn = 0;
    }

    info.pv = extractPV(board, stats, depth);

    stats.onIteration(info);
}
// Stop when a forced mate has been established
// within the completed search depth.
// dev sharma
if (bestScore >= MATE_SCORE - depth)
{
    break;
}
// dev sharma 
// int64_t elapsed = elapsedMs(stats);

// if (stats.optimalMs > 0 &&  
//     elapsed >= softLimit)
// {
//     break;
// }
// dev sharma 
int64_t elapsed = elapsedMs(stats);

if (stats.optimalMs > 0)
{
    // Avoid starting another iteration too late.
    const int64_t latestStart =
        softLimit * 55 / 100;

    const int64_t estimatedNextIteration =
        std::max<int64_t>(
            1,
            stats.lastIterationMs * 3 / 2
        );

    if (elapsed >= softLimit ||
        elapsed >= latestStart ||
        elapsed + estimatedNextIteration >= softLimit)
    {
        break;
    }
}
        // Store root result for the next iteration.
        

//         std::cerr << "Depth: " << depth << '\n';
//         std::cerr << "Score: " << bestScore << '\n';
//         std::cerr << "Best move: "
//                   << bestMove.from().index()
//                   << " -> "
//                   << bestMove.to().index()
//                   << '\n';
//         std::cerr << "Root TT hits: "
//                   << stats.rootTTHits << '\n';
//         std::cerr << "----------------\n";
//         std::cerr << "Aspiration fail-low: "
//           << stats.aspirationFailLow << '\n';

// std::cerr << "Aspiration fail-high: "
//           << stats.aspirationFailHigh << '\n';
          
//           std::cerr << "LMR reductions: "
//           << stats.lmrReductions << '\n';

// std::cerr << "LMR researches: "
//           << stats.lmrResearches << '\n';
         
    }

    return bestMove;
}
}

