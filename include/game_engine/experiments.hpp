#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "alphabeta.hpp"
#include "connect4.hpp"
#include "core.hpp"
#include "expectimax.hpp"
#include "ludo.hpp"
#include "metrics.hpp"
#include "minichess.hpp"
#include "minimax.hpp"
#include "mcts.hpp"

namespace game_engine {

struct ExperimentConfig {
    std::string game = "connect4";      // connect4 | minichess | ludo
    std::string algorithm = "alphabeta";// minimax | alphabeta | expectimax | mcts | random
    std::string algorithmP2;            // optional: different algo for player 2
    int depth = 4;                      // search depth (tree algorithms)
    int simulations = 500;              // MCTS rollouts per move
    unsigned seed = 42;
    int maxMoves = 0;                   // 0 => game-specific default
};

struct ExperimentResult {
    ExperimentConfig config;
    Metrics metrics;
    std::vector<DecisionStats> decisions;  // per-decision log
};

inline std::unique_ptr<GameState> makeGame(const std::string& name) {
    if (name == "connect4") return std::make_unique<Connect4State>();
    if (name == "minichess") return std::make_unique<MiniChessState>();
    if (name == "ludo") return std::make_unique<LudoState>();
    return nullptr;
}

inline int defaultMaxMoves(const std::string& game) {
    if (game == "connect4") return 60;
    if (game == "minichess") return 200;
    if (game == "ludo") return 400;
    return 300;
}

inline Move selectMove(const GameState& s, const std::string& algo, int depth,
                       int sims, unsigned seed, SearchStats& st) {
    if (algo == "minimax") return minimax(s, depth, st);
    if (algo == "alphabeta") return alphabeta(s, depth, st);
    if (algo == "expectimax") return expectimax(s, depth, st);
    if (algo == "mcts") return mcts(s, sims, seed, st);
    // "random" baseline
    auto moves = s.getMoves();
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
    st.milliseconds = 0.0;
    return moves[pick(rng)];
}

inline const std::vector<std::string>& knownGames() {
    static const std::vector<std::string> g = {"connect4", "minichess", "ludo"};
    return g;
}

inline const std::vector<std::string>& knownAlgorithms() {
    static const std::vector<std::string> a = {"minimax", "alphabeta",
                                               "expectimax", "mcts", "random"};
    return a;
}

// Play one full AI-vs-AI match and collect metrics.
inline ExperimentResult runExperiment(const ExperimentConfig& cfg) {
    auto state = makeGame(cfg.game);
    Metrics m;
    m.game = cfg.game;
    m.algorithm = cfg.algorithm + (cfg.algorithmP2.empty() ? "" : " vs " + cfg.algorithmP2);
    m.depth = cfg.depth;
    m.simulations = cfg.simulations;
    m.seed = cfg.seed;

    std::mt19937 rng(cfg.seed);
    int maxMoves = cfg.maxMoves > 0 ? cfg.maxMoves : defaultMaxMoves(cfg.game);
    int moves = 0;
    std::vector<DecisionStats> decisions;

    while (!state->isTerminal() && moves < maxMoves) {
        if (state->isChanceNode()) {
            // Resolve dice by real random sampling (not by an algorithm).
            auto outcomes = state->getMoves();
            double total = 0.0;
            for (auto& o : outcomes) total += o.weight;
            std::uniform_real_distribution<double> dist(0.0, total);
            double r = dist(rng), acc = 0.0;
            const Move* chosen = &outcomes.front();
            for (const auto& o : outcomes) {
                acc += o.weight;
                if (r <= acc) { chosen = &o; break; }
            }
            state->applyMove(*chosen);
        } else {
            SearchStats st;
            unsigned algoSeed = rng();
            const std::string& algo =
                (state->currentPlayer() == 1) ? cfg.algorithm
                                              : (cfg.algorithmP2.empty() ? cfg.algorithm : cfg.algorithmP2);
            Move mv = selectMove(*state, algo, cfg.depth, cfg.simulations, algoSeed, st);
            m.totalDecisionMs += st.milliseconds;
            m.totalNodes += st.nodes;
            m.totalPruned += st.pruned;
            m.totalSims += st.simulations;
            // Per-decision record: enables nodes/time-vs-depth plots.
            DecisionStats d;
            d.algorithm = algo;
            d.ply = moves;
            d.player = state->currentPlayer();
            d.nodes = st.nodes;
            d.pruned = st.pruned;
            d.sims = st.simulations;
            d.milliseconds = st.milliseconds;
            decisions.push_back(d);
            state->applyMove(mv);
        }
        ++moves;
    }

    m.movesPlayed = moves;
    m.result = state->isTerminal() ? state->winner() : kDraw;
    int nDecisions = std::max(1, m.movesPlayed);
    m.avgDecisionMs = m.totalDecisionMs / nDecisions;
    return {cfg, m, decisions};
}

inline std::vector<ExperimentResult> runExperimentSuite(
    const std::vector<ExperimentConfig>& configs, bool verbose = true) {
    std::vector<ExperimentResult> results;
    for (const auto& cfg : configs) {
        if (verbose)
            std::printf("[run] %-9s %-22s depth=%d sims=%d seed=%u ...\n",
                        cfg.game.c_str(),
                        (cfg.algorithm + (cfg.algorithmP2.empty() ? "" : " vs " + cfg.algorithmP2)).c_str(),
                        cfg.depth, cfg.simulations, cfg.seed);
        results.push_back(runExperiment(cfg));
    }
    return results;
}

inline std::string toCsv(const std::vector<ExperimentResult>& rs) {
    std::ostringstream os;
    os << "game,algorithm,depth,simulations,seed,moves,avg_decision_ms,"
          "total_nodes,total_pruned,total_sims,result_p1\n";
    for (const auto& r : rs) {
        os << r.metrics.game << ',' << r.metrics.algorithm << ','
           << r.metrics.depth << ',' << r.metrics.simulations << ','
           << r.metrics.seed << ',' << r.metrics.movesPlayed << ','
           << r.metrics.avgDecisionMs << ',' << r.metrics.totalNodes << ','
           << r.metrics.totalPruned << ',' << r.metrics.totalSims << ','
           << r.metrics.result << '\n';
    }
    return os.str();
}

// Per-decision CSV: one row per AI move, for plotting
// nodes / time vs depth per algorithm.
inline std::string toCsvDecisions(const std::vector<ExperimentResult>& rs) {
    std::ostringstream os;
    os << "game,algorithm,depth,simulations,seed,ply,player,"
          "nodes,pruned,sims,decision_ms\n";
    for (const auto& r : rs) {
        for (const auto& d : r.decisions) {
            os << r.metrics.game << ',' << d.algorithm << ','
               << r.metrics.depth << ',' << r.metrics.simulations << ','
               << r.metrics.seed << ',' << d.ply << ',' << d.player << ','
               << d.nodes << ',' << d.pruned << ',' << d.sims << ','
               << d.milliseconds << '\n';
        }
    }
    return os.str();
}

inline bool writeTextFile(const std::string& path, const std::string& content) {
    // Create parent directories (e.g. "results/") if missing.
    std::filesystem::path p(path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());
    std::ofstream f(path);
    if (!f) return false;
    f << content;
    return true;
}

}  // namespace game_engine
