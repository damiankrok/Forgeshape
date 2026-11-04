package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;

/** The live size of a shape being drawn (`WORKBENCH-UX-R1`). */
public final class SketchLivePresentationTest {

    @Test
    public void a_rectangle_reads_its_width_by_its_height() {
        assertEquals("2 m × 1.5 m", SketchLivePresentation.label(
                NativeViewport.SKETCH_TOOL_RECTANGLE, 2.0, 1.5, LengthUnit.METERS));
    }

    @Test
    public void a_circle_reads_its_diameter_and_a_line_its_length() {
        assertEquals("Ø 500 mm", SketchLivePresentation.label(
                NativeViewport.SKETCH_TOOL_CIRCLE, 0.5, 0.0, LengthUnit.MILLIMETERS));
        assertEquals("L 1.235 m", SketchLivePresentation.label(
                NativeViewport.SKETCH_TOOL_LINE, 1.23456, 0.0, LengthUnit.METERS));
    }

    @Test
    public void a_tool_that_places_no_sized_shape_reads_nothing() {
        assertNull(SketchLivePresentation.label(NativeViewport.SKETCH_TOOL_SPLINE, 1, 1,
                LengthUnit.METERS));
        assertNull(SketchLivePresentation.label(NativeViewport.SKETCH_TOOL_SELECT, 1, 1,
                LengthUnit.METERS));
    }
}
