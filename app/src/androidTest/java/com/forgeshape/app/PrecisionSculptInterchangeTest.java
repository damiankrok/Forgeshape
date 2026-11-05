package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
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
 * The owner's workflow of 2026-11-04 end to end on a device: arrange a part
 * precisely as in CAD (`PLACEMENT-PRECISION-R1`), sculpt detail into a CAD Body
 * (`CAD-SCULPT-R1`), and move the model in and out in several formats
 * (`INTERCHANGE-R1`).
 */
@RunWith(AndroidJUnit4.class)
public final class PrecisionSculptInterchangeTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void aCadPart() {
        resetToBaselineConstruction(rule.getScenario());
        final int converted = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.sceneConvertToCadPart(NativeViewport.sceneActiveBodyId());
            workspace.onNativeStateChanged();
            return s;
        });
        assertEquals(NativeViewport.CAD_OK, converted);
        settleLayout();
    }

    @After
    public void backToConstruction() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setGizmoSnap(0.0, 0.0);
            NativeViewport.enterConstructionMode();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    @Test
    public void a_cad_part_is_sculpted_and_the_way_back_names_it() {
        final int froze = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.freezeToSculpt();
            workspace.onNativeStateChanged();
            return s;
        });
        settleLayout();
        assertEquals("a CAD Body freezes into a sculpt mesh", NativeViewport.SCULPT_OK, froze);
        assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View back = workspace.findViewById(R.id.back_to_construction);
            assertNotNull(back);
            assertEquals("the way back names the CAD Body",
                    activity.getString(R.string.back_to_cad_body),
                    String.valueOf(back.getContentDescription()));
            return null;
        });
        // A sculpted CAD part saves: the `.forge` bytes carry its sculpt mesh.
        final byte[] project = NativeViewport.encodeProject();
        assertTrue("a sculpted CAD part encodes", project != null && project.length > 0);
        // And it exports from Sculpt as the sculpted mesh.
        final byte[] stl = NativeViewport.exportMesh(NativeViewport.EXPORT_FORMAT_STL);
        assertTrue("the sculpted part exports as STL", stl != null && stl.length > 84);
    }

    @Test
    public void editing_the_part_after_sculpting_marks_the_sculpt_stale_in_cad_words() {
        final int froze = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.freezeToSculpt();
            workspace.onNativeStateChanged();
            return s;
        });
        assertEquals(NativeViewport.SCULPT_OK, froze);
        // Back to the part, a CAD edit (a deeper extrusion), then Resume.
        final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        final int applied = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            NativeViewport.cadState(cad);
            final int s = NativeViewport.cadApplyExtrude(cad[NativeViewport.CAD_DEPTH] * 1.5,
                    (int) cad[NativeViewport.CAD_DIRECTION]);
            NativeViewport.enterSculptMode();
            workspace.onNativeStateChanged();
            return s;
        });
        settleLayout();
        assertEquals("the CAD edit applies", NativeViewport.APPLY_APPLIED, applied);
        final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.sculptState(sculpt);
        assertTrue("the sculpt mesh is marked as made from an older part",
                sculpt[NativeViewport.SCULPT_SOURCE_STALE] != 0.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final String expected = activity.getString(R.string.stale_source_warning_cad);
            final android.widget.TextView warning = workspace.sculptContext()
                    .findViewById(R.id.stale_source_warning);
            assertNotNull(warning);
            assertEquals("the warning is shown", View.VISIBLE, warning.getVisibility());
            assertEquals("and it names the CAD part", expected,
                    String.valueOf(warning.getText()));
            return null;
        });
    }

    @Test
    public void drop_to_ground_and_centre_on_origin_are_one_step_each() {
        final double[] placement = new double[NativeViewport.TRANSFORM_SIZE];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(0.37, 2.5, -0.8, 0, 0, 0, 1, 1, 1);
            workspace.onNativeStateChanged();
            return null;
        });
        final long depth = NativeViewport.constructionUndoDepth();
        final int dropped = NativeViewport.arrangeActiveBody(NativeViewport.ARRANGE_DROP_TO_GROUND);
        final int centred = NativeViewport.arrangeActiveBody(NativeViewport.ARRANGE_CENTER_ON_ORIGIN);
        NativeViewport.boxTransform(placement);
        assertEquals(NativeViewport.ARRANGE_OK, dropped);
        assertEquals(NativeViewport.ARRANGE_OK, centred);
        assertEquals("each arrange act is one Undo step", depth + 2,
                NativeViewport.constructionUndoDepth());
        assertEquals("centred on X", 0.0, placement[0], 1e-5);
        assertEquals("centred on Z", 0.0, placement[2], 1e-5);
        assertEquals("a second drop finds it already there", NativeViewport.ARRANGE_NOTHING_TO_DO,
                NativeViewport.arrangeActiveBody(NativeViewport.ARRANGE_DROP_TO_GROUND));
        // Snap while dragging: the step native holds is the one chosen.
        NativeViewport.setGizmoSnap(0.01, 15.0);
        final double[] snap = new double[2];
        NativeViewport.gizmoSnapState(snap);
        assertEquals(0.01, snap[0], 0.0);
        assertEquals(15.0, snap[1], 0.0);
    }

    @Test
    public void every_export_format_writes_its_own_file_and_reads_back() {
        final int bodies = NativeViewport.sceneBodyCount();
        final byte[] stl = NativeViewport.exportMesh(NativeViewport.EXPORT_FORMAT_STL);
        final byte[] obj = NativeViewport.exportMesh(NativeViewport.EXPORT_FORMAT_OBJ);
        final byte[] ply = NativeViewport.exportMesh(NativeViewport.EXPORT_FORMAT_PLY);
        final byte[] threeMf = NativeViewport.exportMesh(NativeViewport.EXPORT_FORMAT_3MF);
        assertNotNull(stl);
        assertNotNull(obj);
        assertNotNull(ply);
        assertNotNull(threeMf);
        final int triangles = (stl[80] & 0xFF) | (stl[81] & 0xFF) << 8 | (stl[82] & 0xFF) << 16
                | (stl[83] & 0xFF) << 24;
        assertEquals("a binary STL is 84 + 50 bytes per triangle", 84 + 50 * triangles, stl.length);
        assertTrue("an OBJ is text naming ForgeShape", new String(obj, 0, 12,
                java.nio.charset.StandardCharsets.US_ASCII).startsWith("# ForgeShape"));
        assertTrue("a PLY starts with its magic", ply[0] == 'p' && ply[1] == 'l' && ply[2] == 'y');
        assertTrue("a 3MF is a zip", threeMf[0] == 'P' && threeMf[1] == 'K');
        final String[] extensions = {".stl", ".obj", ".ply", ".3mf"};
        final byte[][] files = {stl, obj, ply, threeMf};
        for (int i = 0; i < files.length; i++) {
            final byte[] file = files[i];
            final String extension = extensions[i];
            final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                final int s = NativeViewport.importMeshDurable(file, extension);
                workspace.onNativeStateChanged();
                return s;
            });
            assertEquals(extension + " imports back", NativeViewport.IMPORT_OK, status);
        }
        assertEquals("each import added a body", bodies + 4, NativeViewport.sceneBodyCount());
    }

    @Test
    public void the_export_chip_opens_the_format_menu() {
        // Export as… is a row of the project drawer (`WORKBENCH-RIBBON-R2`):
        // open the drawer, press the row; the drawer closes and the toolbar's
        // format menu opens from the mark.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.project_actions_button).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.export_action).performClick();
            return null;
        });
        settleLayout();
        final int items = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.PopupMenu menu = workspace.globalToolbar().exportMenuForTest();
            final int count = menu == null ? 0 : menu.getMenu().size();
            if (menu != null) {
                menu.dismiss();
            }
            return count;
        });
        assertEquals("GLB, STL, 3MF, OBJ and PLY", 5, items);
        // A choice stages that format's bytes for the picker.
        final byte[] staged = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setProjectTransferHost(new EditorWorkspaceView.ProjectTransferHost() {
                @Override
                public boolean requestCreateProjectDocument() {
                    return false;
                }

                @Override
                public boolean requestOpenProjectDocument() {
                    return false;
                }

                @Override
                public boolean requestCreateDiagnosticsDocument() {
                    return false;
                }

                @Override
                public boolean requestCreateGlbDocument() {
                    return true;
                }

                @Override
                public boolean requestOpenGlbDocument() {
                    return false;
                }

                @Override
                public boolean requestCreateMeshDocument(String mimeType, String suggestedName) {
                    return "model/3mf".equals(mimeType) && suggestedName.endsWith(".3mf");
                }
            });
            workspace.onExportRequested(NativeViewport.EXPORT_FORMAT_3MF);
            final byte[] bytes = workspace.pendingExportBytesForTest();
            workspace.setProjectTransferHost(activity.projectTransferHost());
            return bytes;
        });
        assertTrue("the 3MF bytes wait for the picker", staged != null && staged[0] == 'P');
    }
}
