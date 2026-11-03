package com.forgeshape.app;

import java.math.BigDecimal;
import java.math.MathContext;

/**
 * A small, SAFE arithmetic reader for the drafting fields
 * (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`): {@code 12+3}, {@code 50/2},
 * {@code 2*7.5}, {@code (10-4)*2}, a leading minus.
 *
 * <p>Exact decimal arithmetic on {@link BigDecimal}, no scripting engine, no
 * variables, no functions: a recursive-descent reader over digits, a decimal
 * point (a comma is read as one, as {@link LengthUnit#parse} does), the four
 * operators and parentheses. Division keeps 34 significant digits
 * ({@link MathContext#DECIMAL128}); everything else is exact. Bounded: the text
 * is at most {@link #MAX_LENGTH} characters, so the recursion is too.
 *
 * <p>Anything else — an empty field, a stray symbol, a division by zero,
 * unbalanced parentheses — raises {@link NumberFormatException}, which every
 * caller already reports by name. A plain number reads exactly as
 * {@link LengthUnit#parse} reads it.
 */
final class ExactExpression {

    /** The longest expression a drafting field accepts. */
    static final int MAX_LENGTH = 64;

    private final String text;
    private int at;

    private ExactExpression(String text) {
        this.text = text;
    }

    static BigDecimal parse(String raw) {
        final String text = raw == null ? "" : raw.trim().replace(',', '.').replace(" ", "");
        if (text.isEmpty()) {
            throw new NumberFormatException("blank");
        }
        if (text.length() > MAX_LENGTH) {
            throw new NumberFormatException("too long");
        }
        final ExactExpression reader = new ExactExpression(text);
        final BigDecimal value = reader.sum();
        if (reader.at != text.length()) {
            throw new NumberFormatException("unexpected '" + text.charAt(reader.at) + "'");
        }
        return value;
    }

    private BigDecimal sum() {
        BigDecimal value = product();
        while (at < text.length()) {
            final char c = text.charAt(at);
            if (c == '+') {
                at++;
                value = value.add(product());
            } else if (c == '-') {
                at++;
                value = value.subtract(product());
            } else {
                break;
            }
        }
        return value;
    }

    private BigDecimal product() {
        BigDecimal value = unary();
        while (at < text.length()) {
            final char c = text.charAt(at);
            if (c == '*') {
                at++;
                value = value.multiply(unary());
            } else if (c == '/') {
                at++;
                final BigDecimal divisor = unary();
                if (divisor.signum() == 0) {
                    throw new NumberFormatException("division by zero");
                }
                value = value.divide(divisor, MathContext.DECIMAL128);
            } else {
                break;
            }
        }
        return value;
    }

    private BigDecimal unary() {
        if (at < text.length() && text.charAt(at) == '-') {
            at++;
            return unary().negate();
        }
        if (at < text.length() && text.charAt(at) == '+') {
            at++;
            return unary();
        }
        return atom();
    }

    private BigDecimal atom() {
        if (at < text.length() && text.charAt(at) == '(') {
            at++;
            final BigDecimal value = sum();
            if (at >= text.length() || text.charAt(at) != ')') {
                throw new NumberFormatException("unbalanced parentheses");
            }
            at++;
            return value;
        }
        final int start = at;
        while (at < text.length()
                && (Character.isDigit(text.charAt(at)) || text.charAt(at) == '.')) {
            at++;
        }
        if (start == at) {
            throw new NumberFormatException(at < text.length()
                    ? "unexpected '" + text.charAt(at) + "'" : "missing number");
        }
        return new BigDecimal(text.substring(start, at));
    }
}
