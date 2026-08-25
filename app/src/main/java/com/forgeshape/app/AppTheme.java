package com.forgeshape.app;

/**
 * The three appearances ForgeShape can wear.
 *
 * <p><b>Presentation, and nothing else.</b> An appearance decides what colour a
 * surface is drawn in and what the viewport is cleared to. It cannot change a
 * Construction parameter, a {@code PrimitiveKind}, a transform, an
 * {@code ObjectId}, a {@code MeshRevision}, a {@code SculptRevision}, what is
 * pickable or what is selected — and the switch is implemented so that it
 * provably does not: see {@link ForgeShapeActivity#applyTheme}.
 *
 * <p>Exactly three, chosen explicitly by the user, and all three DARK. That is
 * the authored appearance set rather than a default with variants: ForgeShape is
 * a viewport-first modelling tool, and every ground in the set is chosen so a
 * neutral clay render reads as lit. Following the system's own light/dark
 * setting is a separate decision that has not been made, so there is
 * deliberately no "System" member and no {@code -night} resource qualifier
 * anywhere in the project.
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
            NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE),

    /** A cooler steel-grey workspace. */
    NEUTRAL_CHARCOAL(R.style.Theme_ForgeShape_NeutralCharcoal,
            NativeViewport.VIEWPORT_BACKGROUND_NEUTRAL_CHARCOAL),

    /** The lightest ground in the set, and still a dark workspace. */
    LIGHT_CHARCOAL(R.style.Theme_ForgeShape_LightCharcoal,
            NativeViewport.VIEWPORT_BACKGROUND_LIGHT_CHARCOAL);

    private final int styleRes;
    private final int viewportBackground;

    AppTheme(int styleRes, int viewportBackground) {
        this.styleRes = styleRes;
        this.viewportBackground = viewportBackground;
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
}
