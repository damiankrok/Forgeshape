package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.view.View;
import android.view.ViewGroup;

import androidx.lifecycle.Lifecycle;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * R1B1-01..08 — the New Project start flow.
 *
 * <p>Two claims are being tested and they are not the same. One is that the
 * question is asked exactly once per process and survives every in-process
 * event that rebuilds the view tree. The other is that the Sculpt answer lands
 * on a genuinely sculptable mesh <b>through the product's own Freeze</b>, and
 * that the exact Construction Source it was frozen from is still there
 * afterwards — which is the invariant the whole two-representation design rests
 * on and the one a shortcut here would quietly break.
 *
 * <p>The scene is process-scoped and there is no delete, so bodies accumulate
 * across a run. Nothing here assumes a body count; what it asserts is that the
 * count is <i>unchanged</i> by answering the chooser, which is the actual
 * claim — neither answer creates anything.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceStartFlowTest {

    /** A sphere's source topology, whatever its diameter: the tessellation is
     *  fixed, so this is what proves a frozen mesh really came from a sphere. */
    private static final int SPHERE_VERTEX_COUNT = 482;
    private static final int SPHERE_INDEX_COUNT = 2880;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @After
    public void leaveTheProductInConstruction() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.dismissStartChooserForConstruction();
            workspace.syncFromNative();
            return null;
        });
        releaseOrientation(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // R1B1-01 -- the question, and only the question
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_01_firstStartOffersExactlyConstructionAndSculpt() {
        askAsFirstLaunch();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View chooser = workspace.findViewById(R.id.start_chooser);
            assertNotNull("a fresh process asks how the model begins", chooser);
            assertEquals(View.VISIBLE, chooser.getVisibility());
            assertTrue(workspace.startChooserVisible());

            assertNotNull(workspace.findViewById(R.id.start_option_construction));
            assertNotNull(workspace.findViewById(R.id.start_option_sculpt));

            // "Exactly" has teeth: counted rather than spot-checked, so a third
            // option — a template, a recent file, a Sketch entry point that does
            // not exist — would fail here rather than pass unnoticed.
            assertEquals("the chooser offers two ways to begin and nothing else",
                    2, countClickable(chooser));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B1-02 / R1B1-03 -- the Construction answer changes nothing but the view
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_02_choosingConstructionEntersTheConstructionWorkspace() {
        askAsFirstLaunch();
        choose(R.id.start_option_construction);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(View.GONE,
                    workspace.findViewById(R.id.start_chooser).getVisibility());
            assertEquals("Construction is where the product already was",
                    NativeViewport.MODE_CONSTRUCTION, NativeViewport.productMode());
            assertNotNull("the Construction rail is on screen",
                    workspace.findViewById(R.id.tool_rail_shape));
            assertNotNull(workspace.findViewById(R.id.tool_rail_place));
            assertNotNull("with its exact-value commit",
                    workspace.findViewById(R.id.apply_shape));
            // The brush controls stay in the tree and are hidden, which is how
            // the shell has always expressed "not in this mode" for them.
            assertEquals("and no brush stands on the model in Construction",
                    View.GONE,
                    workspace.findViewById(R.id.brush_edge_controls).getVisibility());
            return null;
        });
    }

    @Test
    public void r1b1_03_choosingConstructionCreatesNothingAndKeepsTheExactEditor() {
        askAsFirstLaunch();

        final long[] before = onWorkspace(rule.getScenario(), (activity, workspace) ->
                new long[]{NativeViewport.sceneBodyCount(),
                        NativeViewport.sceneActiveBodyId()});

        choose(R.id.start_option_construction);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("answering the question adds no body",
                    before[0], NativeViewport.sceneBodyCount());
            assertEquals("and changes which body is being edited not at all",
                    before[1], NativeViewport.sceneActiveBodyId());
            assertTrue("the default Body is a legal start state",
                    NativeViewport.sceneBodyCount() >= 1);

            // The exact editor is on screen and pointing at the active body.
            assertNotNull(workspace.findViewById(R.id.primitive_chooser));
            assertNotNull("the active body has a row of its own",
                    workspace.objectsSection().rowFor(NativeViewport.sceneActiveBodyId()));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B1-04 -- the Sculpt answer lands on a real, sphere-derived frozen mesh
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_04_choosingSculptLandsOnAFrozenSphereWithNoVisibleFreeze() {
        askAsFirstLaunch();

        final long bodiesBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> (long) NativeViewport.sceneBodyCount());

        choose(R.id.start_option_sculpt);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the user asked to sculpt and is sculpting",
                    NativeViewport.MODE_SCULPT, NativeViewport.productMode());

            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            assertEquals("a Frozen Sculpt Mesh exists",
                    1.0, sculpt[NativeViewport.SCULPT_HAS_MESH], 0.0);
            // The topology is what proves this came from a SPHERE rather than
            // from whatever the body happened to be. A box would be 8:36.
            assertEquals("the frozen mesh is sphere-derived", SPHERE_VERTEX_COUNT,
                    (int) sculpt[NativeViewport.SCULPT_VERTEX_COUNT]);
            assertEquals(SPHERE_INDEX_COUNT,
                    (int) sculpt[NativeViewport.SCULPT_INDEX_COUNT]);
            assertEquals("nothing has been sculpted into it yet",
                    0.0, sculpt[NativeViewport.SCULPT_HAS_EDITS], 0.0);

            assertEquals("choosing Sculpt creates no body",
                    bodiesBefore, (long) NativeViewport.sceneBodyCount());

            // The Sculpt workspace, in full, with no Construction editor on it.
            assertEquals(View.VISIBLE,
                    workspace.findViewById(R.id.brush_edge_controls).getVisibility());
            assertNotNull(workspace.findViewById(R.id.tool_rail_grab));
            assertNotNull(workspace.findViewById(R.id.freeze_again));
            assertNull("nothing on screen may edit the Construction Source here",
                    workspace.findViewById(R.id.apply_shape));
            // The user never pressed Freeze and is not being offered it either:
            // the toolbar's Sculpt context is Back, and Freeze is not on screen
            // to be pressed.
            assertEquals(View.GONE,
                    workspace.findViewById(R.id.freeze_to_sculpt).getVisibility());
            assertEquals(View.VISIBLE,
                    workspace.findViewById(R.id.back_to_construction).getVisibility());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B1-05 / R1B1-06 -- and the exact source is still there behind it
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_05_backFromADirectSculptStartShowsTheSphereSource() {
        askAsFirstLaunch();
        choose(R.id.start_option_sculpt);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.MODE_CONSTRUCTION, NativeViewport.productMode());
            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            assertEquals("the Construction Source was never consumed by the Freeze",
                    NativeViewport.PRIMITIVE_SPHERE, (int) primitive[0]);
            assertTrue("and it still has a usable diameter",
                    primitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER] > 0.0);
            assertTrue("the exact editor shows it as a sphere",
                    workspace.findViewById(R.id.primitive_option_sphere).isActivated());
            assertNotNull(workspace.findViewById(R.id.field_sphere_diameter));
            return null;
        });
    }

    @Test
    public void r1b1_06_resumeAfterADirectSculptStartReturnsTheSameFrozenMesh() {
        askAsFirstLaunch();
        choose(R.id.start_option_sculpt);

        final double[] frozen = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(state);
            return state;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("with a frozen mesh in hand the toolbar offers Resume",
                    View.VISIBLE,
                    workspace.findViewById(R.id.resume_sculpt).getVisibility());
            assertEquals("and not Freeze, which would replace it", View.GONE,
                    workspace.findViewById(R.id.freeze_to_sculpt).getVisibility());
            workspace.findViewById(R.id.resume_sculpt).performClick();
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            final double[] resumed = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(resumed);
            assertEquals("Resume returns the SAME mesh, not a fresh freeze",
                    frozen[NativeViewport.SCULPT_REVISION],
                    resumed[NativeViewport.SCULPT_REVISION], 0.0);
            assertEquals(frozen[NativeViewport.SCULPT_VERTEX_COUNT],
                    resumed[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
            assertEquals("and it still belongs to the same body",
                    frozen[NativeViewport.SCULPT_OBJECT_ID],
                    resumed[NativeViewport.SCULPT_OBJECT_ID], 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1B1-07 / R1B1-08 -- asked once per process, not once per view tree
    // -----------------------------------------------------------------------

    @Test
    public void r1b1_07_theChooserDoesNotComeBackOnRotation() {
        askAsFirstLaunch();
        choose(R.id.start_option_construction);

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        assertChooserStaysAnswered("a rotation must not re-ask a question already answered");

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        assertChooserStaysAnswered("and neither must rotating back");
    }

    @Test
    public void r1b1_08_theChooserDoesNotComeBackAfterHomeAndResume() {
        askAsFirstLaunch();
        choose(R.id.start_option_sculpt);

        rule.getScenario().moveToState(Lifecycle.State.CREATED);
        settleLayout();
        rule.getScenario().moveToState(Lifecycle.State.RESUMED);
        settleLayout();

        assertChooserStaysAnswered("a resume must not re-ask how the model began");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("and the mode it started in survives with it",
                    NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    /** Puts the question back, as a genuinely fresh process would. */
    private void askAsFirstLaunch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.showStartChooserAsFirstLaunch();
            return null;
        });
        settleLayout();
    }

    private void choose(int optionId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(optionId).performClick();
            return null;
        });
        settleLayout();
    }

    private void assertChooserStaysAnswered(String why) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(why, View.GONE,
                    workspace.findViewById(R.id.start_chooser).getVisibility());
            return null;
        });
    }

    /** How many descendants of {@code root} a user could actually press. */
    private static int countClickable(View root) {
        int count = root.isClickable() ? 1 : 0;
        if (root instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); i++) {
                count += countClickable(group.getChildAt(i));
            }
        }
        return count;
    }
}
