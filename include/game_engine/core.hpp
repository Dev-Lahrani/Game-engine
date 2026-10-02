#pragma once

#include <memory>
#include <string>
#include <vector>

namespace game_engine {

constexpr int kDraw = 0;

// A Move is interpreted by each game. Common fields:
//   from, to  : board squares / token indices (game-specific meaning)
//   param     : extra data (dice value, promotion piece, ...)
//   weight    : probability weight for CHANCE outcomes (e.g. dice faces)
struct Move {
    int from = -1;
    int to = -1;
    int param = 0;
    double weight = 1.0;
    std::string label;

    bool operator==(const Move& o) const {
        return from == o.from && to == o.to && param == o.param;
    }
};

// Abstract game state. A state is exactly one of:
//   * DECISION node - currentPlayer() must act; getMoves() lists legal actions.
//   * CHANCE node   - isChanceNode() == true; getMoves() lists stochastic
//                      outcomes (dice faces) with weights. Games like Ludo
//                      alternate chance -> decision -> chance -> ...
//   * TERMINAL node - isTerminal() == true; winner() reports 1, 2 or kDraw.
//
// Convention at terminal states: currentPlayer() returns the side that would
// move next (i.e. the player who just moved has already finished), so the
// negamax sign flip stays consistent: a terminal value is winning for the
// side to move iff winner() == currentPlayer().
class GameState {
public:
    virtual ~GameState() = default;

    virtual std::unique_ptr<GameState> clone() const = 0;
    virtual std::vector<Move> getMoves() const = 0;
    virtual void applyMove(const Move& m) = 0;

    virtual bool isTerminal() const = 0;
    virtual int winner() const = 0;          // 0 (draw) | 1 | 2
    virtual int currentPlayer() const = 0;   // 1 | 2
    virtual bool isChanceNode() const { return false; }

    // Static evaluation from `player`'s point of view (used at depth limits).
    virtual double heuristic(int player) const = 0;

    virtual std::string gameName() const = 0;
    virtual std::string toString() const { return ""; }
};

}  // namespace game_engine
