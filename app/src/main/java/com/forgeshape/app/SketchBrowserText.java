package com.forgeshape.app;

import android.content.Context;

/**
 * The words the Sketches and Features segments are read with ({@code CAD-R2}):
 * a sketch's name, where it stands, how it is used, its dependencies, and why
 * an act it does not offer is absent.
 *
 * <p>Separate from {@link SketchBrowserPresentation} because it needs
 * resources; every decision about WHICH row offers what is made there, on the
 * JVM, and every reason is native's ({@link CadStatusMessages}).
 */
final class SketchBrowserText {

    private SketchBrowserText() {}

    /** "Sketch 2" — the History's own ordinal for the same durable id. */
    static String name(Context context, SketchBrowserPresentation.SketchRow row) {
        return context.getString(R.string.history_row_sketch, row.ordinal);
    }

    /** Where a sketch stands. */
    static String where(Context context, SketchBrowserPresentation.SketchRow row,
                        FeatureHistoryPresentation.Model timeline) {
        switch (row.support) {
            case NativeViewport.SKETCH_SUPPORT_FEATURE_FACE: {
                final FeatureHistoryPresentation.Row feature =
                        SketchBrowserPresentation.featureRow(timeline, row.supportFeature);
                return context.getString(R.string.sketch_where_feature, feature == null
                        ? String.valueOf(row.supportFeature)
                        : FeatureHistoryText.name(context, feature));
            }
            case NativeViewport.SKETCH_SUPPORT_BODY_FACE:
                return context.getString(R.string.sketch_where_body_face);
            default:
                return context.getString(R.string.sketch_where_plane,
                        context.getString(CadFeatureEditorView.planeName(row.plane)));
        }
    }

    /** How a sketch is used: Unused, Used by Extrude 2 · Cut, Shared by 3 features. */
    static String usage(Context context, SketchBrowserPresentation.SketchRow row,
                        FeatureHistoryPresentation.Model timeline) {
        switch (row.usage()) {
            case SketchBrowserPresentation.USAGE_UNUSED:
                return context.getString(R.string.sketch_usage_unused);
            case SketchBrowserPresentation.USAGE_SINGLE: {
                final FeatureHistoryPresentation.Row feature =
                        SketchBrowserPresentation.featureRow(timeline, row.firstConsumer);
                return context.getString(R.string.sketch_usage_single, feature == null
                        ? String.valueOf(row.firstConsumer)
                        : FeatureHistoryText.name(context, feature));
            }
            default:
                return context.getString(R.string.sketch_usage_shared, row.consumers);
        }
    }

    /** A sketch row's text: mark and name, then usage · where · entities. */
    static String rowText(Context context, SketchBrowserPresentation.SketchRow row,
                          FeatureHistoryPresentation.Model timeline) {
        return row.mark() + "  " + name(context, row) + "\n" + context.getString(
                R.string.sketch_browser_detail, usage(context, row, timeline),
                where(context, row, timeline), row.entities);
    }

    /**
     * The selected sketch's dependencies and the reasons an act is absent: the
     * features that extrude it by name, a lost support, why it cannot be
     * deleted or reused. Every reason is native's own.
     */
    static String detail(Context context, SketchBrowserPresentation.SketchRow row,
                         SketchBrowserPresentation.Inventory inventory,
                         FeatureHistoryPresentation.Model timeline) {
        final StringBuilder out = new StringBuilder();
        final long[] consumers = inventory.consumersOf(row.id);
        final StringBuilder names = new StringBuilder();
        for (long id : consumers) {
            final FeatureHistoryPresentation.Row feature =
                    SketchBrowserPresentation.featureRow(timeline, id);
            if (names.length() > 0) {
                names.append(", ");
            }
            names.append(feature == null ? String.valueOf(id) : FeatureHistoryText.name(context, feature));
        }
        out.append(context.getString(R.string.sketch_dependencies, names.length() == 0
                ? context.getString(R.string.sketch_dependencies_none) : names.toString()));
        if (row.state == NativeViewport.SKETCH_ROW_SUPPORT_LOST
                || row.state == NativeViewport.SKETCH_ROW_FAILED) {
            out.append('\n').append(context.getString(R.string.sketch_support_lost,
                    CadStatusMessages.describe(context, row.status)));
        }
        if (row.consumers > 1) {
            out.append('\n').append(context.getString(R.string.sketch_shared_edit_warning,
                    row.consumers));
        }
        if (!row.offers(NativeViewport.SKETCH_ACT_DELETE)
                && row.deleteRefusal != NativeViewport.CAD_OK) {
            out.append('\n').append(context.getString(R.string.sketch_cannot_delete,
                    CadStatusMessages.describe(context, row.deleteRefusal)));
        }
        if (!row.offers(NativeViewport.SKETCH_ACT_REUSE_ADD)
                && row.reuseRefusal != NativeViewport.CAD_OK) {
            out.append('\n').append(context.getString(R.string.sketch_cannot_reuse,
                    CadStatusMessages.describe(context, row.reuseRefusal)));
        }
        return out.toString();
    }

    /** A selected feature's dependencies: the sketch it is built from, and where. */
    static String featureDetail(Context context, LengthUnit unit,
                                FeatureHistoryPresentation.Row feature,
                                SketchBrowserPresentation.Inventory inventory,
                                FeatureHistoryPresentation.Model timeline) {
        final SketchBrowserPresentation.SketchRow sketch = inventory.find(feature.sketchId);
        final StringBuilder out = new StringBuilder(FeatureHistoryText.detail(context, unit,
                feature));
        if (sketch != null) {
            out.append('\n').append(context.getString(R.string.feature_dependencies,
                    name(context, sketch), where(context, sketch, timeline)));
            if (sketch.consumers > 1) {
                out.append('\n').append(context.getString(R.string.sketch_shared_edit_warning,
                        sketch.consumers));
            }
        }
        return out.toString();
    }

    /** The label of one act chip. */
    static int actLabel(int act) {
        switch (act) {
            case SketchBrowserPresentation.ACT_EDIT_FEATURE: return R.string.history_act_edit_feature;
            case SketchBrowserPresentation.ACT_REUSE_ADD: return R.string.history_act_reuse_add;
            case SketchBrowserPresentation.ACT_REUSE_CUT: return R.string.history_act_reuse_cut;
            case SketchBrowserPresentation.ACT_DELETE_SKETCH: return R.string.history_act_delete_sketch;
            default: return R.string.history_act_edit_sketch;
        }
    }

    /** The id of one act chip; an id names the act. */
    static int actId(int act) {
        switch (act) {
            case SketchBrowserPresentation.ACT_EDIT_FEATURE: return R.id.history_act_edit_feature;
            case SketchBrowserPresentation.ACT_REUSE_ADD: return R.id.history_act_reuse_add;
            case SketchBrowserPresentation.ACT_REUSE_CUT: return R.id.history_act_reuse_cut;
            case SketchBrowserPresentation.ACT_DELETE_SKETCH: return R.id.history_act_delete_sketch;
            default: return R.id.history_act_edit_sketch;
        }
    }

    /** The full content description of one act on a named target. */
    static String actDescription(Context context, int act, String target) {
        switch (act) {
            case SketchBrowserPresentation.ACT_EDIT_FEATURE:
                return context.getString(R.string.history_act_edit_feature_description, target);
            case SketchBrowserPresentation.ACT_REUSE_ADD:
                return context.getString(R.string.history_act_reuse_add_description, target);
            case SketchBrowserPresentation.ACT_REUSE_CUT:
                return context.getString(R.string.history_act_reuse_cut_description, target);
            case SketchBrowserPresentation.ACT_DELETE_SKETCH:
                return context.getString(R.string.history_act_delete_sketch_description, target);
            default:
                return context.getString(R.string.history_act_edit_sketch_description, target);
        }
    }
}
