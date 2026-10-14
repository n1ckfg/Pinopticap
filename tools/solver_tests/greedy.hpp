// Scratch: the solver as it was before leave-one-out, renamed.
#pragma once
inline int intersectRaysGreedy(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) {
    const double maxTan = tan(maxAngle);
    const double maxError = maxTan * maxTan;
    int numRays = n;
    for (int i = 0; i < n; i++) rays[i].weight = 1.0;

    for (bool weighted = false; numRays >= 2 && numRays * 2 > n; weighted = true) {
        RayIntersector intersector;
        for (int i = 0; i < n; i++) {
            if (rays[i].weight > 0) intersector.add(rays[i].origin, rays[i].direction, rays[i].weight);
        }
        if (!intersector.solve(x)) return 0;

        int worst = -1;
        double worstError = maxError;
        for (int i = 0; i < n; i++) {
            if (!(rays[i].weight > 0)) continue;
            const glm::dvec3 v = x - rays[i].origin;
            const double depth = glm::dot(v, rays[i].direction);
            double error = std::numeric_limits<double>::infinity(); // behind the eye
            if (depth > 1e-6) {
                // squared tangent of the angle between the ray and the point
                error = glm::dot(v, v) / (depth * depth) - 1.0;
                rays[i].weight = 1.0 / (depth * depth);
            }
            if (error > worstError) {
                worst = i;
                worstError = error;
            }
        }

        if (worst != -1) {
            rays[worst].weight = 0;
            numRays--;
        } else if (weighted) {
            return numRays;
        }
    }

    return 0;
}
