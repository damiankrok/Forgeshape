#include "forgeshape_viewport_frame.h"

#include <utility>

#include "forgeshape_body_dimension_overlay.h"
#include "forgeshape_cad_extrude_tool.h"
#include "forgeshape_display.h"
#include "forgeshape_freeform_session.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_import_preview.h"
#include "forgeshape_renderer.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_support_chooser.h"

namespace forgeshape {

void composeViewportFrame(Renderer& renderer, const ViewportFrameSources& sources) {
    CameraController& camera = editorCamera();
    {
        std::lock_guard<std::mutex> lock(editorStateMutex());
        renderer.setCamera(camera.snapshot());
        // One immutable snapshot of every renderable body: its own
        // published mesh, its own derived model matrix and its own
        // selection flag. Taken under the state mutex; the geometry
        // work the renderer then does with it happens after the mutex
        // is released, because the snapshot holds shared_ptrs rather
        // than borrowing anything the scene could change underneath.
        // WHICH scene is drawn. The imported preview is a diagnostic
        // VIEW, so it replaces what the renderer is handed for the
        // frame and changes nothing about the project: the scene it is
        // standing in front of is untouched, still selected, still
        // editable, and one call to setPreviewVisible(false) brings it
        // straight back. The preview never merges with the project
        // snapshot, because a mixed list is a list somebody would
        // eventually pick, save or export from.
        if (importedMeshPreview().visible()) {
            renderer.setScene(importedMeshPreview().snapshot());
        } else {
            // The viewport's ONE list: the scene under whatever
            // restriction the Sculpt session holds (Sculpt Isolate).
            // Taps pick against the same function, so an isolated-out
            // body is neither drawn nor pickable, and the selection
            // outline's mask pass follows because it draws this list.
            SceneSnapshot list = viewSceneSnapshot();
            applyCadOperationPreview(&list);
            renderer.setScene(std::move(list));
        }
        // Taken under the SAME mutex and from the same instant as the
        // camera and the scene, so the pivot the handles are drawn
        // around is the placement this frame's body is drawn at. Taking
        // it separately is exactly how a handle would lag a drag by a
        // frame.
        //
        // Handles are withdrawn while the preview is shown, for the
        // same reason the editing controls are: a gizmo drawn over an
        // imported mesh would be pointing at a body that is not on the
        // screen, and dragging it would move something invisible.
        //
        // They are withdrawn in Dimensions mode for a related reason
        // (Stage 020M): the leaders and the handles are two instruments
        // for the same placement, and drawing both would invite a drag
        // in a mode whose whole point is an exact typed value. The
        // session already turned the gizmo off when it opened; this is
        // the frame's own answer, so a mode entered mid-drag cannot
        // leave one standing.
        if (importedMeshPreview().visible()
            || bodyDimensionSession().active()) {
            renderer.setGizmo(GizmoSnapshot{});
        } else if (freeformEditSession().active()) {
            // The cage's own instrument at the selection, on the
            // gizmo's terms (`MODELING-FOUNDATIONS-R1` B).
            renderer.setGizmo(freeformEditSession().gizmoSnapshot(
                camera.snapshot(), camera.viewportWidth(), camera.viewportHeight()));
        } else {
            renderer.setGizmo(gizmoSession().snapshot(
                camera.snapshot(), camera.viewportWidth(),
                camera.viewportHeight()));
        }
        // The sketch overlay, from the same instant. Sized by the same
        // camera-derived world-per-unit the gizmo uses, so the snap
        // marker reads at the same size the handles do. Empty -- and
        // therefore free -- whenever no sketch is in progress.
        {
            float worldPerUnit = 0.0f;
            gizmoWorldScale(camera.snapshot(),
                                        Vec3{0.0f, 0.0f, 0.0f},
                                        camera.viewportHeight(), &worldPerUnit);
            // The spatial support chooser and the sketch never both run:
            // the chooser draws the plane/face targets, then the sketch
            // it starts draws the entities. When neither is active the
            // overlay is empty and free.
            //
            // Dimensions mode is the third producer for the same one
            // slot, and it borrows the whole path rather than adding a
            // second: a body's dimension leaders and a sketch's
            // dimension annotation are the same kind of drawing, so the
            // renderer needed no change for either. The three are
            // mutually exclusive by construction -- a sketch cannot be
            // open over a body being measured, because the mode refuses
            // to open in Sculpt and creation is refused in a sketch.
            freeformEditSession().reconcile();
            if (supportChooser().active()) {
                renderer.setSketchOverlay(supportChooser().overlay());
            } else if (freeformEditSession().active()) {
                // The cage over the smooth surface: a fourth producer
                // for the one overlay slot, in its own revision space.
                renderer.setSketchOverlay(
                    freeformEditSession().overlay(worldPerUnit));
            } else if (bodyDimensionSession().active()) {
                LocalBounds bounds;
                const ConstructionScene& scene =
                    constructionScene();
                // The ONE predicate, asked here and by every chrome
                // read, so a frame and a refresh can never disagree
                // about whether the mode still has a body.
                const bool measurable = editorActiveBodyDimensionsEditable(&bounds, nullptr);
                if (measurable) {
                    renderer.setSketchOverlay(bodyDimensionSession().overlay(
                        bounds, scene.activeBody().transform().values(), worldPerUnit));
                } else {
                    // The mode CLOSES itself the moment what it measures
                    // stops being measurable -- another body selected,
                    // this one locked, hidden or deleted, Sculpt entered,
                    // the project closed. One place decides that, every
                    // frame, so no caller has to remember to unwind it
                    // and the shell's next refresh simply finds the mode
                    // shut.
                    bodyDimensionSession().close();
                    renderer.setSketchOverlay(SketchOverlayPtr{});
                }
            } else {
                // The manipulator's ONE scale fact for this frame, read
                // at its own base anchor rather than at the origin the
                // grid and the snap marker are sized from, so the drawn
                // head is the head the hit test grabs.
                SketchSession& session = sketchSession();
                // The shell's own overlays for this one slot -- an offer's
                // edge highlight, a measurement -- while they still live in a
                // shell (`ViewportFrameSources`).
                SketchOverlayPtr edgeHighlight;
                if (sources.shellOverlay) {
                    edgeHighlight = sources.shellOverlay(session.active());
                }
                if (edgeHighlight) {
                    renderer.setSketchOverlay(edgeHighlight);
                } else {
                    CadExtrudeViewFacts view;
                    session.extrudeViewFacts(camera.snapshot(), camera.viewportWidth(),
                                             camera.viewportHeight(), &view);
                    renderer.setSketchOverlay(session.overlay(worldPerUnit, view));
                }
            }
        }
    }
    // Presentation only, and deliberately OUTSIDE the state mutex: the
    // display settings are plain atomics that no domain invariant
    // depends on, so a frame must never wait on the geometry lock to
    // find out which shading model to draw with.
    renderer.setDisplaySettings(displaySettings().snapshot());
}

}  // namespace forgeshape
