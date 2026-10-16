
#pragma once

namespace search::tuning
{
    // Reverse futility pruning
    inline constexpr int RFP_MARGIN_PER_DEPTH = 120;
    inline constexpr int RFP_NOT_IMPROVING_PENALTY = 70;
    inline constexpr int RFP_MAX_DEPTH = 6;

    // Quiet-move futility pruning
    inline constexpr int FUTILITY_MARGIN_PER_DEPTH = 120;
    inline constexpr int FUTILITY_MAX_DEPTH = 3;

    // Late Move Pruning
    inline constexpr int LMP_BASE = 3;
    inline constexpr int LMP_MAX_DEPTH = 8;

    // Quiet-move history pruning
    inline constexpr int HISTORY_PRUNING_MAX_DEPTH = 3;
    inline constexpr int HISTORY_PRUNING_THRESHOLD = 4000;

    // Quiet-move SEE pruning
    inline constexpr int QUIET_SEE_MAX_DEPTH = 4;
    inline constexpr int QUIET_SEE_MARGIN = 20;

    // LMR formula and adjustments
    inline constexpr double LMR_LOG_BASE = 0.4;
    inline constexpr double LMR_LOG_DIVISOR = 2.0;
    inline constexpr int LMR_HISTORY_DIVISOR = 8192;
    inline constexpr int LMR_NOT_IMPROVING_BONUS = 1;
    inline constexpr int LMR_CUTNODE_BONUS = 1;
    inline constexpr int LMR_TT_CAPTURE_BONUS = 1;
    inline constexpr int LMR_PV_DISCOUNT = 1;
    inline constexpr int LMR_KILLER_DISCOUNT = 1;

    // Aspiration window
    inline constexpr int ASPIRATION_INITIAL_WINDOW = 50;
    inline constexpr int ASPIRATION_DEEP_WINDOW = 20;
    inline constexpr int ASPIRATION_DEEP_DEPTH = 5;
}
