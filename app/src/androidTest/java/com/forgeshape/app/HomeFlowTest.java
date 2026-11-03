package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.sketchPlane;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapTapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.net.Uri;
import android.view.View;
import android.view.ViewGroup;

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

/**
 * `E2E-APPH1`: Home, New Project, the CAD bootstrap, Open File and the
 * unsaved-changes guard, through the real chrome and real MotionEvents.
 *
 * <p>Home is not a project. Every case here starts from the cold-launch state
 * the product's own close produces — no project, no bootstrap, no question —
 * and asserts against native truth: how many bodies exist, whether a project
 * is open, what the history holds, and that <b>no active body is ever read
 * while there is none</b> ({@code debugActiveBodyMisuseCount} is unchanged
 * across every journey).
 *
 * <p>No control is located by coordinate. The one place a pixel appears is the
 * viewport gesture, and that pixel is asked for from {@code debugProjectWorld}.
 * The system document picker cannot be driven from instrumentation, so the
 * cases that reach it swap in a recording host and feed the picker's answer
 * through the same {@code onOpenProjectDocumentChosen} the Activity calls, with
 * a real {@code Uri} a real {@code ContentResolver} opens.
 */
@RunWith(AndroidJUnit4.class)
public final class HomeFlowTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private long misuseAtStart;
    private RecordingTransferHost recordingHost;

    @Before
    public void startAtHome() {
        misuseAtStart = NativeViewport.debugActiveBodyMisuseCount();
        recordingHost = new RecordingTransferHost();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            workspace.setProjectTransferHost(recordingHost);
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    @After
    public void restoreTheRealHostAndAProject() {
        assertEquals("no active body was read while no project was open",
                misuseAtStart, NativeViewport.debugActiveBodyMisuseCount());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setProjectTransferHost(activity.projectTransferHost());
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // E2E-APPH1-01: cold launch is Home, with no project and no editor
    // -----------------------------------------------------------------------

    @Test
    public void e2eAppH1_01_coldLaunchIsHomeWithNoProjectBehindIt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Home is on screen", workspace.homeVisible());
            assertEquals(View.VISIBLE, workspace.findViewById(R.id.home_surface).getVisibility());
            assertFalse("no project is open", NativeViewport.projectOpen());
            assertEquals("and no default primitive stands behind Home",
                    0, NativeViewport.sceneBodyCount());
            assertEquals("nothing is selected", NativeViewport.NO_OBJECT,
                    NativeViewport.sceneActiveBodyId());
            assertFalse("the editor chrome is withdrawn",
                    workspace.findViewById(R.id.global_toolbar).isShown());
            assertFalse("no bootstrap is open", workspace.bootstrapVisible());
            assertFalse(NativeViewport.supportChooserActive());
            assertEquals("no sketch is open", NativeViewport.SKETCH_INACTIVE, sketchState());
            assertEquals("no history exists", 0, NativeViewport.constructionUndoDepth());
            assertEquals("no document can describe Home", 0L, NativeViewport.projectFingerprint());
            assertNull("and no `.forge` bytes can be written for it",
                    NativeViewport.encodeProject());
            // Exactly two primary actions, counted rather than spot-checked --
            // plus, since UI-PREF-R1, the quiet Settings row at the foot, which
            // is not a way to have a project and is drawn as a secondary action.
            assertNotNull(workspace.findViewById(R.id.home_new_project));
            assertNotNull(workspace.findViewById(R.id.home_open_file));
            assertNotNull(workspace.findViewById(R.id.home_settings));
            assertEquals("Home offers two ways to have a project, Settings, and nothing else",
                    3, countClickable(workspace.findViewById(R.id.home_panel)));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-APPH1-02: New Project offers CAD and Sculpt, and can be cancelled
    // -----------------------------------------------------------------------

    @Test
    public void e2eAppH1_02_newProjectOffersCadAndSculptAndCancelReturnsHome() {
        press(R.id.home_new_project);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the New Project chooser is on screen",
                    workspace.newProjectChooserVisible());
            assertNotNull(workspace.findViewById(R.id.new_project_cad));
            assertNotNull(workspace.findViewById(R.id.new_project_sculpt));
            // CAD, Sculpt, and since `MODELING-FOUNDATIONS-R1` Freeform and
            // Surface: four ways to begin, and the way not to.
            assertNotNull(workspace.findViewById(R.id.new_project_freeform));
            assertNotNull(workspace.findViewById(R.id.new_project_surface));
            assertEquals("four ways to begin, and the way not to",
                    5, countClickable(workspace.findViewById(R.id.new_project_chooser_panel)));
            assertFalse("choosing is not creating", NativeViewport.projectOpen());
            return null;
        });
        press(R.id.new_project_cancel);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse(workspace.newProjectChooserVisible());
            assertTrue("Cancel returns to Home", workspace.homeVisible());
            assertFalse(NativeViewport.projectOpen());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-APPH1-03: New CAD -> the flat XY sketch -> rectangle -> Finish ->
    // Extrude -> the first durable body and the editor
    //
    // `SKETCH-UX-R1` B1 removed the plane-chooser step from this journey: the
    // first sketch of a brand-new project opens DIRECTLY on the flat XY grid,
    // because there was nothing in the empty world to relate three floating
    // squares to. The plane is changed from the orientation navigator instead,
    // where the sketch can be seen. The spatial chooser is unchanged and still
    // the normal path for a LATER New Sketch — E2E-APPH1-05 below drives it.
    // -----------------------------------------------------------------------

    @Test
    public void e2eAppH1_03_newCadBootstrapsDirectlyIntoAFlatSketch() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("no floating-plane step stands between New CAD and drawing",
                    NativeViewport.supportChooserActive());
            assertEquals("New CAD lands directly in a sketch", NativeViewport.SKETCH_EDITING,
                    sketchState());
            assertEquals("on XY", NativeViewport.WORKPLANE_XY, sketchPlane());
            assertTrue(workspace.bootstrapVisible());
            assertFalse("Home is gone", workspace.homeVisible());
            assertFalse("and still no project exists", NativeViewport.projectOpen());
            assertEquals(0, NativeViewport.sceneBodyCount());
            assertTrue("the toolbar is drawn for the bootstrap",
                    workspace.globalToolbar().showingBootstrap());
            assertTrue("with Back to Home as the way out",
                    workspace.findViewById(R.id.back_to_home).isShown());
            assertFalse("and no project control to save nothing with",
                    workspace.findViewById(R.id.project_actions_button).isShown());
            assertFalse("no Objects capsule for a scene with no objects",
                    workspace.findViewById(R.id.objects_capsule).isShown());
            assertTrue("the sketch tools are on the rail",
                    workspace.findViewById(R.id.tool_rail_rectangle).isShown());
            assertTrue(workspace.findViewById(R.id.finish_sketch).isShown());
            assertTrue("and the orientation navigator is in the corner",
                    workspace.findViewById(R.id.sketch_orientation_navigator).isShown());
            return null;
        });

        final long created = drawRectangleAndExtrude(rule.getScenario(), 2.0, 1.0, "1.5");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the first Extrude created the project", NativeViewport.projectOpen());
            assertEquals("with exactly one body", 1, NativeViewport.sceneBodyCount());
            assertEquals(created, NativeViewport.sceneActiveBodyId());
            assertTrue(NativeViewport.sceneActiveBodyIsCad());
            assertFalse("a world-plane body, not a dependent",
                    NativeViewport.sceneActiveBodyIsFaceSupportedCad());
            assertEquals("a new project starts with an empty history",
                    0, NativeViewport.constructionUndoDepth());
            assertEquals(0, NativeViewport.constructionRedoDepth());
            assertEquals(NativeViewport.MODE_CONSTRUCTION, NativeViewport.productMode());
            assertNotEquals("and now a document describes it",
                    0L, NativeViewport.projectFingerprint());
            assertNotNull(NativeViewport.encodeProject());
            assertFalse("the editor is the ordinary one", workspace.bootstrapVisible());
            assertFalse(workspace.globalToolbar().showingBootstrap());
            assertTrue(workspace.findViewById(R.id.project_actions_button).isShown());
            assertTrue(workspace.findViewById(R.id.tool_rail_shape).isShown());
            assertTrue("a project that exists nowhere yet is dirty",
                    workspace.projectDirty());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-APPH1-04: New Sculpt is a real sculpt project
    // -----------------------------------------------------------------------

    @Test
    public void e2eAppH1_04_newSculptCreatesASculptableProject() {
        press(R.id.home_new_project);
        press(R.id.new_project_sculpt);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.projectOpen());
            assertEquals("one body, seeded", 1, NativeViewport.sceneBodyCount());
            assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            assertEquals(1.0, sculpt[NativeViewport.SCULPT_HAS_MESH], 0.0);
            assertEquals("sphere-derived", 482, (int) sculpt[NativeViewport.SCULPT_VERTEX_COUNT]);
            assertEquals("the seed is not the first Undo",
                    0, NativeViewport.constructionUndoDepth());
            assertFalse(NativeViewport.sculptUndoAvailable());
            assertTrue(workspace.findViewById(R.id.brush_edge_controls).isShown());
            assertFalse(workspace.homeVisible());
            return null;
        });
        // Sculpt Undo stays green: one real stroke, one entry, taken back.
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals(1, NativeViewport.sculptUndoDepth());
        assertEquals(NativeViewport.HISTORY_OK, NativeViewport.historyUndo());
        assertEquals(0, NativeViewport.sculptUndoDepth());
    }

    // -----------------------------------------------------------------------
    // E2E-APPH1-05: Open File from Home opens a saved CAD dependency project
    // -----------------------------------------------------------------------

    @Test
    public void e2eAppH1_05_openFileFromHomeOpensASavedDependencyProject() {
        // Build a producer + dependent project first, save its bytes, and
        // return to Home. The dependency is what the reopened file must carry.
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        final long producer = drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "2");
        openSpatialChooserInProject();
        tapTapWorld(rule.getScenario(), 0.0, 0.0, 2.0);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        final long dependent = drawRectangleAndExtrude(rule.getScenario(), 1.0, 1.0, "0.5");
        assertTrue(NativeViewport.sceneActiveBodyIsFaceSupportedCad());
        final byte[] saved = NativeViewport.encodeProject();
        assertNotNull(saved);
        final File document = new File(scratch(), "dependency.forge");
        writeFile(document, saved);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
        assertFalse(NativeViewport.projectOpen());

        press(R.id.home_open_file);
        assertEquals("Open File asks the system picker", 1, recordingHost.openProjectRequests);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(document));
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the file opened atomically into the editor", NativeViewport.projectOpen());
            assertFalse(workspace.homeVisible());
            assertEquals(2, NativeViewport.sceneBodyCount());
            assertEquals(dependent, NativeViewport.sceneActiveBodyId());
            assertTrue("the dependency was restored",
                    NativeViewport.sceneActiveBodyIsFaceSupportedCad());
            assertEquals("and the producer still cannot be deleted under it",
                    NativeViewport.DELETE_REFUSED_HAS_DEPENDENTS,
                    NativeViewport.sceneDeleteBody(producer));
            assertFalse("an opened file is what the user has: not dirty",
                    workspace.projectDirty());
            assertEquals(0, NativeViewport.constructionUndoDepth());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-APPH1-06/07: Open File cancel stays Home; a corrupt file is
    // non-mutating and stays Home
    // -----------------------------------------------------------------------

    @Test
    public void e2eAppH1_06_openFileCancelStaysAtHome() {
        press(R.id.home_open_file);
        assertEquals(1, recordingHost.openProjectRequests);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(null);
            assertTrue("cancel stays on Home", workspace.homeVisible());
            assertFalse(NativeViewport.projectOpen());
            assertEquals(activity.getString(R.string.status_home_open_cancelled),
                    workspace.homeStatusText().toString());
            return null;
        });
    }

    @Test
    public void e2eAppH1_07_aCorruptOpenIsNonMutatingAndStaysAtHome() {
        final File corrupt = new File(scratch(), "corrupt.forge");
        writeFile(corrupt, new byte[]{'F', 'O', 'R', 'G', 'E', 'S', 'H', '1', 0, 0, 1, 2, 3});
        final File missing = new File(scratch(), "missing.forge");
        press(R.id.home_open_file);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(corrupt));
            assertTrue("a refused file leaves Home standing", workspace.homeVisible());
            assertFalse("and creates nothing", NativeViewport.projectOpen());
            assertEquals(0, NativeViewport.sceneBodyCount());
            assertTrue("with a clear verdict on Home",
                    workspace.homeStatusText().length() > 0);
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(missing));
            assertTrue(workspace.homeVisible());
            assertFalse(NativeViewport.projectOpen());
            assertEquals(activity.getString(R.string.status_project_damaged),
                    workspace.homeStatusText().toString());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-APPH1-08/09/10: the unsaved-changes guard
    // -----------------------------------------------------------------------

    @Test
    public void e2eAppH1_08_dirtyNewProjectCancelPreservesTheProject() {
        final byte[] before = makeADirtyConstructionProject();
        openProjectSurfaceAndPress(R.id.project_new);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("a dirty project is asked about first", workspace.unsavedPromptVisible());
            assertFalse(workspace.newProjectChooserVisible());
            assertTrue("and stays open under the question", NativeViewport.projectOpen());
            return null;
        });
        press(R.id.unsaved_cancel);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse(workspace.unsavedPromptVisible());
            assertFalse("Cancel opens nothing", workspace.newProjectChooserVisible());
            assertTrue(NativeViewport.projectOpen());
            assertArrayEquals("and the project is exactly what it was",
                    before, NativeViewport.encodeProject());
            assertTrue(workspace.projectDirty());
            return null;
        });
    }

    @Test
    public void e2eAppH1_09_dirtyNewProjectDiscardLeavesAndOpensTheChooser() {
        makeADirtyConstructionProject();
        awaitAutosave();
        assertTrue("a checkpoint was protecting the changes",
                ProjectCheckpoint.exists(context()));
        openProjectSurfaceAndPress(R.id.project_new);
        press(R.id.unsaved_discard);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("Discard leaves the project", NativeViewport.projectOpen());
            assertEquals(0, NativeViewport.sceneBodyCount());
            assertTrue("and opens the New Project chooser",
                    workspace.newProjectChooserVisible());
            assertFalse(workspace.unsavedPromptVisible());
            assertFalse("without writing the discarded work anywhere",
                    ProjectCheckpoint.exists(context()));
            return null;
        });
        press(R.id.new_project_cancel);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Cancel from there lands at Home", workspace.homeVisible());
            return null;
        });
    }

    @Test
    public void e2eAppH1_10_dirtyOpenFileSavePersistsTheOldProjectBeforeOpening() {
        final byte[] dirty = makeADirtyConstructionProject();
        openProjectSurfaceAndPress(R.id.project_open_file);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.unsavedPromptVisible());
            assertEquals("the picker waits for the answer", 0, recordingHost.openProjectRequests);
            return null;
        });
        press(R.id.unsaved_save);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertArrayEquals("Save persisted the old project first",
                    dirty, ProjectSlot.read(context()));
            assertFalse(workspace.unsavedPromptVisible());
            assertFalse("and it is no longer dirty", workspace.projectDirty());
            assertEquals("then the picker was asked", 1, recordingHost.openProjectRequests);
            assertTrue("with the project still live until a file opens",
                    NativeViewport.projectOpen());
            return null;
        });
        // A failed Save keeps the project alive and the question open.
        makeADirtyConstructionProject();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setProjectTransferHost(recordingHost);
            return null;
        });
        openProjectSurfaceAndPress(R.id.project_open_file);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.unsavedPromptVisible());
            assertTrue(NativeViewport.projectOpen());
            return null;
        });
        press(R.id.unsaved_cancel);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Cancel leaves the project exactly where it was",
                    NativeViewport.projectOpen());
            assertEquals(1, recordingHost.openProjectRequests);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-APPH1-11: Back is deterministic in every shell phase
    // -----------------------------------------------------------------------

    @Test
    public void e2eAppH1_11_backIsDeterministicInEveryShellPhase() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("Home with nothing open: Back is the platform's",
                    workspace.hasDismissibleSurface());
            assertFalse(workspace.dismissTopmostSurface());
            return null;
        });
        press(R.id.home_new_project);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.hasDismissibleSurface());
            assertTrue("chooser: Back cancels it", workspace.dismissTopmostSurface());
            assertFalse(workspace.newProjectChooserVisible());
            assertTrue(workspace.homeVisible());
            return null;
        });
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // ONE step out of the bootstrap now, not two: the first sketch IS
            // the bootstrap since `SKETCH-UX-R1` B1, and the scene behind it is
            // empty, so there is nothing to go back TO but Home.
            assertTrue("bootstrap sketch: Back goes Home",
                    workspace.dismissTopmostSurface());
            assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
            assertFalse(NativeViewport.supportChooserActive());
            assertTrue(workspace.homeVisible());
            assertFalse("and Back from the bootstrap created nothing",
                    NativeViewport.projectOpen());
            assertEquals(0, NativeViewport.sceneBodyCount());
            return null;
        });
        makeADirtyConstructionProject();
        openProjectSurfaceAndPress(R.id.project_new);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.unsavedPromptVisible());
            assertTrue("unsaved question: Back means Cancel, never Discard",
                    workspace.dismissTopmostSurface());
            assertFalse(workspace.unsavedPromptVisible());
            assertTrue(NativeViewport.projectOpen());
            assertTrue(workspace.projectDirty());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-APPH1-12: Home, the chooser and the bootstrap survive a recreation
    // -----------------------------------------------------------------------

    @Test
    public void e2eAppH1_12_homeChooserAndBootstrapSurviveARecreation() {
        rule.getScenario().recreate();
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            workspace.setProjectTransferHost(recordingHost);
            workspace.syncFromNative();
            assertTrue("Home is derived from native truth, so it comes back",
                    workspace.homeVisible());
            assertFalse(NativeViewport.projectOpen());
            return null;
        });
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        rule.getScenario().recreate();
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setProjectTransferHost(recordingHost);
            workspace.syncFromNative();
            assertEquals("the bootstrap SKETCH is native state and survives a recreation",
                    NativeViewport.SKETCH_EDITING, sketchState());
            assertTrue(workspace.bootstrapVisible());
            assertFalse(workspace.homeVisible());
            assertTrue(workspace.findViewById(R.id.back_to_home).isShown());
            return null;
        });
        press(R.id.back_to_home);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.homeVisible());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    /** A recording stand-in for the system document picker. */
    private static final class RecordingTransferHost
            implements EditorWorkspaceView.ProjectTransferHost {
        int openProjectRequests;

        @Override
        public boolean requestCreateProjectDocument() {
            return true;
        }

        @Override
        public boolean requestOpenProjectDocument() {
            openProjectRequests++;
            return true;
        }

        @Override
        public boolean requestCreateDiagnosticsDocument() {
            return true;
        }

        @Override
        public boolean requestCreateGlbDocument() {
            return true;
        }

        @Override
        public boolean requestOpenGlbDocument() {
            return true;
        }
    }

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("control " + id + " must exist", control);
            assertTrue("control " + id + " must be on screen", control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    private void openProjectSurfaceAndPress(final int rowId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.project_actions_button).performClick();
            return null;
        });
        settleLayout();
        press(rowId);
    }

    /** New Sketch inside an open project lands directly in the spatial chooser. */
    private void openSpatialChooserInProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.add_sketch).performClick();
            return null;
        });
        settleLayout();
        assertTrue(NativeViewport.supportChooserActive());
    }

    /** How many dirty projects this case has made, so each edit is a real change. */
    private int dirtyEdits;

    /** A Construction project with an unsaved edit; returns its bytes. */
    private byte[] makeADirtyConstructionProject() {
        final double width = 3.25 + 0.5 * (dirtyEdits++);
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.ensureConstructionProjectForTest();
            // A different size every time: an identical re-apply is UNCHANGED
            // and would leave a just-saved project honestly clean.
            NativeViewport.applyConstructionBox(width, 1.5, 0.75);
            workspace.syncFromNative();
            assertTrue(NativeViewport.projectOpen());
            assertTrue(workspace.projectDirty());
            return NativeViewport.encodeProject();
        });
    }

    private void awaitAutosave() {
        final AutosaveController autosave = onWorkspace(rule.getScenario(),
                (activity, workspace) -> {
                    workspace.noteProjectMaybeDirty();
                    return workspace.autosaveController();
                });
        assertTrue(autosave.awaitIdle(10_000L));
    }

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    private static File scratch() {
        final File dir = new File(context().getCacheDir(), "home-flow");
        //noinspection ResultOfMethodCallIgnored
        dir.mkdirs();
        return dir;
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException error) {
            throw new AssertionError("could not write " + file, error);
        }
    }

    /** How many descendants of {@code root} a user could actually press. */
    private static int countClickable(View root) {
        int count = root.isClickable() && root.getVisibility() == View.VISIBLE ? 1 : 0;
        if (root instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); i++) {
                count += countClickable(group.getChildAt(i));
            }
        }
        return count;
    }
}
