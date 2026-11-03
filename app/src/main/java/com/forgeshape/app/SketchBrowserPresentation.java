package com.forgeshape.app;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * What the Sketches and Features segments of the History surface show
 * ({@code CAD-R2}), decided from native reads.
 *
 * <p><b>Pure and Android-free</b>, so every decision is pinned on the JVM:
 * which sketches exist and in what order, which are used and by how many
 * features, which acts a row offers, which segment a body has, and how an
 * action strip wraps so no target falls under 48 dp. Text is
 * {@link SketchBrowserText}'s business, because it needs resources.
 *
 * <p><b>Nothing here is truth.</b> The inventory is derived below JNI from the
 * body's sketch table on every read; this class reshapes one read and keeps
 * none of it. The only thing the surface remembers is WHICH row the user
 * selected, by durable id, and {@link #reconcile} drops that id the moment a
 * read no longer carries it — a selection never survives as a second copy of
 * a list that moved.
 *
 * <p><b>Native decides every act.</b> A row offers an act exactly when the
 * native {@code acts} bit says it would be accepted; a control that cannot
 * succeed is not drawn, and the reason it is absent is native's too.
 */
final class SketchBrowserPresentation {

    private SketchBrowserPresentation() {}

    /** The History surface's three views of one body. */
    static final int SEGMENT_STEPS = 0;
    static final int SEGMENT_SKETCHES = 1;
    static final int SEGMENT_FEATURES = 2;

    /** The acts a selected row can offer, in the order they are drawn. */
    static final int ACT_EDIT_SKETCH = 1;
    static final int ACT_EDIT_FEATURE = 2;
    static final int ACT_REUSE_ADD = 3;
    static final int ACT_REUSE_CUT = 4;
    static final int ACT_DELETE_SKETCH = 5;

    /** How a sketch is used. */
    static final int USAGE_UNUSED = 0;
    static final int USAGE_SINGLE = 1;
    static final int USAGE_SHARED = 2;

    /** One retained sketch, exactly as native reported it. */
    static final class SketchRow {
        final long id;
        final int ordinal;
        final boolean root;
        final int support;
        final int plane;
        final long supportFeature;
        final int entities;
        final int dimensions;
        final int consumers;
        final long firstConsumer;
        final long lastConsumer;
        final int state;
        final int status;
        final int acts;
        final int deleteRefusal;
        final int reuseRefusal;

        SketchRow(double[] v, int at) {
            id = (long) v[at + NativeViewport.SKETCH_INV_ID];
            ordinal = (int) v[at + NativeViewport.SKETCH_INV_ORDINAL];
            root = v[at + NativeViewport.SKETCH_INV_ROOT] != 0.0;
            support = (int) v[at + NativeViewport.SKETCH_INV_SUPPORT];
            plane = (int) v[at + NativeViewport.SKETCH_INV_PLANE];
            supportFeature = (long) v[at + NativeViewport.SKETCH_INV_SUPPORT_FEATURE];
            entities = (int) v[at + NativeViewport.SKETCH_INV_ENTITIES];
            dimensions = (int) v[at + NativeViewport.SKETCH_INV_DIMENSIONS];
            consumers = (int) v[at + NativeViewport.SKETCH_INV_CONSUMERS];
            firstConsumer = (long) v[at + NativeViewport.SKETCH_INV_FIRST_CONSUMER];
            lastConsumer = (long) v[at + NativeViewport.SKETCH_INV_LAST_CONSUMER];
            state = (int) v[at + NativeViewport.SKETCH_INV_STATE];
            status = (int) v[at + NativeViewport.SKETCH_INV_STATUS_CODE];
            acts = (int) v[at + NativeViewport.SKETCH_INV_ACTS];
            deleteRefusal = (int) v[at + NativeViewport.SKETCH_INV_DELETE_REFUSAL];
            reuseRefusal = (int) v[at + NativeViewport.SKETCH_INV_REUSE_REFUSAL];
        }

        boolean offers(int nativeAct) {
            return (acts & nativeAct) != 0;
        }

        int usage() {
            return consumers == 0 ? USAGE_UNUSED : consumers == 1 ? USAGE_SINGLE : USAGE_SHARED;
        }

        boolean unused() {
            return consumers == 0;
        }

        /** The glyph a row leads with: the timeline's own state marks. */
        String mark() {
            switch (state) {
                case NativeViewport.SKETCH_ROW_FAILED: return FeatureHistoryPresentation.MARK_FAILED;
                case NativeViewport.SKETCH_ROW_NOT_REGENERATED:
                    return FeatureHistoryPresentation.MARK_NOT_REGENERATED;
                case NativeViewport.SKETCH_ROW_SUPPORT_LOST:
                    return FeatureHistoryPresentation.MARK_FAILED;
                case NativeViewport.SKETCH_ROW_UNUSED: return FeatureHistoryPresentation.MARK_UNUSED;
                default: return FeatureHistoryPresentation.MARK_OK;
            }
        }
    }

    /** One whole inventory read. */
    static final class Inventory {
        final int status;
        final long failedFeature;
        final List<SketchRow> rows;
        /** Per sketch id, the consumer feature ids in chain order (native's read). */
        private final Map<Long, long[]> consumers;

        Inventory(int status, long failedFeature, List<SketchRow> rows,
                  Map<Long, long[]> consumers) {
            this.status = status;
            this.failedFeature = failedFeature;
            this.rows = Collections.unmodifiableList(rows);
            this.consumers = consumers == null ? new HashMap<Long, long[]>() : consumers;
        }

        static Inventory empty() {
            return new Inventory(NativeViewport.CAD_OK, 0L, new ArrayList<SketchRow>(), null);
        }

        /** The features extruding `sketchId`, in chain order; empty when none. */
        long[] consumersOf(long sketchId) {
            final long[] ids = consumers.get(sketchId);
            return ids == null ? new long[0] : ids.clone();
        }

        SketchRow find(long sketchId) {
            for (SketchRow row : rows) {
                if (row.id == sketchId) {
                    return row;
                }
            }
            return null;
        }

        int unusedCount() {
            int n = 0;
            for (SketchRow row : rows) {
                if (row.unused()) {
                    n++;
                }
            }
            return n;
        }
    }

    /** Reads one native inventory; a negative count (not a CAD body) is empty. */
    static Inventory fromNative(double[] header, double[] rows, int count) {
        return fromNative(header, rows, count, null);
    }

    /** The same, with each sketch's consumer list as native reported it. */
    static Inventory fromNative(double[] header, double[] rows, int count,
                                Map<Long, long[]> consumers) {
        if (count < 0 || header == null || header.length < NativeViewport.SKETCH_INV_HEADER_SIZE) {
            return Inventory.empty();
        }
        final int usable = rows == null ? 0
                : Math.min(count, rows.length / NativeViewport.SKETCH_INV_ROW_SIZE);
        final List<SketchRow> list = new ArrayList<>(usable);
        for (int i = 0; i < usable; i++) {
            list.add(new SketchRow(rows, i * NativeViewport.SKETCH_INV_ROW_SIZE));
        }
        return new Inventory((int) header[NativeViewport.SKETCH_INV_STATUS],
                (long) header[NativeViewport.SKETCH_INV_FAILED_FEATURE], list, consumers);
    }

    /**
     * The acts a selected sketch offers, in drawing order: Edit Sketch, Use for
     * Add, Use for Cut, Delete Sketch — each exactly when native accepts it.
     */
    static List<Integer> sketchActs(SketchRow row) {
        final List<Integer> acts = new ArrayList<>();
        if (row == null) {
            return acts;
        }
        if (row.offers(NativeViewport.SKETCH_ACT_EDIT)) {
            acts.add(ACT_EDIT_SKETCH);
        }
        if (row.offers(NativeViewport.SKETCH_ACT_REUSE_ADD)) {
            acts.add(ACT_REUSE_ADD);
        }
        if (row.offers(NativeViewport.SKETCH_ACT_REUSE_CUT)) {
            acts.add(ACT_REUSE_CUT);
        }
        if (row.offers(NativeViewport.SKETCH_ACT_DELETE)) {
            acts.add(ACT_DELETE_SKETCH);
        }
        return acts;
    }

    /**
     * The acts a selected FEATURE offers: Edit Feature and Edit Sketch, when
     * the row may be opened at all. A feature row's own id is what Edit Feature
     * opens, its sketch id what Edit Sketch opens — never "the first" or "the
     * last selected".
     */
    static List<Integer> featureActs(FeatureHistoryPresentation.Row row) {
        final List<Integer> acts = new ArrayList<>();
        if (row == null || row.isSketch()
                || row.domain != FeatureHistoryPresentation.DOMAIN_CAD) {
            return acts;
        }
        if (row.editable()) {
            acts.add(ACT_EDIT_FEATURE);
            if (row.sketchId > 0L) {
                acts.add(ACT_EDIT_SKETCH);
            }
        }
        return acts;
    }

    /** The timeline FEATURE row with durable id `featureId`, or null. */
    static FeatureHistoryPresentation.Row featureRow(FeatureHistoryPresentation.Model model,
                                                     long featureId) {
        for (FeatureHistoryPresentation.Row row : model.rows) {
            if (!row.isSketch() && row.id == featureId) {
                return row;
            }
        }
        return null;
    }

    /** The FEATURE rows of a timeline, in chain order. */
    static List<FeatureHistoryPresentation.Row> features(FeatureHistoryPresentation.Model model) {
        final List<FeatureHistoryPresentation.Row> out = new ArrayList<>();
        for (FeatureHistoryPresentation.Row row : model.rows) {
            if (!row.isSketch()) {
                out.add(row);
            }
        }
        return out;
    }

    /** Whether a body offers the Sketches and Features segments at all. */
    static boolean segmentsShown(boolean cadBody) {
        return cadBody;
    }

    /** A segment the surface can show for this body; Steps otherwise. */
    static int segmentFor(int requested, boolean cadBody) {
        if (!cadBody) {
            return SEGMENT_STEPS;
        }
        return requested == SEGMENT_SKETCHES || requested == SEGMENT_FEATURES ? requested
                : SEGMENT_STEPS;
    }

    /**
     * The selection that survives a fresh read: the same durable id if the
     * read still carries it, 0 otherwise. Never the "nearest" row.
     */
    static long reconcileSketch(Inventory inventory, long selectedId) {
        return selectedId > 0L && inventory.find(selectedId) != null ? selectedId : 0L;
    }

    static long reconcileFeature(FeatureHistoryPresentation.Model model, long selectedId) {
        if (selectedId <= 0L) {
            return 0L;
        }
        for (FeatureHistoryPresentation.Row row : model.rows) {
            if (!row.isSketch() && row.id == selectedId) {
                return selectedId;
            }
        }
        return 0L;
    }

    /**
     * How many action targets fit on one line of the strip so none is narrower
     * than {@code minTargetPx} (48 dp) and none overlaps another: the strip
     * wraps rather than shrinks. At least one per line, at most {@code count}.
     */
    static int actionColumns(int availableWidthPx, int minTargetPx, int gapPx, int count) {
        if (count <= 0) {
            return 0;
        }
        if (minTargetPx <= 0 || availableWidthPx < minTargetPx) {
            return 1;
        }
        final int columns = (availableWidthPx + gapPx) / (minTargetPx + gapPx);
        return Math.max(1, Math.min(count, columns));
    }

    /** How many lines {@code count} targets take at {@code columns} per line. */
    static int actionLines(int count, int columns) {
        return columns <= 0 ? 0 : (count + columns - 1) / columns;
    }
}
