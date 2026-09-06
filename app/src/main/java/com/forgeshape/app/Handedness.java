package com.forgeshape.app;

/**
 * Which edge the persistent contextual rail stands on (`UI-PREF-R1` C,
 * UI-SPEC-R0 Rev1).
 *
 * <p><b>Presentation, and only edge anchoring.</b> Left mirrors the trailing
 * edge zone of the Editor Workspace — the Tool Rail host, the side-placed
 * precision surface, the Sculpt brush controls and the expanded window's
 * Objects column — to the other side of the window. It mirrors nothing else:
 * no CAD coordinate, no world axis, no workplane, no camera, no gizmo
 * arithmetic, no object transform, no exported byte and no gesture meaning.
 * A left-handed user's model is the same model seen from the same camera.
 *
 * <p>Right is the default and reproduces the accepted `UI-LAYOUT-R2` geometry
 * exactly; nothing is stored for it.
 *
 * <p>Holds no Android type, so the mapping is unit-testable on the JVM.
 */
enum Handedness {

    /** The accepted right-edge rail. First, so it is the fallback. */
    RIGHT,

    /** The same rail zone mirrored to the left edge, 8 dp off it. */
    LEFT;

    /** The product default, named rather than assumed at each call site. */
    static Handedness defaultHandedness() {
        return RIGHT;
    }

    /**
     * Maps a stored name back to a member, or to the default.
     *
     * <p>By NAME rather than ordinal, so a stored value survives a reordering
     * of this enum and an unknown name — a future member, a corrupt file — is
     * the default rather than an exception on launch.
     */
    static Handedness fromStoredName(String name) {
        for (Handedness candidate : values()) {
            if (candidate.name().equals(name)) {
                return candidate;
            }
        }
        return defaultHandedness();
    }
}
