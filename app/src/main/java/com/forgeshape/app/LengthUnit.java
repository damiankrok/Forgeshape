package com.forgeshape.app;

import java.math.BigDecimal;

/**
 * A display unit for a Construction length.
 *
 * <p><b>Presentation only.</b> The authoritative Construction unit is the meter,
 * carried as a {@code double} by native code; nothing in this enum is geometry
 * truth and nothing here ever reaches native state on its own. Choosing a unit
 * changes how a number is written down, never what the box is.
 *
 * <p>Every conversion is an exact decimal point shift on a {@link BigDecimal}
 * ({@code 10^exponent}), never a floating-point multiply, so switching
 * m -> cm -> mm -> m reproduces the original digits exactly and cannot
 * accumulate drift. The factors are fixed by definition: 1000 mm, 100 cm and
 * 1 m per meter.
 */
enum LengthUnit {
    MILLIMETERS("mm", 3),
    CENTIMETERS("cm", 2),
    METERS("m", 0);

    private final String label;

    /** {@code displayed = meters * 10^decimalExponent}. */
    private final int decimalExponent;

    LengthUnit(String label, int decimalExponent) {
        this.label = label;
        this.decimalExponent = decimalExponent;
    }

    String label() {
        return label;
    }

    /**
     * Renders an authoritative meter value in this unit.
     *
     * <p>{@link BigDecimal#valueOf(double)} is used deliberately: it takes the
     * shortest decimal that round-trips the {@code double}, so 3.333 m prints as
     * {@code 3333} mm rather than the exact binary expansion of the double. The
     * point shift is exact, and trailing zeros are stripped so a unit change does
     * not invent precision. No grouping separator is ever emitted.
     */
    String format(double meters) {
        return present(BigDecimal.valueOf(meters).movePointRight(decimalExponent));
    }

    /**
     * The same value with its unit written after it, for a label that stands on
     * its OWN rather than under a caption naming the unit.
     *
     * <p>The dimension annotation is the one place a length is read away from
     * the precision surface's captioned rows (`SKETCH-UX-R1` E1), and a bare
     * number floating over a drawing is ambiguous in exactly the way a technical
     * drawing must not be.
     *
     * <p><b>An annotation is READ, not edited, so it is written at the display
     * precision</b> (`CAD-FOUNDATION-C2`): {@link #DISPLAY_DECIMALS} places in
     * this unit, half-up, trailing zeros stripped — the one bounded-precision rule
     * the product already had for a profile's area label, now shared rather than
     * restated. A dragged depth is a binary64 the user never typed, and its
     * shortest round-trip decimal ({@code 2.3593521118164062}) is noise on a
     * drawing. Nothing about the value changes: the editor a label opens is still
     * seeded by {@link #format}, which keeps every digit, and native keeps the
     * double.
     */
    String formatWithUnit(double meters) {
        return present(atDisplayPrecision(BigDecimal.valueOf(meters)
                .movePointRight(decimalExponent))) + " " + label;
    }

    /**
     * Decimal places a READ-ONLY label shows in its own unit: 3, so a metre
     * reads to the millimetre and a millimetre to the micrometre. OWNER-TUNABLE
     * presentation; never applied to a field, a submission or native state.
     */
    static final int DISPLAY_DECIMALS = 3;

    /** The one bounded-precision rule for a label: {@link #DISPLAY_DECIMALS}, half-up. */
    private static BigDecimal atDisplayPrecision(BigDecimal shifted) {
        return shifted.setScale(DISPLAY_DECIMALS, java.math.RoundingMode.HALF_UP);
    }

    /** Converts a value already written in {@code from} into this unit, exactly. */
    BigDecimal convertFrom(LengthUnit from, BigDecimal value) {
        return value.movePointRight(decimalExponent - from.decimalExponent);
    }

    /**
     * Renders an area in this unit squared, for a profile's label.
     *
     * <p>Presentation only, and deliberately not an authored value: the domain
     * never stores an area, and nothing may be typed as one. The point shift is
     * twice the length shift, and the result is bounded to three decimals so a
     * chip does not carry a double's whole expansion.
     */
    static String formatArea(LengthUnit unit, double squareMeters) {
        final BigDecimal shifted = atDisplayPrecision(BigDecimal.valueOf(squareMeters)
                .movePointRight(2 * unit.decimalExponent));
        return present(shifted) + " " + unit.label + "²";
    }

    /** Converts a value written in this unit into authoritative meters, exactly. */
    BigDecimal toMeters(BigDecimal display) {
        return display.movePointLeft(decimalExponent);
    }

    /**
     * Canonical field text for a decimal value: no exponent form, no grouping
     * separator, no trailing zeros.
     */
    static String present(BigDecimal value) {
        return value.stripTrailingZeros().toPlainString();
    }

    /**
     * Parses one field's text as a decimal number.
     *
     * <p>Accepts {@code .} or {@code ,} as the decimal separator. Blank and
     * malformed text (including {@code NaN} and {@code Infinity}, which
     * {@link BigDecimal} does not accept) raise {@link NumberFormatException};
     * this method does not judge whether the number is a usable dimension, which
     * is the caller's and ultimately native code's decision.
     */
    static BigDecimal parse(String raw) {
        final String text = raw == null ? "" : raw.trim().replace(',', '.');
        if (text.isEmpty()) {
            throw new NumberFormatException("blank");
        }
        return new BigDecimal(text);
    }
}
