#pragma once

#include <sstream>
#include <string>

#include "core.hpp"

namespace game_engine {

// Gardner's Minichess: the classic 5x5 variant used in AI courses.
//   - Pieces: King, Queen, Rook, Bishop, Knight, 5 Pawns per side
//   - No castling, no en passant, no double pawn step
//   - Pawns promote to a queen on the last rank
//   - Checkmate wins; stalemate and bare kings draw.
// White (player 1) = uppercase, moves first.
class MiniChessState : public GameState {
public:
    static constexpr int N = 5;  // board is N x N

    char board[N * N];  // '.' empty | 'PNBRQK' white | 'pnbrqk' black
    int player = 1;
    int plies = 0;

    MiniChessState() { reset(); }

    void reset() {
        // Row 0 = top = black's home; row 4 = bottom = white's home.
        static const char* back = "RNBQK";
        for (int i = 0; i < N * N; ++i) board[i] = '.';
        for (int c = 0; c < N; ++c) {
            board[0 * N + c] = (char)(back[c] - 'A' + 'a');  // black back rank
            board[1 * N + c] = 'p';                          // black pawns
            board[3 * N + c] = 'P';                          // white pawns
            board[4 * N + c] = back[c];                      // white back rank
        }
        player = 1;
        plies = 0;
    }

    std::unique_ptr<GameState> clone() const override {
        return std::make_unique<MiniChessState>(*this);
    }
    std::string gameName() const override { return "minichess"; }

    static bool isWhite(char pc) { return pc >= 'A' && pc <= 'Z'; }
    static char toUpper(char pc) { return (char)(pc >= 'a' && pc <= 'z' ? pc - 32 : pc); }

    bool inBounds(int r, int c) const { return r >= 0 && r < N && c >= 0 && c < N; }

    // Is square (r,c) attacked by any piece of colour `white`?
    bool attacked(int r, int c, bool white) const {
        auto at = [&](int rr, int cc) { return board[rr * N + cc]; };
        // Pawns: a white pawn attacks towards row-1, so it sits at r+1.
        int pr = white ? r + 1 : r - 1;
        for (int dc : {-1, 1}) {
            int pc = c + dc;
            if (inBounds(pr, pc) && at(pr, pc) == (white ? 'P' : 'p')) return true;
        }
        static const int kn[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        for (auto& o : kn) {
            int rr = r + o[0], cc = c + o[1];
            if (inBounds(rr, cc) && at(rr, cc) == (white ? 'N' : 'n')) return true;
        }
        for (int dr = -1; dr <= 1; ++dr)
            for (int dc = -1; dc <= 1; ++dc) {
                if (!dr && !dc) continue;
                int rr = r + dr, cc = c + dc;
                if (inBounds(rr, cc) && at(rr, cc) == (white ? 'K' : 'k')) return true;
            }
        static const int dirs[8][2] = {{-1,-1},{-1,1},{1,-1},{1,1},{-1,0},{1,0},{0,-1},{0,1}};
        for (int d = 0; d < 8; ++d) {
            bool diag = d < 4;
            int rr = r, cc = c;
            while (true) {
                rr += dirs[d][0];
                cc += dirs[d][1];
                if (!inBounds(rr, cc)) break;
                char pc = at(rr, cc);
                if (pc != '.') {
                    if (isWhite(pc) == white) {
                        char t = toUpper(pc);
                        if (t == 'Q' || (diag && t == 'B') || (!diag && t == 'R'))
                            return true;
                    }
                    break;
                }
            }
        }
        return false;
    }

    void findKing(bool white, int& kr, int& kc) const {
        char k = white ? 'K' : 'k';
        kr = kc = -1;
        for (int r = 0; r < N; ++r)
            for (int c = 0; c < N; ++c)
                if (board[r * N + c] == k) { kr = r; kc = c; return; }
    }

    bool kingSafe(bool white) const {
        int kr, kc;
        findKing(white, kr, kc);
        return kr >= 0 && !attacked(kr, kc, !white);
    }

    static std::string squareName(int sq) {
        return std::string(1, (char)('a' + sq % N)) + std::to_string(sq / N + 1);
    }

    // Pseudo-legal moves for the piece at (r,c).
    void pieceMoves(int r, int c, std::vector<Move>& out) const {
        char pc = board[r * N + c];
        if (pc == '.') return;
        bool white = isWhite(pc);
        char t = toUpper(pc);
        auto push = [&](int rr, int cc, int promo = 0) {
            Move m;
            m.from = r * N + c;
            m.to = rr * N + cc;
            m.param = promo;
            m.label = squareName(m.from) + "-" + squareName(m.to);
            if (promo) m.label += "=" + std::string(1, (char)promo);
            out.push_back(m);
        };
        auto canEnter = [&](int rr, int cc) {
            char x = board[rr * N + cc];
            return x == '.' || isWhite(x) != white;
        };

        if (t == 'P') {
            int fwd = white ? -1 : 1;
            int promoRow = white ? 0 : N - 1;
            if (inBounds(r + fwd, c) && board[(r + fwd) * N + c] == '.') {
                push(r + fwd, c, (r + fwd) == promoRow ? (white ? 'Q' : 'q') : 0);
            }
            for (int dc : {-1, 1}) {
                int rr = r + fwd, cc = c + dc;
                if (inBounds(rr, cc) && board[rr * N + cc] != '.' && canEnter(rr, cc)) {
                    push(rr, cc, rr == promoRow ? (white ? 'Q' : 'q') : 0);
                }
            }
        } else if (t == 'N') {
            static const int kn[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
            for (auto& o : kn) {
                int rr = r + o[0], cc = c + o[1];
                if (inBounds(rr, cc) && canEnter(rr, cc)) push(rr, cc);
            }
        } else if (t == 'K') {
            for (int dr = -1; dr <= 1; ++dr)
                for (int dc = -1; dc <= 1; ++dc) {
                    if (!dr && !dc) continue;
                    int rr = r + dr, cc = c + dc;
                    if (inBounds(rr, cc) && canEnter(rr, cc)) push(rr, cc);
                }
        } else {  // B, R, Q sliders
            static const int dirs[8][2] = {{-1,-1},{-1,1},{1,-1},{1,1},{-1,0},{1,0},{0,-1},{0,1}};
            int start = (t == 'B') ? 0 : (t == 'R') ? 4 : 0;
            int ndir = (t == 'Q') ? 8 : 4;
            for (int d = start; d < start + ndir; ++d) {
                int rr = r, cc = c;
                while (true) {
                    rr += dirs[d][0];
                    cc += dirs[d][1];
                    if (!inBounds(rr, cc) || !canEnter(rr, cc)) break;
                    push(rr, cc);
                    if (board[rr * N + cc] != '.') break;  // capture stops sliding
                }
            }
        }
    }

    std::vector<Move> getMoves() const override {
        std::vector<Move> pseudo, legal;
        bool white = (player == 1);
        for (int r = 0; r < N; ++r)
            for (int c = 0; c < N; ++c) {
                char pc = board[r * N + c];
                if (pc != '.' && isWhite(pc) == white) pieceMoves(r, c, pseudo);
            }
        for (const Move& m : pseudo) {
            MiniChessState next = *this;
            next.applyMove(m);
            if (next.kingSafe(white)) legal.push_back(m);
        }
        return legal;
    }

    void applyMove(const Move& m) override {
        char pc = board[m.from];
        board[m.to] = (m.param != 0) ? (char)m.param : pc;
        board[m.from] = '.';
        player = 3 - player;
        ++plies;
    }

    bool noLegalMoves() const { return getMoves().empty(); }

    bool bareKings() const {
        for (int i = 0; i < N * N; ++i)
            if (board[i] != '.' && toUpper(board[i]) != 'K') return false;
        return true;
    }

    bool isTerminal() const override {
        return noLegalMoves() || bareKings() || plies >= 200;
    }

    int winner() const override {
        if (noLegalMoves()) {
            int kr, kc;
            findKing(player == 1, kr, kc);
            if (kr >= 0 && attacked(kr, kc, player != 1)) return 3 - player;  // checkmate
            return kDraw;  // stalemate
        }
        return kDraw;
    }

    int currentPlayer() const override { return player; }

    // Material + centralisation heuristic, from `p`'s perspective.
    double heuristic(int p) const override {
        auto pieceValue = [](char pc) {
            switch (toUpper(pc)) {
                case 'P': return 100.0;
                case 'N': return 320.0;
                case 'B': return 330.0;
                case 'R': return 500.0;
                case 'Q': return 900.0;
                default:  return 0.0;
            }
        };
        double score = 0.0;
        bool white = (p == 1);
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                char pc = board[r * N + c];
                if (pc == '.') continue;
                double v = pieceValue(pc);
                if (r >= 1 && r <= 3 && c >= 1 && c <= 3) v += 6.0;  // centre bonus
                score += (isWhite(pc) == white) ? v : -v;
            }
        }
        return score;
    }

    std::string toString() const override {
        std::ostringstream os;
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) os << board[r * N + c] << ' ';
            os << '\n';
        }
        return os.str();
    }
};

}  // namespace game_engine
