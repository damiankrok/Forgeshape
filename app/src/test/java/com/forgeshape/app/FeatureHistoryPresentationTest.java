package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * The Parametric History's presentation decisions, on the JVM
 * ({@code MODELING-FOUNDATIONS-R1} A): rows read in native's order by durable
 * id, the one failing row, which rows a tap may open, the state glyphs, and
 * when the regeneration issue card is owed.
 */
public class FeatureHistoryPresentationTest {

    private static double[] header(int status, long failed, long editing, boolean evaluated, int rows) {
        final double[] h = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        h[NativeViewport.TIMELINE_STATUS] = status;
        h[NativeViewport.TIMELINE_FAILED_FEATURE] = failed;
        h[NativeViewport.TIMELINE_EDITING_FEATURE] = editing;
        h[NativeViewport.TIMELINE_EVALUATED] = evaluated ? 1.0 : 0.0;
        h[NativeViewport.TIMELINE_ROW_COUNT] = rows;
        return h;
    }

    /** Rows: [kind, id, editTarget, state, editing] per row. */
    private static double[] rows(long[][] spec) {
        final double[] v = new double[spec.length * NativeViewport.TIMELINE_ROW_SIZE];
        for (int i = 0; i < spec.length; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            v[at + NativeViewport.TIMELINE_ROW_KIND] = spec[i][0];
            v[at + NativeViewport.TIMELINE_ROW_ID] = spec[i][1];
            v[at + NativeViewport.TIMELINE_ROW_EDIT_FEATURE] = spec[i][2];
            v[at + NativeViewport.TIMELINE_ROW_STATE] = spec[i][3];
            v[at + NativeViewport.TIMELINE_ROW_EDITING] = spec[i][4];
            v[at + NativeViewport.TIMELINE_ROW_ORDINAL] = i + 1;
        }
        return v;
    }

    private static final int S = NativeViewport.TIMELINE_KIND_SKETCH;
    private static final int F = NativeViewport.TIMELINE_KIND_FEATURE;
    private static final int OK = NativeViewport.TIMELINE_STATE_OK;
    private static final int FAILED = NativeViewport.TIMELINE_STATE_FAILED;
    private static final int NOT = NativeViewport.TIMELINE_STATE_NOT_REGENERATED;
    private static final int PENDING = NativeViewport.TIMELINE_STATE_PENDING;
    private static final int UNUSED = NativeViewport.TIMELINE_STATE_UNUSED;

    /** The staged chain of PAR-09: base edited, Cut 3 fails, Cut 4 not rebuilt. */
    private static FeatureHistoryPresentation.Model stagedFailure() {
        final long[][] spec = {
                {S, 1, 1, OK, 1}, {F, 1, 1, OK, 1},
                {S, 2, 2, OK, 0}, {F, 2, 2, OK, 0},
                {S, 3, 3, OK, 0}, {F, 3, 3, FAILED, 0},
                {S, 4, 4, OK, 0}, {F, 4, 4, NOT, 0}};
        return FeatureHistoryPresentation.fromNative(FeatureHistoryPresentation.DOMAIN_CAD,
                header(40, 3, 1, true, spec.length), rows(spec), spec.length);
    }

    @Test
    public void rowsKeepNativeOrderAndDurableIds() {
        final FeatureHistoryPresentation.Model model = stagedFailure();
        assertEquals(8, model.rows.size());
        assertTrue(model.rows.get(0).isSketch());
        assertEquals(1L, model.rows.get(0).id);
        assertFalse(model.rows.get(5).isSketch());
        assertEquals(3L, model.rows.get(5).id);
        assertEquals(3L, model.rows.get(5).editTarget);
    }

    @Test
    public void exactlyTheFailingRowIsFoundByIdNotPosition() {
        final FeatureHistoryPresentation.Model model = stagedFailure();
        assertEquals(5, model.failedRowIndex());
        assertEquals(3L, model.failedRow().id);
        assertTrue(model.failedRow().isFailed());
    }

    @Test
    public void glyphsCarryTheStateByShape() {
        final FeatureHistoryPresentation.Model model = stagedFailure();
        assertEquals(FeatureHistoryPresentation.MARK_EDITING + " " + FeatureHistoryPresentation.MARK_OK,
                model.rows.get(1).mark());
        assertEquals(FeatureHistoryPresentation.MARK_FAILED, model.rows.get(5).mark());
        assertEquals(FeatureHistoryPresentation.MARK_NOT_REGENERATED, model.rows.get(7).mark());
    }

    @Test
    public void anUnusedSketchIsListedButNotOffered() {
        final long[][] spec = {{S, 1, 1, OK, 0}, {F, 1, 1, OK, 0}, {S, 9, 0, UNUSED, 0}};
        final FeatureHistoryPresentation.Model model = FeatureHistoryPresentation.fromNative(
                FeatureHistoryPresentation.DOMAIN_CAD, header(0, 0, 0, true, 3), rows(spec), 3);
        assertTrue(model.rows.get(0).editable());
        assertTrue(model.rows.get(1).editable());
        assertFalse(model.rows.get(2).editable());
        assertEquals(FeatureHistoryPresentation.MARK_UNUSED, model.rows.get(2).mark());
        assertNull(model.failedRow());
    }

    @Test
    public void theIssueCardIsOwedOnlyForADownstreamFailure() {
        final FeatureHistoryPresentation.Model staged = stagedFailure();
        assertTrue(staged.downstreamFailure());
        assertTrue(FeatureHistoryPresentation.issueShown(staged, true, ""));
        // No edit session: nothing to fix or cancel.
        assertFalse(FeatureHistoryPresentation.issueShown(staged, false, ""));
        // Collapsed by Fix: stays collapsed while the same failure stands...
        assertFalse(FeatureHistoryPresentation.issueShown(staged, true, staged.issueKey()));
        // ...and returns when the failure changes.
        final long[][] spec = {{S, 1, 1, OK, 1}, {F, 1, 1, OK, 1}, {S, 2, 2, OK, 0},
                {F, 2, 2, FAILED, 0}};
        final FeatureHistoryPresentation.Model other = FeatureHistoryPresentation.fromNative(
                FeatureHistoryPresentation.DOMAIN_CAD, header(41, 2, 1, true, 4), rows(spec), 4);
        assertTrue(FeatureHistoryPresentation.issueShown(other, true, staged.issueKey()));
    }

    @Test
    public void theEditedFeatureFailingItselfIsNotADownstreamIssue() {
        final long[][] spec = {{S, 1, 1, OK, 1}, {F, 1, 1, FAILED, 1}};
        final FeatureHistoryPresentation.Model model = FeatureHistoryPresentation.fromNative(
                FeatureHistoryPresentation.DOMAIN_CAD, header(19, 1, 1, true, 2), rows(spec), 2);
        assertFalse(model.downstreamFailure());
    }

    @Test
    public void anUnevaluatedStagedChainOwesNoCardAndReadsPending() {
        final long[][] spec = {{S, 1, 1, OK, 1}, {F, 1, 1, PENDING, 1}, {S, 2, 2, OK, 0},
                {F, 2, 2, PENDING, 0}};
        final FeatureHistoryPresentation.Model model = FeatureHistoryPresentation.fromNative(
                FeatureHistoryPresentation.DOMAIN_CAD, header(23, 0, 1, false, 4), rows(spec), 4);
        assertFalse(model.downstreamFailure());
        assertEquals(FeatureHistoryPresentation.MARK_PENDING, model.rows.get(3).mark());
    }

    @Test
    public void theControlIsDrawnOnlyWhenItCanSucceed() {
        assertTrue(FeatureHistoryPresentation.controlShown(true, false, false, true));
        assertFalse(FeatureHistoryPresentation.controlShown(false, false, false, true));
        assertFalse(FeatureHistoryPresentation.controlShown(true, true, false, true));
        assertFalse(FeatureHistoryPresentation.controlShown(true, false, true, true));
        assertFalse(FeatureHistoryPresentation.controlShown(true, false, false, false));
    }

    @Test
    public void aNegativeCountOrShortArraysReadEmpty() {
        assertEquals(0, FeatureHistoryPresentation.fromNative(FeatureHistoryPresentation.DOMAIN_CAD,
                header(0, 0, 0, true, 0), new double[0], -1).rows.size());
        assertEquals(0, FeatureHistoryPresentation.fromNative(FeatureHistoryPresentation.DOMAIN_CAD,
                new double[2], new double[0], 3).rows.size());
        // A row array shorter than the count is read only as far as it goes.
        final long[][] spec = {{S, 1, 1, OK, 0}};
        assertEquals(1, FeatureHistoryPresentation.fromNative(FeatureHistoryPresentation.DOMAIN_CAD,
                header(0, 0, 0, true, 2), rows(spec), 2).rows.size());
    }
}
