// Sketch MODIFY operations (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`): Trim, Extend,
// Offset, Mirror, the Construction role toggle and Delete-with-dimensions.
//
// Platform-neutral C++17: pure functions from one `CadSketch` to another. None
// of them is a history step or a session state -- the sketch session applies a
// result to its STAGED sketch, and the one commit that already existed makes it
// project truth. Every refusal is a named `CadStatus` and writes nothing.
//
// One intersection machinery
// --------------------------
// Trim and Extend do not intersect curves themselves: they derive the planar
// arrangement (forgeshape_sketch_arrangement.h) over the sketch -- Construction
// included, through `cadSketchAllCurvesView` -- and read the FRAGMENTS it
// splits a curve into. So the interval Trim removes is exactly the fragment the
// fill cells are bounded by, a Spline cuts by its exact Bezier spans, and an
// overlap the arrangement calls ambiguous is refused by that name rather than
// guessed at.
//
// Authored truth only
// -------------------
// Results are built from authored values and exact arrangement nodes: a Line
// keeps its untouched endpoint bit for bit, a trimmed Circle becomes an Arc
// through points on the original circle, an offset Arc keeps its angles, a
// mirrored Spline reflects its authored points. Nothing is read back out of a
// tessellation. Dimensions are never remapped or copied: Trim refuses an entity
// a dimension names, and Offset and Mirror create geometry with none.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_sketch.h"

namespace forgeshape {

// The distance from a point to an entity's stroke, metres: the hit test every
// drafting tap uses (curves against their derived polyline, exactly as drawn).
double sketchEntityDistance(const SketchEntity& entity, const SketchPoint& point);

// The entity whose stroke is nearest `point` within `tolerance`, ties keeping
// the earlier entity; kNoSketchEntity when none is.
SketchEntityId hitSketchEntity(const CadSketch& sketch, const SketchPoint& point, double tolerance);

// The straight edge of `entityId` nearest `point` (its edge-local index), for a
// Line, Polyline or Rectangle. False for a curve or an unknown id.
bool nearestSketchStraightEdge(const CadSketch& sketch, SketchEntityId entityId,
                               const SketchPoint& point, CadSketchEdgeRef* out);

// --- Construction role -------------------------------------------------------

// Sets every listed entity's role, keeping ids and geometry. `UnknownEntity`
// for an id the sketch does not carry, writing nothing.
CadStatus setSketchEntitiesRole(CadSketch* sketch, const std::vector<SketchEntityId>& ids,
                                SketchEntityRole role);

// The ONE deterministic toggle for a selection: Construction when ANY listed
// entity is Regular, Regular when all are Construction.
SketchEntityRole sketchRoleToggleTarget(const CadSketch& sketch,
                                        const std::vector<SketchEntityId>& ids);

// --- Delete ------------------------------------------------------------------

// Removes the listed entities and every dimension that names ONLY removed
// entities, in one result. A dimension naming a removed AND a surviving entity
// refuses the whole act (`SketchDimensionDependency`): no dimension may be left
// pointing at nothing, and none is silently dropped for an entity the user kept.
CadStatus deleteSketchEntities(CadSketch* sketch, const std::vector<SketchEntityId>& ids,
                               uint32_t* outRemovedDimensions = nullptr);

// --- Trim ----------------------------------------------------------------------

struct SketchTrimPlan {
    SketchEntityId target = kNoSketchEntity;
    uint32_t edgeLocalIndex = 0;
    // The derived polyline of the interval that will be removed: the preview.
    std::vector<SketchPoint> removed;
    // The whole sketch after the trim.
    CadSketch result;
    // True when a Rectangle was decomposed into Lines.
    bool rectangleConverted = false;
    // Ids the trim minted (fresh pieces), ascending.
    std::vector<SketchEntityId> createdIds;
    // True when the target was removed outright (no cut on it).
    bool targetDeleted = false;
};

// Plans the Trim of the bounded interval of the entity under `point` (within
// `tolerance`): the interval between the nearest valid intersections on either
// side, along the source curve. Refused: `DraftingNoTarget` (nothing under the
// point), `TrimSplineUnsupported`, `SketchDimensionDependency` (a dimension
// names the target), the arrangement's own name when it cannot derive, and the
// geometry's when a piece would be invalid.
//
// `allCurves`, when given, must be the all-curves arrangement of this very
// sketch (`deriveSketchArrangement(cadSketchAllCurvesView(sketch))`); the
// session passes the one it already derived for snapping.
struct SketchArrangement;
CadStatus planSketchTrim(const CadSketch& sketch, const SketchPoint& point, double tolerance,
                         SketchTrimPlan* out, const SketchArrangement* allCurves = nullptr);

// --- Extend --------------------------------------------------------------------

struct SketchExtendPlan {
    SketchEntityId target = kNoSketchEntity;
    // True when the END (a Line's P1, a Polyline's last vertex, an Arc's end)
    // moves; false for the start.
    bool atEnd = true;
    SketchPoint from{};
    SketchPoint to{};
    // The added piece, as a derived polyline: the preview.
    std::vector<SketchPoint> added;
    CadSketch result;
};

// Plans the Extend of the entity under `point` at its end nearer `point`, along
// its exact continuation, to the nearest forward intersection with another
// curve. Refused: `DraftingNoTarget`, `ExtendUnsupported` (Circle, Rectangle,
// closed Polyline, Spline), `SketchDimensionLocked` (a Driving LineLength),
// `ExtendNoTarget`, `ExtendAmbiguous` (the continuation runs along another
// curve).
CadStatus planSketchExtend(const CadSketch& sketch, const SketchPoint& point, double tolerance,
                           SketchExtendPlan* out);

// --- Offset --------------------------------------------------------------------

// The miter ratio above which a polyline corner is refused: |miter| / |d|. Four
// is a 29-degree corner; a sharper one is refused rather than bevelled.
constexpr double kOffsetMiterLimit = 4.0;

// Whether an entity kind can be offset (every kind but Spline).
bool sketchEntityOffsettable(const SketchEntity& entity);

// The signed offset distance a point implies for an entity: the side of a Line
// or Polyline (positive LEFT of its direction), the radial distance for a
// Circle or Arc and the outward distance for a Rectangle (positive OUTWARD).
bool sketchOffsetDistanceAt(const SketchEntity& entity, const SketchPoint& point, double* out);

// The offset geometry, as payloads (not yet in the sketch): one entity for every
// supported kind. Refused: `OffsetSplineUnsupported`, `OffsetInvalid` (zero or
// non-finite distance, a radius or size not positive, a collapsing segment),
// `OffsetMiterLimit`, `OffsetSelfIntersecting`.
CadStatus offsetSketchEntity(const SketchEntity& entity, double signedDistance,
                             std::vector<SketchEntity::Payload>* out);

// Applies an offset: the result entities are APPENDED with fresh ids and the
// source's role; no dimension is created. `outIds` receives the new ids.
CadStatus applySketchOffset(CadSketch* sketch, SketchEntityId source, double signedDistance,
                            std::vector<SketchEntityId>* outIds = nullptr);

// --- Mirror --------------------------------------------------------------------

// The mirror axis, resolved: a straight edge of the sketch (Regular or
// Construction). `MirrorAxisInvalid` for a curve, an unknown ref or a degenerate
// edge.
CadStatus resolveSketchMirrorAxis(const CadSketch& sketch, const CadSketchEdgeRef& axis,
                                  SketchPoint* outPoint, double* outDu, double* outDv);

// Reflects `point` across the infinite line through `a` with unit direction d.
SketchPoint reflectSketchPoint(const SketchPoint& point, const SketchPoint& a, double du, double dv);

// The mirrored geometry of `ids` (the axis entity itself is skipped), as
// (payload, role) pairs in sketch order. A Rectangle stays a Rectangle when the
// axis is exactly parallel to a sketch axis; otherwise it becomes four Lines in
// its canonical edge order, because a rotated rectangle is not a parameter the
// sketch has. Refused: `MirrorAxisInvalid`, `MirrorNothingSelected`,
// `UnknownEntity`.
struct SketchMirrorPiece {
    SketchEntity::Payload payload;
    SketchEntityRole role = SketchEntityRole::Regular;
};
CadStatus mirrorSketchEntities(const CadSketch& sketch, const std::vector<SketchEntityId>& ids,
                               const CadSketchEdgeRef& axis, std::vector<SketchMirrorPiece>* out);

// Applies a mirror: the pieces are APPENDED with fresh ids; no dimension.
CadStatus applySketchMirror(CadSketch* sketch, const std::vector<SketchEntityId>& ids,
                            const CadSketchEdgeRef& axis,
                            std::vector<SketchEntityId>* outIds = nullptr);

}  // namespace forgeshape
