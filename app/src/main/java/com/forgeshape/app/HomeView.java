package com.forgeshape.app;

import android.content.Context;
import android.view.View;

/**
 * Home: what ForgeShape shows when no project is open (`APP-H1`).
 *
 * <p>Exactly two primary actions, because there are exactly two ways a project
 * comes to exist. <b>New Project</b> asks which representation the project
 * begins in; <b>Open File…</b> reads a {@code .forge} the user picks through the
 * system's own document UI. No recent list, no template gallery, no account —
 * a Home that offered any of those would be advertising something the product
 * does not have.
 *
 * <p>Home is <b>not a project</b>. Behind the scrim the viewport is genuinely
 * empty: no default primitive stands in for the project the user has not
 * started, nothing is drawn, nothing can be picked, no history exists and no
 * {@code .forge} byte, checkpoint or fingerprint describes it. Whether Home is
 * on screen is derived from native truth ({@code projectOpen()}) on every
 * refresh and is never remembered here.
 *
 * <p>It carries its own status line, because at Home the toolbar's is not on
 * screen: a file that could not be opened says so here, and Home stays.
 */
final class HomeView extends ChooserSurfaceView {

    /** Told which way the user chose to begin. */
    interface OnHomeAction {
        void onHomeNewProjectRequested();

        void onHomeOpenFileRequested();
    }

    HomeView(Context context, final OnHomeAction listener) {
        super(context, R.id.home_surface, R.id.home_panel,
                context.getString(R.string.home_title), context.getString(R.string.home_prompt));

        addOption(R.id.home_new_project, R.drawable.ic_add,
                context.getString(R.string.new_project),
                context.getString(R.string.home_new_project_description),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onHomeNewProjectRequested();
                    }
                });
        addOption(R.id.home_open_file, R.drawable.ic_project,
                context.getString(R.string.project_open_file),
                context.getString(R.string.home_open_file_description),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onHomeOpenFileRequested();
                    }
                });
        addStatusLine();
    }
}
