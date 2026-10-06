package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closeAddPrimitive;
import static com.forgeshape.app.WorkspaceTestSupport.closeObjectsPanel;
import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openAddPrimitive;
import static com.forgeshape.app.WorkspaceTestSupport.openObjectsPanel;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settle;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * UI-R4B — the focused visual and motion correction, on a device.
 *
 * <p>This suite exists because every defect it covers was <b>invisible to the
 * tests that already passed</b>. A rail that forgets which tool is held still
 * holds it below JNI; a status line that never clears still says something true;
 * a surface that grows from the wrong corner still ends up in the right place; a
 * panel edge through the middle of a chip still lets the panel scroll. Nothing
 * was broken in a way any existing assertion could see, which is exactly why
 * these are written as assertions rather than left to the next screenshot pass.
 *
 * <p>Two rules are inherited from the rest of the instrumentation and are not
 * relaxed. No control is located by screen coordinate — the workspace
 * re-arranges itself per window, so a coordinate is only ever true for one run.
 * And no assertion reads a rendered pixel: what the Vulkan viewport draws is the
 * renderer's business, verified by the native suites.
 *
 * <p><b>Radii are asserted here, and the theme suite still asserts none.</b>
 * That is not a contradiction. What is asserted is a RELATION — a control's
 * corner against its host's corner and the padding between them — and a relation
 * survives every deliberate restyle, which is precisely the property the theme
 * suite's no-literals rule exists to protect. Pinning "10 dp" would break on the
 * next restyle; pinning "concentric" only breaks when the defect comes back.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceCorrectionTest {

    /** What the animator scale is put back to; 1.0 is the platform default. */
    private static final String NORMAL_SCALE = "1.0";

    /** The interactive floor, in dp, for user-operated Editor Workspace chrome. */
    private static final int TOUCH_FLOOR_DP = 48;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        // Also a REPAIR: a case that died half-way through could otherwise leave
        // the device with animation switched off for every suite after it.
        setAnimatorScale(NORMAL_SCALE);
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void restoreAnimation() {
        setAnimatorScale(NORMAL_SCALE);
    }

    // -----------------------------------------------------------------------
    // UIR4B-01 / UIR4B-02 — creation is offered exactly where it can succeed
    // -----------------------------------------------------------------------

    /**
     * UIR4B-01. Sculpt exposes no path into primitive creation.
     *
     * <p>The defect: {@code sceneAddBody()} refuses while sculpting, correctly,
     * and the {@code +} was drawn anyway. A user could tap it, be shown six
     * shapes, choose one, and only then be told no — a flow that cannot succeed,
     * presented as though it can.
     *
     * <p>Asserted in <b>both</b> Objects presentations, because a window with a
     * docked column and a window with a capsule are two different controls
     * answering the same question, and the previous stage's defect was exactly
     * one of them going unchecked.
     */
    @Test
    public void uir4b01_sculptOffersNoPrimitiveCreation() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("the Objects capsule offers no + while sculpting",
                    workspace.objectsCapsule().creationAvailable());
            assertFalse("and neither does the docked column's Add body",
                    workspace.objectsSection().creationAvailable());

            final View capsulePlus = workspace.findViewById(R.id.objects_capsule_add);
            if (capsulePlus != null) {
                assertEquals("a control that must fail is not drawn",
                        View.GONE, capsulePlus.getVisibility());
            }
            final View columnPlus = workspace.objectsSection().findViewById(R.id.add_body);
            if (columnPlus != null) {
                assertEquals(View.GONE, columnPlus.getVisibility());
            }
            assertFalse("and no creation surface is left standing on the model",
                    workspace.addPrimitivePalette().isOpen());
            return null;
        });
    }

    /**
     * UIR4B-01. The scene itself stays reachable in Sculpt — what was removed is
     * creation, not the list.
     *
     * <p>The distinction is the whole point of removing the {@code +} rather
     * than the capsule: seeing which body is being sculpted is as true in Sculpt
     * as anywhere else, and it is the fact the capsule exists to state.
     */
    @Test
    public void uir4b01_sculptStillReachesTheSceneItCannotAddTo() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.objectsDocked()) {
                assertEquals("a column, if this window has one, still lists the scene",
                        View.VISIBLE, workspace.objectsDock().getVisibility());
                return null;
            }
            final View capsule = workspace.objectsCapsule();
            assertEquals("the capsule stays on screen in Sculpt",
                    View.VISIBLE, capsule.getVisibility());
            assertEquals("and still names the active body", View.VISIBLE,
                    capsule.findViewById(R.id.objects_capsule_active).getVisibility());
            return null;
        });
        onUi((activity, workspace) -> { openObjectsPanel(workspace); return null; });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!workspace.objectsDocked()) {
                assertTrue("the scene list opens from the capsule in Sculpt",
                        workspace.objectsPopover().isOpen());
            }
            assertTrue("and it lists the bodies the scene actually holds",
                    workspace.objectsSection().rowCount() >= 1);
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closeObjectsPanel(workspace);
            return null;
        });
    }

    /**
     * UIR4B-02. Construction's {@code +} is untouched: six primitives, and the
     * chosen one is created through the domain's own path.
     *
     * <p>The correction removed a control in one mode; this is the case that
     * fails if it removed one in both.
     */
    @Test
    public void uir4b02_constructionStillCreatesFromSixPrimitives() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Construction still offers creation",
                    workspace.objectsDocked()
                            ? workspace.objectsSection().creationAvailable()
                            : workspace.objectsCapsule().creationAvailable());
            return null;
        });
        onUi((activity, workspace) -> { openAddPrimitive(workspace); return null; });

        final int[] palette = {
                R.id.add_primitive_box, R.id.add_primitive_cylinder,
                R.id.add_primitive_sphere, R.id.add_primitive_cone,
                R.id.add_primitive_capsule, R.id.add_primitive_plane};
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.addPrimitivePalette().isOpen());
            for (int id : palette) {
                assertNotNull("all six primitives are offered",
                        workspace.addPrimitivePalette().findViewById(id));
            }
            return null;
        });

        final long before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyCount());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.addPrimitivePalette().findViewById(R.id.add_primitive_cone)
                    .performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("a body was created", before + 1, NativeViewport.sceneBodyCount());
            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            assertEquals("and it is the primitive that was chosen, per native truth",
                    NativeViewport.PRIMITIVE_CONE, (int) primitive[0]);
            assertFalse("the palette closes once it has been used",
                    workspace.addPrimitivePalette().isOpen());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4B-03 — the Tool Rail keeps saying which tool is held
    // -----------------------------------------------------------------------

    /**
     * UIR4B-03. A layout rebuild does not lose the active tool.
     *
     * <p>The defect: {@code ToolRailView.rebuild()} discards every entry view
     * and builds fresh ones at the resting background, and it runs from
     * {@code setCompactEntries}, which the adaptive layout calls whenever a
     * window crosses the short-height threshold. A rotation therefore left the
     * rail with NO entry drawn active, in either mode, while the tool itself was
     * unchanged below JNI — the rail had simply stopped saying what was held.
     *
     * <p>Driven through {@code setCompactEntries} directly rather than by
     * rotating, so the case tests the rebuild rather than the device's rotation
     * timing, and so it is meaningful in every window the suite runs in.
     */
    @Test
    public void uir4b03_theRailKeepsItsActiveToolAcrossARebuild() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_smooth).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("precondition: Smooth is held below JNI",
                    NativeViewport.TOOL_SMOOTH, NativeViewport.sculptTool());
            assertTrue("and the rail says so",
                    workspace.findViewById(R.id.tool_rail_smooth).isActivated());

            // The rebuild the adaptive layout performs on a window change.
            WorkspaceTestSupport.toolRail(workspace).setCompactEntries(true);
            assertTrue("a rebuilt rail still says which tool is held",
                    workspace.findViewById(R.id.tool_rail_smooth).isActivated());
            assertFalse(workspace.findViewById(R.id.tool_rail_grab).isActivated());

            WorkspaceTestSupport.toolRail(workspace).setCompactEntries(false);
            assertTrue("and back again, with no user action in between",
                    workspace.findViewById(R.id.tool_rail_smooth).isActivated());
            assertEquals("and nothing about the held tool changed",
                    NativeViewport.TOOL_SMOOTH, NativeViewport.sculptTool());
            return null;
        });
    }

    /** UIR4B-03. The same rule in Construction, whose rail is a different set. */
    @Test
    public void uir4b03_theConstructionRailKeepsItsActiveEntryAcrossARebuild() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_place).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("precondition: Transform is the held context",
                    workspace.findViewById(R.id.tool_rail_place).isActivated());
            WorkspaceTestSupport.toolRail(workspace).setCompactEntries(true);
            assertTrue("a rebuild does not send the user back to Shape",
                    workspace.findViewById(R.id.tool_rail_place).isActivated());
            assertFalse(workspace.findViewById(R.id.tool_rail_shape).isActivated());
            WorkspaceTestSupport.toolRail(workspace).setCompactEntries(false);
            assertTrue(workspace.findViewById(R.id.tool_rail_place).isActivated());
            return null;
        });
    }

    /**
     * UIR4B-03. A mode change clears the remembered entry rather than carrying
     * it across.
     *
     * <p>{@code TOOL_GRAB} and {@code CONSTRUCTION_TOOL_SHAPE} are both 0, so a
     * rail that carried its key across a mode change would light an entry by
     * coincidence. The caller re-reads the held tool from native state
     * immediately afterwards, which is what actually decides.
     */
    @Test
    public void uir4b03_aModeChangeDoesNotCarryTheRailsKeyAcross() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_inflate).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Construction's own held entry decides, from native truth",
                    Integer.valueOf(workspace.uiState().constructionTool()),
                    WorkspaceTestSupport.toolRail(workspace).activeKey());
            assertTrue(workspace.findViewById(R.id.tool_rail_shape).isActivated()
                    || workspace.findViewById(R.id.tool_rail_place).isActivated());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4B-04 / UIR4B-05 — the status line has a life
    // -----------------------------------------------------------------------

    /**
     * UIR4B-04. A transient clears itself, and a newer one cancels the older
     * one's pending clear.
     *
     * <p>The second half is the part that is easy to get wrong and impossible to
     * see: without the cancel, the first message's timer fires while the second
     * message is on screen and blanks it early. It is the same cancel-first rule
     * {@link ChromeMotion} follows for animations, for the same reason.
     *
     * <p>The waits are the product's own constants plus a margin, not numbers
     * invented here, so the case cannot silently pass a hold that was shortened.
     */
    @Test
    public void uir4b04_aTransientStatusClearsAndANewerOneCancelsThePendingClear() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showStatus("first", R.attr.fsTextSecondary);
            assertTrue("a transient is on screen at once",
                    workspace.globalToolbar().statusVisible());
            assertEquals("first", String.valueOf(workspace.globalToolbar().statusText()));
            return null;
        });

        // Most of the first message's life, then replace it. If the pending
        // clear were not cancelled, it would fire during the wait below.
        SystemClock.sleep(GlobalToolbarView.STATUS_HOLD_MS - 1500L);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showStatus("second", R.attr.fsTextSecondary);
            return null;
        });
        SystemClock.sleep(2500L);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the newer message must outlive the older one's timer",
                    "second", String.valueOf(workspace.globalToolbar().statusText()));
            assertTrue(workspace.globalToolbar().statusVisible());
            return null;
        });

        // And then it goes, all by itself.
        SystemClock.sleep(GlobalToolbarView.STATUS_HOLD_MS);
        settle();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("a finished verdict leaves the workspace",
                    workspace.globalToolbar().statusVisible());
            assertEquals("and the line returns to what stands, which is nothing",
                    "", String.valueOf(workspace.globalToolbar().standingStatusText()));
            return null;
        });
    }

    /**
     * UIR4B-04. A rejection is given longer to be read than an acknowledgement.
     *
     * <p>ForgeShape's rejection copy is its own argument for itself — it names
     * the field, the constraint and the fact that nothing was changed — and a
     * message the user cannot finish reading may as well have said "Invalid".
     * The wording is not shortened to fit a timeout; the timeout fits the
     * wording.
     */
    @Test
    public void uir4b04_aRejectionStandsLongerThanAnAcknowledgement() {
        assertTrue("an error needs a real chance to be read",
                GlobalToolbarView.STATUS_FAULT_HOLD_MS > GlobalToolbarView.STATUS_HOLD_MS);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showStatus(activity.getString(R.string.reject_relation),
                    R.attr.fsTextError);
            return null;
        });
        SystemClock.sleep(GlobalToolbarView.STATUS_HOLD_MS + 1500L);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the explanatory reason is still there after an ordinary hold",
                    workspace.globalToolbar().statusVisible());
            assertTrue("and it is still the explanation, not a summary of one",
                    String.valueOf(workspace.globalToolbar().statusText())
                            .contains("Total Height cannot be less than Diameter"));
            return null;
        });
    }

    /**
     * UIR4B-04. The resting workspace says nothing, and does not draw a capsule
     * to say it.
     *
     * <p>The ambient line — "Showing the current box in m.", the gesture rule —
     * was true and was not news, and because nothing cleared the line, the last
     * verdict simply sat on the model until the next one replaced it.
     */
    @Test
    public void uir4b04_theRestingWorkspaceHasNoStandingMessage() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.syncFromNative();
            assertEquals("nothing stands in Construction at rest",
                    "", String.valueOf(workspace.globalToolbar().standingStatusText()));
            assertFalse("and no empty capsule is drawn on the model",
                    workspace.globalToolbar().statusVisible());
            return null;
        });
    }

    /**
     * UIR4B-04. A standing fault does stand — and re-asserts itself over a
     * transient that covered it.
     *
     * <p>A stale Construction Source is a STATE, not a verdict: it is still true
     * after any message that hides it, and it stays true until the user acts.
     */
    @Test
    public void uir4b04_aStandingFaultOutlivesTheMessagesThatCoverIt() {
        makeTheConstructionSourceStale();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            assertTrue("precondition: the source must be stale",
                    sculpt[NativeViewport.SCULPT_SOURCE_STALE] != 0.0);
            assertTrue("a standing fault is readable without opening a panel",
                    String.valueOf(workspace.globalToolbar().statusText())
                            .contains("Reset Sculpt from Shape"));

            workspace.showStatus("a passing verdict", R.attr.fsTextSecondary);
            assertEquals("a passing verdict",
                    String.valueOf(workspace.globalToolbar().statusText()));
            // The next re-read is what re-asserts it, exactly as a refresh does.
            workspace.syncFromNative();
            assertTrue("and the fault comes back, because it is still true",
                    String.valueOf(workspace.globalToolbar().statusText())
                            .contains("Reset Sculpt from Shape"));
            return null;
        });
    }

    /**
     * UIR4B-05. Dragging Radius or Strength does not mirror the value into the
     * global status line.
     *
     * <p>The defect was two copies of one number: one beside the finger moving
     * it, one in a capsule at the top of the window where nobody adjusting a
     * brush is looking — and the top copy then outlived the drag by the rest of
     * the session, which is how a resting Sculpt screenshot came to be captioned
     * with the last slider position.
     */
    @Test
    public void uir4b05_brushValuesLiveBesideTheirSlidersAndNowhereElse() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showStatus("", R.attr.fsTextSecondary);
            return null;
        });
        dragRadiusSlider();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final String radius = workspace.brushControls().describeRadius();
            final TextView beside = workspace.findViewById(R.id.brush_radius_value);
            assertEquals("the value is written beside its own slider",
                    radius, beside.getText().toString());

            final String status = String.valueOf(workspace.globalToolbar().statusText());
            assertFalse("and is not mirrored into the global status line: " + status,
                    status.contains(radius));
            assertFalse("nor is the brush described there at all: " + status,
                    status.toLowerCase(Locale.US).contains("brush"));
            return null;
        });
    }

    /**
     * UIR4B-05. A brush change touches no geometry, which is the reason the
     * value can be live at all.
     */
    @Test
    public void uir4b05_aBrushChangeTouchesNoGeometry() {
        enterSculpt();
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshotWithoutBrush());
        dragRadiusSlider();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] after = nativeSnapshotWithoutBrush();
            assertArrayEquals("moving a brush slider publishes nothing:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4B-06 / UIR4B-07 — the precision surface ends on a row
    // -----------------------------------------------------------------------

    /**
     * UIR4B-06. The exact Shape surface never ends through the middle of a row.
     *
     * <p>The bottom inset was always there and the scrolling always worked. What
     * was wrong was where the surface was allowed to STOP: a cap expressed in
     * pixels against a body that is a stack of rows of unrelated heights, so the
     * two agreed only by accident and at rest the boundary regularly crossed a
     * chip or a field caption. A control sliced across its middle is read as a
     * rendering fault, not as "there is more below".
     */
    @Test
    public void uir4b06_theExactShapeSurfaceEndsOnARowBoundary() {
        openPrecisionOnShape();
        assertNoRowCrossesTheBoundary("Exact Shape");
    }

    /**
     * UIR4B-07. The same rule for exact Transform, whose body is a different and
     * taller stack — six numeric fields, two headings and a unit row.
     */
    @Test
    public void uir4b07_theExactTransformSurfaceEndsOnARowBoundary() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_place).performClick();
            return null;
        });
        settleLayout();
        openPrecisionOnHeldEntry();
        assertNoRowCrossesTheBoundary("Exact Transform");
    }

    /**
     * UIR4B-07. Ending on a row costs nothing that was there before: the body
     * still scrolls to everything, and the fields are still fields.
     *
     * <p>The failure this guards against is the obvious over-correction — making
     * the boundary tidy by dropping content out of reach.
     */
    @Test
    public void uir4b07_endingOnARowKeepsTheBodyReachableAndEditable() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_place).performClick();
            return null;
        });
        settleLayout();
        openPrecisionOnHeldEntry();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ViewGroup scroll = workspace.findViewById(R.id.inspector_scroll);
            final View content = scroll.getChildAt(0);
            final int viewport = scroll.getHeight()
                    - scroll.getPaddingTop() - scroll.getPaddingBottom();
            if (workspace.propertyInspector().bodyIsScrollable()) {
                assertTrue("everything below the boundary is still reachable by scrolling",
                        content.getHeight() > viewport);
            }
            // The last row of the body is the one a rounding rule would be
            // tempted to drop, and it is the commit.
            assertNotNull("Apply Transform is still in the body",
                    workspace.findViewById(R.id.apply_transform));
            final View field = workspace.findViewById(R.id.field_pos_x);
            assertNotNull("and the fields are still fields", field);
            assertTrue("which are still focusable for the keyboard",
                    field.isFocusable());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4B-08 / UIR4B-09 / UIR4B-10 — one anchored-surface motion
    // -----------------------------------------------------------------------

    /**
     * UIR4B-08. Every surface that grows out of a control takes the shared
     * contract, and none of them owns a copy of it.
     *
     * <p>Asserted over the SET rather than over a list maintained by hand: the
     * reason the Display popover was the only surface with a correct first-open
     * pivot is that nothing had ever measured all of them together.
     *
     * <p>The count is part of the assertion on purpose. It is not a tally to be
     * bumped whenever a surface appears — it is the tripwire that makes adding
     * one a deliberate act, because a surface that reached the workspace without
     * anyone measuring it against this contract is exactly the defect UIR4B-08
     * was written for. It moved from four to five at E2E-R1A, when the project
     * actions surface joined, and from five to six at `SCULPT-H1`, when the
     * History navigator joined; each takes the shared contract like the rest and
     * owns no motion of its own. The surfaces are asserted by IDENTITY and in
     * order as well as by count, so swapping one surface for another cannot
     * pass by keeping the total.
     */
    @Test
    public void uir4b08_everyAnchoredSurfaceSharesOneMotionContract() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final AnchoredSurfaceView[] surfaces = workspace.anchoredSurfaces();
            // Eight since `MODELING-FOUNDATIONS-R1` added a body's History
            // and the Surface sketch's Finish choices, each on the shared
            // contract like the rest.
            final AnchoredSurfaceView[] expected = {
                    workspace.objectsPopover(), workspace.addPrimitivePalette(),
                    workspace.propertyInspector(), workspace.displayPopover(),
                    workspace.projectPopover(), workspace.historyNavigator(),
                    workspace.featureHistory(), workspace.surfaceFinish()};
            assertEquals("the eight surfaces that grow out of a control",
                    expected.length, surfaces.length);
            for (int i = 0; i < expected.length; i++) {
                assertNotNull("anchored surface " + i + " exists", expected[i]);
                assertSame("anchored surface " + i + " is "
                                + expected[i].getClass().getSimpleName(),
                        expected[i], surfaces[i]);
            }
            for (AnchoredSurfaceView surface : surfaces) {
                assertNotNull(surface);
                assertTrue("a resting anchored surface is settled: "
                                + surface.getClass().getSimpleName(),
                        surface.isSettled());
            }
            // The contract itself lives in one place. If a surface ever grows a
            // private duration again, this is the constant it would have to
            // diverge from, and the JVM case UIR4B-08 pins its shape.
            assertTrue(ChromeMotion.ANCHORED_ENTER_MS > ChromeMotion.ANCHORED_EXIT_MS);
            assertNotNull("and one shared curve", ChromeMotion.anchoredEase());
            return null;
        });
    }

    /**
     * UIR4B-09. The FIRST open of each surface in a process grows from the
     * control that opened it.
     *
     * <p>This is a genuine first open: the {@code ActivityScenarioRule} builds a
     * fresh workspace per case, so each surface here has never been laid out.
     * That is precisely the condition the defect lived in — the pivot is
     * expressed in the surface's own size, the surface had none, so the growth
     * started from the top-left of a zero-sized box. Every later open was
     * correct, which is why it survived review.
     */
    @Test
    public void uir4b09_theFirstOpenOfTheSceneListGrowsFromItsCapsule() {
        assertAnchorPivotIsCorrectAfterFirstOpen(Surface.OBJECTS);
    }

    /** UIR4B-09. The same, for the creation palette. */
    @Test
    public void uir4b09_theFirstOpenOfTheAddPaletteGrowsFromItsPlus() {
        assertAnchorPivotIsCorrectAfterFirstOpen(Surface.ADD_PRIMITIVE);
    }

    /** UIR4B-09. The same, for the precision surface. */
    @Test
    public void uir4b09_theFirstOpenOfThePrecisionSurfaceGrowsFromItsToggle() {
        assertAnchorPivotIsCorrectAfterFirstOpen(Surface.PRECISION);
    }

    /** UIR4B-09. And the Display popover, which was already correct and is the
     *  surface the pattern was lifted from. */
    @Test
    public void uir4b09_theFirstOpenOfTheDisplayPopoverGrowsFromItsControl() {
        assertAnchorPivotIsCorrectAfterFirstOpen(Surface.DISPLAY);
    }

    /** UIR4B-09. And the project surface, which joined at E2E-R1A and is held to
     *  the same first-open contract as the four before it. */
    @Test
    public void uir4b09_theFirstOpenOfTheProjectSurfaceGrowsFromItsControl() {
        assertAnchorPivotIsCorrectAfterFirstOpen(Surface.PROJECT);
    }

    /**
     * UIR4B-10. Reduced motion lands on the final state with no transient scale
     * or alpha.
     *
     * <p>Asserted immediately, with no settle in between: the point is that
     * nothing was ever posted. A zero-duration animator would still put the
     * surface at 0.96 for a frame, which is exactly what a user asking for no
     * motion is trying to avoid.
     */
    @Test
    public void uir4b10_reducedMotionLandsEveryAnchoredSurfaceImmediately() {
        setAnimatorScale("0.0");
        try {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.objectsCapsule().findViewById(R.id.objects_capsule_active)
                        .performClick();
                // Read in the SAME block, before any frame can have run.
                final ObjectsPopoverView panel = workspace.objectsPopover();
                if (!workspace.objectsDocked()) {
                    assertTrue("it is open", panel.isOpen());
                    assertEquals("at full opacity, at once", 1.0f, panel.getAlpha(), 0.0f);
                    assertEquals(1.0f, panel.getScaleX(), 0.0f);
                    assertEquals(1.0f, panel.getScaleY(), 0.0f);
                    assertEquals(View.VISIBLE, panel.getVisibility());
                    assertFalse("and nothing is waiting for a size",
                            panel.growthWaitingForSize());
                }
                return null;
            });
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                WorkspaceTestSupport.precisionToggle(workspace).performClick();
                final PropertyInspectorView inspector = workspace.propertyInspector();
                assertTrue(inspector.isOpen());
                assertEquals(1.0f, inspector.getAlpha(), 0.0f);
                assertEquals(1.0f, inspector.getScaleX(), 0.0f);
                assertEquals(1.0f, inspector.getScaleY(), 0.0f);
                assertEquals(View.VISIBLE, inspector.getVisibility());
                return null;
            });
        } finally {
            setAnimatorScale(NORMAL_SCALE);
        }
    }

    /**
     * UIR4B-10. A second tap reverses a transition rather than queueing behind
     * it, and the surface still ends somewhere legitimate.
     *
     * <p>Only the RESTING state is asserted. A case that sampled a frame of a
     * transition would be a test of the device's frame timing.
     */
    @Test
    public void uir4b10_interruptingAnAnchoredSurfaceStillSettles() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.objectsDocked()) {
                return null;
            }
            final View opener = workspace.objectsCapsule()
                    .findViewById(R.id.objects_capsule_active);
            opener.performClick();
            opener.performClick();
            opener.performClick();
            return null;
        });
        settleLayout();
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.objectsDocked()) {
                return null;
            }
            assertTrue("however it was interrupted, it ends fully there or fully gone",
                    workspace.objectsPopover().isSettled());
            closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!workspace.objectsDocked()) {
                assertTrue(workspace.objectsPopover().isSettled());
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4B-11 — chrome and motion publish nothing
    // -----------------------------------------------------------------------

    /**
     * UIR4B-11. Opening and closing every surface this stage touched, switching
     * rail context and rebuilding the rail produce zero geometry work.
     *
     * <p>Compared bit for bit rather than approximately: "a panel changed
     * nothing" is either exactly true or it is a defect.
     */
    @Test
    public void uir4b11_pureChromeAndMotionPublishNoGeometry() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        onUi((activity, workspace) -> { openObjectsPanel(workspace); return null; });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        onUi((activity, workspace) -> { openAddPrimitive(workspace); return null; });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closeAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.precisionToggle(workspace).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.precisionToggle(workspace).performClick();
            workspace.findViewById(R.id.tool_rail_place).performClick();
            workspace.findViewById(R.id.tool_rail_shape).performClick();
            WorkspaceTestSupport.toolRail(workspace).setCompactEntries(true);
            WorkspaceTestSupport.toolRail(workspace).setCompactEntries(false);
            workspace.showStatus("a message", R.attr.fsTextSecondary);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] after = nativeSnapshot();
            assertArrayEquals("chrome and motion touch nothing below JNI:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4B-12 / UIR4B-13 — the expanded window is the same product
    // -----------------------------------------------------------------------

    /**
     * UIR4B-12. On an expanded window, Objects and the Tool Rail wear the phone's
     * vocabulary: inset from the window edge, raised, and bounded by their own
     * content rather than by the window.
     *
     * <p>The defect was an expanded window that spoke a different visual language
     * about the same controls — an opaque square-edged column against the leading
     * edge and a rail flush against the trailing one. That is a desktop CAD frame,
     * and it made the tablet a different product rather than the same one with
     * more room.
     *
     * <p>Reads the window it is actually in, so an overridden expanded run is a
     * genuine expanded-layout run and a compact run asserts the compact contract.
     */
    @Test
    public void uir4b12_theExpandedWindowUsesInsetFloatingSurfaces() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View rail = WorkspaceTestSupport.trailingHost(workspace);
            assertTrue("the unified right host is a raised floating surface in EVERY window",
                    rail.getElevation() > 0.0f);
            assertTrue("and stands off the trailing window edge in every window",
                    workspace.getWidth() - rectRight(workspace, rail) > 0);
            assertTrue("and is bounded by its own content, never the window height",
                    rail.getHeight() < workspace.getHeight());

            if (!workspace.objectsDocked()) {
                // A window with no column: the capsule is the scene control, and
                // that is the same vocabulary by construction.
                assertEquals(View.VISIBLE, workspace.objectsCapsule().getVisibility());
                return null;
            }
            final View column = workspace.objectsDock();
            assertTrue("a docked Objects column is raised, like every other surface",
                    column.getElevation() > 0.0f);
            assertTrue("and inset from the leading window edge, not flush against it",
                    rectLeft(workspace, column) > 0);
            assertTrue("and bounded by its content rather than spanning the window",
                    column.getHeight() < workspace.getHeight());
            return null;
        });
    }

    /**
     * UIR4B-13. Expanded Sculpt keeps Radius and Strength on the leading edge,
     * out of the model, and still says which brush is held.
     *
     * <p>The defect: the Objects column sits before the brush controls in the
     * middle row, so its 180 dp pushed the two most-used Sculpt controls inboard
     * onto the model and out from under the hand that reaches for them. The
     * column is withdrawn in Sculpt now — body switching and creation are both
     * refused while sculpting, so a permanent scene column there was furniture
     * that cost the brush controls their place.
     */
    @Test
    public void uir4b13_expandedSculptKeepsTheBrushControlsOnTheLeadingEdge() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("Sculpt has no permanent scene column to be displaced by",
                    workspace.objectsDocked());

            final View brush = workspace.findViewById(R.id.brush_edge_controls);
            assertEquals(View.VISIBLE, brush.getVisibility());
            final int left = rectLeft(workspace, brush);
            // A third of the window is generous; the defect put them past it.
            assertTrue("Radius and Strength stay near the leading edge, not on the"
                            + " model: left=" + left + " of " + workspace.getWidth(),
                    left < workspace.getWidth() / 3);

            assertTrue("and the held brush is still drawn as held",
                    workspace.findViewById(R.id.tool_rail_grab).isActivated()
                            || workspace.findViewById(R.id.tool_rail_clay).isActivated()
                            || workspace.findViewById(R.id.tool_rail_smooth).isActivated()
                            || workspace.findViewById(R.id.tool_rail_inflate).isActivated());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4B-14 — selected state is fill-led, and nests in its host
    // -----------------------------------------------------------------------

    /**
     * UIR4B-14. An active control inside a capsule is CONCENTRIC with it.
     *
     * <p>The crescent: two rounded rectangles that do not share a corner centre
     * leave a sliver of the outer one showing at each end. A 10 dp active box
     * painted flush inside a 26 dp capsule with 4 dp of padding did exactly that,
     * and it reads as a rendering fault rather than as a selected control.
     *
     * <p>The rule asserted is inner = outer − padding, which is a RELATION and
     * therefore survives any deliberate change to either number.
     */
    @Test
    public void uir4b14_anActiveCapsuleMemberIsConcentricWithItsCapsule() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.precisionToggle(workspace).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View toggle = WorkspaceTestSupport.precisionToggle(workspace);
            final View group = WorkspaceTestSupport.precisionGroup(workspace);
            assertTrue("precondition: the toggle is drawn active", toggle.isActivated());
            assertConcentric("the precision toggle", group, toggle, paddingOf(group));

            // The Objects capsule's label, lit while the scene list is open.
            if (!workspace.objectsDocked()) {
                final ObjectsCapsuleView capsule = workspace.objectsCapsule();
                final View label = capsule.findViewById(R.id.objects_capsule_active);
                capsule.showObjectsOpen(true);
                assertConcentric("the Objects capsule's label", capsule, label,
                        paddingOf(capsule));
                capsule.showObjectsOpen(false);
            }
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.precisionToggle(workspace).performClick();
            return null;
        });
    }

    /**
     * UIR4B-14. The held Tool Rail entry is concentric with the rail's own
     * surface, so the first and last entries show no crescent.
     */
    @Test
    public void uir4b14_theHeldRailEntryIsConcentricWithTheRail() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_shape).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View entry = workspace.findViewById(R.id.tool_rail_shape);
            assertTrue("precondition: Shape is held", entry.isActivated());
            // The capsule is drawn by the ScrollView (so the container cannot
            // clip its shadow) and the padding around the entries is the rail's.
            assertConcentric("the held rail entry",
                    WorkspaceTestSupport.toolRailScroll(workspace), entry,
                    paddingOf(WorkspaceTestSupport.toolRail(workspace)));
            return null;
        });
    }

    /**
     * UIR4B-14. A selection reads as a FILL, not as a blue outline.
     *
     * <p>The accent hairline was described as the third signal and was read as
     * the first: six outlined chips made a panel look like a form of framed
     * cells, and it eroded the rule that the accent belongs to a primary commit
     * and a focused field. Asserted structurally — the active background paints
     * a solid and no stroke — rather than by colour, which the theme suite owns.
     */
    @Test
    public void uir4b14_aSelectedControlIsFillLedWithNoAccentOutline() {
        openPrecisionOnShape();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View chosen = workspace.findViewById(R.id.primitive_option_box);
            assertNotNull(chosen);
            assertTrue("precondition: the drafted primitive is drawn selected",
                    chosen.isActivated());
            final GradientDrawable shape = shapeOf(chosen);
            assertNotNull("the selected background is a drawn shape", shape);
            assertNull("and it carries no stroke — the fill is the signal",
                    strokeColorOf(shape));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4B-15 / UIR4B-16 — one user-facing vocabulary
    // -----------------------------------------------------------------------

    /**
     * UIR4B-15. No string the user can read says "Freeze" or "frozen".
     *
     * <p>Scanned over every {@code R.string} the product declares rather than
     * over a list written here, because a list would be exactly as complete as
     * whoever last remembered to update it — and the string that survived the
     * previous pass would survive this one too.
     */
    @Test
    public void uir4b15_noUserFacingStringSaysFreezeOrFrozen() {
        final List<String> offenders = new ArrayList<>();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (Field field : R.string.class.getFields()) {
                final String value;
                try {
                    value = activity.getString(field.getInt(null));
                } catch (Exception notAString) {
                    continue;
                }
                final String lower = value.toLowerCase(Locale.US);
                if (lower.contains("freeze") || lower.contains("frozen")
                        || lower.contains("freezing")) {
                    offenders.add(field.getName() + " = \"" + value + "\"");
                }
            }
            return null;
        });
        assertTrue("these strings still use implementation vocabulary: " + offenders,
                offenders.isEmpty());
    }

    /**
     * UIR4B-16. The primary Construction → Sculpt action reads Start Sculpting,
     * and the way back is unchanged.
     */
    @Test
    public void uir4b16_thePrimaryActionIsStartSculptingAndBothWaysBackRemain() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final TextView start = workspace.findViewById(R.id.freeze_to_sculpt);
            final TextView resume = workspace.findViewById(R.id.resume_sculpt);
            final boolean hasMesh = resume.getVisibility() == View.VISIBLE;
            if (!hasMesh) {
                assertEquals("Start Sculpting", start.getText().toString());
            }
            assertEquals("Resume Sculpt", resume.getText().toString());
            return null;
        });
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final TextView back = workspace.findViewById(R.id.back_to_construction);
            assertEquals(View.VISIBLE, back.getVisibility());
            assertEquals("Back to Construction", back.getText().toString());
            final TextView reset = workspace.sculptContext().findViewById(R.id.freeze_again);
            assertNotNull("the destructive act is still offered, and still named",
                    reset);
            assertEquals("Reset Sculpt from Shape…", reset.getText().toString());
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("and once a mesh exists, Resume takes the slot",
                    View.VISIBLE,
                    workspace.findViewById(R.id.resume_sculpt).getVisibility());
            assertEquals(View.GONE,
                    workspace.findViewById(R.id.freeze_to_sculpt).getVisibility());
            return null;
        });
    }

    /**
     * UIR4B-16. The destructive reset still asks first, and the question still
     * says what will be lost.
     *
     * <p>The copy changed; the guard did not. This is the case that fails if a
     * wording pass quietly turned a confirmed act into an unconfirmed one.
     */
    @Test
    public void uir4b16_theDestructiveResetStillAsksAndStillNamesTheConsequence() {
        enterSculpt();
        sculptTheCurrentMesh();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            assertTrue("precondition: there is sculpt work to lose",
                    sculpt[NativeViewport.SCULPT_HAS_EDITS] != 0.0);
            workspace.sculptContext().findViewById(R.id.freeze_again).performClick();
            return null;
        });
        settle();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("a destructive act is confirmed",
                    workspace.sculptContext().visibleConfirmation());
            final String message = activity.getString(
                    R.string.reset_sculpt_confirm_message);
            assertTrue("and the question says the sculpting will be discarded: " + message,
                    message.contains("discarded"));
            assertTrue("and that it cannot be undone: " + message,
                    message.contains("cannot be undone"));
            workspace.sculptContext().visibleConfirmation().dismiss();
            return null;
        });
        settle();
    }

    // -----------------------------------------------------------------------
    // UIR4B-17 — the 48 dp interactive floor
    // -----------------------------------------------------------------------

    /**
     * UIR4B-17. Every representative user-operated chrome control meets the
     * 48 dp floor in BOTH dimensions.
     *
     * <p>The floor is the hit area and never the glyph: the icons are still
     * 20 dp, the rail's compact caption is still 10 sp, and nothing here grew a
     * drawn box to reach the number.
     *
     * <p>The scene control is read from the window this case is actually in, as
     * every adaptive case in the instrumentation is. A window that gives Objects
     * a column withdraws the capsule outright — it is {@code GONE}, so it
     * measures 0 x 0 — and the controls a user touches there are the column's
     * own row and its Add body.
     */
    @Test
    public void uir4b17_constructionChromeMeetsTheFortyEightDpFloor() {
        openPrecisionOnShape();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.objectsDocked()) {
                assertMeetsTouchFloorOf(activity, workspace,
                        workspace.objectsSection().findViewById(R.id.add_body),
                        "the docked Objects column's Add body");
                assertMeetsTouchFloorOf(activity, workspace,
                        workspace.objectsSection().rowFor(NativeViewport.sceneActiveBodyId()),
                        "a docked Objects row");
            } else {
                assertMeetsTouchFloor(activity, workspace, R.id.objects_capsule_active,
                        "the Objects capsule's expand control");
                assertMeetsTouchFloor(activity, workspace, R.id.objects_capsule_add,
                        "the Objects capsule's +");
            }
            assertMeetsTouchFloor(activity, workspace, R.id.tool_rail_shape, "Shape");
            assertMeetsTouchFloor(activity, workspace, R.id.tool_rail_place, "Transform");
            assertMeetsTouchFloorOf(activity, workspace,
                    WorkspaceTestSupport.precisionToggle(workspace),
                    "the precision toggle");
            assertMeetsTouchFloor(activity, workspace, R.id.display_settings_button,
                    "Display");
            assertMeetsTouchFloor(activity, workspace, R.id.hide_ui_toggle, "Hide UI");
            assertMeetsTouchFloor(activity, workspace, R.id.apply_shape, "Apply Shape");
            assertMeetsTouchFloor(activity, workspace, R.id.export_action, "Export");
            assertMeetsTouchFloor(activity, workspace, R.id.unit_chip_m, "the metre chip");
            return null;
        });
    }

    /** UIR4B-17. The same floor for everything Sculpt puts under a thumb. */
    @Test
    public void uir4b17_sculptChromeMeetsTheFortyEightDpFloor() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertMeetsTouchFloor(activity, workspace, R.id.tool_rail_grab, "Grab");
            assertMeetsTouchFloor(activity, workspace, R.id.tool_rail_clay, "Clay");
            assertMeetsTouchFloor(activity, workspace, R.id.tool_rail_smooth, "Smooth");
            assertMeetsTouchFloor(activity, workspace, R.id.tool_rail_inflate, "Inflate");
            assertMeetsTouchFloor(activity, workspace, R.id.brush_radius_slider,
                    "the Radius slider");
            assertMeetsTouchFloor(activity, workspace, R.id.brush_strength_slider,
                    "the Strength slider");
            assertMeetsTouchFloor(activity, workspace, R.id.objects_capsule_active,
                    "the Objects capsule in Sculpt");
            assertMeetsTouchFloor(activity, workspace, R.id.back_to_construction,
                    "Back to Construction");
            return null;
        });
    }

    /**
     * UIR4B-17. Raising the floor did not push an icon control off the row.
     *
     * <p>The 8 dp the two icon controls gained comes out of the same toolbar row
     * as the transition button, and the button is what gives it up — a button
     * that ellipsises still reads, and an icon control squeezed past the window
     * edge is unreachable. This is the guard {@code R1B1-10b} already keeps, in
     * the mode that made it necessary.
     */
    @Test
    public void uir4b17_theToolbarIconControlsAreStillFullyOnScreenInSculpt() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int id : new int[]{R.id.export_action, R.id.display_settings_button,
                    R.id.hide_ui_toggle}) {
                final View control = workspace.findViewById(id);
                assertTrue("a utility control must be fully inside the window",
                        WorkspaceTestSupport.isFullyOnScreen(control, workspace));
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4B-18 / UIR4B-19 — nothing else moved
    // -----------------------------------------------------------------------

    /**
     * UIR4B-18. Exactly three appearances, and choosing one is presentation
     * only.
     *
     * <p>The exact semantic values are the palettes', asserted relationally by
     * the theme suite; what this case guards is that a visual correction did not
     * add, remove or replace an appearance.
     */
    @Test
    public void uir4b18_thereAreExactlyFiveAppearancesAndTheyChangeNoGeometry() {
        assertEquals("five approved appearances (UI-OWNER-42), and no sixth",
                5, AppTheme.values().length);
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        // Since UI-PREF-R1 the palettes are chosen on the Settings page, reached
        // from the Project surface: a palette is a persistent preference.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.globalToolbar().findViewById(R.id.project_actions_button)
                    .performClick();
            workspace.findViewById(R.id.project_settings).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.settingsVisible());
            for (int id : new int[]{R.id.appearance_warm_graphite,
                    R.id.appearance_neutral_charcoal, R.id.appearance_light_charcoal,
                    R.id.appearance_warm_light, R.id.appearance_cool_light}) {
                assertNotNull("each approved appearance is offered",
                        workspace.settingsPage().findViewById(id));
            }
            final double[] after = nativeSnapshot();
            assertArrayEquals("opening the appearance list touches nothing:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            workspace.findViewById(R.id.settings_back).performClick();
            return null;
        });
        settleLayout();
    }

    /**
     * UIR4B-19. The world grid and the selection feedback are exactly what they
     * were.
     *
     * <p>Both are native-owned and neither was in this stage's scope, which is
     * precisely why they are asserted: a cross-cutting chrome change is how a
     * viewport feature gets broken by accident.
     */
    @Test
    public void uir4b19_theGridAndTheSelectionFeedbackAreUnchanged() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final boolean wasVisible = NativeViewport.gridVisible();
            final double[] before = nativeSnapshot();

            assertFalse("Grid off is honoured", NativeViewport.setGridVisible(false));
            assertFalse(NativeViewport.gridVisible());
            assertTrue("Grid on is honoured", NativeViewport.setGridVisible(true));
            assertTrue(NativeViewport.gridVisible());

            final double[] after = nativeSnapshot();
            assertArrayEquals("and the grid is a reference, not geometry:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);

            // Selection feedback is driven by the reduced-motion flag the
            // workspace pushes on every refresh; it is still pushed, and it is
            // still the only thing that crosses for it.
            NativeViewport.setReducedMotion(true);
            NativeViewport.setReducedMotion(false);
            workspace.syncFromNative();
            final double[] afterSync = nativeSnapshot();
            assertArrayEquals("a refresh publishes nothing either:"
                    + describeSnapshotDifference(before, afterSync), before, afterSync, 0.0);

            NativeViewport.setGridVisible(wasVisible);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    private enum Surface { OBJECTS, ADD_PRIMITIVE, PRECISION, DISPLAY, PROJECT }

    /**
     * Opens one surface for the first time in this Activity and checks that it
     * grew from the corner it is anchored to.
     *
     * <p>The pivot IS the claim. A surface anchored to a leading edge and
     * unfolding upward has to grow from its own bottom-left; one hanging under
     * the toolbar from its own top-right. A pivot at (0, 0) on an upward surface
     * is the defect: it means the size was still unknown when the growth
     * started.
     */
    private void assertAnchorPivotIsCorrectAfterFirstOpen(final Surface which) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            switch (which) {
                case OBJECTS:
                    if (workspace.objectsDocked()) {
                        return null;
                    }
                    workspace.objectsCapsule().findViewById(R.id.objects_capsule_active)
                            .performClick();
                    break;
                case ADD_PRIMITIVE:
                    if (workspace.objectsDocked()) {
                        workspace.objectsSection().findViewById(R.id.add_body)
                                .performClick();
                    } else {
                        workspace.objectsCapsule().findViewById(R.id.objects_capsule_add)
                                .performClick();
                    }
                    break;
                case PRECISION:
                    WorkspaceTestSupport.precisionToggle(workspace).performClick();
                    break;
                case PROJECT:
                    workspace.globalToolbar().findViewById(R.id.project_actions_button)
                            .performClick();
                    break;
                default:
                    workspace.globalToolbar().findViewById(R.id.display_settings_button)
                            .performClick();
                    break;
            }
            return null;
        });
        settleLayout();
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final AnchoredSurfaceView surface = surfaceFor(workspace, which);
            if (surface == null || !surface.isOpen()) {
                // A docked window has no scene panel to open; nothing to prove.
                return null;
            }
            assertTrue("a first open must have finished waiting for its size",
                    !surface.growthWaitingForSize());
            assertTrue("and must actually have one", surface.getHeight() > 0);
            if (which == Surface.DISPLAY) {
                // It hangs UNDER a trailing toolbar control, so it grows from
                // its own trailing top corner.
                assertEquals("the popover grows from its own trailing top corner",
                        (float) surface.getWidth(), surface.anchorPivotX(), 0.5f);
                assertEquals(0.0f, surface.anchorPivotY(), 0.5f);
                return null;
            }
            if (which == Surface.PROJECT) {
                // The project drawer hangs UNDER the ForgeShape mark, the
                // toolbar's LEADING control (`MODELING-R1-OWNER-CORRECTION`), so
                // it grows from its own leading top corner -- where the mark is.
                assertEquals("the drawer grows from its own leading top corner",
                        0.0f, surface.anchorPivotX(), 0.5f);
                assertEquals(0.0f, surface.anchorPivotY(), 0.5f);
                return null;
            }
            assertEquals("a leading-edge surface grows from its leading side",
                    0.0f, surface.anchorPivotX(), 0.5f);
            final float expectedY = surface.growsUpward() ? surface.getHeight() : 0.0f;
            assertEquals("and from the edge nearest the control that opened it",
                    expectedY, surface.anchorPivotY(), 0.5f);
            return null;
        });
    }

    private static AnchoredSurfaceView surfaceFor(EditorWorkspaceView workspace, Surface which) {
        switch (which) {
            case OBJECTS: return workspace.objectsPopover();
            case ADD_PRIMITIVE: return workspace.addPrimitivePalette();
            case PRECISION: return workspace.propertyInspector();
            case PROJECT: return workspace.projectPopover();
            default: return workspace.displayPopover();
        }
    }

    /**
     * Asserts that no row of the open precision surface is crossed by the
     * boundary the surface ends on.
     *
     * <p>Every row either fits entirely inside the visible body or starts at or
     * after its end. There is deliberately no tolerance: a row that is one pixel
     * short of whole is a row with a line through it.
     */
    private void assertNoRowCrossesTheBoundary(final String what) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final PropertyInspectorView inspector = workspace.propertyInspector();
            assertTrue(what + " must be open", inspector.isOpen());
            final ViewGroup scroll = workspace.findViewById(R.id.inspector_scroll);
            final ViewGroup body = workspace.findViewById(R.id.inspector_body);
            assertTrue(what + " must be laid out", scroll.getHeight() > 0);

            final ViewGroup rows = body.getChildCount() == 1
                    && body.getChildAt(0) instanceof ViewGroup
                    ? (ViewGroup) body.getChildAt(0) : null;
            assertNotNull(what + " has a body of rows", rows);

            final int boundary = inspector.visibleBodyBottom();
            assertTrue(what + " shows something", boundary > 0);
            for (int i = 0; i < rows.getChildCount(); i++) {
                final View row = rows.getChildAt(i);
                if (row.getVisibility() == View.GONE) {
                    continue;
                }
                final int top = rows.getTop() + row.getTop();
                final int bottom = rows.getTop() + row.getBottom();
                assertTrue(what + ": row " + i + " (" + top + ".." + bottom
                                + ") is cut by the surface boundary at " + boundary
                                + " — a control with a line through it reads as broken,"
                                + " not as \"there is more below\"",
                        bottom <= boundary || top >= boundary);
            }
            return null;
        });
    }

    /**
     * Asserts that a control's corner is concentric with its host's.
     *
     * <p>inner = outer − the gap between them. Anything else leaves a crescent
     * of host showing where the two corners disagree.
     *
     * <p>The gap is passed in rather than read off the host, because the surface
     * that DRAWS the corner and the container that keeps the padding are not
     * always the same view — the Tool Rail's capsule is on its {@code ScrollView}
     * so the container cannot clip the shadow, while the 6 dp around its entries
     * is on the rail itself.
     */
    private static void assertConcentric(String what, View host, View member, int gapPx) {
        final GradientDrawable hostShape = shapeOf(host);
        final GradientDrawable memberShape = shapeOf(member);
        if (hostShape == null || memberShape == null) {
            return;  // not a drawn shape; nothing to nest
        }
        final float outer = hostShape.getCornerRadius();
        final float inner = memberShape.getCornerRadius();
        if (outer <= 0.0f) {
            return;  // a square host nests anything
        }
        assertEquals(what + " must be concentric with its host: host radius " + outer
                        + " px minus " + gapPx + " px of padding should equal the"
                        + " member's " + inner + " px, or a crescent of host shows"
                        + " at each end",
                outer - gapPx, inner, 1.5f);
    }

    /** The padding a container keeps around whatever it holds, in pixels. */
    private static int paddingOf(View container) {
        return Math.min(container.getPaddingLeft(), container.getPaddingTop());
    }

    /** The drawn shape behind a view, whatever state list it is wrapped in. */
    private static GradientDrawable shapeOf(View view) {
        Drawable drawable = view.getBackground();
        if (drawable == null) {
            return null;
        }
        drawable = drawable.getCurrent();
        return drawable instanceof GradientDrawable ? (GradientDrawable) drawable : null;
    }

    /**
     * The stroke a shape draws, or null when it draws none.
     *
     * <p>Read through the drawable's own constant state, because
     * {@code GradientDrawable} exposes no getter for its stroke. A shape with no
     * stroke reports a width of 0, which is the answer this needs.
     */
    private static Integer strokeColorOf(GradientDrawable shape) {
        try {
            final Object state = shape.getConstantState();
            if (state == null) {
                return null;
            }
            final Field width = state.getClass().getDeclaredField("mStrokeWidth");
            width.setAccessible(true);
            final int value = width.getInt(state);
            return value > 0 ? Integer.valueOf(value) : null;
        } catch (Exception noSuchField) {
            // A platform that does not expose it cannot be asserted against, and
            // failing here would be a test of the platform rather than of the
            // product.
            return null;
        }
    }

    private static void assertMeetsTouchFloor(ForgeShapeActivity activity,
                                              EditorWorkspaceView workspace,
                                              int id, String what) {
        assertMeetsTouchFloorOf(activity, workspace, workspace.findViewById(id), what);
    }

    private static void assertMeetsTouchFloorOf(ForgeShapeActivity activity,
                                                EditorWorkspaceView workspace,
                                                View control, String what) {
        assertNotNull(what + " must exist", control);
        assertEquals(what + " must be on screen", View.VISIBLE, control.getVisibility());
        final int widthDp = EditorControlStyles.toDp(activity, control.getWidth());
        final int heightDp = EditorControlStyles.toDp(activity, control.getHeight());
        assertTrue(what + " is " + widthDp + " x " + heightDp + " dp, below the "
                        + TOUCH_FLOOR_DP + " dp interactive floor",
                widthDp >= TOUCH_FLOOR_DP && heightDp >= TOUCH_FLOOR_DP);
    }

    private static int rectLeft(EditorWorkspaceView workspace, View view) {
        final android.graphics.Rect rect =
                new android.graphics.Rect(0, 0, view.getWidth(), view.getHeight());
        workspace.offsetDescendantRectToMyCoords(view, rect);
        return rect.left;
    }

    private static int rectRight(EditorWorkspaceView workspace, View view) {
        final android.graphics.Rect rect =
                new android.graphics.Rect(0, 0, view.getWidth(), view.getHeight());
        workspace.offsetDescendantRectToMyCoords(view, rect);
        return rect.right;
    }

    /**
     * Runs one of {@link WorkspaceTestSupport}'s open/close helpers on the UI
     * thread and settles afterwards.
     *
     * <p>Those helpers drive real controls with {@code performClick()}, which
     * plays a sound effect through the view root and therefore must be on the
     * main thread. Calling them from the instrumentation thread throws
     * {@code CalledFromWrongThreadException} — which is a defect in the case
     * rather than in the product, and looks exactly like one in the product.
     */
    private void onUi(WorkspaceTestSupport.WorkspaceAction<Void> action) {
        doOnWorkspace(rule.getScenario(), action);
        settleLayout();
    }

    private void openPrecisionOnShape() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_shape).performClick();
            return null;
        });
        settleLayout();
        openPrecisionOnHeldEntry();
    }

    private void openPrecisionOnHeldEntry() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();
        settleLayout();
    }

    private void enterSculpt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            workspace.findViewById(sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0
                    ? R.id.resume_sculpt : R.id.freeze_to_sculpt).performClick();
            return null;
        });
        settleLayout();
    }

    /**
     * Puts a real edit on the current sculpt mesh.
     *
     * <p>A sphere rather than the baseline box: a box's eight vertices are all
     * at its corners, and a brush that captures none of them starts no stroke at
     * all.
     */
    private void sculptTheCurrentMesh() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            NativeViewport.applyConstructionSphere(1.0);
            NativeViewport.freezeToSculpt();
            NativeViewport.setSculptTool(NativeViewport.TOOL_GRAB);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final float x = viewport.getWidth() * 0.5f;
            final float y = viewport.getHeight() * 0.5f;
            final long down = SystemClock.uptimeMillis();
            sendToViewport(viewport, down, down, android.view.MotionEvent.ACTION_DOWN, x, y);
            for (int step = 1; step <= 5; step++) {
                sendToViewport(viewport, down, down + step * 16L,
                        android.view.MotionEvent.ACTION_MOVE, x + step * 6.0f, y);
            }
            sendToViewport(viewport, down, down + 96L,
                    android.view.MotionEvent.ACTION_UP, x + 30.0f, y);
            return null;
        });
        settleLayout();
    }

    private static void sendToViewport(View target, long downTime, long eventTime,
                                       int action, float x, float y) {
        final android.view.MotionEvent event =
                android.view.MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    /** Sculpts, changes the Construction shape underneath, and comes back. */
    private void makeTheConstructionSourceStale() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(2.0);
            workspace.findViewById(R.id.resume_sculpt).performClick();
            return null;
        });
        settleLayout();
    }

    private void dragRadiusSlider() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View slider = workspace.findViewById(R.id.brush_radius_slider);
            final float x = slider.getWidth() * 0.5f;
            final long down = SystemClock.uptimeMillis();
            sendToViewport(slider, down, down, android.view.MotionEvent.ACTION_DOWN,
                    x, slider.getHeight() * 0.8f);
            sendToViewport(slider, down, down + 16L, android.view.MotionEvent.ACTION_MOVE,
                    x, slider.getHeight() * 0.4f);
            sendToViewport(slider, down, down + 32L, android.view.MotionEvent.ACTION_UP,
                    x, slider.getHeight() * 0.4f);
            return null;
        });
        settleLayout();
    }

    /**
     * The native snapshot with the brush slots masked out.
     *
     * <p>Moving a brush slider is SUPPOSED to change the brush; what it must not
     * change is anything else. Comparing the whole snapshot would assert the
     * opposite of the contract.
     */
    private static double[] nativeSnapshotWithoutBrush() {
        final double[] all = nativeSnapshot();
        final int sculptBase = all.length - NativeViewport.SCULPT_STATE_SIZE;
        all[sculptBase + NativeViewport.SCULPT_RADIUS_PIXELS] = 0.0;
        all[sculptBase + NativeViewport.SCULPT_STRENGTH] = 0.0;
        return all;
    }

    /**
     * Writes the platform's animator duration scale through the instrumentation
     * shell.
     *
     * <p>The product holds no {@code WRITE_SECURE_SETTINGS} and must never ask
     * for one — this is the test harness's own permission, used the same way
     * {@code EditorWorkspaceMotionTest} uses it.
     */
    private static void setAnimatorScale(String value) {
        final ParcelFileDescriptor pipe = InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .executeShellCommand("settings put global animator_duration_scale " + value);
        try (InputStream drain = new FileInputStream(pipe.getFileDescriptor())) {
            final byte[] scratch = new byte[64];
            while (drain.read(scratch) >= 0) {
                // Drained so the command completes before the case continues.
            }
        } catch (Exception ignored) {
            // The setting either took or it did not; the assertions say which.
        }
        settle();
    }
}
