package com.forgeshape.app;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;

/**
 * The extrude action DOCK (`CAD-V6-S2-OWNER-CORRECTION-E2E-R1`, HUD3D): ONE
 * compact badge stating the operation, drawn into the four corners native
 * projected from a world rectangle on the extrusion axis, and the ONE touch
 * target that opens the action palette.
 *
 * <p><b>Drawn, not placed.</b> The badge is laid out once in a flat square
 * source box and carried onto the projected quad by a perspective homography
 * ({@link CadHud3dPresentation#homography}), so it foreshortens and turns with
 * the arrow it belongs to. The view's own box is only the bounds of what it
 * draws and claims; it is never scaled or rotated, and nothing here decides
 * where the dock stands — native did, in world space, and hid it whole when it
 * could not be drawn honestly.
 *
 * <p><b>Claims only its shape.</b> A Down inside the projected quad, or inside
 * the 48 dp floor square on its centre, is the dock's; any other Down in its
 * box is declined (returning false), so the viewport — and the sketch cell
 * under the finger — receives it.
 *
 * <p>Holds no domain value: the operation, the refusal and the corners are
 * handed in on every refresh.
 */
final class CadExtrudeDockView extends View {

    private final float floorPx;
    private final Drawable plate;
    private final Drawable refusalRing;
    private Drawable glyph;
    private int glyphRes;
    private ColorStateList glyphTint;
    private boolean refused;

    private CadHud3dPresentation.Dock dock = new CadHud3dPresentation.Dock();
    private final Matrix matrix = new Matrix();
    private final float[] local = new float[8];
    private float side = 1.0f;
    private boolean drawable;

    CadExtrudeDockView(Context context, float floorPx) {
        super(context);
        this.floorPx = floorPx;
        plate = context.getDrawable(R.drawable.bg_hud_panel).mutate();
        refusalRing = context.getDrawable(R.drawable.bg_hud_glyph_invalid).mutate();
        setClickable(true);
        setFocusable(true);
        setWillNotDraw(false);
    }

    /**
     * Binds one frame: the dock native projected (in viewport pixels), the
     * operation's glyph and tint, and whether the candidate would be refused.
     * The caller places this view at {@code dock.left, dock.top}.
     */
    void bind(CadHud3dPresentation.Dock frame, int iconRes, ColorStateList tint,
              boolean candidateRefused) {
        dock = frame;
        if (iconRes != glyphRes || glyph == null) {
            glyph = getContext().getDrawable(iconRes).mutate();
            glyphRes = iconRes;
            glyphTint = null;
        }
        if (tint != glyphTint) {
            glyph.setTintList(tint);
            glyphTint = tint;
        }
        refused = candidateRefused;
        if (!frame.visible) {
            drawable = false;
            invalidate();
            return;
        }
        for (int i = 0; i < 4; i++) {
            local[2 * i] = frame.quad[2 * i] - frame.left;
            local[2 * i + 1] = frame.quad[2 * i + 1] - frame.top;
        }
        side = CadHud3dPresentation.sourceSide(local);
        final float[] values = CadHud3dPresentation.homography(side, side, local);
        drawable = values != null;
        if (drawable) {
            matrix.setValues(values);
        }
        if (getAlpha() != frame.alpha) {
            setAlpha(frame.alpha);
        }
        invalidate();
    }

    /** The dock this view last drew; verification. */
    CadHud3dPresentation.Dock dock() {
        return dock;
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        final int[] state = getDrawableState();
        plate.setState(state);
        refusalRing.setState(state);
        if (glyph != null) {
            glyph.setState(state);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!drawable || glyph == null) {
            return;
        }
        final int s = Math.max(1, Math.round(side));
        canvas.save();
        canvas.concat(matrix);
        plate.setBounds(0, 0, s, s);
        plate.draw(canvas);
        final int inset = Math.round(s * 0.18f);
        if (refused) {
            final int ring = Math.round(s * 0.10f);
            refusalRing.setBounds(ring, ring, s - ring, s - ring);
            refusalRing.draw(canvas);
        }
        glyph.setBounds(inset, inset, s - inset, s - inset);
        glyph.draw(canvas);
        canvas.restore();
    }

    /** Whether the dock claims a point in this view's own coordinates. */
    boolean claimsLocal(float x, float y) {
        return CadHud3dPresentation.claims(dock, x + dock.left, y + dock.top, floorPx);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN
                && !claimsLocal(event.getX(), event.getY())) {
            // Declined: dispatch hands the Down to the next view under the
            // finger, and in the end to the viewport.
            return false;
        }
        return super.onTouchEvent(event);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        // A declined Down must not even reach the click machinery.
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN
                && !claimsLocal(event.getX(), event.getY())) {
            return false;
        }
        return super.dispatchTouchEvent(event);
    }
}
