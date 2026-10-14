// Scratch: solvers side by side on N cameras with k mislabelled, by ray count.
#include "Triangulation.hpp"
#include "greedy.hpp"
#include "loo_proto.hpp"
#include "cand.hpp"
#include "cand2.hpp"
#include "cand3.hpp"
#include "cand4.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
typedef int (*Solver)(BlobRay*, int, double, glm::dvec3&);
int main(int argc, char** argv) {
    const string dir = argv[1];
    const int reps = argc > 2 ? atoi(argv[2]) : 3;
    const double wrongAt = argc > 3 ? atof(argv[3]) : 0.1;
    vector<pair<string, Solver>> solvers = {{"old (worst of joint)", intersectRaysGreedy}, {"prototype", intersectRaysProto}, {"scratch refit+sum", intersectRaysAM<0, 1 << 30>}, {"header", intersectRays}};
    const int S = solvers.size();
    ifstream in(dir + "/cases.txt");
    map<string, EyeCalibration> rig; string line; vector<BlobRay> rays; int noise = 0;
    struct Stats { int cases = 0, right[8] = {}, none[8] = {}, wrong[8] = {}; double sq[8] = {}, sec[8] = {}; long differ = 0; };
    map<string, Stats> stats; vector<string> order;
    while (getline(in, line)) {
        istringstream ls(line); string word; ls >> word;
        if (word == "rig") { string f; ls >> f; rig = loadEyeCalibrations(dir + "/" + f); }
        else if (word == "group") { string name, a, b, c, d, e; ls >> name >> a >> b >> c >> d >> e; noise = c.back() - '0'; }
        else if (word == "ray") { string h; double x, y; ls >> h >> x >> y; rays.push_back({0, rig[h].center, rig[h].ray(x, y), 0}); }
        else if (word == "truth") {
            int m, k; glm::dvec3 truth; ls >> m >> k >> truth.x >> truth.y >> truth.z;
            char key[64]; snprintf(key, sizeof key, "noise %d  n=%2d  k=%d", noise, m, k);
            if (!stats.count(key)) order.push_back(key);
            Stats& s = stats[key]; s.cases++;
            glm::dvec3 xs[8]; int ns[8];
            for (int j = 0; j < S; j++) {
                glm::dvec3 x(0.0); int n = 0;
                auto t0 = std::chrono::steady_clock::now();
                for (int r = 0; r < reps; r++) n = solvers[j].second(rays.data(), (int) rays.size(), glm::radians(1.0), x);
                s.sec[j] += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / reps;
                xs[j] = x; ns[j] = n;
                if (n == 0) { s.none[j]++; continue; }
                const double e = glm::length(x - truth);
                if (e < wrongAt) { s.right[j]++; s.sq[j] += e * e; } else s.wrong[j]++;
            }
            if (ns[2] != ns[3] || (ns[1] > 0 && glm::length(xs[2] - xs[3]) > 1e-9)) s.differ++;
            rays.clear();
        }
    }
    sort(order.begin(), order.end());
    printf("%-22s %6s |", "", "cases");
    for (int j = 0; j < S; j++) printf(" %-20s |", solvers[j].first.c_str());
    printf(" scratch != header\n%-22s %6s |", "", "");
    for (int j = 0; j < S; j++) printf(" %6s %5s %7s |", "right%", "wrng%", "us");
    printf("\n");
    for (auto& g : order) { Stats& s = stats[g];
        printf("%-22s %6d |", g.c_str(), s.cases);
        for (int j = 0; j < S; j++) printf(" %6.1f %5.2f %7.2f |", 100.0 * s.right[j] / s.cases, 100.0 * s.wrong[j] / s.cases, 1e6 * s.sec[j] / s.cases);
        printf(" %ld\n", s.differ); }
}
