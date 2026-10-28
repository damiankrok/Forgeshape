package com.forgeshape.app;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The Sketch Browser and the feature action menu, on the JVM ({@code CAD-R2}):
 * rows in native's order by durable id, used / unused / shared, the acts each
 * row offers exactly as native's bits say, selection kept by id and dropped
 * when the id is gone, segments per body kind, and the action strip's wrap
 * so no target is narrower than 48 dp (case ids {@code R2-JVM-01..}).
 */
public class SketchBrowserPresentationTest {

    private static final int ALL = NativeViewport.SKETCH_ACT_EDIT | NativeViewport.SKETCH_ACT_DELETE
            | NativeViewport.SKETCH_ACT_REUSE_ADD | NativeViewport.SKETCH_ACT_REUSE_CUT;

    private static double[] header(int status, int rows, long next, long failed) {
        final double[] h = new double[NativeViewport.SKETCH_INV_HEADER_SIZE];
        h[NativeViewport.SKETCH_INV_STATUS] = status;
        h[NativeViewport.SKETCH_INV_ROW_COUNT] = rows;
        h[NativeViewport.SKETCH_INV_NEXT_ID] = next;
        h[NativeViewport.SKETCH_INV_FAILED_FEATURE] = failed;
        return h;
    }

    /** Rows: [id, ordinal, root, support, consumers, first, last, state, acts, deleteWhy]. */
    private static double[] rows(long[][] spec) {
        final double[] v = new double[spec.length * NativeViewport.SKETCH_INV_ROW_SIZE];
        for (int i = 0; i < spec.length; i++) {
            final int at = i * NativeViewport.SKETCH_INV_ROW_SIZE;
            v[at + NativeViewport.SKETCH_INV_ID] = spec[i][0];
            v[at + NativeViewport.SKETCH_INV_ORDINAL] = spec[i][1];
            v[at + NativeViewport.SKETCH_INV_ROOT] = spec[i][2];
            v[at + NativeViewport.SKETCH_INV_SUPPORT] = spec[i][3];
            v[at + NativeViewport.SKETCH_INV_CONSUMERS] = spec[i][4];
            v[at + NativeViewport.SKETCH_INV_FIRST_CONSUMER] = spec[i][5];
            v[at + NativeViewport.SKETCH_INV_LAST_CONSUMER] = spec[i][6];
            v[at + NativeViewport.SKETCH_INV_STATE] = spec[i][7];
            v[at + NativeViewport.SKETCH_INV_ACTS] = spec[i][8];
            v[at + NativeViewport.SKETCH_INV_DELETE_REFUSAL] = spec[i][9];
            v[at + NativeViewport.SKETCH_INV_ENTITIES] = 4;
        }
        return v;
    }

    private static final int OK = NativeViewport.SKETCH_ROW_OK;
    private static final int UNUSED = NativeViewport.SKETCH_ROW_UNUSED;
    private static final int WORLD = NativeViewport.SKETCH_SUPPORT_WORLD_PLANE;
    private static final int FACE = NativeViewport.SKETCH_SUPPORT_FEATURE_FACE;
    private static final int EDIT_REUSE = NativeViewport.SKETCH_ACT_EDIT
            | NativeViewport.SKETCH_ACT_REUSE_ADD | NativeViewport.SKETCH_ACT_REUSE_CUT;

    /**
     * The shop body after Keep Sketch and a reuse: S1 root (base), S2 (Add 2),
     * S3 (Cut 3), S4 shared by Add 4 and Cut 5, S6 unused (S5 was deleted).
     */
    private static SketchBrowserPresentation.Inventory shop() {
        final long[][] spec = {
                {1, 1, 1, WORLD, 1, 1, 1, OK, EDIT_REUSE, NativeViewport.CAD_SKETCH_IS_ROOT},
                {2, 2, 0, FACE, 1, 2, 2, OK, EDIT_REUSE, NativeViewport.CAD_SKETCH_IN_USE},
                {3, 3, 0, FACE, 1, 3, 3, OK, EDIT_REUSE, NativeViewport.CAD_SKETCH_IN_USE},
                {4, 4, 0, FACE, 2, 4, 5, OK, EDIT_REUSE, NativeViewport.CAD_SKETCH_IN_USE},
                {6, 5, 0, FACE, 0, 0, 0, UNUSED, ALL, NativeViewport.CAD_OK}};
        final Map<Long, long[]> consumers = new HashMap<>();
        consumers.put(1L, new long[] {1});
        consumers.put(2L, new long[] {2});
        consumers.put(3L, new long[] {3});
        consumers.put(4L, new long[] {4, 5});
        return SketchBrowserPresentation.fromNative(header(NativeViewport.CAD_OK, spec.length, 7, 0),
                rows(spec), spec.length, consumers);
    }

    private static FeatureHistoryPresentation.Model timeline() {
        final int s = NativeViewport.TIMELINE_KIND_SKETCH;
        final int f = NativeViewport.TIMELINE_KIND_FEATURE;
        final long[][] spec = {
                {s, 1, 1, 1}, {f, 1, 1, 1}, {s, 2, 2, 2}, {f, 2, 2, 2}, {s, 3, 3, 3}, {f, 3, 3, 3},
                {s, 4, 4, 4}, {f, 4, 4, 4}, {f, 5, 5, 4}, {s, 6, 0, 6}};
        final double[] v = new double[spec.length * NativeViewport.TIMELINE_ROW_SIZE];
        for (int i = 0; i < spec.length; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            v[at + NativeViewport.TIMELINE_ROW_KIND] = spec[i][0];
            v[at + NativeViewport.TIMELINE_ROW_ID] = spec[i][1];
            v[at + NativeViewport.TIMELINE_ROW_EDIT_FEATURE] = spec[i][2];
            v[at + NativeViewport.TIMELINE_ROW_SKETCH] = spec[i][3];
            v[at + NativeViewport.TIMELINE_ROW_STATE] = spec[i][2] == 0
                    ? NativeViewport.TIMELINE_STATE_UNUSED : NativeViewport.TIMELINE_STATE_OK;
        }
        final double[] h = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        h[NativeViewport.TIMELINE_EVALUATED] = 1.0;
        h[NativeViewport.TIMELINE_ROW_COUNT] = spec.length;
        return FeatureHistoryPresentation.fromNative(FeatureHistoryPresentation.DOMAIN_CAD, h, v,
                spec.length);
    }

    // R2-JVM-01
    @Test
    public void rowsKeepNativeOrderAndDurableIdsNotPositions() {
        final SketchBrowserPresentation.Inventory inventory = shop();
        assertEquals(5, inventory.rows.size());
        assertEquals(1L, inventory.rows.get(0).id);
        assertEquals(6L, inventory.rows.get(4).id);
        assertEquals("the ordinal is presentation, the id identity", 5,
                inventory.rows.get(4).ordinal);
        assertNotNull(inventory.find(6L));
        assertNull("a deleted id is not found", inventory.find(5L));
    }

    // R2-JVM-02
    @Test
    public void usageIsUnusedSingleOrShared() {
        final SketchBrowserPresentation.Inventory inventory = shop();
        assertEquals(SketchBrowserPresentation.USAGE_SINGLE, inventory.find(2L).usage());
        assertEquals(SketchBrowserPresentation.USAGE_SHARED, inventory.find(4L).usage());
        assertEquals(SketchBrowserPresentation.USAGE_UNUSED, inventory.find(6L).usage());
        assertEquals(1, inventory.unusedCount());
    }

    // R2-JVM-03
    @Test
    public void consumersAreNativesListInChainOrderAndACopy() {
        final SketchBrowserPresentation.Inventory inventory = shop();
        final long[] shared = inventory.consumersOf(4L);
        assertArrayEquals(new long[] {4, 5}, shared);
        shared[0] = 99;
        assertArrayEquals("a caller cannot edit the read", new long[] {4, 5},
                inventory.consumersOf(4L));
        assertEquals(0, inventory.consumersOf(6L).length);
    }

    // R2-JVM-04
    @Test
    public void anUnusedSketchOffersEveryActInDrawingOrder() {
        assertEquals(Arrays.asList(SketchBrowserPresentation.ACT_EDIT_SKETCH,
                        SketchBrowserPresentation.ACT_REUSE_ADD, SketchBrowserPresentation.ACT_REUSE_CUT,
                        SketchBrowserPresentation.ACT_DELETE_SKETCH),
                SketchBrowserPresentation.sketchActs(shop().find(6L)));
    }

    // R2-JVM-05
    @Test
    public void aRootOrConsumedSketchOffersNoDeleteAndKeepsNativesReason() {
        final SketchBrowserPresentation.SketchRow root = shop().find(1L);
        final SketchBrowserPresentation.SketchRow used = shop().find(4L);
        assertFalse(SketchBrowserPresentation.sketchActs(root)
                .contains(SketchBrowserPresentation.ACT_DELETE_SKETCH));
        assertFalse(SketchBrowserPresentation.sketchActs(used)
                .contains(SketchBrowserPresentation.ACT_DELETE_SKETCH));
        assertEquals(NativeViewport.CAD_SKETCH_IS_ROOT, root.deleteRefusal);
        assertEquals(NativeViewport.CAD_SKETCH_IN_USE, used.deleteRefusal);
    }

    // R2-JVM-06
    @Test
    public void actsFollowNativeBitsAndNothingElse() {
        final long[][] spec = {{9, 1, 0, FACE, 0, 0, 0, NativeViewport.SKETCH_ROW_SUPPORT_LOST,
                NativeViewport.SKETCH_ACT_DELETE, NativeViewport.CAD_OK}};
        final SketchBrowserPresentation.Inventory lost = SketchBrowserPresentation.fromNative(
                header(NativeViewport.CAD_OK, 1, 10, 0), rows(spec), 1);
        assertEquals("a lost support offers Delete alone, because native says so",
                Arrays.asList(SketchBrowserPresentation.ACT_DELETE_SKETCH),
                SketchBrowserPresentation.sketchActs(lost.find(9L)));
        assertEquals(FeatureHistoryPresentation.MARK_FAILED, lost.find(9L).mark());
        assertTrue(SketchBrowserPresentation.sketchActs(null).isEmpty());
    }

    // R2-JVM-07
    @Test
    public void featureActsOpenTheExactFeatureAndItsSketch() {
        final FeatureHistoryPresentation.Model model = timeline();
        final FeatureHistoryPresentation.Row cut5 = SketchBrowserPresentation.featureRow(model, 5L);
        assertNotNull(cut5);
        assertEquals(4L, cut5.sketchId);
        assertEquals(Arrays.asList(SketchBrowserPresentation.ACT_EDIT_FEATURE,
                SketchBrowserPresentation.ACT_EDIT_SKETCH), SketchBrowserPresentation.featureActs(cut5));
        assertTrue("a sketch row is not a feature", SketchBrowserPresentation.featureActs(
                model.rows.get(0)).isEmpty());
    }

    // R2-JVM-08
    @Test
    public void featuresListsOnlyFeatureRowsInChainOrder() {
        final List<FeatureHistoryPresentation.Row> features =
                SketchBrowserPresentation.features(timeline());
        assertEquals(5, features.size());
        for (int i = 0; i < features.size(); i++) {
            assertEquals(i + 1L, features.get(i).id);
            assertFalse(features.get(i).isSketch());
        }
    }

    // R2-JVM-09
    @Test
    public void aSelectionSurvivesARereadOnlyByItsOwnId() {
        final SketchBrowserPresentation.Inventory inventory = shop();
        assertEquals(6L, SketchBrowserPresentation.reconcileSketch(inventory, 6L));
        assertEquals("a deleted sketch's selection is dropped, never moved to a neighbour", 0L,
                SketchBrowserPresentation.reconcileSketch(inventory, 5L));
        assertEquals(0L, SketchBrowserPresentation.reconcileSketch(inventory, 0L));
        assertEquals(5L, SketchBrowserPresentation.reconcileFeature(timeline(), 5L));
        assertEquals(0L, SketchBrowserPresentation.reconcileFeature(timeline(), 7L));
    }

    // R2-JVM-10
    @Test
    public void repeatedReadsAreIdempotent() {
        final SketchBrowserPresentation.Inventory a = shop();
        final SketchBrowserPresentation.Inventory b = shop();
        assertEquals(a.rows.size(), b.rows.size());
        for (int i = 0; i < a.rows.size(); i++) {
            assertEquals(a.rows.get(i).id, b.rows.get(i).id);
            assertEquals(SketchBrowserPresentation.sketchActs(a.rows.get(i)),
                    SketchBrowserPresentation.sketchActs(b.rows.get(i)));
        }
    }

    // R2-JVM-11
    @Test
    public void segmentsExistForACadBodyAlone() {
        assertTrue(SketchBrowserPresentation.segmentsShown(true));
        assertFalse(SketchBrowserPresentation.segmentsShown(false));
        assertEquals(SketchBrowserPresentation.SEGMENT_STEPS, SketchBrowserPresentation.segmentFor(
                SketchBrowserPresentation.SEGMENT_SKETCHES, false));
        assertEquals(SketchBrowserPresentation.SEGMENT_SKETCHES, SketchBrowserPresentation.segmentFor(
                SketchBrowserPresentation.SEGMENT_SKETCHES, true));
        assertEquals(SketchBrowserPresentation.SEGMENT_STEPS,
                SketchBrowserPresentation.segmentFor(42, true));
    }

    // R2-JVM-12
    @Test
    public void aNegativeCountOrAShortHeaderIsEmptyNotACrash() {
        assertTrue(SketchBrowserPresentation.fromNative(header(0, 0, 0, 0), new double[0], -1)
                .rows.isEmpty());
        assertTrue(SketchBrowserPresentation.fromNative(new double[1], new double[40], 2)
                .rows.isEmpty());
        // A count larger than the array holds reads only what is there.
        final long[][] spec = {{1, 1, 1, WORLD, 1, 1, 1, OK, EDIT_REUSE, 0}};
        assertEquals(1, SketchBrowserPresentation.fromNative(header(0, 5, 2, 0), rows(spec), 5)
                .rows.size());
    }

    // R2-JVM-13
    @Test
    public void theActionStripWrapsRatherThanShrinkingATarget() {
        final int dp48 = 48;
        // The History surface: 236 dp usable, targets of 96 dp, 4 dp gaps -> 2 per line.
        assertEquals(2, SketchBrowserPresentation.actionColumns(236, 96, 4, 4));
        assertEquals(2, SketchBrowserPresentation.actionLines(4, 2));
        // A narrow window: one per line, never a target below its floor.
        assertEquals(1, SketchBrowserPresentation.actionColumns(100, 96, 4, 4));
        assertEquals(4, SketchBrowserPresentation.actionLines(4, 1));
        // Even narrower than one target: still one, never zero.
        assertEquals(1, SketchBrowserPresentation.actionColumns(30, dp48, 4, 3));
        // Wide: never more columns than acts.
        assertEquals(3, SketchBrowserPresentation.actionColumns(1200, dp48, 4, 3));
        assertEquals(0, SketchBrowserPresentation.actionColumns(1200, dp48, 4, 0));
    }

    // R2-JVM-14
    @Test
    public void everyLineOfTheStripKeepsEachTargetAtLeastTheFloor() {
        for (int width = 48; width <= 900; width += 7) {
            for (int count = 1; count <= 5; count++) {
                final int columns = SketchBrowserPresentation.actionColumns(width, 48, 4, count);
                assertTrue(columns >= 1 && columns <= count);
                if (width >= 48) {
                    final int each = (width - (columns - 1) * 4) / columns;
                    assertTrue("width " + width + " count " + count + " -> " + each,
                            each >= 48);
                }
            }
        }
    }

    // R2-JVM-15
    @Test
    public void marksAreTheTimelinesOwnShapes() {
        final SketchBrowserPresentation.Inventory inventory = shop();
        assertEquals(FeatureHistoryPresentation.MARK_OK, inventory.find(1L).mark());
        assertEquals(FeatureHistoryPresentation.MARK_UNUSED, inventory.find(6L).mark());
    }

    // R2-JVM-16
    @Test
    public void statusCodesAreAppendedNeverRenumbered() {
        assertEquals(90, NativeViewport.CAD_MIRROR_NOTHING_SELECTED);
        assertEquals(91, NativeViewport.CAD_SKETCH_IN_USE);
        assertEquals(92, NativeViewport.CAD_SKETCH_IS_ROOT);
    }

    // R2-JVM-17
    @Test
    public void theRowLayoutIsTheNativeOneSlotForSlot() {
        assertEquals(20, NativeViewport.SKETCH_INV_ROW_SIZE);
        assertEquals(4, NativeViewport.SKETCH_INV_HEADER_SIZE);
        assertTrue(NativeViewport.SKETCH_INV_REUSE_REFUSAL < NativeViewport.SKETCH_INV_ROW_SIZE);
        assertEquals(16, NativeViewport.SKETCH_INV_MAX_ROWS);
    }
}
