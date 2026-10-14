// Scratch test of Pinopticap/src/Triangulation.hpp against vectors made by gen_vectors.py.
#include "Triangulation.hpp"
#include <fstream>
#include <sstream>

int main(int, char** argv) {
    const string dir = argv[1];
    ifstream in(dir + "/vectors.txt");
    map<string, map<string, EyeCalibration>> rigs;
    string line, group;
    map<string, EyeCalibration>* rig = nullptr;
    vector<BlobRay> rays;
    int failures = 0;

    struct Stats { int cases = 0, bad = 0, none = 0, rays = 0, used = 0; double sqRef = 0, worstRef = 0, sqTruth = 0; };
    map<string, Stats> stats;
    vector<string> order;

    while (getline(in, line)) {
        istringstream ls(line);
        string word;
        ls >> word;
        if (word == "rig") {
            string file; ls >> file;
            if (!rigs.count(file)) {
                rigs[file] = loadEyeCalibrations(dir + "/" + file);
                cout << "loaded " << rigs[file].size() << " eyes from " << file << ":";
                int shown = 0;
                for (auto& c : rigs[file]) if (shown++ < 3) cout << " " << c.first;
                cout << " ..." << endl;
            }
            rig = &rigs[file];
        } else if (word == "center") {       // center <host> x y z : expected camera position
            string host; glm::dvec3 c; ls >> host >> c.x >> c.y >> c.z;
            if (!rig->count(host) || glm::length((*rig)[host].center - c) > 1e-9) { cout << "FAIL center " << host << endl; failures++; }
        } else if (word == "group") {
            ls >> group;
            if (!stats.count(group)) order.push_back(group);
        } else if (word == "ray") {          // ray <host> x y
            string host; double x, y; ls >> host >> x >> y;
            if (!rig->count(host)) { cout << "FAIL unknown host " << host << endl; failures++; continue; }
            BlobRay r; r.index = 0; r.origin = (*rig)[host].center; r.direction = (*rig)[host].ray(x, y);
            rays.push_back(r);
        } else if (word == "expect") {       // expect <maxAngleDeg> <numRays or -1> <tol> rx ry rz tx ty tz
            double deg, tol; int want; glm::dvec3 ref, truth;
            ls >> deg >> want >> tol >> ref.x >> ref.y >> ref.z >> truth.x >> truth.y >> truth.z;
            glm::dvec3 x(0.0);
            int got = intersectRays(rays.data(), (int) rays.size(), glm::radians(deg), x);
            Stats& s = stats[group];
            s.cases++; s.rays += rays.size(); s.used += got;
            bool ok = (want < 0 || got == want);
            if (got == 0) s.none++;
            if (got > 0 && want != 0) {
                double e = glm::length(x - ref);
                s.sqRef += e * e; s.worstRef = max(s.worstRef, e);
                s.sqTruth += glm::dot(x - truth, x - truth);
                if (tol >= 0 && !(e <= tol)) ok = false;
            }
            if (!ok) { s.bad++; failures++; if (s.bad <= 3) cout << "FAIL " << group << ": got " << got << " rays (want " << want << "), |x - ref| = " << glm::length(x - ref) << endl; }
            rays.clear();
        }
    }

    printf("\n%-34s %6s %5s %9s %9s %11s %11s %s\n", "group", "cases", "none", "rays in", "rays used", "max |x-ref|", "rms |x-truth|", "failed");
    for (auto& g : order) {
        Stats& s = stats[g];
        int solved = max(s.cases - s.none, 1);
        printf("%-34s %6d %5d %9d %9d %11.3g %11.3g %d\n", g.c_str(), s.cases, s.none, s.rays, s.used, s.worstRef, sqrt(s.sqTruth / solved), s.bad);
    }

    // hand-made degenerate cases
    auto check = [&](const char* name, bool ok) { printf("%-34s %s\n", name, ok ? "ok" : "FAIL"); if (!ok) failures++; };
    glm::dvec3 x;
    {
        BlobRay r[2] = {{0, glm::dvec3(0, 0, 0), glm::dvec3(0, 0, 1), 0}, {0, glm::dvec3(1, 0, 0), glm::dvec3(0, 0, 1), 0}};
        check("two parallel rays -> no point", intersectRays(r, 2, glm::radians(1.0), x) == 0);
    }
    {
        BlobRay r[1] = {{0, glm::dvec3(0, 0, 0), glm::dvec3(0, 0, 1), 0}};
        check("one ray -> no point", intersectRays(r, 1, glm::radians(1.0), x) == 0);
        check("no rays -> no point", intersectRays(r, 0, glm::radians(1.0), x) == 0);
    }
    {
        // three eyes looking at (0, 0, 5), a fourth looking directly away from it
        glm::dvec3 p(0, 0, 5), c[4] = {{-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        BlobRay r[4];
        for (int i = 0; i < 4; i++) r[i] = {0, c[i], glm::normalize(p - c[i]), 0};
        r[3].direction = -r[3].direction;
        int n = intersectRays(r, 4, glm::radians(1.0), x);
        check("point behind one eye -> eye dropped", n == 3 && glm::length(x - p) < 1e-9 && r[3].weight == 0);
    }
    {
        // exact rays to one point, at distances from 1 to 1000
        glm::dvec3 p(0.3, -0.2, 0.1);
        vector<BlobRay> r;
        for (int i = 0; i < 40; i++) {
            double a = i * 2.39996, d = pow(1000.0, i / 39.0);
            glm::dvec3 c = p + d * glm::dvec3(cos(a), 0.3 * sin(3 * a), sin(a));
            r.push_back({0, c, glm::normalize(p - c), 0});
        }
        int n = intersectRays(r.data(), 40, glm::radians(1.0), x);
        check("40 eyes at 1..1000 units", n == 40 && glm::length(x - p) < 1e-7);
    }

    printf("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
