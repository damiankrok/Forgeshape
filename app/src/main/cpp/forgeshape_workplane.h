// The three principal workplanes a sketch can be drawn on, and the one mapping
// between a sketch's 2D coordinates and a body's local 3D space.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer type and
// no camera type. A workplane is a fact about the BODY -- a sketch is authored
// in the body's local space, on one of its three principal planes -- and it
// stays true however the camera looks at it. Nothing here reads a pixel.
//
// The mapping is fixed, right-handed and stated once
// -------------------------------------------------
// Every plane names its U axis, its V axis and its NORMAL, and the three form
// a right-handed frame (U x V = N) so a profile that is counter-clockwise in
// (u, v) is counter-clockwise seen from +N -- which is what lets the extrusion
// produce canonical outward winding without asking which plane it is on.
//
//     XY   U = +X   V = +Y   N = +Z     a front view, from +Z
//     XZ   U = +X   V = -Z   N = +Y     a top view, from +Y
//     YZ   U = -Z   V = +Y   N = +X     a side view, from +X
//
// XZ's V runs along -Z and YZ's U along -Z so that, seen from the plane's
// positive normal with world +Y up, U runs to the RIGHT and V runs UP -- the
// same reading the ordinary front view has. A plane whose V ran along +Z would
// be a top view drawn upside down.
//
// The sketch origin is the body's local origin. Nothing is recentred, and the
// origin is the pivot every later tool inherits, exactly as it is for a
// primitive or an imported object.
#pragma once

#include <cstdint>

#include "forgeshape_math.h"

namespace forgeshape {

// The closed set of workplanes. Deliberately only the three principal planes:
// a face-based or offset plane is a different feature with its own approval.
enum class Workplane : uint8_t {
    XY,
    XZ,
    YZ,
};

constexpr int kWorkplaneCount = 3;

const char* workplaneName(Workplane plane);
bool workplaneFromIndex(int index, Workplane* out);
int workplaneIndex(Workplane plane);

// One sketch coordinate, in METRES on the plane. Double, like every other
// authored Construction length; the derived mesh is float.
struct SketchPoint {
    double u = 0.0;
    double v = 0.0;
};

// The plane's frame, as three unit LOCAL directions. Exact -- every component
// is 0 or +/-1 -- so no trigonometry and no rounding is involved in placing a
// sketch point in 3D.
struct WorkplaneFrame {
    Vec3 uAxis;
    Vec3 vAxis;
    Vec3 normal;
};

WorkplaneFrame workplaneFrame(Workplane plane);

// (u, v) on the plane -> body-local 3D. The inverse below drops the component
// along the normal, so a point that is not ON the plane maps to its projection.
Vec3 workplaneToLocal(Workplane plane, const SketchPoint& point);
SketchPoint localToWorkplane(Workplane plane, const Vec3& local);

// A point on the plane offset along its normal, for the far cap of an
// extrusion. `offset` is signed and in metres.
Vec3 workplaneToLocalAtOffset(Workplane plane, const SketchPoint& point, double offset);

}  // namespace forgeshape
