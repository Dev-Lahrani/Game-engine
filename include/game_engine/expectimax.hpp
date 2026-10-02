#pragma once

#include <chrono>
#include <limits>

#include "core.hpp"
#include "metrics.hpp"

namespace game_engine {

// Expectimax - the natural algorithm for stochastic games like Ludo.
//   maxValue    : DECISION node where `player` to move  -> maximise
//   minValue    : DECISION node where the opponent moves -> minimise
//   chanceValue : CHANCE node -> expected value over dice outcomes
namespace detail {

inline double maxValue(const GameState& s, int player, int depth, SearchStats& st);
inline double minValue(const GameState& s, int player, int depth, SearchStats& st);
inline double chanceValue(const GameState& s, int player, int depth, SearchStats& st);

inline double maxValue(const GameState& s, int player, int depth, SearchStats& st) {
    ++st.nodes;
    if (s.isTerminal()) {
        int w = s.winner();
        if (w == kDraw) return 0.0;
        return (w == player) ? 100000.0 + depth : -(100000.0 + depth);
    }
    if (depth <= 0) return s.heuristic(player);
    double best = -std::numeric_limits<double>::infinity();
    for (const Move& m : s.getMoves()) {
        auto next = s.clone();
        next->applyMove(m);
        double v = (next->isChanceNode())
                       ? chanceValue(*next, player, depth - 1, st)
                       : (next->currentPlayer() == player
                              ? maxValue(*next, player, depth - 1, st)
                              : minValue(*next, player, depth - 1, st));
        best = std::max(best, v);
    }
    return best;
}

inline double minValue(const GameState& s, int player, int depth, SearchStats& st) {
    ++st.nodes;
    if (s.isTerminal()) {
        int w = s.winner();
        if (w == kDraw) return 0.0;
        return (w == player) ? 100000.0 + depth : -(100000.0 + depth);
    }
    if (depth <= 0) return s.heuristic(player);
    double best = std::numeric_limits<double>::infinity();
    for (const Move& m : s.getMoves()) {
        auto next = s.clone();
        next->applyMove(m);
        double v = (next->isChanceNode())
                       ? chanceValue(*next, player, depth - 1, st)
                       : (next->currentPlayer() == player
                              ? maxValue(*next, player, depth - 1, st)
                              : minValue(*next, player, depth - 1, st));
        best = std::min(best, v);
    }
    return best;
}

inline double chanceValue(const GameState& s, int player, int depth, SearchStats& st) {
    ++st.nodes;
    // Terminal check is essential: in Ludo a finishing move leaves the
    // game at a TERMINAL CHANCE node, and enumerating dice outcomes on
    // a finished game would recurse forever (stack overflow).
    if (s.isTerminal()) {
        int w = s.winner();
        if (w == kDraw) return 0.0;
        return (w == player) ? 100000.0 + depth : -(100000.0 + depth);
    }
    if (depth <= 0) return s.heuristic(player);
    double total = 0.0, wsum = 0.0;
    for (const Move& m : s.getMoves()) {
        auto next = s.clone();
        next->applyMove(m);
        double v = (next->isChanceNode())
                       ? chanceValue(*next, player, depth - 1, st)
                       : (next->currentPlayer() == player
                              ? maxValue(*next, player, depth - 1, st)
                              : minValue(*next, player, depth - 1, st));
        total += m.weight * v;
        wsum += m.weight;
    }
    return total / wsum;
}

}  // namespace detail

inline Move expectimax(const GameState& root, int depth, SearchStats& st) {
    auto t0 = std::chrono::steady_clock::now();
    st = SearchStats{};
    st.depthReached = depth;

    int player = root.currentPlayer();
    Move best;
    double bestVal = -std::numeric_limits<double>::infinity();
    for (const Move& m : root.getMoves()) {
        auto next = root.clone();
        next->applyMove(m);
        double v = (next->isChanceNode())
                       ? detail::chanceValue(*next, player, depth - 1, st)
                       : (next->currentPlayer() == player
                              ? detail::maxValue(*next, player, depth - 1, st)
                              : detail::minValue(*next, player, depth - 1, st));
        if (v > bestVal) { bestVal = v; best = m; }
    }
    st.milliseconds = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0).count();
    return best;
}

}  // namespace game_engine
