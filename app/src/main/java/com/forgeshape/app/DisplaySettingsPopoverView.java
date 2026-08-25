package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The compact display control: how the viewport PRESENTS the object.
 *
 * <p>Five labelled groups and nothing else — the shading model (Studio Solid or
 * MatCap), the surface shading (Smooth or Faceted), the camera projection
 * (Perspective or Orthographic), the <b>view</b> (the world reference grid) and
 * the appearance (one of the three palettes). It is deliberately not a material
 * editor, a preset browser, a light rig or a view-cube: those are later
 * decisions with their own approvals, and a surface that looks like it could
 * grow into one invites exactly that.
 *
 * <p><b>Every control here works.</b> Nothing in this popover is drawn disabled
 * as a promise — the Tool Rail carries reserved entries because a rail is a map
 * of the product, and a settings surface is not. That is why the View group
 * holds one chip pair and not five: a selection outline, a view cube and named
 * views arrive in this group when they arrive, and not before.
 *
 * <p>Projection is the odd member and is here on purpose. It is <em>camera</em>
 * state rather than a display setting — native code keeps it with the camera
 * pose, not in the display store — but it is the other control that changes how
 * the object reads without changing what it is, and giving it its own chrome
 * surface would cost the user a second place to look for the same kind of
 * decision.
 *
 * <p><b>Owns no state.</b> Every chip reports a request; native code decides,
 * and {@link #showSettings} repaints from what native code reports afterwards.
 * That is why the settings survive a HOME/resume with no save/restore code here
 * — the truth was never in this view to begin with.
 *
 * <h2>Motion</h2>
 *
 * <p>Three patterns, taken from how creative apps on iOS handle a compact
 * settings surface (Craft's line-style popover, Apple Mail's shape popover,
 * Freeform's alignment settings):
 *
 * <ul>
 *   <li><b>The panel grows from its anchor</b> rather than sliding in from a
 *       screen edge, so it reads as belonging to the button that opened it.
 *   <li><b>Selection feedback happens in place.</b> Choosing Faceted repaints
 *       one chip; the popover does not move, re-animate or close. Freeform's
 *       alignment menu does the same thing, and it is what makes trying several
 *       options feel like one continuous act instead of four separate ones.
 *   <li><b>The surface is not modal</b> and stays open across several changes.
 * </ul>
 *
 * <p>Every animation here is short, interruptible, and cancelled outright when
 * the system animator scale is zero — the platform's own "reduce motion"
 * signal. Nothing here is ever on the path of a stylus sample: the popover is a
 * chrome surface, and a viewport gesture never touches it.
 *
 * <p><b>None of that motion is written here any more.</b> This popover was the
 * one surface that had already solved the anchor-pivot problem, so at UI-R4B
 * the solution was lifted out of it into {@link AnchoredSurfaceView} and the
 * other three surfaces were moved onto it. The only thing this class still says
 * about its own motion is which edge it hangs off, which is a fact about where
 * it is anchored rather than about how it moves.
 */
final class DisplaySettingsPopoverView extends AnchoredSurfaceView {

    /** Told which display setting was requested; the caller owns what it means. */
    interface OnDisplaySettingChanged {
        void onShadingModelRequested(int model);

        void onSurfaceShadingRequested(int shading);

        void onProjectionModeRequested(int mode);

        void onAppThemeRequested(AppTheme theme);

        void onGridVisibleRequested(boolean visible);
    }

    private final TextView studioChip;
    private final TextView matcapChip;
    private final TextView debugChip;
    private final TextView smoothChip;
    private final TextView facetedChip;
    private final TextView perspectiveChip;
    private final TextView orthographicChip;
    private final TextView gridOnChip;
    private final TextView gridOffChip;

    /**
     * The appearance options, in {@link AppTheme} declaration order.
     *
     * <p>Three parallel arrays rather than three fields, because there is
     * nothing per-palette to say: every option is the same control with a
     * different name and a different {@code AppTheme}, and a fourth palette
     * would be one more entry rather than one more block of code. The view ids
     * stay explicit and stable, because verification locates a control by its
     * semantic id and never by position.
     */
    private static final AppTheme[] APPEARANCES = {
            AppTheme.WARM_GRAPHITE, AppTheme.NEUTRAL_CHARCOAL, AppTheme.LIGHT_CHARCOAL};
    private static final int[] APPEARANCE_IDS = {
            R.id.appearance_warm_graphite, R.id.appearance_neutral_charcoal,
            R.id.appearance_light_charcoal};
    private static final int[] APPEARANCE_LABELS = {
            R.string.appearance_warm_graphite, R.string.appearance_neutral_charcoal,
            R.string.appearance_light_charcoal};

    private final TextView[] appearanceOptions;

    DisplaySettingsPopoverView(Context context, final OnDisplaySettingChanged listener,
                               boolean includeDebugShading) {
        super(context);
        setId(R.id.display_settings_popover);
        // It hangs UNDER the toolbar's Display control in every window, so it
        // never unfolds upward. Stated once here rather than pushed in by the
        // workspace, because unlike the three surfaces that grow out of the
        // Objects capsule, this one's anchor cannot move.
        setGrowsUpward(false);
        // TIER 2. It carries five labelled groups of prose-named options, which
        // is a body of content to be read rather than a capsule to be glanced
        // at, so it is opaque where the rail and the toolbar groups are not.
        EditorControlStyles.applyContextSurface(this);

        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        final int gap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);
        setPadding(pad, pad, pad, pad);

        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.shading)),
                EditorControlStyles.rowParams(0));

        final LinearLayout shadingRow = new LinearLayout(context);
        shadingRow.setOrientation(HORIZONTAL);
        addView(shadingRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        studioChip = EditorControlStyles.chip(context, R.id.display_mode_studio,
                context.getString(R.string.shading_studio));
        studioChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onShadingModelRequested(NativeViewport.SHADING_STUDIO);
            }
        });
        // Chips are sized to their labels rather than sharing the row equally.
        // An equal share divides whatever width the popover happens to get, and
        // with three chips that clipped "MatCap" to "MatCa" — a control whose
        // own name does not fit is worse than a slightly ragged row.
        shadingRow.addView(studioChip, EditorControlStyles.wrap(0));

        matcapChip = EditorControlStyles.chip(context, R.id.display_mode_matcap,
                context.getString(R.string.shading_matcap));
        matcapChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onShadingModelRequested(NativeViewport.SHADING_MATCAP);
            }
        });
        shadingRow.addView(matcapChip, EditorControlStyles.wrap(gap));

        // The source-colour path exists so a before/after comparison is one tap
        // away rather than a second build. It is a diagnostic, so it appears
        // only in a debuggable build and is never the default; a release user
        // has no way to reach it and nothing to be confused by.
        if (includeDebugShading) {
            debugChip = EditorControlStyles.chip(context, R.id.display_mode_debug,
                    context.getString(R.string.shading_debug));
            debugChip.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    listener.onShadingModelRequested(NativeViewport.SHADING_DEBUG_SOURCE_COLOR);
                }
            });
            shadingRow.addView(debugChip, EditorControlStyles.wrap(gap));
        } else {
            debugChip = null;
        }

        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.surface)),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));

        final LinearLayout surfaceRow = new LinearLayout(context);
        surfaceRow.setOrientation(HORIZONTAL);
        addView(surfaceRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        smoothChip = EditorControlStyles.chip(context, R.id.surface_shading_smooth,
                context.getString(R.string.surface_smooth));
        smoothChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onSurfaceShadingRequested(NativeViewport.SURFACE_SMOOTH);
            }
        });
        surfaceRow.addView(smoothChip, EditorControlStyles.wrap(0));

        facetedChip = EditorControlStyles.chip(context, R.id.surface_shading_faceted,
                context.getString(R.string.surface_faceted));
        facetedChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onSurfaceShadingRequested(NativeViewport.SURFACE_FACETED);
            }
        });
        surfaceRow.addView(facetedChip, EditorControlStyles.wrap(gap));

        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.projection)),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));

        // The two projection labels are long, so they get their own row rather
        // than sharing one with anything else. Like the shading chips they are
        // sized to their labels: "Orthographic" clipped to "Orthograph" would be
        // exactly the failure the comment above the shading row describes.
        final LinearLayout projectionRow = new LinearLayout(context);
        projectionRow.setOrientation(HORIZONTAL);
        addView(projectionRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        perspectiveChip = EditorControlStyles.chip(context, R.id.projection_perspective,
                context.getString(R.string.projection_perspective));
        perspectiveChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onProjectionModeRequested(NativeViewport.PROJECTION_PERSPECTIVE);
            }
        });
        projectionRow.addView(perspectiveChip, EditorControlStyles.wrap(0));

        orthographicChip = EditorControlStyles.chip(context, R.id.projection_orthographic,
                context.getString(R.string.projection_orthographic));
        orthographicChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onProjectionModeRequested(NativeViewport.PROJECTION_ORTHOGRAPHIC);
            }
        });
        projectionRow.addView(orthographicChip, EditorControlStyles.wrap(gap));

        // View: what the viewport draws BESIDES the model.
        //
        // A group rather than a lone chip, because what will join it later is
        // the same kind of thing — a selection outline, a view cube, named
        // views. What it must NOT do is show them now: a disabled control
        // promising a feature that does not exist is a worse answer than an
        // absent one, and it would also start turning this popover into the
        // settings screen it was designed not to be. In UI-R1C2 the group
        // holds exactly one working control.
        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.view)),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));

        final LinearLayout viewRow = new LinearLayout(context);
        viewRow.setOrientation(HORIZONTAL);
        viewRow.setGravity(Gravity.CENTER_VERTICAL);
        addView(viewRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        // The caption carries the name so the two chips can be plain On and
        // Off. Two chips each reading "Grid ..." would say the word twice and
        // make the row wider than the popover wants to be.
        final TextView gridLabel =
                EditorControlStyles.fieldLabel(context, context.getString(R.string.view_grid));
        viewRow.addView(gridLabel, EditorControlStyles.wrap(0));

        gridOnChip = EditorControlStyles.chip(context, R.id.view_grid_on,
                context.getString(R.string.view_grid_on));
        // Named for verification and for a screen reader, which see "On" with
        // no idea what it turns on: the caption beside it is a sibling view,
        // not part of the chip.
        gridOnChip.setContentDescription(context.getString(R.string.view_grid) + " "
                + context.getString(R.string.view_grid_on));
        gridOnChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onGridVisibleRequested(true);
            }
        });
        viewRow.addView(gridOnChip, EditorControlStyles.wrap(gap));

        gridOffChip = EditorControlStyles.chip(context, R.id.view_grid_off,
                context.getString(R.string.view_grid_off));
        gridOffChip.setContentDescription(context.getString(R.string.view_grid) + " "
                + context.getString(R.string.view_grid_off));
        gridOffChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onGridVisibleRequested(false);
            }
        });
        viewRow.addView(gridOffChip, EditorControlStyles.wrap(gap));

        // Appearance sits with the other three because it is the same kind of
        // decision: it changes how the model READS and nothing about what it is.
        // Giving the palettes their own settings screen would cost the user a
        // second place to look for one act, and this popover is already the
        // product's answer to "how is this drawn".
        //
        // A COLUMN of full-width rows rather than a row of chips, and that is
        // about the content rather than about taste: the three palettes have
        // real names, they are mutually exclusive, and three chips carrying
        // "Neutral Charcoal" would either wrap, ellipsise or make the popover
        // wider than everything else in it. A short list of named options is
        // also how a palette is chosen in every tool that has palettes.
        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.appearance)),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));

        appearanceOptions = new TextView[APPEARANCES.length];
        for (int i = 0; i < APPEARANCES.length; i++) {
            final AppTheme theme = APPEARANCES[i];
            final TextView option = EditorControlStyles.listRow(context, APPEARANCE_IDS[i],
                    context.getString(APPEARANCE_LABELS[i]));
            option.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    listener.onAppThemeRequested(theme);
                }
            });
            final LinearLayout.LayoutParams params = EditorControlStyles.rowParams(
                    EditorControlStyles.dimen(context,
                            i == 0 ? R.dimen.row_gap_small : R.dimen.row_gap_small));
            params.width = ViewGroup.LayoutParams.MATCH_PARENT;
            addView(option, params);
            appearanceOptions[i] = option;
        }

        setVisibility(GONE);
    }

    /**
     * Repaints every chip from the values native code reports.
     *
     * <p>Called after any request and on every resume, so the popover shows what
     * is actually in effect rather than what was last tapped — which is the
     * difference that matters when a request was refused.
     */
    void showSettings(int shadingModel, int surfaceShading, int projectionMode,
                      AppTheme theme, boolean gridVisible) {
        // Read back from native truth like every other chip here, never from
        // what was tapped: the grid's visibility is process-scoped native
        // presentation state, so on a resume it is already whatever it was and
        // this only makes the control agree with it.
        EditorControlStyles.setChipActive(gridOnChip, gridVisible);
        EditorControlStyles.setChipActive(gridOffChip, !gridVisible);
        // The appearance is UI truth rather than native truth, so it is passed
        // in like the rest instead of being read here: this view owns nothing
        // and reports what it is told, whichever layer the answer came from.
        for (int i = 0; i < APPEARANCES.length; i++) {
            EditorControlStyles.setListRowActive(appearanceOptions[i], APPEARANCES[i] == theme);
        }
        EditorControlStyles.setChipActive(studioChip, shadingModel == NativeViewport.SHADING_STUDIO);
        EditorControlStyles.setChipActive(matcapChip, shadingModel == NativeViewport.SHADING_MATCAP);
        if (debugChip != null) {
            EditorControlStyles.setChipActive(debugChip,
                    shadingModel == NativeViewport.SHADING_DEBUG_SOURCE_COLOR);
        }
        EditorControlStyles.setChipActive(smoothChip,
                surfaceShading == NativeViewport.SURFACE_SMOOTH);
        EditorControlStyles.setChipActive(facetedChip,
                surfaceShading == NativeViewport.SURFACE_FACETED);
        EditorControlStyles.setChipActive(perspectiveChip,
                projectionMode == NativeViewport.PROJECTION_PERSPECTIVE);
        EditorControlStyles.setChipActive(orthographicChip,
                projectionMode == NativeViewport.PROJECTION_ORTHOGRAPHIC);
    }

    /**
     * It hangs off the Display control at the workspace's TRAILING top corner,
     * which is the one thing about this surface's growth that is its own.
     *
     * <p>It never unfolds upward: the control that opens it is in the toolbar,
     * so the panel always hangs below it, and {@link AnchoredSurfaceView} takes
     * the leading-edge case for the three surfaces that grow out of a capsule
     * low in the window.
     */
    @Override
    boolean anchoredToTrailingEdge() {
        return true;
    }

    /** Layout params that hang the popover under the Global Toolbar's right end. */
    static ViewGroup.LayoutParams anchoredParams(Context context, int topOffsetPx) {
        final android.widget.FrameLayout.LayoutParams params =
                new android.widget.FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.TOP | Gravity.END;
        params.topMargin = topOffsetPx;
        params.rightMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        return params;
    }
}
