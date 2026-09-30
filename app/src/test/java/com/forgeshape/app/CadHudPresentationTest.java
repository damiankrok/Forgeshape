package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import java.util.HashSet;
import java.util.Set;

import org.junit.Test;

/**
 * The CAD extrude HUD's presentation rules, on the JVM
 * (`CAD-VERTICAL-SLICE-R1`, `CAD-FOUNDATION-C1`).
 *
 * <p>What is held here is the part of the HUD that is arithmetic and lookup:
 * the glyph and the value text follow the camera-attached scale over a wide
 * band while the invisible touch proxy never moves off the 48 dp floor; the
 * value reads upright along its leader and stands above it on the visible part
 * of the line; the attached glyphs stand along the leader without their proxies
 * overlapping each other or the value, and a control whose point is off screen
 * is hidden rather than clamped; each extent and each operation has its own
 * glyph and caption, and captions belong to palettes; the operation palette
 * offers exactly what native says is available; Flip is a One Side control. A
 * device proves the views obey it; this proves the rule itself.
 */
public final class CadHudPresentationTest {

    /** The camera-attached multiplier's band, restated from the native rule. */
    private static final double SCALE_MIN = 0.40;
    private static final double SCALE_MAX = 1.60;

    // -----------------------------------------------------------------------
    // Visual size and hit proxy
    // -----------------------------------------------------------------------

    @Test
    public void theGlyphFollowsTheWholeBandAndSaturatesAtBothEnds() {
        assertEquals(28.0f, CadHudPresentation.glyphDp(1.0), 1e-6f);
        assertEquals(28.0f * 0.40f, CadHudPresentation.glyphDp(SCALE_MIN), 1e-4f);
        assertEquals(28.0f * 1.60f, CadHudPresentation.glyphDp(SCALE_MAX), 1e-4f);
        assertEquals("below the band it saturates", CadHudPresentation.glyphDp(SCALE_MIN),
                CadHudPresentation.glyphDp(0.1), 0.0f);
        assertEquals("above the band it saturates", CadHudPresentation.glyphDp(SCALE_MAX),
                CadHudPresentation.glyphDp(5.0), 0.0f);
        float previous = 0.0f;
        for (int step = 0; step <= 120; step++) {
            final double scale = SCALE_MIN + (SCALE_MAX - SCALE_MIN) * step / 120.0;
            final float glyph = CadHudPresentation.glyphDp(scale);
            assertTrue("strictly monotonic inside the band", glyph > previous);
            previous = glyph;
        }
    }

    @Test
    public void theVisualRangeIsFarWiderThanTheOldTwentyFourToThirtyTwoClamp() {
        final float smallest = CadHudPresentation.glyphDp(SCALE_MIN);
        final float largest = CadHudPresentation.glyphDp(SCALE_MAX);
        assertTrue("a pulled-back camera draws well under the old 24 dp floor: " + smallest,
                smallest < 24.0f * 0.5f);
        assertTrue("a close camera draws past the old 32 dp ceiling: " + largest,
                largest > 32.0f);
        assertEquals("a factor of four, not the old four thirds", 4.0f, largest / smallest, 1e-4f);
    }

    @Test
    public void theHitProxyIsTheFloorAtEveryScaleWhateverIsDrawn() {
        final double[] scales = {SCALE_MIN, 0.5, 0.9, 1.0, 1.25, SCALE_MAX, 0.1, 5.0, Double.NaN,
                Double.POSITIVE_INFINITY, 0.0, -1.0};
        for (double scale : scales) {
            assertEquals("the proxy never follows the camera", 48,
                    CadHudPresentation.hitDp(scale));
        }
        for (float density : new float[]{1.0f, 1.5f, 2.0f, 2.625f, 3.0f, 3.5f, 4.0f}) {
            assertEquals(Math.round(48 * density), CadHudPresentation.hitPx(density));
            for (int step = 0; step <= 16; step++) {
                final double scale = SCALE_MIN + (SCALE_MAX - SCALE_MIN) * step / 16.0;
                assertTrue("the drawn glyph fits inside the proxy in pixels too",
                        CadHudPresentation.glyphPx(scale, density)
                                < CadHudPresentation.hitPx(density));
            }
        }
    }

    @Test
    public void theValueTextFollowsThePanelsScaleInsideItsLegibilityBand() {
        assertEquals(14.0f, CadHudPresentation.valueTextSp(1.0), 1e-6f);
        assertEquals("the floor keeps it readable", 9.0f,
                CadHudPresentation.valueTextSp(SCALE_MIN), 0.0f);
        assertEquals("the ceiling keeps it an annotation", 18.0f,
                CadHudPresentation.valueTextSp(SCALE_MAX), 0.0f);
        assertEquals("the SAME saturating multiplier as the panel, not its own",
                CadHudPresentation.valueTextSp(SCALE_MIN), CadHudPresentation.valueTextSp(0.05),
                0.0f);
        float previous = 0.0f;
        for (int step = 0; step <= 60; step++) {
            final double scale = SCALE_MIN + (SCALE_MAX - SCALE_MIN) * step / 60.0;
            final float sp = CadHudPresentation.valueTextSp(scale);
            assertTrue("monotonic", sp >= previous);
            assertTrue("banded", sp >= 9.0f && sp <= 18.0f);
            // Inside the band the text and the glyph keep one ratio: one policy.
            if (sp > 9.0f && sp < 18.0f) {
                assertEquals(14.0f / 28.0f, sp / CadHudPresentation.glyphDp(scale), 1e-4f);
            }
            previous = sp;
        }
    }

    @Test
    public void theLoneRetainedSketchChipKeepsItsCompactBand() {
        assertEquals(24.0f, CadHudPresentation.loneGlyphDp(SCALE_MIN), 0.0f);
        assertEquals(28.0f, CadHudPresentation.loneGlyphDp(1.0), 1e-6f);
        assertEquals(32.0f, CadHudPresentation.loneGlyphDp(SCALE_MAX), 0.0f);
    }

    @Test
    public void aMultiplierThatSaysNothingIsTheReferenceSize() {
        for (double scale : new double[]{Double.NaN, Double.POSITIVE_INFINITY,
                Double.NEGATIVE_INFINITY, 0.0, -0.5}) {
            assertEquals(1.0f, CadHudPresentation.effectiveScale(scale), 0.0f);
            assertEquals(28.0f, CadHudPresentation.glyphDp(scale), 0.0f);
            assertEquals(14.0f, CadHudPresentation.valueTextSp(scale), 0.0f);
        }
        assertEquals(1.25f, CadHudPresentation.effectiveScale(1.25), 0.0f);
    }

    @Test
    public void captionsBelongToPalettesAndNeverToAPanelGlyph() {
        assertTrue(CadHudPresentation.captionShown(true, false));
        assertFalse(CadHudPresentation.captionShown(true, true));
        assertFalse(CadHudPresentation.captionShown(false, false));
        assertFalse(CadHudPresentation.captionShown(false, true));
    }

    // -----------------------------------------------------------------------
    // The leader: upright reading, clipping, layout
    // -----------------------------------------------------------------------

    @Test
    public void theReadingAngleIsAlwaysUpright() {
        assertEquals(0.0f, CadHudPresentation.readingAngleDegrees(1.0f, 0.0f), 1e-4f);
        assertEquals("a leftward line reads left to right", 0.0f,
                CadHudPresentation.readingAngleDegrees(-1.0f, 0.0f), 1e-4f);
        assertEquals("a vertical line reads bottom to top", -90.0f,
                CadHudPresentation.readingAngleDegrees(0.0f, 1.0f), 1e-4f);
        assertEquals(-90.0f, CadHudPresentation.readingAngleDegrees(0.0f, -1.0f), 1e-4f);
        assertEquals(45.0f, CadHudPresentation.readingAngleDegrees(1.0f, 1.0f), 1e-4f);
        assertEquals("flipped past 90 degrees rather than read upside down", 45.0f,
                CadHudPresentation.readingAngleDegrees(-1.0f, -1.0f), 1e-4f);
        assertEquals(-45.0f, CadHudPresentation.readingAngleDegrees(-1.0f, 1.0f), 1e-4f);
        for (int degrees = -360; degrees <= 360; degrees += 5) {
            final double r = Math.toRadians(degrees);
            final float a = CadHudPresentation.readingAngleDegrees((float) Math.cos(r),
                    (float) Math.sin(r));
            assertTrue("in [-90, 90) at " + degrees + ": " + a, a >= -90.0f && a < 90.0f);
        }
        assertEquals(0.0f, CadHudPresentation.readingAngleDegrees(0.0f, 0.0f), 0.0f);
    }

    @Test
    public void aLeaderIsClippedToItsVisiblePartAndAWhollyHiddenOneHasNone() {
        final float[] inside = CadHudPresentation.clipToViewport(10, 10, 90, 50, 100, 100);
        assertEquals(10.0f, inside[0], 1e-4f);
        assertEquals(90.0f, inside[2], 1e-4f);
        final float[] crossing = CadHudPresentation.clipToViewport(-100, 50, 50, 50, 100, 100);
        assertEquals(0.0f, crossing[0], 1e-4f);
        assertEquals(50.0f, crossing[2], 1e-4f);
        assertEquals(null, CadHudPresentation.clipToViewport(-50, -50, -10, -10, 100, 100));
        assertEquals(null, CadHudPresentation.clipToViewport(150, 10, 300, 90, 100, 100));
        assertEquals(null, CadHudPresentation.clipToViewport(Float.NaN, 0, 1, 1, 100, 100));
    }

    private static CadHudPresentation.LeaderLayout layout(float sx, float sy, float ex,
                                                          float ey) {
        return CadHudPresentation.layoutLeader(sx, sy, ex, ey, 1080.0f, 2000.0f, 40.0f, 8.0f);
    }

    @Test
    public void theValueStandsAboveTheMiddleOfItsLeaderAndReadsAlongIt() {
        final CadHudPresentation.LeaderLayout l = layout(300, 1000, 700, 1000);
        assertTrue(l.valueVisible);
        assertEquals(0.0f, l.rotation, 1e-4f);
        assertEquals("centred along the line", 500.0f, l.valueX, 1e-3f);
        assertEquals("above it by half the text and the gap", 1000.0f - 20.0f - 8.0f, l.valueY,
                1e-3f);
        assertEquals(400.0f, l.visibleLength, 1e-3f);
        // A steep leader drawn from bottom to top still reads upright, above.
        final CadHudPresentation.LeaderLayout steep = layout(500, 1400, 500, 600);
        assertEquals(-90.0f, steep.rotation, 1e-4f);
        assertTrue("above a vertical line is to its left", steep.valueX < 500.0f);
        assertEquals(1000.0f, steep.valueY, 1e-3f);
        // The same leader drawn the other way gives the same reading.
        final CadHudPresentation.LeaderLayout reversed = layout(500, 600, 500, 1400);
        assertEquals(steep.rotation, reversed.rotation, 1e-4f);
        assertEquals(steep.valueX, reversed.valueX, 1e-3f);
    }

    @Test
    public void aValueStandsOnTheVisiblePartAndAWhollyHiddenLeaderHasNone() {
        final CadHudPresentation.LeaderLayout l = layout(700, 1000, 1400, 1000);
        assertTrue(l.valueVisible);
        assertEquals((700.0f + 1080.0f) * 0.5f, l.valueX, 1e-3f);
        assertEquals(380.0f, l.visibleLength, 1e-3f);
        assertFalse(layout(1200, 100, 1500, 100).valueVisible);
    }

    // -----------------------------------------------------------------------
    // The action panel (CAD-FOUNDATION-C2)
    // -----------------------------------------------------------------------

    /** A 2.625-density phone, 1080 x 2000 px: 48 dp = 126 px. */
    private static final float DENSITY = 2.625f;

    private static CadHudPresentation.PanelLayout panel(float hx, float hy, float ax, float ay,
                                                        int icons, double scale) {
        return CadHudPresentation.layoutPanel(true, hx, hy, ax, ay, Float.NaN, Float.NaN, icons,
                scale, DENSITY, 1080.0f, 2000.0f);
    }

    @Test
    public void thePanelIsOneRigidPlateWhoseInternalSpacingNeverChangesWithZoom() {
        assertEquals(3, CadHudPresentation.panelIconCount(NativeViewport.EXTENT_ONE_SIDE));
        assertEquals(2, CadHudPresentation.panelIconCount(NativeViewport.EXTENT_SYMMETRIC));
        assertEquals(2, CadHudPresentation.panelIconCount(NativeViewport.EXTENT_TWO_SIDES));
        // Reference plate: 3 x 28 + 2 x 2 + 2 x 4 = 96 x 36 dp.
        assertEquals(96.0f, CadHudPresentation.panelReferenceWidthDp(3), 1e-4f);
        assertEquals(66.0f, CadHudPresentation.panelReferenceWidthDp(2), 1e-4f);
        assertEquals(36.0f, CadHudPresentation.panelReferenceHeightDp(), 1e-4f);
        for (int step = 0; step <= 24; step++) {
            final double scale = SCALE_MIN + (SCALE_MAX - SCALE_MIN) * step / 24.0;
            final float s = CadHudPresentation.visualScale(scale);
            final float glyph = CadHudPresentation.glyphDp(scale);
            final float first = CadHudPresentation.panelIconOffsetDp(0, 3, scale);
            final float middle = CadHudPresentation.panelIconOffsetDp(1, 3, scale);
            final float last = CadHudPresentation.panelIconOffsetDp(2, 3, scale);
            assertEquals("the middle glyph is the plate's centre", 0.0f, middle, 1e-4f);
            assertEquals("evenly pitched", middle - first, last - middle, 1e-4f);
            assertEquals("the pitch is one fixed multiple of the glyph at every zoom",
                    30.0f / 28.0f, (middle - first) / glyph, 1e-4f);
            final CadHudPresentation.PanelLayout p = panel(540, 1000, 80, 0, 3, scale);
            assertEquals("the plate scales as ONE unit",
                    96.0f / 36.0f, p.plateWidth / p.plateHeight, 1e-4f);
            assertEquals(96.0f * s * DENSITY, p.plateWidth, 1e-2f);
            assertEquals(s, p.scale, 0.0f);
        }
    }

    @Test
    public void thePanelStandsJustPastTheArrowsPointAlongTheArrow() {
        for (double scale : new double[]{SCALE_MIN, 1.0, SCALE_MAX}) {
            final CadHudPresentation.PanelLayout p = panel(540, 1000, 80, 0, 3, scale);
            assertTrue(p.visible);
            assertEquals(CadHudPresentation.PANEL_BEYOND, p.placement);
            assertEquals("on the arrow's line", 1000.0f, p.centreY, 1e-3f);
            assertTrue("past the point", p.centreX > 540.0f);
            final float clear = (CadHudPresentation.ARROW_CORRIDOR_DP
                    + CadHudPresentation.PANEL_CLEAR_DP) * DENSITY;
            assertEquals("its proxy's near edge exactly one clear corridor from the point",
                    clear, CadHudPresentation.panelClearance(p), 1e-2f);
            assertEquals(540.0f + clear + p.hitWidth * 0.5f, p.centreX, 1e-2f);
        }
        // Any direction: the box follows the arrow, and never nears the point.
        for (int degrees = 0; degrees < 360; degrees += 10) {
            final double r = Math.toRadians(degrees);
            // A screen direction in PIXELS, as native's points give it.
            final CadHudPresentation.PanelLayout p = panel(540, 1000,
                    (float) (Math.cos(r) * 80.0), (float) (Math.sin(r) * 80.0), 3, 1.0);
            assertTrue(p.visible);
            assertEquals(CadHudPresentation.PANEL_BEYOND, p.placement);
            final float along = (float) ((p.centreX - 540) * Math.cos(r)
                    + (p.centreY - 1000) * Math.sin(r));
            assertTrue("past the point at " + degrees, along > 0.0f);
            assertTrue("clear of the arrow's grab corridor at " + degrees,
                    CadHudPresentation.panelClearance(p)
                            >= CadHudPresentation.ARROW_CORRIDOR_DP * DENSITY);
        }
    }

    @Test
    public void thePanelIsShownWholeOrNotAtAllAndNeverClampedAwayFromTheArrow() {
        // Past the point would leave the viewport: beside it, away from the leader.
        final CadHudPresentation.PanelLayout beside = CadHudPresentation.layoutPanel(true, 900,
                1000, 1, 0, 800, 900, 3, 1.0, DENSITY, 1080, 2000);
        assertTrue(beside.visible);
        assertEquals(CadHudPresentation.PANEL_AWAY, beside.placement);
        assertTrue("away from the leader, which stands above", beside.centreY > 1000.0f);
        assertInside(beside);
        // Nowhere a whole panel fits (a corner): hidden WHOLE, never partial.
        final CadHudPresentation.PanelLayout corner = CadHudPresentation.layoutPanel(true, 1075,
                5, 1, -1, Float.NaN, Float.NaN, 3, 1.0, DENSITY, 1080, 2000);
        assertFalse(corner.visible);
        // The point behind the camera or off screen: hidden, not guessed.
        assertFalse(CadHudPresentation.layoutPanel(false, 540, 1000, 1, 0, Float.NaN, Float.NaN,
                3, 1.0, DENSITY, 1080, 2000).visible);
        assertFalse(panel(-20, 1000, 1, 0, 3, 1.0).visible);
        // Every visible placement over a sweep is wholly inside the viewport.
        for (int x = 0; x <= 1080; x += 60) {
            for (int y = 0; y <= 2000; y += 100) {
                final CadHudPresentation.PanelLayout p = panel(x, y, 60, 60, 3, 1.3);
                if (p.visible) {
                    assertInside(p);
                }
            }
        }
    }

    private static void assertInside(CadHudPresentation.PanelLayout p) {
        assertTrue(p.centreX - p.hitWidth * 0.5f >= 0.0f);
        assertTrue(p.centreY - p.hitHeight * 0.5f >= 0.0f);
        assertTrue(p.centreX + p.hitWidth * 0.5f <= 1080.0f);
        assertTrue(p.centreY + p.hitHeight * 0.5f <= 2000.0f);
    }

    @Test
    public void anArrowSeenEndOnStillPlacesThePanelDeterministically() {
        final CadHudPresentation.PanelLayout a = panel(540, 1000, 0.2f, 0.1f, 2, 1.0);
        final CadHudPresentation.PanelLayout b = panel(540, 1000, 0.0f, 0.0f, 2, 1.0);
        assertTrue(a.visible && b.visible);
        assertEquals("a sub-pixel direction reads as screen right", a.centreX, b.centreX, 0.0f);
        assertEquals(1000.0f, b.centreY, 0.0f);
    }

    @Test
    public void theHitAreaIsOneGroupProxyIndependentOfTheDrawnSize() {
        final float hit = CadHudPresentation.HIT_DP * DENSITY;
        for (int step = 0; step <= 24; step++) {
            final double scale = SCALE_MIN + (SCALE_MAX - SCALE_MIN) * step / 24.0;
            final CadHudPresentation.PanelLayout p = panel(300, 1000, 80, 0, 3, scale);
            assertTrue("at least 48 dp each way", p.hitWidth >= hit && p.hitHeight >= hit);
            assertTrue("covering the whole drawn plate",
                    p.hitWidth >= p.plateWidth && p.hitHeight >= p.plateHeight);
            // Every drawn glyph centre is owned by the ONE proxy, whatever the zoom.
            for (int i = 0; i < 3; i++) {
                final float gx = p.centreX
                        + CadHudPresentation.panelIconOffsetDp(i, 3, scale) * DENSITY;
                assertTrue(CadHudPresentation.panelOwnsTouch(p, gx, p.centreY));
            }
            assertFalse("the arrow's point is never the panel's",
                    CadHudPresentation.panelOwnsTouch(p, 300, 1000));
        }
        // At the scale the drawn pitch would put three 48 dp proxies on top of
        // each other: the reason there is one proxy, stated as numbers.
        final float pitchDp = CadHudPresentation.panelIconOffsetDp(1, 3, 1.0)
                - CadHudPresentation.panelIconOffsetDp(0, 3, 1.0);
        assertTrue(pitchDp < CadHudPresentation.HIT_DP);
        assertFalse(CadHudPresentation.panelOwnsTouch(new CadHudPresentation.PanelLayout(), 0, 0));
    }

    @Test
    public void theAnnotationCollapsesWholeOnlyAtTheFloorAndWhenTheValueOutgrowsItsLeader() {
        assertTrue("at the floor, a leader shorter than its value",
                CadHudPresentation.annotationCollapsed(true, 40.0f, 90.0f));
        assertFalse("at the floor, a long leader: a big model still reads",
                CadHudPresentation.annotationCollapsed(true, 400.0f, 90.0f));
        assertFalse("a short leader still shrinking with the camera: a thin extrusion up close",
                CadHudPresentation.annotationCollapsed(false, 10.0f, 90.0f));
        assertFalse(CadHudPresentation.annotationCollapsed(false, 400.0f, 90.0f));
    }

    @Test
    public void aRotatedValueCoversItsRotatedBounds() {
        final float[] flat = CadHudPresentation.rotatedBounds(100, 40, 0);
        assertEquals(100.0f, flat[0], 1e-3f);
        assertEquals(40.0f, flat[1], 1e-3f);
        final float[] upright = CadHudPresentation.rotatedBounds(100, 40, -90);
        assertEquals(40.0f, upright[0], 1e-3f);
        assertEquals(100.0f, upright[1], 1e-3f);
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
