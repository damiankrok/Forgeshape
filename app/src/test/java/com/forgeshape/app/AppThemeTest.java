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
 * The theme choice, checked without a device.
 *
 * <p>What matters here is not which colours a theme has — those are resources,
 * and a device draws them — but the three rules the choice itself has to obey:
 * Dark is the default a fresh process reports, the two appearances map onto two
 * DIFFERENT viewport backgrounds, and choosing one changes nothing else the UI
 * is allowed to remember.
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
    public void darkIsTheDocumentedDefault() {
        assertSame(AppTheme.DARK, AppTheme.defaultTheme());
        assertSame("a process that has not been asked wears the default",
                AppTheme.DARK, EditorUiState.currentAppTheme());
        assertSame(AppTheme.DARK, new EditorUiState().appTheme());
    }

    @Test
    public void thereAreExactlyTwoAppearances() {
        assertEquals("Dark and Light, and no automatic System member:"
                        + " following the system is a separate decision",
                2, AppTheme.values().length);
    }

    @Test
    public void eachAppearanceCarriesItsOwnViewportBackground() {
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_DARK,
                AppTheme.DARK.viewportBackground());
        assertEquals(NativeViewport.VIEWPORT_BACKGROUND_LIGHT,
                AppTheme.LIGHT.viewportBackground());
        assertNotEquals("a theme that did not change the viewport would leave a"
                        + " dark render behind light chrome",
                AppTheme.DARK.viewportBackground(), AppTheme.LIGHT.viewportBackground());
        assertNotEquals(AppTheme.DARK.styleRes(), AppTheme.LIGHT.styleRes());
    }

    @Test
    public void anUnrecognisedOrdinalFallsBackToTheDefault() {
        assertSame(AppTheme.DARK, AppTheme.fromOrdinal(0));
        assertSame(AppTheme.LIGHT, AppTheme.fromOrdinal(1));
        assertSame("the worst outcome of a bad index is the wrong colour, which"
                        + " is not a reason to refuse to start a workspace",
                AppTheme.DARK, AppTheme.fromOrdinal(2));
        assertSame(AppTheme.DARK, AppTheme.fromOrdinal(-1));
    }

    @Test
    public void choosingTheAppearanceAlreadyInForceIsNotAChange() {
        assertFalse("recreating the Activity for this would be a flash and"
                        + " nothing else", EditorUiState.setCurrentAppTheme(AppTheme.DARK));
        assertTrue(EditorUiState.setCurrentAppTheme(AppTheme.LIGHT));
        assertFalse(EditorUiState.setCurrentAppTheme(AppTheme.LIGHT));
        assertSame(AppTheme.LIGHT, EditorUiState.currentAppTheme());
    }

    @Test
    public void theAppearanceIsProcessScopedNotPerWorkspace() {
        EditorUiState.setCurrentAppTheme(AppTheme.LIGHT);
        // The recreation that APPLIES a theme destroys the workspace holding it,
        // so an instance field would be lost by the very act of applying it and
        // the new workspace would come back in the theme just left.
        assertSame("a workspace built after the switch wears the new appearance",
                AppTheme.LIGHT, new EditorUiState().appTheme());
    }

    @Test
    public void choosingAnAppearanceTouchesNothingElseTheUiRemembers() {
        final EditorUiState state = new EditorUiState();
        state.setDisplayUnit(LengthUnit.MILLIMETERS);
        state.setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_PLACE);
        state.setInspectorExpanded(false, false);

        EditorUiState.setCurrentAppTheme(AppTheme.LIGHT);

        assertEquals("a theme is presentation and may not move a display unit",
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
