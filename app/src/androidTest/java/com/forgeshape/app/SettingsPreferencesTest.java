package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closePrecision;
import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.selectConstructionTool;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.trailingHost;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * UIPREFR1-01..40 on the device: the Settings hub, the persisted preferences,
 * the mirrored rail zone and the gizmo appearance preferences, through the
 * real chrome.
 *
 * <p>Two things are proven together and they are not the same. One is that
 * each preference does what it says: the page is reachable from both doors,
 * the rail moves edge, the gizmo grows and thickens, and all of it survives
 * a recreation and a fresh read from disk. The other — the one the stage is
 * most careful about — is that <b>nothing below JNI moves</b>: every case
 * that changes a preference compares the project's bytes, its fingerprint,
 * its dirty flag, both history depths and the whole native snapshot before and
 * after, bit for bit.
 *
 * <p>No control is located by coordinate. The pixels that appear are the
 * gizmo's own handle points, asked for from native code.
 */
@RunWith(AndroidJUnit4.class)
public final class SettingsPreferencesTest {

    @Rule
    public final ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromTheDefaultsInAConstructionProject() {
        restoreDefaults();
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void leaveTheDefaultsBehind() {
        closeSettings();
        restoreDefaults();
        releaseOrientation(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // UIPREFR1-01 / 02 — two doors, one page
    // -----------------------------------------------------------------------

    @Test
    public void uiprefr1_01_settingsIsReachableFromHomeAndBackReturnsHome() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.homeVisible());
            workspace.findViewById(R.id.home_settings).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the Settings page stands", workspace.settingsVisible());
            assertFalse("and replaces Home rather than standing on it",
                    workspace.homeVisible());
            assertFalse("no project was created by browsing preferences",
                    NativeViewport.projectOpen());
            final View page = workspace.findViewById(R.id.settings_page);
            final View root = (View) page.getParent();
            assertEquals("a page owns the whole window", root.getWidth(), page.getWidth());
            assertEquals(root.getHeight(), page.getHeight());
            assertTrue("Back is ours while it stands", workspace.hasDismissibleSurface());
            assertTrue(workspace.dismissTopmostSurface());
            return null;
        });
        settleLayout();
        doOn(rule, (activity, workspace) -> {
            assertFalse(workspace.settingsVisible());
            assertTrue("Back returns to Home", workspace.homeVisible());
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void uiprefr1_02_settingsIsReachableFromTheProjectSurfaceOverAnOpenProject() {
        final long body = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
        openSettings();
        doOn(rule, (activity, workspace) -> {
            assertTrue(workspace.settingsVisible());
            assertTrue("the project stays open behind the page", NativeViewport.projectOpen());
            assertEquals(body, NativeViewport.sceneActiveBodyId());
            assertFalse("the editor chrome is withdrawn under an opaque page",
                    workspace.findViewById(R.id.global_toolbar).isShown());
            assertFalse("and the Project surface closed before the page opened",
                    workspace.projectPopover().isOpen());
            assertNotNull(workspace.findViewById(R.id.settings_back));
        });
        closeSettings();
        doOn(rule, (activity, workspace) -> {
            assertFalse(workspace.settingsVisible());
            assertTrue("Back returns to the workspace",
                    workspace.findViewById(R.id.global_toolbar).isShown());
            assertEquals(body, NativeViewport.sceneActiveBodyId());
        });
    }

    // -----------------------------------------------------------------------
    // UIPREFR1-03 / 04 / 12 / 29 / 32 — one store, and the defaults are the product
    // -----------------------------------------------------------------------

    @Test
    public void uiprefr1_03_04_theDefaultsReproduceTheProductExactly() {
        doOn(rule, (activity, workspace) -> {
            final AppPreferences current = AppPreferencesStore.current(activity);
            assertEquals(AppPreferences.defaults(), current);
            assertSame(AppTheme.WARM_GRAPHITE, workspace.appTheme());
            assertFalse(workspace.leftHanded());
            assertEquals("native holds the default visual size", 1.0f,
                    NativeViewport.gizmoVisualScale(), 0.0f);
            assertEquals("and the default stroke weight", NativeViewport.GIZMO_STROKE_REGULAR,
                    NativeViewport.gizmoStrokeWeight());
            assertEquals(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE,
                    NativeViewport.viewportBackground());
            assertSame("the page and the Activity read the same store",
                    workspace.currentPreferences(), AppPreferencesStore.current(activity));
        });
        assertRightHandedFrame("defaults");
    }

    // -----------------------------------------------------------------------
    // UIPREFR1-05 / 06 — the preferences survive recreation and a fresh read
    // -----------------------------------------------------------------------

    @Test
    public void uiprefr1_05_preferencesSurviveActivityRecreation() {
        openSettings();
        choose(R.id.handedness_left);
        choose(R.id.gizmo_size_large);
        choose(R.id.gizmo_weight_bold);
        closeSettings();
        rule.getScenario().recreate();
        settleLayout();
        waitForLayout(rule.getScenario(), false);
        doOn(rule, (activity, workspace) -> {
            final AppPreferences current = AppPreferencesStore.current(activity);
            assertSame(Handedness.LEFT, current.handedness());
            assertEquals(1.25f, current.gizmoVisualScale(), 0.0f);
            assertSame(GizmoStrokeWeight.BOLD, current.gizmoStrokeWeight());
            assertTrue("the rebuilt workspace seats the rail on the left",
                    workspace.leftHanded());
            assertEquals(1.25f, NativeViewport.gizmoVisualScale(), 0.0f);
            assertEquals(NativeViewport.GIZMO_STROKE_BOLD, NativeViewport.gizmoStrokeWeight());
        });
        assertLeftHandedFrame("after recreation");
    }

    @Test
    public void uiprefr1_06_preferencesAreOnDiskAndAFreshReadReturnsThem() {
        openSettings();
        choosePalette(R.id.appearance_cool_light, AppTheme.COOL_LIGHT);
        choose(R.id.handedness_left);
        choose(R.id.gizmo_size_largest);
        choose(R.id.gizmo_weight_thin);
        closeSettings();
        doOn(rule, (activity, workspace) -> {
            assertTrue(AppPreferencesStore.existsOnDisk(activity));
            final AppPreferences written = AppPreferencesStore.current(activity);
            // An instrumentation case cannot kill its own process, so the
            // in-memory copy is dropped and the next read is what a fresh
            // process would read from the file.
            AppPreferencesStore.dropCacheForVerification();
            final AppPreferences reread = AppPreferencesStore.current(activity);
            assertEquals("what was written is what a fresh process reads", written, reread);
            assertSame(AppTheme.COOL_LIGHT, reread.palette());
            assertSame(Handedness.LEFT, reread.handedness());
            assertEquals(1.5f, reread.gizmoVisualScale(), 0.0f);
            assertSame(GizmoStrokeWeight.THIN, reread.gizmoStrokeWeight());
        });
    }

    // -----------------------------------------------------------------------
    // UIPREFR1-07 / 08 — a bad file lands on the documented fallbacks
    // -----------------------------------------------------------------------

    @Test
    public void uiprefr1_07_08_unknownAndInvalidStoredValuesFallBackSafely() {
        doOn(rule, (activity, workspace) -> {
            AppPreferencesStore.plantForVerification(activity, "NEON_PINK", "AMBIDEXTROUS",
                    Float.NaN, "HAIRLINE", false, 42);
            assertEquals("unknown names and a NaN are the exact defaults",
                    AppPreferences.defaults(), AppPreferencesStore.current(activity));

            AppPreferencesStore.plantForVerification(activity, "COOL_LIGHT", "LEFT", 9.0f,
                    "THIN", false, AppPreferences.SCHEMA_VERSION);
            final AppPreferences clamped = AppPreferencesStore.current(activity);
            assertSame(AppTheme.COOL_LIGHT, clamped.palette());
            assertSame(Handedness.LEFT, clamped.handedness());
            assertEquals("an out-of-range size is clamped to the nearer bound",
                    AppPreferences.GIZMO_VISUAL_SCALE_MAX, clamped.gizmoVisualScale(), 0.0f);
            assertSame(GizmoStrokeWeight.THIN, clamped.gizmoStrokeWeight());

            AppPreferencesStore.plantForVerification(activity, "WARM_LIGHT", "RIGHT", -3.0f,
                    "BOLD", false, AppPreferences.SCHEMA_VERSION);
            assertEquals(AppPreferences.GIZMO_VISUAL_SCALE_MIN,
                    AppPreferencesStore.current(activity).gizmoVisualScale(), 0.0f);
            // Applying a clamped store to the live workspace pushes an
            // in-range value native accepts: nothing is refused.
            workspace.reapplyPreferencesForTest();
            assertEquals(AppPreferences.GIZMO_VISUAL_SCALE_MIN,
                    NativeViewport.gizmoVisualScale(), 0.0f);
            assertEquals(NativeViewport.GIZMO_STROKE_BOLD, NativeViewport.gizmoStrokeWeight());
        });
    }

    // -----------------------------------------------------------------------
    // UIPREFR1-09 / 10 / 11 / 25 / 37 — a preference is never project truth
    // -----------------------------------------------------------------------

    @Test
    public void uiprefr1_09_10_11_37_changingEveryPreferenceLeavesTheProjectBytesIdentical() {
        // A project with something in it: a turned cone with a half-typed
        // history, so a leaked preference would have somewhere to show.
        final byte[] before = readOn(rule, (activity, workspace) -> {
            NativeViewport.applyConstructionCone(1.5, 3.0);
            NativeViewport.applyBoxTransform(0.75, -0.25, 1.5, 10.0, 20.0, 30.0, 1.0, 1.0, 1.0);
            workspace.syncFromNative();
            return NativeViewport.encodeProject();
        });
        assertNotNull(before);
        final long[] baseline = readOn(rule, (activity, workspace) -> new long[]{
                NativeViewport.projectFingerprint(), NativeViewport.constructionUndoDepth(),
                NativeViewport.constructionRedoDepth(), NativeViewport.sculptUndoDepth(),
                workspace.projectDirty() ? 1L : 0L, NativeViewport.sceneActiveBodyId(),
                NativeViewport.constructionMeshRevision()});
        final double[] snapshot = readOn(rule, (activity, workspace) -> nativeSnapshot());

        openSettings();
        choose(R.id.handedness_left);
        choose(R.id.gizmo_size_largest);
        choose(R.id.gizmo_weight_bold);
        choosePalette(R.id.appearance_warm_light, AppTheme.WARM_LIGHT);
        choosePalette(R.id.appearance_cool_light, AppTheme.COOL_LIGHT);
        choose(R.id.gizmo_size_small);
        choose(R.id.gizmo_weight_thin);
        choose(R.id.handedness_right);
        closeSettings();

        doOn(rule, (activity, workspace) -> {
            assertArrayEquals("UIPREFR1-37: the .forge bytes are identical after every"
                    + " preference changed", before, NativeViewport.encodeProject());
            assertEquals("UIPREFR1-10: the fingerprint did not move",
                    baseline[0], NativeViewport.projectFingerprint());
            assertEquals("UIPREFR1-11: no Construction history step",
                    baseline[1], NativeViewport.constructionUndoDepth());
            assertEquals(baseline[2], NativeViewport.constructionRedoDepth());
            assertEquals("no Sculpt history step", baseline[3], NativeViewport.sculptUndoDepth());
            assertEquals("UIPREFR1-10: the dirty flag did not move",
                    baseline[4], workspace.projectDirty() ? 1L : 0L);
            assertEquals(baseline[5], NativeViewport.sceneActiveBodyId());
            assertEquals("no mesh was republished", baseline[6],
                    NativeViewport.constructionMeshRevision());
            final double[] after = nativeSnapshot();
            assertArrayEquals("UIPREFR1-25: nothing below JNI moved:"
                    + describeSnapshotDifference(snapshot, after), snapshot, after, 0.0);
        });
    }

    // -----------------------------------------------------------------------
    // UIPREFR1-13 / 14 / 15 / 16 / 17 / 18 — the mirrored rail zone
    // -----------------------------------------------------------------------

    @Test
    public void uiprefr1_13_14_leftHandedRailKeepsWidthTopAndAnEightDpInsetOnTheLeft() {
        final Rect right = hostFrame();
        openSettings();
        choose(R.id.handedness_left);
        closeSettings();
        final Rect left = hostFrame();
        doOn(rule, (activity, workspace) -> {
            final int inset = EditorControlStyles.dimen(activity, R.dimen.brush_gap);
            assertTrue(workspace.leftHanded());
            assertEquals("UIPREFR1-13: 8 dp off the left edge", inset,
                    left.left - chromePadding(workspace).left);
            assertEquals("UIPREFR1-14: the same fixed width", right.width(), left.width());
            assertEquals("and the same top", right.top, left.top);
            assertEquals("the host is the FIRST member of the row on the left",
                    trailingHost(workspace), workspace.workspaceMiddleRow().getChildAt(0));
            assertTrue("no second rail stands on the right",
                    left.right < workspace.getWidth() / 2);
        });
        assertLeftHandedFrame("left-handed");
        openSettings();
        choose(R.id.handedness_right);
        closeSettings();
        assertRightHandedFrame("back to right-handed");
        assertEquals("UIPREFR1-12: the accepted right frame is restored exactly", right,
                hostFrame());
    }

    @Test
    public void uiprefr1_15_17_theSidePlacedPrecisionSurfaceOpensInwardOfALeftRail() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        openSettings();
        choose(R.id.handedness_left);
        closeSettings();
        doOn(rule, (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            openPrecision(workspace);
        });
        settleLayout();
        doOn(rule, (activity, workspace) -> {
            final PropertyInspectorView inspector = workspace.propertyInspector();
            assertTrue(inspector.isOpen());
            assertEquals("a short window places the surface beside the model",
                    WorkspaceLayoutMode.InspectorPlacement.SIDE_OVERLAY,
                    workspace.inspectorPlacement());
            final LinearLayout row = workspace.workspaceMiddleRow();
            assertSame("seated AFTER the host, inboard of a left rail",
                    trailingHost(workspace), row.getChildAt(0));
            assertSame(inspector, row.getChildAt(1));
            final Rect host = frameOf(workspace, trailingHost(workspace));
            final Rect panel = frameOf(workspace, inspector);
            final int gap = EditorControlStyles.dimen(activity, R.dimen.overlay_anchor_gap);
            assertTrue("UIPREFR1-15: opens toward the interior, clear of the rail: panel.left="
                            + panel.left + " host.right=" + host.right,
                    panel.left >= host.right + gap - 1);
            assertFalse("and never overlaps it", Rect.intersects(host, panel));
        });
        // UIPREFR1-17, through the page: opening Settings is an opaque page
        // over the workspace, so it CLOSES the precision surface (and its
        // keyboard) before the switch rather than leaving one standing on a
        // model the user cannot see; nothing is left detached on the old side.
        openSettings();
        doOn(rule, (activity, workspace) -> {
            assertFalse("the page closed Exact before anything moved",
                    workspace.propertyInspector().isOpen());
        });
        choose(R.id.handedness_right);
        closeSettings();
        doOn(rule, (activity, workspace) -> {
            assertFalse(workspace.propertyInspector().isOpen());
            openPrecision(workspace);
        });
        settleLayout();
        doOn(rule, (activity, workspace) -> {
            final PropertyInspectorView inspector = workspace.propertyInspector();
            assertTrue(inspector.isOpen());
            final LinearLayout row = workspace.workspaceMiddleRow();
            final int last = row.getChildCount() - 1;
            assertSame("seated BEFORE the host again on the right",
                    trailingHost(workspace), row.getChildAt(last));
            assertSame(inspector, row.getChildAt(last - 1));
            final Rect host = frameOf(workspace, trailingHost(workspace));
            final Rect panel = frameOf(workspace, inspector);
            assertTrue(panel.right <= host.left);
        });
        // UIPREFR1-17, the re-seat itself: a handedness change applied while
        // Exact IS open — the store changed behind the page, as a synced or
        // planted preference would — re-seats the open surface on the new
        // edge rather than leaving it detached on the old one.
        doOn(rule, (activity, workspace) -> {
            AppPreferencesStore.update(activity,
                    AppPreferencesStore.current(activity).withHandedness(Handedness.LEFT));
            workspace.reapplyPreferencesForTest();
        });
        settleLayout();
        doOn(rule, (activity, workspace) -> {
            final PropertyInspectorView inspector = workspace.propertyInspector();
            assertTrue("Exact stays open across a live re-seat", inspector.isOpen());
            assertTrue(workspace.leftHanded());
            final LinearLayout row = workspace.workspaceMiddleRow();
            assertSame(trailingHost(workspace), row.getChildAt(0));
            assertSame("re-seated AFTER the host on the left", inspector, row.getChildAt(1));
            final Rect host = frameOf(workspace, trailingHost(workspace));
            final Rect panel = frameOf(workspace, inspector);
            assertTrue("clear of the rail, on the new side", panel.left >= host.right);
            assertFalse(Rect.intersects(host, panel));
            closePrecision(workspace);
        });
    }

    @Test
    public void uiprefr1_16_18_switchingHandednessPreservesTheToolAndMirrorsNoMath() {
        final float[] handleBefore = new float[2];
        final float[] cameraBefore = new float[8];
        final double[] snapshotBefore = readOn(rule, (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            workspace.onTransformModeRequested(NativeViewport.GIZMO_MODE_ROTATE);
            assertTrue(NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_AXIS_X,
                    handleBefore));
            NativeViewport.debugCameraPose(cameraBefore);
            return nativeSnapshot();
        });
        final long body = readOn(rule, (activity, workspace) ->
                NativeViewport.sceneActiveBodyId());
        openSettings();
        choose(R.id.handedness_left);
        closeSettings();
        doOn(rule, (activity, workspace) -> {
            assertTrue(workspace.leftHanded());
            assertEquals("UIPREFR1-16: the tool is kept", EditorUiState.CONSTRUCTION_TOOL_TRANSFORM,
                    workspace.uiState().constructionTool());
            final double[] gizmo = new double[NativeViewport.GIZMO_STATE_SIZE];
            NativeViewport.gizmoState(gizmo);
            assertEquals("and the gizmo mode", NativeViewport.GIZMO_MODE_ROTATE,
                    (int) gizmo[NativeViewport.GIZMO_MODE]);
            assertEquals("and the selection", body, NativeViewport.sceneActiveBodyId());
            assertEquals(NativeViewport.MODE_CONSTRUCTION, NativeViewport.productMode());
            // UIPREFR1-18: the world did not mirror. The X handle projects to the
            // same pixel, the camera is where it was, and nothing native moved.
            final float[] handleAfter = new float[2];
            assertTrue(NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_AXIS_X,
                    handleAfter));
            assertArrayEquals("the X axis still points where it pointed", handleBefore,
                    handleAfter, 0.0f);
            final float[] cameraAfter = new float[8];
            NativeViewport.debugCameraPose(cameraAfter);
            assertArrayEquals(cameraBefore, cameraAfter, 0.0f);
            final double[] after = nativeSnapshot();
            assertArrayEquals(describeSnapshotDifference(snapshotBefore, after),
                    snapshotBefore, after, 0.0);
            assertEquals("no transform was written", 0, NativeViewport.constructionUndoDepth());
        });
    }

    // -----------------------------------------------------------------------
    // UIPREFR1-28..36 — the gizmo's visual size and thickness
    // -----------------------------------------------------------------------

    @Test
    public void uiprefr1_28_33_gizmoSizeAndThicknessAreBoundedAndReachNative() {
        openSettings();
        final int[] sizeRows = {R.id.gizmo_size_small, R.id.gizmo_size_default,
                R.id.gizmo_size_large, R.id.gizmo_size_largest};
        for (int i = 0; i < sizeRows.length; i++) {
            choose(sizeRows[i]);
            final float expected = AppPreferences.GIZMO_VISUAL_SCALE_PRESETS[i];
            final int row = sizeRows[i];
            doOn(rule, (activity, workspace) -> {
                assertEquals(expected, NativeViewport.gizmoVisualScale(), 0.0f);
                assertEquals(expected, AppPreferencesStore.current(activity).gizmoVisualScale(),
                        0.0f);
                assertTrue("the chosen row is drawn chosen, in every channel",
                        workspace.settingsPage().optionChosen(row));
            });
        }
        final int[] weightRows = {R.id.gizmo_weight_thin, R.id.gizmo_weight_regular,
                R.id.gizmo_weight_bold};
        final int[] weights = {NativeViewport.GIZMO_STROKE_THIN,
                NativeViewport.GIZMO_STROKE_REGULAR, NativeViewport.GIZMO_STROKE_BOLD};
        for (int i = 0; i < weightRows.length; i++) {
            choose(weightRows[i]);
            final int expected = weights[i];
            doOn(rule, (activity, workspace) -> {
                assertEquals(expected, NativeViewport.gizmoStrokeWeight());
            });
        }
        closeSettings();
        doOn(rule, (activity, workspace) -> {
            // UIPREFR1-28 / 31: native refuses what the model cannot hold, and
            // the value in force stands.
            final float inForce = NativeViewport.gizmoVisualScale();
            assertFalse(NativeViewport.setGizmoVisualScale(0.5f));
            assertFalse(NativeViewport.setGizmoVisualScale(2.0f));
            assertFalse(NativeViewport.setGizmoVisualScale(Float.NaN));
            assertEquals(inForce, NativeViewport.gizmoVisualScale(), 0.0f);
            assertEquals(NativeViewport.GIZMO_STROKE_BOLD, NativeViewport.setGizmoStrokeWeight(7));
            assertEquals(NativeViewport.GIZMO_STROKE_BOLD, NativeViewport.setGizmoStrokeWeight(-1));
        });
    }

    @Test
    public void uiprefr1_30_35_36_everyHandleIsPickableAtTheSmallestAndLargestSize() {
        doOn(rule, (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
        });
        settleLayout();
        final int[] sizeRows = {R.id.gizmo_size_small, R.id.gizmo_size_largest,
                R.id.gizmo_size_default};
        for (int sizeRow : sizeRows) {
            openSettings();
            choose(sizeRow);
            closeSettings();
            doOn(rule, (activity, workspace) -> {
                final float scale = NativeViewport.gizmoVisualScale();
                final int[] modes = {NativeViewport.GIZMO_MODE_MOVE,
                        NativeViewport.GIZMO_MODE_ROTATE, NativeViewport.GIZMO_MODE_SCALE};
                for (int mode : modes) {
                    workspace.onTransformModeRequested(mode);
                    final double[] gizmo = new double[NativeViewport.GIZMO_STATE_SIZE];
                    NativeViewport.gizmoState(gizmo);
                    assertEquals(1.0, gizmo[NativeViewport.GIZMO_VISIBLE], 0.0);
                    final int[] handles = mode == NativeViewport.GIZMO_MODE_ROTATE
                            ? new int[]{NativeViewport.GIZMO_HANDLE_AXIS_X,
                                    NativeViewport.GIZMO_HANDLE_AXIS_Y,
                                    NativeViewport.GIZMO_HANDLE_AXIS_Z}
                            : mode == NativeViewport.GIZMO_MODE_SCALE
                                    ? new int[]{NativeViewport.GIZMO_HANDLE_UNIFORM,
                                            NativeViewport.GIZMO_HANDLE_PLANE_XY,
                                            NativeViewport.GIZMO_HANDLE_PLANE_XZ,
                                            NativeViewport.GIZMO_HANDLE_PLANE_YZ,
                                            NativeViewport.GIZMO_HANDLE_AXIS_X,
                                            NativeViewport.GIZMO_HANDLE_AXIS_Y,
                                            NativeViewport.GIZMO_HANDLE_AXIS_Z}
                                    : new int[]{NativeViewport.GIZMO_HANDLE_PLANE_XY,
                                            NativeViewport.GIZMO_HANDLE_PLANE_XZ,
                                            NativeViewport.GIZMO_HANDLE_PLANE_YZ,
                                            NativeViewport.GIZMO_HANDLE_AXIS_X,
                                            NativeViewport.GIZMO_HANDLE_AXIS_Y,
                                            NativeViewport.GIZMO_HANDLE_AXIS_Z};
                    for (int handle : handles) {
                        final float[] pixel = new float[2];
                        assertTrue("scale " + scale + " mode " + mode + " handle " + handle
                                + " has a grab point", NativeViewport.gizmoHandlePoint(handle, pixel));
                        assertEquals("UIPREFR1-35/36: at visual size " + scale + " in mode "
                                        + mode + ", handle " + handle + " is grabbed at its own pixel",
                                handle, NativeViewport.gizmoHitTest(pixel[0], pixel[1]));
                    }
                }
                workspace.onTransformModeRequested(NativeViewport.GIZMO_MODE_MOVE);
            });
        }
    }

    @Test
    public void uiprefr1_33_thicknessAndSizeChangeNoHitSemanticsOrHistory() {
        final float[][] pixels = new float[3][2];
        doOn(rule, (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            for (int i = 0; i < 3; i++) {
                assertTrue(NativeViewport.gizmoHandlePoint(
                        NativeViewport.GIZMO_HANDLE_AXIS_X + i, pixels[i]));
            }
        });
        openSettings();
        choose(R.id.gizmo_weight_bold);
        choose(R.id.gizmo_weight_thin);
        closeSettings();
        doOn(rule, (activity, workspace) -> {
            for (int i = 0; i < 3; i++) {
                final float[] after = new float[2];
                assertTrue(NativeViewport.gizmoHandlePoint(
                        NativeViewport.GIZMO_HANDLE_AXIS_X + i, after));
                assertArrayEquals("UIPREFR1-33: a stroke weight moves no handle",
                        pixels[i], after, 0.0f);
                assertEquals(NativeViewport.GIZMO_HANDLE_AXIS_X + i,
                        NativeViewport.gizmoHitTest(after[0], after[1]));
            }
            assertEquals("and records nothing", 0, NativeViewport.constructionUndoDepth());
        });
    }

    // -----------------------------------------------------------------------
    // UIPREFR1-22..24 / 26 — the two light palettes are real and readable
    // -----------------------------------------------------------------------

    @Test
    public void uiprefr1_22_26_lightPalettesAreLightReadableAndFlipTheSystemBars() {
        for (final AppTheme theme : new AppTheme[]{AppTheme.WARM_LIGHT, AppTheme.COOL_LIGHT}) {
            openSettings();
            choosePalette(theme == AppTheme.WARM_LIGHT
                    ? R.id.appearance_warm_light : R.id.appearance_cool_light, theme);
            doOn(rule, (activity, workspace) -> {
                assertSame(theme, workspace.appTheme());
                assertEquals(theme.viewportBackground(), NativeViewport.viewportBackground());
                assertTrue("dark icons over a light canvas", activity.systemBarsLight());
                final int window = EditorControlStyles.themeColor(activity,
                        android.R.attr.windowBackground);
                final int primary = EditorControlStyles.themeColor(activity, R.attr.fsTextPrimary);
                final int secondary = EditorControlStyles.themeColor(activity,
                        R.attr.fsTextSecondary);
                final int error = EditorControlStyles.themeColor(activity, R.attr.fsTextError);
                for (int ground : new int[]{android.R.attr.windowBackground,
                        R.attr.fsChromeSurface, R.attr.fsSurfaceContext,
                        R.attr.fsSurfacePrecision, R.attr.fsControlSurface,
                        R.attr.fsFieldSurface, R.attr.fsStartPageSurface}) {
                    final int background = EditorControlStyles.themeColor(activity, ground);
                    final String name = theme + "/"
                            + activity.getResources().getResourceEntryName(ground);
                    assertTrue(name + " primary " + contrast(primary, background),
                            contrast(primary, background) >= 4.5);
                    assertTrue(name + " secondary " + contrast(secondary, background),
                            contrast(secondary, background) >= 4.5);
                    assertTrue(name + " error " + contrast(error, background),
                            contrast(error, background) >= 4.5);
                }
                assertTrue(theme + " is a light canvas", luminance(window) > 0.6);
                // The same component tree: the Settings page is on screen in
                // this palette with every row it has in the dark ones.
                assertTrue(workspace.settingsVisible());
                assertNotNull(workspace.settingsPage().findViewById(R.id.gizmo_weight_bold));
                assertTrue(workspace.settingsPage().optionChosen(theme == AppTheme.WARM_LIGHT
                        ? R.id.appearance_warm_light : R.id.appearance_cool_light));
            });
            closeSettings();
        }
        // And the two are materially distinct from each other.
        final int[] warm = resolvedGrounds(AppTheme.WARM_LIGHT);
        final int[] cool = resolvedGrounds(AppTheme.COOL_LIGHT);
        int differing = 0;
        for (int i = 0; i < warm.length; i++) {
            if (warm[i] != cool[i]) {
                differing++;
            }
        }
        assertTrue("Warm Light and Cool Light differ on their grounds", differing >= 3);
        openSettings();
        choosePalette(R.id.appearance_warm_graphite, AppTheme.WARM_GRAPHITE);
        doOn(rule, (activity, workspace) -> {
            assertFalse("light icons return over a dark canvas", activity.systemBarsLight());
        });
        closeSettings();
    }

    @Test
    public void uiprefr1_40_theSettingsPageOffersExactlyTheApprovedRowsAndNoHandleStyle() {
        openSettings();
        doOn(rule, (activity, workspace) -> {
            final View page = workspace.settingsPage();
            final int floor = EditorControlStyles.dimen(activity, R.dimen.control_height);
            final int[] rows = {R.id.appearance_warm_graphite, R.id.appearance_neutral_charcoal,
                    R.id.appearance_light_charcoal, R.id.appearance_warm_light,
                    R.id.appearance_cool_light, R.id.handedness_right, R.id.handedness_left,
                    R.id.gizmo_size_small, R.id.gizmo_size_default, R.id.gizmo_size_large,
                    R.id.gizmo_size_largest, R.id.gizmo_weight_thin, R.id.gizmo_weight_regular,
                    R.id.gizmo_weight_bold, R.id.tool_labels_off, R.id.tool_labels_on,
                    R.id.settings_back};
            for (int id : rows) {
                final View row = page.findViewById(id);
                final String name = activity.getResources().getResourceEntryName(id);
                assertNotNull(name, row);
                assertTrue(name + " is clickable", row.isClickable());
                assertTrue(name + " carries a content description",
                        row.getContentDescription() != null
                                && row.getContentDescription().length() > 0);
                assertTrue(name + " is " + row.getHeight() + " px tall, floor " + floor,
                        row.getHeight() >= floor);
            }
            // `CAD-VERTICAL-SLICE-R1` added Interface's two Tool Labels rows.
            assertEquals("sixteen preference rows and Back, and nothing else is pressable",
                    rows.length, countClickable(workspace.findViewById(R.id.settings_panel)));
            // The chosen rows say so in more than colour.
            assertTrue(workspace.settingsPage().optionChosen(R.id.handedness_right));
            assertTrue(((android.widget.TextView) page.findViewById(R.id.handedness_right))
                    .getText().toString().startsWith("✓"));
            assertFalse(workspace.settingsPage().optionChosen(R.id.handedness_left));
            assertTrue("Tool Labels is OFF by default",
                    workspace.settingsPage().optionChosen(R.id.tool_labels_off));
            assertFalse(workspace.settingsPage().optionChosen(R.id.tool_labels_on));
            assertNull("no Handle Style row is drawn: the variant is deferred, not faked",
                    page.findViewWithTag("handle_style"));
        });
        closeSettings();
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    private void restoreDefaults() {
        final Boolean wearingDefault = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.appTheme() == AppTheme.defaultTheme());
        if (!Boolean.TRUE.equals(wearingDefault)) {
            activityBeforePalette = readOn(rule, (activity, workspace) ->
                    System.identityHashCode(activity));
            doOn(rule, (activity, workspace) -> activity.requestTheme(
                    AppTheme.defaultTheme()));
            waitForPalette(AppTheme.defaultTheme());
        }
        doOn(rule, (activity, workspace) -> {
            AppPreferencesStore.resetForVerification(activity);
            workspace.reapplyPreferencesForTest();
        });
        settleLayout();
    }

    private void openSettings() {
        doOn(rule, (activity, workspace) -> {
            if (workspace.settingsVisible()) {
                return;
            }
            if (workspace.homeVisible()) {
                workspace.findViewById(R.id.home_settings).performClick();
                return;
            }
            workspace.findViewById(R.id.project_actions_button).performClick();
            workspace.findViewById(R.id.project_settings).performClick();
        });
        settleLayout();
        doOn(rule, (activity, workspace) -> assertTrue(workspace.settingsVisible()));
    }

    private void closeSettings() {
        doOn(rule, (activity, workspace) -> {
            if (workspace.settingsVisible()) {
                workspace.findViewById(R.id.settings_back).performClick();
            }
        });
        settleLayout();
    }

    /** Presses one option row on the open Settings page. */
    private void choose(final int id) {
        doOn(rule, (activity, workspace) -> {
            assertTrue("Settings must be open to choose", workspace.settingsVisible());
            workspace.settingsPage().findViewById(id).performClick();
        });
        settleLayout();
    }

    /** The Activity the last palette change left; a change must replace it. */
    private int activityBeforePalette;

    /** Presses one palette row and waits for the Activity it recreates. */
    private void choosePalette(final int row, final AppTheme theme) {
        activityBeforePalette = readOn(rule, (activity, workspace) ->
                System.identityHashCode(activity));
        choose(row);
        waitForPalette(theme);
    }

    /**
     * Waits for the Activity a palette change recreates: a NEW instance, laid
     * out, wearing the palette. The store answers the new palette before the
     * recreation has even begun, so the palette alone is not the signal.
     */
    private void waitForPalette(final AppTheme theme) {
        for (int attempt = 0; attempt < 100; attempt++) {
            settleLayout();
            final Boolean ready = onWorkspace(rule.getScenario(),
                    (activity, workspace) -> workspace.appTheme() == theme
                            && workspace.getWidth() > 0
                            && System.identityHashCode(activity) != activityBeforePalette
                            && EditorControlStyles.themeColor(activity,
                                    android.R.attr.windowBackground)
                            == activity.getColor(windowColorOf(theme)));
            if (Boolean.TRUE.equals(ready)) {
                return;
            }
            SystemClock.sleep(100);
        }
        throw new AssertionError("the workspace never came back wearing " + theme);
    }

    private static int windowColorOf(AppTheme theme) {
        switch (theme) {
            case NEUTRAL_CHARCOAL: return R.color.p2_viewport_background;
            case LIGHT_CHARCOAL: return R.color.p3_viewport_background;
            case WARM_LIGHT: return R.color.p4_viewport_background;
            case COOL_LIGHT: return R.color.p5_viewport_background;
            default: return R.color.p1_viewport_background;
        }
    }

    private Rect hostFrame() {
        settleLayout();
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> frameOf(workspace, trailingHost(workspace)));
    }

    private static Rect frameOf(EditorWorkspaceView workspace, View view) {
        final Rect rect = new Rect(0, 0, view.getWidth(), view.getHeight());
        workspace.offsetDescendantRectToMyCoords(view, rect);
        return rect;
    }

    private static Rect chromePadding(EditorWorkspaceView workspace) {
        // The chrome's inset off the bars, read from the overlay that shares it.
        final View overlay = (View) workspace.findViewById(R.id.restore_ui_chip).getParent();
        return new Rect(overlay.getPaddingLeft(), overlay.getPaddingTop(),
                overlay.getPaddingRight(), overlay.getPaddingBottom());
    }

    private void assertRightHandedFrame(final String where) {
        final Rect frame = hostFrame();
        doOn(rule, (activity, workspace) -> {
            final int inset = EditorControlStyles.dimen(activity, R.dimen.brush_gap);
            final int width = EditorControlStyles.dimen(activity, R.dimen.trailing_host_width);
            assertFalse(where + ": right-handed", workspace.leftHanded());
            assertEquals(where + ": the accepted 8 dp right inset (UI-LAYOUT-R2)", inset,
                    workspace.getWidth() - chromePadding(workspace).right - frame.right);
            assertEquals(where + ": the accepted width", width, frame.width());
            final LinearLayout row = workspace.workspaceMiddleRow();
            assertSame(where + ": the host is the last member of the row",
                    trailingHost(workspace), row.getChildAt(row.getChildCount() - 1));
            final LinearLayout.LayoutParams params =
                    (LinearLayout.LayoutParams) trailingHost(workspace).getLayoutParams();
            assertEquals(inset, params.rightMargin);
            assertEquals(0, params.leftMargin);
        });
    }

    private void assertLeftHandedFrame(final String where) {
        final Rect frame = hostFrame();
        doOn(rule, (activity, workspace) -> {
            final int inset = EditorControlStyles.dimen(activity, R.dimen.brush_gap);
            final int width = EditorControlStyles.dimen(activity, R.dimen.trailing_host_width);
            assertTrue(where + ": left-handed", workspace.leftHanded());
            assertEquals(where + ": 8 dp off the left edge", inset,
                    frame.left - chromePadding(workspace).left);
            assertEquals(where + ": the accepted width", width, frame.width());
            final LinearLayout.LayoutParams params =
                    (LinearLayout.LayoutParams) trailingHost(workspace).getLayoutParams();
            assertEquals(inset, params.leftMargin);
            assertEquals(0, params.rightMargin);
        });
    }

    private int[] resolvedGrounds(final AppTheme theme) {
        openSettings();
        choosePalette(theme == AppTheme.WARM_LIGHT ? R.id.appearance_warm_light
                : R.id.appearance_cool_light, theme);
        final int[] grounds = onWorkspace(rule.getScenario(), (activity, workspace) -> new int[]{
                EditorControlStyles.themeColor(activity, android.R.attr.windowBackground),
                EditorControlStyles.themeColor(activity, R.attr.fsChromeSurface),
                EditorControlStyles.themeColor(activity, R.attr.fsSurfacePrecision),
                EditorControlStyles.themeColor(activity, R.attr.fsFieldSurface),
                EditorControlStyles.themeColor(activity, R.attr.fsTextPrimary)});
        closeSettings();
        return grounds;
    }

    private static int countClickable(View root) {
        int count = root.isClickable() && root.getVisibility() == View.VISIBLE ? 1 : 0;
        if (root instanceof android.view.ViewGroup) {
            final android.view.ViewGroup group = (android.view.ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); i++) {
                count += countClickable(group.getChildAt(i));
            }
        }
        return count;
    }

    private static double contrast(int foreground, int background) {
        final double a = luminance(foreground);
        final double b = luminance(background);
        return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05);
    }

    private static double luminance(int color) {
        return 0.2126 * channel(android.graphics.Color.red(color))
                + 0.7152 * channel(android.graphics.Color.green(color))
                + 0.0722 * channel(android.graphics.Color.blue(color));
    }

    private static double channel(int value) {
        final double c = value / 255.0;
        return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
    }

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    /** A block for its effect, on the UI thread, with the rule's scenario. */
    private static void doOn(ActivityScenarioRule<ForgeShapeActivity> rule,
                                      final WorkspaceEffect effect) {
        WorkspaceTestSupport.doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            effect.run(activity, workspace);
            return null;
        });
    }

    private interface WorkspaceEffect {
        void run(ForgeShapeActivity activity, EditorWorkspaceView workspace);
    }

    private static <T> T readOn(ActivityScenarioRule<ForgeShapeActivity> rule,
                                     WorkspaceTestSupport.WorkspaceAction<T> action) {
        return WorkspaceTestSupport.onWorkspace(rule.getScenario(), action);
    }
}
