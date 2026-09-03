#include "forgeshape_sketch.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace forgeshape {

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

const char* cadStatusName(CadStatus status) {
    switch (status) {
        case CadStatus::Ok: return "Ok";
        case CadStatus::NonFinite: return "NonFinite";
        case CadStatus::OutOfRange: return "OutOfRange";
        case CadStatus::ZeroLengthLine: return "ZeroLengthLine";
        case CadStatus::DuplicateEdge: return "DuplicateEdge";
        case CadStatus::TooFewVertices: return "TooFewVertices";
        case CadStatus::ZeroSizeRectangle: return "ZeroSizeRectangle";
        case CadStatus::InvalidCircleRadius: return "InvalidCircleRadius";
        case CadStatus::TooManyEntities: return "TooManyEntities";
        case CadStatus::UnknownEntity: return "UnknownEntity";
        case CadStatus::InvalidWorkplane: return "InvalidWorkplane";
        case CadStatus::OpenProfile: return "OpenProfile";
        case CadStatus::SelfIntersectingProfile: return "SelfIntersectingProfile";
        case CadStatus::ZeroAreaProfile: return "ZeroAreaProfile";
        case CadStatus::BranchingChain: return "BranchingChain";
        case CadStatus::NoClosedProfile: return "NoClosedProfile";
        case CadStatus::AmbiguousProfile: return "AmbiguousProfile";
        case CadStatus::ProfileNotFound: return "ProfileNotFound";
        case CadStatus::NestedProfileUnsupported: return "NestedProfileUnsupported";
        case CadStatus::InvalidExtrudeDepth: return "InvalidExtrudeDepth";
        case CadStatus::InvalidExtrudeDirection: return "InvalidExtrudeDirection";
        case CadStatus::TriangulationFailed: return "TriangulationFailed";
        case CadStatus::RegenerationFailed: return "RegenerationFailed";
        case CadStatus::NotSketching: return "NotSketching";
        case CadStatus::NotCadBody: return "NotCadBody";
        case CadStatus::RefusedEditInProgress: return "RefusedEditInProgress";
    }
    return "unknown";
}

int cadStatusCode(CadStatus status) { return static_cast<int>(status); }

bool cadStatusFromCode(int code, CadStatus* out) {
    if (out == nullptr || code < 0 || code >= kCadStatusCount) {
        return false;
    }
    *out = static_cast<CadStatus>(code);
    return true;
}

const char* sketchEntityKindName(SketchEntityKind kind) {
    switch (kind) {
        case SketchEntityKind::Line: return "Line";
        case SketchEntityKind::Polyline: return "Polyline";
        case SketchEntityKind::Rectangle: return "Rectangle";
        case SketchEntityKind::Circle: return "Circle";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Small geometry
// ---------------------------------------------------------------------------

namespace {

bool sameBits(double a, double b) {
    uint64_t left = 0;
    uint64_t right = 0;
    std::memcpy(&left, &a, sizeof(left));
    std::memcpy(&right, &b, sizeof(right));
    return left == right;
}

bool samePointBits(const SketchPoint& a, const SketchPoint& b) {
    return sameBits(a.u, b.u) && sameBits(a.v, b.v);
}

bool coincident(const SketchPoint& a, const SketchPoint& b) {
    const double du = a.u - b.u;
    const double dv = a.v - b.v;
    return (du * du + dv * dv) <= kSketchCoincidenceMeters * kSketchCoincidenceMeters;
}

CadStatus validatePoint(const SketchPoint& p) {
    if (!std::isfinite(p.u) || !std::isfinite(p.v)) {
        return CadStatus::NonFinite;
    }
    if (std::fabs(p.u) > kMaxSketchCoordinateMeters || std::fabs(p.v) > kMaxSketchCoordinateMeters) {
        return CadStatus::OutOfRange;
    }
    return CadStatus::Ok;
}

// A length must pass the SAME rule every primitive dimension passes -- finite,
// positive, and resolvable once it becomes a float half-extent -- and must
// stay inside the sketch range. One rule for what a Construction length is.
bool usableLength(Meters value) {
    return validateDimensionMeters(value) == DimensionValidation::Ok
           && value <= kMaxSketchCoordinateMeters;
}

double cross(const SketchPoint& o, const SketchPoint& a, const SketchPoint& b) {
    return (a.u - o.u) * (b.v - o.v) - (a.v - o.v) * (b.u - o.u);
}

bool onSegment(const SketchPoint& p, const SketchPoint& a, const SketchPoint& b) {
    return p.u <= std::fmax(a.u, b.u) && p.u >= std::fmin(a.u, b.u)
           && p.v <= std::fmax(a.v, b.v) && p.v >= std::fmin(a.v, b.v);
}

int orientationSign(const SketchPoint& o, const SketchPoint& a, const SketchPoint& b) {
    const double c = cross(o, a, b);
    if (c > 0.0) return 1;
    if (c < 0.0) return -1;
    return 0;
}

// Point strictly inside a polygon, by the even-odd rule. A point ON an edge
// is reported as not strictly inside.
bool pointStrictlyInside(const SketchPoint& p, const std::vector<SketchPoint>& polygon) {
    const size_t n = polygon.size();
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const SketchPoint& a = polygon[i];
        const SketchPoint& b = polygon[j];
        if (orientationSign(a, b, p) == 0 && onSegment(p, a, b)) {
            return false;  // on the boundary
        }
        const bool crosses = ((a.v > p.v) != (b.v > p.v))
                             && (p.u < (b.u - a.u) * (p.v - a.v) / (b.v - a.v) + a.u);
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

}  // namespace

bool sketchSegmentsIntersect(const SketchPoint& a0, const SketchPoint& a1,
                             const SketchPoint& b0, const SketchPoint& b1) {
    const int o1 = orientationSign(a0, a1, b0);
    const int o2 = orientationSign(a0, a1, b1);
    const int o3 = orientationSign(b0, b1, a0);
    const int o4 = orientationSign(b0, b1, a1);
    if (o1 != o2 && o3 != o4) {
        return true;
    }
    if (o1 == 0 && onSegment(b0, a0, a1)) return true;
    if (o2 == 0 && onSegment(b1, a0, a1)) return true;
    if (o3 == 0 && onSegment(a0, b0, b1)) return true;
    if (o4 == 0 && onSegment(a1, b0, b1)) return true;
    return false;
}

double polygonSignedAreaTwice(const std::vector<SketchPoint>& polygon) {
    double twice = 0.0;
    const size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i) {
        const SketchPoint& a = polygon[i];
        const SketchPoint& b = polygon[(i + 1) % n];
        twice += a.u * b.v - b.u * a.v;
    }
    return twice;
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------

bool sameSketchEntity(const SketchEntity& a, const SketchEntity& b) {
    if (a.id() != b.id() || a.kind() != b.kind()) {
        return false;
    }
    switch (a.kind()) {
        case SketchEntityKind::Line:
            return samePointBits(a.line()->start, b.line()->start)
                   && samePointBits(a.line()->end, b.line()->end);
        case SketchEntityKind::Polyline: {
            const SketchPolyline& l = *a.polyline();
            const SketchPolyline& r = *b.polyline();
            if (l.closed != r.closed || l.vertices.size() != r.vertices.size()) {
                return false;
            }
            for (size_t i = 0; i < l.vertices.size(); ++i) {
                if (!samePointBits(l.vertices[i], r.vertices[i])) {
                    return false;
                }
            }
            return true;
        }
        case SketchEntityKind::Rectangle:
            return samePointBits(a.rectangle()->center, b.rectangle()->center)
                   && sameBits(a.rectangle()->width, b.rectangle()->width)
                   && sameBits(a.rectangle()->height, b.rectangle()->height);
        case SketchEntityKind::Circle:
            return samePointBits(a.circle()->center, b.circle()->center)
                   && sameBits(a.circle()->radius, b.circle()->radius);
    }
    return false;
}

CadStatus validateSketchEntity(const SketchEntity& entity) {
    switch (entity.kind()) {
        case SketchEntityKind::Line: {
            const SketchLine& line = *entity.line();
            CadStatus why = validatePoint(line.start);
            if (why != CadStatus::Ok) return why;
            why = validatePoint(line.end);
            if (why != CadStatus::Ok) return why;
            if (coincident(line.start, line.end)) {
                return CadStatus::ZeroLengthLine;
            }
            return CadStatus::Ok;
        }
        case SketchEntityKind::Polyline: {
            const SketchPolyline& polyline = *entity.polyline();
            if (polyline.vertices.size() > kMaxPolylineVertices) {
                return CadStatus::TooManyEntities;
            }
            if (polyline.vertices.size() < 2 || (polyline.closed && polyline.vertices.size() < 3)) {
                return CadStatus::TooFewVertices;
            }
            for (const SketchPoint& p : polyline.vertices) {
                const CadStatus why = validatePoint(p);
                if (why != CadStatus::Ok) return why;
            }
            for (size_t i = 1; i < polyline.vertices.size(); ++i) {
                if (coincident(polyline.vertices[i - 1], polyline.vertices[i])) {
                    return CadStatus::DuplicateEdge;
                }
            }
            if (polyline.closed
                && coincident(polyline.vertices.back(), polyline.vertices.front())) {
                // The closing edge is the one between the last vertex and the
                // first; a closed polyline that repeats its first vertex has a
                // zero-length closing edge.
                return CadStatus::DuplicateEdge;
            }
            return CadStatus::Ok;
        }
        case SketchEntityKind::Rectangle: {
            const SketchRectangle& rectangle = *entity.rectangle();
            const CadStatus why = validatePoint(rectangle.center);
            if (why != CadStatus::Ok) return why;
            if (!std::isfinite(rectangle.width) || !std::isfinite(rectangle.height)) {
                return CadStatus::NonFinite;
            }
            if (!usableLength(rectangle.width) || !usableLength(rectangle.height)) {
                return CadStatus::ZeroSizeRectangle;
            }
            return CadStatus::Ok;
        }
        case SketchEntityKind::Circle: {
            const SketchCircle& circle = *entity.circle();
            const CadStatus why = validatePoint(circle.center);
            if (why != CadStatus::Ok) return why;
            if (!std::isfinite(circle.radius)) {
                return CadStatus::NonFinite;
            }
            if (!usableLength(circle.radius)) {
                return CadStatus::InvalidCircleRadius;
            }
            return CadStatus::Ok;
        }
    }
    return CadStatus::UnknownEntity;
}

// ---------------------------------------------------------------------------
// The sketch
// ---------------------------------------------------------------------------

bool sameCadSketch(const CadSketch& a, const CadSketch& b) {
    if (a.plane != b.plane || a.nextEntityId != b.nextEntityId
        || a.entities.size() != b.entities.size()) {
        return false;
    }
    for (size_t i = 0; i < a.entities.size(); ++i) {
        if (!sameSketchEntity(a.entities[i], b.entities[i])) {
            return false;
        }
    }
    return true;
}

CadStatus validateCadSketch(const CadSketch& sketch) {
    if (workplaneIndex(sketch.plane) < 0 || workplaneIndex(sketch.plane) >= kWorkplaneCount) {
        return CadStatus::InvalidWorkplane;
    }
    if (sketch.entities.size() > kMaxSketchEntities) {
        return CadStatus::TooManyEntities;
    }
    if (sketch.nextEntityId == kNoSketchEntity) {
        return CadStatus::UnknownEntity;
    }
    for (size_t i = 0; i < sketch.entities.size(); ++i) {
        const SketchEntity& entity = sketch.entities[i];
        if (entity.id() == kNoSketchEntity || entity.id() >= sketch.nextEntityId) {
            return CadStatus::UnknownEntity;
        }
        for (size_t j = 0; j < i; ++j) {
            if (sketch.entities[j].id() == entity.id()) {
                return CadStatus::UnknownEntity;
            }
        }
        const CadStatus why = validateSketchEntity(entity);
        if (why != CadStatus::Ok) {
            return why;
        }
    }
    return CadStatus::Ok;
}

CadStatus addSketchEntity(CadSketch* sketch, SketchEntity::Payload payload,
                          SketchEntityId* outId) {
    if (sketch == nullptr) {
        return CadStatus::UnknownEntity;
    }
    if (sketch->entities.size() >= kMaxSketchEntities) {
        return CadStatus::TooManyEntities;
    }
    // Validated with a provisional id BEFORE anything is minted, so a refusal
    // costs no id and leaves the allocator exactly where it was.
    const SketchEntity candidate(sketch->nextEntityId, std::move(payload));
    const CadStatus why = validateSketchEntity(candidate);
    if (why != CadStatus::Ok) {
        return why;
    }
    if (sketch->nextEntityId == UINT32_MAX) {
        return CadStatus::TooManyEntities;
    }
    sketch->entities.push_back(candidate);
    ++sketch->nextEntityId;
    if (outId != nullptr) {
        *outId = candidate.id();
    }
    return CadStatus::Ok;
}

CadStatus replaceSketchEntity(CadSketch* sketch, SketchEntityId id, SketchEntity::Payload payload) {
    if (sketch == nullptr) {
        return CadStatus::UnknownEntity;
    }
    for (SketchEntity& entity : sketch->entities) {
        if (entity.id() != id) {
            continue;
        }
        const SketchEntity candidate(id, std::move(payload));
        const CadStatus why = validateSketchEntity(candidate);
        if (why != CadStatus::Ok) {
            return why;
        }
        entity = candidate;
        return CadStatus::Ok;
    }
    return CadStatus::UnknownEntity;
}

CadStatus removeSketchEntity(CadSketch* sketch, SketchEntityId id) {
    if (sketch == nullptr) {
        return CadStatus::UnknownEntity;
    }
    for (size_t i = 0; i < sketch->entities.size(); ++i) {
        if (sketch->entities[i].id() == id) {
            sketch->entities.erase(sketch->entities.begin() + static_cast<std::ptrdiff_t>(i));
            return CadStatus::Ok;
        }
    }
    return CadStatus::UnknownEntity;
}

const SketchEntity* findSketchEntity(const CadSketch& sketch, SketchEntityId id) {
    for (const SketchEntity& entity : sketch.entities) {
        if (entity.id() == id) {
            return &entity;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Profile polygons
// ---------------------------------------------------------------------------

std::vector<SketchPoint> rectangleProfilePolygon(const SketchRectangle& rectangle) {
    const double hw = rectangle.width * 0.5;
    const double hh = rectangle.height * 0.5;
    const double cu = rectangle.center.u;
    const double cv = rectangle.center.v;
    return {SketchPoint{cu - hw, cv - hh}, SketchPoint{cu + hw, cv - hh},
            SketchPoint{cu + hw, cv + hh}, SketchPoint{cu - hw, cv + hh}};
}

std::vector<SketchPoint> circleProfilePolygon(const SketchCircle& circle) {
    std::vector<SketchPoint> polygon;
    polygon.reserve(kSketchCircleSegments);
    const uint32_t quarter = kSketchCircleSegments / 4u;
    for (uint32_t i = 0; i < kSketchCircleSegments; ++i) {
        double c = 0.0;
        double s = 0.0;
        // The four cardinal points are exact 0 / +/-1 rather than trigonometry,
        // for the same reason the round primitives do it: the extents are then
        // exactly the radius.
        if (i % quarter == 0) {
            switch (i / quarter) {
                case 0: c = 1.0; s = 0.0; break;
                case 1: c = 0.0; s = 1.0; break;
                case 2: c = -1.0; s = 0.0; break;
                default: c = 0.0; s = -1.0; break;
            }
        } else {
            const double angle = 2.0 * 3.14159265358979323846 * static_cast<double>(i)
                                 / static_cast<double>(kSketchCircleSegments);
            c = std::cos(angle);
            s = std::sin(angle);
        }
        polygon.push_back(SketchPoint{circle.center.u + circle.radius * c,
                                      circle.center.v + circle.radius * s});
    }
    return polygon;
}

namespace {

// The rules every candidate loop is held to. Normalizes the orientation to
// counter-clockwise on success; refuses, writing nothing, otherwise.
CadStatus validateLoop(std::vector<SketchPoint>* polygon, double* outArea) {
    const size_t n = polygon->size();
    if (n < 3) {
        return CadStatus::TooFewVertices;
    }
    if (n > kMaxProfileVertices) {
        return CadStatus::TooManyEntities;
    }
    for (size_t i = 0; i < n; ++i) {
        if (coincident((*polygon)[i], (*polygon)[(i + 1) % n])) {
            return CadStatus::DuplicateEdge;
        }
    }
    // Crossings BEFORE area: a bow tie's two lobes cancel to zero area, and
    // "it crosses itself" is the reason the user can act on. Non-adjacent
    // edges may not cross or touch; adjacent edges share a vertex by
    // construction and are skipped, so for a triangle every pair is adjacent.
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const bool adjacent = (j == i + 1) || (i == 0 && j == n - 1);
            if (adjacent) {
                continue;
            }
            if (sketchSegmentsIntersect((*polygon)[i], (*polygon)[(i + 1) % n], (*polygon)[j],
                                        (*polygon)[(j + 1) % n])) {
                return CadStatus::SelfIntersectingProfile;
            }
        }
    }
    const double twice = polygonSignedAreaTwice(*polygon);
    if (!std::isfinite(twice) || std::fabs(twice) * 0.5 < kMinProfileAreaSquareMeters) {
        return CadStatus::ZeroAreaProfile;
    }
    if (twice < 0.0) {
        std::reverse(polygon->begin(), polygon->end());
    }
    *outArea = std::fabs(twice) * 0.5;
    return CadStatus::Ok;
}

struct LoopCandidate {
    SketchEntityId anchor = kNoSketchEntity;
    std::vector<SketchPoint> polygon;
    bool fromCircle = false;
    std::vector<SketchEntityId> members;
};

// Reads every loop a set of LINES closes.
//
// Endpoints within the coincidence tolerance are one NODE. A component in
// which every node meets exactly two line ends is one loop; a free end is an
// open profile and a node with three or more ends is a fork. Each component
// is reported once, under its smallest line id.
void chainLines(const CadSketch& sketch, std::vector<LoopCandidate>* loops,
                std::vector<ProfileRejection>* rejections) {
    struct LineRef {
        SketchEntityId id;
        SketchPoint p[2];
        int node[2];
    };
    std::vector<LineRef> lines;
    for (const SketchEntity& entity : sketch.entities) {
        if (const SketchLine* line = entity.line()) {
            lines.push_back(LineRef{entity.id(), {line->start, line->end}, {-1, -1}});
        }
    }
    if (lines.empty()) {
        return;
    }
    std::sort(lines.begin(), lines.end(),
              [](const LineRef& a, const LineRef& b) { return a.id < b.id; });

    // Nodes: the first endpoint seen at a location is the representative.
    std::vector<SketchPoint> nodes;
    std::vector<int> degree;
    for (LineRef& line : lines) {
        for (int e = 0; e < 2; ++e) {
            int found = -1;
            for (size_t k = 0; k < nodes.size(); ++k) {
                if (coincident(nodes[k], line.p[e])) {
                    found = static_cast<int>(k);
                    break;
                }
            }
            if (found < 0) {
                found = static_cast<int>(nodes.size());
                nodes.push_back(line.p[e]);
                degree.push_back(0);
            }
            line.node[e] = found;
            ++degree[static_cast<size_t>(found)];
        }
    }

    // Components over lines, by shared nodes. Union-find over line indices.
    std::vector<int> parent(lines.size());
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = static_cast<int>(i);
    const auto find = [&parent](int x) {
        while (parent[static_cast<size_t>(x)] != x) {
            parent[static_cast<size_t>(x)] = parent[static_cast<size_t>(parent[static_cast<size_t>(x)])];
            x = parent[static_cast<size_t>(x)];
        }
        return x;
    };
    std::vector<int> nodeOwner(nodes.size(), -1);
    for (size_t i = 0; i < lines.size(); ++i) {
        for (int e = 0; e < 2; ++e) {
            const int node = lines[i].node[e];
            int& owner = nodeOwner[static_cast<size_t>(node)];
            if (owner < 0) {
                owner = static_cast<int>(i);
            } else {
                parent[static_cast<size_t>(find(owner))] = find(static_cast<int>(i));
            }
        }
    }

    // Visit each component once, in order of its smallest line id, which is
    // the order `lines` is already in.
    std::vector<bool> reported(lines.size(), false);
    for (size_t start = 0; start < lines.size(); ++start) {
        if (reported[start]) {
            continue;
        }
        const int root = find(static_cast<int>(start));
        std::vector<size_t> member;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (find(static_cast<int>(i)) == root) {
                member.push_back(i);
                reported[i] = true;
            }
        }
        const SketchEntityId anchor = lines[start].id;

        bool fork = false;
        bool open = false;
        for (size_t i : member) {
            for (int e = 0; e < 2; ++e) {
                const int d = degree[static_cast<size_t>(lines[i].node[e])];
                if (d > 2) fork = true;
                if (d < 2) open = true;
            }
        }
        if (fork) {
            rejections->push_back(ProfileRejection{anchor, CadStatus::BranchingChain});
            continue;
        }
        if (open) {
            rejections->push_back(ProfileRejection{anchor, CadStatus::OpenProfile});
            continue;
        }

        // Every node has degree exactly two, so walking from the first line
        // returns to it and visits every line in the component exactly once.
        LoopCandidate loop;
        loop.anchor = anchor;
        std::vector<bool> used(lines.size(), false);
        size_t current = start;
        int enterNode = lines[start].node[0];
        int leaveNode = lines[start].node[1];
        for (size_t steps = 0; steps < member.size(); ++steps) {
            used[current] = true;
            loop.members.push_back(lines[current].id);
            loop.polygon.push_back(nodes[static_cast<size_t>(enterNode)]);
            // The other line at the leaving node.
            size_t next = lines.size();
            for (size_t i : member) {
                if (used[i]) continue;
                if (lines[i].node[0] == leaveNode || lines[i].node[1] == leaveNode) {
                    next = i;
                    break;
                }
            }
            if (next == lines.size()) {
                break;
            }
            enterNode = leaveNode;
            leaveNode = (lines[next].node[0] == leaveNode) ? lines[next].node[1]
                                                            : lines[next].node[0];
            current = next;
        }
        if (loop.members.size() != member.size()) {
            // Cannot happen when every degree is two and the component is
            // connected; refused rather than trusted if it ever does.
            rejections->push_back(ProfileRejection{anchor, CadStatus::OpenProfile});
            continue;
        }
        std::sort(loop.members.begin(), loop.members.end());
        loops->push_back(std::move(loop));
    }
}

}  // namespace

ProfileExtraction extractClosedProfiles(const CadSketch& sketch) {
    ProfileExtraction out;
    std::vector<LoopCandidate> candidates;

    for (const SketchEntity& entity : sketch.entities) {
        LoopCandidate loop;
        loop.anchor = entity.id();
        loop.members = {entity.id()};
        if (const SketchRectangle* rectangle = entity.rectangle()) {
            loop.polygon = rectangleProfilePolygon(*rectangle);
        } else if (const SketchCircle* circle = entity.circle()) {
            loop.polygon = circleProfilePolygon(*circle);
            loop.fromCircle = true;
        } else if (const SketchPolyline* polyline = entity.polyline()) {
            loop.polygon = polyline->vertices;
            bool closed = polyline->closed;
            if (!closed && loop.polygon.size() >= 4
                && coincident(loop.polygon.front(), loop.polygon.back())) {
                // Closed by endpoint snap: the repeated last vertex is the
                // closing edge's end, not a fifth corner.
                loop.polygon.pop_back();
                closed = true;
            }
            if (!closed) {
                out.rejections.push_back(ProfileRejection{entity.id(), CadStatus::OpenProfile});
                continue;
            }
        } else {
            continue;  // lines are chained below
        }
        candidates.push_back(std::move(loop));
    }
    chainLines(sketch, &candidates, &out.rejections);

    for (LoopCandidate& candidate : candidates) {
        double area = 0.0;
        const CadStatus why = validateLoop(&candidate.polygon, &area);
        if (why != CadStatus::Ok) {
            out.rejections.push_back(ProfileRejection{candidate.anchor, why});
            continue;
        }
        ClosedProfile profile;
        profile.anchorEntityId = candidate.anchor;
        profile.polygon = std::move(candidate.polygon);
        profile.area = area;
        profile.fromCircle = candidate.fromCircle;
        profile.memberEntityIds = std::move(candidate.members);
        out.profiles.push_back(std::move(profile));
    }

    // Nesting. A profile that contains another is a solid with a hole, and a
    // hole is not R0: the OUTER one is refused by name and the inner one stays
    // extrudable on its own. Containment is "a vertex strictly inside and no
    // edge crossing"; two profiles that merely overlap are each still a
    // simple solid and are both kept.
    std::vector<bool> outer(out.profiles.size(), false);
    for (size_t a = 0; a < out.profiles.size(); ++a) {
        for (size_t b = 0; b < out.profiles.size(); ++b) {
            if (a == b) continue;
            const std::vector<SketchPoint>& pa = out.profiles[a].polygon;
            const std::vector<SketchPoint>& pb = out.profiles[b].polygon;
            bool inside = false;
            for (const SketchPoint& p : pb) {
                if (pointStrictlyInside(p, pa)) {
                    inside = true;
                    break;
                }
            }
            if (!inside) continue;
            bool crosses = false;
            for (size_t i = 0; i < pa.size() && !crosses; ++i) {
                for (size_t j = 0; j < pb.size(); ++j) {
                    if (sketchSegmentsIntersect(pa[i], pa[(i + 1) % pa.size()], pb[j],
                                                pb[(j + 1) % pb.size()])) {
                        crosses = true;
                        break;
                    }
                }
            }
            if (!crosses) {
                outer[a] = true;
            }
        }
    }
    std::vector<ClosedProfile> kept;
    for (size_t a = 0; a < out.profiles.size(); ++a) {
        if (outer[a]) {
            out.rejections.push_back(ProfileRejection{out.profiles[a].anchorEntityId,
                                                      CadStatus::NestedProfileUnsupported});
        } else {
            kept.push_back(std::move(out.profiles[a]));
        }
    }
    out.profiles = std::move(kept);

    std::sort(out.profiles.begin(), out.profiles.end(),
              [](const ClosedProfile& l, const ClosedProfile& r) {
                  return l.anchorEntityId < r.anchorEntityId;
              });
    std::sort(out.rejections.begin(), out.rejections.end(),
              [](const ProfileRejection& l, const ProfileRejection& r) {
                  return l.anchorEntityId < r.anchorEntityId;
              });
    return out;
}

const ClosedProfile* findClosedProfile(const ProfileExtraction& extraction,
                                       SketchEntityId anchorEntityId) {
    for (const ClosedProfile& profile : extraction.profiles) {
        if (profile.anchorEntityId == anchorEntityId) {
            return &profile;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Triangulation
// ---------------------------------------------------------------------------

CadStatus triangulateSimplePolygon(const std::vector<SketchPoint>& polygon,
                                   std::vector<uint32_t>* outIndices) {
    if (outIndices == nullptr) {
        return CadStatus::TriangulationFailed;
    }
    const size_t n = polygon.size();
    if (n < 3 || n > kMaxProfileVertices) {
        return CadStatus::TriangulationFailed;
    }
    if (polygonSignedAreaTwice(polygon) <= 0.0) {
        return CadStatus::TriangulationFailed;  // must be counter-clockwise
    }

    std::vector<uint32_t> remaining(n);
    for (size_t i = 0; i < n; ++i) remaining[i] = static_cast<uint32_t>(i);
    std::vector<uint32_t> triangles;
    triangles.reserve((n - 2) * 3);

    // Whether the triangle (a, b, c) is an ear of the remaining polygon: convex
    // at b, and containing no OTHER remaining vertex. A vertex on the ear's
    // boundary counts as inside, so a collinear vertex is never cut across.
    const auto isEar = [&](size_t ia, size_t ib, size_t ic) {
        const SketchPoint& a = polygon[remaining[ia]];
        const SketchPoint& b = polygon[remaining[ib]];
        const SketchPoint& c = polygon[remaining[ic]];
        if (cross(a, b, c) <= 0.0) {
            return false;
        }
        for (size_t k = 0; k < remaining.size(); ++k) {
            if (k == ia || k == ib || k == ic) continue;
            const SketchPoint& p = polygon[remaining[k]];
            if (cross(a, b, p) >= 0.0 && cross(b, c, p) >= 0.0 && cross(c, a, p) >= 0.0) {
                return false;
            }
        }
        return true;
    };

    // Bounded: each outer pass removes one vertex or gives up.
    size_t cursor = 0;
    while (remaining.size() > 3) {
        const size_t m = remaining.size();
        bool clipped = false;
        for (size_t attempt = 0; attempt < m; ++attempt) {
            const size_t ib = (cursor + attempt) % m;
            const size_t ia = (ib + m - 1) % m;
            const size_t ic = (ib + 1) % m;
            if (isEar(ia, ib, ic)) {
                triangles.push_back(remaining[ia]);
                triangles.push_back(remaining[ib]);
                triangles.push_back(remaining[ic]);
                remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(ib));
                cursor = ib % remaining.size();
                clipped = true;
                break;
            }
        }
        if (!clipped) {
            return CadStatus::TriangulationFailed;
        }
    }
    triangles.push_back(remaining[0]);
    triangles.push_back(remaining[1]);
    triangles.push_back(remaining[2]);
    *outIndices = std::move(triangles);
    return CadStatus::Ok;
}

}  // namespace forgeshape
