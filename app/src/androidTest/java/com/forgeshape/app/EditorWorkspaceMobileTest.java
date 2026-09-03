package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closeAddPrimitive;
import static com.forgeshape.app.WorkspaceTestSupport.closePrecision;
import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.dragConsumed;
import static com.forgeshape.app.WorkspaceTestSupport.isFullyOnScreen;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openAddPrimitive;
import static com.forgeshape.app.WorkspaceTestSupport.openObjectsPanel;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.unoccludedViewportFraction;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * UI-R4A: the mobile workspace.
 *
 * <p>What this suite is <b>for</b> is the one structural claim the redesign
 * makes — the viewport is the workspace and every other surface is asked for.
 * So the cases here are mostly about what is <i>absent</i> at rest, which is
 * exactly the kind of contract that rots silently: nothing fails when a panel
 * quietly comes back, unless something is measuring the bottom edge.
 *
 * <p>Two rules are inherited from the rest of the instrumentation and are not
 * relaxed here. No control is located by screen coordinate — the workspace
 * re-arranges itself per window, so a coordinate is only ever true for one run.
 * And no assertion reads a rendered pixel: what the Vulkan viewport draws is the
 * renderer's business.
 *
 * <p>Cases that assert something about a specific window read the window they
 * are actually in and assert the contract belonging to it, so running the suite
 * under an overridden window size is a genuine expanded-layout run rather than a
 * simulation.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceMobileTest {

    /** The six primitives the product actually builds, in the palette's order. */
    private static final int[] PALETTE_IDS = {
            R.id.add_primitive_box, R.id.add_primitive_cylinder,
            R.id.add_primitive_sphere, R.id.add_primitive_cone,
            R.id.add_primitive_capsule, R.id.add_primitive_plane
    };

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // UIR4A-01 / UIR4A-11 -- nothing owns the bottom edge
    // -----------------------------------------------------------------------

    /**
     * UIR4A-01. A resting Construction workspace has no full-width surface
     * along its bottom edge.
     *
     * <p>Measured rather than asserted from a flag, and measured as a
     * <b>width</b> rather than as an absence: the defect this replaces was not
     * "a panel is open", it was "a collapsed panel is still a strip across the
     * window". So every chrome rectangle that reaches the bottom of the window
     * is required to be narrow enough that the model is reachable beside it,
     * which the Objects capsule satisfies and a sheet or a bar does not.
     */
    @Test
    public void uir4a01_restingConstructionHasNoFullWidthBottomSurface() {
        assertNoBottomBar("resting Construction");
    }

    /**
     * UIR4A-11. The same rule in Sculpt, plus the two controls that must not
     * have been traded away for it: Radius and Strength are direct and stay
     * direct.
     */
    @Test
    public void uir4a11_restingSculptHasNoBottomAnchorAndKeepsItsBrushControls() {
        enterSculpt();
        assertNoBottomBar("resting Sculpt");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Radius and Strength stay direct in Sculpt", View.VISIBLE,
                    workspace.findViewById(R.id.brush_edge_controls).getVisibility());
            assertTrue("the Radius slider is laid out",
                    workspace.findViewById(R.id.brush_radius_slider).getWidth() > 0);
            assertTrue("and so is Strength",
                    workspace.findViewById(R.id.brush_strength_slider).getWidth() > 0);
            assertEquals("and no precision surface stands on the model at rest",
                    View.GONE, workspace.propertyInspector().getVisibility());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4A-02 -- the Objects capsule
    // -----------------------------------------------------------------------

    /**
     * UIR4A-02. At rest the capsule says which body is active and offers the
     * two acts: expand, and create.
     *
     * <p>The body name is compared against native truth rather than against a
     * remembered string, because the point of the capsule is that it cannot
     * disagree with the scene.
     */
    @Test
    public void uir4a02_theObjectsCapsuleNamesTheActiveBodyAndOffersThePlus() {
        final Boolean docked = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.objectsDocked());
        if (Boolean.TRUE.equals(docked)) {
            // A window with a column has no capsule; UIR4A-17 covers that case.
            return;
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View capsule = workspace.objectsCapsule();
            assertEquals("the capsule is on screen at rest",
                    View.VISIBLE, capsule.getVisibility());
            assertTrue("and fully inside the window", isFullyOnScreen(capsule, workspace));

            final android.widget.TextView active =
                    workspace.findViewById(R.id.objects_capsule_active);
            assertEquals("the capsule names the body native code reports active",
                    activity.getString(R.string.body_label,
                            NativeViewport.sceneActiveBodyId()),
                    active.getText().toString());
            assertTrue("the name is the expand affordance", active.isClickable());

            final View add = workspace.findViewById(R.id.objects_capsule_add);
            assertEquals("and the plus is beside it", View.VISIBLE, add.getVisibility());
            assertTrue(add.isClickable());
            return null;
        });
    }

    /** UIR4A-02b. Expanding the capsule shows the one scene list, unduplicated. */
    @Test
    public void uir4a02b_expandingTheCapsuleShowsTheOneSceneList() {
        final Boolean docked = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.objectsDocked());
        if (Boolean.TRUE.equals(docked)) {
            return;
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openObjectsPanel(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the list opened", workspace.objectsPopover().isOpen());
            assertTrue("and it is the ONE Objects section, not a copy",
                    workspace.objectsPopover().hosts(workspace.objectsSection()));
            assertEquals("with one row per body, read back from native scene state",
                    NativeViewport.sceneBodyCount(), workspace.objectsSection().rowCount());
            assertNotNull("including the active one",
                    workspace.objectsSection().rowFor(NativeViewport.sceneActiveBodyId()));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4A-03 / UIR4A-04 / UIR4A-07 -- Add Primitive
    // -----------------------------------------------------------------------

    /**
     * UIR4A-03. The palette is anchored to the control that opened it.
     *
     * <p>"Anchored" is checked as a spatial relation rather than as a flag: the
     * palette's own rectangle has to overlap the invoking control's horizontal
     * span and sit within a short distance of it vertically. That is what makes
     * the surface read as coming out of the plus rather than arriving from a
     * window edge, and it is the half of the contract a layout change is most
     * likely to break silently.
     */
    @Test
    public void uir4a03_addPrimitiveIsAnchoredToTheControlThatOpenedIt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final AddPrimitivePaletteView palette = workspace.addPrimitivePalette();
            assertTrue("the plus opened the palette", palette.isOpen());
            assertTrue("and it is fully on screen", isFullyOnScreen(palette, workspace));

            final View invoker = workspace.objectsDocked()
                    ? workspace.objectsSection().findViewById(R.id.add_body)
                    : workspace.findViewById(R.id.objects_capsule_add);
            final Rect from = boundsIn(invoker, workspace);
            final Rect surface = boundsIn(palette, workspace);

            assertTrue("the palette shares the invoking control's edge: invoker "
                            + from + " vs palette " + surface,
                    surface.left <= from.right && surface.right >= from.left);

            // Within one gap of the control, on whichever side it grew.
            final int gap = EditorControlStyles.dimen(activity, R.dimen.overlay_anchor_gap);
            final int distance = surface.bottom <= from.top
                    ? from.top - surface.bottom : surface.top - from.bottom;
            assertTrue("the palette hangs off the control that opened it, saw "
                            + distance + " px", distance >= 0 && distance <= gap * 3);
            return null;
        });
    }

    /**
     * UIR4A-04. Exactly the creation actions the product performs — the six
     * primitives and, since CAD-R0-A1A2, New Sketch — and no fake future one.
     *
     * <p>The count is asserted as well as the ids, because an <i>Add from
     * file</i> tile drawn disabled would satisfy a per-id check and is precisely
     * what this stage is not allowed to ship. The plane chooser New Sketch
     * opens is inside the same palette and is NOT on screen until asked for,
     * so it contributes nothing to the resting count.
     */
    @Test
    public void uir4a04_addPrimitiveOffersTheSixRealPrimitivesAndNothingElse() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            int tiles = 0;
            for (int id : PALETTE_IDS) {
                final View tile = workspace.findViewById(id);
                assertNotNull(activity.getResources().getResourceEntryName(id)
                        + " must be offered", tile);
                assertTrue("every creation action offered must work", tile.isEnabled());
                assertTrue("and be tappable", tile.isClickable());
                tiles++;
            }
            assertEquals("all six primitives are offered", PALETTE_IDS.length, tiles);
            final View newSketch = workspace.findViewById(R.id.add_sketch);
            assertNotNull("and New Sketch is offered", newSketch);
            assertTrue(newSketch.isEnabled() && newSketch.isClickable());
            assertFalse("the plane chooser is not on screen until New Sketch is tapped",
                    workspace.addPrimitivePalette().showingPlanes());
            assertEquals("and the palette holds exactly those seven and no placeholder",
                    PALETTE_IDS.length + 1, countTiles(workspace.addPrimitivePalette()));
            return null;
        });
    }

    /**
     * UIR4A-05. Choosing Sphere creates a body whose <b>native</b> primitive is
     * a sphere.
     *
     * <p>Read back from {@code constructionPrimitive}, not from anything the UI
     * believes: "a default box visually relabelled" would pass any assertion
     * made against the shell and fail this one.
     */
    @Test
    public void uir4a05_choosingSphereCreatesABodyThatIsNativelyASphere() {
        assertPaletteCreates(R.id.add_primitive_sphere, NativeViewport.PRIMITIVE_SPHERE);
    }

    /**
     * UIR4A-06. And Plane likewise — the newest primitive, and the one with a
     * sidedness contract of its own, so the one most likely to have been routed
     * through a shortcut.
     */
    @Test
    public void uir4a06_choosingPlaneRoutesThroughTheNativePlanePath() {
        assertPaletteCreates(R.id.add_primitive_plane, NativeViewport.PRIMITIVE_PLANE);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            // The plane's own two parameters, from the domain's own defaults.
            // Positive is the whole claim: a plane built by some other route
            // could arrive with a zero extent and still report its kind.
            assertTrue("the plane has a real width",
                    primitive[NativeViewport.PRIMITIVE_PLANE_WIDTH] > 0.0);
            assertTrue("and a real depth",
                    primitive[NativeViewport.PRIMITIVE_PLANE_WIDTH + 1] > 0.0);
            return null;
        });
    }

    /**
     * UIR4A-07. Closing the palette returns to the same Objects control, and
     * leaves exactly one Objects section in the product.
     */
    @Test
    public void uir4a07_closingAddPrimitiveLeavesOneObjectsControlBehind() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closeAddPrimitive(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("the palette is put away",
                    workspace.addPrimitivePalette().isOpen());
            assertEquals("and occupies nothing",
                    View.GONE, workspace.addPrimitivePalette().getVisibility());

            // Exactly one Objects section in the whole tree, and it is the one
            // the workspace owns. A second would be a second place for "which
            // body is active" to be remembered.
            assertEquals("there is one Objects section and it is the one instance",
                    1, countById(workspace, R.id.objects_section));
            assertNotNull(workspace.objectsSection().getParent());

            if (!workspace.objectsDocked()) {
                assertEquals("and the capsule that opened it is back at rest",
                        View.VISIBLE, workspace.objectsCapsule().getVisibility());
                assertFalse("with its plus no longer drawn active",
                        workspace.findViewById(R.id.objects_capsule_add).isActivated());
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4A-08 / UIR4A-09 -- the exact values, one action away
    // -----------------------------------------------------------------------

    /**
     * UIR4A-08. The exact shape values are one contextual action from the
     * Construction workspace, and they edit the active body.
     */
    @Test
    public void uir4a08_exactShapeIsOneActionAwayAndEditsTheActiveBody() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_shape).performClick();
            assertFalse("the exact values are not on screen until asked for",
                    workspace.propertyInspector().isOpen());
            openPrecision(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("one action put them on screen",
                    workspace.propertyInspector().isOpen());
            assertTrue("the dimension fields are reachable",
                    workspace.findViewById(R.id.field_box_width).isShown());
            assertTrue("and so is the commit",
                    workspace.findViewById(R.id.apply_shape).isShown());
            assertTrue("the surface names the body it edits",
                    title(workspace).contains(activity.getString(R.string.body_label,
                            NativeViewport.sceneActiveBodyId())));
            return null;
        });

        // And what it applies goes to that body and nowhere else.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            ((android.widget.EditText) workspace.findViewById(R.id.field_box_width))
                    .setText("3.25");
            workspace.findViewById(R.id.apply_shape).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            assertEquals("the typed width reached the active body exactly",
                    3.25, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH], 1.0e-12);
            return null;
        });
    }

    /**
     * UIR4A-09. The exact position and rotation are equally reachable, and the
     * transform convention they submit is unchanged.
     */
    @Test
    public void uir4a09_exactTransformIsReachableAndKeepsItsConvention() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_place).performClick();
            openPrecision(workspace);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Position X is reachable",
                    workspace.findViewById(R.id.field_pos_x).isShown());
            assertTrue("and Rotation Z", workspace.findViewById(R.id.field_rot_z).isShown());
            assertTrue("with its own separate commit",
                    workspace.findViewById(R.id.apply_transform).isShown());
            assertNull("shape and transform keep separate commits",
                    shownOrNull(workspace, R.id.apply_shape));

            ((android.widget.EditText) workspace.findViewById(R.id.field_pos_y))
                    .setText("1.5");
            ((android.widget.EditText) workspace.findViewById(R.id.field_rot_z))
                    .setText("30");
            workspace.findViewById(R.id.apply_transform).performClick();
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] transform = new double[NativeViewport.TRANSFORM_SIZE];
            NativeViewport.boxTransform(transform);
            assertEquals("position is meters, in the same slot order as before",
                    1.5, transform[1], 1.0e-12);
            assertEquals("and rotation is degrees, uncanonicalized", 30.0, transform[5],
                    1.0e-12);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4A-10 -- the rail keeps its icon language
    // -----------------------------------------------------------------------

    /**
     * UIR4A-10. A short window shortens the rail's entries; it never turns them
     * into a text list, and it never puts one under the touch floor.
     *
     * <p>Asserted in whatever window the suite is running in, because the rule
     * is unconditional: the icon is what is recognised at a glance, so a rail
     * that drops it is a different control from the one the user learned.
     */
    @Test
    public void uir4a10_theToolRailKeepsItsIconsAndItsTouchFloor() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int floor = EditorControlStyles.dimen(activity, R.dimen.control_height);
            final ToolRailView rail = workspace.findViewById(R.id.tool_rail);
            assertTrue("the rail has entries", rail.getChildCount() > 0);

            for (int id : new int[]{R.id.tool_rail_shape, R.id.tool_rail_place}) {
                final ViewGroup entry = workspace.findViewById(id);
                assertNotNull("each semantic Construction entry exists", entry);
                final String name = String.valueOf(entry.getContentDescription());
                assertTrue(name + " must keep a vector icon", hasIconChild(entry));
                assertTrue(name + " is " + entry.getHeight() + " px tall, under the "
                        + floor + " px touch floor", entry.getHeight() >= floor);
                assertTrue(name + " is " + entry.getWidth() + " px wide, under the "
                        + floor + " px touch floor", entry.getWidth() >= floor);
            }

            // The precision toggle is attached to the rail and is held to the
            // same floor: it is the control that reaches the exact values.
            final View toggle = WorkspaceTestSupport.precisionToggle(workspace);
            final int iconFloor = EditorControlStyles.dimen(activity, R.dimen.icon_button_size);
            assertTrue("the precision toggle keeps the touch floor too",
                    toggle.getWidth() >= iconFloor && toggle.getHeight() >= iconFloor);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UIR4A-14 -- opening surfaces is not geometry
    // -----------------------------------------------------------------------

    /**
     * UIR4A-14. Opening and closing every contextual surface in the workspace
     * publishes nothing, rebuilds nothing and uploads nothing.
     *
     * <p>Compared bit for bit against native state: "a panel is presentation" is
     * either exactly true or it is a defect.
     */
    @Test
    public void uir4a14_openingContextSurfacesRebuildsNoGeometry() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closeAddPrimitive(workspace);
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
        assertArrayEquals("opening and closing surfaces must leave native state identical:"
                + describeSnapshotDifference(before, after), before, after, 0.0);
    }

    // -----------------------------------------------------------------------
    // UIR4A-15 -- the new surfaces obey the gesture rule
    // -----------------------------------------------------------------------

    /**
     * UIR4A-15. Every surface introduced by this stage swallows its own drag,
     * so reaching for the scene, for a shape or for the exact values can never
     * orbit the camera or, in Sculpt, deform the model.
     *
     * <p>Consumption is the guarantee that matters: the viewport is a sibling
     * <i>below</i> the chrome, and Android never offers a consumed event to a
     * sibling underneath.
     */
    @Test
    public void uir4a15_theNewChromeSurfacesNeverLeakAGestureToTheViewport() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();

        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        final Boolean consumed = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            boolean all = dragConsumed(workspace.objectsCapsule());
            all &= dragConsumed(workspace.addPrimitivePalette());
            all &= dragConsumed(WorkspaceTestSupport.precisionGroup(workspace));
            all &= dragConsumed(workspace.propertyInspector());
            return all;
        });
        assertTrue("every new chrome surface must consume its own drag",
                Boolean.TRUE.equals(consumed));

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("a drag on chrome changes nothing below JNI:"
                + describeSnapshotDifference(before, after), before, after, 0.0);
    }

    // -----------------------------------------------------------------------
    // UIR4A-17 -- the expanded window speaks the same language
    // -----------------------------------------------------------------------

    /**
     * UIR4A-17. Whatever window this is, the interaction vocabulary is the same
     * one, and no chrome surface is an empty full-height slab.
     *
     * <p>A docked window swaps the capsule for a column and moves the plus into
     * it; it does not gain a different way of creating a body, and it does not
     * get a permanently open exact-value panel. Both halves are asserted from
     * the window the suite is actually in.
     */
    @Test
    public void uir4a17_everyWindowUsesOneInteractionVocabulary() {
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // One creation affordance is reachable in every window.
            final View plus = workspace.objectsDocked()
                    ? workspace.objectsSection().findViewById(R.id.add_body)
                    : workspace.findViewById(R.id.objects_capsule_add);
            assertNotNull("every window offers exactly one creation control", plus);
            assertTrue("and it is operable", plus.isEnabled());

            // The exact values are asked for in every window, never resident.
            assertEquals("no window opens the precision surface by itself",
                    View.GONE, workspace.propertyInspector().getVisibility());
            assertNotNull("and the toggle that opens it is present in every window",
                    WorkspaceTestSupport.precisionToggle(workspace));

            // No chrome surface spans the window's height.
            for (View surface : new View[]{workspace.objectsDock(),
                    WorkspaceTestSupport.trailingHost(workspace),
                    workspace.propertyInspector()}) {
                if (surface.getVisibility() != View.VISIBLE) {
                    continue;
                }
                assertTrue(surface + " must wrap its content rather than span the window: "
                                + surface.getHeight() + " of " + workspace.getHeight(),
                        surface.getHeight() < workspace.getHeight());
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    /**
     * Asserts that no chrome surface reaching the bottom of the window spans it.
     *
     * <p>The threshold is a proportion of the window rather than a dp figure,
     * because what is being ruled out is a <i>bar</i>: a surface wide enough
     * that the model is no longer reachable beside it along that edge. The
     * Objects capsule is sized to a body name and clears it comfortably in every
     * supported window.
     */
    private void assertNoBottomBar(final String where) {
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(where + ": no precision surface is in the resting workspace",
                    View.GONE, workspace.propertyInspector().getVisibility());

            final int windowWidth = workspace.getWidth();
            final int windowHeight = workspace.getHeight();
            // Anything whose lower edge is within a control's height of the
            // window's bottom is "on the bottom edge" for this rule.
            final int band = EditorControlStyles.dimen(activity, R.dimen.control_height);
            for (Rect rect : workspace.chromeRects()) {
                if (rect.bottom < windowHeight - band) {
                    continue;
                }
                assertTrue(where + ": a surface on the bottom edge spans " + rect.width()
                                + " of " + windowWidth + " px — that is a bar",
                        rect.width() <= windowWidth * 0.75);
            }

            // And the model is still the subject of the window it rests in.
            final double visible = unoccludedViewportFraction(windowWidth, windowHeight,
                    workspace.chromeRects());
            android.util.Log.i("ForgeShape", String.format(java.util.Locale.US,
                    "FORGESHAPE_UIR4A_RESTING %s window=%dx%d mode=%s unoccluded=%.1f%%",
                    where, windowWidth, windowHeight, workspace.layoutMode(),
                    visible * 100.0));
            assertTrue(where + ": the model must own most of the resting window, saw "
                    + Math.round(visible * 100) + " %", visible >= 0.60);
            return null;
        });
    }

    /**
     * Chooses a primitive from the palette and proves the body native code built
     * is that primitive.
     */
    private void assertPaletteCreates(final int tileId, final int expectedKind) {
        final int countBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyCount());
        final long activeBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(tileId).performClick();
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("exactly one body was added",
                    countBefore + 1, NativeViewport.sceneBodyCount());
            final long created = NativeViewport.sceneActiveBodyId();
            assertNotEquals("and it is a different body", activeBefore, created);

            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            assertEquals("the new body's NATIVE primitive kind is what was chosen —"
                            + " not a box wearing another name",
                    expectedKind, (int) primitive[0]);

            assertEquals("the capsule follows the new body",
                    activity.getString(R.string.body_label, created),
                    ((android.widget.TextView)
                            workspace.findViewById(R.id.objects_capsule_active))
                            .getText().toString());
            assertFalse("and the palette has closed",
                    workspace.addPrimitivePalette().isOpen());
            return null;
        });
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

    private static String title(EditorWorkspaceView workspace) {
        return ((android.widget.TextView) workspace.findViewById(R.id.inspector_title))
                .getText().toString();
    }

    /** The view for an id, but only when it is genuinely on screen. */
    private static View shownOrNull(EditorWorkspaceView workspace, int id) {
        final View view = workspace.findViewById(id);
        return (view != null && view.isShown()) ? view : null;
    }

    /**
     * Whether a rail entry still carries a vector glyph.
     *
     * <p>Asked of the entry's children rather than of a flag, because the rule
     * is about what is actually drawn: a compact window shrinks the icon and a
     * regression would remove it, and both look identical to any state the
     * entry itself reports.
     */
    private static boolean hasIconChild(ViewGroup entry) {
        for (int i = 0; i < entry.getChildCount(); i++) {
            final View child = entry.getChildAt(i);
            if (child instanceof android.widget.ImageView
                    && ((android.widget.ImageView) child).getDrawable() != null) {
                return true;
            }
        }
        return false;
    }

    private static Rect boundsIn(View view, EditorWorkspaceView workspace) {
        final Rect rect = new Rect(0, 0, view.getWidth(), view.getHeight());
        workspace.offsetDescendantRectToMyCoords(view, rect);
        return rect;
    }

    /** How many tappable tiles the palette actually offers. */
    /**
     * Every VISIBLE clickable leaf under the palette, at any depth.
     *
     * <p>Recursive since CAD-R0-A1A2, because the palette now holds two
     * sections — the shapes and the plane chooser New Sketch opens — and only
     * the one on screen offers anything. A hidden section's controls are not
     * offered and are not counted; a disabled placeholder would still be
     * visible, still clickable, and still caught.
     */
    private static int countTiles(ViewGroup palette) {
        int tiles = 0;
        for (int i = 0; i < palette.getChildCount(); i++) {
            final View child = palette.getChildAt(i);
            if (child.getVisibility() != View.VISIBLE) {
                continue;
            }
            if (child instanceof ViewGroup && !child.isClickable()) {
                tiles += countTiles((ViewGroup) child);
            } else if (child.isClickable()) {
                tiles++;
            }
        }
        return tiles;
    }

    private static int countById(View root, int id) {
        int found = root.getId() == id ? 1 : 0;
        if (root instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); i++) {
                found += countById(group.getChildAt(i), id);
            }
        }
        return found;
    }
}
