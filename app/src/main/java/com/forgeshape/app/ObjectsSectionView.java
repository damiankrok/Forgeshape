package com.forgeshape.app;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The Objects section: one row per body, plus <b>Add Body</b>.
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
 * <p>Scope: root-level bodies only, with add, select and — since `UI-OWNER-45` —
 * delete. There is deliberately no duplicate, rename, hide, lock, group, nesting
 * or reorder, and no speculative parent field anywhere.
 *
 * <p><b>Delete is a second target, never a second meaning for the first.</b> A
 * row is a pair: the label selects, and the control beside it removes. The two
 * never share a gesture, so choosing a body cannot destroy it. See
 * {@link #onBodyDeleted} for why it is not confirmed.
 *
 * <p><b>Its {@code +} creates nothing by itself.</b> It opens the Add Primitive
 * palette, and a body exists only once a shape has been chosen there — a
 * choice about the model, not an administrative operation on a list.
 *
 * <p><b>In Sculpt and while sketching the {@code +} is not drawn at all.</b>
 * See {@link #showCreationAvailable}: creation is refused below JNI there, so
 * offering it could only lead the user through a palette to a refusal.
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

    /**
     * Whether the mode allows deletion at all right now.
     *
     * <p>Held rather than re-asked, because the rows are rebuilt on every
     * refresh and the workspace sets this after that rebuild. Defaults to true:
     * Construction is the resting mode, and the scene's own size still decides
     * whether any row draws the control.
     */
    private boolean deletionAvailable = true;

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
     *
     * <p>Each row is a <b>pair</b>: the label, which selects, and the Delete
     * control beside it, which removes. Two targets rather than one gesture with
     * two meanings, because a list where the tap that chooses a thing sits
     * anywhere near the tap that destroys it is a list people stop trusting. The
     * label keeps the {@code object_row} id, the tag and the activated state it
     * has always had — the pair is a plain container with no id of its own — so
     * every existing caller that reaches a row still gets the label.
     */
    void refreshFromNative() {
        final int count = NativeViewport.sceneBodyCount();
        if (idBuffer.length < count) {
            idBuffer = new long[count];
        }
        final int written = NativeViewport.sceneBodyIds(idBuffer);
        final long activeId = NativeViewport.sceneActiveBodyId();
        // The domain refuses to remove the last body, so the control is not
        // drawn on the last body's row. A control that cannot succeed is not
        // drawn; the guard below JNI stays regardless.
        final boolean canDeleteAny = written > 1;

        list.removeAllViews();
        final Context context = getContext();
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        for (int i = 0; i < written; i++) {
            final long objectId = idBuffer[i];
            final LinearLayout pair = new LinearLayout(context);
            pair.setOrientation(HORIZONTAL);
            pair.setGravity(Gravity.CENTER_VERTICAL);

            final TextView row = EditorControlStyles.listRow(context, R.id.object_row,
                    BodyLabels.of(context, objectId));
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
            final LinearLayout.LayoutParams labelParams =
                    new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            pair.addView(row, labelParams);

            if (canDeleteAny) {
                final TextView label = row;
                final ImageView delete = EditorControlStyles.iconButton(context,
                        R.id.object_row_delete, R.drawable.ic_delete,
                        context.getString(R.string.delete_body, label.getText()));
                // The one destructive control in the Objects surface wears the
                // product's existing error colour rather than the ordinary
                // content tint, which is the whole of the destructive visual
                // language here — no red box, no second shape.
                delete.setImageTintList(ColorStateList.valueOf(
                        EditorControlStyles.themeColor(context, R.attr.fsTextError)));
                delete.setTag(Long.valueOf(objectId));
                delete.setVisibility(deletionAvailable ? VISIBLE : GONE);
                delete.setOnClickListener(new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        onBodyDeleted(objectId);
                    }
                });
                pair.addView(delete, EditorControlStyles.iconButtonParams(context, gap));
            }

            final LinearLayout.LayoutParams params =
                    EditorControlStyles.rowParams(i == 0 ? 0 : gap);
            params.width = ViewGroup.LayoutParams.MATCH_PARENT;
            list.addView(pair, params);
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

    /**
     * Shows or withdraws every row's Delete, according to whether deletion is
     * possible at all right now.
     *
     * <p>The same rule and the same reason as the {@code +} above: deletion is
     * refused below JNI while sculpting — the Sculpt target is fixed for the
     * duration of the mode, and Undo is refused there too, so a delete made
     * there could not be taken back until the user left. The list stays; the
     * destructive control does not.
     *
     * <p>The other half of the answer is the scene's own size and is not here:
     * the last body's row draws no Delete at all, because the domain refuses to
     * remove it.
     */
    void showDeletionAvailable(boolean available) {
        deletionAvailable = available;
        for (int i = 0; i < list.getChildCount(); i++) {
            final View delete = list.getChildAt(i).findViewById(R.id.object_row_delete);
            if (delete != null) {
                delete.setVisibility(available ? VISIBLE : GONE);
            }
        }
    }

    /**
     * The row LABEL for a given body, or {@code null}.
     *
     * <p>Found by tag, never by index. It returns the label rather than the pair
     * that holds it because the label is what {@code object_row} names, what
     * carries the activated state and what a tap on the row does — the pair is
     * layout.
     */
    View rowFor(long objectId) {
        for (int i = 0; i < list.getChildCount(); i++) {
            final View label = list.getChildAt(i).findViewById(R.id.object_row);
            if (label != null && Long.valueOf(objectId).equals(label.getTag())) {
                return label;
            }
        }
        return null;
    }

    /**
     * The Delete this row OFFERS for a given body, or {@code null} when it
     * offers none.
     *
     * <p>Deliberately one answer for both withdrawals. The last remaining body's
     * row never builds the control, and every row hides it while sculpting; both
     * mean the same thing to a user and to a caller — Delete is not available
     * here — so a view that is present but {@code GONE} is reported the same way
     * as one that was never built.
     */
    View deleteControlFor(long objectId) {
        for (int i = 0; i < list.getChildCount(); i++) {
            final View delete = list.getChildAt(i).findViewById(R.id.object_row_delete);
            if (delete != null && delete.getVisibility() == VISIBLE
                    && Long.valueOf(objectId).equals(delete.getTag())) {
                return delete;
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

    /**
     * Removes one body from the project.
     *
     * <p><b>Unconfirmed, deliberately.</b> The product confirms exactly one act
     * — Reset Sculpt from Shape — and confirms it because it genuinely cannot be
     * undone. This one is one Undo away, the history capsule is on the same
     * screen, and the verdict written below says so. A dialog in front of a
     * reversible act is what trains a user to dismiss the dialog in front of the
     * irreversible one.
     *
     * <p>The label is read BEFORE the delete, because afterwards the body is
     * gone and {@link BodyLabels} would have nothing to name.
     */
    private void onBodyDeleted(long objectId) {
        final Context context = getContext();
        final String label = BodyLabels.of(context, objectId);
        final int status = NativeViewport.sceneDeleteBody(objectId);
        if (status != NativeViewport.DELETE_OK) {
            // Every one of these is a refusal that changed nothing at all, and
            // the message says so. The last-body case gets its own words because
            // it is the only one a user can reason about.
            host.showStatus(context.getString(
                    status == NativeViewport.DELETE_REFUSED_LAST_BODY
                            ? R.string.status_body_delete_refused_last
                            : R.string.status_body_delete_failed),
                    R.attr.fsTextError);
            return;
        }
        // The scene, the active body and the history have all moved; every
        // surface re-reads them from the one native answer.
        host.onNativeStateChanged();
        host.showStatus(context.getString(R.string.status_body_deleted, label),
                R.attr.fsTextSecondary);
    }
}
