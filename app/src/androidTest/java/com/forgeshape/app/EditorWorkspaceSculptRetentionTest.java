package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * UIR4A-12 and UIR4B-20: the Start Sculpting / Back / Resume round trip loses no
 * sculpt work.
 *
 * <p><b>UI-R4B renamed the door and not the room.</b> The control that used to
 * read "Freeze to Sculpt" reads "Start Sculpting", and the guarded re-freeze
 * reads "Reset Sculpt from Shape…"; the ids, the native calls and every one of
 * the assertions below are unchanged. A copy change that quietly altered which
 * mesh comes back would be exactly the kind of defect a wording pass is trusted
 * not to introduce, and this suite is what makes that trust checkable — which is
 * why the cases here are cited as UIR4B-20 as well.
 *
 * <p>This suite is a <b>regression guard on behaviour this stage deliberately
 * did not redesign</b>, and that is the whole reason it exists. The accepted
 * product direction is a one-way Construction-project-to-Sculpt-project copy,
 * but durable persistence does not exist yet, so the current path — freeze,
 * sculpt, look at the Construction Source, come back — is still the only way a
 * user's sculpt work survives a trip through the other representation. A
 * workspace redesign that quietly broke it would be a data-loss defect wearing
 * a UI change's clothes.
 *
 * <p>What is asserted is the <b>current</b> semantics, stated plainly so the
 * next stage can change it deliberately rather than by accident:
 *
 * <ul>
 *   <li>Sculpting mints a revision and moves vertices.</li>
 *   <li><i>Back to Construction</i> shows the Construction Source
 *       <b>unchanged</b> — sculpting never writes the Construction Source, so
 *       this is correct and not a loss.</li>
 *   <li><i>Resume Sculpt</i> returns the <b>same</b> frozen mesh: the same
 *       revision, the same vertex count, the same stroke history. That is the
 *       claim that would make a loss visible.</li>
 * </ul>
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceSculptRetentionTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    /**
     * UIR4A-12. Freeze, make a real stroke, leave, come back — the edits are
     * still there.
     */
    @Test
    public void uir4a12_freezeStrokeBackResumeKeepsTheSculptEdits() {
        freezeFreshSculptableMesh();

        final double[] beforeStroke = onWorkspace(rule.getScenario(),
                (activity, workspace) -> sculptState());
        sculptTheCurrentMesh();

        final double[] afterStroke = onWorkspace(rule.getScenario(),
                (activity, workspace) -> sculptState());
        assertNotEquals("precondition: the stroke must have edited this mesh — with no "
                        + "edit there is nothing for the round trip to lose",
                beforeStroke[NativeViewport.SCULPT_REVISION],
                afterStroke[NativeViewport.SCULPT_REVISION]);
        assertTrue("and native code must agree the CURRENT mesh has edits",
                afterStroke[NativeViewport.SCULPT_HAS_EDITS] != 0.0);

        // The Construction Source, as it stands with a sculpted mesh alongside.
        final double[] sourceBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> constructionState());

        // Back to Construction.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Back to Construction really does leave Sculpt",
                    NativeViewport.MODE_CONSTRUCTION, NativeViewport.productMode());
            // Current legacy semantics, and correct: a sculpt edit may never
            // write the Construction Source, so the exact primitive and its
            // placement are exactly what they were.
            final double[] sourceAfter = constructionState();
            assertArrayEquals("the Construction Source is untouched by sculpting:"
                            + describeSnapshotDifference(sourceBefore, sourceAfter),
                    sourceBefore, sourceAfter, 0.0);
            // And the frozen mesh has not been discarded by leaving it.
            final double[] sculpt = sculptState();
            assertTrue("the Frozen Sculpt Mesh still exists after leaving Sculpt",
                    sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0);
            return null;
        });

        // Resume Sculpt.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.resume_sculpt).performClick();
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Resume Sculpt returns to Sculpt Mode",
                    NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            final double[] resumed = sculptState();

            // The claim that would make a data loss visible: the mesh that
            // comes back is the same mesh, not a fresh copy of the source.
            assertEquals("the SAME sculpt revision returns — a re-freeze would "
                            + "have minted a new one",
                    afterStroke[NativeViewport.SCULPT_REVISION],
                    resumed[NativeViewport.SCULPT_REVISION], 0.0);
            assertEquals("with the same vertex count",
                    afterStroke[NativeViewport.SCULPT_VERTEX_COUNT],
                    resumed[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
            assertEquals("and the same index count",
                    afterStroke[NativeViewport.SCULPT_INDEX_COUNT],
                    resumed[NativeViewport.SCULPT_INDEX_COUNT], 0.0);
            assertTrue("and native code still reports the edits on this mesh",
                    resumed[NativeViewport.SCULPT_HAS_EDITS] != 0.0);
            assertEquals("the stroke history is the same session's",
                    afterStroke[NativeViewport.SCULPT_STROKE_COUNT],
                    resumed[NativeViewport.SCULPT_STROKE_COUNT], 0.0);
            assertEquals("and it is the same body's mesh",
                    afterStroke[NativeViewport.SCULPT_OBJECT_ID],
                    resumed[NativeViewport.SCULPT_OBJECT_ID], 0.0);
            return null;
        });
    }

    /**
     * UIR4A-12b. The stale-source warning is still reachable without a resident
     * panel.
     *
     * <p>It used to live in the Sculpt inspector, which opened by itself on a
     * roomy window. Nothing opens by itself any more, so a standing fault has to
     * be visible from the resting workspace or it is effectively hidden — which
     * is why it is written to the status line as well as into the surface that
     * resolves it.
     */
    @Test
    public void uir4a12b_aStaleConstructionSourceIsVisibleFromTheRestingWorkspace() {
        freezeFreshSculptableMesh();
        sculptTheCurrentMesh();

        // Change the Construction Source under the frozen mesh, which is what
        // makes it stale, then come back to Sculpt and look at the workspace.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(2.0);
            workspace.findViewById(R.id.resume_sculpt).performClick();
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = sculptState();
            assertTrue("precondition: the source must now be stale",
                    sculpt[NativeViewport.SCULPT_SOURCE_STALE] != 0.0);
            // The wording changed at UI-R4B — no user-facing string says
            // "Freeze" any more — and what the warning has to DO is unchanged:
            // name the standing fault and name the act that resolves it.
            assertTrue("a standing fault must be readable without opening a panel",
                    String.valueOf(workspace.globalToolbar().statusText())
                            .contains("Reset Sculpt from Shape"));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    /**
     * Puts the product in Sculpt on a mesh that is both freshly frozen and
     * actually sculptable.
     *
     * <p>The primitive matters: a box's eight vertices are all at its corners
     * and a brush that captures none of them starts no stroke at all, so a
     * fixture that froze whatever the previous case left behind could not
     * reliably produce an edit. A sphere's vertices are spread over its surface.
     */
    private void freezeFreshSculptableMesh() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            NativeViewport.setSculptBrush(300.0, 1.0);
            if (NativeViewport.productMode() != NativeViewport.MODE_SCULPT) {
                final double[] sculpt = sculptState();
                workspace.findViewById(sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0
                        ? R.id.resume_sculpt : R.id.freeze_to_sculpt).performClick();
            }
            return null;
        });
        // A second, unconditional Freeze so the mesh under test is always newly
        // built from the sphere above, whatever state the run arrived in.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            assertEquals("the fixture must freeze cleanly",
                    NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            NativeViewport.setSculptTool(NativeViewport.TOOL_GRAB);
            return null;
        });
        settleLayout();
    }

    /** Drives a real Grab stroke through the native touch path. */
    private void sculptTheCurrentMesh() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final int w = viewport.getWidth();
            final int h = viewport.getHeight();
            assertTrue("precondition: the viewport must be laid out", w > 0 && h > 0);
            final float cx = w / 2f;
            final float cy = h / 2f;
            sendTouch(MotionEvent.ACTION_DOWN, cx, cy, w, h);
            for (int step = 1; step <= 8; ++step) {
                sendTouch(MotionEvent.ACTION_MOVE, cx + step * 6f, cy + step * 4f, w, h);
            }
            sendTouch(MotionEvent.ACTION_UP, cx + 48f, cy + 32f, w, h);
            return null;
        });
        settleLayout();
    }

    private static void sendTouch(int action, float x, float y, int width, int height) {
        // Null stylus arrays on purpose: this is a plain finger, and native code
        // fills in the documented defaults — full pressure, no tilt — exactly as
        // it does for hardware that reports nothing.
        NativeViewport.touchEvent(action, -1, 1, new int[]{0}, new float[]{x},
                new float[]{y}, null, null, null, null, width, height);
    }

    private static double[] sculptState() {
        final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.sculptState(state);
        return state;
    }

    /** The Construction Source: the exact primitive and its placement. */
    private static double[] constructionState() {
        final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        final double[] transform = new double[6];
        NativeViewport.constructionPrimitive(primitive);
        NativeViewport.boxTransform(transform);
        final double[] all = new double[primitive.length + transform.length];
        System.arraycopy(primitive, 0, all, 0, primitive.length);
        System.arraycopy(transform, 0, all, primitive.length, transform.length);
        return all;
    }
}
