package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;

/**
 * `CAD-V6-S2-CORRECTION-FILL-PICK-R2` on the JVM: the debug-only HUD touch
 * attribution names the surface that consumed a Down, and stays silent when
 * the HUD did not consume it.
 */
public final class CadHudTouchAttributionTest {

    @Test
    public void aConsumedDownNamesItsVisibleSurface() {
        assertEquals("FORGESHAPE_CAD_HUD_TOUCH:value",
                CadHudTouchAttribution.token(true, CadHudTouchAttribution.VALUE));
        assertEquals("FORGESHAPE_CAD_HUD_TOUCH:panel",
                CadHudTouchAttribution.token(true, CadHudTouchAttribution.PANEL));
        assertEquals("FORGESHAPE_CAD_HUD_TOUCH:palette",
                CadHudTouchAttribution.token(true, CadHudTouchAttribution.PALETTE));
        assertEquals("FORGESHAPE_CAD_HUD_TOUCH:editor",
                CadHudTouchAttribution.token(true, CadHudTouchAttribution.EDITOR));
        assertEquals("FORGESHAPE_CAD_HUD_TOUCH:edit_sketch",
                CadHudTouchAttribution.token(true, CadHudTouchAttribution.EDIT_SKETCH));
    }

    @Test
    public void aDownTheHudDidNotConsumeLogsNothing() {
        final int[] surfaces = {
                CadHudTouchAttribution.NONE, CadHudTouchAttribution.VALUE,
                CadHudTouchAttribution.PANEL, CadHudTouchAttribution.PALETTE,
                CadHudTouchAttribution.EDITOR, CadHudTouchAttribution.EDIT_SKETCH,
        };
        for (int surface : surfaces) {
            assertNull("surface " + surface, CadHudTouchAttribution.token(false, surface));
        }
    }

    @Test
    public void aConsumedDownOverNoKnownSurfaceLogsNothing() {
        assertNull(CadHudTouchAttribution.token(true, CadHudTouchAttribution.NONE));
        assertNull(CadHudTouchAttribution.token(true, 99));
    }
}
