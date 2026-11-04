#include "forgeshape_editor_core.h"

#include <algorithm>
#include <utility>

#include "forgeshape_body_commands.h"
#include "forgeshape_cad_extrude_tool.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_project_bootstrap.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_support_chooser.h"
#include "forgeshape_surface_authoring.h"

namespace forgeshape {

CameraController& editorCamera() {
    static CameraController camera;
    return camera;
}

SelectionController& editorSelection() {
    static SelectionController selection;
    return selection;
}

std::mutex& editorStateMutex() {
    static std::mutex mutex;
    return mutex;
}

bool editorActiveBodyDimensions(LocalBounds* outBounds, BodyDimensions* outDimensions) {
    const ConstructionScene& scene = constructionScene();
    if (!scene.hasProject()) {
        return false;
    }
    return sceneBodyDimensions(scene.activeBodyId(), scene, outBounds, outDimensions);
}

bool editorActiveBodyDimensionsEditable(LocalBounds* outBounds, BodyDimensions* outDimensions) {
    const ConstructionScene& scene = constructionScene();
    return editorActiveBodyDimensions(outBounds, outDimensions) && scene.hasProject()
        && bodySizeEditable(scene.activeBody()) && !sculptSession().inSculptMode();
}

GizmoActivation editorSetGizmoActive(bool active) {
    const ConstructionScene& scene = constructionScene();
    GizmoSession& gizmo = gizmoSession();
    if (active && sculptSession().inSculptMode()) {
        gizmo.setActive(false);
        return GizmoActivation::RefusedInSculpt;
    }
    if (active && scene.hasProject() && scene.activeBody().locked()) {
        gizmo.setActive(false);
        return GizmoActivation::RefusedLocked;
    }
    if (active && scene.hasProject() && scene.activeBody().isFaceSupportedCad()) {
        gizmo.setActive(false);
        return GizmoActivation::RefusedDerived;
    }
    gizmo.setActive(active);
    return gizmo.active() ? GizmoActivation::Active : GizmoActivation::Inactive;
}

namespace {

// ---------------------------------------------------------------------------
// The CAD operation preview (`CAD-VERTICAL-SLICE-R1`)
// ---------------------------------------------------------------------------
//
// What an Add, a Cut or a New Body WILL make, drawn before it is committed. It
// is the sketch session's own candidate evaluation -- the same regeneration
// the commit applies -- handed to the RENDERER'S copy of the scene list only:
// `viewSceneSnapshot()` stays exactly what picking, saving and export read, so
// a preview can never be picked, saved, checkpointed or exported.
//
// Latest-only by construction: the evaluation is keyed by the candidate
// revision and this runs once per frame, so a drag that moves forty times
// between two frames costs at most one regeneration per frame and never
// queues one per MotionEvent. The RuntimeMesh is rebuilt only when the
// evaluation's revision changes, under a revision space of its own (high bit)
// that no body's store can reach, so the renderer's revision-only upload gate
// re-uploads on every change -- and again when the real mesh returns.
constexpr ObjectId kCadNewBodyPreviewRenderKey = 1ull << 61;
constexpr MeshRevision kCadPreviewRevisionBase = 1ull << 62;

struct CadPreviewCache {
    uint64_t evaluationRevision = 0;
    ObjectId key = kNoObject;
    RuntimeMeshPtr mesh;
    MeshRevision nextRevision = kCadPreviewRevisionBase;
};

CadPreviewCache& cadPreviewCache() {
    static CadPreviewCache cache;
    return cache;
}

}  // namespace

void applyCadOperationPreview(SceneSnapshot* list) {
    SketchSession& session = sketchSession();
    if (list == nullptr || session.state() != SketchSessionState::Ready) {
        return;
    }
    const CadCandidateEvaluation& evaluation = session.evaluateCandidate();
    if (evaluation.status != CadStatus::Ok || evaluation.mesh == nullptr) {
        // An invalid candidate shows NO result -- never the last valid one --
        // and the HUD names why.
        return;
    }
    const ObjectId key = evaluation.targetBodyId != kNoObject
                                             ? evaluation.targetBodyId
                                             : kCadNewBodyPreviewRenderKey;
    CadPreviewCache& cache = cadPreviewCache();
    if (cache.mesh == nullptr || cache.evaluationRevision != evaluation.revision
        || cache.key != key) {
        const ConstructionMesh& m = evaluation.mesh->mesh;
        RuntimeMeshPtr runtime = createRuntimeMesh(
                key, cache.nextRevision++, m.vertices.data(),
                static_cast<uint32_t>(m.vertices.size()), m.indices.data(),
                static_cast<uint32_t>(m.indices.size()), nullptr, m.renderBothSides);
        if (runtime == nullptr) {
            return;
        }
        cache.mesh = std::move(runtime);
        cache.evaluationRevision = evaluation.revision;
        cache.key = key;
    }
    float tint[4];
    cadOperationPreviewTint(evaluation.operation, tint);
    if (evaluation.targetBodyId != kNoObject) {
        // The target body is drawn AS its candidate: same key, same model.
        for (SceneDrawItem& item : *list) {
            if (item.objectId == evaluation.targetBodyId) {
                item.mesh = cache.mesh;
                std::copy(tint, tint + 4, item.previewTint);
            }
        }
        return;
    }
    Mat4 model;
    if (!session.candidateWorldModel(constructionScene(), &model)) {
        return;
    }
    SceneDrawItem item;
    item.objectId = key;
    item.mesh = cache.mesh;
    item.model = model;
    if (!mat4AffineInverse(model, &item.inverseModel)) {
        return;
    }
    item.normalModel = mat4NormalMatrix(model);
    item.selected = false;
    std::copy(tint, tint + 4, item.previewTint);
    list->push_back(std::move(item));
}


bool editorSceneBounds(Vec3* outMin, Vec3* outMax) {
    return editorBodyBounds(kNoObject, outMin, outMax);
}

bool editorBodyBounds(ObjectId only, Vec3* outMin, Vec3* outMax) {
    bool any = false;
    Vec3 lo{0.0f, 0.0f, 0.0f}, hi{0.0f, 0.0f, 0.0f};
    for (const SceneDrawItem& item : viewSceneSnapshot()) {
        if (!item.mesh) continue;
        if (only != kNoObject && item.objectId != only) continue;
        const MeshVertex* vertices = item.mesh->vertices();
        for (uint32_t i = 0; i < item.mesh->vertexCount(); ++i) {
            const Vec3 local{vertices[i].position[0], vertices[i].position[1],
                             vertices[i].position[2]};
            const Vec3 p = mat4TransformPoint(item.model, local);
            if (!any) {
                lo = hi = p;
                any = true;
            } else {
                lo = Vec3{std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
                hi = Vec3{std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
            }
        }
    }
    if (any && outMin != nullptr && outMax != nullptr) {
        *outMin = lo;
        *outMax = hi;
    }
    return any;
}

bool editorFitAll() {
    Vec3 lo, hi;
    return editorSceneBounds(&lo, &hi) && editorCamera().frameBounds(lo, hi);
}

bool editorFitBody(ObjectId id) {
    Vec3 lo, hi;
    return editorBodyBounds(id, &lo, &hi) && editorCamera().frameBounds(lo, hi);
}

// ---------------------------------------------------------------------------
// The sketch acts both shells take (`DESKTOP-R2`)
// ---------------------------------------------------------------------------

namespace {

// The view a sketch borrows (`CAD-R0-A1A2`). A sketch is drawn through a view
// straight down its plane's normal; that view is PRESENTATION -- the sketch
// stores no camera -- and the user's own viewpoint (pose, projection and
// orthographic span) is kept here for exactly as long as the sketch is open.
// Guarded by editorStateMutex() like the camera it describes.
struct SketchViewState {
    CameraController::Pose savedPose;
    bool poseSaved = false;
    // Ready entered with no region chosen: the first region takes the tilt.
    bool featureViewPending = false;
};

SketchViewState& sketchViewState() {
    static SketchViewState state;
    return state;
}

}  // namespace

// Frames the camera EXACTLY normal to the active sketch -- no pitch-clamp
// approximation, and correct for a face frame at any orientation (`CAD-A3`
// B3) -- through the session's VIEW frame, so the orientation navigator's
// flip and roll are what the user actually sees. A world-plane and a face
// sketch take the same path.
void editorBeginSketchView() {
    SketchViewState& view = sketchViewState();
    CameraController& camera = editorCamera();
    if (!view.poseSaved) {
        view.savedPose = camera.capturePose();
        view.poseSaved = true;
    }
    frameCameraOnSketchView(camera, sketchSession());
}

void editorEndSketchView() {
    SketchViewState& view = sketchViewState();
    if (view.poseSaved) {
        editorCamera().restorePose(view.savedPose);
        view.poseSaved = false;
    }
}

void editorForgetSketchView() {
    sketchViewState().poseSaved = false;
}

// The sketch's view and the extrusion's are two presentations of one authored
// truth, and Finish Sketch is where the second begins: the first is aimed
// EXACTLY along the support normal, which is the extrusion axis, so from it
// the arrow has no screen extent and no axial drag can be resolved
// (`OQ-CAD-UX-01`). The saved pre-sketch pose is deliberately NOT consumed:
// it stays the view a cancel or a commit gives back.
CadFeatureViewSource editorBeginExtrudeFeatureView() {
    CadExtrudeAnchors anchors;
    if (!sketchSession().extrudeAnchors(&anchors)) {
        return CadFeatureViewSource::Unavailable;
    }
    SketchViewState& view = sketchViewState();
    CameraController& camera = editorCamera();
    CameraController::Pose preview;
    const CadFeatureViewSource source =
            cadFeatureViewPose(camera.capturePose(), view.poseSaved ? &view.savedPose : nullptr,
                               sketchSession().frame(), anchors, &preview);
    if (source != CadFeatureViewSource::Unavailable) {
        // restorePose is the one clamped installer, and it leaves the sketch
        // view: from here orbit, pan and pinch mean what they mean anywhere.
        camera.restorePose(preview);
    }
    return source;
}

void editorBeginRevolveFeatureView() {
    CameraController& camera = editorCamera();
    CameraController::Pose pose;
    if (sketchSession().revolveViewPose(camera.capturePose(), &pose)) {
        camera.restorePose(pose);
    }
}

void editorSetFeatureViewPending(bool pending) {
    sketchViewState().featureViewPending = pending;
}

void editorBeginPendingExtrudeFeatureView() {
    SketchViewState& view = sketchViewState();
    if (!view.featureViewPending || sketchSession().state() != SketchSessionState::Ready) {
        return;
    }
    if (editorBeginExtrudeFeatureView() != CadFeatureViewSource::Unavailable) {
        view.featureViewPending = false;
    }
}

CadStatus editorSketchBegin(Workplane plane) {
    CadStatus status = CadStatus::Ok;
    if (sculptSession().inSculptMode()) {
        status = CadStatus::NotSketching;
    } else if (constructionHistory().editInProgress()) {
        status = CadStatus::RefusedEditInProgress;
    } else {
        // Every CAD begin ends any Surface purpose a dropped sketch left.
        surfaceSketchPurpose() = SurfaceSketchPurpose{};
        status = sketchSession().begin(plane);
    }
    if (status == CadStatus::Ok) {
        // A plane chosen while the spatial chooser was still open supersedes
        // it: the sketch owns the gesture from here.
        supportChooser().cancel();
        gizmoSession().setActive(false);
        editorSelection().resetGesture();
        editorCamera().resetGesture();
        editorBeginSketchView();
    }
    return status;
}

bool editorSupportChooserBegin(bool allowFaces) {
    if (sculptSession().inSculptMode() || sketchSession().active()
        || constructionHistory().editInProgress()) {
        return false;
    }
    supportChooser().begin(allowFaces);
    return true;
}

CadStatus editorConfirmChosenSupport(bool* outStale) {
    if (outStale != nullptr) {
        *outStale = false;
    }
    // Never the frame stored at the tap (`CAD-V6-S2`): the support is
    // re-validated against the scene as it is now and its frame recomputed,
    // so a producer an Undo or a Redo moved or removed cannot open a sketch on
    // where its face used to be.
    ChosenSupport c;
    const CadStatus fresh = refreshChosenSupport(constructionScene(), supportChooser().selected(), &c);
    if (fresh != CadStatus::Ok) {
        if (outStale != nullptr) {
            *outStale = true;
        }
        return fresh;
    }
    CadStatus status = CadStatus::NotSketching;
    if (c.kind == ChosenSupport::Kind::WorldPlane) {
        // Every CAD begin ends any Surface purpose a dropped sketch left.
        surfaceSketchPurpose() = SurfaceSketchPurpose{};
        status = sketchSession().begin(c.plane);
    } else if (c.kind == ChosenSupport::Kind::Face) {
        // The producer's authored state is staged with the sketch
        // (`CAD-VERTICAL-SLICE-R1`), so the sketch can become an Add or a Cut
        // of THAT body -- the same SceneObject -- as well as a new
        // face-supported body. Nothing is written until the one commit.
        const SceneObject* producer = constructionScene().findBody(c.faceRef.producerObjectId);
        const CadBody* producerCad = producer != nullptr ? producer->cadOrNull() : nullptr;
        surfaceSketchPurpose() = SurfaceSketchPurpose{};
        status = sketchSession().beginOnFace(c.worldFrame, c.faceRef,
                                             producerCad != nullptr ? &producerCad->state() : nullptr);
    } else {
        return status;
    }
    if (status == CadStatus::Ok) {
        supportChooser().cancel();
        gizmoSession().setActive(false);
        editorSelection().resetGesture();
        editorCamera().resetGesture();
        editorBeginSketchView();
    }
    return status;
}

bool editorSketchCancel() {
    const bool wasActive = sketchSession().active();
    sketchSession().cancel();
    // A Surface sketch's purpose ends with the sketch.
    surfaceSketchPurpose() = SurfaceSketchPurpose{};
    editorEndSketchView();
    return wasActive;
}

CadStatus editorSketchFinish(CadFeatureViewSource* outView) {
    const CadStatus status = sketchSession().finish();
    CadFeatureViewSource view = CadFeatureViewSource::Unavailable;
    if (status == CadStatus::Ok) {
        view = editorBeginExtrudeFeatureView();
        editorSetFeatureViewPending(view == CadFeatureViewSource::Unavailable);
    }
    if (outView != nullptr) {
        *outView = view;
    }
    return status;
}

void editorSketchBackToEditing() {
    SketchSession& session = sketchSession();
    if (session.state() == SketchSessionState::Ready && !session.canReturnToSketch()) {
        // A reuse session extrudes a SHARED sketch it may not edit (`CAD-R2`):
        // nothing changes, the camera included.
        session.backToEditing();  // records SketchInUse as the last status
        return;
    }
    session.backToEditing();
    // Back to the authored sketch is back to the EXACT support-normal view
    // (`CAD-UX-S1-C1`): the drawing is authored through the aligned one.
    editorBeginSketchView();
}

bool editorCadFeatureWorldFrame(ObjectId id, uint32_t featureId, SketchFrame* out) {
    if (out == nullptr) return false;
    ConstructionScene& scene = constructionScene();
    const SceneObject* object = scene.findBody(id);
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr) return false;
    CadFeatureGeometry geometry;
    if (buildCadFeatureGeometry(body->state(), featureId, &geometry) != CadStatus::Ok) {
        return false;
    }
    Mat4 model = mat4Identity();
    scene.resolveWorldModel(id, &model);
    out->origin = mat4TransformPoint(model, vec3FromDVec3(geometry.placement.origin));
    out->u = vec3Normalize(mat4TransformDirection(model, vec3FromDVec3(geometry.placement.u)));
    out->v = vec3Normalize(mat4TransformDirection(model, vec3FromDVec3(geometry.placement.v)));
    out->n = vec3Normalize(mat4TransformDirection(model, vec3FromDVec3(geometry.placement.n)));
    return true;
}

bool editorRetainedSketchWorldFrame(ObjectId id, CadSketchId sketchId, SketchFrame* out) {
    if (out == nullptr) return false;
    ConstructionScene& scene = constructionScene();
    const SceneObject* object = scene.findBody(id);
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    CadFrame64 placement;
    if (body == nullptr || cadSketchPlacement(body->state(), sketchId, &placement) != CadStatus::Ok) {
        return false;
    }
    Mat4 model = mat4Identity();
    scene.resolveWorldModel(id, &model);
    out->origin = mat4TransformPoint(model, vec3FromDVec3(placement.origin));
    out->u = vec3Normalize(mat4TransformDirection(model, vec3FromDVec3(placement.u)));
    out->v = vec3Normalize(mat4TransformDirection(model, vec3FromDVec3(placement.v)));
    out->n = vec3Normalize(mat4TransformDirection(model, vec3FromDVec3(placement.n)));
    return true;
}

namespace {

// What every staged-edit begin does once the session has accepted it.
void enterEditedSketch() {
    supportChooser().cancel();
    gizmoSession().setActive(false);
    editorSelection().resetGesture();
    editorCamera().resetGesture();
    editorBeginSketchView();
}

}  // namespace

CadStatus editorSketchBeginEditFeature(ObjectId id, uint32_t featureId, bool startReady) {
    ConstructionScene& scene = constructionScene();
    CadStatus status = CadStatus::Ok;
    SketchFrame frame;
    const SceneObject* object = scene.hasProject() ? scene.findBody(id) : nullptr;
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (!scene.hasProject() || sculptSession().inSculptMode()) {
        status = CadStatus::NotSketching;
    } else if (constructionHistory().editInProgress()) {
        status = CadStatus::RefusedEditInProgress;
    } else if (body == nullptr) {
        status = CadStatus::NotCadBody;
    } else if (!editorCadFeatureWorldFrame(id, featureId, &frame)) {
        status = CadStatus::ProfileNotFound;
    } else {
        // Every CAD begin ends any Surface purpose a dropped sketch left.
        surfaceSketchPurpose() = SurfaceSketchPurpose{};
        status = sketchSession().beginEditFeature(id, body->state(), featureId, frame, startReady);
    }
    if (status == CadStatus::Ok) {
        enterEditedSketch();
        if (startReady) {
            // Straight to the extrusion: the same tilt Finish Sketch takes.
            editorSetFeatureViewPending(editorBeginExtrudeFeatureView() ==
                                        CadFeatureViewSource::Unavailable);
        }
    }
    return status;
}

CadStatus editorSketchBeginEditSketch(ObjectId id, CadSketchId sketchId) {
    ConstructionScene& scene = constructionScene();
    CadStatus status = CadStatus::Ok;
    if (!scene.hasProject() || sculptSession().inSculptMode() || sketchSession().active()) {
        status = CadStatus::NotSketching;
    } else if (constructionHistory().editInProgress()) {
        status = CadStatus::RefusedEditInProgress;
    }
    const SceneObject* object = status == CadStatus::Ok ? scene.findBody(id) : nullptr;
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    SketchFrame frame;
    if (status == CadStatus::Ok && body == nullptr) {
        status = CadStatus::NotCadBody;
    } else if (status == CadStatus::Ok && !editorRetainedSketchWorldFrame(id, sketchId, &frame)) {
        status = CadStatus::SketchNotFound;
    }
    if (status == CadStatus::Ok) {
        surfaceSketchPurpose() = SurfaceSketchPurpose{};
        status = sketchSession().beginEditSketch(id, body->state(), sketchId, frame);
    }
    if (status == CadStatus::Ok) {
        enterEditedSketch();
    }
    return status;
}

bool editorSketchLivePlacement(int viewWidth, int viewHeight, EditorLivePlacement* out) {
    SketchSession::LivePlacement live;
    if (out == nullptr || !sketchSession().livePlacement(&live)) {
        return false;
    }
    const SketchFrame& frame = sketchSession().frame();
    const Vec3 world = vec3Add(frame.origin,
                               vec3Add(vec3Scale(frame.u, static_cast<float>(live.labelAt.u)),
                                       vec3Scale(frame.v, static_cast<float>(live.labelAt.v))));
    float x = 0.0f, y = 0.0f;
    if (!projectWorldToScreen(editorCamera().snapshot(), world, viewWidth, viewHeight, &x, &y)) {
        return false;
    }
    out->tool = live.tool;
    out->first = live.first;
    out->second = live.second;
    out->screenX = x;
    out->screenY = y;
    return true;
}

CadStatus editorSketchCommitEdit() {
    const CadStatus status = sketchSession().commitEdit(constructionScene(), constructionHistory());
    if (status == CadStatus::Ok) {
        editorEndSketchView();
    }
    return status;
}

EditorSketchCommit editorSketchCommit() {
    EditorSketchCommit result;
    ConstructionScene& scene = constructionScene();
    if (surfaceSketchPurpose().active) {
        result.status = CadStatus::NotSketching;
    } else if (!scene.hasProject()) {
        result.firstProject = true;
        FirstProjectReport report;
        result.status = commitFirstCadProject(sketchSession(), scene, sculptSession(),
                                              constructionHistory(), &report);
        result.created = report.bodyId;
        if (result.status == CadStatus::Ok) {
            supportChooser().cancel();
            editorEndSketchView();
        }
    } else {
        result.operation = sketchSession().operation();
        result.status = sketchSession().commit(scene, constructionHistory(), &result.created);
        if (result.status == CadStatus::Ok) {
            editorEndSketchView();
        }
    }
    return result;
}

EditorSketchTouch editorSketchTouch(TouchAction action, int32_t actionPointerId,
                                    const TouchPointer* pointers, int count, int viewWidth,
                                    int viewHeight) {
    EditorSketchTouch result;
    SketchSession& sketch = sketchSession();
    CameraController& camera = editorCamera();
    SelectionController& selection = editorSelection();
    const uint32_t tapSerialBefore = sketch.tapOutcomeSerial();
    const CadSketchEdgeRef axisBefore = sketch.revolveParameters().axis;
    const bool pickingBefore = sketch.revolveAxisPicking();
    result.consumed = sketch.onTouch(action, actionPointerId, pointers, count, camera.snapshot(),
                                     viewWidth, viewHeight);
    if (sketch.tapOutcomeSerial() != tapSerialBefore) {
        result.tapResolved = true;
        result.tapOutcome = sketch.lastTapOutcome();
        result.selectedAreas = sketch.selectedAreaCount();
        // `CAD-TO-FACE-R1`: a Ready tap beside every region, while the
        // extrusion ends on a face, chooses the face it lands on -- the scene
        // picked exactly where the session looked (the Down pixel through the
        // Down camera), the face named by the chooser's own rule.
        if (result.tapOutcome == SketchTapOutcome::Exterior && extrudeIsToFace(sketch.extrude())
            && viewWidth > 0 && viewHeight > 0) {
            const SceneHit faceHit = pickScene(sketch.regionTapCamera(), sketch.regionTapX(),
                                               sketch.regionTapY(), viewWidth, viewHeight);
            ChosenSupport faceChosen;
            if (faceHit.hit
                && resolveFaceSupportFromHit(constructionScene(), faceHit, &faceChosen)) {
                result.toFaceRetargeted = true;
                result.toFaceStatus = sketch.setToFaceTargetFromTopoRef(faceChosen.faceRef);
                result.toFaceFeature = faceChosen.faceRef.producerLocalFeatureId;
            }
        }
    }
    // READY is the one state where an unclaimed single pointer NAVIGATES
    // (`CAD-UX-S1-C1`), except while a Ready TAP is still armed
    // (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`): `sketchEventReachesCamera` is
    // the whole rule, and a held-back event resets the camera's gesture so
    // the first event after the tap disarms re-anchors without a jump.
    const bool navigable = sketch.state() == SketchSessionState::Ready;
    if (!sketchEventReachesCamera(sketch.state(), result.consumed, count,
                                  sketch.readyTapArmed())) {
        camera.resetGesture();
        selection.resetGesture();
    } else {
        camera.onTouch(action, actionPointerId, count > 0 ? pointers : nullptr, count);
        selection.resetGesture();
    }
    if (sketch.gestureActive() || action == TouchAction::Up || action == TouchAction::Cancel) {
        result.logPending = true;
    }
    if (action == TouchAction::Up && navigable) {
        // A Ready tap may have chosen the first region...
        editorBeginPendingExtrudeFeatureView();
        // ...or, in a Revolve, the axis: the ring's view follows.
        if (pickingBefore && !sketch.revolveAxisPicking()
            && !sameCadSketchEdgeRef(axisBefore, sketch.revolveParameters().axis)) {
            editorBeginRevolveFeatureView();
            result.revolveAxisChosen = true;
        }
    }
    return result;
}

}  // namespace forgeshape
