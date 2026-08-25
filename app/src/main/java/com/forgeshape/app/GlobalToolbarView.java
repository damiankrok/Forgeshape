package com.forgeshape.app;

import android.content.Context;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
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

        void onObjectsRequested();

        void onDisplaySettingsRequested();
    }

    private final TextView contextLabel;
    private final TextView freezeButton;
    private final TextView resumeButton;
    private final TextView backButton;
    private final TextView exportAction;
    private final ImageView objectsButton;
    private final ImageView displaySettingsButton;
    private final ImageView hideUiToggle;
    private final TextView statusMessage;

    private final LinearLayout controlsRow;
    private final FrameLayout statusSlot;

    private boolean statusInline = true;

    GlobalToolbarView(Context context, final OnGlobalAction actions) {
        super(context);
        setId(R.id.global_toolbar);
        setOrientation(VERTICAL);
        EditorControlStyles.applyChromeSurface(this);

        final int padH = EditorControlStyles.dimen(context, R.dimen.toolbar_padding_horizontal);
        final int gap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);
        setPadding(padH, 0, padH, 0);

        controlsRow = new LinearLayout(context);
        controlsRow.setOrientation(HORIZONTAL);
        controlsRow.setGravity(Gravity.CENTER_VERTICAL);
        addView(controlsRow, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                EditorControlStyles.dimen(context, R.dimen.toolbar_height)));

        contextLabel = EditorControlStyles.titleText(context, R.id.editing_context_label, "");
        contextLabel.setSingleLine(true);
        contextLabel.setEllipsize(TextUtils.TruncateAt.END);
        contextLabel.setMaxWidth(
                EditorControlStyles.dimen(context, R.dimen.toolbar_context_max_width));
        // THE LABEL IS THE FLEXIBLE CHILD, and that is load-bearing rather than
        // cosmetic. A wrap-content label keeps whatever width its text wants
        // even when the row has run out, and a LinearLayout that has run out
        // squeezes the LAST child instead — which put both icon controls below
        // the 44 dp touch floor on a 411 dp-wide window (measured at 35 dp and
        // 33 dp). Weighted, the label gives up its own width first and
        // ellipsises, and every action keeps the size it asked for. The cap
        // stops it growing absurdly wide on a tablet, where there is room.
        controlsRow.addView(contextLabel, new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        statusSlot = new FrameLayout(context);
        final LinearLayout.LayoutParams slotParams = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 0.0f);
        slotParams.leftMargin = gap;
        slotParams.rightMargin = gap;
        controlsRow.addView(statusSlot, slotParams);

        freezeButton = EditorControlStyles.primaryButton(context, R.id.freeze_to_sculpt,
                context.getString(R.string.freeze_to_sculpt));
        boundTransitionWidth(freezeButton);
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
        boundTransitionWidth(resumeButton);
        resumeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onResumeSculpt();
            }
        });
        controlsRow.addView(resumeButton, EditorControlStyles.wrap(0));

        // NOT a primary commit, and this is the one transition where that
        // distinction is real. Freeze and Resume change which representation is
        // being edited and Freeze builds a mesh; Back to Construction destroys
        // nothing, publishes the Construction Source's own mesh and is pure
        // navigation. Drawn as a solid accent block it was the loudest thing on
        // the Sculpt workspace — louder than the model — for the act of leaving.
        backButton = EditorControlStyles.chip(context, R.id.back_to_construction,
                context.getString(R.string.back_to_construction));
        backButton.setTextColor(
                EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        boundTransitionWidth(backButton);
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

        // Objects sits here for the same reason Display does: WHICH body is
        // being edited is true in every mode, so it belongs to no tool and to
        // no inspector body. Before this it lived inside the Construction shape
        // editor, which made a scene-level question reachable only by expanding
        // a panel named after something else.
        //
        // It is hidden, not disabled, on a window wide enough to give Objects a
        // permanent column: a control that opens a panel duplicating what is
        // already on screen is noise. See setObjectsActionVisible.
        objectsButton = EditorControlStyles.iconButton(context, R.id.objects_button,
                R.drawable.ic_objects, context.getString(R.string.objects_open));
        objectsButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onObjectsRequested();
            }
        });
        controlsRow.addView(objectsButton, EditorControlStyles.iconButtonParams(context, gap));

        // Display sits in the Global Toolbar because it is mode-independent:
        // how the surface is shaded is as true in Sculpt as in Construction, so
        // it does not belong to the Tool Rail or to either inspector body.
        displaySettingsButton = EditorControlStyles.iconButton(context,
                R.id.display_settings_button, R.drawable.ic_display,
                context.getString(R.string.display_settings));
        displaySettingsButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onDisplaySettingsRequested();
            }
        });
        controlsRow.addView(displaySettingsButton,
                EditorControlStyles.iconButtonParams(context, gap));

        hideUiToggle = EditorControlStyles.iconButton(context, R.id.hide_ui_toggle,
                R.drawable.ic_chrome_hide, context.getString(R.string.hide_ui));
        hideUiToggle.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onChromeHideRequested();
            }
        });
        controlsRow.addView(hideUiToggle, EditorControlStyles.iconButtonParams(context, gap));

        statusMessage = EditorControlStyles.statusText(context, R.id.status_message);
        statusMessage.setMaxLines(2);
        statusMessage.setEllipsize(TextUtils.TruncateAt.END);
        attachStatus(true);
    }

    /**
     * Bounds a mode-transition button's width so it can never push an icon
     * control off the end of the row.
     *
     * <p>The context label is normally the child that absorbs a squeeze, but a
     * {@code COMPACT} window withdraws it outright — and with it gone the
     * row's next-widest child was an unbounded wrap-content button. On a 411 dp
     * window in Sculpt Mode, "Back to Construction" plus Export plus three icon
     * controls measured wider than the window, and a {@code LinearLayout} that
     * has run out squeezes its LAST child: the Hide UI control was drawn clipped
     * by the window edge, under the touch floor and partly unreachable. That is
     * the same defect {@code R1B1-10b} guards in Construction, arriving through
     * the one mode that test never entered.
     *
     * <p>Bounded and ellipsised, the button gives up its own width first and
     * every icon control keeps the size it asked for. The full wording stays as
     * the content description, so nothing is lost to a screen reader.
     */
    private void boundTransitionWidth(TextView button) {
        button.setMaxWidth(EditorControlStyles.dimen(
                getContext(), R.dimen.toolbar_transition_max_width));
        button.setSingleLine(true);
        button.setEllipsize(TextUtils.TruncateAt.END);
    }

    /**
     * Shows or hides the Objects control.
     *
     * <p>Hidden exactly when the window gives Objects a permanent column of its
     * own, because then the panel it opens would duplicate what is already
     * beside the model. {@code GONE} rather than invisible, so it gives its
     * width back to the context label instead of leaving a hole.
     */
    void setObjectsActionVisible(boolean visible) {
        objectsButton.setVisibility(visible ? VISIBLE : GONE);
    }

    /**
     * Draws the Objects control as active while its panel is open, and says so
     * to accessibility.
     *
     * <p>The same pattern the Tool Rail uses for the held tool: a control that
     * opens a panel should say whether that panel is open, or the only way to
     * find out is to tap it.
     */
    void setObjectsActionOpen(boolean open) {
        EditorControlStyles.setIconButtonActive(objectsButton, open);
        objectsButton.setContentDescription(getContext().getString(
                open ? R.string.objects_close : R.string.objects_open));
    }

    /** Whether the Objects control is currently on screen. */
    boolean objectsActionVisible() {
        return objectsButton.getVisibility() == VISIBLE;
    }

    /**
     * Shows or hides the context label.
     *
     * <p>It is the row's flexible child, so on a narrow window it gives up all
     * of its width to keep every action above the touch floor — and a label
     * measured at zero draws nothing while still claiming to be the thing that
     * says what is being edited. Where it cannot fit it is therefore withdrawn
     * outright rather than left as an empty promise.
     *
     * <p>Nothing is lost by that: the status line directly beneath it always
     * names the mode, and since UI-R2 the Property Inspector's own title names
     * the body — "Shape — Body #1". The label is the third and least legible of
     * the three, and it is the only one competing for a row that has run out.
     */
    void setContextLabelVisible(boolean visible) {
        contextLabel.setVisibility(visible ? VISIBLE : GONE);
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
        // The slot only competes for row width while it actually holds the
        // message. Empty and weighted it would take room from the context
        // label for nothing, and a short window is precisely where there is
        // none to spare.
        final LinearLayout.LayoutParams slotParams =
                (LinearLayout.LayoutParams) statusSlot.getLayoutParams();
        slotParams.weight = inline ? 1.0f : 0.0f;
        statusSlot.setLayoutParams(slotParams);

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

    /** Marks the Display button active while its popover is open. */
    void showDisplaySettingsOpen(boolean open) {
        EditorControlStyles.setIconButtonActive(displaySettingsButton, open);
    }

    /**
     * Says which way the chrome control now goes.
     *
     * <p>The icon changes with it rather than only the description: a control
     * whose glyph means "hide" while pressing it would show is exactly the kind
     * of thing that made the Unicode set unreadable.
     */
    void showChromeHidden(boolean hidden) {
        hideUiToggle.setImageResource(
                hidden ? R.drawable.ic_chrome_show : R.drawable.ic_chrome_hide);
        hideUiToggle.setContentDescription(getContext().getString(
                hidden ? R.string.show_ui : R.string.hide_ui));
    }

    void showStatus(CharSequence message, int colorAttr) {
        statusMessage.setTextColor(
                EditorControlStyles.themeColor(getContext(), colorAttr));
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
