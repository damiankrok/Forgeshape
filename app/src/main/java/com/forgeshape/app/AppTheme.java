package com.forgeshape.app;

/**
 * The five appearances ForgeShape can wear (UI-OWNER-42).
 *
 * <p><b>Presentation, and nothing else.</b> An appearance decides what colour a
 * surface is drawn in and what the viewport is cleared to. It cannot change a
 * Construction parameter, a {@code PrimitiveKind}, a transform, an
 * {@code ObjectId}, a {@code MeshRevision}, a {@code SculptRevision}, what is
 * pickable or what is selected — and the switch is implemented so that it
 * provably does not: see {@link ForgeShapeActivity#applyTheme}.
 *
 * <p>Exactly five, chosen explicitly by the user. The three DARK palettes are
 * the original authored set and are untouched by `UI-PREF-R1`: every ground in
 * that set is chosen so a neutral clay render reads as lit. The two LIGHT
 * palettes are derived through the same semantic roles ({@code attrs.xml}) and
 * measured to the same contrast targets; they are a warm paper and a cool
 * steel paper rather than one light option with a tint. Following the system's
 * own light/dark setting is a separate decision that has not been made, so
 * there is deliberately no "System" member and no {@code -night} resource
 * qualifier anywhere in the project.
 *
 * <p>Since `UI-PREF-R1` the choice is PERSISTED in {@link AppPreferences}; it
 * is still never project truth.
 *
 * <p>Holds no Android type, so the mapping below is unit-testable on the JVM.
 */
enum AppTheme {

    /**
     * The product default: a warm dark studio ground.
     *
     * <p>First, so {@code values()[0]} is the default and an index that has lost
     * its meaning falls back to the appearance the product ships with.
     */
    WARM_GRAPHITE(R.style.Theme_ForgeShape_WarmGraphite,
            NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE, false,
            R.string.appearance_warm_graphite),

    /** A cooler steel-grey workspace. */
    NEUTRAL_CHARCOAL(R.style.Theme_ForgeShape_NeutralCharcoal,
            NativeViewport.VIEWPORT_BACKGROUND_NEUTRAL_CHARCOAL, false,
            R.string.appearance_neutral_charcoal),

    /** The lightest ground of the dark three, and still a dark workspace. */
    LIGHT_CHARCOAL(R.style.Theme_ForgeShape_LightCharcoal,
            NativeViewport.VIEWPORT_BACKGROUND_LIGHT_CHARCOAL, false,
            R.string.appearance_light_charcoal),

    /** A warm, cream-biased light workspace (`UI-PREF-R1` D2). */
    WARM_LIGHT(R.style.Theme_ForgeShape_WarmLight,
            NativeViewport.VIEWPORT_BACKGROUND_WARM_LIGHT, true,
            R.string.appearance_warm_light),

    /** A cool, steel-biased light workspace (`UI-PREF-R1` D3). */
    COOL_LIGHT(R.style.Theme_ForgeShape_CoolLight,
            NativeViewport.VIEWPORT_BACKGROUND_COOL_LIGHT, true,
            R.string.appearance_cool_light);

    private final int styleRes;
    private final int viewportBackground;
    private final boolean light;
    private final int labelRes;

    AppTheme(int styleRes, int viewportBackground, boolean light, int labelRes) {
        this.styleRes = styleRes;
        this.viewportBackground = viewportBackground;
        this.light = light;
        this.labelRes = labelRes;
    }

    /** The Android style the Activity applies before it inflates anything. */
    int styleRes() {
        return styleRes;
    }

    /**
     * The one presentation value the renderer is told.
     *
     * <p>Native code is handed a viewport <i>appearance</i>, never this enum and
     * never an Android theme: the geometry domain has no opinion about a
     * `SurfaceView` and must not acquire one. What crosses JNI is a closed
     * index that the display store refuses if it does not recognise it.
     */
    int viewportBackground() {
        return viewportBackground;
    }

    /**
     * Whether this is a LIGHT ground, which is what decides the system bars'
     * icon appearance: dark icons over a light palette, light icons over a dark
     * one. The theme states the same fact declaratively; the Activity restates
     * it through the insets controller so the answer does not depend on which
     * platform release resolved the attribute.
     */
    boolean isLight() {
        return light;
    }

    /** The user-facing name. */
    int labelRes() {
        return labelRes;
    }

    /** The product default, named rather than assumed at each call site. */
    static AppTheme defaultTheme() {
        return WARM_GRAPHITE;
    }

    /**
     * Maps a stored ordinal back to an appearance.
     *
     * <p>Anything unrecognised falls back to the default rather than throwing:
     * the worst outcome of a bad index is the wrong appearance, and refusing to
     * start a workspace over one would be far worse.
     */
    static AppTheme fromOrdinal(int ordinal) {
        final AppTheme[] all = values();
        return (ordinal >= 0 && ordinal < all.length) ? all[ordinal] : defaultTheme();
    }

    /**
     * Maps a stored NAME back to an appearance, or to the default.
     *
     * <p>What {@link AppPreferencesStore} writes is the name, not the ordinal,
     * so a reordering of this enum cannot silently change a user's palette and
     * an unknown name — a future member, a corrupt file — is the default rather
     * than an exception on launch.
     */
    static AppTheme fromStoredName(String name) {
        for (AppTheme candidate : values()) {
            if (candidate.name().equals(name)) {
                return candidate;
            }
        }
        return defaultTheme();
    }
}
