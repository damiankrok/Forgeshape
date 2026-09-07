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

const char* extrudeExtentModeName(ExtrudeExtentMode mode) {
    switch (mode) {
        case ExtrudeExtentMode::OneSide: return "OneSide";
        case ExtrudeExtentMode::Symmetric: return "Symmetric";
        case ExtrudeExtentMode::TwoSides: return "TwoSides";
    }
    return "unknown";
}

bool extrudeExtentModeFromIndex(int index, ExtrudeExtentMode* out) {
    if (out == nullptr) {
        return false;
    }
    switch (index) {
        case 0: *out = ExtrudeExtentMode::OneSide; return true;
        case 1: *out = ExtrudeExtentMode::Symmetric; return true;
        case 2: *out = ExtrudeExtentMode::TwoSides; return true;
        default: return false;
    }
}

int extrudeExtentModeIndex(ExtrudeExtentMode mode) { return static_cast<int>(mode); }

// ---------------------------------------------------------------------------
// The two durable distances
// ---------------------------------------------------------------------------

Meters extrudePositiveDistance(const ExtrudeFeature& extrude) {
    switch (extrude.extent) {
        case ExtrudeExtentMode::OneSide:
            return extrude.direction == ExtrudeDirection::AlongNormal ? extrude.depth : 0.0;
        case ExtrudeExtentMode::Symmetric:
            return extrude.depth;
        case ExtrudeExtentMode::TwoSides:
            return extrude.depth;
    }
    return 0.0;
}

Meters extrudeNegativeDistance(const ExtrudeFeature& extrude) {
    switch (extrude.extent) {
        case ExtrudeExtentMode::OneSide:
            return extrude.direction == ExtrudeDirection::AgainstNormal ? extrude.depth : 0.0;
        case ExtrudeExtentMode::Symmetric:
            return extrude.depth;
        case ExtrudeExtentMode::TwoSides:
            return extrude.secondDistance;
    }
    return 0.0;
}

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

bool extentValid(ExtrudeExtentMode mode) {
    const int index = extrudeExtentModeIndex(mode);
    return index >= 0 && index < kExtrudeExtentModeCount;
}

// One SIDE distance: finite, non-negative, within the sketch bound, and -- when
// it is not exactly zero -- a length the Construction domain would accept. Zero
// is legitimate on one side of a Two Sides extrusion and nowhere else; the
// caller checks the mode's own rule.
bool sideDistanceValid(Meters d) {
    if (!std::isfinite(d) || d < 0.0 || d > kMaxSketchCoordinateMeters) {
        return false;
    }
    return d == 0.0 || validateDimensionMeters(d) == DimensionValidation::Ok;
}

}  // namespace

bool extrudeFeatureCanonical(const ExtrudeFeature& extrude) {
    if (!extentValid(extrude.extent) || !directionValid(extrude.direction)) {
        return false;
    }
    if (extrude.extent != ExtrudeExtentMode::OneSide
        && extrude.direction != ExtrudeDirection::AlongNormal) {
        return false;
    }
    if (extrude.extent != ExtrudeExtentMode::TwoSides && !sameBits(extrude.secondDistance, 0.0)) {
        return false;
    }
    return true;
}

// The side whose value a transition keeps. `preferredSide` is honoured unless
// its distance is zero, in which case the other side carries the extent and
// taking the preferred one would produce a solid with none.
namespace {
bool transitionKeepsPositive(const ExtrudeFeature& from, ExtrudeDirection preferredSide) {
    const bool wantPositive = preferredSide != ExtrudeDirection::AgainstNormal;
    const Meters chosen = wantPositive ? extrudePositiveDistance(from) : extrudeNegativeDistance(from);
    if (chosen > 0.0) {
        return wantPositive;
    }
    const Meters other = wantPositive ? extrudeNegativeDistance(from) : extrudePositiveDistance(from);
    return other > 0.0 ? !wantPositive : wantPositive;
}
}  // namespace

ExtrudeFeature extrudeFeatureWithExtent(const ExtrudeFeature& from, ExtrudeExtentMode to,
                                        ExtrudeDirection preferredSide) {
    if (!extentValid(to) || to == from.extent) {
        return from;
    }
    ExtrudeFeature out = from;
    out.extent = to;
    const bool keepPositive = transitionKeepsPositive(from, preferredSide);
    const Meters kept = keepPositive ? extrudePositiveDistance(from) : extrudeNegativeDistance(from);
    switch (to) {
        case ExtrudeExtentMode::OneSide:
            // The kept side's value, on the kept side. Never a negative depth.
            out.depth = kept;
            out.direction = keepPositive ? ExtrudeDirection::AlongNormal
                                         : ExtrudeDirection::AgainstNormal;
            out.secondDistance = 0.0;
            break;
        case ExtrudeExtentMode::Symmetric:
            // The kept side's value on BOTH sides. Deliberately not an average
            // of A and B: an average is a number the user never typed.
            out.depth = kept;
            out.direction = ExtrudeDirection::AlongNormal;
            out.secondDistance = 0.0;
            break;
        case ExtrudeExtentMode::TwoSides:
            // Both sides start from what the previous mode reached, so the
            // solid does not jump and the second side is a readable, editable
            // starting point rather than zero.
            out.depth = extrudePositiveDistance(from) > 0.0 ? extrudePositiveDistance(from) : kept;
            out.secondDistance =
                    extrudeNegativeDistance(from) > 0.0 ? extrudeNegativeDistance(from) : kept;
            out.direction = ExtrudeDirection::AlongNormal;
            break;
    }
    return out;
}

ExtrudeFeature extrudeFeatureWithSide(const ExtrudeFeature& from, bool positiveSide,
                                      Meters distance) {
    ExtrudeFeature out = from;
    switch (from.extent) {
        case ExtrudeExtentMode::OneSide: {
            const bool solidIsPositive = from.direction == ExtrudeDirection::AlongNormal;
            if (positiveSide != solidIsPositive) {
                return from;  // there is no handle on that side to have moved
            }
            out.depth = distance;
            break;
        }
        case ExtrudeExtentMode::Symmetric:
            // Either handle writes the ONE distance, which is what keeps the
            // two sides equal through a drag as well as through a typed value.
            out.depth = distance;
            break;
        case ExtrudeExtentMode::TwoSides:
            if (positiveSide) {
                out.depth = distance;
            } else {
                out.secondDistance = distance;
            }
            break;
    }
    return out;
}

ExtrudeFeature extrudeFeatureWithPrimary(const ExtrudeFeature& from, Meters distance,
                                         ExtrudeDirection direction) {
    ExtrudeFeature out = from;
    out.depth = distance;
    if (from.extent == ExtrudeExtentMode::OneSide) {
        out.direction = direction;
    }
    return out;
}

bool sameCadBodyState(const CadBodyState& a, const CadBodyState& b) {
    return sameCadSketch(a.sketch, b.sketch)
           && a.extrude.profileEntityId == b.extrude.profileEntityId
           && sameBits(a.extrude.depth, b.extrude.depth)
           && a.extrude.direction == b.extrude.direction
           && a.extrude.extent == b.extrude.extent
           && sameBits(a.extrude.secondDistance, b.extrude.secondDistance);
}

CadStatus validateCadBodyState(const CadBodyState& state, ProfileExtraction* outProfiles) {
    const CadStatus sketchWhy = validateCadSketch(state.sketch);
    if (sketchWhy != CadStatus::Ok) {
        return sketchWhy;
    }
    if (!std::isfinite(state.extrude.depth) || !std::isfinite(state.extrude.secondDistance)) {
        return CadStatus::NonFinite;
    }
    if (!directionValid(state.extrude.direction)) {
        return CadStatus::InvalidExtrudeDirection;
    }
    // The extent mode, and the ONE canonical form it allows. Refused by name
    // rather than repaired, so a file, a history step and a live edit can never
    // disagree about which of two encodings of one solid is the real one.
    if (!extentValid(state.extrude.extent) || !extrudeFeatureCanonical(state.extrude)) {
        return CadStatus::InvalidExtrudeExtent;
    }
    const Meters positive = extrudePositiveDistance(state.extrude);
    const Meters negative = extrudeNegativeDistance(state.extrude);
    if (!sideDistanceValid(positive) || !sideDistanceValid(negative)) {
        return CadStatus::InvalidExtrudeDepth;
    }
    // A side may be zero only in Two Sides, where the other side carries the
    // extent. One Side and Symmetric both state a length, and a length of zero
    // is refused exactly as it always was -- never clamped.
    if (state.extrude.extent != ExtrudeExtentMode::TwoSides
        && validateDimensionMeters(state.extrude.depth) != DimensionValidation::Ok) {
        return CadStatus::InvalidExtrudeDepth;
    }
    // The SOLID's own rule: whatever the mode, the total span is a usable
    // Construction length within the sketch's bound. This is what refuses a
    // Two Sides body whose two sides are both zero.
    const Meters span = positive + negative;
    if (validateDimensionMeters(span) != DimensionValidation::Ok
        || span > kMaxSketchCoordinateMeters) {
        return CadStatus::InvalidExtrudeDepth;
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
    // The solid always spans from its -N face to its +N face, and the two
    // DISTANCES say how far each reaches. Building it this way means the
    // winding rule below never has to ask which mode or which side the user
    // chose: One Side puts one of them at zero, Symmetric makes them equal, and
    // Two Sides makes them independent, and the arithmetic here is one line for
    // all three.
    const double nearOffset = -extrudeNegativeDistance(state.extrude);
    const double farOffset = extrudePositiveDistance(state.extrude);

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
    candidate.extrude = extrudeFeatureWithPrimary(candidate.extrude, depth, direction);
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
        // A chain anchored by a Line, an Arc or a Spline, and a closed
        // Polyline, are all Polygon: the shell offers no per-vertex numeric
        // field for any of them, and a curve's authored points are edited in
        // the sketch rather than through the body's own editor.
        case SketchEntityKind::Polyline:
        case SketchEntityKind::Arc:
        case SketchEntityKind::Spline:
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
