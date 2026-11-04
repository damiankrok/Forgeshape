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

    /**
     * The same reading as key/value ROWS (`WORKBENCH-UX-R1`): what the Measure
     * section lays out as an aligned two-column grid rather than a bold block.
     */
    static String[][] rows(Context context, LengthUnit unit, double[] mass, double density) {
        final boolean closed = mass[NativeViewport.MASS_CLOSED] != 0.0;
        final String size = unit.formatWithUnit(mass[NativeViewport.MASS_SIZE_X]) + " × "
                + unit.formatWithUnit(mass[NativeViewport.MASS_SIZE_X + 1]) + " × "
                + unit.formatWithUnit(mass[NativeViewport.MASS_SIZE_X + 2]);
        final String area = LengthUnit.formatArea(unit, mass[NativeViewport.MASS_AREA]);
        if (!closed) {
            return new String[][] {
                    {context.getString(R.string.measure_key_volume),
                            context.getString(R.string.measure_value_open)},
                    {context.getString(R.string.measure_key_area), area},
                    {context.getString(R.string.measure_key_size), size}};
        }
        final double volume = mass[NativeViewport.MASS_VOLUME];
        return new String[][] {
                {context.getString(R.string.measure_key_volume), LengthUnit.formatVolume(unit, volume)},
                {context.getString(R.string.measure_key_area), area},
                {context.getString(R.string.measure_key_size), size},
                {context.getString(R.string.measure_key_centre),
                        unit.formatWithUnit(mass[NativeViewport.MASS_CENTROID_X]) + ", "
                                + unit.formatWithUnit(mass[NativeViewport.MASS_CENTROID_X + 1]) + ", "
                                + unit.formatWithUnit(mass[NativeViewport.MASS_CENTROID_X + 2])},
                {context.getString(R.string.measure_key_mass), formatMass(volume * density)}};
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

    /**
     * The Measure distance reading (`CAD-MEASURE-R1`): how many of the two
     * points are picked, then the distance and its world-axis components in
     * the display unit.
     */
    static String describeDistance(Context context, LengthUnit unit, double[] measure) {
        final int count = (int) measure[NativeViewport.MEASURE_COUNT];
        if (count < 2) {
            return context.getString(R.string.measure_distance_points, count);
        }
        final String distance = context.getString(R.string.measure_distance_result,
                unit.formatWithUnit(measure[NativeViewport.MEASURE_DISTANCE]),
                unit.formatWithUnit(measure[NativeViewport.MEASURE_DX]),
                unit.formatWithUnit(measure[NativeViewport.MEASURE_DX + 1]),
                unit.formatWithUnit(measure[NativeViewport.MEASURE_DX + 2]));
        if (measure[NativeViewport.MEASURE_HAS_ANGLE] == 0.0) {
            return distance;
        }
        return distance + "\n" + context.getString(R.string.measure_angle_result,
                CadRevolvePresentation.formatDegrees(measure[NativeViewport.MEASURE_ANGLE]));
    }
}
