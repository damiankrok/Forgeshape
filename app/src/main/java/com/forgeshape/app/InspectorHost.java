package com.forgeshape.app;

/**
 * What a Property Inspector body may ask of the workspace around it.
 *
 * <p>Deliberately short. An inspector body edits one section and reports on it;
 * it does not know about modes, rails, layout or the other bodies, and it
 * cannot reach them through this. Anything a body appears to need beyond these
 * is a sign it is taking on a responsibility that belongs to
 * {@link EditorWorkspaceView}.
 */
interface InspectorHost {

    /**
     * Writes the one status message in the workspace.
     *
     * @param colorAttr a semantic colour ROLE — {@code R.attr.fsTextSecondary},
     *                  {@code fsTextSuccess} or {@code fsTextError} — saying
     *                  what kind of message this is: a neutral note, a success
     *                  or a rejection. A role rather than a colour, so a caller
     *                  cannot state a verdict in a shade that only reads on one
     *                  of the two themes
     */
    void showStatus(CharSequence message, int colorAttr);

    /**
     * Announces that native state changed, so every surface must re-read it.
     *
     * <p>Called after an Apply that landed, a Freeze or a mode change — never
     * after a presentation-only action, because nothing below JNI moved.
     */
    void onNativeStateChanged();

    /**
     * Asks for another display unit.
     *
     * <p>Routed through the host rather than handled in place because one unit
     * governs every length in the workspace at once, including the lengths in
     * whichever editor is currently off screen. Presentation only: the host
     * makes no native call for it.
     */
    void onDisplayUnitRequested(LengthUnit unit);

    /**
     * Hands focus and the soft keyboard back to the viewport once an edit has
     * landed, so the next touch navigates the model instead of typing.
     */
    void finishEditing();

    /**
     * Asks for the Add Primitive palette, grown out of a given control.
     *
     * <p>Routed through the host, and taking the control it grew from, because
     * creation is a <b>scene-level</b> act with two invoking controls — the
     * Objects capsule's {@code +} on a phone and the docked Objects column's on
     * a tablet — and exactly one surface. The host owns that surface and where
     * it is anchored; the caller owns only which control was pressed.
     *
     * <p>Nothing is created by asking. A body exists only once a shape has been
     * chosen from the palette.
     */
    void onAddPrimitiveRequested(android.view.View invoker);

    /**
     * Asks to reopen the active CAD Body's sketch for editing
     * (`SKETCH-UX-R1` F1).
     *
     * <p>Routed through the host because it is a <b>mode transition</b>, not a
     * value edit: the workspace leaves the precision surface, hands the viewport
     * to the sketch session and re-frames the camera. The panel that asked owns
     * only the fact that the user asked.
     */
    void onEditCadSketchRequested();

    /**
     * Asks to reopen ONE feature of the active CAD Body's chain
     * (`CAD-VERTICAL-SLICE-R1`): feature 1 is the body's first sketch and
     * extrusion, a later id an Add or a Cut. A mode transition on exactly
     * {@link #onEditCadSketchRequested()}'s terms, staged until Finish.
     */
    void onEditCadFeatureRequested(long featureId);

    /**
     * A precision-surface act changed the open sketch's staged extrusion — a
     * region row or a side chip (`CAD-VERTICAL-SLICE-R1`). The host brings the
     * toolbar's Extrude and the canvas HUD to the same candidate the panel now
     * shows, and reports its verdict.
     */
    void onSketchCandidateChanged();

    /** The UI-owned draft, presentation and layout state. */
    EditorUiState uiState();
}
