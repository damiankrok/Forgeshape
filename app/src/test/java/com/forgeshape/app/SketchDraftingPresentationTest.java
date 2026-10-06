package com.forgeshape.app;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.math.BigDecimal;
import java.util.HashSet;
import java.util.Set;

import org.junit.Test;

/**
 * The sketch drafting chrome's presentation rules, on the JVM
 * (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`).
 *
 * <p>The palette draws exactly what can succeed for the selection; labels
 * write R / Ø / ° and set a Reference apart by parentheses as well as by
 * emphasis; Driving is offered only for the six kinds native lets drive;
 * overlapping labels are hidden by a deterministic priority, never moved;
 * every label keeps the 48 dp touch floor; each snap kind has its own spoken
 * name; and the exact-value fields read safe arithmetic and nothing else. A
 * device proves the views obey these rules; this proves the rules.
 */
public final class SketchDraftingPresentationTest {

    private static final int EDITING = NativeViewport.SKETCH_EDITING;
    private static final int READY = NativeViewport.SKETCH_READY;

    // -----------------------------------------------------------------------
    // The palette
    // -----------------------------------------------------------------------

    @Test
    public void theModifyEntryIsADrawingControlAndAbsentInReady() {
        assertTrue(SketchDraftingPresentation.modifyEntryShown(EDITING));
        assertFalse("Ready withdraws the drawing chrome",
                SketchDraftingPresentation.modifyEntryShown(READY));
        final boolean[] none = SketchDraftingPresentation.paletteActions(READY, 1,
                NativeViewport.SKETCH_ENTITY_KIND_LINE);
        for (boolean shown : none) {
            assertFalse("no action is drawn outside a drawn sketch", shown);
        }
    }

    @Test
    public void withNothingSelectedThePaletteOffersTheTapModes() {
        final boolean[] shown = SketchDraftingPresentation.paletteActions(EDITING, 0, -1);
        assertTrue(shown[SketchDraftingPresentation.ACTION_SELECT_MULTIPLE]);
        assertTrue(shown[SketchDraftingPresentation.ACTION_DIMENSION]);
        assertTrue(shown[SketchDraftingPresentation.ACTION_TRIM]);
        assertTrue(shown[SketchDraftingPresentation.ACTION_EXTEND]);
        assertTrue(shown[SketchDraftingPresentation.ACTION_OFFSET]);
        assertFalse("nothing to make Construction", shown[SketchDraftingPresentation.ACTION_CONSTRUCTION]);
        assertFalse("nothing to mirror", shown[SketchDraftingPresentation.ACTION_MIRROR]);
        assertFalse("nothing to delete", shown[SketchDraftingPresentation.ACTION_DELETE]);
    }

    @Test
    public void oneEntityOffersWhatItsKindSupports() {
        for (int kind = NativeViewport.SKETCH_ENTITY_KIND_LINE;
             kind <= NativeViewport.SKETCH_ENTITY_KIND_ARC; kind++) {
            final boolean[] shown = SketchDraftingPresentation.paletteActions(EDITING, 1, kind);
            assertTrue("kind " + kind, shown[SketchDraftingPresentation.ACTION_DIMENSION]);
            assertTrue("kind " + kind, shown[SketchDraftingPresentation.ACTION_OFFSET]);
            assertTrue(shown[SketchDraftingPresentation.ACTION_CONSTRUCTION]);
            assertTrue(shown[SketchDraftingPresentation.ACTION_MIRROR]);
            assertTrue(shown[SketchDraftingPresentation.ACTION_DELETE]);
            assertTrue("Trim acts on what the finger touches, so a selection never withdraws it",
                    shown[SketchDraftingPresentation.ACTION_TRIM]);
            assertTrue(shown[SketchDraftingPresentation.ACTION_EXTEND]);
        }
        final boolean[] spline = SketchDraftingPresentation.paletteActions(EDITING, 1,
                NativeViewport.SKETCH_ENTITY_KIND_SPLINE);
        assertFalse("a spline has no dimension", spline[SketchDraftingPresentation.ACTION_DIMENSION]);
        assertFalse("a spline cannot be offset", spline[SketchDraftingPresentation.ACTION_OFFSET]);
        assertTrue(spline[SketchDraftingPresentation.ACTION_CONSTRUCTION]);
        assertTrue(spline[SketchDraftingPresentation.ACTION_MIRROR]);
    }

    @Test
    public void severalEntitiesOfferActsOnTheWholeSelectionAndTheTapModes() {
        final boolean[] shown = SketchDraftingPresentation.paletteActions(EDITING, 3, -1);
        assertTrue(shown[SketchDraftingPresentation.ACTION_SELECT_MULTIPLE]);
        assertTrue(shown[SketchDraftingPresentation.ACTION_CONSTRUCTION]);
        assertTrue(shown[SketchDraftingPresentation.ACTION_MIRROR]);
        assertTrue(shown[SketchDraftingPresentation.ACTION_DELETE]);
        assertFalse(shown[SketchDraftingPresentation.ACTION_DIMENSION]);
        assertFalse(shown[SketchDraftingPresentation.ACTION_OFFSET]);
        assertTrue("the tap modes do not depend on the selection",
                shown[SketchDraftingPresentation.ACTION_TRIM]);
        assertTrue(shown[SketchDraftingPresentation.ACTION_EXTEND]);
    }

    @Test
    public void thePaletteHoldsModifyActsAndNeverTheDrawingTools() {
        // The palette is the ONE home of the modify acts; the Tool Rail keeps
        // the seven drawing tools and nothing here duplicates one, so there is
        // no second (bottom) toolbar.
        assertEquals(8, SketchDraftingPresentation.ACTION_COUNT);
        final Set<Integer> union = new HashSet<>();
        for (int selection = 0; selection <= 3; selection++) {
            for (int kind = -1; kind <= NativeViewport.SKETCH_ENTITY_KIND_SPLINE; kind++) {
                final boolean[] shown = SketchDraftingPresentation.paletteActions(EDITING, selection, kind);
                assertEquals(SketchDraftingPresentation.ACTION_COUNT, shown.length);
                for (int a = 0; a < shown.length; a++) {
                    if (shown[a]) union.add(a);
                }
                assertTrue("Select multiple is always reachable",
                        shown[SketchDraftingPresentation.ACTION_SELECT_MULTIPLE]);
            }
        }
        assertEquals("every act is reachable for some selection", 8, union.size());
    }

    @Test
    public void theConstructionLabelNamesWhatTheToggleWillDo() {
        assertEquals(R.string.sketch_make_construction,
                SketchDraftingPresentation.constructionLabel(true));
        assertEquals(R.string.sketch_make_regular,
                SketchDraftingPresentation.constructionLabel(false));
    }

    // -----------------------------------------------------------------------
    // Dimension labels
    // -----------------------------------------------------------------------

    @Test
    public void labelsWriteRadiusDiameterAndDegreesMarks() {
        final LengthUnit mm = LengthUnit.MILLIMETERS;
        assertEquals("R 12.5 mm", SketchDraftingPresentation.label(
                NativeViewport.DIM_CIRCLE_RADIUS, false, 0.0125, mm));
        assertEquals("R 12.5 mm", SketchDraftingPresentation.label(
                NativeViewport.DIM_ARC_RADIUS, false, 0.0125, mm));
        assertEquals("Ø 25 mm", SketchDraftingPresentation.label(
                NativeViewport.DIM_CIRCLE_DIAMETER, false, 0.025, mm));
        assertEquals("45°", SketchDraftingPresentation.label(
                NativeViewport.DIM_LINE_ANGLE, false, 45.0, mm));
        assertEquals("90°", SketchDraftingPresentation.label(
                NativeViewport.DIM_EDGE_ANGLE, true, 90.0, mm).replace("(", "").replace(")", ""));
        assertEquals("120.5°", SketchDraftingPresentation.label(
                NativeViewport.DIM_ARC_SWEEP, false, 120.5, mm));
        assertEquals("40 mm", SketchDraftingPresentation.label(
                NativeViewport.DIM_LINE_LENGTH, false, 0.04, mm));
        assertEquals("an angle never takes a length unit", "33.333°",
                SketchDraftingPresentation.label(NativeViewport.DIM_LINE_ANGLE, false,
                        33.33333333, LengthUnit.METERS));
    }

    @Test
    public void aReferenceReadsApartFromADrivingDimensionByMoreThanColour() {
        final LengthUnit mm = LengthUnit.MILLIMETERS;
        final String driving = SketchDraftingPresentation.label(
                NativeViewport.DIM_LINE_LENGTH, false, 0.04, mm);
        final String reference = SketchDraftingPresentation.label(
                NativeViewport.DIM_LINE_LENGTH, true, 0.04, mm);
        assertEquals("(40 mm)", reference);
        assertNotEquals(driving, reference);
        assertEquals("(R 5 mm)", SketchDraftingPresentation.label(
                NativeViewport.DIM_ARC_RADIUS, true, 0.005, mm));
    }

    @Test
    public void drivingIsOfferedForExactlyTheSixDrivingKinds() {
        final Set<Integer> driving = new HashSet<>();
        for (int kind = 0; kind < NativeViewport.DIM_KIND_COUNT; kind++) {
            if (SketchDraftingPresentation.drivingAllowed(kind)) driving.add(kind);
        }
        final Set<Integer> expected = new HashSet<>();
        expected.add(NativeViewport.DIM_LINE_LENGTH);
        expected.add(NativeViewport.DIM_LINE_ANGLE);
        expected.add(NativeViewport.DIM_RECTANGLE_WIDTH);
        expected.add(NativeViewport.DIM_RECTANGLE_HEIGHT);
        expected.add(NativeViewport.DIM_CIRCLE_RADIUS);
        expected.add(NativeViewport.DIM_CIRCLE_DIAMETER);
        assertEquals(expected, driving);
        assertFalse("a horizontal projection is measured, never typed",
                SketchDraftingPresentation.drivingAllowed(NativeViewport.DIM_LINE_HORIZONTAL));
        assertFalse(SketchDraftingPresentation.drivingAllowed(NativeViewport.DIM_ARC_SWEEP));
        assertFalse(SketchDraftingPresentation.drivingAllowed(NativeViewport.DIM_EDGE_ANGLE));
    }

    @Test
    public void everyKindHasAChipName() {
        for (int kind = 0; kind < NativeViewport.DIM_KIND_COUNT; kind++) {
            assertNotEquals(0, SketchDraftingPresentation.kindName(kind));
        }
        assertEquals(12, NativeViewport.DIM_KIND_COUNT);
    }

    @Test
    public void overlappingLabelsAreHiddenByPriorityAndNeverMoved() {
        final float[] x = {100f, 110f, 400f};
        final float[] y = {100f, 105f, 100f};
        final float[] w = {96f, 96f, 96f};
        final float[] h = {48f, 48f, 48f};
        // The selection's own label wins over an older driving one beside it.
        final int[] priority = {
                SketchDraftingPresentation.priority(false, true, 1),
                SketchDraftingPresentation.priority(true, false, 7),
                SketchDraftingPresentation.priority(false, false, 3)};
        assertArrayEquals(new boolean[] {false, true, true},
                SketchDraftingPresentation.resolveVisible(x, y, w, h, priority));
        // Driving over Reference, then the older id.
        assertTrue(SketchDraftingPresentation.priority(false, true, 9)
                > SketchDraftingPresentation.priority(false, false, 1));
        assertTrue(SketchDraftingPresentation.priority(false, true, 2)
                > SketchDraftingPresentation.priority(false, true, 3));
        // Deterministic: the same inputs give the same answer, and an exact tie
        // keeps the earlier index.
        final int[] tie = {5, 5, 5};
        assertArrayEquals(new boolean[] {true, false, true},
                SketchDraftingPresentation.resolveVisible(x, y, w, h, tie));
        assertArrayEquals(SketchDraftingPresentation.resolveVisible(x, y, w, h, tie),
                SketchDraftingPresentation.resolveVisible(x, y, w, h, tie));
    }

    @Test
    public void labelsThatDoNotOverlapAllStand() {
        final int n = 20;
        final float[] x = new float[n];
        final float[] y = new float[n];
        final float[] w = new float[n];
        final float[] h = new float[n];
        final int[] p = new int[n];
        for (int i = 0; i < n; i++) {
            x[i] = 60f + 120f * (i % 5);
            y[i] = 60f + 70f * (i / 5);
            w[i] = 96f;
            h[i] = 48f;
            p[i] = SketchDraftingPresentation.priority(false, i % 2 == 0, i + 1);
        }
        for (boolean visible : SketchDraftingPresentation.resolveVisible(x, y, w, h, p)) {
            assertTrue(visible);
        }
    }

    @Test
    public void aLabelStandsOffTheStrokeItMeasuresByItsWholeBox() {
        final float clear = 8f * 2.625f;
        final float w = 210f;  // an 80 dp chip at xxhdpi
        final float h = 126f;  // the 48 dp floor
        // Off a VERTICAL edge (attach on the edge, anchor 41 dp to its right):
        // the half WIDTH reaches back, so the box is pushed until its left side
        // clears the edge by the stated gap.
        final float[] vertical = SketchDraftingPresentation.standOffCentre(
                500f + 107.6f, 1000f, 500f, 1000f, w, h, clear);
        assertEquals(1000f, vertical[1], 1e-3f);
        assertEquals("the box's near side is the clearance off the edge",
                500f + clear, vertical[0] - 0.5f * w, 1e-3f);
        // Off a HORIZONTAL edge the half HEIGHT reaches back, already clear at
        // native's 41 dp: the anchor is kept, never pulled in.
        final float[] horizontal = SketchDraftingPresentation.standOffCentre(
                500f, 1000f - 107.6f, 500f, 1000f, w, h, clear);
        assertEquals(500f, horizontal[0], 1e-3f);
        assertEquals(1000f - 107.6f, horizontal[1], 1e-3f);
        // Diagonal: the box's corner support along the ray clears the attach.
        final float d = 107.6f / (float) Math.sqrt(2.0);
        final float[] diagonal = SketchDraftingPresentation.standOffCentre(
                500f + d, 1000f - d, 500f, 1000f, w, h, clear);
        final float ux = (float) Math.sqrt(0.5);
        final float along = (diagonal[0] - 500f) * ux - (diagonal[1] - 1000f) * ux;
        assertEquals("pushed only along the ray", diagonal[0] - 500f, -(diagonal[1] - 1000f), 1e-3f);
        assertEquals(0.5f * (ux * w + ux * h) + clear, along, 1e-2f);
        // No honest direction: the anchor itself.
        assertArrayEquals(new float[] {40f, 50f},
                SketchDraftingPresentation.standOffCentre(40f, 50f, 40f, 50f, w, h, clear), 0f);
    }

    @Test
    public void aLabelThatWouldLeaveTheViewportIsHiddenNotClamped() {
        assertTrue(SketchDraftingPresentation.boxInside(200f, 200f, 210f, 126f, 1080, 2400));
        assertFalse("the left edge", SketchDraftingPresentation.boxInside(100f, 200f, 210f, 126f, 1080, 2400));
        assertFalse("the top edge", SketchDraftingPresentation.boxInside(200f, 60f, 210f, 126f, 1080, 2400));
        assertFalse("the right edge", SketchDraftingPresentation.boxInside(1000f, 200f, 210f, 126f, 1080, 2400));
        assertFalse("the bottom edge", SketchDraftingPresentation.boxInside(200f, 2350f, 210f, 126f, 1080, 2400));
        assertTrue("exactly touching fits", SketchDraftingPresentation.boxInside(105f, 63f, 210f, 126f, 1080, 2400));
        assertFalse("no viewport yet", SketchDraftingPresentation.boxInside(200f, 200f, 210f, 126f, 0, 0));
    }

    @Test
    public void everyLabelKeepsTheFortyEightDpTouchFloor() {
        final int floor = 48 * 3;  // 48 dp at xxhdpi
        for (int measured = 0; measured <= 400; measured += 7) {
            final int extent = SketchDraftingPresentation.touchExtent(measured, floor);
            assertTrue(extent >= floor);
            assertTrue("never larger than it has to be",
                    extent == Math.max(measured, floor));
        }
    }

    // -----------------------------------------------------------------------
    // Snap feedback
    // -----------------------------------------------------------------------

    @Test
    public void eachSnapKindHasItsOwnName() {
        final int[] kinds = {NativeViewport.SNAP_ENDPOINT, NativeViewport.SNAP_INTERSECTION,
                NativeViewport.SNAP_MIDPOINT, NativeViewport.SNAP_CENTER, NativeViewport.SNAP_ORIGIN,
                NativeViewport.SNAP_HORIZONTAL_GUIDE, NativeViewport.SNAP_VERTICAL_GUIDE,
                NativeViewport.SNAP_GRID};
        final Set<Integer> names = new HashSet<>();
        for (int kind : kinds) {
            final int name = SketchDraftingPresentation.snapDescription(kind);
            assertNotEquals("kind " + kind, 0, name);
            names.add(name);
        }
        assertEquals("no two snap kinds share a name", kinds.length, names.size());
        assertEquals("no snap, nothing to say", 0,
                SketchDraftingPresentation.snapDescription(NativeViewport.SNAP_NONE));
    }

    @Test
    public void theSnapCodesMatchTheNativePriorityOrder() {
        // Endpoint > Intersection > Midpoint > Center > Origin > guides > Grid is
        // the native order; the codes are the enum's and must not drift.
        assertEquals(0, NativeViewport.SNAP_NONE);
        assertEquals(1, NativeViewport.SNAP_GRID);
        assertEquals(2, NativeViewport.SNAP_ENDPOINT);
        assertEquals(3, NativeViewport.SNAP_INTERSECTION);
        assertEquals(4, NativeViewport.SNAP_MIDPOINT);
        assertEquals(5, NativeViewport.SNAP_CENTER);
        assertEquals(6, NativeViewport.SNAP_ORIGIN);
        assertEquals(7, NativeViewport.SNAP_HORIZONTAL_GUIDE);
        assertEquals(8, NativeViewport.SNAP_VERTICAL_GUIDE);
    }

    // -----------------------------------------------------------------------
    // Exact input
    // -----------------------------------------------------------------------

    @Test
    public void exactFieldsReadSafeArithmeticExactly() {
        assertEquals(0, new BigDecimal("15").compareTo(ExactExpression.parse("12+3")));
        assertEquals(0, new BigDecimal("25").compareTo(ExactExpression.parse("50/2")));
        assertEquals(0, new BigDecimal("15").compareTo(ExactExpression.parse("2*7.5")));
        assertEquals(0, new BigDecimal("12").compareTo(ExactExpression.parse("(10-4)*2")));
        assertEquals(0, new BigDecimal("-4").compareTo(ExactExpression.parse("-4")));
        assertEquals(0, new BigDecimal("12.5").compareTo(ExactExpression.parse(" 12,5 ")));
        assertEquals(0, new BigDecimal("0.3").compareTo(ExactExpression.parse("0.1+0.2")));
        assertEquals("a plain number reads exactly as LengthUnit.parse reads it",
                LengthUnit.parse("40.125"), ExactExpression.parse("40.125"));
    }

    @Test
    public void anythingElseIsRefusedByName() {
        final String[] bad = {"", "  ", "abc", "1/0", "(1+2", "1+", "2**3", "1e3", "NaN",
                "Infinity", "sqrt(4)", "1..2", "x+1", "123456789012345678901234567890123456789012345678901234567890+12345"};
        for (String text : bad) {
            try {
                ExactExpression.parse(text);
                fail("accepted '" + text + "'");
            } catch (NumberFormatException expected) {
                // refused
            }
        }
    }

    @Test
    public void angleFormattingIsTheDisplayPrecision() {
        assertEquals("45°", SketchDraftingPresentation.formatDegrees(45.0));
        assertEquals("-30°", SketchDraftingPresentation.formatDegrees(-30.0));
        assertEquals("33.333°", SketchDraftingPresentation.formatDegrees(33.3333333));
        assertEquals("0.001°", SketchDraftingPresentation.formatDegrees(0.0005));
    }
}
