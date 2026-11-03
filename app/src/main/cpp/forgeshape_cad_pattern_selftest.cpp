#include "forgeshape_cad_pattern_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_pattern.h"
#include "forgeshape_cad_timeline.h"
#include "forgeshape_history.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool passed) { out->push_back(ArrangementSelfTestCheck{name, passed}); }
};

bool near(double a, double b, double tolerance = 1e-9) { return std::fabs(a - b) <= tolerance; }

ExtrudeFeature oneSide(double depth, ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = 1;
    extrude.depth = depth;
    extrude.direction = direction;
    return extrude;
}

CadSketch rectSketch(double cu, double cv, double w, double h) {
    CadSketch sketch;
    SketchRectangle r;
    r.center = SketchPoint{cu, cv};
    r.width = w;
    r.height = h;
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(&sketch, r, &id);
    return sketch;
}

CadSketch circleSketch(double cu, double cv, double radius) {
    CadSketch sketch;
    SketchCircle c;
    c.center = SketchPoint{cu, cv};
    c.radius = radius;
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(&sketch, c, &id);
    return sketch;
}

// The 32-gon a circle is extruded as.
double diskArea(double r) { return 0.5 * r * r * 32.0 * std::sin(2.0 * kPi / 32.0); }

// A 4 x 4 x 1 block on XY: x, y in [-2, 2], z in [0, 1].
CadBodyState blockState() {
    CadBodyState state;
    cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
    state.extrude = oneSide(1.0);
    return state;
}

CadFeatureSupport topCap(const CadBodyState& state) {
    CadFeatureSupport support;
    support.featureId = kCadFeatureId;
    support.face.kind = CadFaceKind::CapFar;
    support.lineageToken = cadFeatureTopologySignature(state, kCadFeatureId);
    return support;
}

// The block with a round blind pocket (r 0.3, 0.5 deep) at (u, v) on its top.
CadBodyState pocketState(double u, double v) {
    CadBodyState state = blockState();
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut, topCap(state),
                                    circleSketch(u, v, 0.3),
                                    oneSide(0.5, ExtrudeDirection::AgainstNormal));
    return state;
}

// The block with a 0.4 x 0.4 x 0.5 boss at (u, v) on its top.
CadBodyState bossState(double u, double v) {
    CadBodyState state = blockState();
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Add, topCap(state),
                                    rectSketch(u, v, 0.4, 0.4), oneSide(0.5));
    return state;
}

CadFeaturePattern linear(uint32_t featureId, CadBodyAxis axis, uint32_t count, double spacing) {
    CadFeaturePattern p;
    p.kind = CadPatternKind::Linear;
    p.featureIds = {featureId};
    p.axis = axis;
    p.count = count;
    p.spacing = spacing;
    return p;
}

CadFeaturePattern circular(uint32_t featureId, CadBodyAxis axis, uint32_t count, double angle) {
    CadFeaturePattern p;
    p.kind = CadPatternKind::Circular;
    p.featureIds = {featureId};
    p.axis = axis;
    p.count = count;
    p.angleDegrees = angle;
    return p;
}

CadFeaturePattern mirror(uint32_t featureId, CadBodyAxis axis, double offset) {
    CadFeaturePattern p;
    p.kind = CadPatternKind::Mirror;
    p.featureIds = {featureId};
    p.axis = axis;
    p.count = 2;
    p.offset = offset;
    return p;
}

CadBodyState withPattern(CadBodyState state, CadFeaturePattern pattern) {
    appendCadFeaturePattern(&state, std::move(pattern));
    return state;
}

struct Regen {
    CadStatus why = CadStatus::RegenerationFailed;
    uint32_t failed = 0;
    CadBodyMesh mesh;
};

Regen regen(const CadBodyState& state) {
    Regen g;
    CadRegenerationReport report;
    g.why = validateCadBodyState(state);
    if (g.why == CadStatus::Ok) {
        g.why = regenerateCadBody(state, &g.mesh, &report);
        g.failed = report.failedFeatureId;
    }
    return g;
}

bool volumeIs(const Regen& g, double volume) {
    return g.why == CadStatus::Ok && near(g.mesh.volume, volume, 1e-7);
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

size_t cadPayloadAt(const std::vector<uint8_t>& bytes, uint16_t* version, uint64_t* length) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            *version = static_cast<uint16_t>(bytes[offset + 4] | (bytes[offset + 5] << 8));
            *length = payloadBytes;
            return offset + kForgeSectionHeaderBytes;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
    return 0;
}

bool rotationIs(const CadPatternTransform& t, double degrees, int axis) {
    const double c = std::cos(degrees * kPi / 180.0);
    const double s = std::sin(degrees * kPi / 180.0);
    const int a1 = (axis + 1) % 3;
    const int a2 = (axis + 2) % 3;
    return near(t.r[axis * 3 + axis], 1.0) && near(t.r[a1 * 3 + a1], c)
           && near(t.r[a1 * 3 + a2], -s) && near(t.r[a2 * 3 + a1], s)
           && near(t.r[a2 * 3 + a2], c) && !t.reflects;
}

}  // namespace

void runCadPatternSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    if (out == nullptr) return;
    Checks r{out};
    const double hole = diskArea(0.3) * 0.5;
    const double boss = 0.4 * 0.4 * 0.5;

    // --- PT_01..04: the instance transforms --------------------------------
    {
        CadFeaturePattern grid = linear(2, CadBodyAxis::X, 3, 0.5);
        grid.axis2 = CadBodyAxis::Y;
        grid.count2 = 2;
        grid.spacing2 = 0.25;
        const std::vector<CadPatternTransform> t = cadPatternInstanceTransforms(grid);
        bool ok = t.size() == 5u;
        // Row by row: (1,0) (2,0) (0,1) (1,1) (2,1).
        const double want[5][2] = {{0.5, 0.0}, {1.0, 0.0}, {0.0, 0.25}, {0.5, 0.25}, {1.0, 0.25}};
        for (size_t i = 0; ok && i < 5; ++i) {
            ok = near(t[i].t[0], want[i][0]) && near(t[i].t[1], want[i][1]) && near(t[i].t[2], 0.0)
                 && !t[i].reflects;
        }
        r.check("PT_01_a_linear_grid_is_every_offset_but_the_original", ok);
        CadFeaturePattern back = linear(2, CadBodyAxis::Z, 2, 0.5);
        back.reversed = true;
        const std::vector<CadPatternTransform> b = cadPatternInstanceTransforms(back);
        r.check("PT_01_reversed_walks_the_negative_axis",
                b.size() == 1u && near(b[0].t[2], -0.5) && near(b[0].t[0], 0.0));
    }
    {
        CadFeaturePattern full = circular(2, CadBodyAxis::Z, 4, 360.0);
        full.center[0] = 1.0;
        const std::vector<CadPatternTransform> t = cadPatternInstanceTransforms(full);
        bool ok = t.size() == 3u && rotationIs(t[0], 90.0, 2) && rotationIs(t[1], 180.0, 2)
                  && rotationIs(t[2], 270.0, 2);
        // About the axis through (1, 0, 0): that point stays where it is.
        for (size_t i = 0; ok && i < t.size(); ++i) {
            ok = near(t[i].r[0] * 1.0 + t[i].t[0], 1.0) && near(t[i].r[3] * 1.0 + t[i].t[1], 0.0);
        }
        r.check("PT_02_a_full_turn_spaces_instances_evenly_about_the_axis_point", ok);
        const std::vector<CadPatternTransform> half =
                cadPatternInstanceTransforms(circular(2, CadBodyAxis::X, 3, 180.0));
        r.check("PT_02_a_partial_sweep_spans_first_to_last",
                half.size() == 2u && rotationIs(half[0], 90.0, 0) && rotationIs(half[1], 180.0, 0));
    }
    {
        const std::vector<CadPatternTransform> t =
                cadPatternInstanceTransforms(mirror(2, CadBodyAxis::Y, 0.75));
        r.check("PT_03_a_mirror_reflects_across_the_offset_plane_and_says_so",
                t.size() == 1u && t[0].reflects && near(t[0].r[4], -1.0) && near(t[0].r[0], 1.0)
                        && near(t[0].t[1], 1.5));
        // A reflected solid still winds outward: the kernel accepts it.
        CadFeatureGeometry g;
        CadSolid solid;
        const CadBodyState state = pocketState(0.5, 0.5);
        bool ok = buildCadFeatureGeometry(state, 2u, &g) == CadStatus::Ok
                  && appendCadFeatureSolid(g, 0u, &solid) == CadStatus::Ok;
        const double before = cadSolidVolume(solid);
        transformCadSolid(t[0], &solid);
        CadSolidMeasure measure;
        r.check("PT_03_a_mirrored_tool_stays_a_valid_outward_solid",
                ok && cadKernelValidateSolid(solid, &measure) == CadKernelStatus::Ok
                        && near(measure.volume, before, 1e-9));
    }
    r.check("PT_04_an_invalid_pattern_has_no_instances",
            cadPatternInstanceTransforms(linear(2, CadBodyAxis::X, 1, 0.5)).empty()
                    && cadPatternInstanceTransforms(circular(2, CadBodyAxis::X, 3, 0.0)).empty());

    // --- PT_05..07: refusals by name, before the kernel ----------------------
    {
        const CadBodyState base = pocketState(0.0, 0.0);
        struct Bad {
            const char* name;
            CadFeaturePattern pattern;
            CadStatus why;
        };
        CadFeaturePattern angled = linear(2, CadBodyAxis::X, 2, 0.5);
        angled.angleDegrees = 10.0;
        CadFeaturePattern sameAxes = linear(2, CadBodyAxis::X, 2, 0.5);
        sameAxes.count2 = 2;
        sameAxes.spacing2 = 0.5;
        sameAxes.axis2 = CadBodyAxis::X;
        CadFeaturePattern tooMany = linear(2, CadBodyAxis::X, 9, 0.5);
        tooMany.axis2 = CadBodyAxis::Y;
        tooMany.count2 = 8;
        tooMany.spacing2 = 0.5;
        CadFeaturePattern mirror3 = mirror(2, CadBodyAxis::X, 0.0);
        mirror3.count = 3;
        CadFeaturePattern empty = linear(2, CadBodyAxis::X, 2, 0.5);
        empty.featureIds.clear();
        CadFeaturePattern twice = linear(2, CadBodyAxis::X, 2, 0.5);
        twice.featureIds = {2, 2};
        const Bad bad[] = {
                {"PT_05_one_instance_is_refused", linear(2, CadBodyAxis::X, 1, 0.5),
                 CadStatus::PatternInvalid},
                {"PT_05_a_zero_spacing_is_refused", linear(2, CadBodyAxis::X, 2, 0.0),
                 CadStatus::PatternInvalid},
                {"PT_05_a_nan_spacing_is_refused", linear(2, CadBodyAxis::X, 2, std::nan("")),
                 CadStatus::PatternInvalid},
                {"PT_05_a_value_another_kind_uses_is_refused", angled, CadStatus::PatternInvalid},
                {"PT_05_two_directions_along_one_axis_are_refused", sameAxes,
                 CadStatus::PatternInvalid},
                {"PT_05_more_than_64_instances_are_refused", tooMany, CadStatus::PatternInvalid},
                {"PT_05_a_zero_angle_is_refused", circular(2, CadBodyAxis::Z, 3, 0.0),
                 CadStatus::PatternInvalid},
                {"PT_05_more_than_a_full_turn_is_refused", circular(2, CadBodyAxis::Z, 3, 361.0),
                 CadStatus::PatternInvalid},
                {"PT_05_a_mirror_of_three_is_refused", mirror3, CadStatus::PatternInvalid},
                {"PT_06_no_feature_is_refused", empty, CadStatus::PatternInvalid},
                {"PT_06_a_repeated_feature_is_refused", twice, CadStatus::PatternInvalid},
                {"PT_06_the_base_feature_is_refused", linear(1, CadBodyAxis::X, 2, 0.5),
                 CadStatus::PatternTargetUnsupported},
                {"PT_06_an_unknown_feature_is_refused", linear(9, CadBodyAxis::X, 2, 0.5),
                 CadStatus::PatternFeatureNotFound},
        };
        for (const Bad& b : bad) {
            r.check(b.name, validateCadBodyState(withPattern(base, b.pattern)) == b.why);
        }
        // A pattern may only name a feature BEFORE it.
        CadBodyState later = withPattern(base, linear(2, CadBodyAxis::X, 2, 0.5));
        later.patterns[0].featureIds = {later.patterns[0].patternId + 1u};
        r.check("PT_06_a_feature_after_the_pattern_is_refused",
                validateCadBodyState(later) == CadStatus::PatternFeatureNotFound);
        CadBodyState collide = withPattern(base, linear(2, CadBodyAxis::X, 2, 0.5));
        collide.patterns[0].patternId = 2u;
        r.check("PT_06_an_id_a_feature_wears_is_refused",
                validateCadBodyState(collide) == CadStatus::PatternInvalid);
        CadBodyState full = base;
        for (uint32_t i = 0; i < kMaxCadFeaturePatterns; ++i) {
            appendCadFeaturePattern(&full, linear(2, CadBodyAxis::X, 2, 0.5));
        }
        r.check("PT_07_the_pattern_cap_holds",
                full.patterns.size() == kMaxCadFeaturePatterns
                        && appendCadFeaturePattern(&full, linear(2, CadBodyAxis::X, 2, 0.5)) == 0u);
    }

    // --- PT_08..12: exact geometry through the kernel ------------------------
    {
        const double block = 16.0;
        const Regen one = regen(pocketState(0.0, 0.0));
        r.check("PT_08_the_seed_pocket_removes_its_own_volume", volumeIs(one, block - hole));
        const Regen row = regen(withPattern(pocketState(0.0, 0.0), linear(2, CadBodyAxis::X, 3, 0.8)));
        r.check("PT_08_a_linear_row_of_three_removes_three_pockets", volumeIs(row, block - 3 * hole));
        CadFeaturePattern grid = linear(2, CadBodyAxis::X, 2, 1.0);
        grid.axis2 = CadBodyAxis::Y;
        grid.count2 = 2;
        grid.spacing2 = 1.0;
        const Regen g = regen(withPattern(pocketState(0.0, 0.0), grid));
        r.check("PT_08_a_two_by_two_grid_removes_four_pockets", volumeIs(g, block - 4 * hole));

        // Circular: a pocket one metre off the axis, four round the block's
        // own +Z axis through its centre.
        CadFace cap;
        CadFaceToken capToken;
        capToken.kind = CadFaceKind::CapFar;
        const bool centred = resolveCadFeatureFace(blockState(), kCadFeatureId, capToken, &cap)
                                     == CadStatus::Ok
                             && std::fabs(cap.origin.x) < 1e-6f && std::fabs(cap.origin.y) < 1e-6f;
        const Regen ring = regen(withPattern(pocketState(1.0, 0.0), circular(2, CadBodyAxis::Z, 4, 360.0)));
        r.check("PT_09_a_circular_pattern_of_four_removes_four_pockets",
                centred && volumeIs(ring, block - 4 * hole));
        const Regen onAxis = regen(withPattern(pocketState(0.0, 0.0), circular(2, CadBodyAxis::Z, 4, 360.0)));
        r.check("PT_09_instances_on_the_axis_cut_nothing_new_and_are_refused_by_name",
                onAxis.why == CadStatus::PatternInstanceNoEffect && onAxis.failed == 3u);

        const Regen mirrored = regen(withPattern(pocketState(1.0, 0.5), mirror(2, CadBodyAxis::X, 0.0)));
        r.check("PT_10_a_mirror_adds_one_reflected_pocket", volumeIs(mirrored, block - 2 * hole));
        const Regen bosses = regen(withPattern(bossState(-1.0, 1.0), linear(2, CadBodyAxis::X, 3, 0.8)));
        r.check("PT_10_a_row_of_bosses_adds_each_boss", volumeIs(bosses, block + 3 * boss));
        const Regen off = regen(withPattern(bossState(1.0, 1.0), linear(2, CadBodyAxis::X, 2, 6.0)));
        r.check("PT_11_a_boss_copy_standing_apart_from_the_body_is_refused",
                off.why == CadStatus::PatternInstanceNoEffect && off.failed == 3u);
        const Regen miss = regen(withPattern(pocketState(0.0, 0.0), linear(2, CadBodyAxis::Y, 2, 6.0)));
        r.check("PT_11_a_pocket_copy_that_misses_the_body_is_refused",
                miss.why == CadStatus::PatternInstanceNoEffect && miss.failed == 3u);

        // Instance faces are never supports, and are named by the pattern.
        bool tagged = false;
        bool ineligible = true;
        for (size_t t = 0; row.why == CadStatus::Ok && t < row.mesh.triangleFace.size(); ++t) {
            const CadMeshFace& face = row.mesh.faces[row.mesh.triangleFace[t]];
            if (face.featureId == 3u) {
                tagged = true;
                ineligible = ineligible && !face.eligible;
            }
        }
        r.check("PT_12_instance_triangles_wear_the_patterns_ineligible_entry", tagged && ineligible);
    }

    // --- PT_13: the chain order by id ----------------------------------------
    {
        // A pattern of the pocket, then a LATER pocket on the same spot as an
        // instance: the later Cut sees the instance and removes nothing new.
        CadBodyState state = withPattern(pocketState(0.0, 0.0), linear(2, CadBodyAxis::X, 2, 1.0));
        CadBodyState after = state;
        appendCadLaterFeatureWithSketch(&after, CadFeatureOperation::Cut, topCap(after),
                                        circleSketch(0.0, 0.0, 0.3),
                                        oneSide(0.5, ExtrudeDirection::AgainstNormal));
        const Regen g = regen(after);
        r.check("PT_13_a_feature_after_a_pattern_sees_its_instances",
                g.why == CadStatus::CutNoIntersection && g.failed == 4u);
    }

    // --- PT_14: CADB v10 -------------------------------------------------------
    {
        CadFeaturePattern ring = circular(2, CadBodyAxis::Z, 5, 270.0);
        ring.reversed = true;
        ring.center[0] = 0.25;
        const CadBodyState state = withPattern(pocketState(1.0, 0.0), ring);
        const CadBodyState plain = pocketState(1.0, 0.0);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state), &why);
        const std::vector<uint8_t> plainBytes = encodeProjectV1(documentFor(plain), &why);
        uint16_t version = 0, plainVersion = 0;
        uint64_t length = 0, plainLength = 0;
        const size_t base = cadPayloadAt(bytes, &version, &length);
        cadPayloadAt(plainBytes, &plainVersion, &plainLength);
        r.check("PT_14_a_patterned_body_is_written_cadb_v10",
                base != 0 && version == kCadSectionVersionV10);
        r.check("PT_14_a_body_without_patterns_keeps_its_version",
                plainVersion < kCadSectionVersionV9);
        ProjectDocument decoded;
        r.check("PT_14_v10_round_trips_the_pattern_exactly",
                decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                        && decoded.cad.bodies.size() == 1u
                        && sameCadBodyState(decoded.cad.bodies[0].state, state));
        // One pattern naming one feature is 81 bytes at the payload's end: the
        // kind byte is its fifth, the first boolean its fifteenth.
        auto refusedWith = [&](size_t fromEnd, uint8_t value) {
            if (base == 0 || length < fromEnd) return false;
            std::vector<uint8_t> bad = bytes;
            bad[base + static_cast<size_t>(length) - fromEnd] = value;
            const uint32_t crc = crc32IsoHdlc(&bad[base], static_cast<size_t>(length));
            std::memcpy(&bad[base - kForgeSectionHeaderBytes + 16], &crc, 4);
            ProjectDocument scratch;
            return decodeProject(bad.data(), bad.size(), &scratch)
                   == ProjectCodecStatus::InvalidSemanticValue;
        };
        r.check("PT_14_an_unknown_pattern_kind_is_refused_by_name", refusedWith(77u, 9u));
        r.check("PT_14_a_boolean_byte_above_one_is_refused", refusedWith(67u, 2u));
        CadBodyState stale = state;
        stale.patterns[0].featureIds = {7u};
        ProjectCodecStatus staleWhy = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> staleBytes = encodeProjectV1(documentFor(stale), &staleWhy);
        r.check("PT_14_a_document_naming_a_missing_feature_is_not_encoded",
                staleWhy != ProjectCodecStatus::Ok || staleBytes.empty());
    }

    // --- PT_15..17: the acts, the fingerprint and the History ---------------
    const auto t0 = std::chrono::steady_clock::now();
    {
        ConstructionScene scene{NoProjectTag{}};
        ConstructionHistory history{scene};
        SceneObject* added = scene.addCadBody(pocketState(0.0, 0.0));
        const ObjectId id = added != nullptr ? added->objectId() : kNoObject;
        if (added != nullptr) publishSceneObject(*added);
        auto volume = [&scene, id]() {
            std::shared_ptr<const CadBodyMesh> mesh;
            const CadBody* body = scene.findBody(id)->cadOrNull();
            return body->regenerated(&mesh) == CadStatus::Ok ? mesh->volume : -1.0;
        };
        const uint64_t before = projectSemanticFingerprint(scene, ProjectKind::Construction);
        uint32_t patternId = 0;
        const CadStatus addWhy = addCadFeaturePatternToBody(scene, history, id,
                                                            linear(2, CadBodyAxis::X, 3, 0.8),
                                                            &patternId);
        r.check("PT_15_add_is_one_undo_step_and_moves_the_fingerprint",
                addWhy == CadStatus::Ok && patternId == 3u && history.undoDepth() == 1u
                        && near(volume(), 16.0 - 3 * hole, 1e-7)
                        && projectSemanticFingerprint(scene, ProjectKind::Construction) != before);
        CadFeaturePattern wider = linear(2, CadBodyAxis::X, 2, 1.2);
        const CadStatus editWhy = editCadFeaturePatternOfBody(scene, history, id, patternId, wider);
        r.check("PT_15_edit_keeps_the_id_and_is_one_step",
                editWhy == CadStatus::Ok && history.undoDepth() == 2u
                        && findCadFeaturePattern(scene.findBody(id)->cadOrNull()->state(), patternId)
                                   != nullptr
                        && near(volume(), 16.0 - 2 * hole, 1e-7));
        const size_t depth = history.undoDepth();
        const CadStatus refused = editCadFeaturePatternOfBody(scene, history, id, patternId,
                                                              linear(2, CadBodyAxis::X, 2, 9.0));
        r.check("PT_16_a_refused_edit_records_nothing_and_changes_nothing",
                refused == CadStatus::PatternInstanceNoEffect && history.undoDepth() == depth
                        && near(volume(), 16.0 - 2 * hole, 1e-7));
        const CadTimeline timeline = buildCadTimelineFor(scene.findBody(id)->cadOrNull()->state());
        bool listed = false;
        for (const CadTimelineRow& row : timeline.rows) {
            listed = listed
                     || (row.kind == CadTimelineRowKind::Pattern && row.id == patternId
                         && row.patternInstances == 2u && row.state == CadTimelineRowState::Ok);
        }
        r.check("PT_17_the_history_lists_the_pattern_after_the_feature_it_follows",
                listed && timeline.rows.back().kind == CadTimelineRowKind::Pattern);
        CadBodyState failing = scene.findBody(id)->cadOrNull()->state();
        failing.patterns[0].spacing = 9.0;
        const CadTimeline bad = buildCadTimelineFor(failing);
        bool failedRow = false;
        for (const CadTimelineRow& row : bad.rows) {
            failedRow = failedRow
                        || (row.kind == CadTimelineRowKind::Pattern
                            && row.state == CadTimelineRowState::Failed
                            && row.status == CadStatus::PatternInstanceNoEffect);
        }
        r.check("PT_17_a_failing_pattern_is_the_failed_row", failedRow);
        const CadStatus deleteWhy = deleteCadFeaturePatternOfBody(scene, history, id, patternId);
        const bool deleted = deleteWhy == CadStatus::Ok
                             && scene.findBody(id)->cadOrNull()->state().patterns.empty()
                             && near(volume(), 16.0 - hole, 1e-7);
        history.undo();
        r.check("PT_15_delete_is_one_step_and_undo_brings_the_same_pattern_back",
                deleted && findCadFeaturePattern(scene.findBody(id)->cadOrNull()->state(), patternId)
                                   != nullptr
                        && near(volume(), 16.0 - 2 * hole, 1e-7));
        history.undo();
        history.undo();
        r.check("PT_15_undo_walks_back_to_the_unpatterned_body",
                scene.findBody(id)->cadOrNull()->state().patterns.empty()
                        && projectSemanticFingerprint(scene, ProjectKind::Construction) == before);
    }
    // Suggestions the editor seeds with.
    {
        const CadBodyState state = bossState(1.0, 1.0);
        const double spacing = suggestCadPatternSpacing(state, {2u}, CadBodyAxis::X);
        CadBodyAxis axis = CadBodyAxis::X;
        double center[3] = {9, 9, 9};
        const bool axisOk = suggestCadPatternCircularAxis(state, &axis, center);
        r.check("PT_18_the_seeded_spacing_clears_the_feature_and_the_seeded_axis_is_the_normal",
                near(spacing, 0.5) && axisOk && axis == CadBodyAxis::Z && near(center[0], 0.0, 1e-6)
                        && near(center[1], 0.0, 1e-6) && center[2] == 0.0
                        && volumeIs(regen(withPattern(state, linear(2, CadBodyAxis::X, 2, spacing))),
                                    16.0 + 2 * boss));
    }
    // The seeded run stays on the part: a boss near +X runs toward -X, as far
    // as the part reaches, and that run regenerates.
    {
        const CadBodyState state = bossState(1.0, 1.0);
        uint32_t count = 0;
        bool reversed = false;
        suggestCadPatternLinearRun(state, 2u, CadBodyAxis::X, 0.5, 3u, &count, &reversed);
        CadFeaturePattern run = linear(2, CadBodyAxis::X, count, 0.5);
        run.reversed = reversed;
        uint32_t tight = 0;
        bool tightReversed = true;
        suggestCadPatternLinearRun(bossState(0.0, 0.0), 2u, CadBodyAxis::X, 9.0, 3u, &tight,
                                   &tightReversed);
        r.check("PT_20_the_seeded_run_turns_toward_the_room_and_fits_the_part",
                count == 3u && reversed && volumeIs(regen(withPattern(state, run)), 16.0 + 3 * boss)
                        && tight == 2u);
    }
    if (performance != nullptr) {
        // One bounded run: a 4 x 4 grid of pockets.
        CadFeaturePattern grid = linear(2, CadBodyAxis::X, 4, 0.8);
        grid.axis2 = CadBodyAxis::Y;
        grid.count2 = 4;
        grid.spacing2 = 0.8;
        const CadBodyState state = withPattern(pocketState(-1.2, -1.2), grid);
        const auto g0 = std::chrono::steady_clock::now();
        const Regen g = regen(state);
        const double gridUs =
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - g0).count();
        r.check("PT_19_a_four_by_four_grid_regenerates_exactly", volumeIs(g, 16.0 - 16 * hole));
        char line[128];
        std::snprintf(line, sizeof(line), "pattern_grid16_us=%.0f pattern_acts_us=%.0f", gridUs,
                      std::chrono::duration<double, std::micro>(g0 - t0).count());
        *performance = line;
    }
}

}  // namespace forgeshape
