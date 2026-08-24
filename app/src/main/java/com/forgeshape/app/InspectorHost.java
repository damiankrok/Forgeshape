package com.forgeshape.app;

/**
 * What a Property Inspector body may ask of the workspace around it.
 *
 * <p>Deliberately four methods. An inspector body edits one section and reports
 * on it; it does not know about modes, rails, layout or the other bodies, and
 * it cannot reach them through this. Anything a body appears to need beyond
 * these is a sign it is taking on a responsibility that belongs to
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

    /** The UI-owned draft, presentation and layout state. */
    EditorUiState uiState();
}
