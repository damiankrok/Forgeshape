package com.forgeshape.app;

import android.content.Context;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The mode-independent strip at the top of the Editor Workspace.
 *
 * <p>It carries only what is true in every mode: what is being edited, the way
 * across the Construction/Sculpt seam, the reserved global Export action, the
 * chrome hide control, and — the part that matters most — the status and error
 * message.
 *
 * <p><b>The status message is always laid out inside the window.</b> The
 * previous shell put it at the bottom of a wrap-content panel, where in
 * landscape it measured below the window edge with no scroll container anywhere
 * to reach it, which made validation verdicts and the stale-source warning
 * silently unreachable in the one configuration that needed them most. Here it
 * has a reserved place in a fixed-height strip: inline with the controls when
 * the window is short, on its own full-width line when it is not.
 *
 * <p><b>Owns no mode.</b> The three transition buttons ask native code; which
 * one is on screen is decided by {@link #showContext} from what native code
 * reports afterwards.
 */
final class GlobalToolbarView extends LinearLayout {

    /** Told which global action was invoked; the caller owns what it means. */
    interface OnGlobalAction {
        void onFreezeToSculpt();

        void onResumeSculpt();

        void onBackToConstruction();

        void onChromeHideRequested();
    }

    private final TextView contextLabel;
    private final TextView freezeButton;
    private final TextView resumeButton;
    private final TextView backButton;
    private final TextView exportAction;
    private final TextView hideUiToggle;
    private final TextView statusMessage;

    private final LinearLayout controlsRow;
    private final FrameLayout statusSlot;

    private boolean statusInline = true;

    GlobalToolbarView(Context context, final OnGlobalAction actions) {
        super(context);
        setId(R.id.global_toolbar);
        setOrientation(VERTICAL);
        setBackground(EditorControlStyles.chromeSurface(context));

        final int padH = EditorControlStyles.dimen(context, R.dimen.toolbar_padding_horizontal);
        final int gap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);
        setPadding(padH, 0, padH, 0);

        controlsRow = new LinearLayout(context);
        controlsRow.setOrientation(HORIZONTAL);
        controlsRow.setGravity(Gravity.CENTER_VERTICAL);
        addView(controlsRow, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                EditorControlStyles.dimen(context, R.dimen.toolbar_height)));

        contextLabel = new TextView(context);
        contextLabel.setId(R.id.editing_context_label);
        contextLabel.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_title));
        contextLabel.setTextColor(context.getColor(R.color.text_primary));
        contextLabel.setSingleLine(true);
        // The label absorbs the squeeze so no button is ever clipped: on a
        // narrow window the mode name ellipsises, the actions stay whole.
        contextLabel.setEllipsize(TextUtils.TruncateAt.END);
        contextLabel.setMaxWidth(EditorControlStyles.dp(context, 150));
        controlsRow.addView(contextLabel, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        statusSlot = new FrameLayout(context);
        final LinearLayout.LayoutParams slotParams = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        slotParams.leftMargin = gap;
        slotParams.rightMargin = gap;
        controlsRow.addView(statusSlot, slotParams);

        freezeButton = EditorControlStyles.primaryButton(context, R.id.freeze_to_sculpt,
                context.getString(R.string.freeze_to_sculpt));
        freezeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onFreezeToSculpt();
            }
        });
        controlsRow.addView(freezeButton, EditorControlStyles.wrap(0));

        // Resume is a different act from Freeze and therefore a different
        // control, not one button whose meaning depends on hidden state:
        // Freeze rebuilds the sculpt mesh from the Construction shape, Resume
        // returns to the sculpt work exactly as it was left.
        resumeButton = EditorControlStyles.primaryButton(context, R.id.resume_sculpt,
                context.getString(R.string.resume_sculpt));
        resumeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onResumeSculpt();
            }
        });
        controlsRow.addView(resumeButton, EditorControlStyles.wrap(0));

        backButton = EditorControlStyles.primaryButton(context, R.id.back_to_construction,
                context.getString(R.string.back_to_construction));
        backButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onBackToConstruction();
            }
        });
        controlsRow.addView(backButton, EditorControlStyles.wrap(0));

        // Export is an approved global action with no implementation behind it.
        // It is drawn, named, readable and inert: a reserved home the user can
        // see, and nothing that could be mistaken for a working export.
        exportAction = EditorControlStyles.chip(context, R.id.export_action,
                context.getString(R.string.export));
        EditorControlStyles.setChipReserved(exportAction,
                context.getString(R.string.export_reserved_note));
        controlsRow.addView(exportAction, EditorControlStyles.wrap(gap));

        hideUiToggle = EditorControlStyles.chip(context, R.id.hide_ui_toggle, "⊟");
        hideUiToggle.setContentDescription(context.getString(R.string.hide_ui));
        hideUiToggle.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onChromeHideRequested();
            }
        });
        controlsRow.addView(hideUiToggle, EditorControlStyles.wrap(gap));

        statusMessage = EditorControlStyles.statusText(context, R.id.status_message);
        statusMessage.setMaxLines(2);
        statusMessage.setEllipsize(TextUtils.TruncateAt.END);
        attachStatus(true);
    }

    /**
     * Chooses where the status message is laid out.
     *
     * <p>A short window cannot spare a second full-width line, so the message
     * shares the control row; a tall one gives it the whole width, where a long
     * rejection reason reads properly. It is the same view either way — there
     * is exactly one status message in the workspace, so no caller can write to
     * the copy that is not on screen.
     */
    void setStatusInline(boolean inline) {
        if (inline == statusInline && statusMessage.getParent() != null) {
            return;
        }
        statusInline = inline;
        attachStatus(inline);
    }

    private void attachStatus(boolean inline) {
        final ViewGroup parent = (ViewGroup) statusMessage.getParent();
        if (parent != null) {
            parent.removeView(statusMessage);
        }
        if (inline) {
            final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            params.gravity = Gravity.CENTER_VERTICAL;
            statusSlot.addView(statusMessage, params);
        } else {
            final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            params.bottomMargin = EditorControlStyles.dimen(getContext(), R.dimen.row_gap_small);
            addView(statusMessage, params);
        }
    }

    /**
     * Draws the current editing context and the one transition that belongs to
     * it.
     *
     * @param sculpting        whether native code reports Sculpt Mode
     * @param hasFrozenMesh    whether a Frozen Sculpt Mesh exists, which is what
     *                         makes the difference between offering Freeze and
     *                         offering Resume
     */
    void showContext(boolean sculpting, boolean hasFrozenMesh) {
        final Context context = getContext();
        contextLabel.setText(context.getString(
                sculpting ? R.string.context_sculpt : R.string.context_construction));
        contextLabel.setContentDescription(contextLabel.getText());

        backButton.setVisibility(sculpting ? VISIBLE : GONE);
        // Freeze and Resume are mutually exclusive by meaning: there is nothing
        // to resume until something has been frozen, and once there is, the
        // non-destructive act is the one that gets the toolbar slot.
        freezeButton.setVisibility(!sculpting && !hasFrozenMesh ? VISIBLE : GONE);
        resumeButton.setVisibility(!sculpting && hasFrozenMesh ? VISIBLE : GONE);
    }

    void showChromeHidden(boolean hidden) {
        hideUiToggle.setContentDescription(getContext().getString(
                hidden ? R.string.show_ui : R.string.hide_ui));
    }

    void showStatus(CharSequence message, int colorRes) {
        statusMessage.setTextColor(getContext().getColor(colorRes));
        statusMessage.setText(message);
        statusMessage.setContentDescription(message);
    }

    CharSequence statusText() {
        return statusMessage.getText();
    }

    /**
     * Swallows every touch the toolbar's own controls did not take, so reaching
     * for a mode button never orbits the camera behind it.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }
}
