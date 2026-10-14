// The header's intersectRays against the first leave-one-out prototype (which scores each trial by
// its worst angle), on the four-camera vectors and labels of gen4.py, with the time per index group.
#include "Triangulation.hpp"
#include <fstream>
#include <sstream>
#include <set>
#include <chrono>

// weighted fit of the rays with weight > 0; returns the worst squared tangent among them
static bool fitRays(BlobRay* rays, int n, glm::dvec3& x, double& worstError) {
    for (int pass = 0; pass < 2; pass++) {
        RayIntersector intersector;
        for (int i = 0; i < n; i++) if (rays[i].weight > 0) intersector.add(rays[i].origin, rays[i].direction, pass ? rays[i].weight : 1.0);
        if (!intersector.solve(x)) return false;
        worstError = 0;
        for (int i = 0; i < n; i++) {
            if (!(rays[i].weight > 0)) continue;
            const glm::dvec3 v = x - rays[i].origin;
            const double depth = glm::dot(v, rays[i].direction);
            if (!(depth > 1e-6)) { worstError = std::numeric_limits<double>::infinity(); rays[i].weight = 1.0; continue; }
            worstError = std::max(worstError, glm::dot(v, v) / (depth * depth) - 1.0);
            rays[i].weight = 1.0 / (depth * depth);
        }
    }
    return true;
}

static int intersectRaysLOO(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) {
    const double maxTan = tan(maxAngle), maxError = maxTan * maxTan;
    int numRays = n;
    for (int i = 0; i < n; i++) rays[i].weight = 1.0;
    while (numRays >= 2 && numRays * 2 > n) {
        double worst;
        if (!fitRays(rays, n, x, worst)) return 0;
        if (worst <= maxError) return numRays;
        if (numRays - 1 < 2 || (numRays - 1) * 2 <= n) return 0;
        // leave out the ray without which the rest agree best
        int best = -1; double bestError = std::numeric_limits<double>::infinity();
        for (int i = 0; i < n; i++) {
            if (!(rays[i].weight > 0)) continue;
            const double keep = rays[i].weight;
            rays[i].weight = 0;
            glm::dvec3 xi; double e;
            if (fitRays(rays, n, xi, e) && e < bestError) { best = i; bestError = e; }
            rays[i].weight = keep;
        }
        if (best == -1) return 0;
        rays[best].weight = 0;
        numRays--;
    }
    return 0;
}

typedef int (*Solver)(BlobRay*, int, double, glm::dvec3&);

int main(int, char** argv) {
    const string dir = argv[1];
    Solver solvers[2] = {intersectRays, intersectRaysLOO};
    {
        ifstream in(dir + "/vectors.txt");
        map<string, map<string, EyeCalibration>> rigs; map<string, EyeCalibration>* rig = nullptr;
        string line, group; vector<BlobRay> rays;
        struct Stats { int cases = 0, none[2] = {0, 0}, right[2] = {0, 0}, wrong[2] = {0, 0}; double sq[2] = {0, 0}; };
        map<string, Stats> stats; vector<string> order;
        while (getline(in, line)) {
            istringstream ls(line); string word; ls >> word;
            if (word == "rig") { string f; ls >> f; if (!rigs.count(f)) rigs[f] = loadEyeCalibrations(dir + "/" + f); rig = &rigs[f]; }
            else if (word == "group") { ls >> group; group = group.substr(group.find('/') == string::npos ? 0 : 0); if (!stats.count(group)) order.push_back(group); }
            else if (word == "ray") { string h; double x, y; ls >> h >> x >> y; rays.push_back({0, (*rig)[h].center, (*rig)[h].ray(x, y), 0}); }
            else if (word == "expect") {
                double deg, tol; int want; glm::dvec3 ref, truth; ls >> deg >> want >> tol >> ref.x >> ref.y >> ref.z >> truth.x >> truth.y >> truth.z;
                Stats& s = stats[group]; s.cases++;
                for (int k = 0; k < 2; k++) {
                    glm::dvec3 x(0.0);
                    if (solvers[k](rays.data(), (int) rays.size(), glm::radians(deg), x) == 0) { s.none[k]++; continue; }
                    double e = glm::length(x - truth);
                    if (e < 0.1) { s.right[k]++; s.sq[k] += e * e; } else s.wrong[k]++;
                }
                rays.clear();
            }
        }
        printf("%-36s %6s | %21s | %21s\n", "", "", "header", "prototype");
        printf("%-36s %6s | %6s %6s %6s %8s | %6s %6s %6s %8s\n", "group", "cases", "right", "none", "wrong", "rms err", "right", "none", "wrong", "rms err");
        for (auto& g : order) { Stats& s = stats[g]; if (g.find("ungated") != string::npos) continue;
            printf("%-36s %6d | %6d %6d %6d %8.4f | %6d %6d %6d %8.4f\n", g.c_str(), s.cases, s.right[0], s.none[0], s.wrong[0], sqrt(s.sq[0] / std::max(s.right[0], 1)), s.right[1], s.none[1], s.wrong[1], sqrt(s.sq[1] / std::max(s.right[1], 1))); }
    }
    {
        ifstream in(dir + "/labels.txt");
        map<string, EyeCalibration> rig; vector<glm::dvec3> truth; set<int> found[2]; vector<BlobRay> rays; string line, name;
        struct Stats { int possible = 0, recovered[2] = {0, 0}, ghosts[2] = {0, 0}; };
        map<string, Stats> stats; vector<string> order;
        auto endFrame = [&]() { if (!name.empty()) { stats[name].possible += truth.size(); for (int k = 0; k < 2; k++) stats[name].recovered[k] += found[k].size(); } truth.clear(); found[0].clear(); found[1].clear(); };
        long solves = 0; double seconds[2] = {0, 0};
        while (getline(in, line)) {
            istringstream ls(line); string word; ls >> word;
            if (word == "rig") { endFrame(); name.clear(); string f; ls >> f; rig = loadEyeCalibrations(dir + "/" + f); }
            else if (word == "config") { ls >> name; order.push_back(name); }
            else if (word == "frame") endFrame();
            else if (word == "truth") { glm::dvec3 p; ls >> p.x >> p.y >> p.z; truth.push_back(p); }
            else if (word == "ray") { string h; double x, y; ls >> h >> x >> y; rays.push_back({0, rig[h].center, rig[h].ray(x, y), 0}); }
            else if (word == "index") {
                solves++;
                for (int k = 0; k < 2; k++) {
                    glm::dvec3 x; int n = 0;
                    auto t0 = std::chrono::steady_clock::now();
                    for (int rep = 0; rep < 200; rep++) n = solvers[k](rays.data(), (int) rays.size(), glm::radians(1.0), x);
                    seconds[k] += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / 200;
                    if (n > 0) {
                        double best = 1e9; int which = -1;
                        for (int i = 0; i < (int) truth.size(); i++) { double d = glm::length(x - truth[i]); if (d < best) { best = d; which = i; } }
                        if (best < 0.1) found[k].insert(which); else stats[name].ghosts[k]++;
                    }
                }
                rays.clear();
            }
        }
        endFrame();
        printf("\n%-34s %8s | %9s %7s %6s | %9s %7s %6s\n", "labels config", "true pts", "recovered", "percent", "ghosts", "recovered", "percent", "ghosts");
        for (auto& g : order) { Stats& s = stats[g];
            printf("%-34s %8d | %9d %6.1f%% %6d | %9d %6.1f%% %6d\n", g.c_str(), s.possible, s.recovered[0], 100.0 * s.recovered[0] / std::max(s.possible, 1), s.ghosts[0], s.recovered[1], 100.0 * s.recovered[1] / std::max(s.possible, 1), s.ghosts[1]); }
        printf("\nmean time per index group: header %.0f ns, prototype %.0f ns (%ld groups)\n", 1e9 * seconds[0] / solves, 1e9 * seconds[1] / solves, solves);
    }
}
