package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.ViewGroup;

/**
 * What Finish Sketch means for a Surface sketch ({@code MODELING-FOUNDATIONS-R1}
 * C): a surface that grows DOWN out of the toolbar, where Finish Sketch is, and
 * offers Patch, Extrude, Revolve, Loft, Trim and Keep as Section — each drawn
 * only while its native candidate would be committed.
 *
 * <p><b>Why not the precision surface.</b> While a sketch is open the sketch's
 * own chrome (the Tool Rail, the orientation navigator, the actions palette)
 * holds the middle of the window, and the bottom sheet is left a strip one
 * row tall — fine for the CAD sketch, whose one commit is pinned, and too
 * small for a choice of six. An anchored surface hangs from the control that
 * opened it, over the model, and scrolls inside a bounded height, so every
 * choice is reachable on a phone.
 *
 * <p>Owns nothing: its body is a {@link SurfaceEditorView}, which reads native
 * on every refresh and holds no candidate.
 */
final class SurfaceFinishView extends AnchoredSurfaceView {

    private final BoundedScrollView scroller;
    private final SurfaceEditorView body;

    SurfaceFinishView(Context context, SurfaceEditorView.Host host) {
        super(context);
        setId(R.id.surface_finish);
        EditorControlStyles.applyContextSurface(this);
        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        setPadding(pad, pad, pad, pad);
        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.surface_finish_title)),
                EditorControlStyles.rowParams(0));
        scroller = new BoundedScrollView(context);
        scroller.setId(R.id.surface_finish_list);
        scroller.setFillViewport(false);
        scroller.setVerticalScrollBarEnabled(true);
        addView(scroller, EditorControlStyles.rowParams(EditorControlStyles.dimen(context, R.dimen.row_gap_small)));
        body = new SurfaceEditorView(context, host, true);
        scroller.addView(body, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    /** The bounded height its body scrolls within, set from the window. */
    void setMaxBodyHeightPx(int px) {
        scroller.setMaxHeightPx(px);
    }

    SurfaceEditorView body() {
        return body;
    }

    static ViewGroup.LayoutParams anchoredParams(Context context, int topOffsetPx) {
        final android.widget.FrameLayout.LayoutParams params =
                new android.widget.FrameLayout.LayoutParams(
                        EditorControlStyles.dimen(context, R.dimen.feature_history_width),
                        ViewGroup.LayoutParams.WRAP_CONTENT);
        // Leading side: the trailing side holds the sketch's own chrome. The
        // workspace re-seats it on every open (placeSurfaceFinish), mirrored
        // for a left-handed layout and stopped short of that chrome.
        params.gravity = Gravity.TOP | Gravity.START;
        params.topMargin = topOffsetPx;
        params.leftMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        return params;
    }
}
