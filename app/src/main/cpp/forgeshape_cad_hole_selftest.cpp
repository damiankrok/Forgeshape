#include "forgeshape_cad_hole_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_hole.h"
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

// The 32-gon every round thing is built as.
double disk(double r) { return 0.5 * r * r * 32.0 * std::sin(2.0 * kPi / 32.0); }

// A 32-gon frustum between radii a and b over height h.
double frustum(double a, double b, double h) {
    const double A = disk(a);
    const double B = disk(b);
    return h / 3.0 * (A + B + std::sqrt(A * B));
}

// A 4 x 4 x `height` block on XY: x, y in [-2, 2], z in [0, height].
CadBodyState blockState(double height = 1.0) {
    CadBodyState state;
    cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
    state.extrude = oneSide(height);
    return state;
}

CadFeatureSupport supportOn(const CadBodyState& state, uint32_t featureId, CadFaceToken token) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face = token;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

CadFaceToken capFar() {
    CadFaceToken token;
    token.kind = CadFaceKind::CapFar;
    return token;
}

// A hole of `kind` drilled into `support` with its axis through the body-local
// point `at` (which must lie on that face's plane).
CadFeatureHole holeAt(const CadBodyState& state, const CadFeatureSupport& support, DVec3 at,
                      double diameter) {
    CadFeatureHole hole;
    hole.support = support;
    hole.diameter = diameter;
    std::vector<CadFeatureGeometry> chain;
    buildCadChainGeometry(state, &chain);
    CadFrame64 frame;
    if (resolveCadFeatureSupportFrame(chain, support, &frame)) {
        const DVec3 d = dvec3Sub(at, frame.origin);
        hole.u = dvec3Dot(d, frame.u);
        hole.v = dvec3Dot(d, frame.v);
    }
    return hole;
}

CadFeatureHole topHole(const CadBodyState& state, double x, double y, double diameter) {
    return holeAt(state, supportOn(state, kCadFeatureId, capFar()), DVec3{x, y, 1.0}, diameter);
}

CadBodyState withHole(CadBodyState state, CadFeatureHole hole) {
    appendCadFeatureHole(&state, std::move(hole));
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

}  // namespace

void runCadHoleSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    if (out == nullptr) return;
    Checks r{out};
    const CadBodyState block = blockState();

    // --- HO_01..04: the four shapes, exact ------------------------------------
    {
        const CadFeatureHole through = topHole(block, 0.0, 0.0, 0.6);
        r.check("HO_01_a_through_all_hole_removes_its_bore_exactly",
                volumeIs(regen(withHole(block, through)), 16.0 - disk(0.3)));
        CadFeatureHole blind = through;
        blind.throughAll = false;
        blind.depth = 0.4;
        r.check("HO_02_a_blind_hole_removes_its_depth_exactly",
                volumeIs(regen(withHole(block, blind)), 16.0 - disk(0.3) * 0.4));
        CadFeatureHole bore = topHole(block, 0.5, -0.5, 0.4);
        bore.kind = CadHoleKind::Counterbore;
        bore.boreDiameter = 0.8;
        bore.boreDepth = 0.2;
        r.check("HO_03_a_counterbore_removes_its_bore_and_its_step",
                volumeIs(regen(withHole(block, bore)),
                         16.0 - disk(0.4) * 0.2 - disk(0.2) * 0.8));
        CadFeatureHole sink = topHole(block, -0.5, 0.5, 0.4);
        sink.kind = CadHoleKind::Countersink;
        sink.sinkDiameter = 0.8;
        sink.sinkAngleDegrees = 90.0;
        r.check("HO_04_a_countersink_cone_is_as_deep_as_its_angle_says",
                near(cadHoleCountersinkDepth(sink), 0.2));
        r.check("HO_04_a_countersink_removes_its_cone_and_its_bore",
                volumeIs(regen(withHole(block, sink)),
                         16.0 - frustum(0.4, 0.2, 0.2) - disk(0.2) * 0.8));
    }

    // --- HO_05..06: every refusal by name --------------------------------------
    {
        const Regen off = regen(withHole(block, topHole(block, 10.0, 0.0, 0.4)));
        r.check("HO_05_a_hole_beside_the_part_is_no_effect_and_names_itself",
                off.why == CadStatus::HoleNoEffect && off.failed == 2u);
        auto refused = [&](CadFeatureHole hole, CadStatus expected) {
            return validateCadBodyState(withHole(block, std::move(hole))) == expected;
        };
        const CadFeatureHole ok = topHole(block, 0.0, 0.0, 0.4);
        CadFeatureHole h = ok;
        h.diameter = 0.0;
        bool values = refused(h, CadStatus::HoleInvalid);
        h = ok;
        h.diameter = NAN;
        values = values && refused(h, CadStatus::HoleInvalid);
        h = ok;
        h.depth = 0.3;  // Through All stores no depth
        values = values && refused(h, CadStatus::HoleInvalid);
        h = ok;
        h.throughAll = false;  // a blind hole states one
        values = values && refused(h, CadStatus::HoleInvalid);
        h = ok;
        h.boreDiameter = 0.8;  // a Simple hole carries no bore
        values = values && refused(h, CadStatus::HoleInvalid);
        h = ok;
        h.kind = CadHoleKind::Counterbore;
        h.boreDiameter = 0.3;  // narrower than the hole
        h.boreDepth = 0.2;
        values = values && refused(h, CadStatus::HoleInvalid);
        h.boreDiameter = 0.8;
        h.throughAll = false;
        h.depth = 0.2;  // the step must be above the bottom
        values = values && refused(h, CadStatus::HoleInvalid);
        h = ok;
        h.kind = CadHoleKind::Countersink;
        h.sinkDiameter = 0.8;
        h.sinkAngleDegrees = 10.0;
        values = values && refused(h, CadStatus::HoleInvalid);
        h.sinkAngleDegrees = 90.0;
        h.u = INFINITY;
        values = values && refused(h, CadStatus::HoleInvalid);
        r.check("HO_06_every_out_of_bounds_or_non_canonical_value_is_hole_invalid", values);
        h = ok;
        h.support.featureId = 9;
        bool supports = refused(h, CadStatus::HoleSupportInvalid);
        h = ok;
        h.support.lineageToken ^= 1u;
        supports = supports && refused(h, CadStatus::HoleSupportInvalid);
        h = ok;
        h.support.face.kind = CadFaceKind::Side;
        h.support.face.edgeEntityId = 77;
        supports = supports && refused(h, CadStatus::HoleSupportInvalid);
        r.check("HO_06_a_face_that_does_not_resolve_is_refused_never_retargeted", supports);
        CadBodyState full = block;
        for (uint32_t i = 0; i < kMaxCadHoles; ++i) {
            appendCadFeatureHole(&full, topHole(block, -1.5 + 0.09 * i, 0.0, 0.05));
        }
        CadBodyState over = full;
        over.holes.push_back(over.holes.back());
        over.holes.back().holeId = over.nextFeatureId++;
        r.check("HO_06_the_hole_cap_is_enforced",
                appendCadFeatureHole(&full, ok) == 0u
                        && validateCadBodyState(over) == CadStatus::TooManyHoles);
        CadBodyState reused = withHole(block, ok);
        reused.holes[0].holeId = 1;
        r.check("HO_06_a_hole_id_outside_the_fresh_id_space_is_refused",
                validateCadBodyState(reused) == CadStatus::HoleInvalid);
    }

    // --- HO_07..08: other faces ------------------------------------------------
    {
        // A side of the block: the hole runs the whole 4 m across it.
        std::vector<CadFace> faces;
        enumerateCadFeatureFaces(block, kCadFeatureId, &faces);
        bool sideOk = false;
        for (const CadFace& face : faces) {
            if (face.token.kind != CadFaceKind::Side) continue;
            const DVec3 centre{2.0 * face.n.x, 2.0 * face.n.y, 0.5};
            const CadFeatureHole side =
                    holeAt(block, supportOn(block, kCadFeatureId, face.token), centre, 0.4);
            sideOk = volumeIs(regen(withHole(block, side)), 16.0 - disk(0.2) * 4.0);
            break;
        }
        r.check("HO_07_a_hole_in_a_side_face_runs_across_the_part", sideOk);
        // A boss's top: Through All goes through the boss AND the block.
        CadBodyState boss = block;
        appendCadLaterFeatureWithSketch(&boss, CadFeatureOperation::Add,
                                        supportOn(block, kCadFeatureId, capFar()),
                                        rectSketch(1.0, 1.0, 0.4, 0.4), oneSide(0.5));
        const CadFeatureHole onBoss =
                holeAt(boss, supportOn(boss, 2u, capFar()), DVec3{1.0, 1.0, 1.5}, 0.2);
        r.check("HO_08_through_all_from_a_boss_goes_through_the_whole_part",
                volumeIs(regen(withHole(boss, onBoss)), 16.0 + 0.08 - disk(0.1) * 1.5));
        // A pocket's floor: a material-outward face drilled through the rest.
        CadBodyState pocket = block;
        appendCadLaterFeatureWithSketch(&pocket, CadFeatureOperation::Cut,
                                        supportOn(block, kCadFeatureId, capFar()),
                                        rectSketch(0.0, 0.0, 1.0, 1.0),
                                        oneSide(0.5, ExtrudeDirection::AgainstNormal));
        const CadFeatureHole floor =
                holeAt(pocket, supportOn(pocket, 2u, capFar()), DVec3{0.0, 0.0, 0.5}, 0.2);
        r.check("HO_08_a_hole_in_a_pocket_floor_goes_through_what_is_left",
                volumeIs(regen(withHole(pocket, floor)), 16.0 - 0.5 - disk(0.1) * 0.5));
    }

    // --- HO_09..11: the chain ----------------------------------------------------
    {
        // A later Cut on the very bore sees the hole and removes nothing new.
        CadBodyState state = withHole(block, topHole(block, 0.0, 0.0, 0.6));
        appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut,
                                        supportOn(state, kCadFeatureId, capFar()),
                                        circleSketch(0.0, 0.0, 0.3),
                                        oneSide(0.5, ExtrudeDirection::AgainstNormal));
        const Regen g = regen(state);
        r.check("HO_09_a_feature_after_a_hole_sees_it",
                g.why == CadStatus::CutNoIntersection && g.failed == 3u);
        // Through All is DERIVED: a taller block makes a longer bore.
        CadBodyState tall = withHole(block, topHole(block, 0.0, 0.0, 0.6));
        tall.extrude.depth = 2.0;
        tall.holes[0].support.lineageToken = cadFeatureTopologySignature(tall, kCadFeatureId);
        // The top moved, so the hole's point moves with the face it stands on.
        r.check("HO_10_through_all_follows_an_upstream_size_edit",
                volumeIs(regen(tall), 32.0 - disk(0.3) * 2.0));
        const Regen tagged = regen(withHole(block, topHole(block, 0.0, 0.0, 0.6)));
        bool holeFaces = tagged.why == CadStatus::Ok;
        bool sawHole = false;
        for (size_t t = 0; holeFaces && t < tagged.mesh.triangleFace.size(); ++t) {
            const CadMeshFace& face = tagged.mesh.faces[tagged.mesh.triangleFace[t]];
            if (face.featureId == 2u) {
                sawHole = true;
                holeFaces = holeFaces && !face.eligible;
            }
        }
        r.check("HO_11_bore_triangles_wear_the_holes_ineligible_entry", holeFaces && sawHole);
    }

    // --- HO_12: CADB v11 ---------------------------------------------------------
    {
        CadFeatureHole bore = topHole(block, 0.5, -0.5, 0.4);
        bore.kind = CadHoleKind::Counterbore;
        bore.boreDiameter = 0.8;
        bore.boreDepth = 0.2;
        bore.throughAll = false;
        bore.depth = 0.7;
        const CadBodyState state = withHole(block, bore);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state), &why);
        const std::vector<uint8_t> plainBytes = encodeProjectV1(documentFor(block), &why);
        uint16_t version = 0, plainVersion = 0;
        uint64_t length = 0, plainLength = 0;
        const size_t base = cadPayloadAt(bytes, &version, &length);
        cadPayloadAt(plainBytes, &plainVersion, &plainLength);
        r.check("HO_12_a_drilled_body_is_written_cadb_v11",
                base != 0 && version == kCadSectionVersionV11);
        r.check("HO_12_a_body_without_holes_keeps_its_version", plainVersion < kCadSectionVersionV9);
        ProjectDocument decoded;
        r.check("HO_12_v11_round_trips_the_hole_exactly",
                decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                        && decoded.cad.bodies.size() == 1u
                        && sameCadBodyState(decoded.cad.bodies[0].state, state));
        // One hole on a whole-face token is 91 bytes at the payload's end: the
        // kind byte is its fifth, the Through All byte its fifty-first.
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
        r.check("HO_12_an_unknown_hole_kind_is_refused_by_name", refusedWith(87u, 9u));
        r.check("HO_12_a_through_all_byte_above_one_is_refused", refusedWith(41u, 2u));
    }

    // --- HO_13..14: the acts, the fingerprint and the History --------------------
    const auto t0 = std::chrono::steady_clock::now();
    {
        ConstructionScene scene{NoProjectTag{}};
        ConstructionHistory history{scene};
        SceneObject* added = scene.addCadBody(block);
        const ObjectId id = added != nullptr ? added->objectId() : kNoObject;
        if (added != nullptr) publishSceneObject(*added);
        auto volume = [&scene, id]() {
            std::shared_ptr<const CadBodyMesh> mesh;
            const CadBody* body = scene.findBody(id)->cadOrNull();
            return body->regenerated(&mesh) == CadStatus::Ok ? mesh->volume : -1.0;
        };
        const uint64_t before = projectSemanticFingerprint(scene, ProjectKind::Construction);
        uint32_t holeId = 0;
        const CadStatus addWhy =
                addCadFeatureHoleToBody(scene, history, id, topHole(block, 0.0, 0.0, 0.6), &holeId);
        r.check("HO_13_add_is_one_undo_step_and_moves_the_fingerprint",
                addWhy == CadStatus::Ok && holeId == 2u && history.undoDepth() == 1u
                        && near(volume(), 16.0 - disk(0.3), 1e-7)
                        && projectSemanticFingerprint(scene, ProjectKind::Construction) != before);
        CadFeatureHole wider = topHole(block, 0.0, 0.0, 0.8);
        wider.support.featureId = 99;  // an edit keeps the face it stands on
        const CadStatus editWhy = editCadFeatureHoleOfBody(scene, history, id, holeId, wider);
        r.check("HO_13_edit_keeps_the_id_and_the_face_and_is_one_step",
                editWhy == CadStatus::Ok && history.undoDepth() == 2u
                        && findCadFeatureHole(scene.findBody(id)->cadOrNull()->state(), holeId)
                                       ->support.featureId
                                   == kCadFeatureId
                        && near(volume(), 16.0 - disk(0.4), 1e-7));
        const size_t depth = history.undoDepth();
        CadFeatureHole off = topHole(block, 9.0, 0.0, 0.4);
        const CadStatus refused = editCadFeatureHoleOfBody(scene, history, id, holeId, off);
        r.check("HO_13_a_refused_edit_records_nothing_and_changes_nothing",
                refused == CadStatus::HoleNoEffect && history.undoDepth() == depth
                        && near(volume(), 16.0 - disk(0.4), 1e-7));
        const CadTimeline timeline = buildCadTimelineFor(scene.findBody(id)->cadOrNull()->state());
        bool listed = false;
        for (const CadTimelineRow& row : timeline.rows) {
            listed = listed
                     || (row.kind == CadTimelineRowKind::Hole && row.id == holeId
                         && row.holeThroughAll && near(row.holeDiameter, 0.8)
                         && row.state == CadTimelineRowState::Ok);
        }
        r.check("HO_14_the_history_lists_the_hole", listed);
        CadBodyState failing = scene.findBody(id)->cadOrNull()->state();
        failing.holes[0].u = 9.0;
        bool failedRow = false;
        for (const CadTimelineRow& row : buildCadTimelineFor(failing).rows) {
            failedRow = failedRow
                        || (row.kind == CadTimelineRowKind::Hole
                            && row.state == CadTimelineRowState::Failed
                            && row.status == CadStatus::HoleNoEffect);
        }
        r.check("HO_14_a_failing_hole_is_the_failed_row", failedRow);
        const CadStatus missing = deleteCadFeatureHoleOfBody(scene, history, id, 77u);
        const CadStatus deleteWhy = deleteCadFeatureHoleOfBody(scene, history, id, holeId);
        const bool deleted = missing == CadStatus::HoleNotFound && deleteWhy == CadStatus::Ok
                             && scene.findBody(id)->cadOrNull()->state().holes.empty()
                             && near(volume(), 16.0, 1e-7);
        history.undo();
        r.check("HO_13_delete_is_one_step_and_undo_brings_the_same_hole_back",
                deleted && findCadFeatureHole(scene.findBody(id)->cadOrNull()->state(), holeId)
                                   != nullptr
                        && near(volume(), 16.0 - disk(0.4), 1e-7));
        history.undo();
        history.undo();
        r.check("HO_13_undo_walks_back_to_the_undrilled_body",
                scene.findBody(id)->cadOrNull()->state().holes.empty()
                        && projectSemanticFingerprint(scene, ProjectKind::Construction) == before);
    }

    // --- HO_15: the editor's seeds -------------------------------------------------
    {
        const double d = suggestCadHoleDiameter(block);
        r.check("HO_15_the_seeded_diameter_is_a_round_fifth_of_the_thinnest_extent",
                near(d, 0.2) && near(snapCadHolePosition(0.537, d), 0.55)
                        && snapCadHolePosition(-0.01, d) == 0.0
                        && !std::signbit(snapCadHolePosition(-0.01, d)));
    }

    if (performance != nullptr) {
        CadFeatureHole sink = topHole(block, 0.0, 0.0, 0.4);
        sink.kind = CadHoleKind::Countersink;
        sink.sinkDiameter = 0.8;
        sink.sinkAngleDegrees = 90.0;
        const auto g0 = std::chrono::steady_clock::now();
        const Regen g = regen(withHole(block, sink));
        const double holeUs =
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - g0).count();
        char line[128];
        std::snprintf(line, sizeof(line), "hole_countersink_us=%.0f hole_acts_us=%.0f%s", holeUs,
                      std::chrono::duration<double, std::micro>(g0 - t0).count(),
                      g.why == CadStatus::Ok ? "" : " hole_perf_failed");
        *performance = line;
    }
}

}  // namespace forgeshape
