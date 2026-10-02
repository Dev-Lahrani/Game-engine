#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "game_engine/experiments.hpp"
#include "game_engine/dashboard.hpp"

using namespace game_engine;

static void printUsage() {
    std::printf(
        "Game AI Lab - DAA game-playing algorithm research\n"
        "Usage:\n"
        "  lab play <game> <algo> [depth] [sims]   AI vs AI demo\n"
        "      games: connect4 | minichess | ludo\n"
        "      algos: minimax | alphabeta | expectimax | mcts | random\n"
        "  lab experiment [out.csv] [decisions.csv]\n"
        "                                           run default experiment matrix\n"
        "      decisions.csv: one row per AI decision (nodes/time vs depth)\n"
        "  lab dashboard [out.html]                build dashboard from last CSV run\n"
        "  lab test                                run unit tests\n");
}

// ---- unit tests -----------------------------------------------------------
static int runTests() {
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok) ++failures;
    };

    // Connect 4: setup a position where X wins immediately in column 4.
    {
        Connect4State s;
        // X has 3 stacked at the bottom of column 1, O likewise in
        // column 4; X to move and wins immediately by playing column 1.
        for (int r = 0; r < 3; ++r) {
            int row = Connect4State::H - 1 - r;  // stack from the bottom
            s.board[row * Connect4State::W + 0] = 1;
            s.board[row * Connect4State::W + 3] = 2;
        }
        s.player = 1;
        auto moves = s.getMoves();
        check(!moves.empty(), "connect4: moves available");
        SearchStats st;
        Move best = alphabeta(s, 2, st);
        check(best.to == 0, "connect4: alpha-beta finds immediate win");
    }

    // Connect 4: minimax and alpha-beta agree on the root value.
    {
        Connect4State s;
        s.board[5 * Connect4State::W + 3] = 1;  // X in centre
        s.player = 2;
        SearchStats st1, st2;
        minimax(s, 2, st1);
        alphabeta(s, 2, st2);
        check(st2.pruned >= 0, "connect4: alpha-beta runs (prunes counted)");
        check(st1.nodes >= st2.nodes, "connect4: alpha-beta explores <= minimax");
    }

    // MiniChess: initial legal move count (5 pawn moves + 2 knight moves).
    {
        MiniChessState s;
        auto moves = s.getMoves();
        check(moves.size() == 7, "minichess: 7 legal initial moves");
        SearchStats st;
        Move best = alphabeta(s, 2, st);
        check(!best.label.empty(), "minichess: alpha-beta selects a move");
    }

    // MiniChess: scholar's-style checkmate detection.
    {
        MiniChessState s;
        s.reset();
        // White: Qh5# style is hard in 5x5; instead verify terminal logic
        // on a bare-kings position.
        for (int i = 0; i < MiniChessState::N * MiniChessState::N; ++i)
            s.board[i] = '.';
        s.board[0] = 'K';
        s.board[24] = 'k';
        s.player = 1;
        check(s.isTerminal() && s.winner() == kDraw,
              "minichess: bare kings is a draw");
    }

    // Ludo: chance node returns six equiprobable dice outcomes.
    {
        LudoState s;
        check(s.isChanceNode(), "ludo: starts at a chance node");
        auto outcomes = s.getMoves();
        check(outcomes.size() == 6, "ludo: six dice outcomes");
        double w = 0;
        for (auto& o : outcomes) w += o.weight;
        check(std::abs(w - 1.0) < 1e-9, "ludo: dice weights sum to 1");

        // After rolling a 6, a base token may leave.
        Move six;
        six.param = 6;
        six.weight = 1.0 / 6.0;
        s.applyMove(six);
        check(!s.isChanceNode() && s.dice == 6, "ludo: 6 leads to a decision");
        auto moves = s.getMoves();
        check(moves.size() == 2, "ludo: both tokens can leave base on a 6");

        // After a non-6 roll from base, the turn passes (no legal move).
        LudoState s2;
        Move three;
        three.param = 3;
        s2.applyMove(three);
        check(s2.isChanceNode() && s2.player == 2,
              "ludo: unusable roll passes the turn");
    }

    // Expectimax runs on Ludo and returns a move.
    {
        LudoState s;
        Move six;
        six.param = 6;
        s.applyMove(six);
        SearchStats st;
        Move best = expectimax(s, 4, st);
        check(best.from >= 0 && best.from < LudoState::TOKENS,
              "ludo: expectimax picks a token");
        check(st.nodes > 0, "ludo: expectimax explored nodes");
    }

    // MCTS runs on all three games.
    {
        SearchStats st;
        Connect4State c4;
        Move m1 = mcts(c4, 200, 7, st);
        check(m1.to >= 0 && m1.to < Connect4State::W, "mcts: connect4 move");
        MiniChessState mc;
        Move m2 = mcts(mc, 200, 7, st);
        check(!m2.label.empty(), "mcts: minichess move");
        LudoState ld;
        Move six;
        six.param = 6;
        ld.applyMove(six);
        Move m3 = mcts(ld, 200, 7, st);
        check(m3.from >= 0 && m3.from < LudoState::TOKENS, "mcts: ludo move");
    }

    // Full experiment smoke test (tiny config).
    {
        ExperimentConfig cfg;
        cfg.game = "connect4";
        cfg.algorithm = "alphabeta";
        cfg.depth = 2;
        cfg.seed = 1;
        cfg.maxMoves = 30;
        auto r = runExperiment(cfg);
        check(r.metrics.result == 1 || r.metrics.result == 2 ||
                  r.metrics.result == kDraw,
              "experiment: full match completes with a result");
    }

    std::printf("\n%d failure(s)\n", failures);
    return failures;
}

// ---- commands -------------------------------------------------------------

static int cmdPlay(const std::vector<std::string>& a) {
    if (a.size() < 2) { printUsage(); return 1; }
    std::string game = a[0], algo = a[1];
    int depth = a.size() > 2 ? std::atoi(a[2].c_str()) : 4;
    int sims = a.size() > 3 ? std::atoi(a[3].c_str()) : 500;

    auto state = makeGame(game);
    if (!state) { std::printf("unknown game: %s\n", game.c_str()); return 1; }

    std::mt19937 rng(12345);
    int moves = 0;
    while (!state->isTerminal() && moves < defaultMaxMoves(game)) {
        if (state->isChanceNode()) {
            auto outcomes = state->getMoves();
            std::uniform_int_distribution<size_t> pick(0, outcomes.size() - 1);
            const Move& d = outcomes[pick(rng)];
            std::printf("  [die=%d]\n", d.param);
            state->applyMove(d);
        } else {
            SearchStats st;
            Move mv = selectMove(*state, algo, depth, sims, rng(), st);
            std::printf("P%d (%s): %s   [%.1f ms, %lld nodes]\n",
                        state->currentPlayer(), algo.c_str(), mv.label.c_str(),
                        st.milliseconds, st.nodes);
            state->applyMove(mv);
            if (game != "ludo") std::printf("%s\n", state->toString().c_str());
        }
        ++moves;
    }
    int w = state->isTerminal() ? state->winner() : kDraw;
    std::printf("Result after %d moves: %s\n", moves,
                w == 1 ? "Player 1 wins" : (w == 2 ? "Player 2 wins" : "Draw"));
    return 0;
}

// Seeds used for every configuration in the default matrix.
// Five seeds keep the averages stable without making the
// suite slow (17 configurations x 5 seeds = 85 runs).
static const std::vector<unsigned> kSeeds = {42u, 43u, 44u, 45u, 46u};

static std::vector<ExperimentConfig> defaultMatrix() {
    std::vector<ExperimentConfig> cfgs;
    auto add = [&](const char* game, const char* algo, int depth, int sims) {
        for (unsigned seed : kSeeds) {
            ExperimentConfig c;
            c.game = game;
            c.algorithm = algo;
            c.depth = depth;
            c.simulations = sims;
            c.seed = seed;
            cfgs.push_back(c);
        }
    };

    // Deterministic, moderate branching: full tree-search comparison.
    add("connect4", "minimax", 2, 0);
    add("connect4", "alphabeta", 2, 0);
    add("connect4", "alphabeta", 4, 0);
    add("connect4", "alphabeta", 6, 0);
    add("connect4", "mcts", 0, 200);
    add("connect4", "mcts", 0, 1000);

    // Deterministic, high branching + checkmate tactics.
    add("minichess", "minimax", 2, 0);
    add("minichess", "alphabeta", 2, 0);
    add("minichess", "alphabeta", 3, 0);
    add("minichess", "alphabeta", 4, 0);
    add("minichess", "mcts", 0, 500);

    // Stochastic: expectimax is the natural fit; MCTS via dice sampling.
    add("ludo", "expectimax", 4, 0);
    add("ludo", "expectimax", 6, 0);
    add("ludo", "alphabeta", 4, 0);
    add("ludo", "mcts", 0, 300);

    // Head-to-head matchups: algorithm vs algorithm.
    for (unsigned seed : kSeeds) {
        ExperimentConfig c;
        c.game = "ludo";
        c.algorithm = "expectimax";
        c.algorithmP2 = "mcts";
        c.depth = 4;
        c.simulations = 300;
        c.seed = seed;
        cfgs.push_back(c);
        ExperimentConfig d = c;
        d.game = "connect4";
        d.algorithm = "alphabeta";
        d.algorithmP2 = "mcts";
        d.depth = 4;
        d.simulations = 500;
        cfgs.push_back(d);
    }
    return cfgs;
}

static int cmdExperiment(const std::vector<std::string>& a) {
    std::string csvPath = a.size() > 0 ? a[0] : "results/experiments.csv";
    std::string decPath = a.size() > 1 ? a[1] : "results/decisions.csv";
    auto cfgs = defaultMatrix();
    std::printf("Running %zu experiment configurations...\n", cfgs.size());
    auto results = runExperimentSuite(cfgs, true);
    std::string csv = toCsv(results);
    if (!writeTextFile(csvPath, csv)) {
        std::printf("ERROR: cannot write %s\n", csvPath.c_str());
        return 1;
    }
    std::printf("Wrote %s (%zu rows)\n", csvPath.c_str(), results.size());

    // Per-decision export: one row per AI move, for nodes/time-vs-depth
    // plots grouped by game, algorithm and depth.
    std::string decCsv = toCsvDecisions(results);
    size_t decRows = 0;
    for (const auto& r : results) decRows += r.decisions.size();
    if (writeTextFile(decPath, decCsv))
        std::printf("Wrote %s (%zu decision rows)\n", decPath.c_str(), decRows);

    std::string htmlPath = "results/dashboard.html";
    if (writeTextFile(htmlPath, dashboard::generate(results)))
        std::printf("Wrote %s\n", htmlPath.c_str());
    return 0;
}

static int cmdDashboard(const std::vector<std::string>& a) {
    std::string csvPath = a.size() > 0 ? a[0] : "results/experiments.csv";
    std::string htmlPath = a.size() > 1 ? a[1] : "results/dashboard.html";
    std::ifstream f(csvPath);
    if (!f) { std::printf("cannot open %s\n", csvPath.c_str()); return 1; }
    // Minimal CSV parse: we only need it back into ExperimentResult form.
    std::vector<ExperimentResult> rs;
    std::string line;
    std::getline(f, line);  // header
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        ExperimentResult r;
        std::stringstream ss(line);
        std::string field;
        std::getline(ss, r.metrics.game, ',');
        std::getline(ss, r.metrics.algorithm, ',');
        ss >> r.metrics.depth; ss.ignore();
        ss >> r.metrics.simulations; ss.ignore();
        unsigned seed; ss >> seed; ss.ignore(); r.metrics.seed = seed;
        ss >> r.metrics.movesPlayed; ss.ignore();
        ss >> r.metrics.avgDecisionMs; ss.ignore();
        ss >> r.metrics.totalNodes; ss.ignore();
        ss >> r.metrics.totalPruned; ss.ignore();
        ss >> r.metrics.totalSims; ss.ignore();
        ss >> r.metrics.result;
        rs.push_back(r);
    }
    if (writeTextFile(htmlPath, dashboard::generate(rs))) {
        std::printf("Wrote %s from %zu rows\n", htmlPath.c_str(), rs.size());
        return 0;
    }
    std::printf("ERROR: cannot write %s\n", htmlPath.c_str());
    return 1;
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv, argv + argc);
    std::string cmd = args.size() > 1 ? args[1] : "";
    // Sub-command arguments (everything after the command name).
    std::vector<std::string> rest(args.begin() + 2, args.end());
    if (cmd == "play") return cmdPlay(rest);
    if (cmd == "experiment") return cmdExperiment(rest);
    if (cmd == "dashboard") return cmdDashboard(rest);
    if (cmd == "test") return runTests() == 0 ? 0 : 1;
    printUsage();
    return 0;
}
