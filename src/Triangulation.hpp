#pragma once

#include "ofMain.h"
#include "ofxJSONElement.h"

// ~ ~ ~ CALIBRATION ~ ~ ~

// Where one camera is and how its lens maps the image, from an OpenSfM reconstruction.
struct EyeCalibration {
    glm::dvec3 center = glm::dvec3(0.0);       // camera position in the world
    glm::dmat3 camToWorld = glm::dmat3(1.0);   // camera axes in the world: x right, y down, z forward
    double scaleX = 1.0, scaleY = 1.0;         // image width and height in focal lengths
    double k1 = 0.0, k2 = 0.0;                 // radial distortion

    // unit direction, in the world, of the ray through an image point with x and y from 0 to 1
    glm::dvec3 ray(double x, double y) const {
        const double xd = (x - 0.5) * scaleX, yd = (y - 0.5) * scaleY;
        double xu = xd, yu = yd;
        if (k1 != 0.0 || k2 != 0.0) {
            // undo the distortion by fixed-point iteration
            for (int i = 0; i < 20; i++) {
                const double r2 = xu * xu + yu * yu;
                const double f = 1.0 + r2 * (k1 + r2 * k2);
                const double dx = xd / f - xu, dy = yd / f - yu;
                xu += dx;
                yu += dy;
                if (dx * dx + dy * dy < 1e-18) break;
            }
        }
        return glm::normalize(camToWorld * glm::dvec3(xu, yu, 1.0));
    }
};

// Reads the cameras of an OpenSfM reconstruction.json, by the host name of the node
// that took each shot. Shots are named <hostName>_<anything>.jpg
inline map<string, EyeCalibration> loadEyeCalibrations(const string& filename) {
    map<string, EyeCalibration> calibrations;

    try {
        ofxJSONElement json;
        if (!json.open(filename) || !json.isArray() || json.size() < 1) return calibrations;
        const Json::Value& reconstruction = json[0];
        const Json::Value& models = reconstruction["cameras"];
        const Json::Value& shots = reconstruction["shots"];

        for (const string& shotName : shots.getMemberNames()) {
            const Json::Value& shot = shots[shotName];
            const Json::Value& rotation = shot["rotation"];
            const Json::Value& translation = shot["translation"];
            const Json::Value& model = models[shot["camera"].asString()];

            bool valid = rotation.isArray() && rotation.size() == 3 && translation.isArray() && translation.size() == 3;
            valid = valid && model.isObject() && model.get("projection_type", "perspective").asString() == "perspective";
            valid = valid && model["focal"].asDouble() > 0 && model["width"].asDouble() > 0 && model["height"].asDouble() > 0;
            if (!valid) {
                cout << "Calibration: skipping " << shotName << ", need a perspective camera with rotation and translation" << endl;
                continue;
            }

            // world to camera is p = R * X + t, the rotation being stored as axis * angle
            glm::dvec3 r(rotation[0].asDouble(), rotation[1].asDouble(), rotation[2].asDouble());
            glm::dvec3 t(translation[0].asDouble(), translation[1].asDouble(), translation[2].asDouble());
            double angle = glm::length(r);
            glm::dmat3 R(1.0);
            if (angle > 1e-12) R = glm::mat3_cast(glm::angleAxis(angle, r / angle));

            // focal is in units of the longer side of the image
            double width = model["width"].asDouble(), height = model["height"].asDouble();
            double focal = model["focal"].asDouble() * max(width, height);

            EyeCalibration calibration;
            calibration.camToWorld = glm::transpose(R);
            calibration.center = -(calibration.camToWorld * t);
            calibration.scaleX = width / focal;
            calibration.scaleY = height / focal;
            calibration.k1 = model.get("k1", 0.0).asDouble();
            calibration.k2 = model.get("k2", 0.0).asDouble();

            calibrations[shotName.substr(0, shotName.find('_'))] = calibration;
        }
    } catch (std::exception& e) {
        cout << "Exception: " << e.what() << endl;
    }

    return calibrations;
}

// ~ ~ ~ TRIANGULATION ~ ~ ~

// Least-squares intersection of any number of rays: the point X with the smallest
// sum of w * (distance from X to the ray)^2, from one symmetric 3x3 system.
struct RayIntersector {
    double a00 = 0, a01 = 0, a02 = 0, a11 = 0, a12 = 0, a22 = 0, b0 = 0, b1 = 0, b2 = 0, wsum = 0;

    // d must be unit length
    void add(const glm::dvec3& c, const glm::dvec3& d, double w = 1.0) {
        const double p00 = w * (1 - d.x * d.x), p01 = -w * d.x * d.y, p02 = -w * d.x * d.z;
        const double p11 = w * (1 - d.y * d.y), p12 = -w * d.y * d.z, p22 = w * (1 - d.z * d.z);
        a00 += p00; a01 += p01; a02 += p02; a11 += p11; a12 += p12; a22 += p22;
        b0 += p00 * c.x + p01 * c.y + p02 * c.z;
        b1 += p01 * c.x + p11 * c.y + p12 * c.z;
        b2 += p02 * c.x + p12 * c.y + p22 * c.z;
        wsum += w;
    }

    // false if the rays are too close to parallel to meet anywhere
    bool solve(glm::dvec3& x) const {
        const double c00 = a11 * a22 - a12 * a12, c01 = a02 * a12 - a01 * a22, c02 = a01 * a12 - a02 * a11;
        const double det = a00 * c00 + a01 * c01 + a02 * c02;
        if (!(det > 1e-5 * wsum * wsum * wsum)) return false;
        const double c11 = a00 * a22 - a02 * a02, c12 = a01 * a02 - a00 * a12, c22 = a00 * a11 - a01 * a01;
        x = glm::dvec3(c00 * b0 + c01 * b1 + c02 * b2, c01 * b0 + c11 * b1 + c12 * b2, c02 * b0 + c12 * b1 + c22 * b2) / det;
        return true;
    }
};

// One sighting of a blob, as a ray from the eye that saw it.
struct BlobRay {
    int index;              // blob index it was reported under
    glm::dvec3 origin;
    glm::dvec3 direction;   // unit length
    double weight;          // set by intersectRays, 0 if the ray was left out
};

// The point nearest to the rays in play, those with a weight, other than ray skip.
// The rays count equally at first and then by 1 / depth^2, so that near and far eyes
// count the same per pixel of error. A ray's error is the squared tangent of the angle
// by which it misses x, infinite if x is behind its eye: worst is the ray with the
// largest error and total the sum over the rays.
// False if the rays are too close to parallel to meet anywhere.
inline bool fitRays(const BlobRay* rays, int n, int skip, glm::dvec3& x, int& worst, double& worstError, double& total) {
    const double behind = std::numeric_limits<double>::infinity();

    RayIntersector equal;
    for (int i = 0; i < n; i++) {
        if (i != skip && rays[i].weight > 0) equal.add(rays[i].origin, rays[i].direction);
    }
    if (!equal.solve(x)) return false;

    RayIntersector weighted;
    for (int i = 0; i < n; i++) {
        if (i == skip || !(rays[i].weight > 0)) continue;
        const double depth = glm::dot(x - rays[i].origin, rays[i].direction);
        if (!(depth > 1e-6)) {
            worst = i;
            worstError = total = behind;
            return true;
        }
        weighted.add(rays[i].origin, rays[i].direction, 1.0 / (depth * depth));
    }
    if (!weighted.solve(x)) return false;

    worst = -1;
    total = 0;
    for (int i = 0; i < n; i++) {
        if (i == skip || !(rays[i].weight > 0)) continue;
        const glm::dvec3 v = x - rays[i].origin;
        const double depth = glm::dot(v, rays[i].direction);
        // squared tangent of the angle between the ray and the point
        const double error = (depth > 1e-6) ? glm::dot(v, v) / (depth * depth) - 1.0 : behind;
        total += error;
        if (worst == -1 || error > worstError) {
            worst = i;
            worstError = error;
        }
    }

    return true;
}

// The point nearest to n rays. As long as a ray passes the point at more than maxAngle
// (radians), one ray is left out: the one without which the rest fit best, found by
// trying each. Among more than maxLeaveOneOut rays that would take too long, and the
// ray that misses the point by most goes instead.
// Returns how many rays met at x, or 0 unless at least two and more than half did.
inline int intersectRays(BlobRay* rays, int n, double maxAngle, glm::dvec3& x) {
    const int maxLeaveOneOut = 16;
    const double maxTan = tan(maxAngle);
    const double maxError = maxTan * maxTan;
    int numRays = n;
    for (int i = 0; i < n; i++) rays[i].weight = 1.0;

    for (;;) {
        int worst;
        double worstError, total;
        if (!fitRays(rays, n, -1, x, worst, worstError, total)) return 0;
        if (worstError <= maxError) return numRays;

        const int numLeft = numRays - 1;
        if (numLeft < 2 || numLeft * 2 <= n) return 0;

        if (numRays <= maxLeaveOneOut) {
            double bestTotal = std::numeric_limits<double>::infinity();
            for (int i = 0; i < n; i++) {
                if (!(rays[i].weight > 0)) continue;
                glm::dvec3 xi;
                int worstI;
                double worstErrorI, totalI;
                if (fitRays(rays, n, i, xi, worstI, worstErrorI, totalI) && totalI < bestTotal) {
                    worst = i;
                    bestTotal = totalI;
                }
            }
        }

        rays[worst].weight = 0;
        numRays = numLeft;
    }
}
