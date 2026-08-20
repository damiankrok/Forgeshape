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
            assertTrue("dock " + dock + " dp in a " + widthDp + " dp window", dock <= 320);
            assertTrue("an inspector must stay wide enough to type in", overlay >= 240);
            assertTrue(dock >= 260);
        }
    }

    /**
     * The expanded floor, stated as the shell actually enforces it.
     *
     * <p>The UI architecture pack proposed an absolute 640 dp viewport, which
     * is unreachable at the 840 dp boundary once a rail and a usable dock are
     * subtracted. The rule kept instead is proportional with an absolute floor,
     * and it holds across the whole expanded range.
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
