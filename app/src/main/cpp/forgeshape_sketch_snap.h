// Drafting snaps and inference guides (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`).
//
// Platform-neutral C++17, a pure function over a sketch and a raw point. The
// sketch session hands it the pointer's raw (u, v) and a tolerance in metres
// (24 reference units at the plane, fixed at pointer-down); what comes back is
// an EXACT coordinate -- another entity's own stored value, an analytic
// midpoint or centre, an arrangement node, the origin, or an exact multiple of
// the grid step -- never a rounded pixel.
//
// ONE deterministic priority, first non-empty level wins:
//
//   1 Endpoint      line ends, polyline vertices, rectangle corners, an arc's
//                   three authored points, a spline's authored points
//   2 Intersection  crossing and T-junction nodes of the ALL-CURVES planar
//                   arrangement (the same analytic machinery PlanarFaces use;
//                   an arrangement that cannot derive -- an ambiguous overlap --
//                   offers no intersection at all rather than an invented one)
//   3 Midpoint      a line, a polyline segment, a rectangle edge
//   4 Center        a circle's centre, an arc's centre, a rectangle's centre
//   5 Origin        exactly (0, 0)
//   6 Guides        horizontal / vertical alignment with a reference point,
//                   captured within half the point tolerance
//   7 Grid
//
// The priority is applied in TWO apertures, inner first: among the candidates
// within `kSketchSnapInnerApertureFraction` of the tolerance (a third: 8 of the
// finger's 24 reference units), then among those within the whole tolerance.
// So an exact centre under the finger beats an intersection a finger-width
// away, while within either aperture the order above holds. Within a level the
// smallest distance wins; an exact tie keeps the candidate earlier in semantic
// order (entity id ascending, then edge / point index; the arrangement's own
// canonical node order for intersections).
//
// Construction entities take part in every kind. An inference guide is a
// PRESENTATION of the moment: it snaps one coordinate and records nothing --
// no relation, no constraint -- so moving either point later keeps nothing.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_sketch.h"

namespace forgeshape {

// The inner aperture's share of the point tolerance (see above).
constexpr double kSketchSnapInnerApertureFraction = 1.0 / 3.0;

// The share of the point tolerance within which an inference guide captures:
// half, so a guide is a hint the finger slides past, not a target it lands on.
constexpr double kSketchGuideToleranceFraction = 0.5;

// What a snap landed on. The first three keep the values they always had
// (`None`, `Grid`, `Endpoint`); the rest are APPENDED.
enum class SketchSnapKind : uint8_t {
    None,
    Grid,
    Endpoint,
    Intersection,
    Midpoint,
    Center,
    Origin,
    HorizontalGuide,
    VerticalGuide,
};

constexpr int kSketchSnapKindCount = 9;

const char* sketchSnapKindName(SketchSnapKind kind);

// Every exact point a sketch offers, by kind, in semantic order. Derived from
// the sketch alone -- no camera -- so it can be built once per sketch content
// and reused for every pointer move.
struct SketchSnapCandidates {
    std::vector<SketchPoint> endpoints;
    std::vector<SketchPoint> intersections;
    std::vector<SketchPoint> midpoints;
    std::vector<SketchPoint> centers;
    // False when the all-curves arrangement could not be derived (an
    // ambiguous overlap, a cap): `intersections` is then empty on purpose.
    bool intersectionsDerived = true;
};

SketchSnapCandidates collectSketchSnapCandidates(const CadSketch& sketch);

struct SketchSnapResult {
    SketchPoint point{};
    SketchSnapKind kind = SketchSnapKind::None;
    // Which guides are active, and the reference point each aligns with. Both
    // may hold at once (the point is then exact in both coordinates).
    bool horizontalGuide = false;
    bool verticalGuide = false;
    SketchPoint horizontalSource{};
    SketchPoint verticalSource{};
};

// Snaps `raw`. `extraEndpoints` are points not yet in the sketch that are
// endpoints all the same (a polyline's placed vertices); `guideAnchor`, when
// given, is the gesture's own start, a guide source like any reference point.
// `toleranceMeters` <= 0 or non-finite disables every snap but the grid; a
// `gridStep` <= 0 disables the grid (the raw point is returned, kind None).
//
// `dragStart`, when given, is where the current drag began: no POINT candidate
// at that place is offered, because a drag's end snapped back onto its own
// start is an entity of no size -- never what the finger meant.
SketchSnapResult snapSketchPoint(const SketchSnapCandidates& candidates, const SketchPoint& raw,
                                 double toleranceMeters, double gridStep,
                                 const std::vector<SketchPoint>& extraEndpoints,
                                 const SketchPoint* guideAnchor,
                                 const SketchPoint* dragStart = nullptr);

}  // namespace forgeshape
