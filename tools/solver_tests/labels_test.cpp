// Scratch: how index-grouped triangulation behaves when blob indices are / are not consistent.
#include "Triangulation.hpp"
#include <fstream>
#include <sstream>
int main(int, char** argv) {
    const string dir = argv[1];
    ifstream in(dir + "/labels.txt");
    map<string, EyeCalibration> rig;
    vector<glm::dvec3> truth;
    vector<BlobRay> rays;
    string line, name;
    struct Stats { int groups = 0, blobs = 0, real = 0, ghosts = 0, majBlobs = 0, majReal = 0, majGhosts = 0; };
    map<string, Stats> stats; vector<string> order;
    while (getline(in, line)) {
        istringstream ls(line); string word; ls >> word;
        if (word == "rig") { string f; ls >> f; rig = loadEyeCalibrations(dir + "/" + f); }
        else if (word == "config") { ls >> name; if (!stats.count(name)) order.push_back(name); }
        else if (word == "truth") { glm::dvec3 p; ls >> p.x >> p.y >> p.z; truth.push_back(p); }
        else if (word == "ray") { string h; double x, y; ls >> h >> x >> y; rays.push_back({0, rig[h].center, rig[h].ray(x, y), 0}); }
        else if (word == "index") {          // end of one blob index within a frame
            if (rays.size() >= 2) {
                Stats& s = stats[name]; s.groups++;
                glm::dvec3 x; int n = intersectRays(rays.data(), (int) rays.size(), glm::radians(1.0), x);
                if (n > 0) {
                    double best = 1e9; for (auto& t : truth) best = min(best, glm::length(x - t));
                    bool real = best < 0.03, majority = 2 * n > (int) rays.size();
                    s.blobs++; (real ? s.real : s.ghosts)++;
                    if (majority) { s.majBlobs++; (real ? s.majReal : s.majGhosts)++; }
                }
            }
            rays.clear();
        } else if (word == "frame") truth.clear();
    }
    printf("%-34s %7s | %6s %6s %6s | %6s %6s %6s\n", "", "", "any 2+", "", "", "majority", "", "");
    printf("%-34s %7s | %6s %6s %6s | %6s %6s %6s\n", "config", "indices", "blobs", "real", "ghost", "blobs", "real", "ghost");
    for (auto& g : order) { Stats& s = stats[g]; printf("%-34s %7d | %6d %6d %6d | %6d %6d %6d\n", g.c_str(), s.groups, s.blobs, s.real, s.ghosts, s.majBlobs, s.majReal, s.majGhosts); }
}
