package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * The Surface context surface's decisions, on the JVM
 * ({@code MODELING-FOUNDATIONS-R1} C): a Finish kind, Stitch, a Thicken and a
 * staged value edit's Apply are drawn exactly when native's candidate would be
 * committed, and a downstream failure is named by its row.
 */
public class SurfacePresentationTest {

    private static double[] sketch(boolean open, int... verdicts) {
        final double[] s = new double[NativeViewport.SURFACE_SKETCH_STATE_SIZE];
        s[NativeViewport.SURFACE_SKETCH_ACTIVE] = open ? 1.0 : 0.0;
        for (int kind = 1; kind <= NativeViewport.SURFACE_CREATE_KIND_COUNT; kind++) {
            s[NativeViewport.SURFACE_SKETCH_VERDICT + kind] = verdicts[kind - 1];
        }
        return s;
    }

    @Test
    public void aFinishKindIsDrawnExactlyWhenItsCandidateSucceeds() {
        final int no = 17;  // any refusal
        final double[] s = sketch(true, 0, 0, NativeViewport.SURFACE_AXIS_UNRESOLVED, 45, 46, no);
        assertTrue(SurfacePresentation.createShown(s, NativeViewport.SURFACE_CREATE_PATCH));
        assertTrue(SurfacePresentation.createShown(s, NativeViewport.SURFACE_CREATE_EXTRUDE));
        assertFalse(SurfacePresentation.createShown(s, NativeViewport.SURFACE_CREATE_REVOLVE));
        assertFalse(SurfacePresentation.createShown(s, NativeViewport.SURFACE_CREATE_LOFT));
        assertFalse(SurfacePresentation.createShown(s, NativeViewport.SURFACE_CREATE_TRIM));
        assertFalse(SurfacePresentation.createShown(s, NativeViewport.SURFACE_CREATE_SECTION));
        assertTrue(SurfacePresentation.anyCreateShown(s));
        final double[] closed = sketch(false, 0, 0, 0, 0, 0, 0);
        assertFalse("nothing is drawn without an open Surface sketch",
                SurfacePresentation.createShown(closed, NativeViewport.SURFACE_CREATE_PATCH));
    }

    @Test
    public void whenNothingCanBeMadeTheAxisIsNamedFirst() {
        final double[] axis = sketch(true, 15, 17, NativeViewport.SURFACE_AXIS_UNRESOLVED, 45, 46, 9);
        assertFalse(SurfacePresentation.anyCreateShown(axis));
        assertEquals(NativeViewport.SURFACE_AXIS_UNRESOLVED, SurfacePresentation.blockingVerdict(axis));
        final double[] region = sketch(true, 15, 17, 17, 45, 46, 9);
        assertEquals("otherwise the Patch verdict", 15, SurfacePresentation.blockingVerdict(region));
    }

    @Test
    public void onlyExtrudeAndRevolveCarryAValue() {
        assertEquals(SurfacePresentation.VALUE_LENGTH,
                SurfacePresentation.createValue(NativeViewport.SURFACE_CREATE_EXTRUDE));
        assertEquals(SurfacePresentation.VALUE_ANGLE,
                SurfacePresentation.createValue(NativeViewport.SURFACE_CREATE_REVOLVE));
        for (int kind : new int[] {NativeViewport.SURFACE_CREATE_PATCH, NativeViewport.SURFACE_CREATE_LOFT,
                NativeViewport.SURFACE_CREATE_TRIM, NativeViewport.SURFACE_CREATE_SECTION}) {
            assertEquals(SurfacePresentation.VALUE_NONE, SurfacePresentation.createValue(kind));
        }
    }

    @Test
    public void stitchAndThickenAreDrawnOnlyOverASucceedingCandidate() {
        assertTrue(SurfacePresentation.stitchShown(NativeViewport.SURFACE_OK));
        assertFalse(SurfacePresentation.stitchShown(33));
        assertTrue(SurfacePresentation.thickenShown(NativeViewport.SURFACE_OK));
        assertFalse(SurfacePresentation.thickenShown(NativeViewport.SURFACE_THICKEN_UNSUPPORTED));
    }

    private static double[] row(int kind, long id, int state, int featureKind, double value) {
        final double[] v = new double[NativeViewport.TIMELINE_ROW_SIZE];
        v[NativeViewport.TIMELINE_ROW_KIND] = kind;
        v[NativeViewport.TIMELINE_ROW_ID] = id;
        v[NativeViewport.TIMELINE_ROW_EDIT_FEATURE] = id;
        v[NativeViewport.TIMELINE_ROW_STATE] = state;
        v[NativeViewport.TIMELINE_ROW_FEATURE_KIND] = featureKind;
        v[NativeViewport.TIMELINE_ROW_POSITIVE] = value;
        return v;
    }

    private static FeatureHistoryPresentation.Model model(int status, long failed, double[]... rows) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        header[NativeViewport.TIMELINE_STATUS] = status;
        header[NativeViewport.TIMELINE_FAILED_FEATURE] = failed;
        header[NativeViewport.TIMELINE_EDITING_FEATURE] = 1;
        header[NativeViewport.TIMELINE_EVALUATED] = 1;
        header[NativeViewport.TIMELINE_ROW_COUNT] = rows.length;
        final double[] all = new double[rows.length * NativeViewport.TIMELINE_ROW_SIZE];
        for (int i = 0; i < rows.length; i++) {
            System.arraycopy(rows[i], 0, all, i * NativeViewport.TIMELINE_ROW_SIZE, rows[i].length);
        }
        return FeatureHistoryPresentation.fromNative(FeatureHistoryPresentation.DOMAIN_SURFACE, header, all,
                rows.length);
    }

    @Test
    public void aStagedEditAppliesOnlyWhenEverythingRebuildsAndTheValueMoved() {
        final FeatureHistoryPresentation.Model ok = model(NativeViewport.SURFACE_OK, 0,
                row(1, 1, NativeViewport.TIMELINE_STATE_OK, NativeViewport.SURFACE_KIND_EXTRUDE, 0.8),
                row(1, 2, NativeViewport.TIMELINE_STATE_OK, NativeViewport.SURFACE_KIND_THICKEN, 0.1));
        assertTrue(SurfacePresentation.applyShown(ok, 0.5, 0.8));
        assertFalse("an unchanged value records nothing, so Apply is absent",
                SurfacePresentation.applyShown(ok, 0.8, 0.8));
        assertNull(SurfacePresentation.stagedFailure(ok));

        final int trimNotCoplanar = 29;
        final FeatureHistoryPresentation.Model broken = model(trimNotCoplanar, 4,
                row(0, 1, NativeViewport.TIMELINE_STATE_OK, 0, 0.25),
                row(1, 3, NativeViewport.TIMELINE_STATE_OK, NativeViewport.SURFACE_KIND_PATCH, 0.0),
                row(1, 4, NativeViewport.TIMELINE_STATE_FAILED, NativeViewport.SURFACE_KIND_TRIM, 0.0),
                row(1, 5, NativeViewport.TIMELINE_STATE_NOT_REGENERATED, NativeViewport.SURFACE_KIND_EXTRUDE, 1.0));
        assertFalse(SurfacePresentation.applyShown(broken, 0.0, 0.25));
        final FeatureHistoryPresentation.Row failure = SurfacePresentation.stagedFailure(broken);
        assertNotNull(failure);
        assertEquals("the first failing feature is the one named", 4L, failure.id);
    }

    @Test
    public void aRowsValueIsADistanceAnAngleAThicknessOrAnOffset() {
        final FeatureHistoryPresentation.Model m = model(NativeViewport.SURFACE_OK, 0,
                row(0, 1, NativeViewport.TIMELINE_STATE_OK, 0, 0.0),
                row(1, 1, NativeViewport.TIMELINE_STATE_OK, NativeViewport.SURFACE_KIND_EXTRUDE, 0.5),
                row(1, 2, NativeViewport.TIMELINE_STATE_OK, NativeViewport.SURFACE_KIND_REVOLVE, 360),
                row(1, 3, NativeViewport.TIMELINE_STATE_OK, NativeViewport.SURFACE_KIND_THICKEN, 0.1),
                row(1, 4, NativeViewport.TIMELINE_STATE_OK, NativeViewport.SURFACE_KIND_PATCH, 0));
        assertEquals(SurfacePresentation.VALUE_LENGTH, SurfacePresentation.rowValue(m.rows.get(0)));
        assertEquals(NativeViewport.SURFACE_VALUE_SKETCH, SurfacePresentation.valueTarget(m.rows.get(0)));
        assertEquals(SurfacePresentation.VALUE_LENGTH, SurfacePresentation.rowValue(m.rows.get(1)));
        assertEquals(SurfacePresentation.VALUE_ANGLE, SurfacePresentation.rowValue(m.rows.get(2)));
        assertEquals(SurfacePresentation.VALUE_LENGTH, SurfacePresentation.rowValue(m.rows.get(3)));
        assertEquals(SurfacePresentation.VALUE_NONE, SurfacePresentation.rowValue(m.rows.get(4)));
        assertEquals(NativeViewport.SURFACE_VALUE_FEATURE, SurfacePresentation.valueTarget(m.rows.get(1)));
    }

    @Test
    public void refusalsTheUserCanActOnHaveTheirOwnWords() {
        assertEquals(R.string.surface_refused_axis, SurfacePresentation.refusalMessage("AxisUnresolved"));
        assertEquals(R.string.surface_refused_stitch_gap, SurfacePresentation.refusalMessage("StitchGapTooLarge"));
        assertEquals(R.string.surface_refused_thicken_type,
                SurfacePresentation.refusalMessage("ThickenUnsupportedForSurfaceType"));
        assertEquals(R.string.surface_refused_other, SurfacePresentation.refusalMessage("SomethingElse"));
    }
}
