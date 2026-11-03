package com.forgeshape.app;

/**
 * What the Surface context surface shows, decided from native reads
 * ({@code MODELING-FOUNDATIONS-R1} C).
 *
 * <p><b>Pure and Android-free</b>, so the decisions are pinned on the JVM:
 * which Finish kinds are drawn for the sketch now open, whether Stitch is
 * drawn, which features a Thicken is offered for, what a staged value edit
 * lets the user do, and which words name a feature kind or a refusal.
 *
 * <p><b>Nothing here is truth.</b> Every verdict is the native candidate's —
 * the whole feature chain regenerated with the act applied — read fresh on
 * every refresh. A control whose candidate would be refused is ABSENT, never
 * drawn and then refused; the guard below JNI stays all the same.
 */
final class SurfacePresentation {

    private SurfacePresentation() {}

    /** The Finish kinds, in the order the surface lists them. */
    static final int[] CREATE_ORDER = {
            NativeViewport.SURFACE_CREATE_PATCH,
            NativeViewport.SURFACE_CREATE_EXTRUDE,
            NativeViewport.SURFACE_CREATE_REVOLVE,
            NativeViewport.SURFACE_CREATE_LOFT,
            NativeViewport.SURFACE_CREATE_TRIM,
            NativeViewport.SURFACE_CREATE_SECTION,
    };

    /** Whether a Surface sketch is open, from {@code surfaceSketchState}. */
    static boolean sketchOpen(double[] sketchState) {
        return sketchState[NativeViewport.SURFACE_SKETCH_ACTIVE] != 0.0;
    }

    /** The native verdict a Finish as {@code kind} would get now. */
    static int createVerdict(double[] sketchState, int kind) {
        return (int) sketchState[NativeViewport.SURFACE_SKETCH_VERDICT + kind];
    }

    /** A Finish kind is drawn exactly when its candidate would be committed. */
    static boolean createShown(double[] sketchState, int kind) {
        return sketchOpen(sketchState) && createVerdict(sketchState, kind) == NativeViewport.SURFACE_OK;
    }

    /** Whether ANY Finish kind can succeed; when none can, the surface says why. */
    static boolean anyCreateShown(double[] sketchState) {
        for (int kind : CREATE_ORDER) {
            if (createShown(sketchState, kind)) {
                return true;
            }
        }
        return false;
    }

    /**
     * The one refusal the surface names when nothing can be made: Revolve's
     * missing axis first (the one a user can fix by marking a line
     * Construction), otherwise the Patch verdict, otherwise the Extrude one.
     */
    static int blockingVerdict(double[] sketchState) {
        if (createVerdict(sketchState, NativeViewport.SURFACE_CREATE_REVOLVE)
                == NativeViewport.SURFACE_AXIS_UNRESOLVED
                && createVerdict(sketchState, NativeViewport.SURFACE_CREATE_PATCH) != NativeViewport.SURFACE_OK
                && createVerdict(sketchState, NativeViewport.SURFACE_CREATE_EXTRUDE) != NativeViewport.SURFACE_OK) {
            return NativeViewport.SURFACE_AXIS_UNRESOLVED;
        }
        final int patch = createVerdict(sketchState, NativeViewport.SURFACE_CREATE_PATCH);
        return patch != NativeViewport.SURFACE_OK ? patch
                : createVerdict(sketchState, NativeViewport.SURFACE_CREATE_EXTRUDE);
    }

    /** Which value a Finish kind carries: a length, an angle, or none. */
    static final int VALUE_NONE = 0;
    static final int VALUE_LENGTH = 1;
    static final int VALUE_ANGLE = 2;

    static int createValue(int kind) {
        if (kind == NativeViewport.SURFACE_CREATE_EXTRUDE) {
            return VALUE_LENGTH;
        }
        return kind == NativeViewport.SURFACE_CREATE_REVOLVE ? VALUE_ANGLE : VALUE_NONE;
    }

    /** A timeline row's value kind: a feature's distance, angle or thickness, a sketch's offset. */
    static int rowValue(FeatureHistoryPresentation.Row row) {
        if (row.isSketch()) {
            return VALUE_LENGTH;
        }
        if (row.featureKind == NativeViewport.SURFACE_KIND_REVOLVE) {
            return VALUE_ANGLE;
        }
        return row.featureKind == NativeViewport.SURFACE_KIND_EXTRUDE
                || row.featureKind == NativeViewport.SURFACE_KIND_THICKEN ? VALUE_LENGTH : VALUE_NONE;
    }

    /** The native value target a row edits. */
    static int valueTarget(FeatureHistoryPresentation.Row row) {
        return row.isSketch() ? NativeViewport.SURFACE_VALUE_SKETCH : NativeViewport.SURFACE_VALUE_FEATURE;
    }

    /** Stitch is drawn when its candidate would succeed. */
    static boolean stitchShown(int stitchVerdict) {
        return stitchVerdict == NativeViewport.SURFACE_OK;
    }

    /** Thicken is offered for a live feature when its candidate would succeed. */
    static boolean thickenShown(int thickenVerdict) {
        return thickenVerdict == NativeViewport.SURFACE_OK;
    }

    /**
     * A staged value edit: Apply is drawn only when the staged chain
     * regenerates AND the value differs from the committed one. A failure
     * downstream of the edit names its row; Fix keeps the edit open and Cancel
     * drops it -- neither writes anything.
     */
    static boolean applyShown(FeatureHistoryPresentation.Model staged, double committed, double typed) {
        return staged.status == NativeViewport.SURFACE_OK && staged.failedRow() == null
                && Double.compare(committed, typed) != 0;
    }

    /** The issue a staged edit raises, or null when it regenerates. */
    static FeatureHistoryPresentation.Row stagedFailure(FeatureHistoryPresentation.Model staged) {
        return staged.status == NativeViewport.SURFACE_OK ? null : staged.failedRow();
    }

    /** A feature kind's name resource. */
    static int kindName(int kind) {
        switch (kind) {
            case NativeViewport.SURFACE_KIND_PATCH: return R.string.surface_kind_patch;
            case NativeViewport.SURFACE_KIND_EXTRUDE: return R.string.surface_kind_extrude;
            case NativeViewport.SURFACE_KIND_REVOLVE: return R.string.surface_kind_revolve;
            case NativeViewport.SURFACE_KIND_LOFT: return R.string.surface_kind_loft;
            case NativeViewport.SURFACE_KIND_TRIM: return R.string.surface_kind_trim;
            case NativeViewport.SURFACE_KIND_STITCH: return R.string.surface_kind_stitch;
            case NativeViewport.SURFACE_KIND_THICKEN: return R.string.surface_kind_thicken;
            default: return R.string.surface_kind_unknown;
        }
    }

    /** A Finish kind's button label resource. */
    static int createLabel(int kind) {
        switch (kind) {
            case NativeViewport.SURFACE_CREATE_PATCH: return R.string.surface_create_patch;
            case NativeViewport.SURFACE_CREATE_EXTRUDE: return R.string.surface_create_extrude;
            case NativeViewport.SURFACE_CREATE_REVOLVE: return R.string.surface_create_revolve;
            case NativeViewport.SURFACE_CREATE_LOFT: return R.string.surface_create_loft;
            case NativeViewport.SURFACE_CREATE_TRIM: return R.string.surface_create_trim;
            default: return R.string.surface_create_section;
        }
    }

    /** A Finish kind's stable control id. */
    static int createId(int kind) {
        switch (kind) {
            case NativeViewport.SURFACE_CREATE_PATCH: return R.id.surface_create_patch;
            case NativeViewport.SURFACE_CREATE_EXTRUDE: return R.id.surface_create_extrude;
            case NativeViewport.SURFACE_CREATE_REVOLVE: return R.id.surface_create_revolve;
            case NativeViewport.SURFACE_CREATE_LOFT: return R.id.surface_create_loft;
            case NativeViewport.SURFACE_CREATE_TRIM: return R.id.surface_create_trim;
            default: return R.id.surface_create_section;
        }
    }

    /**
     * The words for a refusal the user can act on; anything else is named by
     * its native token through {@link R.string#surface_refused_other}.
     */
    static int refusalMessage(String token) {
        switch (token) {
            case "AxisUnresolved": return R.string.surface_refused_axis;
            case "ProfileCrossesAxis": return R.string.surface_refused_crosses_axis;
            case "SectionMissing": return R.string.surface_refused_section_missing;
            case "LoftSectionMismatch": return R.string.surface_refused_loft_mismatch;
            case "LoftSectionCount": return R.string.surface_refused_loft_count;
            case "TrimTargetMissing": return R.string.surface_refused_trim_target;
            case "TrimNotCoplanar": return R.string.surface_refused_trim_coplanar;
            case "TrimUnsupportedTarget": return R.string.surface_refused_trim_unsupported;
            case "TrimRemovesPatch": return R.string.surface_refused_trim_removes;
            case "StitchGapTooLarge": return R.string.surface_refused_stitch_gap;
            case "StitchNonManifold": return R.string.surface_refused_stitch_non_manifold;
            case "StitchIncompatibleBoundary": return R.string.surface_refused_stitch_incompatible;
            case "StitchNoCompatibleEdges": return R.string.surface_refused_stitch_none;
            case "NothingToStitch": return R.string.surface_refused_nothing_to_stitch;
            case "ThickenUnsupportedForSurfaceType": return R.string.surface_refused_thicken_type;
            case "CurveChainForked": return R.string.surface_refused_forked;
            case "RegionInvalid": return R.string.surface_refused_region;
            case "CurveSectionEmpty": return R.string.surface_refused_no_curves;
            default: return R.string.surface_refused_other;
        }
    }
}
