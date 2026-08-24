package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.unoccludedViewportFraction;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.view.View;
import android.widget.ImageView;

import androidx.lifecycle.Lifecycle;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * R1B2-01..17 — the two themes.
 *
 * <p>Two things are being proven and they are not the same. One is that the
 * appearance actually changes: the chrome resolves different colours, the
 * renderer is told a different viewport background, and both survive the events
 * that rebuild the view tree. The other — the one that matters more — is that
 * <b>nothing else changes at all</b>. A theme is presentation, so the scene, the
 * active body, its exact specification, its placement, the product mode and the
 * Frozen Sculpt Mesh must come through a switch bit for bit, even though the
 * switch destroys and rebuilds every view in the product.
 *
 * <p>No test here asserts a rendered pixel. What the viewport draws is proven by
 * the native suites and by runtime evidence; what is asserted from Java is the
 * value the renderer was <i>told</i>, and the colours the chrome resolved.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceThemeTest {

    /** WCAG AA for body text. The Property Inspector's numbers are the reason
     *  this threshold is here rather than a looser one. */
    private static final double MIN_TEXT_CONTRAST = 4.5;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBoxInTheDefaultTheme() {
        resetToBaselineConstruction(rule.getScenario());
        switchTo(AppTheme.DARK);
    }

    @After
    public void leaveTheProcessInTheDefaultTheme() {
        switchTo(AppTheme.DARK);
        releaseOrientation(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // R1B2-01 / R1B2-02 -- the default, and the control
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_01_theProductDefaultIsDark() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertSame("Dark is what a process that has not been asked wears",
                    AppTheme.DARK, workspace.uiState().appTheme());
            assertEquals("and the renderer was told the same thing",
                    NativeViewport.VIEWPORT_BACKGROUND_DARK,
                    NativeViewport.viewportBackground());
            return null;
        });
    }

    @Test
    public void r1b2_02_appearanceOffersExactlyDarkAndLight() {
        openDisplayPopover();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View dark = workspace.findViewById(R.id.appearance_dark);
            final View light = workspace.findViewById(R.id.appearance_light);
            assertNotNull("the theme lives in the Display popover, not its own screen", dark);
            assertNotNull(light);
            assertEquals("exactly two appearances exist to choose between",
                    2, AppTheme.values().length);

            assertTrue("the one in force is drawn active", dark.isActivated());
            assertTrue("and the other is not", !light.isActivated());

            // The stylus-first floor applies here as much as anywhere.
            final int floor = EditorControlStyles.dimen(activity, R.dimen.control_height);
            assertTrue("Dark is " + dark.getHeight() + " px tall",
                    dark.getHeight() >= floor);
            assertTrue("Light is " + light.getHeight() + " px tall",
                    light.getHeight() >= floor);
            assertTrue("and both give pressed feedback like every other chip",
                    hasPressedFeedback(dark) && hasPressedFeedback(light));
            return null;
        });
        closeDisplayPopover();
    }

    // -----------------------------------------------------------------------
    // R1B2-03 / R1B2-04 / R1B2-05 -- the switch actually switches
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_03_switchingToLightResolvesDifferentChromeColours() {
        final int[] dark = resolvedChromeRoles();
        switchTo(AppTheme.LIGHT);
        final int[] light = resolvedChromeRoles();

        for (int i = 0; i < dark.length; i++) {
            assertNotEquals("every chrome role must answer differently in the"
                            + " other theme; role " + i + " did not",
                    dark[i], light[i]);
        }
        // Not merely different: inverted. A light theme whose surfaces were
        // merely a different dark would pass an inequality and be wrong.
        assertTrue("light chrome is lighter than dark chrome",
                luminance(light[0]) > luminance(dark[0]));
        assertTrue("and light text is darker than dark-theme text",
                luminance(light[3]) < luminance(dark[3]));
    }

    @Test
    public void r1b2_04_lightUsesTheWarmRendererBackground() {
        switchTo(AppTheme.LIGHT);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the VIEWPORT changes, not only the chrome — otherwise"
                            + " a dark render would sit behind light panels",
                    NativeViewport.VIEWPORT_BACKGROUND_LIGHT,
                    NativeViewport.viewportBackground());

            // The window behind the surface must agree with what the renderer
            // clears to, or launching flashes the wrong shade.
            final int window = EditorControlStyles.themeColor(
                    activity, android.R.attr.windowBackground);
            assertTrue("the light window background is warm: red leads, blue trails",
                    Color.red(window) > Color.green(window)
                            && Color.green(window) > Color.blue(window));
            assertTrue("and it is not a stark white canvas", Color.red(window) < 245);
            assertTrue("but it is genuinely light", luminance(window) > 0.7);
            return null;
        });
    }

    @Test
    public void r1b2_05_switchingBackRestoresDark() {
        final int[] before = resolvedChromeRoles();
        switchTo(AppTheme.LIGHT);
        switchTo(AppTheme.DARK);

        assertArrayEquals("Dark is restored exactly, not approximately",
                before, resolvedChromeRoles());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.VIEWPORT_BACKGROUND_DARK,
                    NativeViewport.viewportBackground());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B2-06 / R1B2-07 / R1B2-08 -- and changes nothing below JNI
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_06_switchingThemePublishesNothingAndChangesNoState() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        switchTo(AppTheme.LIGHT);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] after = nativeSnapshot();
            // Bit for bit. Every revision the product mints is in here, so a
            // republication or a re-freeze caused by the switch would show as a
            // moved slot rather than having to be inferred.
            assertArrayEquals("a theme is presentation and may not touch anything"
                            + " below JNI:" + describeSnapshotDifference(before, after),
                    before, after, 0.0);
            return null;
        });
    }

    @Test
    public void r1b2_07_theActiveBodyAndItsExactValuesSurvive() {
        final long[] identity = new long[2];
        final double[] before = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionCone(1.5, 3.0);
            NativeViewport.applyBoxTransform(0.75, -0.25, 1.5, 10.0, 20.0, 30.0);
            workspace.syncFromNative();
            identity[0] = NativeViewport.sceneActiveBodyId();
            identity[1] = NativeViewport.sceneBodyCount();
            return nativeSnapshot();
        });

        switchTo(AppTheme.LIGHT);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the active ObjectId is native truth and outlives the"
                            + " Activity the switch destroyed",
                    identity[0], NativeViewport.sceneActiveBodyId());
            assertEquals("and no body was created or lost",
                    identity[1], NativeViewport.sceneBodyCount());

            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            assertEquals(NativeViewport.PRIMITIVE_CONE, (int) primitive[0]);
            assertEquals(1.5, primitive[NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER], 1e-9);

            final double[] transform = new double[6];
            NativeViewport.boxTransform(transform);
            assertArrayEquals("the placement is untouched",
                    new double[]{0.75, -0.25, 1.5, 10.0, 20.0, 30.0}, transform, 1e-9);

            assertArrayEquals(before, nativeSnapshot(), 0.0);
            return null;
        });
    }

    @Test
    public void r1b2_08_constructionModeAndItsSurfacesSurvive() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.MODE_CONSTRUCTION, NativeViewport.productMode());
            return null;
        });

        switchTo(AppTheme.LIGHT);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the mode is native truth, read back by the workspace"
                            + " the switch built", NativeViewport.MODE_CONSTRUCTION,
                    NativeViewport.productMode());
            assertNotNull("and the Construction surfaces came back with it",
                    workspace.findViewById(R.id.tool_rail_shape));
            assertNotNull(workspace.findViewById(R.id.apply_shape));
            assertNotNull(workspace.findViewById(R.id.primitive_chooser));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B2-09 -- including a mesh that only exists because it was frozen
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_09_sculptModeAndTheFrozenMeshSurvive() {
        enterSculpt();
        final double[] frozen = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(state);
            return state;
        });

        switchTo(AppTheme.LIGHT);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("a theme change must not drop the user out of Sculpt",
                    NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            final double[] after = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(after);

            assertEquals("the SAME frozen mesh, not a fresh freeze",
                    frozen[NativeViewport.SCULPT_REVISION],
                    after[NativeViewport.SCULPT_REVISION], 0.0);
            assertEquals(frozen[NativeViewport.SCULPT_VERTEX_COUNT],
                    after[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
            assertEquals("belonging to the same body",
                    frozen[NativeViewport.SCULPT_OBJECT_ID],
                    after[NativeViewport.SCULPT_OBJECT_ID], 0.0);
            assertEquals("with its edits intact",
                    frozen[NativeViewport.SCULPT_HAS_EDITS],
                    after[NativeViewport.SCULPT_HAS_EDITS], 0.0);

            assertEquals(View.VISIBLE,
                    workspace.findViewById(R.id.brush_edge_controls).getVisibility());
            assertNotNull(workspace.findViewById(R.id.tool_rail_grab));
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.syncFromNative();
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B2-10 / R1B2-11 / R1B2-12 -- the choice, and the chooser, hold
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_10_theStartChooserDoesNotReappearAfterAThemeRecreation() {
        switchTo(AppTheme.LIGHT);
        assertChooserStaysAnswered("a theme change is not a new session");
        switchTo(AppTheme.DARK);
        assertChooserStaysAnswered("and neither is changing back");
    }

    @Test
    public void r1b2_11_theAppearanceSurvivesRotation() {
        switchTo(AppTheme.LIGHT);
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        assertWearing(AppTheme.LIGHT, "rotating must not change the appearance");

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        assertWearing(AppTheme.LIGHT, "and neither must rotating back");
    }

    @Test
    public void r1b2_12_theAppearanceSurvivesHomeAndResume() {
        switchTo(AppTheme.LIGHT);
        rule.getScenario().moveToState(Lifecycle.State.CREATED);
        settleLayout();
        rule.getScenario().moveToState(Lifecycle.State.RESUMED);
        settleLayout();

        assertWearing(AppTheme.LIGHT, "a resume must not change the appearance");
        assertChooserStaysAnswered("nor re-ask how the model began");
    }

    // -----------------------------------------------------------------------
    // R1B2-13 -- the chooser itself
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_13_theStartChooserIsDrawnInTheThemeInForce() {
        for (AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.showStartChooserAsFirstLaunch();
                return null;
            });
            settleLayout();
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertEquals(theme + ": the chooser is on screen", View.VISIBLE,
                        workspace.findViewById(R.id.start_chooser).getVisibility());
                final int card = EditorControlStyles.themeColor(
                        activity, R.attr.fsChooserCardSurface);
                final int title = EditorControlStyles.themeColor(activity, R.attr.fsTextPrimary);
                assertTrue(theme + ": an option's title must be readable on its card,"
                                + " contrast was " + contrast(title, card),
                        contrast(title, card) >= MIN_TEXT_CONTRAST);
                // A scrim has to DARKEN in both themes, or the panel does not
                // come forward on a light one.
                final int scrim = EditorControlStyles.themeColor(activity, R.attr.fsChooserScrim);
                assertTrue(theme + ": the scrim darkens", luminance(scrim) < 0.25);
                assertTrue(theme + ": and is partial, so the viewport shows through",
                        Color.alpha(scrim) < 255);
                workspace.dismissStartChooserForConstruction();
                return null;
            });
            settleLayout();
        }
    }

    // -----------------------------------------------------------------------
    // R1B2-14 / R1B2-15 -- the UI-R1B1 rules still hold on a light ground
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_14_iconsAndPressedFeedbackStillWorkInLight() {
        switchTo(AppTheme.LIGHT);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int id : new int[]{R.id.display_settings_button, R.id.hide_ui_toggle,
                    R.id.inspector_toggle}) {
                final View control = workspace.findViewById(id);
                final String name = activity.getResources().getResourceEntryName(id);
                assertTrue(name + " is still an image", control instanceof ImageView);
                assertNotNull(name + " still carries an icon",
                        ((ImageView) control).getDrawable());
                assertNotNull(name + " must tint from the theme, not a fixed colour",
                        ((ImageView) control).getImageTintList());
                assertTrue(name + " must still answer a press",
                        hasPressedFeedback(control));
            }
            for (int id : new int[]{R.id.tool_rail_shape, R.id.primitive_option_box,
                    R.id.add_body, R.id.apply_shape}) {
                assertTrue(activity.getResources().getResourceEntryName(id)
                                + " must still answer a press in Light",
                        hasPressedFeedback(workspace.findViewById(id)));
            }
            // A reserved entry must still read as reserved rather than merely
            // pale, which is a real risk when every surface got lighter.
            final View sketch = workspace.findViewById(R.id.tool_rail_sketch);
            assertTrue("Sketch stays inert", !sketch.isEnabled());
            final int disabled = EditorControlStyles.themeColor(activity, R.attr.fsTextDisabled);
            final int secondary = EditorControlStyles.themeColor(activity, R.attr.fsTextSecondary);
            assertTrue("and visibly dimmer than an available one",
                    luminance(disabled) > luminance(secondary));
            return null;
        });
    }

    @Test
    public void r1b2_15_thePropertyInspectorStaysReadableInLight() {
        switchTo(AppTheme.LIGHT);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int surface = EditorControlStyles.themeColor(activity, R.attr.fsChromeSurface);
            final int control = EditorControlStyles.themeColor(activity, R.attr.fsControlSurface);
            final int primary = EditorControlStyles.themeColor(activity, R.attr.fsTextPrimary);
            final int secondary = EditorControlStyles.themeColor(activity, R.attr.fsTextSecondary);

            // The exact-value editors are the surface this stage must not trade
            // away for a look. A typed dimension has to be legible before a
            // panel is allowed to be pretty.
            assertTrue("a typed value on its field: " + contrast(primary, control),
                    contrast(primary, control) >= MIN_TEXT_CONTRAST);
            assertTrue("a field label on the panel: " + contrast(secondary, surface),
                    contrast(secondary, surface) >= MIN_TEXT_CONTRAST);
            assertTrue("the panel title: " + contrast(primary, surface),
                    contrast(primary, surface) >= MIN_TEXT_CONTRAST);

            // Every verdict must be distinguishable from body text AND readable.
            for (int attr : new int[]{R.attr.fsTextSuccess, R.attr.fsTextError,
                    R.attr.fsTextMeasure}) {
                final int verdict = EditorControlStyles.themeColor(activity, attr);
                final String name = activity.getResources().getResourceEntryName(attr);
                assertTrue(name + " on the panel: " + contrast(verdict, surface),
                        contrast(verdict, surface) >= 3.0);
                assertNotEquals(name + " must not have collapsed into body text",
                        verdict, primary);
            }

            // A primary commit carries its own label colour, which on a light
            // theme is NOT the body colour.
            final int onPrimary = EditorControlStyles.themeColor(activity, R.attr.fsTextOnPrimary);
            final int primaryFill = EditorControlStyles.themeColor(activity, R.attr.fsPrimaryFill);
            assertTrue("Apply's label on its own fill: " + contrast(onPrimary, primaryFill),
                    contrast(onPrimary, primaryFill) >= MIN_TEXT_CONTRAST);

            // Dense numeric chrome stays opaque. This is the "no glass over the
            // numbers" rule expressed as something a test can check.
            assertEquals("the Property Inspector's surface must not be translucent",
                    255, Color.alpha(surface));
            assertEquals("and neither may a field", 255, Color.alpha(control));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B2-16 / R1B2-17 -- nothing else regressed
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_16_theViewportKeepsItsFloorInBothThemes() {
        for (AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            waitForLayout(rule.getScenario(), false);
            assertViewportFloorHolds(theme + " portrait");

            setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
            waitForLayout(rule.getScenario(), true);
            assertViewportFloorHolds(theme + " rotated");
        }
    }

    @Test
    public void r1b2_17_theDisplayPopoverBehavesTheSameInBothThemes() {
        for (AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertEquals(theme + ": starts closed", View.GONE,
                        workspace.findViewById(R.id.display_settings_popover).getVisibility());
                workspace.findViewById(R.id.display_settings_button).performClick();
                return null;
            });
            settleLayout();
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final View popover = workspace.findViewById(R.id.display_settings_popover);
                assertEquals(theme + ": opens", View.VISIBLE, popover.getVisibility());
                assertEquals(theme + ": grows from its anchor corner",
                        popover.getWidth(), (int) popover.getPivotX());
                assertEquals(0, (int) popover.getPivotY());
                assertEquals(theme + ": hangs under the toolbar's actual height",
                        workspace.findViewById(R.id.global_toolbar).getHeight(),
                        ((android.view.ViewGroup.MarginLayoutParams) popover.getLayoutParams())
                                .topMargin);
                // Selection feedback in place: a choice repaints one chip and
                // leaves the surface where it is.
                workspace.findViewById(R.id.surface_shading_faceted).performClick();
                return null;
            });
            settleLayout();
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertEquals(theme + ": a choice does not dismiss it", View.VISIBLE,
                        workspace.findViewById(R.id.display_settings_popover).getVisibility());
                workspace.findViewById(R.id.surface_shading_smooth).performClick();
                workspace.findViewById(R.id.display_settings_button).performClick();
                return null;
            });
            settleLayout();
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertEquals(theme + ": closes on a second tap", View.GONE,
                        workspace.findViewById(R.id.display_settings_popover).getVisibility());
                return null;
            });
        }
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    /**
     * Changes appearance through the product's own control, and waits for the
     * Activity the change recreates.
     *
     * <p>Driven by pressing the chip rather than by setting the field, because
     * the recreation IS the mechanism under test: a helper that skipped it
     * would prove that a boolean changed and nothing about whether the workspace
     * survives being rebuilt.
     */
    private void switchTo(AppTheme theme) {
        final Boolean alreadyWearing = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.uiState().appTheme() == theme);
        if (Boolean.TRUE.equals(alreadyWearing)) {
            return;
        }
        openDisplayPopover();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(theme == AppTheme.DARK
                    ? R.id.appearance_dark : R.id.appearance_light).performClick();
            return null;
        });
        // The Activity is being torn down and rebuilt underneath us; wait for a
        // workspace that reports the new appearance and has been laid out.
        for (int attempt = 0; attempt < 80; attempt++) {
            settleLayout();
            final Boolean ready = onWorkspace(rule.getScenario(),
                    (activity, workspace) -> workspace.uiState().appTheme() == theme
                            && workspace.getWidth() > 0);
            if (Boolean.TRUE.equals(ready)) {
                return;
            }
            SystemClock.sleep(100);
        }
        throw new AssertionError("the workspace never came back wearing " + theme);
    }

    private void openDisplayPopover() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.findViewById(R.id.display_settings_popover)
                    .getVisibility() != View.VISIBLE) {
                workspace.findViewById(R.id.display_settings_button).performClick();
            }
            return null;
        });
        settleLayout();
    }

    private void closeDisplayPopover() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.findViewById(R.id.display_settings_popover)
                    .getVisibility() == View.VISIBLE) {
                workspace.findViewById(R.id.display_settings_button).performClick();
            }
            return null;
        });
        settleLayout();
    }

    private void enterSculpt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            workspace.findViewById(sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0
                    ? R.id.resume_sculpt : R.id.freeze_to_sculpt).performClick();
            return null;
        });
        settleLayout();
    }

    private void assertWearing(final AppTheme theme, final String why) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertSame(why, theme, workspace.uiState().appTheme());
            assertEquals(why + " (the renderer too)", theme.viewportBackground(),
                    NativeViewport.viewportBackground());
            return null;
        });
    }

    private void assertChooserStaysAnswered(final String why) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(why, View.GONE,
                    workspace.findViewById(R.id.start_chooser).getVisibility());
            return null;
        });
    }

    /** The chrome roles that decide what the workspace looks like, resolved. */
    private int[] resolvedChromeRoles() {
        final int[] roles = onWorkspace(rule.getScenario(), (activity, workspace) ->
                new int[]{
                        EditorControlStyles.themeColor(activity, R.attr.fsChromeSurface),
                        EditorControlStyles.themeColor(activity, R.attr.fsChromeOverlay),
                        EditorControlStyles.themeColor(activity, R.attr.fsControlSurface),
                        EditorControlStyles.themeColor(activity, R.attr.fsTextPrimary),
                        EditorControlStyles.themeColor(activity, R.attr.fsTextSecondary),
                        EditorControlStyles.themeColor(activity, R.attr.fsControlBorder),
                });
        assertNotNull(roles);
        return roles;
    }

    private void assertViewportFloorHolds(final String where) {
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Rect[] chrome = workspace.chromeRects();
            final double unoccluded = unoccludedViewportFraction(workspace.getWidth(),
                    workspace.getHeight(), chrome);
            final boolean inspectorOpen = workspace.propertyInspector().isExpanded();
            final double floor = workspace.layoutMode() == WorkspaceLayoutMode.EXPANDED
                    ? 0.40 : (inspectorOpen ? 0.50 : 0.60);
            android.util.Log.i("ForgeShape", String.format(java.util.Locale.US,
                    "FORGESHAPE_R1B2_VIEWPORT %s unoccluded=%.1f%% floor=%.0f%%",
                    where, unoccluded * 100.0, floor * 100.0));
            assertTrue("a theme may not cost the model room (" + where + "): "
                    + String.format(java.util.Locale.US, "%.1f%%", unoccluded * 100.0),
                    unoccluded >= floor);
            return null;
        });
    }

    private static boolean hasPressedFeedback(View view) {
        final Drawable background = view.getBackground();
        if (background == null || !background.isStateful()) {
            return false;
        }
        final int[] restore = view.getDrawableState();
        try {
            background.setState(new int[]{android.R.attr.state_enabled,
                    android.R.attr.state_pressed});
            final Drawable pressed = background.getCurrent();
            background.setState(new int[]{android.R.attr.state_enabled});
            final Drawable idle = background.getCurrent();
            if (pressed == idle) {
                return false;
            }
            assertNotSame(pressed, idle);
            return true;
        } finally {
            background.setState(restore);
        }
    }

    // -----------------------------------------------------------------------
    // Contrast
    //
    // WCAG relative luminance and contrast ratio, implemented here rather than
    // depended on: this is a dozen lines of arithmetic and the project has no
    // runtime dependency to spend on it.
    // -----------------------------------------------------------------------

    private static double channel(int component) {
        final double v = component / 255.0;
        return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
    }

    private static double luminance(int color) {
        return 0.2126 * channel(Color.red(color))
                + 0.7152 * channel(Color.green(color))
                + 0.0722 * channel(Color.blue(color));
    }

    private static double contrast(int foreground, int background) {
        final double a = luminance(foreground);
        final double b = luminance(background);
        return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05);
    }
}
