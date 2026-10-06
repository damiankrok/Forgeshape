#include "forgeshape_surface.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_workplane.h"

namespace forgeshape {

const char* surfaceStatusName(SurfaceStatus status) {
    switch (status) {
        case SurfaceStatus::Ok: return "Ok";
        case SurfaceStatus::NonFinite: return "NonFinite";
        case SurfaceStatus::OutOfRange: return "OutOfRange";
        case SurfaceStatus::TooManySketches: return "TooManySketches";
        case SurfaceStatus::TooManyFeatures: return "TooManyFeatures";
        case SurfaceStatus::IdInvalid: return "IdInvalid";
        case SurfaceStatus::HighWaterInvalid: return "HighWaterInvalid";
        case SurfaceStatus::NoFeatures: return "NoFeatures";
        case SurfaceStatus::FirstFeatureInvalid: return "FirstFeatureInvalid";
        case SurfaceStatus::UnknownSketch: return "UnknownSketch";
        case SurfaceStatus::SketchInvalid: return "SketchInvalid";
        case SurfaceStatus::PayloadMismatch: return "PayloadMismatch";
        case SurfaceStatus::UnknownFeature: return "UnknownFeature";
        case SurfaceStatus::FeatureOrderInvalid: return "FeatureOrderInvalid";
        case SurfaceStatus::FeatureConsumed: return "FeatureConsumed";
        case SurfaceStatus::RegionInvalid: return "RegionInvalid";
        case SurfaceStatus::CurveNotFound: return "CurveNotFound";
        case SurfaceStatus::CurveSectionEmpty: return "CurveSectionEmpty";
        case SurfaceStatus::CurveChainForked: return "CurveChainForked";
        case SurfaceStatus::DistanceInvalid: return "DistanceInvalid";
        case SurfaceStatus::AxisUnresolved: return "AxisUnresolved";
        case SurfaceStatus::AxisNotStraight: return "AxisNotStraight";
        case SurfaceStatus::AngleInvalid: return "AngleInvalid";
        case SurfaceStatus::ProfileCrossesAxis: return "ProfileCrossesAxis";
        case SurfaceStatus::LoftSectionCount: return "LoftSectionCount";
        case SurfaceStatus::LoftSectionMismatch: return "LoftSectionMismatch";
        case SurfaceStatus::LoftCorrespondenceInvalid: return "LoftCorrespondenceInvalid";
        case SurfaceStatus::LoftSectionsCoincide: return "LoftSectionsCoincide";
        case SurfaceStatus::TrimUnsupportedTarget: return "TrimUnsupportedTarget";
        case SurfaceStatus::TrimNotCoplanar: return "TrimNotCoplanar";
        case SurfaceStatus::TrimRegionInvalid: return "TrimRegionInvalid";
        case SurfaceStatus::TrimRemovesPatch: return "TrimRemovesPatch";
        case SurfaceStatus::StitchNoCompatibleEdges: return "StitchNoCompatibleEdges";
        case SurfaceStatus::StitchGapTooLarge: return "StitchGapTooLarge";
        case SurfaceStatus::StitchNonManifold: return "StitchNonManifold";
        case SurfaceStatus::StitchIncompatibleBoundary: return "StitchIncompatibleBoundary";
        case SurfaceStatus::ThickenUnsupportedForSurfaceType: return "ThickenUnsupportedForSurfaceType";
        case SurfaceStatus::ThickenInvalidThickness: return "ThickenInvalidThickness";
        case SurfaceStatus::ThickenInvalidSolid: return "ThickenInvalidSolid";
        case SurfaceStatus::TooManyPatches: return "TooManyPatches";
        case SurfaceStatus::TooManyTriangles: return "TooManyTriangles";
        case SurfaceStatus::TessellationFailed: return "TessellationFailed";
        case SurfaceStatus::NotSurfaceBody: return "NotSurfaceBody";
        case SurfaceStatus::EditInProgress: return "EditInProgress";
    }
    return "unknown";
}

int surfaceStatusCode(SurfaceStatus status) { return static_cast<int>(status); }

const char* surfaceFeatureKindName(SurfaceFeatureKind kind) {
    switch (kind) {
        case SurfaceFeatureKind::PlanarPatch: return "PlanarPatch";
        case SurfaceFeatureKind::ExtrudedSurface: return "ExtrudedSurface";
        case SurfaceFeatureKind::RevolvedSurface: return "RevolvedSurface";
        case SurfaceFeatureKind::LoftSurface: return "LoftSurface";
        case SurfaceFeatureKind::TrimSurface: return "TrimSurface";
        case SurfaceFeatureKind::Stitch: return "Stitch";
        case SurfaceFeatureKind::Thicken: return "Thicken";
    }
    return "unknown";
}

bool surfaceFeatureKindFromCode(int code, SurfaceFeatureKind* out) {
    if (code < 1 || code > 7) return false;
    if (out != nullptr) *out = static_cast<SurfaceFeatureKind>(code);
    return true;
}

bool sameSurfacePatchId(const SurfacePatchId& a, const SurfacePatchId& b) {
    return a.feature == b.feature && a.ordinal == b.ordinal;
}

bool sameSurfaceEdgeId(const SurfaceEdgeId& a, const SurfaceEdgeId& b) {
    return sameSurfacePatchId(a.patch, b.patch) && a.ordinal == b.ordinal;
}

namespace {

bool sameBits(double a, double b) { return std::memcmp(&a, &b, sizeof(double)) == 0; }

bool sameRegions(const std::vector<ProfileRegionRef>& a, const std::vector<ProfileRegionRef>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!sameProfileRegionRef(a[i], b[i])) return false;
    }
    return true;
}

}  // namespace

bool sameSurfaceSection(const SurfaceSection& a, const SurfaceSection& b) {
    return a.sketchId == b.sketchId && a.curves == b.curves;
}

bool sameSurfaceFeature(const SurfaceFeature& a, const SurfaceFeature& b) {
    return a.id == b.id && a.kind == b.kind && sameSurfaceSection(a.section, b.section)
           && sameRegions(a.regions, b.regions) && sameBits(a.distance, b.distance)
           && a.direction == b.direction && sameCadSketchEdgeRef(a.axis, b.axis)
           && sameBits(a.angleDegrees, b.angleDegrees) && a.revolveDirection == b.revolveDirection
           && sameSurfaceSection(a.sectionB, b.sectionB) && a.reverseB == b.reverseB
           && a.startOffsetB == b.startOffsetB && a.target == b.target && a.keepInside == b.keepInside
           && a.stitchFeatures == b.stitchFeatures && a.source == b.source
           && sameBits(a.thickness, b.thickness);
}

bool sameSurfaceBodyState(const SurfaceBodyState& a, const SurfaceBodyState& b) {
    if (a.sketches.size() != b.sketches.size() || a.features.size() != b.features.size()
        || a.nextSketchId != b.nextSketchId || a.nextFeatureId != b.nextFeatureId) {
        return false;
    }
    for (size_t i = 0; i < a.sketches.size(); ++i) {
        if (a.sketches[i].id != b.sketches[i].id || !sameBits(a.sketches[i].offset, b.sketches[i].offset)
            || !sameCadSketch(a.sketches[i].sketch, b.sketches[i].sketch)) {
            return false;
        }
    }
    for (size_t i = 0; i < a.features.size(); ++i) {
        if (!sameSurfaceFeature(a.features[i], b.features[i])) return false;
    }
    return true;
}

const SurfaceSketchRecord* findSurfaceSketch(const SurfaceBodyState& state, uint32_t id) {
    for (const SurfaceSketchRecord& record : state.sketches) {
        if (record.id == id) return &record;
    }
    return nullptr;
}

const SurfaceFeature* findSurfaceFeature(const SurfaceBodyState& state, SurfaceFeatureId id) {
    for (const SurfaceFeature& feature : state.features) {
        if (feature.id == id) return &feature;
    }
    return nullptr;
}

CadFrame64 surfaceSketchFrame(const SurfaceSketchRecord& record) {
    const WorkplaneFrame w = workplaneFrame(record.sketch.plane);
    CadFrame64 frame;
    frame.u = dvec3FromVec3(w.uAxis);
    frame.v = dvec3FromVec3(w.vAxis);
    frame.n = dvec3FromVec3(w.normal);
    frame.origin = dvec3Scale(frame.n, record.offset);
    return frame;
}

// ---------------------------------------------------------------------------
// Curve sections -> chains
// ---------------------------------------------------------------------------

namespace {

constexpr double kChainCoincidence = 1.0e-9;

struct Chain {
    std::vector<SketchPoint> points;  // a closed chain repeats no closing vertex
    bool closed = false;
    bool straight = true;
    bool single = false;
    SketchEntity entity;
    SketchEntityId firstId = kNoSketchEntity;
};

bool samePoint(const SketchPoint& a, const SketchPoint& b) {
    return std::fabs(a.u - b.u) <= kChainCoincidence && std::fabs(a.v - b.v) <= kChainCoincidence;
}

double signedArea(const std::vector<SketchPoint>& p) { return 0.5 * polygonSignedAreaTwice(p); }

// Reverses a closed loop keeping its first point first.
void reverseLoopKeepingStart(std::vector<SketchPoint>* p) {
    if (p->size() > 2u) std::reverse(p->begin() + 1, p->end());
}

struct OpenPiece {
    SketchEntityId id = kNoSketchEntity;
    std::vector<SketchPoint> points;  // start -> end, inclusive
    bool straight = true;
    SketchEntity entity;
};

SurfaceStatus buildChains(const CadSketch& sketch, const std::vector<SketchEntityId>& curves,
                          std::vector<Chain>* out) {
    out->clear();
    if (curves.empty()) return SurfaceStatus::CurveSectionEmpty;
    std::vector<OpenPiece> open;
    for (SketchEntityId id : curves) {
        const SketchEntity* entity = findSketchEntity(sketch, id);
        if (entity == nullptr || entity->construction()) return SurfaceStatus::CurveNotFound;
        Chain closed;
        closed.firstId = id;
        closed.closed = true;
        closed.single = true;
        closed.entity = *entity;
        if (const SketchRectangle* r = entity->rectangle()) {
            closed.points = rectangleProfilePolygon(*r);
            out->push_back(closed);
            continue;
        }
        if (const SketchCircle* c = entity->circle()) {
            closed.points = circleProfilePolygon(*c);
            closed.straight = false;
            out->push_back(closed);
            continue;
        }
        if (const SketchPolyline* p = entity->polyline()) {
            std::vector<SketchPoint> pts = p->vertices;
            const bool ends = pts.size() > 2u && samePoint(pts.front(), pts.back());
            if (p->closed || ends) {
                if (ends) pts.pop_back();
                if (signedArea(pts) < 0.0) reverseLoopKeepingStart(&pts);
                closed.points = pts;
                out->push_back(closed);
                continue;
            }
            OpenPiece piece{id, pts, true, *entity};
            open.push_back(piece);
            continue;
        }
        OpenPiece piece;
        piece.id = id;
        piece.entity = *entity;
        piece.straight = entity->line() != nullptr;
        if (const SketchLine* line = entity->line()) {
            piece.points = {line->start, line->end};
        } else if (tessellateSketchCurve(*entity, &piece.points) != CadStatus::Ok) {
            return SurfaceStatus::CurveNotFound;
        }
        if (piece.points.size() < 2u) return SurfaceStatus::CurveNotFound;
        open.push_back(piece);
    }
    // Endpoint nodes: coincident AUTHORED ends join; three or more is a fork.
    std::vector<SketchPoint> nodes;
    auto nodeOf = [&nodes](const SketchPoint& p) {
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (samePoint(nodes[i], p)) return i;
        }
        nodes.push_back(p);
        return nodes.size() - 1u;
    };
    std::vector<std::array<size_t, 2>> ends(open.size());
    for (size_t i = 0; i < open.size(); ++i) {
        ends[i] = {nodeOf(open[i].points.front()), nodeOf(open[i].points.back())};
    }
    std::vector<std::vector<size_t>> incident(nodes.size());
    for (size_t i = 0; i < open.size(); ++i) {
        incident[ends[i][0]].push_back(i);
        incident[ends[i][1]].push_back(i);
    }
    for (const std::vector<size_t>& at : incident) {
        if (at.size() > 2u) return SurfaceStatus::CurveChainForked;
    }
    std::vector<uint8_t> used(open.size(), 0u);
    // Pieces are visited in ascending id order (the section is ascending), so a
    // chain is found from its smallest member.
    for (size_t first = 0; first < open.size(); ++first) {
        if (used[first]) continue;
        // Walk back from this piece's start to find whether the chain is open,
        // and where its free end is.
        size_t piece = first;
        bool forward = true;
        bool cycle = false;
        for (size_t guard = 0; guard <= open.size(); ++guard) {
            const size_t startNode = forward ? ends[piece][0] : ends[piece][1];
            size_t previous = open.size();
            for (size_t other : incident[startNode]) {
                if (other != piece) previous = other;
            }
            if (previous == open.size()) break;
            if (previous == first) {
                cycle = true;
                break;
            }
            forward = ends[previous][1] == startNode;
            piece = previous;
        }
        size_t startPiece = first;
        bool startForward = true;
        if (!cycle) {
            // The free end nearer the smaller id starts the walk.
            startPiece = piece;
            startForward = forward;
            // Find the other free end, and start from whichever end belongs to
            // the smaller id (ties: the authored start of that piece).
            size_t walk = startPiece;
            bool dir = startForward;
            size_t endPiece = walk;
            bool endDir = dir;
            for (size_t guard = 0; guard <= open.size(); ++guard) {
                const size_t endNode = dir ? ends[walk][1] : ends[walk][0];
                size_t next = open.size();
                for (size_t other : incident[endNode]) {
                    if (other != walk) next = other;
                }
                endPiece = walk;
                endDir = dir;
                if (next == open.size()) break;
                dir = ends[next][0] == endNode;
                walk = next;
            }
            if (open[endPiece].id < open[startPiece].id
                || (endPiece == startPiece && !startForward)) {
                startPiece = endPiece;
                startForward = !endDir;
            }
        }
        Chain chain;
        chain.closed = cycle;
        chain.firstId = open[first].id;
        size_t count = 0;
        size_t walk = startPiece;
        bool dir = startForward;
        for (size_t guard = 0; guard <= open.size(); ++guard) {
            used[walk] = 1u;
            ++count;
            chain.straight = chain.straight && open[walk].straight;
            std::vector<SketchPoint> pts = open[walk].points;
            if (!dir) std::reverse(pts.begin(), pts.end());
            for (size_t k = chain.points.empty() ? 0u : 1u; k < pts.size(); ++k) chain.points.push_back(pts[k]);
            const size_t endNode = dir ? ends[walk][1] : ends[walk][0];
            size_t next = open.size();
            for (size_t other : incident[endNode]) {
                if (other != walk) next = other;
            }
            if (next == open.size() || next == startPiece) break;
            dir = ends[next][0] == endNode;
            walk = next;
        }
        if (count == 1u) {
            chain.single = true;
            chain.entity = open[startPiece].entity;
        }
        if (chain.closed) {
            if (chain.points.size() > 1u && samePoint(chain.points.front(), chain.points.back())) {
                chain.points.pop_back();
            }
            if (chain.points.size() < 3u) return SurfaceStatus::CurveNotFound;
            if (signedArea(chain.points) < 0.0) reverseLoopKeepingStart(&chain.points);
        }
        out->push_back(chain);
    }
    std::sort(out->begin(), out->end(),
              [](const Chain& a, const Chain& b) { return a.firstId < b.firstId; });
    return SurfaceStatus::Ok;
}

DVec3 framePoint(const CadFrame64& frame, const SketchPoint& p, double w) {
    return cadFramePoint(frame, p.u, p.v, w);
}

// ---------------------------------------------------------------------------
// Patch builders
// ---------------------------------------------------------------------------

SurfaceStatus planarPatchFromLoops(const CadFrame64& frame, const std::vector<std::vector<SketchPoint>>& loops,
                                   SurfacePatch* out) {
    std::vector<uint32_t> tris;
    if (loops.empty()) return SurfaceStatus::RegionInvalid;
    if (loops.size() == 1u) {
        if (triangulateSimplePolygon(loops[0], &tris) != CadStatus::Ok) return SurfaceStatus::TessellationFailed;
    } else if (cadKernelTriangulateRegion(loops, &tris) != CadKernelStatus::Ok) {
        return SurfaceStatus::TessellationFailed;
    }
    out->shape = SurfacePatchShape::Planar;
    out->frame = frame;
    out->loops = loops;
    out->positions.clear();
    for (const std::vector<SketchPoint>& loop : loops) {
        SurfaceBoundaryEdge edge;
        edge.closed = true;
        for (const SketchPoint& p : loop) {
            const DVec3 x = framePoint(frame, p, 0.0);
            out->positions.push_back(x);
            edge.points.push_back(x);
        }
        out->edges.push_back(edge);
    }
    out->triangles = tris;
    return SurfaceStatus::Ok;
}

void ruledPatch(const std::vector<DVec3>& a, const std::vector<DVec3>& b, bool closed, SurfacePatch* out) {
    const uint32_t n = static_cast<uint32_t>(a.size());
    out->positions = a;
    out->positions.insert(out->positions.end(), b.begin(), b.end());
    const uint32_t segments = closed ? n : n - 1u;
    for (uint32_t i = 0; i < segments; ++i) {
        const uint32_t j = (i + 1u) % n;
        out->triangles.insert(out->triangles.end(), {i, j, n + j, i, n + j, n + i});
    }
    SurfaceBoundaryEdge first;
    first.points = a;
    first.closed = closed;
    SurfaceBoundaryEdge second;
    second.points = b;
    second.closed = closed;
    out->edges.push_back(first);
    out->edges.push_back(second);
    if (!closed) {
        out->edges.push_back(SurfaceBoundaryEdge{{a.front(), b.front()}, false});
        out->edges.push_back(SurfaceBoundaryEdge{{a.back(), b.back()}, false});
    }
}

SurfaceStatus resolveAxis(const CadSketch& sketch, const CadSketchEdgeRef& ref, SketchPoint* a0,
                          SketchPoint* a1) {
    const SketchEntity* entity = findSketchEntity(sketch, ref.entityId);
    if (entity == nullptr) return SurfaceStatus::AxisUnresolved;
    if (const SketchLine* line = entity->line()) {
        if (ref.edgeLocalIndex != 0u) return SurfaceStatus::AxisUnresolved;
        *a0 = line->start;
        *a1 = line->end;
    } else if (const SketchPolyline* p = entity->polyline()) {
        const size_t n = p->vertices.size();
        const size_t segments = p->closed ? n : n - 1u;
        if (ref.edgeLocalIndex >= segments) return SurfaceStatus::AxisUnresolved;
        *a0 = p->vertices[ref.edgeLocalIndex];
        *a1 = p->vertices[(ref.edgeLocalIndex + 1u) % n];
    } else if (const SketchRectangle* r = entity->rectangle()) {
        if (ref.edgeLocalIndex >= 4u) return SurfaceStatus::AxisUnresolved;
        const std::vector<SketchPoint> corners = rectangleProfilePolygon(*r);
        *a0 = corners[ref.edgeLocalIndex];
        *a1 = corners[(ref.edgeLocalIndex + 1u) % 4u];
    } else {
        return SurfaceStatus::AxisNotStraight;
    }
    if (std::hypot(a1->u - a0->u, a1->v - a0->v) <= kMinSurfaceDistanceMeters) {
        return SurfaceStatus::AxisUnresolved;
    }
    return SurfaceStatus::Ok;
}

DVec3 rotateAbout(const DVec3& p, const DVec3& origin, const DVec3& axis, double radians) {
    const DVec3 v = dvec3Sub(p, origin);
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    const DVec3 cross = dvec3Cross(axis, v);
    const double dot = dvec3Dot(axis, v);
    const DVec3 r = dvec3Add(dvec3Add(dvec3Scale(v, c), dvec3Scale(cross, s)), dvec3Scale(axis, dot * (1.0 - c)));
    return dvec3Add(origin, r);
}

SurfaceStatus revolvedPatch(const CadFrame64& frame, const Chain& chain, const SketchPoint& a0,
                            const SketchPoint& a1, double angleDegrees, RevolveDirection direction,
                            SurfacePatch* out) {
    const double du = a1.u - a0.u;
    const double dv = a1.v - a0.v;
    const double len = std::hypot(du, dv);
    const double eps = 1.0e-9;
    std::vector<uint8_t> onAxis(chain.points.size(), 0u);
    bool positive = false;
    bool negative = false;
    for (size_t i = 0; i < chain.points.size(); ++i) {
        const double s = (du * (chain.points[i].v - a0.v) - dv * (chain.points[i].u - a0.u)) / len;
        if (s > eps) positive = true;
        else if (s < -eps) negative = true;
        else onAxis[i] = 1u;
    }
    if (positive && negative) return SurfaceStatus::ProfileCrossesAxis;
    const bool full = angleDegrees == 360.0;
    const uint32_t steps = full ? kSurfaceRevolveFullTurnSteps
                                : std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(
                                          kSurfaceRevolveFullTurnSteps * angleDegrees / 360.0)));
    const double sign = direction == RevolveDirection::Positive ? 1.0 : -1.0;
    const double total = sign * angleDegrees * 3.14159265358979323846 / 180.0;
    const DVec3 origin = framePoint(frame, a0, 0.0);
    DVec3 axis;
    if (!dvec3Normalized(dvec3Sub(framePoint(frame, a1, 0.0), origin), &axis)) return SurfaceStatus::AxisUnresolved;
    const uint32_t columns = full ? steps : steps + 1u;
    // Vertex index of profile point j at step k; an on-axis point is one apex.
    std::vector<uint32_t> base(chain.points.size());
    out->positions.clear();
    for (size_t j = 0; j < chain.points.size(); ++j) {
        base[j] = static_cast<uint32_t>(out->positions.size());
        const DVec3 p = framePoint(frame, chain.points[j], 0.0);
        if (onAxis[j]) {
            out->positions.push_back(p);
            continue;
        }
        for (uint32_t k = 0; k < columns; ++k) {
            const double t = total * static_cast<double>(k) / static_cast<double>(steps);
            out->positions.push_back(k == 0u ? p : rotateAbout(p, origin, axis, t));
        }
    }
    auto at = [&](size_t j, uint32_t k) {
        return onAxis[j] ? base[j] : base[j] + (k % columns);
    };
    const size_t n = chain.points.size();
    const size_t segments = chain.closed ? n : n - 1u;
    for (size_t j = 0; j < segments; ++j) {
        const size_t j1 = (j + 1u) % n;
        if (onAxis[j] && onAxis[j1]) continue;
        for (uint32_t k = 0; k < steps; ++k) {
            const uint32_t k1 = k + 1u;
            if (onAxis[j]) {
                out->triangles.insert(out->triangles.end(), {at(j, k), at(j1, k), at(j1, k1)});
            } else if (onAxis[j1]) {
                out->triangles.insert(out->triangles.end(), {at(j, k), at(j1, k), at(j, k1)});
            } else {
                out->triangles.insert(out->triangles.end(),
                                      {at(j, k), at(j1, k), at(j1, k1), at(j, k), at(j1, k1), at(j, k1)});
            }
        }
    }
    out->shape = SurfacePatchShape::Revolved;
    if (!full) {
        for (uint32_t k : {0u, steps}) {
            SurfaceBoundaryEdge profile;
            profile.closed = chain.closed;
            for (size_t j = 0; j < n; ++j) profile.points.push_back(out->positions[at(j, k)]);
            out->edges.push_back(profile);
        }
    }
    if (!chain.closed) {
        for (size_t j : {size_t{0}, n - 1u}) {
            if (onAxis[j]) continue;
            SurfaceBoundaryEdge arc;
            arc.closed = full;
            for (uint32_t k = 0; k < columns; ++k) arc.points.push_back(out->positions[at(j, k)]);
            out->edges.push_back(arc);
        }
    }
    return SurfaceStatus::Ok;
}

// Resamples a chain by arc length to `count` points (open: both ends kept;
// closed: from point 0 around the loop).
std::vector<DVec3> resample(const std::vector<DVec3>& points, bool closed, uint32_t count) {
    std::vector<DVec3> pts = points;
    if (closed) pts.push_back(points.front());
    std::vector<double> length(pts.size(), 0.0);
    for (size_t i = 1; i < pts.size(); ++i) {
        length[i] = length[i - 1] + std::sqrt(dvec3Dot(dvec3Sub(pts[i], pts[i - 1]), dvec3Sub(pts[i], pts[i - 1])));
    }
    const double total = length.back();
    std::vector<DVec3> out;
    size_t seg = 1;
    for (uint32_t i = 0; i < count; ++i) {
        const double t = total * static_cast<double>(i) / static_cast<double>(closed ? count : count - 1u);
        while (seg + 1 < pts.size() && length[seg] < t) ++seg;
        const double span = length[seg] - length[seg - 1];
        const double f = span > 0.0 ? std::min(1.0, std::max(0.0, (t - length[seg - 1]) / span)) : 0.0;
        if (!closed && i + 1u == count) {
            out.push_back(points.back());
        } else if (i == 0u) {
            out.push_back(points.front());
        } else {
            out.push_back(dvec3Add(pts[seg - 1], dvec3Scale(dvec3Sub(pts[seg], pts[seg - 1]), f)));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

bool finiteIn(double value, double lo, double hi) { return std::isfinite(value) && value >= lo && value <= hi; }

bool sectionCanonical(const SurfaceSection& s) {
    if (s.curves.size() > kMaxSurfaceSectionCurves) return false;
    for (size_t i = 0; i < s.curves.size(); ++i) {
        if (s.curves[i] == kNoSketchEntity) return false;
        if (i > 0 && s.curves[i] <= s.curves[i - 1]) return false;
    }
    return true;
}

// Every field the kind does not use keeps its default: one feature, one form.
bool payloadCanonical(const SurfaceFeature& f) {
    const SurfaceFeature d;
    const bool usesSection = f.kind != SurfaceFeatureKind::Stitch && f.kind != SurfaceFeatureKind::Thicken;
    const bool usesCurves = f.kind == SurfaceFeatureKind::ExtrudedSurface
                            || f.kind == SurfaceFeatureKind::RevolvedSurface
                            || f.kind == SurfaceFeatureKind::LoftSurface;
    const bool usesRegions = f.kind == SurfaceFeatureKind::PlanarPatch || f.kind == SurfaceFeatureKind::TrimSurface;
    if (!usesSection && !sameSurfaceSection(f.section, d.section)) return false;
    if (usesSection && f.section.sketchId == 0u) return false;
    if (!usesCurves && !f.section.curves.empty()) return false;
    if (usesCurves && f.section.curves.empty()) return false;
    if (!sectionCanonical(f.section) || !sectionCanonical(f.sectionB)) return false;
    if (!usesRegions && !f.regions.empty()) return false;
    if (usesRegions && f.regions.empty()) return false;
    if (f.kind != SurfaceFeatureKind::ExtrudedSurface
        && (!sameBits(f.distance, d.distance) || f.direction != d.direction)) {
        return false;
    }
    if (f.kind != SurfaceFeatureKind::RevolvedSurface
        && (!sameCadSketchEdgeRef(f.axis, d.axis) || !sameBits(f.angleDegrees, d.angleDegrees)
            || f.revolveDirection != d.revolveDirection)) {
        return false;
    }
    if (f.kind != SurfaceFeatureKind::LoftSurface
        && (!sameSurfaceSection(f.sectionB, d.sectionB) || f.reverseB != d.reverseB
            || f.startOffsetB != d.startOffsetB)) {
        return false;
    }
    if (f.kind == SurfaceFeatureKind::LoftSurface && (f.sectionB.sketchId == 0u || f.sectionB.curves.empty())) {
        return false;
    }
    if (f.kind != SurfaceFeatureKind::TrimSurface && (f.target != d.target || f.keepInside != d.keepInside)) {
        return false;
    }
    if (f.kind != SurfaceFeatureKind::Stitch && !f.stitchFeatures.empty()) return false;
    if (f.kind != SurfaceFeatureKind::Thicken && (f.source != d.source || !sameBits(f.thickness, d.thickness))) {
        return false;
    }
    return true;
}

}  // namespace

SurfaceStatus validateSurfaceBodyState(const SurfaceBodyState& state, SurfaceFeatureId* outFailed) {
    if (outFailed != nullptr) *outFailed = kNoSurfaceFeature;
    if (state.sketches.size() > kMaxSurfaceSketches) return SurfaceStatus::TooManySketches;
    if (state.features.size() > kMaxSurfaceFeatures) return SurfaceStatus::TooManyFeatures;
    if (state.features.empty()) return SurfaceStatus::NoFeatures;
    uint32_t last = 0;
    for (const SurfaceSketchRecord& record : state.sketches) {
        if (record.id == 0u || record.id <= last) return SurfaceStatus::IdInvalid;
        last = record.id;
        if (!finiteIn(record.offset, -kMaxSurfaceDistanceMeters, kMaxSurfaceDistanceMeters)) {
            return SurfaceStatus::OutOfRange;
        }
        if (record.sketch.hasFaceSupport || validateCadSketch(record.sketch) != CadStatus::Ok) {
            return SurfaceStatus::SketchInvalid;
        }
    }
    if (state.nextSketchId <= last) return SurfaceStatus::HighWaterInvalid;
    last = 0;
    for (const SurfaceFeature& f : state.features) {
        if (outFailed != nullptr) *outFailed = f.id;
        if (idOf(f.id) == 0u || idOf(f.id) <= last) return SurfaceStatus::IdInvalid;
        last = idOf(f.id);
        if (!surfaceFeatureKindFromCode(static_cast<int>(f.kind), nullptr)) return SurfaceStatus::PayloadMismatch;
        if (!payloadCanonical(f)) return SurfaceStatus::PayloadMismatch;
        auto sketchKnown = [&state](uint32_t id) { return findSurfaceSketch(state, id) != nullptr; };
        if (f.kind != SurfaceFeatureKind::Stitch && f.kind != SurfaceFeatureKind::Thicken
            && !sketchKnown(f.section.sketchId)) {
            return SurfaceStatus::UnknownSketch;
        }
        if (f.kind == SurfaceFeatureKind::LoftSurface && !sketchKnown(f.sectionB.sketchId)) {
            return SurfaceStatus::UnknownSketch;
        }
        auto earlier = [&state, &f](SurfaceFeatureId ref) {
            const SurfaceFeature* other = findSurfaceFeature(state, ref);
            if (other == nullptr) return SurfaceStatus::UnknownFeature;
            return idOf(other->id) < idOf(f.id) ? SurfaceStatus::Ok : SurfaceStatus::FeatureOrderInvalid;
        };
        switch (f.kind) {
            case SurfaceFeatureKind::ExtrudedSurface:
                if (!finiteIn(f.distance, kMinSurfaceDistanceMeters, kMaxSurfaceDistanceMeters)) {
                    return SurfaceStatus::DistanceInvalid;
                }
                break;
            case SurfaceFeatureKind::RevolvedSurface:
                if (!finiteIn(f.angleDegrees, kMinSurfaceAngleDegrees, kMaxSurfaceAngleDegrees)) {
                    return SurfaceStatus::AngleInvalid;
                }
                if (f.axis.entityId == kNoSketchEntity) return SurfaceStatus::AxisUnresolved;
                break;
            case SurfaceFeatureKind::TrimSurface: {
                const SurfaceStatus why = earlier(f.target);
                if (why != SurfaceStatus::Ok) return why;
                break;
            }
            case SurfaceFeatureKind::Stitch: {
                if (f.stitchFeatures.empty() || f.stitchFeatures.size() > kMaxSurfaceStitchFeatures) {
                    return SurfaceStatus::PayloadMismatch;
                }
                for (size_t i = 0; i < f.stitchFeatures.size(); ++i) {
                    if (i > 0 && idOf(f.stitchFeatures[i]) <= idOf(f.stitchFeatures[i - 1])) {
                        return SurfaceStatus::PayloadMismatch;
                    }
                    const SurfaceStatus why = earlier(f.stitchFeatures[i]);
                    if (why != SurfaceStatus::Ok) return why;
                }
                break;
            }
            case SurfaceFeatureKind::Thicken: {
                if (!std::isfinite(f.thickness) || std::fabs(f.thickness) < kMinSurfaceDistanceMeters
                    || std::fabs(f.thickness) > kMaxSurfaceDistanceMeters) {
                    return SurfaceStatus::ThickenInvalidThickness;
                }
                const SurfaceStatus why = earlier(f.source);
                if (why != SurfaceStatus::Ok) return why;
                break;
            }
            default:
                break;
        }
    }
    const SurfaceFeatureKind firstKind = state.features.front().kind;
    if (firstKind == SurfaceFeatureKind::TrimSurface || firstKind == SurfaceFeatureKind::Stitch
        || firstKind == SurfaceFeatureKind::Thicken) {
        if (outFailed != nullptr) *outFailed = state.features.front().id;
        return SurfaceStatus::FirstFeatureInvalid;
    }
    if (state.nextFeatureId <= last) return SurfaceStatus::HighWaterInvalid;
    if (outFailed != nullptr) *outFailed = kNoSurfaceFeature;
    return SurfaceStatus::Ok;
}

// ---------------------------------------------------------------------------
// Regeneration
// ---------------------------------------------------------------------------

namespace {

struct Live {
    std::vector<SurfacePatch> patches;
    std::vector<uint8_t> consumed;  // per feature index
    std::vector<SurfaceStitchPair> stitches;
    CadSolid solid;
};

std::vector<std::vector<SketchPoint>> regionLoops(const CadSketch& sketch, const std::vector<ProfileRegionRef>& regions,
                                                  SurfaceStatus* why, std::vector<std::vector<std::vector<SketchPoint>>>* components) {
    const SketchRegionExtraction extraction = extractSketchRegions(sketch);
    std::vector<std::vector<SketchPoint>> all;
    if (validateRegionSelection(extraction, regions) != CadStatus::Ok) {
        *why = SurfaceStatus::RegionInvalid;
        return all;
    }
    for (const SketchRegionComponent& component : mergeSelectedRegions(extraction, regions)) {
        const std::vector<std::vector<SketchPoint>> loops = sketchComponentLoops(extraction, component);
        if (components != nullptr) components->push_back(loops);
        all.insert(all.end(), loops.begin(), loops.end());
    }
    *why = all.empty() ? SurfaceStatus::RegionInvalid : SurfaceStatus::Ok;
    return all;
}

bool evenOddInside(const SketchPoint& p, const std::vector<std::vector<SketchPoint>>& loops) {
    bool inside = false;
    for (const std::vector<SketchPoint>& loop : loops) {
        if (sketchPointStrictlyInside(p, loop)) inside = !inside;
    }
    return inside;
}

bool sameFramePlane(const CadFrame64& a, const CadFrame64& b) {
    auto same = [](const DVec3& x, const DVec3& y) {
        return std::fabs(x.x - y.x) <= 1e-12 && std::fabs(x.y - y.y) <= 1e-12 && std::fabs(x.z - y.z) <= 1e-12;
    };
    return same(a.u, b.u) && same(a.v, b.v) && same(a.n, b.n) && same(a.origin, b.origin);
}

SurfaceStatus trimPatch(const SurfacePatch& target, const std::vector<std::vector<SketchPoint>>& cutter,
                        const CadSketch& trimSketch, bool keepInside,
                        std::vector<std::vector<std::vector<SketchPoint>>>* out) {
    // One sketch for the arrangement: the patch's loops as closed polylines
    // (ids 1..k) beside the trim sketch's own material, renumbered after them.
    CadSketch combined;
    combined.plane = trimSketch.plane;
    SketchEntityId next = 1;
    for (const std::vector<SketchPoint>& loop : target.loops) {
        if (loop.size() > kMaxPolylineVertices) return SurfaceStatus::TrimRegionInvalid;
        SketchPolyline polyline;
        polyline.vertices = loop;
        polyline.closed = true;
        combined.entities.emplace_back(next++, polyline);
    }
    const CadSketch material = cadSketchMaterialView(trimSketch);
    for (const SketchEntity& entity : material.entities) {
        combined.entities.emplace_back(next++, entity.payload());
    }
    combined.nextEntityId = next;
    if (combined.entities.size() > kMaxSketchEntities) return SurfaceStatus::TrimRegionInvalid;
    const SketchArrangement arrangement = deriveSketchArrangement(combined);
    if (arrangement.status != ArrangementStatus::Ok) return SurfaceStatus::TrimRegionInvalid;
    std::vector<size_t> kept;
    for (size_t i = 0; i < arrangement.faces.size(); ++i) {
        std::vector<PlanarProfileComponent> one;
        if (mergePlanarFaces(arrangement, {i}, &one) != ArrangementStatus::Ok || one.empty()) {
            return SurfaceStatus::TrimRegionInvalid;
        }
        std::vector<std::vector<SketchPoint>> loops{one[0].outer.polygon};
        for (const PlanarProfileLoop& hole : one[0].holes) loops.push_back(hole.polygon);
        SketchPoint inner;
        if (!sketchLoopsInteriorPoint(loops, &inner)) return SurfaceStatus::TrimRegionInvalid;
        if (evenOddInside(inner, target.loops) && evenOddInside(inner, cutter) == keepInside) kept.push_back(i);
    }
    if (kept.empty()) return SurfaceStatus::TrimRemovesPatch;
    std::vector<PlanarProfileComponent> merged;
    if (mergePlanarFaces(arrangement, kept, &merged) != ArrangementStatus::Ok) {
        return SurfaceStatus::TrimRegionInvalid;
    }
    for (const PlanarProfileComponent& component : merged) {
        std::vector<std::vector<SketchPoint>> loops{component.outer.polygon};
        for (const PlanarProfileLoop& hole : component.holes) loops.push_back(hole.polygon);
        out->push_back(loops);
    }
    return SurfaceStatus::Ok;
}

// The Hausdorff distance between two boundary polylines, both ways.
double pointSegment(const DVec3& p, const DVec3& a, const DVec3& b) {
    const DVec3 ab = dvec3Sub(b, a);
    const double l2 = dvec3Dot(ab, ab);
    double t = l2 > 0.0 ? dvec3Dot(dvec3Sub(p, a), ab) / l2 : 0.0;
    t = std::max(0.0, std::min(1.0, t));
    const DVec3 d = dvec3Sub(p, dvec3Add(a, dvec3Scale(ab, t)));
    return std::sqrt(dvec3Dot(d, d));
}

double pointPolyline(const DVec3& p, const SurfaceBoundaryEdge& e) {
    double best = 1e300;
    const size_t n = e.points.size();
    if (n == 1u) return pointSegment(p, e.points[0], e.points[0]);
    const size_t segments = e.closed ? n : n - 1u;
    for (size_t i = 0; i < segments; ++i) best = std::min(best, pointSegment(p, e.points[i], e.points[(i + 1u) % n]));
    return best;
}

double hausdorff(const SurfaceBoundaryEdge& a, const SurfaceBoundaryEdge& b) {
    double d = 0.0;
    for (const DVec3& p : a.points) d = std::max(d, pointPolyline(p, b));
    for (const DVec3& p : b.points) d = std::max(d, pointPolyline(p, a));
    return d;
}

double distance(const DVec3& a, const DVec3& b) {
    const DVec3 d = dvec3Sub(a, b);
    return std::sqrt(dvec3Dot(d, d));
}

SurfaceStatus stitch(Live* live, const std::vector<size_t>& patchIndices) {
    struct EdgeRef {
        size_t patch;
        uint32_t ordinal;
    };
    std::vector<EdgeRef> edges;
    for (size_t p : patchIndices) {
        for (uint32_t e = 0; e < live->patches[p].edges.size(); ++e) edges.push_back(EdgeRef{p, e});
    }
    std::vector<std::pair<size_t, size_t>> matches;
    bool gap = false;
    bool incompatible = false;
    for (size_t i = 0; i < edges.size(); ++i) {
        for (size_t j = i + 1; j < edges.size(); ++j) {
            if (edges[i].patch == edges[j].patch) continue;
            const SurfaceBoundaryEdge& a = live->patches[edges[i].patch].edges[edges[i].ordinal];
            const SurfaceBoundaryEdge& b = live->patches[edges[j].patch].edges[edges[j].ordinal];
            if (a.closed != b.closed) continue;
            const double h = hausdorff(a, b);
            if (h <= kSurfaceStitchToleranceMeters) {
                matches.emplace_back(i, j);
                continue;
            }
            if (!a.closed) {
                const bool ends = (distance(a.points.front(), b.points.front()) <= kSurfaceStitchToleranceMeters
                                   && distance(a.points.back(), b.points.back()) <= kSurfaceStitchToleranceMeters)
                                  || (distance(a.points.front(), b.points.back()) <= kSurfaceStitchToleranceMeters
                                      && distance(a.points.back(), b.points.front()) <= kSurfaceStitchToleranceMeters);
                if (ends) {
                    incompatible = true;
                    continue;
                }
            }
            if (h <= kSurfaceStitchSearchMeters) gap = true;
        }
    }
    std::vector<int> uses(edges.size(), 0);
    for (const auto& m : matches) {
        ++uses[m.first];
        ++uses[m.second];
    }
    for (int u : uses) {
        if (u > 1) return SurfaceStatus::StitchNonManifold;
    }
    if (incompatible) return SurfaceStatus::StitchIncompatibleBoundary;
    if (gap) return SurfaceStatus::StitchGapTooLarge;
    if (matches.empty()) return SurfaceStatus::StitchNoCompatibleEdges;
    for (const auto& m : matches) {
        SurfaceStitchPair pair;
        pair.a = SurfaceEdgeId{live->patches[edges[m.first].patch].id, edges[m.first].ordinal};
        pair.b = SurfaceEdgeId{live->patches[edges[m.second].patch].id, edges[m.second].ordinal};
        live->stitches.push_back(pair);
    }
    return SurfaceStatus::Ok;
}

// A prism over loops (outer first, holes after; each counter-clockwise in
// (u, v)) between w0 < w1 on `frame`, appended outward-wound to `solid`.
SurfaceStatus appendPrism(const CadFrame64& frame, const std::vector<std::vector<SketchPoint>>& loops, double w0,
                          double w1, CadSolid* solid) {
    std::vector<uint32_t> cap;
    if (loops.size() == 1u) {
        if (triangulateSimplePolygon(loops[0], &cap) != CadStatus::Ok) return SurfaceStatus::ThickenInvalidSolid;
    } else if (cadKernelTriangulateRegion(loops, &cap) != CadKernelStatus::Ok) {
        return SurfaceStatus::ThickenInvalidSolid;
    }
    CadSolid prism;
    uint32_t total = 0;
    for (const std::vector<SketchPoint>& loop : loops) total += static_cast<uint32_t>(loop.size());
    for (double w : {w0, w1}) {
        for (const std::vector<SketchPoint>& loop : loops) {
            for (const SketchPoint& p : loop) {
                const DVec3 x = framePoint(frame, p, w);
                prism.positions.insert(prism.positions.end(), {x.x, x.y, x.z});
            }
        }
    }
    const uint32_t lower = 0;
    const uint32_t upper = total;
    for (size_t t = 0; t + 2 < cap.size(); t += 3) {
        prism.indices.insert(prism.indices.end(), {upper + cap[t], upper + cap[t + 1], upper + cap[t + 2]});
        prism.indices.insert(prism.indices.end(), {lower + cap[t], lower + cap[t + 2], lower + cap[t + 1]});
    }
    uint32_t offset = 0;
    for (size_t l = 0; l < loops.size(); ++l) {
        const uint32_t n = static_cast<uint32_t>(loops[l].size());
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t j = (i + 1u) % n;
            const uint32_t li = lower + offset + i, lj = lower + offset + j;
            const uint32_t ui = upper + offset + i, uj = upper + offset + j;
            if (l == 0u) prism.indices.insert(prism.indices.end(), {li, lj, uj, li, uj, ui});
            else prism.indices.insert(prism.indices.end(), {li, uj, lj, li, ui, uj});
        }
        offset += n;
    }
    prism.faceTags.assign(prism.indices.size() / 3u, 0u);
    CadSolidMeasure measure;
    if (cadKernelValidateSolid(prism, &measure) != CadKernelStatus::Ok || !(measure.volume > 0.0)) {
        return SurfaceStatus::ThickenInvalidSolid;
    }
    const uint32_t base = solid->vertexCount();
    solid->positions.insert(solid->positions.end(), prism.positions.begin(), prism.positions.end());
    for (uint32_t index : prism.indices) solid->indices.push_back(base + index);
    solid->faceTags.insert(solid->faceTags.end(), prism.faceTags.begin(), prism.faceTags.end());
    return SurfaceStatus::Ok;
}

bool polygonSimple(const std::vector<SketchPoint>& p) {
    const size_t n = p.size();
    if (n < 3u) return false;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            if (j == i + 1 || (i == 0 && j == n - 1)) continue;
            if (sketchSegmentsIntersect(p[i], p[(i + 1) % n], p[j], p[(j + 1) % n])) return false;
        }
    }
    return std::fabs(polygonSignedAreaTwice(p)) > 0.0;
}

// The mitred offset of a polyline by `t` along its LEFT normal (a closed loop
// counter-clockwise: positive t moves it inward... see the callers).
bool offsetPolyline(const std::vector<SketchPoint>& p, bool closed, double t, std::vector<SketchPoint>* out) {
    const size_t n = p.size();
    std::vector<SketchPoint> normals;
    const size_t segments = closed ? n : n - 1u;
    for (size_t i = 0; i < segments; ++i) {
        const SketchPoint& a = p[i];
        const SketchPoint& b = p[(i + 1u) % n];
        const double du = b.u - a.u;
        const double dv = b.v - a.v;
        const double len = std::hypot(du, dv);
        if (!(len > 0.0)) return false;
        normals.push_back(SketchPoint{-dv / len, du / len});
    }
    out->clear();
    for (size_t i = 0; i < n; ++i) {
        SketchPoint m;
        if (!closed && i == 0u) {
            m = normals.front();
        } else if (!closed && i + 1u == n) {
            m = normals.back();
        } else {
            const SketchPoint& n1 = normals[(i + segments - 1u) % segments];
            const SketchPoint& n2 = normals[i % segments];
            const double su = n1.u + n2.u;
            const double sv = n1.v + n2.v;
            const double sl = std::hypot(su, sv);
            if (!(sl > 0.0)) return false;
            const double cosHalf = (su * n1.u + sv * n1.v) / sl;
            if (cosHalf < 0.1) return false;  // too sharp to mitre honestly
            m = SketchPoint{su / sl / cosHalf, sv / sl / cosHalf};
        }
        out->push_back(SketchPoint{p[i].u + m.u * t, p[i].v + m.v * t});
    }
    return true;
}

SurfaceStatus thickenPatch(const SurfacePatch& patch, double t, CadSolid* solid) {
    if (patch.shape == SurfacePatchShape::Planar) {
        return appendPrism(patch.frame, patch.loops, std::min(0.0, t), std::max(0.0, t), solid);
    }
    if (patch.shape != SurfacePatchShape::Ruled) return SurfaceStatus::ThickenUnsupportedForSurfaceType;
    std::vector<std::vector<SketchPoint>> loops;
    if (patch.chainSingleCurve && patch.chainCurveKind == SketchEntityKind::Circle) {
        const SketchCircle* c = patch.chainEntity.circle();
        const double r1 = c->radius + t;
        if (!(r1 > kMinSurfaceDistanceMeters)) return SurfaceStatus::ThickenInvalidSolid;
        SketchCircle inner = *c;
        SketchCircle outer = *c;
        inner.radius = std::min(c->radius, r1);
        outer.radius = std::max(c->radius, r1);
        loops = {circleProfilePolygon(outer), circleProfilePolygon(inner)};
    } else if (patch.chainSingleCurve && patch.chainCurveKind == SketchEntityKind::Arc) {
        SketchPoint centre;
        double radius = 0.0, start = 0.0, sweep = 0.0;
        if (arcGeometry(*patch.chainEntity.arc(), &centre, &radius, &start, &sweep) != CadStatus::Ok) {
            return SurfaceStatus::ThickenInvalidSolid;
        }
        const double r1 = radius + t;
        if (!(r1 > kMinSurfaceDistanceMeters)) return SurfaceStatus::ThickenInvalidSolid;
        std::vector<SketchPoint> ring = patch.chain;
        std::vector<SketchPoint> other;
        for (auto it = ring.rbegin(); it != ring.rend(); ++it) {
            const double du = it->u - centre.u;
            const double dv = it->v - centre.v;
            other.push_back(SketchPoint{centre.u + du * r1 / radius, centre.v + dv * r1 / radius});
        }
        ring.insert(ring.end(), other.begin(), other.end());
        if (polygonSignedAreaTwice(ring) < 0.0) std::reverse(ring.begin(), ring.end());
        if (!polygonSimple(ring)) return SurfaceStatus::ThickenInvalidSolid;
        loops = {ring};
    } else if (patch.chainStraight) {
        std::vector<SketchPoint> offset;
        if (patch.chainClosed) {
            // A counter-clockwise loop's outward normal is its RIGHT normal, so
            // a positive thickness grows outward: offset by -t on the left.
            if (!offsetPolyline(patch.chain, true, -t, &offset) || !polygonSimple(offset)) {
                return SurfaceStatus::ThickenInvalidSolid;
            }
            const bool outward = t > 0.0;
            loops = {outward ? offset : patch.chain, outward ? patch.chain : offset};
            if (polygonSignedAreaTwice(loops[1]) <= 0.0 || polygonSignedAreaTwice(loops[0]) <= 0.0) {
                return SurfaceStatus::ThickenInvalidSolid;
            }
        } else {
            if (!offsetPolyline(patch.chain, false, t, &offset)) return SurfaceStatus::ThickenInvalidSolid;
            std::vector<SketchPoint> wall = patch.chain;
            for (auto it = offset.rbegin(); it != offset.rend(); ++it) wall.push_back(*it);
            if (polygonSignedAreaTwice(wall) < 0.0) std::reverse(wall.begin(), wall.end());
            if (!polygonSimple(wall)) return SurfaceStatus::ThickenInvalidSolid;
            loops = {wall};
        }
    } else {
        return SurfaceStatus::ThickenUnsupportedForSurfaceType;
    }
    return appendPrism(patch.frame, loops, patch.extentNear, patch.extentFar, solid);
}

}  // namespace

SurfaceStatus regenerateSurfaceBody(const SurfaceBodyState& state, SurfaceBodyMesh* out,
                                    SurfaceRegenerationReport* report) {
    SurfaceRegenerationReport local;
    SurfaceFeatureId failed = kNoSurfaceFeature;
    SurfaceStatus why = validateSurfaceBodyState(state, &failed);
    auto fail = [&](SurfaceStatus status, SurfaceFeatureId feature) {
        local.status = status;
        local.failedFeature = feature;
        if (report != nullptr) *report = local;
        return status;
    };
    if (why != SurfaceStatus::Ok) return fail(why, failed);
    Live live;
    live.consumed.assign(state.features.size(), 0u);
    auto featureIndex = [&state](SurfaceFeatureId id) {
        for (size_t i = 0; i < state.features.size(); ++i) {
            if (state.features[i].id == id) return i;
        }
        return state.features.size();
    };
    auto livePatchesOf = [&live](SurfaceFeatureId id) {
        std::vector<size_t> at;
        for (size_t i = 0; i < live.patches.size(); ++i) {
            if (live.patches[i].id.feature == id) at.push_back(i);
        }
        return at;
    };
    for (size_t fi = 0; fi < state.features.size(); ++fi) {
        const SurfaceFeature& f = state.features[fi];
        std::vector<SurfacePatch> made;
        auto push = [&made, &f](SurfacePatch patch) {
            patch.id = SurfacePatchId{f.id, static_cast<uint32_t>(made.size())};
            made.push_back(std::move(patch));
        };
        const SurfaceSketchRecord* record = findSurfaceSketch(state, f.section.sketchId);
        switch (f.kind) {
            case SurfaceFeatureKind::PlanarPatch: {
                std::vector<std::vector<std::vector<SketchPoint>>> components;
                regionLoops(record->sketch, f.regions, &why, &components);
                if (why != SurfaceStatus::Ok) return fail(why, f.id);
                for (const auto& loops : components) {
                    SurfacePatch patch;
                    why = planarPatchFromLoops(surfaceSketchFrame(*record), loops, &patch);
                    if (why != SurfaceStatus::Ok) return fail(why, f.id);
                    push(std::move(patch));
                }
                break;
            }
            case SurfaceFeatureKind::ExtrudedSurface: {
                std::vector<Chain> chains;
                why = buildChains(record->sketch, f.section.curves, &chains);
                if (why != SurfaceStatus::Ok) return fail(why, f.id);
                const CadFrame64 frame = surfaceSketchFrame(*record);
                const double far = f.direction == ExtrudeDirection::AlongNormal ? f.distance : -f.distance;
                for (const Chain& chain : chains) {
                    std::vector<DVec3> a;
                    std::vector<DVec3> b;
                    for (const SketchPoint& p : chain.points) {
                        a.push_back(framePoint(frame, p, 0.0));
                        b.push_back(framePoint(frame, p, far));
                    }
                    SurfacePatch patch;
                    ruledPatch(a, b, chain.closed, &patch);
                    patch.shape = SurfacePatchShape::Ruled;
                    patch.frame = frame;
                    patch.chain = chain.points;
                    patch.chainClosed = chain.closed;
                    patch.chainStraight = chain.straight;
                    patch.chainSingleCurve = chain.single;
                    patch.chainEntity = chain.entity;
                    patch.chainCurveKind = chain.entity.kind();
                    patch.extentNear = std::min(0.0, far);
                    patch.extentFar = std::max(0.0, far);
                    push(std::move(patch));
                }
                break;
            }
            case SurfaceFeatureKind::RevolvedSurface: {
                for (SketchEntityId curve : f.section.curves) {
                    if (curve == f.axis.entityId) return fail(SurfaceStatus::AxisUnresolved, f.id);
                }
                SketchPoint a0, a1;
                why = resolveAxis(record->sketch, f.axis, &a0, &a1);
                if (why != SurfaceStatus::Ok) return fail(why, f.id);
                std::vector<Chain> chains;
                why = buildChains(record->sketch, f.section.curves, &chains);
                if (why != SurfaceStatus::Ok) return fail(why, f.id);
                for (const Chain& chain : chains) {
                    SurfacePatch patch;
                    why = revolvedPatch(surfaceSketchFrame(*record), chain, a0, a1, f.angleDegrees,
                                        f.revolveDirection, &patch);
                    if (why != SurfaceStatus::Ok) return fail(why, f.id);
                    push(std::move(patch));
                }
                break;
            }
            case SurfaceFeatureKind::LoftSurface: {
                const SurfaceSketchRecord* recordB = findSurfaceSketch(state, f.sectionB.sketchId);
                std::vector<Chain> a;
                std::vector<Chain> b;
                why = buildChains(record->sketch, f.section.curves, &a);
                if (why != SurfaceStatus::Ok) return fail(why, f.id);
                why = buildChains(recordB->sketch, f.sectionB.curves, &b);
                if (why != SurfaceStatus::Ok) return fail(why, f.id);
                if (a.size() != 1u || b.size() != 1u) return fail(SurfaceStatus::LoftSectionCount, f.id);
                if (a[0].closed != b[0].closed) return fail(SurfaceStatus::LoftSectionMismatch, f.id);
                const bool closed = a[0].closed;
                const uint32_t count = std::min<uint32_t>(
                        kMaxSurfaceLoftSamples,
                        std::max<uint32_t>(closed ? 3u : 2u,
                                           static_cast<uint32_t>(std::max(a[0].points.size(), b[0].points.size()))));
                if ((!closed && f.startOffsetB != 0u) || (closed && f.startOffsetB >= count)) {
                    return fail(SurfaceStatus::LoftCorrespondenceInvalid, f.id);
                }
                std::vector<DVec3> pa;
                std::vector<DVec3> pb;
                for (const SketchPoint& p : a[0].points) pa.push_back(framePoint(surfaceSketchFrame(*record), p, 0.0));
                for (const SketchPoint& p : b[0].points) pb.push_back(framePoint(surfaceSketchFrame(*recordB), p, 0.0));
                std::vector<DVec3> sa = resample(pa, closed, count);
                std::vector<DVec3> sb = resample(pb, closed, count);
                if (f.reverseB) {
                    if (closed) std::reverse(sb.begin() + 1, sb.end());
                    else std::reverse(sb.begin(), sb.end());
                }
                if (closed && f.startOffsetB != 0u) std::rotate(sb.begin(), sb.begin() + f.startOffsetB, sb.end());
                bool coincide = true;
                for (uint32_t i = 0; i < count; ++i) coincide = coincide && distance(sa[i], sb[i]) <= kMinSurfaceDistanceMeters;
                if (coincide) return fail(SurfaceStatus::LoftSectionsCoincide, f.id);
                SurfacePatch patch;
                ruledPatch(sa, sb, closed, &patch);
                patch.shape = SurfacePatchShape::Lofted;
                push(std::move(patch));
                break;
            }
            case SurfaceFeatureKind::TrimSurface: {
                const size_t ti = featureIndex(f.target);
                if (live.consumed[ti]) return fail(SurfaceStatus::FeatureConsumed, f.id);
                const std::vector<size_t> targets = livePatchesOf(f.target);
                if (targets.empty()) return fail(SurfaceStatus::FeatureConsumed, f.id);
                std::vector<std::vector<SketchPoint>> cutter = regionLoops(record->sketch, f.regions, &why, nullptr);
                if (why != SurfaceStatus::Ok) return fail(SurfaceStatus::TrimRegionInvalid, f.id);
                const CadFrame64 trimFrame = surfaceSketchFrame(*record);
                for (size_t index : targets) {
                    const SurfacePatch& target = live.patches[index];
                    if (target.shape != SurfacePatchShape::Planar) return fail(SurfaceStatus::TrimUnsupportedTarget, f.id);
                    if (!sameFramePlane(target.frame, trimFrame)) return fail(SurfaceStatus::TrimNotCoplanar, f.id);
                    std::vector<std::vector<std::vector<SketchPoint>>> pieces;
                    why = trimPatch(target, cutter, record->sketch, f.keepInside, &pieces);
                    if (why != SurfaceStatus::Ok) return fail(why, f.id);
                    for (const auto& loops : pieces) {
                        SurfacePatch patch;
                        why = planarPatchFromLoops(trimFrame, loops, &patch);
                        if (why != SurfaceStatus::Ok) return fail(why, f.id);
                        push(std::move(patch));
                    }
                }
                live.consumed[ti] = 1u;
                live.patches.erase(std::remove_if(live.patches.begin(), live.patches.end(),
                                                  [&f](const SurfacePatch& p) { return p.id.feature == f.target; }),
                                   live.patches.end());
                break;
            }
            case SurfaceFeatureKind::Stitch: {
                std::vector<size_t> indices;
                for (SurfaceFeatureId id : f.stitchFeatures) {
                    if (live.consumed[featureIndex(id)]) return fail(SurfaceStatus::FeatureConsumed, f.id);
                    const std::vector<size_t> at = livePatchesOf(id);
                    if (at.empty()) return fail(SurfaceStatus::FeatureConsumed, f.id);
                    indices.insert(indices.end(), at.begin(), at.end());
                }
                why = stitch(&live, indices);
                if (why != SurfaceStatus::Ok) return fail(why, f.id);
                break;
            }
            case SurfaceFeatureKind::Thicken: {
                const size_t si = featureIndex(f.source);
                if (live.consumed[si]) return fail(SurfaceStatus::FeatureConsumed, f.id);
                const std::vector<size_t> sources = livePatchesOf(f.source);
                if (sources.empty()) return fail(SurfaceStatus::FeatureConsumed, f.id);
                for (const SurfaceStitchPair& pair : live.stitches) {
                    if (pair.a.patch.feature == f.source || pair.b.patch.feature == f.source) {
                        return fail(SurfaceStatus::ThickenUnsupportedForSurfaceType, f.id);
                    }
                }
                CadSolid solid = live.solid;
                for (size_t index : sources) {
                    why = thickenPatch(live.patches[index], f.thickness, &solid);
                    if (why != SurfaceStatus::Ok) return fail(why, f.id);
                }
                live.solid = std::move(solid);
                live.consumed[si] = 1u;
                live.patches.erase(std::remove_if(live.patches.begin(), live.patches.end(),
                                                  [&f](const SurfacePatch& p) { return p.id.feature == f.source; }),
                                   live.patches.end());
                break;
            }
        }
        for (SurfacePatch& patch : made) live.patches.push_back(std::move(patch));
        if (live.patches.size() > kMaxSurfacePatches) return fail(SurfaceStatus::TooManyPatches, f.id);
        size_t triangles = live.solid.triangleCount();
        for (const SurfacePatch& p : live.patches) triangles += p.triangles.size() / 3u;
        if (triangles > kMaxSurfaceTriangles) return fail(SurfaceStatus::TooManyTriangles, f.id);
    }

    SurfaceBodyMesh mesh;
    mesh.stitches = live.stitches;
    mesh.solid = live.solid;
    uint32_t edgeCount = 0;
    for (size_t p = 0; p < live.patches.size(); ++p) {
        const SurfacePatch& patch = live.patches[p];
        const uint32_t base = static_cast<uint32_t>(mesh.render.vertices.size());
        for (const DVec3& x : patch.positions) {
            MeshVertex v{};
            v.position[0] = static_cast<float>(x.x);
            v.position[1] = static_cast<float>(x.y);
            v.position[2] = static_cast<float>(x.z);
            for (int c = 0; c < 3; ++c) v.color[c] = kSurfaceVertexColor[c];
            mesh.render.vertices.push_back(v);
        }
        for (uint32_t index : patch.triangles) mesh.render.indices.push_back(base + index);
        for (size_t t = 0; t < patch.triangles.size() / 3u; ++t) mesh.trianglePatch.push_back(static_cast<int32_t>(p));
        edgeCount += static_cast<uint32_t>(patch.edges.size());
    }
    const uint32_t solidBase = static_cast<uint32_t>(mesh.render.vertices.size());
    for (uint32_t i = 0; i < live.solid.vertexCount(); ++i) {
        MeshVertex v{};
        for (int c = 0; c < 3; ++c) {
            v.position[c] = static_cast<float>(live.solid.positions[i * 3u + c]);
            v.color[c] = kSurfaceVertexColor[c];
        }
        mesh.render.vertices.push_back(v);
    }
    for (uint32_t index : live.solid.indices) mesh.render.indices.push_back(solidBase + index);
    for (uint32_t t = 0; t < live.solid.triangleCount(); ++t) mesh.trianglePatch.push_back(-1);
    mesh.openEdgeCount = edgeCount - 2u * static_cast<uint32_t>(live.stitches.size());
    mesh.open = mesh.openEdgeCount > 0u || (!live.patches.empty() && live.solid.triangleCount() == 0u
                                            && edgeCount == 0u && false);
    // A patch with no open edge left (every one stitched) still has two sides
    // a viewer can see unless it encloses a volume; R1 draws every PATCH from
    // both sides and a pure solid from its outside only.
    mesh.render.renderBothSides = !live.patches.empty();
    mesh.patches = std::move(live.patches);
    if (out != nullptr) *out = std::move(mesh);
    local.status = SurfaceStatus::Ok;
    if (report != nullptr) *report = local;
    return SurfaceStatus::Ok;
}

uint64_t surfaceMeshDigest(const SurfaceBodyMesh& mesh) {
    uint64_t hash = 0xCBF29CE484222325ull;
    auto mix = [&hash](const void* data, size_t size) {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 0x100000001B3ull;
        }
    };
    for (const SurfacePatch& p : mesh.patches) {
        for (const DVec3& x : p.positions) {
            mix(&x.x, sizeof(double));
            mix(&x.y, sizeof(double));
            mix(&x.z, sizeof(double));
        }
        mix(p.triangles.data(), p.triangles.size() * sizeof(uint32_t));
    }
    mix(mesh.solid.positions.data(), mesh.solid.positions.size() * sizeof(double));
    mix(mesh.solid.indices.data(), mesh.solid.indices.size() * sizeof(uint32_t));
    return hash;
}

std::vector<SurfaceEdgeId> surfaceOpenEdges(const SurfaceBodyMesh& mesh) {
    std::vector<SurfaceEdgeId> out;
    for (const SurfacePatch& p : mesh.patches) {
        for (uint32_t e = 0; e < p.edges.size(); ++e) {
            const SurfaceEdgeId id{p.id, e};
            bool stitched = false;
            for (const SurfaceStitchPair& pair : mesh.stitches) {
                stitched = stitched || sameSurfaceEdgeId(pair.a, id) || sameSurfaceEdgeId(pair.b, id);
            }
            if (!stitched) out.push_back(id);
        }
    }
    return out;
}

uint32_t appendSurfaceSketch(SurfaceBodyState* state, const CadSketch& sketch, double offset) {
    if (state->sketches.size() >= kMaxSurfaceSketches || sketch.hasFaceSupport
        || validateCadSketch(sketch) != CadStatus::Ok || !std::isfinite(offset)) {
        return 0u;
    }
    SurfaceSketchRecord record;
    record.id = state->nextSketchId++;
    record.offset = offset;
    record.sketch = sketch;
    state->sketches.push_back(record);
    return record.id;
}

SurfaceFeatureId appendSurfaceFeature(SurfaceBodyState* state, SurfaceFeature feature) {
    if (state->features.size() >= kMaxSurfaceFeatures) return kNoSurfaceFeature;
    feature.id = SurfaceFeatureId{state->nextFeatureId++};
    state->features.push_back(feature);
    return feature.id;
}

std::vector<ProfileRegionRef> surfaceDefaultPatchRegions(const CadSketch& sketch) {
    const SketchRegionExtraction extraction = extractSketchRegions(cadSketchMaterialView(sketch));
    std::vector<ProfileRegionRef> out;
    for (const SketchRegion& region : extraction.regions) {
        if (region.status == CadStatus::Ok && region.depth % 2u == 0u) out.push_back(sketchRegionRef(region));
    }
    return out;
}

std::vector<SketchEntityId> surfaceDefaultCurves(const CadSketch& sketch, SketchEntityId except) {
    std::vector<SketchEntityId> out;
    for (const SketchEntity& entity : sketch.entities) {
        if (!entity.construction() && entity.id() != except) out.push_back(entity.id());
    }
    std::sort(out.begin(), out.end());
    return out;
}

SurfaceStatus SurfaceBody::applyState(const SurfaceBodyState& next, bool* outChanged,
                                      SurfaceRegenerationReport* report) {
    if (outChanged != nullptr) *outChanged = false;
    if (next.nextSketchId < state_.nextSketchId || next.nextFeatureId < state_.nextFeatureId) {
        return SurfaceStatus::HighWaterInvalid;
    }
    SurfaceBodyMesh mesh;
    const SurfaceStatus why = regenerateSurfaceBody(next, &mesh, report);
    if (why != SurfaceStatus::Ok) return why;
    if (sameSurfaceBodyState(next, state_)) return SurfaceStatus::Ok;
    state_ = next;
    mesh_ = std::make_shared<const SurfaceBodyMesh>(std::move(mesh));
    if (outChanged != nullptr) *outChanged = true;
    return SurfaceStatus::Ok;
}

void SurfaceBody::restoreState(const SurfaceBodyState& state) {
    SurfaceBodyMesh mesh;
    if (regenerateSurfaceBody(state, &mesh) == SurfaceStatus::Ok) {
        state_ = state;
        mesh_ = std::make_shared<const SurfaceBodyMesh>(std::move(mesh));
    }
}

}  // namespace forgeshape
