// Scratch: leave out the ray without which the rest fit best (smallest least-squares cost), in closed form.
#pragma once
struct RayIntersector2 {
    double a00 = 0, a01 = 0, a02 = 0, a11 = 0, a12 = 0, a22 = 0, b0 = 0, b1 = 0, b2 = 0, c0 = 0, wsum = 0;
    void add(const glm::dvec3& c, const glm::dvec3& d, double w = 1.0) {
        const double p00 = w * (1 - d.x * d.x), p01 = -w * d.x * d.y, p02 = -w * d.x * d.z;
        const double p11 = w * (1 - d.y * d.y), p12 = -w * d.y * d.z, p22 = w * (1 - d.z * d.z);
        a00 += p00; a01 += p01; a02 += p02; a11 += p11; a12 += p12; a22 += p22;
        const double q0 = p00 * c.x + p01 * c.y + p02 * c.z, q1 = p01 * c.x + p11 * c.y + p12 * c.z, q2 = p02 * c.x + p12 * c.y + p22 * c.z;
        b0 += q0; b1 += q1; b2 += q2;
        c0 += q0 * c.x + q1 * c.y + q2 * c.z;
        wsum += w;
    }
    bool solve(glm::dvec3& x) const {
        const double c00 = a11 * a22 - a12 * a12, c01 = a02 * a12 - a01 * a22, c02 = a01 * a12 - a02 * a11;
        const double det = a00 * c00 + a01 * c01 + a02 * c02;
        if (!(det > 1e-5 * wsum * wsum * wsum)) return false;
        const double c11 = a00 * a22 - a02 * a02, c12 = a01 * a02 - a00 * a12, c22 = a00 * a11 - a01 * a01;
        x = glm::dvec3(c00 * b0 + c01 * b1 + c02 * b2, c01 * b0 + c11 * b1 + c12 * b2, c02 * b0 + c12 * b1 + c22 * b2) / det;
        return true;
    }
    // the sum at the point solve() gave
    double cost(const glm::dvec3& x) const { return c0 - (b0 * x.x + b1 * x.y + b2 * x.z); }
};
static int gMode = 0;   // 0: weights of the joint fit, 1: all rays equal, 2: cost first, min-max among trials that agree
static int intersectRaysCost(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) {
    const double maxTan = tan(maxAngle);
    const double maxError = maxTan * maxTan;
    const double inf = std::numeric_limits<double>::infinity();
    int numRays = n;
    for (int i = 0; i < n; i++) rays[i].weight = 1.0;

    while (numRays >= 2 && numRays * 2 > n) {
        int worst;
        double worstError;
        if (!fitRaysC(rays, n, x, worst, worstError)) return 0;
        if (worstError <= maxError) return numRays;

        numRays--;
        if (numRays < 2 || numRays * 2 <= n) return 0;

        if (worstError < inf) {
            RayIntersector2 all;
            for (int i = 0; i < n; i++) {
                if (rays[i].weight > 0) all.add(rays[i].origin - x, rays[i].direction, gMode == 1 ? 1.0 : rays[i].weight);
            }
            double bestCost = inf;
            for (int i = 0; i < n; i++) {
                if (!(rays[i].weight > 0)) continue;
                RayIntersector2 rest = all;
                rest.add(rays[i].origin - x, rays[i].direction, gMode == 1 ? -1.0 : -rays[i].weight);
                glm::dvec3 y;
                if (!rest.solve(y)) continue;
                const double cost = rest.cost(y);
                if (cost < bestCost) {
                    worst = i;
                    bestCost = cost;
                }
            }
        }

        rays[worst].weight = 0;
    }

    return 0;
}
template <int M> static int intersectRaysCostM(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) { gMode = M; return intersectRaysCost(rays, n, maxAngle, x); }
