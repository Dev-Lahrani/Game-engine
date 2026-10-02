# Game AI Lab — DAA Research Experiment

A C++ research harness for a Design & Analysis of Algorithms (DAA) course
experiment. It measures how **game characteristics** (deterministic vs
stochastic, branching factor, game-tree depth) affect the **computational
performance and suitability** of four game-playing algorithms:
**Minimax, Alpha-Beta, Expectimax and MCTS**.

> Research question: how do game characteristics affect the computational
> performance of DAA-based game-playing algorithms?
> We are **not** researching which game is "better". No shortest-path
> algorithms (Dijkstra/A*) are involved anywhere in this project.

## Research workflow

```
Game Characteristics → Algorithm Selection → Game-Tree Search → Decision →
Performance Measurement → Experimentation → Comparison → Analysis
```

1. Select a game (Connect 4 / MiniChess / Ludo)
2. Create the current game state (board, pieces, turn, dice phase)
3. Generate possible moves
4. Apply the suitable algorithm
5. The algorithm searches possible future states
6. The AI selects the best action
7. The game state changes and the process repeats
8. Real metrics are collected during execution (time, nodes, pruning, simulations)
9. Experiments run many configurations (depths / simulation budgets / seeds)
10. Algorithms are compared across the three games
11. Results are displayed as graphs in a self-contained dashboard
12. Analysis: which algorithm behaves differently under which game characteristics

## Games

| Game | Type | Branching | Notes |
|------|------|-----------|-------|
| **Connect 4** | Deterministic | ~7 | 7×6 board, first to connect 4 wins |
| **MiniChess** | Deterministic | ~7–15 | 5×5 Gardner minichess (no castling / en passant / double step, promote to queen), 200-ply cap, stalemate = draw |
| **Ludo** | Stochastic | 2 tokens × 6 dice | 24-cell shared ring (entry cells safe), 4-cell home column, exact roll to finish, leave base only on 6, roll 6 grants an extra turn, capture on non-safe cells |

States are modelled as three node types: **DECISION** (a player acts),
**CHANCE** (dice outcomes with weights summing to 1) and **TERMINAL**.

## Algorithms

- **Minimax** — full game-tree search; chance nodes evaluated as expectations.
- **Alpha-Beta** — minimax with alpha-beta cutoffs; pruning is counted.
- **Expectimax** — the natural fit for stochastic games: max/min layers at
  decision nodes, expectation layers at chance nodes.
- **MCTS (UCT)** — samples dice outcomes during selection/rollout instead of
  enumerating them; C = √2, root child chosen by most visits.

All tree searches use an explicit player perspective (negamax sign-flip is
invalid for Ludo because a roll of 6 keeps the turn with the same player).

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Requires a C++17 compiler (GCC/Clang) and CMake ≥ 3.10. No dependencies.

## Usage

```bash
./build/lab test                                # 19 unit tests
./build/lab play <game> <algo> [depth] [sims]   # AI vs AI demo
./build/lab experiment [out.csv] [decisions.csv]  # run the experiment matrix
./build/lab dashboard [in.csv] [out.html]       # rebuild the dashboard
```

Examples:

```bash
./build/lab play connect4 alphabeta 4
./build/lab play minichess minimax 3
./build/lab play ludo expectimax 4
./build/lab experiment            # writes results/experiments.csv + results/dashboard.html
```

## Metrics collected

- **Decision time** (ms per move, wall clock)
- **Nodes explored** (game-tree nodes evaluated)
- **Nodes pruned** (alpha-beta cutoffs)
- **MCTS simulations** (rollouts)
- **Win / Draw / Loss** record per configuration

## Experiment matrix (default: 17 configurations × 5 seeds = 85 runs)

- **Connect 4**: minimax d2; alpha-beta d2/d4/d6; MCTS 200/1000 sims
- **MiniChess**: minimax d2; alpha-beta d2/d3/d4; MCTS 500 sims
- **Ludo**: expectimax d4/d6; alpha-beta d4; MCTS 300 sims
- **Head-to-head**: expectimax vs MCTS (Ludo), alpha-beta vs MCTS (Connect 4)
- Every configuration runs with seeds 42–46

## Output

- `results/experiments.csv` — one row per match: game, algorithm, depth,
  sims, seed, moves, avg ms, nodes, pruned, sims, result
- `results/decisions.csv` — one row per AI decision (game, algorithm, depth,
  sims, seed, ply, player, nodes, pruned, sims, decision_ms) for plotting
  nodes/time vs depth per algorithm
- `results/dashboard.html` — self-contained HTML (inline SVG bar charts, no
  external assets): per-game charts for avg time, nodes, pruned, simulations,
  plus W/D/L tables and the raw data table

## Project structure

```
CMakeLists.txt
src/main.cpp                      # CLI, 19 unit tests, experiment matrix
include/game_engine/
  core.hpp      GameState / Move abstraction (decision/chance/terminal)
  metrics.hpp   SearchStats + aggregated Metrics
  connect4.hpp  Connect 4 rules + heuristic
  minichess.hpp 5×5 minichess rules + heuristic
  ludo.hpp      Ludo-lite rules (stochastic)
  minimax.hpp   Minimax
  alphabeta.hpp Alpha-Beta (counts prunes)
  expectimax.hpp Expectimax (chance expectations)
  mcts.hpp      MCTS with UCT
  experiments.hpp AI-vs-AI match runner, CSV export
  dashboard.hpp HTML/SVG dashboard generator
docs/PLAN.md    workflow mapping + analysis notes
```

## Selected observations from the default run

- Alpha-Beta at depth 2 on Connect 4 evaluates the same 1,607 nodes as
  Minimax (no cutoffs at shallow depth), while depth 6 evaluates 376k nodes
  and prunes ~60k — pruning savings grow with depth.
- Ludo's tiny effective branching (≤2 token moves × 6 dice) keeps Expectimax
  at ~1.7k nodes (d4) and ~17.6k nodes (d6) — stochastic games need
  expectation layers, not bigger trees.
- MCTS on MiniChess costs ~470 ms/move at 500 sims because each rollout
  can run 400 plies; its node count is far lower than Alpha-Beta's but the
  time is dominated by random-playout length.
