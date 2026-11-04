// The editor state both shells share (`DESKTOP-R1`).
//
// What the Android JNI layer used to own as file-scoped globals and the
// Windows shell needs too: the camera, the selection, the one mutex that makes
// a tap's pick and the render thread's frame see one consistent state, and the
// handful of read-only rules a frame and a shell both ask. Platform-neutral:
// no JNI, no Win32, no Vulkan type crosses this header, so the host self-test
// build compiles it like any other domain file.
//
// Glue moves here act by act (`docs/DESKTOP_AND_TABLET_PLAN.md`); what still
// lives in a shell reaches a frame through `ViewportFrameSources`.
#pragma once

#include <mutex>

#include "forgeshape_body_dimensions.h"
#include "forgeshape_cad_body.h"
#include "forgeshape_cad_extrude_tool.h"
#include "forgeshape_camera.h"
#include "forgeshape_input.h"
#include "forgeshape_scene.h"
#include "forgeshape_selection.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_workplane.h"

namespace forgeshape {

// Process-scoped, created on first use, never destroyed before exit: the
// camera pose and the selected body survive every surface, exactly as the
// JNI's globals did.
CameraController& editorCamera();
SelectionController& editorSelection();

// Guards the camera, the selection and every read of the scene a frame or a
// shell makes. One mutex for both, because a tap resolves a pick against the
// camera snapshot and then updates the selection.
std::mutex& editorStateMutex();

// The ACTIVE body's local bounds and overall dimensions, read off its source.
// Called under editorStateMutex().
bool editorActiveBodyDimensions(LocalBounds* outBounds, BodyDimensions* outDimensions);

// Whether the ACTIVE body is one Dimensions mode may stand on right now: the
// ONE predicate a frame closes the mode with and every shell read asks first.
// Called under editorStateMutex().
bool editorActiveBodyDimensionsEditable(LocalBounds* outBounds, BodyDimensions* outDimensions);

// What turning the transform gizmo on over the active body came to.
enum class GizmoActivation {
    Active,
    Inactive,
    RefusedInSculpt,   // the Sculpt target holds still; strokes own the finger
    RefusedLocked,     // a locked body refuses being moved (Stage 018A)
    RefusedDerived,    // a face-supported part stands where its face puts it
};

// Turns the transform gizmo on or off, refusing by name where a drag could
// only be refused. The ONE rule both shells ask; a refusal leaves it off.
// Called under editorStateMutex().
GizmoActivation editorSetGizmoActive(bool active);

// Substitutes the sketch session's candidate (`CAD-VERTICAL-SLICE-R1`, "the
// preview IS the candidate") into the RENDERER's copy of the scene list, or
// appends it for a New Body. Never the list picking, saving or export read.
// Called under editorStateMutex(), on the thread that draws.
void applyCadOperationPreview(SceneSnapshot* list);

// The world box of everything the viewport DRAWS (`viewSceneSnapshot()`:
// hidden bodies and an isolated-away body excluded), from the published
// meshes placed by their models. False with nothing drawn.
bool editorSceneBounds(Vec3* outMin, Vec3* outMax);
// Fit (`WORKBENCH-RIBBON-R1`): the camera framed on that box, the view
// direction kept. False with nothing to frame or inside a sketch view.
bool editorFitAll();
// The same bounds and framing for ONE drawn body (a context menu's Fit to
// body); false when that body is not drawn.
bool editorBodyBounds(ObjectId id, Vec3* outMin, Vec3* outMax);
bool editorFitBody(ObjectId id);

// ---------------------------------------------------------------------------
// The sketch acts both shells take (`DESKTOP-R2`)
// ---------------------------------------------------------------------------
//
// `SketchSession` already takes platform-neutral samples; what a shell used to
// add around it -- the view a sketch borrows, the feature view after Finish,
// the first-project commit -- lives here, so a sketch is framed, finished and
// committed by ONE path whichever window it was drawn in. Every function is
// called under editorStateMutex().

// The view a sketch borrows (`CAD-R0-A1A2`): the user's own pose, projection
// and span are saved once and framed EXACTLY along the sketch's view frame;
// end gives them back on a cancel and on a commit. Presentation only.
void editorBeginSketchView();
void editorEndSketchView();
// Forgets the saved view without restoring it (the debug camera reset).
void editorForgetSketchView();

// The view the staged extrusion is adjusted through (`CAD-UX-S1-C1`), from
// `cadFeatureViewPose`. Unavailable installs nothing.
CadFeatureViewSource editorBeginExtrudeFeatureView();
// The Revolve's 3/4 view once an axis is set.
void editorBeginRevolveFeatureView();
// Ready entered with NO region chosen leaves the tilt PENDING, and the first
// region chosen takes it (`CAD-VERTICAL-SLICE-R1`).
void editorSetFeatureViewPending(bool pending);
void editorBeginPendingExtrudeFeatureView();

// Begins a sketch on a world plane: refused while sculpting, inside an open
// Construction edit and while one is open; on success the chooser is closed,
// the gizmo withdrawn, the gestures reset and the view framed.
CadStatus editorSketchBegin(Workplane plane);
// Enters spatial support selection (`CAD-A3`): `allowFaces` for New Sketch
// inside a project. Refused (false) while sculpting, while a sketch is open
// and inside an open Construction edit.
bool editorSupportChooserBegin(bool allowFaces);
// Begins the sketch on the chooser's selection, re-validated against the
// scene as it is NOW (`CAD-V6-S2`): a world plane, or a face whose producer's
// authored state is staged with the sketch so it can become an Add, a Cut or
// an Intersect of that body. A stale support begins nothing, and says so
// through `outStale` when given.
CadStatus editorConfirmChosenSupport(bool* outStale = nullptr);

// Ends the sketch, whatever its state, and gives the view back. Returns
// whether one was open.
bool editorSketchCancel();
// Finish Sketch: Editing -> Ready, then the feature view (or a pending one).
CadStatus editorSketchFinish(CadFeatureViewSource* outView);
// Back to the authored sketch, in its exact aligned view; a reuse session
// that may not edit its shared sketch changes nothing.
void editorSketchBackToEditing();

// --- Editing what was made (`WORKBENCH-UX-R1`), moved out of JNI so the
// desktop reaches the same acts: a feature of a CAD body's chain, or one of
// its retained sketches, opened as a STAGED edit -- nothing is truth until
// editorSketchCommitEdit, which is ONE Construction step on the same body.

// Where feature `featureId` of CAD body `body` is authored in WORLD space: its
// body-local placement through the body's resolved world model. False when
// the feature does not place.
bool editorCadFeatureWorldFrame(ObjectId body, uint32_t featureId, SketchFrame* out);
// The same for retained sketch `sketchId`.
bool editorRetainedSketchWorldFrame(ObjectId body, CadSketchId sketchId, SketchFrame* out);
// Opens a staged edit of ONE feature (the base is id 1). With `startReady`
// the session opens on the extrusion, in the feature-preview view; otherwise
// on its sketch in the exact aligned view.
CadStatus editorSketchBeginEditFeature(ObjectId body, uint32_t featureId, bool startReady);
// Opens a staged edit of a retained sketch by id.
CadStatus editorSketchBeginEditSketch(ObjectId body, CadSketchId sketchId);
// Commits an open edit: one transaction, one Undo; a refusal changes nothing.
CadStatus editorSketchCommitEdit();

// The live size of the shape a drag is placing (`WORKBENCH-UX-R1`), with the
// screen point its label stands at: presentation for both shells' label, read
// from the session and the camera together. False when nothing is being
// placed or the point does not project.
struct EditorLivePlacement {
    SketchTool tool = SketchTool::Select;
    double first = 0.0;
    double second = 0.0;
    float screenX = 0.0f;
    float screenY = 0.0f;
};
bool editorSketchLivePlacement(int viewWidth, int viewHeight, EditorLivePlacement* out);

// THE commit: the first project when the scene is empty (`APP-H1`), a New
// Body, an Add, a Cut or an Intersect otherwise. One transaction, one Undo.
struct EditorSketchCommit {
    CadStatus status = CadStatus::NotSketching;
    ObjectId created = kNoObject;
    bool firstProject = false;
    CadFeatureOperation operation = CadFeatureOperation::NewBody;
};
EditorSketchCommit editorSketchCommit();

// One pointer sample while a sketch is open: the session takes it first, a
// Ready tap beside every region retargets a To Face extent to the face under
// it, the camera sees only what `sketchEventReachesCamera` lets through, and
// a Ready tap that chose the first region (or a Revolve axis) takes the view
// that waited for it.
struct EditorSketchTouch {
    bool consumed = false;
    bool tapResolved = false;               // a Ready tap reached an outcome
    SketchTapOutcome tapOutcome = SketchTapOutcome::None;
    size_t selectedAreas = 0;
    bool toFaceRetargeted = false;
    CadStatus toFaceStatus = CadStatus::Ok;
    uint32_t toFaceFeature = 0;
    bool revolveAxisChosen = false;
    bool logPending = false;                // a gesture is live or just ended
};
EditorSketchTouch editorSketchTouch(TouchAction action, int32_t actionPointerId,
                                    const TouchPointer* pointers, int count, int viewWidth,
                                    int viewHeight);

}  // namespace forgeshape
