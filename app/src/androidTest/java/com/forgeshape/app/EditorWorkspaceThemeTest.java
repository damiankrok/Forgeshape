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
import static org.junit.Assert.assertFalse;
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
 * R1B2-01..17 — the three approved appearances.
 *
 * <p>Two things are being proven and they are not the same. One is that the
 * appearance actually changes: the chrome resolves different colours, the
 * renderer is told a different viewport ground, and both survive the events
 * that rebuild the view tree. The other — the one that matters more — is that
 * <b>nothing else changes at all</b>. An appearance is presentation, so the
 * scene, the active body, its exact specification, its placement, the product
 * mode and the Frozen Sculpt Mesh must come through a switch bit for bit, even
 * though the switch destroys and rebuilds every view in the product.
 *
 * <p>No test here asserts a rendered pixel. What the viewport draws is proven by
 * the native suites and by runtime evidence; what is asserted from Java is the
 * value the renderer was <i>told</i>, and the colours the chrome resolved.
 *
 * <h2>What the contrast thresholds mean</h2>
 *
 * <p>The twelve anchor values of each palette are owner-approved and are not
 * this suite's to move, so the thresholds state what those palettes actually
 * deliver rather than a number they would have to be redesigned to reach.
 * <b>Primary</b> text — every typed dimension, every panel title, every control
 * label — is held to WCAG AA at 4.5:1, because a value you cannot read is a
 * defect whatever it looks like. <b>Secondary</b> text is a caption role that
 * never carries an exact value, and is held to 3.0:1; across the three palettes
 * it measures between roughly 3.6 and 4.7. Verdict colours are held to 2.4:1
 * against the precision surface and must also stay distinguishable from body
 * text, which is the tightest number in the suite and is documented as such in
 * `PROJECT_STATUS.md`.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceThemeTest {

    /** WCAG AA for body text. Primary text and typed values are held to it. */
    private static final double MIN_TEXT_CONTRAST = 4.5;

    /** A caption role that never carries an exact value. See the class comment. */
    private static final double MIN_CAPTION_CONTRAST = 3.0;

    /** A verdict on the precision surface. See the class comment. */
    private static final double MIN_VERDICT_CONTRAST = 2.4;

    /** The id of the row that chooses each appearance, in declaration order. */
    private static final int[] APPEARANCE_IDS = {
            R.id.appearance_warm_graphite, R.id.appearance_neutral_charcoal,
            R.id.appearance_light_charcoal};

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBoxInTheDefaultAppearance() {
        resetToBaselineConstruction(rule.getScenario());
        switchTo(AppTheme.defaultTheme());
    }

    @After
    public void leaveTheProcessInTheDefaultAppearance() {
        switchTo(AppTheme.defaultTheme());
        releaseOrientation(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // R1B2-01 / R1B2-02 -- the default, and the control
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_01_theProductDefaultIsWarmGraphite() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertSame("Warm Graphite is what a process that has not been asked wears",
                    AppTheme.WARM_GRAPHITE, workspace.uiState().appTheme());
            assertEquals("and the renderer was told the same thing",
                    NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE,
                    NativeViewport.viewportBackground());
            return null;
        });
    }

    @Test
    public void r1b2_02_appearanceOffersExactlyTheThreeApprovedPalettes() {
        openDisplayPopover();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the approved set is three palettes",
                    3, AppTheme.values().length);
            assertEquals(AppTheme.values().length, APPEARANCE_IDS.length);

            final int floor = EditorControlStyles.dimen(activity, R.dimen.control_height);
            for (int i = 0; i < APPEARANCE_IDS.length; i++) {
                final View option = workspace.findViewById(APPEARANCE_IDS[i]);
                final String name = activity.getResources().getResourceEntryName(
                        APPEARANCE_IDS[i]);
                assertNotNull(name + ": the palettes live in the Display popover,"
                        + " not on a settings screen of their own", option);
                assertEquals(name + ": exactly the one in force is drawn selected",
                        AppTheme.values()[i] == workspace.uiState().appTheme(),
                        option.isActivated());
                // The stylus-first floor applies here as much as anywhere.
                assertTrue(name + " is " + option.getHeight() + " px tall",
                        option.getHeight() >= floor);
                assertTrue(name + " must answer a press like every other control",
                        hasPressedFeedback(option));
            }
            return null;
        });
        closeDisplayPopover();
    }

    // -----------------------------------------------------------------------
    // R1B2-03 / R1B2-04 / R1B2-05 -- the switch actually switches
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_03_everyPaletteResolvesItsOwnChromeColours() {
        final int[][] resolved = new int[AppTheme.values().length][];
        for (int i = 0; i < AppTheme.values().length; i++) {
            switchTo(AppTheme.values()[i]);
            resolved[i] = resolvedChromeRoles();
        }
        for (int a = 0; a < resolved.length; a++) {
            for (int b = a + 1; b < resolved.length; b++) {
                boolean differs = false;
                for (int role = 0; role < resolved[a].length; role++) {
                    if (resolved[a][role] != resolved[b][role]) {
                        differs = true;
                    }
                }
                assertTrue(AppTheme.values()[a] + " and " + AppTheme.values()[b]
                        + " resolved an identical chrome — they are the same"
                        + " appearance wearing two names", differs);
            }
        }
        // Not merely different: ordered. Light Charcoal is the lightest ground
        // in the set and its chrome has to be lighter than the other two, or the
        // palette does not do what its own name says.
        assertTrue("Light Charcoal's base chrome is the lightest of the three",
                luminance(resolved[2][0]) > luminance(resolved[0][0])
                        && luminance(resolved[2][0]) > luminance(resolved[1][0]));
    }

    @Test
    public void r1b2_04_everyPaletteIsADarkGroundAndTheWindowAgreesWithIt() {
        for (AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertEquals(theme + ": the VIEWPORT changes, not only the chrome"
                                + " — otherwise one palette's render would sit behind"
                                + " another palette's panels",
                        theme.viewportBackground(), NativeViewport.viewportBackground());

                // The window behind the surface must agree with what the
                // renderer clears to, or launching flashes the wrong shade.
                final int window = EditorControlStyles.themeColor(
                        activity, android.R.attr.windowBackground);
                assertTrue(theme + ": every approved ground is DARK, luminance was "
                                + luminance(window), luminance(window) < 0.20);
                assertTrue(theme + ": and none of them is near-black either,"
                                + " which is the ground the set replaced",
                        luminance(window) > 0.015);
                // Text on the model has to survive over the ground itself, since
                // the toolbar's control groups are translucent.
                final int primary = EditorControlStyles.themeColor(activity, R.attr.fsTextPrimary);
                assertTrue(theme + ": primary text over the bare ground: "
                                + contrast(primary, window),
                        contrast(primary, window) >= MIN_TEXT_CONTRAST);
                return null;
            });
        }
    }

    @Test
    public void r1b2_05_switchingBackRestoresTheDefaultExactly() {
        final int[] before = resolvedChromeRoles();
        switchTo(AppTheme.NEUTRAL_CHARCOAL);
        switchTo(AppTheme.LIGHT_CHARCOAL);
        switchTo(AppTheme.WARM_GRAPHITE);

        assertArrayEquals("the default is restored exactly, not approximately",
                before, resolvedChromeRoles());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE,
                    NativeViewport.viewportBackground());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B2-06 / R1B2-07 / R1B2-08 -- and changes nothing below JNI
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_06_switchingAppearancePublishesNothingAndChangesNoState() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        // Every hop in the set, not one: a switch that leaked would leak from
        // whichever pair happened to be untested.
        switchTo(AppTheme.NEUTRAL_CHARCOAL);
        switchTo(AppTheme.LIGHT_CHARCOAL);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] after = nativeSnapshot();
            // Bit for bit. Every revision the product mints is in here, so a
            // republication or a re-freeze caused by the switch would show as a
            // moved slot rather than having to be inferred.
            assertArrayEquals("an appearance is presentation and may not touch"
                            + " anything below JNI:" + describeSnapshotDifference(before, after),
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

        switchTo(AppTheme.LIGHT_CHARCOAL);

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

        switchTo(AppTheme.NEUTRAL_CHARCOAL);

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

        switchTo(AppTheme.LIGHT_CHARCOAL);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("an appearance change must not drop the user out of Sculpt",
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
    public void r1b2_10_theStartChooserDoesNotReappearAfterAnAppearanceRecreation() {
        for (AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            assertChooserStaysAnswered(theme + ": an appearance change is not a new session");
        }
    }

    @Test
    public void r1b2_11_theAppearanceSurvivesRotation() {
        switchTo(AppTheme.NEUTRAL_CHARCOAL);
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        assertWearing(AppTheme.NEUTRAL_CHARCOAL, "rotating must not change the appearance");

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        assertWearing(AppTheme.NEUTRAL_CHARCOAL, "and neither must rotating back");
    }

    @Test
    public void r1b2_12_theAppearanceSurvivesHomeAndResume() {
        switchTo(AppTheme.LIGHT_CHARCOAL);
        rule.getScenario().moveToState(Lifecycle.State.CREATED);
        settleLayout();
        rule.getScenario().moveToState(Lifecycle.State.RESUMED);
        settleLayout();

        assertWearing(AppTheme.LIGHT_CHARCOAL, "a resume must not change the appearance");
        assertChooserStaysAnswered("nor re-ask how the model began");
    }

    // -----------------------------------------------------------------------
    // R1B2-13 -- the chooser itself
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_13_theStartChooserIsDrawnInTheAppearanceInForce() {
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
                // A scrim has to DARKEN, or the panel does not come forward.
                final int scrim = EditorControlStyles.themeColor(activity, R.attr.fsChooserScrim);
                assertTrue(theme + ": the scrim darkens", luminance(scrim) < 0.25);
                assertTrue(theme + ": and is partial, so the viewport shows through",
                        Color.alpha(scrim) < 255);
                // Product chrome rather than a dialog: the panel is separated by
                // tone and depth, and draws no outline of its own.
                assertTrue(theme + ": the chooser panel is raised",
                        workspace.findViewById(R.id.start_chooser_panel).getElevation() > 0.0f);
                workspace.dismissStartChooserForConstruction();
                return null;
            });
            settleLayout();
        }
    }

    // -----------------------------------------------------------------------
    // R1B2-14 / R1B2-15 -- the UI-R1B1 rules hold in every appearance
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_14_iconsAndPressedFeedbackWorkInEveryAppearance() {
        for (AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                for (int id : new int[]{R.id.display_settings_button, R.id.hide_ui_toggle,
                        R.id.precision_toggle, R.id.objects_capsule_add,
                        R.id.inspector_toggle}) {
                    final View control = workspace.findViewById(id);
                    final String name = theme + "/"
                            + activity.getResources().getResourceEntryName(id);
                    assertTrue(name + " is still an image", control instanceof ImageView);
                    assertNotNull(name + " still carries an icon",
                            ((ImageView) control).getDrawable());
                    assertNotNull(name + " must tint from the theme, not a fixed colour",
                            ((ImageView) control).getImageTintList());
                    assertTrue(name + " must still answer a press",
                            hasPressedFeedback(control));
                }
                for (int id : new int[]{R.id.tool_rail_shape, R.id.primitive_option_box,
                        R.id.add_body, R.id.apply_shape, R.id.objects_capsule_active,
                        R.id.add_primitive_sphere}) {
                    assertTrue(theme + "/"
                                    + activity.getResources().getResourceEntryName(id)
                                    + " must still answer a press",
                            hasPressedFeedback(workspace.findViewById(id)));
                }
                // A reserved control must still read as reserved rather than
                // merely quiet. The Tool Rail no longer has one — every entry on
                // it works — so the rule is asserted where it is still true, on
                // the one approved-but-unimplemented GLOBAL action.
                final View export = workspace.findViewById(R.id.export_action);
                assertFalse(theme + ": Export stays inert", export.isEnabled());
                final int disabled = EditorControlStyles.themeColor(activity, R.attr.fsTextDisabled);
                final int secondary = EditorControlStyles.themeColor(activity,
                        R.attr.fsTextSecondary);
                assertTrue(theme + ": a reserved label is visibly dimmer than an"
                                + " available one — on a dark ground that means DARKER",
                        luminance(disabled) < luminance(secondary));
                return null;
            });
        }
    }

    @Test
    public void r1b2_15_thePropertyInspectorStaysReadableInEveryAppearance() {
        for (AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final int precision = EditorControlStyles.themeColor(
                        activity, R.attr.fsSurfacePrecision);
                final int field = EditorControlStyles.themeColor(activity, R.attr.fsFieldSurface);
                final int primary = EditorControlStyles.themeColor(activity, R.attr.fsTextPrimary);
                final int secondary = EditorControlStyles.themeColor(activity,
                        R.attr.fsTextSecondary);

                // The exact-value editors are the surface this stage must not
                // trade away for a look. A typed dimension has to be legible
                // before a panel is allowed to be pretty.
                assertTrue(theme + ": a typed value in its well: " + contrast(primary, field),
                        contrast(primary, field) >= MIN_TEXT_CONTRAST);
                assertTrue(theme + ": the panel title: " + contrast(primary, precision),
                        contrast(primary, precision) >= MIN_TEXT_CONTRAST);
                assertTrue(theme + ": a field caption: " + contrast(secondary, precision),
                        contrast(secondary, precision) >= MIN_CAPTION_CONTRAST);

                // Every verdict must be distinguishable from body text AND
                // readable. See the class comment on this threshold.
                for (int attr : new int[]{R.attr.fsTextSuccess, R.attr.fsTextError,
                        R.attr.fsTextMeasure}) {
                    final int verdict = EditorControlStyles.themeColor(activity, attr);
                    final String name = theme + "/"
                            + activity.getResources().getResourceEntryName(attr);
                    assertTrue(name + " on the panel: " + contrast(verdict, precision),
                            contrast(verdict, precision) >= MIN_VERDICT_CONTRAST);
                    assertNotEquals(name + " must not have collapsed into body text",
                            verdict, primary);
                }

                // A primary commit carries its own label colour, which is an INK
                // rather than white: the accent is bright enough that white on it
                // is 3.4:1 and would fail this outright.
                final int onPrimary = EditorControlStyles.themeColor(activity,
                        R.attr.fsTextOnPrimary);
                final int primaryFill = EditorControlStyles.themeColor(activity,
                        R.attr.fsPrimaryFill);
                assertTrue(theme + ": Apply's label on its own fill: "
                                + contrast(onPrimary, primaryFill),
                        contrast(onPrimary, primaryFill) >= MIN_TEXT_CONTRAST);

                // Dense numeric chrome stays opaque. This is the "no glass over
                // the numbers" rule expressed as something a test can check —
                // and it is why the precision tier exists as its own tier.
                assertEquals(theme + ": the Property Inspector's surface must not"
                        + " be translucent", 255, Color.alpha(precision));
                assertEquals(theme + ": and neither may a field", 255, Color.alpha(field));
                return null;
            });
        }
    }

    // -----------------------------------------------------------------------
    // R1B2-18 -- the material tiers, and where the accent is spent
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_18_theThreeMaterialTiersAreDistinctInEveryAppearance() {
        for (AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final int floating = EditorControlStyles.themeColor(
                        activity, R.attr.fsSurfaceFloating);
                final int context = EditorControlStyles.themeColor(
                        activity, R.attr.fsSurfaceContext);
                final int precision = EditorControlStyles.themeColor(
                        activity, R.attr.fsSurfacePrecision);

                // Tier 1 is the only tier that lets the model through. That is
                // the whole difference between a capsule standing ON the picture
                // and a panel carrying a body of content.
                assertTrue(theme + ": Tier 1 is translucent", Color.alpha(floating) < 255);
                assertEquals(theme + ": Tier 2 is opaque", 255, Color.alpha(context));
                assertEquals(theme + ": Tier 3 is opaque", 255, Color.alpha(precision));
                assertNotEquals(theme + ": a precision surface that matched the"
                                + " context surface would make three tiers into two",
                        context, precision);

                // Nothing here blurs, and nothing pretends to: the viewport is a
                // SurfaceView. Translucency is the whole of the effect, so the
                // one translucent tier must actually be translucent enough to
                // read as material rather than as a solid slab.
                assertTrue(theme + ": Tier 1's opacity is a material, not a pane"
                                + " of glass — alpha was " + Color.alpha(floating),
                        Color.alpha(floating) >= 200);
                return null;
            });
        }
    }

    @Test
    public void r1b2_19_aSelectionIsNotACommitInAnyAppearance() {
        for (AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final int selected = EditorControlStyles.themeColor(activity, R.attr.fsAccentFill);
                final int commit = EditorControlStyles.themeColor(activity, R.attr.fsPrimaryFill);
                final int accent = EditorControlStyles.themeColor(activity, R.attr.fsAccent);
                final int control = EditorControlStyles.themeColor(activity,
                        R.attr.fsControlSurface);

                assertNotEquals(theme + ": a selection wearing the commit's fill is"
                        + " what made five blue blocks compete on one screen",
                        selected, commit);
                assertEquals(theme + ": a commit IS the accent", accent, commit);
                assertTrue(theme + ": a selection is far quieter than the accent:"
                                + " selected=" + luminance(selected) + " accent="
                                + luminance(accent),
                        luminance(selected) < luminance(accent));
                // Quiet, but not invisible: a selected control must still be a
                // visible step off the control beside it, and since UI-R4B
                // removed the accent hairline that step carries MORE of the
                // signal than it did — which is why this assertion matters more
                // now, not less.
                //
                // Measured as a CHANNEL distance rather than as a contrast
                // ratio, deliberately. Two of the three palettes separate the
                // selected surface from a resting one mostly by lean — Neutral
                // Charcoal's selection is bluer at almost the same luminance —
                // and a luminance-only test would call a step the eye reads
                // easily "no step at all". The selection does not rest on this
                // alone: the brightened label from control_content_tint is the
                // second signal, and it is what keeps a selection readable for
                // someone who cannot separate the two blues.
                assertTrue(theme + ": a selection must be a visible step off a"
                                + " resting control, distance was "
                                + channelDistance(selected, control),
                        channelDistance(selected, control) >= 8);
                return null;
            });
        }
    }

    // -----------------------------------------------------------------------
    // R1B2-16 / R1B2-17 -- nothing else regressed
    // -----------------------------------------------------------------------

    @Test
    public void r1b2_16_theViewportKeepsItsFloorInEveryAppearance() {
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
    public void r1b2_17_theDisplayPopoverBehavesTheSameInEveryAppearance() {
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
     * <p>Driven by pressing the row rather than by setting the field, because
     * the recreation IS the mechanism under test: a helper that skipped it
     * would prove that a field changed and nothing about whether the workspace
     * survives being rebuilt.
     */
    private void switchTo(final AppTheme theme) {
        final Boolean alreadyWearing = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.uiState().appTheme() == theme);
        if (Boolean.TRUE.equals(alreadyWearing)) {
            return;
        }
        openDisplayPopover();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(APPEARANCE_IDS[theme.ordinal()]).performClick();
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
                        EditorControlStyles.themeColor(activity, R.attr.fsSurfaceFloating),
                        EditorControlStyles.themeColor(activity, R.attr.fsSurfaceContext),
                        EditorControlStyles.themeColor(activity, R.attr.fsSurfacePrecision),
                        EditorControlStyles.themeColor(activity, R.attr.fsControlSurface),
                        EditorControlStyles.themeColor(activity, R.attr.fsTextPrimary),
                        EditorControlStyles.themeColor(activity, R.attr.fsTextSecondary),
                        EditorControlStyles.themeColor(activity, R.attr.fsChromeHairline),
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
            final boolean inspectorOpen = workspace.propertyInspector().isOpen();
            final double floor = workspace.layoutMode() == WorkspaceLayoutMode.EXPANDED
                    ? 0.40 : (inspectorOpen ? 0.50 : 0.60);
            android.util.Log.i("ForgeShape", String.format(java.util.Locale.US,
                    "FORGESHAPE_R1B2_VIEWPORT %s unoccluded=%.1f%% floor=%.0f%%",
                    where, unoccluded * 100.0, floor * 100.0));
            assertTrue("an appearance may not cost the model room (" + where + "): "
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

    /**
     * The largest per-channel difference between two colours, 0..255.
     *
     * <p>Catches a separation the eye reads as a hue lean, which a luminance
     * contrast ratio scores as nothing at all.
     */
    private static int channelDistance(int a, int b) {
        return Math.max(Math.abs(Color.red(a) - Color.red(b)),
                Math.max(Math.abs(Color.green(a) - Color.green(b)),
                        Math.abs(Color.blue(a) - Color.blue(b))));
    }

    private static double contrast(int foreground, int background) {
        final double a = luminance(foreground);
        final double b = luminance(background);
        return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05);
    }
}
