#pragma once

#include <string>

#include "core.hpp"

namespace game_engine {

// Counters accumulated inside a single search call.
struct SearchStats {
    long long nodes = 0;        // game-tree nodes evaluated
    long long pruned = 0;       // branches cut by alpha-beta
    long long simulations = 0;  // MCTS rollouts
    double milliseconds = 0.0;
    int depthReached = 0;
};

// One AI decision inside a match (per-decision export row).
struct DecisionStats {
    std::string algorithm;  // algorithm that made this decision
    int ply = 0;             // plies elapsed when the decision was made
    int player = 0;          // 1 | 2 (side that acted)
    long long nodes = 0;     // nodes evaluated by this search
    long long pruned = 0;    // alpha-beta cutoffs in this search
    long long sims = 0;      // MCTS rollouts in this search
    double milliseconds = 0.0;
};

// Aggregated metrics for one full match (experiment row).
struct Metrics {
    std::string game;
    std::string algorithm;      // "p1algo" or "p1algo vs p2algo"
    int depth = 0;
    int simulations = 0;
    unsigned seed = 0;

    int movesPlayed = 0;
    double totalDecisionMs = 0.0;
    double avgDecisionMs = 0.0;
    long long totalNodes = 0;
    long long totalPruned = 0;
    long long totalSims = 0;

    int result = kDraw;  // outcome for player 1: 1 win, 2 loss, 0 draw
};

}  // namespace game_engine
