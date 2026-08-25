package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.unoccludedViewportFraction;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * R1B1-09..14 — the visual foundation, and the Tool Rail defect it shipped
 * alongside.
 *
 * <p>Nothing here asserts a colour, a corner radius or a shadow. Those are
 * judged by eye and by the runtime walkthrough, and a test that pinned them
 * would break on every deliberate restyle while proving nothing about whether
 * the product works. What is asserted instead is the part that is a
 * <i>contract</i>: that no control is a font glyph any more, that pressing one
 * visibly does something, that a tap on a tool is a tap and a scroll is a
 * scroll, and that none of it cost the viewport any of the room it had.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceFoundationTest {

    /**
     * The glyphs the shell used as icons before this stage.
     *
     * <p>Listed explicitly rather than checked by character range, so this
     * fails loudly if one is reintroduced and does not accidentally forbid
     * ordinary text. The em dash and the middle dot in the status strings are
     * prose and are deliberately absent from this list.
     */
    private static final String[] RETIRED_PSEUDO_ICONS = {
            "◈", "●", "≈", "◎", "▣", "⊕", "▱", "▤", "◐", "⊟", "⊞", "⌄", "⌃"
    };

    /**
     * The viewport floors, matching the layout suite exactly.
     *
     * <p>The floor depends on what the shell is currently showing, which is the
     * honest way to state it: an open inspector legitimately costs the model
     * more room than a collapsed one, and a docked panel on a wide window costs
     * more again while leaving far more absolute area. A single number would
     * either be too weak to catch a regression or fail on correct behaviour.
     */
    private static final double COMPACT_FLOOR = 0.60;
    private static final double COMPACT_FLOOR_INSPECTOR_OPEN = 0.50;
    private static final double EXPANDED_FLOOR = 0.40;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void restoreTheRailAndTheOrientation() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // R1B1-11/12 shrink the rail's scroll container so it can actually
            // scroll. Put it back, or a later case measures a rail that is not
            // the product's.
            final ScrollView scroll = workspace.toolRailScroll();
            final ViewGroup.LayoutParams params = scroll.getLayoutParams();
            params.height = ViewGroup.LayoutParams.WRAP_CONTENT;
            scroll.setLayoutParams(params);
            scroll.scrollTo(0, 0);
            NativeViewport.enterConstructionMode();
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
        releaseOrientation(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // R1B1-09 -- every icon is a drawable, not a character
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_09_noChromeControlIsAUnicodePseudoIcon() {
        // Pinned to portrait for one reason: a window with no height to spare
        // deliberately drops the rail's icons and keeps its labels, because the
        // label is the part that says which tool this is without prior
        // learning. Asserting an icon in that window would be asserting against
        // the shell's own adaptive rule.
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);

        assertNoPseudoIconsAnywhere("in Construction");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Both icon-only toolbar controls, the restore chip, the precision
            // toggle, the capsule's plus and the inspector's close control are
            // images with real drawables behind them.
            for (int id : new int[]{R.id.display_settings_button, R.id.hide_ui_toggle,
                    R.id.restore_ui_chip, R.id.precision_toggle,
                    R.id.objects_capsule_add, R.id.inspector_toggle}) {
                final View control = workspace.findViewById(id);
                assertNotNull(activity.getResources().getResourceEntryName(id)
                        + " must exist", control);
                assertTrue(activity.getResources().getResourceEntryName(id)
                        + " must be an image, not a glyph in a TextView",
                        control instanceof ImageView);
                assertNotNull(activity.getResources().getResourceEntryName(id)
                                + " must actually carry an icon",
                        ((ImageView) control).getDrawable());
                assertNotNull("an icon-only control has no visible label, so its "
                                + "content description is the only name it has",
                        control.getContentDescription());
            }
            return null;
        });

        enterSculpt();
        assertNoPseudoIconsAnywhere("in Sculpt");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int id : new int[]{R.id.tool_rail_grab, R.id.tool_rail_clay,
                    R.id.tool_rail_smooth, R.id.tool_rail_inflate}) {
                assertTrue("every sculpt tool draws a real icon",
                        hasIconChild(workspace.findViewById(id)));
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B1-10 -- pressing a control does something immediately
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_10_chipsRailEntriesAndObjectRowsAllShowPressedFeedback() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long activeBody = NativeViewport.sceneActiveBodyId();
            final View[] controls = {
                    workspace.findViewById(R.id.unit_chip_mm),
                    workspace.findViewById(R.id.primitive_option_box),
                    workspace.findViewById(R.id.tool_rail_shape),
                    workspace.objectsSection().rowFor(activeBody),
                    workspace.findViewById(R.id.add_body),
                    workspace.findViewById(R.id.apply_shape),
                    workspace.findViewById(R.id.display_settings_button),
                    workspace.findViewById(R.id.objects_capsule_active),
                    workspace.findViewById(R.id.add_primitive_sphere),
            };
            final String[] names = {"a unit chip", "a primitive chip", "a rail entry",
                    "an Objects row", "the Objects column's plus", "Apply Shape",
                    "the Display button", "the Objects capsule's body name",
                    "an Add Primitive tile"};

            for (int i = 0; i < controls.length; i++) {
                assertNotNull(names[i] + " must be on screen", controls[i]);
                assertTrue(names[i] + " must look different while it is held down,"
                                + " or the product feels dead until native code answers",
                        hasPressedFeedback(controls[i]));
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B1-10b -- and it is still big enough to press
    // -----------------------------------------------------------------------

    /**
     * Every icon-only control keeps the stylus-first touch floor.
     *
     * <p>This is a regression guard for a defect the walkthrough found and this
     * stage fixed. The Global Toolbar's row can run out of width on a compact
     * window, and a {@code LinearLayout} that has run out squeezes its LAST
     * child — so Display and Hide UI were being measured at 33 dp and 35 dp,
     * well under the 44 dp floor INPUT-OWNER-01 requires, on an ordinary phone
     * in portrait. Measured rather than assumed, because the declared size was
     * always correct; it was the arithmetic above it that was not.
     */
    @Test
    public void r1b1_10b_everyIconOnlyControlKeepsTheTouchFloor() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int floor = EditorControlStyles.dimen(activity, R.dimen.icon_button_size);
            // The precision toggle and the capsule's plus joined this row in
            // UI-R4A. Both are icon-only and both live on the smallest window's
            // busiest edges, so they are guarded rather than assumed.
            for (int id : new int[]{R.id.precision_toggle, R.id.objects_capsule_add,
                    R.id.display_settings_button, R.id.hide_ui_toggle}) {
                final View control = workspace.findViewById(id);
                final String name = activity.getResources().getResourceEntryName(id);
                // isShown(), not getVisibility(): a control the window withdrew
                // has no touch target to guard, and it can be withdrawn by its
                // CONTAINER — the capsule's plus is VISIBLE inside a capsule
                // that is GONE exactly when the window docks Objects in a
                // column, because then the column carries its own plus.
                if (!control.isShown()) {
                    continue;
                }
                assertTrue(name + " must be laid out", control.getWidth() > 0);
                assertTrue(name + " is " + control.getWidth() + " px wide, under the "
                                + floor + " px touch floor",
                        control.getWidth() >= floor);
                assertTrue(name + " is " + control.getHeight() + " px tall, under the "
                                + floor + " px touch floor",
                        control.getHeight() >= floor);
            }
            return null;
        });
    }

    /**
     * The same floor, in <b>Sculpt Mode</b>, where the row is at its widest.
     *
     * <p>UI-R3 found the original defect alive in the one mode {@code R1B1-10b}
     * never entered. "Back to Construction" is the longest transition label in
     * the product, and it was an unbounded wrap-content child; a {@code COMPACT}
     * window withdraws the context label that normally absorbs a squeeze, so the
     * row ran past the window edge and the LAST child — Hide UI — was drawn
     * clipped, measured under the floor and partly unreachable.
     *
     * <p>Guarded by measuring the RIGHT EDGE as well as the width: a control can
     * report its full requested width while sitting half outside the window, so
     * width alone would not have caught this.
     */
    @Test
    public void uir3_01_everyIconOnlyControlKeepsTheTouchFloorInSculptToo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Whichever transition this process's state currently offers: the
            // suite shares one process and an earlier case may already have
            // frozen, in which case Freeze is GONE and Resume is the way in.
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            workspace.findViewById(sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0
                    ? R.id.resume_sculpt : R.id.freeze_to_sculpt).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("this case only means something in Sculpt Mode",
                    NativeViewport.productMode() == NativeViewport.MODE_SCULPT);
            final int floor = EditorControlStyles.dimen(activity, R.dimen.icon_button_size);
            final int windowWidth = workspace.getWidth();
            for (int id : new int[]{R.id.precision_toggle, R.id.objects_capsule_add,
                    R.id.display_settings_button, R.id.hide_ui_toggle}) {
                final View control = workspace.findViewById(id);
                final String name = activity.getResources().getResourceEntryName(id);
                // See r1b1_10b: a container can withdraw a control, so the
                // question is whether it is SHOWN and not merely visible.
                if (!control.isShown()) {
                    continue;
                }
                assertTrue(name + " is " + control.getWidth() + " px wide, under the "
                                + floor + " px touch floor", control.getWidth() >= floor);
                final int[] onScreen = new int[2];
                control.getLocationInWindow(onScreen);
                assertTrue(name + " ends at x=" + (onScreen[0] + control.getWidth())
                                + " in a " + windowWidth + " px window, so part of its"
                                + " touch target is off screen",
                        onScreen[0] + control.getWidth() <= windowWidth);
            }
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // R1B1-11 / R1B1-12 -- a tap is a tap, and a scroll is a scroll
    //
    // These two are a pair and only mean something together. Both run against a
    // rail whose container CAN scroll, because that is the situation the defect
    // needed: a scroll container will take a gesture from a control the moment
    // it passes the platform slop, and it was doing so to taps.
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_11_asmallDriftOnARailEntryStillSelectsThatTool() {
        enterSculpt();
        makeTheRailScrollable();
        setTool(NativeViewport.TOOL_SMOOTH);

        final int drift = onWorkspace(rule.getScenario(), (activity, workspace) ->
                touchSlop(activity) / 2);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            dragRailEntry(workspace, R.id.tool_rail_grab, -drift, 3);
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("a few pixels of drift is a tap, and a tap on a tool"
                            + " selects it", NativeViewport.TOOL_GRAB,
                    NativeViewport.sculptTool());
            assertEquals("and it must not have scrolled the rail by doing so",
                    0, workspace.toolRailScroll().getScrollY());
            assertTrue("the selected tool is the one drawn active",
                    workspace.findViewById(R.id.tool_rail_grab).isActivated());
            return null;
        });
    }

    @Test
    public void r1b1_12_arealScrollOnARailEntrySelectsNothing() {
        enterSculpt();
        makeTheRailScrollable();
        setTool(NativeViewport.TOOL_GRAB);

        final int travel = onWorkspace(rule.getScenario(), (activity, workspace) ->
                touchSlop(activity) * 8);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Starts on the CLAY entry and drags well past it. If the entry
            // kept the gesture this would select Clay; if it releases it, the
            // container scrolls and the platform cancels the press.
            dragRailEntry(workspace, R.id.tool_rail_clay, -travel, 10);
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("dragging across the rail must not select the tool it"
                            + " happened to start on", NativeViewport.TOOL_GRAB,
                    NativeViewport.sculptTool());
            assertTrue("and the rail must actually have scrolled, or this case"
                            + " proves nothing about interception",
                    workspace.toolRailScroll().getScrollY() > 0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B1-13 -- none of it cost the model any room
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_13_theViewportKeepsItsFloorInPortraitAndRotated() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        assertViewportFloorHolds("portrait");

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        assertViewportFloorHolds("rotated");
    }

    // -----------------------------------------------------------------------
    // R1B1-14 -- the one animation in the product is untouched
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_14_theDisplayPopoverStillAnchorsOpensAndCloses() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the popover starts closed", View.GONE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View popover = workspace.findViewById(R.id.display_settings_popover);
            assertEquals(View.VISIBLE, popover.getVisibility());
            assertTrue("the button that opened it is drawn active",
                    workspace.findViewById(R.id.display_settings_button).isActivated());

            // It grows from the control that opened it rather than sliding in
            // from a screen edge, which is the whole point of the pattern.
            assertEquals("the panel's pivot is its anchor corner",
                    popover.getWidth(), (int) popover.getPivotX());
            assertEquals(0, (int) popover.getPivotY());

            final ViewGroup.MarginLayoutParams params =
                    (ViewGroup.MarginLayoutParams) popover.getLayoutParams();
            assertEquals("and it hangs under the toolbar's ACTUAL height",
                    workspace.findViewById(R.id.global_toolbar).getHeight(),
                    params.topMargin);

            // Selection feedback happens in place: choosing a chip repaints one
            // chip and leaves the surface exactly where it is.
            workspace.findViewById(R.id.surface_shading_faceted).performClick();
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("a choice must not dismiss the panel", View.VISIBLE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());
            assertTrue(workspace.findViewById(R.id.surface_shading_faceted).isActivated());
            workspace.findViewById(R.id.surface_shading_smooth).performClick();
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("tapping Display again closes it", View.GONE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

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

    private void setTool(final int tool) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setSculptTool(tool);
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
    }

    /**
     * Shrinks the rail's scroll container until its entries overflow it.
     *
     * <p>Otherwise the container has nothing to scroll and the interception
     * these cases are about never happens, so both would pass without testing
     * anything. Restored in {@code @After}.
     */
    private void makeTheRailScrollable() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ScrollView scroll = workspace.toolRailScroll();
            final ViewGroup.LayoutParams params = scroll.getLayoutParams();
            params.height = EditorControlStyles.dimen(activity, R.dimen.rail_item_height) * 3;
            scroll.setLayoutParams(params);
            scroll.scrollTo(0, 0);
            return null;
        });
        settleLayout();
    }

    private static int touchSlop(android.content.Context context) {
        return android.view.ViewConfiguration.get(context).getScaledTouchSlop();
    }

    /**
     * Drags from the centre of a rail entry, THROUGH the scroll container.
     *
     * <p>Dispatching to the container rather than to the entry is the point:
     * the tap-versus-scroll rule is a negotiation between the two, and a
     * gesture delivered straight to the entry would bypass the very thing
     * being tested. The start point is read from the entry's own measured
     * position, never from a screen coordinate written down here.
     */
    private static void dragRailEntry(EditorWorkspaceView workspace, int entryId,
                                      float totalDy, int steps) {
        final ScrollView scroll = workspace.toolRailScroll();
        final View entry = workspace.findViewById(entryId);
        assertNotNull("the entry being dragged must be on screen", entry);

        final int[] scrollLocation = new int[2];
        final int[] entryLocation = new int[2];
        scroll.getLocationInWindow(scrollLocation);
        entry.getLocationInWindow(entryLocation);
        final float x = entryLocation[0] - scrollLocation[0] + entry.getWidth() * 0.5f;
        final float y = entryLocation[1] - scrollLocation[1] + entry.getHeight() * 0.5f;

        final long down = SystemClock.uptimeMillis();
        send(scroll, down, down, MotionEvent.ACTION_DOWN, x, y);
        for (int step = 1; step <= steps; step++) {
            send(scroll, down, down + step * 16L, MotionEvent.ACTION_MOVE,
                    x, y + totalDy * step / steps);
        }
        send(scroll, down, down + (steps + 1) * 16L, MotionEvent.ACTION_UP,
                x, y + totalDy);
    }

    private static void send(View target, long downTime, long eventTime, int action,
                             float x, float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    /**
     * Whether a control's background genuinely draws differently while held.
     *
     * <p>Asked of the drawable rather than of a colour: a state list that
     * resolves to the same drawable pressed and idle would look identical no
     * matter what colours it names, and that is exactly the defect this
     * replaces.
     */
    private static boolean hasPressedFeedback(View view) {
        final Drawable background = view.getBackground();
        if (background == null || !background.isStateful()) {
            return false;
        }
        final int[] restore = view.getDrawableState();
        try {
            background.setState(new int[]{android.R.attr.state_enabled,
                    android.R.attr.state_pressed});
            final Drawable pressed = background.getCurrent();
            background.setState(new int[]{android.R.attr.state_enabled});
            final Drawable idle = background.getCurrent();
            if (pressed == idle) {
                return false;
            }
            assertNotSame(pressed, idle);
            return true;
        } finally {
            background.setState(restore);
        }
    }

    /** Whether a composed control draws an icon rather than spelling one. */
    private static boolean hasIconChild(View root) {
        if (root instanceof ImageView) {
            return ((ImageView) root).getDrawable() != null;
        }
        if (root instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); i++) {
                if (hasIconChild(group.getChildAt(i))) {
                    return true;
                }
            }
        }
        return false;
    }

    private void assertNoPseudoIconsAnywhere(final String where) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final StringBuilder offenders = new StringBuilder();
            collectPseudoIcons(workspace, offenders);
            assertEquals("no control may be a font glyph " + where + ":" + offenders,
                    0, offenders.length());
            return null;
        });
    }

    private static void collectPseudoIcons(View root, StringBuilder offenders) {
        if (root.getVisibility() != View.VISIBLE) {
            return;
        }
        if (root instanceof TextView) {
            final CharSequence text = ((TextView) root).getText();
            if (text != null) {
                for (String glyph : RETIRED_PSEUDO_ICONS) {
                    if (text.toString().contains(glyph)) {
                        offenders.append(" \"").append(glyph).append("\" in \"")
                                .append(text).append('"');
                    }
                }
            }
        }
        if (root instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); i++) {
                collectPseudoIcons(group.getChildAt(i), offenders);
            }
        }
    }

    private void assertViewportFloorHolds(final String where) {
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Rect[] chrome = workspace.chromeRects();
            final double unoccluded = unoccludedViewportFraction(workspace.getWidth(),
                    workspace.getHeight(), chrome);
            assertTrue("the model must never be fully covered (" + where + ")",
                    unoccluded > 0.0);

            final boolean inspectorOpen = workspace.propertyInspector().isOpen();
            final double floor = workspace.layoutMode() == WorkspaceLayoutMode.EXPANDED
                    ? EXPANDED_FLOOR
                    : (inspectorOpen ? COMPACT_FLOOR_INSPECTOR_OPEN : COMPACT_FLOOR);

            // Logged as well as asserted: these numbers ARE the evidence that
            // the new depth, radius and icon work cost the viewport nothing,
            // and a passing assertion does not record them.
            android.util.Log.i("ForgeShape", String.format(java.util.Locale.US,
                    "FORGESHAPE_R1B1_VIEWPORT %s window=%dx%d mode=%s placement=%s "
                            + "inspector=%s unoccluded=%.1f%% floor=%.0f%%",
                    where, workspace.getWidth(), workspace.getHeight(),
                    workspace.layoutMode(), workspace.inspectorPlacement(),
                    inspectorOpen ? "open" : "collapsed", unoccluded * 100.0, floor * 100.0));

            assertTrue("unoccluded viewport " + String.format(java.util.Locale.US, "%.1f%%",
                            unoccluded * 100.0) + " (" + where + ", " + workspace.layoutMode()
                            + ", inspector " + (inspectorOpen ? "open" : "collapsed")
                            + ") is below the floor " + floor,
                    unoccluded >= floor);
            return null;
        });
    }
}
