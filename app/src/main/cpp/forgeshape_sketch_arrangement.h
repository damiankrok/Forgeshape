// The planar arrangement of a sketch's curves (`CAD-PLANAR-FACE-PF-S1`).
//
// What it is
// ----------
// The region model (`forgeshape_sketch_region.h`) is loop NESTING: a crossing
// never splits a curve, so the cells a circle and a rectangle cut each other
// into -- the lens, the crescent, a protrusion closed against a rectangle's
// edge -- are not regions at all. This module derives them:
//
//   authored curves -> analytic intersections + T-junctions -> nodes
//   -> source-curve FRAGMENTS between consecutive cuts -> half-edge graph
//   -> bounded ATOMIC FACES -> a canonical, semantic `PlanarFaceRef` per face
//
// Everything here is DERIVED from a `CadSketch` and regenerated on demand. It
// owns no authored truth, no selection, no session state, no persistence and
// no presentation, and nothing in the product consumes it yet: PF-S1 proves
// the engine and its identity before any format or UI depends on it.
//
// Identity
// --------
// A face is identified by its canonical BOUNDARY, never by a number, a
// coordinate, a vector index, a tessellation index or a triangle. A boundary
// is a cycle of fragments, and a fragment is named only by authored identity:
// the source entity id, the source edge's local index (rectangle side 0..3,
// polyline segment k, 0 for a line, circle or arc) and the two CUTS bounding
// it -- the source's own start or end, or the k-th intersection of this edge
// with a named partner edge, counted along this edge's own parameter. So a
// ref survives re-derivation, input order and any edit that keeps the same
// intersection partners and ordinals, and it CHANGES -- it no longer resolves
// -- when an edit adds or removes an intersection. Resolution is exact tuple
// equality; there is no nearest-face or seed-point fallback.
//
// Supported curves: Line, Polyline segments, Rectangle sides, Circle, Arc --
// all intersected analytically in binary64 sketch coordinates. A Spline is
// refused (`UnsupportedCurve`) rather than intersected on its tessellation,
// because a tessellation index is not an identity.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "forgeshape_sketch.h"

namespace forgeshape {

// Why an arrangement could not be derived. A derived, module-local status:
// it never crosses JNI and is never stored, so it does not extend `CadStatus`.
enum class ArrangementStatus : uint8_t {
    Ok,
    // The sketch fails `validateCadSketch` (or an arc has no circle).
    InvalidSketch,
    // A Spline is in the sketch. No tessellation-derived identity is made.
    UnsupportedCurve,
    // Two curves share a stretch longer than the coincidence tolerance
    // (collinear overlapping segments, coincident arcs or circles): there is
    // no finite set of nodes to split them at, so nothing is guessed.
    AmbiguousOverlap,
    // A bounded cycle below `kMinProfileAreaSquareMeters`.
    DegenerateFace,
    // More source edges or intersections than the caps below.
    CapExceeded,
    // `mergePlanarFaces` (`CAD-V6-S2`): a face index that names no face, or a
    // repeat.
    InvalidSelection,
    // `mergePlanarFaces`: the union's boundary passes one node twice -- two
    // chosen faces touch at a single point -- so no simple loop bounds it.
    // Refused rather than extruded as a pinched solid.
    PinchedSelection,
};

const char* arrangementStatusName(ArrangementStatus status);

// The arrangement's own caps, checked BEFORE the quadratic work completes.
// The sketch's caps allow 256 polylines x 256 vertices -- tens of thousands of
// straight edges and quadratic intersection work no touch sketch needs -- so
// the arrangement bounds itself and refuses above that by name.
constexpr uint32_t kMaxArrangementSourceEdges = 1024;
constexpr uint32_t kMaxArrangementContacts = 4096;

// ---------------------------------------------------------------------------
// Semantic identity
// ---------------------------------------------------------------------------

// `ArrangementCutKind` and `ArrangementCut` -- where a fragment of a source
// edge starts or ends -- are declared in forgeshape_sketch.h, beside the face
// token that names a fragment side by two of them (`CAD-V6-S2`). Where several
// curves meet at one point the cut is named by the smallest of their refs.

// One fragment of one source edge, and the direction a cycle walks it.
// `reversed` is false for the source's own parameter direction.
struct FragmentRef {
    SketchEntityId sourceEntityId = kNoSketchEntity;
    uint32_t sourceEdgeLocalIndex = 0;
    ArrangementCut startCut{};
    ArrangementCut endCut{};
    bool reversed = false;
};

using FragmentCycle = std::vector<FragmentRef>;

// A face: its outer boundary counter-clockwise and each hole clockwise, every
// cycle rotated to start at its lexicographically smallest fragment, holes
// sorted by that first fragment. Two refs are the same face exactly when
// they are equal as tuples.
struct PlanarFaceRef {
    FragmentCycle outer;
    std::vector<FragmentCycle> holes;
};

// Total orders over the semantic tuples (negative, zero, positive).
int compareArrangementCut(const ArrangementCut& a, const ArrangementCut& b);
int compareFragmentRef(const FragmentRef& a, const FragmentRef& b);
int comparePlanarFaceRef(const PlanarFaceRef& a, const PlanarFaceRef& b);
bool samePlanarFaceRef(const PlanarFaceRef& a, const PlanarFaceRef& b);

// ---------------------------------------------------------------------------
// The derived arrangement
// ---------------------------------------------------------------------------

// Counted by contact type, for tests and diagnostics.
struct ArrangementStats {
    uint32_t sourceEdges = 0;
    uint32_t crossings = 0;            // interior of both curves
    uint32_t tJunctions = 0;           // an endpoint on the other's interior
    uint32_t endpointCoincidences = 0; // an endpoint on the other's endpoint
    uint32_t tangents = 0;             // touching without crossing: no node
    uint32_t nodes = 0;
    uint32_t fragments = 0;
    uint32_t prunedFragments = 0;      // dangling or bridging: bound no face
};

// One fragment, in its source's own direction.
struct ArrangementFragment {
    FragmentRef ref;          // `reversed` is always false here
    uint32_t startNode = 0;   // index into `SketchArrangement::nodes`
    uint32_t endNode = 0;
    bool curved = false;      // a piece of a circle or an arc
    bool boundsFace = false;  // false when pruned as dangling or a bridge
    // The fragment's DERIVED geometry (`CAD-V6-S2`), in the source's own
    // direction from `nodes[startNode]` to `nodes[endNode]` inclusive -- those
    // two points EXACTLY, so neighbouring fragments share their end vertex
    // bit for bit. A straight piece is those two points; a piece of a circle
    // or arc is clipped to its own sweep and tessellated at the density an
    // authored arc gets (`sketchArcSegmentCount`). Never identity: a denser
    // tessellation changes these points and no ref.
    std::vector<SketchPoint> points;
};

struct AtomicPlanarFace {
    PlanarFaceRef ref;
    // Exact enclosed area (outer minus holes), m^2, arcs integrated
    // analytically. Presentation and tests only; never identity.
    double area = 0.0;
};

struct SketchArrangement {
    ArrangementStatus status = ArrangementStatus::Ok;
    // Node positions in canonical order (by the smallest semantic key of
    // what met there). Geometry, not identity.
    std::vector<SketchPoint> nodes;
    // Sorted by `ref`.
    std::vector<ArrangementFragment> fragments;
    // The BOUNDED atomic faces only, sorted by `ref`. The unbounded exterior
    // is never emitted.
    std::vector<AtomicPlanarFace> faces;
    ArrangementStats stats;
};

// Derives the arrangement of every supported curve in the sketch. Pure,
// deterministic, bounded by the caps above; independent of the order of
// `sketch.entities`.
SketchArrangement deriveSketchArrangement(const CadSketch& sketch);

// Finds the face whose canonical ref EQUALS `ref`. False (and `*outIndex`
// untouched) when the boundary no longer exists -- no nearest-face search.
bool resolvePlanarFaceRef(const SketchArrangement& arrangement, const PlanarFaceRef& ref,
                          size_t* outIndex);

// ---------------------------------------------------------------------------
// The union of chosen faces (`CAD-V6-S2`)
// ---------------------------------------------------------------------------
//
// What an extrusion of several atomic faces extrudes is their UNION, and it is
// derived here, on the arrangement's own half-edges, never on a second graph:
//
//   every chosen face contributes its boundary cycles, each walked with the
//   face on the LEFT (outer counter-clockwise, holes clockwise);
//   a fragment walked BOTH ways by chosen faces separates two of them and is
//   interior to the union: it cancels;
//   what remains is walked into cycles by the same rotation the faces were --
//   from a half-edge, its own face's successor, and past every cancelled one
//   to the next chosen face around the node -- so no angle is recomputed;
//   a positive cycle is an outer boundary, a negative one a hole, owned by the
//   smallest outer that contains it.
//
// Each loop keeps its FRAGMENTS (identity) and a polygon (geometry) built from
// the fragments' derived points; the two are joined per polygon edge, so every
// wall of the extruded solid knows the fragment it stands on.

struct PlanarProfileLoop {
    // The loop's fragments in walk order (union on the left), rotated to start
    // at the smallest. `reversed` is the walk direction against the source's.
    FragmentCycle fragments;
    // Parallel to `fragments`: whether that fragment is a piece of a curve.
    std::vector<uint8_t> fragmentCurved;
    // Counter-clockwise, with no repeated closing vertex -- a HOLE is stored
    // re-oriented, so every loop reads the way an extracted profile does.
    std::vector<SketchPoint> polygon;
    // Per polygon edge k (polygon[k] -> polygon[(k + 1) % n]): the index into
    // `fragments` of the fragment it lies on.
    std::vector<uint32_t> edgeFragment;
    // The polygon's area, positive, square metres.
    double area = 0.0;
};

struct PlanarProfileComponent {
    PlanarProfileLoop outer;
    // Ascending by their fragment cycles.
    std::vector<PlanarProfileLoop> holes;
    // Outer minus holes, polygon areas.
    double area = 0.0;
};

// The union of `faceIndices` (indices into `arrangement.faces`, any order, no
// repeats), as its connected components in canonical order (by the outer
// cycle), each with its holes. Deterministic; derived, never stored.
ArrangementStatus mergePlanarFaces(const SketchArrangement& arrangement,
                                   const std::vector<size_t>& faceIndices,
                                   std::vector<PlanarProfileComponent>* out);

}  // namespace forgeshape
