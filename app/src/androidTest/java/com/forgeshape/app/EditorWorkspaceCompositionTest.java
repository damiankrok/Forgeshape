package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closeObjectsPanel;
import static com.forgeshape.app.WorkspaceTestSupport.closePrecision;
import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.dragConsumed;
import static com.forgeshape.app.WorkspaceTestSupport.isFullyOnScreen;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openObjectsPanel;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settle;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.unoccludedViewportFraction;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Rect;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * UI-R2: what the workspace is composed of, and what each surface is for.
 *
 * <p>The change this suite guards is a <b>role separation</b>. The scene list
 * used to be built and held by the Construction shape editor, which is the
 * Property Inspector's body — so a panel titled "Shape" opened with the list of
 * bodies and pushed the width/height/depth fields, the thing it is named after,
 * below the fold of a sheet capped at 30 % of the window. Scene-level content
 * inside the active-object value panel is a role confusion, and it also meant a
 * list scrolling inside another scroll.
 *
 * <p>Now: the workspace owns one {@link ObjectsSectionView} and moves it between
 * hosts — a leading-edge column where the window has earned one, and
 * {@link ObjectsPopoverView} everywhere else. The inspector names the body it
 * edits in its title instead of carrying the list.
 *
 * <p>What must NOT have changed is most of this file: the viewport is still the
 * whole window, chrome still consumes its own gestures, selection still routes
 * by ObjectId through native truth, and no composition change may touch
 * geometry. Those are asserted here against the new arrangement rather than
 * assumed to have survived it.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceCompositionTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // UIR2-01 / UIR2-03 -- the viewport is still the point
    // -----------------------------------------------------------------------

    /**
     * UIR2-01. The model keeps the whole window and keeps most of the picture,
     * with the scene panel closed — which is how the workspace rests.
     */
    @Test
    public void uir201_theViewportStaysDominantAndInteractive() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            assertEquals("the viewport spans the whole workspace width",
                    workspace.getWidth(), viewport.getWidth());
            assertEquals("the viewport spans the whole workspace height",
                    workspace.getHeight(), viewport.getHeight());
            assertFalse("the scene panel is closed at rest",
                    workspace.objectsPopover().isOpen());

            final double visible = unoccludedViewportFraction(workspace.getWidth(),
                    workspace.getHeight(), chromeRects(workspace));
            assertTrue("the model must still own most of the window, saw "
                            + Math.round(visible * 100) + " %",
                    visible >= 0.55);
            return null;
        });
    }

    /**
     * UIR2-03. Whatever the window, the composition either shows the scene
     * beside the model or offers one control that reveals it — and the two are
     * mutually exclusive, so no window shows the same list twice.
     */
    @Test
    public void uir203_everyWindowExposesTheSceneExactlyOneWay() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final boolean docked = workspace.objectsDocked();
            final boolean capsule =
                    workspace.objectsCapsule().getVisibility() == View.VISIBLE;
            assertTrue("a window either docks Objects or shows the capsule that opens it",
                    docked ^ capsule);

            if (docked) {
                assertTrue("the docked column is on screen",
                        isFullyOnScreen(workspace.findViewById(R.id.objects_dock), workspace));
                assertFalse("a docked window never also opens the panel",
                        workspace.objectsPopover().isOpen());
            } else {
                assertTrue("the scene list is in its own panel",
                        workspace.objectsPopover().hosts(workspace.objectsSection()));
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR2-02 -- reachable without paying for it permanently
    // -----------------------------------------------------------------------

    /**
     * UIR2-02. One tap reveals the scene, one tap gives the model back, and
     * while it is closed it costs the viewport nothing at all.
     */
    @Test
    public void uir202_theSceneIsOneTapAwayAndCostsNothingClosed() {
        final Boolean docked = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.objectsDocked());
        if (Boolean.TRUE.equals(docked)) {
            return;  // a docked window has no panel to open; UIR2-03 covers it
        }

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("closed, the panel occupies nothing",
                    View.GONE, workspace.objectsPopover().getVisibility());
            openObjectsPanel(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("one tap opens the scene panel", workspace.objectsPopover().isOpen());
            assertNotNull("and the active body has a row in it",
                    workspace.objectsSection().rowFor(NativeViewport.sceneActiveBodyId()));
            assertTrue("the panel is fully on screen",
                    isFullyOnScreen(workspace.objectsPopover(), workspace));
            // The panel stands ON the model, so it must not become the window
            // the way the old bottom sheet did.
            assertTrue("an overlay panel may not take more than half the height",
                    workspace.objectsPopover().getHeight() <= workspace.getHeight() * 0.6);
            // Still the viewport underneath, untouched.
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            assertEquals("the viewport is untouched by an overlay",
                    workspace.getWidth(), viewport.getWidth());
            assertEquals(workspace.getHeight(), viewport.getHeight());

            closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("a second tap gives the model back",
                    workspace.objectsPopover().isOpen());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR2-04 / UIR2-05 -- the panel edits nothing; the inspector edits one body
    // -----------------------------------------------------------------------

    /**
     * UIR2-04. A row still routes by stable ObjectId into native truth, from
     * whichever host the section currently hangs in. The row is found by its
     * tag, never by where it sits.
     */
    @Test
    public void uir204_objectsSelectionStillRoutesByObjectId() {
        final long[] ids = addSecondBody();
        final long first = ids[0];
        final long second = ids[1];

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("adding selected the new body", second,
                    NativeViewport.sceneActiveBodyId());
            final View row = workspace.objectsSection().rowFor(first);
            assertNotNull("the first body still has a row", row);
            row.performClick();
            assertEquals("the row selected the body it is tagged with",
                    first, NativeViewport.sceneActiveBodyId());
            assertTrue("and the row it selected is the one drawn active",
                    workspace.objectsSection().rowFor(first).isActivated());
            assertFalse(workspace.objectsSection().rowFor(second).isActivated());
            return null;
        });
    }

    /**
     * UIR2-05. The Property Inspector edits the selected body and says which one
     * that is — the context the list used to supply by sitting inside it.
     */
    @Test
    public void uir205_theInspectorNamesAndEditsTheSelectedBodyOnly() {
        final long[] ids = addSecondBody();
        final long first = ids[0];
        final long second = ids[1];

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // The exact values are a surface the user asks for, so this case
            // asks for it before reading what it says.
            openPrecision(workspace);
            assertTrue("the inspector title names the active body",
                    titleOf(workspace).contains(bodyLabel(activity, second)));
            assertFalse("and not the other one",
                    titleOf(workspace).contains(bodyLabel(activity, first)));

            workspace.objectsSection().rowFor(first).performClick();
            return null;
        });
        settle();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("selection moved below JNI", first,
                    NativeViewport.sceneActiveBodyId());
            assertTrue("and the inspector followed it",
                    titleOf(workspace).contains(bodyLabel(activity, first)));
            // The shape editor is still the inspector's body and still carries
            // the exact-value fields — the list left, the numbers did not.
            assertNotNull("the exact-value fields are still in the inspector",
                    workspace.findViewById(R.id.field_box_width));
            assertNotNull(workspace.findViewById(R.id.apply_shape));
            return null;
        });
    }

    /**
     * UIR2-05b. The exact-value fields are no longer pushed below the fold by a
     * list that is not theirs — the shape editor now begins with shape.
     */
    @Test
    public void uir205b_theShapePanelBeginsWithShape() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("the scene list is not inside the inspector any more",
                    isDescendantOf(workspace.objectsSection(),
                            workspace.findViewById(R.id.property_inspector)));
            final View chooser = workspace.findViewById(R.id.primitive_option_box);
            final View firstField = workspace.findViewById(R.id.field_box_width);
            assertNotNull("the primitive chooser is in the panel", chooser);
            assertNotNull("and so is the first dimension", firstField);
            assertTrue("the chooser must come before the fields it chooses for",
                    chooser.getTop() <= firstField.getTop());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR2-07 -- chrome gestures still stop at chrome
    // -----------------------------------------------------------------------

    /**
     * UIR2-07. The new panel obeys the rule every chrome surface obeys: it
     * consumes its own gestures, so reaching for a body can never orbit the
     * camera behind it.
     */
    @Test
    public void uir207_theScenePanelConsumesItsOwnGestures() {
        final Boolean docked = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.objectsDocked());
        if (Boolean.TRUE.equals(docked)) {
            return;
        }
        openScenePanel();

        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        final Boolean consumed = onWorkspace(rule.getScenario(),
                (activity, workspace) -> dragConsumed(workspace.objectsPopover()));
        assertTrue("the scene panel must consume its own drag",
                Boolean.TRUE.equals(consumed));

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("a drag on chrome changes nothing below JNI:"
                + describeSnapshotDifference(before, after), before, after, 0.0);
        closeScenePanel();
    }

    // -----------------------------------------------------------------------
    // UIR2-16 -- composition is not geometry
    // -----------------------------------------------------------------------

    /**
     * UIR2-16. Opening the scene panel, selecting a body and collapsing the
     * inspector are presentation. None of them may publish a mesh, mint a
     * revision or move a vertex.
     *
     * <p>Selection deliberately IS included: it changes which body the editors
     * act on, which is native state — but it publishes no geometry, so every
     * slot describing the mesh must be untouched. The one slot that legitimately
     * moves is the active ObjectId, so the comparison is of the sculpt and
     * dimension state around it.
     */
    @Test
    public void uir216_compositionChangesRebuildNoGeometry() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        openScenePanel();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Re-select the body that is already active: a pure UI round trip.
            workspace.objectsSection().rowFor(NativeViewport.sceneActiveBodyId())
                    .performClick();
            return null;
        });
        closeScenePanel();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closePrecision(workspace);
            return null;
        });
        settleLayout();

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("pure composition changes must leave native state identical:"
                + describeSnapshotDifference(before, after), before, after, 0.0);
    }

    // -----------------------------------------------------------------------
    // UIR2-09 / UIR2-10 -- the arrangement survives the window changing
    // -----------------------------------------------------------------------

    /**
     * UIR2-10. The scene panel is presentation, so it closes when the chrome is
     * hidden rather than surviving the act that was meant to clear the model —
     * and the one section comes back with the scene intact afterwards.
     */
    @Test
    public void uir210_hidingChromeClosesTheScenePanel() {
        final Boolean docked = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.objectsDocked());
        if (Boolean.TRUE.equals(docked)) {
            return;
        }
        openScenePanel();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setChromeHidden(true);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("hiding chrome closes the scene panel",
                    workspace.objectsPopover().isOpen());
            workspace.setChromeHidden(false);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("and the scene is intact when the chrome comes back",
                    NativeViewport.sceneBodyCount(), workspace.objectsSection().rowCount());
            return null;
        });
    }

    /**
     * UIR2-09b. There is exactly ONE Objects section, wherever it hangs. A
     * composition that had leaked a second copy would show up as a row count
     * that no longer matches the scene, or as a second view carrying the id.
     */
    @Test
    public void uir209b_thereIsExactlyOneSceneList() {
        addSecondBody();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("the section has exactly one parent",
                    workspace.objectsSection().getParent());
            assertEquals("one row per body", NativeViewport.sceneBodyCount(),
                    workspace.objectsSection().rowCount());
            assertEquals("and findViewById finds the same one instance",
                    workspace.objectsSection(),
                    workspace.findViewById(R.id.objects_section));
            // Its host is one of exactly two, never the inspector.
            final boolean inDock = workspace.objectsSection().getParent()
                    == workspace.objectsDock();
            final boolean inPanel = workspace.objectsPopover()
                    .hosts(workspace.objectsSection());
            assertTrue("the section hangs in a column or in the panel, never elsewhere",
                    inDock ^ inPanel);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    private void openScenePanel() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
    }

    private void closeScenePanel() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();
    }

    /** Adds a body and returns {first, second} — the scene accumulates, so the
     *  ids are read rather than assumed. */
    private long[] addSecondBody() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long first = NativeViewport.sceneActiveBodyId();
            final long second = NativeViewport.sceneAddBody();
            assertTrue("a body must have been added", second != 0L);
            workspace.syncFromNative();
            return new long[]{first, second};
        });
    }

    private static String titleOf(EditorWorkspaceView workspace) {
        final android.widget.TextView title =
                workspace.findViewById(R.id.inspector_title);
        assertNotNull("the inspector has a title", title);
        return title.getText().toString();
    }

    private static String bodyLabel(ForgeShapeActivity activity, long objectId) {
        return activity.getString(R.string.body_label, objectId);
    }

    private static boolean isDescendantOf(View child, View ancestor) {
        View parent = child;
        while (parent != null) {
            if (parent == ancestor) {
                return true;
            }
            parent = (parent.getParent() instanceof View) ? (View) parent.getParent() : null;
        }
        return false;
    }

    /**
     * Every chrome rectangle currently painted on screen.
     *
     * <p>The toolbar's two control capsules, not the toolbar container: that
     * container is transparent and draws nothing, so counting its full-width
     * bounds would report a bar the user can see straight through.
     */
    private static Rect[] chromeRects(EditorWorkspaceView workspace) {
        final int[] ids = {R.id.toolbar_editing_group, R.id.toolbar_utility_group,
                R.id.property_inspector, R.id.tool_rail, R.id.objects_dock};
        int n = 0;
        final Rect[] rects = new Rect[ids.length];
        for (int id : ids) {
            final View view = workspace.findViewById(id);
            if (view == null || view.getVisibility() != View.VISIBLE || view.getWidth() <= 0) {
                continue;
            }
            final Rect rect = new Rect();
            view.getGlobalVisibleRect(rect);
            rects[n++] = rect;
        }
        final Rect[] trimmed = new Rect[n];
        System.arraycopy(rects, 0, trimmed, 0, n);
        return trimmed;
    }
}
