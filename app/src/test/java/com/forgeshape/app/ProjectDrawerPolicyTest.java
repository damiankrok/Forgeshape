package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.junit.Test;

/**
 * `MODELING-R1-OWNER-CORRECTION` on the JVM: the ForgeShape mark's project
 * drawer -- what it holds, in what order, what each row DOES, when it is
 * drawn -- and the toolbar row arithmetic on a narrow phone.
 *
 * <p>The device ({@code OwnerProjectShellSketchSupportTest}) proves the views
 * obey it with real window touches; this pins the rules without one.
 */
public final class ProjectDrawerPolicyTest {

    /** Records which handler each act reached, and the close before it. */
    private static final class Recorder implements ProjectActionsPopoverView.OnProjectAction,
            ProjectDrawerPolicy.Closer {
        final List<String> calls = new ArrayList<>();

        @Override public void closeDrawer() { calls.add("close"); }
        @Override public void onNewSketchRequested() { calls.add("newSketch"); }
        @Override public void onNewProjectRequested() { calls.add("newProject"); }
        @Override public void onSaveProjectRequested() { calls.add("save"); }
        @Override public void onOpenProjectRequested() { calls.add("open"); }
        @Override public void onSaveCopyRequested() { calls.add("saveCopy"); }
        @Override public void onOpenFileRequested() { calls.add("openFile"); }
        @Override public void onShareDiagnosticsRequested() { calls.add("diagnostics"); }
        @Override public void onImportGlbRequested() { calls.add("import"); }
        @Override public void onSettingsRequested() { calls.add("settings"); }
    }

    private static List<String> perform(ProjectDrawerPolicy.Action action) {
        final Recorder recorder = new Recorder();
        ProjectDrawerPolicy.perform(action, recorder, recorder);
        return recorder.calls;
    }

    // OSS-01: every drawer row reaches exactly the ONE existing handler.
    @Test
    public void oss01_everyRowMapsToItsExistingHandler() {
        final String[][] expected = {
            {"NEW_SKETCH", "newSketch"}, {"NEW_PROJECT", "newProject"},
            {"SAVE_PROJECT", "save"}, {"OPEN_SAVED_PROJECT", "open"},
            {"SAVE_COPY", "saveCopy"}, {"OPEN_FILE", "openFile"},
            {"SHARE_DIAGNOSTICS", "diagnostics"}, {"IMPORT_GLB", "import"},
            {"SETTINGS", "settings"},
        };
        assertEquals("one row per act, no more", expected.length, ProjectDrawerPolicy.ROWS.length);
        for (String[] pair : expected) {
            final ProjectDrawerPolicy.Action action = ProjectDrawerPolicy.Action.valueOf(pair[0]);
            assertEquals(pair[0], Arrays.asList("close", pair[1]), perform(action));
        }
        // The semantic ids the old popover's rows had are kept, so every
        // existing verification still names the same act.
        final int[] ids = {R.id.project_new_sketch, R.id.project_new, R.id.project_save,
                R.id.project_open, R.id.project_save_copy, R.id.project_open_file,
                R.id.project_share_diagnostics, R.id.import_glb, R.id.project_settings};
        for (int i = 0; i < ids.length; i++) {
            assertEquals("row " + i, ids[i], ProjectDrawerPolicy.ROWS[i].id);
        }
    }

    // OSS-02: New Sketch is the existing New Sketch, after the drawer closes.
    @Test
    public void oss02_newSketchClosesTheDrawerThenEntersTheSupportChooserPath() {
        assertEquals("closed first, so the chooser's planes and faces are not under it",
                Arrays.asList("close", "newSketch"), perform(ProjectDrawerPolicy.Action.NEW_SKETCH));
        assertEquals(R.string.new_sketch, ProjectDrawerPolicy.ROWS[0].label);
    }

    @Test
    public void groupsAreCreateProjectAndAppInThatOrder() {
        final List<ProjectDrawerPolicy.Group> order = new ArrayList<>();
        for (ProjectDrawerPolicy.Row row : ProjectDrawerPolicy.ROWS) {
            if (order.isEmpty() || order.get(order.size() - 1) != row.group) {
                order.add(row.group);
            }
        }
        assertEquals(Arrays.asList(ProjectDrawerPolicy.Group.CREATE,
                ProjectDrawerPolicy.Group.PROJECT, ProjectDrawerPolicy.Group.TRANSFER,
                ProjectDrawerPolicy.Group.IMPORT, ProjectDrawerPolicy.Group.APP), order);
        assertEquals(ProjectDrawerPolicy.Action.NEW_SKETCH, ProjectDrawerPolicy.ROWS[0].action);
        assertEquals(ProjectDrawerPolicy.Action.NEW_PROJECT, ProjectDrawerPolicy.ROWS[1].action);
        assertEquals(ProjectDrawerPolicy.Group.PROJECT, ProjectDrawerPolicy.ROWS[2].group);
        assertEquals(ProjectDrawerPolicy.Action.SAVE_PROJECT, ProjectDrawerPolicy.ROWS[2].action);
        assertEquals(ProjectDrawerPolicy.Action.OPEN_SAVED_PROJECT, ProjectDrawerPolicy.ROWS[3].action);
        assertEquals(ProjectDrawerPolicy.Action.SETTINGS,
                ProjectDrawerPolicy.ROWS[ProjectDrawerPolicy.ROWS.length - 1].action);
        assertEquals(R.string.project_drawer_create,
                ProjectDrawerPolicy.groupLabel(ProjectDrawerPolicy.Group.CREATE));
        assertEquals(R.string.project, ProjectDrawerPolicy.groupLabel(ProjectDrawerPolicy.Group.PROJECT));
        assertEquals(R.string.application_section,
                ProjectDrawerPolicy.groupLabel(ProjectDrawerPolicy.Group.APP));
    }

    @Test
    public void theMarkStandsOverEveryOpenProjectAndNotInTheBootstrap() {
        assertTrue(ProjectDrawerPolicy.markShown(true, false));
        assertFalse("no project yet: Back to Home is the way out",
                ProjectDrawerPolicy.markShown(false, true));
        assertFalse(ProjectDrawerPolicy.markShown(false, false));
    }

    @Test
    public void newSketchIsOfferedExactlyWhenItCanSucceed() {
        assertTrue("an open project at rest", ProjectDrawerPolicy.newSketchShown(true, false, false, false));
        assertFalse("no project", ProjectDrawerPolicy.newSketchShown(false, false, false, false));
        assertFalse("in Sculpt", ProjectDrawerPolicy.newSketchShown(true, true, false, false));
        assertFalse("a sketch is open", ProjectDrawerPolicy.newSketchShown(true, false, true, false));
        assertFalse("the chooser is already up",
                ProjectDrawerPolicy.newSketchShown(true, false, false, true));
    }

    @Test
    public void theDrawerGrowsFromTheLeadingEdgeWhereTheMarkIs() {
        assertTrue(ProjectDrawerPolicy.anchoredToLeadingEdge());
    }

    // The mark's hit area is the 48 dp interactive floor.
    @Test
    public void theMarksTouchTargetIsTheInteractiveFloor() throws IOException {
        final String dimens = read("src/main/res/values/dimens.xml");
        assertEquals("48dp", dimen(dimens, "icon_button_size"));
        assertEquals("48dp", dimen(dimens, "control_height"));
    }

    // The old trailing project opener is gone: the mark is the one door, it is
    // the product's own drawable, and the utility group carries no project
    // control.
    @Test
    public void theOldTopRightOpenerIsAbsentAndTheMarkIsTheOneDoor() throws IOException {
        final String toolbar = read("src/main/java/com/forgeshape/app/GlobalToolbarView.java");
        assertFalse("no generic project icon", toolbar.contains("R.drawable.ic_project"));
        assertEquals("one project opener", 1, count(toolbar, "R.id.project_actions_button"));
        assertTrue("drawn as the ForgeShape mark", toolbar.contains("R.drawable.ic_forgeshape_mark"));
        assertFalse("not in the trailing utility group",
                toolbar.contains("utilityGroup.addView(projectMark"));
        assertTrue("the mark leads the row",
                toolbar.indexOf("controlsRow.addView(markGroup")
                        < toolbar.indexOf("controlsRow.addView(editingGroup"));
    }

    // Narrow portrait: on the 411 dp phone every device run uses (and the
    // narrowest window the product is verified in), the mark's capsule, the
    // longest transition read in full and the utility group fit the row
    // without overlap. Widths in dp from dimens.xml: the mark is the 48 dp
    // target in a 4 dp-padded capsule plus the 4 dp gap; the utility group is
    // Export (a chip of about 66 dp), Display and Hide UI (48 dp each, a 4 dp
    // gap before each) in its 4 dp-padded capsule, plus the status slot's two
    // 4 dp margins.
    @Test
    public void aNarrowPortraitRowFitsTheMarkTheTransitionAndTheUtilityGroup() {
        final int density = 3;               // px per dp on a 1080 px-wide phone
        final int row = (411 - 2 * 8) * density;
        final int mark = (48 + 2 * 4 + 4) * density;
        final int utility = (66 + 4 + 48 + 4 + 48 + 2 * 4 + 2 * 4) * density;
        final int floor = 96 * density;
        final int budget = ToolbarRowBudget.transitionBudget(row, mark, utility, 0, 0, 2 * 4 * density,
                floor);
        assertTrue("the transition keeps at least its floor: " + budget, budget >= floor);
        assertTrue("and everything fits the row", mark + budget + 2 * 4 * density + utility <= row);
        // "Finish Sketch" -- the longest forward transition -- reads in full:
        // about 98 dp of text at body size plus 2 x 14 dp of padding.
        assertTrue("Finish Sketch fits in full: " + budget / density + " dp",
                budget >= 126 * density);
        // A squeeze never takes width from the mark or the utility group: only
        // the transition gives, and only down to its floor.
        assertEquals(floor, ToolbarRowBudget.transitionBudget(200 * density, mark, utility, 0, 0, 0,
                floor));
    }

    private static String read(String relative) throws IOException {
        Path path = Paths.get(relative);
        if (!Files.exists(path)) {
            path = Paths.get("app").resolve(relative);
        }
        return new String(Files.readAllBytes(path), StandardCharsets.UTF_8);
    }

    private static String dimen(String xml, String name) {
        final Matcher m = Pattern.compile("<dimen name=\"" + name + "\">([^<]+)</dimen>").matcher(xml);
        return m.find() ? m.group(1).trim() : "";
    }

    private static int count(String text, String needle) {
        int n = 0;
        for (int i = text.indexOf(needle); i >= 0; i = text.indexOf(needle, i + 1)) {
            n++;
        }
        return n;
    }
}
