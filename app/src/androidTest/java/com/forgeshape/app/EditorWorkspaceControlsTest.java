package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * UI-01, UI-02, UI-04, UI-05, UI-06 and UI-13: which controls exist, what they
 * do, and what they must leave alone.
 *
 * <p>Every control is reached by its stable semantic id. No assertion here
 * depends on where anything is on screen.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceControlsTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // UI-01 -- each mode shows its own control set
    // -----------------------------------------------------------------------

    @Test
    public void ui01_constructionShowsConstructionControlsAndNoBrush() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("Construction rail carries the shape editor",
                    workspace.findViewById(R.id.tool_rail_shape));
            assertNotNull("Construction rail carries the placement editor",
                    workspace.findViewById(R.id.tool_rail_place));
            assertNotNull("Sketch is drawn as a reserved home",
                    workspace.findViewById(R.id.tool_rail_sketch));
            assertNotNull("Extrude is drawn as a reserved home",
                    workspace.findViewById(R.id.tool_rail_extrude));
            assertNull("no sculpt brush belongs in Construction",
                    workspace.findViewById(R.id.tool_rail_grab));

            assertEquals("the brush controls are absent, not merely disabled",
                    View.GONE, workspace.findViewById(R.id.brush_edge_controls).getVisibility());
            assertNotNull("the shape editor's commit is on screen",
                    workspace.findViewById(R.id.apply_shape));
            assertNull("nothing in Construction can freeze again",
                    workspace.findViewById(R.id.freeze_again));
            return null;
        });
    }

    @Test
    public void ui01_reservedRailEntriesAreVisibleAndInert() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int id : new int[]{R.id.tool_rail_sketch, R.id.tool_rail_extrude}) {
                final View entry = workspace.findViewById(id);
                assertEquals("a reserved entry stays visible so the shell's shape "
                        + "does not change when it arrives", View.VISIBLE, entry.getVisibility());
                assertFalse("a reserved entry must not be operable", entry.isEnabled());
                assertTrue("and must say why",
                        String.valueOf(entry.getContentDescription()).contains("not implemented"));
            }
            final View export = workspace.findViewById(R.id.export_action);
            assertEquals(View.VISIBLE, export.getVisibility());
            assertFalse("Export is approved but not implemented", export.isEnabled());
            return null;
        });
    }

    @Test
    public void ui01_sculptShowsBrushControlsAndNoShapeEditor() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            for (int id : new int[]{R.id.tool_rail_grab, R.id.tool_rail_clay,
                    R.id.tool_rail_smooth, R.id.tool_rail_inflate}) {
                assertNotNull("every sculpt tool is one tap away",
                        workspace.findViewById(id));
            }
            assertEquals("Radius and Strength are direct, always on screen",
                    View.VISIBLE, workspace.findViewById(R.id.brush_edge_controls).getVisibility());
            assertNotNull(workspace.findViewById(R.id.brush_radius_slider));
            assertNotNull(workspace.findViewById(R.id.brush_strength_slider));
            assertNull("nothing on screen in Sculpt may edit the Construction Source",
                    workspace.findViewById(R.id.apply_shape));
            assertNull(workspace.findViewById(R.id.apply_transform));
            assertNull(workspace.findViewById(R.id.tool_rail_shape));
            assertNotNull("the guarded re-Freeze lives in the Sculpt inspector",
                    workspace.findViewById(R.id.freeze_again));
            return null;
        });
    }

    @Test
    public void ui01_constructionRailSwitchesTheInspectorBodyWithoutTouchingTheObject() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_place).performClick();
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("the placement editor is on screen",
                    workspace.findViewById(R.id.apply_transform));
            assertNotNull(workspace.findViewById(R.id.field_pos_x));
            assertNotNull(workspace.findViewById(R.id.field_rot_z));
            assertNull("shape and placement have separate commits",
                    workspace.findViewById(R.id.apply_shape));
            final double[] after = nativeSnapshot();
            assertArrayEquals("choosing a rail entry is presentation:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UI-02 -- every primitive exposes its own exact fields
    // -----------------------------------------------------------------------

    @Test
    public void ui02_eachPrimitiveShowsOnlyItsOwnFields() {
        final int[] chips = {R.id.primitive_option_box, R.id.primitive_option_cylinder,
                R.id.primitive_option_sphere, R.id.primitive_option_cone,
                R.id.primitive_option_capsule, R.id.primitive_option_plane};
        final int[] rows = {R.id.primitive_row_box, R.id.primitive_row_cylinder,
                R.id.primitive_row_sphere, R.id.primitive_row_cone, R.id.primitive_row_capsule,
                R.id.primitive_row_plane};
        final int[][] fields = {
                {R.id.field_box_width, R.id.field_box_height, R.id.field_box_depth},
                {R.id.field_cylinder_diameter, R.id.field_cylinder_height},
                {R.id.field_sphere_diameter},
                {R.id.field_cone_diameter, R.id.field_cone_height},
                {R.id.field_capsule_diameter, R.id.field_capsule_total_height},
                {R.id.field_plane_width, R.id.field_plane_depth},
        };

        for (int kind = 0; kind < chips.length; kind++) {
            final int selected = kind;
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.findViewById(chips[selected]).performClick();
                return null;
            });
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                for (int row = 0; row < rows.length; row++) {
                    assertEquals("exactly the drafted primitive's row is on screen",
                            row == selected ? View.VISIBLE : View.GONE,
                            workspace.findViewById(rows[row]).getVisibility());
                }
                for (int fieldId : fields[selected]) {
                    final View field = workspace.findViewById(fieldId);
                    assertNotNull("field " + fieldId + " belongs to this primitive", field);
                    assertTrue("and it must be reachable", field.isShown());
                }
                assertTrue("the chosen primitive is unmistakable",
                        workspace.findViewById(chips[selected]).isActivated());
                return null;
            });
        }
    }

    /**
     * PLN-08: moving the chooser to the Plane chip — the newest, so the one
     * most likely to have skipped this contract by accident — is pure
     * presentation. No native Apply happens and nothing observable through JNI
     * changes, exactly as UI-01's rail switch and every other chooser move.
     */
    @Test
    public void ui02_selectingThePlaneChipMakesNoNativeCall() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.primitive_option_plane).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the Plane chip is now the drafted kind",
                    workspace.findViewById(R.id.primitive_option_plane).isActivated());
            final double[] after = nativeSnapshot();
            assertArrayEquals("selecting a chip is presentation only:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            return null;
        });
    }

    /**
     * PLN-07: all six primitive payloads survive a full round trip of kind
     * switches — each remembers its own last-applied values independently, and
     * the placement (set once, before any shape change) survives every one of
     * them. ObjectId is not separately observable from Java outside Sculpt
     * mode; identity stability under a kind change is covered natively (see
     * FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK's round-trip checks).
     */
    @Test
    public void ui07_allSixPrimitivePayloadsSurviveASelectorRoundTrip() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(0.4, -0.2, 0.1, 5.0, 10.0, 15.0);
            NativeViewport.applyConstructionBox(1.1, 2.2, 3.3);
            NativeViewport.applyConstructionCylinder(1.5, 2.5);
            NativeViewport.applyConstructionSphere(0.9);
            NativeViewport.applyConstructionCone(1.2, 1.8);
            NativeViewport.applyConstructionCapsule(0.6, 2.4);
            NativeViewport.applyConstructionPlane(2.0, 1.25);
            workspace.syncFromNative();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            assertEquals("ends on the last-applied kind",
                    NativeViewport.PRIMITIVE_PLANE, (int) primitive[0]);
            assertEquals(1.1, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH], 0.0);
            assertEquals(2.2, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 1], 0.0);
            assertEquals(3.3, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 2], 0.0);
            assertEquals(1.5, primitive[NativeViewport.PRIMITIVE_CYLINDER_DIAMETER], 0.0);
            assertEquals(2.5, primitive[NativeViewport.PRIMITIVE_CYLINDER_DIAMETER + 1], 0.0);
            assertEquals(0.9, primitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER], 0.0);
            assertEquals(1.2, primitive[NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER], 0.0);
            assertEquals(1.8, primitive[NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER + 1], 0.0);
            assertEquals(0.6, primitive[NativeViewport.PRIMITIVE_CAPSULE_DIAMETER], 0.0);
            assertEquals(2.4, primitive[NativeViewport.PRIMITIVE_CAPSULE_DIAMETER + 1], 0.0);
            assertEquals(2.0, primitive[NativeViewport.PRIMITIVE_PLANE_WIDTH], 0.0);
            assertEquals(1.25, primitive[NativeViewport.PRIMITIVE_PLANE_WIDTH + 1], 0.0);

            final double[] transform = new double[6];
            NativeViewport.boxTransform(transform);
            assertArrayEquals("placement survives every shape change in the chain",
                    new double[]{0.4, -0.2, 0.1, 5.0, 10.0, 15.0}, transform, 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UI-04 -- an invalid exact value is reported and commits nothing
    // -----------------------------------------------------------------------

    @Test
    public void ui04_textThatIsNotANumberIsRejectedAndNothingIsSubmitted() {
        assertApplyShapeRejects("abc", "is not a number");
    }

    @Test
    public void ui04_aDimensionThatIsNotPositiveIsRejectedAndNothingIsSubmitted() {
        assertApplyShapeRejects("-1", "greater than 0");
    }

    @Test
    public void ui04_anEmptyFieldIsRejectedAndNothingIsSubmitted() {
        assertApplyShapeRejects("", "is empty");
    }

    /**
     * A value native code refuses: the panel renders the domain's reason rather
     * than restating the rule itself, and the object is untouched.
     */
    @Test
    public void ui04_aCapsuleRelationRejectionCarriesTheDomainsReason() {
        final double[] before = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.primitive_option_capsule).performClick();
            return nativeSnapshot();
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            ((EditText) workspace.findViewById(R.id.field_capsule_diameter)).setText("2");
            ((EditText) workspace.findViewById(R.id.field_capsule_total_height)).setText("1");
            workspace.findViewById(R.id.apply_shape).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final CharSequence status =
                    ((TextView) workspace.findViewById(R.id.status_message)).getText();
            assertTrue("the reason must name the relation, not say \"invalid\": " + status,
                    String.valueOf(status).contains("Total Height cannot be less than Diameter"));
            final double[] after = nativeSnapshot();
            assertArrayEquals("a rejected Apply changes nothing:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            return null;
        });
    }

    private void assertApplyShapeRejects(final String text, final String expectedReason) {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            ((EditText) workspace.findViewById(R.id.field_box_width)).setText(text);
            workspace.findViewById(R.id.apply_shape).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final CharSequence status =
                    ((TextView) workspace.findViewById(R.id.status_message)).getText();
            assertTrue("the message must name the field and the problem: " + status,
                    String.valueOf(status).contains("Width")
                            && String.valueOf(status).contains(expectedReason));
            final double[] after = nativeSnapshot();
            assertArrayEquals("nothing may be partially committed:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UI-05 -- Freeze and Resume wording, state and non-destructiveness
    // -----------------------------------------------------------------------

    @Test
    public void ui05_freezeAndResumeWordingFollowsWhetherAMeshExists() {
        final boolean hadMesh = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            final boolean has = sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0;
            assertEquals("with no frozen mesh the only offer is Freeze to Sculpt",
                    has ? View.GONE : View.VISIBLE,
                    workspace.findViewById(R.id.freeze_to_sculpt).getVisibility());
            assertEquals("with a frozen mesh the offer is the non-destructive Resume",
                    has ? View.VISIBLE : View.GONE,
                    workspace.findViewById(R.id.resume_sculpt).getVisibility());
            assertEquals(View.GONE,
                    workspace.findViewById(R.id.back_to_construction).getVisibility());
            return has;
        });

        if (!hadMesh) {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.findViewById(R.id.freeze_to_sculpt).performClick();
                return null;
            });
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
                workspace.findViewById(R.id.back_to_construction).performClick();
                return null;
            });
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertEquals("once something is frozen, Resume takes the slot",
                        View.VISIBLE, workspace.findViewById(R.id.resume_sculpt).getVisibility());
                assertEquals(View.GONE,
                        workspace.findViewById(R.id.freeze_to_sculpt).getVisibility());
                return null;
            });
        }
    }

    /**
     * Resume is not a destructive act and is never confirmed: it returns to the
     * sculpt work exactly as it was left, and guarding it would train the user
     * to dismiss the guard that matters.
     */
    @Test
    public void ui05_resumeSculptIsNonDestructiveAndUnconfirmed() {
        enterSculpt();
        final double[] frozen = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return nativeSnapshot();
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.resume_sculpt).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            assertNull("Resume must never ask for confirmation",
                    workspace.sculptContext().visibleConfirmation());
            final double[] sculptNow = nativeSnapshot();
            final int revisionSlot = NativeViewport.PRIMITIVE_STATE_SIZE + 6
                    + NativeViewport.SCULPT_REVISION;
            final int vertexSlot = NativeViewport.PRIMITIVE_STATE_SIZE + 6
                    + NativeViewport.SCULPT_VERTEX_COUNT;
            assertEquals("Resume re-freezes nothing", frozen[revisionSlot],
                    sculptNow[revisionSlot], 0.0);
            assertEquals(frozen[vertexSlot], sculptNow[vertexSlot], 0.0);
            return null;
        });
    }

    /**
     * The one irreversible act, guarded exactly when there is something to
     * lose. A mesh with no stroke on it can be replaced by an identical copy,
     * so confirming that would be noise.
     */
    @Test
    public void ui05_reFreezeIsUnguardedWhenNoStrokeHasLanded() {
        enterSculpt();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            if (sculpt[NativeViewport.SCULPT_STROKE_COUNT] != 0.0) {
                return null;   // a previous test sculpted; the guarded case covers this
            }
            workspace.findViewById(R.id.freeze_again).performClick();
            assertNull("re-freezing an unsculpted mesh discards nothing",
                    workspace.sculptContext().visibleConfirmation());
            assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UI-06 -- the four sculpt tools
    // -----------------------------------------------------------------------

    @Test
    public void ui06_everySculptToolIsSelectableAndTheActiveOneIsVisiblyActive() {
        enterSculpt();
        final int[] railIds = {R.id.tool_rail_grab, R.id.tool_rail_clay,
                R.id.tool_rail_smooth, R.id.tool_rail_inflate};
        final int[] tools = {NativeViewport.TOOL_GRAB, NativeViewport.TOOL_CLAY,
                NativeViewport.TOOL_SMOOTH, NativeViewport.TOOL_INFLATE};

        for (int i = 0; i < railIds.length; i++) {
            final int index = i;
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.findViewById(railIds[index]).performClick();
                return null;
            });
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertEquals("native code owns which tool is held",
                        tools[index], NativeViewport.sculptTool());
                for (int other = 0; other < railIds.length; other++) {
                    assertEquals("exactly one entry may be drawn active",
                            other == index,
                            workspace.findViewById(railIds[other]).isActivated());
                }
                return null;
            });
        }
    }

    @Test
    public void ui06_switchingToolLeavesTheBrushAndTheMeshAlone() {
        enterSculpt();
        final double[] before = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_grab).performClick();
            return nativeSnapshot();
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_inflate).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] after = nativeSnapshot();
            final int base = NativeViewport.PRIMITIVE_STATE_SIZE + 6;
            assertEquals("Radius is shared by every tool",
                    before[base + NativeViewport.SCULPT_RADIUS_PIXELS],
                    after[base + NativeViewport.SCULPT_RADIUS_PIXELS], 0.0);
            assertEquals("Strength is shared by every tool",
                    before[base + NativeViewport.SCULPT_STRENGTH],
                    after[base + NativeViewport.SCULPT_STRENGTH], 0.0);
            assertEquals("changing tool publishes no revision",
                    before[base + NativeViewport.SCULPT_REVISION],
                    after[base + NativeViewport.SCULPT_REVISION], 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UI-03 and UI-13 -- presentation-only actions change nothing below JNI
    // -----------------------------------------------------------------------

    @Test
    public void ui03_switchingTheDisplayUnitIsPresentationOnly() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.unit_chip_mm).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("2 m is 2000 mm, exactly", "2000",
                    ((EditText) workspace.findViewById(R.id.field_box_width)).getText()
                            .toString());
            assertArrayEquals("switching unit makes no native call:"
                            + describeSnapshotDifference(before, nativeSnapshot()),
                    before, nativeSnapshot(), 0.0);
            workspace.findViewById(R.id.unit_chip_cm).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("200",
                    ((EditText) workspace.findViewById(R.id.field_box_width)).getText()
                            .toString());
            workspace.findViewById(R.id.unit_chip_m).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("m -> mm -> cm -> m reproduces the authored digits", "2",
                    ((EditText) workspace.findViewById(R.id.field_box_width)).getText()
                            .toString());
            assertArrayEquals("and the object is bit-identical throughout:"
                            + describeSnapshotDifference(before, nativeSnapshot()),
                    before, nativeSnapshot(), 0.0);
            return null;
        });
    }

    @Test
    public void ui03_theDisplayUnitGovernsTheEditorThatIsOffScreenToo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(1.5, 0.0, 0.0, 0.0, 0.0, 0.0);
            workspace.syncFromNative();
            workspace.findViewById(R.id.unit_chip_mm).performClick();
            workspace.findViewById(R.id.tool_rail_place).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the placement editor converted while it was away", "1500",
                    ((EditText) workspace.findViewById(R.id.field_pos_x)).getText().toString());
            return null;
        });
    }

    /**
     * UI-13: nothing that is purely UI may mint a mesh revision. The revision
     * counters are read straight from native state, so this is a claim about
     * the domain and not about what the UI believes it did.
     */
    @Test
    public void ui13_layoutAndPresentationActionsMintNoRevision() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.primitive_option_cone).performClick();
            workspace.findViewById(R.id.unit_chip_cm).performClick();
            workspace.findViewById(R.id.tool_rail_place).performClick();
            workspace.findViewById(R.id.inspector_toggle).performClick();
            workspace.findViewById(R.id.hide_ui_toggle).performClick();
            workspace.findViewById(R.id.restore_ui_chip).performClick();
            workspace.applyLayoutForWindow(workspace.getWidth(), workspace.getHeight());
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] after = nativeSnapshot();
            assertArrayEquals("drafting, unit, rail, detent, hide and relayout are all UI:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------

    private void enterSculpt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (NativeViewport.productMode() != NativeViewport.MODE_SCULPT) {
                final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
                NativeViewport.sculptState(sculpt);
                if (sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0) {
                    workspace.findViewById(R.id.resume_sculpt).performClick();
                } else {
                    workspace.findViewById(R.id.freeze_to_sculpt).performClick();
                }
            }
            return null;
        });
    }
}
