#include "forgeshape_cad_body.h"

#include <cmath>
#include <cstring>

namespace forgeshape {

const char* extrudeDirectionName(ExtrudeDirection direction) {
    switch (direction) {
        case ExtrudeDirection::AlongNormal: return "AlongNormal";
        case ExtrudeDirection::AgainstNormal: return "AgainstNormal";
    }
    return "unknown";
}

bool extrudeDirectionFromIndex(int index, ExtrudeDirection* out) {
    if (out == nullptr) {
        return false;
    }
    switch (index) {
        case 0: *out = ExtrudeDirection::AlongNormal; return true;
        case 1: *out = ExtrudeDirection::AgainstNormal; return true;
        default: return false;
    }
}

int extrudeDirectionIndex(ExtrudeDirection direction) { return static_cast<int>(direction); }

namespace {

bool sameBits(double a, double b) {
    uint64_t left = 0;
    uint64_t right = 0;
    std::memcpy(&left, &a, sizeof(left));
    std::memcpy(&right, &b, sizeof(right));
    return left == right;
}

bool directionValid(ExtrudeDirection direction) {
    const int index = extrudeDirectionIndex(direction);
    return index >= 0 && index < kExtrudeDirectionCount;
}

}  // namespace

bool sameCadBodyState(const CadBodyState& a, const CadBodyState& b) {
    return sameCadSketch(a.sketch, b.sketch)
           && a.extrude.profileEntityId == b.extrude.profileEntityId
           && sameBits(a.extrude.depth, b.extrude.depth)
           && a.extrude.direction == b.extrude.direction;
}

CadStatus validateCadBodyState(const CadBodyState& state, ProfileExtraction* outProfiles) {
    const CadStatus sketchWhy = validateCadSketch(state.sketch);
    if (sketchWhy != CadStatus::Ok) {
        return sketchWhy;
    }
    if (!std::isfinite(state.extrude.depth)) {
        return CadStatus::NonFinite;
    }
    // The SAME rule every primitive dimension passes: finite, positive, and
    // resolvable as a float, plus the sketch's own bound.
    if (validateDimensionMeters(state.extrude.depth) != DimensionValidation::Ok
        || state.extrude.depth > kMaxSketchCoordinateMeters) {
        return CadStatus::InvalidExtrudeDepth;
    }
    if (!directionValid(state.extrude.direction)) {
        return CadStatus::InvalidExtrudeDirection;
    }
    ProfileExtraction extraction = extractClosedProfiles(state.sketch);
    if (extraction.profiles.empty()) {
        return CadStatus::NoClosedProfile;
    }
    if (findClosedProfile(extraction, state.extrude.profileEntityId) == nullptr) {
        return CadStatus::ProfileNotFound;
    }
    if (outProfiles != nullptr) {
        *outProfiles = std::move(extraction);
    }
    return CadStatus::Ok;
}

CadStatus generateCadMesh(const CadBodyState& state, ConstructionMesh* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    ProfileExtraction extraction;
    const CadStatus why = validateCadBodyState(state, &extraction);
    if (why != CadStatus::Ok) {
        return why;
    }
    const ClosedProfile* profile = findClosedProfile(extraction, state.extrude.profileEntityId);
    if (profile == nullptr) {
        return CadStatus::ProfileNotFound;  // proven above; re-checked before the deref
    }

    std::vector<uint32_t> capIndices;
    const CadStatus triWhy = triangulateSimplePolygon(profile->polygon, &capIndices);
    if (triWhy != CadStatus::Ok) {
        return triWhy;
    }

    const uint32_t n = static_cast<uint32_t>(profile->polygon.size());
    const Workplane plane = state.sketch.plane;
    // The solid always spans from its -N face to its +N face; which of the two
    // sits ON the sketch plane is the direction. Building it this way means
    // the winding rule below never has to ask which way the user chose.
    const double nearOffset = (state.extrude.direction == ExtrudeDirection::AlongNormal)
                                  ? 0.0
                                  : -state.extrude.depth;
    const double farOffset = nearOffset + state.extrude.depth;

    ConstructionMesh mesh;
    mesh.vertices.resize(static_cast<size_t>(cadExtrusionVertexCount(n)));
    for (uint32_t i = 0; i < n; ++i) {
        const Vec3 nearPoint = workplaneToLocalAtOffset(plane, profile->polygon[i], nearOffset);
        const Vec3 farPoint = workplaneToLocalAtOffset(plane, profile->polygon[i], farOffset);
        MeshVertex& lower = mesh.vertices[i];
        MeshVertex& upper = mesh.vertices[n + i];
        lower.position[0] = nearPoint.x;
        lower.position[1] = nearPoint.y;
        lower.position[2] = nearPoint.z;
        upper.position[0] = farPoint.x;
        upper.position[1] = farPoint.y;
        upper.position[2] = farPoint.z;
        for (int c = 0; c < 3; ++c) {
            lower.color[c] = kCadBodyVertexColor[c];
            upper.color[c] = kCadBodyVertexColor[c];
        }
    }

    mesh.indices.reserve(static_cast<size_t>(cadExtrusionIndexCount(n)));
    // The +N cap keeps the profile's counter-clockwise order, which is
    // counter-clockwise seen from +N -- outside. The -N cap is the same
    // triangles reversed.
    for (size_t t = 0; t < capIndices.size(); t += 3) {
        mesh.indices.push_back(n + capIndices[t]);
        mesh.indices.push_back(n + capIndices[t + 1]);
        mesh.indices.push_back(n + capIndices[t + 2]);
    }
    for (size_t t = 0; t < capIndices.size(); t += 3) {
        mesh.indices.push_back(capIndices[t]);
        mesh.indices.push_back(capIndices[t + 2]);
        mesh.indices.push_back(capIndices[t + 1]);
    }
    // One quad per edge. For a counter-clockwise profile the outward normal
    // of edge (i -> i+1) is edge x N, and (lower_i, lower_i+1, upper_i+1) has
    // exactly that normal.
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = (i + 1u) % n;
        mesh.indices.push_back(i);
        mesh.indices.push_back(j);
        mesh.indices.push_back(n + j);
        mesh.indices.push_back(i);
        mesh.indices.push_back(n + j);
        mesh.indices.push_back(n + i);
    }
    mesh.renderBothSides = false;

    for (const MeshVertex& v : mesh.vertices) {
        if (!std::isfinite(v.position[0]) || !std::isfinite(v.position[1])
            || !std::isfinite(v.position[2])) {
            return CadStatus::RegenerationFailed;
        }
    }
    *out = std::move(mesh);
    return CadStatus::Ok;
}

// ---------------------------------------------------------------------------
// The body
// ---------------------------------------------------------------------------

CadStatus CadBody::applyState(const CadBodyState& requested, bool* outChanged) {
    if (outChanged != nullptr) {
        *outChanged = false;
    }
    // Regenerate ONCE into a scratch mesh: the only proof a state is usable is
    // that the whole path runs, and running it here is what makes the refusal
    // land before a byte of the body has moved.
    ConstructionMesh scratch;
    const CadStatus why = generateCadMesh(requested, &scratch);
    if (why != CadStatus::Ok) {
        ++rejectedUpdates_;
        return why;
    }
    if (sameCadBodyState(state_, requested)) {
        return CadStatus::Ok;  // identical: nothing written, nothing counted
    }
    state_ = requested;
    ++updateCount_;
    if (outChanged != nullptr) {
        *outChanged = true;
    }
    return CadStatus::Ok;
}

CadStatus applyCadExtrude(CadBody& body, Meters depth, ExtrudeDirection direction,
                          bool* outChanged) {
    if (outChanged != nullptr) {
        *outChanged = false;
    }
    CadBodyState candidate = body.state();
    candidate.extrude.depth = depth;
    candidate.extrude.direction = direction;
    return body.applyState(candidate, outChanged);
}

CadStatus applyCadRectangle(CadBody& body, Meters width, Meters height, bool* outChanged) {
    if (outChanged != nullptr) {
        *outChanged = false;
    }
    CadBodyState candidate = body.state();
    const SketchEntity* anchor =
            findSketchEntity(candidate.sketch, candidate.extrude.profileEntityId);
    if (anchor == nullptr || anchor->rectangle() == nullptr) {
        return CadStatus::ProfileNotFound;
    }
    SketchRectangle rectangle = *anchor->rectangle();
    rectangle.width = width;
    rectangle.height = height;
    const CadStatus why = replaceSketchEntity(&candidate.sketch, anchor->id(), rectangle);
    if (why != CadStatus::Ok) {
        return why;
    }
    return body.applyState(candidate, outChanged);
}

CadStatus applyCadCircle(CadBody& body, Meters radius, bool* outChanged) {
    if (outChanged != nullptr) {
        *outChanged = false;
    }
    CadBodyState candidate = body.state();
    const SketchEntity* anchor =
            findSketchEntity(candidate.sketch, candidate.extrude.profileEntityId);
    if (anchor == nullptr || anchor->circle() == nullptr) {
        return CadStatus::ProfileNotFound;
    }
    SketchCircle circle = *anchor->circle();
    circle.radius = radius;
    const CadStatus why = replaceSketchEntity(&candidate.sketch, anchor->id(), circle);
    if (why != CadStatus::Ok) {
        return why;
    }
    return body.applyState(candidate, outChanged);
}

CadProfileKind cadProfileKind(const CadBodyState& state) {
    const SketchEntity* anchor = findSketchEntity(state.sketch, state.extrude.profileEntityId);
    if (anchor == nullptr) {
        return CadProfileKind::None;
    }
    switch (anchor->kind()) {
        case SketchEntityKind::Rectangle: return CadProfileKind::Rectangle;
        case SketchEntityKind::Circle: return CadProfileKind::Circle;
        case SketchEntityKind::Polyline:
        case SketchEntityKind::Line: return CadProfileKind::Polygon;
    }
    return CadProfileKind::None;
}

const char* cadProfileKindName(CadProfileKind kind) {
    switch (kind) {
        case CadProfileKind::None: return "None";
        case CadProfileKind::Rectangle: return "Rectangle";
        case CadProfileKind::Circle: return "Circle";
        case CadProfileKind::Polygon: return "Polygon";
    }
    return "unknown";
}

}  // namespace forgeshape
