package com.forgeshape.app;

import android.content.Context;
import android.provider.Settings;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The compact display control: how the viewport SHADES the object.
 *
 * <p>Two labelled groups and nothing else — the shading model (Studio Solid or
 * MatCap) and the surface shading (Smooth or Faceted). It is deliberately not a
 * material editor, a preset browser or a light rig: those are later decisions
 * with their own approvals, and a surface that looks like it could grow into one
 * invites exactly that.
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
 */
final class DisplaySettingsPopoverView extends LinearLayout {

    /** Told which display setting was requested; the caller owns what it means. */
    interface OnDisplaySettingChanged {
        void onShadingModelRequested(int model);

        void onSurfaceShadingRequested(int shading);
    }

    /**
     * Short enough to feel immediate rather than animated. A popover that takes
     * longer than this to arrive is a popover the user has already looked away
     * from.
     */
    private static final long OPEN_DURATION_MS = 120L;
    private static final long CLOSE_DURATION_MS = 90L;

    private final TextView studioChip;
    private final TextView matcapChip;
    private final TextView debugChip;
    private final TextView smoothChip;
    private final TextView facetedChip;

    DisplaySettingsPopoverView(Context context, final OnDisplaySettingChanged listener,
                               boolean includeDebugShading) {
        super(context);
        setId(R.id.display_settings_popover);
        setOrientation(VERTICAL);
        setBackground(EditorControlStyles.chromeOverlay(context));

        final int pad = EditorControlStyles.dimen(context, R.dimen.row_gap);
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

        setVisibility(GONE);
    }

    /**
     * Repaints every chip from the values native code reports.
     *
     * <p>Called after any request and on every resume, so the popover shows what
     * is actually in effect rather than what was last tapped — which is the
     * difference that matters when a request was refused.
     */
    void showSettings(int shadingModel, int surfaceShading) {
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
    }

    boolean isOpen() {
        return getVisibility() == VISIBLE;
    }

    /** Opens or closes the popover, animating from the anchor corner. */
    void setOpen(boolean open) {
        if (open == isOpen()) {
            return;
        }
        // Always interruptible: a second tap while the open animation is still
        // running must close it, not queue behind it.
        animate().cancel();

        // The panel grows out of the control that opened it, which sits at the
        // top-right of the workspace.
        setPivotX(getWidth());
        setPivotY(0.0f);

        if (!animationsEnabled()) {
            // The platform's own reduce-motion / developer animator scale says
            // no animation. Honour it exactly: land on the final state, do not
            // run a shortened one.
            setAlpha(1.0f);
            setScaleX(1.0f);
            setScaleY(1.0f);
            setVisibility(open ? VISIBLE : GONE);
            return;
        }

        if (open) {
            setAlpha(0.0f);
            setScaleX(0.92f);
            setScaleY(0.92f);
            setVisibility(VISIBLE);
            animate().alpha(1.0f).scaleX(1.0f).scaleY(1.0f).setDuration(OPEN_DURATION_MS).start();
        } else {
            animate().alpha(0.0f).scaleX(0.96f).scaleY(0.96f).setDuration(CLOSE_DURATION_MS)
                    .withEndAction(new Runnable() {
                        @Override
                        public void run() {
                            setVisibility(GONE);
                            // Left in the resting state so the next open starts
                            // from a known transform rather than from wherever
                            // a cancelled animation stopped.
                            setAlpha(1.0f);
                            setScaleX(1.0f);
                            setScaleY(1.0f);
                        }
                    }).start();
        }
    }

    /** Closes immediately, with no animation. Used when chrome is hidden. */
    void closeImmediately() {
        animate().cancel();
        setVisibility(GONE);
        setAlpha(1.0f);
        setScaleX(1.0f);
        setScaleY(1.0f);
    }

    /**
     * Whether the system wants animations at all.
     *
     * <p>A zero animator duration scale is set by the "Remove animations"
     * accessibility option and by the developer-options scale, and it means
     * exactly what it says. Reading it per use rather than caching it is
     * deliberate: the setting can change while the app is running.
     */
    private boolean animationsEnabled() {
        final float scale = Settings.Global.getFloat(getContext().getContentResolver(),
                Settings.Global.ANIMATOR_DURATION_SCALE, 1.0f);
        return scale > 0.0f;
    }

    /**
     * Swallows every touch its own chips did not take, so reaching for a display
     * setting never orbits the camera behind the panel.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
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
