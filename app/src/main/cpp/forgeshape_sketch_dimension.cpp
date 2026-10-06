#include "forgeshape_sketch_dimension.h"

#include <algorithm>
#include <cmath>

#include "forgeshape_construction.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

// The target a kind measures. Edge kinds name a straight edge of a Polyline or
// a Rectangle; EdgeAngle names any straight edge, twice.
enum class TargetShape { Line, Rectangle, Circle, Arc, PolylineOrRectangleEdge, StraightEdge };

TargetShape targetShapeOf(SketchDimensionKind kind) {
    switch (kind) {
        case SketchDimensionKind::LineLength:
        case SketchDimensionKind::LineAngle:
        case SketchDimensionKind::LineHorizontal:
        case SketchDimensionKind::LineVertical:
            return TargetShape::Line;
        case SketchDimensionKind::RectangleWidth:
        case SketchDimensionKind::RectangleHeight:
            return TargetShape::Rectangle;
        case SketchDimensionKind::CircleRadius:
        case SketchDimensionKind::CircleDiameter:
            return TargetShape::Circle;
        case SketchDimensionKind::ArcRadius:
        case SketchDimensionKind::ArcSweep:
            return TargetShape::Arc;
        case SketchDimensionKind::EdgeLength:
            return TargetShape::PolylineOrRectangleEdge;
        case SketchDimensionKind::EdgeAngle:
            return TargetShape::StraightEdge;
    }
    return TargetShape::Line;
}

bool kindInEnum(SketchDimensionKind kind) {
    return static_cast<unsigned>(kind) < static_cast<unsigned>(kSketchDimensionKindCount);
}

bool modeInEnum(SketchDimensionMode mode) {
    return mode == SketchDimensionMode::Driving || mode == SketchDimensionMode::Reference;
}

// The straight edge `ref` names, or false.
bool resolveStraightEdge(const CadSketch& sketch, const CadSketchEdgeRef& ref,
                         SketchStraightEdge* out) {
    const SketchEntity* entity = findSketchEntity(sketch, ref.entityId);
    if (entity == nullptr) {
        return false;
    }
    for (const SketchStraightEdge& edge : sketchEntityStraightEdges(*entity)) {
        if (edge.edgeLocalIndex == ref.edgeLocalIndex) {
            if (out != nullptr) *out = edge;
            return true;
        }
    }
    return false;
}

bool targetMatches(const CadSketch& sketch, TargetShape shape, const CadSketchEdgeRef& ref) {
    const SketchEntity* entity = findSketchEntity(sketch, ref.entityId);
    if (entity == nullptr) {
        return false;
    }
    switch (shape) {
        case TargetShape::Line:
            return entity->line() != nullptr && ref.edgeLocalIndex == 0u;
        case TargetShape::Rectangle:
            return entity->rectangle() != nullptr && ref.edgeLocalIndex == 0u;
        case TargetShape::Circle:
            return entity->circle() != nullptr && ref.edgeLocalIndex == 0u;
        case TargetShape::Arc:
            return entity->arc() != nullptr && ref.edgeLocalIndex == 0u;
        case TargetShape::PolylineOrRectangleEdge:
            return (entity->polyline() != nullptr || entity->rectangle() != nullptr)
                   && resolveStraightEdge(sketch, ref, nullptr);
        case TargetShape::StraightEdge:
            return resolveStraightEdge(sketch, ref, nullptr);
    }
    return false;
}

// The degree of freedom a Driving kind owns, or 0 for none.
int drivenDof(SketchDimensionKind kind) {
    switch (kind) {
        case SketchDimensionKind::LineLength: return 1;
        case SketchDimensionKind::LineAngle: return 2;
        case SketchDimensionKind::RectangleWidth: return 3;
        case SketchDimensionKind::RectangleHeight: return 4;
        case SketchDimensionKind::CircleRadius:
        case SketchDimensionKind::CircleDiameter:
            return 5;  // ONE radius, two views of it
        default:
            return 0;
    }
}

double length(const SketchPoint& a, const SketchPoint& b) {
    const double du = b.u - a.u;
    const double dv = b.v - a.v;
    return std::sqrt(du * du + dv * dv);
}

bool usableLength(double value) {
    return std::isfinite(value) && validateDimensionMeters(value) == DimensionValidation::Ok
           && value <= kMaxSketchCoordinateMeters;
}

// The angle of a direction to +U in degrees, in (-180, 180]: atan2's -180 is
// the same direction as +180 and the canonical interval keeps +180.
double canonicalDegrees(double du, double dv) {
    double degrees = std::atan2(dv, du) * 180.0 / kPi;
    if (degrees <= -180.0) {
        degrees = 180.0;
    }
    return degrees;
}

// cos and sin of an angle in degrees, with the four cardinal angles EXACT so a
// typed 0, 90, 180 or -90 lands on an exactly horizontal or vertical line.
void exactCosSin(double degrees, double* c, double* s) {
    if (degrees == 0.0) { *c = 1.0; *s = 0.0; return; }
    if (degrees == 90.0) { *c = 0.0; *s = 1.0; return; }
    if (degrees == 180.0) { *c = -1.0; *s = 0.0; return; }
    if (degrees == -90.0) { *c = 0.0; *s = -1.0; return; }
    const double r = degrees * kPi / 180.0;
    *c = std::cos(r);
    *s = std::sin(r);
}

}  // namespace

bool sketchDimensionIsAngle(SketchDimensionKind kind) {
    return kind == SketchDimensionKind::LineAngle || kind == SketchDimensionKind::ArcSweep
           || kind == SketchDimensionKind::EdgeAngle;
}

bool sketchDimensionKindNamesEdge(SketchDimensionKind kind) {
    return kind == SketchDimensionKind::EdgeLength || kind == SketchDimensionKind::EdgeAngle;
}

bool sketchDimensionKindAllowsDriving(SketchDimensionKind kind) {
    return drivenDof(kind) != 0;
}

std::vector<SketchDimensionKind> applicableSketchDimensionKinds(const CadSketch& sketch,
                                                                const CadSketchEdgeRef& ref) {
    std::vector<SketchDimensionKind> kinds;
    for (int k = 0; k < kSketchDimensionKindCount; ++k) {
        const SketchDimensionKind kind = static_cast<SketchDimensionKind>(k);
        if (targetMatches(sketch, targetShapeOf(kind), ref)) {
            kinds.push_back(kind);
        }
    }
    return kinds;
}

CadStatus validateSketchDimensionTarget(const CadSketch& sketch, const SketchDimension& dimension) {
    if (!kindInEnum(dimension.kind) || !modeInEnum(dimension.mode)) {
        return CadStatus::SketchDimensionInvalid;
    }
    if (dimension.mode == SketchDimensionMode::Driving
        && !sketchDimensionKindAllowsDriving(dimension.kind)) {
        return CadStatus::SketchDimensionInvalid;
    }
    const TargetShape shape = targetShapeOf(dimension.kind);
    if (!targetMatches(sketch, shape, dimension.first)) {
        return CadStatus::SketchDimensionInvalid;
    }
    if (dimension.kind == SketchDimensionKind::EdgeAngle) {
        if (!targetMatches(sketch, shape, dimension.second)
            || sameCadSketchEdgeRef(dimension.first, dimension.second)) {
            return CadStatus::SketchDimensionInvalid;
        }
    } else if (dimension.second.entityId != kNoSketchEntity
               || dimension.second.edgeLocalIndex != 0u) {
        return CadStatus::SketchDimensionInvalid;
    }
    return CadStatus::Ok;
}

CadStatus validateSketchDimensions(const CadSketch& sketch) {
    if (sketch.dimensions.size() > kMaxSketchDimensions
        || sketch.nextDimensionId == kNoSketchDimension) {
        return CadStatus::SketchDimensionInvalid;
    }
    SketchDimensionId previous = kNoSketchDimension;
    for (size_t i = 0; i < sketch.dimensions.size(); ++i) {
        const SketchDimension& dimension = sketch.dimensions[i];
        // Non-zero, strictly ascending (so unique), and below the high-water
        // mark, so the next mint can never collide.
        if (dimension.id == kNoSketchDimension || dimension.id <= previous
            || dimension.id >= sketch.nextDimensionId) {
            return CadStatus::SketchDimensionInvalid;
        }
        previous = dimension.id;
        const CadStatus why = validateSketchDimensionTarget(sketch, dimension);
        if (why != CadStatus::Ok) {
            return why;
        }
        for (size_t j = 0; j < i; ++j) {
            const SketchDimension& earlier = sketch.dimensions[j];
            const bool duplicate = earlier.kind == dimension.kind
                                   && sameCadSketchEdgeRef(earlier.first, dimension.first)
                                   && sameCadSketchEdgeRef(earlier.second, dimension.second);
            const bool bothDrive = earlier.mode == SketchDimensionMode::Driving
                                   && dimension.mode == SketchDimensionMode::Driving
                                   && earlier.first.entityId == dimension.first.entityId
                                   && drivenDof(earlier.kind) == drivenDof(dimension.kind)
                                   && drivenDof(dimension.kind) != 0;
            if (duplicate || bothDrive) {
                return CadStatus::SketchDimensionConflict;
            }
        }
    }
    return CadStatus::Ok;
}

const SketchDimension* findSketchDimension(const CadSketch& sketch, SketchDimensionId id) {
    for (const SketchDimension& dimension : sketch.dimensions) {
        if (dimension.id == id) {
            return &dimension;
        }
    }
    return nullptr;
}

CadStatus sketchDimensionValue(const CadSketch& sketch, const SketchDimension& dimension,
                               double* outValue) {
    if (outValue == nullptr || validateSketchDimensionTarget(sketch, dimension) != CadStatus::Ok) {
        return CadStatus::SketchDimensionInvalid;
    }
    const SketchEntity* entity = findSketchEntity(sketch, dimension.first.entityId);
    double value = 0.0;
    switch (dimension.kind) {
        case SketchDimensionKind::LineLength:
            value = length(entity->line()->start, entity->line()->end);
            break;
        case SketchDimensionKind::LineAngle:
            value = canonicalDegrees(entity->line()->end.u - entity->line()->start.u,
                                     entity->line()->end.v - entity->line()->start.v);
            break;
        case SketchDimensionKind::LineHorizontal:
            value = std::fabs(entity->line()->end.u - entity->line()->start.u);
            break;
        case SketchDimensionKind::LineVertical:
            value = std::fabs(entity->line()->end.v - entity->line()->start.v);
            break;
        case SketchDimensionKind::RectangleWidth:
            value = entity->rectangle()->width;
            break;
        case SketchDimensionKind::RectangleHeight:
            value = entity->rectangle()->height;
            break;
        case SketchDimensionKind::CircleRadius:
            value = entity->circle()->radius;
            break;
        case SketchDimensionKind::CircleDiameter:
            value = 2.0 * entity->circle()->radius;
            break;
        case SketchDimensionKind::EdgeLength: {
            SketchStraightEdge edge;
            resolveStraightEdge(sketch, dimension.first, &edge);
            value = length(edge.start, edge.end);
            break;
        }
        case SketchDimensionKind::ArcRadius:
        case SketchDimensionKind::ArcSweep: {
            SketchPoint center;
            double radius = 0.0;
            double start = 0.0;
            double sweep = 0.0;
            if (arcGeometry(*entity->arc(), &center, &radius, &start, &sweep) != CadStatus::Ok) {
                return CadStatus::SketchDimensionInvalid;
            }
            value = dimension.kind == SketchDimensionKind::ArcRadius
                            ? radius
                            : std::fabs(sweep) * 180.0 / kPi;
            break;
        }
        case SketchDimensionKind::EdgeAngle: {
            SketchStraightEdge a;
            SketchStraightEdge b;
            resolveStraightEdge(sketch, dimension.first, &a);
            resolveStraightEdge(sketch, dimension.second, &b);
            const double au = a.end.u - a.start.u;
            const double av = a.end.v - a.start.v;
            const double bu = b.end.u - b.start.u;
            const double bv = b.end.v - b.start.v;
            // Unsigned angle between the two directed edges, [0, 180]: atan2 of
            // |cross| and dot is exact at both ends, where acos is not.
            value = std::atan2(std::fabs(au * bv - av * bu), au * bu + av * bv) * 180.0 / kPi;
            break;
        }
    }
    if (!std::isfinite(value)) {
        return CadStatus::SketchDimensionInvalid;
    }
    *outValue = value;
    return CadStatus::Ok;
}

CadStatus addSketchDimension(CadSketch* sketch, SketchDimensionKind kind, SketchDimensionMode mode,
                             const CadSketchEdgeRef& first, const CadSketchEdgeRef& second,
                             SketchDimensionId* outId) {
    if (sketch == nullptr) {
        return CadStatus::SketchDimensionInvalid;
    }
    if (sketch->dimensions.size() >= kMaxSketchDimensions
        || sketch->nextDimensionId == UINT32_MAX) {
        return CadStatus::TooManyEntities;
    }
    CadSketch candidate = *sketch;
    SketchDimension dimension;
    dimension.id = candidate.nextDimensionId;
    dimension.kind = kind;
    dimension.mode = mode;
    dimension.first = first;
    if (kind == SketchDimensionKind::EdgeAngle) {
        dimension.second = second;
    }
    candidate.dimensions.push_back(dimension);
    ++candidate.nextDimensionId;
    const CadStatus why = validateSketchDimensions(candidate);
    if (why != CadStatus::Ok) {
        return why;
    }
    *sketch = std::move(candidate);
    if (outId != nullptr) {
        *outId = dimension.id;
    }
    return CadStatus::Ok;
}

CadStatus removeSketchDimension(CadSketch* sketch, SketchDimensionId id) {
    if (sketch == nullptr) {
        return CadStatus::SketchDimensionInvalid;
    }
    for (size_t i = 0; i < sketch->dimensions.size(); ++i) {
        if (sketch->dimensions[i].id == id) {
            // The annotation goes; the geometry and the high-water mark stay.
            sketch->dimensions.erase(sketch->dimensions.begin() + static_cast<std::ptrdiff_t>(i));
            return CadStatus::Ok;
        }
    }
    return CadStatus::SketchDimensionInvalid;
}

CadStatus applySketchDimensionValue(CadSketch* sketch, SketchDimensionId id, double value) {
    if (sketch == nullptr) {
        return CadStatus::SketchDimensionInvalid;
    }
    const SketchDimension* dimension = findSketchDimension(*sketch, id);
    if (dimension == nullptr
        || validateSketchDimensionTarget(*sketch, *dimension) != CadStatus::Ok) {
        return CadStatus::SketchDimensionInvalid;
    }
    if (dimension->mode != SketchDimensionMode::Driving) {
        return CadStatus::SketchDimensionReadOnly;
    }
    if (!std::isfinite(value)) {
        return CadStatus::SketchDimensionValueInvalid;
    }
    const SketchEntityId entityId = dimension->first.entityId;
    const SketchEntity* entity = findSketchEntity(*sketch, entityId);
    SketchEntity::Payload payload = entity->payload();
    switch (dimension->kind) {
        case SketchDimensionKind::LineLength: {
            if (!usableLength(value)) return CadStatus::SketchDimensionValueInvalid;
            SketchLine line = *entity->line();
            const double current = length(line.start, line.end);
            if (!(current > 0.0)) return CadStatus::ZeroLengthLine;
            const double scale = value / current;
            // P0 FIXED, direction PRESERVED: the second endpoint alone moves.
            line.end = SketchPoint{line.start.u + (line.end.u - line.start.u) * scale,
                                   line.start.v + (line.end.v - line.start.v) * scale};
            payload = line;
            break;
        }
        case SketchDimensionKind::LineAngle: {
            if (!(value > -180.0 && value <= 180.0)) {
                return CadStatus::SketchDimensionValueInvalid;
            }
            SketchLine line = *entity->line();
            const double current = length(line.start, line.end);
            double c = 0.0;
            double s = 0.0;
            exactCosSin(value, &c, &s);
            // P0 FIXED, LENGTH PRESERVED: the second endpoint turns about it.
            line.end = SketchPoint{line.start.u + current * c, line.start.v + current * s};
            payload = line;
            break;
        }
        case SketchDimensionKind::RectangleWidth:
        case SketchDimensionKind::RectangleHeight: {
            if (!usableLength(value)) return CadStatus::SketchDimensionValueInvalid;
            SketchRectangle rectangle = *entity->rectangle();
            if (dimension->kind == SketchDimensionKind::RectangleWidth) {
                rectangle.width = value;
            } else {
                rectangle.height = value;
            }
            payload = rectangle;
            break;
        }
        case SketchDimensionKind::CircleRadius:
        case SketchDimensionKind::CircleDiameter: {
            const double radius =
                    dimension->kind == SketchDimensionKind::CircleRadius ? value : value * 0.5;
            if (!usableLength(value) || !usableLength(radius)) {
                return CadStatus::SketchDimensionValueInvalid;
            }
            SketchCircle circle = *entity->circle();
            circle.radius = radius;
            payload = circle;
            break;
        }
        default:
            return CadStatus::SketchDimensionReadOnly;
    }
    return replaceSketchEntity(sketch, entityId, std::move(payload));
}

std::vector<SketchDimensionId> sketchDimensionsReferencing(const CadSketch& sketch,
                                                           SketchEntityId entityId) {
    std::vector<SketchDimensionId> ids;
    for (const SketchDimension& dimension : sketch.dimensions) {
        if (dimension.first.entityId == entityId
            || (dimension.kind == SketchDimensionKind::EdgeAngle
                && dimension.second.entityId == entityId)) {
            ids.push_back(dimension.id);
        }
    }
    return ids;
}

bool sketchLineLengthDriven(const CadSketch& sketch, SketchEntityId lineId) {
    for (const SketchDimension& dimension : sketch.dimensions) {
        if (dimension.kind == SketchDimensionKind::LineLength
            && dimension.mode == SketchDimensionMode::Driving
            && dimension.first.entityId == lineId) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// The annotation
// ---------------------------------------------------------------------------

namespace {

SketchPoint add(const SketchPoint& p, double du, double dv, double k) {
    return SketchPoint{p.u + du * k, p.v + dv * k};
}

void segment(std::vector<SketchPoint>* out, const SketchPoint& a, const SketchPoint& b) {
    out->push_back(a);
    out->push_back(b);
}

// An open arrowhead whose TIP is at `tip`, pointing along (du, dv) (unit).
void arrow(std::vector<SketchPoint>* out, const SketchPoint& tip, double du, double dv, double size) {
    // Two wings 22 degrees off the shaft, behind the tip.
    const double c = 0.92718385456678742;  // cos 22
    const double s = 0.37460659341591203;  // sin 22
    const double w1u = -(du * c - dv * s);
    const double w1v = -(du * s + dv * c);
    const double w2u = -(du * c + dv * s);
    const double w2v = -(-du * s + dv * c);
    segment(out, tip, add(tip, w1u, w1v, size));
    segment(out, tip, add(tip, w2u, w2v, size));
}

// A linear dimension between `a` and `b`, offset along the unit normal
// (nu, nv): extension lines, the dimension line, and arrows pointing out to the
// extension lines. The label stands clear of the dimension line's midpoint.
void linear(SketchDimensionAnnotation* out, const SketchPoint& a, const SketchPoint& b, double nu,
            double nv, double unit) {
    const double offset = kSketchDimensionOffsetUnits * unit;
    const double gap = kSketchDimensionExtensionGapUnits * unit;
    const double overshoot = kSketchDimensionOvershootUnits * unit;
    const double arrowSize = kSketchDimensionArrowUnits * unit;
    segment(&out->segments, add(a, nu, nv, gap), add(a, nu, nv, offset + overshoot));
    segment(&out->segments, add(b, nu, nv, gap), add(b, nu, nv, offset + overshoot));
    const SketchPoint da = add(a, nu, nv, offset);
    const SketchPoint db = add(b, nu, nv, offset);
    segment(&out->segments, da, db);
    const double l = length(da, db);
    if (l > 0.0) {
        const double du = (db.u - da.u) / l;
        const double dv = (db.v - da.v) / l;
        arrow(&out->segments, db, du, dv, arrowSize);
        arrow(&out->segments, da, -du, -dv, arrowSize);
    }
    const SketchPoint mid{(da.u + db.u) * 0.5, (da.v + db.v) * 0.5};
    out->label = add(mid, nu, nv, kSketchDimensionLabelClearUnits * 0.5 * unit);
    out->attach = SketchPoint{(a.u + b.u) * 0.5, (a.v + b.v) * 0.5};
}

// An angle arc about `centre` from angle `from` sweeping `sweep` radians at
// radius `radius`, with arrows at both ends, and the label beyond its middle.
void angular(SketchDimensionAnnotation* out, const SketchPoint& centre, double from, double sweep,
             double radius, double unit) {
    const uint32_t steps = std::max<uint32_t>(
            6u, static_cast<uint32_t>(std::ceil(std::fabs(sweep) / (kPi / 24.0))));
    SketchPoint previous = add(centre, std::cos(from), std::sin(from), radius);
    for (uint32_t i = 1; i <= steps; ++i) {
        const double t = from + sweep * static_cast<double>(i) / static_cast<double>(steps);
        const SketchPoint next = add(centre, std::cos(t), std::sin(t), radius);
        segment(&out->segments, previous, next);
        previous = next;
    }
    const double arrowSize = kSketchDimensionArrowUnits * unit;
    if (std::fabs(sweep) * radius > 2.5 * arrowSize) {
        const double sign = sweep >= 0.0 ? 1.0 : -1.0;
        const double to = from + sweep;
        // Tangents at the two ends, pointing OUT along the arc.
        arrow(&out->segments, add(centre, std::cos(to), std::sin(to), radius),
              -std::sin(to) * sign, std::cos(to) * sign, arrowSize);
        arrow(&out->segments, add(centre, std::cos(from), std::sin(from), radius),
              std::sin(from) * sign, -std::cos(from) * sign, arrowSize);
    }
    const double middle = from + sweep * 0.5;
    out->label = add(centre, std::cos(middle), std::sin(middle),
                     radius + kSketchDimensionLabelClearUnits * unit);
    out->attach = add(centre, std::cos(middle), std::sin(middle), radius);
}

// A radial leader from `centre` through the curve point at `angle` (radius r),
// arrow at the curve, carried on past it to the label.
void radial(SketchDimensionAnnotation* out, const SketchPoint& centre, double angle, double r,
            double unit, bool diameter) {
    const double du = std::cos(angle);
    const double dv = std::sin(angle);
    const double arrowSize = kSketchDimensionArrowUnits * unit;
    const SketchPoint onCurve = add(centre, du, dv, r);
    const SketchPoint start = diameter ? add(centre, du, dv, -r) : centre;
    segment(&out->segments, start, add(centre, du, dv, r + kSketchDimensionOvershootUnits * 1.5 * unit));
    arrow(&out->segments, onCurve, du, dv, arrowSize);
    if (diameter) {
        arrow(&out->segments, start, -du, -dv, arrowSize);
    }
    out->label = add(centre, du, dv, r + (kSketchDimensionLabelClearUnits + 8.0) * unit);
    out->attach = onCurve;
}

}  // namespace

bool buildSketchDimensionAnnotation(const CadSketch& sketch, const SketchDimension& dimension,
                                    double worldPerUnit, SketchDimensionAnnotation* out) {
    if (out == nullptr || !std::isfinite(worldPerUnit) || worldPerUnit <= 0.0) {
        return false;
    }
    SketchDimensionAnnotation built;
    built.id = dimension.id;
    built.kind = dimension.kind;
    built.mode = dimension.mode;
    if (sketchDimensionValue(sketch, dimension, &built.value) != CadStatus::Ok) {
        return false;
    }
    const double unit = worldPerUnit;
    const SketchEntity* entity = findSketchEntity(sketch, dimension.first.entityId);
    switch (dimension.kind) {
        case SketchDimensionKind::LineLength: {
            const SketchLine& line = *entity->line();
            const double l = length(line.start, line.end);
            // The line's own CCW perpendicular: the side the selected-line
            // dimension has always stood on, so the two never disagree.
            linear(&built, line.start, line.end, -(line.end.v - line.start.v) / l,
                   (line.end.u - line.start.u) / l, unit);
            break;
        }
        case SketchDimensionKind::LineHorizontal:
        case SketchDimensionKind::LineVertical: {
            const SketchLine& line = *entity->line();
            const bool horizontal = dimension.kind == SketchDimensionKind::LineHorizontal;
            // The projection onto one sketch axis, measured on a line BELOW the
            // lower end (horizontal) or RIGHT of the rightmost end (vertical).
            const double base = horizontal ? std::min(line.start.v, line.end.v)
                                           : std::max(line.start.u, line.end.u);
            SketchPoint a = line.start;
            SketchPoint b = line.end;
            if (horizontal) {
                a.v = base; b.v = base;
                // Extension lines from each END down to the measuring line.
                segment(&built.segments, line.start, a);
                segment(&built.segments, line.end, b);
                if (a.u > b.u) std::swap(a, b);
                linear(&built, a, b, 0.0, -1.0, unit);
            } else {
                a.u = base; b.u = base;
                segment(&built.segments, line.start, a);
                segment(&built.segments, line.end, b);
                if (a.v > b.v) std::swap(a, b);
                linear(&built, a, b, 1.0, 0.0, unit);
            }
            break;
        }
        case SketchDimensionKind::LineAngle: {
            const SketchLine& line = *entity->line();
            const double l = length(line.start, line.end);
            const double radius = std::min(l, kSketchDimensionAngleRadiusUnits * unit);
            // The +U reference ray from P0, then the arc from it to the line.
            segment(&built.segments, line.start,
                    add(line.start, 1.0, 0.0, radius + kSketchDimensionOvershootUnits * unit));
            angular(&built, line.start, 0.0, built.value * kPi / 180.0, radius, unit);
            // A SMALL angle's bisector runs almost along the line, where the
            // Length label already stands (on the line's CCW side): the label
            // would be hidden as a collision exactly when the angle is hard to
            // read. So below 45 degrees it stands at the arc's end on the
            // line's CLOCKWISE side, two label clearances off the line -- the
            // drafting habit of writing a cramped angle outside its arc. Two
            // clearances is the least that keeps the two label boxes apart for
            // every line of 90 reference units or more at every such angle.
            if (l > 0.0 && std::fabs(built.value) < 45.0) {
                const double du = (line.end.u - line.start.u) / l;
                const double dv = (line.end.v - line.start.v) / l;
                const double along = radius;
                const double off = 2.0 * kSketchDimensionLabelClearUnits * unit;
                built.label = SketchPoint{line.start.u + du * along + dv * off,
                                          line.start.v + dv * along - du * off};
                // Off the LINE, square to it: the box clears the stroke.
                built.attach = SketchPoint{line.start.u + du * along, line.start.v + dv * along};
            }
            break;
        }
        case SketchDimensionKind::RectangleWidth:
        case SketchDimensionKind::RectangleHeight: {
            const std::vector<SketchPoint> c = rectangleProfilePolygon(*entity->rectangle());
            // Width on the bottom edge, measured below it; height on the right
            // edge, measured right of it: always OUTSIDE the rectangle.
            if (dimension.kind == SketchDimensionKind::RectangleWidth) {
                linear(&built, c[0], c[1], 0.0, -1.0, unit);
            } else {
                linear(&built, c[1], c[2], 1.0, 0.0, unit);
            }
            break;
        }
        case SketchDimensionKind::EdgeLength: {
            SketchStraightEdge edge;
            resolveStraightEdge(sketch, dimension.first, &edge);
            const double l = length(edge.start, edge.end);
            const double du = (edge.end.u - edge.start.u) / l;
            const double dv = (edge.end.v - edge.start.v) / l;
            // A rectangle is counter-clockwise, so its OUTSIDE is the right of
            // each edge; a polyline's dimension stands on its left, the side a
            // line's does.
            if (entity->rectangle() != nullptr) {
                linear(&built, edge.start, edge.end, dv, -du, unit);
            } else {
                linear(&built, edge.start, edge.end, -dv, du, unit);
            }
            break;
        }
        case SketchDimensionKind::CircleRadius:
            radial(&built, entity->circle()->center, kPi * 0.25, entity->circle()->radius, unit,
                   false);
            break;
        case SketchDimensionKind::CircleDiameter:
            // A different direction from the radius leader, so both can stand.
            radial(&built, entity->circle()->center, -kPi * 0.25, entity->circle()->radius, unit,
                   true);
            break;
        case SketchDimensionKind::ArcRadius:
        case SketchDimensionKind::ArcSweep: {
            SketchPoint centre;
            double radius = 0.0;
            double start = 0.0;
            double sweep = 0.0;
            arcGeometry(*entity->arc(), &centre, &radius, &start, &sweep);
            if (dimension.kind == SketchDimensionKind::ArcRadius) {
                const double middle = start + sweep * 0.5;
                radial(&built, centre, middle, radius, unit, false);
                // The Sweep label stands OUTSIDE the arc on this same middle
                // ray, so the radius is written INSIDE, on its own leader: the
                // two never claim one box.
                const double inside = std::max(
                        0.0, radius - (2.0 * kSketchDimensionLabelClearUnits + 6.0) * unit);
                built.label = add(centre, std::cos(middle), std::sin(middle), inside);
                // Inward from the arc: the box clears the curve it measures.
                built.attach = add(centre, std::cos(middle), std::sin(middle), radius);
            } else {
                const double ring = radius + kSketchDimensionOffsetUnits * unit;
                for (double angle : {start, start + sweep}) {
                    segment(&built.segments,
                            add(centre, std::cos(angle), std::sin(angle),
                                radius + kSketchDimensionExtensionGapUnits * unit),
                            add(centre, std::cos(angle), std::sin(angle),
                                ring + kSketchDimensionOvershootUnits * unit));
                }
                angular(&built, centre, start, sweep, ring, unit);
            }
            break;
        }
        case SketchDimensionKind::EdgeAngle: {
            SketchStraightEdge a;
            SketchStraightEdge b;
            resolveStraightEdge(sketch, dimension.first, &a);
            resolveStraightEdge(sketch, dimension.second, &b);
            const double au = a.end.u - a.start.u;
            const double av = a.end.v - a.start.v;
            const double bu = b.end.u - b.start.u;
            const double bv = b.end.v - b.start.v;
            const double cross = au * bv - av * bu;
            const SketchPoint midA{(a.start.u + a.end.u) * 0.5, (a.start.v + a.end.v) * 0.5};
            const SketchPoint midB{(b.start.u + b.end.u) * 0.5, (b.start.v + b.end.v) * 0.5};
            const double scale = std::max(length(a.start, a.end), length(b.start, b.end));
            if (std::fabs(cross) <= 1e-12 * scale * scale) {
                // Parallel: the angle is 0 or 180 and has no vertex; a leader
                // joins the two edges and the label stands on it.
                segment(&built.segments, midA, midB);
                built.label = SketchPoint{(midA.u + midB.u) * 0.5, (midA.v + midB.v) * 0.5};
                built.attach = built.label;
                break;
            }
            // The vertex: where the two infinite lines meet.
            const double t = ((b.start.u - a.start.u) * bv - (b.start.v - a.start.v) * bu) / cross;
            const SketchPoint vertex{a.start.u + au * t, a.start.v + av * t};
            const double from = std::atan2(av, au);
            double sweep = std::atan2(bv, bu) - from;
            while (sweep > kPi) sweep -= 2.0 * kPi;
            while (sweep <= -kPi) sweep += 2.0 * kPi;
            const double radius = kSketchDimensionAngleRadiusUnits * unit;
            // Leaders from the vertex to each edge's midpoint, so the angle
            // reads even where the vertex is off the drawn edges.
            segment(&built.segments, vertex, midA);
            segment(&built.segments, vertex, midB);
            angular(&built, vertex, from, sweep, radius, unit);
            break;
        }
    }
    for (const SketchPoint& p : built.segments) {
        if (!std::isfinite(p.u) || !std::isfinite(p.v)) {
            return false;
        }
    }
    if (!std::isfinite(built.label.u) || !std::isfinite(built.label.v)
        || !std::isfinite(built.attach.u) || !std::isfinite(built.attach.v)) {
        return false;
    }
    *out = std::move(built);
    return true;
}

}  // namespace forgeshape
