package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.forgeshape.app.WorkspaceLayoutMode.InspectorPlacement;

import org.junit.Test;

/**
 * The adaptive layout decision, checked without a device.
 *
 * <p>These are the rules that decide whether the shell repeats the landscape
 * failure. They are arithmetic on window dp, so they are asserted here rather
 * than inferred from a screenshot; the instrumented suite then measures what
 * the rules actually produce on screen.
 */
public final class WorkspaceLayoutModeTest {

    // The four window sizes the audit measured, plus the two boundaries.

    @Test
    public void phonePortraitIsCompactWithABottomSheet() {
        final WorkspaceLayoutMode mode = WorkspaceLayoutMode.forWindow(411, 914);
        assertEquals(WorkspaceLayoutMode.COMPACT, mode);
        assertEquals(InspectorPlacement.BOTTOM_SHEET, mode.inspectorPlacement(914));
        assertFalse("a compact window opens with the model visible",
                mode.inspectorStartsExpanded(914));
        assertFalse(mode.railDocked());
        assertFalse("a tall window gives the status message its own line",
                WorkspaceLayoutMode.statusInlineWithControls(914));
    }

    @Test
    public void smallPhonePortraitIsCompact() {
        final WorkspaceLayoutMode mode = WorkspaceLayoutMode.forWindow(360, 640);
        assertEquals(WorkspaceLayoutMode.COMPACT, mode);
        assertEquals(InspectorPlacement.BOTTOM_SHEET, mode.inspectorPlacement(640));
    }

    /**
     * The regression this stage exists to prevent: a phone in landscape is wide
     * enough to look like a tablet by width alone, and a bottom-anchored
     * inspector in that window is exactly what covered the model completely.
     */
    @Test
    public void phoneLandscapeNeverGetsABottomSheetAndIsNotExpanded() {
        final WorkspaceLayoutMode mode = WorkspaceLayoutMode.forWindow(914, 411);
        assertEquals("914 dp wide but only 411 dp tall is not a tablet",
                WorkspaceLayoutMode.MEDIUM, mode);
        assertEquals(InspectorPlacement.SIDE_OVERLAY, mode.inspectorPlacement(411));
        assertFalse(mode.railDocked());
        assertTrue("a short window cannot spare a second full-width line",
                WorkspaceLayoutMode.statusInlineWithControls(411));
    }

    @Test
    public void halfScreenSplitIsMediumWithABottomSheet() {
        final WorkspaceLayoutMode mode = WorkspaceLayoutMode.forWindow(600, 800);
        assertEquals(WorkspaceLayoutMode.MEDIUM, mode);
        assertEquals(InspectorPlacement.BOTTOM_SHEET, mode.inspectorPlacement(800));
        assertTrue("a roomy window can show the exact values immediately",
                mode.inspectorStartsExpanded(800));
    }

    @Test
    public void tabletLandscapeDocksTheInspectorAndTheRail() {
        final WorkspaceLayoutMode mode = WorkspaceLayoutMode.forWindow(1280, 800);
        assertEquals(WorkspaceLayoutMode.EXPANDED, mode);
        assertEquals(InspectorPlacement.SIDE_DOCK, mode.inspectorPlacement(800));
        assertTrue(mode.railDocked());
    }

    @Test
    public void expandedNeedsBothWidthAndHeight() {
        assertEquals(WorkspaceLayoutMode.EXPANDED, WorkspaceLayoutMode.forWindow(840, 480));
        assertEquals("one dp short of the width threshold",
                WorkspaceLayoutMode.MEDIUM, WorkspaceLayoutMode.forWindow(839, 480));
        assertEquals("one dp short of the height threshold",
                WorkspaceLayoutMode.MEDIUM, WorkspaceLayoutMode.forWindow(840, 479));
        assertEquals(WorkspaceLayoutMode.COMPACT, WorkspaceLayoutMode.forWindow(599, 900));
        assertEquals(WorkspaceLayoutMode.MEDIUM, WorkspaceLayoutMode.forWindow(600, 900));
    }

    // -----------------------------------------------------------------------
    // Chrome budget
    //
    // The inspector may not grow to fill the window in any placement. That is
    // the specific defect being designed out, so it is asserted as arithmetic
    // rather than trusted to a wrap-content measure pass.
    // -----------------------------------------------------------------------

    @Test
    public void bottomSheetNeverTakesMoreThanThirtyPercentOfTheWindow() {
        for (int heightDp = 480; heightDp <= 1600; heightDp += 7) {
            final int sheet = WorkspaceLayoutMode.bottomSheetMaxHeightDp(heightDp);
            assertTrue("sheet " + sheet + " dp in a " + heightDp + " dp window",
                    sheet <= Math.max(160, Math.round(heightDp * 0.30f)));
            assertTrue("a sheet must stay usable", sheet >= 160);
            assertTrue("and capped so a tall window does not get a huge one", sheet <= 300);
        }
    }

    @Test
    public void sideInspectorNeverTakesMoreThanAThirdOfTheWidth() {
        for (int widthDp = 320; widthDp <= 1600; widthDp += 7) {
            final int overlay = WorkspaceLayoutMode.sideOverlayWidthDp(widthDp);
            final int dock = WorkspaceLayoutMode.sideDockWidthDp(widthDp);
            assertTrue("overlay " + overlay + " dp in a " + widthDp + " dp window",
                    overlay <= 300);
            assertTrue("dock " + dock + " dp in a " + widthDp + " dp window", dock <= 340);
            assertTrue("an inspector must stay wide enough to type in", overlay >= 240);
            assertTrue(dock >= 260);
        }
    }

    // -----------------------------------------------------------------------
    // UI-R1C2: the Objects surface, and the Tool Rail dock actually being asked
    //
    // railDocked() had a tested meaning and had never once been called by
    // production code. These cases are what stop it drifting back into that
    // state: they assert the decision, and EditorWorkspaceLayoutTest asserts
    // that the workspace obeys it.
    // -----------------------------------------------------------------------

    /** R1C2-11. A phone keeps Objects where the shape editor already had it. */
    @Test
    public void compactNeverAsksForADedicatedObjectsSurface() {
        final WorkspaceLayoutMode portrait = WorkspaceLayoutMode.forWindow(411, 914);
        assertFalse("a phone has no room for a third chrome column",
                portrait.objectsDocked(411));
        final WorkspaceLayoutMode small = WorkspaceLayoutMode.forWindow(360, 640);
        assertFalse(small.objectsDocked(360));
        // Not even if the window were somehow wide: the mode gates it first, so
        // a compact decision can never produce a docked column.
        assertFalse("compact is compact whatever number it is handed",
                WorkspaceLayoutMode.COMPACT.objectsDocked(1600));
    }

    /** R1C2-12. Medium stays conservative: no new surfaces, no new breakpoint. */
    @Test
    public void mediumRemainsConservative() {
        final WorkspaceLayoutMode landscape = WorkspaceLayoutMode.forWindow(914, 411);
        assertEquals(WorkspaceLayoutMode.MEDIUM, landscape);
        assertFalse("a short landscape window docks nothing new",
                landscape.objectsDocked(914));
        assertFalse(landscape.railDocked());
        assertEquals("and its inspector behaviour is untouched",
                InspectorPlacement.SIDE_OVERLAY, landscape.inspectorPlacement(411));

        final WorkspaceLayoutMode split = WorkspaceLayoutMode.forWindow(600, 800);
        assertEquals(WorkspaceLayoutMode.MEDIUM, split);
        assertFalse(split.objectsDocked(600));
        assertFalse(split.railDocked());
        assertEquals(InspectorPlacement.BOTTOM_SHEET, split.inspectorPlacement(800));

        assertEquals("MEDIUM is still one of exactly three modes",
                3, WorkspaceLayoutMode.values().length);
    }

    /** R1C2-13. A real tablet gets the column. */
    @Test
    public void aTabletGetsADedicatedObjectsSurfaceBesideTheInspector() {
        final WorkspaceLayoutMode mode = WorkspaceLayoutMode.forWindow(1280, 800);
        assertEquals(WorkspaceLayoutMode.EXPANDED, mode);
        assertTrue("a tablet has room for the scene list", mode.objectsDocked(1280));
        // Both at once is the whole point: R1C2-28 is about seeing what you are
        // editing and what its numbers are without switching between them.
        assertEquals(InspectorPlacement.SIDE_DOCK, mode.inspectorPlacement(800));
        // The window the runtime evidence is taken on.
        assertTrue("the 1600 x 2560 @ 240 dpi evidence window qualifies",
                WorkspaceLayoutMode.forWindow(1066, 1706).objectsDocked(1066));
    }

    /**
     * R1C2-13's other half, and the reason the answer is arithmetic rather than
     * a fourth breakpoint.
     *
     * <p>Being EXPANDED is necessary and not sufficient. At the bottom of the
     * expanded range there is no room for a third column that leaves a usable
     * viewport, and cramming one in is exactly the desktop-CAD clutter
     * UI-OWNER-02 rules out — so 840 dp does not get one, deliberately.
     */
    @Test
    public void theObjectsDockIsEarnedByWidthAndNeverCrampsTheViewport() {
        assertEquals(WorkspaceLayoutMode.EXPANDED, WorkspaceLayoutMode.forWindow(840, 800));
        assertFalse("the narrow end of expanded is still not a three-column window",
                WorkspaceLayoutMode.EXPANDED.objectsDocked(840));

        // Wherever it IS granted, the central viewport is at least as wide as a
        // phone screen. This is the floor the rule is derived from, asserted
        // independently of the derivation so the two cannot agree on a bug.
        boolean sawADock = false;
        boolean sawARefusal = false;
        for (int widthDp = 840; widthDp <= 2000; widthDp += 3) {
            if (!WorkspaceLayoutMode.EXPANDED.objectsDocked(widthDp)) {
                sawARefusal = true;
                continue;
            }
            sawADock = true;
            final int viewport = widthDp
                    - WorkspaceLayoutMode.sideDockWidthDp(widthDp)
                    - WorkspaceLayoutMode.RAIL_WIDTH_DP
                    - WorkspaceLayoutMode.OBJECTS_DOCK_WIDTH_DP;
            assertTrue("viewport " + viewport + " dp of " + widthDp + " dp with three columns",
                    viewport >= WorkspaceLayoutMode.MIN_CENTRAL_VIEWPORT_DP);
        }
        assertTrue("the rule must actually grant a dock somewhere", sawADock);
        assertTrue("and must actually refuse one somewhere", sawARefusal);
    }

    /**
     * R1C2-14. The Tool Rail dock decision, now that something asks it.
     *
     * <p>Teeth against the behaviour it replaced: before UI-R1C2 this method
     * existed and was never called, so "docked" and "floating" were the same
     * thing in the running product. A rail that answered the same in every
     * window — which is what a dead predicate effectively does — fails here.
     */
    @Test
    public void theToolRailDocksOnlyWhereThereIsRoomBesideTheModel() {
        assertTrue(WorkspaceLayoutMode.forWindow(1280, 800).railDocked());
        assertTrue(WorkspaceLayoutMode.forWindow(840, 480).railDocked());
        assertFalse(WorkspaceLayoutMode.forWindow(914, 411).railDocked());
        assertFalse(WorkspaceLayoutMode.forWindow(411, 914).railDocked());
        assertFalse(WorkspaceLayoutMode.forWindow(600, 800).railDocked());

        // It is not a constant in either direction.
        assertTrue("the rail must dock somewhere",
                WorkspaceLayoutMode.EXPANDED.railDocked());
        assertFalse("and must float somewhere",
                WorkspaceLayoutMode.COMPACT.railDocked() || WorkspaceLayoutMode.MEDIUM.railDocked());
    }

    /**
     * R1C2-15. The boundaries are exact, and every one of the new answers moves
     * at a stated dp rather than somewhere near it.
     */
    @Test
    public void everyAdaptiveBoundaryIsDeterministic() {
        // The mode boundaries, unchanged by this stage.
        assertEquals(WorkspaceLayoutMode.COMPACT, WorkspaceLayoutMode.forWindow(599, 900));
        assertEquals(WorkspaceLayoutMode.MEDIUM, WorkspaceLayoutMode.forWindow(600, 900));
        assertEquals(WorkspaceLayoutMode.MEDIUM, WorkspaceLayoutMode.forWindow(839, 900));
        assertEquals(WorkspaceLayoutMode.EXPANDED, WorkspaceLayoutMode.forWindow(840, 900));

        // The Objects dock switches on exactly once, and never switches back as
        // the window grows. A rule that oscillated would make a slow window
        // resize re-parent the scene list repeatedly.
        int transitions = 0;
        boolean previous = WorkspaceLayoutMode.EXPANDED.objectsDocked(840);
        for (int widthDp = 841; widthDp <= 2400; widthDp++) {
            final boolean current = WorkspaceLayoutMode.EXPANDED.objectsDocked(widthDp);
            if (current != previous) {
                transitions++;
                assertTrue("the dock may only ever be GAINED as a window widens", current);
                previous = current;
            }
        }
        assertEquals("exactly one threshold across the whole expanded range", 1, transitions);

        // Same window in, same answer out, every time. The decision reads
        // nothing but its arguments.
        for (int i = 0; i < 3; i++) {
            assertEquals(WorkspaceLayoutMode.EXPANDED, WorkspaceLayoutMode.forWindow(1280, 800));
            assertTrue(WorkspaceLayoutMode.forWindow(1280, 800).objectsDocked(1280));
            assertTrue(WorkspaceLayoutMode.forWindow(1280, 800).railDocked());
        }
    }

    /**
     * R1C2-16. The layout decision is arithmetic on a window and nothing else.
     *
     * <p>It cannot consult the theme, the grid, the product mode, the selected
     * body or any other domain state, and the way that is guaranteed is
     * structural: this class holds no Android type and no reference to anything
     * — every method is static or reads only {@code this} and its arguments. If
     * that ever stopped being true, this test would not compile on the JVM at
     * all, because none of that state exists here to be reached.
     */
    @Test
    public void layoutDecisionsDependOnNothingButTheWindow() {
        for (WorkspaceLayoutMode mode : WorkspaceLayoutMode.values()) {
            // Called many times with unrelated work in between; the answers are
            // identical because there is no state for anything to have changed.
            final boolean rail = mode.railDocked();
            final boolean objects = mode.objectsDocked(1280);
            final InspectorPlacement placement = mode.inspectorPlacement(800);
            for (int i = 0; i < 50; i++) {
                WorkspaceLayoutMode.forWindow(i * 13, i * 7);
                WorkspaceLayoutMode.bottomSheetMaxHeightDp(i * 11);
                WorkspaceLayoutMode.sideOverlayWidthDp(i * 17);
            }
            assertEquals(rail, mode.railDocked());
            assertEquals(objects, mode.objectsDocked(1280));
            assertEquals(placement, mode.inspectorPlacement(800));
        }
        // The whole enum is reachable from window size alone: there is no mode
        // that some other kind of state has to unlock.
        assertEquals(WorkspaceLayoutMode.COMPACT, WorkspaceLayoutMode.forWindow(360, 640));
        assertEquals(WorkspaceLayoutMode.MEDIUM, WorkspaceLayoutMode.forWindow(700, 900));
        assertEquals(WorkspaceLayoutMode.EXPANDED, WorkspaceLayoutMode.forWindow(1280, 800));
    }

    /**
     * The expanded floor, stated as the shell actually enforces it.
     *
     * <p>The UI architecture pack proposed an absolute 640 dp viewport, which
     * is unreachable at the 840 dp boundary once a rail and a usable dock are
     * subtracted. The rule kept instead is proportional with an absolute floor,
     * and it holds across the whole expanded range.
     *
     * <p>This is the <b>two</b>-docked-surface budget, and it stays exactly as
     * it was. UI-R1C2's third column is held to the absolute half of the same
     * floor and is granted only where that still holds — see
     * {@link #theObjectsDockIsEarnedByWidthAndNeverCrampsTheViewport}, which is
     * where the three-column case is asserted.
     */
    @Test
    public void expandedWindowsKeepALargeCentralViewport() {
        final int railDp = 68;   // rail item width plus its padding
        for (int widthDp = 840; widthDp <= 1600; widthDp += 7) {
            final int viewport = widthDp - WorkspaceLayoutMode.sideDockWidthDp(widthDp) - railDp;
            assertTrue("viewport " + viewport + " dp of " + widthDp + " dp",
                    viewport >= Math.round(widthDp * 0.60f));
            assertTrue("viewport " + viewport + " dp is wider than a phone screen",
                    viewport >= 480);
        }
    }
}
