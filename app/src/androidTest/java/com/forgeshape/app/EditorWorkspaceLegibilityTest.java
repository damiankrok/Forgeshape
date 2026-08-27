package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closePrecision;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.isFullyOnScreen;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openAddPrimitive;
import static com.forgeshape.app.WorkspaceTestSupport.openObjectsPanel;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.selectConstructionTool;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settle;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileInputStream;

/**
 * UIR5A-01..16 — the structural shell, the exact-value panel, semantic contrast
 * and the gizmo's touch contract.
 *
 * <p>Every case here exists because a measured defect existed. The trailing
 * cluster squeezed its own last children to 17 dp and then to nothing; System
 * Back left the app with a palette open; a long negative coordinate rendered as
 * a plausible positive one and a tap on a populated field appended to it; two
 * semantic text roles failed the repository's own contrast target in all three
 * appearances; and the gizmo's handles were told apart by position alone.
 *
 * <p>Two rules, the same as every other suite here. Nothing is located by screen
 * coordinate — every control is named by its stable semantic id. And nothing
 * asserts a rendered pixel: what the viewport draws is proven by the native
 * suites and by runtime evidence, and what is asserted from Java is geometry,
 * state and resolved colour values.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceLegibilityTest {

    /** The interactive floor, and it is the HIT AREA. */
    private static final int TOUCH_FLOOR_DP = 48;

    /** WCAG AA for normal-sized text. Both roles corrected in this stage are
     *  held to it on every ground they are drawn on. */
    private static final double MIN_TEXT_CONTRAST = 4.5;

    /** The system font scale the large-text cases run at. */
    private static final String LARGE_FONT_SCALE = "1.3";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void leaveTheDeviceAsItWasFound() {
        shell("settings put system font_scale 1.0");
        waitForFontScale(1.0f);
        releaseOrientation(rule.getScenario());
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closePrecision(workspace);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR5A-01 / UIR5A-02 / UIR5A-03 / UIR5A-04 — the trailing cluster
    // -----------------------------------------------------------------------

    /**
     * UIR5A-01. Compact portrait: the persistent trailing controls keep a stable
     * anchor across the contextual changes, and every one of them keeps a 48 dp
     * target.
     */
    @Test
    public void uir5a01_compactPortraitKeepsItsAnchorAndItsTouchTargets() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        settleLayout();

        final Rect railAtRest = boundsOf(R.id.tool_rail);
        final Rect toggleAtRest = boundsOf(R.id.precision_toggle);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            return null;
        });
        settleLayout();

        assertEquals("selecting Transform must not move the Tool Rail: it is on"
                        + " screen in every state and the selectors are not",
                railAtRest, boundsOf(R.id.tool_rail));
        assertEquals("nor the precision toggle", toggleAtRest,
                boundsOf(R.id.precision_toggle));
        assertTouchFloor("compact portrait, Transform held");
    }

    /** UIR5A-02. A short landscape window keeps every persistent control usable. */
    @Test
    public void uir5a02_shortLandscapeKeepsEveryPersistentControlUsable() {
        enterTransformInLandscape();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the window under test must actually be short and wide,"
                            + " or this case proves nothing",
                    workspace.getWidth() > workspace.getHeight());
            assertVisible(workspace, R.id.precision_toggle);
            assertVisible(workspace, R.id.transform_mode_group);
            assertVisible(workspace, R.id.transform_space_group);
            return null;
        });
        assertTouchFloor("short landscape, Transform held");
    }

    /**
     * UIR5A-03. A larger system font grows the type and must not take a control
     * with it.
     *
     * <p>This is the configuration that measured the worst: the precision toggle
     * was 17.1 dp — 64 % of its height gone — because text elsewhere in the
     * column grew and the column squeezed whatever was last.
     */
    @Test
    public void uir5a03_largeTextDoesNotCollapseTheTrailingControls() {
        shell("settings put system font_scale " + LARGE_FONT_SCALE);
        assertTrue("the font scale did not take effect, so this case proves"
                + " nothing", waitForFontScale(1.25f));
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertVisible(workspace, R.id.precision_toggle);
            assertVisible(workspace, R.id.transform_mode_group);
            return null;
        });
        assertTouchFloor("font_scale " + LARGE_FONT_SCALE);
    }

    /**
     * UIR5A-04. The keyboard takes the bottom of the window, and it may not take
     * a control with it.
     *
     * <p>Measured before this stage: with the IME up the transform-mode capsule
     * was left 2.3 dp tall, and the space capsule and the precision toggle were
     * not in the view tree at all.
     *
     * <p><b>The keyboard reaches the workspace as one thing: a bottom inset.</b>
     * The chrome consumes it as padding and the {@code SurfaceView} ignores it,
     * so what the cluster has to survive is a shorter row — and this case makes
     * that squeeze happen rather than asking a device for one. A real soft
     * keyboard is raised first and used when it appears; when it does not, an
     * IME inset of the same size is dispatched instead, and the case asserts the
     * chrome actually took it before it measures anything. Runtime evidence
     * covers the real keyboard visually.
     */
    @Test
    public void uir5a04_theKeyboardDoesNotDestroyTheTrailingControls() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            openPrecision(workspace);
            return null;
        });
        settleLayout();

        int imeInset = 0;
        for (int attempt = 0; attempt < 2 && imeInset == 0; attempt++) {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final EditText field = workspace.findViewById(R.id.field_pos_x);
                field.requestFocus();
                activity.getSystemService(InputMethodManager.class)
                        .showSoftInput(field, InputMethodManager.SHOW_IMPLICIT);
                return null;
            });
            settleLayout();
            settleLayout();
            imeInset = onWorkspace(rule.getScenario(),
                    (activity, workspace) -> imeBottomInset(workspace));
        }

        final int requested = imeInset;
        final int applied = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (requested > 0) {
                return workspace.chromeBottomInsetPx();
            }
            // No soft keyboard on this device. Dispatch an inset of the size one
            // would have — 40 % of the window, which is a large phone keyboard —
            // through the same path the platform uses.
            final int synthetic = Math.round(workspace.getHeight() * 0.40f);
            workspace.dispatchApplyWindowInsets(imeInsets(workspace, synthetic));
            return synthetic;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the squeeze must actually be in force, or this case proves"
                            + " nothing: chrome bottom inset was "
                            + workspace.chromeBottomInsetPx() + " px for a keyboard of "
                            + applied + " px",
                    workspace.chromeBottomInsetPx() >= Math.min(applied, 1));
            assertVisible(workspace, R.id.precision_toggle);
            assertVisible(workspace, R.id.transform_mode_group);
            assertVisible(workspace, R.id.transform_space_group);
            return null;
        });
        assertTouchFloor("IME open");
    }

    /** A {@link WindowInsets} carrying an IME inset of this height. */
    private static WindowInsets imeInsets(View workspace, int bottomPx) {
        final WindowInsets current = workspace.getRootWindowInsets();
        final WindowInsets.Builder builder = current != null
                ? new WindowInsets.Builder(current) : new WindowInsets.Builder();
        return builder.setInsets(WindowInsets.Type.ime(),
                android.graphics.Insets.of(0, 0, 0, bottomPx)).build();
    }

    /**
     * UIR5A-05. The controls that are not contextual keep their bounds while the
     * contextual ones come and go.
     *
     * <p>The Tool Rail is anchored and never moves at all. The precision toggle
     * keeps its exact bounds across every transform-mode change, including the
     * one that withdraws the space capsule — which is the change that used to
     * move the mode capsule out from under the finger that had just pressed it.
     */
    @Test
    public void uir5a05_contextualChangesDisplaceNothingElseInTheCluster() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            return null;
        });
        settleLayout();

        final Rect rail = boundsOf(R.id.tool_rail);
        final Rect toggle = boundsOf(R.id.precision_toggle);
        final Rect mode = boundsOf(R.id.transform_mode_group);

        for (final int modeButton : new int[]{R.id.transform_mode_rotate,
                R.id.transform_mode_scale, R.id.transform_mode_move}) {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.findViewById(modeButton).performClick();
                return null;
            });
            settleLayout();
            assertEquals("the Tool Rail must not move when the transform mode does",
                    rail, boundsOf(R.id.tool_rail));
            assertEquals("nor the precision toggle", toggle, boundsOf(R.id.precision_toggle));
            assertEquals("nor the mode capsule itself — two identical taps must"
                            + " land on the same control", mode,
                    boundsOf(R.id.transform_mode_group));
        }

        // And the surface that opens over the model restores every bound exactly
        // when it closes.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();
        assertEquals("the rail keeps its anchor while the precision surface is open",
                rail.top, boundsOf(R.id.tool_rail).top);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closePrecision(workspace);
            return null;
        });
        settleLayout();
        assertEquals("and everything is exactly restored when it closes", rail,
                boundsOf(R.id.tool_rail));
        assertEquals("including the toggle that opened it", toggle,
                boundsOf(R.id.precision_toggle));
    }

    // -----------------------------------------------------------------------
    // UIR5A-06 — System Back
    // -----------------------------------------------------------------------

    /**
     * UIR5A-06. Back dismisses the topmost context surface, and only leaves once
     * there is nothing left to dismiss.
     *
     * <p>The palette case is driven with a real {@code KEYCODE_BACK} rather than
     * by calling the workspace, because the defect was not in what dismissal
     * does — it was that the platform's most common dismissal gesture never
     * reached it.
     */
    @Test
    public void uir5a06_backDismissesTheTopmostSurfaceBeforeItLeaves() {
        assertFalse("a resting workspace has nothing for Back to dismiss, so Back"
                        + " still means what the platform says it means",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.hasDismissibleSurface()));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();

        assertTrue("Add Primitive is open, so Back is ours to consume",
                onWorkspace(rule.getScenario(), (activity, workspace) -> {
                    assertTrue("the palette must actually be open",
                            workspace.addPrimitivePalette().isOpen());
                    return workspace.hasDismissibleSurface();
                }));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            assertTrue("and the platform callback is registered while it is",
                    onWorkspace(rule.getScenario(),
                            (activity, workspace) -> activity.backDismissalArmed()));
        }

        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("one Back closed the palette", workspace.addPrimitivePalette().isOpen());
            assertFalse("and did NOT leave the app", activity.isFinishing());
            return null;
        });

        // Every other dismissible surface, through the same one decision.
        assertBackDismisses(R.id.objects_capsule_active);
        assertBackDismissesPrecision();
        assertBackDismissesDisplay();
    }

    // -----------------------------------------------------------------------
    // UIR5A-07 / UIR5A-08 / UIR5A-09 / UIR5A-10 — the exact-value panel
    // -----------------------------------------------------------------------

    /**
     * UIR5A-07. A long signed value keeps its sign and its leading digits, and
     * the complete value is what is parsed.
     */
    @Test
    public void uir5a07_aLongSignedValueKeepsItsSignAndItsCompleteValue() {
        final double authored = -98765.4321098;
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            openPrecision(workspace);
            NativeViewport.applyBoxTransform(authored, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            workspace.syncFromNative();
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final NumericPropertyRow row = workspace.placementEditor().rowFor(R.id.field_pos_x);
            assertNotNull("the field under test must exist", row);
            final String complete = row.text();
            final String drawn = row.field().getText().toString();

            assertEquals("the COMPLETE value is what the panel holds and what"
                            + " Apply would submit", authored,
                    Double.parseDouble(complete.replace(',', '.')), 0.0);
            assertTrue("and what is drawn starts with the sign, whether or not it"
                            + " was shortened: " + drawn, drawn.startsWith("-9"));
            if (row.displayIsShortened()) {
                assertTrue("a shortened display says so, at its END: " + drawn,
                        drawn.endsWith("…"));
                assertTrue("and it is a PREFIX of the complete value, so no digit"
                                + " it shows is a digit the value does not have",
                        complete.startsWith(drawn.substring(0, drawn.length() - 1)));
            }
            return null;
        });

        // And focusing it brings the whole value back, so an edit is an edit of
        // the number and never of an ellipsis.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final NumericPropertyRow row = workspace.placementEditor().rowFor(R.id.field_pos_x);
            row.field().requestFocus();
            return null;
        });
        settle();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final NumericPropertyRow row = workspace.placementEditor().rowFor(R.id.field_pos_x);
            assertEquals("editing edits the complete value", row.text(),
                    row.field().getText().toString());
            return null;
        });
    }

    /**
     * UIR5A-08. A tap on a populated field selects it, so the first keystroke
     * replaces rather than appends.
     *
     * <p>The malformed string this prevents was measured: a field reading
     * {@code 0}, typed into, produced {@code 0-98765.4321098}, and the panel
     * accepted it into its own state.
     */
    @Test
    public void uir5a08_tappingAPopulatedFieldReplacesRatherThanAppends() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            openPrecision(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final NumericPropertyRow row = workspace.placementEditor().rowFor(R.id.field_pos_x);
            assertTrue("the field must start populated, or this case proves nothing",
                    row.text().length() > 0);
            row.field().requestFocus();
            return null;
        });
        settle();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final NumericPropertyRow row = workspace.placementEditor().rowFor(R.id.field_pos_x);
            final EditText field = row.field();
            final int start = Math.min(field.getSelectionStart(), field.getSelectionEnd());
            final int end = Math.max(field.getSelectionStart(), field.getSelectionEnd());
            assertEquals("focus selects from the first character", 0, start);
            assertEquals("to the last", field.getText().length(), end);

            // What a keystroke does to that selection, done exactly as the IME
            // would do it: replace what is selected.
            field.getText().replace(start, end, "-5");
            assertEquals("so one keystroke replaces the value", "-5",
                    field.getText().toString());
            assertEquals("and the complete value follows what was typed", "-5", row.text());
            return null;
        });
    }

    /**
     * UIR5A-09. Scale offers no unit, and the unit control sits inside the group
     * it actually governs.
     */
    @Test
    public void uir5a09_scaleOffersNoUnitAndTheChipsBelongToPosition() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            openPrecision(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ConstructionPlacementEditorView editor = workspace.placementEditor();
            final int chips = indexOfAncestorChild(editor,
                    workspace.findViewById(R.id.unit_chip_m));
            final int position = indexOfAncestorChild(editor,
                    workspace.findViewById(R.id.field_pos_x));
            final int rotation = indexOfAncestorChild(editor,
                    workspace.findViewById(R.id.field_rot_x));
            final int scale = indexOfAncestorChild(editor,
                    workspace.findViewById(R.id.field_scale_x));
            assertTrue("every group must be found: " + position + "/" + rotation + "/"
                    + scale + "/" + chips, position >= 0 && rotation >= 0 && scale >= 0
                    && chips >= 0);
            assertTrue("the unit chips sit under Position, which is what they"
                    + " convert", chips > position && chips < rotation);
            assertTrue("and nothing about a unit follows the Scale row, which is"
                    + " unitless by a hard product rule", chips < scale);

            // Rotation is degrees and Scale is a bare multiplier, in both cases
            // stated in the heading rather than left to be assumed.
            assertTrue("Rotation names its unit",
                    textOfSectionBefore(editor, rotation).toLowerCase().contains("degrees"));
            assertFalse("Scale names no length unit",
                    textOfSectionBefore(editor, scale).toLowerCase().contains("mm"));
            return null;
        });
    }

    /**
     * UIR5A-10. The commit is on screen without scrolling, in every window this
     * run can reach.
     *
     * <p>It is pinned below the scrolling body rather than being the last row of
     * it, so this is true by construction in every placement — bottom sheet,
     * side overlay and the expanded window's docked column alike.
     */
    @Test
    public void uir5a10_applyIsReachableWithoutScrollingInEveryWindow() {
        assertApplyIsPinned("compact portrait", ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, false);
        assertApplyIsPinned("short landscape", ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, true);
    }

    // -----------------------------------------------------------------------
    // UIR5A-11 — semantic text contrast
    // -----------------------------------------------------------------------

    /**
     * UIR5A-11. The secondary and error text roles clear WCAG AA against every
     * ground they are drawn on, in all three appearances.
     */
    @Test
    public void uir5a11_secondaryAndErrorTextClearTheContrastTargetEverywhere() {
        for (final AppTheme theme : AppTheme.values()) {
            switchTo(theme);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final int secondary = EditorControlStyles.themeColor(activity,
                        R.attr.fsTextSecondary);
                final int error = EditorControlStyles.themeColor(activity, R.attr.fsTextError);
                // Every ground a normal-sized secondary or error string is
                // actually drawn on: the status capsule stands on the floating
                // material over the viewport, captions and headings on the
                // precision surface, chip labels on a control fill, and typed
                // text in a field well.
                final int[] grounds = {android.R.attr.windowBackground, R.attr.fsChromeSurface,
                        R.attr.fsSurfaceContext, R.attr.fsSurfacePrecision,
                        R.attr.fsControlSurface, R.attr.fsFieldSurface};
                for (int ground : grounds) {
                    final int background = EditorControlStyles.themeColor(activity, ground);
                    final String name = theme + "/"
                            + activity.getResources().getResourceEntryName(ground);
                    assertTrue(name + ": a caption is " + contrast(secondary, background)
                                    + ":1 and must be at least " + MIN_TEXT_CONTRAST,
                            contrast(secondary, background) >= MIN_TEXT_CONTRAST);
                    assertTrue(name + ": a refusal is " + contrast(error, background)
                                    + ":1 and must be at least " + MIN_TEXT_CONTRAST,
                            contrast(error, background) >= MIN_TEXT_CONTRAST);
                }
                // And a quieter role is still quieter: raising contrast must not
                // have flattened the hierarchy it sits in.
                assertTrue(theme + ": secondary stays below primary",
                        luminance(secondary) < luminance(EditorControlStyles.themeColor(
                                activity, R.attr.fsTextPrimary)));
                assertTrue(theme + ": and a reserved label stays below secondary",
                        luminance(EditorControlStyles.themeColor(activity, R.attr.fsTextDisabled))
                                < luminance(secondary));
                return null;
            });
        }
        switchTo(AppTheme.defaultTheme());
    }

    // -----------------------------------------------------------------------
    // UIR5A-12 / UIR5A-13 / UIR5A-14 — the gizmo
    // -----------------------------------------------------------------------

    /**
     * UIR5A-12. Every handle answers to a touch across the whole of its declared
     * target, and where two targets overlap the answer is deterministic.
     *
     * <p>The pixels come from native code, which is the same answer the renderer
     * draws with — no coordinate is written down here.
     */
    @Test
    public void uir5a12_everyHandleMeetsTheTouchContract() {
        // The pivot's pixel, read where it is a handle and therefore reportable.
        // The camera and the body are fixed by the baseline reset, so it is the
        // same point in all three modes — and it is where the ONE deliberate
        // non-hit lives.
        enterTransform(NativeViewport.GIZMO_MODE_SCALE);
        final float[] pivot = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] point = new float[2];
            assertTrue("the uniform handle sits on the pivot and is reportable",
                    NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_UNIFORM, point));
            return point;
        });

        for (final int mode : new int[]{NativeViewport.GIZMO_MODE_MOVE,
                NativeViewport.GIZMO_MODE_ROTATE, NativeViewport.GIZMO_MODE_SCALE}) {
            enterTransform(mode);
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final float density = activity.getResources().getDisplayMetrics().density;
                // Inside the declared 24 dp radius, and far enough in to be a
                // real probe rather than a rounding.
                final float probePx = (TOUCH_FLOOR_DP * 0.5f - 4.0f) * density;
                // The pivot disc names no handle in Move and Rotate by design:
                // every shaft converges there and every ring passes around it.
                final float deadPx = TOUCH_FLOOR_DP * 0.5f * density;
                for (int handle : handlesFor(mode)) {
                    final float[] point = new float[2];
                    assertTrue("native must report where handle " + handle + " is",
                            NativeViewport.gizmoHandlePoint(handle, point));

                    assertEquals("mode " + mode + ": a touch on a handle grabs that"
                                    + " handle", handle,
                            NativeViewport.gizmoHitTest(point[0], point[1]));

                    // And the target is at least the floor ACROSS. A neighbour may
                    // win where two targets overlap — that is the declared
                    // priority, uniform before planes before axes — but no probe
                    // inside a handle's own radius may grab nothing, unless it has
                    // fallen into the pivot disc, which grabs nothing on purpose.
                    final float[][] offsets = {{probePx, 0}, {-probePx, 0},
                            {0, probePx}, {0, -probePx}};
                    for (float[] offset : offsets) {
                        final float x = point[0] + offset[0];
                        final float y = point[1] + offset[1];
                        final int hit = NativeViewport.gizmoHitTest(x, y);
                        final boolean insidePivotDisc =
                                mode != NativeViewport.GIZMO_MODE_SCALE
                                        && Math.hypot(x - pivot[0], y - pivot[1]) < deadPx;
                        if (insidePivotDisc) {
                            assertEquals("mode " + mode + ": the pivot disc names no"
                                            + " handle, and that is deliberate",
                                    NativeViewport.GIZMO_HANDLE_NONE, hit);
                            continue;
                        }
                        assertTrue("mode " + mode + " handle " + handle + " at +("
                                        + offset[0] + "," + offset[1] + ") grabbed nothing",
                                hit != NativeViewport.GIZMO_HANDLE_NONE);
                    }
                }
                return null;
            });
        }
    }

    /**
     * UIR5A-13. The handle a drag is holding is the one native code reports, in
     * every mode — which is what the renderer draws stronger and draws every
     * other handle away from.
     *
     * <p>What that LOOKS like is asserted where it is decided: the gizmo
     * self-test checks that the held colour is one no axis owns and that the
     * unheld weight is lower. Here the claim is that the state reaches the
     * renderer at all, for a real grab in each mode.
     */
    @Test
    public void uir5a13_theHeldHandleIsReportedInEveryMode() {
        for (final int mode : new int[]{NativeViewport.GIZMO_MODE_MOVE,
                NativeViewport.GIZMO_MODE_ROTATE, NativeViewport.GIZMO_MODE_SCALE}) {
            enterTransform(mode);
            final int handle = handlesFor(mode)[0];
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final float[] point = new float[2];
                assertTrue(NativeViewport.gizmoHandlePoint(handle, point));
                final View viewport = workspace.findViewById(R.id.viewport_surface);
                final long down = SystemClock.uptimeMillis();
                dispatchToViewport(viewport, down, down,
                        android.view.MotionEvent.ACTION_DOWN, point[0], point[1]);

                final double[] state = new double[NativeViewport.GIZMO_STATE_SIZE];
                NativeViewport.gizmoState(state);
                assertEquals("mode " + mode + ": the grab captured",
                        1.0, state[NativeViewport.GIZMO_CAPTURING], 0.0);
                assertEquals("and the held handle is the one that was grabbed",
                        (double) handle, state[NativeViewport.GIZMO_HANDLE], 0.0);

                dispatchToViewport(viewport, down, down + 16L,
                        android.view.MotionEvent.ACTION_CANCEL, point[0], point[1]);
                NativeViewport.gizmoState(state);
                assertEquals("and a cancelled grab holds nothing", 0.0,
                        state[NativeViewport.GIZMO_CAPTURING], 0.0);
                return null;
            });
        }
    }

    /**
     * UIR5A-14. The transform semantics this stage was not allowed to touch are
     * exactly as they were: which handles each mode offers, which space each
     * mode allows, and where each handle is grabbed.
     */
    @Test
    public void uir5a14_transformSemanticsAreUnchanged() {
        // Which handles a mode OFFERS is asserted the way a finger asks: by what
        // a touch on that handle's own point grabs. `gizmoHandlePoint` answers
        // for any handle from the snapshot's geometry and is not the membership
        // question.
        enterTransform(NativeViewport.GIZMO_MODE_MOVE);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertGrabs("Move", new int[]{NativeViewport.GIZMO_HANDLE_AXIS_X,
                    NativeViewport.GIZMO_HANDLE_AXIS_Y, NativeViewport.GIZMO_HANDLE_AXIS_Z,
                    NativeViewport.GIZMO_HANDLE_PLANE_XY, NativeViewport.GIZMO_HANDLE_PLANE_XZ,
                    NativeViewport.GIZMO_HANDLE_PLANE_YZ});
            assertPivotGrabs("Move", NativeViewport.GIZMO_HANDLE_NONE);
            assertTrue("Move offers a space choice",
                    NativeViewport.setGizmoSpace(NativeViewport.GIZMO_SPACE_LOCAL));
            assertTrue(NativeViewport.setGizmoSpace(NativeViewport.GIZMO_SPACE_WORLD));
            return null;
        });

        enterTransform(NativeViewport.GIZMO_MODE_ROTATE);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertGrabs("Rotate", new int[]{NativeViewport.GIZMO_HANDLE_AXIS_X,
                    NativeViewport.GIZMO_HANDLE_AXIS_Y, NativeViewport.GIZMO_HANDLE_AXIS_Z});
            assertPivotGrabs("Rotate", NativeViewport.GIZMO_HANDLE_NONE);
            assertTrue("Rotate offers a space choice",
                    NativeViewport.setGizmoSpace(NativeViewport.GIZMO_SPACE_LOCAL));
            assertTrue(NativeViewport.setGizmoSpace(NativeViewport.GIZMO_SPACE_WORLD));
            return null;
        });

        enterTransform(NativeViewport.GIZMO_MODE_SCALE);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertGrabs("Scale", new int[]{NativeViewport.GIZMO_HANDLE_AXIS_X,
                    NativeViewport.GIZMO_HANDLE_AXIS_Y, NativeViewport.GIZMO_HANDLE_AXIS_Z,
                    NativeViewport.GIZMO_HANDLE_PLANE_XY, NativeViewport.GIZMO_HANDLE_PLANE_XZ,
                    NativeViewport.GIZMO_HANDLE_PLANE_YZ});
            assertPivotGrabs("Scale", NativeViewport.GIZMO_HANDLE_UNIFORM);
            assertFalse("Scale is Local only — a world-axis scale of a turned"
                            + " body is a shear",
                    NativeViewport.setGizmoSpace(NativeViewport.GIZMO_SPACE_WORLD));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR5A-15 / UIR5A-16 — what must not have regressed
    // -----------------------------------------------------------------------

    /** UIR5A-15. The anchored surfaces still displace nothing. */
    @Test
    public void uir5a15_anchoredSurfacesStillDisplaceNothing() {
        final int[] watched = {R.id.tool_rail, R.id.precision_toggle, R.id.objects_capsule_active,
                R.id.history_group, R.id.status_message};
        final Rect[] before = boundsOfAll(watched);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        assertBoundsUnchanged("Add Primitive", watched, before);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closeAddPrimitive(workspace);
            openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        assertBoundsUnchanged("the Objects popover", watched, before);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        assertBoundsUnchanged("closing them again", watched, before);
    }

    /**
     * UIR5A-16. A Back dismissal is a dismissal like any other: it plays the
     * surface's own exit, and under reduced motion it lands at once.
     */
    @Test
    public void uir5a16_backDismissalUsesTheSurfacesOwnExit() {
        shell("settings put global animator_duration_scale 0");
        settle();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("dismissal is ours", workspace.dismissTopmostSurface());
            assertFalse("reduced motion lands at once — no transient state",
                    workspace.addPrimitivePalette().isOpen());
            assertTrue("and the surface has settled in the same frame",
                    workspace.addPrimitivePalette().isSettled());
            return null;
        });
        shell("settings put global animator_duration_scale 1");
        settle();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.dismissTopmostSurface());
            assertFalse("with motion enabled the surface is closing rather than"
                            + " already gone: the exit that was already written is"
                            + " what a Back press now reaches",
                    workspace.addPrimitivePalette().isSettled());
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("and it finishes", workspace.addPrimitivePalette().isOpen());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    private void enterTransform(final int mode) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            NativeViewport.setGizmoMode(mode);
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
    }

    private void enterTransformInLandscape() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            return null;
        });
        settleLayout();
    }

    private static int[] handlesFor(int mode) {
        if (mode == NativeViewport.GIZMO_MODE_ROTATE) {
            return new int[]{NativeViewport.GIZMO_HANDLE_AXIS_X,
                    NativeViewport.GIZMO_HANDLE_AXIS_Y, NativeViewport.GIZMO_HANDLE_AXIS_Z};
        }
        if (mode == NativeViewport.GIZMO_MODE_SCALE) {
            return new int[]{NativeViewport.GIZMO_HANDLE_UNIFORM,
                    NativeViewport.GIZMO_HANDLE_PLANE_XY, NativeViewport.GIZMO_HANDLE_PLANE_XZ,
                    NativeViewport.GIZMO_HANDLE_PLANE_YZ, NativeViewport.GIZMO_HANDLE_AXIS_X,
                    NativeViewport.GIZMO_HANDLE_AXIS_Y, NativeViewport.GIZMO_HANDLE_AXIS_Z};
        }
        return new int[]{NativeViewport.GIZMO_HANDLE_PLANE_XY,
                NativeViewport.GIZMO_HANDLE_PLANE_XZ, NativeViewport.GIZMO_HANDLE_PLANE_YZ,
                NativeViewport.GIZMO_HANDLE_AXIS_X, NativeViewport.GIZMO_HANDLE_AXIS_Y,
                NativeViewport.GIZMO_HANDLE_AXIS_Z};
    }

    /** Every listed handle is grabbed by a touch on its own grab point. */
    private static void assertGrabs(String mode, int[] handles) {
        final float[] point = new float[2];
        for (int handle : handles) {
            assertTrue(mode + ": native must report where handle " + handle + " is",
                    NativeViewport.gizmoHandlePoint(handle, point));
            assertEquals(mode + ": a touch on handle " + handle + " grabs it", handle,
                    NativeViewport.gizmoHitTest(point[0], point[1]));
        }
    }

    /**
     * What a touch on the pivot grabs: nothing in Move and Rotate, where every
     * shaft converges and every ring passes around it, and the uniform handle in
     * Scale, where that disc IS a handle.
     */
    private static void assertPivotGrabs(String mode, int expected) {
        final float[] pivot = new float[2];
        assertTrue(mode + ": the pivot's pixel must be reportable",
                NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_UNIFORM, pivot));
        assertEquals(mode + ": what the pivot grabs", expected,
                NativeViewport.gizmoHitTest(pivot[0], pivot[1]));
    }

    private static void dispatchToViewport(View viewport, long downTime, long eventTime,
                                           int action, float x, float y) {
        final android.view.MotionEvent event =
                android.view.MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            viewport.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    private void assertApplyIsPinned(final String where, int orientation,
                                     boolean landscape) {
        setOrientation(rule.getScenario(), orientation);
        waitForLayout(rule.getScenario(), landscape);
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            openPrecision(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View apply = workspace.findViewById(R.id.apply_transform);
            assertNotNull(where + ": the commit must be on screen", apply);
            assertTrue(where + ": Apply is pinned below the body rather than"
                    + " being the last row of it", workspace.propertyInspector()
                    .hasPinnedCommit());
            assertTrue(where + ": and it is fully visible with no scrolling",
                    isFullyOnScreen(apply, workspace));
            assertEquals(where + ": which is true at the top of the body, not"
                            + " after a swipe", 0,
                    workspace.findViewById(R.id.inspector_scroll).getScrollY());
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    private void assertBackDismisses(final int openerId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.objectsDocked()) {
                return null;  // no panel to open in a window that has the column
            }
            workspace.findViewById(openerId).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!workspace.hasDismissibleSurface()) {
                return null;
            }
            assertTrue("Back dismisses it", workspace.dismissTopmostSurface());
            assertFalse("and there is nothing left to dismiss",
                    workspace.hasDismissibleSurface());
            return null;
        });
        settleLayout();
    }

    private void assertBackDismissesPrecision() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the precision surface is dismissible",
                    workspace.hasDismissibleSurface());
            assertTrue(workspace.dismissTopmostSurface());
            assertFalse("and closing it un-lights the toggle that opened it, which"
                            + " is why Back goes through the workspace and not the"
                            + " surface", workspace.precisionToggle().isActivated());
            return null;
        });
        settleLayout();
    }

    private void assertBackDismissesDisplay() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.globalToolbar().findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the display popover is dismissible",
                    workspace.hasDismissibleSurface());
            assertTrue(workspace.dismissTopmostSurface());
            assertFalse("and its control is un-lit with it",
                    workspace.displayPopover().isOpen());
            return null;
        });
        settleLayout();
    }

    /** Every interactive control in the trailing cluster, against the floor. */
    private void assertTouchFloor(final String where) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floorPx = Math.round(TOUCH_FLOOR_DP * density) - 1;
            final int[] controls = {R.id.precision_toggle, R.id.transform_mode_move,
                    R.id.transform_mode_rotate, R.id.transform_mode_scale,
                    R.id.transform_space_world, R.id.transform_space_local};
            for (int id : controls) {
                final View control = workspace.findViewById(id);
                assertNotNull(where + ": " + name(activity, id) + " must be in the"
                        + " tree, not removed to make room", control);
                if (control.getVisibility() != View.VISIBLE) {
                    continue;  // withdrawn by a domain rule, not squeezed away
                }
                assertTrue(where + ": " + name(activity, id) + " is "
                                + dp(activity, control.getHeight()) + " dp tall",
                        control.getHeight() >= floorPx);
                assertTrue(where + ": " + name(activity, id) + " is "
                                + dp(activity, control.getWidth()) + " dp wide",
                        control.getWidth() >= floorPx);
            }
            return null;
        });
    }

    private static void assertVisible(EditorWorkspaceView workspace, int id) {
        final View view = workspace.findViewById(id);
        assertNotNull("control " + id + " must exist", view);
        assertEquals("control " + id + " must be on screen", View.VISIBLE,
                view.getVisibility());
    }

    private static String name(ForgeShapeActivity activity, int id) {
        return activity.getResources().getResourceEntryName(id);
    }

    private static int dp(ForgeShapeActivity activity, int px) {
        return Math.round(px / activity.getResources().getDisplayMetrics().density);
    }

    private Rect boundsOf(final int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.findViewById(id);
            assertNotNull("control " + id + " must exist to be measured", view);
            final int[] viewLocation = new int[2];
            final int[] rootLocation = new int[2];
            view.getLocationInWindow(viewLocation);
            workspace.getLocationInWindow(rootLocation);
            final int left = viewLocation[0] - rootLocation[0];
            final int top = viewLocation[1] - rootLocation[1];
            return new Rect(left, top, left + view.getWidth(), top + view.getHeight());
        });
    }

    private Rect[] boundsOfAll(int[] ids) {
        final Rect[] rects = new Rect[ids.length];
        for (int i = 0; i < ids.length; i++) {
            rects[i] = boundsOf(ids[i]);
        }
        return rects;
    }

    private void assertBoundsUnchanged(String what, int[] ids, Rect[] before) {
        for (int i = 0; i < ids.length; i++) {
            assertEquals(what + " displaced control " + ids[i], before[i], boundsOf(ids[i]));
        }
    }

    /**
     * Which direct child of {@code parent} the given descendant lives under, so
     * a case can state the ORDER of the groups without naming a row by index.
     */
    private static int indexOfAncestorChild(View parent, View descendant) {
        View view = descendant;
        while (view != null && view.getParent() != parent) {
            view = view.getParent() instanceof View ? (View) view.getParent() : null;
        }
        if (view == null) {
            return -1;
        }
        final android.view.ViewGroup group = (android.view.ViewGroup) parent;
        for (int i = 0; i < group.getChildCount(); i++) {
            if (group.getChildAt(i) == view) {
                return i;
            }
        }
        return -1;
    }

    /** The nearest heading above a group, which is the label that names it. */
    private static String textOfSectionBefore(View parent, int childIndex) {
        final android.view.ViewGroup group = (android.view.ViewGroup) parent;
        for (int i = childIndex - 1; i >= 0; i--) {
            final View child = group.getChildAt(i);
            if (child instanceof android.widget.TextView) {
                return ((android.widget.TextView) child).getText().toString();
            }
        }
        return "";
    }

    private void switchTo(final AppTheme theme) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            activity.requestTheme(theme);
            return null;
        });
        settleLayout();
    }

    private static int imeBottomInset(View view) {
        final WindowInsets insets = view.getRootWindowInsets();
        if (insets == null) {
            return 0;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return insets.getInsets(WindowInsets.Type.ime()).bottom;
        }
        return insets.getSystemWindowInsetBottom();
    }

    private boolean waitForFontScale(float atLeast) {
        for (int attempt = 0; attempt < 40; attempt++) {
            final Float scale = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                final Configuration configuration =
                        activity.getResources().getConfiguration();
                return configuration.fontScale;
            });
            if (scale != null && (scale >= atLeast || atLeast <= 1.0f)) {
                settle();
                return true;
            }
            SystemClock.sleep(100);
        }
        return false;
    }

    /** Runs a shell command through the instrumentation, and waits for it. */
    private static void shell(String command) {
        final ParcelFileDescriptor pipe = InstrumentationRegistry.getInstrumentation()
                .getUiAutomation().executeShellCommand(command);
        try (FileInputStream stream = new ParcelFileDescriptor.AutoCloseInputStream(pipe)) {
            final byte[] scratch = new byte[64];
            while (stream.read(scratch) >= 0) {
                // Drained so the command completes before the case continues.
            }
        } catch (java.io.IOException ignored) {
            // The command still ran; nothing here reads its output.
        }
        SystemClock.sleep(200);
    }

    // WCAG relative luminance and contrast, implemented here rather than
    // depended on, so the number a failure prints is one this file computed.

    private static double contrast(int foreground, int background) {
        final double a = luminance(foreground);
        final double b = luminance(background);
        final double lighter = Math.max(a, b);
        final double darker = Math.min(a, b);
        return (lighter + 0.05) / (darker + 0.05);
    }

    private static double luminance(int color) {
        return 0.2126 * channel(Color.red(color)) + 0.7152 * channel(Color.green(color))
                + 0.0722 * channel(Color.blue(color));
    }

    private static double channel(int value) {
        final double c = value / 255.0;
        return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
    }
}
