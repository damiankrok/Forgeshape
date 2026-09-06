#include "forgeshape_cad_a3_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "forgeshape_body_commands.h"
#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_camera.h"
#include "forgeshape_history.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_math.h"
#include "forgeshape_project_bootstrap.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch.h"
#include "forgeshape_support_chooser.h"
#include "forgeshape_gizmo.h"

namespace forgeshape {
namespace {

struct Recorder {
    CadA3SelfTestResult* out;
    int max;
    int n = 0;
    void check(const char* name, bool ok) {
        if (n < max) {
            out[n].name = name;
            out[n].passed = ok;
            ++n;
        }
    }
};

std::string g_performance = "not measured";

bool nearf(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) <= tol; }
bool near3(const Vec3& a, const Vec3& b, float tol = 1e-4f) {
    return nearf(a.x, b.x, tol) && nearf(a.y, b.y, tol) && nearf(a.z, b.z, tol);
}

CadBodyState rectBody(Workplane plane, double w, double h, double depth,
                      ExtrudeDirection dir = ExtrudeDirection::AlongNormal) {
    CadBodyState state;
    state.sketch.plane = plane;
    SketchRectangle r;
    r.center = SketchPoint{0.0, 0.0};
    r.width = w;
    r.height = h;
    addSketchEntity(&state.sketch, r);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = depth;
    state.extrude.direction = dir;
    return state;
}

CadBodyState circleBody(Workplane plane, double radius, double depth) {
    CadBodyState state;
    state.sketch.plane = plane;
    SketchCircle c;
    c.center = SketchPoint{0.0, 0.0};
    c.radius = radius;
    addSketchEntity(&state.sketch, c);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = depth;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

// A face-supported child rectangle on the given producer face.
CadBodyState childOn(const CadBodyState& producer, ObjectId producerId, const CadFaceToken& token,
                     double w, double h, double depth) {
    CadBodyState state = rectBody(Workplane::XY, w, h, depth);
    state.sketch.hasFaceSupport = true;
    state.sketch.faceSupport.producerObjectId = producerId;
    state.sketch.faceSupport.producerLocalFeatureId = kCadFeatureId;
    state.sketch.faceSupport.face = token;
    state.sketch.faceSupport.lineageToken = cadTopologySignature(producer);
    return state;
}

CadFaceToken tokenFor(const CadBodyState& state, CadFaceKind kind, uint32_t sideIndex = 0) {
    std::vector<CadFace> faces;
    enumerateCadFaces(state, &faces);
    for (const CadFace& f : faces) {
        if (f.token.kind == kind && (kind != CadFaceKind::Side || f.token.edgeLocalIndex == sideIndex)) {
            return f.token;
        }
    }
    return CadFaceToken{};
}

double microseconds(std::chrono::steady_clock::time_point t) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t).count();
}

}  // namespace

const char* cadA3PerformanceReport() { return g_performance.c_str(); }

int runCadA3SelfTests(CadA3SelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};

    // -----------------------------------------------------------------------
    // CADA3-16..23: semantic faces, frames, ranges, lineage
    // -----------------------------------------------------------------------
    {
        const CadBodyState box = rectBody(Workplane::XY, 2.0, 1.0, 3.0);
        std::vector<CadFace> faces;
        const CadStatus why = enumerateCadFaces(box, &faces);
        // Two caps + four sides.
        r.check("CADA3_16_a_rectangle_extrusion_has_two_caps_and_four_sides",
                why == CadStatus::Ok && faces.size() == 6
                        && faces[0].token.kind == CadFaceKind::CapPlane
                        && faces[1].token.kind == CadFaceKind::CapFar);
        // The plane cap sits on XY (z=0), outward -Z; the far cap at z=+3, outward +Z.
        r.check("CADA3_16_CapPlane_frame_on_the_sketch_plane_outward_away_from_solid",
                near3(faces[0].origin, Vec3{0, 0, 0}) && near3(faces[0].n, Vec3{0, 0, -1}));
        r.check("CADA3_17_CapFar_frame_at_the_far_end_outward",
                near3(faces[1].origin, Vec3{0, 0, 3}) && near3(faces[1].n, Vec3{0, 0, 1}));
        // Every face frame is right-handed and orthonormal.
        bool framesOk = true;
        for (const CadFace& f : faces) {
            framesOk &= near3(vec3Cross(f.u, f.v), f.n) && nearf(vec3Dot(f.u, f.v), 0.0f)
                        && nearf(vec3Dot(f.u, f.u), 1.0f) && nearf(vec3Dot(f.n, f.n), 1.0f);
        }
        r.check("CADA3_22_23_every_face_frame_is_right_handed_and_orthonormal", framesOk);
        // The four sides carry the rectangle's edge tokens 0..3, all eligible.
        bool sidesOk = faces.size() == 6;
        for (int i = 2; i < 6 && sidesOk; ++i) {
            sidesOk &= faces[i].token.kind == CadFaceKind::Side
                       && faces[i].token.edgeEntityId == 1
                       && faces[i].token.edgeLocalIndex == static_cast<uint32_t>(i - 2)
                       && faces[i].eligible;
        }
        r.check("CADA3_18_rectangle_sides_have_stable_edge_tokens_0_to_3", sidesOk);
        // A side outward normal points away from the box centre.
        const CadFace side0 = faces[2];
        r.check("CADA3_18_side_outward_normal_points_out",
                vec3Dot(side0.n, vec3Sub(side0.origin, Vec3{0, 0, 1.5f})) > 0.0f);

        // Triangle ranges tile the mesh index count and tag every face.
        ConstructionMesh mesh;
        generateCadMesh(box, &mesh);
        std::vector<CadFaceRange> ranges;
        r.check("CADA3_20_face_ranges_tile_the_generated_index_buffer",
                cadFaceRanges(box, &ranges) == CadStatus::Ok && [&] {
                    uint32_t covered = 0;
                    uint32_t cursor = 0;
                    bool contiguous = true;
                    for (const CadFaceRange& rg : ranges) {
                        contiguous &= rg.firstIndex == cursor;
                        cursor += rg.indexCount;
                        covered += rg.indexCount;
                    }
                    return contiguous && covered == mesh.indices.size();
                }());

        // Lineage is stable across a size and a depth edit, and changes when the
        // profile identity changes.
        const uint64_t sig = cadTopologySignature(box);
        r.check("CADA3_31_32_lineage_stable_across_size_and_depth_edits",
                sig != 0 && cadTopologySignature(rectBody(Workplane::XY, 5.0, 0.5, 3.0)) == sig
                        && cadTopologySignature(rectBody(Workplane::XY, 2.0, 1.0, 9.0)) == sig
                        && cadTopologySignature(rectBody(Workplane::XY, 2.0, 1.0, 3.0,
                                                         ExtrudeDirection::AgainstNormal)) == sig);
        r.check("CADA3_31_lineage_changes_when_the_profile_kind_changes",
                cadTopologySignature(circleBody(Workplane::XY, 1.0, 3.0)) != sig);
    }

    // -----------------------------------------------------------------------
    // CADA3-19: a circle's cap is eligible, its side is not
    // -----------------------------------------------------------------------
    {
        const CadBodyState cyl = circleBody(Workplane::XY, 1.0, 2.0);
        std::vector<CadFace> faces;
        enumerateCadFaces(cyl, &faces);
        bool capsEligible = faces.size() >= 2 && faces[0].eligible && faces[1].eligible;
        bool sidesIneligible = true;
        int sideCount = 0;
        for (const CadFace& f : faces) {
            if (f.token.kind == CadFaceKind::Side) {
                ++sideCount;
                sidesIneligible &= !f.eligible;
            }
        }
        r.check("CADA3_19_circle_caps_eligible_cylindrical_side_not",
                capsEligible && sidesIneligible && sideCount == static_cast<int>(kSketchCircleSegments));
        std::vector<CadFaceRange> ranges;
        cadFaceRanges(cyl, &ranges);
        bool rangeSidesIneligible = true;
        for (const CadFaceRange& rg : ranges) {
            if (rg.token.kind == CadFaceKind::Side) rangeSidesIneligible &= !rg.eligible;
        }
        r.check("CADA3_19_circle_side_ranges_report_ineligible", rangeSidesIneligible);
    }

    // -----------------------------------------------------------------------
    // CADA3-16/17/28/33: scene dependency resolution and parent transform
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        // The startup body is a Box; ignore it. Add a world-plane CAD body A.
        SceneObject* a = scene.addCadBody(rectBody(Workplane::XY, 2.0, 2.0, 1.0));
        const ObjectId aId = a->objectId();
        publishSceneObject(*a);

        // A child B on A's far cap (z = +1).
        const CadFaceToken capFar = tokenFor(a->cadOrNull()->state(), CadFaceKind::CapFar);
        CadStatus why = CadStatus::Ok;
        SceneObject* b = scene.addCadBody(
            childOn(a->cadOrNull()->state(), aId, capFar, 1.0, 1.0, 0.5), &why);
        r.check("CADA3_28_a_face_supported_child_is_created",
                b != nullptr && why == CadStatus::Ok && b->isFaceSupportedCad());
        const ObjectId bId = b != nullptr ? b->objectId() : kNoObject;
        if (b != nullptr) publishSceneObject(*b);

        // B's resolved world model places its local origin on A's far cap: at
        // (0,0,1) with the frame's normal +Z.
        Mat4 bModel;
        r.check("CADA3_16_child_resolves_onto_the_producer_cap",
                scene.resolveWorldModel(bId, &bModel)
                        && near3(mat4TransformPoint(bModel, Vec3{0, 0, 0}), Vec3{0, 0, 1}));

        // Move A: B follows, with no state stored on B.
        TransformValues moved;
        moved.positionX = 5.0;
        moved.positionY = -2.0;
        a->transform().setValues(moved);
        r.check("CADA3_33_moving_the_producer_carries_the_dependent",
                scene.resolveWorldModel(bId, &bModel)
                        && near3(mat4TransformPoint(bModel, Vec3{0, 0, 0}), Vec3{5, -2, 1}));

        // Rotate A 90 deg about Z: B's cap origin rotates with it.
        TransformValues rot;
        rot.rotationZ = 90.0;
        a->transform().setValues(rot);
        Vec3 origin = scene.resolveWorldModel(bId, &bModel)
                          ? mat4TransformPoint(bModel, Vec3{0, 0, 0})
                          : Vec3{9, 9, 9};
        r.check("CADA3_33_rotating_the_producer_carries_the_dependent",
                near3(origin, Vec3{0, 0, 1}, 1e-3f));  // the cap centre is on the Z axis

        a->transform().setValues(TransformValues{});  // reset

        // Dependents and Delete refusal.
        r.check("CADA3_38_producer_reports_its_dependents",
                scene.hasCadDependents(aId) && scene.cadDependentsOf(aId).size() == 1
                        && scene.cadDependentsOf(aId)[0] == bId
                        && !scene.hasCadDependents(bId));

        // A size edit on A keeps B's lineage valid (B still resolves).
        bool changed = false;
        applyCadRectangle(*a->cadOrNull(), 4.0, 4.0, &changed);
        r.check("CADA3_31_parent_size_edit_keeps_the_dependent_resolving",
                changed && scene.resolveWorldModel(bId, &bModel));

        // A depth edit moves A's far cap; a cap-supported child follows.
        applyCadExtrude(*a->cadOrNull(), 3.0, ExtrudeDirection::AlongNormal, &changed);
        r.check("CADA3_32_parent_depth_edit_moves_the_cap_supported_child",
                scene.resolveWorldModel(bId, &bModel)
                        && near3(mat4TransformPoint(bModel, Vec3{0, 0, 0}), Vec3{0, 0, 3}));
    }

    // -----------------------------------------------------------------------
    // CADA3-18/31: a side-supported child follows a width edit
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        SceneObject* a = scene.addCadBody(rectBody(Workplane::XY, 2.0, 2.0, 1.0));
        publishSceneObject(*a);
        // Side edge 0 of the rectangle: from (-1,-1) to (1,-1), outward -Y at y=-1.
        const CadFaceToken side0 = tokenFor(a->cadOrNull()->state(), CadFaceKind::Side, 0);
        CadStatus why = CadStatus::Ok;
        SceneObject* b = scene.addCadBody(
            childOn(a->cadOrNull()->state(), a->objectId(), side0, 0.5, 0.5, 0.25), &why);
        r.check("CADA3_18_a_side_supported_child_is_created", b != nullptr && why == CadStatus::Ok);
        Mat4 bModel;
        Vec3 before = scene.resolveWorldModel(b->objectId(), &bModel)
                          ? mat4TransformPoint(bModel, Vec3{0, 0, 0})
                          : Vec3{};
        // The side-0 face origin sits at the middle of that face: y = -1, z = 0.5.
        r.check("CADA3_23_side_frame_origin_on_the_face", nearf(before.y, -1.0f) && nearf(before.z, 0.5f));
        // Grow the rectangle's height: the y=-1 face moves to y=-2. The child follows.
        bool changed = false;
        applyCadRectangle(*a->cadOrNull(), 2.0, 4.0, &changed);
        Vec3 after = scene.resolveWorldModel(b->objectId(), &bModel)
                         ? mat4TransformPoint(bModel, Vec3{0, 0, 0})
                         : Vec3{};
        r.check("CADA3_31_side_supported_child_follows_a_height_edit",
                changed && nearf(after.y, -2.0f) && nearf(after.z, 0.5f));
    }

    // -----------------------------------------------------------------------
    // CADA3-36/37: bad face ref and cycle fail closed
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        SceneObject* a = scene.addCadBody(rectBody(Workplane::XY, 2.0, 2.0, 1.0));
        publishSceneObject(*a);
        // A token that names no face of A (a side index past the rectangle).
        CadFaceToken bad{CadFaceKind::Side, 1, 99};
        CadStatus why = CadStatus::Ok;
        SceneObject* b = scene.addCadBody(
            childOn(a->cadOrNull()->state(), a->objectId(), bad, 1.0, 1.0, 0.5), &why);
        r.check("CADA3_37_a_bad_face_ref_is_refused_no_body_minted",
                b == nullptr && why == CadStatus::ProfileNotFound);

        // A ref to a producer whose lineage no longer matches.
        const CadFaceToken cap = tokenFor(a->cadOrNull()->state(), CadFaceKind::CapFar);
        CadBodyState stale = childOn(a->cadOrNull()->state(), a->objectId(), cap, 1.0, 1.0, 0.5);
        stale.sketch.faceSupport.lineageToken ^= 0x1234u;  // corrupt the token
        SceneObject* c = scene.addCadBody(stale, &why);
        r.check("CADA3_37_a_stale_lineage_ref_is_refused",
                c == nullptr && why == CadStatus::ProfileNotFound);

        // Circle side (ineligible) refused.
        SceneObject* cyl = scene.addCadBody(circleBody(Workplane::XY, 1.0, 1.0));
        publishSceneObject(*cyl);
        CadFaceToken cylSide{CadFaceKind::Side, cyl->cadOrNull()->sketch().entities[0].id(), 0};
        SceneObject* d = scene.addCadBody(
            childOn(cyl->cadOrNull()->state(), cyl->objectId(), cylSide, 1.0, 1.0, 0.5), &why);
        r.check("CADA3_19_a_curved_side_is_not_a_valid_support",
                d == nullptr && why == CadStatus::NotCadBody);
    }

    // -----------------------------------------------------------------------
    // CADA3-35: A -> B -> C chain resolves; performance
    // -----------------------------------------------------------------------
    {
        struct Case {
            const char* name;
            int len;
        };
        Case cases[3] = {{"chain1", 1}, {"chain8", 8}, {"chain32", 32}};
        std::string report;
        char buf[256];
        bool chainOk = true;
        for (const Case& c : cases) {
            ConstructionScene scene;
            SceneObject* prev = scene.addCadBody(rectBody(Workplane::XY, 4.0, 4.0, 1.0));
            publishSceneObject(*prev);
            ObjectId lastId = prev->objectId();
            for (int i = 0; i < c.len; ++i) {
                const CadFaceToken cap = tokenFor(prev->cadOrNull()->state(), CadFaceKind::CapFar);
                CadStatus why = CadStatus::Ok;
                SceneObject* next = scene.addCadBody(
                    childOn(prev->cadOrNull()->state(), lastId, cap, 1.0, 1.0, 1.0), &why);
                chainOk &= next != nullptr;
                if (next == nullptr) break;
                publishSceneObject(*next);
                prev = next;
                lastId = next->objectId();
            }
            Mat4 model;
            const auto t = std::chrono::steady_clock::now();
            const bool resolved = scene.resolveWorldModel(lastId, &model);
            const double us = microseconds(t);
            chainOk &= resolved;
            // A is at z 0..1; each child sits on the previous far cap, so the
            // last body's local origin is at world z = chain length.
            chainOk &= nearf(mat4TransformPoint(model, Vec3{0, 0, 0}).z,
                             static_cast<float>(c.len), 1e-2f);
            std::snprintf(buf, sizeof(buf), "%s resolveUs=%.1f depth=%d; ", c.name, us, c.len);
            report += buf;
        }
        g_performance = report;
        r.check("CADA3_35_dependency_chain_resolves_deterministically", chainOk);
    }

    // -----------------------------------------------------------------------
    // CADA3-40/41/42: CADB v1 byte-identical, v2 face-support round-trip
    // -----------------------------------------------------------------------
    {
        // A world-only CAD project still encodes at v1 and is byte-identical to
        // what it was before CAD-A3 -- the support field is absent.
        ConstructionScene worldOnly;
        SceneObject* a = worldOnly.addCadBody(rectBody(Workplane::XY, 2.0, 2.0, 1.0));
        publishSceneObject(*a);
        const ProjectDocument v1doc = captureProjectDocument(worldOnly, ProjectKind::Construction);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> v1bytes = encodeProjectV1(v1doc, &why);
        // Byte 34 of the file is the first section's version word; the CAD
        // section here is v1. We assert the decode reports a v1 CAD by
        // round-tripping and by the absence of any face support.
        ProjectDocument v1back;
        const bool v1ok = why == ProjectCodecStatus::Ok
                          && decodeProject(v1bytes.data(), v1bytes.size(), &v1back)
                                 == ProjectCodecStatus::Ok
                          && !v1back.cad.bodies.empty()
                          && !v1back.cad.bodies[0].state.sketch.hasFaceSupport
                          && encodeProjectV1(v1back) == v1bytes;
        r.check("CADA3_40_world_only_cad_project_round_trips_at_v1", v1ok);

        // A face-supported project encodes at v2, round-trips bit for bit, and
        // validates its dependency graph.
        ConstructionScene scene;
        SceneObject* pa = scene.addCadBody(rectBody(Workplane::XY, 3.0, 3.0, 1.0));
        publishSceneObject(*pa);
        const CadFaceToken cap = tokenFor(pa->cadOrNull()->state(), CadFaceKind::CapFar);
        SceneObject* pb = scene.addCadBody(
            childOn(pa->cadOrNull()->state(), pa->objectId(), cap, 1.0, 1.0, 0.5));
        publishSceneObject(*pb);
        const ProjectDocument doc = captureProjectDocument(scene, ProjectKind::Construction);
        const std::vector<uint8_t> bytes = encodeProjectV1(doc, &why);
        ProjectDocument back;
        const bool ok = why == ProjectCodecStatus::Ok
                        && decodeProject(bytes.data(), bytes.size(), &back)
                               == ProjectCodecStatus::Ok
                        && validateProjectDocument(back) == ProjectCodecStatus::Ok
                        && back.cad.bodies.size() == 2
                        && back.cad.bodies[1].state.sketch.hasFaceSupport
                        && sameTopoRef(back.cad.bodies[1].state.sketch.faceSupport,
                                       pb->cadOrNull()->sketch().faceSupport)
                        && encodeProjectV1(back) == bytes;
        r.check("CADA3_41_42_face_supported_project_round_trips_at_v2", ok);

        // The fingerprint is deterministic and follows the face-supported
        // scene: two captures agree, and a world-only scene hashes differently.
        r.check("CADA3_43_semantic_fingerprint_is_deterministic_and_support_aware",
                projectSemanticFingerprint(scene, ProjectKind::Construction)
                        == projectSemanticFingerprint(scene, ProjectKind::Construction)
                        && projectSemanticFingerprint(scene, ProjectKind::Construction)
                        != projectSemanticFingerprint(worldOnly, ProjectKind::Construction));

        // Corruption: a document whose child names a nonexistent producer is
        // refused; a cycle is refused.
        ProjectDocument badRef = doc;
        badRef.cad.bodies[1].state.sketch.faceSupport.producerObjectId = 9999;
        r.check("CADA3_37_bad_producer_ref_document_is_refused",
                validateProjectDocument(badRef) != ProjectCodecStatus::Ok);

        ProjectDocument cycle = doc;
        // Make A depend on B and B depend on A: a 2-cycle.
        cycle.cad.bodies[0].state.sketch.hasFaceSupport = true;
        cycle.cad.bodies[0].state.sketch.plane = Workplane::XY;
        cycle.cad.bodies[0].state.sketch.faceSupport.producerObjectId =
            cycle.cad.bodies[1].objectId;
        cycle.cad.bodies[0].state.sketch.faceSupport.producerLocalFeatureId = kCadFeatureId;
        cycle.cad.bodies[0].state.sketch.faceSupport.face =
            CadFaceToken{CadFaceKind::CapFar, 0, 0};
        cycle.cad.bodies[0].state.sketch.faceSupport.lineageToken =
            cadTopologySignature(cycle.cad.bodies[1].state);
        r.check("CADA3_36_dependency_cycle_document_is_refused",
                validateProjectDocument(cycle) != ProjectCodecStatus::Ok);
    }

    // -----------------------------------------------------------------------
    // CADA3-29/30: face-supported creation is one history step
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        SceneObject* a = scene.addCadBody(rectBody(Workplane::XY, 3.0, 3.0, 1.0));
        publishSceneObject(*a);
        const ObjectId aId = a->objectId();
        const CadFaceToken cap = tokenFor(a->cadOrNull()->state(), CadFaceKind::CapFar);

        ObjectId childId = kNoObject;
        {
            ScopedConstructionEdit edit(history);
            SceneObject* b = scene.addCadBody(
                childOn(a->cadOrNull()->state(), aId, cap, 1.0, 1.0, 0.5));
            if (b != nullptr) {
                childId = b->objectId();
                publishSceneObject(*b);
            }
        }
        const CadBodyState childState =
            childId != kNoObject ? scene.findBody(childId)->cadOrNull()->state() : CadBodyState{};
        // The scene starts with the default body, plus A, plus the child = 3.
        r.check("CADA3_28_face_supported_creation_is_one_history_step",
                childId != kNoObject && history.undoDepth() == 1 && scene.bodyCount() == 3
                        && scene.findBody(childId)->isFaceSupportedCad());

        ConstructionRestoreReport report;
        r.check("CADA3_29_undo_removes_only_the_dependent_not_the_producer",
                history.undo(&report) && scene.bodyCount() == 2
                        && scene.findBody(childId) == nullptr && scene.findBody(aId) != nullptr);
        r.check("CADA3_30_redo_restores_the_same_topo_ref",
                history.redo(&report) && scene.bodyCount() == 3
                        && scene.findBody(childId) != nullptr
                        && scene.findBody(childId)->isFaceSupportedCad()
                        && sameTopoRef(scene.findBody(childId)->cadFaceSupportOrNull()
                                           ? *scene.findBody(childId)->cadFaceSupportOrNull()
                                           : TopoRef{},
                                       childState.sketch.faceSupport));
    }

    // -----------------------------------------------------------------------
    // CADA3-08/24/25/26: spatial support picking through the viewport
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        // Replace the default body's effect by adding a CAD box up at the top so
        // faces are unobstructed. A rectangle 2x2 extruded 1 on XY: far cap at z=1.
        SceneObject* a = scene.addCadBody(rectBody(Workplane::XY, 2.0, 2.0, 1.0));
        publishSceneObject(*a);
        CameraController cam;
        cam.setViewport(1000, 1000);
        // A 3/4 view so the far cap and a side are both visible.
        cam.frameWorkplane(0.7f, 0.6f);
        const CameraSnapshot s = cam.snapshot();

        // Project the far-cap centre (0,0,1) to screen, then pick there.
        float fx = 0, fy = 0;
        const bool projected = projectWorldToScreen(s, Vec3{0, 0, 1}, 1000, 1000, &fx, &fy);
        SupportChooser& chooser = supportChooser();
        chooser.begin(/*allowFaces=*/true);
        const ChosenSupport capPick = chooser.select(s, fx, fy, 1000, 1000, scene);
        r.check("CADA3_24_tapping_a_planar_cap_selects_that_face",
                projected && capPick.kind == ChosenSupport::Kind::Face
                        && capPick.faceRef.face.kind == CadFaceKind::CapFar
                        && capPick.faceRef.producerObjectId == a->objectId());

        // A tap on empty space selects nothing (a plane target may still be hit,
        // but far off the body it is the plane, not a face).
        const ChosenSupport miss = chooser.select(s, 5.0f, 5.0f, 1000, 1000, scene);
        r.check("CADA3_26_a_tap_far_off_target_is_world_plane_or_none",
                miss.kind != ChosenSupport::Kind::Face);

        // A curved side of a circle body is never a face support.
        ConstructionScene cscene;
        SceneObject* cyl = cscene.addCadBody(circleBody(Workplane::XY, 1.0, 2.0));
        publishSceneObject(*cyl);
        CameraController cc;
        cc.setViewport(1000, 1000);
        cc.frameWorkplane(0.0f, 0.0f);  // straight at the cylinder side
        const CameraSnapshot cs = cc.snapshot();
        float sx = 0, sy = 0;
        projectWorldToScreen(cs, Vec3{1.0f, 0.0f, 1.0f}, 1000, 1000, &sx, &sy);  // side midpoint
        chooser.begin(true);
        const ChosenSupport sidePick = chooser.select(cs, sx, sy, 1000, 1000, cscene);
        r.check("CADA3_26_a_curved_side_never_selects_as_a_face",
                sidePick.kind != ChosenSupport::Kind::Face);
        chooser.cancel();

        // World-plane picking: with faces off, a tap on the XY plane near the
        // origin selects the XY world plane.
        chooser.begin(/*allowFaces=*/false);
        float ox = 0, oy = 0;
        projectWorldToScreen(s, Vec3{0.5f, 0.5f, 0.0f}, 1000, 1000, &ox, &oy);
        const ChosenSupport planePick = chooser.select(s, ox, oy, 1000, 1000, scene);
        r.check("CADA3_08_tapping_a_world_plane_selects_it",
                planePick.kind == ChosenSupport::Kind::WorldPlane
                        && planePick.plane == Workplane::XY);
        chooser.cancel();
    }

    // -----------------------------------------------------------------------
    // CADA3-12: exact-normal orthographic sketch camera, no pitch clamp
    // -----------------------------------------------------------------------
    {
        CameraController cam;
        cam.setViewport(1000, 1000);
        // A top-down view of the XZ plane: normal +Y, u +X, v -Z. This is the
        // case the old pitch clamp could not express.
        cam.frameSketchView(Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 0, -1}, Vec3{0, 1, 0});
        const CameraSnapshot s = cam.snapshot();
        // Orthographic (proj m[11] == 0), and the eye is straight above along +Y.
        r.check("CADA3_12_sketch_camera_is_orthographic",
                s.projection == ProjectionMode::Orthographic && nearf(s.proj.m[11], 0.0f));
        r.check("CADA3_12_sketch_camera_eye_is_along_the_frame_normal",
                s.eye.x == 0.0f && s.eye.z == 0.0f && s.eye.y > 0.0f);
        // The view direction (row 2 of view, negated) is exactly -normal = -Y.
        const Vec3 viewDir{-s.view.m[2], -s.view.m[6], -s.view.m[10]};
        r.check("CADA3_12_sketch_camera_looks_exactly_down_the_normal",
                near3(viewDir, Vec3{0, -1, 0}));
        // A frame-right world point (origin + u) projects to screen right of a
        // frame-up point; proving u maps right and v up.
        float rx = 0, ry = 0, ux = 0, uy = 0;
        const bool pr = projectWorldToScreen(s, Vec3{1, 0, 0}, 1000, 1000, &rx, &ry);
        const bool pu = projectWorldToScreen(s, Vec3{0, 0, -1}, 1000, 1000, &ux, &uy);
        r.check("CADA3_12_frame_u_is_screen_right_and_v_is_screen_up",
                pr && pu && rx > 500.0f && uy < 500.0f);
    }

    // -----------------------------------------------------------------------
    // CADA3-44/45: adaptive grid step
    // -----------------------------------------------------------------------
    {
        // Nice 1/2/5 x 10^k steps, monotonic with zoom, and bounded.
        const double s1 = adaptiveSketchGridStep(0.001);   // fine zoom
        const double s2 = adaptiveSketchGridStep(0.02);    // mid
        const double s3 = adaptiveSketchGridStep(0.5);     // coarse zoom
        const auto isNice = [](double s) {
            const double p = std::pow(10.0, std::floor(std::log10(s)));
            const double f = s / p;
            return nearf(static_cast<float>(f), 1.0f, 1e-3f) || nearf(static_cast<float>(f), 2.0f, 1e-3f)
                   || nearf(static_cast<float>(f), 5.0f, 1e-3f);
        };
        r.check("CADA3_44_adaptive_grid_steps_are_nice_and_monotonic",
                isNice(s1) && isNice(s2) && isNice(s3) && s1 < s2 && s2 < s3);
        r.check("CADA3_44_adaptive_grid_step_is_bounded",
                s1 >= kSketchGridMinStepMeters && s3 <= kSketchGridMaxStepMeters
                        && adaptiveSketchGridStep(0.0) == kSketchGridSpacingMeters);
        // A face sketch session frames the camera on the producer's face and
        // authors on it: a point typed at (0,0) maps to the face origin in world.
        ConstructionScene scene;
        SceneObject* a = scene.addCadBody(rectBody(Workplane::XY, 2.0, 2.0, 1.0));
        publishSceneObject(*a);
        CadFace cap;
        resolveCadFace(a->cadOrNull()->state(), tokenFor(a->cadOrNull()->state(), CadFaceKind::CapFar),
                       &cap);
        Mat4 world;
        scene.resolveWorldModel(a->objectId(), &world);
        const Mat4 faceWorld = mat4Multiply(world, cadFaceFrameMatrix(cap));
        SketchFrame frame;
        frame.origin = mat4TransformPoint(faceWorld, Vec3{0, 0, 0});
        frame.u = vec3Normalize(mat4TransformDirection(faceWorld, Vec3{1, 0, 0}));
        frame.v = vec3Normalize(mat4TransformDirection(faceWorld, Vec3{0, 1, 0}));
        frame.n = vec3Normalize(mat4TransformDirection(faceWorld, Vec3{0, 0, 1}));
        SketchSession session;
        TopoRef ref;
        ref.producerObjectId = a->objectId();
        ref.producerLocalFeatureId = kCadFeatureId;
        ref.face = tokenFor(a->cadOrNull()->state(), CadFaceKind::CapFar);
        ref.lineageToken = cadTopologySignature(a->cadOrNull()->state());
        r.check("CADA3_45_a_face_session_authors_on_the_producer_frame",
                session.beginOnFace(frame, ref) == CadStatus::Ok
                        && near3(session.sketchToWorld(SketchPoint{0, 0}), Vec3{0, 0, 1})
                        && session.frame().origin.z == 1.0f);
        session.cancel();
    }

    // -----------------------------------------------------------------------
    // CADA3-BOOT: Home is not a project, and the CAD bootstrap's first commit
    // (`APP-H1`). Every scene here is the no-project scene the process starts
    // with; nothing reads process-scoped state.
    // -----------------------------------------------------------------------
    {
        // A no-project scene holds nothing, selects nothing and draws nothing.
        ConstructionScene home{NoProjectTag{}};
        r.check("CADA3_BOOT_01_a_no_project_scene_is_empty_and_selects_nothing",
                !home.hasProject() && home.bodyCount() == 0
                        && home.activeBodyId() == kNoObject && home.snapshot().empty());

        // Reading the active body across Home is COUNTED, answered by a body in
        // no scene, and never dereferences an empty list.
        const uint64_t misuseBefore = ConstructionScene::activeBodyMisuseCount();
        const SceneObject& nobody = home.activeBody();
        r.check("CADA3_BOOT_02_reading_the_active_body_with_no_project_is_counted",
                ConstructionScene::activeBodyMisuseCount() == misuseBefore + 1
                        && nobody.objectId() == kNoObject && home.findBody(kNoObject) == nullptr
                        && !home.hasProject());

        // No project, no document: the capture is empty and the codec refuses
        // to write it, so no `.forge` byte and no checkpoint can describe Home.
        {
            const ProjectDocument empty = captureProjectDocument(home, ProjectKind::Construction);
            ProjectCodecStatus why = ProjectCodecStatus::Ok;
            const std::vector<uint8_t> bytes = encodeProjectV1(empty, &why);
            r.check("CADA3_BOOT_03_no_project_has_no_document_to_write",
                    empty.scene.bodies.empty() && !empty.hasCad && !empty.hasConstruction
                            && bytes.empty() && why != ProjectCodecStatus::Ok
                            && projectSemanticFingerprint(home, ProjectKind::Construction)
                                   == projectSemanticFingerprint(home, ProjectKind::Construction));
        }

        // A sketch session over an EMPTY scene, driven through real touches
        // under the camera the product frames it with, exactly as the shell
        // does. `draw` lays a rectangle; `line` lays a single open line.
        struct Bootstrap {
            SketchSession sketch;
            CameraController camera;
            enum : int { kW = 1000, kH = 1000 };
            bool begin() {
                if (sketch.begin(Workplane::XY) != CadStatus::Ok) return false;
                camera.setViewport(kW, kH);
                const SketchFrame& f = sketch.frame();
                camera.frameSketchView(f.origin, f.u, f.v, f.n);
                return true;
            }
            bool at(TouchAction action, float x, float y) {
                TouchPointer p{7, x, y};
                return sketch.onTouch(action, action == TouchAction::Up ? 7 : -1, &p, 1,
                                      camera.snapshot(), kW, kH);
            }
            bool drag(SketchTool tool, const SketchPoint& from, const SketchPoint& to) {
                sketch.setTool(tool);
                float x0, y0, x1, y1;
                if (!sketch.sketchToScreen(camera.snapshot(), from, kW, kH, &x0, &y0)
                    || !sketch.sketchToScreen(camera.snapshot(), to, kW, kH, &x1, &y1)) {
                    return false;
                }
                bool ok = at(TouchAction::Down, x0, y0);
                for (int step = 1; step <= 4; ++step) {
                    const float t = step / 4.0f;
                    ok &= at(TouchAction::Move, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
                }
                ok &= at(TouchAction::Up, x1, y1);
                return ok;
            }
        };

        const uint64_t misuseAtBootstrapStart = ConstructionScene::activeBodyMisuseCount();

        // The whole first-project journey: plane -> rectangle -> Finish ->
        // depth -> the first commit. Exactly one durable CAD body, active,
        // published, world-supported, at the identity; the sketch is over; the
        // history is empty both ways; the session is Construction.
        {
            ConstructionScene scene{NoProjectTag{}};
            ConstructionHistory history(scene);
            SculptSession session;
            Bootstrap boot;
            const ObjectId expectedId = scene.nextObjectId();
            const bool drawn = boot.begin()
                               && boot.drag(SketchTool::Rectangle, SketchPoint{-1.0, -1.0},
                                            SketchPoint{1.0, 1.0})
                               && boot.sketch.sketch().entities.size() == 1
                               && boot.sketch.finish() == CadStatus::Ok
                               && boot.sketch.setExtrude(1.5, ExtrudeDirection::AlongNormal)
                                      == CadStatus::Ok;
            r.check("CADA3_BOOT_04_the_bootstrap_sketch_reaches_ready_over_an_empty_scene",
                    drawn && !scene.hasProject() && boot.sketch.state() == SketchSessionState::Ready);

            FirstProjectReport report;
            const CadStatus committed =
                commitFirstCadProject(boot.sketch, scene, session, history, &report);
            const SceneObject* body = scene.findBody(report.bodyId);
            r.check("CADA3_BOOT_05_first_extrude_creates_exactly_one_durable_cad_body",
                    committed == CadStatus::Ok && scene.hasProject() && scene.bodyCount() == 1
                            && body != nullptr && body->isCad() && !body->isFaceSupportedCad()
                            && scene.activeBodyId() == report.bodyId
                            && report.bodyId == expectedId
                            && body->transform().isIdentity()
                            && body->meshStore().currentRevision() != kNoMeshRevision
                            && report.revision == body->meshStore().currentRevision()
                            && body->cadOrNull()->state().extrude.depth == 1.5);
            r.check("CADA3_BOOT_06_the_first_project_starts_with_an_empty_history_and_no_sketch",
                    history.undoDepth() == 0 && history.redoDepth() == 0
                            && !history.editInProgress() && !boot.sketch.active()
                            && !session.inSculptMode());
            // What the first project would write is an ordinary world-only CAD
            // project: CADB v1, one body, exactly what New Sketch would have made.
            const ProjectDocument doc = captureProjectDocument(scene, ProjectKind::Construction);
            ProjectCodecStatus why = ProjectCodecStatus::Ok;
            const std::vector<uint8_t> bytes = encodeProjectV1(doc, &why);
            r.check("CADA3_BOOT_07_the_first_project_is_an_ordinary_cad_project_document",
                    why == ProjectCodecStatus::Ok && !bytes.empty() && doc.hasCad
                            && doc.cad.bodies.size() == 1 && !doc.hasConstruction
                            && !doc.cad.bodies[0].state.sketch.hasFaceSupport
                            && doc.scene.activeObjectId == report.bodyId);
            // A second first-commit is refused now that a project is open, and
            // the project is untouched.
            Bootstrap again;
            const bool againReady = again.begin()
                                    && again.drag(SketchTool::Rectangle, SketchPoint{0, 0},
                                                  SketchPoint{1, 1})
                                    && again.sketch.finish() == CadStatus::Ok;
            r.check("CADA3_BOOT_08_the_first_commit_is_refused_while_a_project_is_open",
                    againReady
                            && commitFirstCadProject(again.sketch, scene, session, history)
                                   == CadStatus::NotSketching
                            && scene.bodyCount() == 1 && again.sketch.active());
            again.sketch.cancel();
        }

        // Cancel before the first commit: no project, no body, no history --
        // the scene never learned the sketch existed.
        {
            ConstructionScene scene{NoProjectTag{}};
            ConstructionHistory history(scene);
            Bootstrap boot;
            const bool drawn = boot.begin()
                               && boot.drag(SketchTool::Rectangle, SketchPoint{-1.0, -1.0},
                                            SketchPoint{1.0, 1.0})
                               && boot.sketch.finish() == CadStatus::Ok;
            boot.sketch.cancel();
            r.check("CADA3_BOOT_09_cancel_before_the_first_commit_leaves_no_project",
                    drawn && !scene.hasProject() && scene.bodyCount() == 0
                            && history.undoDepth() == 0 && !boot.sketch.active()
                            && scene.nextObjectId() == kFirstBodyObjectId);
        }

        // An invalid profile -- one open line -- cannot finish, so the commit
        // is refused in the sketch's own vocabulary and no project is created.
        {
            ConstructionScene scene{NoProjectTag{}};
            ConstructionHistory history(scene);
            SculptSession session;
            Bootstrap boot;
            const bool drawn = boot.begin()
                               && boot.drag(SketchTool::Line, SketchPoint{-1.0, 0.0},
                                            SketchPoint{1.0, 0.0})
                               && boot.sketch.sketch().entities.size() == 1;
            const CadStatus finished = boot.sketch.finish();
            const CadStatus committed = commitFirstCadProject(boot.sketch, scene, session, history);
            r.check("CADA3_BOOT_10_an_invalid_profile_creates_no_project",
                    drawn && finished != CadStatus::Ok && committed == CadStatus::NotSketching
                            && !scene.hasProject() && boot.sketch.active()
                            && history.undoDepth() == 0);
            boot.sketch.cancel();
        }

        r.check("CADA3_BOOT_11_the_bootstrap_never_reads_an_active_body",
                ConstructionScene::activeBodyMisuseCount() == misuseAtBootstrapStart);

        // Closing a project empties the scene without rolling the allocator
        // back, so a later project's ids can never collide with the old one's.
        {
            ConstructionScene scene;
            scene.addBody();
            const ObjectId next = scene.nextObjectId();
            scene.closeProject();
            r.check("CADA3_BOOT_12_close_project_empties_the_scene_and_keeps_ids_unique",
                    !scene.hasProject() && scene.bodyCount() == 0
                            && scene.activeBodyId() == kNoObject && scene.nextObjectId() == next
                            && scene.snapshot().empty());
            // And the Sculpt bootstrap's seed -- the first body of a new project
            // made inside the session-initialization bracket -- records nothing.
            ConstructionHistory history(scene);
            history.beginSessionInitialization();
            {
                ScopedConstructionEdit edit(history);
                scene.addBody();
            }
            history.endSessionInitialization();
            r.check("CADA3_BOOT_13_seeding_the_first_body_inside_the_bracket_records_nothing",
                    scene.hasProject() && scene.bodyCount() == 1 && history.undoDepth() == 0
                            && history.redoDepth() == 0 && scene.activeBodyId() == next);
        }
    }


    // -----------------------------------------------------------------------
    // OBJ018A-11: what Duplicate does about a CAD dependency (Stage 018A)
    // -----------------------------------------------------------------------
    //
    // A world-plane CAD Body duplicates like anything else. A FACE-SUPPORTED
    // one is refused BY NAME, because its world placement is derived from its
    // producer's face frame and is not stored: the copy would stand exactly
    // where the original stands, permanently, and `isFaceSupportedCad` is the
    // very predicate that refuses to let the user move it apart. Nothing is
    // retargeted, nothing is detached from its TopoRef, and no dependency is
    // rewritten — the refusal is the whole behaviour.
    {
        ConstructionScene scene(NoProjectTag{});
        ConstructionHistory history(scene);
        const CadBodyState producerState = rectBody(Workplane::XY, 2.0, 1.0, 1.5);
        SceneObject* producer = scene.addCadBody(producerState);
        r.check("obj018a_11_a_world_plane_cad_producer_exists",
                producer != nullptr && producer->isCad() && !producer->isFaceSupportedCad());

        const ObjectId producerId = producer->objectId();
        DuplicateBodyReport report;
        r.check("obj018a_11_a_world_plane_cad_body_duplicates",
                duplicateSceneBody(producerId, scene, history, &report)
                        == BodyCommandStatus::Ok
                    && scene.bodyCount() == 2
                    && report.newBodyId != producerId
                    && scene.findBody(report.newBodyId) != nullptr
                    && scene.findBody(report.newBodyId)->isCad());
        r.check("obj018a_11_the_cad_copy_carries_the_authored_sketch",
                sameCadBodyState(scene.findBody(report.newBodyId)->cadOrNull()->state(),
                                 producerState));
        r.check("obj018a_11_the_cad_copy_regenerated_its_own_mesh",
                scene.findBody(report.newBodyId)->meshStore().currentRevision()
                    != kNoMeshRevision);

        // Now a dependent standing on one of the producer's caps.
        const CadFaceToken token = tokenFor(producerState, CadFaceKind::CapFar);
        SceneObject* follower =
            scene.addCadBody(childOn(producerState, producerId, token, 0.4, 0.4, 0.3));
        r.check("obj018a_11_a_face_supported_dependent_exists",
                follower != nullptr && follower->isFaceSupportedCad());
        if (follower != nullptr) {
            const ObjectId followerId = follower->objectId();
            const size_t bodiesBefore = scene.bodyCount();
            const ObjectId allocatorBefore = scene.nextObjectId();
            const size_t depthBefore = history.undoDepth();

            DuplicateBodyReport refused;
            r.check("obj018a_11_a_face_supported_cad_body_is_refused_by_name",
                    duplicateSceneBody(followerId, scene, history, &refused)
                        == BodyCommandStatus::RefusedFaceSupportedCad);
            // A refusal changes NOTHING: no body, no minted id, no step.
            r.check("obj018a_11_the_refusal_created_nothing",
                    scene.bodyCount() == bodiesBefore
                        && scene.nextObjectId() == allocatorBefore
                        && history.undoDepth() == depthBefore
                        && refused.newBodyId == kNoObject);
            // And the dependency it declined to copy is exactly as it was.
            r.check("obj018a_11_the_dependency_graph_is_intact",
                    scene.validateCadFaceSupport(*scene.findBody(followerId)
                                                      ->cadFaceSupportOrNull())
                            == CadStatus::Ok
                        && scene.cadDependentsOf(producerId).size() == 1);
            // Duplicating the PRODUCER is still allowed while it has a
            // dependent: a single-object Duplicate copies one body and never a
            // graph, so the copy is simply a producer of its own with none.
            DuplicateBodyReport producerCopy;
            r.check("obj018a_11_a_producer_with_dependents_still_duplicates",
                    duplicateSceneBody(producerId, scene, history, &producerCopy)
                            == BodyCommandStatus::Ok
                        && scene.cadDependentsOf(producerId).size() == 1
                        && scene.cadDependentsOf(producerCopy.newBodyId).empty());
        }
    }
    return r.n;
}

}  // namespace forgeshape
