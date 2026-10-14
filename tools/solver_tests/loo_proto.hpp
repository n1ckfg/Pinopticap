// Scratch: the leave-one-out prototype from compare4.cpp.
#pragma once
static bool fitRaysProto(BlobRay* rays, int n, glm::dvec3& x, double& worstError) {
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
static int intersectRaysProto(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) {
    const double maxTan = tan(maxAngle), maxError = maxTan * maxTan;
    int numRays = n;
    for (int i = 0; i < n; i++) rays[i].weight = 1.0;
    while (numRays >= 2 && numRays * 2 > n) {
        double worst;
        if (!fitRaysProto(rays, n, x, worst)) return 0;
        if (worst <= maxError) return numRays;
        if (numRays - 1 < 2 || (numRays - 1) * 2 <= n) return 0;
        int best = -1; double bestError = std::numeric_limits<double>::infinity();
        for (int i = 0; i < n; i++) {
            if (!(rays[i].weight > 0)) continue;
            const double keep = rays[i].weight; rays[i].weight = 0;
            glm::dvec3 xi; double e;
            if (fitRaysProto(rays, n, xi, e) && e < bestError) { best = i; bestError = e; }
            rays[i].weight = keep;
        }
        if (best == -1) return 0;
        rays[best].weight = 0; numRays--;
    }
    return 0;
}
