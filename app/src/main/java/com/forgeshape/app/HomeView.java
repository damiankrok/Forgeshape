package com.forgeshape.app;

import android.content.Context;
import android.view.View;

/**
 * Home: what ForgeShape shows when no project is open (`APP-H1`).
 *
 * <p><b>A full-screen start page</b> (`SKETCH-UX-R1` A1), not a card over the
 * viewport. Home is not a project, and there is nothing behind it worth showing
 * through — the viewport at Home is genuinely empty, so a partial scrim was
 * revealing emptiness and the panel on it read as a dialog over a broken
 * editor. The page owns the whole window, carries the product mark and name at
 * the top, and the editor's chrome is not drawn at all while it stands.
 *
 * <p>Exactly two primary actions, because there are exactly two ways a project
 * comes to exist. <b>New Project</b> asks which representation the project
 * begins in; <b>Open File…</b> reads a {@code .forge} the user picks through the
 * system's own document UI. No recent list, no template gallery, no account —
 * a Home that offered any of those would be advertising something the product
 * does not have. Beneath them, and quieter, <b>Settings</b> (`UI-PREF-R1` A1)
 * opens the persistent application preferences: not a way to have a project,
 * which is why it is a secondary action at the foot rather than a third row.
 *
 * <p>Behind the page nothing is drawn, nothing can be picked, no history exists
 * and no {@code .forge} byte, checkpoint or fingerprint describes it. Whether
 * Home is on screen is derived from native truth ({@code projectOpen()}) on
 * every refresh and is never remembered here.
 *
 * <p>It carries its own status line, because at Home the toolbar's is not on
 * screen: a file that could not be opened says so here, and Home stays.
 */
final class HomeView extends StartPageView {

    /** Told which way the user chose to begin. */
    interface OnHomeAction {
        void onHomeNewProjectRequested();

        void onHomeOpenFileRequested();

        /** `UI-PREF-R1`: open the Settings page over Home. */
        void onHomeSettingsRequested();
    }

    HomeView(Context context, final OnHomeAction listener) {
        super(context, R.id.home_surface, R.id.home_panel,
                context.getString(R.string.home_title), context.getString(R.string.home_prompt));

        addAction(R.id.home_new_project, R.drawable.ic_add,
                context.getString(R.string.new_project),
                context.getString(R.string.home_new_project_description),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onHomeNewProjectRequested();
                    }
                });
        addAction(R.id.home_open_file, R.drawable.ic_project,
                context.getString(R.string.project_open_file),
                context.getString(R.string.home_open_file_description),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onHomeOpenFileRequested();
                    }
                });
        addStatusLine();
        addSecondaryAction(R.id.home_settings, context.getString(R.string.settings),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onHomeSettingsRequested();
                    }
                }).setContentDescription(context.getString(R.string.settings) + ". "
                + context.getString(R.string.settings_home_description));
    }
}
