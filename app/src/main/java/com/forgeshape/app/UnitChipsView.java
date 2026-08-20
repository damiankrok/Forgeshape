package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The mm / cm / m selector.
 *
 * <p>One display unit governs every length in the workspace at once: an object
 * is not measured in three different units simultaneously, and neither is its
 * position. Which is why this is a single shared control rather than one per
 * section.
 *
 * <p><b>Changes nothing but presentation.</b> Selecting a unit makes no native
 * call: no parameter, no transform, no mesh revision and no GPU upload is
 * touched. The conversion is an exact decimal point shift, so switching back
 * and forth reproduces the original digits.
 */
final class UnitChipsView extends LinearLayout {

    /** Told which unit was chosen; the caller owns what that means. */
    interface OnUnitSelected {
        void onUnitSelected(LengthUnit unit);
    }

    private static final int[] CHIP_IDS = {
            R.id.unit_chip_mm, R.id.unit_chip_cm, R.id.unit_chip_m
    };

    private final TextView[] chips = new TextView[LengthUnit.values().length];

    UnitChipsView(Context context, final OnUnitSelected listener) {
        super(context);
        setId(R.id.unit_chips);
        setOrientation(HORIZONTAL);
        setContentDescription(context.getString(R.string.unit_selector));

        final LengthUnit[] units = LengthUnit.values();
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        for (int i = 0; i < units.length; i++) {
            final LengthUnit unit = units[i];
            final TextView chip = EditorControlStyles.chip(context, CHIP_IDS[i], unit.label());
            chip.setContentDescription(context.getString(R.string.unit_selector) + " "
                    + unit.label());
            chip.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    listener.onUnitSelected(unit);
                }
            });
            chips[i] = chip;
            addView(chip, EditorControlStyles.evenShare(i == 0 ? 0 : gap));
        }
    }

    /** Draws the selected unit; the caller decides which one that is. */
    void showSelected(LengthUnit unit) {
        for (int i = 0; i < chips.length; i++) {
            EditorControlStyles.setChipActive(chips[i], LengthUnit.values()[i] == unit);
        }
    }
}
