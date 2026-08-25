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
 * and a device draws them — but the three rules the choice itself has to obey:
 * Warm Graphite is the default a fresh process reports, the three appearances
 * map onto three DIFFERENT viewport backgrounds, and choosing one changes
 * nothing else the UI is allowed to remember.
 */
public final class AppThemeTest {

    @Before
    public void startFromAFreshProcess() {
        EditorUiState.resetAppTheme();
    }

    @After
    public void leaveTheProcessAtTheDefault() {
        EditorUiState.resetAppTheme();
        EditorUiState.carryAcrossRecreation(null);
    }

    @Test
    public void warmGraphiteIsTheDocumentedDefault() {
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.defaultTheme());
        assertSame("a process that has not been asked wears the default",
                AppTheme.WARM_GRAPHITE, EditorUiState.currentAppTheme());
        assertSame(AppTheme.WARM_GRAPHITE, new EditorUiState().appTheme());
    }

    @Test
    public void thereAreExactlyThreeApprovedAppearances() {
        assertEquals("Warm Graphite, Neutral Charcoal and Light Charcoal — the"
                        + " approved set, with no automatic System member:"
                        + " following the system is a separate decision",
                3, AppTheme.values().length);
    }

    @Test
    public void eachAppearanceCarriesItsOwnViewportBackground() {
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE,
                AppTheme.WARM_GRAPHITE.viewportBackground());
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_NEUTRAL_CHARCOAL,
                AppTheme.NEUTRAL_CHARCOAL.viewportBackground());
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_LIGHT_CHARCOAL,
                AppTheme.LIGHT_CHARCOAL.viewportBackground());

        // No two appearances may share a ground or a style. An appearance that
        // did not change the viewport would leave one palette's render behind
        // another palette's chrome.
        final AppTheme[] all = AppTheme.values();
        for (int i = 0; i < all.length; i++) {
            for (int j = i + 1; j < all.length; j++) {
                assertNotEquals(all[i] + " and " + all[j] + " share a viewport ground",
                        all[i].viewportBackground(), all[j].viewportBackground());
                assertNotEquals(all[i] + " and " + all[j] + " share a style",
                        all[i].styleRes(), all[j].styleRes());
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
    public void anUnrecognisedOrdinalFallsBackToTheDefault() {
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.fromOrdinal(0));
        assertSame(AppTheme.NEUTRAL_CHARCOAL, AppTheme.fromOrdinal(1));
        assertSame(AppTheme.LIGHT_CHARCOAL, AppTheme.fromOrdinal(2));
        assertSame("the worst outcome of a bad index is the wrong colour, which"
                        + " is not a reason to refuse to start a workspace",
                AppTheme.WARM_GRAPHITE, AppTheme.fromOrdinal(3));
        assertSame(AppTheme.WARM_GRAPHITE, AppTheme.fromOrdinal(-1));
    }

    @Test
    public void choosingTheAppearanceAlreadyInForceIsNotAChange() {
        assertFalse("recreating the Activity for this would be a flash and"
                        + " nothing else",
                EditorUiState.setCurrentAppTheme(AppTheme.WARM_GRAPHITE));
        assertTrue(EditorUiState.setCurrentAppTheme(AppTheme.NEUTRAL_CHARCOAL));
        assertFalse(EditorUiState.setCurrentAppTheme(AppTheme.NEUTRAL_CHARCOAL));
        assertSame(AppTheme.NEUTRAL_CHARCOAL, EditorUiState.currentAppTheme());
        assertTrue(EditorUiState.setCurrentAppTheme(AppTheme.LIGHT_CHARCOAL));
        assertSame(AppTheme.LIGHT_CHARCOAL, EditorUiState.currentAppTheme());
    }

    @Test
    public void theAppearanceIsProcessScopedNotPerWorkspace() {
        EditorUiState.setCurrentAppTheme(AppTheme.LIGHT_CHARCOAL);
        // The recreation that APPLIES an appearance destroys the workspace
        // holding it, so an instance field would be lost by the very act of
        // applying it and the new workspace would come back in the one just
        // left.
        assertSame("a workspace built after the switch wears the new appearance",
                AppTheme.LIGHT_CHARCOAL, new EditorUiState().appTheme());
    }

    @Test
    public void choosingAnAppearanceTouchesNothingElseTheUiRemembers() {
        final EditorUiState state = new EditorUiState();
        state.setDisplayUnit(LengthUnit.MILLIMETERS);
        state.setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_PLACE);
        state.setInspectorExpanded(false, false);

        EditorUiState.setCurrentAppTheme(AppTheme.NEUTRAL_CHARCOAL);

        assertEquals("an appearance is presentation and may not move a display unit",
                LengthUnit.MILLIMETERS, state.displayUnit());
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_PLACE, state.constructionTool());
        assertFalse(state.inspectorExpanded(false));
    }

    // -----------------------------------------------------------------------
    // Carrying the session across the recreation that applies a theme
    // -----------------------------------------------------------------------

    @Test
    public void aFreshWorkspaceGetsFreshStateWhenNothingWasCarried() {
        final EditorUiState fresh = EditorUiState.forNewWorkspace();
        assertEquals(LengthUnit.METERS, fresh.displayUnit());
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_SHAPE, fresh.constructionTool());
    }

    @Test
    public void theSessionSurvivesTheRecreationThatAppliesATheme() {
        final EditorUiState before = new EditorUiState();
        before.setDisplayUnit(LengthUnit.MILLIMETERS);
        before.setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_PLACE);
        before.setInspectorExpanded(false, false);
        before.recordStartChoice();

        EditorUiState.carryAcrossRecreation(before);
        final EditorUiState after = EditorUiState.forNewWorkspace();

        assertSame("changing colour must not also reset the session", before, after);
        assertEquals(LengthUnit.MILLIMETERS, after.displayUnit());
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_PLACE, after.constructionTool());
        assertFalse(after.inspectorExpanded(false));
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
