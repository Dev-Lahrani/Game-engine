# Game AI Lab — DAA Research Experiment

A C++17 research harness for a **Design & Analysis of Algorithms (DAA)**
course experiment. It measures how **game characteristics** (deterministic
vs stochastic, branching factor, game-tree depth, game length) affect the
**computational performance and suitability** of four game-playing
algorithms: **Minimax, Alpha-Beta, Expectimax and MCTS (UCT)**.

> Research question: how do game characteristics affect the computational
> performance of DAA-based game-playing algorithms?
> We are **not** researching which game is "better". No shortest-path
> algorithms (Dijkstra/A*) are involved anywhere in this project.

Verified end-to-end on Linux (GCC 16.2.1, CMake 3.10+, no external
dependencies) — see [Session log](#session-log-clone--build--run) for the
exact commands and outputs reproduced on 2026-10-15.

---

## Table of contents

1. [Research workflow (12 steps)](#research-workflow-12-steps)
2. [Games](#games)
3. [State model (DECISION / CHANCE / TERMINAL)](#state-model-decision--chance--terminal)
4. [Algorithms](#algorithms)
5. [Build](#build)
6. [Usage (CLI reference)](#usage-cli-reference)
7. [Metrics collected](#metrics-collected)
8. [Experiment matrix](#experiment-matrix-17-configurations--5-seeds--85-runs)
9. [Results (default run, verified 2026-10-15)](#results-default-run-verified-2026-10-15)
10. [Output files](#output-files)
11. [Project structure](#project-structure)
12. [Implementation notes (bugs found)](#implementation-notes-bugs-found-while-building)
13. [Analysis — how characteristics change the algorithms](#analysis--how-characteristics-change-the-algorithms)
14. [Session log (clone → build → run)](#session-log-clone--build--run)
15. [Team contributions (3 members)](#team-contributions-3-members)
16. [Reproducibility checklist](#reproducibility-checklist)
17. [Conclusion template for the report](#conclusion-template-for-the-report)

---

## Research workflow (12 steps)

```
Game Characteristics → Algorithm Selection → Game-Tree Search → Decision →
Performance Measurement → Experimentation → Comparison → Analysis
```

| # | Step | Where in code |
|---|------|---------------|
| 1 | Select a game (Connect 4 / MiniChess / Ludo) | `makeGame()` in `experiments.hpp`; `lab play <game>` |
| 2 | Create the current game state (board, pieces, turn, dice phase) | `Connect4State`, `MiniChessState`, `LudoState` (`reset()`) |
| 3 | Generate possible moves | `GameState::getMoves()` — legal moves at DECISION nodes, weighted dice outcomes at CHANCE nodes |
| 4 | Apply the suitable algorithm | `selectMove()` dispatches to `minimax` / `alphabeta` / `expectimax` / `mcts` |
| 5 | Algorithm searches possible future states | `minimaxValue` / `abValue` / `maxValue`+`minValue`+`chanceValue` / MCTS selection–expansion–simulation–backpropagation |
| 6 | AI selects the best action | root move with best value (tree searches) or most-visited child (MCTS) |
| 7 | Game state changes, process repeats | `applyMove()`; AI-vs-AI loop in `runExperiment()` until terminal |
| 8 | Collect real metrics | `SearchStats` per move (nodes, pruned, sims, ms) → `Metrics`, plus `DecisionStats` per AI move |
| 9 | Run experiments with different depths/configs | `defaultMatrix()` — 17 configurations × seeds 42–46 (85 runs) |
| 10 | Compare algorithms across the three games | `toCsv()` → `results/experiments.csv`; `toCsvDecisions()` → `results/decisions.csv` |
| 11 | Display results through graphs/dashboard | `dashboard::generate()` → self-contained `results/dashboard.html` (inline SVG, no external assets) |
| 12 | Analyze behaviour under different game characteristics | See [Analysis](#analysis--how-characteristics-change-the-algorithms) |

Full mapping + analysis notes live in `docs/PLAN.md`.

---

## Games

### Implemented now (in this repo)

| Game | Type | Branching | Length | Win condition / rules |
|------|------|-----------|--------|-----------------------|
| **Connect 4** | Deterministic | ~7 | ~30–45 plies | 7×6 board, first to connect 4 wins, full board = draw |
| **MiniChess** | Deterministic | ~7–15 | ~30–45 plies | 5×5 Gardner minichess (no castling / en passant / double step, promote to queen), 200-ply cap, stalemate = draw, bare kings = draw |
| **Ludo** | Stochastic | 2 tokens × 6 dice | ~60–90 plies | 24-cell shared ring (entry cells safe), 4-cell home column, exact roll to finish, leave base only on 6, roll 6 grants an extra turn, capture on non-safe cells |

Game-characteristics matrix (implemented now):

| Characteristic | Connect 4 | MiniChess | Ludo |
|---|---|---|---|
| Deterministic? | Yes | Yes | **No** (dice) |
| Chance nodes | none | none | every other ply |
| Initial branching | 7 | 7 (5 pawn + 2 knight) | 2 (after a 6) × 6 dice |
| Terminal test | 4-in-a-row / full board | checkmate / stalemate / 200-ply cap / bare kings | all tokens home |

---

### Future games (extend the harness)

All of these reuse the existing `GameState` interface — add `<game>State::reset()`, `getMoves()`, `applyMove()`, `toString()`, `isTerminal()`/`winner()`, then a heuristic in its `makeGame()` branch and one or two lines in `selectMove()` and `defaultMatrix()`. Keep experiments short via `maxMoves` and an easy heuristic, or the tree-search rows will time out.

| Game | Type | Branching / state size | Why add it | Implementation catch |
|------|------|------------------------|------------|----------------------|
| **Tic-Tac-Toe** | Deterministic | 3×3, trivial full enumeration | Smallest full tree — good first validation of harness, can reach terminal quickly, clear draw/forced-win structure | 9 moves only; depth needs capping to avoid no-ops |
| **Checkers (English Draughts, 8×8)** | Deterministic | bigger tree, captures + king promotion + forced captures | Classic search-testbed game; lots of forced-move structure, direct comparison vs Connect 4 | capture loops + forced capture rule; king promotion on reaching far row; draw by repetition or stale-position rules |
| **Full Chess (standard, 8×8)** | Deterministic | extremely large; branching grows fast | Ultimate tree-search benchmark and the textbook case where pure Minimax/Alpha-Beta + hand heuristic is the old-school engine approach (pre-AlphaZero) | castling, en passant, double-step, promotion to many pieces, 50-move and repetition draws; 200-ply cap clearly not enough — need bigger depth cap + good terminal/repetition tracking |
| **Nim (multiple heaps)** | Deterministic | small state, solved by nim-sum XOR (Grundy) | Adds a **non-search theorem** contrast: optimal move is a math property, not tree depth — shows where Alpha-Beta/MCTS are overkill | trivial rules, but interesting vs your tree-search baseline: do searches find nim-sum-optimal move? |
| **Mancala / Kalah (small variant)** | Deterministic + capture + extra-turn mechanics | small–medium branching | Adds "move order and extra-turn" dynamics beyond Connect 4 | last seed in empty own bowl captures, last seed in store = extra turn |
| **Othello / Reversi (small board, e.g. 6×6 or 8×8)** | Deterministic | branching grows with board; evaluation-driven | Good next step when heuristics matter more than depth — Alpha-Beta rewards decent evaluation | flipping chains, corner/edge evaluation, pass rules |
| **Backgammon (lite / without doubling cube)** | Stochastic + chance + race | chance nodes (dice) like Ludo, but bigger state + bear-off | Extends the **stochastic** side beyond Ludo; another Expectimax/MCTS-vs-dice case | roll-6-again is rarer; hit/entry rules; bear-off rules; simulation can be long |
| **Bar games: Nine Men's Morris, Mancala other variants, Connect-5/6** | Deterministic or simple | mostly small | Easy playgrounds for extending the same codebase | keep board small or cap depth |
| **Single-player puzzles (8-puzzle / 15-puzzle) — restricted** | Single-player, not adversarial | state space large | Only if you explicitly want a **single-agent search** contrast; README says no Dijkstra/A* currently | would need A*/IDA* or MCTS planner, not two-player minimax — adds a new theme |

Recommendation for a next cycle: **Tic-Tac-Toe** (smallest, sanity-check harness), then **Checkers or Othello** (real larger deterministic game) and/or **Nim** (theory contrast). Keep full Chess as a stretch goal, not a first extension.

---

Game-characteristics matrix (all listed games):

| Characteristic | Connect 4 | MiniChess | Ludo | Tic-Tac-Toe | Checkers | Full Chess | Nim | Othello | Backgammon |
|---|---|---|---|---|---|---|---|---|---|
| Deterministic? | Yes | Yes | **No** (dice) | Yes | Yes | Yes | Yes | Yes | **No** (dice) |
| Chance nodes | none | none | every other ply | none | none | none | none | none | every other ply |
| Typical branching | ~7 | ~7–15 | 2 × 6 dice | ≤9 | medium (captures) | very large | heap-dependent | medium | 2 × 6 dice |
| State space | moderate | moderate (5×5) | moderate (24+home) | tiny | large | huge | very small | medium–large | large |
| Known status | solved (7×6) | small research game | small stochastic sim | solved (trivial) | deep, not fully solved | huge, solved at GM+ level with search + eval | solved by math (nim-sum) | strong solved variants exist | large, stochastic |
| Fits harness model? | yes (now) | yes (now) | yes (now) | yes (easy) | yes (medium) | yes (heavy) | yes (easy) | yes (medium) | yes (stochastic) |


| Characteristic | Connect 4 | MiniChess | Ludo |
|---|---|---|---|
| Deterministic? | Yes | Yes | **No** (dice) |
| Chance nodes | none | none | every other ply |
| Initial branching | 7 | 7 (5 pawn + 2 knight) | 2 (after a 6) × 6 dice |
| Terminal test | 4-in-a-row / full board | checkmate / stalemate / 200-ply cap / bare kings | all tokens home |

---

## State model (DECISION / CHANCE / TERMINAL)

All games implement the `GameState` abstraction (`include/game_engine/core.hpp`):

- **DECISION** — a player acts (`getMoves()` returns legal moves).
- **CHANCE** — dice outcomes with weights summing to 1 (Ludo only; 6 outcomes, weight 1/6 each).
- **TERMINAL** — game over (`winner()` returns 1 / 2 / draw).

Ludo chance-phase transition: a usable roll leads to a DECISION node;
an unusable roll passes the turn and the state stays a CHANCE node.
A finishing move can leave the game at a *terminal CHANCE* node — this
must be checked before enumerating dice (see Implementation notes).

---

## Algorithms

- **Minimax** (`minimax.hpp`, 72 lines) — full game-tree search baseline;
  chance nodes evaluated as expectations so it also runs on Ludo.
- **Alpha-Beta** (`alphabeta.hpp`, 87 lines) — minimax with alpha-beta
  cutoffs; every cutoff increments `pruned`. Same optimal move as minimax
  on deterministic games, fewer nodes at depth.
- **Expectimax** (`expectimax.hpp`, 116 lines) — the natural fit for
  stochastic games: max/min layers at decision nodes, expectation layers
  at chance nodes (weighted average over dice).
- **MCTS (UCT)** (`mcts.hpp`, 142 lines) — samples dice outcomes during
  selection/rollout instead of enumerating them; exploration constant
  C = √2, root child chosen by **most visits**.

All tree searches carry an explicit player perspective. Negamax sign-flip
is **invalid** for Ludo because a roll of 6 keeps the turn with the same
player.

Algorithm selection rationale:

- **Connect 4 / MiniChess (deterministic)** → Minimax is the baseline;
  Alpha-Beta is the practical choice (same move, fewer nodes).
  Expectimax is wasteful (no chance nodes to average).
- **Ludo (stochastic)** → Expectimax is correct; Alpha-Beta still runs
  (chance as expectation, "alpha-beta over expectimax") but pruning at
  chance nodes is not guaranteed to preserve value. MCTS needs no
  explicit chance layer — it samples dice.
- **MCTS everywhere** as the comparison point; trades depth for rollouts.

---

## Build

Requires a C++17 compiler (GCC/Clang) and CMake ≥ 3.10. **No dependencies.**

```bash
# clone via SSH
git clone git@github.com:Dev-Lahrani/Game-engine.git
cd Game-engine

# configure + build (Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# binary
./build/lab
```

`CMakeLists.txt`: `project(game-ai-lab CXX)`, `CMAKE_CXX_STANDARD 17`,
`add_executable(lab src/main.cpp)`, `target_include_directories(lab PRIVATE include)`,
CTest entry `lab_unit_tests` → `lab test`.

---

## Usage (CLI reference)

```bash
./build/lab test                                 # 19 unit tests
./build/lab play <game> <algo> [depth] [sims]    # AI vs AI demo
./build/lab experiment [out.csv] [decisions.csv] # run the experiment matrix
./build/lab dashboard [in.csv] [out.html]        # rebuild the dashboard
```

- `game`: `connect4 | minichess | ludo`
- `algo`: `minimax | alphabeta | expectimax | mcts | random`
- `depth`: tree-search depth (default 4); `sims`: MCTS rollouts (default 500)

Examples (all verified):

```bash
./build/lab play connect4 alphabeta 4
./build/lab play connect4 alphabeta 2
./build/lab play minichess minimax 3
./build/lab play ludo expectimax 4
./build/lab experiment            # writes results/experiments.csv + results/decisions.csv + results/dashboard.html
./build/lab dashboard             # rebuilds results/dashboard.html from results/experiments.csv
```

Demo output shape (`play`):

```
P1 (alphabeta): col 5   [0.0 ms, 1 nodes]
...
Result after 39 moves: Player 1 wins
```

Ludo interleaves dice lines (`[die=5]`) with token moves
(`token 2   [0.0 ms, 49 nodes]`).

Test output shape (`test`): 19 × `[PASS] ...` then `0 failure(s)`.
The 19 tests cover: Connect 4 moves/immediate-win/prune-count/agreement,
MiniChess 7 initial moves/move-selection/bare-kings-draw, Ludo
chance-node/6-outcomes/weights/6-leads-to-decision/2-tokens/unusable-roll-passes/
expectimax-picks-token/nodes-explored, MCTS on all 3 games, full-match
smoke test.

---

## Metrics collected

Per AI decision (`SearchStats` → `DecisionStats`):

- **Decision time** (ms per move, wall clock)
- **Nodes explored** (game-tree nodes evaluated)
- **Nodes pruned** (alpha-beta cutoffs; 0 for other algorithms)
- **MCTS simulations** (rollouts; 0 for tree searches)

Per match (`Metrics`), aggregated over the whole game:

- moves played, avg decision ms, total nodes, total pruned, total sims
- **Win / Draw / Loss** record per configuration (`result_p1`: 1 / 2 / 0=draw)

CSV schemas:

`results/experiments.csv` (one row per match):
```
game,algorithm,depth,simulations,seed,moves,avg_decision_ms,total_nodes,total_pruned,total_sims,result_p1
```

`results/decisions.csv` (one row per AI decision — for nodes/time-vs-depth plots):
```
game,algorithm,depth,simulations,seed,ply,player,nodes,pruned,sims,decision_ms
```

---

## Experiment matrix (17 configurations × 5 seeds = 85 runs)

Seeds `42, 43, 44, 45, 46` for every configuration (stable averages,
still fast). Defined in `defaultMatrix()` in `src/main.cpp`.

- **Connect 4**: minimax d2; alpha-beta d2/d4/d6; MCTS 200/1000 sims
- **MiniChess**: minimax d2; alpha-beta d2/d3/d4; MCTS 500 sims
- **Ludo**: expectimax d4/d6; alpha-beta d4; MCTS 300 sims
- **Head-to-head**: expectimax vs MCTS (Ludo, d4/sims300),
  alpha-beta vs MCTS (Connect 4, d4/sims500)
- Max moves per game via `defaultMaxMoves(game)`; AI-vs-AI until terminal.

---

## Results (default run, verified 2026-10-15)

85 rows in `results/experiments.csv`, 2623 decision rows in
`results/decisions.csv`. Averages over 5 seeds each
(`result_p1` string lists seeds 42→46; `1`=P1 win, `2`=P2 win, `0`=draw):

| game | algorithm | depth | sims | avg ms/move | avg nodes | pruned | sims | results |
|------|-----------|-------|------|-------------|-----------|--------|------|---------|
| connect4 | minimax | 2 | 0 | 0.0347 | 1607 | 0 | 0 | 22222 |
| connect4 | alphabeta | 2 | 0 | 0.0311 | 1607 | 0 | 0 | 22222 |
| connect4 | alphabeta | 4 | 0 | 0.3685 | 21197 | 2473 | 0 | 11111 |
| connect4 | alphabeta | 6 | 0 | 5.1651 | 376349 | 59847 | 0 | 00000 |
| connect4 | mcts | 0 | 200 | 1.9898 | 3027 | 0 | 3200 | 21221 |
| connect4 | mcts | 0 | 1000 | 8.9655 | 17255 | 0 | 19400 | 21121 |
| connect4 | alphabeta vs mcts | 4 | 500 | 2.5263 | 15068 | 1110 | 3800 | 11111 |
| minichess | minimax | 2 | 0 | 0.4598 | 3234 | 0 | 0 | 11111 |
| minichess | alphabeta | 2 | 0 | 0.4412 | 3234 | 0 | 0 | 11111 |
| minichess | alphabeta | 3 | 0 | 1.8673 | 22681 | 2912 | 0 | 00000 |
| minichess | alphabeta | 4 | 0 | 17.0286 | 163176 | 14850 | 0 | 11111 |
| minichess | mcts | 0 | 500 | 209.1940 | 10689 | 0 | 11300 | 12121 |
| ludo | expectimax | 4 | 0 | 0.0031 | 2330 | 0 | 0 | 12121 |
| ludo | expectimax | 6 | 0 | 0.0375 | 30469 | 0 | 0 | 12221 |
| ludo | alphabeta | 4 | 0 | 0.0028 | 2623 | 0 | 0 | 12121 |
| ludo | mcts | 0 | 300 | 1.2791 | 8337 | 0 | 8760 | 12221 |
| ludo | expectimax vs mcts | 4 | 300 | 0.6971 | 6355 | 0 | 5340 | 12122 |

Head-to-head rows report player-1 search stats only.

Selected observations:

- Alpha-Beta at d2 on Connect 4 = Minimax (1,607 nodes, 0 prunes);
  at d6 = 376k nodes + ~60k prunes — **pruning savings grow with depth**.
- MiniChess d4 = 163k nodes / 14.8k pruned; MCTS-500 costs ~209 ms/move
  (here) because rollouts run up to 400 plies — **MCTS time follows game
  length, not tree size**.
- Ludo Expectimax d4 ≈ 2.3k nodes, d6 ≈ 30k nodes — tiny effective
  branching (≤2 moves × 6 dice); stochastic games need expectation
  layers, not bigger trees.
- `result_p1` flips with depth/seed (e.g. Connect 4 AB d4 = 11111 but
  d6 = 00000) — deeper ≠ always P1-favoured in symmetric AI-vs-AI.

---

## Output files

- `results/experiments.csv` — 86 lines (header + 85 matches); per-match
  summary described above.
- `results/decisions.csv` — 2624 lines (header + 2623 decisions);
  per-decision nodes/time for vs-depth curves.
- `results/dashboard.html` — ~40K self-contained HTML (inline SVG bar
  charts, no external assets): per-game avg time / nodes / pruned /
  simulations charts, W/D/L tables, raw data table. Rebuild any time via
  `./build/lab dashboard [in.csv] [out.html]`.

---

## Project structure

```
CMakeLists.txt                    # C++17, lab target, CTest entry
src/main.cpp                      # 336 lines: CLI, 19 unit tests, experiment matrix (17×5)
include/game_engine/
  core.hpp        (58)  GameState / Move abstraction (decision/chance/terminal)
  metrics.hpp     (47)  SearchStats + aggregated Metrics
  connect4.hpp    (127) Connect 4 rules + heuristic
  minichess.hpp   (256) 5×5 minichess rules + heuristic
  ludo.hpp        (177) Ludo-lite rules (stochastic, chance nodes)
  minimax.hpp     (72)  Minimax (chance as expectation)
  alphabeta.hpp   (87)  Alpha-Beta (counts prunes)
  expectimax.hpp  (116) Expectimax (max/min + chance expectations)
  mcts.hpp        (142) MCTS-UCT (C=√2, most-visited root child)
  experiments.hpp (200) AI-vs-AI match runner, CSV export, makeGame/selectMove
  dashboard.hpp   (217) HTML/SVG dashboard generator
docs/PLAN.md      (104) 12-step workflow mapping + analysis notes
README.md                       this file
results/
  experiments.csv               85-row match summary (verified 2026-10-15)
  decisions.csv                 2623-row per-decision log
  dashboard.html                self-contained charts + tables
build/                          CMake build dir (gitignored) → build/lab binary
```

Total: ~1835 lines of headers + `main.cpp` (+ README/PLAN/results).

---

## Implementation notes (bugs found while building)

1. **Expectimax must check terminal at chance nodes.** A finishing Ludo
   move leaves a *terminal CHANCE* node (`chancePhase == true`).
   Enumerating dice on a finished game recurses forever → stack overflow.
   Fix: terminal check (and depth floor) at the top of `chanceValue()`.
2. **Negamax sign-flip is invalid for Ludo** (roll-6-again keeps the turn),
   so all searches carry an explicit `player` perspective.
3. **Chance-phase transition**: usable roll → DECISION node; unusable roll
   → pass turn, stay CHANCE (`chancePhase = !any`).
4. Dice weights at a chance node sum to 1 (unit-tested with 1e-9 tolerance).

---

## Analysis — how characteristics change the algorithms

### Deterministic, moderate branching (Connect 4)
Node count grows ~exponentially: d2 = 1.6k, d4 = 21k, d6 = 376k per match.
Alpha-Beta = Minimax at d2 (0 prunes — no cutoff opportunities in a
shallow tree) but prunes ~60k at d6. Time tracks nodes (0.03 → 5.2 ms).

### Deterministic, tactics + higher branching (MiniChess)
Same depth curve, steeper: d4 = 163k nodes / 14.8k pruned / 17 ms per move.
MCTS explores only ~10.7k nodes but costs ~209 ms/move here — dominated by
**rollout length** (up to 400 plies), not tree size.

### Stochastic (Ludo)
Effective branching tiny → Expectimax cheap (d4 ≈ 2.3k nodes, d6 ≈ 30k).
Expectimax and Alpha-Beta both finish in microseconds, but only
Expectimax's expectation layers are *correct* for dice; Alpha-Beta pruning
at chance nodes is not value-preserving. MCTS-300 (≈8.3k nodes, ~1.3 ms)
needs no explicit chance layer — it samples dice.

### Head-to-head
`expectimax vs mcts` (Ludo) and `alphabeta vs mcts` (Connect 4) rows show
W/D/L when two algorithms play each other; metrics columns report P1 only.

---

## Session log (clone → build → run)

All executed 2026-10-15 (exit 0 each; SSH as `Dev-Lahrani`):

```bash
git clone git@github.com:Dev-Lahrani/Game-engine.git dev/Game-engine
# Cloning into 'dev/Game-engine'... done.

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# -- The CXX compiler identification is GNU 16.2.1 ... Build files written

cmake --build build -j$(nproc)
# [ 50%] Building CXX object ... [100%] Built target lab (166K build/lab)

./build/lab test
# [PASS] connect4: moves available ... [PASS] experiment: full match completes
# 0 failure(s)   (19/19)

./build/lab play connect4 alphabeta 4
# Result after 39 moves: Player 1 wins

./build/lab play ludo expectimax 4
# Result after 70 moves: Player 1 wins

./build/lab experiment
# Wrote results/experiments.csv (85 rows)
# Wrote results/decisions.csv (2623 decision rows)
# Wrote results/dashboard.html

./build/lab dashboard
# Wrote results/dashboard.html from 85 rows
```

---

## Team contributions (3 members)

Single-commit history (`d7f5c94`) — functional split for report/viva:

| Member | Owns | Files |
|--------|------|-------|
| Person 1 — Game Modeling | state abstraction, metrics, deterministic games + their tests | `core.hpp`, `metrics.hpp`, `connect4.hpp`, `minichess.hpp`, tests 1–7 |
| Person 2 — Algorithms + stochastic game | 4 search algorithms, Ludo engine + its tests | `minimax.hpp`, `alphabeta.hpp`, `expectimax.hpp`, `mcts.hpp`, `ludo.hpp`, tests 8–18 |
| Person 3 — Experimentation & visualization | runner, matrix, CLI, dashboard, docs, analysis | `experiments.hpp`, `dashboard.hpp`, `src/main.cpp`, `CMakeLists.txt`, `README.md`, `docs/PLAN.md`, `results/*` |

Replace Person 1/2/3 with names + roll numbers for submission.

---

## Reproducibility checklist

- [x] SSH clone works (`Hi Dev-Lahrani! ... authenticated`)
- [x] CMake configure + build, no dependencies
- [x] 19/19 unit tests pass
- [x] Demo games complete with a result (Connect 4, Ludo verified)
- [x] `experiment` writes 85-row CSV + 2623-row decisions + HTML
- [x] `dashboard` rebuilds from CSV
- [x] Results table above matches `results/experiments.csv` in repo

---

## Conclusion template for the report

> Game characteristics determine which algorithm is suitable:
> deterministic games with high branching reward Alpha-Beta's pruning
> (savings grow with depth); stochastic games require expectation layers
> (Expectimax) or chance sampling (MCTS); MCTS's cost is governed by
> rollout length rather than branching factor, making it attractive when
> the state space is complex but games are short. The experiment isolates
> these effects by measuring time, nodes, prunes and simulations per
> decision across a fixed matrix of games, depths and seeds.
```

