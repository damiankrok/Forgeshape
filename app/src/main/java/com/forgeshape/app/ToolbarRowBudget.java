package com.forgeshape.app;

/**
 * The Global Toolbar's row arithmetic, as a pure function the JVM can pin
 * (`MODELING-R1-OWNER-CORRECTION`).
 *
 * <p>The row is the ForgeShape mark's capsule, the editing group (context label
 * and the ONE mode transition), the flexible gap, an inline status and the
 * utility group. Everything but the transition has a fixed or floored width
 * and is reserved first; the transition gets what is left, never less than
 * its own floor. So on the narrowest phone the mark and the trailing Display
 * and Hide UI controls keep their 48 dp targets and the transition is what
 * gives -- by a shorter label where one is approved, by an ellipsis only past
 * any window the product supports.
 */
final class ToolbarRowBudget {

    private ToolbarRowBudget() {}

    /**
     * The width the one mode transition may take.
     *
     * @param rowWidth        the row's inner width
     * @param markWidth       the mark's capsule with its margins (0 when withdrawn)
     * @param utilityWidth    the utility group with the status slot's margins
     * @param statusFloor     an inline status's floor (0 when none is inline)
     * @param labelWidth      the context label with its margins (0 when withdrawn)
     * @param editingPadding  the editing group's own horizontal padding
     * @param transitionFloor the transition's own minimum width
     */
    static int transitionBudget(int rowWidth, int markWidth, int utilityWidth, int statusFloor,
                                int labelWidth, int editingPadding, int transitionFloor) {
        final int budget = rowWidth - markWidth - utilityWidth - statusFloor - labelWidth
                - editingPadding;
        return Math.max(budget, transitionFloor);
    }
}
