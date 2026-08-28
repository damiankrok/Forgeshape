package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closeAddPrimitive;
import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openAddPrimitive;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * UI-R4C — the four composition defects the UI-R4B screenshot review found.
 *
 * <p>Every one of them was <b>visible</b> and none of them was assertable: the
 * workspace laid out, every control was reachable, every id was where it should
 * be, and the product still read wrong. A critical destination abbreviated to
 * "Back to Constructi…"; a creation surface parked half on top of a live
 * control; a primary action drawn as a button inside a button; and a caption
 * standing across the top of the viewport describing the workflow the user was
 * already performing. What follows turns each of the four into a rule, so the
 * next restyle has to keep it rather than re-discover it.
 *
 * <p>The two rules the rest of the instrumentation follows are not relaxed here.
 * Nothing is located by screen coordinate — every control is named by its stable
 * semantic id, because the workspace re-composes itself per window. And nothing
 * reads a rendered pixel: what the Vulkan viewport draws is the renderer's
 * business, verified by the native suites.
 *
 * <p><b>Windows.</b> Cases that are about a window read the window they are
 * actually in and assert the contract that belongs to it, so running this suite
 * under a display override is a genuine expanded run rather than a simulation.
 * That is what makes {@code UIR4C-01} (compact), {@code UIR4C-02} (short
 * landscape) and {@code UIR4C-03} (expanded) the same assertions in three
 * configurations rather than three copies of one case.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceChromeCompositionTest {

    /** The interactive floor, in dp, for user-operated Editor Workspace chrome. */
    private static final int TOUCH_FLOOR_DP = 48;

    /**
     * The narrowest window the product is verified in.
     *
     * <p>At or above it the full wording of the critical back navigation must be
     * drawn — that is the whole claim of {@code UIR4C-01}. Below it the approved
     * short form is allowed, and {@code assertBackNavigationReads} still refuses
     * an ellipsis either way.
     */
    private static final int NARROWEST_VERIFIED_WIDTH_DP = 360;

    /** The six primitives the product actually builds, in the palette's order. */
    private static final int[] PALETTE_IDS = {
            R.id.add_primitive_box, R.id.add_primitive_cylinder,
            R.id.add_primitive_sphere, R.id.add_primitive_cone,
            R.id.add_primitive_capsule, R.id.add_primitive_plane
    };

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void leaveTheWindowAsItWasFound() {
        releaseOrientation(rule.getScenario());
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // UIR4C-01 / UIR4C-02 / UIR4C-03 — the way out of Sculpt is never guessed at
    // -----------------------------------------------------------------------

    /**
     * UIR4C-01 (compact) and UIR4C-03 (expanded). Back to Construction is drawn
     * in full, with no ellipsis and no clipping, in the window this run is in.
     *
     * <p>The defect: the transition button's width was a single dp constant,
     * sized against the narrowest window the product supports and then applied
     * to every window. The label is the longest in the product, so it truncated
     * on a phone, on a short landscape window and on a tablet alike — three
     * windows, two of which had upwards of 50 dp of unused row beside it. Back
     * to Construction is the only way out of Sculpt Mode, and a destination the
     * user has to guess at is not navigation.
     */
    @Test
    public void uir4c01_compactAndExpandedSculptDrawTheBackNavigationInFull() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertBackNavigationReads(activity, workspace, "in the window under test");
            return null;
        });
    }

    /**
     * UIR4C-02. And so does a short landscape window, which is the one that has
     * to carry an inline status message in the same row.
     */
    @Test
    public void uir4c02_shortLandscapeDrawsTheBackNavigationInFull() {
        enterSculpt();
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertBackNavigationReads(activity, workspace, "in short landscape");
            return null;
        });
    }

    /**
     * UIR4C-08. The floor survived the geometry reduction.
     *
     * <p>The transition control gave up its host capsule's 8 dp of padding, so
     * it is a 48 dp control rather than a 48 dp control inside a 56 dp pill.
     * That is a change to the drawn mass and must not be a change to the hit
     * area, which is the number {@code UIR4B-17} raised and this keeps.
     */
    @Test
    public void uir4c08_theTransitionControlStillMeetsTheFortyEightDpFloor() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertMeetsTouchFloor(activity, visibleTransition(workspace),
                    "the Construction → Sculpt transition");
            return null;
        });
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertMeetsTouchFloor(activity,
                    workspace.findViewById(R.id.back_to_construction),
                    "Back to Construction");
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4C-04 / UIR4C-05 — a surface may stand on the model, not on a control
    // -----------------------------------------------------------------------

    /**
     * UIR4C-04. Add Primitive covers no independently live control.
     *
     * <p>The defect: the palette is wider than the distance from the Objects
     * capsule's {@code +} to the window edge, and the anchor's clamp was "as far
     * right as the window allows" — which slid it under the trailing tool
     * cluster and left a crescent of the precision toggle showing from behind
     * it. A half-covered control still takes a touch, and the panel over it
     * reads as a rendering fault rather than as a layer.
     *
     * <p>Asserted against the cluster's own bounds rather than against a
     * coordinate, and against the toggle as well as the column, because the
     * column is what the anchor avoids and the toggle is what the review saw.
     */
    @Test
    public void uir4c04_addPrimitiveCoversNoLiveTrailingControl() {
        assertAddPrimitiveClearsTheToolCluster("in the window under test");
    }

    /** UIR4C-04, in the short window, where the row is tightest. */
    @Test
    public void uir4c04_addPrimitiveCoversNoLiveTrailingControlInShortLandscape() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        settleLayout();
        assertAddPrimitiveClearsTheToolCluster("in short landscape");
    }

    /**
     * UIR4C-05. Moving the palette changed nothing about what it offers.
     *
     * <p>Six tiles, and the routing still ends in the domain: the created body's
     * kind is read back from {@code constructionPrimitive}, so a palette that
     * had quietly become a Java-side generator would fail here even though every
     * tile still drew.
     */
    @Test
    public void uir4c05_addPrimitiveStillOffersSixPrimitivesAndRoutesNatively() {
        onUi((activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int id : PALETTE_IDS) {
                final View tile = workspace.findViewById(id);
                assertNotNull(activity.getResources().getResourceEntryName(id)
                        + " must still be offered", tile);
                assertTrue("and still be tappable", tile.isClickable());
            }
            assertEquals("the palette holds exactly the six primitives",
                    PALETTE_IDS.length, countTiles(workspace.addPrimitivePalette()));
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.add_primitive_sphere).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            // Slot 0 is the active kind, whatever the kind is.
            assertEquals("choosing Sphere still ends in the domain's own sphere",
                    NativeViewport.PRIMITIVE_SPHERE, (int) primitive[0]);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4C-06 / UIR4C-07 — one control, drawn once
    // -----------------------------------------------------------------------

    /**
     * UIR4C-06. Start Sculpting is a single control, not a control in a host.
     *
     * <p>The defect: the editing group is a floating capsule, and a compact
     * window withdraws the context label out of it — leaving a 26 dp dark pill
     * drawn around a single 22 dp blue button, with a crescent of host showing
     * all the way round. It read as a halo, and it made the transition the
     * heaviest object in a resting workspace whose subject is the model.
     *
     * <p>A capsule is a relation between controls, so where there is one control
     * there is no capsule: the group stops painting and stops padding, and the
     * lone member takes the capsule's own corner and depth. Where the group
     * genuinely holds two members the segmented relation is unchanged, and the
     * member stays concentric with the host — which is what this asserts in an
     * expanded window rather than skipping.
     */
    @Test
    public void uir4c06_startSculptingIsOneVisualControl() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEditingGroupIsHonest(workspace, "Start Sculpting");
            return null;
        });
    }

    /**
     * UIR4C-07. Resume Sculpt follows the same geometry, and returns the same
     * mesh.
     *
     * <p>Both halves matter together: the point of the cleanup is that the two
     * states of one transition slot look like one control, and the point of the
     * control is that leaving and coming back loses nothing. A change that made
     * the geometry agree by making Resume a different act would pass the first
     * assertion and fail the second.
     */
    @Test
    public void uir4c07_resumeSculptUsesTheSameGeometryAndTheSameMesh() {
        enterSculpt();
        final double[] frozen = onWorkspace(rule.getScenario(),
                (activity, workspace) -> sculptState());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Resume Sculpt is the transition offered once a mesh exists",
                    View.VISIBLE,
                    workspace.findViewById(R.id.resume_sculpt).getVisibility());
            assertEditingGroupIsHonest(workspace, "Resume Sculpt");
            workspace.findViewById(R.id.resume_sculpt).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] resumed = sculptState();
            assertEquals("Resume returns the same sculpt revision",
                    frozen[NativeViewport.SCULPT_REVISION],
                    resumed[NativeViewport.SCULPT_REVISION], 0.0);
            assertEquals("and the same mesh",
                    frozen[NativeViewport.SCULPT_VERTEX_COUNT],
                    resumed[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4C-09 / UIR4C-10 — the status line reports, and does not caption
    // -----------------------------------------------------------------------

    /**
     * UIR4C-09. A resting Construction workspace says nothing.
     *
     * <p>The defect: entering Construction wrote "Construction — choose a shape,
     * type its exact values, then Apply." Every resting Construction screenshot
     * of the review therefore opened with a sentence across the top of the
     * viewport, describing the product rather than reporting an event — and
     * because the status capsule is a surface, an instruction that is always
     * true is a permanent claim on the workspace.
     *
     * <p>Asserted from the real entry point, the start question's own
     * Construction option, rather than from the reset helper — the defect was in
     * what that entry point wrote.
     */
    @Test
    public void uir4c09_restingConstructionCarriesNoInstructionalPill() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showStartChooserAsFirstLaunch();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.start_option_construction).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNoStatusCapsule(workspace, "a resting Construction workspace");
            return null;
        });
    }

    /**
     * UIR4C-10. Opening the exact values adds no global instruction, and the
     * transient path still works.
     *
     * <p>The second defect of the same kind: every Shape/Transform switch wrote
     * "Shape and placement — edit exact values, then Apply.", which is a third
     * copy of what the held rail entry, the toggle beneath it and the surface's
     * own title — "Exact Shape — Body #1" — already say. Because it was written
     * on every switch, it stood over the model exactly while the user was doing
     * what it described.
     *
     * <p>What must not go with it is the lifecycle: a rejection still has to be
     * written, still has to stand, and still has to leave nothing behind.
     */
    @Test
    public void uir4c10_openingTheExactValuesAddsNoGlobalInstruction() {
        onUi((activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_shape).performClick();
            return null;
        });
        onUi((activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNoStatusCapsule(workspace, "Exact Shape open");
            return null;
        });
        onUi((activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_place).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNoStatusCapsule(workspace, "Exact Transform open");

            // And the lifecycle the instruction was NOT part of is intact.
            workspace.showStatus(activity.getString(R.string.reject_relation),
                    R.attr.fsTextError);
            assertTrue("a rejection is still written",
                    workspace.globalToolbar().statusVisible());
            assertEquals(activity.getString(R.string.reject_relation),
                    workspace.globalToolbar().statusText().toString());
            assertEquals("and there is still nothing standing behind it", "",
                    workspace.globalToolbar().standingStatusText().toString());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4C-11 / UIR4C-12 — nothing else moved
    // -----------------------------------------------------------------------

    /**
     * UIR4C-11. The appearance set is still three, and the composition cleanup
     * introduced no colour.
     *
     * <p>The lone transition control needed a background at the capsule's own
     * radius rather than at the member radius inside it. That is a corner, not a
     * palette: each new form has to carry exactly the fill of the member form it
     * replaces, in every appearance, or the cleanup has quietly authored a
     * fourth value of a twelve-value approved palette.
     *
     * <p>Resolved through a {@link ContextThemeWrapper} per appearance rather
     * than by switching the running one, so all three are asserted in one case
     * and none of them depends on an Activity recreation landing.
     */
    @Test
    public void uir4c11_theThreeAppearancesKeepTheirExactValues() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("exactly three appearances", 3, AppTheme.values().length);
            for (AppTheme theme : AppTheme.values()) {
                final Context themed = new ContextThemeWrapper(activity, theme.styleRes());
                assertEquals(theme + ": the lone primary form takes the member form's fill",
                        restingFillOf(themed, R.drawable.bg_capsule_primary),
                        restingFillOf(themed, R.drawable.bg_pill_primary));
                assertEquals(theme + ": and so does the lone tonal form",
                        restingFillOf(themed, R.drawable.bg_capsule_tonal),
                        restingFillOf(themed, R.drawable.bg_pill_tonal));
                assertEquals(theme + ": the lone form is drawn at the capsule's own radius",
                        EditorControlStyles.dimen(themed, R.dimen.radius_capsule),
                        Math.round(restingShapeOf(themed, R.drawable.bg_pill_primary)
                                .getCornerRadius()));
            }
            return null;
        });
    }

    /**
     * UIR4C-12. None of it touched geometry.
     *
     * <p>Every act in this stage is presentation: a label that fits, a panel
     * that moves, a capsule that stops being drawn, a caption that is not
     * written. The domain must not learn that any of it happened — no
     * publication, no rebuild, no upload — and the way to assert that from Java
     * is to compare the whole native state bit for bit across the sequence.
     */
    @Test
    public void uir4c12_theCompositionCleanupPublishesNoGeometry() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        onUi((activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        onUi((activity, workspace) -> {
            closeAddPrimitive(workspace);
            return null;
        });
        onUi((activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_place).performClick();
            return null;
        });
        onUi((activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_shape).performClick();
            return null;
        });
        onUi((activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("pure UI work published nothing:"
                + describeSnapshotDifference(before, after), before, after, 0.0);
    }

    // -----------------------------------------------------------------------
    // Assertions
    // -----------------------------------------------------------------------

    /**
     * The back navigation reads, whole, in whatever window this is.
     *
     * <p>Three claims, and the ellipsis one is the load-bearing half: a
     * {@code TextView} that has ellipsised reports it through its own layout, so
     * this asks the view what it drew rather than comparing a measured width to
     * a guess. The full wording is required outright at or above the narrowest
     * window the product is verified in; below that the approved short form is
     * accepted, and only when the full wording genuinely does not fit.
     */
    private static void assertBackNavigationReads(ForgeShapeActivity activity,
                                                  EditorWorkspaceView workspace,
                                                  String where) {
        final TextView back = workspace.findViewById(R.id.back_to_construction);
        assertNotNull("Back to Construction must exist " + where, back);
        assertEquals("Back to Construction must be drawn " + where,
                View.VISIBLE, back.getVisibility());

        final String full = activity.getString(R.string.back_to_construction);
        final String abbreviated = activity.getString(R.string.back_to_construction_short);
        final String drawn = back.getText().toString();
        final int widthDp = EditorControlStyles.toDp(activity, workspace.getWidth());

        if (widthDp >= NARROWEST_VERIFIED_WIDTH_DP) {
            assertEquals("a " + widthDp + " dp window can carry the whole wording " + where,
                    full, drawn);
        } else {
            assertTrue("only the full wording or the approved short form may be drawn "
                    + where + ", and this read \"" + drawn + "\"",
                    full.equals(drawn) || abbreviated.equals(drawn));
        }
        assertTrue("the short form is only for a row that cannot carry the sentence",
                full.equals(drawn)
                        || naturalWidth(back, full) > back.getMaxWidth());
        assertEquals("what a screen reader announces never changes with the window",
                full, String.valueOf(back.getContentDescription()));

        assertNotNull("the control must have been laid out " + where, back.getLayout());
        assertEquals("critical navigation is never ellipsised " + where,
                0, back.getLayout().getEllipsisCount(0));
        assertTrue("and never clipped by the window " + where,
                WorkspaceTestSupport.isFullyOnScreen(back, workspace));
    }

    /**
     * The palette stands clear of the trailing tool cluster.
     *
     * <p>Bounds against bounds, in the workspace's own coordinates — which is
     * not "locating a control by coordinate": neither rectangle is written down
     * anywhere, both are read from the views the ids name, and the assertion is
     * a relation between them that holds in any window.
     */
    private void assertAddPrimitiveClearsTheToolCluster(String where) {
        onUi((activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View palette = workspace.addPrimitivePalette();
            assertEquals("the palette must be open " + where,
                    View.VISIBLE, palette.getVisibility());
            assertNoOverlap(workspace, palette, WorkspaceTestSupport.trailingHost(workspace),
                    "the trailing tool cluster " + where);
            assertNoOverlap(workspace, palette, WorkspaceTestSupport.precisionGroup(workspace),
                    "the precision control " + where);
            assertTrue("and the palette itself stays inside the window " + where,
                    WorkspaceTestSupport.isFullyOnScreen(palette, workspace));
            return null;
        });
        onUi((activity, workspace) -> {
            closeAddPrimitive(workspace);
            return null;
        });
    }

    private static void assertNoOverlap(EditorWorkspaceView workspace, View surface,
                                        View control, String what) {
        if (control.getVisibility() != View.VISIBLE || control.getWidth() <= 0) {
            return;  // withdrawn in this window; there is nothing to cover
        }
        final Rect surfaceBounds = boundsIn(workspace, surface);
        final Rect controlBounds = boundsIn(workspace, control);
        assertFalse("Add Primitive " + surfaceBounds + " must not cover " + what + " "
                        + controlBounds,
                Rect.intersects(surfaceBounds, controlBounds));
    }

    /**
     * The editing group draws a capsule only while it is holding a group.
     *
     * <p>Both directions are asserted, because both are the rule: one visible
     * member and the group must not be drawn at all, two and it must be drawn
     * with its member concentric inside it.
     */
    private static void assertEditingGroupIsHonest(EditorWorkspaceView workspace,
                                                   String what) {
        final View group = workspace.findViewById(R.id.toolbar_editing_group);
        assertNotNull("the editing group must exist", group);
        final View lone = onlyVisibleChildOf(group);
        if (lone == null) {
            assertNotNull(what + ": a group of two or more is still drawn as a capsule",
                    group.getBackground());
            return;
        }
        assertNull(what + " is the only control in its host, so the host must not be"
                + " drawn around it — a capsule around one control is a halo",
                group.getBackground());
        assertEquals(what + ": a host that draws nothing pads nothing either",
                0, group.getPaddingLeft());
        assertEquals(0, group.getPaddingTop());
        final GradientDrawable shape = restingShapeOf(lone);
        assertNotNull(what + " must still be a drawn control", shape);
        assertEquals(what + " takes the capsule's own corner once it IS the capsule",
                EditorControlStyles.dimen(group.getContext(), R.dimen.radius_capsule),
                Math.round(shape.getCornerRadius()));
        assertTrue(what + " carries the depth its host used to",
                lone.getElevation() > 0.0f);
    }

    private static void assertNoStatusCapsule(EditorWorkspaceView workspace, String where) {
        assertFalse(where + " must carry no status capsule, and it read \""
                        + workspace.globalToolbar().statusText() + "\"",
                workspace.globalToolbar().statusVisible());
        assertEquals(where + " has nothing standing either", "",
                workspace.globalToolbar().standingStatusText().toString());
    }

    private static void assertMeetsTouchFloor(ForgeShapeActivity activity, View control,
                                              String what) {
        assertNotNull(what + " must exist", control);
        assertEquals(what + " must be on screen", View.VISIBLE, control.getVisibility());
        final int widthDp = EditorControlStyles.toDp(activity, control.getWidth());
        final int heightDp = EditorControlStyles.toDp(activity, control.getHeight());
        assertTrue(what + " is " + widthDp + " x " + heightDp + " dp, below the "
                        + TOUCH_FLOOR_DP + " dp interactive floor",
                widthDp >= TOUCH_FLOOR_DP && heightDp >= TOUCH_FLOOR_DP);
    }

    // -----------------------------------------------------------------------
    // Reading the workspace
    // -----------------------------------------------------------------------

    private static Rect boundsIn(EditorWorkspaceView workspace, View view) {
        final Rect bounds = new Rect(0, 0, view.getWidth(), view.getHeight());
        workspace.offsetDescendantRectToMyCoords(view, bounds);
        return bounds;
    }

    /** The one visible child of a container, or null when it holds another
     *  number of them. */
    private static View onlyVisibleChildOf(View container) {
        final android.view.ViewGroup group = (android.view.ViewGroup) container;
        View only = null;
        for (int i = 0; i < group.getChildCount(); i++) {
            final View child = group.getChildAt(i);
            if (child.getVisibility() != View.VISIBLE) {
                continue;
            }
            if (only != null) {
                return null;
            }
            only = child;
        }
        return only;
    }

    private static View visibleTransition(EditorWorkspaceView workspace) {
        for (int id : new int[]{R.id.freeze_to_sculpt, R.id.resume_sculpt,
                R.id.back_to_construction}) {
            final View control = workspace.findViewById(id);
            if (control != null && control.getVisibility() == View.VISIBLE) {
                return control;
            }
        }
        return null;
    }

    private static int countTiles(View palette) {
        int tiles = 0;
        for (int id : PALETTE_IDS) {
            if (palette.findViewById(id) != null) {
                tiles++;
            }
        }
        return tiles;
    }

    private static int naturalWidth(TextView view, String text) {
        return Math.round(view.getPaint().measureText(text))
                + view.getPaddingLeft() + view.getPaddingRight();
    }

    /** The shape a control draws at rest, through whatever state list wraps it. */
    private static GradientDrawable restingShapeOf(View view) {
        final Drawable drawable = view.getBackground();
        if (drawable == null) {
            return null;
        }
        drawable.setState(new int[0]);
        final Drawable current = drawable.getCurrent();
        return current instanceof GradientDrawable ? (GradientDrawable) current : null;
    }

    private static GradientDrawable restingShapeOf(Context themed, int drawableRes) {
        final Drawable drawable = themed.getDrawable(drawableRes);
        assertNotNull("the drawable must resolve in this appearance", drawable);
        drawable.setState(new int[0]);
        return (GradientDrawable) drawable.getCurrent();
    }

    private static int restingFillOf(Context themed, int drawableRes) {
        return restingShapeOf(themed, drawableRes).getColor().getDefaultColor();
    }

    private static double[] sculptState() {
        final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.sculptState(sculpt);
        return sculpt;
    }

    // -----------------------------------------------------------------------
    // Driving the workspace
    // -----------------------------------------------------------------------

    /**
     * Runs one of the shared open/close helpers on the UI thread and waits for
     * the traversal it caused.
     *
     * <p>Those helpers drive real controls with {@code performClick()}, which
     * plays a sound effect through the view root and therefore must be on the
     * main thread.
     */
    private void onUi(WorkspaceTestSupport.WorkspaceAction<Void> action) {
        doOnWorkspace(rule.getScenario(), action);
        settleLayout();
    }

    private void enterSculpt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = sculptState();
            workspace.findViewById(sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0
                    ? R.id.resume_sculpt : R.id.freeze_to_sculpt).performClick();
            return null;
        });
        settleLayout();
    }
}
