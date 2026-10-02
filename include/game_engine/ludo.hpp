#pragma once

#include <sstream>
#include <string>

#include "core.hpp"

namespace game_engine {

// Simplified "Ludo-lite" for stochastic-game research:
//   - 2 players, 2 tokens each
//   - Shared 24-cell ring; player 1 enters at cell 0, player 2 at cell 12
//   - A token needs an exact roll to finish (home column of 4 cells)
//   - A token leaves base only on a roll of 6 (classic rule)
//   - Landing on an opponent's ring cell captures it (entry cells are safe)
//   - Rolling a 6 grants an extra turn
// State alternates CHANCE (roll the die) -> DECISION (pick a token) -> ...
class LudoState : public GameState {
public:
    static constexpr int TRACK = 24;             // ring cells
    static constexpr int HOME = 4;               // home-column cells
    static constexpr int FINISH = TRACK + HOME;  // progress == FINISH-1 done
    static constexpr int TOKENS = 2;
    static constexpr int ENTRY[2] = {0, 12};     // ring entry cell per player
    static constexpr int SAFE_CELLS[2] = {ENTRY[0], ENTRY[1]};

    int tokens[2][TOKENS];  // -1 = base, 0..FINISH-1 = progress
    int player = 1;
    bool chancePhase = true;  // true: die to roll; false: token to move
    int dice = 0;             // rolled value once chancePhase == false
    int plies = 0;

    LudoState() { reset(); }

    void reset() {
        for (int p = 0; p < 2; ++p)
            for (int t = 0; t < TOKENS; ++t) tokens[p][t] = -1;
        player = 1;
        chancePhase = true;
        dice = 0;
        plies = 0;
    }

    std::unique_ptr<GameState> clone() const override {
        return std::make_unique<LudoState>(*this);
    }
    std::string gameName() const override { return "ludo"; }

    bool isChanceNode() const override { return chancePhase; }

    // Ring cell occupied by token of player p at progress pos (-1 => none).
    int ringCell(int p, int pos) const {
        if (pos < 0 || pos >= TRACK) return -1;
        return (ENTRY[p] + pos) % TRACK;
    }

    static bool isSafeCell(int cell) {
        return cell == ENTRY[0] || cell == ENTRY[1];
    }

    // Is moving token t of player p by `roll` legal?
    bool legalMove(int p, int t, int roll) const {
        int pos = tokens[p][t];
        if (pos < 0) return roll == 6;              // leave base only on a 6
        return pos + roll < FINISH;                 // exact roll to finish
    }

    std::vector<Move> getMoves() const override {
        std::vector<Move> moves;
        if (chancePhase) {
            // CHANCE node: six equiprobable dice outcomes.
            for (int d = 1; d <= 6; ++d) {
                Move m;
                m.param = d;
                m.weight = 1.0 / 6.0;
                m.label = "die=" + std::to_string(d);
                moves.push_back(m);
            }
        } else {
            int p = player - 1;
            for (int t = 0; t < TOKENS; ++t) {
                if (legalMove(p, t, dice)) {
                    Move m;
                    m.from = t;
                    m.param = dice;
                    m.label = "token " + std::to_string(t + 1);
                    moves.push_back(m);
                }
            }
        }
        return moves;
    }

    void applyMove(const Move& m) override {
        if (chancePhase) {
            // Dice outcome: either a decision follows, or the turn passes
            // (no token can use this roll).
            dice = m.param;
            int p = player - 1;
            bool any = false;
            for (int t = 0; t < TOKENS; ++t)
                if (legalMove(p, t, dice)) { any = true; break; }
            // A usable roll leads to a DECISION node; an unusable one
            // passes the turn and we stay at a CHANCE node.
            chancePhase = !any;
            if (!any) player = 3 - player;
            ++plies;
            return;
        }

        // Token move.
        int p = player - 1;
        int t = m.from;
        int& pos = tokens[p][t];
        pos = (pos < 0) ? 0 : pos + dice;  // leaving base enters at progress 0

        // Capture on the shared ring (entry cells are safe).
        int cell = ringCell(p, pos);
        if (cell >= 0 && !isSafeCell(cell)) {
            int opp = 1 - p;
            for (int ot = 0; ot < TOKENS; ++ot) {
                if (ringCell(opp, tokens[opp][ot]) == cell)
                    tokens[opp][ot] = -1;
            }
        }

        // Rolling a 6 keeps the turn; otherwise the turn passes.
        if (dice != 6) player = 3 - player;
        chancePhase = true;
        ++plies;
    }

    bool allHome(int p) const {
        for (int t = 0; t < TOKENS; ++t)
            if (tokens[p][t] < FINISH - 1) return false;
        return true;
    }

    bool isTerminal() const override { return allHome(0) || allHome(1); }

    int winner() const override {
        if (allHome(0)) return 1;
        if (allHome(1)) return 2;
        return kDraw;
    }

    int currentPlayer() const override { return player; }

    // Progress-based heuristic: total advancement plus finished-token bonus.
    double heuristic(int p) const override {
        auto side = [&](int pl) {
            double s = 0.0;
            for (int t = 0; t < TOKENS; ++t) {
                int pos = tokens[pl][t];
                if (pos < 0) s += 0.0;
                else if (pos >= FINISH - 1) s += 1000.0;
                else s += pos * 10.0;
            }
            return s;
        };
        return side(p - 1) - side(2 - p);
    }

    std::string toString() const override {
        std::ostringstream os;
        os << "player " << player << (chancePhase ? " (rolling)\n" : " dice=" + std::to_string(dice) + "\n");
        for (int p = 0; p < 2; ++p) {
            os << "  P" << (p + 1) << ":";
            for (int t = 0; t < TOKENS; ++t)
                os << " " << (tokens[p][t] < 0 ? "base" : std::to_string(tokens[p][t]));
            os << "\n";
        }
        return os.str();
    }
};

}  // namespace game_engine
