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

---

# PLAN v2 — Human vs AI, more algorithms, more games, step-by-step frontend

## 1. Goal

Extend the harness from *batch experiment mode only* to a **playable frontend** with:

- human vs AI matches, play-by-play, one move shown at a time
- more algorithms beside Minimax / Alpha-Beta / Expectimax / MCTS
- more games than Connect 4 / MiniChess / Ludo
- a step-by-step board renderer that replays every move

Keep the existing experiment matrix and CLI intact. This is v2, not a rewrite of v1 research mode.

## 2. Scope for the first v2 cycle

### 2.1 Human vs AI

- New CLI: `lab play-human <game> <algo> [depth] [sims]`
- AI moves, human moves via stdin / interactive prompt
- Need explicit **whose turn** display, move prompt, and move validation
- The current `game_ai_lab::main()` play loop only does AI-vs-AI; AI-vs-AI and human-vs-AI should be separate paths

Critical points:
- Ludo human turns must show the dice roll and ask for a token choice, not just any move
- Chance-phase games need AI to announce the dice outcome and then ask the human for the token move
- Interactive prompts must be robust: retry on illegal move, allow `quit` or `undo` if we add that later

### 2.2 More algorithms

Add these to `selectMove()` dispatch and to experiment matrix:

- **Negamax** (same core idea as minimax but sign-flipped value). Valid for deterministic games; must still not be used blindly for Ludo because of roll-6-again.
- **Iterative Deepening Alpha-Beta** — run alpha-beta at depth 1, 2, 3…; useful for time control in human play because we can stop after a time budget and keep the last best move.
- **Monte Carlo rollouts with policy bias** — beyond pure UCT, try a tiny heuristic tweak or move ordering for rollout start; keep it optional.
- **Opening book / simple transposition caching** — start with a small in-memory transposition table for exact-repeat states; keep it minimal.

Implementation rule:
- Algorithms live under `include/game_engine/` as separate headers
- One shared C++ interface: pick a move given state + params
- Metrics collection stays uniform so new algorithms still produce comparable rows

### 2.3 More games

Pick a staged set so v2 grows without exploding.

Stage A (fast wins):
- **Tic-Tac-Toe** — tiny, good sanity check for human play, deterministic, easy terminal and draw detection
- **Connect 5 or Connect 6** — same family as Connect 4, slightly bigger, easy rules

Stage B (real classic games):
- **Checkers (8×8 English Draughts)** — captures, king promotion, forced captures
- **Small Othello/Reversi** — e.g. 6×6 or full 8×8; evaluation matters; good next test for heuristics
- **Nim (multiple heaps)** — simple, deterministic, non-search contrast (optimal = nim-sum), good didactic game
- **Mancala / Kalah variant** — captures + extra-turn mechanics, deterministic

Stage C (stretch):
- **Full Chess** — big, needs careful move generation, castling, en passant, promotion, repetition/50-move draws, and cutoffs well beyond 200 plies
- **Backgammon-lite** — stochastic, bigger state, bear-off, entry/hitting; keep without doubling cube for v2

Game design rule:
- Every new game must implement the same `GameState` shape: `reset()`, `getMoves()`, `applyMove()`, `toString()` for frontend, `isTerminal()`/`winner()`
- Deterministic games can use minimax/alphabeta/negamax/ICPV; stochastic games need expectimax or MCTS with dice sampling

### 2.4 Step-by-step frontend

Two-tier approach: first a **self-contained local frontend**, then optionally a hosted web UI.

Option A — simplest, fits research repo:

- Add a `lab play-human --tee <out.json>` mode that writes every move as structured JSONL
- Add a small static frontend that reads that JSONL and replays the game step by step
- Frontend can be:
  - a browser page with inline JS + SVG/HTML boards, or
  - a tiny local HTTP server that serves the static files

Recommended minimal frontend features:
- Show board state for current ply
- Show whose turn, last move text, last dice roll if any
- Step forward/backward through the replay
- Highlight illegal vs legal moves for the human when it’s their turn
- Show search stats for AI moves: time, nodes, prunes, sims if applicable

Option B — richer but more work:

- A WebSocket or local API where the C++ process streams move events to a browser client
- Real-time AI move computation shown with a spinner or progress
- Play-by-play analytics per ply

This project currently has **no web server stack**. Adding one means either:
- a small C++ HTTP server, or
- a sidecar web app talking to the C++ binary over JSON/CLI

For v2 first cycle, Option A is the least risky and keeps dependencies near zero.

## 3. Human vs AI command design

Concrete proposal:

```
./build/lab play-human <game> <algo> [depth] [sims] [--replay out.json]
```

Example flows:

- `./build/lab play-human connect4 alphabeta 4`
  - AI is player 1 or 2, human is the other
  - Human enters moves as column numbers for Connect 4
  - Board printed after each human move and after each AI move
- `./build/lab play-human ludo expectimax 4`
  - AI announces dice roll
  - Human chooses which token to move
- `./build/lab play-human tictactoe minimax 2`
  - Human enters `row col`

Important UX details:
- Show legal moves for the human each turn
- Reject illegal input and reprint legal moves
- Print the game result at the end
- Optionally print per-move AI stats inline

## 4. Metrics to keep for human games

For a human-vs-AI game, keep both:

- **AI move metrics** same as experiment mode: time, nodes, pruned, sims
- **Human move log**: ply, player, move, time spent by human if we can measure it, legal moves shown

This lets us later compare human decision patterns vs AI decision patterns if we want.

## 5. Risk list

- Interactive human play is harder to automate in tests; need careful CLI parsing and manual demo verification
- Frontend adds new files; keep them out of the core research build if we want to keep v1 small
- More games mean more rule bugs; add per-game unit tests early
- Iterative deepening and transposition table add statefulness; keep them optional and well isolated
- Full chess is large; treat as optional later stage, not first v2 goal

## 6. Suggested v2 rollout order

1. Add **Tic-Tac-Toe** as a new game and get human-vs-AI working there first
2. Add **Iterative Deepening Alpha-Beta** as the main human-friendly AI (time-controlled, interruptible)
3. Add **Negamax** for deterministic games
4. Add **play-human** CLI with move replay JSON export
5. Add **small static frontend** to replay the JSON and show step-by-step boards
6. Add **Checkers / Nim / Othello small board** next
7. Keep **full chess** and **Backgammon** as later stretch goals

## 7. What this needs from us

- Decide which frontend style we want first: static replay page vs live web app
- Decide whether we keep everything in one repo or separate the frontend into its own folder/app
- Decide which game is first after Connect 4 for human play: Tic-Tac-Toe or Checkers
- Decide which new algorithm is highest priority: Iterative Deepening Alpha-Beta, Negamax, or both
