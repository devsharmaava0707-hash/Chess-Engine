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

    if (limits.moveTimeMs > 0)
    {
        const int64_t usable =
            std::max<int64_t>(
                1,
                limits.moveTimeMs -
                // COMMUNICATION_MARGIN_MS
                limits.moveOverheadMs
            );

        return {
            usable,
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

    int movesToGo =
        limits.movesToGo;

    if (movesToGo <= 0)
        movesToGo =
            estimateMovesToGo(moveNumber);

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

    int64_t overhead =limits.moveOverheadMs;
        // COMMUNICATION_MARGIN_MS; 

    if (timeLeft >= 10000)
        overhead = 75;

    if (timeLeft >= 30000)
        overhead = 100;

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

    int64_t base =
        usable / movesToGo;

    base += effectiveIncrement;

    // --------------------------------------------------------
    // Give ourselves a little more time when the clock is
    // comfortable, but never spend too much of the bank.
    // --------------------------------------------------------

    int64_t soft =
        base;

    if (timeLeft > 30000)
        soft += usable / 20;      // +5%

    else if (timeLeft > 10000)
        soft += usable / 30;      // +3.3%

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
    const int64_t hardCap =
        std::max<int64_t>(
            1,
            (timeLeft * 85) / 100
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

    int64_t hard =
        soft * 5 / 2;

    hard =
        std::min(
            hard,
            hardCap
        );

    hard =
        std::min(
            hard,
            hardAvailable
        );

    hard =
        std::max(
            hard,
            soft
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
    // int score = eval::evaluate(board);
    int score = eval::evaluate(board);

    return board.sideToMove() == chess::Color::WHITE
        ? score
        : -score;
    // return eval::evaluate(board);
}
int quiescence(chess::Board& board,int alpha,int beta,SearchStats& stats,int qply){
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
if (qply > stats.seldepth)
    stats.seldepth = qply;
if (board.isRepetition(1))
    return 0;

if (board.isHalfMoveDraw())
    return 0;

if (board.isInsufficientMaterial())
    return 0;
    // Safety limit for the tactical search.
    if (qply >= MAX_QPLY)
        return evaluateForSideToMove(board);
const int qDepth = -qply;
uint64_t key = board.hash();
tt::Entry* entry = stats.table.probe(key);

int alphaOriginal = alpha;

if (entry != nullptr &&
    entry->depth >= qDepth)
{
    int ttScore =
        tt::valueFromTT(entry->score, qply);

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

    /*
        If we are not in check, we can stand pat:
        "What if I make no more tactical captures?"
    */
    if (!inCheck)
{
    int standPat = evaluateForSideToMove(board);

    // if (standPat >= beta)
    //     return beta;
    if (standPat >= beta)
{
    if (!stats.stop)
        stats.table.store(
            key,
            qDepth,
            tt::valueToTT(standPat, qply),
            tt::Bound::LOWERBOUND,
            chess::Move::NO_MOVE
        );

    return standPat;
}

    if (standPat > alpha)
        alpha = standPat;

    // Delta pruning
    constexpr int DELTA_MARGIN = 200;

    // if (standPat +
    //     piece_value_bonus(
    //         chess::PieceType::QUEEN,
    //         true
    //     ) +
    //     DELTA_MARGIN < alpha)
    if (standPat +
    eval::materialValue(chess::PieceType::QUEEN) + // for arun evaluation 
    DELTA_MARGIN < alpha)
    {
        return standPat;
    }
}

    chess::Movelist moves;

    if (inCheck)
    {
        // In check: we must consider every legal response.
        chess::movegen::legalmoves(moves, board);
    }
    else
    {
        // Not in check: only forcing captures.
        chess::movegen::legalmoves<
            chess::movegen::MoveGenType::CAPTURE
        >(moves, board);
    }

    // Checkmate during qsearch.
    if (moves.empty())
    {
        if (inCheck)
            return -MATE_SCORE + qply;

        return alpha;
    }

    for (const auto& move : moves)
    {
    // In a normal quiescence node, skip captures
    // that SEE says are materially losing.
    //
    // When in check, we must examine every legal move.
    if (!inCheck && search::see::evaluate(board, move) < 0)
        continue;

    board.makeMove(move);

    int score = -quiescence(
        board,
        -beta,
        -alpha,
        stats,
        qply + 1
    );

    board.unmakeMove(move);
    if (stats.stop)
    {
        return 0;
    }
    // if (score >= beta)
    //     return beta;

    if (score >= beta)
{
    if (!stats.stop)
        stats.table.store(
            key,
            qDepth,
            tt::valueToTT(score, qply),
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

if (alpha <= alphaOriginal)
    bound = tt::Bound::UPPERBOUND;
else if (alpha >= beta)
    bound = tt::Bound::LOWERBOUND;

if (!stats.stop)
{
    stats.table.store(
        key,
        qDepth,
        tt::valueToTT(alpha, qply),
        bound,
        chess::Move::NO_MOVE
    );
}

return alpha;
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

    int negamax(chess::Board& board,int depth,int alpha,int beta,SearchStats& stats,int ply,chess:: Move prevMove,bool nullMoveAllowed,int extCount)
{
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
// Draw detection.
// Do not apply this at the root.
if (ply > 0)
{
    if (board.isRepetition(1))
        return 0;

    if (board.isHalfMoveDraw())
        return 0;

    if (board.isInsufficientMaterial())
        return 0;
}
    int alphaOriginal = alpha;
    uint64_t key = board.hash();
    tt::Entry* entry = stats.table.probe(key);

if (entry != nullptr)
{
    ++stats.ttHits;

    if (entry->depth >= depth)
    {
        int ttScore =
            tt::valueFromTT(entry->score, ply);

        if (entry->bound == tt::Bound::EXACT)
        {
            ++stats.ttCutoffs;
            return ttScore;
        }

        if (entry->bound == tt::Bound::LOWERBOUND &&
            ttScore >= beta)
        {
            ++stats.ttCutoffs;
            return ttScore;
        }

        if (entry->bound == tt::Bound::UPPERBOUND &&
            ttScore <= alpha)
        {
            ++stats.ttCutoffs;
            return ttScore;
        }
    }
}

    if (depth <= 0)return quiescence(board, alpha, beta, stats,ply);
    // REF
    bool isPV = (beta - alpha > 1);
    int staticEval = evaluateForSideToMove(board);
    bool improving = false;

if (!board.inCheck())
{
    stats.staticEvalStack[ply] = staticEval;

    // Compare with the same side's evaluation two plies earlier.
    // We start at ply 3 because the root itself is not searched through
    // negamax in the current implementation.
    if (ply >= 3)
    {
        improving =
            staticEval >= stats.staticEvalStack[ply - 2];
    }
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
    !board.inCheck() &&
    depth <= 6 &&
    staticEval - rfpMargin >= beta)
{
    return staticEval;
} 
    // Razoring
    if (!isPV &&
    !board.inCheck() &&
    depth <= 3 &&
    staticEval + 200 * depth < alpha)
    {
        return quiescence(board, alpha, beta, stats,ply);
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
    if (!board.inCheck() &&
        depth >= 4 &&
        (entry == nullptr || entry->bestMove == chess::Move::NO_MOVE))
    {
        searchDepth = isPV ? depth - 1 : depth - 2;
        ++stats.iirReductions;   // add this counter to SearchStats, like lmrReductions
    } 
    if (!isPV &&
    !board.inCheck() &&
    nullMoveAllowed &&
    depth >= 3 &&
     
    staticEval >= beta)
{
    // ++nullMoveAttempts = 0;
     ++stats.nullMoveAttempts;
    chess::Color us = board.sideToMove();

    chess::Bitboard nonPawnPieces =
        board.us(us) &
        ~board.pieces(chess::PieceType::PAWN, us) &
        ~board.pieces(chess::PieceType::KING, us);

    if (nonPawnPieces.count() >= 1)
    {
        int R = (depth >= 6 ? 3 : 2);

        if (staticEval - beta >= 200)
            ++R;

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
    // }
            board.makeNullMove();

        int nullScore = -negamax(
            board,
            depth - 1 - R,
            -beta,
            -beta + 1,
            stats,
            ply + 1,
            chess::Move::NO_MOVE,
            false,extCount
        );

        board.unmakeNullMove();

        if (stats.stop)
            return 0;

        if (nullScore >= beta)
        {
            ++stats.nullMoveCutoffs;

            if (depth < NMP_VERIFICATION_MIN_DEPTH)
                return beta;

            int verifyScore = negamax(
                board,
                depth - R,
                beta - 1,
                beta,
                stats,
                ply + 1,
                prevMove,
                false,extCount
            );

            if (stats.stop)
                return 0;

            if (verifyScore >= beta)
                return beta;
        }
    }
}
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    if (moves.empty())
{
    if (board.inCheck())
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
    search::ordering::orderMoves(
        board,
        moves,
        stats.killers[safePly],
        counterMove,
        stats.history[side],
        // stats.continuationHistory,
        ttMove
    );

    int bestScore = -INF;
    chess::Move bestMove = chess::Move::NO_MOVE;
    constexpr int MAX_QUIETS = 256;
    chess::Move quietsTried[MAX_QUIETS];
    int quietCount = 0;
    bool firstMove=true;
    bool inCheck = board.inCheck();
    int moveIndex=0;
    for (const auto& move : moves)
{
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
    // Step 5: Main-search SEE pruning
if (!isPV &&
    !inCheck &&
    depth <= 6 &&
    isCapture &&
    !isPromotion &&
    search::see::evaluate(board, move) < 0)
{
    continue;
}

board.makeMove(move);


    bool givesCheck = board.inCheck();
// extension add
    int extension = 0;

if (givesCheck && depth >= 2 && extCount < stats.extensionCap)
    extension = 1;
// // above is extension add
    int reduction = 0;
    int score;

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
            true,extCount + extension
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
                isCapture,
                isPromotion,
                inCheck,
                givesCheck,
                isKillerOrCounter,
                historyScore
            );
            if (reduction > 0)
        ++stats.lmrReductions;
        score = -negamax(
            board,
            searchDepth - 1 -reduction +extension, // extension add
            -alpha - 1,
            -alpha,
            stats,
            ply + 1,
            move,
            true,extCount + extension
        );

        if (score > alpha &&
            (reduction > 0 || isPV))
        {
            if (reduction > 0)
    ++stats.lmrResearches;
            score = -negamax(
                board,
                searchDepth - 1+extension, // extension add
                -beta,
                -alpha,
                stats,
                ply + 1,
                move,
                true,extCount + extension
            );
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

    if (alpha >= beta)
    {
        if (isQuiet)
        {
            int safePly =
                std::min(ply, MAX_PLY - 1);

            stats.killers[safePly][1] =
                stats.killers[safePly][0];

            stats.killers[safePly][0] =
                move;
            if (prevMove != chess::Move::NO_MOVE)
{
    stats.counterMoves[side]
                        [prevMove.from().index()]
                        [prevMove.to().index()] =
        move;
}


           if (prevMove != chess::Move::NO_MOVE)
{
    stats.counterMoves[side]
        [prevMove.from().index()]
        [prevMove.to().index()] =
        move;
}

int& h =
    stats.history[side]
                [move.from().index()]
                [move.to().index()];

h += depth * depth;

if (h > 30000)
    h = 30000;

            for (int i = 0; i < quietCount; ++i)
            {
                const chess::Move& qm =
                    quietsTried[i];

                int& hq =
                    stats.history[side]
                                [qm.from().index()]
                                [qm.to().index()];

                hq -= depth * depth / 2;

                if (hq < -30000)
                    hq = -30000;
            }
        }

        break;
    }

    ++moveIndex;
}

    tt::Bound bound = tt::Bound::EXACT;

if (bestScore <= alphaOriginal)
    bound = tt::Bound::UPPERBOUND;
else if (bestScore >= beta)
    bound = tt::Bound::LOWERBOUND;
if (stats.stop)
    return 0;
stats.table.store(
    key,
    depth,
    tt::valueToTT(bestScore, ply),
    bound,
    bestMove
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

    stats.stop = false;

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

chess::Move bookMove =
    stats.book.probe(board);

if (bookMove != chess::Move::NO_MOVE)
{
    return bookMove;
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

// Always have a legal fallback move.
chess::Move bestMove = rootMoves[0];

int bestScore = -INF;
stats.extensionCap = maxDepth * 2;   // NEW
    for (int depth = 1; depth <= maxDepth; ++depth)
    {
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
        search::ordering::orderMoves(
    board,
    moves,
    stats.killers[0],
    chess::Move::NO_MOVE,
    stats.history[                              // risky
        static_cast<int>(board.sideToMove())
    ],
    // stats.continuationHistory,
    ttMove
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
int delta = ASPIRATION_WINDOW; // risky entire till beta=bestcore
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
    const chess::Move& move = moves[i];

    if (stats.stop)
        break;

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
            true,0
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
            true,0
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
                true,0
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

                beta  = (alpha + beta) / 2;
                alpha = std::max(iterationBestScore - delta, -INF);
                delta += delta / 2;
                continue;
            }

            if (iterationBestScore >= beta)
            {
                ++stats.aspirationFailHigh;

                beta = std::min(iterationBestScore + delta, INF);
                delta += delta / 2;
                continue;
            }
            // Score is inside the aspiration window.
            break;
        }
        if (stats.stop) break;
    
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

int64_t elapsed = elapsedMs(stats);

if (stats.optimalMs > 0 &&
    elapsed >= softLimit)
{
    break;
}
        // Store root result for the next iteration.
        stats.table.store(
            key,
            depth,
            tt::valueToTT(bestScore, 0),
            tt::Bound::EXACT,
            bestMove
        );

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

