#include "forgeshape_cad_v6_selftest.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_history.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch_arrangement.h"

namespace forgeshape {

namespace {

struct Checks {
    std::vector<CadV6SelfTestCheck>* out;
    void check(const char* name, bool ok) { out->push_back(CadV6SelfTestCheck{name, ok}); }
};

// ---------------------------------------------------------------------------
// Sketches, faces and states
// ---------------------------------------------------------------------------

SketchEntityId add(CadSketch* sketch, SketchEntity::Payload payload) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, std::move(payload), &id);
    return id;
}

void rect(CadSketch* s, double cu, double cv, double w, double h) {
    SketchRectangle r;
    r.center = SketchPoint{cu, cv};
    r.width = w;
    r.height = h;
    add(s, r);
}

void circle(CadSketch* s, double cu, double cv, double radius) {
    SketchCircle c;
    c.center = SketchPoint{cu, cv};
    c.radius = radius;
    add(s, c);
}

void line(CadSketch* s, double u0, double v0, double u1, double v1) {
    SketchLine l;
    l.start = SketchPoint{u0, v0};
    l.end = SketchPoint{u1, v1};
    add(s, l);
}

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

// PF-S1-01's sketch: a 4 x 3 m rectangle and a 0.5 m circle on its right side.
CadSketch lensSketch() {
    CadSketch s;
    rect(&s, 0.0, 0.0, 4.0, 3.0);
    circle(&s, 2.0, 0.0, 0.5);
    return s;
}

// PF-S1-02 without its dangling lines: three lines closing a 1 x 1 m square
// against the rectangle's right side through two T-junctions.
CadSketch protrusionSketch() {
    CadSketch s;
    rect(&s, 0.0, 0.0, 4.0, 3.0);
    line(&s, 2.0, -0.5, 3.0, -0.5);
    line(&s, 3.0, -0.5, 3.0, 0.5);
    line(&s, 3.0, 0.5, 2.0, 0.5);
    return s;
}

CadSketch twoCircleSketch(double offset, double radius) {
    CadSketch s;
    circle(&s, -offset, 0.0, radius);
    circle(&s, offset, 0.0, radius);
    return s;
}

CadSketch rectSketch(double cu, double cv, double w, double h) {
    CadSketch s;
    rect(&s, cu, cv, w, h);
    return s;
}

// The face of `sketch`'s arrangement with `area` whose first outer fragment is
// walked forward (or, with `firstReversed`, backward) -- the lens INSIDE the
// rectangle for the lens sketch, the protrusion beside it. Taken from the
// DERIVED arrangement, never written by hand: what the production encoder then
// writes is the engine's own canonical ref.
PlanarFaceRef derivedFace(const CadSketch& sketch, double area, bool firstReversed = false) {
    const SketchArrangement arrangement = deriveSketchArrangement(sketch);
    for (const AtomicPlanarFace& face : arrangement.faces) {
        if (near(face.area, area, 1e-6) && !face.ref.outer.empty()
            && face.ref.outer[0].reversed == firstReversed) {
            return face.ref;
        }
    }
    return PlanarFaceRef{};
}

constexpr double kPi = 3.14159265358979323846;
const double kLensArea = kPi * 0.25 * 0.5;  // half of a 0.5 m disk

double circleLensArea(double r, double d) {
    return 2.0 * r * r * std::acos(d / (2.0 * r)) - 0.5 * d * std::sqrt(4.0 * r * r - d * d);
}

ExtrudeFeature loopExtrude(SketchEntityId profile, std::vector<SketchEntityId> holes,
                           std::vector<ProfileRegionRef> additional, double depth,
                           ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature e;
    e.profileEntityId = profile;
    e.profileHoleIds = std::move(holes);
    e.additionalRegions = std::move(additional);
    e.depth = depth;
    e.direction = direction;
    return e;
}

ExtrudeFeature faceExtrude(std::vector<PlanarFaceRef> faces, double depth) {
    ExtrudeFeature e;
    e.selection = CadSelectionKind::PlanarFaces;
    e.planarFaces = std::move(faces);
    e.profileEntityId = kNoSketchEntity;
    e.depth = depth;
    return e;
}

CadFeatureSupport capSupport(const CadBodyState& state, uint32_t featureId,
                             CadFaceKind kind = CadFaceKind::CapFar) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face.kind = kind;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

// The 2 x 2 m block, 1 m along +Z.
CadBodyState blockState() {
    return makeCadBodyState(rectSketch(0.0, 0.0, 2.0, 2.0), loopExtrude(1, {}, {}, 1.0));
}

// ONE sketch -- a 4 x 3 m rectangle around a 1 x 1 m square -- extruded by TWO
// features: the base takes the ring AND the square (the whole block), and an
// Add takes the square alone, 0.5 m against the normal, from the same sketch.
CadBodyState sharedState() {
    CadSketch sketch;
    rect(&sketch, 0.0, 0.0, 4.0, 3.0);
    rect(&sketch, 0.0, 0.0, 1.0, 1.0);
    CadBodyState state = makeCadBodyState(
            std::move(sketch), loopExtrude(1, {2}, {ProfileRegionRef{2, {}}}, 1.0));
    appendCadLaterFeature(&state, CadFeatureOperation::Add, kBaseCadSketchId,
                          loopExtrude(2, {}, {}, 0.5, ExtrudeDirection::AgainstNormal));
    return state;
}

CadBodyState lensState() {
    return makeCadBodyState(lensSketch(), faceExtrude({derivedFace(lensSketch(), kLensArea)}, 1.0));
}

CadBodyState protrusionState() {
    return makeCadBodyState(protrusionSketch(),
                            faceExtrude({derivedFace(protrusionSketch(), 1.0, true)}, 1.0));
}

CadBodyState twoCirclesState() {
    const CadSketch sketch = twoCircleSketch(0.4, 1.0);
    return makeCadBodyState(sketch,
                            faceExtrude({derivedFace(sketch, circleLensArea(1.0, 0.8))}, 1.0));
}

// A LoopRegions base and a PlanarFaces Add on its far cap.
CadBodyState mixedState() {
    CadBodyState state = blockState();
    const CadSketch onCap = twoCircleSketch(0.2, 0.5);
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Add, capSupport(state, 1), onCap,
                                    faceExtrude({derivedFace(onCap, circleLensArea(0.5, 0.4))}, 0.25));
    return state;
}

ProjectDocument documentFor(const CadBodyState& state) {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;
    ProjectBodyPlacement placement;
    placement.objectId = 1;
    document.scene.bodies.push_back(placement);
    document.hasCad = true;
    ProjectCadBody body;
    body.objectId = 1;
    body.state = state;
    document.cad.bodies.push_back(std::move(body));
    return document;
}

// ---------------------------------------------------------------------------
// Bytes
// ---------------------------------------------------------------------------

size_t cadPayloadAt(const std::vector<uint8_t>& bytes, uint16_t* outVersion = nullptr,
                    uint64_t* outPayloadBytes = nullptr) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            if (outVersion != nullptr) {
                *outVersion = static_cast<uint16_t>(bytes[offset + 4] | (bytes[offset + 5] << 8));
            }
            if (outPayloadBytes != nullptr) {
                *outPayloadBytes = payloadBytes;
            }
            return offset + kForgeSectionHeaderBytes;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
    return 0;
}

uint16_t cadVersionOf(const std::vector<uint8_t>& bytes) {
    uint16_t version = 0;
    return cadPayloadAt(bytes, &version) != 0 ? version : 0;
}

// Writes `size` bytes at `at` (a CADB payload offset) and repairs the section
// CRC, so only a semantic or structural check can refuse the result.
std::vector<uint8_t> patchedCad(std::vector<uint8_t> bytes, size_t payloadOffset, const void* value,
                                size_t size) {
    uint64_t payloadBytes = 0;
    const size_t base = cadPayloadAt(bytes, nullptr, &payloadBytes);
    if (base == 0 || payloadOffset + size > payloadBytes) {
        return {};
    }
    std::memcpy(&bytes[base + payloadOffset], value, size);
    const uint32_t crc = crc32IsoHdlc(&bytes[base], static_cast<size_t>(payloadBytes));
    std::memcpy(&bytes[base - kForgeSectionHeaderBytes + 16], &crc, 4);
    return bytes;
}

uint32_t u32At(const std::vector<uint8_t>& b, size_t at) {
    uint32_t v = 0;
    if (at + 4 <= b.size()) std::memcpy(&v, &b[at], 4);
    return v;
}

double f64At(const std::vector<uint8_t>& b, size_t at) {
    double v = 0.0;
    if (at + 8 <= b.size()) std::memcpy(&v, &b[at], 8);
    return v;
}

ProjectCodecStatus decodeStatus(const std::vector<uint8_t>& bytes, ProjectDocument* out = nullptr) {
    ProjectDocument scratch;
    ProjectDocument& target = out != nullptr ? *out : scratch;
    if (bytes.empty()) {
        return ProjectCodecStatus::Truncated;
    }
    return decodeProject(bytes.data(), bytes.size(), &target);
}

// Refused with `want`, and nothing written to the destination.
bool refusedAs(const std::vector<uint8_t>& bytes, ProjectCodecStatus want) {
    ProjectDocument out;
    return !bytes.empty() && decodeStatus(bytes, &out) == want && !out.hasCad
           && out.cad.bodies.empty() && out.scene.bodies.empty();
}

std::string sha(const std::vector<uint8_t>& bytes) {
    return bytes.empty() ? std::string("unavailable") : projectFixtureSha256Hex(bytes);
}

// ---------------------------------------------------------------------------
// A. The model
// ---------------------------------------------------------------------------

void testModel(Checks& c) {
    {
        const CadBodyState fresh;
        c.check("CADV6_M01_a_new_body_has_one_root_sketch_id_1_and_both_high_water_marks_at_2",
                fresh.sketches.size() == 1u && fresh.sketches[0].sketchId == kBaseCadSketchId
                        && !fresh.sketches[0].hasFeatureSupport
                        && fresh.baseSketchId == kBaseCadSketchId && fresh.nextSketchId == 2u
                        && fresh.nextFeatureId == 2u && nextCadFeatureId(fresh) == 2u
                        && cadBodyStateLegacyRepresentable(fresh));
    }
    {
        const CadBodyState shared = sharedState();
        CadBodyMesh mesh;
        const CadStatus why = regenerateCadBody(shared, &mesh);
        c.check("CADV6_M02_two_features_extrude_ONE_sketch_block_plus_boss_volume_12_5",
                validateCadBodyState(shared) == CadStatus::Ok && why == CadStatus::Ok
                        && shared.sketches.size() == 1u && shared.laterFeatures.size() == 1u
                        && shared.laterFeatures[0].sketchId == shared.baseSketchId
                        && near(mesh.volume, 12.5, 1e-9) && mesh.components == 1u
                        && !cadBodyStateLegacyRepresentable(shared));
        CadFeatureView base;
        CadFeatureView boss;
        const bool views = findCadFeature(shared, 1u, &base) && findCadFeature(shared, 2u, &boss);
        c.check("CADV6_M03_both_features_read_the_SAME_record_and_carry_no_sketch_of_their_own",
                views && base.sketch == boss.sketch && base.sketch == &shared.sketches[0].sketch
                        && base.sketchId == boss.sketchId && boss.support == nullptr
                        && cadFeatureSketchRecord(shared, 1u) == cadFeatureSketchRecord(shared, 2u));
        // Editing the sketch record is what BOTH features read: the square
        // grows to 1.2 m, the block (ring + square) stays 4 x 3 x 1 and the
        // boss becomes 1.44 x 0.5.
        CadBodyState edited = shared;
        SketchRectangle bigger;
        bigger.width = 1.2;
        bigger.height = 1.2;
        const bool replaced =
                replaceSketchEntity(&findCadSketchRecord(edited, kBaseCadSketchId)->sketch, 2, bigger)
                == CadStatus::Ok;
        CadBodyMesh editedMesh;
        c.check("CADV6_M04_one_sketch_edit_changes_what_both_features_extrude",
                replaced && regenerateCadBody(edited, &editedMesh) == CadStatus::Ok
                        && near(editedMesh.volume, 12.0 + 1.44 * 0.5, 1e-9)
                        && !sameCadBodyState(edited, shared));
    }
    {
        // Ids are minted from the high-water marks, never from what is left:
        // feature 3 and its sketch 3 are deleted, and the next feature is
        // 4 on sketch 4 -- nothing is handed the dead ids.
        CadBodyState chain = blockState();
        const uint32_t second = appendCadLaterFeatureWithSketch(
                &chain, CadFeatureOperation::Add, capSupport(chain, 1),
                rectSketch(-0.5, -0.5, 0.5, 0.5), loopExtrude(1, {}, {}, 0.25));
        const uint32_t third = appendCadLaterFeatureWithSketch(
                &chain, CadFeatureOperation::Add, capSupport(chain, 1),
                rectSketch(0.5, 0.5, 0.5, 0.5), loopExtrude(1, {}, {}, 0.25));
        const bool built = second == 2u && third == 3u && chain.nextSketchId == 4u
                           && chain.nextFeatureId == 4u
                           && validateCadBodyState(chain) == CadStatus::Ok;
        chain.laterFeatures.pop_back();
        chain.sketches.pop_back();
        const bool afterDelete = validateCadBodyState(chain) == CadStatus::Ok
                                 && chain.nextSketchId == 4u && chain.nextFeatureId == 4u
                                 && !cadBodyStateLegacyRepresentable(chain);
        const uint32_t fourth = appendCadLaterFeatureWithSketch(
                &chain, CadFeatureOperation::Add, capSupport(chain, 1),
                rectSketch(0.5, -0.5, 0.5, 0.5), loopExtrude(1, {}, {}, 0.25));
        c.check("CADV6_M05_a_deleted_feature_and_sketch_never_hand_their_ids_to_the_next",
                built && afterDelete && fourth == 4u && chain.laterFeatures.back().sketchId == 4u
                        && chain.nextSketchId == 5u && chain.nextFeatureId == 5u
                        && validateCadBodyState(chain) == CadStatus::Ok);
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(chain));
        ProjectDocument back;
        c.check("CADV6_M06_non_contiguous_ids_persist_through_v6_exactly",
                cadVersionOf(bytes) == kCadSectionVersionV6
                        && decodeStatus(bytes, &back) == ProjectCodecStatus::Ok
                        && sameCadBodyState(back.cad.bodies[0].state, chain)
                        && encodeProjectV1(back) == bytes);
    }
    {
        CadBodyState low = sharedState();
        low.nextFeatureId = 2u;  // the Add is feature 2
        CadBodyState lowSketch = sharedState();
        lowSketch.nextSketchId = 1u;
        CadBodyState highFeature = blockState();
        highFeature.nextFeatureId = 9u;
        c.check("CADV6_M07_a_high_water_mark_that_could_mint_a_collision_is_refused",
                validateCadBodyState(low) == CadStatus::HighWaterInvalid
                        && validateCadBodyState(lowSketch) == CadStatus::HighWaterInvalid
                        && validateCadBodyState(highFeature) == CadStatus::Ok
                        && !cadBodyStateLegacyRepresentable(highFeature));
    }
    {
        // A retained sketch no feature extrudes: legal, validated, persisted.
        CadBodyState retained = blockState();
        CadSketch disk;
        circle(&disk, 0.0, 0.0, 0.3);
        const CadFeatureSupport onCap = capSupport(retained, 1);
        const CadSketchId id = addCadSketchRecord(&retained, disk, &onCap);
        CadBodyMesh mesh;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(retained));
        ProjectDocument back;
        c.check("CADV6_M08_an_unconsumed_retained_sketch_is_legal_and_round_trips_v6",
                id == 2u && validateCadBodyState(retained) == CadStatus::Ok
                        && regenerateCadBody(retained, &mesh) == CadStatus::Ok
                        && near(mesh.volume, 4.0, 1e-9) && cadVersionOf(bytes) == kCadSectionVersionV6
                        && decodeStatus(bytes, &back) == ProjectCodecStatus::Ok
                        && sameCadBodyState(back.cad.bodies[0].state, retained));
        CadBodyState stale = retained;
        stale.sketches[1].featureSupport.lineageToken ^= 1u;
        CadBodyState broken = retained;
        std::get<SketchCircle>(broken.sketches[1].sketch.entities[0].payload()).radius = -1.0;
        c.check("CADV6_M09_an_unconsumed_sketch_is_held_to_its_own_rule_and_its_support",
                validateCadBodyState(stale) == CadStatus::FeatureSupportInvalid
                        && validateCadBodyState(broken) == CadStatus::InvalidCircleRadius);
    }
    {
        CadBodyState zero = sharedState();
        zero.sketches[0].sketchId = kNoCadSketch;
        CadBodyState twin = mixedState();
        twin.sketches[1].sketchId = 1u;
        CadBodyState disorder = mixedState();
        std::swap(disorder.sketches[0], disorder.sketches[1]);
        CadBodyState missing = sharedState();
        missing.laterFeatures[0].sketchId = 7u;
        CadBodyState noBase = blockState();
        noBase.baseSketchId = 9u;
        CadBodyState secondRoot = mixedState();
        secondRoot.sketches[1].hasFeatureSupport = false;
        CadBodyState baseOnFace = mixedState();
        baseOnFace.baseSketchId = 2u;
        CadBodyState crowded = blockState();
        for (uint32_t i = 0; i < kMaxCadSketches; ++i) {
            CadSketch disk;
            circle(&disk, 0.0, 0.0, 0.1);
            CadSketchRecord record;
            record.sketchId = crowded.nextSketchId++;
            record.hasFeatureSupport = true;
            record.featureSupport = capSupport(crowded, 1);
            record.sketch = disk;
            crowded.sketches.push_back(record);
        }
        c.check("CADV6_M10_the_table_refuses_zero_duplicate_unordered_and_missing_ids_by_name",
                validateCadBodyState(zero) == CadStatus::SketchIdInvalid
                        && validateCadBodyState(twin) == CadStatus::DuplicateSketchId
                        && validateCadBodyState(disorder) == CadStatus::SketchIdInvalid
                        && validateCadBodyState(missing) == CadStatus::SketchNotFound
                        && validateCadBodyState(noBase) == CadStatus::SketchNotFound);
        c.check("CADV6_M11_one_root_sketch_the_base_s_and_a_bounded_table",
                validateCadBodyState(secondRoot) == CadStatus::SketchSupportInvalid
                        && validateCadBodyState(baseOnFace) == CadStatus::SketchSupportInvalid
                        && crowded.sketches.size() == kMaxCadSketches + 1u
                        && validateCadBodyState(crowded) == CadStatus::TooManySketches);
    }
    {
        CadBodyState unknown = blockState();
        unknown.extrude.selection = static_cast<CadSelectionKind>(7);
        CadBodyState loopsWithFaces = blockState();
        loopsWithFaces.extrude.planarFaces.push_back(derivedFace(lensSketch(), kLensArea));
        CadBodyState facesWithLoops = lensState();
        facesWithLoops.extrude.profileEntityId = 1u;
        c.check("CADV6_M12_the_selection_kind_is_explicit_and_carries_one_payload",
                validateCadBodyState(unknown) == CadStatus::InvalidSelectionKind
                        && validateCadBodyState(loopsWithFaces) == CadStatus::InvalidSelectionKind
                        && validateCadBodyState(facesWithLoops) == CadStatus::InvalidSelectionKind);
    }
    {
        const CadBodyState shared = sharedState();
        CadBodyState copied = shared;
        // The same authored sketch as a SECOND record on the base's own plane
        // cannot be expressed (one root), so the copy is compared as data:
        // equality and the fingerprint tell a shared sketch from a copy.
        CadBodyState withCopy = blockState();
        const CadFeatureSupport onCap = capSupport(withCopy, 1);
        appendCadLaterFeatureWithSketch(&withCopy, CadFeatureOperation::Add, onCap,
                                        rectSketch(0.0, 0.0, 0.5, 0.5), loopExtrude(1, {}, {}, 0.25));
        CadBodyState sharing = withCopy;
        sharing.laterFeatures.push_back(sharing.laterFeatures[0]);
        sharing.laterFeatures[1].featureId = sharing.nextFeatureId++;
        sharing.laterFeatures[1].operation = CadFeatureOperation::Cut;
        sharing.laterFeatures[1].extrude.direction = ExtrudeDirection::AgainstNormal;
        sharing.laterFeatures[1].extrude.depth = 0.5;
        CadBodyState twoCopies = withCopy;
        const CadSketchRecord& first = twoCopies.sketches[1];
        appendCadLaterFeatureWithSketch(&twoCopies, CadFeatureOperation::Cut, first.featureSupport,
                                        first.sketch,
                                        loopExtrude(1, {}, {}, 0.5, ExtrudeDirection::AgainstNormal));
        c.check("CADV6_M13_copy_and_equality_tell_a_shared_sketch_from_two_equal_ones",
                sameCadBodyState(copied, shared) && !sameCadBodyState(sharing, twoCopies)
                        && validateCadBodyState(sharing) == CadStatus::Ok
                        && validateCadBodyState(twoCopies) == CadStatus::Ok
                        && sharing.sketches.size() == 2u && twoCopies.sketches.size() == 3u
                        && sameCadSketch(twoCopies.sketches[1].sketch, twoCopies.sketches[2].sketch));
        ConstructionScene a((NoProjectTag()));
        ConstructionScene b((NoProjectTag()));
        SceneObject* oa = a.addCadBody(sharing);
        SceneObject* ob = b.addCadBody(twoCopies);
        if (oa != nullptr) publishSceneObject(*oa);
        if (ob != nullptr) publishSceneObject(*ob);
        c.check("CADV6_M14_the_fingerprint_moves_for_sharing_that_a_copy_would_hide",
                oa != nullptr && ob != nullptr
                        && projectSemanticFingerprint(a, ProjectKind::Construction)
                                   != projectSemanticFingerprint(b, ProjectKind::Construction));
    }
}

// ---------------------------------------------------------------------------
// B. Planar-face selections
// ---------------------------------------------------------------------------

void testPlanarFaces(Checks& c) {
    const CadBodyState lens = lensState();
    const CadBodyState protrusion = protrusionState();
    const CadBodyState twoCircles = twoCirclesState();
    const CadBodyState mixed = mixedState();
    const PlanarFaceRef lensRef = lens.extrude.planarFaces.empty() ? PlanarFaceRef{}
                                                                  : lens.extrude.planarFaces[0];
    c.check("CADV6_F01_the_lens_protrusion_and_two_circle_faces_resolve_exactly",
            lensRef.outer.size() == 2u && validateCadBodyState(lens) == CadStatus::Ok
                    && validateCadBodyState(protrusion) == CadStatus::Ok
                    && protrusion.extrude.planarFaces[0].outer.size() == 4u
                    && validateCadBodyState(twoCircles) == CadStatus::Ok
                    && validateCadBodyState(mixed) == CadStatus::Ok);
    {
        // Every face of the lens sketch is a legal selection, alone or all
        // three together in canonical order.
        const SketchArrangement arrangement = deriveSketchArrangement(lensSketch());
        std::vector<PlanarFaceRef> all;
        bool each = arrangement.faces.size() == 3u;
        for (const AtomicPlanarFace& face : arrangement.faces) {
            all.push_back(face.ref);
            each = each
                   && validateCadBodyState(makeCadBodyState(lensSketch(), faceExtrude({face.ref}, 1.0)))
                              == CadStatus::Ok;
        }
        c.check("CADV6_F02_every_derived_face_and_all_three_together_are_valid_selections",
                each
                        && validateCadBodyState(makeCadBodyState(lensSketch(), faceExtrude(all, 1.0)))
                                   == CadStatus::Ok);
    }
    {
        CadBody body(1);
        CadBodyMesh mesh;
        ConstructionScene scene((NoProjectTag()));
        CadStatus why = CadStatus::Ok;
        const SceneObject* added = scene.addCadBody(lens, &why);
        c.check("CADV6_F03_a_face_selection_is_validated_but_never_regenerated_in_S1",
                regenerateCadBody(lens, &mesh) == CadStatus::PlanarFaceRegenerationUnavailable
                        && regenerateCadBody(mixed, &mesh) == CadStatus::PlanarFaceRegenerationUnavailable
                        && body.applyState(lens) == CadStatus::PlanarFaceRegenerationUnavailable
                        && added == nullptr && why == CadStatus::PlanarFaceRegenerationUnavailable
                        && !scene.hasProject());
        CadBodyState onFaces = lens;
        CadFeatureSupport support;
        support.featureId = 1;
        support.face.kind = CadFaceKind::CapFar;
        appendCadLaterFeatureWithSketch(&onFaces, CadFeatureOperation::Add, support,
                                        rectSketch(0.0, 0.0, 0.1, 0.1), loopExtrude(1, {}, {}, 0.1));
        c.check("CADV6_F04_a_sketch_on_a_face_selection_s_faces_is_refused_by_name",
                validateCadBodyState(onFaces) == CadStatus::PlanarFaceRegenerationUnavailable);
    }
    {
        CadBodyState rotated = lens;
        std::rotate(rotated.extrude.planarFaces[0].outer.begin(),
                    rotated.extrude.planarFaces[0].outer.begin() + 1,
                    rotated.extrude.planarFaces[0].outer.end());
        CadBodyState repeated = lens;
        repeated.extrude.planarFaces[0].outer.push_back(repeated.extrude.planarFaces[0].outer[1]);
        const SketchArrangement arrangement = deriveSketchArrangement(lensSketch());
        CadBodyState backwards = makeCadBodyState(
                lensSketch(), faceExtrude({arrangement.faces[1].ref, arrangement.faces[0].ref}, 1.0));
        CadBodyState twice = makeCadBodyState(lensSketch(), faceExtrude({lensRef, lensRef}, 1.0));
        CadBodyState holesBackwards = lens;
        FragmentCycle a{FragmentRef{3u, 0u, {}, {ArrangementCutKind::SourceEnd, 0u, 0u, 0u}, false}};
        FragmentCycle b{FragmentRef{4u, 0u, {}, {ArrangementCutKind::SourceEnd, 0u, 0u, 0u}, false}};
        holesBackwards.extrude.planarFaces[0].holes = {b, a};
        c.check("CADV6_F05_a_non_canonical_rotation_order_or_repeat_is_refused_never_re_sorted",
                validateCadBodyState(rotated) == CadStatus::PlanarFaceRefNotCanonical
                        && validateCadBodyState(repeated) == CadStatus::PlanarFaceRefNotCanonical
                        && validateCadBodyState(backwards) == CadStatus::PlanarFaceRefNotCanonical
                        && validatePlanarFaceRefForm(holesBackwards.extrude.planarFaces[0])
                                   == CadStatus::PlanarFaceRefNotCanonical
                        && validateCadBodyState(twice) == CadStatus::DuplicatePlanarFace);
    }
    {
        const auto withFirst = [&lens](void (*mutate)(FragmentRef*)) {
            CadBodyState s = lens;
            mutate(&s.extrude.planarFaces[0].outer[0]);
            return validateCadBodyState(s);
        };
        CadBodyState empty = lens;
        empty.extrude.planarFaces[0].outer.clear();
        CadBodyState tooLong = lens;
        tooLong.extrude.planarFaces[0].outer.resize(kMaxPlanarFaceCycleFragments + 1u,
                                                    lensRef.outer[1]);
        CadBodyState tooManyHoles = lens;
        tooManyHoles.extrude.planarFaces[0].holes.resize(kMaxPlanarFaceHoles + 1u);
        CadBodyState none = lens;
        none.extrude.planarFaces.clear();
        CadBodyState many = lens;
        many.extrude.planarFaces.assign(kMaxPlanarFaceSelection + 1u, lensRef);
        c.check("CADV6_F06_a_malformed_ref_is_refused_by_name",
                withFirst([](FragmentRef* f) { f->startCut.kind = ArrangementCutKind::SourceEnd; })
                                == CadStatus::PlanarFaceRefMalformed
                        && withFirst([](FragmentRef* f) { f->endCut.kind = ArrangementCutKind::SourceStart; })
                                   == CadStatus::PlanarFaceRefMalformed
                        && withFirst([](FragmentRef* f) {
                               f->startCut = ArrangementCut{ArrangementCutKind::SourceStart, 2u, 0u, 0u};
                           }) == CadStatus::PlanarFaceRefMalformed
                        && withFirst([](FragmentRef* f) { f->startCut.partnerEntityId = 0u; })
                                   == CadStatus::PlanarFaceRefMalformed
                        && withFirst([](FragmentRef* f) {
                               f->startCut.kind = static_cast<ArrangementCutKind>(9);
                           }) == CadStatus::PlanarFaceRefMalformed
                        && withFirst([](FragmentRef* f) { f->sourceEntityId = 0u; })
                                   == CadStatus::PlanarFaceRefMalformed
                        && validateCadBodyState(empty) == CadStatus::PlanarFaceRefMalformed
                        && validateCadBodyState(tooLong) == CadStatus::PlanarFaceRefMalformed
                        && validateCadBodyState(tooManyHoles) == CadStatus::PlanarFaceRefMalformed);
        c.check("CADV6_F07_zero_faces_is_nothing_chosen_and_seventeen_is_too_many",
                validateCadBodyState(none) == CadStatus::ProfileNotFound
                        && validateCadBodyState(many) == CadStatus::TooManyRegions);
    }
    {
        CadBodyState moved = lens;
        moved.extrude.planarFaces[0].outer[1].endCut.ordinal = 2u;
        CadBodyState outside = lens;
        // The half-disk OUTSIDE the rectangle is a real face; walked the lens's
        // way round it is not, and nothing nearby is substituted.
        outside.extrude.planarFaces[0].outer[0].reversed = true;
        CadBodyState pulledClear = lens;
        replaceSketchEntity(&cadBaseSketch(pulledClear), 2,
                            SketchCircle{SketchPoint{3.0, 0.0}, 0.5});
        c.check("CADV6_F08_a_ref_the_arrangement_does_not_derive_is_unresolved_no_fallback",
                validateCadBodyState(moved) == CadStatus::PlanarFaceUnresolved
                        && validateCadBodyState(outside) == CadStatus::PlanarFaceUnresolved
                        && validateCadBodyState(pulledClear) == CadStatus::PlanarFaceUnresolved);
        CadBodyState stretched = lens;
        replaceSketchEntity(&cadBaseSketch(stretched), 2, SketchCircle{SketchPoint{2.1, 0.3}, 0.6});
        c.check("CADV6_F09_an_edit_that_keeps_the_crossings_keeps_the_face",
                validateCadBodyState(stretched) == CadStatus::Ok);
    }
    {
        CadBodyState spline = lens;
        SketchSpline curve;
        curve.points = {SketchPoint{-1.0, -1.0}, SketchPoint{0.0, -0.5}, SketchPoint{1.0, -1.0}};
        add(&cadBaseSketch(spline), curve);
        CadSketch overlapSketch;
        rect(&overlapSketch, 0.0, 0.0, 4.0, 3.0);
        line(&overlapSketch, -1.0, -1.5, 1.0, -1.5);
        FragmentCycle whole;
        for (uint32_t k = 0; k < 4; ++k) {
            whole.push_back(FragmentRef{1u, k, {}, {ArrangementCutKind::SourceEnd, 0u, 0u, 0u}, false});
        }
        const CadBodyState overlap =
                makeCadBodyState(overlapSketch, faceExtrude({PlanarFaceRef{whole, {}}}, 1.0));
        CadSketch grid;
        for (int i = 0; i < 65; ++i) {
            const double t = -1.0 + 2.0 * i / 64.0;
            line(&grid, t, -1.5, t, 1.5);
        }
        for (int i = 0; i < 65; ++i) {
            const double t = -1.0 + 2.0 * i / 64.0;
            line(&grid, -1.5, t, 1.5, t);
        }
        FragmentCycle oneLine{FragmentRef{1u, 0u, {}, {ArrangementCutKind::SourceEnd, 0u, 0u, 0u}, false}};
        const CadBodyState capped = makeCadBodyState(grid, faceExtrude({PlanarFaceRef{oneLine, {}}}, 1.0));
        c.check("CADV6_F10_a_spline_an_overlap_and_the_arrangement_cap_are_refused_by_name",
                validateCadBodyState(spline) == CadStatus::PlanarFaceUnsupportedCurve
                        && validateCadBodyState(overlap) == CadStatus::PlanarFaceAmbiguousOverlap
                        && validateCadBodyState(capped) == CadStatus::PlanarFaceCapExceeded);
        c.check("CADV6_F11_the_three_arrangement_refusals_are_deterministic",
                validateCadBodyState(spline) == validateCadBodyState(spline)
                        && validateCadBodyState(overlap) == validateCadBodyState(overlap)
                        && validateCadBodyState(capped) == validateCadBodyState(capped));
    }
}

// ---------------------------------------------------------------------------
// C. Persistence
// ---------------------------------------------------------------------------

void testPersistence(Checks& c, std::string* digests) {
    // --- legacy migration and byte parity --------------------------------
    CadBodyState twins = blockState();
    const CadSketch square = rectSketch(0.5, 0.5, 0.5, 0.5);
    appendCadLaterFeatureWithSketch(&twins, CadFeatureOperation::Add, capSupport(twins, 1), square,
                                    loopExtrude(1, {}, {}, 0.25));
    appendCadLaterFeatureWithSketch(&twins, CadFeatureOperation::Cut, capSupport(twins, 1), square,
                                    loopExtrude(1, {}, {}, 0.25, ExtrudeDirection::AgainstNormal));
    const std::vector<uint8_t> twinBytes = encodeProjectV1(documentFor(twins));
    ProjectDocument twinBack;
    ProjectDocument twinAgain;
    const bool twinDecoded = decodeStatus(twinBytes, &twinBack) == ProjectCodecStatus::Ok
                             && decodeStatus(twinBytes, &twinAgain) == ProjectCodecStatus::Ok;
    const CadBodyState& migrated = twinBack.cad.bodies.empty() ? twins : twinBack.cad.bodies[0].state;
    c.check("CADV6_P01_a_legacy_shaped_chain_still_writes_v5",
            validateCadBodyState(twins) == CadStatus::Ok && cadBodyStateLegacyRepresentable(twins)
                    && cadVersionOf(twinBytes) == kCadSectionVersionV5);
    c.check("CADV6_P02_v5_reads_into_the_table_one_sketch_per_feature_ids_in_chain_order",
            twinDecoded && migrated.sketches.size() == 3u && migrated.sketches[0].sketchId == 1u
                    && migrated.sketches[1].sketchId == 2u && migrated.sketches[2].sketchId == 3u
                    && migrated.baseSketchId == 1u && migrated.laterFeatures[0].sketchId == 2u
                    && migrated.laterFeatures[1].sketchId == 3u && migrated.nextSketchId == 4u
                    && migrated.nextFeatureId == 4u
                    && migrated.extrude.selection == CadSelectionKind::LoopRegions
                    && migrated.laterFeatures[1].extrude.selection == CadSelectionKind::LoopRegions);
    c.check("CADV6_P03_two_byte_identical_legacy_sketches_stay_two_sketches",
            twinDecoded && sameCadSketch(migrated.sketches[1].sketch, migrated.sketches[2].sketch)
                    && migrated.sketches[1].sketchId != migrated.sketches[2].sketchId);
    c.check("CADV6_P04_the_migration_is_deterministic_and_rewrites_the_same_legacy_bytes",
            twinDecoded && sameProjectDocument(twinBack, twinAgain)
                    && encodeProjectV1(twinBack) == twinBytes && cadVersionOf(encodeProjectV1(twinBack)) == 5u);
    {
        const std::vector<uint8_t> block = encodeProjectV1(documentFor(blockState()));
        CadBodyState bumped = blockState();
        bumped.nextFeatureId = 7u;
        CadBodyState reused = twins;
        reused.laterFeatures[1].sketchId = 2u;
        reused.sketches.pop_back();
        c.check("CADV6_P05_v6_is_written_only_when_v1_to_v5_cannot_say_the_state",
                cadVersionOf(block) == kCadSectionVersion
                        && cadVersionOf(encodeProjectV1(documentFor(bumped))) == kCadSectionVersionV6
                        && validateCadBodyState(reused) == CadStatus::Ok
                        && cadVersionOf(encodeProjectV1(documentFor(reused))) == kCadSectionVersionV6
                        && cadVersionOf(encodeProjectV1(documentFor(lensState()))) == kCadSectionVersionV6);
    }

    // --- the v6 corpus ------------------------------------------------------
    const CadBodyState positives[5] = {sharedState(), lensState(), protrusionState(),
                                       twoCirclesState(), mixedState()};
    const char* const positiveNames[5] = {"sketch_shared", "face_lens", "face_protrusion",
                                          "face_two_circles", "mixed_selection"};
    std::vector<uint8_t> bytes[5];
    bool writes[5] = {};
    bool roundTrips[5] = {};
    for (int i = 0; i < 5; ++i) {
        const ProjectDocument document = documentFor(positives[i]);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        bytes[i] = encodeProjectV1(document, &why);
        writes[i] = why == ProjectCodecStatus::Ok && cadVersionOf(bytes[i]) == kCadSectionVersionV6;
        ProjectDocument back;
        roundTrips[i] = decodeStatus(bytes[i], &back) == ProjectCodecStatus::Ok
                        && back.cad.bodies.size() == 1u
                        && sameCadBodyState(back.cad.bodies[0].state, positives[i])
                        && sameProjectDocument(back, document) && encodeProjectV1(back) == bytes[i];
        *digests += std::string(i == 0 ? "" : " ") + positiveNames[i] + "=" + sha(bytes[i]);
    }
    c.check("CADV6_P06_each_positive_fixture_state_writes_cadb_v6",
            writes[0] && writes[1] && writes[2] && writes[3] && writes[4]);
    c.check("CADV6_P07_each_positive_fixture_decodes_to_its_state_and_re_encodes_byte_identical",
            roundTrips[0] && roundTrips[1] && roundTrips[2] && roundTrips[3] && roundTrips[4]);

    // The v6 record at the offsets DATA_PACKAGE_SPEC.md §7g fixes, on the
    // shared-sketch fixture: one sketch, two features naming it.
    uint64_t sharedPayload = 0;
    const size_t at = cadPayloadAt(bytes[0], nullptr, &sharedPayload);
    const std::vector<uint8_t>& b = bytes[0];
    const bool layout = at != 0 && sharedPayload == 208u && u32At(b, at) == 1u
                        && u32At(b, at + 12) == 2u && u32At(b, at + 16) == 3u
                        && u32At(b, at + 20) == 1u && u32At(b, at + 24) == 1u && b[at + 28] == 1u
                        && b[at + 29] == 1u && u32At(b, at + 30) == 3u && u32At(b, at + 34) == 2u
                        && u32At(b, at + 112) == 2u && u32At(b, at + 116) == 1u && b[at + 120] == 1u
                        && u32At(b, at + 121) == 1u && f64At(b, at + 127) == 1.0
                        && b[at + 143] == 1u && u32At(b, at + 144) == 1u && u32At(b, at + 148) == 1u
                        && u32At(b, at + 152) == 2u && u32At(b, at + 160) == 2u
                        && u32At(b, at + 168) == 2u && b[at + 172] == 2u && u32At(b, at + 173) == 1u
                        && b[at + 178] == 2u && f64At(b, at + 179) == 0.5 && b[at + 195] == 1u
                        && u32At(b, at + 196) == 2u;
    c.check("CADV6_P08_the_v6_record_sits_at_the_specified_offsets", layout);

    // The refusals, CONSTRUCTED from states with the bad value in place --
    // through the same writer without its validation -- exactly as the
    // independent builder constructs them.
    CadBodyState badRef = sharedState();
    badRef.laterFeatures[0].sketchId = 7u;
    CadBodyState duplicate = mixedState();
    duplicate.sketches[1].sketchId = 1u;
    duplicate.laterFeatures[0].sketchId = 1u;
    CadBodyState noncanonical = lensState();
    std::rotate(noncanonical.extrude.planarFaces[0].outer.begin(),
                noncanonical.extrude.planarFaces[0].outer.begin() + 1,
                noncanonical.extrude.planarFaces[0].outer.end());
    CadBodyState unresolved = lensState();
    unresolved.extrude.planarFaces[0].outer[1].endCut.ordinal = 2u;
    CadBodyState spline = lensState();
    SketchSpline curve;
    curve.points = {SketchPoint{-1.0, -1.0}, SketchPoint{0.0, -0.5}, SketchPoint{1.0, -1.0}};
    add(&cadBaseSketch(spline), curve);
    CadSketch overlapSketch;
    rect(&overlapSketch, 0.0, 0.0, 4.0, 3.0);
    line(&overlapSketch, -1.0, -1.5, 1.0, -1.5);
    FragmentCycle whole;
    for (uint32_t k = 0; k < 4; ++k) {
        whole.push_back(FragmentRef{1u, k, {}, {ArrangementCutKind::SourceEnd, 0u, 0u, 0u}, false});
    }
    const CadBodyState overlap =
            makeCadBodyState(overlapSketch, faceExtrude({PlanarFaceRef{whole, {}}}, 1.0));
    // The selection-kind byte of the lens fixture: 24 bytes of body header,
    // the 14-byte sketch header, a rectangle (37) and a circle (29), the
    // feature count, and the feature's 27 fixed bytes.
    const uint8_t badKind = 9;
    const std::vector<uint8_t> badSelection = patchedCad(bytes[1], 135, &badKind, 1);
    const CadBodyState* const negativeStates[7] = {&badRef, &duplicate, nullptr, &noncanonical,
                                                   &unresolved, &spline, &overlap};
    const char* const negativeNames[7] = {"bad_sketch_ref", "duplicate_sketch_id",
                                          "bad_selection_kind", "noncanonical_face",
                                          "unresolved_face", "spline_face", "overlap_face"};
    const CadStatus domainWhy[7] = {CadStatus::SketchNotFound,
                                    CadStatus::DuplicateSketchId,
                                    CadStatus::Ok,
                                    CadStatus::PlanarFaceRefNotCanonical,
                                    CadStatus::PlanarFaceUnresolved,
                                    CadStatus::PlanarFaceUnsupportedCurve,
                                    CadStatus::PlanarFaceAmbiguousOverlap};
    std::vector<uint8_t> negative[7];
    bool named = bytes[1].size() > 135u && bytes[1][cadPayloadAt(bytes[1]) + 135] == 2u;
    bool refused = true;
    for (int i = 0; i < 7; ++i) {
        if (negativeStates[i] != nullptr) {
            named = named && validateCadBodyState(*negativeStates[i]) == domainWhy[i];
            // The checked writer refuses to produce a file for any of them.
            ProjectCodecStatus why = ProjectCodecStatus::Ok;
            named = named && encodeProjectV1(documentFor(*negativeStates[i]), &why).empty()
                    && why == ProjectCodecStatus::InvalidSemanticValue;
            negative[i] = encodeProjectV1Unchecked(documentFor(*negativeStates[i]));
        } else {
            negative[i] = badSelection;
        }
        refused = refused && cadVersionOf(negative[i]) == kCadSectionVersionV6
                  && refusedAs(negative[i], ProjectCodecStatus::InvalidSemanticValue);
        *digests += std::string(" ") + negativeNames[i] + "=" + sha(negative[i]);
    }
    c.check("CADV6_P09_each_refusal_state_is_refused_by_its_domain_name_and_never_written",
            named);
    c.check("CADV6_P10_each_refusal_fixture_decodes_to_InvalidSemanticValue_writing_nothing",
            refused);

    // The corpus `scripts/build-forge-corpus.ps1` CONSTRUCTS from
    // DATA_PACKAGE_SPEC.md §7g, sharing no line with this codec.
    const char* const committed[12] = {
            "9dedb935d190c8831e80c31798a21fc6ebb7f7325564e4e7d30c3f0fd80a8342",  // cad_sketch_shared_v6
            "928f42163c3983f97c79cf408b5f00a5a4b990cc20e4001a2c8073bf43458e12",  // cad_face_lens_v6
            "f5602527a1bf228746016334ad030d02904c60d5c6fafe775a8d66ccdf087528",  // cad_face_protrusion_v6
            "41675dd1d5edbf40886c5cb73ecfab00801453ba5ba77d84f2d900123315c081",  // cad_face_two_circles_v6
            "4406c9f85a89481ace78a14591134dcfa8373a4b30b43908cf2fec608e3d27a5",  // cad_mixed_selection_v6
            "cde2cab0a6230a6bb5f479874c4c6db0c36321ea3a17cbe77823651d32df66e7",  // cad_bad_sketch_ref_v6
            "ead5488fc5cccdbe89f8d29d3e9c79cc0467de8447f7cf9930179b9bb77e6a1d",  // cad_duplicate_sketch_id_v6
            "cf3b0c64e066b6b809a5744e4b9d32d46864ae7d690b89c4bc5abd6785517e27",  // cad_bad_selection_kind_v6
            "2aed2f15bb5437286906a3352af39d3d8dd29c40b876a18d65d14b0aac8ca2a3",  // cad_noncanonical_face_v6
            "9602fc4a281e4d6b7bf79fedf766d76ad2e2e2d142e9917d5b44635c65d05b4b",  // cad_unresolved_face_v6
            "d85f98db59968bbe6c47f1842f59bb2f93f167f61deba67f5f846e98651cd800",  // cad_spline_face_v6
            "313652c95c14d3ebfd5e451447b8b46d0890fbed35b7c680911fb94426e118f9",  // cad_overlap_face_v6
    };
    bool corpus = true;
    for (int i = 0; i < 5; ++i) {
        corpus = corpus && sha(bytes[i]) == committed[i];
    }
    for (int i = 0; i < 7; ++i) {
        corpus = corpus && sha(negative[i]) == committed[5 + i];
    }
    c.check("CADV6_P11_every_v6_fixture_matches_the_independent_corpus_digest", corpus);

    // --- structural refusals the codec makes before the domain sees anything
    {
        const uint8_t zero = 0;
        const uint8_t two = 2;
        const uint32_t zero32 = 0;
        const uint32_t second = 2;
        const uint8_t nine = 9;
        // Lens offsets: sketchCount @20, placement @28, featureCount @104,
        // featureId @108, selection kind @135, faceCount @136, fragmentCount
        // @140, the first fragment's two ids @144..151, its start cut's kind
        // @152, and -- two 13-byte intersection cuts later -- its reversed
        // byte @178.
        const size_t firstCutKind = 136 + 4 + 4 + 4 + 4;
        const size_t reversedByte = firstCutKind + 13 + 13;
        c.check("CADV6_P12_codes_and_counts_are_refused_before_the_domain_is_asked",
                refusedAs(patchedCad(bytes[1], 135, &zero, 1), ProjectCodecStatus::InvalidSemanticValue)
                        && refusedAs(patchedCad(bytes[1], 28, &nine, 1),
                                     ProjectCodecStatus::InvalidSemanticValue)
                        && refusedAs(patchedCad(bytes[1], firstCutKind, &zero, 1),
                                     ProjectCodecStatus::InvalidSemanticValue)
                        && refusedAs(patchedCad(bytes[1], reversedByte, &two, 1),
                                     ProjectCodecStatus::BadPayload)
                        && refusedAs(patchedCad(bytes[1], 136, &zero32, 4),
                                     ProjectCodecStatus::ImpossibleCount)
                        && refusedAs(patchedCad(bytes[1], 140, &zero32, 4),
                                     ProjectCodecStatus::ImpossibleCount)
                        && refusedAs(patchedCad(bytes[1], 20, &zero32, 4),
                                     ProjectCodecStatus::ImpossibleCount)
                        && refusedAs(patchedCad(bytes[1], 104, &zero32, 4),
                                     ProjectCodecStatus::ImpossibleCount)
                        && refusedAs(patchedCad(bytes[1], 108, &second, 4),
                                     ProjectCodecStatus::InvalidSemanticValue)
                        && bytes[1].size() > reversedByte
                        && bytes[1][cadPayloadAt(bytes[1]) + reversedByte] == 0u);
    }

    // --- placement 2: a body standing on another body's face, in v6 --------
    {
        // A producer block and a dependent on its far cap whose Add re-extrudes
        // the dependent's OWN root sketch: v6 (a shared sketch), carrying the
        // TopoRef as placement 2.
        ConstructionScene scene((NoProjectTag()));
        SceneObject* producer = scene.addCadBody(blockState());
        CadBodyState dependent = makeCadBodyState(rectSketch(0.0, 0.0, 0.5, 0.5),
                                                  loopExtrude(1, {}, {}, 0.5));
        CadSketch& root = cadBaseSketch(dependent);
        root.hasFaceSupport = true;
        root.faceSupport.producerObjectId = producer != nullptr ? producer->objectId() : kNoObject;
        root.faceSupport.producerLocalFeatureId = kCadFeatureId;
        root.faceSupport.face.kind = CadFaceKind::CapFar;
        root.faceSupport.lineageToken = cadFeatureTopologySignature(blockState(), kCadFeatureId);
        appendCadLaterFeature(&dependent, CadFeatureOperation::Add, kBaseCadSketchId,
                              loopExtrude(1, {}, {}, 0.25, ExtrudeDirection::AgainstNormal));
        SceneObject* supported = producer != nullptr ? scene.addCadBody(dependent) : nullptr;
        if (producer != nullptr) publishSceneObject(*producer);
        if (supported != nullptr) publishSceneObject(*supported);
        const ProjectDocument document = captureProjectDocument(scene, ProjectKind::Construction);
        const std::vector<uint8_t> faceBytes = encodeProjectV1(document);
        ProjectDocument back;
        c.check("CADV6_P15_a_face_supported_body_round_trips_its_topo_ref_as_placement_2",
                supported != nullptr && cadVersionOf(faceBytes) == kCadSectionVersionV6
                        && decodeStatus(faceBytes, &back) == ProjectCodecStatus::Ok
                        && back.cad.bodies.size() == 2u
                        && sameCadBodyState(back.cad.bodies[1].state, dependent)
                        && cadBaseSketch(back.cad.bodies[1].state).hasFaceSupport
                        && encodeProjectV1(back) == faceBytes);
    }

    // --- the runtime: all-or-nothing load ----------------------------------
    {
        ConstructionScene scene((NoProjectTag()));
        ConstructionHistory history(scene);
        SculptSession session;
        SceneObject* existing = scene.addCadBody(blockState());
        if (existing != nullptr) publishSceneObject(*existing);
        const uint64_t before = projectSemanticFingerprint(scene, ProjectKind::Construction);
        ProjectDocument lensDocument;
        ProjectDocument mixedDocument;
        const bool decoded = decodeStatus(bytes[1], &lensDocument) == ProjectCodecStatus::Ok
                             && decodeStatus(bytes[4], &mixedDocument) == ProjectCodecStatus::Ok;
        const ProjectCodecStatus lensLoad = loadProjectDocument(lensDocument, scene, session, history);
        const ProjectCodecStatus mixedLoad = loadProjectDocument(mixedDocument, scene, session, history);
        c.check("CADV6_P13_a_face_selection_project_is_refused_on_load_and_changes_nothing",
                existing != nullptr && decoded && !runtimeCanEvaluateProject(lensDocument)
                        && !runtimeCanEvaluateProject(mixedDocument)
                        && lensLoad == ProjectCodecStatus::MissingRequiredSection
                        && mixedLoad == ProjectCodecStatus::MissingRequiredSection
                        && scene.bodyCount() == 1u && scene.bodyAt(0).isCad()
                        && sameCadBodyState(scene.bodyAt(0).cadOrNull()->state(), blockState())
                        && projectSemanticFingerprint(scene, ProjectKind::Construction) == before
                        && history.undoDepth() == 0u);
        ProjectDocument sharedDocument;
        const bool sharedDecoded = decodeStatus(bytes[0], &sharedDocument) == ProjectCodecStatus::Ok;
        const ProjectCodecStatus sharedLoad = loadProjectDocument(sharedDocument, scene, session, history);
        const std::vector<uint8_t> recaptured =
                encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction));
        c.check("CADV6_P14_the_shared_sketch_project_loads_and_re_captures_byte_identical",
                sharedDecoded && runtimeCanEvaluateProject(sharedDocument)
                        && sharedLoad == ProjectCodecStatus::Ok && scene.bodyCount() == 1u
                        && scene.bodyAt(0).isCad()
                        && sameCadBodyState(scene.bodyAt(0).cadOrNull()->state(), sharedState())
                        && recaptured == bytes[0] && history.undoDepth() == 0u);
    }
}

}  // namespace

void runCadV6SelfTests(std::vector<CadV6SelfTestCheck>* out, std::string* digests) {
    if (out == nullptr) {
        return;
    }
    Checks c{out};
    std::string local;
    testModel(c);
    testPlanarFaces(c);
    testPersistence(c, digests != nullptr ? digests : &local);
}

}  // namespace forgeshape
