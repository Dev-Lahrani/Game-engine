# PLAN — Research Workflow Mapping

This document maps the 12-step course workflow onto the code and
records the analysis notes for the experiment.

## The 12 steps → code

| # | Step | Where |
|---|------|-------|
| 1 | Select a game → Chess / Connect 4 / Ludo | `makeGame()` in `experiments.hpp`; `lab play <game>` |
| 2 | Create the current game state (board, pieces, turn) | `Connect4State`, `MiniChessState`, `LudoState` (`reset()`) |
| 3 | Generate possible moves / actions | `GameState::getMoves()` — legal moves at DECISION nodes, weighted dice outcomes at CHANCE nodes |
| 4 | Apply the suitable algorithm | `selectMove()` dispatches to `minimax` / `alphabeta` / `expectimax` / `mcts` |
| 5 | Algorithm searches possible future states | `minimaxValue` / `abValue` / `maxValue`+`minValue`+`chanceValue` / MCTS selection-expansion-simulation-backpropagation |
| 6 | AI selects the best action | root move with best value (tree searches) or most-visited child (MCTS) |
| 7 | Game state changes, process repeats | `applyMove()`; AI-vs-AI loop in `runExperiment()` until terminal |
| 8 | Collect real metrics | `SearchStats` per move (nodes, pruned, sims, ms) aggregated into `Metrics`, plus a `DecisionStats` record per AI move |
| 9 | Run experiments with different depths/configs | `defaultMatrix()` — 17 configurations × seeds 42–46 (85 runs) |
| 10 | Compare algorithms across the three games | `toCsv()` → `results/experiments.csv`; `toCsvDecisions()` → `results/decisions.csv` (per-decision nodes/time for vs-depth plots) |
| 11 | Display results through graphs/dashboard | `dashboard::generate()` → self-contained `results/dashboard.html` with inline SVG bar charts |
| 12 | Analyze behaviour under different game characteristics | see "Analysis" below |

## Game characteristics matrix

| Characteristic | Connect 4 | MiniChess | Ludo |
|---|---|---|---|
| Deterministic? | Yes | Yes | **No** (dice) |
| Chance nodes | none | none | every other ply |
| Initial branching | 7 | 7 | 2 (after a 6) × 6 dice |
| Typical game length | ~30–45 plies | ~30–45 plies | ~60–90 plies |
| Terminal test | 4-in-a-row / full board | checkmate / stalemate / 200-ply cap | all tokens home |

## Algorithm selection rationale

- **Connect 4, MiniChess (deterministic)** → Minimax is the baseline;
  Alpha-Beta is the practical choice (same optimal move, fewer nodes).
  Expectimax would be wasteful (no chance nodes to average over).
- **Ludo (stochastic)** → Expectimax is the natural fit: decision
  nodes are max/min layers, chance nodes are expectation layers over
  the six dice. Alpha-Beta still runs (chance nodes evaluated as
  expectations — "alpha-beta over expectimax") but pruning is not
  valid at chance nodes, so savings are limited. MCTS handles chance
  by *sampling* dice instead of enumerating.
- **MCTS** applies to all three; it trades tree depth for random
  rollouts and is included everywhere as the comparison point.

## Implementation notes (bugs found while building)

1. **Expectimax must check for terminal states at chance nodes.**
   In Ludo a finishing move leaves the game at a *terminal CHANCE*
   node (`chancePhase == true`). Enumerating dice outcomes on a
   finished game recurses forever → stack overflow. Fix: terminal
   check (and depth floor) at the top of `chanceValue()`.
2. **Negamax sign-flip is invalid for Ludo** (roll-6-again keeps
   the turn), so all searches carry an explicit `player` perspective.
3. **Chance-phase transition**: a usable roll leads to a DECISION
   node; an unusable roll passes the turn and the state stays a
   CHANCE node (`chancePhase = !any`).
4. Dice weights at a chance node sum to 1 (checked in tests).

## Analysis — how characteristics change the algorithms

### Deterministic, moderate branching (Connect 4)
- Node count grows ~exponentially with depth: d2 = 1.6k nodes,
  d4 = 21k, d6 = 376k per match.
- Alpha-Beta equals Minimax at d2 (0 prunes — the shallow tree
  offers no cutoff opportunities) but prunes ~60k nodes at d6.
  **Pruning savings grow with depth and branching.**
- Decision time tracks node count (0.05 ms → 7.7 ms per move).

### Deterministic, tactics + slightly higher branching (MiniChess)
- Same depth curve, steeper: d4 = 163k nodes, 14.8k pruned.
- MCTS explores only ~9.7k nodes but takes ~470 ms/move: cost is
  dominated by **rollout length** (up to 400 plies of random play),
  not tree size. **MCTS time is insensitive to branching factor but
  sensitive to game length.**

### Stochastic (Ludo)
- Effective branching is tiny (≤2 token moves; 6 dice outcomes at
  chance nodes), so Expectimax stays cheap: ~1.7k nodes (d4) and
  ~17.6k nodes (d6).
- Expectimax and Alpha-Beta both finish in microseconds; the
  difference is that Expectimax's expectation layers are *correct*
  for the dice, while Alpha-Beta's pruning at chance nodes is not
  guaranteed to preserve the expectimax value.
- MCTS (300 sims) uses ~8.6k nodes and ~4 ms/move — competitive,
  and it needs no explicit chance layer at all.

### Head-to-head
- "expectimax vs mcts" (Ludo) and "alphabeta vs mcts" (Connect 4)
  rows show W/D/L when two algorithms play each other; the metrics
  columns report player 1's search statistics only.

## Conclusion template for the report

> Game characteristics determine which algorithm is suitable:
> deterministic games with high branching reward Alpha-Beta's
> pruning (savings grow with depth); stochastic games require
> expectation layers (Expectimax) or chance sampling (MCTS);
> MCTS's cost is governed by rollout length rather than branching
> factor, making it attractive when the state space is complex but
> games are short. The experiment isolates these effects by
> measuring time, nodes, prunes and simulations per decision across
> a fixed matrix of games, depths and seeds.
