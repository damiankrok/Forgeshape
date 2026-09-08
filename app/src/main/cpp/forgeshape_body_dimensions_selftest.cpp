#include "forgeshape_body_dimensions_selftest.h"

#include <cmath>
#include <limits>
#include <vector>

#include "forgeshape_body_dimension_overlay.h"
#include "forgeshape_body_dimensions.h"
#include "forgeshape_construction.h"
#include "forgeshape_history.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

struct Recorder {
    BodyDimensionsSelfTestResult* out;
    int max;
    int n = 0;

    void check(const char* name, bool ok) {
        if (n < max) {
            out[n].name = name;
            out[n].passed = ok;
            ++n;
        }
    }
};

// The tolerance every geometric assertion in this suite is stated at.
//
// The anchor correction carries the body's orientation through
// `rotationMatrixFromEuler`, whose matrix is float by contract because the
// renderer and the picker consume it. So a world point recomposed from that
// same float matrix agrees to about a part in 10^7 of its magnitude, not to a
// double ULP -- "exact within the current transform tolerance", which is what
// DIM020M-07 asks for and what an honest assertion says.
constexpr double kWorldEpsilon = 1e-5;
// Scale and dimension arithmetic is pure double: target / extent and
// old * multiplier round once each.
constexpr double kExactEpsilon = 1e-12;

bool near(double a, double b, double epsilon) { return std::fabs(a - b) <= epsilon; }

// Where a LOCAL point sits in the world under one placement.
//
// Written here from the stated convention `Model = T * Rz * Ry * Rx * S`
// rather than borrowed from the solver, so the anchor cases are checked
// against the contract instead of against the code under test.
void worldPointOf(const TransformValues& v, double lx, double ly, double lz, double* out) {
    const Mat4 rotation = rotationMatrixFromEuler(eulerOf(v));
    const double sx = lx * v.scaleX;
    const double sy = ly * v.scaleY;
    const double sz = lz * v.scaleZ;
    for (int row = 0; row < 3; ++row) {
        out[row] = static_cast<double>(rotation.m[0 * 4 + row]) * sx
                 + static_cast<double>(rotation.m[1 * 4 + row]) * sy
                 + static_cast<double>(rotation.m[2 * 4 + row]) * sz;
    }
    out[0] += v.positionX;
    out[1] += v.positionY;
    out[2] += v.positionZ;
}

bool samePosition(const TransformValues& a, const TransformValues& b) {
    return a.positionX == b.positionX && a.positionY == b.positionY && a.positionZ == b.positionZ;
}

bool sameScale(const TransformValues& a, const TransformValues& b) {
    return a.scaleX == b.scaleX && a.scaleY == b.scaleY && a.scaleZ == b.scaleZ;
}

bool sameValues(const TransformValues& a, const TransformValues& b) {
    return samePosition(a, b) && sameScale(a, b) && a.rotationX == b.rotationX
        && a.rotationY == b.rotationY && a.rotationZ == b.rotationZ;
}

// The suite's own scene and history. One body, a deliberately non-cubic box, so
// a wrong axis is visible in the numbers themselves.
struct Fixture {
    ConstructionScene scene;
    ConstructionHistory history{scene};

    Fixture() { publishConstructionObject(scene.activeBody().construction(),
                                          scene.activeBody().meshStore()); }

    SceneObject& body() { return scene.activeBody(); }
    ObjectId id() { return scene.activeBody().objectId(); }

    void setShape(const PrimitiveSpec& spec) {
        ScopedConstructionEdit edit(history);
        applyPrimitive(body().construction(), body().meshStore(), spec);
    }

    void setPlacement(const TransformValues& values) {
        ScopedConstructionEdit edit(history);
        applyTransformValues(body().transform(), values);
    }

    TransformValues placement() { return body().transform().values(); }
};

TransformValues turned(double rx, double ry, double rz) {
    TransformValues v;
    v.rotationX = rx;
    v.rotationY = ry;
    v.rotationZ = rz;
    return v;
}

}  // namespace

int runBodyDimensionsSelfTests(BodyDimensionsSelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};

    // -----------------------------------------------------------------------
    // DIM020M-01  dimension = local extent x absolute scale
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues v = f.placement();
        v.scaleX = 3.0;
        v.scaleY = 4.0;
        v.scaleZ = 0.5;
        f.setPlacement(v);

        LocalBounds bounds;
        BodyDimensions dims;
        const bool read = sceneBodyDimensions(f.id(), f.scene, &bounds, &dims);
        r.check("DIM020M-01 box dimensions read", read);
        r.check("DIM020M-01 unscaled extents are the box parameters",
                near(bounds.extent(0), 2.0, kExactEpsilon)
                    && near(bounds.extent(1), 1.0, kExactEpsilon)
                    && near(bounds.extent(2), 0.5, kExactEpsilon));
        r.check("DIM020M-01 dimension is extent times absolute scale",
                near(dims.x, 6.0, kExactEpsilon) && near(dims.y, 4.0, kExactEpsilon)
                    && near(dims.z, 0.25, kExactEpsilon));

        // Every primitive, so no generator's contract is assumed rather than
        // read: a cylinder's X and Z are its diameter, a capsule's Y is its
        // TOTAL height, a plane's Y is exactly zero.
        f.setShape(PrimitiveSpec::forCylinder(1.5, 4.0));
        f.setPlacement(TransformValues{});
        sceneBodyDimensions(f.id(), f.scene, &bounds, &dims);
        r.check("DIM020M-01 cylinder is diameter, height, diameter",
                near(dims.x, 1.5, kExactEpsilon) && near(dims.y, 4.0, kExactEpsilon)
                    && near(dims.z, 1.5, kExactEpsilon));

        f.setShape(PrimitiveSpec::forSphere(2.5));
        sceneBodyDimensions(f.id(), f.scene, &bounds, &dims);
        r.check("DIM020M-01 sphere is its diameter on all three",
                near(dims.x, 2.5, kExactEpsilon) && near(dims.y, 2.5, kExactEpsilon)
                    && near(dims.z, 2.5, kExactEpsilon));

        f.setShape(PrimitiveSpec::forCone(3.0, 2.0));
        sceneBodyDimensions(f.id(), f.scene, &bounds, &dims);
        r.check("DIM020M-01 cone is bottom diameter and height",
                near(dims.x, 3.0, kExactEpsilon) && near(dims.y, 2.0, kExactEpsilon)
                    && near(dims.z, 3.0, kExactEpsilon));

        f.setShape(PrimitiveSpec::forCapsule(1.0, 3.0));
        sceneBodyDimensions(f.id(), f.scene, &bounds, &dims);
        r.check("DIM020M-01 capsule Y is the TOTAL height",
                near(dims.x, 1.0, kExactEpsilon) && near(dims.y, 3.0, kExactEpsilon)
                    && near(dims.z, 1.0, kExactEpsilon));

        f.setShape(PrimitiveSpec::forPlane(4.0, 2.0));
        sceneBodyDimensions(f.id(), f.scene, &bounds, &dims);
        r.check("DIM020M-01 plane reports a truthful ZERO thickness",
                near(dims.x, 4.0, kExactEpsilon) && dims.y == 0.0
                    && near(dims.z, 2.0, kExactEpsilon));
    }

    // -----------------------------------------------------------------------
    // DIM020M-02  rotation does not change reported local dimensions
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        BodyDimensions before;
        sceneBodyDimensions(f.id(), f.scene, nullptr, &before);

        // Three awkward angles at once, so no axis is accidentally aligned and
        // a world-AABB implementation could not pass.
        TransformValues v = turned(31.0, -47.5, 118.25);
        v.positionX = 5.0;
        v.positionY = -2.0;
        v.positionZ = 9.5;
        f.setPlacement(v);

        BodyDimensions after;
        sceneBodyDimensions(f.id(), f.scene, nullptr, &after);
        r.check("DIM020M-02 rotation and position leave the dimensions untouched",
                after.x == before.x && after.y == before.y && after.z == before.z);
        r.check("DIM020M-02 the dimensions are still the box parameters",
                near(after.x, 2.0, kExactEpsilon) && near(after.y, 1.0, kExactEpsilon)
                    && near(after.z, 0.5, kExactEpsilon));
    }

    // -----------------------------------------------------------------------
    // DIM020M-03  an exact dimension updates ONLY the intended scale axis, and
    //             Center writes no position
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues start = f.placement();
        start.positionX = 1.5;
        start.positionY = -0.25;
        start.positionZ = 3.0;
        start.scaleX = 2.0;
        start.scaleY = 3.0;
        start.scaleZ = 4.0;
        f.setPlacement(start);
        start = f.placement();

        const BodySizeResult result = applyBodyDimension(f.id(), 0, 10.0, ResizeAnchor::Center,
                                                         f.scene, f.history);
        r.check("DIM020M-03 exact dimension applied", result.status == BodySizeStatus::Ok);
        const TransformValues now = f.placement();
        // 10 m across a 2 m box is a scale of 5.
        r.check("DIM020M-03 the edited axis takes target / extent",
                near(now.scaleX, 5.0, kExactEpsilon));
        r.check("DIM020M-03 the other two scale axes are untouched",
                now.scaleY == start.scaleY && now.scaleZ == start.scaleZ);
        r.check("DIM020M-03 rotation is untouched",
                now.rotationX == start.rotationX && now.rotationY == start.rotationY
                    && now.rotationZ == start.rotationZ);
        BodyDimensions dims;
        sceneBodyDimensions(f.id(), f.scene, nullptr, &dims);
        r.check("DIM020M-03 the body now reads the requested dimension",
                near(dims.x, 10.0, kExactEpsilon));
        r.check("DIM020M-03 the other two dimensions are unchanged",
                near(dims.y, 3.0, kExactEpsilon) && near(dims.z, 2.0, kExactEpsilon));
    }

    // -----------------------------------------------------------------------
    // DIM020M-06  Center anchor leaves Position unchanged
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues start = turned(20.0, 35.0, -10.0);
        start.positionX = -3.25;
        start.positionY = 7.0;
        start.positionZ = 0.5;
        f.setPlacement(start);
        start = f.placement();

        for (int axis = 0; axis < kBodyAxisCount; ++axis) {
            applyBodyDimension(f.id(), axis, 6.0, ResizeAnchor::Center, f.scene, f.history);
        }
        const TransformValues now = f.placement();
        r.check("DIM020M-06 Center writes no position, on any axis",
                samePosition(now, start));
        r.check("DIM020M-06 Center still resized all three axes",
                now.scaleX != start.scaleX && now.scaleY != start.scaleY
                    && now.scaleZ != start.scaleZ);
    }

    // -----------------------------------------------------------------------
    // DIM020M-04 / -05 / -07  one-sided anchors hold a world point still, for
    //                         an unturned body and for a turned one
    // -----------------------------------------------------------------------
    {
        struct Case {
            const char* label;
            TransformValues placement;
        };
        Fixture unturnedFixture;
        unturnedFixture.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));

        std::vector<Case> cases;
        {
            TransformValues v;
            v.positionX = 1.0;
            v.positionY = 2.0;
            v.positionZ = -3.0;
            v.scaleX = 1.5;
            v.scaleY = 2.0;
            v.scaleZ = 0.75;
            cases.push_back(Case{"unturned", v});
        }
        {
            // DIM020M-07: three non-zero Euler components, so a solver that
            // added the correction along a WORLD axis instead of the body's own
            // could not pass.
            TransformValues v = turned(23.0, -61.0, 142.0);
            v.positionX = -4.5;
            v.positionY = 0.75;
            v.positionZ = 6.25;
            v.scaleX = 0.8;
            v.scaleY = 1.6;
            v.scaleZ = 3.2;
            cases.push_back(Case{"turned", v});
        }

        for (const Case& c : cases) {
            for (int axis = 0; axis < kBodyAxisCount; ++axis) {
                Fixture f;
                f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
                f.setPlacement(c.placement);
                LocalBounds bounds;
                sceneBodyDimensions(f.id(), f.scene, &bounds, nullptr);
                const TransformValues before = f.placement();

                // Negative side.
                {
                    double lx = axis == 0 ? bounds.min(0) : 0.0;
                    double ly = axis == 1 ? bounds.min(1) : 0.0;
                    double lz = axis == 2 ? bounds.min(2) : 0.0;
                    double anchorBefore[3];
                    worldPointOf(before, lx, ly, lz, anchorBefore);
                    const AxisResizeSolution s = solveAxisDimension(
                        before, bounds, axis, 7.0, ResizeAnchor::NegativeSide);
                    double anchorAfter[3];
                    worldPointOf(s.values, lx, ly, lz, anchorAfter);
                    const bool held = s.status == ResizeStatus::Ok
                                   && near(anchorBefore[0], anchorAfter[0], kWorldEpsilon)
                                   && near(anchorBefore[1], anchorAfter[1], kWorldEpsilon)
                                   && near(anchorBefore[2], anchorAfter[2], kWorldEpsilon);
                    r.check(c.placement.rotationY == 0.0
                                ? "DIM020M-04 negative-side world point stationary (unturned)"
                                : "DIM020M-04/07 negative-side world point stationary (turned)",
                            held);
                    // The opposite face is the one that MOVED: a resize that
                    // held both would not be a resize.
                    double farBefore[3];
                    double farAfter[3];
                    const double fx = axis == 0 ? bounds.max(0) : 0.0;
                    const double fy = axis == 1 ? bounds.max(1) : 0.0;
                    const double fz = axis == 2 ? bounds.max(2) : 0.0;
                    worldPointOf(before, fx, fy, fz, farBefore);
                    worldPointOf(s.values, fx, fy, fz, farAfter);
                    const double moved = std::fabs(farBefore[0] - farAfter[0])
                                       + std::fabs(farBefore[1] - farAfter[1])
                                       + std::fabs(farBefore[2] - farAfter[2]);
                    r.check("DIM020M-04 the opposite face is the one that moved", moved > 1e-6);
                }

                // Positive side.
                {
                    double lx = axis == 0 ? bounds.max(0) : 0.0;
                    double ly = axis == 1 ? bounds.max(1) : 0.0;
                    double lz = axis == 2 ? bounds.max(2) : 0.0;
                    double anchorBefore[3];
                    worldPointOf(before, lx, ly, lz, anchorBefore);
                    const AxisResizeSolution s = solveAxisDimension(
                        before, bounds, axis, 0.4, ResizeAnchor::PositiveSide);
                    double anchorAfter[3];
                    worldPointOf(s.values, lx, ly, lz, anchorAfter);
                    r.check(c.placement.rotationY == 0.0
                                ? "DIM020M-05 positive-side world point stationary (unturned)"
                                : "DIM020M-05/07 positive-side world point stationary (turned)",
                            s.status == ResizeStatus::Ok
                                && near(anchorBefore[0], anchorAfter[0], kWorldEpsilon)
                                && near(anchorBefore[1], anchorAfter[1], kWorldEpsilon)
                                && near(anchorBefore[2], anchorAfter[2], kWorldEpsilon));
                }

                // Whatever the anchor, the resulting SIZE is the requested one
                // and the other two axes are untouched.
                {
                    const AxisResizeSolution s = solveAxisDimension(
                        before, bounds, axis, 7.0, ResizeAnchor::NegativeSide);
                    const BodyDimensions dims = bodyDimensionsOf(bounds, s.values);
                    r.check("DIM020M-04/05 a one-sided resize still reaches the target size",
                            near(dims.axis(axis), 7.0, kExactEpsilon));
                    bool othersHeld = true;
                    for (int other = 0; other < kBodyAxisCount; ++other) {
                        if (other == axis) continue;
                        othersHeld = othersHeld
                            && near(dims.axis(other),
                                    bodyDimensionsOf(bounds, before).axis(other), kExactEpsilon);
                    }
                    r.check("DIM020M-04/05 a one-sided resize touches no other axis", othersHeld);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // DIM020M-07 (continued)  a NON-CENTRED bounds is anchored correctly too
    // -----------------------------------------------------------------------
    //
    // Every Construction Source is centred on its local origin today, so this
    // case is stated against the SOLVER rather than against a body: it is the
    // property Stage 020D will need and the reason LocalBounds carries min and
    // max instead of a half-extent.
    {
        LocalBounds offset;
        offset.minX = 1.0;
        offset.maxX = 4.0;  // extent 3, entirely on the positive side
        offset.minY = -0.5;
        offset.maxY = 0.5;
        offset.minZ = -2.0;
        offset.maxZ = 1.0;  // extent 3, centre at -0.5
        TransformValues start = turned(15.0, 40.0, -25.0);
        start.positionX = 2.0;
        start.positionY = -1.0;
        start.positionZ = 0.5;
        start.scaleX = 1.25;
        start.scaleY = 1.0;
        start.scaleZ = 2.0;

        bool held = true;
        for (int axis = 0; axis < kBodyAxisCount; ++axis) {
            for (int side = 0; side < 2; ++side) {
                const ResizeAnchor anchor =
                    side == 0 ? ResizeAnchor::NegativeSide : ResizeAnchor::PositiveSide;
                const double b = side == 0 ? offset.min(axis) : offset.max(axis);
                double lx = axis == 0 ? b : 0.0;
                double ly = axis == 1 ? b : 0.0;
                double lz = axis == 2 ? b : 0.0;
                double beforePoint[3];
                double afterPoint[3];
                worldPointOf(start, lx, ly, lz, beforePoint);
                const AxisResizeSolution s =
                    solveAxisDimension(start, offset, axis, 5.5, anchor);
                worldPointOf(s.values, lx, ly, lz, afterPoint);
                held = held && s.status == ResizeStatus::Ok
                    && near(beforePoint[0], afterPoint[0], kWorldEpsilon)
                    && near(beforePoint[1], afterPoint[1], kWorldEpsilon)
                    && near(beforePoint[2], afterPoint[2], kWorldEpsilon);
            }
        }
        r.check("DIM020M-07 anchors hold for NON-CENTRED bounds on a turned body", held);

        const AxisResizeSolution centre =
            solveAxisDimension(start, offset, 0, 5.5, ResizeAnchor::Center);
        r.check("DIM020M-07 Center still writes no position for non-centred bounds",
                centre.status == ResizeStatus::Ok && samePosition(centre.values, start));
    }

    // -----------------------------------------------------------------------
    // DIM020M-08  invalid targets refuse, non-mutating
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues start = turned(10.0, 20.0, 30.0);
        start.positionX = 1.0;
        start.scaleX = 2.0;
        f.setPlacement(start);
        start = f.placement();
        const size_t depthBefore = f.history.undoDepth();

        const double bad[] = {0.0, -1.0, -0.0001,
                              std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity()};
        bool allRefused = true;
        bool nothingMoved = true;
        for (double value : bad) {
            const BodySizeResult result =
                applyBodyDimension(f.id(), 0, value, ResizeAnchor::Center, f.scene, f.history);
            allRefused = allRefused && result.status == BodySizeStatus::RefusedGeometry;
            nothingMoved = nothingMoved && sameValues(f.placement(), start);
        }
        r.check("DIM020M-08 zero, negative, NaN and infinity are all refused", allRefused);
        r.check("DIM020M-08 a refused target changes nothing at all", nothingMoved);
        r.check("DIM020M-08 a refused target records no history step",
                f.history.undoDepth() == depthBefore);

        const BodySizeResult badAxis =
            applyBodyDimension(f.id(), 7, 3.0, ResizeAnchor::Center, f.scene, f.history);
        r.check("DIM020M-08 an out-of-range axis is refused by name",
                badAxis.status == BodySizeStatus::RefusedGeometry
                    && badAxis.resize == ResizeStatus::InvalidAxis);

        // A relative multiplier is held to the same rule, and fails closed on
        // all three at once.
        const BodySizeResult badMultiplier = applyBodyRelativeScale(
            f.id(), 2.0, 0.0, 3.0, f.scene, f.history);
        r.check("DIM020M-08 one bad multiplier refuses all three",
                badMultiplier.status == BodySizeStatus::RefusedGeometry
                    && badMultiplier.resize == ResizeStatus::NotPositive
                    && sameValues(f.placement(), start));
    }

    // -----------------------------------------------------------------------
    // DIM020M-09  a degenerate local axis refuses resize
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forPlane(4.0, 2.0));
        const TransformValues start = f.placement();
        const size_t depthBefore = f.history.undoDepth();

        LocalBounds bounds;
        BodyDimensions dims;
        sceneBodyDimensions(f.id(), f.scene, &bounds, &dims);
        r.check("DIM020M-09 the plane's local Y extent is exactly zero",
                bounds.extent(1) == 0.0 && dims.y == 0.0);

        const BodySizeResult result =
            applyBodyDimension(f.id(), 1, 1.0, ResizeAnchor::Center, f.scene, f.history);
        r.check("DIM020M-09 resizing a zero-thickness axis is refused by name",
                result.status == BodySizeStatus::RefusedGeometry
                    && result.resize == ResizeStatus::DegenerateAxis);
        r.check("DIM020M-09 the refusal fabricates no thickness",
                sameValues(f.placement(), start) && f.history.undoDepth() == depthBefore);

        // The plane's two REAL axes still resize normally: a degenerate axis is
        // refused, not a degenerate body.
        const BodySizeResult wide =
            applyBodyDimension(f.id(), 0, 8.0, ResizeAnchor::Center, f.scene, f.history);
        r.check("DIM020M-09 the plane's own width still resizes",
                wide.status == BodySizeStatus::Ok && near(f.placement().scaleX, 2.0,
                                                          kExactEpsilon));
    }

    // -----------------------------------------------------------------------
    // DIM020M-10  one dimension commit is one history step, Undo/Redo exact
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues start = turned(12.0, -33.0, 71.0);
        start.positionX = 0.5;
        start.positionY = 1.5;
        start.positionZ = -2.5;
        start.scaleX = 1.25;
        f.setPlacement(start);
        start = f.placement();
        const size_t depthBefore = f.history.undoDepth();

        const BodySizeResult result = applyBodyDimension(f.id(), 0, 9.0,
                                                         ResizeAnchor::NegativeSide, f.scene,
                                                         f.history);
        r.check("DIM020M-10 the edit applied", result.status == BodySizeStatus::Ok);
        r.check("DIM020M-10 one edit is exactly one step",
                f.history.undoDepth() == depthBefore + 1);
        const TransformValues edited = f.placement();
        r.check("DIM020M-10 a one-sided edit moved BOTH scale and position",
                edited.scaleX != start.scaleX && !samePosition(edited, start));

        r.check("DIM020M-10 undo succeeded", f.history.undo());
        r.check("DIM020M-10 undo restores Position and Absolute Scale exactly",
                sameValues(f.placement(), start));
        r.check("DIM020M-10 redo succeeded", f.history.redo());
        r.check("DIM020M-10 redo reapplies Position and Absolute Scale exactly",
                sameValues(f.placement(), edited));

        // A commit that finds nothing different records nothing.
        const size_t depthAfter = f.history.undoDepth();
        const BodySizeResult again = applyBodyDimension(f.id(), 0, 9.0,
                                                        ResizeAnchor::NegativeSide, f.scene,
                                                        f.history);
        r.check("DIM020M-10 re-applying the same dimension is Unchanged",
                again.status == BodySizeStatus::Unchanged
                    && f.history.undoDepth() == depthAfter);
    }

    // -----------------------------------------------------------------------
    // DIM020M-11 / -13  Relative Scale opens at 1/1/1, every time
    // -----------------------------------------------------------------------
    //
    // The identity is a DOMAIN constant rather than a remembered value, and
    // nothing anywhere stores a multiplier: there is no accessor to read one
    // back from, which is the strongest form the "resets on every activation"
    // rule can take. What is asserted is exactly that -- applying the identity
    // is a no-op, and the constant is one.
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues start = f.placement();
        start.scaleX = 6.64;
        start.scaleY = 2.0;
        start.scaleZ = 0.5;
        f.setPlacement(start);
        start = f.placement();

        r.check("DIM020M-11 the Relative Scale identity is one",
                kRelativeScaleIdentity == 1.0);
        const RelativeScaleSolution identity = solveRelativeScale(
            start, kRelativeScaleIdentity, kRelativeScaleIdentity, kRelativeScaleIdentity);
        r.check("DIM020M-11 committing the opening multiplier changes nothing",
                identity.status == ResizeStatus::Ok && sameValues(identity.values, start));

        const size_t depthBefore = f.history.undoDepth();
        const BodySizeResult applied = applyBodyRelativeScale(f.id(), 1.0, 1.0, 1.0, f.scene,
                                                              f.history);
        r.check("DIM020M-11 an identity Apply is Unchanged and records nothing",
                applied.status == BodySizeStatus::Unchanged
                    && f.history.undoDepth() == depthBefore);

        // DIM020M-13: after a real commit, the NEXT interaction opens at the
        // identity again -- so applying the identity to the committed state is
        // still a no-op, and the committed absolute scale is what stands.
        applyBodyRelativeScale(f.id(), 2.0, 1.0, 1.0, f.scene, f.history);
        const TransformValues committed = f.placement();
        const RelativeScaleSolution reopened = solveRelativeScale(
            committed, kRelativeScaleIdentity, kRelativeScaleIdentity, kRelativeScaleIdentity);
        r.check("DIM020M-13 reopening starts from the identity, not from the last multiplier",
                reopened.status == ResizeStatus::Ok && sameValues(reopened.values, committed));
        r.check("DIM020M-13 a second identity Apply is still Unchanged",
                applyBodyRelativeScale(f.id(), 1.0, 1.0, 1.0, f.scene, f.history).status
                    == BodySizeStatus::Unchanged);
    }

    // -----------------------------------------------------------------------
    // DIM020M-12  Relative Scale multiplies the stored Absolute Scale exactly
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues start = turned(5.0, 55.0, -5.0);
        start.positionX = 3.0;
        start.positionY = -4.0;
        start.positionZ = 1.0;
        start.scaleX = 6.64;
        start.scaleY = 1.5;
        start.scaleZ = 0.25;
        f.setPlacement(start);
        start = f.placement();

        const BodySizeResult result =
            applyBodyRelativeScale(f.id(), 2.0, 0.5, 4.0, f.scene, f.history);
        const TransformValues now = f.placement();
        r.check("DIM020M-12 the Apply succeeded", result.status == BodySizeStatus::Ok);
        // The OWNER's own example: 6.64 x 2 = 13.28.
        r.check("DIM020M-12 the stored Absolute Scale is the product, per axis",
                near(now.scaleX, 13.28, kExactEpsilon)
                    && near(now.scaleY, 0.75, kExactEpsilon)
                    && near(now.scaleZ, 1.0, kExactEpsilon));
        r.check("DIM020M-12 Relative Scale is pivot-based and moves no position",
                samePosition(now, start));
        r.check("DIM020M-12 rotation is untouched",
                now.rotationX == start.rotationX && now.rotationY == start.rotationY
                    && now.rotationZ == start.rotationZ);

        BodyDimensions dims;
        LocalBounds bounds;
        sceneBodyDimensions(f.id(), f.scene, &bounds, &dims);
        r.check("DIM020M-12 the dimensions follow the new absolute scale",
                near(dims.x, 2.0 * 13.28, kExactEpsilon));
    }

    // -----------------------------------------------------------------------
    // DIM020M-14  one Relative Scale Apply is one history step, Undo/Redo exact
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues start = f.placement();
        start.scaleX = 3.0;
        start.scaleY = 3.0;
        start.scaleZ = 3.0;
        start.positionX = 2.0;
        f.setPlacement(start);
        start = f.placement();
        const size_t depthBefore = f.history.undoDepth();

        applyBodyRelativeScale(f.id(), 2.0, 3.0, 0.5, f.scene, f.history);
        const TransformValues edited = f.placement();
        r.check("DIM020M-14 one Apply is exactly one step",
                f.history.undoDepth() == depthBefore + 1);
        r.check("DIM020M-14 undo restores the previous Absolute Scale exactly",
                f.history.undo() && sameValues(f.placement(), start));
        r.check("DIM020M-14 redo reapplies it exactly",
                f.history.redo() && sameValues(f.placement(), edited));
    }

    // -----------------------------------------------------------------------
    // DIM020M-15  a locked Construction body refuses both acts; so do the
    //             representations this stage does not cover, and a hidden body
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues start = f.placement();
        start.scaleX = 2.0;
        f.setPlacement(start);
        start = f.placement();

        f.body().setLocked(true);
        const size_t depthBefore = f.history.undoDepth();
        const BodySizeResult dimension =
            applyBodyDimension(f.id(), 0, 8.0, ResizeAnchor::Center, f.scene, f.history);
        const BodySizeResult relative =
            applyBodyRelativeScale(f.id(), 2.0, 2.0, 2.0, f.scene, f.history);
        r.check("DIM020M-15 a locked body refuses an exact dimension edit",
                dimension.status == BodySizeStatus::RefusedLocked);
        r.check("DIM020M-15 a locked body refuses a Relative Scale Apply",
                relative.status == BodySizeStatus::RefusedLocked);
        r.check("DIM020M-15 neither refusal mutates or records",
                sameValues(f.placement(), start) && f.history.undoDepth() == depthBefore);
        r.check("DIM020M-15 the controls are withdrawn for a locked body",
                !bodySizeEditable(f.body()));
        f.body().setLocked(false);
        r.check("DIM020M-15 unlocking restores the act",
                applyBodyDimension(f.id(), 0, 8.0, ResizeAnchor::Center, f.scene, f.history).status
                    == BodySizeStatus::Ok);

        // Hidden: refused without touching the visibility the user set.
        f.body().setVisible(false);
        const TransformValues hiddenStart = f.placement();
        const BodySizeResult hidden =
            applyBodyDimension(f.id(), 1, 4.0, ResizeAnchor::Center, f.scene, f.history);
        r.check("DIM020M-15 a hidden body refuses by its own name",
                hidden.status == BodySizeStatus::RefusedHidden);
        r.check("DIM020M-15 the refusal leaves the body hidden and unmoved",
                !f.body().visible() && sameValues(f.placement(), hiddenStart));
        r.check("DIM020M-15 the controls are withdrawn for a hidden body",
                !bodySizeEditable(f.body()));
        f.body().setVisible(true);

        // An unknown body, and a representation this stage does not cover.
        r.check("DIM020M-15 an unknown body is refused by name",
                applyBodyDimension(kNoObject, 0, 1.0, ResizeAnchor::Center, f.scene, f.history)
                        .status == BodySizeStatus::UnknownBody);
        const std::vector<float> positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
        const std::vector<float> normals{0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
        const std::vector<uint32_t> indices{0, 1, 2};
        const std::vector<ImportedMeshBatch> batches{ImportedMeshBatch{0, 3, false}};
        SceneObject* imported = f.scene.addImportedBody(
            ImportedMesh::build(positions, normals, indices, batches), "imported");
        r.check("DIM020M-15 the imported fixture body was created", imported != nullptr);
        if (imported != nullptr) {
            const ObjectId importedId = imported->objectId();
            r.check("DIM020M-15 an Imported Mesh has no readable dimension in this stage",
                    !sceneBodyDimensions(importedId, f.scene, nullptr, nullptr));
            r.check("DIM020M-15 an Imported Mesh refuses by representation",
                    applyBodyDimension(importedId, 0, 2.0, ResizeAnchor::Center, f.scene,
                                       f.history)
                            .status == BodySizeStatus::RefusedRepresentation);
            r.check("DIM020M-15 the controls are absent for an Imported Mesh",
                    !bodySizeEditable(*imported));
        }
    }

    // -----------------------------------------------------------------------
    // DIM020M-16  no `.forge` schema or byte change beyond the transform values
    // -----------------------------------------------------------------------
    //
    // Two scenes reach the SAME placement two ways: one through the exact
    // dimension edit this stage adds, one by typing the solved numbers into the
    // transform the product already had. If the files are byte-identical then
    // the new act writes no new field, no new section and no new version --
    // which is the whole persistence claim of Stage 020M.
    {
        Fixture viaDimension;
        viaDimension.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        TransformValues start = turned(17.0, -29.0, 63.0);
        start.positionX = 1.25;
        start.positionY = -0.5;
        start.positionZ = 2.75;
        start.scaleX = 1.5;
        start.scaleY = 2.5;
        start.scaleZ = 0.5;
        viaDimension.setPlacement(start);
        applyBodyDimension(viaDimension.id(), 2, 3.0, ResizeAnchor::PositiveSide,
                           viaDimension.scene, viaDimension.history);
        const TransformValues solved = viaDimension.placement();

        Fixture viaTransform;
        viaTransform.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        viaTransform.setPlacement(solved);

        ProjectCodecStatus whyA = ProjectCodecStatus::Ok;
        ProjectCodecStatus whyB = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> a = encodeProjectV1(
            captureProjectDocument(viaDimension.scene, ProjectKind::Construction), &whyA);
        const std::vector<uint8_t> b = encodeProjectV1(
            captureProjectDocument(viaTransform.scene, ProjectKind::Construction), &whyB);
        r.check("DIM020M-16 both projects encode",
                whyA == ProjectCodecStatus::Ok && whyB == ProjectCodecStatus::Ok && !a.empty());
        r.check("DIM020M-16 a dimension edit writes exactly the bytes a transform edit does",
                a == b);

        // And it round-trips: the file carries the solved transform and nothing
        // that had to be invented for it.
        ProjectDocument decoded;
        r.check("DIM020M-16 the file decodes through the ordinary decoder",
                decodeProject(a.data(), a.size(), &decoded) == ProjectCodecStatus::Ok);
        r.check("DIM020M-16 the decoded project is a Construction project with no CAD or import",
                !decoded.hasCad && !decoded.hasImported && decoded.hasConstruction);
    }

    // -----------------------------------------------------------------------
    // DIM020M-17  the label anchor is a property of the BODY, not of the frame
    // -----------------------------------------------------------------------
    //
    // `UI-3D-STATE-C1`. The chrome that positions the three numbers asks
    // `bodyDimensionLabelAnchors` for the instant it is refreshing, instead of
    // reading what a frame last cached -- which is what left the labels absent
    // on the frame the mode opened and standing on the PREVIOUS body after a
    // switch. That read is only sound while two things hold, so both are pinned
    // here rather than assumed.
    {
        LocalBounds bounds;
        bounds.minX = -1.0;
        bounds.maxX = 1.5;
        bounds.minY = -0.25;
        bounds.maxY = 0.75;
        bounds.minZ = -2.0;
        bounds.maxZ = 0.5;
        TransformValues placement = turned(23.0, -41.0, 12.0);
        placement.positionX = -0.75;
        placement.positionY = 2.5;
        placement.positionZ = 1.25;
        placement.scaleX = 1.75;
        placement.scaleY = 0.5;
        placement.scaleZ = 2.25;
        const float worldPerUnit = 0.0042f;

        // ONE: an anchor does not depend on which axis is active. The active
        // axis decides which RANGE a leader is emitted in -- how it is drawn --
        // and the anchors-only read passes no axis at all, so if that were ever
        // to move a midpoint the number would drift off the line it measures
        // the moment the user tapped it.
        BodyDimensionLabelAnchors read;
        r.check("DIM020M-17 the anchors-only read succeeds for a valid body",
                bodyDimensionLabelAnchors(bounds, placement, worldPerUnit, &read) && read.valid);
        bool matchesEveryActiveAxis = true;
        for (int active = -1; active < kBodyAxisCount; ++active) {
            BodyDimensionLabelAnchors built;
            buildBodyDimensionOverlay(bounds, placement, active, worldPerUnit, 7, &built);
            if (!built.valid) {
                matchesEveryActiveAxis = false;
                break;
            }
            for (int a = 0; a < kBodyAxisCount; ++a) {
                matchesEveryActiveAxis = matchesEveryActiveAxis
                                      && std::fabs(built.axis[a].x - read.axis[a].x) <= kWorldEpsilon
                                      && std::fabs(built.axis[a].y - read.axis[a].y) <= kWorldEpsilon
                                      && std::fabs(built.axis[a].z - read.axis[a].z) <= kWorldEpsilon;
            }
        }
        r.check("DIM020M-17 the anchors are the drawn midpoints for every active axis",
                matchesEveryActiveAxis);

        // TWO: it fails closed on exactly the builder's terms, so a chrome read
        // can never place a label from a derivation that did not happen.
        BodyDimensionLabelAnchors refused;
        r.check("DIM020M-17 a non-positive camera scale is refused, not guessed",
                !bodyDimensionLabelAnchors(bounds, placement, 0.0f, &refused) && !refused.valid);
        LocalBounds inverted = bounds;
        inverted.maxY = inverted.minY - 1.0;
        r.check("DIM020M-17 invalid bounds are refused",
                !bodyDimensionLabelAnchors(inverted, placement, worldPerUnit, &refused));
        TransformValues broken = placement;
        broken.rotationX = std::numeric_limits<double>::quiet_NaN();
        r.check("DIM020M-17 a non-finite placement is refused",
                !bodyDimensionLabelAnchors(bounds, broken, worldPerUnit, &refused));
    }

    return r.n;
}

}  // namespace forgeshape
