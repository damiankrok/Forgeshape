package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

/**
 * The one question ForgeShape asks when it finds work the user never saved.
 *
 * <p>Exactly two answers, because there are exactly two honest ones.
 * <b>Recover</b> loads the checkpoint through the same fail-closed path an Open
 * takes. <b>Discard</b> retires it and starts normally. There is no third
 * option, no "ask me later" and no automatic choice — see below.
 *
 * <p><b>Nothing is replaced before the user chooses.</b> That is the entire
 * reason this surface exists rather than a silent restore. Automatically loading
 * a checkpoint would mean the app deciding, on the user's behalf and without
 * telling them, that an interrupted session is worth more than whatever they
 * explicitly saved. It might be. It is not the app's call.
 *
 * <p><b>It is not a Home screen or a document browser.</b> It appears only when
 * a validated candidate exists, it is answered once, and the answer takes the
 * user straight into the ordinary workspace. Deliberately built like
 * {@link StartChooserView} — the same scrim, the same panel, the same two-card
 * shape — because it occupies the same moment and a second visual grammar for
 * "one question before you start" would be a second thing to learn.
 *
 * <p><b>It owns no state and makes no native call.</b> It reports which option
 * was pressed; {@link EditorWorkspaceView} owns what that means.
 */
final class RecoveryPromptView extends FrameLayout {

    /** Told what the user decided about the unsaved work that was found. */
    interface OnRecoveryChoice {
        void onRecoverRequested();

        void onDiscardRecoveryRequested();
    }

    private final ScrollView panel;

    RecoveryPromptView(Context context, final OnRecoveryChoice listener) {
        super(context);
        setId(R.id.recovery_prompt);
        setBackgroundResource(R.drawable.bg_chooser_scrim);
        EditorControlStyles.allowChildShadows(this);

        panel = new ScrollView(context);
        panel.setId(R.id.recovery_prompt_panel);
        panel.setBackgroundResource(R.drawable.bg_chooser_panel);
        panel.setElevation(EditorControlStyles.dimen(context, R.dimen.elevation_chooser));
        final int pad = EditorControlStyles.dimen(context, R.dimen.chooser_padding);
        panel.setPadding(pad, pad, pad, pad);
        panel.setClipToPadding(false);

        final LinearLayout content = new LinearLayout(context);
        content.setOrientation(LinearLayout.VERTICAL);
        panel.addView(content, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        content.addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.app_name)),
                EditorControlStyles.rowParams(0));

        content.addView(EditorControlStyles.displayText(context,
                context.getString(R.string.recovery_title)),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        content.addView(EditorControlStyles.captionText(context, View.NO_ID,
                context.getString(R.string.recovery_prompt)),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        content.addView(buildOption(context, R.id.recovery_recover,
                        context.getString(R.string.recovery_recover_title),
                        context.getString(R.string.recovery_recover_description),
                        new OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                listener.onRecoverRequested();
                            }
                        }),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.section_gap)));

        content.addView(buildOption(context, R.id.recovery_discard,
                        context.getString(R.string.recovery_discard_title),
                        context.getString(R.string.recovery_discard_description),
                        new OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                listener.onDiscardRecoveryRequested();
                            }
                        }),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));

        final LayoutParams params = new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.CENTER;
        addView(panel, params);
    }

    /**
     * One answer: what it is, and one line of what it does to the user's work.
     *
     * <p>The description carries the whole weight here. "Discard" on its own
     * does not say that an explicitly saved project is untouched by it, and that
     * is exactly the fact a user needs before they dare press it.
     */
    private View buildOption(Context context, int id, CharSequence title,
                             CharSequence description, OnClickListener onChosen) {
        final LinearLayout option = new LinearLayout(context);
        option.setId(id);
        option.setOrientation(LinearLayout.VERTICAL);
        option.setBackgroundResource(R.drawable.bg_chooser_card);
        final int pad = EditorControlStyles.dimen(context, R.dimen.chooser_option_padding);
        option.setPadding(pad, pad, pad, pad);
        option.setClickable(true);
        option.setFocusable(true);
        // The 48 dp floor as HIT AREA. These are the widest controls in the
        // product and clear it on width by a distance; the minimum is set on
        // height so a very short description cannot shrink one below the floor.
        option.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
        option.setContentDescription(title + ". " + description);
        option.setOnClickListener(onChosen);

        option.addView(EditorControlStyles.titleText(context, View.NO_ID, title),
                EditorControlStyles.rowParams(0));
        option.addView(EditorControlStyles.captionText(context, View.NO_ID, description),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap_small)));
        return option;
    }

    /** Caps the panel's width the same way the start question does. */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final Context context = getContext();
        final int margin = EditorControlStyles.dimen(context, R.dimen.chooser_padding);
        final int cap = EditorControlStyles.dimen(context, R.dimen.chooser_max_width);
        final int available = MeasureSpec.getSize(widthMeasureSpec) - 2 * margin;
        final int wanted = Math.min(cap, available);
        final ViewGroup.LayoutParams params = panel.getLayoutParams();
        if (wanted > 0 && params.width != wanted) {
            params.width = wanted;
            panel.setLayoutParams(params);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Swallows every touch the two answers did not take.
     *
     * <p>Nothing behind this may be operated while the question is open. It
     * matters more here than for the start question: the project under the scrim
     * is about to be replaced or kept, and an edit made through it would belong
     * to neither outcome.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }
}
