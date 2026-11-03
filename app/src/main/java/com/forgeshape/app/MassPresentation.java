package com.forgeshape.app;

import android.content.Context;

import java.math.BigDecimal;
import java.math.RoundingMode;

/**
 * How the Measure section reads a body's mass properties (`CAD-INSPECT-R1`).
 * Pure presentation over native's numbers: the volume only when the surface
 * is closed, the area, the world size and the centre of mass in the display
 * unit, and a mass at a density the user typed — none of it truth, none of it
 * stored.
 */
final class MassPresentation {

    private MassPresentation() {}

    /** The density a session starts with: water, 1000 kg/m³. */
    static final double DEFAULT_DENSITY = 1000.0;

    static String formatDensity(double kgPerCubicMeter) {
        return LengthUnit.present(BigDecimal.valueOf(kgPerCubicMeter)
                .setScale(3, RoundingMode.HALF_UP));
    }

    /** The mass in kilograms, at the display precision. */
    static String formatMass(double kilograms) {
        return LengthUnit.present(BigDecimal.valueOf(kilograms).setScale(3, RoundingMode.HALF_UP))
                + " kg";
    }

    static String describe(Context context, LengthUnit unit, double[] mass, double density) {
        final boolean closed = mass[NativeViewport.MASS_CLOSED] != 0.0;
        final String size = context.getString(R.string.measure_size,
                unit.formatWithUnit(mass[NativeViewport.MASS_SIZE_X]),
                unit.formatWithUnit(mass[NativeViewport.MASS_SIZE_X + 1]),
                unit.formatWithUnit(mass[NativeViewport.MASS_SIZE_X + 2]));
        final String area = context.getString(R.string.measure_area,
                LengthUnit.formatArea(unit, mass[NativeViewport.MASS_AREA]));
        if (!closed) {
            return context.getString(R.string.measure_open, area, size);
        }
        final double volume = mass[NativeViewport.MASS_VOLUME];
        return context.getString(R.string.measure_closed,
                LengthUnit.formatVolume(unit, volume), area, size,
                unit.formatWithUnit(mass[NativeViewport.MASS_CENTROID_X]),
                unit.formatWithUnit(mass[NativeViewport.MASS_CENTROID_X + 1]),
                unit.formatWithUnit(mass[NativeViewport.MASS_CENTROID_X + 2]),
                formatMass(volume * density));
    }
}
