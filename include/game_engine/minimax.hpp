#pragma once

#include <chrono>
#include <limits>

#include "core.hpp"
#include "metrics.hpp"

namespace game_engine {

// Classic Minimax. DECISION nodes are max/min layers; CHANCE nodes (Ludo
// dice) are expectation layers. The value is always computed from the
// perspective of a fixed `player`, which is required because a chance node
// can keep the turn with the same player (roll-a-6-again rule), so the
// negamax sign flip is not valid across chance nodes.
namespace detail {

inline double minimaxValue(const GameState& s, int player, int depth,
                           SearchStats& st) {
    ++st.nodes;
    if (s.isTerminal()) {
        int w = s.winner();
        if (w == kDraw) return 0.0;
        return (w == player) ? 100000.0 + depth : -(100000.0 + depth);
    }
    if (depth <= 0) return s.heuristic(player);

    if (s.isChanceNode()) {
        double total = 0.0, wsum = 0.0;
        for (const Move& m : s.getMoves()) {
            auto next = s.clone();
            next->applyMove(m);
            total += m.weight * minimaxValue(*next, player, depth - 1, st);
            wsum += m.weight;
        }
        return total / wsum;
    }

    bool maximising = (s.currentPlayer() == player);
    double best = maximising ? -std::numeric_limits<double>::infinity()
                             : std::numeric_limits<double>::infinity();
    for (const Move& m : s.getMoves()) {
        auto next = s.clone();
        next->applyMove(m);
        double v = minimaxValue(*next, player, depth - 1, st);
        best = maximising ? std::max(best, v) : std::min(best, v);
    }
    return best;
}

}  // namespace detail

inline Move minimax(const GameState& root, int depth, SearchStats& st) {
    auto t0 = std::chrono::steady_clock::now();
    st = SearchStats{};
    st.depthReached = depth;

    int player = root.currentPlayer();
    Move best;
    double bestVal = -std::numeric_limits<double>::infinity();
    for (const Move& m : root.getMoves()) {
        auto next = root.clone();
        next->applyMove(m);
        double v = detail::minimaxValue(*next, player, depth - 1, st);
        if (v > bestVal) { bestVal = v; best = m; }
    }
    st.milliseconds = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0).count();
    return best;
}

}  // namespace game_engine
