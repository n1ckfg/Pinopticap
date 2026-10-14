// Scratch: time per index group, old solver against the header, by labels config.
#include "Triangulation.hpp"
#include "greedy.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
int main(int, char** argv) {
    const string dir = argv[1];
    ifstream in(dir + "/labels.txt");
    map<string, EyeCalibration> rig; vector<BlobRay> rays; string line, name;
    struct Stats { long groups = 0, raysIn = 0; double sec[2] = {0, 0}, worst[2] = {0, 0}; int got[2] = {0, 0}; };
    map<string, Stats> stats; vector<string> order;
    while (getline(in, line)) {
        istringstream ls(line); string word; ls >> word;
        if (word == "rig") { name.clear(); string f; ls >> f; rig = loadEyeCalibrations(dir + "/" + f); }
        else if (word == "config") { ls >> name; order.push_back(name); }
        else if (word == "ray") { string h; double x, y; ls >> h >> x >> y; rays.push_back({0, rig[h].center, rig[h].ray(x, y), 0}); }
        else if (word == "index") {
            Stats& s = stats[name]; s.groups++; s.raysIn += rays.size();
            for (int k = 0; k < 2; k++) {
                glm::dvec3 x; int n = 0; double best = 1e9;
                for (int rep = 0; rep < 5; rep++) {
                    auto t0 = std::chrono::steady_clock::now();
                    n = k ? intersectRays(rays.data(), (int) rays.size(), glm::radians(1.0), x) : intersectRaysGreedy(rays.data(), (int) rays.size(), glm::radians(1.0), x);
                    best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
                }
                s.sec[k] += best; s.worst[k] = std::max(s.worst[k], best); if (n > 0) s.got[k]++;
            }
            rays.clear();
        }
    }
    printf("%-34s %7s %5s | %7s %8s %8s | %7s %8s %8s\n", "", "", "", "old", "", "", "new", "", "");
    printf("%-34s %7s %5s | %7s %8s %8s | %7s %8s %8s\n", "config", "groups", "rays", "points", "mean us", "max us", "points", "mean us", "max us");
    for (auto& g : order) { Stats& s = stats[g];
        printf("%-34s %7ld %5.1f | %7d %8.2f %8.2f | %7d %8.2f %8.2f\n", g.c_str(), s.groups, (double) s.raysIn / s.groups, s.got[0], 1e6 * s.sec[0] / s.groups, 1e6 * s.worst[0], s.got[1], 1e6 * s.sec[1] / s.groups, 1e6 * s.worst[1]); }
}
