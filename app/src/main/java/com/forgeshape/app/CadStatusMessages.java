package com.forgeshape.app;

import android.content.Context;

/**
 * What the status line says about a CAD refusal.
 *
 * <p>One place, so the sketch editor, the CAD editor and the workspace's own
 * transitions all describe the same refusal in the same words. The exact
 * reason is always in the log by name ({@code cadStatusToken}); these are the
 * short user-facing forms, and the ones with no sentence of their own fall back
 * to the name.
 *
 * <p>Holds no state and reads none.
 */
final class CadStatusMessages {

    private CadStatusMessages() {}

    static String describe(Context context, int code) {
        switch (code) {
            case NativeViewport.CAD_OPEN_PROFILE:
                return context.getString(R.string.status_cad_open_profile);
            case NativeViewport.CAD_SELF_INTERSECTING_PROFILE:
                return context.getString(R.string.status_cad_self_intersecting);
            case NativeViewport.CAD_NO_CLOSED_PROFILE:
                return context.getString(R.string.status_cad_no_closed_profile);
            case NativeViewport.CAD_AMBIGUOUS_PROFILE:
                return context.getString(R.string.status_cad_ambiguous_profile);
            case NativeViewport.CAD_INVALID_EXTRUDE_DEPTH:
                return context.getString(R.string.status_cad_invalid_depth);
            case NativeViewport.CAD_NESTED_PROFILE_UNSUPPORTED:
                return context.getString(R.string.status_cad_nested_profile);
            case NativeViewport.CAD_ZERO_AREA_PROFILE:
                return context.getString(R.string.status_cad_zero_area);
            case NativeViewport.CAD_BRANCHING_CHAIN:
                return context.getString(R.string.status_cad_branching);
            case NativeViewport.CAD_INVALID_ARC:
                return context.getString(R.string.status_cad_invalid_arc);
            case NativeViewport.CAD_INVALID_SPLINE:
                return context.getString(R.string.status_cad_invalid_spline);
            case NativeViewport.CAD_SKETCH_NOT_EMPTY:
                return context.getString(R.string.status_sketch_plane_fixed);
            case NativeViewport.CAD_DEPENDENT_FACE_LOST:
                return context.getString(R.string.status_cad_dependent_face_lost);
            case NativeViewport.CAD_INVALID_EXTRUDE_EXTENT:
                return context.getString(R.string.status_cad_invalid_extent);
            case NativeViewport.CAD_ZERO_LENGTH_LINE:
                return context.getString(R.string.status_cad_zero_length);
            // `CAD-VERTICAL-SLICE-R1`: regions, the feature chain and the
            // operations. Each names what the user can change, never the kernel.
            case NativeViewport.CAD_PROFILE_REGION_MISMATCH:
                return context.getString(R.string.status_cad_region_mismatch);
            case NativeViewport.CAD_OVERLAPPING_REGIONS:
                return context.getString(R.string.status_cad_overlapping_regions);
            case NativeViewport.CAD_OVERLAPPING_HOLES:
                return context.getString(R.string.status_cad_overlapping_holes);
            case NativeViewport.CAD_TOO_MANY_REGIONS:
                return context.getString(R.string.status_cad_too_many_regions);
            case NativeViewport.CAD_INVALID_FEATURE_OPERATION:
                return context.getString(R.string.status_cad_invalid_operation);
            case NativeViewport.CAD_TOO_MANY_FEATURES:
                return context.getString(R.string.status_cad_too_many_features);
            case NativeViewport.CAD_FEATURE_SUPPORT_INVALID:
                return context.getString(R.string.status_cad_feature_support_invalid);
            case NativeViewport.CAD_SUPPORT_FACE_LOST:
                return context.getString(R.string.status_cad_support_face_lost);
            case NativeViewport.CAD_OPERATION_NEEDS_TARGET:
                return context.getString(R.string.status_cad_operation_needs_target);
            case NativeViewport.CAD_ADD_DISJOINT:
                return context.getString(R.string.status_cad_add_disjoint);
            case NativeViewport.CAD_ADD_NO_EFFECT:
                return context.getString(R.string.status_cad_add_no_effect);
            case NativeViewport.CAD_CUT_NO_INTERSECTION:
                return context.getString(R.string.status_cad_cut_no_intersection);
            case NativeViewport.CAD_CUT_REMOVES_BODY:
                return context.getString(R.string.status_cad_cut_removes_body);
            case NativeViewport.CAD_KERNEL_FAILED:
                return context.getString(R.string.status_cad_kernel_failed);
            // `CAD-V6-S2-CORRECTION-FILL-HUD-R1`: why crossing curves could not
            // be split into areas.
            case NativeViewport.CAD_PLANAR_FACE_AMBIGUOUS_OVERLAP:
                return context.getString(R.string.status_cad_curves_overlap);
            case NativeViewport.CAD_PLANAR_FACE_CAP_EXCEEDED:
                return context.getString(R.string.status_cad_too_many_crossings);
            case NativeViewport.CAD_PLANAR_FACE_DEGENERATE:
                return context.getString(R.string.status_cad_degenerate_area);
            case NativeViewport.CAD_PLANAR_FACES_TOUCH_AT_POINT:
                return context.getString(R.string.status_cad_areas_touch_at_point);
            case NativeViewport.CAD_REVOLVE_NEEDS_AXIS:
                return context.getString(R.string.status_cad_revolve_needs_axis);
            case NativeViewport.CAD_REVOLVE_AXIS_NOT_STRAIGHT:
                return context.getString(R.string.status_cad_revolve_axis_not_straight);
            case NativeViewport.CAD_REVOLVE_AXIS_UNRESOLVED:
                return context.getString(R.string.status_cad_revolve_axis_unresolved);
            case NativeViewport.CAD_REVOLVE_ANGLE_INVALID:
                return context.getString(R.string.status_cad_revolve_angle_invalid);
            case NativeViewport.CAD_REVOLVE_PROFILE_CROSSES_AXIS:
                return context.getString(R.string.status_cad_revolve_crosses_axis);
            case NativeViewport.CAD_REVOLVE_ZERO_RADIUS:
                return context.getString(R.string.status_cad_revolve_zero_radius);
            case NativeViewport.CAD_REVOLVE_COMPONENTS_OVERLAP:
                return context.getString(R.string.status_cad_revolve_components_overlap);
            // `CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`: dimensions and the modify
            // tools. Each says what to change, never the arithmetic.
            case NativeViewport.CAD_SKETCH_DIMENSION_INVALID:
                return context.getString(R.string.status_sketch_dimension_invalid);
            case NativeViewport.CAD_SKETCH_DIMENSION_CONFLICT:
                return context.getString(R.string.status_sketch_dimension_conflict);
            case NativeViewport.CAD_SKETCH_DIMENSION_DEPENDENCY:
                return context.getString(R.string.status_sketch_dimension_dependency);
            case NativeViewport.CAD_SKETCH_DIMENSION_LOCKED:
                return context.getString(R.string.status_sketch_dimension_locked);
            case NativeViewport.CAD_SKETCH_DIMENSION_READ_ONLY:
                return context.getString(R.string.status_sketch_dimension_read_only);
            case NativeViewport.CAD_SKETCH_DIMENSION_VALUE_INVALID:
                return context.getString(R.string.status_sketch_dimension_value_invalid);
            case NativeViewport.CAD_TRIM_SPLINE_UNSUPPORTED:
                return context.getString(R.string.status_sketch_trim_spline);
            case NativeViewport.CAD_DRAFTING_NO_TARGET:
                return context.getString(R.string.status_sketch_no_target);
            case NativeViewport.CAD_EXTEND_UNSUPPORTED:
                return context.getString(R.string.status_sketch_extend_unsupported);
            case NativeViewport.CAD_EXTEND_NO_TARGET:
                return context.getString(R.string.status_sketch_extend_no_target);
            case NativeViewport.CAD_EXTEND_AMBIGUOUS:
                return context.getString(R.string.status_sketch_extend_ambiguous);
            case NativeViewport.CAD_OFFSET_SPLINE_UNSUPPORTED:
                return context.getString(R.string.status_sketch_offset_spline);
            case NativeViewport.CAD_OFFSET_INVALID:
                return context.getString(R.string.status_sketch_offset_invalid);
            case NativeViewport.CAD_OFFSET_MITER_LIMIT:
                return context.getString(R.string.status_sketch_offset_miter);
            case NativeViewport.CAD_OFFSET_SELF_INTERSECTING:
                return context.getString(R.string.status_sketch_offset_self_intersecting);
            case NativeViewport.CAD_MIRROR_AXIS_INVALID:
                return context.getString(R.string.status_sketch_mirror_axis_invalid);
            case NativeViewport.CAD_MIRROR_NOTHING_SELECTED:
                return context.getString(R.string.status_sketch_mirror_nothing);
            default:
                return context.getString(R.string.status_cad_refused,
                        NativeViewport.cadStatusToken(code));
        }
    }

    /**
     * A Delete refusal: a dimension that names a deleted AND a kept entity is
     * the one refusal whose sentence differs from Trim's.
     */
    static String describeDelete(Context context, int code) {
        return code == NativeViewport.CAD_SKETCH_DIMENSION_DEPENDENCY
                ? context.getString(R.string.status_sketch_delete_dependency)
                : describe(context, code);
    }

    /** What a landed sketch Delete says: the dimensions it took, when it took any. */
    static String deleted(Context context, int removedDimensions) {
        return removedDimensions > 0
                ? context.getString(R.string.status_sketch_deleted_dimensions, removedDimensions)
                : context.getString(R.string.status_sketch_entity_deleted);
    }
}
