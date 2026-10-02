#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "experiments.hpp"

namespace game_engine {

namespace dashboard {

struct Group {
    std::string game;
    std::string algorithm;
    int depth = 0;
    int sims = 0;
    int count = 0;
    double avgMs = 0.0;
    double nodes = 0.0;
    double pruned = 0.0;
    double simsTotal = 0.0;
    int wins = 0, draws = 0, losses = 0;  // from player-1 perspective
};

inline std::vector<Group> groupResults(const std::vector<ExperimentResult>& rs) {
    std::map<std::string, Group> map;
    auto key = [](const ExperimentResult& r) {
        return r.metrics.game + "|" + r.metrics.algorithm + "|" +
               std::to_string(r.metrics.depth) + "|" +
               std::to_string(r.metrics.simulations);
    };
    for (const auto& r : rs) {
        Group& g = map[key(r)];
        if (g.count == 0) {
            g.game = r.metrics.game;
            g.algorithm = r.metrics.algorithm;
            g.depth = r.metrics.depth;
            g.sims = r.metrics.simulations;
        }
        ++g.count;
        g.avgMs += r.metrics.avgDecisionMs;
        g.nodes += r.metrics.totalNodes;
        g.pruned += r.metrics.totalPruned;
        g.simsTotal += r.metrics.totalSims;
        if (r.metrics.result == 1) ++g.wins;
        else if (r.metrics.result == 2) ++g.losses;
        else ++g.draws;
    }
    std::vector<Group> out;
    for (auto& kv : map) {
        kv.second.avgMs /= kv.second.count;
        out.push_back(kv.second);
    }
    return out;
}

inline std::string svgBarChart(const std::string& title,
                               const std::vector<std::string>& labels,
                               const std::vector<double>& values,
                               const std::string& unit) {
    const int barW = 64, gap = 18, padL = 60, padT = 40, padB = 52, H = 260;
    int n = (int)labels.size();
    int W = padL + n * (barW + gap) + 10;
    double maxV = 1e-9;
    for (double v : values) maxV = std::max(maxV, v);
    std::ostringstream os;
    os << "<svg width='" << W << "' height='" << H << "' "
          "style='background:#fff;border:1px solid #ddd;border-radius:6px'>";
    os << "<text x='" << (W / 2) << "' y='20' text-anchor='middle' "
          "font-size='13' font-weight='bold' fill='#222'>" << title << "</text>";
    // y gridlines
    for (int g = 0; g <= 4; ++g) {
        int y = padT + (H - padT - padB) * g / 4;
        os << "<line x1='" << padL << "' y1='" << y << "' x2='" << (W - 10)
           << "' y2='" << y << "' stroke='#eee'/>";
    }
    for (int i = 0; i < n; ++i) {
        int x = padL + i * (barW + gap);
        int h = (int)((H - padT - padB) * values[i] / maxV);
        int y = H - padB - h;
        os << "<rect x='" << x << "' y='" << y << "' width='" << barW
           << "' height='" << h << "' fill='" << (i % 2 ? "#4c78a8" : "#f58518")
           << "' rx='3'/>";
        char buf[64];
        if (values[i] >= 1000)
            std::snprintf(buf, sizeof(buf), "%.1fk", values[i] / 1000.0);
        else
            std::snprintf(buf, sizeof(buf), "%.2f", values[i]);
        os << "<text x='" << (x + barW / 2) << "' y='" << (y - 4)
           << "' text-anchor='middle' font-size='9' fill='#333'>" << buf
           << "</text>";
        os << "<text x='" << (x + barW / 2) << "' y='" << (H - padB + 14)
           << "' text-anchor='middle' font-size='9' fill='#333' "
              "transform='rotate(-25 " << (x + barW / 2) << " "
           << (H - padB + 14) << ")'>" << labels[i] << "</text>";
    }
    os << "<text x='10' y='" << (H / 2) << "' font-size='10' fill='#666' "
          "transform='rotate(-90 12 " << (H / 2) << ")'>" << unit
       << "</text></svg>";
    return os.str();
}

inline std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

inline std::string generate(const std::vector<ExperimentResult>& rs) {
    auto groups = groupResults(rs);
    std::ostringstream os;
    os << "<!DOCTYPE html><html><head><meta charset='utf-8'>"
          "<title>Game AI Lab - Experiment Dashboard</title><style>"
          "body{font-family:Arial,Helvetica,sans-serif;margin:24px;color:#222;"
          "background:#fafafa}"
          "h1{border-bottom:3px solid #4c78a8;padding-bottom:8px}"
          "h2{color:#4c78a8;margin-top:32px}"
          "h3{color:#555}"
          ".charts{display:flex;flex-wrap:wrap;gap:16px}"
          "table{border-collapse:collapse;margin:12px 0;background:#fff}"
          "th,td{border:1px solid #ddd;padding:6px 12px;font-size:13px;"
          "text-align:right}"
          "th{background:#4c78a8;color:#fff;text-align:center}"
          "td.l,th.l{text-align:left}"
          "tr:nth-child(even){background:#f4f7fb}"
          ".note{background:#fffbe6;border-left:4px solid #f5c518;"
          "padding:10px 14px;margin:12px 0;max-width:900px}"
          "</style></head><body>";
    os << "<h1>Game AI Lab - Experiment Dashboard</h1>";
    os << "<div class='note'><b>Research question:</b> how do game "
          "characteristics (deterministic vs stochastic, branching factor, "
          "depth) affect the computational performance and suitability of "
          "Minimax, Alpha-Beta, Expectimax and MCTS? We are NOT ranking "
          "games against each other.</div>";

    const std::vector<std::string> games = {"connect4", "minichess", "ludo"};
    for (const auto& game : games) {
        std::vector<const Group*> gg;
        for (const auto& g : groups)
            if (g.game == game) gg.push_back(&g);
        if (gg.empty()) continue;

        os << "<h2>" << esc(game) << "</h2>";

        // charts: one per metric
        std::vector<std::string> labels;
        for (auto* g : gg)
            labels.push_back(g->algorithm + (g->depth > 0 ? " d" + std::to_string(g->depth)
                                                            : " s" + std::to_string(g->sims)));
        std::vector<double> vMs, vNodes, vPruned, vSims;
        for (auto* g : gg) {
            vMs.push_back(g->avgMs);
            vNodes.push_back(g->nodes / g->count);
            vPruned.push_back(g->pruned / g->count);
            vSims.push_back(g->simsTotal / g->count);
        }
        os << "<div class='charts'>";
        os << svgBarChart("Avg decision time (ms)", labels, vMs, "ms");
        os << svgBarChart("Avg nodes explored / match", labels, vNodes, "nodes");
        os << svgBarChart("Avg nodes pruned / match", labels, vPruned, "pruned");
        if (vSims[0] > 0)
            os << svgBarChart("Avg MCTS simulations / match", labels, vSims, "sims");
        os << "</div>";

        // results table
        os << "<table><tr><th class='l'>Algorithm</th><th>Depth</th>"
              "<th>Sims</th><th>Matches</th><th>Avg ms/move</th>"
              "<th>Nodes/move</th><th>Pruned/move</th><th>W</th><th>D</th>"
              "<th>L</th></tr>";
        for (auto* g : gg) {
            int moves = std::max(1, g->count);
            os << "<tr><td class='l'>" << esc(g->algorithm) << "</td>"
               << "<td>" << g->depth << "</td><td>" << g->sims << "</td>"
               << "<td>" << g->count << "</td>"
               << "<td>" << g->avgMs << "</td>"
               << "<td>" << (long long)(g->nodes / g->count) << "</td>"
               << "<td>" << (long long)(g->pruned / g->count) << "</td>"
               << "<td>" << g->wins << "</td><td>" << g->draws << "</td>"
               << "<td>" << g->losses << "</td></tr>";
        }
        os << "</table>";
    }

    // raw data
    os << "<h2>Raw experiment data</h2><table>"
          "<tr><th class='l'>Game</th><th class='l'>Algorithm</th>"
          "<th>Depth</th><th>Sims</th><th>Seed</th><th>Moves</th>"
          "<th>Avg ms</th><th>Nodes</th><th>Pruned</th><th>Sims</th>"
          "<th>Result (P1)</th></tr>";
    for (const auto& r : rs) {
        const auto& m = r.metrics;
        os << "<tr><td class='l'>" << esc(m.game) << "</td>"
           << "<td class='l'>" << esc(m.algorithm) << "</td>"
           << "<td>" << m.depth << "</td><td>" << m.simulations << "</td>"
           << "<td>" << m.seed << "</td><td>" << m.movesPlayed << "</td>"
           << "<td>" << m.avgDecisionMs << "</td><td>" << m.totalNodes
           << "</td><td>" << m.totalPruned << "</td><td>" << m.totalSims
           << "</td><td>"
           << (m.result == 1 ? "Win" : (m.result == 2 ? "Loss" : "Draw"))
           << "</td></tr>";
    }
    os << "</table>";
    os << "</body></html>";
    return os.str();
}

}  // namespace dashboard
}  // namespace game_engine
