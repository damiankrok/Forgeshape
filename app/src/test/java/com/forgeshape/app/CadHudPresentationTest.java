package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import java.util.HashSet;
import java.util.Set;

import org.junit.Test;

/**
 * The compact CAD extrude HUD's presentation rules, on the JVM
 * (`CAD-VERTICAL-SLICE-R1`).
 *
 * <p>What is held here is the part of the HUD that is arithmetic and lookup:
 * the glyph follows the camera-attached scale inside a fixed band while the
 * touch target never moves off the 48 dp floor; each extent and each operation
 * has its own glyph and caption; the operation palette offers exactly what
 * native says is available; Flip is a One Side control; and the placement puts
 * the VALUE, not the cluster, on the arrow's anchor. A device proves the views
 * obey it; this proves the rule itself.
 */
public final class CadHudPresentationTest {

    /** The camera-attached multiplier's band, restated from the native rule. */
    private static final double SCALE_MIN = 0.80;
    private static final double SCALE_MAX = 1.60;

    // -----------------------------------------------------------------------
    // Glyph and hit
    // -----------------------------------------------------------------------

    @Test
    public void theGlyphStaysInsideItsBandForEveryScaleInTheNativeRange() {
        for (int step = 0; step <= 80; step++) {
            final double scale = SCALE_MIN + (SCALE_MAX - SCALE_MIN) * step / 80.0;
            final float glyph = CadHudPresentation.glyphDp(scale);
            assertTrue("glyph " + glyph + " dp at scale " + scale + " is at least 24 dp",
                    glyph >= 24.0f);
            assertTrue("glyph " + glyph + " dp at scale " + scale + " is at most 32 dp",
                    glyph <= 32.0f);
            assertTrue("and never reaches its own hit rectangle",
                    glyph < CadHudPresentation.hitDp(scale));
        }
        assertEquals("the floor saturates", 24.0f, CadHudPresentation.glyphDp(SCALE_MIN), 0.0f);
        assertEquals("the ceiling saturates", 32.0f, CadHudPresentation.glyphDp(SCALE_MAX), 0.0f);
    }

    @Test
    public void theGlyphFollowsTheCameraBetweenTheBounds() {
        assertEquals(28.0f, CadHudPresentation.glyphDp(1.0), 1e-6f);
        assertEquals(28.0f * 1.1f, CadHudPresentation.glyphDp(1.1), 1e-4f);
        float previous = 0.0f;
        for (int step = 0; step <= 80; step++) {
            final double scale = SCALE_MIN + (SCALE_MAX - SCALE_MIN) * step / 80.0;
            final float glyph = CadHudPresentation.glyphDp(scale);
            assertTrue("monotonic in the scale", glyph >= previous);
            previous = glyph;
        }
    }

    @Test
    public void theHitRectangleIsTheFloorAtEveryScale() {
        final double[] scales = {SCALE_MIN, 0.9, 1.0, 1.25, SCALE_MAX, 0.1, 5.0, Double.NaN,
                Double.POSITIVE_INFINITY, 0.0, -1.0};
        for (double scale : scales) {
            assertTrue("hit >= 48 dp at scale " + scale, CadHudPresentation.hitDp(scale) >= 48);
            assertEquals("and exactly the floor: the hit area never follows the camera",
                    48, CadHudPresentation.hitDp(scale));
        }
        for (float density : new float[]{1.0f, 1.5f, 2.0f, 2.625f, 3.0f, 3.5f, 4.0f}) {
            assertEquals(Math.round(48 * density), CadHudPresentation.hitPx(density));
            for (int step = 0; step <= 16; step++) {
                final double scale = SCALE_MIN + (SCALE_MAX - SCALE_MIN) * step / 16.0;
                assertTrue("the glyph fits inside the hit rectangle in pixels too",
                        CadHudPresentation.glyphPx(scale, density)
                                < CadHudPresentation.hitPx(density));
            }
        }
    }

    @Test
    public void theReferenceGlyphIsWellUnderTheOldSixtyDpControl() {
        // The cluster this replaces authored every control at 60 dp and scaled
        // the whole VIEW, so the smallest a control could be drawn was exactly
        // 48 dp and the largest 96 dp of text pill. The glyph is now 28 dp at the
        // reference scale and 32 dp at the largest, inside an unscaled 48 dp
        // target.
        final float oldAuthoredDp = 60.0f;
        assertEquals(28.0f, CadHudPresentation.glyphDp(1.0), 0.0f);
        assertTrue(CadHudPresentation.glyphDp(1.0) <= oldAuthoredDp / 2.0f);
        assertTrue("even the largest glyph is smaller than the old floor",
                CadHudPresentation.glyphDp(SCALE_MAX) < 48.0f);
        assertTrue("and the hit area is smaller than the old reference control",
                CadHudPresentation.hitDp(1.0) < oldAuthoredDp);
    }

    @Test
    public void aMultiplierThatSaysNothingIsTheReferenceSize() {
        for (double scale : new double[]{Double.NaN, Double.POSITIVE_INFINITY,
                Double.NEGATIVE_INFINITY, 0.0, -0.5}) {
            assertEquals(1.0f, CadHudPresentation.effectiveScale(scale), 0.0f);
            assertEquals(28.0f, CadHudPresentation.glyphDp(scale), 0.0f);
        }
        assertEquals(1.25f, CadHudPresentation.effectiveScale(1.25), 0.0f);
    }

    @Test
    public void theValuesPillIsSmallerThanItsHitRow() {
        assertEquals("the value's visible pill is 32 dp", 32, CadHudPresentation.VALUE_PILL_DP);
        assertTrue("inside the 48 dp hit row", CadHudPresentation.VALUE_PILL_DP
                < CadHudPresentation.hitDp(1.0));
    }

    // -----------------------------------------------------------------------
    // Extent
    // -----------------------------------------------------------------------

    @Test
    public void everyExtentHasItsOwnGlyphCaptionAndDescription() {
        final Set<Integer> icons = new HashSet<>();
        final Set<Integer> captions = new HashSet<>();
        final Set<Integer> descriptions = new HashSet<>();
        for (int mode : CadHudPresentation.EXTENTS) {
            icons.add(CadHudPresentation.extentIcon(mode));
            captions.add(CadHudPresentation.extentCaption(mode));
            descriptions.add(CadHudPresentation.extentDescription(mode));
        }
        assertEquals(3, icons.size());
        assertEquals(3, captions.size());
        assertEquals(3, descriptions.size());
        assertEquals(R.drawable.ic_extent_one_side,
                CadHudPresentation.extentIcon(NativeViewport.EXTENT_ONE_SIDE));
        assertEquals(R.drawable.ic_extent_symmetric,
                CadHudPresentation.extentIcon(NativeViewport.EXTENT_SYMMETRIC));
        assertEquals(R.drawable.ic_extent_two_sides,
                CadHudPresentation.extentIcon(NativeViewport.EXTENT_TWO_SIDES));
        assertEquals(R.string.cad_hud_caption_one_side,
                CadHudPresentation.extentCaption(NativeViewport.EXTENT_ONE_SIDE));
        assertEquals(R.string.cad_hud_caption_symmetric,
                CadHudPresentation.extentCaption(NativeViewport.EXTENT_SYMMETRIC));
        assertEquals(R.string.cad_hud_caption_two_sides,
                CadHudPresentation.extentCaption(NativeViewport.EXTENT_TWO_SIDES));
    }

    @Test
    public void theExtentsAreInTheNativeEnumsOrder() {
        assertEquals(NativeViewport.EXTENT_ONE_SIDE, CadHudPresentation.EXTENTS[0]);
        assertEquals(NativeViewport.EXTENT_SYMMETRIC, CadHudPresentation.EXTENTS[1]);
        assertEquals(NativeViewport.EXTENT_TWO_SIDES, CadHudPresentation.EXTENTS[2]);
    }

    @Test
    public void flipIsAOneSideControlAndTheSecondValueATwoSidesOne() {
        assertTrue(CadHudPresentation.flipPresent(NativeViewport.EXTENT_ONE_SIDE));
        assertFalse("Symmetric reaches both sides already",
                CadHudPresentation.flipPresent(NativeViewport.EXTENT_SYMMETRIC));
        assertFalse("Two Sides states both sides outright",
                CadHudPresentation.flipPresent(NativeViewport.EXTENT_TWO_SIDES));
        assertFalse("an unknown mode offers no Flip", CadHudPresentation.flipPresent(7));

        assertFalse(CadHudPresentation.secondValuePresent(NativeViewport.EXTENT_ONE_SIDE));
        assertFalse("Symmetric is ONE distance for two arrows",
                CadHudPresentation.secondValuePresent(NativeViewport.EXTENT_SYMMETRIC));
        assertTrue(CadHudPresentation.secondValuePresent(NativeViewport.EXTENT_TWO_SIDES));
    }

    // -----------------------------------------------------------------------
    // Operation
    // -----------------------------------------------------------------------

    @Test
    public void everyOperationHasItsOwnGlyphCaptionDescriptionAndColour() {
        final Set<Integer> icons = new HashSet<>();
        final Set<Integer> captions = new HashSet<>();
        final Set<Integer> descriptions = new HashSet<>();
        final Set<Integer> colours = new HashSet<>();
        for (int operation : CadHudPresentation.OPERATIONS) {
            assertTrue(CadHudPresentation.isKnownOperation(operation));
            icons.add(CadHudPresentation.operationIcon(operation));
            captions.add(CadHudPresentation.operationCaption(operation));
            descriptions.add(CadHudPresentation.operationDescription(operation));
            colours.add(CadHudPresentation.operationColorAttr(operation));
        }
        assertEquals("three SHAPES, so the meaning survives monochrome", 3, icons.size());
        assertEquals(3, captions.size());
        assertEquals(3, descriptions.size());
        assertEquals(3, colours.size());
        assertEquals(R.drawable.ic_op_new_body,
                CadHudPresentation.operationIcon(NativeViewport.OPERATION_NEW_BODY));
        assertEquals(R.drawable.ic_op_add,
                CadHudPresentation.operationIcon(NativeViewport.OPERATION_ADD));
        assertEquals(R.drawable.ic_op_cut,
                CadHudPresentation.operationIcon(NativeViewport.OPERATION_CUT));
        assertEquals(R.string.cad_hud_caption_new_body,
                CadHudPresentation.operationCaption(NativeViewport.OPERATION_NEW_BODY));
        assertEquals(R.string.cad_hud_caption_add,
                CadHudPresentation.operationCaption(NativeViewport.OPERATION_ADD));
        assertEquals(R.string.cad_hud_caption_cut,
                CadHudPresentation.operationCaption(NativeViewport.OPERATION_CUT));
        assertEquals("Add is the positive role", R.attr.fsTextSuccess,
                CadHudPresentation.operationColorAttr(NativeViewport.OPERATION_ADD));
        assertEquals("Cut is the destructive role", R.attr.fsTextError,
                CadHudPresentation.operationColorAttr(NativeViewport.OPERATION_CUT));
        assertNotEquals("New Body never spends the accent a commit owns", R.attr.fsAccent,
                CadHudPresentation.operationColorAttr(NativeViewport.OPERATION_NEW_BODY));
        assertFalse(CadHudPresentation.isKnownOperation(3));
        assertFalse(CadHudPresentation.isKnownOperation(-1));
    }

    @Test
    public void theOperationsAndTheirBitsAreTheNativeContract() {
        assertEquals(NativeViewport.OPERATION_NEW_BODY, CadHudPresentation.OPERATIONS[0]);
        assertEquals(NativeViewport.OPERATION_ADD, CadHudPresentation.OPERATIONS[1]);
        assertEquals(NativeViewport.OPERATION_CUT, CadHudPresentation.OPERATIONS[2]);
        assertEquals(NativeViewport.OPERATION_BIT_NEW_BODY,
                CadHudPresentation.operationBit(NativeViewport.OPERATION_NEW_BODY));
        assertEquals(NativeViewport.OPERATION_BIT_ADD,
                CadHudPresentation.operationBit(NativeViewport.OPERATION_ADD));
        assertEquals(NativeViewport.OPERATION_BIT_CUT,
                CadHudPresentation.operationBit(NativeViewport.OPERATION_CUT));
        assertEquals("an unknown operation has no bit", 0, CadHudPresentation.operationBit(9));
    }

    @Test
    public void thePaletteOffersExactlyWhatTheBitmaskOffers() {
        final int newBody = NativeViewport.OPERATION_BIT_NEW_BODY;
        final int add = NativeViewport.OPERATION_BIT_ADD;
        final int cut = NativeViewport.OPERATION_BIT_CUT;
        for (int bits = 0; bits < 8; bits++) {
            int expected = 0;
            for (int operation : CadHudPresentation.OPERATIONS) {
                final boolean offered = CadHudPresentation.operationOffered(operation, bits);
                assertEquals((bits & CadHudPresentation.operationBit(operation)) != 0, offered);
                if (offered) {
                    expected++;
                }
            }
            assertEquals(expected, CadHudPresentation.offeredCount(bits));
        }
        assertEquals(1, CadHudPresentation.offeredCount(newBody));
        assertEquals(3, CadHudPresentation.offeredCount(newBody | add | cut));
        assertFalse("bits outside the three are never an operation",
                CadHudPresentation.operationOffered(3, 0xFF));
    }

    @Test
    public void theBadgeIsAControlOnlyWhenThereIsSomethingElseToChoose() {
        final int newBody = NativeViewport.OPERATION_BIT_NEW_BODY;
        final int add = NativeViewport.OPERATION_BIT_ADD;
        final int cut = NativeViewport.OPERATION_BIT_CUT;
        assertFalse("only New Body: a statement, not a control",
                CadHudPresentation.operationBadgeHasChoice(
                        NativeViewport.OPERATION_NEW_BODY, newBody));
        assertFalse("nothing at all: still a statement",
                CadHudPresentation.operationBadgeHasChoice(NativeViewport.OPERATION_NEW_BODY, 0));
        assertTrue(CadHudPresentation.operationBadgeHasChoice(
                NativeViewport.OPERATION_NEW_BODY, newBody | add | cut));
        assertTrue(CadHudPresentation.operationBadgeHasChoice(
                NativeViewport.OPERATION_CUT, newBody | cut));
        assertFalse("the palette would hold only the answer already given",
                CadHudPresentation.operationBadgeHasChoice(NativeViewport.OPERATION_ADD, add));
        assertTrue("a current operation that is no longer offered can still be changed",
                CadHudPresentation.operationBadgeHasChoice(NativeViewport.OPERATION_ADD, newBody));
    }

    // -----------------------------------------------------------------------
    // Placement
    // -----------------------------------------------------------------------

    @Test
    public void theValueCentreOffsetPutsTheValueOnTheAnchor() {
        // A 236-wide cluster whose value starts at 56 and is 72 wide: the value's
        // centre is 92 from the cluster's left, the cluster's centre 118.
        final int width = 236;
        final int valueStart = 56;
        final int valueWidth = 72;
        final float offset = CadHudPresentation.valueCentreOffset(width, valueStart, valueWidth);
        assertEquals(26.0f, offset, 0.0f);
        // ViewportAnchorSpace centres the CLUSTER on (anchor + offset); the
        // value's centre then lands on the anchor exactly.
        final float anchor = 540.0f;
        final float clusterLeft = (anchor + offset) - width * 0.5f;
        assertEquals(anchor, clusterLeft + valueStart + valueWidth * 0.5f, 1e-4f);
        assertEquals("a value that IS the whole cluster needs no offset", 0.0f,
                CadHudPresentation.valueCentreOffset(100, 0, 100), 0.0f);
        assertTrue("a value right of centre pulls the cluster left",
                CadHudPresentation.valueCentreOffset(200, 150, 40) < 0.0f);
    }

    @Test
    public void clampedStartMirrorsTheSharedViewportClamp() {
        assertEquals("centred when it fits", 450.0f,
                CadHudPresentation.clampedStart(500.0f, 100.0f, 1080.0f), 0.0f);
        assertEquals("held at the leading edge", 0.0f,
                CadHudPresentation.clampedStart(20.0f, 100.0f, 1080.0f), 0.0f);
        assertEquals("held at the trailing edge", 980.0f,
                CadHudPresentation.clampedStart(1070.0f, 100.0f, 1080.0f), 0.0f);
        assertEquals("a box wider than the viewport starts at 0, never past the far edge",
                0.0f, CadHudPresentation.clampedStart(500.0f, 1200.0f, 1080.0f), 0.0f);
    }

    @Test
    public void aPaletteHangsBelowTheClusterUnlessThatLeavesTheViewport() {
        final float gap = 8.0f;
        // Room below: directly under the cluster.
        assertEquals(100.0f + 56.0f + gap + 28.0f,
                CadHudPresentation.paletteCentreY(100.0f, 56.0f, 56.0f, gap, 2000.0f), 0.0f);
        // No room below: directly above it.
        assertEquals(1900.0f - gap - 28.0f,
                CadHudPresentation.paletteCentreY(1900.0f, 56.0f, 56.0f, gap, 2000.0f), 0.0f);
        // Room on neither side: whichever side has more, and the shared clamp
        // keeps it on screen.
        final float tinyBelow = CadHudPresentation.paletteCentreY(10.0f, 56.0f, 200.0f, gap,
                150.0f);
        assertTrue("more room below than above", tinyBelow > 10.0f + 56.0f);
        final float tinyAbove = CadHudPresentation.paletteCentreY(90.0f, 56.0f, 200.0f, gap,
                150.0f);
        assertTrue("more room above than below", tinyAbove < 90.0f);
    }
}
