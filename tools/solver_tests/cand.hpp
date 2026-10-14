// Scratch: candidate for the header. Leave-one-out as a test for agreement, worst of the joint fit otherwise.
#pragma once
static int gLeaveOneOut = 12;   // most rays among which each one is tried

static bool fitRaysC(BlobRay* rays, int n, glm::dvec3& x, int& worst, double& worstError) {
    for (int pass = 0; pass < 2; pass++) {
        RayIntersector intersector;
        for (int i = 0; i < n; i++) {
            if (rays[i].weight > 0) intersector.add(rays[i].origin, rays[i].direction, pass == 0 ? 1.0 : rays[i].weight);
        }
        if (!intersector.solve(x)) return false;

        worst = -1;
        for (int i = 0; i < n; i++) {
            if (!(rays[i].weight > 0)) continue;
            const glm::dvec3 v = x - rays[i].origin;
            const double depth = glm::dot(v, rays[i].direction);
            double error = std::numeric_limits<double>::infinity(); // behind the eye
            if (depth > 1e-6) {
                error = glm::dot(v, v) / (depth * depth) - 1.0;
                rays[i].weight = 1.0 / (depth * depth);
            }
            if (worst == -1 || error > worstError) {
                worst = i;
                worstError = error;
            }
        }
        if (worstError == std::numeric_limits<double>::infinity()) break;
    }
    return true;
}

static int intersectRaysC(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) {
    const double maxTan = tan(maxAngle);
    const double maxError = maxTan * maxTan;
    int numRays = n;
    for (int i = 0; i < n; i++) rays[i].weight = 1.0;

    while (numRays >= 2 && numRays * 2 > n) {
        int worst;
        double worstError;
        if (!fitRaysC(rays, n, x, worst, worstError)) return 0;
        if (worstError <= maxError) return numRays;

        numRays--;
        if (numRays < 2 || numRays * 2 <= n) return 0;

        if (numRays < gLeaveOneOut) {
            int best = -1;
            double bestError = maxError;
            for (int i = 0; i < n; i++) {
                if (!(rays[i].weight > 0)) continue;
                rays[i].weight = 0;
                glm::dvec3 xi;
                int wi;
                double ei;
                if (fitRaysC(rays, n, xi, wi, ei) && ei <= bestError) {
                    best = i;
                    bestError = ei;
                }
                rays[i].weight = 1.0;
            }
            if (best != -1) worst = best;
        }

        rays[worst].weight = 0;
    }

    return 0;
}
template <int L> static int intersectRaysCL(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) { gLeaveOneOut = L; return intersectRaysC(rays, n, maxAngle, x); }
