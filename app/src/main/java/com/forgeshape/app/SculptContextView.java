package com.forgeshape.app;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
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
 * state and one guarded action: what the sculpt mesh currently is, whether the
 * Construction Source has moved on since sculpting started, and the only
 * irreversible act in the product.
 *
 * <p><b>Owns no vertex and no mode.</b> Everything it draws is read back from
 * native state on refresh.
 *
 * <p><b>Vocabulary.</b> What the user reads here is "Sculpt mesh" and "Reset
 * Sculpt from Shape"; what the code below calls it is a freeze, because that is
 * what {@link NativeViewport#freezeToSculpt()} actually does — it copies the
 * Construction mesh into an editable one. The two names are for two different
 * readers and both are accurate. Nothing about the operation changed at UI-R4B;
 * only what it is called on screen did.
 */
final class SculptContextView extends LinearLayout {

    /** One line per tool saying what the finger will do. */
    private static final int[] TOOL_HINTS = {
            R.string.hint_grab, R.string.hint_clay, R.string.hint_smooth, R.string.hint_inflate
    };

    private final InspectorHost host;
    private final TextView meshSummary;
    private final TextView staleWarning;
    private final TextView resetSculpt;

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
        meshSummary.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        meshSummary.setLineSpacing(
                EditorControlStyles.dimen(context, R.dimen.text_line_spacing), 1.0f);
        addView(meshSummary, EditorControlStyles.rowParams(0));

        // The stale-source warning gets a bordered block of its own rather than
        // a line of status text. It reports a state that persists until the
        // user acts on it, and it sits directly above the action that resolves
        // it -- which is the whole reason it is here and not in the toolbar's
        // status line, where the next message would overwrite it.
        staleWarning = EditorControlStyles.captionText(context, R.id.stale_source_warning,
                context.getString(R.string.stale_source_warning));
        staleWarning.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextMeasure));
        staleWarning.setContentDescription(context.getString(R.string.stale_source_warning));
        final int pad = EditorControlStyles.dimen(context, R.dimen.row_gap);
        staleWarning.setPadding(pad, pad, pad, pad);
        staleWarning.setBackgroundResource(R.drawable.bg_warning);
        addView(staleWarning, EditorControlStyles.rowParams(gap));

        resetSculpt = EditorControlStyles.actionChip(context, R.id.freeze_again,
                context.getString(R.string.reset_sculpt_from_shape));
        resetSculpt.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onResetSculptRequested();
            }
        });
        final LinearLayout.LayoutParams resetParams = EditorControlStyles.rowParams(gap);
        resetParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        addView(resetSculpt, resetParams);

        refreshFromNative();
    }

    /** Rebuilds the summary and the stale-source state from native truth. */
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
     * <p>Resetting rebuilds the sculpt mesh from the current Construction shape,
     * which discards whatever was sculpted into the old one. The guard appears
     * <b>only when there is something to lose</b>: if nothing has been sculpted
     * into the mesh that exists right now, resetting replaces a copy with an
     * identical copy and confirming it would teach the user to dismiss the
     * dialog that matters. Cancel changes nothing at all — it makes no native
     * call.
     *
     * <p>The question asked is {@link NativeViewport#SCULPT_HAS_EDITS}, about
     * the <b>current</b> sculpt mesh, and deliberately not the session-lifetime
     * {@link NativeViewport#SCULPT_STROKE_COUNT}. Strokes made on an earlier
     * mesh are already gone; warning about them would make every later reset of
     * an untouched mesh raise a dialog with nothing behind it, which is exactly
     * the training-to-dismiss the owner contract forbids. For the same reason
     * the message states no number: this layer knows only that edits exist, and
     * quoting a historical count would be a false claim about what is being
     * lost.
     */
    private void onResetSculptRequested() {
        NativeViewport.sculptState(nativeState);
        if (nativeState[NativeViewport.SCULPT_HAS_EDITS] == 0.0) {
            rebuildSculptMeshFromShape();
            return;
        }
        final Context context = getContext();
        confirmation = new AlertDialog.Builder(context)
                .setTitle(R.string.reset_sculpt_confirm_title)
                .setMessage(context.getString(R.string.reset_sculpt_confirm_message))
                // The button carries the verb, not "OK": the user should be
                // able to read what pressing it does without re-reading the
                // message above it.
                .setPositiveButton(R.string.reset_sculpt_confirm_action,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                rebuildSculptMeshFromShape();
                            }
                        })
                .setNegativeButton(R.string.cancel, null)
                .create();
        confirmation.show();
    }

    private void rebuildSculptMeshFromShape() {
        if (NativeViewport.freezeToSculpt() != NativeViewport.SCULPT_OK) {
            host.showStatus(getContext().getString(R.string.status_sculpt_prepare_failed),
                    R.attr.fsTextError);
            return;
        }
        host.onNativeStateChanged();
        host.showStatus(getContext().getString(R.string.status_now_sculpting, describeNativeKind()),
                R.attr.fsTextSuccess);
    }

    private String describeNativeKind() {
        final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        NativeViewport.constructionPrimitive(primitive);
        switch ((int) primitive[0]) {
            case NativeViewport.PRIMITIVE_CYLINDER: return "cylinder";
            case NativeViewport.PRIMITIVE_SPHERE: return "sphere";
            case NativeViewport.PRIMITIVE_CONE: return "cone";
            case NativeViewport.PRIMITIVE_CAPSULE: return "capsule";
            case NativeViewport.PRIMITIVE_PLANE: return "plane";
            default: return "box";
        }
    }

    /** The reset confirmation currently on screen, or {@code null}. */
    AlertDialog visibleConfirmation() {
        return (confirmation != null && confirmation.isShowing()) ? confirmation : null;
    }
}
