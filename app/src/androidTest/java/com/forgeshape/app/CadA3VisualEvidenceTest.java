package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.net.Uri;
import android.os.SystemClock;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * `E2E-CADA3-VIS`: deterministic screenshot evidence for the CAD-A3 + APP-H1
 * journey, captured through the real chrome on the authoritative emulator.
 *
 * <p>One journey, ten captures, in the order the brief names them: Home; the
 * New Project chooser; New CAD's spatial world planes; one plane highlighted by
 * a single aiming tap; the exact orthographic sketch view on it; an existing
 * CAD body with an eligible face highlighted; that face entering a sketch; the
 * face-supported sketch completed; the repeated Extrude with producer and
 * dependent on screen; and the saved `.forge` reopened with the dependency
 * restored.
 *
 * <p>Every capture is written beside a line of <b>measurable facts</b> — an
 * element's presence and bounds, a selected or highlighted state, the camera's
 * projection mode, the body count — and nothing aesthetic is asserted. Frames
 * come from {@code UiAutomation.takeScreenshot()}, which captures the composed
 * display, so the Vulkan viewport is in the picture rather than a black hole
 * where a {@code SurfaceView} sits. The files land in the app's own external
 * files directory, from where {@code scripts\collect-cad-a3-evidence.ps1}
 * pulls them and builds the contact sheet.
 *
 * <p>No control is located by coordinate: viewport pixels come from
 * {@code debugProjectWorld} and every control from its semantic id.
 */
@RunWith(AndroidJUnit4.class)
public final class CadA3VisualEvidenceTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private File outDir;
    private final List<String> facts = new ArrayList<>();
    private int captureIndex;

    @Before
    public void startAtHome() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-a3-app-h1");
        //noinspection ResultOfMethodCallIgnored
        outDir.mkdirs();
        for (File stale : orEmpty(outDir.listFiles())) {
            //noinspection ResultOfMethodCallIgnored
            stale.delete();
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    @After
    public void leaveAProjectBehind() {
        writeFacts();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void e2eCadA3Vis_theWholeJourneyInTenCaptures() {
        // 1. Home.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.homeVisible());
            fact("home_visible", true);
            fact("project_open", NativeViewport.projectOpen());
            fact("body_count", NativeViewport.sceneBodyCount());
            bounds(workspace, "home_new_project", R.id.home_new_project);
            bounds(workspace, "home_open_file", R.id.home_open_file);
            return null;
        });
        capture("01_home");

        // 2. New Project chooser.
        press(R.id.home_new_project);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.newProjectChooserVisible());
            fact("new_project_chooser_visible", true);
            bounds(workspace, "new_project_cad", R.id.new_project_cad);
            bounds(workspace, "new_project_sculpt", R.id.new_project_sculpt);
            bounds(workspace, "new_project_cancel", R.id.new_project_cancel);
            return null;
        });
        capture("02_new_project_chooser");

        // 3. New CAD: the three spatial world planes, no project behind them.
        press(R.id.new_project_cad);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.supportChooserActive());
            fact("support_chooser_active", true);
            fact("project_open", NativeViewport.projectOpen());
            fact("selected_kind", NativeViewport.supportChooserSelectedKind());
            fact("projection_mode", NativeViewport.projectionMode());
            bounds(workspace, "back_to_home", R.id.back_to_home);
            planeTarget("xy", 2.0, 2.0, 0.0);
            planeTarget("xz", 2.0, 0.0, 2.0);
            planeTarget("yz", 0.0, 2.0, 2.0);
            return null;
        });
        capture("03_new_cad_world_planes");

        // 4. One world plane highlighted by a single aiming tap.
        tapWorld(rule.getScenario(), 2.0, 0.0, 2.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.WORKPLANE_XZ, NativeViewport.supportChooserSelectedKind());
            fact("selected_kind", NativeViewport.supportChooserSelectedKind());
            fact("selected_plane", "XZ");
            fact("sketch_state", sketchState());
            return null;
        });
        capture("04_world_plane_highlighted");

        // 5. The exact orthographic sketch view on that plane.
        tapWorld(rule.getScenario(), 2.0, 0.0, 2.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
            fact("sketch_state", sketchState());
            fact("projection_mode", NativeViewport.projectionMode());
            fact("grid_step_m", NativeViewport.sketchGridStep());
            fact("project_open", NativeViewport.projectOpen());
            bounds(workspace, "finish_sketch", R.id.finish_sketch);
            bounds(workspace, "tool_rail_rectangle", R.id.tool_rail_rectangle);
            return null;
        });
        capture("05_orthographic_sketch_view");

        // The first project: a 2x2 rectangle extruded 2 on XZ.
        final long producer = drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "2");
        assertTrue(NativeViewport.projectOpen());

        // 6. The existing CAD body with an eligible face highlighted. The far
        // cap of an XZ extrusion along +Y sits at y = 2; look from above it.
        assertTrue(NativeViewport.debugSetCameraPose(0.7f, 0.9f, 8.0f));
        openSpatialChooserInProject();
        tapWorld(rule.getScenario(), 0.0, 2.0, 0.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("a CAD face is aimed at", 3, NativeViewport.supportChooserSelectedKind());
            fact("selected_kind", NativeViewport.supportChooserSelectedKind());
            fact("selected_is_face", true);
            fact("producer_id", producer);
            fact("body_count", NativeViewport.sceneBodyCount());
            return null;
        });
        capture("06_cad_face_highlighted");

        // 7. The selected planar face entering a sketch.
        tapWorld(rule.getScenario(), 0.0, 2.0, 0.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
            fact("sketch_state", sketchState());
            fact("projection_mode", NativeViewport.projectionMode());
            fact("support_chooser_active", NativeViewport.supportChooserActive());
            return null;
        });
        capture("07_face_entering_sketch");

        // 8. The face-supported sketch, completed: a rectangle drawn on the
        // face and Finish Sketch pressed, a profile found.
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -0.5, -0.5, 0.5, 0.5);
        assertEquals(1, sketchEntityCount());
        press(R.id.finish_sketch);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SKETCH_READY, sketchState());
            fact("sketch_state", sketchState());
            fact("sketch_entities", sketchEntityCount());
            bounds(workspace, "extrude_sketch", R.id.extrude_sketch);
            return null;
        });
        capture("08_face_sketch_completed");

        // 9. The repeated Extrude: producer and dependent on screen.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.EditText depth =
                    workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            depth.setText("0.5");
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        assertTrue(NativeViewport.debugSetCameraPose(0.7f, 0.5f, 8.2f));
        settleLayout();
        final long dependent = NativeViewport.sceneActiveBodyId();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.sceneActiveBodyIsFaceSupportedCad());
            fact("body_count", NativeViewport.sceneBodyCount());
            fact("producer_id", producer);
            fact("dependent_id", dependent);
            fact("dependent_face_supported", true);
            fact("delete_producer_status", NativeViewport.sceneDeleteBody(producer));
            bounds(workspace, "objects_capsule", R.id.objects_capsule);
            return null;
        });
        capture("09_producer_and_dependent");

        // 10. The saved `.forge` reopened from Home, dependency restored.
        final byte[] saved = NativeViewport.encodeProject();
        assertNotNull(saved);
        final File document = new File(outDir, "cad_a3_dependency.forge");
        writeFile(document, saved);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(document));
            return null;
        });
        settleLayout();
        assertTrue(NativeViewport.debugSetCameraPose(0.7f, 0.5f, 8.2f));
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.projectOpen());
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(dependent));
            workspace.syncFromNative();
            assertTrue(NativeViewport.sceneActiveBodyIsFaceSupportedCad());
            fact("reopened_from_file", document.getName());
            fact("body_count", NativeViewport.sceneBodyCount());
            fact("dependent_face_supported", true);
            fact("delete_producer_status", NativeViewport.sceneDeleteBody(producer));
            fact("undo_depth", NativeViewport.constructionUndoDepth());
            return null;
        });
        capture("10_reopened_dependency_restored");
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull(control);
            assertTrue(control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    private void openSpatialChooserInProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.add_sketch).performClick();
            return null;
        });
        settleLayout();
        assertTrue(NativeViewport.supportChooserActive());
    }

    /** Takes one composed-display frame after a short settle. */
    private void capture(String name) {
        settleLayout();
        SystemClock.sleep(400);  // two or three more frames, so the viewport has drawn the state
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull("the display could be captured", frame);
        captureIndex++;
        final File png = new File(outDir, name + ".png");
        try (FileOutputStream out = new FileOutputStream(png)) {
            assertTrue(frame.compress(Bitmap.CompressFormat.PNG, 100, out));
        } catch (IOException error) {
            throw new AssertionError("could not write " + png, error);
        }
        facts.add("capture=" + name + ".png width=" + frame.getWidth() + " height="
                + frame.getHeight());
        facts.add("");
    }

    private void fact(String key, Object value) {
        facts.add("  " + key + "=" + value);
    }

    private void bounds(EditorWorkspaceView workspace, String label, int id) {
        final View view = workspace.findViewById(id);
        if (view == null || !view.isShown()) {
            facts.add("  " + label + "=absent");
            return;
        }
        final int[] at = new int[2];
        view.getLocationOnScreen(at);
        final Rect rect = new Rect(at[0], at[1], at[0] + view.getWidth(), at[1] + view.getHeight());
        facts.add("  " + label + "=" + rect.toShortString() + " width_px=" + view.getWidth()
                + " height_px=" + view.getHeight());
    }

    private void planeTarget(String plane, double x, double y, double z) {
        final float[] at = new float[2];
        if (NativeViewport.debugProjectWorld(x, y, z, at)) {
            facts.add("  plane_" + plane + "_target_px=(" + (int) at[0] + "," + (int) at[1] + ")");
        } else {
            facts.add("  plane_" + plane + "_target_px=offscreen");
        }
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts.txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            out.println("captures=" + captureIndex);
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            throw new AssertionError("could not write " + file, error);
        }
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException error) {
            throw new AssertionError("could not write " + file, error);
        }
    }

    private static File[] orEmpty(File[] files) {
        return files == null ? new File[0] : files;
    }
}
