package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

/**
 * `FSR1B-14..18`: the diagnostic channel is bounded and local, the renderer
 * losing its device costs the user nothing, and the project surface is still the
 * shape the accepted layout says it is.
 *
 * <h2>How a lost device is provoked</h2>
 *
 * <p>Through the debug injection seam, never by destabilising a real GPU. The
 * repository forbids doing that to the authoritative emulator, and a test that
 * did it would be measuring the driver rather than ForgeShape. Everything after
 * the injection point is the real recovery path: the same classification, the
 * same policy, the same teardown and the same rebuild.
 */
@RunWith(AndroidJUnit4.class)
public final class DiagnosticsAndRendererLossTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Before
    public void setUp() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void tearDown() {
        // The report is deliberately LEFT in place. It is app-private, bounded,
        // and carries none of the user's work — the cases above are what prove
        // that — and leaving it is what lets the evidence package hold a real
        // sample of one rather than a hand-written imitation.
        context().deleteFile(ProjectCheckpoint.CHECKPOINT_FILE_NAME);
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // FSR1B-14 / FSR1B-15: a bounded, local report with no model in it
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b15_aReportIsWrittenLocallyAndIsBounded() {
        // Give the log something real to hold: a save, an open and an edit each
        // record a persistence event.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionBox(3.5, 1.75, 0.875);
            workspace.noteProjectMaybeDirty();
            return null;
        });
        settleLayout();

        assertTrue("the report is written into app-private storage",
                Diagnostics.writeReport(context(), Diagnostics.REASON_MANUAL));
        final File report = Diagnostics.reportFile(context());
        assertTrue(report.isFile());
        assertTrue("and it is not empty", report.length() > 0);
        assertTrue("and it is bounded far below anything unreadable",
                report.length() < 256 * 1024);
    }

    @Test
    public void fsr1b15_aReportNamesTheBuildAndTheDeviceButNeverTheModel() {
        buildAProjectWithDistinctiveNumbers();
        final String report = onWorkspace(rule.getScenario(),
                (activity, workspace) -> Diagnostics.renderReport(context(),
                        Diagnostics.REASON_MANUAL, null));

        // What it MUST contain: enough to know which code ran.
        assertTrue(report.contains("ForgeShape diagnostic report"));
        assertTrue(report.contains("package=com.forgeshape.app"));
        assertTrue(report.contains("versionName="));
        assertTrue(report.contains("android="));
        assertTrue(report.contains("abis="));
        assertTrue(report.contains("rendererLifecycle="));
        assertTrue(report.contains("reason=" + Diagnostics.REASON_MANUAL));

        // What it must NEVER contain: the user's work. The dimensions below are
        // in the live project right now, so if any of them appeared, the report
        // would be carrying the model.
        assertFalse("no project dimension", report.contains("7.3125"));
        assertFalse("no project dimension", report.contains("4.8125"));
        assertFalse("no `.forge` magic", report.contains("FORGESH1"));
        assertFalse("no vertex data", report.toLowerCase(java.util.Locale.US)
                .contains("vertexpositions"));
    }

    @Test
    public void fsr1b15_theReportIsBoundedEvenAfterAFloodOfEvents() {
        for (int i = 0; i < DiagnosticLog.MAX_RECORDS * 4; i++) {
            Diagnostics.info(DiagnosticLog.CAT_PERSISTENCE, "FLOOD_EVENT_" + i, "detail" + i);
        }
        final String report = Diagnostics.renderReport(context(), Diagnostics.REASON_MANUAL,
                null);
        assertTrue("the rendered report stays inside the ring's bound",
                report.length() < DiagnosticLog.MAX_RENDERED_CHARS + 4096);
        assertTrue("and says how many records it dropped rather than hiding them",
                report.contains("dropped"));
    }

    /**
     * `E2ER1B-08`: nothing is sent anywhere, and it cannot be.
     *
     * <p>The strongest available proof, and a structural one: ForgeShape does
     * not hold the {@code INTERNET} permission. It is not that no code happens
     * to make a request — the process is incapable of making one. Sharing is the
     * user picking a destination through the system's own document UI.
     */
    @Test
    public void e2er1b08_forgeShapeCannotMakeANetworkRequestAtAll() {
        final PackageManager packages = context().getPackageManager();
        final PackageInfo info;
        try {
            info = packages.getPackageInfo(context().getPackageName(),
                    PackageManager.GET_PERMISSIONS);
        } catch (PackageManager.NameNotFoundException error) {
            throw new AssertionError("the package under test must be installed", error);
        }
        final String[] requested = info.requestedPermissions;
        if (requested != null) {
            for (String permission : requested) {
                assertFalse("ForgeShape must not request " + permission,
                        permission.contains("INTERNET")
                                || permission.contains("ACCESS_NETWORK_STATE"));
            }
        }
        assertEquals("and the platform agrees it does not hold INTERNET",
                PackageManager.PERMISSION_DENIED,
                packages.checkPermission("android.permission.INTERNET",
                        context().getPackageName()));
    }

    @Test
    public void e2er1b08_sharingAsksForADocumentAndWritesTheReportThere() {
        final File destination = new File(context().getCacheDir(), "diagnostics-out.txt");
        destination.delete();

        // The intent that would be sent: a document creation, never a network
        // target and never an implicit share to an arbitrary app.
        final android.content.Intent intent = ProjectTransfer.createDiagnosticsIntent();
        assertEquals(android.content.Intent.ACTION_CREATE_DOCUMENT, intent.getAction());
        assertEquals("text/plain", intent.getType());
        assertNotNull(intent.getStringExtra(android.content.Intent.EXTRA_TITLE));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onCreateDiagnosticsDocumentChosen(
                    android.net.Uri.fromFile(destination));
            return null;
        });
        settleLayout();

        assertTrue("the report lands where the user chose", destination.isFile());
        assertTrue(destination.length() > 0);
        destination.delete();
    }

    // -----------------------------------------------------------------------
    // FSR1B-16 / E2ER1B-07: the device is lost and the project is not
    // -----------------------------------------------------------------------

    @Test
    public void e2er1b07_aLostDeviceCostsTheProjectNothing() {
        buildAProjectWithDistinctiveNumbers();
        final double[] before = WorkspaceTestSupport.nativeSnapshot();
        final byte[] documentBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
        final int rebuildsBefore = NativeViewport.debugRendererDeviceRebuilds();

        assertTrue("the debug build must offer the injection seam",
                NativeViewport.debugInjectDeviceLoss());

        // The render thread has to reach a frame, classify the loss, tear the
        // device down and build it again. There is no idle hook to wait on — the
        // render loop is free-running — so this polls for a bounded time for
        // either outcome the contract allows.
        final int settled = awaitRendererSettled(rebuildsBefore);

        // WHICHEVER branch was taken, the project is untouched. That is the
        // claim that matters: GPU resources were never project truth.
        assertArrayEquals("a lost device moves no project value",
                before, WorkspaceTestSupport.nativeSnapshot(), 0.0);
        assertArrayEquals("and the project still encodes to the same document",
                documentBefore,
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.encodeProject()));

        if (settled == NativeViewport.RENDERER_HEALTHY) {
            assertTrue("a healthy renderer after a loss must have actually rebuilt",
                    NativeViewport.debugRendererDeviceRebuilds() > rebuildsBefore);
        } else {
            assertEquals("the only other permitted outcome is an explicit restart-required",
                    NativeViewport.RENDERER_RESTART_REQUIRED, settled);
            // The fail-closed branch owes the user a checkpoint and a sentence.
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.onRendererRestartRequired();
                return null;
            });
            settleLayout();
            assertTrue("a stopped renderer leaves the work checkpointed",
                    onWorkspace(rule.getScenario(),
                            (activity, workspace) ->
                                    workspace.autosaveController().awaitIdle(15_000L)));
            assertTrue(ProjectCheckpoint.exists(context()));
        }
    }

    @Test
    public void e2er1b07_theProjectStaysEditableAfterTheDeviceComesBack() {
        final int rebuildsBefore = NativeViewport.debugRendererDeviceRebuilds();
        NativeViewport.debugInjectDeviceLoss();
        final int settled = awaitRendererSettled(rebuildsBefore);
        org.junit.Assume.assumeTrue(
                "this case is about the rebuild branch; the restart-required branch is"
                        + " covered by the case above",
                settled == NativeViewport.RENDERER_HEALTHY);

        // An ordinary edit after the rebuild. What this proves is that the CPU
        // side never noticed: the scene, the history and the publication path
        // are exactly as they were.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyConstructionBox(2.5, 1.25, 0.625));
            return null;
        });
        settleLayout();
        final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.constructionPrimitive(primitive);
            return null;
        });
        assertEquals(2.5, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH], 0.0);
        assertTrue("and geometry is still being published",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionMeshRevision()) > 0L);
    }

    // -----------------------------------------------------------------------
    // FSR1B-17: the new controls, and the frame they must not move
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b17_everyNewProjectControlCarriesTheInteractiveFloor() {
        final Rect hostBefore = trailingHostBounds();
        openProjectSurface();
        final Rect hostOpen = trailingHostBounds();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = workspace.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            for (int id : new int[]{R.id.project_actions_button, R.id.project_save,
                    R.id.project_open, R.id.project_save_copy, R.id.project_open_file,
                    R.id.project_share_diagnostics}) {
                final View control = workspace.findViewById(id);
                assertNotNull("control " + id + " must exist by semantic id", control);
                assertTrue("control " + id + " must reach the 48 dp hit area in height,"
                                + " measured " + control.getHeight(),
                        control.getHeight() >= floor);
                assertTrue("control " + id + " must reach the 48 dp hit area in width,"
                                + " measured " + control.getWidth(),
                        control.getWidth() >= floor);
            }
            return null;
        });

        assertEquals("the accepted R2 right host must not move when the project"
                + " surface opens", hostBefore, hostOpen);
        assertEquals("nor after it is closed again", hostBefore, trailingHostBounds());
    }

    @Test
    public void fsr1b17_theRecoveryPromptControlsCarryTheInteractiveFloor() {
        // Plant a candidate and offer it, so the two answers are real, laid-out
        // views rather than a constructed-but-never-measured tree.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionBox(4.25, 2.0, 1.0);
            workspace.noteProjectMaybeDirty();
            return null;
        });
        assertTrue(onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.autosaveController().awaitIdle(15_000L)));
        final Rect hostBefore = trailingHostBounds();

        assertTrue("the candidate must be offered before it can be measured",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.offerRecoveryForTest()));
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = workspace.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            for (int id : new int[]{R.id.recovery_recover, R.id.recovery_discard}) {
                final View control = workspace.findViewById(id);
                assertNotNull("control " + id + " must exist", control);
                assertTrue("control " + id + " must reach the 48 dp floor, measured "
                        + control.getHeight(), control.getHeight() >= floor);
                assertTrue(control.getWidth() >= floor);
            }
            return null;
        });
        assertEquals("and the recovery question does not move the accepted right host",
                hostBefore, trailingHostBounds());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.recovery_discard).performClick();
            return null;
        });
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // Shared machinery
    // -----------------------------------------------------------------------

    /**
     * Waits for the render thread to settle after an injected device loss.
     *
     * <p>A bounded poll rather than a hook: the render loop is free-running and
     * has no idle signal, and inventing one for a test would put a
     * synchronisation point on the frame path. It stops as soon as either
     * permitted outcome is observed.
     *
     * @return the lifecycle it settled into
     */
    private int awaitRendererSettled(int rebuildsBefore) {
        for (int attempt = 0; attempt < 150; attempt++) {
            final int lifecycle = NativeViewport.rendererLifecycle();
            if (lifecycle == NativeViewport.RENDERER_RESTART_REQUIRED) {
                return lifecycle;
            }
            if (lifecycle == NativeViewport.RENDERER_HEALTHY
                    && NativeViewport.debugRendererDeviceRebuilds() > rebuildsBefore) {
                return lifecycle;
            }
            SystemClock.sleep(100);
        }
        return NativeViewport.rendererLifecycle();
    }

    private void buildAProjectWithDistinctiveNumbers() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionBox(7.3125, 4.8125, 2.40625);
            NativeViewport.applyBoxTransform(1.5, -0.5, 2.25, 370.0, 0.0, 0.0, 1.0, 2.0, 0.5);
            return null;
        });
        settleLayout();
    }

    private void openProjectSurface() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!workspace.projectPopover().isOpen()) {
                workspace.findViewById(R.id.project_actions_button).performClick();
            }
            return null;
        });
        settleLayout();
    }

    private Rect trailingHostBounds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View host = WorkspaceTestSupport.trailingHost(workspace);
            final int[] location = new int[2];
            host.getLocationInWindow(location);
            return new Rect(location[0], location[1], location[0] + host.getWidth(),
                    location[1] + host.getHeight());
        });
    }
}
