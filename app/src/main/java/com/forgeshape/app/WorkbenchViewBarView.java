package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

/**
 * The workbench's view bar ({@code WORKBENCH-RIBBON-R1}): Fit, the Home view,
 * Front, Top and Right, and the projection — one floating capsule standing on
 * the model, as SolidCraft's and Shapr3D's do.
 *
 * <p>Every button is one call into native's camera (the core the desktop
 * shares), and the projection button shows the projection native reports on
 * every refresh, so the Display popover's projection chips and this button
 * cannot become two answers.
 */
final class WorkbenchViewBarView extends LinearLayout {

    interface Host {
        void onStandardViewChosen(int view);

        void onFitAllRequested();

        void onProjectionToggleRequested();
    }

    private final ImageView projection;

    WorkbenchViewBarView(Context context, final Host host) {
        super(context);
        setId(R.id.view_bar);
        setOrientation(HORIZONTAL);
        final LinearLayout group = EditorControlStyles.controlGroup(context);
        addView(group, new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        final int gap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);
        addButton(context, group, R.id.view_fit, R.drawable.ic_view_fit, R.string.view_fit, 0,
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        host.onFitAllRequested();
                    }
                });
        addButton(context, group, R.id.view_home, R.drawable.ic_view_home, R.string.view_home, gap,
                view(host, NativeViewport.VIEW_HOME));
        addButton(context, group, R.id.view_front, R.drawable.ic_view_front, R.string.view_front,
                gap, view(host, NativeViewport.VIEW_FRONT));
        addButton(context, group, R.id.view_top, R.drawable.ic_view_top, R.string.view_top, gap,
                view(host, NativeViewport.VIEW_TOP));
        addButton(context, group, R.id.view_right, R.drawable.ic_view_right, R.string.view_right,
                gap, view(host, NativeViewport.VIEW_RIGHT));
        projection = addButton(context, group, R.id.view_projection,
                R.drawable.ic_projection_perspective, R.string.view_projection_to_orthographic,
                gap, new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        host.onProjectionToggleRequested();
                    }
                });
        refreshFromNative();
    }

    private static OnClickListener view(final Host host, final int which) {
        return new OnClickListener() {
            @Override
            public void onClick(View v) {
                host.onStandardViewChosen(which);
            }
        };
    }

    private static ImageView addButton(Context context, LinearLayout group, int id, int icon,
                                       int description, int leftMargin, OnClickListener listener) {
        final ImageView button = EditorControlStyles.iconButton(context, id, icon,
                context.getString(description));
        button.setOnClickListener(listener);
        group.addView(button, EditorControlStyles.iconButtonParams(context, leftMargin));
        return button;
    }

    /** Shows the projection native reports: the button names the switch it makes. */
    void refreshFromNative() {
        final boolean ortho = NativeViewport.projectionMode() == NativeViewport.PROJECTION_ORTHOGRAPHIC;
        projection.setImageResource(ortho ? R.drawable.ic_projection_orthographic
                : R.drawable.ic_projection_perspective);
        projection.setContentDescription(getContext().getString(ortho
                ? R.string.view_projection_to_perspective
                : R.string.view_projection_to_orthographic));
    }
}
