#include "forgeshape_workplane.h"

namespace forgeshape {

const char* workplaneName(Workplane plane) {
    switch (plane) {
        case Workplane::XY: return "XY";
        case Workplane::XZ: return "XZ";
        case Workplane::YZ: return "YZ";
    }
    return "unknown";
}

bool workplaneFromIndex(int index, Workplane* out) {
    if (out == nullptr) {
        return false;
    }
    switch (index) {
        case 0: *out = Workplane::XY; return true;
        case 1: *out = Workplane::XZ; return true;
        case 2: *out = Workplane::YZ; return true;
        default: return false;  // refused, never clamped
    }
}

int workplaneIndex(Workplane plane) { return static_cast<int>(plane); }

WorkplaneFrame workplaneFrame(Workplane plane) {
    // See the header for why XZ's V and YZ's U run along -Z: read from the
    // plane's positive normal with +Y up, U must run right and V must run up.
    switch (plane) {
        case Workplane::XZ:
            return WorkplaneFrame{Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, -1.0f},
                                  Vec3{0.0f, 1.0f, 0.0f}};
        case Workplane::YZ:
            return WorkplaneFrame{Vec3{0.0f, 0.0f, -1.0f}, Vec3{0.0f, 1.0f, 0.0f},
                                  Vec3{1.0f, 0.0f, 0.0f}};
        case Workplane::XY:
        default:
            return WorkplaneFrame{Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f},
                                  Vec3{0.0f, 0.0f, 1.0f}};
    }
}

Vec3 workplaneToLocalAtOffset(Workplane plane, const SketchPoint& point, double offset) {
    // Written per plane in double and rounded once, rather than as a float
    // basis multiply, so a coordinate that is exact in double stays as exact as
    // a float can hold it: no 0 * u + 1 * v arithmetic can introduce a bit.
    const float u = static_cast<float>(point.u);
    const float v = static_cast<float>(point.v);
    const float n = static_cast<float>(offset);
    switch (plane) {
        case Workplane::XZ: return Vec3{u, n, -v};
        case Workplane::YZ: return Vec3{n, v, -u};
        case Workplane::XY:
        default: return Vec3{u, v, n};
    }
}

Vec3 workplaneToLocal(Workplane plane, const SketchPoint& point) {
    return workplaneToLocalAtOffset(plane, point, 0.0);
}

SketchPoint localToWorkplane(Workplane plane, const Vec3& local) {
    SketchPoint point;
    switch (plane) {
        case Workplane::XZ:
            point.u = local.x;
            point.v = -local.z;
            break;
        case Workplane::YZ:
            point.u = -local.z;
            point.v = local.y;
            break;
        case Workplane::XY:
        default:
            point.u = local.x;
            point.v = local.y;
            break;
    }
    return point;
}

}  // namespace forgeshape
