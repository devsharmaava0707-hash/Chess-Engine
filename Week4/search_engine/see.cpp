#include "see.h"
#include "../manual_evaluation_chess/eval.hpp"
#include<algorithm>
namespace search::see
{
    namespace
    {
        int pieceValue(chess::PieceType type)
        {
            return piece_value_bonus(type, true);
        }

        bool leastValuableAttacker(
            const chess::Board& board,
            chess::Color side,
            chess::Square target,
            const chess::Bitboard& occupancy,
            chess::Square& attackerSquare,
            chess::PieceType& attackerType)
        {
            static constexpr chess::PieceType::underlying order[] =
            {
                chess::PieceType::underlying::PAWN,
                chess::PieceType::underlying::KNIGHT,
                chess::PieceType::underlying::BISHOP,
                chess::PieceType::underlying::ROOK,
                chess::PieceType::underlying::QUEEN,
                chess::PieceType::underlying::KING
            };

            for (auto type : order)
            {
                chess::Bitboard attackers;

                switch (type)
                {
                    case chess::PieceType::underlying::PAWN:
                        attackers =
                            chess::attacks::pawn(~side, target)
                            & board.pieces(
                                chess::PieceType::PAWN,
                                side
                            )
                            & occupancy;
                        break;

                    case chess::PieceType::underlying::KNIGHT:
                        attackers =
                            chess::attacks::knight(target)
                            & board.pieces(
                                chess::PieceType::KNIGHT,
                                side
                            )
                            & occupancy;
                        break;

                    case chess::PieceType::underlying::BISHOP:
                        attackers =
                            chess::attacks::bishop(
                                target,
                                occupancy
                            )
                            & board.pieces(
                                chess::PieceType::BISHOP,
                                side
                            )
                            & occupancy;
                        break;

                    case chess::PieceType::underlying::ROOK:
                        attackers =
                            chess::attacks::rook(
                                target,
                                occupancy
                            )
                            & board.pieces(
                                chess::PieceType::ROOK,
                                side
                            )
                            & occupancy;
                        break;

                    case chess::PieceType::underlying::QUEEN:
                        attackers =
                            chess::attacks::queen(
                                target,
                                occupancy
                            )
                            & board.pieces(
                                chess::PieceType::QUEEN,
                                side
                            )
                            & occupancy;
                        break;

                    case chess::PieceType::underlying::KING:
                        attackers =
                            chess::attacks::king(target)
                            & board.pieces(
                                chess::PieceType::KING,
                                side
                            )
                            & occupancy;
                        break;

                    default:
                        continue;
                }

                if (attackers.count() != 0)
                {
                    attackerSquare =
                        chess::Square(attackers.lsb());

                    attackerType =
                        chess::PieceType(type);

                    return true;
                }
            }

            return false;
        }


        int seeImpl(
            const chess::Board& board,
            const chess::Move& move)
        {
            /*
                SEE only makes sense for captures.
            */

            if (move.typeOf() == chess::Move::CASTLING)
                return 0;

            chess::Color mover = board.sideToMove();

            chess::Square from = move.from();
            chess::Square to = move.to();

            chess::Piece capturedPiece = board.at(to);

            /*
                En passant is special because the captured pawn is
                not located on the destination square.
            */
            if (move.typeOf() == chess::Move::ENPASSANT)
            {
                capturedPiece =
                    chess::Piece(
                        chess::PieceType::PAWN,
                        ~mover
                    );
            }

            /*
                Not a capture.
            */
            if (capturedPiece == chess::Piece::NONE)
                return 0;

            /*
                Temporary occupancy.

                We don't actually make moves on the Board.
                We only modify this occupancy bitboard while
                simulating the exchange.
            */
            chess::Bitboard occupancy = board.occ();

            occupancy.clear(from.index());

            /*
                In en passant, remove the pawn that was captured.
            */
            if (move.typeOf() == chess::Move::ENPASSANT)
            {
                int capturedPawnSquare =
                    to.index() +
                    (mover == chess::Color::WHITE ? -8 : 8);

                occupancy.clear(capturedPawnSquare);
            }

            /*
                What piece ends up on the target square after
                the first move?

                For a promotion, it is the promoted piece.
            */
            chess::PieceType currentPieceType =
                (move.typeOf() == chess::Move::PROMOTION)
                    ? move.promotionType()
                    : board.at(from).type();

            constexpr int MAX_EXCHANGE = 32;

            int gain[MAX_EXCHANGE];
            chess::PieceType pieceOnSquare[MAX_EXCHANGE];

            int depth = 0;

            /*
                Initial gain = value of the piece we captured.
            */
            gain[0] = pieceValue(capturedPiece.type());
            pieceOnSquare[0] = currentPieceType;

            /*
                Opponent gets the first recapture.
            */
            chess::Color side = ~mover;

            while (depth + 1 < MAX_EXCHANGE)
            {
                chess::Square attackerSquare;
                chess::PieceType attackerType;

                if (!leastValuableAttacker(
                        board,
                        side,
                        to,
                        occupancy,
                        attackerSquare,
                        attackerType))
                {
                    break;
                }

                ++depth;

                /*
                    If the opponent captures our current piece,
                    the gain changes by the value of the piece
                    currently sitting on the target square.
                */
                gain[depth] =
                    pieceValue(pieceOnSquare[depth - 1])
                    - gain[depth - 1];

                pieceOnSquare[depth] = attackerType;

                /*
                    Remove the attacker from the temporary board.
                */
                occupancy.clear(attackerSquare.index());

                side = ~side;
            }

            /*
                Fold the exchange backwards.

                Each side can effectively decide whether continuing
                the exchange is better than stopping.

                This is the minimax part of SEE.
            */
            while (depth > 0)
            {
                gain[depth - 1] =
                    -std::max(
                        -gain[depth - 1],
                        gain[depth]
                    );

                --depth;
            }

            return gain[0];
        }
    }
    int evaluate(
    const chess::Board& board,
    const chess::Move& move)
{
    return seeImpl(board, move);
}
}
