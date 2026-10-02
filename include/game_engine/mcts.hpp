#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <random>

#include "core.hpp"
#include "metrics.hpp"

namespace game_engine {

// Monte Carlo Tree Search (UCT). Handles stochastic games by SAMPLING dice
// outcomes during selection/rollout (luck sampling), so one tree covers the
// chance layer without enumerating it.
class MCTSNode {
public:
    std::unique_ptr<GameState> state;
    MCTSNode* parent = nullptr;
    Move move;
    int playerJustMoved = 0;
    std::vector<std::unique_ptr<MCTSNode>> children;
    std::vector<Move> untried;
    double wins = 0.0;   // from playerJustMoved's perspective
    int visits = 0;
};

inline Move mcts(const GameState& root, int iterations, unsigned seed,
                 SearchStats& st, double exploration = 1.41421356,
                 int maxRollout = 400) {
    auto t0 = std::chrono::steady_clock::now();
    st = SearchStats{};
    st.depthReached = 0;

    std::mt19937 rng(seed);
    auto rootNode = std::make_unique<MCTSNode>();
    rootNode->state = root.clone();
    rootNode->untried = root.getMoves();

    auto terminalReward = [&](const GameState& s, int player) {
        int w = s.winner();
        if (w == kDraw) return 0.5;
        return (w == player) ? 1.0 : 0.0;
    };

    for (int it = 0; it < iterations; ++it) {
        MCTSNode* node = rootNode.get();

        // ---- Selection: descend using UCT; sample dice at chance nodes.
        while (!node->state->isTerminal()) {
            if (!node->untried.empty()) break;  // expand below
            if (node->children.empty()) break;
            if (node->state->isChanceNode()) {
                std::uniform_int_distribution<size_t> pick(0, node->children.size() - 1);
                node = node->children[pick(rng)].get();
            } else {
                MCTSNode* best = nullptr;
                double bestUcb = -1e300;
                for (auto& c : node->children) {
                    double ucb = c->wins / c->visits +
                                 exploration *
                                     std::sqrt(std::log((double)node->visits) /
                                               c->visits);
                    if (ucb > bestUcb) { bestUcb = ucb; best = c.get(); }
                }
                node = best;
            }
        }

        // ---- Expansion: add one child (a move or a dice outcome).
        if (!node->state->isTerminal() && !node->untried.empty()) {
            std::uniform_int_distribution<size_t> pick(0, node->untried.size() - 1);
            size_t idx = pick(rng);
            const Move& m = node->untried[idx];
            node->untried.erase(node->untried.begin() + idx);
            auto child = std::make_unique<MCTSNode>();
            child->state = node->state->clone();
            child->state->applyMove(m);
            child->parent = node;
            child->move = m;
            // A dice outcome does not change who moved last; a decision
            // node's move does.
            child->playerJustMoved = node->state->isChanceNode()
                                         ? node->playerJustMoved
                                         : node->state->currentPlayer();
            if (!child->state->isTerminal())
                child->untried = child->state->getMoves();
            node = node->children.emplace_back(std::move(child)).get();
            ++st.nodes;
        }

        // ---- Simulation: random rollout (dice sampled uniformly).
        {
            auto sim = node->state->clone();
            int rolls = 0;
            while (!sim->isTerminal() && rolls < maxRollout) {
                auto moves = sim->getMoves();
                if (moves.empty()) break;
                std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
                sim->applyMove(moves[pick(rng)]);
                ++rolls;
            }
            double reward;
            if (sim->isTerminal()) {
                reward = terminalReward(*sim, node->playerJustMoved);
            } else {
                // rollout cap: fall back to the heuristic as a proxy
                double h = node->playerJustMoved > 0
                               ? sim->heuristic(node->playerJustMoved)
                               : sim->heuristic(sim->currentPlayer());
                reward = 0.5 + 0.5 * std::tanh(h / 200.0);
            }
            // ---- Backpropagation. Each node stores statistics from the
            // perspective of its playerJustMoved; the reward flips only
            // across decision edges where the mover alternates (chance
            // nodes preserve playerJustMoved).
            for (MCTSNode* n = node; n != nullptr; n = n->parent) {
                ++n->visits;
                n->wins += reward;
                if (n->parent && n->playerJustMoved != 0 &&
                    n->parent->playerJustMoved != 0 &&
                    n->parent->playerJustMoved != n->playerJustMoved) {
                    reward = 1.0 - reward;
                }
            }
            ++st.simulations;
        }
    }

    // ---- Choose the most-visited (robust) child of the root.
    MCTSNode* best = nullptr;
    int bestVisits = -1;
    for (auto& c : rootNode->children) {
        if (c->visits > bestVisits) { bestVisits = c->visits; best = c.get(); }
    }
    st.milliseconds = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0).count();
    return best ? best->move : Move{};
}

}  // namespace game_engine
