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
            default:
                return context.getString(R.string.status_cad_refused,
                        NativeViewport.cadStatusToken(code));
        }
    }
}
