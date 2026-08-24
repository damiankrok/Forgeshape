#include "forgeshape_scene_selftest.h"

#include <cmath>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
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

    return r.n;
}

}  // namespace forgeshape
