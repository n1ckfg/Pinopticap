// Scratch: what index-grouped triangulation yields, per frame, against the true scene points.
#include "Triangulation.hpp"
#include <fstream>
#include <sstream>
#include <set>
int main(int, char** argv) {
    const string dir = argv[1];
    ifstream in(dir + "/labels.txt");
    map<string, EyeCalibration> rig;
    vector<glm::dvec3> truth; set<int> found;
    vector<BlobRay> rays;
    string line, name;
    struct Stats { int possible = 0, recovered = 0, blobs = 0, ghosts = 0; double sq = 0, worst = 0; };
    map<string, Stats> stats; vector<string> order;
    auto endFrame = [&]() { if (!name.empty()) { stats[name].possible += truth.size(); stats[name].recovered += found.size(); } truth.clear(); found.clear(); };
    while (getline(in, line)) {
        istringstream ls(line); string word; ls >> word;
        if (word == "rig") { endFrame(); name.clear(); string f; ls >> f; rig = loadEyeCalibrations(dir + "/" + f); }
        else if (word == "config") { ls >> name; order.push_back(name); }
        else if (word == "frame") endFrame();
        else if (word == "truth") { glm::dvec3 p; ls >> p.x >> p.y >> p.z; truth.push_back(p); }
        else if (word == "ray") { string h; double x, y; ls >> h >> x >> y; rays.push_back({0, rig[h].center, rig[h].ray(x, y), 0}); }
        else if (word == "index") {
            glm::dvec3 x;
            if (intersectRays(rays.data(), (int) rays.size(), glm::radians(1.0), x) > 0) {
                Stats& s = stats[name];
                double best = 1e9; int which = -1;
                for (int i = 0; i < (int) truth.size(); i++) { double d = glm::length(x - truth[i]); if (d < best) { best = d; which = i; } }
                s.blobs++;
                if (best < 0.1) { found.insert(which); s.sq += best * best; s.worst = std::max(s.worst, best); } else s.ghosts++;
            }
            rays.clear();
        }
    }
    endFrame();
    printf("%-34s %9s %9s %7s %7s %9s %9s\n", "config", "true pts", "recovered", "percent", "ghosts", "rms err", "max err");
    for (auto& g : order) { Stats& s = stats[g]; int real = std::max(s.blobs - s.ghosts, 1);
        printf("%-34s %9d %9d %6.1f%% %7d %9.4f %9.4f\n", g.c_str(), s.possible, s.recovered, 100.0 * s.recovered / std::max(s.possible, 1), s.ghosts, sqrt(s.sq / real), s.worst); }
}
