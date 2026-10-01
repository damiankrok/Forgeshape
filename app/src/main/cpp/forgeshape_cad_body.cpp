#include "forgeshape_cad_body.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numeric>

#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_kernel.h"

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

const char* cadSelectionKindName(CadSelectionKind kind) {
    switch (kind) {
        case CadSelectionKind::LoopRegions: return "LoopRegions";
        case CadSelectionKind::PlanarFaces: return "PlanarFaces";
    }
    return "unknown";
}

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

// ---------------------------------------------------------------------------
// Region selection (`CAD-VERTICAL-SLICE-R1`)
// ---------------------------------------------------------------------------

std::vector<ProfileRegionRef> extrudeRegions(const ExtrudeFeature& extrude) {
    std::vector<ProfileRegionRef> regions;
    if (extrude.profileEntityId == kNoSketchEntity) {
        return regions;
    }
    regions.push_back(ProfileRegionRef{extrude.profileEntityId, extrude.profileHoleIds});
    regions.insert(regions.end(), extrude.additionalRegions.begin(), extrude.additionalRegions.end());
    return regions;
}

void setExtrudeRegions(ExtrudeFeature* extrude, std::vector<ProfileRegionRef> regions) {
    if (extrude == nullptr) {
        return;
    }
    std::sort(regions.begin(), regions.end(), [](const ProfileRegionRef& a, const ProfileRegionRef& b) {
        return a.outerAnchorId < b.outerAnchorId;
    });
    for (ProfileRegionRef& region : regions) {
        std::sort(region.holeAnchorIds.begin(), region.holeAnchorIds.end());
    }
    extrude->additionalRegions.clear();
    if (regions.empty()) {
        extrude->profileEntityId = kNoSketchEntity;
        extrude->profileHoleIds.clear();
        return;
    }
    extrude->profileEntityId = regions.front().outerAnchorId;
    extrude->profileHoleIds = regions.front().holeAnchorIds;
    extrude->additionalRegions.assign(regions.begin() + 1, regions.end());
}

bool extrudeSelectsSingleSimpleProfile(const ExtrudeFeature& extrude) {
    return extrude.selection == CadSelectionKind::LoopRegions
           && extrude.profileEntityId != kNoSketchEntity && extrude.profileHoleIds.empty()
           && extrude.additionalRegions.empty();
}

// ---------------------------------------------------------------------------
// The feature chain
// ---------------------------------------------------------------------------

const char* cadFeatureOperationName(CadFeatureOperation operation) {
    switch (operation) {
        case CadFeatureOperation::NewBody: return "NewBody";
        case CadFeatureOperation::Add: return "Add";
        case CadFeatureOperation::Cut: return "Cut";
    }
    return "unknown";
}

bool cadFeatureOperationFromIndex(int index, CadFeatureOperation* out) {
    if (out == nullptr) {
        return false;
    }
    switch (index) {
        case 0: *out = CadFeatureOperation::NewBody; return true;
        case 1: *out = CadFeatureOperation::Add; return true;
        case 2: *out = CadFeatureOperation::Cut; return true;
        default: return false;
    }
}

int cadFeatureOperationIndex(CadFeatureOperation operation) { return static_cast<int>(operation); }

bool sameCadFeatureSupport(const CadFeatureSupport& a, const CadFeatureSupport& b) {
    return a.featureId == b.featureId && sameCadFaceToken(a.face, b.face)
           && a.lineageToken == b.lineageToken;
}

bool sameExtrudeFeature(const ExtrudeFeature& a, const ExtrudeFeature& b) {
    if (a.profileEntityId != b.profileEntityId || a.profileHoleIds != b.profileHoleIds
        || a.additionalRegions.size() != b.additionalRegions.size()) {
        return false;
    }
    for (size_t i = 0; i < a.additionalRegions.size(); ++i) {
        if (!sameProfileRegionRef(a.additionalRegions[i], b.additionalRegions[i])) {
            return false;
        }
    }
    if (a.selection != b.selection || a.planarFaces.size() != b.planarFaces.size()) {
        return false;
    }
    for (size_t i = 0; i < a.planarFaces.size(); ++i) {
        if (!samePlanarFaceRef(a.planarFaces[i], b.planarFaces[i])) {
            return false;
        }
    }
    return sameBits(a.depth, b.depth) && a.direction == b.direction && a.extent == b.extent
           && sameBits(a.secondDistance, b.secondDistance);
}

bool sameCadSketchRecord(const CadSketchRecord& a, const CadSketchRecord& b) {
    return a.sketchId == b.sketchId && a.hasFeatureSupport == b.hasFeatureSupport
           && sameCadFeatureSupport(a.featureSupport, b.featureSupport)
           && sameCadSketch(a.sketch, b.sketch);
}

bool sameCadFeature(const CadFeature& a, const CadFeature& b) {
    return a.featureId == b.featureId && a.operation == b.operation && a.sketchId == b.sketchId
           && sameExtrudeFeature(a.extrude, b.extrude);
}

// ---------------------------------------------------------------------------
// The retained sketch table (`CAD-V6-S1`)
// ---------------------------------------------------------------------------

const CadSketchRecord* findCadSketchRecord(const CadBodyState& state, CadSketchId sketchId) {
    if (sketchId == kNoCadSketch) {
        return nullptr;
    }
    for (const CadSketchRecord& record : state.sketches) {
        if (record.sketchId == sketchId) {
            return &record;
        }
    }
    return nullptr;
}

CadSketchRecord* findCadSketchRecord(CadBodyState& state, CadSketchId sketchId) {
    return const_cast<CadSketchRecord*>(
            findCadSketchRecord(static_cast<const CadBodyState&>(state), sketchId));
}

const CadSketch& cadBaseSketch(const CadBodyState& state) {
    const CadSketchRecord* record = findCadSketchRecord(state, state.baseSketchId);
    if (record != nullptr) {
        return record->sketch;
    }
    static const CadSketch kEmpty{};
    return kEmpty;
}

CadSketch& cadBaseSketch(CadBodyState& state) {
    if (CadSketchRecord* record = findCadSketchRecord(state, state.baseSketchId)) {
        return record->sketch;
    }
    // Never on a validated state. The record the state already names is
    // restored in id order, so this door can never mint a second base sketch
    // beside the one the state points at.
    if (state.baseSketchId == kNoCadSketch) {
        state.baseSketchId = state.nextSketchId == kNoCadSketch ? kBaseCadSketchId
                                                                : state.nextSketchId;
    }
    CadSketchRecord record;
    record.sketchId = state.baseSketchId;
    auto at = std::find_if(state.sketches.begin(), state.sketches.end(),
                           [&record](const CadSketchRecord& r) { return r.sketchId > record.sketchId; });
    at = state.sketches.insert(at, std::move(record));
    if (state.nextSketchId <= state.baseSketchId) {
        state.nextSketchId = state.baseSketchId + 1u;
    }
    return at->sketch;
}

const CadSketchRecord* cadFeatureSketchRecord(const CadBodyState& state, uint32_t featureId) {
    if (featureId == kCadFeatureId) {
        return findCadSketchRecord(state, state.baseSketchId);
    }
    for (const CadFeature& feature : state.laterFeatures) {
        if (feature.featureId == featureId) {
            return findCadSketchRecord(state, feature.sketchId);
        }
    }
    return nullptr;
}

CadSketchRecord* cadFeatureSketchRecord(CadBodyState& state, uint32_t featureId) {
    return const_cast<CadSketchRecord*>(
            cadFeatureSketchRecord(static_cast<const CadBodyState&>(state), featureId));
}

CadBodyState makeCadBodyState(CadSketch sketch, ExtrudeFeature extrude) {
    CadBodyState state;
    state.sketches.front().sketch = std::move(sketch);
    state.extrude = std::move(extrude);
    return state;
}

CadSketchId addCadSketchRecord(CadBodyState* state, CadSketch sketch,
                               const CadFeatureSupport* support) {
    if (state == nullptr || state->sketches.size() >= kMaxCadSketches
        || state->nextSketchId == kNoCadSketch || state->nextSketchId == 0xFFFFFFFFu) {
        return kNoCadSketch;
    }
    for (const CadSketchRecord& existing : state->sketches) {
        if (existing.sketchId >= state->nextSketchId) {
            return kNoCadSketch;  // the high-water mark would mint a collision
        }
    }
    CadSketchRecord record;
    record.sketchId = state->nextSketchId++;
    record.hasFeatureSupport = support != nullptr;
    if (support != nullptr) {
        record.featureSupport = *support;
    }
    record.sketch = std::move(sketch);
    // Appending keeps the table ascending: every existing id is below the one
    // just minted.
    state->sketches.push_back(std::move(record));
    return state->sketches.back().sketchId;
}

uint32_t appendCadLaterFeature(CadBodyState* state, CadFeatureOperation operation,
                               CadSketchId sketchId, ExtrudeFeature extrude) {
    if (state == nullptr || cadFeatureCount(*state) >= kMaxCadFeatures
        || state->nextFeatureId <= kCadFeatureId || state->nextFeatureId == 0xFFFFFFFFu) {
        return 0;
    }
    if (!state->laterFeatures.empty() && state->laterFeatures.back().featureId >= state->nextFeatureId) {
        return 0;
    }
    CadFeature feature;
    feature.featureId = state->nextFeatureId++;
    feature.operation = operation;
    feature.sketchId = sketchId;
    feature.extrude = std::move(extrude);
    state->laterFeatures.push_back(std::move(feature));
    return state->laterFeatures.back().featureId;
}

uint32_t appendCadLaterFeatureWithSketch(CadBodyState* state, CadFeatureOperation operation,
                                         const CadFeatureSupport& support, CadSketch sketch,
                                         ExtrudeFeature extrude) {
    if (state == nullptr || cadFeatureCount(*state) >= kMaxCadFeatures) {
        return 0;
    }
    CadBodyState candidate = *state;
    const CadSketchId sketchId = addCadSketchRecord(&candidate, std::move(sketch), &support);
    if (sketchId == kNoCadSketch) {
        return 0;
    }
    const uint32_t featureId =
            appendCadLaterFeature(&candidate, operation, sketchId, std::move(extrude));
    if (featureId == 0) {
        return 0;
    }
    *state = std::move(candidate);
    return featureId;
}

bool cadBodyStateLegacyRepresentable(const CadBodyState& state) {
    const size_t later = state.laterFeatures.size();
    // A fragment side token (`CAD-V6-S2`) has no v1..v5 encoding: a body whose
    // base stands on one -- through a TopoRef to ANOTHER body's face-selected
    // feature -- or any of whose sketches does, is written v6.
    for (const CadSketchRecord& record : state.sketches) {
        if ((record.sketch.hasFaceSupport && record.sketch.faceSupport.face.fragment)
            || (record.hasFeatureSupport && record.featureSupport.face.fragment)) {
            return false;
        }
    }
    // One sketch per feature, ids 1..n in chain order, and the high-water marks
    // a legacy read derives: nothing a v1..v5 record would lose.
    if (state.baseSketchId != kBaseCadSketchId || state.sketches.size() != later + 1u
        || state.nextSketchId != static_cast<CadSketchId>(later + 2u)) {
        return false;
    }
    const uint32_t derivedNextFeature =
            later == 0 ? kCadFeatureId + 1u : state.laterFeatures.back().featureId + 1u;
    if (state.nextFeatureId != derivedNextFeature) {
        return false;
    }
    if (state.extrude.selection != CadSelectionKind::LoopRegions
        || !state.extrude.planarFaces.empty()) {
        return false;
    }
    for (size_t i = 0; i < state.sketches.size(); ++i) {
        const CadSketchRecord& record = state.sketches[i];
        if (record.sketchId != static_cast<CadSketchId>(i + 1u)) {
            return false;
        }
        // v1..v5 carry the base's placement in its own plane and TopoRef, and a
        // later feature's ONLY as a face of an earlier feature on local XY.
        const bool base = i == 0;
        if (record.hasFeatureSupport == base) {
            return false;
        }
        if (!base && (record.sketch.plane != Workplane::XY || record.sketch.hasFaceSupport)) {
            return false;
        }
    }
    for (size_t k = 0; k < later; ++k) {
        const CadFeature& feature = state.laterFeatures[k];
        if (feature.sketchId != static_cast<CadSketchId>(k + 2u)
            || feature.extrude.selection != CadSelectionKind::LoopRegions
            || !feature.extrude.planarFaces.empty()) {
            return false;
        }
    }
    return true;
}

bool cadBodyStateUsesPlanarFaces(const CadBodyState& state) {
    if (state.extrude.selection == CadSelectionKind::PlanarFaces) {
        return true;
    }
    for (const CadFeature& feature : state.laterFeatures) {
        if (feature.extrude.selection == CadSelectionKind::PlanarFaces) {
            return true;
        }
    }
    return false;
}

bool cadFeatureAt(const CadBodyState& state, uint32_t index, CadFeatureView* out) {
    if (out == nullptr || index >= cadFeatureCount(state)) {
        return false;
    }
    const uint32_t featureId =
            index == 0 ? kCadFeatureId : state.laterFeatures[index - 1u].featureId;
    const CadFeatureOperation operation =
            index == 0 ? CadFeatureOperation::NewBody : state.laterFeatures[index - 1u].operation;
    const CadSketchId sketchId =
            index == 0 ? state.baseSketchId : state.laterFeatures[index - 1u].sketchId;
    const ExtrudeFeature* extrude =
            index == 0 ? &state.extrude : &state.laterFeatures[index - 1u].extrude;
    const CadSketchRecord* record = findCadSketchRecord(state, sketchId);
    *out = CadFeatureView{featureId,
                          operation,
                          sketchId,
                          record != nullptr ? &record->sketch : nullptr,
                          extrude,
                          record != nullptr && record->hasFeatureSupport ? &record->featureSupport
                                                                         : nullptr};
    return true;
}

bool findCadFeature(const CadBodyState& state, uint32_t featureId, CadFeatureView* out) {
    const uint32_t count = cadFeatureCount(state);
    for (uint32_t i = 0; i < count; ++i) {
        CadFeatureView view;
        cadFeatureAt(state, i, &view);
        if (view.featureId == featureId) {
            if (out != nullptr) {
                *out = view;
            }
            return true;
        }
    }
    return false;
}

uint32_t nextCadFeatureId(const CadBodyState& state) { return state.nextFeatureId; }

bool sameCadBodyState(const CadBodyState& a, const CadBodyState& b) {
    if (a.sketches.size() != b.sketches.size() || a.nextSketchId != b.nextSketchId
        || a.baseSketchId != b.baseSketchId || a.nextFeatureId != b.nextFeatureId
        || !sameExtrudeFeature(a.extrude, b.extrude)
        || a.laterFeatures.size() != b.laterFeatures.size()) {
        return false;
    }
    for (size_t i = 0; i < a.sketches.size(); ++i) {
        if (!sameCadSketchRecord(a.sketches[i], b.sketches[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < a.laterFeatures.size(); ++i) {
        if (!sameCadFeature(a.laterFeatures[i], b.laterFeatures[i])) {
            return false;
        }
    }
    return true;
}

CadStatus validateExtrudeExtent(const ExtrudeFeature& extrude) {
    if (!std::isfinite(extrude.depth) || !std::isfinite(extrude.secondDistance)) {
        return CadStatus::NonFinite;
    }
    if (!directionValid(extrude.direction)) {
        return CadStatus::InvalidExtrudeDirection;
    }
    // The extent mode, and the ONE canonical form it allows. Refused by name
    // rather than repaired, so a file, a history step and a live edit can never
    // disagree about which of two encodings of one solid is the real one.
    if (!extentValid(extrude.extent) || !extrudeFeatureCanonical(extrude)) {
        return CadStatus::InvalidExtrudeExtent;
    }
    const Meters positive = extrudePositiveDistance(extrude);
    const Meters negative = extrudeNegativeDistance(extrude);
    if (!sideDistanceValid(positive) || !sideDistanceValid(negative)) {
        return CadStatus::InvalidExtrudeDepth;
    }
    // A side may be zero only in Two Sides, where the other side carries the
    // extent. One Side and Symmetric both state a length, and a length of zero
    // is refused exactly as it always was -- never clamped.
    if (extrude.extent != ExtrudeExtentMode::TwoSides
        && validateDimensionMeters(extrude.depth) != DimensionValidation::Ok) {
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
    return CadStatus::Ok;
}

namespace {

bool selectionKindValid(CadSelectionKind kind) {
    const int index = static_cast<int>(kind);
    return index >= 0 && index < kCadSelectionKindCount;
}

bool hasLoopRegionPayload(const ExtrudeFeature& extrude) {
    return extrude.profileEntityId != kNoSketchEntity || !extrude.profileHoleIds.empty()
           || !extrude.additionalRegions.empty();
}

int compareFragmentCycle(const FragmentCycle& a, const FragmentCycle& b) {
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (const int c = compareFragmentRef(a[i], b[i])) {
            return c;
        }
    }
    return a.size() < b.size() ? -1 : (b.size() < a.size() ? 1 : 0);
}

bool cutWellFormed(const ArrangementCut& cut, bool isStart) {
    switch (cut.kind) {
        case ArrangementCutKind::SourceStart:
        case ArrangementCutKind::SourceEnd:
            // A source end is only ever the start (resp. end) of a fragment,
            // and names nothing but itself.
            if ((cut.kind == ArrangementCutKind::SourceStart) != isStart) {
                return false;
            }
            return cut.partnerEntityId == kNoSketchEntity && cut.partnerEdgeLocalIndex == 0u
                   && cut.ordinal == 0u;
        case ArrangementCutKind::Intersection:
            return cut.partnerEntityId != kNoSketchEntity;
    }
    return false;
}

CadStatus validateCycleForm(const FragmentCycle& cycle) {
    if (cycle.empty() || cycle.size() > kMaxPlanarFaceCycleFragments) {
        return CadStatus::PlanarFaceRefMalformed;
    }
    for (const FragmentRef& fragment : cycle) {
        if (fragment.sourceEntityId == kNoSketchEntity || !cutWellFormed(fragment.startCut, true)
            || !cutWellFormed(fragment.endCut, false)) {
            return CadStatus::PlanarFaceRefMalformed;
        }
    }
    // Rotated to its smallest fragment, which is unique: a fragment appears at
    // most once in a cycle, so the first must be STRICTLY below every other.
    for (size_t i = 1; i < cycle.size(); ++i) {
        if (compareFragmentRef(cycle.front(), cycle[i]) >= 0) {
            return CadStatus::PlanarFaceRefNotCanonical;
        }
        for (size_t j = i + 1; j < cycle.size(); ++j) {
            if (compareFragmentRef(cycle[i], cycle[j]) == 0) {
                return CadStatus::PlanarFaceRefNotCanonical;
            }
        }
    }
    return CadStatus::Ok;
}

}  // namespace

CadStatus cadStatusForArrangement(ArrangementStatus status) {
    switch (status) {
        case ArrangementStatus::Ok: return CadStatus::Ok;
        case ArrangementStatus::InvalidSketch: return CadStatus::RegenerationFailed;
        case ArrangementStatus::UnsupportedCurve: return CadStatus::PlanarFaceUnsupportedCurve;
        case ArrangementStatus::AmbiguousOverlap: return CadStatus::PlanarFaceAmbiguousOverlap;
        case ArrangementStatus::DegenerateFace: return CadStatus::PlanarFaceDegenerate;
        case ArrangementStatus::CapExceeded: return CadStatus::PlanarFaceCapExceeded;
        case ArrangementStatus::InvalidSelection: return CadStatus::PlanarFaceUnresolved;
        case ArrangementStatus::PinchedSelection: return CadStatus::PlanarFacesTouchAtPoint;
        case ArrangementStatus::SelfIntersectingCurve: return CadStatus::SelfIntersectingProfile;
    }
    return CadStatus::RegenerationFailed;
}

CadStatus validatePlanarFaceRefForm(const PlanarFaceRef& ref) {
    if (ref.holes.size() > kMaxPlanarFaceHoles) {
        return CadStatus::PlanarFaceRefMalformed;
    }
    CadStatus why = validateCycleForm(ref.outer);
    if (why != CadStatus::Ok) {
        return why;
    }
    for (size_t h = 0; h < ref.holes.size(); ++h) {
        why = validateCycleForm(ref.holes[h]);
        if (why != CadStatus::Ok) {
            return why;
        }
        if (h > 0 && compareFragmentCycle(ref.holes[h - 1], ref.holes[h]) >= 0) {
            return CadStatus::PlanarFaceRefNotCanonical;
        }
    }
    return CadStatus::Ok;
}

CadStatus validatePlanarFaceSelection(const CadSketch& sketch, const ExtrudeFeature& extrude) {
    return resolvePlanarFaceSelection(sketch, extrude, nullptr, nullptr);
}

CadStatus resolvePlanarFaceSelection(const CadSketch& sketch, const ExtrudeFeature& extrude,
                                     SketchArrangement* outArrangement,
                                     std::vector<size_t>* outFaceIndices) {
    const CadStatus sketchWhy = validateCadSketch(sketch);
    if (sketchWhy != CadStatus::Ok) {
        return sketchWhy;
    }
    const CadStatus extentWhy = validateExtrudeExtent(extrude);
    if (extentWhy != CadStatus::Ok) {
        return extentWhy;
    }
    if (extrude.selection != CadSelectionKind::PlanarFaces || hasLoopRegionPayload(extrude)) {
        return CadStatus::InvalidSelectionKind;
    }
    const std::vector<PlanarFaceRef>& faces = extrude.planarFaces;
    if (faces.empty()) {
        return CadStatus::ProfileNotFound;
    }
    if (faces.size() > kMaxPlanarFaceSelection) {
        return CadStatus::TooManyRegions;
    }
    for (const PlanarFaceRef& face : faces) {
        const CadStatus formWhy = validatePlanarFaceRefForm(face);
        if (formWhy != CadStatus::Ok) {
            return formWhy;
        }
    }
    // A repeat anywhere is named as a repeat before the order is judged.
    for (size_t i = 0; i < faces.size(); ++i) {
        for (size_t j = i + 1; j < faces.size(); ++j) {
            if (samePlanarFaceRef(faces[i], faces[j])) {
                return CadStatus::DuplicatePlanarFace;
            }
        }
        if (i > 0 && comparePlanarFaceRef(faces[i - 1], faces[i]) >= 0) {
            return CadStatus::PlanarFaceRefNotCanonical;
        }
    }
    // The arrangement is DERIVED from the sketch every time; the selection
    // resolves against it by exact tuple equality or not at all.
    SketchArrangement arrangement = deriveSketchArrangement(sketch);
    const CadStatus arrangementWhy = cadStatusForArrangement(arrangement.status);
    if (arrangementWhy != CadStatus::Ok) {
        return arrangementWhy;
    }
    std::vector<size_t> indices;
    for (const PlanarFaceRef& face : faces) {
        size_t index = 0;
        if (!resolvePlanarFaceRef(arrangement, face, &index)) {
            return CadStatus::PlanarFaceUnresolved;
        }
        indices.push_back(index);
    }
    if (outArrangement != nullptr) {
        *outArrangement = std::move(arrangement);
    }
    if (outFaceIndices != nullptr) {
        *outFaceIndices = std::move(indices);
    }
    return CadStatus::Ok;
}

bool sketchRequiresPlanarFaces(const SketchArrangement& arrangement,
                               const SketchRegionExtraction& regions) {
    if (arrangement.status != ArrangementStatus::Ok || arrangement.faces.empty()) {
        return false;
    }
    const auto proper = [](const FragmentRef& f) {
        return !(f.startCut.kind == ArrangementCutKind::SourceStart
                 && f.endCut.kind == ArrangementCutKind::SourceEnd);
    };
    bool split = false;
    for (const AtomicPlanarFace& face : arrangement.faces) {
        for (const FragmentRef& f : face.ref.outer) split = split || proper(f);
        for (const FragmentCycle& hole : face.ref.holes) {
            for (const FragmentRef& f : hole) split = split || proper(f);
        }
    }
    return split && arrangement.faces.size() != regions.regions.size();
}

bool sketchLoopsAreExact(const SketchRegionExtraction& regions) {
    const size_t n = regions.loops.profiles.size();
    for (size_t a = 0; a < n; ++a) {
        for (size_t b = a + 1; b < n; ++b) {
            if (regions.loopsConflict(static_cast<uint32_t>(a), static_cast<uint32_t>(b))) {
                return false;
            }
        }
    }
    for (const ProfileRejection& rejection : regions.loops.rejections) {
        if (rejection.why == CadStatus::SelfIntersectingProfile
            || rejection.why == CadStatus::BranchingChain) {
            return false;
        }
    }
    return true;
}

SketchSelectionModeDecision decideSketchSelectionMode(const SketchArrangement& arrangement,
                                                      const SketchRegionExtraction& regions) {
    SketchSelectionModeDecision decision;
    if (arrangement.status == ArrangementStatus::Ok) {
        decision.kind = sketchRequiresPlanarFaces(arrangement, regions)
                                ? CadSelectionKind::PlanarFaces
                                : CadSelectionKind::LoopRegions;
        return decision;
    }
    if (sketchLoopsAreExact(regions)) {
        decision.kind = CadSelectionKind::LoopRegions;
        return decision;
    }
    decision.kind = CadSelectionKind::PlanarFaces;
    decision.status = cadStatusForArrangement(arrangement.status);
    return decision;
}

CadStatus mergePlanarFaceSelection(const SketchArrangement& arrangement,
                                   const std::vector<size_t>& faceIndices,
                                   std::vector<PlanarProfileComponent>* out) {
    std::vector<PlanarProfileComponent> components;
    const CadStatus why =
            cadStatusForArrangement(mergePlanarFaces(arrangement, faceIndices, &components));
    if (why != CadStatus::Ok) {
        return why;
    }
    for (const PlanarProfileComponent& component : components) {
        if (component.outer.polygon.size() > kMaxProfileVertices) {
            return CadStatus::TooManyEntities;
        }
        for (const PlanarProfileLoop& hole : component.holes) {
            if (hole.polygon.size() > kMaxProfileVertices) {
                return CadStatus::TooManyEntities;
            }
        }
    }
    if (out != nullptr) {
        *out = std::move(components);
    }
    return CadStatus::Ok;
}

CadStatus validateCadFeatureGeometry(const CadSketch& sketch, const ExtrudeFeature& extrude,
                                     SketchRegionExtraction* outRegions) {
    const CadStatus sketchWhy = validateCadSketch(sketch);
    if (sketchWhy != CadStatus::Ok) {
        return sketchWhy;
    }
    const CadStatus extentWhy = validateExtrudeExtent(extrude);
    if (extentWhy != CadStatus::Ok) {
        return extentWhy;
    }
    if (!selectionKindValid(extrude.selection)) {
        return CadStatus::InvalidSelectionKind;
    }
    if (extrude.selection == CadSelectionKind::PlanarFaces) {
        // The ONE rule for a face selection; and since `CAD-V6-S2` a resolved
        // one is also a buildable one only when its union merges.
        SketchArrangement arrangement;
        std::vector<size_t> faces;
        const CadStatus why = resolvePlanarFaceSelection(sketch, extrude, &arrangement, &faces);
        if (why != CadStatus::Ok) {
            return why;
        }
        return mergePlanarFaceSelection(arrangement, faces, nullptr);
    }
    if (!extrude.planarFaces.empty()) {
        return CadStatus::InvalidSelectionKind;
    }
    SketchRegionExtraction regions = extractSketchRegions(sketch);
    if (regions.loops.profiles.empty()) {
        return CadStatus::NoClosedProfile;
    }
    // Nothing chosen is `ProfileNotFound`, as it always was; telling "choose
    // one" apart from "the one you chose is gone" is the session's business.
    if (extrude.profileEntityId == kNoSketchEntity) {
        return extrude.profileHoleIds.empty() && extrude.additionalRegions.empty()
                       ? CadStatus::ProfileNotFound
                       : CadStatus::ProfileRegionMismatch;
    }
    const CadStatus regionWhy = validateRegionSelection(regions, extrudeRegions(extrude));
    if (regionWhy != CadStatus::Ok) {
        return regionWhy;
    }
    if (outRegions != nullptr) {
        *outRegions = std::move(regions);
    }
    return CadStatus::Ok;
}

namespace {

// The table's own rules, before any geometry: ids, order, high-water marks,
// references and which sketch may be placed how.
CadStatus validateCadSketchTable(const CadBodyState& state) {
    const std::vector<CadSketchRecord>& table = state.sketches;
    if (table.empty()) {
        return CadStatus::SketchNotFound;
    }
    if (table.size() > kMaxCadSketches) {
        return CadStatus::TooManySketches;
    }
    for (size_t i = 0; i < table.size(); ++i) {
        if (table[i].sketchId == kNoCadSketch) {
            return CadStatus::SketchIdInvalid;
        }
        for (size_t j = i + 1; j < table.size(); ++j) {
            if (table[i].sketchId == table[j].sketchId) {
                return CadStatus::DuplicateSketchId;
            }
        }
    }
    for (size_t i = 1; i < table.size(); ++i) {
        if (table[i].sketchId <= table[i - 1].sketchId) {
            return CadStatus::SketchIdInvalid;  // canonical order, never re-sorted
        }
    }
    if (state.nextSketchId <= table.back().sketchId) {
        return CadStatus::HighWaterInvalid;
    }
    if (cadFeatureCount(state) > kMaxCadFeatures) {
        return CadStatus::TooManyFeatures;
    }
    uint32_t previous = kCadFeatureId;
    for (const CadFeature& feature : state.laterFeatures) {
        if (feature.featureId <= previous) {
            return CadStatus::TooManyFeatures;  // the chain's id rule, as it always was
        }
        previous = feature.featureId;
    }
    if (state.nextFeatureId <= previous) {
        return CadStatus::HighWaterInvalid;
    }
    const CadSketchRecord* base = findCadSketchRecord(state, state.baseSketchId);
    if (base == nullptr) {
        return CadStatus::SketchNotFound;
    }
    if (base->hasFeatureSupport) {
        return CadStatus::SketchSupportInvalid;  // there is no earlier feature to stand on
    }
    for (const CadFeature& feature : state.laterFeatures) {
        if (findCadSketchRecord(state, feature.sketchId) == nullptr) {
            return CadStatus::SketchNotFound;
        }
    }
    // Exactly ONE root sketch: the base's. A second sketch on a workplane or on
    // another body's face would be a second answer to where the body is.
    for (const CadSketchRecord& record : table) {
        if (!record.hasFeatureSupport && record.sketchId != state.baseSketchId) {
            return CadStatus::SketchSupportInvalid;
        }
    }
    return CadStatus::Ok;
}

}  // namespace

CadStatus validateCadBodyState(const CadBodyState& state, ProfileExtraction* outProfiles) {
    const CadStatus tableWhy = validateCadSketchTable(state);
    if (tableWhy != CadStatus::Ok) {
        return tableWhy;
    }
    SketchRegionExtraction baseRegions;
    const CadStatus baseWhy =
            validateCadFeatureGeometry(cadBaseSketch(state), state.extrude, &baseRegions);
    if (baseWhy != CadStatus::Ok) {
        return baseWhy;
    }
    if (!state.laterFeatures.empty() || state.sketches.size() > 1u) {
        // The chain's own rules: operations, every LoopRegions feature's
        // geometry and support, every PlanarFaces feature's selection, and
        // every unconsumed sketch's own rule and support.
        const CadStatus chainWhy = validateCadChain(state);
        if (chainWhy != CadStatus::Ok) {
            return chainWhy;
        }
    }
    if (outProfiles != nullptr) {
        *outProfiles = std::move(baseRegions.loops);
    }
    return CadStatus::Ok;
}

namespace {

// R0's regeneration, UNCHANGED: one simple profile, one prism, float
// throughout. Every body any earlier version created takes this path, so its
// published mesh is bit-identical to what it always was.
CadStatus generateSimpleProfileMesh(const CadBodyState& state, const ClosedProfile& profile,
                                    ConstructionMesh* out) {
    const ClosedProfile* chosen = &profile;
    std::vector<uint32_t> capIndices;
    const CadStatus triWhy = triangulateSimplePolygon(chosen->polygon, &capIndices);
    if (triWhy != CadStatus::Ok) {
        return triWhy;
    }

    const uint32_t n = static_cast<uint32_t>(chosen->polygon.size());
    const Workplane plane = cadBaseSketch(state).plane;
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
        const Vec3 nearPoint = workplaneToLocalAtOffset(plane, chosen->polygon[i], nearOffset);
        const Vec3 farPoint = workplaneToLocalAtOffset(plane, chosen->polygon[i], farOffset);
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

// The derived solid, as the float render mesh plus its per-triangle faces.
CadStatus solidToBodyMesh(const CadSolid& solid, std::vector<CadMeshFace> faces, CadBodyMesh* out) {
    CadBodyMesh result;
    const uint32_t vertexCount = solid.vertexCount();
    result.mesh.vertices.resize(vertexCount);
    for (uint32_t i = 0; i < vertexCount; ++i) {
        MeshVertex& v = result.mesh.vertices[i];
        for (int c = 0; c < 3; ++c) {
            v.position[c] = static_cast<float>(solid.positions[i * 3u + static_cast<uint32_t>(c)]);
            if (!std::isfinite(v.position[c])) {
                return CadStatus::RegenerationFailed;
            }
            v.color[c] = kCadBodyVertexColor[c];
        }
    }
    result.mesh.indices = solid.indices;
    result.mesh.renderBothSides = false;
    result.triangleFace = solid.faceTags;
    for (uint32_t tag : result.triangleFace) {
        if (tag >= faces.size()) {
            return CadStatus::RegenerationFailed;
        }
    }
    result.faces = std::move(faces);
    *out = std::move(result);
    return CadStatus::Ok;
}

// Triangles grouped by ascending face tag, stable within a tag: the canonical
// order a region prism publishes in, so every face is one contiguous range.
void sortSolidByTag(CadSolid* solid) {
    const size_t triangles = solid->faceTags.size();
    std::vector<uint32_t> order(triangles);
    std::iota(order.begin(), order.end(), 0u);
    std::stable_sort(order.begin(), order.end(), [solid](uint32_t a, uint32_t b) {
        return solid->faceTags[a] < solid->faceTags[b];
    });
    std::vector<uint32_t> indices;
    std::vector<uint32_t> tags;
    indices.reserve(solid->indices.size());
    tags.reserve(triangles);
    for (uint32_t t : order) {
        indices.insert(indices.end(), {solid->indices[t * 3u], solid->indices[t * 3u + 1u],
                                       solid->indices[t * 3u + 2u]});
        tags.push_back(solid->faceTags[t]);
    }
    solid->indices = std::move(indices);
    solid->faceTags = std::move(tags);
}

// An Add or a Cut may not be a silent no-op: an effect smaller than this share
// of the tool's own volume is "no effect". Relative, so it means the same for
// a millimetre part and a metre one; far above the kernel's own rounding.
constexpr double kCadOperationEffectFraction = 1.0e-9;

}  // namespace

CadStatus regenerateCadBody(const CadBodyState& state, CadBodyMesh* out,
                            CadRegenerationReport* report) {
    CadRegenerationReport local;
    auto finish = [&local, report](CadStatus why, uint32_t featureId) {
        local.status = why;
        local.failedFeatureId = why == CadStatus::Ok ? 0u : featureId;
        if (report != nullptr) {
            *report = local;
        }
        return why;
    };
    if (out == nullptr) {
        return finish(CadStatus::RegenerationFailed, 0u);
    }
    std::vector<CadFeatureGeometry> chain;
    uint32_t failedFeature = 0;
    const CadStatus chainWhy = buildCadChainGeometry(state, &chain, &failedFeature);
    if (chainWhy != CadStatus::Ok) {
        return finish(chainWhy, failedFeature);
    }
    const CadFeatureGeometry& base = chain.front();

    // The face table: every feature's own faces, in chain order.
    std::vector<CadMeshFace> faces;
    std::vector<uint32_t> tagOffset;
    for (const CadFeatureGeometry& g : chain) {
        tagOffset.push_back(static_cast<uint32_t>(faces.size()));
        for (const CadFeatureFace& face : g.faces) {
            faces.push_back(CadMeshFace{g.featureId, face.token, face.eligible});
        }
    }

    // Every body any earlier version created: R0's path, bit for bit.
    if (chain.size() == 1u && extrudeSelectsSingleSimpleProfile(base.extrude)) {
        const SketchRegion& region = base.regions.regions[base.chosen.front()];
        const ClosedProfile& profile = base.regions.loops.profiles[region.outerLoop];
        CadBodyMesh result;
        const CadStatus why = generateSimpleProfileMesh(state, profile, &result.mesh);
        if (why != CadStatus::Ok) {
            return finish(why, kCadFeatureId);
        }
        // R0's emission order: +N cap, -N cap, then one quad per edge.
        const uint32_t n = static_cast<uint32_t>(profile.polygon.size());
        const uint32_t capTriangles = n - 2u;
        const bool upperIsPlaneCap = state.extrude.direction == ExtrudeDirection::AgainstNormal;
        result.triangleFace.reserve(result.mesh.indices.size() / 3u);
        for (uint32_t t = 0; t < capTriangles; ++t) {
            result.triangleFace.push_back(upperIsPlaneCap ? 0u : 1u);
        }
        for (uint32_t t = 0; t < capTriangles; ++t) {
            result.triangleFace.push_back(upperIsPlaneCap ? 1u : 0u);
        }
        for (uint32_t k = 0; k < n; ++k) {
            result.triangleFace.push_back(2u + k);
            result.triangleFace.push_back(2u + k);
        }
        result.faces = std::move(faces);
        result.volume = region.area * (base.farOffset - base.nearOffset);
        result.components = 1u;
        *out = std::move(result);
        return finish(CadStatus::Ok, 0u);
    }

    CadSolid body;
    CadStatus why = appendCadFeatureSolid(base, tagOffset[0], &body);
    if (why != CadStatus::Ok) {
        return finish(why, kCadFeatureId);
    }
    uint32_t components = base.componentCount();
    double volume = cadSolidVolume(body);
    if (chain.size() > 1u) {
        const auto t0 = std::chrono::steady_clock::now();
        CadSolidMeasure measure;
        if (cadKernelValidateSolid(body, &measure) != CadKernelStatus::Ok) {
            return finish(CadStatus::KernelFailed, kCadFeatureId);
        }
        components = measure.components;
        volume = measure.volume;
        for (size_t i = 1; i < chain.size(); ++i) {
            const CadFeatureGeometry& feature = chain[i];
            // The support must still carry material where this feature stands:
            // an earlier Cut that removed the whole face refuses by name rather
            // than leave a sketch floating in empty space. A feature extruding
            // the ROOT sketch (`CAD-V6-S1`, a sketch the base also extrudes)
            // stands on no face of the body, so there is no face to have lost.
            CadFeatureView view;
            const bool onFeatureFace = cadFeatureAt(state, static_cast<uint32_t>(i), &view)
                                       && view.support != nullptr;
            if (onFeatureFace && !cadSolidHasFaceOn(body, feature.placement)) {
                return finish(CadStatus::SupportFaceLost, feature.featureId);
            }
            CadSolid tool;
            why = appendCadFeatureSolid(feature, tagOffset[i], &tool);
            if (why != CadStatus::Ok) {
                return finish(why, feature.featureId);
            }
            CadSolidMeasure toolMeasure;
            if (cadKernelValidateSolid(tool, &toolMeasure) != CadKernelStatus::Ok) {
                return finish(CadStatus::KernelFailed, feature.featureId);
            }
            const bool add = feature.operation == CadFeatureOperation::Add;
            CadSolid result;
            if (cadKernelBoolean(body, tool, add ? CadBooleanOp::Union : CadBooleanOp::Difference,
                                 &result)
                != CadKernelStatus::Ok) {
                return finish(CadStatus::KernelFailed, feature.featureId);
            }
            CadSolidMeasure resultMeasure;
            if (cadKernelMeasure(result, &resultMeasure) != CadKernelStatus::Ok) {
                return finish(CadStatus::KernelFailed, feature.featureId);
            }
            const double effectFloor = kCadOperationEffectFraction * toolMeasure.volume;
            if (add) {
                // Contact is required: a union that ADDS a shell would be a
                // disconnected lump, which is what New Body is for.
                if (resultMeasure.components > components) {
                    return finish(CadStatus::AddDisjoint, feature.featureId);
                }
                if (resultMeasure.volume - volume <= effectFloor) {
                    return finish(CadStatus::AddNoEffect, feature.featureId);
                }
            } else {
                if (result.empty()) {
                    return finish(CadStatus::CutRemovesBody, feature.featureId);
                }
                if (volume - resultMeasure.volume <= effectFloor) {
                    return finish(CadStatus::CutNoIntersection, feature.featureId);
                }
            }
            body = std::move(result);
            components = resultMeasure.components;
            volume = resultMeasure.volume;
        }
        local.kernelMicros = std::chrono::duration<double, std::micro>(
                                     std::chrono::steady_clock::now() - t0)
                                     .count();
    } else {
        sortSolidByTag(&body);
    }
    CadBodyMesh result;
    why = solidToBodyMesh(body, std::move(faces), &result);
    if (why != CadStatus::Ok) {
        return finish(why, 0u);
    }
    result.volume = volume;
    result.components = components;
    *out = std::move(result);
    return finish(CadStatus::Ok, 0u);
}

CadStatus generateCadMesh(const CadBodyState& state, ConstructionMesh* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    CadBodyMesh result;
    const CadStatus why = regenerateCadBody(state, &result);
    if (why != CadStatus::Ok) {
        return why;
    }
    *out = std::move(result.mesh);
    return CadStatus::Ok;
}

// ---------------------------------------------------------------------------
// The body
// ---------------------------------------------------------------------------

CadStatus CadBody::generateMesh(ConstructionMesh* out) const {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    std::shared_ptr<const CadBodyMesh> mesh;
    const CadStatus why = regenerated(&mesh);
    if (why != CadStatus::Ok) {
        return why;
    }
    *out = mesh->mesh;
    return CadStatus::Ok;
}

CadStatus CadBody::regenerated(std::shared_ptr<const CadBodyMesh>* out) const {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    if (cache_ != nullptr && sameCadBodyState(cacheState_, state_)) {
        *out = cache_;
        return CadStatus::Ok;
    }
    auto fresh = std::make_shared<CadBodyMesh>();
    const CadStatus why = regenerateCadBody(state_, fresh.get());
    if (why != CadStatus::Ok) {
        return why;
    }
    cache_ = fresh;
    cacheState_ = state_;
    *out = cache_;
    return CadStatus::Ok;
}

CadStatus CadBody::applyState(const CadBodyState& requested, bool* outChanged) {
    if (outChanged != nullptr) {
        *outChanged = false;
    }
    // A forward edit never LOWERS a high-water mark. This is what makes an id
    // unique along the forward branch: every state the Undo stack can return
    // to is the body's creation state or was reached from it through this
    // door, so every id it holds is below the current marks, and the only
    // states that can hold an id the next mint re-uses are on the REDO side --
    // which the commit that mints it clears (`CadSketchId`,
    // forgeshape_cad_body.h). A history restore is not an edit and goes
    // through `restoreState`, which moves the marks back with the rest of the
    // snapshot.
    if (requested.nextFeatureId < state_.nextFeatureId
        || requested.nextSketchId < state_.nextSketchId) {
        ++rejectedUpdates_;
        return CadStatus::HighWaterInvalid;
    }
    // Regenerate ONCE into a scratch mesh: the only proof a state is usable is
    // that the whole path runs, and running it here is what makes the refusal
    // land before a byte of the body has moved.
    auto scratch = std::make_shared<CadBodyMesh>();
    const CadStatus why = regenerateCadBody(requested, scratch.get());
    if (why != CadStatus::Ok) {
        ++rejectedUpdates_;
        return why;
    }
    if (sameCadBodyState(state_, requested)) {
        return CadStatus::Ok;  // identical: nothing written, nothing counted
    }
    state_ = requested;
    // The proof that the state regenerates IS its mesh: keep it, so the
    // publish that follows does no kernel work twice.
    cache_ = scratch;
    cacheState_ = state_;
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
            findSketchEntity(cadBaseSketch(candidate), candidate.extrude.profileEntityId);
    if (anchor == nullptr || anchor->rectangle() == nullptr) {
        return CadStatus::ProfileNotFound;
    }
    SketchRectangle rectangle = *anchor->rectangle();
    rectangle.width = width;
    rectangle.height = height;
    const CadStatus why = replaceSketchEntity(&cadBaseSketch(candidate), anchor->id(), rectangle);
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
            findSketchEntity(cadBaseSketch(candidate), candidate.extrude.profileEntityId);
    if (anchor == nullptr || anchor->circle() == nullptr) {
        return CadStatus::ProfileNotFound;
    }
    SketchCircle circle = *anchor->circle();
    circle.radius = radius;
    const CadStatus why = replaceSketchEntity(&cadBaseSketch(candidate), anchor->id(), circle);
    if (why != CadStatus::Ok) {
        return why;
    }
    return body.applyState(candidate, outChanged);
}

CadProfileKind cadProfileKind(const CadBodyState& state) {
    const SketchEntity* anchor = findSketchEntity(cadBaseSketch(state), state.extrude.profileEntityId);
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
