#include "forgeshape_cad_edge_finish_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_edge_finish.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_timeline.h"
#include "forgeshape_history.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_sketch.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool passed) { out->push_back(ArrangementSelfTestCheck{name, passed}); }
};

SketchEntityId addRect(CadSketch* sketch, double cu, double cv, double w, double h) {
    SketchRectangle rectangle;
    rectangle.center = SketchPoint{cu, cv};
    rectangle.width = w;
    rectangle.height = h;
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, rectangle, &id);
    return id;
}

ExtrudeFeature oneSide(double depth, ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = 1;
    extrude.depth = depth;
    extrude.direction = direction;
    return extrude;
}

// A w x h x depth block on XY: x in [-w/2, w/2], y in [-h/2, h/2], z in [0, depth].
CadBodyState blockState(double w = 2.0, double h = 2.0, double depth = 1.0) {
    CadBodyState state;
    addRect(&cadBaseSketch(state), 0.0, 0.0, w, h);
    state.extrude = oneSide(depth);
    return state;
}

CadBodyState circleState() {
    CadBodyState state;
    SketchCircle circle;
    circle.center = SketchPoint{0.0, 0.0};
    circle.radius = 1.0;
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(&cadBaseSketch(state), circle, &id);
    state.extrude = oneSide(1.0);
    return state;
}

// An L: a 2 x 2 square with its +x/+y quadrant removed, extruded 1.
CadBodyState lState() {
    CadBodyState state;
    SketchPolyline polyline;
    polyline.vertices = {SketchPoint{-1, -1}, SketchPoint{1, -1}, SketchPoint{1, 0},
                         SketchPoint{0, 0},   SketchPoint{0, 1},  SketchPoint{-1, 1}};
    polyline.closed = true;
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(&cadBaseSketch(state), polyline, &id);
    state.extrude = oneSide(1.0);
    return state;
}

CadFeatureSupport capSupport(const CadBodyState& state, uint32_t featureId) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face.kind = CadFaceKind::CapFar;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

CadBodyState withFeature(CadBodyState state, CadFeatureOperation operation, double w, double h,
                         double depth, ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    CadSketch sketch;
    addRect(&sketch, 0.0, 0.0, w, h);
    appendCadLaterFeatureWithSketch(&state, operation, capSupport(state, kCadFeatureId),
                                    std::move(sketch), oneSide(depth, direction));
    return state;
}

// The edge of `featureId`'s prism whose midpoint is nearest `near`.
bool edgeNear(const CadBodyState& state, uint32_t featureId, DVec3 near, CadFeatureEdge* out) {
    std::vector<CadFeatureEdge> edges;
    if (enumerateCadFeatureEdges(state, featureId, &edges) != CadStatus::Ok) {
        return false;
    }
    double best = 1e30;
    for (const CadFeatureEdge& edge : edges) {
        const DVec3 mid = dvec3Scale(dvec3Add(edge.start, edge.end), 0.5);
        const DVec3 d = dvec3Sub(mid, near);
        const double dist = dvec3Dot(d, d);
        if (dist < best) {
            best = dist;
            *out = edge;
        }
    }
    return best < 1e-6;
}

bool volumeOf(const CadBodyState& state, double* volume, CadRegenerationReport* report = nullptr) {
    CadBodyMesh mesh;
    if (regenerateCadBody(state, &mesh, report) != CadStatus::Ok) {
        return false;
    }
    *volume = mesh.volume;
    return true;
}

CadStatus finishWith(CadBodyState* state, CadEdgeFinishKind kind, double distance,
                     std::vector<CadEdgeRef> edges) {
    appendCadEdgeFinish(state, kind, distance, std::move(edges));
    CadBodyMesh mesh;
    CadRegenerationReport report;
    const CadStatus valid = validateCadBodyState(*state);
    if (valid != CadStatus::Ok) {
        return valid;
    }
    return regenerateCadBody(*state, &mesh, &report);
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

// Where the CADB payload starts, its version and its length.
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

void runCadEdgeFinishSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                               std::string* performance) {
    Checks r{out};
    const DVec3 topFront{0.0, -1.0, 1.0};  // the block's top edge along x at y = -1

    // EF_01: a block's own extrusion has twelve straight convex 90-degree edges.
    {
        std::vector<CadFeatureEdge> edges;
        const bool ok = enumerateCadFeatureEdges(blockState(), kCadFeatureId, &edges) == CadStatus::Ok;
        bool all = ok && edges.size() == 12u;
        for (const CadFeatureEdge& e : edges) {
            all = all && e.eligible && e.straight && e.convex
                  && std::fabs(e.angleDegrees - 90.0) < 1e-9 && cadEdgeRefCanonical(e.ref);
        }
        r.check("EF_01_a_block_exposes_twelve_eligible_right_angle_edges", all);
        std::vector<CadFeatureEdge> finishable;
        r.check("EF_01_every_block_edge_is_finishable",
                enumerateCadFinishableEdges(blockState(), &finishable) == CadStatus::Ok
                        && finishable.size() == 12u);
    }

    // EF_02: a Chamfer removes exactly its triangle along the edge.
    {
        CadBodyState state = blockState();
        CadFeatureEdge edge;
        const bool found = edgeNear(state, kCadFeatureId, topFront, &edge);
        const uint32_t before = state.nextFeatureId;
        const uint32_t id = appendCadEdgeFinish(&state, CadEdgeFinishKind::Chamfer, 0.2, {edge.ref});
        double volume = 0.0;
        const bool regenerated = validateCadBodyState(state) == CadStatus::Ok && volumeOf(state, &volume);
        r.check("EF_02_a_chamfer_removes_exactly_its_triangle",
                found && regenerated && std::fabs(volume - (4.0 - 0.5 * 0.2 * 0.2 * 2.0)) < 1e-9);
        r.check("EF_02_a_finish_mints_its_id_from_the_feature_high_water_mark",
                id == before && state.nextFeatureId == before + 1u
                        && findCadEdgeFinish(state, id) != nullptr);
        r.check("EF_02_a_finished_body_is_never_legacy_representable",
                !cadBodyStateLegacyRepresentable(state) && cadBodyStateUsesEdgeFinishes(state));
        // The finish's own triangles carry one ineligible face naming it.
        CadBodyMesh mesh;
        std::vector<CadFaceRange> ranges;
        bool tagged = regenerateCadBody(state, &mesh) == CadStatus::Ok
                      && cadFaceRangesFromMesh(mesh, &ranges) == CadStatus::Ok;
        bool sawFinish = false;
        for (const CadFaceRange& range : ranges) {
            if (range.featureId == id) {
                sawFinish = true;
                tagged = tagged && !range.eligible;
            }
        }
        r.check("EF_02_the_bevel_is_its_own_face_and_never_a_sketch_support", tagged && sawFinish);
        // Deterministic: the same chain, the same mesh.
        CadBodyMesh again;
        r.check("EF_02_regeneration_is_deterministic",
                regenerateCadBody(state, &again) == CadStatus::Ok
                        && again.mesh.indices == mesh.mesh.indices
                        && again.mesh.vertices.size() == mesh.mesh.vertices.size());
    }

    // EF_03: a Fillet removes the corner minus the sampled quarter circle.
    {
        CadBodyState state = blockState();
        CadFeatureEdge edge;
        edgeNear(state, kCadFeatureId, topFront, &edge);
        appendCadEdgeFinish(&state, CadEdgeFinishKind::Fillet, 0.2, {edge.ref});
        double volume = 0.0;
        const bool ok = validateCadBodyState(state) == CadStatus::Ok && volumeOf(state, &volume);
        // 8 chords for a quarter turn at 32 per turn: the polygon sector.
        const double r2 = 0.2 * 0.2;
        const double sector = 0.5 * r2 * 8.0 * std::sin(kPi / 16.0);
        const double removed = (r2 - sector) * 2.0;
        r.check("EF_03_a_fillet_removes_the_corner_outside_its_arc",
                ok && std::fabs(volume - (4.0 - removed)) < 1e-9);
    }

    // EF_04: a whole top loop in ONE finish, mitred at the corners.
    {
        CadBodyState state = blockState();
        std::vector<CadEdgeRef> loop;
        for (const DVec3& mid : {DVec3{0, -1, 1}, DVec3{1, 0, 1}, DVec3{0, 1, 1}, DVec3{-1, 0, 1}}) {
            CadFeatureEdge edge;
            if (edgeNear(state, kCadFeatureId, mid, &edge)) loop.push_back(edge.ref);
        }
        appendCadEdgeFinish(&state, CadEdgeFinishKind::Chamfer, 0.2, loop);
        CadBodyMesh mesh;
        const bool ok = loop.size() == 4u && validateCadBodyState(state) == CadStatus::Ok
                        && regenerateCadBody(state, &mesh) == CadStatus::Ok;
        // The frustum the four chamfers leave on top: 4 - (top slab - frustum).
        const double a = 2.0, b = 1.6, hgt = 0.2;
        const double frustum = hgt / 3.0 * (a * a + b * b + a * b);
        const double expected = 4.0 - (a * a * hgt - frustum);
        r.check("EF_04_a_whole_face_loop_is_one_finish_with_mitred_corners",
                ok && mesh.components == 1u && std::fabs(mesh.volume - expected) < 1e-9);
    }

    // EF_05: refusals by name.
    {
        CadBodyState state = blockState();
        CadFeatureEdge edge;
        edgeNear(state, kCadFeatureId, topFront, &edge);
        CadBodyState tooLarge = state;
        r.check("EF_05_a_setback_wider_than_a_face_is_refused",
                finishWith(&tooLarge, CadEdgeFinishKind::Chamfer, 1.5, {edge.ref})
                        == CadStatus::EdgeFinishTooLarge);
        CadBodyState zero = state;
        r.check("EF_05_a_zero_distance_is_refused_never_clamped",
                finishWith(&zero, CadEdgeFinishKind::Fillet, 0.0, {edge.ref})
                        == CadStatus::EdgeFinishInvalid);
        CadBodyState nan = state;
        r.check("EF_05_a_non_finite_distance_is_refused",
                finishWith(&nan, CadEdgeFinishKind::Fillet, std::nan(""), {edge.ref})
                        == CadStatus::EdgeFinishInvalid);
        CadBodyState stale = state;
        CadEdgeRef bogus = edge.ref;
        bogus.faceB.edgeLocalIndex = 77;
        r.check("EF_05_an_edge_that_does_not_exist_is_never_retargeted",
                finishWith(&stale, CadEdgeFinishKind::Chamfer, 0.1, {bogus})
                        == CadStatus::EdgeNotFound);
        CadBodyState dup = state;
        r.check("EF_05_a_duplicated_edge_is_refused",
                finishWith(&dup, CadEdgeFinishKind::Chamfer, 0.1, {edge.ref, edge.ref})
                        == CadStatus::EdgeFinishInvalid);
        CadBodyState empty = state;
        r.check("EF_05_an_empty_edge_list_adds_nothing",
                appendCadEdgeFinish(&empty, CadEdgeFinishKind::Chamfer, 0.1, {}) == 0u
                        && empty.edgeFinishes.empty());
    }

    // EF_06: what R1 does not finish.
    {
        // The L's inner vertical edge is concave.
        std::vector<CadFeatureEdge> edges;
        bool sawConcave = false;
        bool concaveRefused = true;
        if (enumerateCadFeatureEdges(lState(), kCadFeatureId, &edges) == CadStatus::Ok) {
            for (const CadFeatureEdge& e : edges) {
                if (e.straight && !e.convex) {
                    sawConcave = true;
                    concaveRefused = concaveRefused && !e.eligible
                                     && e.why == CadStatus::EdgeNotEligible;
                }
            }
        }
        r.check("EF_06_a_concave_edge_is_not_eligible", sawConcave && concaveRefused);
        // A circle's cap edge meets a curved side.
        std::vector<CadFeatureEdge> circle;
        bool anyEligible = false;
        if (enumerateCadFeatureEdges(circleState(), kCadFeatureId, &circle) == CadStatus::Ok) {
            for (const CadFeatureEdge& e : circle) anyEligible = anyEligible || e.eligible;
        }
        r.check("EF_06_a_cylinder_has_no_finishable_edge", !circle.empty() && !anyEligible);
        // A Cut's pocket edges are not R1's.
        CadBodyState cut = withFeature(blockState(), CadFeatureOperation::Cut, 0.6, 0.6, 0.5,
                                       ExtrudeDirection::AgainstNormal);
        std::vector<CadFeatureEdge> cutEdges;
        bool cutRefused = enumerateCadFeatureEdges(cut, cut.laterFeatures[0].featureId, &cutEdges)
                                  == CadStatus::Ok
                          && !cutEdges.empty();
        for (const CadFeatureEdge& e : cutEdges) {
            cutRefused = cutRefused && !e.eligible && e.why == CadStatus::EdgeFinishTargetUnsupported;
        }
        r.check("EF_06_a_cut_s_edges_are_refused_by_name", cutRefused);
        CadBodyState cutFinish = cut;
        if (!cutEdges.empty()) {
            r.check("EF_06_a_finish_naming_a_cut_edge_is_refused",
                    finishWith(&cutFinish, CadEdgeFinishKind::Chamfer, 0.05, {cutEdges[0].ref})
                            == CadStatus::EdgeFinishTargetUnsupported);
        } else {
            r.check("EF_06_a_finish_naming_a_cut_edge_is_refused", false);
        }
    }

    // EF_07: a boss's vertical edge stops flush on the block it stands on.
    {
        CadBodyState boss = withFeature(blockState(), CadFeatureOperation::Add, 0.8, 0.8, 0.5);
        double before = 0.0;
        const bool okBefore = volumeOf(boss, &before);
        CadFeatureEdge edge;
        // The boss spans x, y in [-0.4, 0.4], z in [1, 1.5]: its +x+y vertical edge.
        const bool found =
                edgeNear(boss, boss.laterFeatures[0].featureId, DVec3{0.4, 0.4, 1.25}, &edge);
        appendCadEdgeFinish(&boss, CadEdgeFinishKind::Chamfer, 0.1, {edge.ref});
        double after = 0.0;
        const bool ok = validateCadBodyState(boss) == CadStatus::Ok && volumeOf(boss, &after);
        r.check("EF_07_a_boss_edge_chamfer_cuts_the_boss_and_not_the_block",
                okBefore && found && ok && std::fabs((before - after) - 0.5 * 0.1 * 0.1 * 0.5) < 1e-9);
    }

    // EF_08: a size edit upstream carries the finish; a structural edit refuses.
    {
        CadBodyState state = blockState();
        CadFeatureEdge edge;
        edgeNear(state, kCadFeatureId, topFront, &edge);
        appendCadEdgeFinish(&state, CadEdgeFinishKind::Fillet, 0.1, {edge.ref});
        CadBodyState deeper = state;
        deeper.extrude.depth = 3.0;
        double volume = 0.0;
        r.check("EF_08_a_depth_edit_carries_the_finish",
                validateCadBodyState(deeper) == CadStatus::Ok && volumeOf(deeper, &volume)
                        && volume < 12.0 && volume > 12.0 - 0.01);
        CadBodyState structural = state;
        CadSketch& sketch = cadBaseSketch(structural);
        sketch.entities.clear();
        SketchCircle circle;
        circle.center = SketchPoint{0, 0};
        circle.radius = 1.0;
        SketchEntityId id = kNoSketchEntity;
        addSketchEntity(&sketch, circle, &id);
        structural.extrude.profileEntityId = id;
        const CadStatus why = validateCadBodyState(structural);
        r.check("EF_08_losing_the_edge_s_faces_refuses_by_name",
                why == CadStatus::EdgeNotFound || why == CadStatus::EdgeNotEligible);
        // A later Cut that removes the whole finished edge names the finish.
        CadBodyState consumed = withFeature(state, CadFeatureOperation::Cut, 3.0, 3.0, 0.5,
                                            ExtrudeDirection::AgainstNormal);
        CadBodyMesh mesh;
        CadRegenerationReport report;
        const CadStatus regen = regenerateCadBody(consumed, &mesh, &report);
        r.check("EF_08_a_later_cut_that_removes_the_edge_names_the_finish",
                regen == CadStatus::EdgeFinishNoEffect
                        && report.failedFeatureId == state.edgeFinishes[0].finishId);
    }

    // EF_09: the tool alone is a closed, outward solid the kernel accepts.
    {
        CadBodyState state = blockState();
        CadFeatureEdge edge;
        edgeNear(state, kCadFeatureId, topFront, &edge);
        CadBodyMesh mesh;
        regenerateCadBody(state, &mesh);
        CadSolid body;
        CadFeatureGeometry geometry;
        buildCadFeatureGeometry(state, kCadFeatureId, &geometry);
        appendCadFeatureSolid(geometry, 0u, &body);
        bool ok = true;
        for (CadEdgeFinishKind kind : {CadEdgeFinishKind::Chamfer, CadEdgeFinishKind::Fillet}) {
            CadSolid tool;
            CadSolidMeasure measure;
            ok = ok && buildCadEdgeFinishTool(edge, kind, 0.2, body, 7u, &tool) == CadStatus::Ok
                 && cadKernelValidateSolid(tool, &measure) == CadKernelStatus::Ok
                 && measure.volume > 0.0;
        }
        r.check("EF_09_both_tools_are_valid_closed_outward_solids", ok);
        r.check("EF_09_point_containment_reads_inside_and_outside",
                cadSolidContainsPoint(body, DVec3{0.1, 0.2, 0.3})
                        && !cadSolidContainsPoint(body, DVec3{3.0, 0.0, 0.5}));
    }

    // EF_11: `CADB` v9 -- written only for a finished body, round-tripped
    // exactly, and an unknown finish kind refused by name.
    {
        CadBodyState plain = blockState();
        CadBodyState state = plain;
        CadFeatureEdge edge;
        edgeNear(state, kCadFeatureId, topFront, &edge);
        appendCadEdgeFinish(&state, CadEdgeFinishKind::Fillet, 0.125, {edge.ref});
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state), &why);
        const std::vector<uint8_t> plainBytes = encodeProjectV1(documentFor(plain), &why);
        uint16_t version = 0, plainVersion = 0;
        uint64_t length = 0, plainLength = 0;
        const size_t base = cadPayloadAt(bytes, &version, &length);
        cadPayloadAt(plainBytes, &plainVersion, &plainLength);
        r.check("EF_11_a_finished_body_is_written_cadb_v9",
                base != 0 && version == kCadSectionVersionV9);
        r.check("EF_11_a_body_without_finishes_keeps_its_version", plainVersion < kCadSectionVersionV6);
        ProjectDocument decoded;
        const ProjectCodecStatus decodedWhy = decodeProject(bytes.data(), bytes.size(), &decoded);
        r.check("EF_11_v9_round_trips_the_finish_exactly",
                decodedWhy == ProjectCodecStatus::Ok && decoded.cad.bodies.size() == 1u
                        && sameCadBodyState(decoded.cad.bodies[0].state, state));
        // The kind byte: the payload's tail is finish id, kind, distance, edge
        // count, then one edge of a feature id and two 9-byte tokens.
        std::vector<uint8_t> bad = bytes;
        bool refused = false;
        if (base != 0 && length > 35u) {
            const size_t kindAt = base + static_cast<size_t>(length) - 22u - 4u - 8u - 1u;
            bad[kindAt] = 9u;
            const uint32_t crc = crc32IsoHdlc(&bad[base], static_cast<size_t>(length));
            std::memcpy(&bad[base - kForgeSectionHeaderBytes + 16], &crc, 4);
            ProjectDocument scratch;
            refused = decodeProject(bad.data(), bad.size(), &scratch)
                      == ProjectCodecStatus::InvalidSemanticValue;
        }
        r.check("EF_11_an_unknown_finish_kind_is_refused_by_name", refused);
        // A stored edge that does not resolve is refused at load, never opened.
        CadBodyState stale = state;
        stale.edgeFinishes[0].edges[0].faceB.edgeLocalIndex = 77;
        stale.edgeFinishes[0].edges[0] = canonicalCadEdgeRef(stale.edgeFinishes[0].edges[0]);
        ProjectCodecStatus staleWhy = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> staleBytes = encodeProjectV1(documentFor(stale), &staleWhy);
        r.check("EF_11_a_document_naming_a_missing_edge_is_not_encoded",
                staleWhy != ProjectCodecStatus::Ok || staleBytes.empty());
    }

    // EF_12: the three user acts, each one Undo, and the History rows.
    {
        ConstructionScene scene{NoProjectTag{}};
        ConstructionHistory history{scene};
        SceneObject* added = scene.addCadBody(blockState());
        const ObjectId id = added != nullptr ? added->objectId() : kNoObject;
        if (added != nullptr) publishSceneObject(*added);
        auto stateOf = [&scene, id]() -> const CadBodyState& {
            return scene.findBody(id)->cadOrNull()->state();
        };
        const uint64_t before = projectSemanticFingerprint(scene, ProjectKind::Construction);
        // The whole top face's edges in one finish.
        std::vector<CadEdgeRef> top;
        CadFaceToken cap;
        cap.kind = CadFaceKind::CapFar;
        const bool listed = cadFinishableEdgesOfFace(stateOf(), kCadFeatureId, cap, &top)
                                    == CadStatus::Ok
                            && top.size() == 4u;
        r.check("EF_12_a_face_lists_its_four_finishable_edges", listed);
        const double suggested = suggestCadEdgeFinishDistance(stateOf(), CadEdgeFinishKind::Fillet, top);
        r.check("EF_12_the_suggested_distance_is_readable_and_fits",
                std::fabs(suggested - 0.2) < 1e-12);
        uint32_t finishId = 0;
        const CadStatus addWhy = addCadEdgeFinishToBody(scene, history, id, CadEdgeFinishKind::Fillet,
                                                        suggested, top, &finishId);
        const bool added1 = addWhy == CadStatus::Ok && stateOf().edgeFinishes.size() == 1u
                            && history.undoDepth() == 1u;
        r.check("EF_12_add_is_one_undo_step", added1);
        r.check("EF_12_a_finish_moves_the_fingerprint",
                projectSemanticFingerprint(scene, ProjectKind::Construction) != before);
        const CadTimeline timeline = buildCadTimelineFor(stateOf());
        bool finishRow = false;
        for (const CadTimelineRow& row : timeline.rows) {
            if (row.kind == CadTimelineRowKind::Finish && row.id == finishId
                && row.finishEdgeCount == 4u && row.state == CadTimelineRowState::Ok) {
                finishRow = true;
            }
        }
        r.check("EF_12_history_lists_the_finish_after_the_features", finishRow
                && timeline.rows.back().kind == CadTimelineRowKind::Finish);
        const CadStatus editWhy = editCadEdgeFinishOfBody(scene, history, id, finishId,
                                                          CadEdgeFinishKind::Chamfer, 0.1);
        r.check("EF_12_edit_changes_kind_and_distance_as_one_step",
                editWhy == CadStatus::Ok && history.undoDepth() == 2u
                        && stateOf().edgeFinishes[0].kind == CadEdgeFinishKind::Chamfer
                        && stateOf().edgeFinishes[0].distance == 0.1);
        const CadStatus tooBig = editCadEdgeFinishOfBody(scene, history, id, finishId,
                                                         CadEdgeFinishKind::Chamfer, 5.0);
        r.check("EF_12_a_refused_edit_changes_nothing",
                tooBig == CadStatus::EdgeFinishTooLarge && history.undoDepth() == 2u
                        && stateOf().edgeFinishes[0].distance == 0.1);
        const CadStatus deleteWhy = deleteCadEdgeFinishOfBody(scene, history, id, finishId);
        r.check("EF_12_delete_is_one_step_and_restores_the_sharp_body",
                deleteWhy == CadStatus::Ok && stateOf().edgeFinishes.empty()
                        && history.undoDepth() == 3u
                        && projectSemanticFingerprint(scene, ProjectKind::Construction) != before);
        history.undo();
        history.undo();
        history.undo();
        r.check("EF_12_undo_returns_to_the_unfinished_body_exactly",
                stateOf().edgeFinishes.empty()
                        && projectSemanticFingerprint(scene, ProjectKind::Construction) == before);
        history.redo();
        r.check("EF_12_redo_restores_the_same_finish",
                stateOf().edgeFinishes.size() == 1u && stateOf().edgeFinishes[0].finishId == finishId);
    }

    // Bounded single-shot timing: a block's top loop filleted.
    {
        CadBodyState state = blockState();
        std::vector<CadEdgeRef> loop;
        for (const DVec3& mid : {DVec3{0, -1, 1}, DVec3{1, 0, 1}, DVec3{0, 1, 1}, DVec3{-1, 0, 1}}) {
            CadFeatureEdge edge;
            if (edgeNear(state, kCadFeatureId, mid, &edge)) loop.push_back(edge.ref);
        }
        appendCadEdgeFinish(&state, CadEdgeFinishKind::Fillet, 0.2, loop);
        const auto t0 = std::chrono::steady_clock::now();
        CadBodyMesh mesh;
        const CadStatus why = regenerateCadBody(state, &mesh);
        const double us = std::chrono::duration<double, std::micro>(
                                  std::chrono::steady_clock::now() - t0)
                                  .count();
        r.check("EF_10_a_filleted_top_loop_regenerates", why == CadStatus::Ok);
        char line[96];
        std::snprintf(line, sizeof line, "edge_finish_fillet_loop_us=%.0f", us);
        if (performance != nullptr) *performance = line;
    }
}

}  // namespace forgeshape
