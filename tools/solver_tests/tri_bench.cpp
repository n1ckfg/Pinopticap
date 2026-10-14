// Benchmark + accuracy comparison for N-view triangulation candidates.
// It has its own copy of the solver and does not use src/Triangulation.hpp.
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>
#include <glm/glm.hpp>
#include <opencv2/core.hpp>

using glm::dvec3;
using glm::dmat3;

// ---- proposed core ---------------------------------------------------------
// Least-squares intersection of N rays: minimises sum w_i |(I - d_i d_i^T)(X - c_i)|^2.
struct RayIntersector {
    double a00 = 0, a01 = 0, a02 = 0, a11 = 0, a12 = 0, a22 = 0, b0 = 0, b1 = 0, b2 = 0, wsum = 0;

    void add(const dvec3& c, const dvec3& d, double w = 1.0) { // d must be unit length
        const double p00 = w * (1 - d.x * d.x), p01 = -w * d.x * d.y, p02 = -w * d.x * d.z;
        const double p11 = w * (1 - d.y * d.y), p12 = -w * d.y * d.z, p22 = w * (1 - d.z * d.z);
        a00 += p00; a01 += p01; a02 += p02; a11 += p11; a12 += p12; a22 += p22;
        b0 += p00 * c.x + p01 * c.y + p02 * c.z;
        b1 += p01 * c.x + p11 * c.y + p12 * c.z;
        b2 += p02 * c.x + p12 * c.y + p22 * c.z;
        wsum += w;
    }

    bool solve(dvec3& x) const {
        const double c00 = a11 * a22 - a12 * a12, c01 = a02 * a12 - a01 * a22, c02 = a01 * a12 - a02 * a11;
        const double det = a00 * c00 + a01 * c01 + a02 * c02;
        if (det <= 1e-5 * wsum * wsum * wsum) return false; // rays (nearly) parallel
        const double c11 = a00 * a22 - a02 * a02, c12 = a01 * a02 - a00 * a12, c22 = a00 * a11 - a01 * a01;
        x = dvec3(c00 * b0 + c01 * b1 + c02 * b2, c01 * b0 + c11 * b1 + c12 * b2, c02 * b0 + c12 * b1 + c22 * b2) / det;
        return true;
    }
};

struct Cam {
    dmat3 R;      // world -> camera (glm is column-major: R * X)
    dvec3 c;      // centre
    dmat3 M;      // pixel (u, v, 1) -> world ray direction (unnormalised) = R^T K^-1
    double f, cx, cy;
    cv::Matx34d P; // K [R | t], for the DLT baseline
};

struct Obs { int cam; double u, v; };

static inline dvec3 rayDir(const Cam& cam, const Obs& o) {
    return glm::normalize(cam.M * dvec3(o.u, o.v, 1.0));
}

static bool midpoint(const std::vector<Cam>& cams, const Obs* obs, int n, int reweight, dvec3& x) {
    dvec3 d[64];
    RayIntersector s;
    for (int i = 0; i < n; i++) { d[i] = rayDir(cams[obs[i].cam], obs[i]); s.add(cams[obs[i].cam].c, d[i]); }
    if (!s.solve(x)) return false;
    for (int pass = 0; pass < reweight; pass++) {
        RayIntersector r;
        for (int i = 0; i < n; i++) {
            const dvec3& c = cams[obs[i].cam].c;
            const double depth = glm::dot(x - c, d[i]);
            if (depth > 1e-6) r.add(c, d[i], 1.0 / (depth * depth));
        }
        if (!r.solve(x)) return false;
    }
    return true;
}

// Gauss-Newton on squared reprojection error, the statistically optimal cost.
static void refine(const std::vector<Cam>& cams, const Obs* obs, int n, int iters, dvec3& x) {
    for (int it = 0; it < iters; it++) {
        double h00 = 0, h01 = 0, h02 = 0, h11 = 0, h12 = 0, h22 = 0, g0 = 0, g1 = 0, g2 = 0;
        for (int i = 0; i < n; i++) {
            const Cam& cam = cams[obs[i].cam];
            const dvec3 p = cam.R * (x - cam.c);
            const double iz = 1.0 / p.z, fx = cam.f * iz;
            const double ru = cam.cx + fx * p.x - obs[i].u, rv = cam.cy + fx * p.y - obs[i].v;
            const dvec3 r0 = glm::row(cam.R, 0), r1 = glm::row(cam.R, 1), r2 = glm::row(cam.R, 2);
            const dvec3 ju = fx * (r0 - (p.x * iz) * r2), jv = fx * (r1 - (p.y * iz) * r2);
            h00 += ju.x * ju.x + jv.x * jv.x; h01 += ju.x * ju.y + jv.x * jv.y; h02 += ju.x * ju.z + jv.x * jv.z;
            h11 += ju.y * ju.y + jv.y * jv.y; h12 += ju.y * ju.z + jv.y * jv.z; h22 += ju.z * ju.z + jv.z * jv.z;
            g0 += ju.x * ru + jv.x * rv; g1 += ju.y * ru + jv.y * rv; g2 += ju.z * ru + jv.z * rv;
        }
        const double c00 = h11 * h22 - h12 * h12, c01 = h02 * h12 - h01 * h22, c02 = h01 * h12 - h02 * h11;
        const double det = h00 * c00 + h01 * c01 + h02 * c02;
        if (!(det > 0)) return;
        const double c11 = h00 * h22 - h02 * h02, c12 = h01 * h02 - h00 * h12, c22 = h00 * h11 - h01 * h01;
        x -= dvec3(c00 * g0 + c01 * g1 + c02 * g2, c01 * g0 + c11 * g1 + c12 * g2, c02 * g0 + c12 * g1 + c22 * g2) / det;
    }
}

// Classic linear (DLT) N-view triangulation via SVD, as cv::sfm::triangulatePoints does.
static bool dlt(const std::vector<Cam>& cams, const Obs* obs, int n, dvec3& x) {
    cv::Mat A(2 * n, 4, CV_64F);
    for (int i = 0; i < n; i++) {
        const cv::Matx34d& P = cams[obs[i].cam].P;
        for (int k = 0; k < 4; k++) {
            A.at<double>(2 * i, k) = obs[i].u * P(2, k) - P(0, k);
            A.at<double>(2 * i + 1, k) = obs[i].v * P(2, k) - P(1, k);
        }
    }
    cv::Mat h;
    cv::SVD::solveZ(A, h);
    const double w = h.at<double>(3);
    if (std::abs(w) < 1e-12) return false;
    x = dvec3(h.at<double>(0), h.at<double>(1), h.at<double>(2)) / w;
    return true;
}

int main() {
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    std::normal_distribution<double> gauss(0.0, 1.0);
    const double W = 640, H = 480, F = 0.85 * 640, noisePx = 1.0;
    const int M = 20000; // points per configuration

    std::printf("doubles, 1 px noise, cameras 1.8-4.8 from a unit cube\n");
    std::printf("%5s | %28s | %45s\n", "", "time per 3D point (ns)", "3D error rms (mm, if 1 unit = 1 m)");
    std::printf("%5s | %8s %8s %8s | %8s %8s %8s %8s %8s\n", "views", "midpoint", "+1 rewt", "DLT/SVD", "midpoint", "+1 rewt", "+2 rewt", "rewt+GN", "DLT/SVD");

    for (int n : {2, 3, 4, 6, 8, 16, 32}) {
        // rig: n cameras at mixed distances, all looking near the origin
        std::vector<Cam> cams(n);
        for (auto& cam : cams) {
            const double dist = 3.0 * (0.6 + uni(rng)), az = 6.283185307 * uni(rng), el = (-20 + 90 * uni(rng)) * 0.0174532925;
            cam.c = dist * dvec3(std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az));
            const dvec3 fwd = glm::normalize(dvec3(0.2 * (uni(rng) - 0.5)) - cam.c);
            const dvec3 right = glm::normalize(glm::cross(fwd, dvec3(0, 1, 0))), down = glm::cross(fwd, right);
            cam.R = glm::transpose(dmat3(right, down, fwd)); // rows = right, down, forward
            cam.f = F; cam.cx = W / 2; cam.cy = H / 2;
            const dmat3 Kinv(dvec3(1 / F, 0, 0), dvec3(0, 1 / F, 0), dvec3(-cam.cx / F, -cam.cy / F, 1));
            cam.M = glm::transpose(cam.R) * Kinv;
            const dvec3 t = -(cam.R * cam.c);
            for (int r = 0; r < 3; r++) {
                const double kf = (r < 2) ? F : 1.0, kc = (r == 0) ? cam.cx : (r == 1) ? cam.cy : 0.0;
                for (int k = 0; k < 3; k++) cam.P(r, k) = kf * cam.R[k][r] + (r < 2 ? kc * cam.R[k][2] : 0.0);
                cam.P(r, 3) = kf * t[r] + (r < 2 ? kc * t[2] : 0.0);
            }
        }
        // observations of M random points, every camera sees every point
        std::vector<dvec3> truth(M);
        std::vector<Obs> obs((size_t)M * n);
        for (int j = 0; j < M; j++) {
            truth[j] = dvec3(uni(rng) - 0.5, uni(rng) - 0.5, uni(rng) - 0.5);
            for (int i = 0; i < n; i++) {
                const dvec3 p = cams[i].R * (truth[j] - cams[i].c);
                obs[(size_t)j * n + i] = {i, cams[i].cx + F * p.x / p.z + noisePx * gauss(rng), cams[i].cy + F * p.y / p.z + noisePx * gauss(rng)};
            }
        }

        auto run = [&](auto&& fn, double& nsPerPoint, double& rmsMm) {
            double sq = 0; int ok = 0; dvec3 x;
            for (int j = 0; j < M; j++) if (fn(&obs[(size_t)j * n], x)) { const dvec3 e = x - truth[j]; sq += glm::dot(e, e); ok++; }
            rmsMm = 1000.0 * std::sqrt(sq / std::max(ok, 1));
            const int reps = 20; double sink = 0;
            const auto t0 = std::chrono::steady_clock::now();
            for (int r = 0; r < reps; r++) for (int j = 0; j < M; j++) if (fn(&obs[(size_t)j * n], x)) sink += x.x;
            const auto t1 = std::chrono::steady_clock::now();
            nsPerPoint = std::chrono::duration<double, std::nano>(t1 - t0).count() / ((double)reps * M);
            if (sink == 12345.678) std::puts("");
        };

        double tMid, eMid, tRw, eRw, tRw2, eRw2, tGn, eGn, tDlt, eDlt, tMg, eMg;
        run([&](const Obs* o, dvec3& x) { return midpoint(cams, o, n, 0, x); }, tMid, eMid);
        run([&](const Obs* o, dvec3& x) { return midpoint(cams, o, n, 1, x); }, tRw, eRw);
        run([&](const Obs* o, dvec3& x) { return midpoint(cams, o, n, 2, x); }, tRw2, eRw2);
        run([&](const Obs* o, dvec3& x) { if (!midpoint(cams, o, n, 1, x)) return false; refine(cams, o, n, 3, x); return true; }, tGn, eGn);
        run([&](const Obs* o, dvec3& x) { return dlt(cams, o, n, x); }, tDlt, eDlt);
        run([&](const Obs* o, dvec3& x) { if (!midpoint(cams, o, n, 0, x)) return false; refine(cams, o, n, 3, x); return true; }, tMg, eMg);
        std::printf("%5d | %8.0f %8.0f %8.0f | %8.2f %8.2f %8.2f %8.2f %8.2f   [check: plain midpoint + 3 GN steps -> %.3f mm vs rewt+GN %.3f vs +1 rewt %.3f; GN path %.0f ns]\n", n, tMid, tRw, tDlt, eMid, eRw, eRw2, eGn, eDlt, eMg, eGn, eRw, tGn);
    }
    return 0;
}
