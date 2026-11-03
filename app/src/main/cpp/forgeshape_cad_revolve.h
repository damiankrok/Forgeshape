// The derived solid of a Revolve feature (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer, no camera,
// no zoom. Everything here is DERIVED from a `RevolveFeature` and its sketch and
// regenerated whenever it is needed; nothing here is stored or read back.
//
// What a revolve is
// -----------------
// The selected area -- the very union an Extrude of the same selection would
// extrude, component by component, holes included -- swept about a straight
// edge of its own sketch by an angle in (0, 360] degrees. A point at axial
// coordinate t and radial distance r from the axis line becomes the circle arc
//
//     X(theta) = A + t*D + r*(cos(theta)*E1 + sin(theta)*E2),   theta in [0, angle]
//
// with A the axis start, D the axis direction, E1 the in-plane radial direction
// towards the area and E2 = +/-(D x E1) by the direction. Every quantity is
// binary64 in the body's local metres until the render mesh.
//
// Which side, and what is refused
// -------------------------------
// The selected area must lie on ONE side of the infinite axis line, touching it
// at most (`RevolveProfileCrossesAxis` otherwise): a crossing area would sweep
// through itself. The test does not trust samples alone: a polygon vertex is a
// point ON the authored curve and is exact, the true arc of a Circle or an Arc
// between two vertices is checked exactly (its closest approach to the axis),
// and a Spline span between two vertices is held to its Bezier deviation bound
// -- a spline near the axis is refused rather than assumed to stay clear.
// Separate components on OPPOSITE sides are legal only when their sweeps cannot
// meet: mirrored across the axis they must neither overlap nor touch, or the
// angle must be under half a turn (`RevolveComponentsOverlap` otherwise) --
// a union of overlapping sweeps is a boolean R1 does not make.
//
// The mesh
// --------
// Deterministic and camera-free: `revolveSegmentCount` angular steps at the
// circle's own density (32 for a full turn). A full turn closes its seam
// topologically -- the ring index wraps, there are no caps and no duplicated seam
// -- and a partial turn closes with a start cap and an end cap triangulated from
// the profile. A profile vertex ON the axis is ONE apex vertex, never a ring of
// coincident copies; on a full turn a vertex that only touches the axis between
// two swept edges gets one apex per fan, so every vertex's neighbourhood is one
// disc. An edge lying on the axis sweeps nothing and emits nothing.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_kernel.h"

namespace forgeshape {

// Within this distance of the axis line a profile vertex IS on the axis: the
// sketch's own coincidence tolerance.
constexpr double kRevolveAxisToleranceMeters = kSketchCoincidenceMeters;

// The angular steps a sweep of `angleDegrees` is built from: the circle's own
// density (`sketchArcSegmentCount`), so a full turn has exactly the 32 segments
// every round primitive and a sketch circle have.
uint32_t revolveSegmentCount(double angleDegrees);

// One boundary loop of a profile component in sketch (u, v): its polygon, and
// per polygon edge k (polygon[k] -> polygon[k+1]) the face tag its swept surface
// carries and which entity the edge lies on and whether it is a piece of a
// curve -- what the exact side test reads.
struct RevolveLoop {
    std::vector<SketchPoint> polygon;
    std::vector<uint32_t> edgeTag;
    std::vector<SketchEntityId> edgeEntity;
    std::vector<uint8_t> edgeCurved;
};

// One connected piece of the selected area: loops[0] the outer boundary, the
// rest its holes -- every loop counter-clockwise in (u, v), as both the region
// and the planar-face derivations store them.
struct RevolveComponent {
    std::vector<RevolveLoop> loops;
};

// Which side of the axis each component lies on (+1 the side the axis's left
// normal points to, -1 the other), or the refusal by name. `angleDegrees`
// decides whether opposite-side components could meet.
CadStatus classifyRevolveComponents(const CadSketch& sketch, const RevolveAxis2D& axis,
                                    double angleDegrees,
                                    const std::vector<RevolveComponent>& components,
                                    std::vector<int>* outSides);

// Appends the swept solid of every component to `solid`: closed, outward-wound
// (positive volume), start cap tagged `startCapTag` and end cap `endCapTag` on a
// partial sweep, every swept surface with its loop edge's tag. `placement` maps
// sketch (u, v, w) into the body. Writes nothing on a refusal.
CadStatus appendRevolveSolid(const CadFrame64& placement, const RevolveAxis2D& axis,
                             double angleDegrees, RevolveDirection direction,
                             const std::vector<RevolveComponent>& components,
                             const std::vector<int>& sides, uint32_t startCapTag,
                             uint32_t endCapTag, CadSolid* solid);

// The world-free geometry of the sweep's frame, for the manipulator and the
// faces: the axis start in the body, the unit axis direction D, the unit radial
// direction E1 towards side +1, and E2 the direction a point on side +1 moves at
// theta = 0 for the given sense. Pure arithmetic over the placement.
struct RevolveFrame {
    DVec3 origin{0.0, 0.0, 0.0};
    DVec3 axis{1.0, 0.0, 0.0};
    DVec3 radial{0.0, 1.0, 0.0};
    DVec3 tangent{0.0, 0.0, 1.0};
};
RevolveFrame revolveFrame(const CadFrame64& placement, const RevolveAxis2D& axis,
                          RevolveDirection direction);

}  // namespace forgeshape
