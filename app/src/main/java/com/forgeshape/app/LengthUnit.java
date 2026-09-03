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
        final BigDecimal shifted = BigDecimal.valueOf(squareMeters)
                .movePointRight(2 * unit.decimalExponent)
                .setScale(3, java.math.RoundingMode.HALF_UP)
                .stripTrailingZeros();
        return shifted.toPlainString() + " " + unit.label + "²";
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
