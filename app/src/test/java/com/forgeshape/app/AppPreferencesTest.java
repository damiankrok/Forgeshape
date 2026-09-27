package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * UIPREFR1-03/04/07/08/40 on the JVM: the preference model's rules, checked
 * without a device.
 *
 * <p>What matters here is not where the values live — that is the store's
 * business and a device proves it — but the rules the model itself has to
 * obey: the defaults reproduce the product exactly, an unknown name is the
 * default, a non-finite number is the default, an out-of-range number is
 * clamped, and the model carries exactly the five approved preferences.
 */
public final class AppPreferencesTest {

    @Test
    public void theDefaultsAreTheProductAsItShipped() {
        final AppPreferences defaults = AppPreferences.defaults();
        assertSame(AppTheme.WARM_GRAPHITE, defaults.palette());
        assertSame(Handedness.RIGHT, defaults.handedness());
        assertEquals(1.0f, defaults.gizmoVisualScale(), 0.0f);
        assertSame(GizmoStrokeWeight.REGULAR, defaults.gizmoStrokeWeight());
        assertFalse("Tool Labels is off: icons only", defaults.toolLabels());
        assertEquals(1, AppPreferences.SCHEMA_VERSION);
    }

    @Test
    public void missingKeysAreTheExactDefaults() {
        final AppPreferences read = AppPreferences.fromStored(
                AppPreferences.SCHEMA_VERSION, null, null, Float.NaN, null, null);
        assertEquals(AppPreferences.defaults(), read);
    }

    @Test
    public void anUnknownEnumNameFallsBackToItsDefault() {
        final AppPreferences read = AppPreferences.fromStored(AppPreferences.SCHEMA_VERSION,
                "NEON_PINK", "AMBIDEXTROUS", 1.25f, "HAIRLINE", null);
        assertSame(AppTheme.WARM_GRAPHITE, read.palette());
        assertSame(Handedness.RIGHT, read.handedness());
        assertSame(GizmoStrokeWeight.REGULAR, read.gizmoStrokeWeight());
        assertEquals("a valid neighbour is kept", 1.25f, read.gizmoVisualScale(), 0.0f);
    }

    @Test
    public void everyStoredNameRoundTrips() {
        for (AppTheme palette : AppTheme.values()) {
            assertSame(palette, AppTheme.fromStoredName(palette.name()));
        }
        for (Handedness handedness : Handedness.values()) {
            assertSame(handedness, Handedness.fromStoredName(handedness.name()));
        }
        for (GizmoStrokeWeight weight : GizmoStrokeWeight.values()) {
            assertSame(weight, GizmoStrokeWeight.fromStoredName(weight.name()));
        }
    }

    @Test
    public void aNonFiniteSizeIsTheDefaultAndAnOutOfRangeSizeIsClamped() {
        assertEquals(1.0f, AppPreferences.clampGizmoVisualScale(Float.NaN), 0.0f);
        assertEquals(1.0f, AppPreferences.clampGizmoVisualScale(Float.POSITIVE_INFINITY), 0.0f);
        assertEquals(1.0f, AppPreferences.clampGizmoVisualScale(Float.NEGATIVE_INFINITY), 0.0f);
        assertEquals(AppPreferences.GIZMO_VISUAL_SCALE_MIN,
                AppPreferences.clampGizmoVisualScale(0.1f), 0.0f);
        assertEquals(AppPreferences.GIZMO_VISUAL_SCALE_MIN,
                AppPreferences.clampGizmoVisualScale(-4.0f), 0.0f);
        assertEquals(AppPreferences.GIZMO_VISUAL_SCALE_MAX,
                AppPreferences.clampGizmoVisualScale(9.0f), 0.0f);
        assertEquals(1.25f, AppPreferences.clampGizmoVisualScale(1.25f), 0.0f);
        // The model cannot HOLD a value outside the range, whichever way it
        // arrives.
        assertEquals(AppPreferences.GIZMO_VISUAL_SCALE_MAX,
                AppPreferences.defaults().withGizmoVisualScale(100.0f).gizmoVisualScale(), 0.0f);
        assertEquals(1.0f, AppPreferences.defaults().withGizmoVisualScale(Float.NaN)
                .gizmoVisualScale(), 0.0f);
    }

    @Test
    public void theBoundsAndPresetsAgreeWithTheDomain() {
        // The domain's constants, restated in NativeViewport for the JNI seam
        // and here for the JVM. All three must say the same thing.
        assertEquals(NativeViewport.GIZMO_VISUAL_SCALE_MIN,
                AppPreferences.GIZMO_VISUAL_SCALE_MIN, 0.0f);
        assertEquals(NativeViewport.GIZMO_VISUAL_SCALE_DEFAULT,
                AppPreferences.GIZMO_VISUAL_SCALE_DEFAULT, 0.0f);
        assertEquals(NativeViewport.GIZMO_VISUAL_SCALE_MAX,
                AppPreferences.GIZMO_VISUAL_SCALE_MAX, 0.0f);
        final float[] presets = AppPreferences.GIZMO_VISUAL_SCALE_PRESETS;
        assertEquals("the two bounds, the default and one step above it", 4, presets.length);
        assertEquals(AppPreferences.GIZMO_VISUAL_SCALE_MIN, presets[0], 0.0f);
        assertEquals(AppPreferences.GIZMO_VISUAL_SCALE_DEFAULT, presets[1], 0.0f);
        assertEquals(AppPreferences.GIZMO_VISUAL_SCALE_MAX, presets[presets.length - 1], 0.0f);
        for (int i = 1; i < presets.length; i++) {
            assertTrue("presets ascend", presets[i] > presets[i - 1]);
            assertEquals("every preset is inside the range, so none is clamped",
                    presets[i], AppPreferences.clampGizmoVisualScale(presets[i]), 0.0f);
        }
    }

    @Test
    public void aNullChoiceIsTheDefaultNeverAHole() {
        final AppPreferences value = AppPreferences.defaults()
                .withPalette(null).withHandedness(null).withGizmoStrokeWeight(null);
        assertEquals(AppPreferences.defaults(), value);
    }

    @Test
    public void changingOneFieldLeavesTheOthersAlone() {
        final AppPreferences base = AppPreferences.defaults()
                .withPalette(AppTheme.COOL_LIGHT).withHandedness(Handedness.LEFT)
                .withGizmoVisualScale(1.25f).withGizmoStrokeWeight(GizmoStrokeWeight.BOLD)
                .withToolLabels(true);
        final AppPreferences changed = base.withPalette(AppTheme.WARM_LIGHT);
        assertSame(AppTheme.WARM_LIGHT, changed.palette());
        assertSame(Handedness.LEFT, changed.handedness());
        assertEquals(1.25f, changed.gizmoVisualScale(), 0.0f);
        assertSame(GizmoStrokeWeight.BOLD, changed.gizmoStrokeWeight());
        assertTrue("and Tool Labels survives a palette change", changed.toolLabels());
        assertNotEquals(base, changed);
        assertEquals(base, changed.withPalette(AppTheme.COOL_LIGHT));
        assertEquals(base.hashCode(), changed.withPalette(AppTheme.COOL_LIGHT).hashCode());
    }

    @Test
    public void aFutureSchemaIsReadWithThisBuildsRules() {
        // A newer build may write a higher version and keys this one does not
        // know. The known fields are read by their own rules and nothing throws.
        final AppPreferences read = AppPreferences.fromStored(99, "COOL_LIGHT", "LEFT", 1.5f,
                "THIN", Boolean.TRUE);
        assertSame(AppTheme.COOL_LIGHT, read.palette());
        assertSame(Handedness.LEFT, read.handedness());
        assertEquals(1.5f, read.gizmoVisualScale(), 0.0f);
        assertSame(GizmoStrokeWeight.THIN, read.gizmoStrokeWeight());
        assertTrue(read.toolLabels());
    }

    @Test
    public void thereAreExactlyTheApprovedPreferencesAndNoOthers() {
        // UIPREFR1-40: five palettes, two handednesses, three weights, one
        // bounded size — and since CAD-VERTICAL-SLICE-R1 one boolean, Tool
        // Labels. A handle style is deliberately NOT here: the gizmo's
        // renderer contract draws one-pixel line lists and no second style
        // exists that shares its hit semantics, so the stage's deviation
        // GIZMO_STYLE_DEFERRED_BY_RENDERER_CONTRACT stands instead of a fake row.
        assertEquals(5, AppTheme.values().length);
        assertEquals(2, Handedness.values().length);
        assertEquals(3, GizmoStrokeWeight.values().length);
        assertFalse(AppPreferences.defaults().toString().contains("handleStyle"));
        // The field set, stated by the value's own description: exactly these
        // five and no sixth.
        assertEquals("AppPreferences{palette=WARM_GRAPHITE, handedness=RIGHT,"
                        + " gizmoVisualScale=1.0, gizmoStrokeWeight=REGULAR, toolLabels=false}",
                AppPreferences.defaults().toString());
    }

    // -----------------------------------------------------------------------
    // Tool Labels (CAD-VERTICAL-SLICE-R1)
    // -----------------------------------------------------------------------

    @Test
    public void toolLabelsDefaultsToOffAndAMissingKeyIsThatDefault() {
        assertFalse(AppPreferences.TOOL_LABELS_DEFAULT);
        assertFalse(AppPreferences.defaults().toolLabels());
        // A record written before the field existed has no such key; the store
        // reads that (and a key of the wrong type) as null.
        assertFalse(AppPreferences.fromStored(AppPreferences.SCHEMA_VERSION, "COOL_LIGHT",
                "LEFT", 1.25f, "BOLD", null).toolLabels());
        assertEquals("adding the field needed no schema bump", 1, AppPreferences.SCHEMA_VERSION);
    }

    @Test
    public void toolLabelsRoundTripsThroughTheStoredForm() {
        for (boolean stored : new boolean[]{false, true}) {
            final AppPreferences written = AppPreferences.defaults().withToolLabels(stored);
            // What the store writes for each field, read back through the one
            // reading path.
            final AppPreferences read = AppPreferences.fromStored(AppPreferences.SCHEMA_VERSION,
                    written.palette().name(), written.handedness().name(),
                    written.gizmoVisualScale(), written.gizmoStrokeWeight().name(),
                    Boolean.valueOf(written.toolLabels()));
            assertEquals(written, read);
            assertEquals(stored, read.toolLabels());
        }
    }

    @Test
    public void withToolLabelsChangesThatFieldAndNothingElse() {
        final AppPreferences base = AppPreferences.defaults()
                .withPalette(AppTheme.NEUTRAL_CHARCOAL).withHandedness(Handedness.LEFT)
                .withGizmoVisualScale(1.5f).withGizmoStrokeWeight(GizmoStrokeWeight.THIN);
        final AppPreferences on = base.withToolLabels(true);
        assertTrue(on.toolLabels());
        assertSame(AppTheme.NEUTRAL_CHARCOAL, on.palette());
        assertSame(Handedness.LEFT, on.handedness());
        assertEquals(1.5f, on.gizmoVisualScale(), 0.0f);
        assertSame(GizmoStrokeWeight.THIN, on.gizmoStrokeWeight());
        assertNotEquals("the field takes part in equality", base, on);
        assertEquals(base, on.withToolLabels(false));
        assertEquals(base.hashCode(), on.withToolLabels(false).hashCode());
        assertEquals("setting what is already held is the same value", on, on.withToolLabels(true));
    }
}
