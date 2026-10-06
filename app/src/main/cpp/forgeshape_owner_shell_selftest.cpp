#include "forgeshape_owner_shell_selftest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_kernel.h"
#include "forgeshape_camera.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_input.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_support_chooser.h"

namespace forgeshape {

namespace {

constexpr int kW = 1000;
constexpr int kH = 1000;

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool ok) { out->push_back(ArrangementSelfTestCheck{name, ok}); }
};

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

// A sketch session driven through the gesture path the product uses: each
// entity is drawn by a real drag and then TYPED exact, so the case never
// depends on snapping (the MULTIFACE driver, with a session already begun).
struct Driver {
    SketchSession sketch;
    CameraController camera;

    void frameOnSession() {
        camera.setViewport(kW, kH);
        const SketchFrame& f = sketch.frame();
        camera.frameSketchView(f.origin, f.u, f.v, f.n);
    }
    bool at(TouchAction action, float x, float y) {
        TouchPointer p{7, x, y};
        return sketch.onTouch(action, action == TouchAction::Up ? 7 : -1, &p, 1, camera.snapshot(),
                              kW, kH);
    }
    bool place(SketchTool tool, SketchEntity::Payload exact, SketchPoint from = SketchPoint{0.13, 0.27},
               SketchPoint to = SketchPoint{0.61, 0.83}) {
        sketch.setTool(tool);
        float x0, y0, x1, y1;
        if (!sketch.sketchToScreen(camera.snapshot(), from, kW, kH, &x0, &y0)
            || !sketch.sketchToScreen(camera.snapshot(), to, kW, kH, &x1, &y1)) {
            return false;
        }
        const size_t before = sketch.sketch().entities.size();
        bool ok = at(TouchAction::Down, x0, y0);
        for (int step = 1; step <= 4; ++step) {
            const float t = step / 4.0f;
            ok &= at(TouchAction::Move, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
        }
        ok &= at(TouchAction::Up, x1, y1);
        if (!ok || sketch.sketch().entities.size() != before + 1u) return false;
        return sketch.replaceEntity(sketch.sketch().entities.back().id(), std::move(exact))
               == CadStatus::Ok;
    }
};

SketchRectangle rect(double cu, double cv, double w, double h) {
    SketchRectangle r;
    r.center = SketchPoint{cu, cv};
    r.width = w;
    r.height = h;
    return r;
}

void addRect(CadSketch* sketch, double cu, double cv, double w, double h) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, rect(cu, cv, w, h), &id);
}

ExtrudeFeature oneSide(double depth, ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = 1;
    extrude.depth = depth;
    extrude.direction = direction;
    return extrude;
}

CadFeatureSupport faceSupport(const CadBodyState& state, uint32_t featureId, CadFaceToken token) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face = token;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

CadFaceToken cap(CadFaceKind kind) {
    CadFaceToken token;
    token.kind = kind;
    return token;
}

// The shop body every face case stands on: a 2 x 2 x 1 block on XY (feature
// 1), a 0.6 m square boss Added 0.5 m up from its top at (-0.5, 0.5) (feature
// 2), and a 0.6 m square pocket Cut 0.4 m down into its top at (0.5, -0.5)
// (feature 3) -- so the top, a side, the boss's top, the pocket's floor and the
// pocket's four straight walls are all planar faces of three different
// operations.
CadBodyState shopState(double pocketDepth = 0.4) {
    CadBodyState state;
    addRect(&cadBaseSketch(state), 0.0, 0.0, 2.0, 2.0);
    state.extrude = oneSide(1.0);
    CadSketch boss;
    addRect(&boss, -0.5, 0.5, 0.6, 0.6);
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Add,
                                    faceSupport(state, kCadFeatureId, cap(CadFaceKind::CapFar)),
                                    std::move(boss), oneSide(0.5));
    CadSketch pocket;
    addRect(&pocket, 0.5, -0.5, 0.6, 0.6);
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut,
                                    faceSupport(state, kCadFeatureId, cap(CadFaceKind::CapFar)),
                                    std::move(pocket),
                                    oneSide(pocketDepth, ExtrudeDirection::AgainstNormal));
    return state;
}

double bodyVolume(const ConstructionScene& scene, ObjectId id) {
    const SceneObject* body = scene.findBody(id);
    std::shared_ptr<const CadBodyMesh> mesh;
    if (body == nullptr || body->cadOrNull() == nullptr
        || body->cadOrNull()->regenerated(&mesh) != CadStatus::Ok || mesh == nullptr) {
        return -1.0;
    }
    return mesh->volume;
}

// A camera looking straight down -n at `origin` (orthographic), the frame
// built right-handed from n and a seed u.
CameraSnapshot lookAlong(const Vec3& origin, Vec3 n, Vec3 seedU) {
    n = vec3Normalize(n);
    Vec3 u = vec3Sub(seedU, vec3Scale(n, vec3Dot(seedU, n)));
    u = vec3Normalize(u);
    const Vec3 v = vec3Cross(n, u);
    CameraController camera;
    camera.setViewport(kW, kH);
    camera.frameSketchView(origin, u, v, n);
    return camera.snapshot();
}

struct FacePick {
    bool ok = false;
    ChosenSupport chosen;
};

// The support chooser exactly as New Sketch drives it: begin with faces,
// select at the projected pixel, then the confirm's re-validation.
FacePick pickAt(const ConstructionScene& scene, const CameraSnapshot& camera, const Vec3& world) {
    FacePick out;
    float x = 0.0f;
    float y = 0.0f;
    if (!projectWorldToScreen(camera, world, kW, kH, &x, &y)) return out;
    SupportChooser& chooser = supportChooser();
    chooser.begin(/*allowFaces=*/true);
    const ChosenSupport picked = chooser.select(camera, x, y, kW, kH, scene);
    chooser.cancel();
    if (picked.kind != ChosenSupport::Kind::Face) return out;
    out.ok = refreshChosenSupport(scene, picked, &out.chosen) == CadStatus::Ok;
    return out;
}

// New Sketch on `pick`, a `w` x `w` square at the face's sketch origin, then
// the feature committed into the producer as `operation` by `depth`.
CadStatus sketchOnFaceAndCommit(ConstructionScene& scene, ConstructionHistory& history,
                                const ChosenSupport& pick, CadFeatureOperation operation,
                                double w, double depth, ObjectId* outId) {
    const SceneObject* producer = scene.findBody(pick.faceRef.producerObjectId);
    if (producer == nullptr || producer->cadOrNull() == nullptr) return CadStatus::NotCadBody;
    Driver d;
    CadStatus why = d.sketch.beginOnFace(pick.worldFrame, pick.faceRef, &producer->cadOrNull()->state());
    if (why != CadStatus::Ok) return why;
    d.frameOnSession();
    if (!d.place(SketchTool::Rectangle, rect(0.0, 0.0, w, w))) return CadStatus::ProfileNotFound;
    why = d.sketch.finish();
    if (why != CadStatus::Ok) return why;
    why = d.sketch.setOperation(operation);
    if (why != CadStatus::Ok) return why;
    why = d.sketch.setExtrude(depth, d.sketch.extrude().direction);
    if (why != CadStatus::Ok) return why;
    return d.sketch.commit(scene, history, outId);
}

// ---------------------------------------------------------------------------
// OSS-03..10: New Sketch on the faces of a committed CAD body
// ---------------------------------------------------------------------------

void testFaceSupport(Checks& r) {
    const CadBodyState shop = shopState();
    const double shopVolume = 4.0 + 0.18 - 0.144;

    // Every planar face of the Cut is eligible by its own shape, the eligibility
    // the lineage mixes is the frozen R1 bit, and the floor faces OUT of the
    // material (+z), the -x wall faces into the pocket (+x).
    {
        std::vector<CadFace> cutFaces;
        bool perFace = enumerateCadFeatureFaces(shop, 3u, &cutFaces) == CadStatus::Ok
                       && cutFaces.size() == 6u;
        for (const CadFace& f : cutFaces) perFace = perFace && f.eligible;
        CadFace floor;
        CadFace mouth;
        const bool floorUp = resolveCadFeatureFace(shop, 3u, cap(CadFaceKind::CapFar), &floor)
                                     == CadStatus::Ok
                             && near(floor.n.z, 1.0, 1e-6) && near(floor.origin.z, 0.6, 1e-6);
        const bool mouthDown = resolveCadFeatureFace(shop, 3u, cap(CadFaceKind::CapPlane), &mouth)
                                       == CadStatus::Ok
                               && near(mouth.n.z, -1.0, 1e-6);
        CadBodyMesh mesh;
        const bool regenerated = regenerateCadBody(shop, &mesh) == CadStatus::Ok;
        r.check("OSS_06a_cut_faces_are_eligible_per_face_and_face_out_of_the_material",
                perFace && floorUp && mouthDown && regenerated && near(mesh.volume, shopVolume, 1e-9)
                        && cadMeshCarriesFace(mesh, floor) && !cadMeshCarriesFace(mesh, mouth));
    }

    ConstructionScene scene{NoProjectTag{}};
    ConstructionHistory history(scene);
    SceneObject* body = scene.addCadBody(shop);
    const ObjectId id = body != nullptr ? body->objectId() : kNoObject;
    if (body != nullptr) publishSceneObject(*body);
    const Vec3 down{0.0f, 0.0f, 1.0f};
    const Vec3 xSeed{1.0f, 0.0f, 0.0f};

    // OSS-03: the base's top cap, beside the boss and the pocket.
    {
        const FacePick pick = pickAt(scene, lookAlong(Vec3{0.5f, 0.5f, 1.0f}, down, xSeed),
                                     Vec3{0.5f, 0.5f, 1.0f});
        const bool named = pick.ok && pick.chosen.faceRef.producerLocalFeatureId == kCadFeatureId
                           && pick.chosen.faceRef.face.kind == CadFaceKind::CapFar
                           && near(pick.chosen.worldFrame.n.z, 1.0, 1e-5)
                           && near(pick.chosen.worldFrame.origin.z, 1.0, 1e-5);
        const double before = bodyVolume(scene, id);
        ObjectId out = kNoObject;
        const CadStatus committed =
                named ? sketchOnFaceAndCommit(scene, history, pick.chosen, CadFeatureOperation::Add,
                                              0.2, 0.1, &out)
                      : CadStatus::NotSketching;
        r.check("OSS_03_base_extrude_planar_cap_support_resolves_and_carries_an_add",
                named && committed == CadStatus::Ok && scene.bodyCount() == 1u
                        && near(bodyVolume(scene, id) - before, 0.004, 1e-9)
                        && history.undoDepth() == 1u);
    }

    // OSS-04: the base's straight +x side.
    {
        const FacePick pick = pickAt(scene, lookAlong(Vec3{1.0f, 0.0f, 0.5f}, Vec3{1, 0, 0},
                                                      Vec3{0, 1, 0}),
                                     Vec3{1.0f, 0.2f, 0.5f});
        const bool named = pick.ok && pick.chosen.faceRef.producerLocalFeatureId == kCadFeatureId
                           && pick.chosen.faceRef.face.kind == CadFaceKind::Side
                           && near(pick.chosen.worldFrame.n.x, 1.0, 1e-5);
        const double before = bodyVolume(scene, id);
        ObjectId out = kNoObject;
        const CadStatus committed =
                named ? sketchOnFaceAndCommit(scene, history, pick.chosen, CadFeatureOperation::Add,
                                              0.2, 0.1, &out)
                      : CadStatus::NotSketching;
        r.check("OSS_04_base_straight_side_support_resolves_and_carries_an_add",
                named && committed == CadStatus::Ok
                        && near(bodyVolume(scene, id) - before, 0.004, 1e-9));
    }

    // OSS-05: the Add's own top cap (feature 2, at z = 1.5).
    {
        const FacePick pick = pickAt(scene, lookAlong(Vec3{-0.5f, 0.5f, 1.5f}, down, xSeed),
                                     Vec3{-0.5f, 0.5f, 1.5f});
        const bool named = pick.ok && pick.chosen.faceRef.producerLocalFeatureId == 2u
                           && pick.chosen.faceRef.face.kind == CadFaceKind::CapFar
                           && near(pick.chosen.worldFrame.origin.z, 1.5, 1e-5);
        const double before = bodyVolume(scene, id);
        ObjectId out = kNoObject;
        const CadStatus committed =
                named ? sketchOnFaceAndCommit(scene, history, pick.chosen, CadFeatureOperation::Cut,
                                              0.2, 0.1, &out)
                      : CadStatus::NotSketching;
        r.check("OSS_05_add_created_planar_cap_support_resolves_and_carries_a_cut",
                named && committed == CadStatus::Ok
                        && near(before - bodyVolume(scene, id), 0.004, 1e-9));
    }

    // OSS-06: the Cut's pocket FLOOR (feature 3, z = 0.6), and a feature on it
    // both ways: an Add rising into the pocket, a Cut sinking below it.
    {
        const FacePick pick = pickAt(scene, lookAlong(Vec3{0.5f, -0.5f, 0.6f}, down, xSeed),
                                     Vec3{0.5f, -0.5f, 0.6f});
        const bool named = pick.ok && pick.chosen.faceRef.producerLocalFeatureId == 3u
                           && pick.chosen.faceRef.face.kind == CadFaceKind::CapFar
                           && near(pick.chosen.worldFrame.n.z, 1.0, 1e-5)
                           && near(pick.chosen.worldFrame.origin.z, 0.6, 1e-5)
                           && scene.validateCadFaceSupport(pick.chosen.faceRef) == CadStatus::Ok;
        const double before = bodyVolume(scene, id);
        ObjectId out = kNoObject;
        const CadStatus added =
                named ? sketchOnFaceAndCommit(scene, history, pick.chosen, CadFeatureOperation::Add,
                                              0.2, 0.1, &out)
                      : CadStatus::NotSketching;
        const double afterAdd = bodyVolume(scene, id);
        const FacePick again = pickAt(scene, lookAlong(Vec3{0.65f, -0.65f, 0.6f}, down, xSeed),
                                      Vec3{0.72f, -0.72f, 0.6f});
        const CadStatus cut =
                again.ok ? sketchOnFaceAndCommit(scene, history, again.chosen,
                                                 CadFeatureOperation::Cut, 0.1, 0.2, &out)
                         : CadStatus::NotSketching;
        r.check("OSS_06_cut_pocket_bottom_support_resolves_and_carries_add_and_cut",
                named && added == CadStatus::Ok && near(afterAdd - before, 0.004, 1e-9)
                        && again.ok && again.chosen.faceRef.producerLocalFeatureId == 3u
                        && cut == CadStatus::Ok && near(afterAdd - bodyVolume(scene, id), 0.002, 1e-9));
    }

    // OSS-07: a straight WALL of the Cut's pocket -- the -x wall at x = 0.2,
    // facing +x into the pocket, seen obliquely down through the pocket mouth.
    {
        const FacePick pick = pickAt(scene, lookAlong(Vec3{0.2f, -0.5f, 0.8f}, Vec3{1, 0, 1},
                                                      Vec3{0, 1, 0}),
                                     Vec3{0.2f, -0.5f, 0.8f});
        const bool named = pick.ok && pick.chosen.faceRef.producerLocalFeatureId == 3u
                           && pick.chosen.faceRef.face.kind == CadFaceKind::Side
                           && near(pick.chosen.worldFrame.n.x, 1.0, 1e-5)
                           && near(pick.chosen.worldFrame.origin.x, 0.2, 1e-5);
        const double before = bodyVolume(scene, id);
        ObjectId out = kNoObject;
        const CadStatus committed =
                named ? sketchOnFaceAndCommit(scene, history, pick.chosen, CadFeatureOperation::Add,
                                              0.1, 0.1, &out)
                      : CadStatus::NotSketching;
        r.check("OSS_07_cut_straight_side_support_resolves_and_carries_an_add",
                named && committed == CadStatus::Ok
                        && near(bodyVolume(scene, id) - before, 0.001, 1e-9));
    }

    // OSS-08: a curved face is no planar support -- a circular pocket's wall
    // and a cylinder's side, through the chooser and in the face table.
    {
        CadBodyState round;
        addRect(&cadBaseSketch(round), 0.0, 0.0, 2.0, 2.0);
        round.extrude = oneSide(1.0);
        CadSketch hole;
        SketchCircle circle;
        circle.center = SketchPoint{0.0, 0.0};
        circle.radius = 0.5;
        SketchEntityId cid = kNoSketchEntity;
        addSketchEntity(&hole, circle, &cid);
        appendCadLaterFeatureWithSketch(&round, CadFeatureOperation::Cut,
                                        faceSupport(round, kCadFeatureId, cap(CadFaceKind::CapFar)),
                                        std::move(hole),
                                        oneSide(0.5, ExtrudeDirection::AgainstNormal));
        std::vector<CadFace> faces;
        bool curvedRefused = enumerateCadFeatureFaces(round, 2u, &faces) == CadStatus::Ok;
        for (const CadFace& f : faces) {
            if (f.token.kind == CadFaceKind::Side) curvedRefused = curvedRefused && !f.eligible;
        }
        ConstructionScene roundScene{NoProjectTag{}};
        SceneObject* roundBody = roundScene.addCadBody(round);
        if (roundBody != nullptr) publishSceneObject(*roundBody);
        // Straight down onto the pocket's far wall at (-0.5, 0, 0.75): the
        // oblique view sees it through the mouth.
        const FacePick wall = pickAt(roundScene, lookAlong(Vec3{-0.5f, 0.0f, 0.75f}, Vec3{1, 0, 1},
                                                           Vec3{0, 1, 0}),
                                     Vec3{-0.5f, 0.0f, 0.75f});
        CadFeatureSupport onWall;
        onWall.featureId = 2u;
        onWall.face = CadFaceToken{CadFaceKind::Side, cid, 0u};
        onWall.lineageToken = cadFeatureTopologySignature(round, 2u);
        CadBodyState standing = round;
        CadSketch tiny;
        addRect(&tiny, 0.0, 0.0, 0.05, 0.05);
        appendCadLaterFeatureWithSketch(&standing, CadFeatureOperation::Add, onWall, std::move(tiny),
                                        oneSide(0.05));
        r.check("OSS_08_curved_side_is_refused_as_a_support",
                curvedRefused && roundBody != nullptr && !wall.ok
                        && validateCadBodyState(standing) == CadStatus::FeatureSupportInvalid);
    }

    // OSS-09: a stale support fails exactly and is never retargeted. A
    // structural edit of the Cut (square -> a different square entity set)
    // changes its lineage, so the TopoRef taken on its floor no longer
    // resolves; a size-only edit that drives the pocket THROUGH the block
    // removes the floor, so the feature standing on it is refused
    // SupportFaceLost by name.
    {
        const FacePick floorPick = pickAt(scene, lookAlong(Vec3{0.35f, -0.35f, 0.6f}, down, xSeed),
                                          Vec3{0.3f, -0.3f, 0.6f});
        // On a fresh shop body: the live one now carries features standing on
        // that floor, which the structural edit would (rightly) refuse first.
        CadBodyState structural = shopState();
        CadSketchRecord* cutRecord = cadFeatureSketchRecord(structural, 3u);
        bool edited = cutRecord != nullptr && !cutRecord->sketch.entities.empty();
        if (edited) {
            // The pocket's square becomes a pentagon over the same spot: the
            // floor is still there, but feature 3's face set -- and so its
            // lineage -- is a different one.
            SketchPolyline pentagon;
            pentagon.closed = true;
            for (int k = 0; k < 5; ++k) {
                const double t = 2.0 * 3.14159265358979323846 * k / 5.0;
                pentagon.vertices.push_back(SketchPoint{0.5 + 0.3 * std::cos(t), -0.5 + 0.3 * std::sin(t)});
            }
            cutRecord->sketch = CadSketch{};
            SketchEntityId pid = kNoSketchEntity;
            addSketchEntity(&cutRecord->sketch, pentagon, &pid);
            edited = pid == 1u;
        }
        ConstructionScene staleScene{NoProjectTag{}};
        SceneObject* staleBody = staleScene.addCadBody(structural);
        TopoRef ref = floorPick.chosen.faceRef;
        if (staleBody != nullptr) ref.producerObjectId = staleBody->objectId();
        const bool lineageMoved = cadFeatureTopologySignature(structural, 3u) != ref.lineageToken;
        const CadStatus stale = staleBody != nullptr ? staleScene.validateCadFaceSupport(ref)
                                                     : CadStatus::Ok;

        CadBodyState onFloor = shopState();
        CadSketch tiny;
        addRect(&tiny, 0.0, 0.0, 0.1, 0.1);
        appendCadLaterFeatureWithSketch(&onFloor, CadFeatureOperation::Add,
                                        faceSupport(onFloor, 3u, cap(CadFaceKind::CapFar)),
                                        std::move(tiny), oneSide(0.1));
        CadBodyState through = onFloor;
        for (CadFeature& feature : through.laterFeatures) {
            if (feature.featureId == 3u) feature.extrude.depth = 1.0;
        }
        CadBodyMesh scratch;
        CadRegenerationReport report;
        const CadStatus lost = regenerateCadBody(through, &scratch, &report);
        CadBodyState deeper = onFloor;
        for (CadFeature& feature : deeper.laterFeatures) {
            if (feature.featureId == 3u) feature.extrude.depth = 0.3;
        }
        CadBodyMesh deeperMesh;
        r.check("OSS_09_stale_edited_producer_support_fails_exactly",
                floorPick.ok && edited && lineageMoved && stale != CadStatus::Ok
                        && validateCadBodyState(onFloor) == CadStatus::Ok
                        && lost == CadStatus::SupportFaceLost && report.failedFeatureId == 4u
                        && regenerateCadBody(deeper, &deeperMesh) == CadStatus::Ok
                        && near(deeperMesh.volume, 4.0 + 0.18 - 0.108 + 0.001, 1e-9));
    }

    // OSS-10: a partial Revolve's caps are NOT supports: R1 derives no exact
    // planar frame for a revolved cap (its frame is the sketch's), so nothing
    // pretends one is.
    {
        CadSketch profile;
        addRect(&profile, 1.0, 0.5, 0.5, 1.0);
        SketchLine axis;
        axis.start = SketchPoint{0.0, 0.0};
        axis.end = SketchPoint{0.0, 1.0};
        SketchEntityId axisId = kNoSketchEntity;
        addSketchEntity(&profile, axis, &axisId);
        RevolveFeature revolve;
        revolve.axis = CadSketchEdgeRef{axisId, 0u};
        revolve.angleDegrees = 90.0;
        setRevolveSelectionFrom(&revolve, oneSide(1.0));
        const CadBodyState revolved = makeCadRevolveBodyState(profile, revolve);
        std::vector<CadFace> faces;
        bool capsIneligible = validateCadBodyState(revolved) == CadStatus::Ok
                              && enumerateCadFeatureFaces(revolved, kCadFeatureId, &faces)
                                         == CadStatus::Ok
                              && faces.size() >= 2u;
        for (const CadFace& f : faces) capsIneligible = capsIneligible && !f.eligible;
        r.check("OSS_10_partial_revolve_caps_stay_ineligible_without_an_exact_frame", capsIneligible);
    }
}

// ---------------------------------------------------------------------------
// OSS-11..18: the OWNER-like fill selection
// ---------------------------------------------------------------------------

// The reproducer of `REPRO_BEFORE.md`: a 6 x 4 rectangle, three overlapping
// circles of r 1.0 in a row and one of r 0.8 crossing all three, a spline
// running side to side, and a 2 x 1.2 rectangle across the lower right corner
// -- 23 atomic faces.
bool ownerSketch(Driver& d) {
    if (d.sketch.begin(Workplane::XY) != CadStatus::Ok) return false;
    d.frameOnSession();
    bool ok = d.place(SketchTool::Rectangle, rect(0.0, 0.0, 6.0, 4.0), {0.13, 0.27}, {0.61, 0.83});
    for (const auto& c : {std::pair<SketchPoint, double>{{-1.2, 0.0}, 1.0},
                          std::pair<SketchPoint, double>{{0.0, 0.0}, 1.0},
                          std::pair<SketchPoint, double>{{1.2, 0.0}, 1.0},
                          std::pair<SketchPoint, double>{{0.0, 0.9}, 0.8}}) {
        SketchCircle circle;
        circle.center = c.first;
        circle.radius = c.second;
        ok = ok && d.place(SketchTool::Circle, circle, {0.13, 0.27}, {0.61, 0.83});
    }
    SketchSpline spline;
    spline.points = {{-3.0, -1.0}, {-1.5, 1.2}, {0.4, -1.3}, {1.8, 1.1}, {3.0, 0.6}};
    ok = ok && d.place(SketchTool::Line, spline, {0.13, 0.27}, {0.61, 0.83});
    ok = ok && d.place(SketchTool::Rectangle, rect(1.6, -1.2, 2.0, 1.2), {0.13, 0.27}, {0.61, 0.83});
    return ok && d.sketch.finish() == CadStatus::Ok;
}

// The face whose interior holds `p`, or the face count.
size_t faceAt(const SketchSession& sketch, const SketchPoint& p) {
    const SketchArrangement& a = sketch.arrangement();
    for (size_t i = 0; i < a.faces.size(); ++i) {
        std::vector<PlanarProfileComponent> shape;
        if (mergePlanarFaceSelection(a, {i}, &shape) != CadStatus::Ok || shape.size() != 1u) continue;
        bool inside = sketchPointStrictlyInside(p, shape[0].outer.polygon);
        for (const PlanarProfileLoop& hole : shape[0].holes) {
            inside = inside && !sketchPointStrictlyInside(p, hole.polygon);
        }
        if (inside) return i;
    }
    return a.faces.size();
}

// The Ready tap a finger makes at the face's own interior point.
bool tapFace(Driver& d, size_t face) {
    SketchPoint p;
    float x = 0.0f;
    float y = 0.0f;
    return d.sketch.planarFaceInfo(face, &p, nullptr)
           && d.sketch.sketchToScreen(d.camera.snapshot(), p, kW, kH, &x, &y)
           && d.sketch.toggleRegionAt(d.camera.snapshot(), x, y, kW, kH)
           && d.sketch.lastTapOutcome() == SketchTapOutcome::Resolved;
}

bool clearAll(SketchSession& sketch) {
    for (size_t i = 0; i < sketch.planarFaceCount(); ++i) {
        if (sketch.planarFaceSelected(i) && sketch.togglePlanarFace(i) != CadStatus::Ok) return false;
    }
    return sketch.selectedAreaCount() == 0u;
}

// The prism a single cell extrudes as, measured: the polygon area the
// extruder sees (curves tessellated), never the analytic one.
double cellPolygonArea(const SketchArrangement& a, size_t face) {
    std::vector<PlanarProfileComponent> shape;
    if (mergePlanarFaceSelection(a, {face}, &shape) != CadStatus::Ok || shape.size() != 1u) return -1.0;
    return shape[0].area;
}

// Every boundary half-edge of `faces` (a fragment only one of them bounds),
// counted at its origin node: a node passed twice is an articulation.
size_t articulationNodes(const SketchArrangement& a, const std::vector<size_t>& faces) {
    std::vector<int> owners(a.fragments.size(), 0);
    std::vector<std::vector<uint32_t>> fragmentsOf;
    for (size_t f : faces) {
        std::vector<uint32_t> own;
        std::vector<const FragmentCycle*> cycles{&a.faces[f].ref.outer};
        for (const FragmentCycle& hole : a.faces[f].ref.holes) cycles.push_back(&hole);
        for (const FragmentCycle* cycle : cycles) {
            for (const FragmentRef& ref : *cycle) {
                for (uint32_t k = 0; k < a.fragments.size(); ++k) {
                    const FragmentRef& g = a.fragments[k].ref;
                    if (g.sourceEntityId == ref.sourceEntityId
                        && g.sourceEdgeLocalIndex == ref.sourceEdgeLocalIndex
                        && compareArrangementCut(g.startCut, ref.startCut) == 0
                        && compareArrangementCut(g.endCut, ref.endCut) == 0) {
                        ++owners[k];
                    }
                }
            }
        }
    }
    std::vector<int> degree(a.nodes.size(), 0);
    for (uint32_t k = 0; k < a.fragments.size(); ++k) {
        if (owners[k] == 1) {
            ++degree[a.fragments[k].startNode];
            ++degree[a.fragments[k].endNode];
        }
    }
    size_t count = 0;
    for (int value : degree) count += value > 2 ? 1u : 0u;
    return count;
}

void testOwnerSelection(Checks& r, std::string* perf) {
    Driver d;
    const bool ready = ownerSketch(d) && d.sketch.selectionKind() == CadSelectionKind::PlanarFaces;
    const SketchArrangement& a = d.sketch.arrangement();
    double total = 0.0;
    for (const AtomicPlanarFace& face : a.faces) total += face.area;
    Driver again;
    const bool deterministic = ownerSketch(again) && again.sketch.arrangement().faces.size() == a.faces.size();
    bool sameRefs = deterministic;
    for (size_t i = 0; sameRefs && i < a.faces.size(); ++i) {
        sameRefs = samePlanarFaceRef(a.faces[i].ref, again.sketch.arrangement().faces[i].ref);
    }
    again.sketch.cancel();
    r.check("OSS_11_owner_like_sketch_derives_23_deterministic_atomic_faces",
            ready && a.status == ArrangementStatus::Ok && a.faces.size() == 23u
                    && near(total, 24.0, 1e-9) && sameRefs);

    // Every cell, tapped alone, toggles exactly itself on and off.
    bool independent = ready;
    for (size_t i = 0; independent && i < a.faces.size(); ++i) {
        independent = tapFace(d, i) && d.sketch.selectedAreaCount() == 1u
                      && d.sketch.planarFaceSelected(i) && tapFace(d, i)
                      && d.sketch.selectedAreaCount() == 0u;
    }
    r.check("OSS_12_every_inner_atomic_cell_toggles_independently", independent);

    // The OWNER selection: every cell but the two inner cells that meet at
    // one node -- (0) the large right region and (4) the left lens piece -- by
    // real taps. 21 cells, one edge-connected group, one articulation node.
    const size_t skipA = faceAt(d.sketch, SketchPoint{2.5567, -0.4000});
    const size_t skipB = faceAt(d.sketch, SketchPoint{-0.7087, -0.2154});
    std::vector<size_t> chosen;
    for (size_t i = 0; i < a.faces.size(); ++i) {
        if (i != skipA && i != skipB) chosen.push_back(i);
    }
    bool tapped = ready && skipA < a.faces.size() && skipB < a.faces.size() && skipA != skipB
                  && clearAll(d.sketch);
    for (size_t i : chosen) tapped = tapped && tapFace(d, i);
    bool exactSet = tapped && d.sketch.selectedAreaCount() == 21u;
    for (size_t i = 0; exactSet && i < a.faces.size(); ++i) {
        exactSet = d.sketch.planarFaceSelected(i) == (i != skipA && i != skipB);
    }
    std::vector<std::vector<size_t>> groups;
    const bool oneGroup = partitionSelectedPlanarFacesBySharedBoundary(a, chosen, &groups)
                                  == ArrangementStatus::Ok
                          && groups.size() == 1u && articulationNodes(a, chosen) == 1u;
    const auto t0 = std::chrono::steady_clock::now();
    const CadCandidateEvaluation& candidate = d.sketch.evaluateCandidate();
    const double evalUs = std::chrono::duration<double, std::micro>(
                                  std::chrono::steady_clock::now() - t0)
                                  .count();
    const double depth = d.sketch.extrude().depth;
    r.check("OSS_13_a_21_cell_selection_by_taps_stays_selected_exactly",
            exactSet && d.sketch.lastStatus() == CadStatus::Ok);

    std::vector<PlanarProfileComponent> components;
    const ArrangementStatus merged = mergePlanarFaces(a, chosen, &components);
    bool touching = merged == ArrangementStatus::Ok && components.size() == 1u
                    && !components[0].holes.empty();
    // The split pieces share the articulation node's position: some vertex of
    // one loop of the component coincides with a vertex of another.
    bool coincident = false;
    if (touching) {
        std::vector<const PlanarProfileLoop*> loops{&components[0].outer};
        for (const PlanarProfileLoop& hole : components[0].holes) loops.push_back(&hole);
        for (size_t x = 0; x < loops.size() && !coincident; ++x) {
            for (size_t y = x + 1; y < loops.size() && !coincident; ++y) {
                for (const SketchPoint& p : loops[x]->polygon) {
                    for (const SketchPoint& q : loops[y]->polygon) {
                        coincident = coincident || (p.u == q.u && p.v == q.v);
                    }
                }
            }
        }
    }
    r.check("OSS_15_an_articulated_group_splits_into_one_outer_and_touching_holes_and_extrudes",
            oneGroup && touching && coincident && candidate.status == CadStatus::Ok
                    && candidate.mesh != nullptr && candidate.mesh->components == 1u);

    double sum = 0.0;
    for (size_t i : chosen) sum += cellPolygonArea(a, i);
    r.check("OSS_16_volume_is_the_sum_of_the_chosen_cells_times_depth",
            candidate.status == CadStatus::Ok && candidate.mesh != nullptr
                    && near(candidate.mesh->volume, sum * depth, 1e-9 * sum * depth + 1e-12)
                    && near(components.empty() ? 0.0 : components[0].area, sum, 1e-9 * sum));

    // The same 21 cells chosen in the OPPOSITE order derive bit-identically.
    const double volume = candidate.mesh != nullptr ? candidate.mesh->volume : -1.0;
    const size_t indexCount = candidate.mesh != nullptr ? candidate.mesh->mesh.indices.size() : 0u;
    bool reversed = clearAll(d.sketch);
    for (auto it = chosen.rbegin(); reversed && it != chosen.rend(); ++it) {
        reversed = tapFace(d, *it);
    }
    const CadCandidateEvaluation& twice = d.sketch.evaluateCandidate();
    r.check("OSS_17_component_count_and_solid_are_deterministic_in_any_tap_order",
            reversed && twice.status == CadStatus::Ok && twice.mesh != nullptr
                    && twice.mesh->components == 1u && twice.mesh->volume == volume
                    && twice.mesh->mesh.indices.size() == indexCount);

    // Pure point contact: the two skipped cells alone meet only at a node --
    // two groups, two separate shells, nothing shared.
    bool pair = clearAll(d.sketch) && tapFace(d, skipA) && tapFace(d, skipB);
    const CadCandidateEvaluation& two = d.sketch.evaluateCandidate();
    std::vector<std::vector<size_t>> pairGroups;
    pair = pair && partitionSelectedPlanarFacesBySharedBoundary(a, {skipA, skipB}, &pairGroups)
                           == ArrangementStatus::Ok
           && pairGroups.size() == 2u;
    r.check("OSS_14_pure_point_contact_new_body_is_two_separate_shells",
            pair && two.status == CadStatus::Ok && two.mesh != nullptr && two.mesh->components == 2u
                    && near(two.mesh->volume,
                            (cellPolygonArea(a, skipA) + cellPolygonArea(a, skipB)) * depth, 1e-9));

    // OSS-18: Add and Cut keep their own rules over the articulated selection.
    // The sketch stands on the top of an 8 x 6 x 1 block: a Cut of the 21
    // cells sinks a self-touching pocket through the kernel and removes
    // exactly their area; an Add of them on the block's top unions; the same
    // Add moved off the block is AddDisjoint, the same Cut there
    // CutNoIntersection.
    {
        CadBodyState block;
        addRect(&cadBaseSketch(block), 0.0, 0.0, 8.0, 6.0);
        block.extrude = oneSide(1.0);
        const CadSketch ownerSketchCopy = [&]() {
            Driver e;
            return ownerSketch(e) ? e.sketch.sketch() : CadSketch{};
        }();
        ExtrudeFeature faces = oneSide(0.5, ExtrudeDirection::AgainstNormal);
        faces.profileEntityId = kNoSketchEntity;
        faces.selection = CadSelectionKind::PlanarFaces;
        for (size_t i : chosen) faces.planarFaces.push_back(a.faces[i].ref);
        std::sort(faces.planarFaces.begin(), faces.planarFaces.end(),
                  [](const PlanarFaceRef& x, const PlanarFaceRef& y) {
                      return comparePlanarFaceRef(x, y) < 0;
                  });
        const CadFeatureSupport top = faceSupport(block, kCadFeatureId, cap(CadFaceKind::CapFar));
        CadBodyState cut = block;
        appendCadLaterFeatureWithSketch(&cut, CadFeatureOperation::Cut, top, ownerSketchCopy, faces);
        ExtrudeFeature up = faces;
        up.direction = ExtrudeDirection::AlongNormal;
        CadBodyState add = block;
        appendCadLaterFeatureWithSketch(&add, CadFeatureOperation::Add, top, ownerSketchCopy, up);
        // The same selection pointed the wrong way for its operation: an Add
        // sunk wholly inside the block adds nothing, a Cut raised above it
        // removes nothing -- each refused by its own name, as before.
        CadBodyState inside = block;
        appendCadLaterFeatureWithSketch(&inside, CadFeatureOperation::Add, top, ownerSketchCopy,
                                        faces);
        CadBodyState above = block;
        appendCadLaterFeatureWithSketch(&above, CadFeatureOperation::Cut, top, ownerSketchCopy, up);
        CadBodyMesh cutMesh;
        CadBodyMesh addMesh;
        CadBodyMesh scratch;
        CadRegenerationReport disjointReport;
        CadRegenerationReport missReport;
        const CadStatus cutWhy = regenerateCadBody(cut, &cutMesh);
        const CadStatus addWhy = regenerateCadBody(add, &addMesh);
        const CadStatus disjointWhy = regenerateCadBody(inside, &scratch, &disjointReport);
        const CadStatus missWhy = regenerateCadBody(above, &scratch, &missReport);
        r.check("OSS_18_add_and_cut_keep_their_rules_over_an_articulated_selection",
                !ownerSketchCopy.entities.empty() && cutWhy == CadStatus::Ok
                        && near(cutMesh.volume, 48.0 - sum * 0.5, 1e-6) && addWhy == CadStatus::Ok
                        && near(addMesh.volume, 48.0 + sum * 0.5, 1e-6)
                        && disjointWhy == CadStatus::AddNoEffect && disjointReport.failedFeatureId == 2u
                        && missWhy == CadStatus::CutNoIntersection && missReport.failedFeatureId == 2u);
    }

    char line[96];
    std::snprintf(line, sizeof(line), "oss_owner21_eval_us=%.0f oss_owner_faces=%zu", evalUs,
                  a.faces.size());
    *perf = line;
    d.sketch.cancel();
}

}  // namespace

void runOwnerShellSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    if (out == nullptr) return;
    Checks r{out};
    std::string perf;
    testFaceSupport(r);
    testOwnerSelection(r, &perf);
    if (performance != nullptr) *performance = perf;
}

}  // namespace forgeshape
