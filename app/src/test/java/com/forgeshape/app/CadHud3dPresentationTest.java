package com.forgeshape.app;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * `CAD-V6-S2-OWNER-CORRECTION-E2E-R1` (HUD3D) on the JVM: the action dock is
 * read whole or not at all from the tool state, drawn through a perspective
 * homography that lands exactly on native's four corners, and claims ONLY its
 * projected quad plus the 48 dp floor square on its centre (HUD3D-07).
 */
public final class CadHud3dPresentationTest {

    /** A 2.625-density phone: 48 dp = 126 px. */
    private static final float FLOOR = 48.0f * 2.625f;

    private static double[] tool(float[] quad, float cx, float cy, float alpha) {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        tool[NativeViewport.CAD_EXTRUDE_ACTIVE] = 1.0;
        tool[NativeViewport.CAD_EXTRUDE_DOCK_VISIBLE] = 1.0;
        tool[NativeViewport.CAD_EXTRUDE_DOCK_ALPHA] = alpha;
        for (int i = 0; i < 8; i++) {
            tool[NativeViewport.CAD_EXTRUDE_DOCK_TL_X + i] = quad[i];
        }
        tool[NativeViewport.CAD_EXTRUDE_DOCK_CENTRE_X] = cx;
        tool[NativeViewport.CAD_EXTRUDE_DOCK_CENTRE_Y] = cy;
        tool[NativeViewport.CAD_EXTRUDE_DOCK_AXIS_SINE] = 0.8;
        return tool;
    }

    /** A small skewed dock: about 40 x 30 px, tilted, centred near (500, 900). */
    private static final float[] SKEWED = {478, 880, 520, 888, 522, 918, 476, 912};

    @Test
    public void hud3d07_theTouchProxyCoversTheDockAndNothingButTheFloorSquareBesideIt() {
        final CadHud3dPresentation.Dock dock =
                CadHud3dPresentation.fromToolState(tool(SKEWED, 500, 900, 1.0f), FLOOR);
        assertTrue(dock.visible);
        // Bounds: the quad united with the floor square -- at least 48 dp each way.
        assertTrue(dock.right - dock.left >= FLOOR - 1e-3f);
        assertTrue(dock.bottom - dock.top >= FLOOR - 1e-3f);
        for (int i = 0; i < 4; i++) {
            assertTrue("bounds cover every corner", SKEWED[2 * i] >= dock.left
                    && SKEWED[2 * i] <= dock.right && SKEWED[2 * i + 1] >= dock.top
                    && SKEWED[2 * i + 1] <= dock.bottom);
        }
        // ...and no more than the floor square needs: a small dock's box IS the square.
        assertEquals(500 - FLOOR / 2, dock.left, 1e-3f);
        assertEquals(500 + FLOOR / 2, dock.right, 1e-3f);
        // Claimed: the centre, every corner, the floor square's own corners.
        assertTrue(CadHud3dPresentation.claims(dock, 500, 900, FLOOR));
        for (int i = 0; i < 4; i++) {
            assertTrue(CadHud3dPresentation.claims(dock, SKEWED[2 * i], SKEWED[2 * i + 1], FLOOR));
        }
        assertTrue(CadHud3dPresentation.claims(dock, 500 + FLOOR / 2 - 1, 900 - FLOOR / 2 + 1,
                FLOOR));
        // Declined: anywhere outside both, however near.
        assertFalse(CadHud3dPresentation.claims(dock, 500 + FLOOR / 2 + 2, 900, FLOOR));
        assertFalse(CadHud3dPresentation.claims(dock, 500, 900 + FLOOR / 2 + 2, FLOOR));
        assertFalse(CadHud3dPresentation.claims(dock, 800, 900, FLOOR));
    }

    @Test
    public void hud3d07_aLargeDockClaimsItsQuadAndDeclinesItsBoxCorners() {
        // A large, strongly skewed dock: the box is the quad's bounds, and the
        // box corners outside the quad and the floor square are declined, so a
        // sketch cell there stays tappable.
        final float[] big = {300, 700, 700, 780, 690, 980, 310, 900};
        final CadHud3dPresentation.Dock dock =
                CadHud3dPresentation.fromToolState(tool(big, 500, 840, 1.0f), FLOOR);
        assertTrue(dock.visible);
        assertEquals(300, dock.left, 1e-3f);
        assertEquals(700, dock.right, 1e-3f);
        assertEquals(700, dock.top, 1e-3f);
        assertEquals(980, dock.bottom, 1e-3f);
        assertTrue(CadHud3dPresentation.claims(dock, 500, 840, FLOOR));
        assertTrue(CadHud3dPresentation.claims(dock, 650, 860, FLOOR));
        assertFalse("top-right box corner", CadHud3dPresentation.claims(dock, 698, 702, FLOOR));
        assertFalse("bottom-left box corner", CadHud3dPresentation.claims(dock, 302, 978, FLOOR));
    }

    @Test
    public void hud3d08_aHiddenDockIsAbsentWholeAndClaimsNothing() {
        final double[] hidden = tool(SKEWED, 500, 900, 1.0f);
        hidden[NativeViewport.CAD_EXTRUDE_DOCK_VISIBLE] = 0.0;
        hidden[NativeViewport.CAD_EXTRUDE_DOCK_HIDDEN] = NativeViewport.DOCK_HIDDEN_OFF_VIEWPORT;
        final CadHud3dPresentation.Dock dock = CadHud3dPresentation.fromToolState(hidden, FLOOR);
        assertFalse(dock.visible);
        assertEquals(NativeViewport.DOCK_HIDDEN_OFF_VIEWPORT, dock.hiddenReason);
        assertFalse(CadHud3dPresentation.claims(dock, 500, 900, FLOOR));
        // An inactive manipulator, a zero alpha or a non-finite corner: absent too.
        final double[] inactive = tool(SKEWED, 500, 900, 1.0f);
        inactive[NativeViewport.CAD_EXTRUDE_ACTIVE] = 0.0;
        assertFalse(CadHud3dPresentation.fromToolState(inactive, FLOOR).visible);
        assertFalse(CadHud3dPresentation.fromToolState(tool(SKEWED, 500, 900, 0.0f), FLOOR)
                .visible);
        final float[] broken = SKEWED.clone();
        broken[3] = Float.NaN;
        assertFalse(CadHud3dPresentation.fromToolState(tool(broken, 500, 900, 1.0f), FLOOR)
                .visible);
        assertFalse(CadHud3dPresentation.fromToolState(null, FLOOR).visible);
        assertFalse(CadHud3dPresentation.fromToolState(new double[3], FLOOR).visible);
    }

    @Test
    public void theHomographyLandsTheSourceBoxExactlyOnTheFourCorners() {
        for (float[] quad : new float[][]{SKEWED, {300, 700, 700, 780, 690, 980, 310, 900},
                {10, 10, 110, 10, 110, 60, 10, 60}, {200, 200, 260, 170, 280, 260, 190, 240}}) {
            final float side = CadHud3dPresentation.sourceSide(quad);
            final float[] m = CadHud3dPresentation.homography(side, side, quad);
            final float[][] src = {{0, 0}, {side, 0}, {side, side}, {0, side}};
            for (int i = 0; i < 4; i++) {
                final float[] at = CadHud3dPresentation.map(m, src[i][0], src[i][1]);
                assertEquals(quad[2 * i], at[0], 0.02f);
                assertEquals(quad[2 * i + 1], at[1], 0.02f);
            }
            // The source centre lands inside the quad: no fold, no mirror.
            final float[] mid = CadHud3dPresentation.map(m, side / 2, side / 2);
            assertTrue(CadHud3dPresentation.insideQuad(quad, mid[0], mid[1]));
        }
        // An axis-aligned rectangle is a pure scale and translation.
        final float[] m = CadHud3dPresentation.homography(50, 50,
                new float[]{10, 10, 110, 10, 110, 60, 10, 60});
        assertArrayEquals(new float[]{2, 0, 10, 0, 1, 10, 0, 0, 1}, m, 1e-5f);
        assertNull("a collapsed quad has no homography",
                CadHud3dPresentation.homography(10, 10, new float[]{0, 0, 0, 0, 0, 0, 0, 0}));
        assertNull(CadHud3dPresentation.homography(0, 10, SKEWED));
    }

    @Test
    public void theSourceBoxIsTheQuadsLongestEdgeSoTheGlyphRastersNearUnitScale() {
        assertEquals(100.0f, CadHud3dPresentation.sourceSide(
                new float[]{10, 10, 110, 10, 110, 60, 10, 60}), 1e-3f);
        assertEquals(1.0f, CadHud3dPresentation.sourceSide(new float[8]), 0.0f);
    }

    @Test
    public void insideQuadAcceptsEitherWindingAndItsEdges() {
        final float[] cw = {0, 0, 10, 0, 10, 10, 0, 10};
        final float[] ccw = {0, 0, 0, 10, 10, 10, 10, 0};
        for (float[] quad : new float[][]{cw, ccw}) {
            assertTrue(CadHud3dPresentation.insideQuad(quad, 5, 5));
            assertTrue(CadHud3dPresentation.insideQuad(quad, 0, 5));
            assertFalse(CadHud3dPresentation.insideQuad(quad, 11, 5));
            assertFalse(CadHud3dPresentation.insideQuad(quad, -0.5f, -0.5f));
        }
    }
}
