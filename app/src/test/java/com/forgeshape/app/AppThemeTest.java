package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/**
 * The appearance choice, checked without a device.
 *
 * <p>What matters here is not which colours a palette has — those are resources,
 * and a device draws them — but the rules the choice itself has to obey:
 * Warm Graphite is the default a fresh process reports, the five appearances
 * map onto five DIFFERENT viewport backgrounds, exactly two of them are light,
 * and choosing one changes nothing else the UI is allowed to remember. Since
 * `UI-PREF-R1` the choice is one field of {@link AppPreferences} rather than a
 * static in {@link EditorUiState}, so the "process-scoped" rules that used to
 * live here are now the store's, proven on a device.
 */
public final class AppThemeTest {

    @Before
    public void startFromAFreshProcess() {
        EditorUiState.carryAcrossRecreation(null);
    }

    @After
    public void leaveTheProcessAtTheDefault() {
        EditorUiState.carryAcrossRecreation(null);
    }

    @Test
    public void warmGraphiteIsTheDocumentedDefault() {
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.defaultTheme());
        assertSame("a process whose preferences were never written wears the default",
                AppTheme.WARM_GRAPHITE, AppPreferences.defaults().palette());
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.values()[0]);
    }

    @Test
    public void thereAreExactlyFiveApprovedAppearances() {
        assertEquals("Warm Graphite, Neutral Charcoal, Light Charcoal, Warm Light and"
                        + " Cool Light — the approved set (UI-OWNER-42), with no automatic"
                        + " System member: following the system is a separate decision",
                5, AppTheme.values().length);
        // The three dark palettes keep their declaration order and indices: the
        // ordinal is what crosses JNI, and a reader of an older log must agree
        // with a reader of this one about what a 2 means.
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.values()[0]);
        assertSame(AppTheme.NEUTRAL_CHARCOAL, AppTheme.values()[1]);
        assertSame(AppTheme.LIGHT_CHARCOAL, AppTheme.values()[2]);
        assertSame(AppTheme.WARM_LIGHT, AppTheme.values()[3]);
        assertSame(AppTheme.COOL_LIGHT, AppTheme.values()[4]);
    }

    @Test
    public void exactlyTheTwoLightPalettesAreLight() {
        assertFalse(AppTheme.WARM_GRAPHITE.isLight());
        assertFalse(AppTheme.NEUTRAL_CHARCOAL.isLight());
        assertFalse(AppTheme.LIGHT_CHARCOAL.isLight());
        assertTrue(AppTheme.WARM_LIGHT.isLight());
        assertTrue(AppTheme.COOL_LIGHT.isLight());
    }

    @Test
    public void eachAppearanceCarriesItsOwnViewportBackground() {
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE,
                AppTheme.WARM_GRAPHITE.viewportBackground());
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_NEUTRAL_CHARCOAL,
                AppTheme.NEUTRAL_CHARCOAL.viewportBackground());
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_LIGHT_CHARCOAL,
                AppTheme.LIGHT_CHARCOAL.viewportBackground());
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_WARM_LIGHT,
                AppTheme.WARM_LIGHT.viewportBackground());
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_COOL_LIGHT,
                AppTheme.COOL_LIGHT.viewportBackground());

        // No two appearances may share a ground, a style or a label. An
        // appearance that did not change the viewport would leave one palette's
        // render behind another palette's chrome.
        final AppTheme[] all = AppTheme.values();
        for (int i = 0; i < all.length; i++) {
            for (int j = i + 1; j < all.length; j++) {
                assertNotEquals(all[i] + " and " + all[j] + " share a viewport ground",
                        all[i].viewportBackground(), all[j].viewportBackground());
                assertNotEquals(all[i] + " and " + all[j] + " share a style",
                        all[i].styleRes(), all[j].styleRes());
                assertNotEquals(all[i] + " and " + all[j] + " share a name",
                        all[i].labelRes(), all[j].labelRes());
            }
        }
    }

    @Test
    public void theViewportIndicesAreTheDeclarationOrder() {
        // What crosses JNI is the ordinal, and native code maps it back by
        // numeric order. If these ever disagree the appearance switch silently
        // selects the wrong ground and nothing fails loudly.
        final AppTheme[] all = AppTheme.values();
        for (int i = 0; i < all.length; i++) {
            assertEquals(all[i] + " must cross JNI as its own index",
                    i, all[i].viewportBackground());
        }
    }

    @Test
    public void anUnrecognisedOrdinalOrNameFallsBackToTheDefault() {
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.fromOrdinal(0));
        assertSame(AppTheme.NEUTRAL_CHARCOAL, AppTheme.fromOrdinal(1));
        assertSame(AppTheme.LIGHT_CHARCOAL, AppTheme.fromOrdinal(2));
        assertSame(AppTheme.WARM_LIGHT, AppTheme.fromOrdinal(3));
        assertSame(AppTheme.COOL_LIGHT, AppTheme.fromOrdinal(4));
        assertSame("the worst outcome of a bad index is the wrong colour, which"
                        + " is not a reason to refuse to start a workspace",
                AppTheme.WARM_GRAPHITE, AppTheme.fromOrdinal(5));
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.fromOrdinal(-1));
        // The store writes NAMES, so a reordering can never change a palette
        // and an unknown name is the default rather than an exception.
        assertSame(AppTheme.COOL_LIGHT, AppTheme.fromStoredName("COOL_LIGHT"));
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.fromStoredName("SOLAR_FLARE"));
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.fromStoredName(null));
    }

    @Test
    public void choosingAnAppearanceTouchesNothingElseTheUiRemembers() {
        final EditorUiState state = new EditorUiState();
        state.setDisplayUnit(LengthUnit.MILLIMETERS);
        state.setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
        state.setPrecisionOpen(false, true);
        state.setSettingsOpen(true);

        final AppPreferences chosen =
                AppPreferences.defaults().withPalette(AppTheme.NEUTRAL_CHARCOAL);

        assertSame(AppTheme.NEUTRAL_CHARCOAL, chosen.palette());
        assertEquals("an appearance is presentation and may not move a display unit",
                LengthUnit.MILLIMETERS, state.displayUnit());
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM, state.constructionTool());
        assertTrue(state.precisionOpen(false));
        assertTrue("nor close the page it was chosen from", state.settingsOpen());
    }

    // -----------------------------------------------------------------------
    // Carrying the session across the recreation that applies a theme
    // -----------------------------------------------------------------------

    @Test
    public void aFreshWorkspaceGetsFreshStateWhenNothingWasCarried() {
        final EditorUiState fresh = EditorUiState.forNewWorkspace();
        assertEquals(LengthUnit.METERS, fresh.displayUnit());
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_SHAPE, fresh.constructionTool());
        assertFalse(fresh.settingsOpen());
    }

    @Test
    public void theSessionSurvivesTheRecreationThatAppliesATheme() {
        final EditorUiState before = new EditorUiState();
        before.setDisplayUnit(LengthUnit.MILLIMETERS);
        before.setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
        before.setPrecisionOpen(false, true);
        before.setNewProjectChooserOpen(true);
        before.setSettingsOpen(true);

        EditorUiState.carryAcrossRecreation(before);
        final EditorUiState after = EditorUiState.forNewWorkspace();

        assertSame("changing colour must not also reset the session", before, after);
        assertEquals(LengthUnit.MILLIMETERS, after.displayUnit());
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM, after.constructionTool());
        assertTrue(after.precisionOpen(false));
        assertTrue("nor close the question the user was reading", after.newProjectChooserOpen());
        assertTrue("and the Settings page the palette was chosen from comes back",
                after.settingsOpen());
    }

    @Test
    public void aCarriedSessionIsHandedOverExactlyOnce() {
        final EditorUiState carried = new EditorUiState();
        carried.setDisplayUnit(LengthUnit.CENTIMETERS);
        EditorUiState.carryAcrossRecreation(carried);

        assertSame(carried, EditorUiState.forNewWorkspace());
        // A workspace built for any other reason must not inherit a session it
        // was never handed.
        assertEquals(LengthUnit.METERS, EditorUiState.forNewWorkspace().displayUnit());
    }
}
