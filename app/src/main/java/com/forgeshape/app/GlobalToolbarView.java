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

        void onProjectActionsRequested();

        /** Export the current model as one GLB file. */
        void onExportGlbRequested();
    }

    private final TextView contextLabel;
    private final TextView freezeButton;
    private final TextView resumeButton;
    private final TextView backButton;
    private final TextView exportAction;
    private final ImageView projectActionsButton;
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

    /** The capsule padding the editing group wears while it holds more than one
     *  control, kept so the group can be un-drawn and drawn again. */
    private final int editingGroupPadding;
    /** The depth a floating capsule carries, moved onto the lone control when
     *  the group stops drawing itself. See {@link #applyEditingComposition}. */
    private final int floatingElevation;

    /** What the editing group last resolved to, so a measure pass that changes
     *  nothing does not restyle three controls. */
    private Boolean editingGroupSolo;
    private View editingGroupLoneMember;

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
        // The lone control carries the capsule's depth when the group stops
        // drawing itself, and a shadow is painted outside the child's bounds —
        // which a group whose bounds are now exactly the child's would clip away.
        EditorControlStyles.allowChildShadows(editingGroup);
        editingGroupPadding = editingGroup.getPaddingLeft();
        floatingElevation = EditorControlStyles.dimen(context, R.dimen.elevation_floating);
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
        // 48 dp touch floor on a 411 dp window. Neither group is weighted; the
        // flexible child is the gap between them.
        final LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        labelParams.leftMargin =
                EditorControlStyles.dimen(context, R.dimen.chip_padding_horizontal);
        labelParams.rightMargin = gap;
        editingGroup.addView(contextLabel, labelParams);

        // "Start Sculpting", and the id is still `freeze_to_sculpt`.
        //
        // The wording changed at UI-R4B and the semantics did not: this control
        // still builds the sculpt mesh from the Construction shape and enters
        // Sculpt Mode, and the Construction Source is still kept. What "Freeze"
        // named was the implementation's act, not the user's — a user starts
        // sculpting; freezing is what the product does to let them. The id is
        // the internal name of that implementation act, it is still accurate,
        // and renaming it would churn every test and every evidence script for
        // a change of copy. See PRODUCT.md for the whole vocabulary.
        freezeButton = EditorControlStyles.primaryButton(context, R.id.freeze_to_sculpt,
                context.getString(R.string.start_sculpting));
        EditorControlStyles.asCapsuleMember(freezeButton, R.drawable.bg_capsule_primary);
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
        EditorControlStyles.asCapsuleMember(resumeButton, R.drawable.bg_capsule_primary);
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
        // rather than as a second commit — on the capsule's own concentric
        // corner, so it is the pill's own member rather than a box inside it.
        backButton = EditorControlStyles.chip(context, R.id.back_to_construction,
                context.getString(R.string.back_to_construction));
        EditorControlStyles.asCapsuleMember(backButton, R.drawable.bg_capsule_tonal);
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

        // Export WORKS now, for exactly one format.
        //
        // It kept this placement through three stages as a drawn, recessed,
        // honestly-unavailable control, which is why it is here and not
        // somewhere new: the reserved home was the promise, and this is the
        // promise being kept rather than a control being added. What changed is
        // that it is an ordinary capsule member instead of a recessed well, and
        // that pressing it does something.
        //
        // The label stays the single word the row was measured for. "Export
        // GLB…" was tried and REJECTED by UIR4B-16: the extra width comes out
        // of the same toolbar row as the transition button, which then
        // abbreviates to "← Construction". Critical navigation outranks naming
        // the format in the label, so the format is carried where it costs the
        // row nothing — the content description, and the .glb filename the
        // system picker opens with.
        exportAction = EditorControlStyles.chip(context, R.id.export_action,
                context.getString(R.string.export));
        EditorControlStyles.asCapsuleMember(exportAction, R.drawable.bg_capsule_tonal);
        exportAction.setTextColor(
                EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        exportAction.setContentDescription(context.getString(R.string.export_glb_description));
        exportAction.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onExportGlbRequested();
            }
        });
        utilityGroup.addView(exportAction, EditorControlStyles.wrap(0));

        // The project actions sit here for the same reason Display does: a
        // project is mode-independent — saving it means the same thing in
        // Construction and in Sculpt — so it belongs to neither the Tool Rail
        // nor either inspector body. It is placed immediately after Export
        // because the two are the same family of thought (what happens to this
        // work outside the viewport), and the ordering says which of them is
        // real: Export is the recessed, reserved one, and this is not.
        //
        // An ICON control, not a chip: the utility group is uniformly tertiary,
        // and a second labelled chip beside Export would read as a second
        // reserved action. Its own surface carries the two prose names.
        projectActionsButton = EditorControlStyles.iconButton(context,
                R.id.project_actions_button, R.drawable.ic_project,
                context.getString(R.string.project_actions));
        projectActionsButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onProjectActionsRequested();
            }
        });
        utilityGroup.addView(projectActionsButton,
                EditorControlStyles.iconButtonParams(context, gap));

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
        // Three lines rather than two, and it is the short-landscape fix. The
        // capsule is sized to its own text, so on a tall window a long rejection
        // wraps and fits; on a SHORT one the message shares the control row,
        // where two lines cut "Rejected: Total Height cannot be less than
        // Diameter — the two rounded…" in the middle of the sentence that
        // carries the reason. A third line is one row of caption text, and it is
        // the difference between a rule the user learns and an ellipsis.
        statusMessage.setMaxLines(3);
        statusMessage.setEllipsize(TextUtils.TruncateAt.END);
        // Nothing to say yet, so nothing is drawn: see write().
        statusMessage.setVisibility(GONE);
        attachStatus(true);
    }

    /**
     * Makes a mode-transition button a single line, and nothing else.
     *
     * <p>How wide it may grow is decided per measure pass by {@link
     * #fitTransitionToRow}, against the row it is actually in. The ellipsis is
     * kept as a last resort so a window narrower than anything the product
     * supports still lays out, but it is no longer the ordinary outcome: it was,
     * while the cap was a single dp constant sized against the narrowest window
     * the product supports and then applied to every window — which is how
     * "Back to Construction" came to read "Back to Constructi…" on a phone, on a
     * short landscape window AND on a tablet, all three with room to spare.
     */
    private void boundTransitionWidth(TextView button) {
        button.setSingleLine(true);
        button.setEllipsize(TextUtils.TruncateAt.END);
    }

    // -----------------------------------------------------------------------
    // Fitting the row
    //
    // Back to Construction is critical navigation: it is the only way out of
    // Sculpt Mode, and a label the user has to guess at is not navigation. It
    // may not be abbreviated wherever the row can carry it, which is every
    // window the product is verified in — the row is short of space only on the
    // narrowest one, and only then by a few dp.
    //
    // So the budget is arithmetic on the row rather than a constant: what is
    // left after the utility group has the width it asked for, an inline status
    // has its floor, and the context label (where it is drawn at all) has its
    // own bounded share. The order is deliberate and is the same priority
    // R1B1-10b established — an icon control never gives up width, because an
    // icon control squeezed below the touch floor is unreachable, while a
    // transition button that gives some up still reads.
    // -----------------------------------------------------------------------

    /**
     * Gives the visible mode transition the width the row can actually spare.
     *
     * <p>Runs at the top of measure, so whatever it decides is part of the same
     * traversal. It is idempotent — the same row width resolves to the same
     * budget — so the second measure pass of a traversal changes nothing.
     */
    private void fitTransitionToRow(int rowWidth) {
        final TextView transition = visibleTransition();
        if (rowWidth <= 0 || transition == null) {
            return;
        }
        final int unspecified = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        utilityGroup.measure(unspecified, unspecified);

        final LinearLayout.LayoutParams slotParams =
                (LinearLayout.LayoutParams) statusSlot.getLayoutParams();
        int taken = utilityGroup.getMeasuredWidth()
                + slotParams.leftMargin + slotParams.rightMargin;
        // An inline status shares the row rather than sitting under it, so it
        // has a floor there — otherwise a long transition label would take the
        // whole row and the one line carrying a rejection would measure to
        // nothing on exactly the window that has no second line to put it on.
        if (statusInline && statusMessage.getVisibility() == VISIBLE) {
            taken += EditorControlStyles.dimen(getContext(),
                    R.dimen.toolbar_status_min_width);
        }

        int budget = rowWidth - taken
                - editingGroup.getPaddingLeft() - editingGroup.getPaddingRight();
        if (contextLabel.getVisibility() == VISIBLE) {
            contextLabel.measure(unspecified, unspecified);
            final LinearLayout.LayoutParams labelParams =
                    (LinearLayout.LayoutParams) contextLabel.getLayoutParams();
            budget -= contextLabel.getMeasuredWidth()
                    + labelParams.leftMargin + labelParams.rightMargin;
        }
        budget = Math.max(budget, EditorControlStyles.dimen(getContext(),
                R.dimen.toolbar_transition_min_width));

        applyTransitionLabel(transition, budget);
        if (transition.getMaxWidth() != budget) {
            transition.setMaxWidth(budget);
        }
    }

    /**
     * Writes the wording this budget can carry, full wherever it fits.
     *
     * <p>Only <i>Back to Construction</i> has a second form, and it is the one
     * control that needed one: it is both the longest label in the product and
     * the one that may not be guessed at. The short form is the same act in the
     * same words minus the preposition, with the arrow the direction is already
     * drawn with elsewhere — and the content description stays the full wording
     * in both, so what a screen reader announces never changes with the window.
     */
    private void applyTransitionLabel(TextView transition, int budget) {
        final Context context = getContext();
        CharSequence label = context.getString(transition == backButton
                ? R.string.back_to_construction
                : transition == resumeButton
                        ? R.string.resume_sculpt : R.string.start_sculpting);
        if (transition == backButton && naturalWidth(transition, label) > budget) {
            label = context.getString(R.string.back_to_construction_short);
        }
        if (!TextUtils.equals(transition.getText(), label)) {
            transition.setText(label);
        }
    }

    /** How wide this single-line control would be with nothing bounding it. */
    private static int naturalWidth(TextView view, CharSequence text) {
        return Math.round(view.getPaint().measureText(text, 0, text.length()))
                + view.getPaddingLeft() + view.getPaddingRight();
    }

    /** The one mode transition currently drawn, or null between modes. */
    private TextView visibleTransition() {
        if (backButton.getVisibility() == VISIBLE) {
            return backButton;
        }
        if (resumeButton.getVisibility() == VISIBLE) {
            return resumeButton;
        }
        return freezeButton.getVisibility() == VISIBLE ? freezeButton : null;
    }

    /**
     * Draws the editing group as a capsule only while it is holding a group.
     *
     * <p>A {@code COMPACT} window withdraws the context label, which leaves the
     * group with exactly one member — and a 26 dp dark pill drawn around a
     * single 22 dp blue button is a button inside a button. It read as a halo:
     * a crescent of host showing all the way round the one control it hosted,
     * making the transition the heaviest object in a resting workspace whose
     * subject is the model. A capsule is a relation between controls, and there
     * is no relation to draw when there is one control.
     *
     * <p>So the lone member simply <b>becomes</b> the capsule: the group stops
     * painting and stops padding, and the control takes the capsule's own corner
     * and the capsule's own depth. Nothing about the control's size, its touch
     * target or its role changes — it loses 8 dp of host, not 8 dp of itself.
     * Where the group genuinely holds two members, the segmented relation is
     * unchanged and each member stays concentric with the host around it.
     */
    private void applyEditingComposition() {
        int visible = 0;
        View lone = null;
        for (int i = 0; i < editingGroup.getChildCount(); i++) {
            final View child = editingGroup.getChildAt(i);
            if (child.getVisibility() == VISIBLE) {
                visible++;
                lone = child;
            }
        }
        final boolean solo = visible == 1;
        if (editingGroupSolo != null && editingGroupSolo == solo
                && editingGroupLoneMember == lone) {
            return;
        }
        editingGroupSolo = solo;
        editingGroupLoneMember = lone;

        if (solo) {
            editingGroup.setBackground(null);
            editingGroup.setElevation(0.0f);
            editingGroup.setPadding(0, 0, 0, 0);
        } else {
            EditorControlStyles.applyFloatingSurface(editingGroup);
            editingGroup.setPadding(editingGroupPadding, editingGroupPadding,
                    editingGroupPadding, editingGroupPadding);
        }
        applyMemberForm(freezeButton, solo && lone == freezeButton,
                R.drawable.bg_pill_primary, R.drawable.bg_capsule_primary);
        applyMemberForm(resumeButton, solo && lone == resumeButton,
                R.drawable.bg_pill_primary, R.drawable.bg_capsule_primary);
        applyMemberForm(backButton, solo && lone == backButton,
                R.drawable.bg_pill_tonal, R.drawable.bg_capsule_tonal);
    }

    private void applyMemberForm(TextView member, boolean alone, int pill, int capsuleMember) {
        EditorControlStyles.asCapsuleMember(member, alone ? pill : capsuleMember);
        member.setElevation(alone ? floatingElevation : 0.0f);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        fitTransitionToRow(MeasureSpec.getSize(widthMeasureSpec)
                - getPaddingLeft() - getPaddingRight());
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
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
     * and a label above the 48 dp touch floor, and the label is the least
     * load-bearing of the four. Where it cannot fit it is withdrawn outright
     * rather than squeezed to an ellipsis that says nothing.
     *
     * <p>Nothing is lost by that: the transition beside it names the mode by
     * naming the way out of it, and since UI-R2 the Property Inspector's own
     * title names the body — "Exact Shape — Body #1". The label is the least
     * legible of the three, and it is the only one competing for a row that has
     * run out. The status line no longer answers this: it reports events and
     * carries no ambient statement of where the user is.
     */
    void setContextLabelVisible(boolean visible) {
        contextLabel.setVisibility(visible ? VISIBLE : GONE);
        // Withdrawing the label is what can leave the group holding one control,
        // and a group of one is not a group. See applyEditingComposition.
        applyEditingComposition();
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
        applyEditingComposition();
    }

    /** Marks the Display button active while its popover is open. */
    void showDisplaySettingsOpen(boolean open) {
        EditorControlStyles.setIconButtonActive(displaySettingsButton, open);
    }

    /** The same, for the surface the project control opens. */
    void showProjectActionsOpen(boolean open) {
        EditorControlStyles.setIconButtonActive(projectActionsButton, open);
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

    // -----------------------------------------------------------------------
    // The status line's lifecycle
    //
    // Before UI-R4B it had none. Every message was written and then simply
    // stayed: "Body #3 selected." was still on the workspace ten minutes and
    // three tools later, and the resting screenshots of the previous review are
    // full of verdicts about acts long finished. A line that always says
    // something says nothing, and it costs a capsule's worth of the top of the
    // model to do it.
    //
    // Two kinds of message, and the difference is whether it describes a STATE
    // or an EVENT:
    //
    //   STANDING  a fault that persists until the user acts on it. Today there
    //             is exactly one — a stale Construction Source. It is written on
    //             every refresh and re-asserts itself after any transient that
    //             covered it, because it is still true.
    //
    //   TRANSIENT a verdict about something that just happened: applied,
    //             rejected, selected, created, switched. It has a life, and when
    //             that life is over the line goes back to whatever is standing —
    //             which is usually nothing, and nothing is drawn as no capsule
    //             at all rather than as an empty one.
    //
    // This is deliberately not a notification framework: two constants, one
    // posted Runnable and one cancel. What makes it correct is the cancel —
    // exactly the rule ChromeMotion follows for animations, for the same reason.
    // A second message must replace the first AND its pending clear, or the
    // first message's timer would wipe the second one off the screen early.
    // -----------------------------------------------------------------------

    /**
     * How long an ordinary transient stands.
     *
     * <p>Long enough to be read after the eye has come back from the model,
     * short enough that a resting workspace is genuinely resting.
     */
    static final long STATUS_HOLD_MS = 5000L;

    /**
     * How long a rejection stands.
     *
     * <p>Longer, because ForgeShape's rejection copy is its own argument for
     * itself: "Rejected: Total Height cannot be less than Diameter — the two
     * rounded ends alone are that tall. Object unchanged." is a sentence that
     * teaches the constraint, and a message the user cannot finish reading may
     * as well have said "Invalid". The wording is not shortened to fit a
     * timeout; the timeout is set to fit the wording.
     */
    static final long STATUS_FAULT_HOLD_MS = 10000L;

    /** What the line returns to when a transient's life is over. */
    private CharSequence standingMessage = "";
    private int standingColorAttr = R.attr.fsTextSecondary;

    /** The one pending clear. Held so it can be cancelled, which is the whole
     *  mechanism. */
    private final Runnable restoreStanding = new Runnable() {
        @Override
        public void run() {
            write(standingMessage, standingColorAttr);
        }
    };

    /**
     * Writes a verdict about something that just happened.
     *
     * <p>Cancel-first: a newer message replaces the older one <b>and</b> its
     * pending clear, so a rapid sequence cannot have an early message's timer
     * blank a later one.
     */
    void showStatus(CharSequence message, int colorAttr) {
        removeCallbacks(restoreStanding);
        write(message, colorAttr);
        if (message == null || message.length() == 0) {
            return;
        }
        postDelayed(restoreStanding, colorAttr == R.attr.fsTextError
                ? STATUS_FAULT_HOLD_MS : STATUS_HOLD_MS);
    }

    /**
     * Sets what the line says at rest, and shows it now.
     *
     * <p>Called from the one refresh every surface re-reads, so a standing fault
     * re-asserts itself after any transient that covered it, and clears the
     * moment it stops being true. An empty standing message is the ordinary
     * case: at rest the workspace has nothing to say.
     */
    void showStandingStatus(CharSequence message, int colorAttr) {
        standingMessage = message == null ? "" : message;
        standingColorAttr = colorAttr;
        // A standing change outranks a transient in flight: the transient
        // described an act, and the state it described has just been re-read.
        removeCallbacks(restoreStanding);
        write(standingMessage, standingColorAttr);
    }

    private void write(CharSequence message, int colorAttr) {
        statusMessage.setTextColor(
                EditorControlStyles.themeColor(getContext(), colorAttr));
        statusMessage.setText(message);
        statusMessage.setContentDescription(message);
        // GONE rather than an empty capsule. The capsule is a surface standing
        // on the model, and a surface with nothing in it is exactly the kind of
        // permanent claim on the workspace that UI-R4A removed everywhere else.
        statusMessage.setVisibility(
                message == null || message.length() == 0 ? GONE : VISIBLE);
    }

    /**
     * Drops the pending clear when this toolbar leaves the window.
     *
     * <p>The platform already discards a detached view's callbacks, so this is
     * belt and braces — but the toolbar IS detached and rebuilt by an appearance
     * change, and a timer left holding a reference to a dead workspace is the
     * kind of thing that is only ever found the hard way.
     */
    @Override
    protected void onDetachedFromWindow() {
        removeCallbacks(restoreStanding);
        super.onDetachedFromWindow();
    }

    CharSequence statusText() {
        return statusMessage.getText();
    }

    /** Whether the line currently has anything to say, for verification. */
    boolean statusVisible() {
        return statusMessage.getVisibility() == VISIBLE;
    }

    /** What the line returns to when a transient expires, for verification. */
    CharSequence standingStatusText() {
        return standingMessage;
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
