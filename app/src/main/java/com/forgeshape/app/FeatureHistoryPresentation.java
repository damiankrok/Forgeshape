package com.forgeshape.app;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * What the Parametric History surface shows, decided from one native read
 * ({@code MODELING-FOUNDATIONS-R1} A).
 *
 * <p><b>Pure and Android-free</b>, so the decisions are pinned on the JVM: which
 * rows exist and in what order, which one failed, which ones a tap may open, the
 * glyph that marks each state, and when the regeneration issue card is owed.
 * The text is the view's business, because it needs resources and the display
 * unit.
 *
 * <p><b>Nothing here is truth.</b> The timeline is derived below JNI from the
 * body's own feature chain on every read; this class only reshapes that read and
 * remembers none of it. A row is named by its durable id — a sketch id or a
 * feature id — never by its position, which moves when an earlier sketch is
 * shared.
 */
final class FeatureHistoryPresentation {

    private FeatureHistoryPresentation() {}

    /** Filled: regenerated. */
    static final String MARK_OK = "●";
    /** A cross: the first feature that cannot be rebuilt. Shape, not colour alone. */
    static final String MARK_FAILED = "✕";
    /** Dotted: after the failure, so never evaluated. */
    static final String MARK_NOT_REGENERATED = "◌";
    /** Ellipsis: waiting for the staged sketch to be finished. */
    static final String MARK_PENDING = "…";
    /** Hollow: a retained sketch nothing builds from. */
    static final String MARK_UNUSED = "○";
    /** Prefixed to the row a staged edit changes. */
    static final String MARK_EDITING = "✎";

    /** Which family of timeline a row belongs to. */
    static final int DOMAIN_CAD = 0;
    static final int DOMAIN_SURFACE = 1;

    /** One row, exactly as native reported it. */
    static final class Row {
        final int domain;
        /** {@link NativeViewport#TIMELINE_KIND_SKETCH} or {@code _FEATURE}. */
        final int kind;
        final long id;
        final long sketchId;
        /** The feature a tap edits; 0 when the row is not editable. */
        final long editTarget;
        final int ordinal;
        final int state;
        final int status;
        final boolean editing;
        final int entities;
        final int plane;
        final boolean onBodyFace;
        final boolean onFeatureFace;
        final int featureKind;
        final int operation;
        final int extent;
        final int side;
        final double positive;
        final double negative;
        final double angle;
        final int revolveDirection;

        Row(int domain, double[] v, int at) {
            this.domain = domain;
            kind = (int) v[at + NativeViewport.TIMELINE_ROW_KIND];
            id = (long) v[at + NativeViewport.TIMELINE_ROW_ID];
            sketchId = (long) v[at + NativeViewport.TIMELINE_ROW_SKETCH];
            editTarget = (long) v[at + NativeViewport.TIMELINE_ROW_EDIT_FEATURE];
            ordinal = (int) v[at + NativeViewport.TIMELINE_ROW_ORDINAL];
            state = (int) v[at + NativeViewport.TIMELINE_ROW_STATE];
            status = (int) v[at + NativeViewport.TIMELINE_ROW_STATUS];
            editing = v[at + NativeViewport.TIMELINE_ROW_EDITING] != 0.0;
            entities = (int) v[at + NativeViewport.TIMELINE_ROW_ENTITIES];
            plane = (int) v[at + NativeViewport.TIMELINE_ROW_PLANE];
            onBodyFace = v[at + NativeViewport.TIMELINE_ROW_ON_BODY_FACE] != 0.0;
            onFeatureFace = v[at + NativeViewport.TIMELINE_ROW_ON_FEATURE_FACE] != 0.0;
            featureKind = (int) v[at + NativeViewport.TIMELINE_ROW_FEATURE_KIND];
            operation = (int) v[at + NativeViewport.TIMELINE_ROW_OPERATION];
            extent = (int) v[at + NativeViewport.TIMELINE_ROW_EXTENT];
            side = (int) v[at + NativeViewport.TIMELINE_ROW_SIDE];
            positive = v[at + NativeViewport.TIMELINE_ROW_POSITIVE];
            negative = v[at + NativeViewport.TIMELINE_ROW_NEGATIVE];
            angle = v[at + NativeViewport.TIMELINE_ROW_ANGLE];
            revolveDirection = (int) v[at + NativeViewport.TIMELINE_ROW_REVOLVE_DIRECTION];
        }

        boolean isSketch() {
            return kind == NativeViewport.TIMELINE_KIND_SKETCH;
        }

        boolean isFailed() {
            return state == NativeViewport.TIMELINE_STATE_FAILED;
        }

        /**
         * Whether a tap may open an editor for this row. A sketch no feature
         * consumes has nothing to rebuild from it, so it is listed and not
         * offered — a control that cannot succeed is not a control.
         */
        boolean editable() {
            return editTarget > 0L && state != NativeViewport.TIMELINE_STATE_UNUSED;
        }

        /** The state glyph, with the editing mark in front of it when staged. */
        String mark() {
            final String state;
            switch (this.state) {
                case NativeViewport.TIMELINE_STATE_FAILED: state = MARK_FAILED; break;
                case NativeViewport.TIMELINE_STATE_NOT_REGENERATED:
                    state = MARK_NOT_REGENERATED;
                    break;
                case NativeViewport.TIMELINE_STATE_PENDING: state = MARK_PENDING; break;
                case NativeViewport.TIMELINE_STATE_UNUSED: state = MARK_UNUSED; break;
                default: state = MARK_OK; break;
            }
            return editing ? MARK_EDITING + " " + state : state;
        }
    }

    /** One whole read: the verdict and the rows. */
    static final class Model {
        final int status;
        final long failedFeature;
        final long editingFeature;
        final boolean evaluated;
        final List<Row> rows;

        Model(int status, long failedFeature, long editingFeature, boolean evaluated, List<Row> rows) {
            this.status = status;
            this.failedFeature = failedFeature;
            this.editingFeature = editingFeature;
            this.evaluated = evaluated;
            this.rows = Collections.unmodifiableList(rows);
        }

        static Model empty() {
            return new Model(0, 0L, 0L, true, new ArrayList<Row>());
        }

        /** The failing row's index, or -1. At most one row fails. */
        int failedRowIndex() {
            for (int i = 0; i < rows.size(); i++) {
                if (rows.get(i).isFailed()) {
                    return i;
                }
            }
            return -1;
        }

        /** The failing row, or null. */
        Row failedRow() {
            final int index = failedRowIndex();
            return index < 0 ? null : rows.get(index);
        }

        /**
         * Whether a staged edit broke a LATER feature: the case the issue card
         * exists for. A refusal of the edited feature itself is already named
         * where its value is typed (the status line, the operation badge), so the
         * card is owed only when the failure is downstream of the edit.
         */
        boolean downstreamFailure() {
            return evaluated && editingFeature > 0L && failedFeature > 0L
                    && failedFeature != editingFeature && failedRow() != null;
        }

        /**
         * What identifies one particular failure, so a Fix that collapsed the
         * card keeps it collapsed until the failure CHANGES — another feature,
         * another reason — rather than until the next refresh.
         */
        String issueKey() {
            return editingFeature + ":" + failedFeature + ":" + status;
        }
    }

    /**
     * Reads one native timeline. {@code count} is what the native call returned;
     * a negative count (not a CAD body) is an empty model.
     */
    static Model fromNative(int domain, double[] header, double[] rows, int count) {
        if (count < 0 || header == null || header.length < NativeViewport.TIMELINE_HEADER_SIZE) {
            return Model.empty();
        }
        final int usable = rows == null ? 0
                : Math.min(count, rows.length / NativeViewport.TIMELINE_ROW_SIZE);
        final List<Row> list = new ArrayList<>(usable);
        for (int i = 0; i < usable; i++) {
            list.add(new Row(domain, rows, i * NativeViewport.TIMELINE_ROW_SIZE));
        }
        return new Model((int) header[NativeViewport.TIMELINE_STATUS],
                (long) header[NativeViewport.TIMELINE_FAILED_FEATURE],
                (long) header[NativeViewport.TIMELINE_EDITING_FEATURE],
                header[NativeViewport.TIMELINE_EVALUATED] != 0.0, list);
    }

    /**
     * Whether the History control is drawn: a project is open, nothing is being
     * sketched or sculpted, and the active body HAS a history to list.
     */
    static boolean controlShown(boolean projectOpen, boolean sketching, boolean sculpting,
                                boolean bodyHasHistory) {
        return projectOpen && !sketching && !sculpting && bodyHasHistory;
    }

    /**
     * Whether the regeneration issue card is drawn: an edit session over this
     * body, a downstream failure, and not the failure the user already chose to
     * fix (collapsed with Fix).
     */
    static boolean issueShown(Model staged, boolean editSession, String dismissedKey) {
        return editSession && staged.downstreamFailure()
                && !staged.issueKey().equals(dismissedKey);
    }
}
