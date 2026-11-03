package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * `CAD-PRIMITIVES-R1` on the device: the Shape panel of a Construction Box
 * offers Make CAD part; taking it replaces the box by a CAD part of the same
 * size and placement as ONE undo step, and Undo brings the box back by the same
 * id. A Sphere, a Cone and a Capsule convert too; a Plane has no CAD part, so the
 * control is not drawn for one and native refuses it by name.
 *
 * <p>Results are asserted from native truth (the representation, the measured
 * volume, the undo depth). Emulator evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class CadPrimitivePartTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABox() {
        // A FRESH project: the baseline body may carry a sculpt mesh an earlier
        // class left (the reset cannot un-freeze one), and a sculpted body has
        // no CAD part by design.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.enterConstructionMode();
            NativeViewport.closeProject();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void restoreAProject() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void a_box_becomes_a_cad_part_of_the_same_size_in_one_undo_step() {
        final long box = NativeViewport.sceneActiveBodyId();
        applied(NativeViewport.applyConstructionBox(2.0, 1.0, 3.0));
        NativeViewport.applyBoxTransform(1.0, 0.5, 0.0, 0.0, 30.0, 0.0, 1.0, 1.0, 1.0);
        assertTrue("a box has a CAD part", NativeViewport.sceneActiveHasCadPart());
        final int bodies = NativeViewport.sceneBodyCount();
        final int undoBefore = NativeViewport.constructionUndoDepth();

        openShapePanel();
        assertTrue("the Shape panel offers Make CAD part", shown(R.id.make_cad_part));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            activity.findViewById(R.id.make_cad_part).performClick();
            return null;
        });
        settleLayout();

        final long part = NativeViewport.sceneActiveBodyId();
        assertNotEquals("the part is a new object", box, part);
        assertEquals("it replaced the box rather than adding a body", bodies,
                NativeViewport.sceneBodyCount());
        assertEquals("the part is a CAD body", NativeViewport.REPRESENTATION_CAD,
                NativeViewport.sceneBodyRepresentation(part));
        assertEquals("as ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        final double[] mass = new double[NativeViewport.MASS_SIZE];
        assertTrue(NativeViewport.bodyMassProperties(part, mass));
        assertEquals("the part is closed", 1.0, mass[NativeViewport.MASS_CLOSED], 0.0);
        assertEquals("its volume is the box's", 6.0, mass[NativeViewport.MASS_VOLUME], 1e-4);
        assertEquals("its centre stands where the box stood", 1.0,
                mass[NativeViewport.MASS_CENTROID_X], 1e-4);
        assertEquals(0.5, mass[NativeViewport.MASS_CENTROID_X + 1], 1e-4);
        assertFalse("a CAD part has no second CAD part", NativeViewport.sceneActiveHasCadPart());

        assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
        assertEquals("Undo brings the box back by its own id", box,
                NativeViewport.sceneActiveBodyId());
        assertEquals(NativeViewport.REPRESENTATION_CONSTRUCTION,
                NativeViewport.sceneBodyRepresentation(box));
    }

    @Test
    public void a_sphere_a_cone_and_a_capsule_convert_and_a_plane_is_refused() {
        applied(NativeViewport.applyConstructionSphere(2.0));
        long part = convertActive();
        final double[] mass = new double[NativeViewport.MASS_SIZE];
        assertTrue(NativeViewport.bodyMassProperties(part, mass));
        assertEquals("the sphere's part is a ball of its volume (tessellated)",
                4.0 / 3.0 * Math.PI, mass[NativeViewport.MASS_VOLUME], 0.03 * 4.0 / 3.0 * Math.PI);
        assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());

        applied(NativeViewport.applyConstructionCone(2.0, 3.0));
        part = convertActive();
        assertTrue(NativeViewport.bodyMassProperties(part, mass));
        assertEquals("the cone's part is a cone of its volume (tessellated)",
                Math.PI, mass[NativeViewport.MASS_VOLUME], 0.03 * Math.PI);
        assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());

        applied(NativeViewport.applyConstructionCapsule(1.0, 3.0));
        part = convertActive();
        assertTrue(NativeViewport.bodyMassProperties(part, mass));
        final double capsuleVolume = Math.PI * 0.25 * 2.0 + 4.0 / 3.0 * Math.PI * 0.125;
        assertEquals("the capsule's part is a capsule of its volume (tessellated)",
                capsuleVolume, mass[NativeViewport.MASS_VOLUME], 0.03 * capsuleVolume);
        assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());

        applied(NativeViewport.applyConstructionPlane(2.0, 2.0));
        assertFalse("a plane has no CAD part", NativeViewport.sceneActiveHasCadPart());
        openShapePanel();
        assertFalse("so the control is not drawn", shown(R.id.make_cad_part));
        final long plane = NativeViewport.sceneActiveBodyId();
        assertEquals("and native refuses it by name", NativeViewport.CAD_PRIMITIVE_NOT_CONVERTIBLE,
                NativeViewport.sceneConvertToCadPart(plane));
        assertEquals("nothing changed", plane, NativeViewport.sceneActiveBodyId());
    }

    // -----------------------------------------------------------------------

    private static void applied(int code) {
        assertTrue("the shape is applied: " + code, code == NativeViewport.APPLY_APPLIED
                || code == NativeViewport.APPLY_UNCHANGED);
    }

    private long convertActive() {
        final long before = NativeViewport.sceneActiveBodyId();
        assertTrue(NativeViewport.sceneActiveHasCadPart());
        final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.sceneConvertToCadPart(before);
            workspace.onNativeStateChanged();
            return s;
        });
        settleLayout();
        assertEquals(NativeViewport.CAD_OK, status);
        final long part = NativeViewport.sceneActiveBodyId();
        assertNotEquals(before, part);
        assertEquals(NativeViewport.REPRESENTATION_CAD,
                NativeViewport.sceneBodyRepresentation(part));
        return part;
    }

    private void openShapePanel() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
    }

    private boolean shown(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = activity.findViewById(id);
            return view != null && view.isShown();
        });
    }
}
