package com.forgeshape.app;

/**
 * How heavily the Construction gizmo's strokes are drawn (`UI-PREF-R1` F).
 *
 * <p>Three bundle recipes native code knows how to author, and nothing in
 * between: the gizmo is a one-pixel line list, so a "thick" stroke is a bundle
 * of parallel lines and the honest choices are the bundles that exist. Regular
 * is byte-identical to the gizmo the product drew before the preference did.
 *
 * <p>Presentation only. Which handle a touch grabs and how far a drag moves a
 * body never read this; native hit testing does not know it exists.
 *
 * <p>Holds no Android type. The native index is the enum's declaration order
 * and is asserted so by {@code DisplaySettingsContractTest}.
 */
enum GizmoStrokeWeight {

    /** The Regular bundle at half the spread. */
    THIN(NativeViewport.GIZMO_STROKE_THIN, R.string.gizmo_weight_thin),

    /** The accepted gizmo, exactly. */
    REGULAR(NativeViewport.GIZMO_STROKE_REGULAR, R.string.gizmo_weight_regular),

    /** A wider, filled bundle. */
    BOLD(NativeViewport.GIZMO_STROKE_BOLD, R.string.gizmo_weight_bold);

    private final int nativeIndex;
    private final int labelRes;

    GizmoStrokeWeight(int nativeIndex, int labelRes) {
        this.nativeIndex = nativeIndex;
        this.labelRes = labelRes;
    }

    /** The closed index that crosses JNI. */
    int nativeIndex() {
        return nativeIndex;
    }

    /** The user-facing name. */
    int labelRes() {
        return labelRes;
    }

    static GizmoStrokeWeight defaultWeight() {
        return REGULAR;
    }

    /** A stored name, or the default for anything unrecognised. */
    static GizmoStrokeWeight fromStoredName(String name) {
        for (GizmoStrokeWeight candidate : values()) {
            if (candidate.name().equals(name)) {
                return candidate;
            }
        }
        return defaultWeight();
    }
}
