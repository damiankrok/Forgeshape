package com.forgeshape.app;

import android.content.Context;
import android.view.View;

/**
 * The question asked when leaving a project that has changes nobody saved.
 *
 * <p>Three answers, because there are exactly three honest ones. <b>Save</b>
 * writes the project to the app's own slot — the same act as Save Project —
 * and only then continues; if the save fails the project stays open and the
 * failure is said. <b>Discard</b> continues without writing anything: the
 * changes are lost, and the recovery checkpoint that was protecting them is
 * retired so the next launch does not offer back what the user chose to
 * drop. <b>Cancel</b> stays in the project, which is left exactly as it was.
 *
 * <p>What "continue" means — a new project, or opening a file — is the
 * workspace's to remember; this surface asks the question and reports the
 * answer. It is asked only when the project is genuinely dirty: a project
 * whose fingerprint still matches what was last saved, opened or recovered
 * is left without a word.
 */
final class UnsavedChangesPromptView extends ChooserSurfaceView {

    /** Told what the user decided about the unsaved changes. */
    interface OnUnsavedChoice {
        void onUnsavedSaveRequested();

        void onUnsavedDiscardRequested();

        void onUnsavedCancelRequested();
    }

    UnsavedChangesPromptView(Context context, final OnUnsavedChoice listener) {
        super(context, R.id.unsaved_prompt, R.id.unsaved_prompt_panel,
                context.getString(R.string.unsaved_title),
                context.getString(R.string.unsaved_prompt));

        addOption(R.id.unsaved_save, R.drawable.ic_project,
                context.getString(R.string.unsaved_save_title),
                context.getString(R.string.unsaved_save_description),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onUnsavedSaveRequested();
                    }
                });
        addOption(R.id.unsaved_discard, R.drawable.ic_delete,
                context.getString(R.string.unsaved_discard_title),
                context.getString(R.string.unsaved_discard_description),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onUnsavedDiscardRequested();
                    }
                });
        addStatusLine();
        addSecondaryAction(R.id.unsaved_cancel, context.getString(R.string.unsaved_cancel_title),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onUnsavedCancelRequested();
                    }
                });
    }
}
