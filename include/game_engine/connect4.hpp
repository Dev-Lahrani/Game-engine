#pragma once

#include <sstream>

#include "core.hpp"

namespace game_engine {

// Standard Connect 4 on a 7x6 board. Deterministic, perfect information.
// Move.to = column (0..6); the piece drops to the lowest free row.
class Connect4State : public GameState {
public:
    static constexpr int W = 7;
    static constexpr int H = 6;

    int board[W * H]{};  // 0 empty | 1 player 1 | 2 player 2
    int player = 1;
    int plies = 0;

    std::unique_ptr<GameState> clone() const override {
        return std::make_unique<Connect4State>(*this);
    }
    std::string gameName() const override { return "connect4"; }

    bool inBounds(int r, int c) const { return r >= 0 && r < H && c >= 0 && c < W; }

    std::vector<Move> getMoves() const override {
        std::vector<Move> moves;
        for (int c = 0; c < W; ++c) {
            if (board[c] == 0) {  // top cell free => column not full
                Move m;
                m.to = c;
                m.label = "col " + std::to_string(c + 1);
                moves.push_back(m);
            }
        }
        return moves;
    }

    void applyMove(const Move& m) override {
        for (int r = H - 1; r >= 0; --r) {
            if (board[r * W + m.to] == 0) {
                board[r * W + m.to] = player;
                break;
            }
        }
        player = 3 - player;
        ++plies;
    }

    int lineWinner() const {
        static const int dr[] = {0, 1, 1, 1};
        static const int dc[] = {1, 0, 1, -1};
        for (int r = 0; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                int p = board[r * W + c];
                if (p == 0) continue;
                for (int d = 0; d < 4; ++d) {
                    bool four = true;
                    for (int k = 1; k < 4; ++k) {
                        int rr = r + dr[d] * k, cc = c + dc[d] * k;
                        if (!inBounds(rr, cc) || board[rr * W + cc] != p) {
                            four = false;
                            break;
                        }
                    }
                    if (four) return p;
                }
            }
        }
        return kDraw;
    }

    bool isTerminal() const override {
        return lineWinner() != kDraw || plies == W * H;
    }

    int winner() const override { return lineWinner(); }
    int currentPlayer() const override { return player; }

    // Value of one 4-cell window, from the perspective of "me".
    static double windowVal(int me, int opp) {
        if (me > 0 && opp > 0) return 0.0;  // blocked window
        switch (me) {
            case 4: return 100000.0;
            case 3: return 120.0;
            case 2: return 12.0;
            case 1: return 1.0;
            default: return 0.0;
        }
    }

    // Sum over all 4-in-a-row windows: my potential minus opponent's potential.
    double heuristic(int p) const override {
        static const int dr[] = {0, 1, 1, 1};
        static const int dc[] = {1, 0, 1, -1};
        double score = 0.0;
        for (int r = 0; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                for (int d = 0; d < 4; ++d) {
                    int er = r + dr[d] * 3, ec = c + dc[d] * 3;
                    if (!inBounds(er, ec)) continue;
                    int cnt[3] = {0, 0, 0};
                    for (int k = 0; k < 4; ++k)
                        ++cnt[board[(r + dr[d] * k) * W + (c + dc[d] * k)]];
                    score += windowVal(cnt[p], cnt[3 - p])
                           - windowVal(cnt[3 - p], cnt[p]);
                }
            }
        }
        return score;
    }

    std::string toString() const override {
        std::ostringstream os;
        for (int r = 0; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                int v = board[r * W + c];
                os << (v == 0 ? '.' : (v == 1 ? 'X' : 'O')) << ' ';
            }
            os << '\n';
        }
        return os.str();
    }
};

}  // namespace game_engine
