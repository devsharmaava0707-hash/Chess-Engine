#ifndef EVALUATION_HPP
#define EVALUATION_HPP

#include "chess.hpp"

// Main evaluation
int evaluate(chess::Board& board);

// Material
int piece_value_bonus(chess::PieceType piece, bool mg);

int piece_value_mg(const chess::Board& board, chess::Square square);
int piece_value_eg(const chess::Board& board, chess::Square square);

int total_piece_value_mg(const chess::Board& board);
int total_piece_value_eg(const chess::Board& board);

// PSQT
int psqt_bonus(const chess::Board& board,
               chess::Square square,
               bool mg);

int total_psqt_mg(const chess::Board& board);
int total_psqt_eg(const chess::Board& board);

// MG / EG
int middlegame_score(const chess::Board& board);
int endgame_score(const chess::Board& board);

// Phase
// int non_pawn_material(const chess::Board& board);
int non_pawn_material(chess::Board& board, chess::Color color);
int phase(chess::Board& board);

// Scale factor helpers
int pawn_count(const chess::Board& board, chess::Color color);
int queen_count(const chess::Board& board, chess::Color color);
int bishop_count(const chess::Board& board, chess::Color color);
int knight_count(const chess::Board& board, chess::Color color);

bool opposite_bishops(const chess::Board& board);

int piece_count(const chess::Board& board);
int piece_count(const chess::Board& board, chess::Color color);

int supported(chess::Board& board,
              chess::Color perspective,
              int x,
              int y);

int candidate_passed(chess::Board& board,
                     chess::Color perspective);

int scale_factor(chess::Board& board, int eg);

// Final evaluation components
int tempo(chess::Board& board);
int rule50(chess::Board& board);
// Mobility
int mobility_mg(const chess::Board& board);
int mobility_eg(const chess::Board& board);
int pawn_structure_mg(const chess::Board& board);
int pawn_structure_eg(const chess::Board& board);
int mobility_total_mg(const chess::Board& board);
int mobility_total_eg(const chess::Board& board);
// Passed pawns
int passed_pawn_mg(const chess::Board& board);
int passed_pawn_eg(const chess::Board& board);

// Bishop evaluation
int bishop_eval_mg(const chess::Board& board);
int bishop_eval_eg(const chess::Board& board);

// Rook activity
int rook_eval_mg(const chess::Board& board);
int rook_eval_eg(const chess::Board& board);

#endif