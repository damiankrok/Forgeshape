package com.forgeshape.app;

/**
 * What the live size of a shape being drawn reads as (`WORKBENCH-UX-R1`), a
 * pure function so the JVM pins it: {@code L 1.2 m} for a Line, {@code 2 m ×
 * 1.5 m} for a Rectangle, {@code Ø 0.5 m} for a Circle, each at the display
 * precision of the chosen unit. Null for a tool that places no sized shape.
 */
final class SketchLivePresentation {

    private SketchLivePresentation() {
    }

    static String label(int tool, double first, double second, LengthUnit unit) {
        switch (tool) {
            case NativeViewport.SKETCH_TOOL_LINE:
                return "L " + unit.formatWithUnit(first);
            case NativeViewport.SKETCH_TOOL_RECTANGLE:
                return unit.formatWithUnit(first) + " × " + unit.formatWithUnit(second);
            case NativeViewport.SKETCH_TOOL_CIRCLE:
                return "Ø " + unit.formatWithUnit(first);
            default:
                return null;
        }
    }
}
