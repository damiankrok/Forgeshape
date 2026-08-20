package com.forgeshape.app;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The Sculpt Property Inspector body.
 *
 * <p>Sculpt has no exact values to type — Radius and Strength are direct edge
 * controls, and the tools are the rail — so what remains for the inspector is
 * state and one guarded action: what the Frozen Sculpt Mesh currently is,
 * whether the Construction Source has moved on since it was frozen, and the
 * only irreversible act in the product.
 *
 * <p><b>Owns no vertex and no mode.</b> Everything it draws is read back from
 * native state on refresh.
 */
final class SculptContextView extends LinearLayout {

    /** One line per tool saying what the finger will do. */
    private static final int[] TOOL_HINTS = {
            R.string.hint_grab, R.string.hint_clay, R.string.hint_smooth, R.string.hint_inflate
    };

    private final InspectorHost host;
    private final TextView meshSummary;
    private final TextView staleWarning;
    private final TextView freezeAgain;

    /** Reused across reads; native fills it with the authoritative state. */
    private final double[] nativeState = new double[NativeViewport.SCULPT_STATE_SIZE];

    /**
     * The confirmation currently on screen, if any.
     *
     * <p>Kept so the guard is observable: a verification can assert that the
     * destructive path really does stop and ask, rather than inferring it from
     * the mesh afterwards.
     */
    private AlertDialog confirmation;

    SculptContextView(Context context, InspectorHost host) {
        super(context);
        this.host = host;
        setOrientation(VERTICAL);

        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);

        meshSummary = new TextView(context);
        meshSummary.setId(R.id.sculpt_mesh_summary);
        meshSummary.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        meshSummary.setTextColor(context.getColor(R.color.text_primary));
        addView(meshSummary, EditorControlStyles.rowParams(0));

        // The stale-source warning gets a bordered block of its own rather than
        // a line of status text. It reports a state that persists until the
        // user acts on it, and it sits directly above the action that resolves
        // it -- which is the whole reason it is here and not in the toolbar's
        // status line, where the next message would overwrite it.
        staleWarning = new TextView(context);
        staleWarning.setId(R.id.stale_source_warning);
        staleWarning.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_status));
        staleWarning.setTextColor(context.getColor(R.color.text_measure));
        staleWarning.setText(context.getString(R.string.stale_source_warning));
        staleWarning.setContentDescription(context.getString(R.string.stale_source_warning));
        final int pad = EditorControlStyles.dimen(context, R.dimen.row_gap);
        staleWarning.setPadding(pad, pad, pad, pad);
        final GradientDrawable warningBackground = new GradientDrawable();
        warningBackground.setColor(context.getColor(R.color.control_surface));
        warningBackground.setCornerRadius(
                EditorControlStyles.dimen(context, R.dimen.control_corner));
        warningBackground.setStroke(
                EditorControlStyles.dimen(context, R.dimen.control_border_width),
                context.getColor(R.color.text_measure));
        staleWarning.setBackground(warningBackground);
        addView(staleWarning, EditorControlStyles.rowParams(gap));

        freezeAgain = EditorControlStyles.chip(context, R.id.freeze_again,
                context.getString(R.string.freeze_again));
        freezeAgain.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onFreezeAgainRequested();
            }
        });
        final LinearLayout.LayoutParams freezeParams = EditorControlStyles.rowParams(gap);
        freezeParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        addView(freezeAgain, freezeParams);

        refreshFromNative();
    }

    /**
     * Rebuilds the summary, the stale-source state and the ellipsis on the
     * re-Freeze label from authoritative native state.
     */
    void refreshFromNative() {
        NativeViewport.sculptState(nativeState);
        final Context context = getContext();
        final int tool = activeTool();
        meshSummary.setText(context.getString(R.string.sculpt_mesh_summary,
                (long) nativeState[NativeViewport.SCULPT_VERTEX_COUNT],
                context.getString(R.string.sculpt_gesture_rule,
                        context.getString(TOOL_HINTS[tool]))));
        meshSummary.setContentDescription(meshSummary.getText());
        staleWarning.setVisibility(
                nativeState[NativeViewport.SCULPT_SOURCE_STALE] != 0.0 ? VISIBLE : GONE);
    }

    private int activeTool() {
        final int tool = (int) nativeState[NativeViewport.SCULPT_TOOL];
        return (tool >= 0 && tool < TOOL_HINTS.length) ? tool : NativeViewport.TOOL_GRAB;
    }

    /**
     * The one irreversible act in the product, and the only one that is guarded.
     *
     * <p>Freezing again rebuilds the sculpt mesh from the current Construction
     * shape, which discards whatever was sculpted into the old one. The guard
     * appears <b>only when there is something to lose</b>: if no stroke has
     * landed on this mesh, re-freezing replaces a copy with an identical copy
     * and confirming it would teach the user to dismiss the dialog that
     * matters. Cancel changes nothing at all — it makes no native call.
     */
    private void onFreezeAgainRequested() {
        NativeViewport.sculptState(nativeState);
        final long strokes = (long) nativeState[NativeViewport.SCULPT_STROKE_COUNT];
        if (strokes <= 0) {
            freezeNow();
            return;
        }
        final Context context = getContext();
        confirmation = new AlertDialog.Builder(context)
                .setTitle(R.string.freeze_confirm_title)
                .setMessage(context.getString(R.string.freeze_confirm_message, strokes,
                        context.getString(strokes == 1 ? R.string.freeze_confirm_stroke
                                                       : R.string.freeze_confirm_strokes)))
                // The button carries the verb, not "OK": the user should be
                // able to read what pressing it does without re-reading the
                // message above it.
                .setPositiveButton(R.string.freeze_confirm_action,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                freezeNow();
                            }
                        })
                .setNegativeButton(R.string.cancel, null)
                .create();
        confirmation.show();
    }

    private void freezeNow() {
        if (NativeViewport.freezeToSculpt() != NativeViewport.SCULPT_OK) {
            host.showStatus(getContext().getString(R.string.status_freeze_failed),
                    R.color.text_error);
            return;
        }
        host.onNativeStateChanged();
        host.showStatus(getContext().getString(R.string.status_frozen, describeNativeKind()),
                R.color.text_success);
    }

    private String describeNativeKind() {
        final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        NativeViewport.constructionPrimitive(primitive);
        switch ((int) primitive[0]) {
            case NativeViewport.PRIMITIVE_CYLINDER: return "cylinder";
            case NativeViewport.PRIMITIVE_SPHERE: return "sphere";
            case NativeViewport.PRIMITIVE_CONE: return "cone";
            case NativeViewport.PRIMITIVE_CAPSULE: return "capsule";
            default: return "box";
        }
    }

    /** The re-Freeze confirmation currently on screen, or {@code null}. */
    AlertDialog visibleConfirmation() {
        return (confirmation != null && confirmation.isShowing()) ? confirmation : null;
    }
}
