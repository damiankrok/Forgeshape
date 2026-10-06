package com.forgeshape.app;

import android.content.Context;

/**
 * The words a Parametric History row is read with: its name ("Sketch 1",
 * "Extrude 2 · Cut", "Revolve 1") and its one-line detail (where a sketch stands,
 * how far an extrusion reaches, the angle of a revolve — or, for a row that is
 * not regenerated, why).
 *
 * <p>Separate from {@link FeatureHistoryPresentation} because it needs
 * resources and the display unit; every decision about WHICH row says what is
 * made there, on the JVM.
 */
final class FeatureHistoryText {

    private FeatureHistoryText() {}

    static String name(Context context, FeatureHistoryPresentation.Row row) {
        if (row.isSketch()) {
            return context.getString(R.string.history_row_sketch, row.ordinal);
        }
        if (row.domain == FeatureHistoryPresentation.DOMAIN_SURFACE) {
            // A Surface feature is named by its kind and its own id, the way
            // the Surface surface's Thicken buttons and the issue text name it.
            return context.getString(R.string.history_row_surface,
                    context.getString(SurfacePresentation.kindName(row.featureKind)), row.id);
        }
        if (row.featureKind == NativeViewport.FEATURE_KIND_REVOLVE) {
            return context.getString(R.string.history_row_revolve, row.ordinal);
        }
        return context.getString(R.string.history_row_extrude, row.ordinal,
                context.getString(CadFeatureEditorView.operationName(row.operation)));
    }

    /** What the row's values are, or why it is not regenerated. */
    static String detail(Context context, LengthUnit unit, FeatureHistoryPresentation.Row row) {
        switch (row.state) {
            case NativeViewport.TIMELINE_STATE_FAILED:
                return context.getString(R.string.history_state_failed,
                        row.domain == FeatureHistoryPresentation.DOMAIN_SURFACE
                                ? SurfaceEditorView.refusal(context, row.status)
                                : CadStatusMessages.describe(context, row.status));
            case NativeViewport.TIMELINE_STATE_NOT_REGENERATED:
                return context.getString(R.string.history_state_not_rebuilt);
            case NativeViewport.TIMELINE_STATE_PENDING:
                return context.getString(R.string.history_state_pending);
            case NativeViewport.TIMELINE_STATE_UNUSED:
                return context.getString(R.string.history_state_unused);
            default:
                break;
        }
        if (row.domain == FeatureHistoryPresentation.DOMAIN_SURFACE) {
            return surfaceDetail(context, unit, row);
        }
        if (row.isSketch()) {
            if (row.onBodyFace || row.onFeatureFace) {
                return context.getString(R.string.history_detail_sketch_face, row.entities);
            }
            return context.getString(R.string.history_detail_sketch_plane,
                    context.getString(CadFeatureEditorView.planeName(row.plane)), row.entities);
        }
        if (row.featureKind == NativeViewport.FEATURE_KIND_REVOLVE) {
            return context.getString(R.string.history_detail_revolve,
                    CadRevolvePresentation.label(row.angle),
                    context.getString(row.revolveDirection == NativeViewport.REVOLVE_NEGATIVE
                            ? R.string.history_revolve_negative
                            : R.string.history_revolve_positive));
        }
        switch (row.extent) {
            case NativeViewport.EXTENT_SYMMETRIC:
                return context.getString(R.string.history_detail_symmetric,
                        unit.formatWithUnit(row.positive));
            case NativeViewport.EXTENT_TWO_SIDES:
                return context.getString(R.string.history_detail_two_sides,
                        unit.formatWithUnit(row.positive), unit.formatWithUnit(row.negative));
            default:
                return context.getString(R.string.history_detail_one_side,
                        unit.formatWithUnit(Math.max(row.positive, row.negative)));
        }
    }

    /** A Surface row: a sketch's plane, offset and size, or a feature's one value. */
    private static String surfaceDetail(Context context, LengthUnit unit, FeatureHistoryPresentation.Row row) {
        if (row.isSketch()) {
            return context.getString(R.string.history_detail_surface_sketch,
                    context.getString(CadFeatureEditorView.planeName(row.plane)),
                    unit.formatWithUnit(row.positive), row.entities);
        }
        switch (SurfacePresentation.rowValue(row)) {
            case SurfacePresentation.VALUE_ANGLE:
                return context.getString(R.string.history_detail_surface_angle,
                        LengthUnit.present(java.math.BigDecimal.valueOf(row.positive)));
            case SurfacePresentation.VALUE_LENGTH:
                return unit.formatWithUnit(row.positive);
            default:
                return context.getString(R.string.history_detail_surface_plain);
        }
    }
}
