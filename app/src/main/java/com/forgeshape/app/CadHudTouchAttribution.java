package com.forgeshape.app;

/**
 * Names the canvas CAD HUD surface that CONSUMED a Down, for debug-only
 * evidence (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`).
 *
 * <p>A Down a HUD view takes never reaches the viewport, so a sketch cell under
 * it cannot be tapped; on a physical device that looks exactly like a picking
 * miss. The debug build logs {@code FORGESHAPE_CAD_HUD_TOUCH:<surface>} so a
 * logcat capture can tell the two apart. Attribution only: it decides nothing,
 * claims nothing and changes no hit area. Pure, so the JVM pins it.
 */
final class CadHudTouchAttribution {
    /** No HUD surface stands under the point. */
    static final int NONE = 0;
    /** A value label (either side's), an invisible ≥ 48 dp band around its text. */
    static final int VALUE = 1;
    /** The action dock: its projected badge or the 48 dp floor on its centre. */
    static final int DOCK = 2;
    /** The open action palette. */
    static final int PALETTE = 3;
    /** An open typed-value editor. */
    static final int EDITOR = 4;
    /** The retained-sketch Edit Sketch control. */
    static final int EDIT_SKETCH = 5;

    static final String TOKEN_PREFIX = "FORGESHAPE_CAD_HUD_TOUCH:";

    private CadHudTouchAttribution() {
    }

    /**
     * The surface name for a Down, or null when nothing should be logged: the
     * HUD did not consume it (the viewport got it), or no known surface stands
     * there.
     */
    static String surfaceFor(boolean consumed, int surface) {
        if (!consumed) {
            return null;
        }
        switch (surface) {
            case VALUE: return "value";
            case DOCK: return "dock";
            case PALETTE: return "palette";
            case EDITOR: return "editor";
            case EDIT_SKETCH: return "edit_sketch";
            default: return null;
        }
    }

    /** The full log line, or null when {@link #surfaceFor} is null. */
    static String token(boolean consumed, int surface) {
        final String name = surfaceFor(consumed, surface);
        return name == null ? null : TOKEN_PREFIX + name;
    }
}
