#pragma once

#include <chrono>
#include <limits>

#include "core.hpp"
#include "metrics.hpp"

namespace game_engine {

// Alpha-Beta search. Pruning happens at DECISION (max/min) nodes; CHANCE
// nodes are evaluated as expectations (alpha-beta over expectimax). Values
// are from a fixed player's perspective - necessary because chance nodes can
// keep the turn with the same player.
namespace detail {

inline double abValue(const GameState& s, int player, int depth,
                      double alpha, double beta, SearchStats& st);

inline double chanceNode(const GameState& s, int player, int depth,
                         double alpha, double beta, SearchStats& st) {
    ++st.nodes;
    double total = 0.0, wsum = 0.0;
    for (const Move& m : s.getMoves()) {
        auto next = s.clone();
        next->applyMove(m);
        total += m.weight * abValue(*next, player, depth - 1, alpha, beta, st);
        wsum += m.weight;
    }
    return total / wsum;
}

inline double abValue(const GameState& s, int player, int depth,
                      double alpha, double beta, SearchStats& st) {
    ++st.nodes;
    if (s.isTerminal()) {
        int w = s.winner();
        if (w == kDraw) return 0.0;
        return (w == player) ? 100000.0 + depth : -(100000.0 + depth);
    }
    if (depth <= 0) return s.heuristic(player);

    if (s.isChanceNode()) return chanceNode(s, player, depth, alpha, beta, st);

    bool maximising = (s.currentPlayer() == player);
    double best = maximising ? -std::numeric_limits<double>::infinity()
                             : std::numeric_limits<double>::infinity();
    for (const Move& m : s.getMoves()) {
        auto next = s.clone();
        next->applyMove(m);
        double v = abValue(*next, player, depth - 1, alpha, beta, st);
        if (maximising) {
            best = std::max(best, v);
            alpha = std::max(alpha, best);
        } else {
            best = std::min(best, v);
            beta = std::min(beta, best);
        }
        if (alpha >= beta) { ++st.pruned; break; }  // cutoff
    }
    return best;
}

}  // namespace detail

inline Move alphabeta(const GameState& root, int depth, SearchStats& st) {
    auto t0 = std::chrono::steady_clock::now();
    st = SearchStats{};
    st.depthReached = depth;

    int player = root.currentPlayer();
    Move best;
    double bestVal = -std::numeric_limits<double>::infinity();
    for (const Move& m : root.getMoves()) {
        auto next = root.clone();
        next->applyMove(m);
        double v = detail::abValue(*next, player, depth - 1,
                                   -std::numeric_limits<double>::infinity(),
                                   std::numeric_limits<double>::infinity(), st);
        if (v > bestVal) { bestVal = v; best = m; }
    }
    st.milliseconds = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0).count();
    return best;
}

}  // namespace game_engine
