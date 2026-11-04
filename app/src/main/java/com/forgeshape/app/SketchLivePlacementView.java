package com.forgeshape.app;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

/**
 * The size of the shape a drag is placing, read beside the finger while it is
 * drawn (`WORKBENCH-UX-R1`): a Line's length, a Rectangle's width and height,
 * a Circle's diameter — what Shapr3D and every desktop CAD sketcher show so the
 * user does not have to finish a shape to learn how big it came out.
 *
 * <p>Holds nothing. Native says on every gesture sample whether a drag is
 * placing a shape and where its size reads ({@link NativeViewport#sketchLivePlacement});
 * the label stands above that point so the finger does not cover it, and is
 * GONE the moment the drag ends. It is not interactive, never takes a touch,
 * and none of it is truth: no history step, no `.forge` byte, no fingerprint.
 */
final class SketchLivePlacementView extends TextView {

    private final InspectorHost host;
    private final ViewportAnchorSpace anchorSpace;
    private final double[] live = new double[NativeViewport.SKETCH_LIVE_SIZE];
    private final float lift;

    SketchLivePlacementView(Context context, InspectorHost host, ViewportAnchorSpace anchorSpace) {
        super(context);
        this.host = host;
        this.anchorSpace = anchorSpace;
        final float density = context.getResources().getDisplayMetrics().density;
        // Above the finger by a fingertip and a half, so it reads while drawn.
        lift = 56.0f * density;
        setId(R.id.sketch_live_size);
        setVisibility(GONE);
        setClickable(false);
        setFocusable(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        setSingleLine(true);
        setGravity(Gravity.CENTER);
        setIncludeFontPadding(false);
        setTextSize(TypedValue.COMPLEX_UNIT_PX, EditorControlStyles.dimen(context, R.dimen.text_body));
        setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        EditorControlStyles.applyMediumWeight(this);
        // The same opaque plate the extrude value reads on.
        final GradientDrawable plate = new GradientDrawable();
        plate.setColor((EditorControlStyles.themeColor(context, R.attr.fsSurfaceFloating)
                & 0x00FFFFFF) | 0xEB000000);
        plate.setStroke(Math.max(1, Math.round(density)),
                EditorControlStyles.themeColor(context, R.attr.fsChromeHairline));
        plate.setCornerRadius(6.0f * density);
        setBackground(plate);
        final int padX = Math.round(8.0f * density);
        final int padY = Math.round(4.0f * density);
        setPadding(padX, padY, padX, padY);
    }

    static FrameLayout.LayoutParams anchoredParams() {
        return new FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT);
    }

    /** Re-reads native and places the size, or withdraws it. */
    void refreshFromNative() {
        if (!anchorSpace.ready() || !NativeViewport.sketchLivePlacement(live,
                anchorSpace.viewportWidth(), anchorSpace.viewportHeight())) {
            setVisibility(GONE);
            return;
        }
        final String text = SketchLivePresentation.label((int) live[NativeViewport.SKETCH_LIVE_TOOL],
                live[NativeViewport.SKETCH_LIVE_FIRST], live[NativeViewport.SKETCH_LIVE_SECOND],
                host.uiState().displayUnit());
        if (text == null) {
            setVisibility(GONE);
            return;
        }
        setText(text);
        setVisibility(View.VISIBLE);
        anchorSpace.measureAndPlace(this, (float) live[NativeViewport.SKETCH_LIVE_SCREEN_X],
                (float) live[NativeViewport.SKETCH_LIVE_SCREEN_Y] - lift, 1.0f);
    }
}
