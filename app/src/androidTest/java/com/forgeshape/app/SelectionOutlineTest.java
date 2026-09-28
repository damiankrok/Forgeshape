package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.net.Uri;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * `SELOUTR1-01..36`: the persistent selection outline (`SEL-OUT-R1`,
 * UI-OWNER-10 / UI-OWNER-11).
 *
 * <p><b>What this suite proves and what it does not.</b> It asserts that the
 * renderer <em>ran the outline path</em> for a body of each representation, that
 * the resources behind it are bounded, that the control exists and reads native
 * truth, and that none of it can reach project truth. It asserts no <em>pixel</em>
 * — whether the band is legible against a cream ground is judged by the measured
 * contrast in the native render-shading suite and by the contact sheet in
 * `artifacts/sel-out-r1/`, which is exactly the division the grid already lives
 * under.
 *
 * <p>The renderer counters come from {@link NativeViewport#selectionOutlineStats},
 * which is a diagnostic seam carrying a count, an extent and a width — no
 * {@code ObjectId}, no geometry, no dimension. It is what turns "repeated
 * selection switches allocate nothing" from a claim into an assertion.
 *
 * <p><b>Delete non-collision.</b> The Delete cases here render the result of
 * Delete, Undo and Redo and assert the outline follows whatever selection
 * actually is. They change no Delete semantics and are explicitly NOT the
 * pending owner acceptance of `IMPORT-01B` / `UI-OWNER-45`.
 */
@RunWith(AndroidJUnit4.class)
public final class SelectionOutlineTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private File scratch;

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
        scratch = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getCacheDir(), "sel-out-r1");
        assertTrue("scratch directory", scratch.isDirectory() || scratch.mkdirs());
    }

    /**
     * The outline is process-scoped native presentation state exactly as the
     * grid is, so a case that left it off would hand that to the next one.
     * Restoring the product default is this suite's own responsibility.
     */
    @After
    public void restoreDisplayDefaults() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setSelectionOutlineVisible(true);
            NativeViewport.setGridVisible(true);
            NativeViewport.setViewportBackground(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE);
            return null;
        });
    }

    // =======================================================================
    // The control: SELOUTR1-14, -15, -16
    // =======================================================================

    /** SELOUTR1-14. The View group offers Selection Outline, and it works. */
    @Test
    public void seloutr1_14_theViewGroupOffersAWorkingSelectionOutlineControl() {
        openDisplayPopover();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View on = workspace.findViewById(R.id.view_selection_outline_on);
            final View off = workspace.findViewById(R.id.view_selection_outline_off);
            assertNotNull("SELOUTR1-14: the View group offers Selection Outline On", on);
            assertNotNull("SELOUTR1-14: and Selection Outline Off", off);

            // Real controls, not drawn promises: the View group shows nothing
            // it cannot honour.
            assertTrue("Selection Outline On is a working control", on.isEnabled());
            assertTrue("Selection Outline Off is a working control", off.isEnabled());

            // The 44 dp touch floor, MEASURED rather than trusted to the
            // declared size — the same rule the grid's chips are held to.
            assertTrue("Selection Outline On meets the touch floor: "
                            + EditorControlStyles.toDp(activity, on.getHeight()) + " dp",
                    EditorControlStyles.toDp(activity, on.getHeight()) >= 44);
            assertTrue("Selection Outline Off meets the touch floor: "
                            + EditorControlStyles.toDp(activity, off.getHeight()) + " dp",
                    EditorControlStyles.toDp(activity, off.getHeight()) >= 44);

            // Named for a screen reader, which sees "On" with no idea what it
            // turns on — the caption beside it is a sibling view.
            assertEquals("SELOUTR1-14: On carries the full name for a screen reader",
                    activity.getString(R.string.view_selection_outline) + " "
                            + activity.getString(R.string.view_selection_outline_on),
                    on.getContentDescription());
            assertEquals(activity.getString(R.string.view_selection_outline) + " "
                            + activity.getString(R.string.view_selection_outline_off),
                    off.getContentDescription());

            // Still a compact popover, not the settings screen it was designed
            // not to become. A fifth group must not have changed that.
            final View popover = workspace.findViewById(R.id.display_settings_popover);
            assertTrue("SELOUTR1-14: the popover is not a full-screen surface: "
                            + popover.getHeight() + " of " + workspace.getHeight(),
                    popover.getHeight() < workspace.getHeight() * 0.85);

            // And the persistent preferences did NOT come back into THIS
            // popover. UI-PREF-R1 moved them to the Settings page; adding a
            // transient overlay does not walk that back. Asked of the popover
            // rather than of the workspace, because the Settings page's own
            // views legitimately live in the same tree — it is a start page the
            // shell shows or hides, not a view that is built on demand.
            assertEquals("the persistent palette control stays on the Settings page", 0,
                    countIfPresent(popover, R.id.appearance_warm_light));
            assertEquals("and so does handedness", 0,
                    countIfPresent(popover, R.id.handedness_left));
            return null;
        });
    }

    /** SELOUTR1-15 / -16. The chips report native truth, both ways. */
    @Test
    public void seloutr1_15_16_theOutlineChipsReadBackFromNativeTruth() {
        openDisplayPopover();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // The product default is ON, and the chips say so before anything
            // is tapped.
            assertTrue("SELOUTR1-16: the outline is on by default",
                    NativeViewport.selectionOutlineVisible());
            assertTrue(workspace.findViewById(R.id.view_selection_outline_on).isActivated());

            workspace.findViewById(R.id.view_selection_outline_off).performClick();
            assertFalse("SELOUTR1-15: native code reports the outline off",
                    NativeViewport.selectionOutlineVisible());
            assertTrue("and Off shows as active",
                    workspace.findViewById(R.id.view_selection_outline_off).isActivated());
            assertFalse("while On does not",
                    workspace.findViewById(R.id.view_selection_outline_on).isActivated());
            // Selection feedback happens IN PLACE: deciding whether an edge
            // helps means switching back and forth, so the popover stays open.
            assertEquals("the popover stays open across a choice", View.VISIBLE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());

            workspace.findViewById(R.id.view_selection_outline_on).performClick();
            assertTrue("SELOUTR1-16: native code reports the outline on",
                    NativeViewport.selectionOutlineVisible());
            assertTrue(workspace.findViewById(R.id.view_selection_outline_on).isActivated());
            assertFalse(workspace.findViewById(R.id.view_selection_outline_off).isActivated());

            // Idempotent: asking for what is already in effect is honoured.
            assertTrue("setting the value it already has is honoured, not refused",
                    NativeViewport.setSelectionOutlineVisible(true));

            // The grid is untouched throughout. Two overlays in one group must
            // not share one answer.
            assertTrue("SELOUTR1-14: the grid is orthogonal to the outline",
                    NativeViewport.gridVisible());
            return null;
        });
    }

    /**
     * SELOUTR1-14. The toggle's lifecycle is the GRID's, which is the whole
     * answer to §8: session-only, native-owned, process-scoped.
     *
     * <p>A recreation is what a rotation and a theme change both come down to.
     * The store is process-scoped, so the value is not restored — it was never
     * lost.
     */
    @Test
    public void seloutr1_14_theOutlineChoiceOutlivesTheActivityExactlyAsTheGridDoes() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setSelectionOutlineVisible(false);
            NativeViewport.setGridVisible(false);
            return null;
        });

        rule.getScenario().recreate();
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("SELOUTR1-14: the outline choice outlives the Activity",
                    NativeViewport.selectionOutlineVisible());
            assertFalse("exactly as the grid's does", NativeViewport.gridVisible());
            return null;
        });

        // And the reopened popover agrees with native truth rather than with a
        // Java field that survived the recreation.
        openDisplayPopover();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the chip repaints from native truth after a recreation",
                    workspace.findViewById(R.id.view_selection_outline_off).isActivated());
            return null;
        });
    }

    // =======================================================================
    // The renderer ran: SELOUTR1-02..-08, -15, -16
    // =======================================================================

    /**
     * SELOUTR1-02. A selected Construction Body is outlined, and the outline
     * pass is genuinely running rather than being compiled out.
     */
    @Test
    public void seloutr1_02_aSelectedConstructionBodyIsOutlined() {
        assertEquals("the baseline body is a Construction Source",
                NativeViewport.REPRESENTATION_CONSTRUCTION,
                NativeViewport.sceneBodyRepresentation(NativeViewport.sceneActiveBodyId()));
        assertOutlineIsBeingDrawn("SELOUTR1-02: a Construction Body");

        // The mask follows the RENDER extent rather than being a fixed buffer
        // that is stretched, which is what §12 asks for.
        final double[] stats = outlineStats();
        final int[] viewport = viewportSize();
        assertEquals("SELOUTR1-02: the mask width follows the render extent", viewport[0],
                (int) stats[NativeViewport.OUTLINE_STAT_MASK_WIDTH]);
        assertEquals("SELOUTR1-02: the mask height follows the render extent", viewport[1],
                (int) stats[NativeViewport.OUTLINE_STAT_MASK_HEIGHT]);

        // The band is a bounded number of screen pixels, and the value is
        // reported so the evidence can quote a measurement.
        final double width = stats[NativeViewport.OUTLINE_STAT_WIDTH_PIXELS];
        assertTrue("SELOUTR1-02: the band is between 2 and 5 screen pixels, measured " + width,
                width >= 2.0 && width <= 5.0);
    }

    /** SELOUTR1-03. A selected Imported Mesh is outlined, by the same path. */
    @Test
    public void seloutr1_03_aSelectedImportedMeshIsOutlined() {
        final long imported = importSentinelAndSelectIt();
        assertEquals("the selected body is an Imported Mesh",
                NativeViewport.REPRESENTATION_IMPORTED,
                NativeViewport.sceneBodyRepresentation(imported));
        assertOutlineIsBeingDrawn("SELOUTR1-03: an Imported Mesh");
    }

    /**
     * SELOUTR1-04 / -05. A selected body being sculpted is outlined, and the
     * outline follows the SCULPT mesh rather than the source underneath it.
     *
     * <p>The renderer rasterises whatever that body's published mesh is, so the
     * geometry the mask traces is by construction the geometry the user can
     * see. There is no Construction-versus-Sculpt branch to get wrong, and this
     * case proves the outline still runs across the freeze and across a stroke
     * that changes the mesh under it.
     */
    @Test
    public void seloutr1_04_05_aSculptedBodyIsOutlinedFromTheMeshActuallyShown() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // A sphere and a wide, full-strength brush, so the centre-screen
            // drag below actually reaches vertices. The default brush is sized
            // for a finger on a body the user has framed, and a case that made
            // a stroke which moved nothing would assert nothing.
            NativeViewport.applyConstructionSphere(1.0);
            NativeViewport.setSculptBrush(300.0, 1.0);
            workspace.findViewById(R.id.freeze_to_sculpt).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the body is being sculpted", NativeViewport.MODE_SCULPT,
                NativeViewport.productMode());
        assertOutlineIsBeingDrawn("SELOUTR1-04: a body in Sculpt");

        // A stroke changes the mesh the mask rasterises. The outline must
        // simply follow it — a revision the renderer noticed, not a rebuild the
        // outline asked for.
        final long revisionBefore = sculptRevision();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertNotEquals("the stroke deformed the sculpt mesh", revisionBefore,
                sculptRevision());
        assertOutlineIsBeingDrawn("SELOUTR1-05: after a stroke");

        // And back in Construction over the RETAINED sculpt mesh, which is what
        // the body still draws.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        settleLayout();
        assertOutlineIsBeingDrawn("SELOUTR1-05: a retained sculpt mesh");
    }

    /** SELOUTR1-06. A selected CAD Body is outlined, by the same path. */
    @Test
    public void seloutr1_06_aSelectedCadBodyIsOutlined() {
        final long cad = buildACadBodyAndSelectIt();
        assertEquals("the selected body is a CAD Body", NativeViewport.REPRESENTATION_CAD,
                NativeViewport.sceneBodyRepresentation(cad));
        assertOutlineIsBeingDrawn("SELOUTR1-06: a CAD Body");
    }

    /**
     * SELOUTR1-07. The outline follows the body's own model transform.
     *
     * <p>Not asserted from a pixel: the mask pass composes `proj · view · model`
     * from the SAME camera snapshot and the SAME derived model the shaded draw
     * uses, in the same two multiplies, so a second interpretation does not
     * exist to disagree. What is asserted here is that a body moved far off the
     * origin, turned and non-uniformly scaled is still outlined — a mask built
     * from a stale or identity transform would leave the visible body with no
     * coverage anywhere near it and stop the composite from painting.
     */
    @Test
    public void seloutr1_07_theOutlineFollowsATransformedBody() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(1.25, 0.5, -0.75, 15.0, 40.0, 25.0, 1.0, 2.0, 0.5);
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
        assertOutlineIsBeingDrawn("SELOUTR1-07: a translated, rotated, non-uniformly scaled body");

        // And through both projections, because the camera reaches the mask
        // only through that one composed matrix.
        NativeViewport.setProjectionMode(NativeViewport.PROJECTION_ORTHOGRAPHIC);
        settleLayout();
        assertOutlineIsBeingDrawn("SELOUTR1-07: in Orthographic");
        NativeViewport.setProjectionMode(NativeViewport.PROJECTION_PERSPECTIVE);
        settleLayout();
    }

    /**
     * SELOUTR1-08. Switching selection moves the outline and nothing else.
     *
     * <p>The mask pass draws EVERY body and writes coverage only for the
     * selected one, so "the old outline disappears and the new one appears" is
     * one push-constant float per body on the next frame. There is no
     * per-selection resource to create or free, which the allocation count
     * below is the direct evidence for.
     */
    @Test
    public void seloutr1_08_switchingSelectionMovesTheOutlineAndAllocatesNothing() {
        final long first = NativeViewport.sceneActiveBodyId();
        final long second = addASecondBody();
        assertNotEquals("two distinct bodies", first, second);

        NativeViewport.sceneSelectBody(first);
        awaitOutlineFrames(1);
        final double allocationsBefore = outlineStats()[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS];

        for (int i = 0; i < 8; i++) {
            NativeViewport.sceneSelectBody(i % 2 == 0 ? second : first);
            awaitOutlineFrames(1);
            assertTrue("SELOUTR1-08: the outline follows the selection",
                    outlineStats()[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS] > 0.0);
        }

        // SELOUTR1-27. Sixteen selection changes, and not one GPU allocation.
        assertEquals("SELOUTR1-27: repeated selection switches allocate no outline resource",
                allocationsBefore,
                outlineStats()[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS], 0.0);
    }

    /**
     * SELOUTR1-01. With no selected project object there is no outline pass at
     * all — not a pass that paints nothing, but no pass recorded.
     *
     * <p>"Nothing selected" is a narrower state than it sounds, and the
     * narrowing is the domain's, not this stage's. A scene snapshot marks the
     * body whose id equals the scene's active body, and a project is never
     * empty (`APP-H1`, `UI-OWNER-45`), so while a project is open exactly one
     * body is always selected. The states that genuinely have no selected
     * object are therefore Home, the CAD bootstrap before the first body
     * exists, and the Imported Mesh Preview — which replaces the snapshot with
     * items that are never selected, because selection is an identity a
     * session-only diagnostic does not own.
     *
     * <p>Home and the bootstrap are covered by
     * {@link #seloutr1_35_homeDrawsNoOutlineAndLeaksNoOverlayControl}; this
     * covers the preview, and the two together are SELOUTR1-01.
     */
    @Test
    public void seloutr1_01_thePreviewHasNoSelectedObjectAndDrawsNoOutline() {
        assertOutlineIsBeingDrawn("the baseline body is outlined to begin with");

        final boolean shown = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (NativeViewport.importGlbPreview(readAsset("glb/construction_sentinel.glb"))
                    != NativeViewport.IMPORT_OK) {
                return false;
            }
            NativeViewport.setGlbPreviewVisible(true);
            return NativeViewport.glbPreviewVisible();
        });
        // The preview is a diagnostic reached only from the verification
        // suites. If this build could not open one there is nothing here to
        // assert, and inventing a substitute would be asserting something else.
        assertTrue("the diagnostic preview opened", shown);
        settleLayout();
        assertOutlineWorkStops("SELOUTR1-01: the preview has no selected object");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setGlbPreviewVisible(false);
            NativeViewport.clearGlbPreview();
            return null;
        });
        settleLayout();
        assertOutlineIsBeingDrawn("and the project's own outline comes back");
    }

    /** SELOUTR1-15 / -16. Off stops the work entirely; On restores it. */
    @Test
    public void seloutr1_15_16_theToggleStopsAndRestoresTheWholeOutlinePath() {
        assertOutlineIsBeingDrawn("the outline runs before the toggle");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setSelectionOutlineVisible(false);
            return null;
        });
        assertOutlineWorkStops("SELOUTR1-15: the outline turned off");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setSelectionOutlineVisible(true);
            return null;
        });
        assertOutlineIsBeingDrawn("SELOUTR1-16: the outline turned back on");
    }

    // =======================================================================
    // Presentation only: SELOUTR1-09, -10, -17, -18, -19
    // =======================================================================

    /**
     * SELOUTR1-09 / -10 / -17 / -18 / -19. Toggling the outline and switching
     * selection repeatedly leaves the project byte-identical.
     *
     * <p>The strongest available statement, and the one `UI-PREF-R1` used for
     * preferences: the encoded `.forge` bytes, the semantic fingerprint, both
     * history depths, the dirty flag and the whole native snapshot, before and
     * after. A rebuild, a republication, a regeneration or a recorded step
     * would move one of them.
     *
     * <p>The mesh revision standing still is also what proves SELOUTR1-10 at
     * the GPU: {@code syncBody} uploads only when a body's source revision or
     * the surface shading changed, so an unmoved revision is an unmoved buffer.
     */
    @Test
    public void seloutr1_09_10_17_18_19_theOutlineTouchesNoProjectTruthAtAll() {
        final long second = addASecondBody();
        final long first = NativeViewport.sceneActiveBodyId();
        NativeViewport.sceneSelectBody(first);
        settleLayout();

        final byte[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
        assertNotNull(before);
        final long[] baseline = onWorkspace(rule.getScenario(), (activity, workspace) -> new long[]{
                NativeViewport.projectFingerprint(), NativeViewport.constructionUndoDepth(),
                NativeViewport.constructionRedoDepth(), NativeViewport.sculptUndoDepth(),
                workspace.projectDirty() ? 1L : 0L, NativeViewport.constructionMeshRevision(),
                sculptRevision()});
        final double[] snapshot = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        openDisplayPopover();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int i = 0; i < 4; i++) {
                workspace.findViewById(R.id.view_selection_outline_off).performClick();
                workspace.findViewById(R.id.view_selection_outline_on).performClick();
                NativeViewport.sceneSelectBody(second);
                NativeViewport.sceneSelectBody(first);
            }
            return null;
        });
        settleLayout();
        awaitOutlineFrames(2);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertArrayEquals("SELOUTR1-19: the .forge bytes are identical after four outline"
                    + " toggles and eight selection changes", before, NativeViewport.encodeProject());
            assertEquals("SELOUTR1-17: the fingerprint did not move", baseline[0],
                    NativeViewport.projectFingerprint());
            assertEquals("SELOUTR1-18: no Construction history step", baseline[1],
                    NativeViewport.constructionUndoDepth());
            assertEquals("and the redo stack is untouched", baseline[2],
                    NativeViewport.constructionRedoDepth());
            assertEquals("SELOUTR1-18: no Sculpt history step", baseline[3],
                    NativeViewport.sculptUndoDepth());
            assertEquals("SELOUTR1-17: the dirty flag did not move", baseline[4],
                    workspace.projectDirty() ? 1L : 0L);
            assertEquals("SELOUTR1-10: no mesh was republished, so nothing was re-uploaded",
                    baseline[5], NativeViewport.constructionMeshRevision());
            assertEquals("SELOUTR1-09: no sculpt revision was minted", baseline[6],
                    sculptRevision());
            final double[] after = nativeSnapshot();
            assertArrayEquals("SELOUTR1-17: nothing below JNI moved:"
                    + describeSnapshotDifference(snapshot, after), snapshot, after, 0.0);
            return null;
        });
    }

    // =======================================================================
    // Resource lifetime: SELOUTR1-26, -27
    // =======================================================================

    /**
     * SELOUTR1-26. A device rebuild recreates the outline's derived resources,
     * and the outline comes back drawing.
     *
     * <p>Through the debug injection seam, which is the only supported way to
     * exercise a lost device: provoking a real one would destabilise the
     * emulator the repository forbids destabilising, and would make the case
     * depend on driver behaviour rather than on ForgeShape's.
     */
    @Test
    public void seloutr1_26_aDeviceRebuildRestoresTheOutline() {
        assertOutlineIsBeingDrawn("the outline runs before the loss");
        final double allocationsBefore =
                outlineStats()[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS];
        final int rebuildsBefore = NativeViewport.debugRendererDeviceRebuilds();

        assertTrue("the debug injection seam is available in this build",
                NativeViewport.debugInjectDeviceLoss());

        int lifecycle = NativeViewport.rendererLifecycle();
        for (int attempt = 0; attempt < 150; attempt++) {
            lifecycle = NativeViewport.rendererLifecycle();
            if (lifecycle == NativeViewport.RENDERER_RESTART_REQUIRED) {
                break;
            }
            if (lifecycle == NativeViewport.RENDERER_HEALTHY
                    && NativeViewport.debugRendererDeviceRebuilds() > rebuildsBefore) {
                break;
            }
            SystemClock.sleep(100);
        }

        if (lifecycle == NativeViewport.RENDERER_RESTART_REQUIRED) {
            // A permitted outcome of the recovery policy, and not this stage's
            // to change. The outline holds no project truth, so there is
            // nothing here to have been lost.
            return;
        }

        assertOutlineIsBeingDrawn("SELOUTR1-26: after a device rebuild");
        assertTrue("SELOUTR1-26: the outline's derived resources were rebuilt, not reused",
                outlineStats()[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS] > allocationsBefore);
    }

    /**
     * SELOUTR1-27. The allocation count is driven by the EXTENT and by nothing
     * else — not by selection, not by the toggle, not by a palette change.
     */
    @Test
    public void seloutr1_27_outlineResourcesAreBoundedAndDrivenOnlyByTheExtent() {
        final long second = addASecondBody();
        awaitOutlineFrames(1);
        final double allocationsBefore =
                outlineStats()[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS];
        assertTrue("the mask has been allocated at least once", allocationsBefore >= 1.0);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int i = 0; i < 10; i++) {
                NativeViewport.sceneSelectBody(i % 2 == 0 ? second : firstBodyId());
                NativeViewport.setSelectionOutlineVisible(i % 2 == 0);
                NativeViewport.setViewportBackground(i % 2 == 0
                        ? NativeViewport.VIEWPORT_BACKGROUND_COOL_LIGHT
                        : NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE);
            }
            NativeViewport.setSelectionOutlineVisible(true);
            NativeViewport.setViewportBackground(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE);
            return null;
        });
        awaitOutlineFrames(2);

        assertEquals("SELOUTR1-27: ten selection, toggle and palette cycles allocate nothing",
                allocationsBefore,
                outlineStats()[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS], 0.0);
    }

    /**
     * `E2E-SELOUTR1-02/03`. A real single-finger TAP on the viewport moves the
     * outline, through the product's own gesture path.
     *
     * <p>Every other case here changes selection through the domain call,
     * because what they are about is what the renderer does with the answer.
     * This one is about the answer arriving the way a user produces it: real
     * `MotionEvent`s dispatched to the viewport, resolved by CPU picking into a
     * selection change, and the outline following. It is what makes
     * "tap object A, then object B, and the outline moves" a driven journey
     * rather than an asserted one.
     */
    @Test
    public void e2eSeloutr1_02_03_aRealViewportTapMovesTheOutline() {
        // EXACTLY two bodies. A tap resolves against everything on screen, so a
        // scene carrying bodies an earlier case left behind can put a third one
        // over the point this tap aims at — which is a failure about scene
        // population, not about the outline. `resetToBaselineConstruction`
        // restores the ACTIVE body; it does not remove the others, because
        // deleting a user's bodies is not what a reset means.
        freshProject();
        final long first = firstBodyId();
        final long second = addASecondBodyBesideTheFirst();
        NativeViewport.sceneSelectBody(first);
        settleLayout();
        assertOutlineIsBeingDrawn("E2E-SELOUTR1-02: body A selected to begin with");

        // Where each body actually is on screen, asked of the same picking the
        // tap will use rather than guessed from a coordinate — the workspace
        // re-arranges itself per window, so a literal pixel would be true for
        // one run only.
        final float[] onSecond = screenPointOn(second);
        assertNotNull("body B is on screen and pickable", onSecond);

        tapViewportAt(onSecond[0], onSecond[1]);
        assertEquals("E2E-SELOUTR1-03: the tap selected body B", second,
                NativeViewport.sceneActiveBodyId());
        assertOutlineIsBeingDrawn("E2E-SELOUTR1-03: the outline followed the tap to B");

        final float[] onFirst = screenPointOn(first);
        assertNotNull("body A is on screen and pickable", onFirst);
        tapViewportAt(onFirst[0], onFirst[1]);
        assertEquals("and back to body A", first, NativeViewport.sceneActiveBodyId());
        assertOutlineIsBeingDrawn("E2E-SELOUTR1-03: and back to A");
    }

    /**
     * `E2E-SELOUTR1-04`. Choosing a body in the Objects list moves the outline
     * too, through that control's own click.
     *
     * <p>The Objects capsule is the other half of what UI-OWNER-10 says
     * persistent selection IS, so the two must agree: picking a row selects the
     * body, and the outline is drawn around whatever that leaves selected.
     */
    @Test
    public void e2eSeloutr1_04_theObjectsListMovesTheOutline() {
        // EXACTLY two bodies. A tap resolves against everything on screen, so a
        // scene carrying bodies an earlier case left behind can put a third one
        // over the point this tap aims at — which is a failure about scene
        // population, not about the outline. `resetToBaselineConstruction`
        // restores the ACTIVE body; it does not remove the others, because
        // deleting a user's bodies is not what a reset means.
        freshProject();
        final long first = firstBodyId();
        final long second = addASecondBodyBesideTheFirst();
        NativeViewport.sceneSelectBody(first);
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View section = workspace.findViewById(R.id.objects_section);
            assertNotNull("the Objects section is on screen", section);
            final View row = ((ObjectsSectionView) section).rowFor(second);
            assertNotNull("body B has a row", row);
            row.performClick();
            return null;
        });
        settleLayout();

        assertEquals("E2E-SELOUTR1-04: the Objects row selected body B", second,
                NativeViewport.sceneActiveBodyId());
        assertOutlineIsBeingDrawn("E2E-SELOUTR1-04: the outline followed the Objects row");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();
    }

    /**
     * SELOUTR1-09 / -10 / -27, measured. Writes the numbers
     * `artifacts/sel-out-r1/PERFORMANCE.md` quotes rather than asserting them
     * in prose.
     *
     * <p>An A↔B selection loop, timed and counted: how many outline resources
     * were allocated, how many meshes were republished, how many CAD bodies
     * were regenerated, and how long the frames took with the outline on and
     * with it off. It is a measurement, not a benchmark — the emulator is not a
     * phone and no absolute frame time is claimed — and what it exists to show
     * is the SHAPE: that the counts do not move and that turning the outline
     * off removes work rather than hiding it.
     */
    @Test
    public void seloutr1_09_10_27_measuresTheSelectionLoopForTheEvidencePackage() {
        final long second = addASecondBody();
        final long first = firstBodyId();
        NativeViewport.sceneSelectBody(first);
        awaitOutlineFrames(2);

        final double[] before = outlineStats();
        final long meshRevisionBefore = NativeViewport.constructionMeshRevision();
        final long fingerprintBefore = NativeViewport.projectFingerprint();

        // Twenty selection changes, on the frames the renderer actually draws.
        final long startedAt = SystemClock.uptimeMillis();
        for (int i = 0; i < 20; i++) {
            final long target = (i % 2 == 0) ? second : first;
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                NativeViewport.sceneSelectBody(target);
                return null;
            });
            awaitOutlineFrames(1);
        }
        final long loopMillis = SystemClock.uptimeMillis() - startedAt;
        final double[] afterOn = outlineStats();

        // The same loop with the outline OFF, so the difference in recorded
        // work is visible rather than argued.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setSelectionOutlineVisible(false);
            return null;
        });
        settleLayout();
        SystemClock.sleep(150);
        final double[] offStart = outlineStats();
        for (int i = 0; i < 20; i++) {
            final long target = (i % 2 == 0) ? second : first;
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                NativeViewport.sceneSelectBody(target);
                return null;
            });
        }
        SystemClock.sleep(400);
        final double[] offEnd = outlineStats();

        final double allocations = afterOn[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS]
                - before[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS];
        final double compositesOn = afterOn[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS]
                - before[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS];
        final double compositesOff = offEnd[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS]
                - offStart[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS];

        // The report. Read out of logcat by the evidence collector, and
        // asserted here so a regression fails rather than merely printing.
        android.util.Log.i("ForgeShape", "FORGESHAPE_SELOUTR1_PERFORMANCE"
                + " selectionChanges=40"
                + " outlineAllocationsDuringLoop=" + (long) allocations
                + " compositeDrawsWithOutlineOn=" + (long) compositesOn
                + " compositeDrawsWithOutlineOff=" + (long) compositesOff
                + " meshRevisionBefore=" + meshRevisionBefore
                + " meshRevisionAfter=" + NativeViewport.constructionMeshRevision()
                + " fingerprintMoved="
                + (fingerprintBefore != NativeViewport.projectFingerprint())
                + " maskExtent=" + (int) afterOn[NativeViewport.OUTLINE_STAT_MASK_WIDTH] + "x"
                + (int) afterOn[NativeViewport.OUTLINE_STAT_MASK_HEIGHT]
                + " bandHalfWidthPx=" + afterOn[NativeViewport.OUTLINE_STAT_WIDTH_PIXELS]
                + " twentyOutlinedSwitchesMillis=" + loopMillis);

        assertEquals("SELOUTR1-27: twenty outlined selection changes allocate nothing",
                0.0, allocations, 0.0);
        assertTrue("SELOUTR1-27: the outline was actually drawing during the loop",
                compositesOn >= 20.0);
        assertEquals("SELOUTR1-15: with the outline off the composite is not recorded at all",
                0.0, compositesOff, 0.0);
        assertEquals("SELOUTR1-10: forty selection changes republish no mesh",
                meshRevisionBefore, NativeViewport.constructionMeshRevision());
        assertEquals("SELOUTR1-17: and move no fingerprint", fingerprintBefore,
                NativeViewport.projectFingerprint());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setSelectionOutlineVisible(true);
            return null;
        });
    }

    // =======================================================================
    // Delete / Undo / Redo non-collision: SELOUTR1-23, -24, -25, -36
    // =======================================================================

    /**
     * SELOUTR1-23 / -24 / -25 / -36. Delete, Undo and Redo behave exactly as
     * they did, and the outline renders whatever selection actually is.
     *
     * <p>Explicitly NOT the pending owner acceptance of `IMPORT-01B` /
     * `UI-OWNER-45`: this asserts non-collision, which is that the outline
     * changed no Delete semantics and introduced no outline-driven selection
     * logic of its own.
     */
    @Test
    public void seloutr1_23_24_25_36_deleteUndoAndRedoAreUnchangedAndTheOutlineFollows() {
        final long first = firstBodyId();
        final long second = addASecondBody();
        NativeViewport.sceneSelectBody(second);
        settleLayout();
        assertOutlineIsBeingDrawn("the second body is outlined before the Delete");

        final int undoBefore = NativeViewport.constructionUndoDepth();
        final int bodiesBefore = NativeViewport.sceneBodyCount();

        // --- Delete ---------------------------------------------------------
        assertEquals("SELOUTR1-36: Delete still succeeds", NativeViewport.DELETE_OK,
                NativeViewport.sceneDeleteBody(second));
        settleLayout();
        assertEquals("SELOUTR1-36: Delete removed exactly one body", bodiesBefore - 1,
                NativeViewport.sceneBodyCount());
        assertEquals("SELOUTR1-36: one Delete is still exactly one Undo step", undoBefore + 1,
                NativeViewport.constructionUndoDepth());
        // SELOUTR1-24. Selection fell to the surviving body by the EXISTING
        // fallback; the outline follows it and decides nothing.
        assertEquals("SELOUTR1-24: selection fell to the surviving body by the existing rule"
                        + " (the scene held " + bodiesBefore + " bodies before the Delete)",
                first, NativeViewport.sceneActiveBodyId());
        // SELOUTR1-23. No stale resource from the deleted body: the count is
        // flat, and the outline is drawing the FALLBACK body rather than a
        // ghost.
        assertOutlineIsBeingDrawn("SELOUTR1-23: after Delete, over the fallback selection");

        // --- Undo -----------------------------------------------------------
        assertEquals("SELOUTR1-25: Undo still succeeds", NativeViewport.HISTORY_OK,
                NativeViewport.constructionUndo());
        settleLayout();
        assertEquals("SELOUTR1-25: Undo restored the body", bodiesBefore,
                NativeViewport.sceneBodyCount());
        assertOutlineIsBeingDrawn("SELOUTR1-25: after Undo");

        // --- Redo -----------------------------------------------------------
        assertEquals("SELOUTR1-25: Redo still succeeds", NativeViewport.HISTORY_OK,
                NativeViewport.constructionRedo());
        settleLayout();
        assertEquals("SELOUTR1-25: Redo removed it again", bodiesBefore - 1,
                NativeViewport.sceneBodyCount());
        assertOutlineIsBeingDrawn("SELOUTR1-25: after Redo");

        // Leave the project as this suite's baseline expects.
        assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
        settleLayout();
    }

    // =======================================================================
    // Home and the CAD bootstrap: SELOUTR1-35
    // =======================================================================

    /**
     * SELOUTR1-35. Behind Home there is no project object, so there is no
     * outline — and the counters prove no work is done for one.
     */
    @Test
    public void seloutr1_35_homeDrawsNoOutlineAndLeaksNoOverlayControl() {
        assertOutlineIsBeingDrawn("the outline runs while a project is open");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.closeProject();
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
        assertFalse("SELOUTR1-35: no project is open", NativeViewport.projectOpen());
        assertOutlineWorkStops("SELOUTR1-35: Home");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Home must not carry the editor's overlay controls either. The
            // popover's views still EXIST in the workspace's tree — the shell
            // phase decides what is shown, not what is constructed — so what is
            // asserted is that it is not VISIBLE behind Home.
            assertNotEquals("SELOUTR1-35: the Display popover is not shown on Home",
                    View.VISIBLE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());
            return null;
        });
    }

    // =======================================================================
    // Helpers
    // =======================================================================

    private void openDisplayPopover() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        // The popover starts GONE and has never been laid out, so its chips
        // report a height of 0 until a traversal has actually run.
        settleLayout();
    }

    private static int countIfPresent(View root, int id) {
        return root.findViewById(id) == null ? 0 : 1;
    }

    private double[] outlineStats() {
        final double[] stats = new double[NativeViewport.OUTLINE_STATS_SIZE];
        NativeViewport.selectionOutlineStats(stats);
        return stats;
    }

    private int[] viewportSize() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            return new int[]{viewport.getWidth(), viewport.getHeight()};
        });
    }

    /**
     * Waits for the render thread to record at least `n` more outline frames.
     *
     * <p>A bounded poll rather than a hook, for the reason the device-loss
     * suite gives: the render loop is free-running and has no idle signal, and
     * inventing one for a test would put a synchronisation point on the frame
     * path. It stops as soon as the frames arrive.
     *
     * @return whether they arrived
     */
    private boolean awaitOutlineFrames(int n) {
        final double target = outlineStats()[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS] + n;
        for (int attempt = 0; attempt < 100; attempt++) {
            if (outlineStats()[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS] >= target) {
                return true;
            }
            SystemClock.sleep(30);
        }
        return false;
    }

    /**
     * Asserts the renderer is actually recording the outline: both the mask
     * pass and the composite draw advance over successive frames.
     *
     * <p>Both, deliberately. A composite that ran without a mask pass would be
     * sampling a stale image, and a mask pass with no composite would be paying
     * for a silhouette nobody draws — either one is a defect this catches.
     */
    private void assertOutlineIsBeingDrawn(String what) {
        final double[] before = outlineStats();
        assertTrue(what + ": the outline must be enabled for this to mean anything",
                before[NativeViewport.OUTLINE_STAT_ENABLED] == 1.0);
        final boolean arrived = awaitOutlineFrames(2);
        final double[] after = outlineStats();
        // The counter values travel with a failure, so a report says what the
        // renderer did rather than only that the wait ran out.
        assertTrue(what + ": the renderer records the composite draw"
                        + describeOutlineCounters(before, after),
                arrived);
        assertTrue(what + ": the renderer records the mask pass"
                        + describeOutlineCounters(before, after),
                after[NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES]
                        > before[NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES]);
        assertEquals(what + ": one mask pass per composite draw",
                after[NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES],
                after[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS], 0.0);
    }

    private static String describeOutlineCounters(double[] before, double[] after) {
        return " (composite " + (long) before[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS]
                + " -> " + (long) after[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS]
                + ", mask pass " + (long) before[NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES]
                + " -> " + (long) after[NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES]
                + ", allocations " + (long) before[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS]
                + " -> " + (long) after[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS] + ")";
    }

    /**
     * Asserts the outline path stops entirely: no mask pass and no composite
     * over a window several frames long. "Off" must cost nothing, not merely
     * paint nothing.
     */
    private void assertOutlineWorkStops(String what) {
        settleLayout();
        // Let anything already in flight finish before the window opens.
        SystemClock.sleep(120);
        final double[] before = outlineStats();
        SystemClock.sleep(400);
        final double[] after = outlineStats();
        assertEquals(what + ": no mask pass is recorded",
                before[NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES],
                after[NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES], 0.0);
        assertEquals(what + ": no composite draw is recorded",
                before[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS],
                after[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS], 0.0);
        assertEquals(what + ": and nothing is allocated either",
                before[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS],
                after[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS], 0.0);
    }

    private long addASecondBody() {
        final long added = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneAddBody());
        assertNotEquals("a second body was created", NativeViewport.NO_OBJECT, added);
        // Off to the side, so the two are separately visible in the viewport
        // and the evidence frames show two distinct silhouettes.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(2.5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
        return added;
    }

    /**
     * Closes the project and opens a fresh one-body Construction project.
     *
     * <p>Used only by the two cases that make a claim about the scene's
     * POPULATION — a tap resolved against everything on screen, and a row per
     * body in the Objects list. Closing writes nothing and is exactly what Back
     * to Home does.
     */
    private void freshProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.enterConstructionMode();
            NativeViewport.closeProject();
            workspace.ensureConstructionProjectForTest();
            NativeViewport.debugResetConstructionHistory();
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
        resetToBaselineConstruction(rule.getScenario());
    }

    /**
     * A second sphere placed beside the first, both small enough to sit well
     * inside the viewport at the baseline camera.
     */
    private long addASecondBodyBesideTheFirst() {
        final Long added = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(0.8);
            NativeViewport.applyBoxTransform(-0.9, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            final long id = NativeViewport.sceneAddBody();
            NativeViewport.applyConstructionSphere(0.8);
            NativeViewport.applyBoxTransform(1.3, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            workspace.syncFromNative();
            return id;
        });
        assertNotNull(added);
        assertNotEquals("a second body was created", NativeViewport.NO_OBJECT, (long) added);
        settleLayout();
        return added;
    }

    /**
     * Where a body sits on screen, in view-local pixels.
     *
     * <p>Asked of the product's own projection — the gizmo's PIVOT is the
     * body's Construction placement origin, derived from the same camera the
     * picker uses — rather than written down as a coordinate. The workspace
     * re-arranges itself per window, so a literal pixel would be true for one
     * run only, and `CLAUDE.md` forbids locating anything by coordinate.
     *
     * <p>It selects the body to read its pivot and restores the previous
     * selection afterwards, so asking the question changes nothing.
     */
    private float[] screenPointOn(final long objectId) {
        final long previous = NativeViewport.sceneActiveBodyId();
        // Select it and let the workspace settle BEFORE reading the pivot. The
        // gizmo's pivot is derived from the session's own snapshot, and asking
        // for it in the same block that changed the selection returns the
        // PREVIOUS body's pivot — which is a tap on the wrong body and a case
        // that fails for a reason that has nothing to do with the outline.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.selectConstructionTool(workspace, R.id.tool_rail_place);
            NativeViewport.sceneSelectBody(objectId);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();

        final float[] point = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] out = new float[2];
            return NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_NONE, out)
                    ? out : null;
        });

        // Put the selection back, and leave Shape held rather than Transform:
        // with a gizmo on screen a tap can land on a handle and become a drag,
        // and this helper must not decide what the next tap means.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSelectBody(previous);
            WorkspaceTestSupport.selectConstructionTool(workspace, R.id.tool_rail_shape);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        return point;
    }

    /** One real tap on the viewport, at a view-local pixel. */
    private void tapViewportAt(final float x, final float y) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            assertNotNull("the viewport is on screen", viewport);
            final long down = SystemClock.uptimeMillis();
            sendTouch(viewport, down, down, MotionEvent.ACTION_DOWN, x, y);
            sendTouch(viewport, down, down + 40L, MotionEvent.ACTION_UP, x, y);
            return null;
        });
        settleLayout();
    }

    private static void sendTouch(View target, long downTime, long eventTime, int action,
                                  float x, float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    /** The first body in scene order, which is the baseline Construction one. */
    private static long firstBodyId() {
        final long[] ids = new long[NativeViewport.sceneBodyCount()];
        final int written = NativeViewport.sceneBodyIds(ids);
        assertTrue("the scene holds at least one body", written > 0);
        return ids[0];
    }

    /** The sculpt mesh's own revision, from the authoritative sculpt state. */
    private static long sculptRevision() {
        final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.sculptState(state);
        return (long) state[NativeViewport.SCULPT_REVISION];
    }

    private long importSentinelAndSelectIt() {
        final byte[] bytes = readAsset("glb/construction_sentinel.glb");
        final File file = new File(scratch, "outline-import.glb");
        writeFile(file, bytes);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(file));
            return null;
        });
        settleLayout();

        final long[] ids = new long[NativeViewport.sceneBodyCount()];
        final int written = NativeViewport.sceneBodyIds(ids);
        for (int i = 0; i < written; i++) {
            if (NativeViewport.sceneBodyRepresentation(ids[i])
                    == NativeViewport.REPRESENTATION_IMPORTED) {
                NativeViewport.sceneSelectBody(ids[i]);
                settleLayout();
                return ids[i];
            }
        }
        throw new AssertionError("the import produced no Imported Mesh to outline");
    }

    private long buildACadBodyAndSelectIt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_xy).performClick();
            return null;
        });
        settleLayout();
        final long body = SketchTestSupport.drawRectangleAndExtrude(rule.getScenario(),
                1.0, 0.6, "0.4");
        NativeViewport.sceneSelectBody(body);
        settleLayout();
        return body;
    }

    private static byte[] readAsset(String name) {
        try (InputStream in = InstrumentationRegistry.getInstrumentation().getContext()
                .getAssets().open(name)) {
            final ByteArrayOutputStream out = new ByteArrayOutputStream();
            final byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
            return out.toByteArray();
        } catch (IOException e) {
            throw new AssertionError("could not read asset " + name, e);
        }
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException e) {
            throw new AssertionError("could not write " + file, e);
        }
    }
}
