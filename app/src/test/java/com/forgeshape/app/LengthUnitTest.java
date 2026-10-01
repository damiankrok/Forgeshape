package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Test;

import java.math.BigDecimal;

/**
 * The display-unit conversion, checked without a device.
 *
 * <p>The property that matters is exactness: switching the unit is presentation
 * and must not perturb a value the user typed, because the number that
 * eventually reaches native code has to be the number that was authored. These
 * assertions are what makes "unit switching changes presentation only" a
 * checkable claim rather than a comment.
 */
public final class LengthUnitTest {

    @Test
    public void conversionRoundTripsExactly() {
        final String[] authored = {"2", "0.5", "3.333", "1.125", "0.001", "12345.6789"};
        for (String text : authored) {
            BigDecimal value = LengthUnit.parse(text);
            value = LengthUnit.CENTIMETERS.convertFrom(LengthUnit.METERS, value);
            value = LengthUnit.MILLIMETERS.convertFrom(LengthUnit.CENTIMETERS, value);
            value = LengthUnit.METERS.convertFrom(LengthUnit.MILLIMETERS, value);
            assertEquals("m -> cm -> mm -> m must reproduce " + text,
                    0, LengthUnit.parse(text).compareTo(value));
        }
    }

    @Test
    public void meterValuesAreWrittenInTheChosenUnit() {
        assertEquals("2", LengthUnit.METERS.format(2.0));
        assertEquals("200", LengthUnit.CENTIMETERS.format(2.0));
        assertEquals("2000", LengthUnit.MILLIMETERS.format(2.0));
        assertEquals("3333", LengthUnit.MILLIMETERS.format(3.333));
    }

    @Test
    public void aLabelIsWrittenAtTheDisplayPrecisionNeverTheDoublesExpansion() {
        // CAD-FOUNDATION-C2: the OWNER's device showed a dragged depth as
        // "2.3593521118164062 m". A label is READ, so it takes the one bounded
        // display rule; the field it opens keeps every digit.
        final double dragged = 2.3593521118164062;
        assertEquals("2.359 m", LengthUnit.METERS.formatWithUnit(dragged));
        assertEquals("235.935 cm", LengthUnit.CENTIMETERS.formatWithUnit(dragged));
        assertEquals("2359.352 mm", LengthUnit.MILLIMETERS.formatWithUnit(dragged));
        assertEquals("the editor seed is still exact", "2.3593521118164062",
                LengthUnit.METERS.format(dragged));
        assertEquals("trailing zeros are not invented", "0.5 m",
                LengthUnit.METERS.formatWithUnit(0.5));
        assertEquals("2 m", LengthUnit.METERS.formatWithUnit(2.0));
        assertEquals("half-up at the last place", "1.001 m",
                LengthUnit.METERS.formatWithUnit(1.0005));
        assertEquals("no exponent form", "0.001 m", LengthUnit.METERS.formatWithUnit(0.001));
        assertEquals("0 m", LengthUnit.METERS.formatWithUnit(0.0001));
        for (double meters = 0.001; meters < 50.0; meters *= 1.37) {
            final String shown = LengthUnit.METERS.formatWithUnit(meters);
            final String digits = shown.substring(0, shown.length() - 2);
            final int point = digits.indexOf('.');
            assertTrue(shown, point < 0 || digits.length() - point - 1 <= LengthUnit.DISPLAY_DECIMALS);
        }
        // The area label is the same rule, which is where it came from.
        assertEquals("11.2 m²", LengthUnit.formatArea(LengthUnit.METERS, 11.2));
        assertEquals("0.499 m²", LengthUnit.formatArea(LengthUnit.METERS, 0.49935));
    }

    @Test
    public void displayValuesConvertBackToAuthoritativeMeters() {
        assertEquals(0, new BigDecimal("2").compareTo(
                LengthUnit.MILLIMETERS.toMeters(LengthUnit.parse("2000"))));
        assertEquals(0, new BigDecimal("0.42").compareTo(
                LengthUnit.CENTIMETERS.toMeters(LengthUnit.parse("42"))));
    }

    @Test
    public void acceptsACommaDecimalSeparatorAndASignedValue() {
        assertEquals(0, new BigDecimal("1.5").compareTo(LengthUnit.parse("1,5")));
        assertEquals(0, new BigDecimal("-90").compareTo(LengthUnit.parse(" -90 ")));
    }

    @Test
    public void refusesTextThatIsNotANumber() {
        final String[] rejected = {"", "   ", "abc", "1.2.3", "NaN", "Infinity", "-"};
        for (String text : rejected) {
            try {
                LengthUnit.parse(text);
                fail("\"" + text + "\" must not parse as a number");
            } catch (NumberFormatException expected) {
                // The field will be reported by name; nothing is submitted.
            }
        }
    }
}
