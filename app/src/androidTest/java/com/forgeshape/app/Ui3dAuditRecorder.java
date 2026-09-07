package com.forgeshape.app;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.SystemClock;
import android.view.View;

import androidx.test.platform.app.InstrumentationRegistry;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * `UI-3D-STATE-AUDIT-R1`: the audit ledger every case writes into.
 *
 * <p><b>An audit records; it does not stop at the first defect.</b> The whole
 * value of this run is a COMPLETE matrix, so a visibility mismatch or a stale
 * anchor is written down as a row with a verdict rather than thrown as an
 * assertion. Only a harness precondition that would make the following rows
 * meaningless — the workspace not laid out, a journey step the domain refused —
 * is asserted, because a row measured from a state that was never reached is
 * worse than a missing row.
 *
 * <p>Two ledgers, because they answer two different questions. The
 * <b>visibility</b> ledger says whether a surface was on screen when the
 * contract matrix said it must be, must not be, or may be. The <b>spatial</b>
 * ledger says how far a world-/feature-anchored surface stood from the anchor
 * native reports for it at that instant, in pixels and in dp.
 *
 * <p>The expected anchor is always re-read from native <em>after</em> the
 * action, never remembered from before it: that is what makes a stale row a
 * measurement of staleness rather than of the test's own bookkeeping.
 *
 * <p>Nothing here is production code, nothing is compiled into the release
 * APK, and nothing it calls mutates the domain.
 */
final class Ui3dAuditRecorder {

    /** The mechanical tolerance for an anchored surface, in dp. */
    static final float TOLERANCE_DP = 4.0f;

    static final String MUST_SHOW = "MUST_SHOW";
    static final String MUST_HIDE = "MUST_HIDE";
    static final String MAY_SHOW = "MAY_SHOW";

    private static final List<String> VISIBILITY = new ArrayList<>();
    private static final List<String> SPATIAL = new ArrayList<>();
    private static final List<String> NOTES = new ArrayList<>();

    private Ui3dAuditRecorder() {
    }

    static File outDir() {
        final File dir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/ui-3d-state-audit-r1");
        //noinspection ResultOfMethodCallIgnored
        dir.mkdirs();
        return dir;
    }

    // -----------------------------------------------------------------------
    // Visibility
    // -----------------------------------------------------------------------

    /**
     * Records one visibility assertion.
     *
     * @param caseId  the `UI3D-nn` case
     * @param state   the state/action the matrix names
     * @param surface the inventory id plus the semantic id, e.g. `S19
     *                body_dimension_label_x`
     * @param expect  {@link #MUST_SHOW}, {@link #MUST_HIDE} or {@link #MAY_SHOW}
     * @param shown   what was actually on screen
     */
    static void visibility(String caseId, String state, String surface, String expect,
                           boolean shown) {
        final String verdict;
        if (MAY_SHOW.equals(expect)) {
            verdict = "PASS";
        } else if (MUST_SHOW.equals(expect)) {
            verdict = shown ? "PASS" : "FAIL";
        } else {
            verdict = shown ? "FAIL" : "PASS";
        }
        VISIBILITY.add(caseId + "\t" + state + "\t" + surface + "\t" + expect + "\t"
                + (shown ? "SHOWN" : "HIDDEN") + "\t" + verdict);
    }

    // -----------------------------------------------------------------------
    // Spatial attachment
    // -----------------------------------------------------------------------

    /**
     * Records one spatial-attachment measurement.
     *
     * @param anchorKind `WORLD_ANCHORED`, `FEATURE_ANCHORED` or
     *                   `OVERLAY_GEOMETRY`
     * @param clamped    whether the surface was deliberately edge-clamped by its
     *                   own placement rule, which makes a non-zero error correct
     *                   rather than stale
     */
    static void spatial(String caseId, String action, String surface, String anchorKind,
                        float expectX, float expectY, float actualX, float actualY,
                        float density, boolean clamped, String note) {
        final float dx = actualX - expectX;
        final float dy = actualY - expectY;
        final float px = (float) Math.sqrt(dx * dx + dy * dy);
        final float dp = px / density;
        final String verdict = clamped ? "CLAMPED"
                : (dp <= TOLERANCE_DP ? "PASS" : "FAIL");
        SPATIAL.add(caseId + "\t" + action + "\t" + surface + "\t" + anchorKind + "\t"
                + fmt(expectX) + "\t" + fmt(expectY) + "\t" + fmt(actualX) + "\t" + fmt(actualY)
                + "\t" + fmt(px) + "\t" + fmt(dp) + "\t" + verdict + "\t" + note);
    }

    /** Records that a spatial surface could not be measured, and why. */
    static void spatialUnmeasured(String caseId, String action, String surface, String anchorKind,
                                  String why) {
        SPATIAL.add(caseId + "\t" + action + "\t" + surface + "\t" + anchorKind
                + "\t-\t-\t-\t-\t-\t-\tNOT_MEASURED\t" + why);
    }

    static void note(String caseId, String text) {
        NOTES.add(caseId + "\t" + text);
    }

    // -----------------------------------------------------------------------
    // Geometry helpers
    // -----------------------------------------------------------------------

    /**
     * The visual centre of a placed chrome view, in viewport-local pixels.
     *
     * <p>The intended attachment point of every anchored surface in this
     * product is its CENTRE — {@code placeAt} in all three placing views
     * subtracts half the measured box from the anchor — so the centre is what
     * is compared, never a bounding-box corner.
     *
     * <p>Scale is folded in because the canvas extrude cluster is drawn at the
     * camera-attached multiplier with its pivot at (0, 0): its top-left does
     * not move under the scale, so its visual centre is half its SCALED box
     * past that corner.
     */
    static float[] centreOf(View shown, View viewport) {
        final int[] here = new int[2];
        final int[] port = new int[2];
        shown.getLocationInWindow(here);
        viewport.getLocationInWindow(port);
        final float w = shown.getWidth() * shown.getScaleX();
        final float h = shown.getHeight() * shown.getScaleY();
        return new float[]{here[0] - port[0] + w * 0.5f, here[1] - port[1] + h * 0.5f};
    }

    /** Whether an anchor is close enough to a window edge that clamping bites. */
    static boolean wouldClamp(View shown, View viewport, float anchorX, float anchorY) {
        final float w = shown.getWidth() * shown.getScaleX();
        final float h = shown.getHeight() * shown.getScaleY();
        final float left = anchorX - w * 0.5f;
        final float top = anchorY - h * 0.5f;
        return left < 0f || top < 0f
                || left + w > viewport.getWidth() || top + h > viewport.getHeight();
    }

    /** The placed ancestor of a leaf control: the child the container moves. */
    static View placedAncestor(View leaf, View container) {
        View walk = leaf;
        while (walk != null && walk.getParent() != container) {
            if (!(walk.getParent() instanceof View)) {
                return leaf;
            }
            walk = (View) walk.getParent();
        }
        return walk == null ? leaf : walk;
    }

    static boolean isShown(View root, int id) {
        final View found = root.findViewById(id);
        return found != null && found.isShown();
    }

    // -----------------------------------------------------------------------
    // Screenshots
    // -----------------------------------------------------------------------

    /** Captures the composed display, so the Vulkan viewport is in the frame. */
    static File capture(String name) {
        SystemClock.sleep(400);
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        return write(frame, name, null, null);
    }

    /**
     * Captures the display and draws the mechanical overlay UI3D-15 asks for:
     * a ring at the semantic anchor native reports and a cross at where the
     * surface actually attached, joined by a line.
     */
    static File captureWithOverlay(String name, float expectX, float expectY,
                                   float actualX, float actualY, int[] viewportOrigin) {
        SystemClock.sleep(400);
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        return write(frame, name, new float[]{expectX + viewportOrigin[0],
                expectY + viewportOrigin[1]}, new float[]{actualX + viewportOrigin[0],
                actualY + viewportOrigin[1]});
    }

    private static File write(Bitmap frame, String name, float[] expect, float[] actual) {
        if (frame == null) {
            note(name, "the display could not be captured");
            return null;
        }
        Bitmap canvasFrame = frame;
        if (expect != null && actual != null) {
            canvasFrame = frame.copy(Bitmap.Config.ARGB_8888, true);
            final Canvas canvas = new Canvas(canvasFrame);
            final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(4f);
            paint.setColor(Color.rgb(0, 220, 120));       // expected: the semantic anchor
            canvas.drawCircle(expect[0], expect[1], 22f, paint);
            paint.setColor(Color.rgb(255, 60, 60));       // actual: where the surface attached
            canvas.drawLine(actual[0] - 26f, actual[1], actual[0] + 26f, actual[1], paint);
            canvas.drawLine(actual[0], actual[1] - 26f, actual[0], actual[1] + 26f, paint);
            paint.setColor(Color.rgb(255, 200, 0));
            paint.setStrokeWidth(2f);
            canvas.drawLine(expect[0], expect[1], actual[0], actual[1], paint);
        }
        final File png = new File(outDir(), name + ".png");
        try (FileOutputStream out = new FileOutputStream(png)) {
            canvasFrame.compress(Bitmap.CompressFormat.PNG, 100, out);
        } catch (IOException error) {
            note(name, "could not write " + png + ": " + error);
            return null;
        }
        return png;
    }

    // -----------------------------------------------------------------------
    // Flush
    // -----------------------------------------------------------------------

    /** Writes both ledgers. Called from every case, so a crash still leaves them. */
    static void flush() {
        writeLines(new File(outDir(), "VISIBILITY.tsv"),
                "case\tstate\tsurface\texpected\tactual\tverdict", VISIBILITY);
        writeLines(new File(outDir(), "SPATIAL_ATTACHMENTS.tsv"),
                "case\taction\tsurface\tanchor_kind\texpect_x\texpect_y\tactual_x\tactual_y"
                        + "\terror_px\terror_dp\tverdict\tnote", SPATIAL);
        writeLines(new File(outDir(), "NOTES.tsv"), "case\tnote", NOTES);
    }

    private static void writeLines(File file, String header, List<String> rows) {
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            out.println(header);
            for (String row : rows) {
                out.println(row);
            }
        } catch (IOException error) {
            throw new AssertionError("could not write " + file, error);
        }
    }

    private static String fmt(float value) {
        return String.format(java.util.Locale.US, "%.2f", value);
    }
}
