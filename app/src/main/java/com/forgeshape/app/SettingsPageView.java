package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

/**
 * The Settings hub (`UI-PREF-R1` A, UI-OWNER-37): every persistent application
 * preference, on one full-window page.
 *
 * <p><b>A start page, reached as navigation.</b> It takes the page grammar
 * Home and New Project already use — opaque, the whole window, grouped rows,
 * Back at the foot — so on a phone it is a page and never a desktop sidebar,
 * and on a wider window it is the same information architecture with more
 * room. The viewport behind it is not reachable while it stands.
 *
 * <p>Three groups, because the stage approved three kinds of preference and
 * no more: <b>Appearance</b> (the five palettes), <b>Workspace</b> (which edge
 * the rail stands on) and <b>Gizmo</b> (visual size and thickness). Nothing
 * here is transient viewport state — Grid, Shading, Projection stay in the
 * Display popover — and nothing here is a fake row: every option is a real,
 * saved preference. Handle Style is deliberately ABSENT rather than inert; see
 * {@code artifacts/ui-pref-r1/INDEX.md} for why.
 *
 * <p><b>Selection is never colour alone.</b> The chosen row carries the
 * selected fill, a check mark in its label, {@code isSelected()} for a screen
 * reader, and a content description that says "selected". Every row is a
 * 48 dp target by its own box.
 *
 * <p><b>Owns no state and makes no native call.</b> Each row reports a
 * request; {@link EditorWorkspaceView} owns what that means and repaints this
 * page from the preferences actually in force.
 */
final class SettingsPageView extends StartPageView {

    /** Told which preference was asked for; the caller owns what it means. */
    interface OnSettingsAction {
        void onPaletteChosen(AppTheme palette);

        void onHandednessChosen(Handedness handedness);

        void onGizmoVisualScaleChosen(float scale);

        void onGizmoStrokeWeightChosen(GizmoStrokeWeight weight);

        void onSettingsBackRequested();
    }

    /** The palette rows, in {@link AppTheme} declaration order. */
    private static final int[] PALETTE_IDS = {
            R.id.appearance_warm_graphite, R.id.appearance_neutral_charcoal,
            R.id.appearance_light_charcoal, R.id.appearance_warm_light,
            R.id.appearance_cool_light};

    private static final int[] HANDEDNESS_IDS = {R.id.handedness_right, R.id.handedness_left};
    private static final int[] HANDEDNESS_LABELS = {
            R.string.handedness_right, R.string.handedness_left};

    /** The size presets, in step with {@link AppPreferences#GIZMO_VISUAL_SCALE_PRESETS}. */
    private static final int[] SIZE_IDS = {
            R.id.gizmo_size_small, R.id.gizmo_size_default, R.id.gizmo_size_large,
            R.id.gizmo_size_largest};
    private static final int[] SIZE_LABELS = {
            R.string.gizmo_size_small, R.string.gizmo_size_default, R.string.gizmo_size_large,
            R.string.gizmo_size_largest};

    private static final int[] WEIGHT_IDS = {
            R.id.gizmo_weight_thin, R.id.gizmo_weight_regular, R.id.gizmo_weight_bold};

    private final TextView[] paletteRows = new TextView[PALETTE_IDS.length];
    private final TextView[] handednessRows = new TextView[HANDEDNESS_IDS.length];
    private final TextView[] sizeRows = new TextView[SIZE_IDS.length];
    private final TextView[] weightRows = new TextView[WEIGHT_IDS.length];

    SettingsPageView(Context context, final OnSettingsAction listener) {
        super(context, R.id.settings_page, R.id.settings_panel,
                context.getString(R.string.settings), context.getString(R.string.settings_prompt));

        // --- Appearance -----------------------------------------------------
        addSectionLabel(context.getString(R.string.appearance), true);
        final AppTheme[] palettes = AppTheme.values();
        for (int i = 0; i < palettes.length; i++) {
            final AppTheme palette = palettes[i];
            paletteRows[i] = addOptionRow(PALETTE_IDS[i],
                    context.getString(palette.labelRes()), new OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            listener.onPaletteChosen(palette);
                        }
                    });
        }

        // --- Workspace ------------------------------------------------------
        addSectionLabel(context.getString(R.string.settings_workspace), false);
        addCaption(context.getString(R.string.handedness_description));
        final Handedness[] handednesses = Handedness.values();
        for (int i = 0; i < handednesses.length; i++) {
            final Handedness handedness = handednesses[i];
            handednessRows[i] = addOptionRow(HANDEDNESS_IDS[i],
                    context.getString(HANDEDNESS_LABELS[i]), new OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            listener.onHandednessChosen(handedness);
                        }
                    });
        }

        // --- Gizmo ----------------------------------------------------------
        addSectionLabel(context.getString(R.string.settings_gizmo), false);
        addCaption(context.getString(R.string.gizmo_description));
        addFieldLabel(context.getString(R.string.gizmo_size));
        for (int i = 0; i < SIZE_IDS.length; i++) {
            final float scale = AppPreferences.GIZMO_VISUAL_SCALE_PRESETS[i];
            sizeRows[i] = addOptionRow(SIZE_IDS[i], context.getString(SIZE_LABELS[i]),
                    new OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            listener.onGizmoVisualScaleChosen(scale);
                        }
                    });
        }
        addFieldLabel(context.getString(R.string.gizmo_weight));
        final GizmoStrokeWeight[] weights = GizmoStrokeWeight.values();
        for (int i = 0; i < weights.length; i++) {
            final GizmoStrokeWeight weight = weights[i];
            weightRows[i] = addOptionRow(WEIGHT_IDS[i], context.getString(weight.labelRes()),
                    new OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            listener.onGizmoStrokeWeightChosen(weight);
                        }
                    });
        }

        addStatusLine();
        addSecondaryAction(R.id.settings_back, context.getString(R.string.back),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        listener.onSettingsBackRequested();
                    }
                });
    }

    /**
     * Repaints every row from the preferences actually in force.
     *
     * <p>Called after any request and whenever the page is shown, so it shows
     * what IS rather than what was last tapped — the difference that matters
     * when a request was refused below JNI.
     */
    void showPreferences(AppPreferences preferences) {
        final AppTheme[] palettes = AppTheme.values();
        for (int i = 0; i < palettes.length; i++) {
            markOption(paletteRows[i], palettes[i] == preferences.palette());
        }
        final Handedness[] handednesses = Handedness.values();
        for (int i = 0; i < handednesses.length; i++) {
            markOption(handednessRows[i], handednesses[i] == preferences.handedness());
        }
        for (int i = 0; i < SIZE_IDS.length; i++) {
            markOption(sizeRows[i], Float.compare(AppPreferences.GIZMO_VISUAL_SCALE_PRESETS[i],
                    preferences.gizmoVisualScale()) == 0);
        }
        final GizmoStrokeWeight[] weights = GizmoStrokeWeight.values();
        for (int i = 0; i < weights.length; i++) {
            markOption(weightRows[i], weights[i] == preferences.gizmoStrokeWeight());
        }
    }

    /**
     * Marks one option chosen or not, in every channel at once.
     *
     * <p>The fill is the visual state every selected control in the product
     * wears; the check mark is the answer for a reader who cannot separate the
     * fills; {@code setSelected} and the description are the answers for a
     * screen reader. The bare label is kept as a tag so the mark can be added
     * and removed without the row remembering its own text.
     */
    private void markOption(TextView row, boolean chosen) {
        final CharSequence label = (CharSequence) row.getTag();
        row.setText(chosen ? "✓  " + label : label);
        row.setContentDescription(chosen
                ? getContext().getString(R.string.settings_selected, label) : label);
        row.setSelected(chosen);
        EditorControlStyles.setListRowActive(row, chosen);
    }

    /** Whether the option row with this id is drawn chosen; verification. */
    boolean optionChosen(int id) {
        final View row = findViewById(id);
        return row != null && row.isSelected() && row.isActivated();
    }
}
