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
 * The mode-independent controls at the top of the Editor Workspace.
 *
 * <p>It carries only what is true in every mode <b>and</b> is not something the
 * user works from: what is being edited, the way across the Construction/Sculpt
 * seam, the reserved global Export action, the chrome hide control, and — the
 * part that matters most — the status and error message. The scene itself is
 * not here: it is the Objects capsule's, which can say which body is current
 * rather than only offering to open a list.
 *
 * <p><b>It is not a bar.</b> This container is transparent and draws nothing of
 * its own; what the user sees is two floating control <i>groups</i> with the
 * model between and behind them — an editing group on the leading edge holding
 * the mode context and the one transition that belongs to it, and a utility
 * group on the trailing edge holding Export and the icon controls. A full-width
 * opaque strip with a hairline under it is the one shape that reads as an
 * Android app bar no matter what colour it is painted, and the workspace is
 * meant to read as one spatial composition rather than as a document editor with
 * a title bar. Grouping is also what makes the hierarchy visible without
 * colour: a primary commit is the loudest thing in the leading group, and the
 * trailing group is uniformly tertiary.
 *
 * <p><b>The status message is always laid out inside the window.</b> The
 * previous shell put it at the bottom of a wrap-content panel, where in
 * landscape it measured below the window edge with no scroll container anywhere
 * to reach it, which made validation verdicts and the stale-source warning
 * silently unreachable in the one configuration that needed them most. Here it
 * has a reserved place: inline with the controls when the window is short, on
 * its own line when it is not. It is a small capsule sized to its own text
 * rather than a full-width band, because it is persistent — see
 * {@link EditorControlStyles#statusText}.
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

        void onDisplaySettingsRequested();
    }

    private final TextView contextLabel;
    private final TextView freezeButton;
    private final TextView resumeButton;
    private final TextView backButton;
    private final TextView exportAction;
    private final ImageView displaySettingsButton;
    private final ImageView hideUiToggle;
    private final TextView statusMessage;

    private final LinearLayout controlsRow;
    /** The leading capsule: what is being edited, and the way across the seam. */
    private final LinearLayout editingGroup;
    /** The trailing capsule: Export and the icon controls, all tertiary. */
    private final LinearLayout utilityGroup;
    private final FrameLayout statusSlot;

    private boolean statusInline = true;

    GlobalToolbarView(Context context, final OnGlobalAction actions) {
        super(context);
        setId(R.id.global_toolbar);
        setOrientation(VERTICAL);
        // Deliberately no background and no elevation. See the class comment:
        // the surfaces the user sees are the two groups below, not this.
        EditorControlStyles.allowChildShadows(this);

        final int padH = EditorControlStyles.dimen(context, R.dimen.toolbar_padding_horizontal);
        final int gap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);
        setPadding(padH, 0, padH, 0);

        controlsRow = new LinearLayout(context);
        controlsRow.setOrientation(HORIZONTAL);
        controlsRow.setGravity(Gravity.CENTER_VERTICAL);
        EditorControlStyles.allowChildShadows(controlsRow);
        addView(controlsRow, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                EditorControlStyles.dimen(context, R.dimen.toolbar_height)));

        editingGroup = EditorControlStyles.controlGroup(context);
        editingGroup.setId(R.id.toolbar_editing_group);
        controlsRow.addView(editingGroup, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        contextLabel = EditorControlStyles.titleText(context, R.id.editing_context_label, "");
        contextLabel.setSingleLine(true);
        contextLabel.setEllipsize(TextUtils.TruncateAt.END);
        contextLabel.setMaxWidth(
                EditorControlStyles.dimen(context, R.dimen.toolbar_context_max_width));
        // Inside the leading capsule with the transition it describes, because
        // "Sculpt" and "Back to Construction" are one thought. The label sits on
        // a material rather than on the raw viewport, which is what lets the
        // toolbar container be transparent at all.
        //
        // It is still bounded and still ellipsises: the row can run out of width
        // on a narrow window, and a LinearLayout that has run out squeezes its
        // LAST child — which is how both icon controls once ended up below the
        // 44 dp touch floor on a 411 dp window. Neither group is weighted; the
        // flexible child is the gap between them.
        final LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        labelParams.leftMargin =
                EditorControlStyles.dimen(context, R.dimen.chip_padding_horizontal);
        labelParams.rightMargin = gap;
        editingGroup.addView(contextLabel, labelParams);

        freezeButton = EditorControlStyles.primaryButton(context, R.id.freeze_to_sculpt,
                context.getString(R.string.freeze_to_sculpt));
        boundTransitionWidth(freezeButton);
        freezeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onFreezeToSculpt();
            }
        });
        editingGroup.addView(freezeButton, EditorControlStyles.wrap(0));

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
        editingGroup.addView(resumeButton, EditorControlStyles.wrap(0));

        // NOT a primary commit, and this is the one transition where that
        // distinction is real. Freeze and Resume change which representation is
        // being edited and Freeze builds a mesh; Back to Construction destroys
        // nothing, publishes the Construction Source's own mesh and is pure
        // navigation. Drawn as a solid accent block it was the loudest thing on
        // the Sculpt workspace — louder than the model — for the act of leaving.
        //
        // Quiet and TONAL: it takes the ordinary control fill inside the group's
        // own capsule, so it reads as one step up from the material around it
        // rather than as a second commit.
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
        editingGroup.addView(backButton, EditorControlStyles.wrap(0));

        // The flexible child of the row, and the only one. It is where the model
        // shows between the two groups, and it is what absorbs a squeeze — so a
        // narrow window closes the gap first and neither group has to give up a
        // touch target.
        controlsRow.addView(EditorControlStyles.spacer(context));

        statusSlot = new FrameLayout(context);
        final LinearLayout.LayoutParams slotParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, 0.0f);
        slotParams.leftMargin = gap;
        slotParams.rightMargin = gap;
        controlsRow.addView(statusSlot, slotParams);

        utilityGroup = EditorControlStyles.controlGroup(context);
        utilityGroup.setId(R.id.toolbar_utility_group);
        controlsRow.addView(utilityGroup, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        // Export is an approved global action with no implementation behind it.
        // It is drawn, named, readable and inert: a reserved home the user can
        // see, and nothing that could be mistaken for a working export. Inside
        // the utility group it RECEDES — a recessed tonal well below the capsule
        // around it — instead of standing beside the working controls as an
        // outlined box of the same size.
        exportAction = EditorControlStyles.chip(context, R.id.export_action,
                context.getString(R.string.export));
        EditorControlStyles.setChipReserved(exportAction,
                context.getString(R.string.export_reserved_note));
        utilityGroup.addView(exportAction, EditorControlStyles.wrap(0));

        // Objects is deliberately NOT here any more. Which body is being edited
        // is true in every mode, but it is also the fact the user works FROM,
        // and a toolbar icon can only open a list — it cannot say which body is
        // current. That answer now sits in the Objects capsule, which names the
        // active body at rest and carries the one creation affordance beside
        // it. Two controls opening the same panel would be two answers to
        // "where does the scene live".
        //
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
        utilityGroup.addView(displaySettingsButton,
                EditorControlStyles.iconButtonParams(context, gap));

        hideUiToggle = EditorControlStyles.iconButton(context, R.id.hide_ui_toggle,
                R.drawable.ic_chrome_hide, context.getString(R.string.hide_ui));
        hideUiToggle.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onChromeHideRequested();
            }
        });
        utilityGroup.addView(hideUiToggle, EditorControlStyles.iconButtonParams(context, gap));

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
     * The surfaces this toolbar actually paints over the model.
     *
     * <p>Not {@code this}: the container is transparent and draws nothing, so
     * reporting its full-width bounds as occlusion would count a bar that is not
     * there. What stands on the model is the two control capsules and the status
     * capsule, and the workspace's viewport-floor measurement has to be told
     * that rather than left to assume the old strip.
     */
    View[] occludingSurfaces() {
        return new View[]{editingGroup, utilityGroup, statusMessage};
    }

    /**
     * Shows or hides the context label.
     *
     * <p>A narrow window cannot carry both control groups, the status capsule
     * and a label above the 44 dp touch floor, and the label is the least
     * load-bearing of the four. Where it cannot fit it is withdrawn outright
     * rather than squeezed to an ellipsis that says nothing.
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
        // message. Empty and weighted it would take room from the two control
        // groups for nothing, and a short window is precisely where there is
        // none to spare.
        final LinearLayout.LayoutParams slotParams =
                (LinearLayout.LayoutParams) statusSlot.getLayoutParams();
        slotParams.width = inline ? 0 : ViewGroup.LayoutParams.WRAP_CONTENT;
        slotParams.weight = inline ? 1.0f : 0.0f;
        statusSlot.setLayoutParams(slotParams);

        if (inline) {
            final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            params.gravity = Gravity.CENTER_VERTICAL;
            statusSlot.addView(statusMessage, params);
        } else {
            // WRAP_CONTENT, not MATCH_PARENT: the capsule is sized to its own
            // text so a one-line hint is a small aside rather than a band across
            // the whole window. Left-aligned under the leading group, which is
            // the surface that names the mode it is describing.
            final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            params.gravity = Gravity.START;
            params.topMargin = EditorControlStyles.dimen(getContext(), R.dimen.row_gap_small);
            params.bottomMargin = EditorControlStyles.dimen(getContext(), R.dimen.row_gap_small);
            params.leftMargin = EditorControlStyles.dimen(getContext(), R.dimen.row_gap_small);
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
     * Deliberately does <b>not</b> swallow a touch, and this is the one chrome
     * container in the product that does not.
     *
     * <p>It draws nothing: it is a transparent frame holding two floating
     * capsules and a status capsule, and the model between and behind them is
     * genuinely visible. A container that consumed everything inside its
     * full-width bounds would put a 56 dp band of dead space across the top of
     * a viewport the user can see straight through — which is the opposite of
     * what making the toolbar transparent was for.
     *
     * <p>The rule itself is unchanged and is simply enforced one level down:
     * each capsule is clickable and swallows what its own controls did not take,
     * so reaching for Display still cannot orbit the camera behind it. See
     * {@link EditorControlStyles#controlGroup}.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return false;
    }
}
