package com.forgeshape.app;

import android.content.Context;
import android.view.View;

/**
 * The one question a new project asks: which representation does it begin in?
 *
 * <p>Exactly two answers, because the product has exactly two ways to make a
 * first body. <b>CAD</b> enters the transient CAD bootstrap — the spatial
 * world-plane chooser, then one sketch — and the first Extrude creates the
 * project. <b>Sculpt</b> seeds a sphere already prepared for a brush, through
 * the product's own Freeze. Neither is a document, a template or a saved
 * project, and both end in the ordinary workspace.
 *
 * <p>Reached from Home and from an open project's Project surface alike; from
 * an open project it stands over the workspace the user is leaving, and Cancel
 * (or System Back) returns to it unchanged.
 *
 * <p>It owns no state and makes no native call. It reports which option was
 * pressed; {@link EditorWorkspaceView} owns what that means.
 */
final class NewProjectChooserView extends ChooserSurfaceView {

    /** Told which way the user chose to begin, or that they chose not to. */
    interface OnNewProjectChoice {
        void onNewCadProjectChosen();

        void onNewSculptProjectChosen();

        void onNewProjectCancelled();
    }

    NewProjectChooserView(Context context, final OnNewProjectChoice listener) {
        super(context, R.id.new_project_chooser, R.id.new_project_chooser_panel,
                context.getString(R.string.new_project),
                context.getString(R.string.new_project_prompt));

        addOption(R.id.new_project_cad, R.drawable.ic_sketch_new,
                context.getString(R.string.new_project_cad_title),
                context.getString(R.string.new_project_cad_description),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onNewCadProjectChosen();
                    }
                });
        addOption(R.id.new_project_sculpt, R.drawable.ic_start_sculpt,
                context.getString(R.string.new_project_sculpt_title),
                context.getString(R.string.new_project_sculpt_description),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onNewSculptProjectChosen();
                    }
                });
        addStatusLine();
        addSecondaryAction(R.id.new_project_cancel, context.getString(R.string.cancel),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onNewProjectCancelled();
                    }
                });
    }
}
