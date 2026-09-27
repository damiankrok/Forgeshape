package com.forgeshape.app;

/**
 * The application's persistent preferences (`UI-PREF-R1`, UI-OWNER-37): one
 * immutable value carrying everything the Settings hub owns.
 *
 * <p><b>App-level truth, never project truth.</b> A preference describes how
 * ForgeShape presents itself to this user on this device — which palette, which
 * edge the rail stands on, how the gizmo is drawn, whether canvas icons carry
 * captions. It enters no {@code .forge} byte, changes no project fingerprint,
 * dirties no project, records no Construction or Sculpt history step, touches
 * no {@code ObjectId} and never reaches the checkpoint or the recovery
 * document. {@code SettingsPreferencesTest}
 * serialises a project before and after changing every field here and asserts
 * the bytes are identical.
 *
 * <p><b>Versioned and forgiving.</b> {@link #SCHEMA_VERSION} names the shape of
 * what is stored. Reading follows one documented rule per field: an unknown
 * enum name is the default; a non-finite number is the default; a finite number
 * outside its range is CLAMPED to the nearer bound; a missing key, or one
 * holding the wrong type, is the exact product default; and a key this reader
 * does not know is ignored, so a newer
 * build's file never crashes an older reader. There is no migration framework:
 * every field has a default, and that is the whole migration.
 *
 * <p>Holds no Android type, so the rules are unit-testable on the JVM. Where
 * the values live on disk is {@link AppPreferencesStore}'s business alone.
 */
final class AppPreferences {

    /** The shape of the stored record. Bumped only when a field's meaning changes. */
    static final int SCHEMA_VERSION = 1;

    /**
     * The gizmo's visual size bounds, restated from the domain's constants so a
     * JVM case can hold them without loading the native library. The native
     * contract test pins that they agree.
     */
    static final float GIZMO_VISUAL_SCALE_MIN = 0.9f;
    static final float GIZMO_VISUAL_SCALE_DEFAULT = 1.0f;
    static final float GIZMO_VISUAL_SCALE_MAX = 1.5f;

    /**
     * The visual sizes the Settings hub OFFERS, as presets on the bounded
     * range: the two bounds, the default, and one step above it. Presets rather
     * than a free slider, so every offered value is one that has been looked at
     * on a screen and each is reachable with one tap.
     */
    static final float[] GIZMO_VISUAL_SCALE_PRESETS = {0.9f, 1.0f, 1.25f, 1.5f};

    private final AppTheme palette;
    private final Handedness handedness;
    private final float gizmoVisualScale;
    private final GizmoStrokeWeight gizmoStrokeWeight;
    /**
     * Whether the canvas's icon controls also draw a one-word caption (Tool
     * Labels, `CAD-VERTICAL-SLICE-R1`). Off by default: the icons carry their
     * meaning by shape and by accessible name, and a caption is an aid the
     * user asks for, not a cost every user pays in canvas width.
     */
    private final boolean toolLabels;

    /** Tool Labels' product default: icons only. */
    static final boolean TOOL_LABELS_DEFAULT = false;

    private AppPreferences(AppTheme palette, Handedness handedness, float gizmoVisualScale,
                           GizmoStrokeWeight gizmoStrokeWeight, boolean toolLabels) {
        this.palette = palette;
        this.handedness = handedness;
        this.gizmoVisualScale = gizmoVisualScale;
        this.gizmoStrokeWeight = gizmoStrokeWeight;
        this.toolLabels = toolLabels;
    }

    /** Exactly the product as it shipped before preferences existed. */
    static AppPreferences defaults() {
        return new AppPreferences(AppTheme.defaultTheme(), Handedness.defaultHandedness(),
                GIZMO_VISUAL_SCALE_DEFAULT, GizmoStrokeWeight.defaultWeight(),
                TOOL_LABELS_DEFAULT);
    }

    /**
     * Rebuilds a value from what a store read, applying the fallback rules.
     *
     * <p>Every argument is what the store found or {@code null} / NaN when the
     * key was absent, so "missing" and "unrecognised" both land on the default
     * through the same path. The schema version is accepted for any value: a
     * newer schema is read with this build's rules, and a field this build
     * does not know is simply not asked for.
     *
     * <p>{@code toolLabels} is a {@code Boolean} for the same reason the names
     * are strings: {@code null} is "the key was absent or held the wrong type",
     * and that is the product default — which is also why adding the field
     * needed no schema bump. A record written before it existed simply has no
     * such key.
     */
    static AppPreferences fromStored(int schemaVersion, String paletteName,
                                     String handednessName, float gizmoVisualScale,
                                     String gizmoStrokeWeightName, Boolean toolLabels) {
        return new AppPreferences(AppTheme.fromStoredName(paletteName),
                Handedness.fromStoredName(handednessName),
                clampGizmoVisualScale(gizmoVisualScale),
                GizmoStrokeWeight.fromStoredName(gizmoStrokeWeightName),
                toolLabels == null ? TOOL_LABELS_DEFAULT : toolLabels.booleanValue());
    }

    /**
     * The ONE numeric rule: non-finite is the default, finite is clamped.
     *
     * <p>Clamped rather than refused, because a stored size that has drifted
     * a little past a bound — an older build with a wider range, a hand-edited
     * file — is still a size the user meant, and the nearer bound is closer to
     * it than the default is. A NaN or an infinity meant nothing, so it gets
     * the default.
     */
    static float clampGizmoVisualScale(float scale) {
        if (Float.isNaN(scale) || Float.isInfinite(scale)) {
            return GIZMO_VISUAL_SCALE_DEFAULT;
        }
        if (scale < GIZMO_VISUAL_SCALE_MIN) {
            return GIZMO_VISUAL_SCALE_MIN;
        }
        if (scale > GIZMO_VISUAL_SCALE_MAX) {
            return GIZMO_VISUAL_SCALE_MAX;
        }
        return scale;
    }

    AppTheme palette() {
        return palette;
    }

    Handedness handedness() {
        return handedness;
    }

    float gizmoVisualScale() {
        return gizmoVisualScale;
    }

    GizmoStrokeWeight gizmoStrokeWeight() {
        return gizmoStrokeWeight;
    }

    boolean toolLabels() {
        return toolLabels;
    }

    AppPreferences withPalette(AppTheme value) {
        return new AppPreferences(value == null ? AppTheme.defaultTheme() : value, handedness,
                gizmoVisualScale, gizmoStrokeWeight, toolLabels);
    }

    AppPreferences withHandedness(Handedness value) {
        return new AppPreferences(palette,
                value == null ? Handedness.defaultHandedness() : value, gizmoVisualScale,
                gizmoStrokeWeight, toolLabels);
    }

    /** Applies the numeric rule, so no value outside the range can be held. */
    AppPreferences withGizmoVisualScale(float value) {
        return new AppPreferences(palette, handedness, clampGizmoVisualScale(value),
                gizmoStrokeWeight, toolLabels);
    }

    AppPreferences withGizmoStrokeWeight(GizmoStrokeWeight value) {
        return new AppPreferences(palette, handedness, gizmoVisualScale,
                value == null ? GizmoStrokeWeight.defaultWeight() : value, toolLabels);
    }

    AppPreferences withToolLabels(boolean value) {
        return new AppPreferences(palette, handedness, gizmoVisualScale, gizmoStrokeWeight,
                value);
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof AppPreferences)) {
            return false;
        }
        final AppPreferences that = (AppPreferences) other;
        return palette == that.palette && handedness == that.handedness
                && Float.compare(gizmoVisualScale, that.gizmoVisualScale) == 0
                && gizmoStrokeWeight == that.gizmoStrokeWeight
                && toolLabels == that.toolLabels;
    }

    @Override
    public int hashCode() {
        int hash = palette.hashCode();
        hash = 31 * hash + handedness.hashCode();
        hash = 31 * hash + Float.floatToIntBits(gizmoVisualScale);
        hash = 31 * hash + gizmoStrokeWeight.hashCode();
        hash = 31 * hash + (toolLabels ? 1 : 0);
        return hash;
    }

    @Override
    public String toString() {
        return "AppPreferences{palette=" + palette + ", handedness=" + handedness
                + ", gizmoVisualScale=" + gizmoVisualScale + ", gizmoStrokeWeight="
                + gizmoStrokeWeight + ", toolLabels=" + toolLabels + "}";
    }
}
