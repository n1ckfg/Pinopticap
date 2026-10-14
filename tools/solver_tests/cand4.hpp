// Scratch: leave-one-out with every trial refitted in two passes and scored by the sum of squared angles.
#pragma once
// fit of the rays with weight > 0, skipping one; does not touch the weights. total = sum of squared tangents, inf if behind an eye
static bool fitWithout(const BlobRay* rays, int n, int skip, glm::dvec3& x, double& total, double& worstError) {
    RayIntersector first;
    for (int i = 0; i < n; i++) if (i != skip && rays[i].weight > 0) first.add(rays[i].origin, rays[i].direction);
    if (!first.solve(x)) return false;
    RayIntersector second;
    for (int i = 0; i < n; i++) {
        if (i == skip || !(rays[i].weight > 0)) continue;
        const double depth = glm::dot(x - rays[i].origin, rays[i].direction);
        if (!(depth > 1e-6)) { total = worstError = std::numeric_limits<double>::infinity(); return true; }
        second.add(rays[i].origin, rays[i].direction, 1.0 / (depth * depth));
    }
    if (!second.solve(x)) return false;
    total = worstError = 0;
    for (int i = 0; i < n; i++) {
        if (i == skip || !(rays[i].weight > 0)) continue;
        const glm::dvec3 v = x - rays[i].origin;
        const double depth = glm::dot(v, rays[i].direction);
        if (!(depth > 1e-6)) { total = worstError = std::numeric_limits<double>::infinity(); return true; }
        const double error = glm::dot(v, v) / (depth * depth) - 1.0;
        total += error;
        worstError = std::max(worstError, error);
    }
    return true;
}
static int gScore = 0;      // 0: sum of squared angles, 1: worst angle
static int gLimit = 1 << 30; // most rays among which trials are refitted; above that, closed-form cost (V2)
static int intersectRaysA(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) {
    const double maxTan = tan(maxAngle);
    const double maxError = maxTan * maxTan;
    const double inf = std::numeric_limits<double>::infinity();
    int numRays = n;
    for (int i = 0; i < n; i++) rays[i].weight = 1.0;

    while (numRays >= 2 && numRays * 2 > n) {
        int worst;
        double worstError = 0;
        if (!fitRaysC(rays, n, x, worst, worstError)) return 0;
        if (worstError <= maxError) return numRays;

        numRays--;
        if (numRays < 2 || numRays * 2 <= n) return 0;

        double best = inf;
        if (numRays < gLimit) {
            for (int i = 0; i < n; i++) {
                if (!(rays[i].weight > 0)) continue;
                glm::dvec3 y;
                double total, w;
                if (!fitWithout(rays, n, i, y, total, w)) continue;
                const double score = gScore == 0 ? total : w;
                if (score < best) { worst = i; best = score; }
            }
        } else {
            RayIntersector2 all;
            for (int i = 0; i < n; i++) if (rays[i].weight > 0) all.add(rays[i].origin - x, rays[i].direction, rays[i].weight);
            for (int i = 0; i < n; i++) {
                if (!(rays[i].weight > 0)) continue;
                RayIntersector2 rest = all;
                rest.add(rays[i].origin - x, rays[i].direction, -rays[i].weight);
                glm::dvec3 y;
                if (!rest.solve(y)) continue;
                const double cost = rest.cost(y);
                if (cost < best && inFront(rays, n, i, x + y)) { worst = i; best = cost; }
            }
        }
        rays[worst].weight = 0;
    }
    return 0;
}
template <int SC, int L> static int intersectRaysAM(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) { gScore = SC; gLimit = L; return intersectRaysA(rays, n, maxAngle, x); }
