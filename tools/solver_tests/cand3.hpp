// Scratch: cost-based leave-one-out, variants of how a point behind an eye is treated.
#pragma once
// 1: lines only   2: a trial must be in front of its rays, else joint worst   3: prefer trials in front, else best cost
static int gBehind = 2;
static bool inFront(const BlobRay* rays, int n, int skip, const glm::dvec3& p) {
    for (int i = 0; i < n; i++) {
        if (i != skip && rays[i].weight > 0 && !(glm::dot(p - rays[i].origin, rays[i].direction) > 1e-6)) return false;
    }
    return true;
}
static int intersectRaysV(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) {
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

        RayIntersector2 all;
        for (int i = 0; i < n; i++) {
            if (rays[i].weight > 0) all.add(rays[i].origin - x, rays[i].direction, rays[i].weight);
        }
        double bestCost = inf, bestAny = inf;
        int any = -1, front = -1;
        for (int i = 0; i < n; i++) {
            if (!(rays[i].weight > 0)) continue;
            RayIntersector2 rest = all;
            rest.add(rays[i].origin - x, rays[i].direction, -rays[i].weight);
            glm::dvec3 y;
            if (!rest.solve(y)) continue;
            const double cost = rest.cost(y);
            if (cost < bestAny) { any = i; bestAny = cost; }
            if (gBehind != 1 && cost < bestCost && inFront(rays, n, i, x + y)) { front = i; bestCost = cost; }
        }
        if (gBehind == 1) { if (any != -1) worst = any; }
        else if (gBehind == 2) { if (front != -1) worst = front; }
        else { if (front != -1) worst = front; else if (any != -1) worst = any; }

        rays[worst].weight = 0;
    }

    return 0;
}
template <int M> static int intersectRaysVM(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) { gBehind = M; return intersectRaysV(rays, n, maxAngle, x); }
