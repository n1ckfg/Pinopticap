// Accuracy of the header's solver (and of the first leave-one-out prototype) on the real four-camera rig,
// with the error split into along and across the cameras' mean viewing direction.
#include "Triangulation.hpp"
#include <fstream>
#include <sstream>
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
        int best = -1; double bestError = std::numeric_limits<double>::infinity();
        for (int i = 0; i < n; i++) {
            if (!(rays[i].weight > 0)) continue;
            const double keep = rays[i].weight; rays[i].weight = 0;
            glm::dvec3 xi; double e;
            if (fitRays(rays, n, xi, e) && e < bestError) { best = i; bestError = e; }
            rays[i].weight = keep;
        }
        if (best == -1) return 0;
        rays[best].weight = 0; numRays--;
    }
    return 0;
}
int main(int argc, char** argv) {
    const string dir = argv[1]; const double gate = argc > 2 ? atof(argv[2]) : 1.0, wrongAt = 0.5;
    ifstream in(dir + "/cases.txt");
    map<string, EyeCalibration> rig; glm::dvec3 axis(0, 0, 1); string line, group; vector<BlobRay> rays;
    struct Stats { int cases = 0, none[2] = {0, 0}, wrong[2] = {0, 0}, right[2] = {0, 0}, used[2] = {0, 0}; double along[2] = {0, 0}, across[2] = {0, 0}, worst[2] = {0, 0}, depth = 0; };
    map<string, Stats> stats; vector<string> order;
    while (getline(in, line)) {
        istringstream ls(line); string word; ls >> word;
        if (word == "rig") { string f; ls >> f; rig = loadEyeCalibrations(dir + "/" + f); printf("calibration: %d eyes\n", (int) rig.size()); }
        else if (word == "axis") ls >> axis.x >> axis.y >> axis.z;
        else if (word == "group") { ls >> group; if (!stats.count(group)) order.push_back(group); }
        else if (word == "ray") { string h; double x, y; ls >> h >> x >> y; rays.push_back({0, rig[h].center, rig[h].ray(x, y), 0}); }
        else if (word == "truth") {
            glm::dvec3 truth; ls >> truth.x >> truth.y >> truth.z;
            Stats& s = stats[group]; s.cases++; s.depth += glm::dot(truth - rays[0].origin, axis);
            for (int k = 0; k < 2; k++) {
                glm::dvec3 x(0.0);
                int n = k ? intersectRaysLOO(rays.data(), (int) rays.size(), glm::radians(gate), x) : intersectRays(rays.data(), (int) rays.size(), glm::radians(gate), x);
                if (n == 0) { s.none[k]++; continue; }
                const glm::dvec3 e = x - truth; const double a = glm::dot(e, axis), total = glm::length(e);
                if (total > wrongAt) { s.wrong[k]++; continue; }
                s.right[k]++; s.used[k] += n; s.along[k] += a * a; s.across[k] += total * total - a * a; s.worst[k] = std::max(s.worst[k], total);
            }
            rays.clear();
        }
    }
    printf("gate %.2f deg; a result more than %.1f units from the truth counts as wrong; errors are rms over the right ones\n", gate, wrongAt);
    printf("%-30s %5s %6s | %5s %5s %5s %5s %8s %8s %8s | %5s %5s %5s\n", "", "", "", "header", "", "", "", "", "", "", "prototype", "", "");
    printf("%-30s %5s %6s | %5s %5s %5s %5s %8s %8s %8s | %5s %5s %5s\n", "group", "cases", "depth", "right", "none", "wrong", "rays", "across", "along", "max", "right", "none", "wrong");
    for (auto& g : order) { Stats& s = stats[g]; int r = std::max(s.right[0], 1);
        printf("%-30s %5d %6.2f | %5d %5d %5d %5.2f %8.4f %8.4f %8.4f | %5d %5d %5d\n", g.c_str(), s.cases, s.depth / s.cases, s.right[0], s.none[0], s.wrong[0], (double) s.used[0] / r, sqrt(s.across[0] / r), sqrt(s.along[0] / r), s.worst[0], s.right[1], s.none[1], s.wrong[1]); }
}
