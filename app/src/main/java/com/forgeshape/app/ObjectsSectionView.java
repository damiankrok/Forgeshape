package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The Objects section: one row per Construction Body, plus <b>Add Body</b>.
 *
 * <p>This view holds <b>no</b> model state. It does not remember which body is
 * selected, how many exist or what their ids are: every refresh re-reads all of
 * that from native code. That is what keeps the Objects list, the Property
 * Inspector and the viewport from ever disagreeing — a tap in the viewport and a
 * tap on a row both end up changing the same one native fact, and both surfaces
 * then read it back.
 *
 * <p>Rows all carry the same {@code object_row} id, because a distinct id per
 * body cannot exist at build time when bodies are created at run time. Which
 * body a row means is its {@link View#getTag() tag}: the {@code Long} ObjectId.
 * Tests select a row by that tag, never by position on screen.
 *
 * <p>Stage 017 scope: root-level bodies only, with add and select. There is
 * deliberately no delete, duplicate, rename, hide, lock, group, nesting or
 * reorder, and no speculative parent field anywhere.
 */
final class ObjectsSectionView extends LinearLayout {

    private final InspectorHost host;
    private final LinearLayout list;

    /** Scratch buffer for the native id read; grown only when the scene grows. */
    private long[] idBuffer = new long[8];

    ObjectsSectionView(Context context, final InspectorHost host) {
        super(context);
        this.host = host;
        setId(R.id.objects_section);
        setOrientation(VERTICAL);

        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.objects)),
                EditorControlStyles.rowParams(0));

        list = new LinearLayout(context);
        list.setId(R.id.objects_list);
        list.setOrientation(VERTICAL);
        addView(list, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        final TextView add = EditorControlStyles.chip(context, R.id.add_body,
                context.getString(R.string.add_body));
        add.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onAddBodyRequested();
            }
        });
        final LinearLayout.LayoutParams addParams = EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small));
        addParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        addView(add, addParams);

        refreshFromNative();
    }

    /**
     * Rebuilds the rows from native scene state.
     *
     * <p>Rebuilt rather than diffed: the list is a handful of rows, and a
     * rebuild cannot leave a stale row pointing at a body that no longer holds
     * that position. Nothing here publishes a mesh or mints a revision.
     */
    void refreshFromNative() {
        final int count = NativeViewport.sceneBodyCount();
        if (idBuffer.length < count) {
            idBuffer = new long[count];
        }
        final int written = NativeViewport.sceneBodyIds(idBuffer);
        final long activeId = NativeViewport.sceneActiveBodyId();

        list.removeAllViews();
        final int gap = EditorControlStyles.dimen(getContext(), R.dimen.row_gap_small);
        for (int i = 0; i < written; i++) {
            final long objectId = idBuffer[i];
            final TextView row = EditorControlStyles.chip(getContext(), R.id.object_row,
                    getContext().getString(R.string.body_label, objectId));
            // The row's identity, and what a test selects it by. Never its
            // index and never where it happens to sit on screen.
            row.setTag(Long.valueOf(objectId));
            EditorControlStyles.setChipActive(row, objectId == activeId);
            row.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    onBodySelected(objectId);
                }
            });
            final LinearLayout.LayoutParams params =
                    EditorControlStyles.rowParams(i == 0 ? 0 : gap);
            params.width = ViewGroup.LayoutParams.MATCH_PARENT;
            list.addView(row, params);
        }
    }

    /** The row for a given body, or {@code null}. Found by tag, never by index. */
    View rowFor(long objectId) {
        for (int i = 0; i < list.getChildCount(); i++) {
            final View child = list.getChildAt(i);
            if (Long.valueOf(objectId).equals(child.getTag())) {
                return child;
            }
        }
        return null;
    }

    /** How many rows are on screen. */
    int rowCount() {
        return list.getChildCount();
    }

    private void onAddBodyRequested() {
        final long created = NativeViewport.sceneAddBody();
        if (created == 0L) {
            // The only refusal is "not while sculpting", which the Objects
            // section is not normally reachable in; say so rather than silently
            // doing nothing.
            host.showStatus(getContext().getString(R.string.status_nothing_frozen),
                    R.color.text_error);
            return;
        }
        // Native code already selected the new body; the whole workspace
        // re-reads, so the Inspector shows the new body's own parameters.
        host.onNativeStateChanged();
        host.showStatus(getContext().getString(R.string.status_body_added,
                getContext().getString(R.string.body_label, created)), R.color.text_success);
    }

    private void onBodySelected(long objectId) {
        if (NativeViewport.sceneSelectBody(objectId) != NativeViewport.SCULPT_OK) {
            return;  // refused (unknown id, or Sculpt mode owns the target)
        }
        // Selection only: nothing was published and no revision was minted, but
        // every surface must now read the newly active body.
        host.onNativeStateChanged();
        host.showStatus(getContext().getString(R.string.status_body_selected,
                getContext().getString(R.string.body_label, objectId)), R.color.text_secondary);
    }
}
