// Retained sketch dimensions (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`): what a
// dimension may measure, the value it shows, the one Driving edit path, and the
// technical-drawing annotation it is drawn as.
//
// Platform-neutral C++17: no JNI, no Android, no renderer, no camera. The
// annotation is built in SKETCH coordinates from the authored geometry and one
// camera-derived scale (`worldPerUnit`, metres per reference unit at the
// plane) the caller hands in, exactly as the selected-line dimension always
// was; nothing here is stored.
//
// The record (`SketchDimension`, forgeshape_sketch.h) says WHAT is measured and
// whether it drives. It never stores a number: `sketchDimensionValue` derives
// the value from the geometry every time, and `applySketchDimensionValue` --
// the ONE Driving edit -- writes the geometry and nothing else, so a dimension
// can never disagree with the entity it measures.
//
// No constraint solver. A Driving dimension owns ONE local degree of freedom of
// ONE entity -- a line's length, a line's angle, a rectangle's width or height,
// a circle's radius (Radius and Diameter are two views of that one DOF) -- and
// a second Driving dimension on an owned DOF is refused by name
// (`SketchDimensionConflict`). Reference dimensions own nothing.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_sketch.h"

namespace forgeshape {

// Whether the kind measures an ANGLE (degrees) rather than a length (metres).
bool sketchDimensionIsAngle(SketchDimensionKind kind);

// Whether the kind names one EDGE of its entity (EdgeLength, EdgeAngle) rather
// than the entity as a whole (every other kind names edge 0).
bool sketchDimensionKindNamesEdge(SketchDimensionKind kind);

// Whether the kind may be Driving in R1. LineLength, LineAngle, RectangleWidth,
// RectangleHeight, CircleRadius and CircleDiameter -- each one local edit that is
// unambiguous. Every other kind is Reference-only.
bool sketchDimensionKindAllowsDriving(SketchDimensionKind kind);

// The kinds that can measure `ref` in this sketch, in enum order. Empty for a
// Spline (no exact length exists), an unknown entity or a bad edge index.
// EdgeAngle is listed for any straight edge: it needs a second edge too.
std::vector<SketchDimensionKind> applicableSketchDimensionKinds(const CadSketch& sketch,
                                                                const CadSketchEdgeRef& ref);

// One record's own rule against the sketch: kind and mode in their enums,
// Driving only where allowed, refs resolving to the right kind of target, a
// second ref exactly for EdgeAngle (two DISTINCT straight edges). The id is not
// judged here. `SketchDimensionInvalid` on any failure.
CadStatus validateSketchDimensionTarget(const CadSketch& sketch, const SketchDimension& dimension);

// The whole table: count bound, `nextDimensionId` non-zero and above every id,
// ids non-zero and strictly ascending, every record valid, no two Driving
// records owning one DOF and no exact duplicate (`SketchDimensionConflict`).
// Called by `validateCadSketch`, so no path can hold a dimension naming an
// entity that is not there.
CadStatus validateSketchDimensions(const CadSketch& sketch);

// The derived value: metres for a length kind, degrees for an angle kind.
// Fails (`SketchDimensionInvalid`) when the record does not resolve.
CadStatus sketchDimensionValue(const CadSketch& sketch, const SketchDimension& dimension,
                               double* outValue);

// Appends a dimension, minting its id from `nextDimensionId`. Validated as the
// whole table would be, so a conflicting or unresolved record mints nothing.
CadStatus addSketchDimension(CadSketch* sketch, SketchDimensionKind kind, SketchDimensionMode mode,
                             const CadSketchEdgeRef& first, const CadSketchEdgeRef& second,
                             SketchDimensionId* outId = nullptr);

// Removes one dimension; the geometry is untouched. `SketchDimensionInvalid`
// for an id the sketch does not carry.
CadStatus removeSketchDimension(CadSketch* sketch, SketchDimensionId id);

const SketchDimension* findSketchDimension(const CadSketch& sketch, SketchDimensionId id);

// THE Driving edit. `value` is metres for a length kind and degrees for an
// angle kind, exactly as typed:
//   LineLength      P0 fixed, direction kept:  P1 = P0 + dir * value
//   LineAngle       P0 fixed, length kept;     value in (-180, 180]
//   RectangleWidth  centre and height kept
//   RectangleHeight centre and width kept
//   CircleRadius    centre kept
//   CircleDiameter  centre kept, radius = value / 2
// Refused, changing nothing: `SketchDimensionReadOnly` for a Reference,
// `SketchDimensionValueInvalid` for a value outside its domain (never clamped),
// and the geometry's own refusal for a result the entity cannot be.
CadStatus applySketchDimensionValue(CadSketch* sketch, SketchDimensionId id, double value);

// The ids of every dimension naming `entityId`, ascending.
std::vector<SketchDimensionId> sketchDimensionsReferencing(const CadSketch& sketch,
                                                           SketchEntityId entityId);

// Whether a Driving LineLength dimension owns the line's length: Extend would
// change it and is refused (`SketchDimensionLocked`).
bool sketchLineLengthDriven(const CadSketch& sketch, SketchEntityId lineId);

// ---------------------------------------------------------------------------
// The technical-drawing annotation
// ---------------------------------------------------------------------------

// The dimension annotation's proportions, in reference units (dp), so it reads
// the same at any zoom (`SKETCH-UX-R1` E1). The dimension line stands this far
// off the stroke it measures -- far enough that the label never sits on the
// geometry -- the extension lines start a small gap away from the endpoints and
// overshoot the dimension line, and the end ticks are the technical-drawing
// slash rather than a filled arrowhead, which reads better at phone sizes.
constexpr double kSketchDimensionOffsetUnits = 30.0;
constexpr double kSketchDimensionExtensionGapUnits = 6.0;
constexpr double kSketchDimensionOvershootUnits = 8.0;
constexpr double kSketchDimensionTickUnits = 7.0;

// The retained dimensions' further proportions, in the same reference units.
constexpr double kSketchDimensionArrowUnits = 9.0;
constexpr double kSketchDimensionAngleRadiusUnits = 44.0;
constexpr double kSketchDimensionLabelClearUnits = 22.0;

// One dimension's annotation in sketch coordinates: line segments as point
// pairs (extension lines, dimension line or arc, arrowheads, leaders), and the
// point its label is centred on. Presentation only; never stored.
struct SketchDimensionAnnotation {
    SketchDimensionId id = kNoSketchDimension;
    SketchDimensionKind kind = SketchDimensionKind::LineLength;
    SketchDimensionMode mode = SketchDimensionMode::Reference;
    double value = 0.0;
    std::vector<SketchPoint> segments;  // pairs
    SketchPoint label{};
};

// Builds the annotation. Deterministic in the geometry and `worldPerUnit`:
// placement is normal to the measured edge on a fixed side (a line's CCW
// perpendicular, a rectangle's outside, a circle's 45-degree leader), so it
// moves continuously under zoom and never flips sides. False when the record
// does not resolve or the scale is unusable.
bool buildSketchDimensionAnnotation(const CadSketch& sketch, const SketchDimension& dimension,
                                    double worldPerUnit, SketchDimensionAnnotation* out);

}  // namespace forgeshape
