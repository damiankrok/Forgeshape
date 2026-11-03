package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * `CAD-V6-REVOLVE-NEWBODY-E2E-R1` on the JVM: which Revolve controls one native
 * state read draws, how an angle is written and read, the handle's bounded
 * touch proxy, and the ring arc's sampling rule. The device proves the views
 * obey it ({@code CadRevolveOwnerTest}); this pins the rules in every state.
 */
public final class CadRevolvePresentationTest {

    private static double[] state() {
        return new double[NativeViewport.REVOLVE_STATE_SIZE];
    }

    /** Ready, an area chosen, extruding, Revolve offered. */
    private static double[] readyExtruding() {
        final double[] s = state();
        s[NativeViewport.REVOLVE_AVAILABLE] = 1.0;
        s[NativeViewport.REVOLVE_SELECTED_AREAS] = 1.0;
        s[NativeViewport.REVOLVE_ANGLE] = 360.0;
        return s;
    }

    /** Revolving, axis chosen, candidate valid. */
    private static double[] revolving() {
        final double[] s = readyExtruding();
        s[NativeViewport.REVOLVE_ACTIVE] = 1.0;
        s[NativeViewport.REVOLVE_AXIS_ENTITY] = 2.0;
        s[NativeViewport.REVOLVE_LABEL_VISIBLE] = 1.0;
        s[NativeViewport.REVOLVE_CANDIDATE_STATUS] = NativeViewport.CAD_OK;
        return s;
    }

    @Test
    public void revolveIsOfferedOnlyWithAChosenAreaAndOnlyWhenNativeCanRevolve() {
        assertTrue("an area chosen, Revolve available",
                CadRevolvePresentation.entryShown(readyExtruding()));
        final double[] none = readyExtruding();
        none[NativeViewport.REVOLVE_SELECTED_AREAS] = 0.0;
        assertFalse("nothing chosen: no entry", CadRevolvePresentation.entryShown(none));
        final double[] unavailable = readyExtruding();
        unavailable[NativeViewport.REVOLVE_AVAILABLE] = 0.0;
        assertFalse("native cannot revolve (an Add/Cut edit): no entry",
                CadRevolvePresentation.entryShown(unavailable));
        assertFalse("already revolving: no second entry",
                CadRevolvePresentation.entryShown(revolving()));
        assertFalse("no session at all", CadRevolvePresentation.entryShown(state()));
    }

    @Test
    public void axisPickWithdrawsTheCommitAndTheLabelAndAsksForAnAxis() {
        final double[] picking = revolving();
        picking[NativeViewport.REVOLVE_AXIS_PICKING] = 1.0;
        picking[NativeViewport.REVOLVE_AXIS_ENTITY] = 0.0;
        picking[NativeViewport.REVOLVE_CANDIDATE_STATUS] = NativeViewport.CAD_REVOLVE_NEEDS_AXIS;
        assertTrue(CadRevolvePresentation.axisPicking(picking));
        assertFalse("no commit while the axis is picked", CadRevolvePresentation.commitShown(picking));
        assertFalse("no angle label without an axis", CadRevolvePresentation.labelShown(picking));
        assertTrue("Extrude instead stays: Back costs nothing",
                CadRevolvePresentation.extrudeInsteadShown(picking));
    }

    @Test
    public void theCommitIsDrawnOnlyForACandidateACommitMayMake() {
        assertTrue(CadRevolvePresentation.commitShown(revolving()));
        final double[] crossing = revolving();
        crossing[NativeViewport.REVOLVE_CANDIDATE_STATUS] =
                NativeViewport.CAD_REVOLVE_PROFILE_CROSSES_AXIS;
        assertFalse("a refused candidate is not committed", CadRevolvePresentation.commitShown(crossing));
        assertFalse("not revolving", CadRevolvePresentation.commitShown(readyExtruding()));
    }

    @Test
    public void anEditOfARevolvedBodyKeepsItsKind() {
        final double[] edit = revolving();
        edit[NativeViewport.REVOLVE_EDITING_REVOLVE_BODY] = 1.0;
        assertFalse("no Extrude instead over a revolved body",
                CadRevolvePresentation.extrudeInsteadShown(edit));
        assertTrue(CadRevolvePresentation.extrudeInsteadShown(revolving()));
    }

    @Test
    public void anglesAreWrittenAsTypedAndNeverAsTheDoublesExpansion() {
        assertEquals("360", CadRevolvePresentation.formatDegrees(360.0));
        assertEquals("180", CadRevolvePresentation.formatDegrees(180.0));
        assertEquals("90", CadRevolvePresentation.formatDegrees(90.0));
        assertEquals("37.5", CadRevolvePresentation.formatDegrees(37.5));
        assertEquals("0.001", CadRevolvePresentation.formatDegrees(0.001));
        assertEquals("12.346", CadRevolvePresentation.formatDegrees(12.3456));
        assertEquals("37.5°", CadRevolvePresentation.label(37.5));
        assertEquals("—", CadRevolvePresentation.formatDegrees(Double.NaN));
    }

    @Test
    public void typedAnglesParseExactlyAndTheRangeIsNativesToJudge() {
        assertEquals(360.0, CadRevolvePresentation.parseDegrees("360"), 0.0);
        assertEquals(37.5, CadRevolvePresentation.parseDegrees(" 37.5 "), 0.0);
        assertEquals(37.5, CadRevolvePresentation.parseDegrees("37,5"), 0.0);
        assertEquals(90.0, CadRevolvePresentation.parseDegrees("90°"), 0.0);
        // Parsed, then refused below JNI by name -- never clamped here.
        assertEquals(400.0, CadRevolvePresentation.parseDegrees("400"), 0.0);
        assertEquals(-90.0, CadRevolvePresentation.parseDegrees("-90"), 0.0);
        assertEquals(0.0, CadRevolvePresentation.parseDegrees("0"), 0.0);
        assertTrue(Double.isNaN(CadRevolvePresentation.parseDegrees("")));
        assertTrue(Double.isNaN(CadRevolvePresentation.parseDegrees("abc")));
        assertTrue(Double.isNaN(CadRevolvePresentation.parseDegrees(null)));
        // The value typed and the value written round-trip exactly.
        for (double degrees : new double[]{360.0, 180.0, 90.0, 37.5}) {
            assertEquals(degrees, CadRevolvePresentation.parseDegrees(
                    CadRevolvePresentation.formatDegrees(degrees)), 0.0);
        }
    }

    @Test
    public void theHandleProxyIsBoundedSoANearbyProfileTapIsNotSwallowed() {
        final float density = 2.625f;
        final float grab = CadRevolvePresentation.HANDLE_GRAB_DP * density;
        assertTrue("on the handle", CadRevolvePresentation.handleProxyContains(500, 500, 500, 500, density));
        assertTrue("inside the grab radius",
                CadRevolvePresentation.handleProxyContains(500, 500, 500 + grab - 1, 500, density));
        assertFalse("a tap on the profile beside the ring is not the handle's",
                CadRevolvePresentation.handleProxyContains(500, 500, 500 + grab + 2, 500, density));
        assertFalse("nor one diagonally beyond it",
                CadRevolvePresentation.handleProxyContains(500, 500, 500 + grab, 500 + grab, density));
        assertTrue("the proxy's diameter reaches the 48 dp floor",
                2 * CadRevolvePresentation.HANDLE_GRAB_DP >= CadRevolvePresentation.MIN_TARGET_DP);
    }

    @Test
    public void theRingArcIsAContinuousBoundedPolylineAtEveryAngle() {
        for (double degrees : new double[]{0.001, 1.0, 37.5, 90.0, 180.0, 270.0, 360.0}) {
            final int steps = CadRevolvePresentation.ringSegments(degrees);
            assertTrue("bounded", steps >= 8 && steps <= 64);
            double worst = 0.0;
            double[] previous = CadRevolvePresentation.ringSample(degrees, 0);
            for (int k = 1; k <= steps; ++k) {
                final double[] p = CadRevolvePresentation.ringSample(degrees, k);
                worst = Math.max(worst, Math.hypot(p[0] - previous[0], p[1] - previous[1]));
                previous = p;
            }
            // A chord of the unit ring never longer than 2*pi/64 * 1.01: no gap,
            // no jump, whatever the angle.
            assertTrue("continuous at " + degrees, worst <= 2.0 * Math.PI / 64.0 * 1.01 + 1e-12
                    || worst <= Math.toRadians(degrees) / 8.0 * 1.01 + 1e-12);
            final double[] end = CadRevolvePresentation.ringSample(degrees, steps);
            assertEquals("ends at the angle", Math.cos(Math.toRadians(degrees)), end[0], 1e-12);
            assertEquals(Math.sin(Math.toRadians(degrees)), end[1], 1e-12);
        }
    }
}
