package com.forgeshape.app;

import java.math.BigDecimal;
import java.util.Locale;

/**
 * CR-06 closeout diagnostic. NOT part of the build. Compiled together with the
 * UNMODIFIED product sources LengthUnit.java, ExactExpression.java and
 * CadRevolvePresentation.java (baseline 70a6c14), so every parse below is the
 * product's own code, not a copy.
 *
 * The one thing that is a MODEL rather than product code is
 * {@link #digitsFilter}: android.text.method.DigitsKeyListener cannot run on a
 * host JVM (android.jar is stubs). It reproduces the documented
 * NumberKeyListener.filter rule for a string-mode DigitsKeyListener
 * (getInstance(String)): every character not in the accepted set is dropped.
 * The product installs that listener as an InputFilter on the field's Editable
 * (TextView.setFilters adds the key listener when it is an InputFilter), so a
 * typed or IME-committed character outside the set never reaches getText().
 */
final class CommaDiag {

    static String digitsFilter(String typed, String accepted) {
        final StringBuilder kept = new StringBuilder();
        for (int i = 0; i < typed.length(); i++) {
            if (accepted.indexOf(typed.charAt(i)) >= 0) {
                kept.append(typed.charAt(i));
            }
        }
        return kept.toString();
    }

    private static String lengthUnitParse(String text) {
        try {
            final BigDecimal parsed = LengthUnit.parse(text);
            // EditorUiState.displayUnit defaults to METERS (EditorUiState.java:94);
            // the canvas editors then call unit.toMeters(typed).doubleValue().
            return parsed.toPlainString() + " -> meters(double)="
                    + LengthUnit.METERS.toMeters(parsed).doubleValue()
                    + " | mm unit -> meters="
                    + LengthUnit.MILLIMETERS.toMeters(parsed).doubleValue();
        } catch (NumberFormatException e) {
            return "NumberFormatException(" + e.getMessage() + ")";
        }
    }

    private static String exactParse(String text) {
        try {
            return ExactExpression.parse(text).toPlainString();
        } catch (NumberFormatException e) {
            return "NumberFormatException(" + e.getMessage() + ")";
        }
    }

    public static void main(String[] args) {
        System.out.println("default_locale=" + Locale.getDefault());
        final String[] inputs = {"2,5", "2.5", "1 000,5"};
        // The three canvas editors: CadExtrudeCanvasView.java:644,
        // SketchDimensionLabelView.java:112, BodyDimensionLabelsView.java:127.
        final String canvasAccepted = "0123456789.-";
        // NumericPropertyRow.java:109 and CadRevolveAngleLabelView.java:87.
        final String rowAccepted = "0123456789.,-";
        // SketchDimensionLabelsView.java:126 and SketchModifyView.java:208.
        final String exprAccepted = "0123456789.,-+*/()";
        for (String typed : inputs) {
            final String canvas = digitsFilter(typed, canvasAccepted);
            final String row = digitsFilter(typed, rowAccepted);
            final String expr = digitsFilter(typed, exprAccepted);
            System.out.println("typed='" + typed + "'");
            System.out.println("  pure LengthUnit.parse(unfiltered)        = " + lengthUnitParse(typed));
            System.out.println("  CANVAS  filter '" + canvasAccepted + "' -> '" + canvas
                    + "' -> LengthUnit.parse = " + lengthUnitParse(canvas));
            System.out.println("  ROW     filter '" + rowAccepted + "' -> '" + row
                    + "' -> LengthUnit.parse = " + lengthUnitParse(row)
                    + " ; CadRevolvePresentation.parseDegrees = "
                    + CadRevolvePresentation.parseDegrees(row));
            System.out.println("  EXPR    filter '" + exprAccepted + "' -> '" + expr
                    + "' -> ExactExpression.parse = " + exactParse(expr));
        }
    }
}
