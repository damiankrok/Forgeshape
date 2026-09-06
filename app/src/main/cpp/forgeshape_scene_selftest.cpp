#include "forgeshape_scene_selftest.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_body_commands.h"
#include "forgeshape_body_delete.h"
#include "forgeshape_camera.h"
#include "forgeshape_construction.h"
#include "forgeshape_history.h"
#include "forgeshape_imported_mesh.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sculpt_history.h"
#include "forgeshape_selection.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

struct Recorder {
    SceneSelfTestResult* out;
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

// The version the SCNE section of an encoded project declares.
//
// Read straight out of the bytes rather than inferred, because "did this
// project stay at v1" is precisely the promise that keeps the whole existing
// fixture corpus byte-identical, and inferring it from the document would prove
// nothing about what was written. The header is 28 bytes and every section
// header begins with a 4-byte tag followed by its u16 version; SCNE is written
// first, in canonical order.
uint16_t sceneSectionVersionOf(const std::vector<uint8_t>& bytes) {
    constexpr size_t kTagOffset = kForgeHeaderBytes;
    if (bytes.size() < kTagOffset + 6u
        || std::memcmp(bytes.data() + kTagOffset, kSectionTagScene, 4) != 0) {
        return 0;
    }
    return static_cast<uint16_t>(bytes[kTagOffset + 4]
                                 | (static_cast<uint16_t>(bytes[kTagOffset + 5]) << 8));
}

constexpr int kViewportWidth = 1080;
constexpr int kViewportHeight = 2400;
constexpr float kCentreX = kViewportWidth * 0.5f;
constexpr float kCentreY = kViewportHeight * 0.5f;

// Every test below builds its OWN ConstructionScene rather than touching the
// process-scoped one. That is deliberate and is the Stage 016-R2 lesson applied
// to the scene: a self-test that reads live global state passes or fails
// depending on what a prior UI test or Activity recreation left behind. A local
// scene makes each case input-independent, and it is possible at all only
// because ConstructionScene is an ordinary class with no hidden global state.

// A camera looking down -Y at the world origin, with an explicit `up` that is
// not parallel to the view direction. Used for the flat-sheet sidedness case.
CameraSnapshot cameraAt(const Vec3& eye) {
    CameraSnapshot s{};
    s.eye = eye;
    s.target = Vec3{0.0f, 0.0f, 0.0f};
    s.distance = std::sqrt(vec3Dot(eye, eye));
    s.projection = ProjectionMode::Perspective;
    s.orthoHalfHeightMeters = s.distance * std::tan(kFovYRadians * 0.5f);
    const Vec3 up = (std::fabs(eye.x) < 1e-4f && std::fabs(eye.z) < 1e-4f)
                        ? Vec3{0.0f, 0.0f, 1.0f}
                        : Vec3{0.0f, 1.0f, 0.0f};
    s.view = mat4LookAt(eye, s.target, up);
    const float aspect = static_cast<float>(kViewportWidth) / static_cast<float>(kViewportHeight);
    s.proj = mat4Perspective(kFovYRadians, aspect, kNearPlane, kFarPlane);
    return s;
}

// Publishes a body's Construction Source through the same path the product
// uses, so a test body is in exactly the state a real one would be.
MeshRevision publishBody(SceneObject& body) {
    return publishConstructionObject(body.construction(), body.meshStore());
}

// Moves a body by writing its authoritative transform values.
bool placeBodyAt(SceneObject& body, double x, double y, double z) {
    TransformValues values = body.transform().values();
    values.positionX = x;
    values.positionY = y;
    values.positionZ = z;
    return body.transform().setValues(values) == TransformUpdateStatus::Applied;
}

bool sameVertices(const std::vector<MeshVertex>& a, const std::vector<MeshVertex>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        for (int c = 0; c < 3; ++c) {
            if (a[i].position[c] != b[i].position[c]) return false;
        }
    }
    return true;
}

}  // namespace

int runSceneSelfTests(SceneSelfTestResult* out, int max) {
    Recorder r{out, max};
    if (out == nullptr || max <= 0) {
        return 0;
    }

    // -----------------------------------------------------------------------
    // S17-01 -- startup is still exactly one Body, with the identity the
    // single-object product already used
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        r.check("s17_01_startup_has_exactly_one_body", scene.bodyCount() == 1);
        r.check("s17_01_startup_body_keeps_the_original_object_id",
                scene.bodyAt(0).objectId() == kConstructionBoxObjectId);
        r.check("s17_01_startup_body_is_active",
                scene.activeBodyId() == kConstructionBoxObjectId);
        r.check("s17_01_startup_body_is_the_default_box",
                scene.bodyAt(0).construction().kind() == PrimitiveKind::Box);
        // Identity transform, exactly as the single-object startup had.
        const TransformValues t = scene.bodyAt(0).transform().values();
        r.check("s17_01_startup_body_is_at_identity",
                t.positionX == 0.0 && t.positionY == 0.0 && t.positionZ == 0.0 &&
                    t.rotationX == 0.0 && t.rotationY == 0.0 && t.rotationZ == 0.0);
    }

    // -----------------------------------------------------------------------
    // S17-02 / S17-20 -- Add Body: unique ids, insertion order, determinism
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        const ObjectId a = scene.bodyAt(0).objectId();
        const ObjectId b = scene.addBody().objectId();
        const ObjectId c = scene.addBody().objectId();

        r.check("s17_02_added_bodies_have_unique_ids", a != b && b != c && a != c);
        r.check("s17_02_ids_are_not_collection_indices",
                a != static_cast<ObjectId>(0) && b != static_cast<ObjectId>(1));
        r.check("s17_02_add_appends_in_insertion_order",
                scene.bodyCount() == 3 && scene.bodyAt(0).objectId() == a &&
                    scene.bodyAt(1).objectId() == b && scene.bodyAt(2).objectId() == c);
        r.check("s17_02_add_selects_the_new_body", scene.activeBodyId() == c);
        r.check("s17_02_find_resolves_every_id",
                scene.findBody(a) != nullptr && scene.findBody(b) != nullptr &&
                    scene.findBody(c) != nullptr);
        r.check("s17_02_find_rejects_an_unknown_id", scene.findBody(9999) == nullptr);

        // S17-20: order survives edits, selection changes and publication, and
        // enumerating twice gives the same answer.
        publishBody(scene.bodyAt(0));
        applyPrimitive(scene.bodyAt(1).construction(), scene.bodyAt(1).meshStore(),
                       PrimitiveSpec::forSphere(1.0));
        scene.setActiveBody(a);
        placeBodyAt(scene.bodyAt(2), 3.0, 0.0, 0.0);
        bool orderHeld = scene.bodyCount() == 3 && scene.bodyAt(0).objectId() == a &&
                         scene.bodyAt(1).objectId() == b && scene.bodyAt(2).objectId() == c;
        for (size_t pass = 0; pass < 3; ++pass) {
            if (scene.bodyAt(0).objectId() != a || scene.bodyAt(1).objectId() != b ||
                scene.bodyAt(2).objectId() != c) {
                orderHeld = false;
            }
        }
        r.check("s17_20_enumeration_order_is_deterministic_across_edits", orderHeld);
        r.check("s17_20_selection_change_does_not_reorder", scene.activeBodyId() == a);

        // Ids survive every edit that is not a delete.
        r.check("s17_03_object_id_survives_a_primitive_change",
                scene.bodyAt(1).objectId() == b &&
                    scene.bodyAt(1).construction().kind() == PrimitiveKind::Sphere);
        r.check("s17_04_object_id_survives_a_transform", scene.bodyAt(2).objectId() == c);
    }

    // -----------------------------------------------------------------------
    // S17-03 / S17-04 / S17-19 -- edits reach ONLY the body they target
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        SceneObject& bodyA = scene.bodyAt(0);
        SceneObject& bodyB = scene.addBody();
        publishBody(bodyA);
        publishBody(bodyB);

        const PrimitiveSpec specBBefore = bodyB.construction().spec();
        const TransformValues transformBBefore = bodyB.transform().values();
        const MeshRevision revABefore = bodyA.meshStore().currentRevision();
        const MeshRevision revBBefore = bodyB.meshStore().currentRevision();
        const std::vector<MeshVertex> verticesBBefore =
            bodyB.construction().generateMesh().vertices;

        // --- S17-03: a primitive edit on A ---
        const PrimitiveApplyResult applied = applyPrimitive(
            bodyA.construction(), bodyA.meshStore(), PrimitiveSpec::forSphere(1.0));
        r.check("s17_03_primitive_edit_a_applies",
                applied.status == PrimitiveUpdateStatus::Applied &&
                    bodyA.construction().kind() == PrimitiveKind::Sphere);
        r.check("s17_03_primitive_edit_a_leaves_b_kind_and_parameters_untouched",
                bodyB.construction().kind() == PrimitiveKind::Box &&
                    sameVertices(bodyB.construction().generateMesh().vertices,
                                 verticesBBefore));
        r.check("s17_03_primitive_edit_a_mints_no_revision_in_b",
                bodyB.meshStore().currentRevision() == revBBefore);
        r.check("s17_03_primitive_edit_a_did_mint_a_revision_in_a",
                bodyA.meshStore().currentRevision() != revABefore);

        // --- S17-04: a transform edit on A ---
        const MeshRevision revAAfterPrimitive = bodyA.meshStore().currentRevision();
        r.check("s17_04_transform_a_applies", placeBodyAt(bodyA, 2.0, 0.0, 0.0));
        r.check("s17_04_transform_a_mints_no_revision_anywhere",
                bodyA.meshStore().currentRevision() == revAAfterPrimitive &&
                    bodyB.meshStore().currentRevision() == revBBefore);
        const TransformValues transformBNow = bodyB.transform().values();
        r.check("s17_04_transform_a_leaves_b_placement_untouched",
                transformBNow.positionX == transformBBefore.positionX &&
                    transformBNow.positionY == transformBBefore.positionY &&
                    transformBNow.positionZ == transformBBefore.positionZ);

        // --- Round trip: switching A -> B -> A returns exactly what was set ---
        const PrimitiveSpec specABefore = bodyA.construction().spec();
        const TransformValues transformABefore = bodyA.transform().values();
        scene.setActiveBody(bodyB.objectId());
        r.check("s17_03_selector_only_switch_publishes_nothing",
                bodyA.meshStore().currentRevision() == revAAfterPrimitive &&
                    bodyB.meshStore().currentRevision() == revBBefore);
        scene.setActiveBody(bodyA.objectId());
        const TransformValues transformAAfter = bodyA.transform().values();
        r.check("s17_03_switching_back_round_trips_the_exact_spec",
                scene.activeBody().construction().spec().kind() == specABefore.kind() &&
                    scene.activeBody().construction().kind() == PrimitiveKind::Sphere);
        r.check("s17_04_switching_back_round_trips_the_exact_transform",
                transformAAfter.positionX == transformABefore.positionX &&
                    transformAAfter.positionY == transformABefore.positionY &&
                    transformAAfter.positionZ == transformABefore.positionZ);
        r.check("s17_03_selector_switch_changes_no_object_id",
                bodyA.objectId() == scene.bodyAt(0).objectId() &&
                    bodyB.objectId() == scene.bodyAt(1).objectId());

        // --- S17-19: a REJECTED edit on A leaves the whole scene consistent --
        const MeshRevision revABeforeInvalid = bodyA.meshStore().currentRevision();
        const PrimitiveApplyResult rejected = applyPrimitive(
            bodyA.construction(), bodyA.meshStore(), PrimitiveSpec::forBox(-1.0, 1.0, 0.5));
        r.check("s17_19_invalid_apply_is_rejected",
                rejected.status == PrimitiveUpdateStatus::Rejected);
        r.check("s17_19_invalid_apply_mints_no_revision",
                bodyA.meshStore().currentRevision() == revABeforeInvalid &&
                    bodyB.meshStore().currentRevision() == revBBefore);
        r.check("s17_19_invalid_apply_leaves_a_unchanged",
                bodyA.construction().kind() == PrimitiveKind::Sphere);
        r.check("s17_19_invalid_apply_leaves_b_unchanged",
                bodyB.construction().kind() == PrimitiveKind::Box &&
                    sameVertices(bodyB.construction().generateMesh().vertices,
                                 verticesBBefore));
        r.check("s17_19_invalid_apply_leaves_the_collection_intact",
                scene.bodyCount() == 2 && scene.bodyAt(0).objectId() == bodyA.objectId() &&
                    scene.bodyAt(1).objectId() == bodyB.objectId());
    }

    // -----------------------------------------------------------------------
    // S17-05 / S17-06 / S17-07 / S17-08 -- the snapshot, and per-body caches
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        SceneObject& bodyA = scene.bodyAt(0);
        SceneObject& bodyB = scene.addBody();
        placeBodyAt(bodyB, 4.0, 0.0, 0.0);
        publishBody(bodyA);
        publishBody(bodyB);
        scene.setActiveBody(bodyA.objectId());

        const SceneSnapshot snapshot = scene.snapshot();
        r.check("s17_05_snapshot_carries_every_published_body", snapshot.size() == 2);
        r.check("s17_05_snapshot_is_in_scene_order",
                snapshot.size() == 2 && snapshot[0].objectId == bodyA.objectId() &&
                    snapshot[1].objectId == bodyB.objectId());
        r.check("s17_05_each_item_carries_its_own_mesh_and_id",
                snapshot.size() == 2 && snapshot[0].mesh != nullptr &&
                    snapshot[1].mesh != nullptr &&
                    snapshot[0].mesh->objectId() == bodyA.objectId() &&
                    snapshot[1].mesh->objectId() == bodyB.objectId() &&
                    snapshot[0].mesh.get() != snapshot[1].mesh.get());
        r.check("s17_05_each_item_carries_its_own_model_transform",
                snapshot.size() == 2 &&
                    snapshot[0].model.m[12] != snapshot[1].model.m[12]);

        // S17-08: only the ACTIVE body is highlighted. A single global
        // "something is selected" flag would light both.
        r.check("s17_08_only_the_selected_body_is_highlighted",
                snapshot.size() == 2 && snapshot[0].selected && !snapshot[1].selected);
        scene.setActiveBody(bodyB.objectId());
        const SceneSnapshot afterSwitch = scene.snapshot();
        r.check("s17_08_highlight_follows_the_selection",
                afterSwitch.size() == 2 && !afterSwitch[0].selected && afterSwitch[1].selected);

        // S17-06: render-cache identity is per body. Two caches, each refreshed
        // from its own body's mesh, hold different derived geometry.
        RenderMeshCache cacheA;
        RenderMeshCache cacheB;
        bool failedA = false;
        bool failedB = false;
        cacheA.refresh(*snapshot[0].mesh, SurfaceShading::Smooth, &failedA);
        cacheB.refresh(*snapshot[1].mesh, SurfaceShading::Smooth, &failedB);
        r.check("s17_06_each_body_builds_its_own_render_data",
                !failedA && !failedB && cacheA.valid() && cacheB.valid());
        r.check("s17_06_render_caches_are_independent_objects",
                cacheA.rebuildCount() == 1 && cacheB.rebuildCount() == 1);

        // S17-07: editing A must not make B's cached revision stale. This is the
        // exact comparison the renderer's per-body gate makes, so a body whose
        // published revision has not moved does no rebuild and no upload.
        const MeshRevision cachedRevisionB = snapshot[1].mesh->revision();
        applyPrimitive(bodyA.construction(), bodyA.meshStore(), PrimitiveSpec::forSphere(1.0));
        const SceneSnapshot afterEditA = scene.snapshot();
        r.check("s17_07_edit_a_moves_only_a_revision",
                afterEditA.size() == 2 &&
                    afterEditA[0].mesh->revision() != snapshot[0].mesh->revision() &&
                    afterEditA[1].mesh->revision() == cachedRevisionB);
        r.check("s17_07_b_render_gate_would_skip",
                afterEditA.size() == 2 && afterEditA[1].mesh->revision() == cachedRevisionB &&
                    cacheB.rebuildCount() == 1);
        // And the old snapshot is still valid: it holds shared_ptrs, so A's
        // previous revision is alive even though a newer one exists.
        r.check("s17_05_old_snapshots_stay_valid",
                snapshot[0].mesh != nullptr && snapshot[0].mesh->vertexCount() > 0);
    }

    // -----------------------------------------------------------------------
    // S17-09 / S17-10 / S17-11 / S17-12 -- scene picking
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        SceneObject& bodyA = scene.bodyAt(0);
        SceneObject& bodyB = scene.addBody();
        // Two default boxes, 6 m apart along X, seen from +Z.
        placeBodyAt(bodyA, -3.0, 0.0, 0.0);
        placeBodyAt(bodyB, 3.0, 0.0, 0.0);
        publishBody(bodyA);
        publishBody(bodyB);

        const CameraSnapshot camera = cameraAt(Vec3{0.0f, 0.0f, 12.0f});
        const SceneSnapshot snapshot = scene.snapshot();

        // Projects a world point to the pixel that looks at it, by inverting
        // exactly the relation buildPickRay uses rather than restating a
        // projection formula: a camera-space point p lies on the ray through a
        // pixel when camX = -p.x/p.z and camY = -p.y/p.z, and the pixel's ndc is
        // that scaled by proj.m[0] / proj.m[5]. Deriving it this way means the
        // test cannot pass because it repeated the same sign error twice.
        auto pixelFor = [&](const Vec3& world, float* outX, float* outY) {
            const Vec3 p = mat4TransformPoint(camera.view, world);
            if (p.z >= -1e-4f) return false;  // behind or on the camera plane
            const float camX = -p.x / p.z;
            const float camY = -p.y / p.z;
            const float ndcX = camX * camera.proj.m[0];
            const float ndcY = camY * camera.proj.m[5];
            *outX = (ndcX + 1.0f) * 0.5f * static_cast<float>(kViewportWidth);
            *outY = (ndcY + 1.0f) * 0.5f * static_cast<float>(kViewportHeight);
            return true;
        };

        float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
        const bool projectedA = pixelFor(Vec3{-3.0f, 0.0f, 0.0f}, &ax, &ay);
        const bool projectedB = pixelFor(Vec3{3.0f, 0.0f, 0.0f}, &bx, &by);
        const SceneHit hitA = pickSceneSnapshot(camera, ax, ay, kViewportWidth, kViewportHeight,
                                                snapshot);
        const SceneHit hitB = pickSceneSnapshot(camera, bx, by, kViewportWidth, kViewportHeight,
                                                snapshot);
        r.check("s17_09_pick_over_body_a_returns_a",
                projectedA && hitA.hit && hitA.objectId == bodyA.objectId());
        r.check("s17_09_pick_over_body_b_returns_b",
                projectedB && hitB.hit && hitB.objectId == bodyB.objectId());
        r.check("s17_09_the_two_picks_are_different_bodies",
                hitA.objectId != hitB.objectId);
        // A pixel clear of both bodies is a miss, not an arbitrary body.
        const SceneHit miss = pickSceneSnapshot(camera, 4.0f, 4.0f, kViewportWidth,
                                                kViewportHeight, snapshot);
        r.check("s17_09_a_pixel_over_neither_body_misses", !miss.hit);

        // --- S17-10: overlapping bodies, nearest wins ---
        ConstructionScene depth;
        SceneObject& far_ = depth.bodyAt(0);
        SceneObject& near_ = depth.addBody();
        placeBodyAt(far_, 0.0, 0.0, -4.0);
        placeBodyAt(near_, 0.0, 0.0, 4.0);
        publishBody(far_);
        publishBody(near_);
        const SceneSnapshot depthSnapshot = depth.snapshot();
        const SceneHit nearest = pickSceneSnapshot(camera, kCentreX, kCentreY, kViewportWidth,
                                                   kViewportHeight, depthSnapshot);
        r.check("s17_10_overlapping_bodies_pick_the_nearest",
                nearest.hit && nearest.objectId == near_.objectId());
        // Teeth: the far body IS hittable on its own, so the check above is
        // choosing between two real candidates rather than reporting the only
        // one that could be hit.
        const SceneHit farAlone =
            pickMesh(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                     depthSnapshot[0].mesh, depthSnapshot[0].model, depthSnapshot[0].inverseModel,
                     true);
        r.check("s17_10_the_far_body_was_a_real_candidate",
                farAlone.hit && farAlone.distance > nearest.distance);

        // --- S17-11: sidedness is per active representation, per body ---
        ConstructionScene mixed;
        SceneObject& solid = mixed.bodyAt(0);
        SceneObject& sheet = mixed.addBody();
        applyPrimitive(sheet.construction(), sheet.meshStore(), PrimitiveSpec::forPlane(2.0, 2.0));
        publishBody(solid);
        publishBody(sheet);
        const SceneSnapshot mixedSnapshot = mixed.snapshot();
        r.check("s17_11_the_plane_body_publishes_two_sided",
                mixedSnapshot.size() == 2 && mixedSnapshot[1].mesh->renderBothSides());
        r.check("s17_11_the_solid_body_publishes_single_sided",
                mixedSnapshot.size() == 2 && !mixedSnapshot[0].mesh->renderBothSides());
        // The sheet lies in the XZ plane at the origin; seen from below it must
        // still be picked, and the solid must NOT pick from inside.
        const CameraSnapshot fromBelow = cameraAt(Vec3{0.0f, -6.0f, 0.0f});
        const SceneHit sheetFromBelow =
            pickMesh(fromBelow, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                     mixedSnapshot[1].mesh, mixedSnapshot[1].model, mixedSnapshot[1].inverseModel,
                     !mixedSnapshot[1].mesh->renderBothSides());
        r.check("s17_11_flat_sheet_body_picks_from_behind", sheetFromBelow.hit);
        const CameraSnapshot inside = cameraAt(Vec3{0.0f, 0.2f, 0.0f});
        const SceneHit solidFromInside =
            pickMesh(inside, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                     mixedSnapshot[0].mesh, mixedSnapshot[0].model, mixedSnapshot[0].inverseModel,
                     !mixedSnapshot[0].mesh->renderBothSides());
        r.check("s17_11_solid_body_still_refuses_a_pick_from_inside", !solidFromInside.hit);
        // And one body's kind cannot change another's sidedness: the solid is
        // still single-sided with a Plane sitting beside it in the same scene.
        r.check("s17_11_a_plane_body_does_not_make_its_neighbour_two_sided",
                !mixedSnapshot[0].mesh->renderBothSides());

        // --- S17-12: the Gate P1 shared-edge tolerance still holds in a scene
        // A ray straight down a box face centre lands exactly on the diagonal
        // two triangles share; before kBarycentricEpsilon that missed on arm64.
        ConstructionScene edge;
        publishBody(edge.bodyAt(0));
        const SceneSnapshot edgeSnapshot = edge.snapshot();
        const CameraSnapshot axial = cameraAt(Vec3{0.0f, 0.0f, 8.0f});
        const SceneHit faceCentre = pickSceneSnapshot(axial, kCentreX, kCentreY, kViewportWidth,
                                                      kViewportHeight, edgeSnapshot);
        r.check("s17_12_axial_pick_at_a_face_centre_still_hits", faceCentre.hit);
    }

    // -----------------------------------------------------------------------
    // S17-13..18 -- per-body Freeze / Resume / stale / edits
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        SceneObject& bodyA = scene.bodyAt(0);
        SceneObject& bodyB = scene.addBody();
        // Spheres: 482 vertices spread over the surface, so a brush captures a
        // meaningful set. A box's 8 corners would prove nothing.
        applyPrimitive(bodyA.construction(), bodyA.meshStore(), PrimitiveSpec::forSphere(1.0));
        applyPrimitive(bodyB.construction(), bodyB.meshStore(), PrimitiveSpec::forSphere(1.0));
        publishBody(bodyA);
        publishBody(bodyB);

        // ONE session, re-pointed at each body in turn — exactly what the
        // product does. The session carries the mode, the tool and the brush;
        // each body carries only its own FrozenSculpt.
        SculptSession session;
        session.setRadiusPixels(300.0f);
        const CameraSnapshot camera = cameraAt(Vec3{0.0f, 0.0f, 8.0f});
        const Mat4 identity = mat4Identity();

        // --- S17-13: Freeze A ---
        const ObjectId idABefore = bodyA.objectId();
        const MeshRevision revBBeforeFreeze = bodyB.meshStore().currentRevision();
        MeshValidation why = MeshValidation::Ok;
        session.bindTarget(&bodyA.frozenSculpt());
        const bool frozeA = session.freezeToSculpt(bodyA.construction().generateMesh(),
                                                   bodyA.objectId(), &why);
        r.check("s17_13_freeze_a_succeeds", frozeA);
        r.check("s17_13_freeze_a_preserves_its_object_id",
                bodyA.objectId() == idABefore &&
                    bodyA.frozenSculpt().mesh.objectId() == idABefore);
        r.check("s17_13_freeze_a_leaves_b_unfrozen",
                !bodyB.frozenSculpt().mesh.frozen());
        r.check("s17_13_freeze_a_publishes_nothing_for_b",
                bodyB.meshStore().currentRevision() == revBBeforeFreeze);

        // --- S17-18: a stroke on A cannot reach B ---
        const std::vector<MeshVertex> bSourceBefore =
            bodyB.construction().generateMesh().vertices;
        const bool begunA = session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth,
                                                kViewportHeight, identity, identity);
        r.check("s17_18_a_stroke_begins_on_a", begunA);
        session.updateStroke(kCentreX + 40.0f, kCentreY + 20.0f);
        session.endStroke();
        r.check("s17_18_the_stroke_edited_a", bodyA.frozenSculpt().mesh.hasEdits());
        r.check("s17_18_the_stroke_did_not_touch_b_source",
                sameVertices(bodyB.construction().generateMesh().vertices, bSourceBefore));
        r.check("s17_18_the_stroke_did_not_freeze_b",
                !bodyB.frozenSculpt().mesh.frozen());
        r.check("s17_18_the_stroke_minted_no_revision_in_b",
                bodyB.meshStore().currentRevision() == revBBeforeFreeze);

        // The brush is the SESSION's, not a body's: rebinding must not change it.
        const float radiusBefore = session.radiusPixels();
        session.bindTarget(&bodyB.frozenSculpt());
        r.check("s17_14_switching_body_does_not_change_the_shared_brush",
                session.radiusPixels() == radiusBefore);

        // --- S17-14 / S17-16: independent frozen state and edit predicate ---
        const bool frozeB = session.freezeToSculpt(bodyB.construction().generateMesh(),
                                                   bodyB.objectId(), &why);
        r.check("s17_14_freeze_b_succeeds", frozeB);
        r.check("s17_16_a_has_edits_and_b_does_not",
                bodyA.frozenSculpt().mesh.hasEdits() && !bodyB.frozenSculpt().mesh.hasEdits());
        const SculptRevision revA = bodyA.frozenSculpt().mesh.revision();
        const SculptRevision revB = bodyB.frozenSculpt().mesh.revision();
        r.check("s17_14_frozen_revisions_are_independent",
                revA > revB && revB == kFrozenSculptRevision);
        r.check("s17_14_both_frozen_meshes_keep_their_own_identity",
                bodyA.frozenSculpt().mesh.objectId() == bodyA.objectId() &&
                    bodyB.frozenSculpt().mesh.objectId() == bodyB.objectId());

        // Edit B; A must not move.
        if (session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                identity, identity)) {
            session.updateStroke(kCentreX - 40.0f, kCentreY - 20.0f);
            session.endStroke();
        }
        r.check("s17_14_editing_b_does_not_move_a_revision",
                bodyA.frozenSculpt().mesh.revision() == revA);
        r.check("s17_16_both_now_report_their_own_edits",
                bodyA.frozenSculpt().mesh.hasEdits() && bodyB.frozenSculpt().mesh.hasEdits());

        // --- S17-15: stale-source is per body ---
        r.check("s17_15_neither_body_is_stale_yet",
                !bodyA.frozenSculpt().sourceStale && !bodyB.frozenSculpt().sourceStale);
        applyPrimitive(bodyA.construction(), bodyA.meshStore(),
                       PrimitiveSpec::forBox(1.0, 1.0, 1.0));
        session.bindTarget(&bodyA.frozenSculpt());
        session.markSourceStale();
        r.check("s17_15_changing_a_source_makes_only_a_stale",
                bodyA.frozenSculpt().sourceStale && !bodyB.frozenSculpt().sourceStale);

        // --- S17-17: A -> B -> A round trip restores A's own frozen mesh ---
        const std::vector<MeshVertex> aFrozenVertices = bodyA.frozenSculpt().mesh.vertices();
        const SculptRevision aRevisionAtHandover = bodyA.frozenSculpt().mesh.revision();
        session.enterConstruction();
        scene.setActiveBody(bodyB.objectId());
        session.bindTarget(&bodyB.frozenSculpt());
        r.check("s17_17_b_resumes_its_own_mesh",
                session.enterSculpt() &&
                    session.mesh().objectId() == bodyB.objectId());
        session.enterConstruction();
        scene.setActiveBody(bodyA.objectId());
        session.bindTarget(&bodyA.frozenSculpt());
        const bool resumedA = session.enterSculpt();
        r.check("s17_17_a_resumes_without_refreezing",
                resumedA && bodyA.frozenSculpt().mesh.freezeCount() == 1);
        r.check("s17_17_a_gets_back_its_own_deformed_vertices",
                sameVertices(bodyA.frozenSculpt().mesh.vertices(), aFrozenVertices));
        r.check("s17_17_a_revision_is_unchanged_by_the_round_trip",
                bodyA.frozenSculpt().mesh.revision() == aRevisionAtHandover);
        r.check("s17_17_b_kept_its_own_state_throughout",
                bodyB.frozenSculpt().mesh.frozen() && bodyB.frozenSculpt().mesh.hasEdits() &&
                    bodyB.frozenSculpt().mesh.objectId() == bodyB.objectId());
        r.check("s17_17_the_two_frozen_meshes_are_still_distinct",
                !sameVertices(bodyA.frozenSculpt().mesh.vertices(),
                              bodyB.frozenSculpt().mesh.vertices()));
        r.check("s17_15_a_stale_flag_survived_the_round_trip",
                bodyA.frozenSculpt().sourceStale && !bodyB.frozenSculpt().sourceStale);
    }


    // -----------------------------------------------------------------------
    // OBJ018A: Rename, Show/Hide, Lock/Unlock and Duplicate (Stage 018A)
    // -----------------------------------------------------------------------
    //
    // Every case builds its OWN scene and its OWN history, for the reason the
    // file comment already gives: a suite that reads process-scoped state
    // passes or fails on what something else left behind.

    // OBJ018A-01: rename is one step, and Undo/Redo are exact.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        SceneObject& body = scene.bodyAt(0);
        const std::string before = body.name();

        const BodyCommandStatus ok =
            renameSceneBody(body.objectId(), "gearbox", scene, history);
        r.check("obj018a_01_rename_succeeds",
                ok == BodyCommandStatus::Ok && body.name() == "gearbox");
        r.check("obj018a_01_rename_is_exactly_one_step", history.undoDepth() == 1);

        r.check("obj018a_01_undo_restores_the_previous_name",
                history.undo() && scene.bodyAt(0).name() == before);
        r.check("obj018a_01_redo_reapplies_the_new_name",
                history.redo() && scene.bodyAt(0).name() == "gearbox");
        r.check("obj018a_01_selection_stayed_on_the_same_object",
                scene.activeBodyId() == body.objectId());

        // Renaming to the name it already has is a no-op: `commitEdit` finds
        // nothing different, so no second step is recorded.
        const size_t depthBefore = history.undoDepth();
        renameSceneBody(body.objectId(), "gearbox", scene, history);
        r.check("obj018a_01_renaming_to_the_same_name_records_nothing",
                history.undoDepth() == depthBefore);

        // An empty name is refused rather than replaced by the fallback label.
        r.check("obj018a_01_empty_name_is_refused",
                renameSceneBody(body.objectId(), "   ", scene, history)
                        == BodyCommandStatus::RefusedInvalidName
                    && scene.bodyAt(0).name() == "gearbox"
                    && history.undoDepth() == depthBefore);
        r.check("obj018a_01_unknown_body_is_refused",
                renameSceneBody(99999u, "x", scene, history)
                    == BodyCommandStatus::UnknownBody);
    }

    // OBJ018A-02: a Unicode name survives the rename and the `.forge` round
    // trip byte for byte, including a supplementary character.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        // A 4-byte sequence (U+1F9F2) beside accented Latin and CJK: exactly
        // the shapes JNI's modified UTF-8 would have mangled.
        const std::string name = "\xC3\xA9t\xC3\xA9-\xE9\x83\xA8\xE5\x93\x81-\xF0\x9F\xA7\xB2";
        r.check("obj018a_02_unicode_name_is_accepted",
                renameSceneBody(scene.bodyAt(0).objectId(), name, scene, history)
                        == BodyCommandStatus::Ok
                    && scene.bodyAt(0).name() == name);
        // The UTF-16 boundary is the half a device cannot prove cheaply, so it
        // is proven here: out and back must be the identical byte string.
        const std::vector<uint16_t> units = utf8ToUtf16(name);
        r.check("obj018a_02_utf16_round_trip_is_exact",
                utf16ToUtf8(units.data(), units.size()) == name);

        const ProjectDocument document =
            captureProjectDocument(scene, ProjectKind::Construction);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(document, &why);
        ProjectDocument decoded;
        r.check("obj018a_02_unicode_name_survives_the_forge_round_trip",
                why == ProjectCodecStatus::Ok && !bytes.empty()
                    && decodeProject(bytes.data(), bytes.size(), &decoded)
                           == ProjectCodecStatus::Ok
                    && decoded.scene.bodies.size() == 1
                    && decoded.scene.bodies[0].name == name);
    }

    // OBJ018A-03: a hidden body is in neither the renderer's list nor the
    // picker's, because they are the same list.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        SceneObject& second = scene.addBody();
        publishSceneObject(scene.bodyAt(0));
        publishSceneObject(second);
        const ObjectId hiddenId = second.objectId();

        r.check("obj018a_03_both_bodies_are_in_the_snapshot_to_begin_with",
                scene.snapshot().size() == 2);
        r.check("obj018a_03_hide_succeeds",
                setSceneBodyVisible(hiddenId, false, scene, history)
                        == BodyCommandStatus::Ok
                    && !scene.findBody(hiddenId)->visible());

        const SceneSnapshot afterHide = scene.snapshot();
        bool hiddenPresent = false;
        for (const SceneDrawItem& item : afterHide) {
            hiddenPresent = hiddenPresent || item.objectId == hiddenId;
        }
        r.check("obj018a_03_hidden_body_is_not_in_the_snapshot",
                afterHide.size() == 1 && !hiddenPresent);
        // The snapshot IS what CPU picking casts against, so this one fact is
        // both halves of the contract. Nothing was destroyed to achieve it.
        r.check("obj018a_03_hidden_body_is_still_in_the_scene",
                scene.bodyCount() == 2 && scene.findBody(hiddenId) != nullptr);
        r.check("obj018a_03_hidden_body_kept_its_published_revision",
                scene.findBody(hiddenId)->meshStore().currentRevision() != kNoMeshRevision);
        r.check("obj018a_03_showing_it_again_costs_no_republication",
                setSceneBodyVisible(hiddenId, true, scene, history) == BodyCommandStatus::Ok
                    && scene.snapshot().size() == 2);
    }

    // OBJ018A-04: visibility Undo/Redo, and hiding the ACTIVE body.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        SceneObject& second = scene.addBody();
        publishSceneObject(scene.bodyAt(0));
        publishSceneObject(second);
        const ObjectId activeId = scene.activeBodyId();
        r.check("obj018a_04_the_second_body_is_the_active_one",
                activeId == second.objectId());

        setSceneBodyVisible(activeId, false, scene, history);
        r.check("obj018a_04_hide_is_exactly_one_step", history.undoDepth() == 1);
        // The selection deliberately does NOT move: the row stays selected so
        // Show is one tap away.
        r.check("obj018a_04_hiding_the_active_body_does_not_move_the_selection",
                scene.activeBodyId() == activeId);
        r.check("obj018a_04_undo_shows_it_again",
                history.undo() && scene.findBody(activeId)->visible()
                    && scene.snapshot().size() == 2);
        r.check("obj018a_04_redo_hides_it_again",
                history.redo() && !scene.findBody(activeId)->visible()
                    && scene.snapshot().size() == 1);
        const size_t depth = history.undoDepth();
        setSceneBodyVisible(activeId, false, scene, history);
        r.check("obj018a_04_hiding_an_already_hidden_body_records_nothing",
                history.undoDepth() == depth);
    }

    // OBJ018A-05: lock blocks the transform, and blocks nothing else.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        SceneObject& body = scene.bodyAt(0);
        publishSceneObject(body);
        const ObjectId id = body.objectId();

        TransformValues moved = body.transform().values();
        moved.positionX = 1.25;
        body.transform().setValues(moved);
        const TransformValues placed = body.transform().values();

        r.check("obj018a_05_lock_succeeds",
                setSceneBodyLocked(id, true, scene, history) == BodyCommandStatus::Ok
                    && scene.findBody(id)->locked());
        // Locked stays VISIBLE and stays in the snapshot: what lock removes is
        // the ability to move, not the ability to see or select.
        r.check("obj018a_05_a_locked_body_is_still_drawn_and_picked",
                scene.snapshot().size() == 1);
        r.check("obj018a_05_a_locked_body_keeps_its_placement",
                sameConstructionPlacement(scene.findBody(id)->transform().values(), placed));
        // The domain guard the JNI transform entry point asks. The refusal
        // itself lives at that boundary; what is proven here is that the fact
        // it reads is the one the command wrote.
        r.check("obj018a_05_the_lock_is_readable_where_the_guard_asks_it",
                scene.activeBody().locked());
        r.check("obj018a_05_rename_still_works_on_a_locked_body",
                renameSceneBody(id, "fixture", scene, history) == BodyCommandStatus::Ok
                    && scene.findBody(id)->name() == "fixture");
        r.check("obj018a_05_hide_still_works_on_a_locked_body",
                setSceneBodyVisible(id, false, scene, history) == BodyCommandStatus::Ok);
        r.check("obj018a_05_unlock_restores_the_transformable_state",
                setSceneBodyLocked(id, false, scene, history) == BodyCommandStatus::Ok
                    && !scene.findBody(id)->locked());
    }

    // OBJ018A-06: lock Undo/Redo.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        const ObjectId id = scene.bodyAt(0).objectId();
        setSceneBodyLocked(id, true, scene, history);
        r.check("obj018a_06_lock_is_exactly_one_step",
                history.undoDepth() == 1 && scene.findBody(id)->locked());
        r.check("obj018a_06_undo_unlocks", history.undo() && !scene.findBody(id)->locked());
        r.check("obj018a_06_redo_locks_again", history.redo() && scene.findBody(id)->locked());
        const size_t depth = history.undoDepth();
        setSceneBodyLocked(id, true, scene, history);
        r.check("obj018a_06_locking_an_already_locked_body_records_nothing",
                history.undoDepth() == depth);
    }

    // OBJ018A-07/08: a duplicate gets a FRESH id and an exact copy of the
    // source's own truth.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        SceneObject& source = scene.bodyAt(0);
        source.construction().setPrimitive(PrimitiveSpec::forCylinder(0.4, 0.9));
        TransformValues placement = source.transform().values();
        placement.positionX = 0.5;
        placement.rotationY = 37.0;
        placement.scaleZ = 2.5;
        source.transform().setValues(placement);
        publishSceneObject(source);
        const ObjectId sourceId = source.objectId();
        renameSceneBody(sourceId, "bracket", scene, history);
        setSceneBodyLocked(sourceId, true, scene, history);
        const ObjectId allocatorBefore = scene.nextObjectId();

        DuplicateBodyReport report;
        r.check("obj018a_07_duplicate_succeeds",
                duplicateSceneBody(sourceId, scene, history, &report)
                        == BodyCommandStatus::Ok
                    && scene.bodyCount() == 2);
        r.check("obj018a_07_the_copy_has_a_fresh_object_id",
                report.newBodyId != sourceId && report.newBodyId != kNoObject
                    && report.newBodyId >= allocatorBefore);
        r.check("obj018a_07_the_allocator_only_moved_forward",
                scene.nextObjectId() > allocatorBefore);
        r.check("obj018a_07_the_copy_is_the_active_body",
                scene.activeBodyId() == report.newBodyId);

        const SceneObject* copy = scene.findBody(report.newBodyId);
        r.check("obj018a_08_the_copy_has_the_same_representation",
                copy != nullptr && copy->representation() == source.representation());
        r.check("obj018a_08_the_copy_has_the_same_shape",
                copy != nullptr
                    && sameConstructionShape(copy->construction().captureState(),
                                             scene.findBody(sourceId)
                                                 ->construction().captureState()));
        r.check("obj018a_08_the_copy_has_the_same_placement",
                copy != nullptr
                    && sameConstructionPlacement(copy->transform().values(), placement));
        r.check("obj018a_08_the_copy_carries_the_lock",
                copy != nullptr && copy->locked());
        r.check("obj018a_08_the_copy_has_a_deterministic_copy_name",
                copy != nullptr && copy->name() == "bracket copy");
        r.check("obj018a_08_the_copy_published_its_own_geometry",
                copy != nullptr && copy->meshStore().currentRevision() != kNoMeshRevision);
        r.check("obj018a_08_the_source_is_untouched",
                scene.findBody(sourceId)->name() == "bracket"
                    && sameConstructionPlacement(
                           scene.findBody(sourceId)->transform().values(), placement));
        // A second copy disambiguates rather than colliding.
        DuplicateBodyReport second;
        duplicateSceneBody(sourceId, scene, history, &second);
        r.check("obj018a_08_a_second_copy_gets_the_next_ordinal",
                scene.findBody(second.newBodyId) != nullptr
                    && scene.findBody(second.newBodyId)->name() == "bracket copy 2");
    }

    // OBJ018A-09: Duplicate is one step; Undo removes ONLY the copy and Redo
    // brings back the SAME identity.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        publishSceneObject(scene.bodyAt(0));
        const ObjectId sourceId = scene.bodyAt(0).objectId();

        DuplicateBodyReport report;
        duplicateSceneBody(sourceId, scene, history, &report);
        r.check("obj018a_09_duplicate_is_exactly_one_step",
                history.undoDepth() == 1 && scene.bodyCount() == 2);
        r.check("obj018a_09_undo_removes_only_the_copy",
                history.undo() && scene.bodyCount() == 1
                    && scene.findBody(sourceId) != nullptr
                    && scene.findBody(report.newBodyId) == nullptr);
        r.check("obj018a_09_redo_restores_the_same_identity",
                history.redo() && scene.bodyCount() == 2
                    && scene.findBody(report.newBodyId) != nullptr);
        // The id allocator is never rolled back: the undone creation's id is
        // restored BY NAME and a later creation mints a fresh one.
        const ObjectId afterRedo = scene.nextObjectId();
        scene.addBody();
        r.check("obj018a_09_the_next_creation_does_not_reuse_the_copys_id",
                scene.bodyAt(2).objectId() != report.newBodyId
                    && scene.bodyAt(2).objectId() >= afterRedo);
    }

    // OBJ018A-10: a duplicate carries the retained sculpt GEOMETRY and does not
    // carry the source's sculpt Undo stack.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        SceneObject& source = scene.bodyAt(0);
        publishSceneObject(source);
        ConstructionMesh seed;
        r.check("obj018a_10_the_source_can_be_frozen",
                buildSculptSourceMesh(source, &seed)
                    && source.frozenSculpt().mesh.freezeFrom(seed, source.objectId()));

        // A real deformation, so the copy has something distinctive to carry.
        SculptMesh& sourceMesh = source.frozenSculpt().mesh;
        const Vec3 moved = vec3Add(sourceMesh.vertexPosition(0), Vec3{0.0f, 0.37f, 0.0f});
        sourceMesh.setVertexPosition(0, moved);
        sourceMesh.advanceRevision();
        // One entry on the source's own stack, which the copy must NOT inherit.
        SculptStrokeDelta stroke;
        stroke.vertexIndices = {0u};
        stroke.beforePositions = {Vec3{moved.x, moved.y - 0.37f, moved.z}};
        stroke.afterPositions = {moved};
        stroke.beforeHasEdits = false;
        stroke.afterHasEdits = true;
        source.frozenSculpt().history.record(stroke);
        const uint32_t sourceVertices = sourceMesh.vertexCount();

        DuplicateBodyReport report;
        duplicateSceneBody(source.objectId(), scene, history, &report);
        const SceneObject* copy = scene.findBody(report.newBodyId);
        r.check("obj018a_10_the_copy_owns_a_frozen_sculpt_mesh",
                report.clonedSculptMesh && copy != nullptr
                    && copy->frozenSculpt().mesh.frozen());
        r.check("obj018a_10_the_copy_carries_the_deformed_geometry",
                copy != nullptr && copy->frozenSculpt().mesh.vertexCount() == sourceVertices
                    && std::fabs(copy->frozenSculpt().mesh.vertexPosition(0).y - moved.y) < 1e-5f);
        r.check("obj018a_10_the_copys_mesh_wears_the_copys_identity",
                copy != nullptr && copy->frozenSculpt().mesh.objectId() == report.newBodyId);
        r.check("obj018a_10_the_copy_reports_the_edited_fact",
                copy != nullptr && copy->frozenSculpt().mesh.hasEdits());
        // The whole point of this case: geometry came over, the STACK did not.
        r.check("obj018a_10_the_copy_has_an_empty_sculpt_history",
                copy != nullptr && copy->frozenSculpt().history.undoDepth() == 0
                    && copy->frozenSculpt().history.redoDepth() == 0);
        r.check("obj018a_10_the_sources_sculpt_history_is_untouched",
                source.frozenSculpt().history.undoDepth() == 1);
    }

    // OBJ018A-12: a `.forge` file written before Stage 018A loads visible and
    // unlocked, and stays BYTE-IDENTICAL because it never reaches v2.
    {
        ConstructionScene scene;
        publishSceneObject(scene.bodyAt(0));
        scene.addBody();
        publishSceneObject(scene.bodyAt(1));
        const ProjectDocument plain =
            captureProjectDocument(scene, ProjectKind::Construction);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(plain, &why);
        r.check("obj018a_12_a_plain_project_still_encodes",
                why == ProjectCodecStatus::Ok && !bytes.empty());
        // The section version is the fact: an unhidden, unlocked, unnamed
        // project must stay at SCNE v1, which is what keeps the whole existing
        // fixture corpus byte-identical.
        r.check("obj018a_12_a_plain_project_stays_at_scne_v1",
                sceneSectionVersionOf(bytes) == kSceneSectionVersion);

        ProjectDocument decoded;
        r.check("obj018a_12_a_v1_file_decodes",
                decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                    && decoded.scene.bodies.size() == 2);
        bool allDefault = true;
        for (const ProjectBodyPlacement& body : decoded.scene.bodies) {
            allDefault = allDefault && body.visible && !body.locked && body.name.empty();
        }
        r.check("obj018a_12_a_v1_file_loads_visible_and_unlocked", allDefault);

        // And applying it to a live scene really does leave both defaults.
        ConstructionScene target(NoProjectTag{});
        ConstructionHistory targetHistory(target);
        SculptSession session;
        r.check("obj018a_12_a_v1_file_applies_with_the_defaults",
                loadProjectDocument(decoded, target, session, targetHistory)
                        == ProjectCodecStatus::Ok
                    && target.bodyCount() == 2 && target.bodyAt(0).visible()
                    && !target.bodyAt(0).locked() && target.bodyAt(1).visible()
                    && !target.bodyAt(1).locked());
    }

    // OBJ018A-13: visibility, lock and name round-trip through `.forge`, and
    // the section is promoted to v2 only when one of them needs it.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        publishSceneObject(scene.bodyAt(0));
        SceneObject& second = scene.addBody();
        publishSceneObject(second);
        const ObjectId firstId = scene.bodyAt(0).objectId();
        const ObjectId secondId = second.objectId();
        renameSceneBody(firstId, "housing", scene, history);
        setSceneBodyVisible(secondId, false, scene, history);
        setSceneBodyLocked(firstId, true, scene, history);

        const ProjectDocument document =
            captureProjectDocument(scene, ProjectKind::Construction);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(document, &why);
        r.check("obj018a_13_a_project_with_state_encodes",
                why == ProjectCodecStatus::Ok && !bytes.empty());
        r.check("obj018a_13_it_is_promoted_to_scne_v2",
                sceneSectionVersionOf(bytes) == kSceneSectionVersionV2);

        ProjectDocument decoded;
        r.check("obj018a_13_it_decodes",
                decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok);
        r.check("obj018a_13_the_document_round_trips_exactly",
                sameProjectDocument(document, decoded));

        ConstructionScene target(NoProjectTag{});
        ConstructionHistory targetHistory(target);
        SculptSession session;
        r.check("obj018a_13_the_state_reaches_the_live_scene",
                loadProjectDocument(decoded, target, session, targetHistory)
                        == ProjectCodecStatus::Ok
                    && target.bodyCount() == 2
                    && target.bodyAt(0).name() == "housing"
                    && target.bodyAt(0).locked() && target.bodyAt(0).visible()
                    && !target.bodyAt(1).visible() && !target.bodyAt(1).locked());
        // A load starts a fresh history, exactly as it always did.
        r.check("obj018a_13_a_load_starts_a_fresh_history",
                targetHistory.undoDepth() == 0 && targetHistory.redoDepth() == 0);
        // Re-encoding what came back must reproduce the same bytes: the writer
        // is deterministic and v2 does not break that.
        const std::vector<uint8_t> again =
            encodeProjectV1(captureProjectDocument(target, ProjectKind::Construction), &why);
        r.check("obj018a_13_the_writer_is_still_deterministic",
                why == ProjectCodecStatus::Ok && again == bytes);

        // The fingerprint moves for each of the three, because all three are
        // project truth and each must earn its own checkpoint.
        ConstructionScene fp;
        ConstructionHistory fpHistory(fp);
        publishSceneObject(fp.bodyAt(0));
        const uint64_t base = projectSemanticFingerprint(fp, ProjectKind::Construction);
        renameSceneBody(fp.bodyAt(0).objectId(), "named", fp, fpHistory);
        const uint64_t named = projectSemanticFingerprint(fp, ProjectKind::Construction);
        setSceneBodyVisible(fp.bodyAt(0).objectId(), false, fp, fpHistory);
        const uint64_t hidden = projectSemanticFingerprint(fp, ProjectKind::Construction);
        setSceneBodyLocked(fp.bodyAt(0).objectId(), true, fp, fpHistory);
        const uint64_t locked = projectSemanticFingerprint(fp, ProjectKind::Construction);
        r.check("obj018a_13_each_command_moves_the_fingerprint",
                named != base && hidden != named && locked != hidden);
    }

    // OBJ018A-14: Delete is not changed by any of this.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        publishSceneObject(scene.bodyAt(0));
        SceneObject& second = scene.addBody();
        publishSceneObject(second);
        const ObjectId firstId = scene.bodyAt(0).objectId();
        const ObjectId secondId = second.objectId();

        // A hidden and a locked body delete exactly as an ordinary one does:
        // lock is about MOVING, and this stage does not redefine Delete.
        setSceneBodyVisible(secondId, false, scene, history);
        setSceneBodyLocked(secondId, true, scene, history);
        const size_t depthBefore = history.undoDepth();
        DeleteBodyReport report;
        r.check("obj018a_14_a_hidden_locked_body_still_deletes",
                deleteSceneBody(secondId, scene, history, &report) == DeleteBodyStatus::Ok
                    && scene.bodyCount() == 1);
        r.check("obj018a_14_delete_is_still_exactly_one_step",
                history.undoDepth() == depthBefore + 1);
        r.check("obj018a_14_undo_restores_the_same_object_with_its_state",
                history.undo() && scene.bodyCount() == 2
                    && scene.findBody(secondId) != nullptr
                    && !scene.findBody(secondId)->visible()
                    && scene.findBody(secondId)->locked());
        r.check("obj018a_14_redo_removes_it_again",
                history.redo() && scene.bodyCount() == 1
                    && scene.findBody(secondId) == nullptr);
        // And the last-body refusal is untouched.
        r.check("obj018a_14_the_last_body_is_still_refused",
                deleteSceneBody(firstId, scene, history) == DeleteBodyStatus::RefusedLastBody
                    && scene.bodyCount() == 1);
        r.check("obj018a_14_the_selection_fallback_is_unchanged",
                scene.activeBodyId() == firstId);
    }
    return r.n;
}

}  // namespace forgeshape
