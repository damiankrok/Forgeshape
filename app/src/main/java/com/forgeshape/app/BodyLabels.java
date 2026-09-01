package com.forgeshape.app;

import android.content.Context;

/**
 * What a body is called wherever the user reads its name.
 *
 * <p>One answer, in one place, because a body's name appears in four surfaces —
 * the Objects list, the Objects capsule, the precision surface's title and the
 * status line — and a row that read <i>Body #7</i> while the panel above it read
 * <i>head_low</i> would be two names for one object.
 *
 * <p>The rule is short. An <b>Imported Mesh</b> arrives named by the file it
 * came from, so it uses that name; losing it would leave the user with a list of
 * rows they could not tell apart. A <b>Construction Body</b> has no stored name
 * — this product has no Rename — so it is labelled from its ObjectId exactly as
 * it always was. The empty string native code returns for a body with no stored
 * name is the SIGNAL for that fallback, not a name.
 *
 * <p>Holds no state and caches nothing: every call re-reads native truth, for
 * the same reason every other surface does.
 */
final class BodyLabels {

    private BodyLabels() {}

    /** The label for one body, whichever representation it has. */
    static String of(Context context, long objectId) {
        final String stored = NativeViewport.sceneBodyName(objectId);
        return stored.isEmpty() ? context.getString(R.string.body_label, objectId) : stored;
    }

    /** The label for the body the editors currently act on. */
    static String ofActive(Context context) {
        return of(context, NativeViewport.sceneActiveBodyId());
    }
}
