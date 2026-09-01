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
 * <p>Scope: root-level bodies only, with add and select. There is deliberately
 * no delete, duplicate, rename, hide, lock, group, nesting or reorder, and no
 * speculative parent field anywhere.
 *
 * <p><b>Its {@code +} creates nothing by itself.</b> It opens the Add Primitive
 * palette, and a body exists only once a shape has been chosen there. A control
 * that silently appended a default box is what made creation read as an
 * administrative operation on a list rather than as a choice about the model —
 * and it also meant the first thing a user did after creating a body was
 * change what it was.
 *
 * <p><b>And in Sculpt the {@code +} is not drawn at all.</b> See
 * {@link #showCreationAvailable}: creation is refused below JNI while sculpting,
 * so offering it here could only ever lead the user through a palette to a
 * refusal.
 */
final class ObjectsSectionView extends LinearLayout {

    private final InspectorHost host;
    private final LinearLayout list;

    /**
     * The column host's {@code +}.
     *
     * <p>Held so it can be withdrawn in Sculpt, where {@code sceneAddBody()}
     * refuses. It is the same rule the Objects capsule follows and for the same
     * reason: a creation path that must fail is worse than an absent one. The
     * two hosts are kept in step by the one refresh the workspace runs, so a
     * window that has a column and a window that has a capsule cannot disagree
     * about whether creation is offered.
     */
    private final TextView add;

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

        // An anchor, not an act, and deliberately QUIETER than the rows above
        // it: the list is what this surface is about, and a filled bordered
        // button beside borderless rows made it outweigh its own subject.
        //
        // This is the column host's `+`. The Objects capsule carries the other
        // one, and both open the same one palette — which is why the palette is
        // the host's and this only reports which control was pressed.
        add = EditorControlStyles.secondaryActionChip(context, R.id.add_body,
                context.getString(R.string.add_body));
        add.setCompoundDrawablesRelativeWithIntrinsicBounds(R.drawable.ic_add, 0, 0, 0);
        add.setCompoundDrawablePadding(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small));
        add.setCompoundDrawableTintList(EditorControlStyles.contentTint(context));
        add.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                host.onAddPrimitiveRequested(v);
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
            final TextView row = EditorControlStyles.listRow(getContext(), R.id.object_row,
                    BodyLabels.of(getContext(), objectId));
            // The row's identity, and what a test selects it by. Never its
            // index and never where it happens to sit on screen.
            row.setTag(Long.valueOf(objectId));
            EditorControlStyles.setListRowActive(row, objectId == activeId);
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

    /**
     * Shows or withdraws the {@code +}, according to whether creation is
     * possible at all right now.
     *
     * <p>The same rule and the same reason as the Objects capsule's: in Sculpt
     * the scene is worth seeing and creation is refused, so the list stays and
     * the creation control does not.
     */
    void showCreationAvailable(boolean available) {
        add.setVisibility(available ? VISIBLE : GONE);
    }

    /** Whether the {@code +} is currently offered, for verification. */
    boolean creationAvailable() {
        return add.getVisibility() == VISIBLE;
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

    private void onBodySelected(long objectId) {
        if (NativeViewport.sceneSelectBody(objectId) != NativeViewport.SCULPT_OK) {
            return;  // refused (unknown id, or Sculpt mode owns the target)
        }
        // Selection only: nothing was published and no revision was minted, but
        // every surface must now read the newly active body.
        host.onNativeStateChanged();
        host.showStatus(getContext().getString(R.string.status_body_selected,
                BodyLabels.of(getContext(), objectId)), R.attr.fsTextSecondary);
    }
}
