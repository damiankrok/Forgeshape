package com.forgeshape.app;

import android.content.Context;
import android.content.res.ColorStateList;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
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
 * <p>Scope: root-level bodies only, with add, select, delete (`UI-OWNER-45`)
 * and — since Stage 018A (`UI-OWNER-40`) — rename, show/hide, lock/unlock and
 * duplicate, plus mirror (`MIRROR-01`). There is deliberately still no group,
 * nesting, reorder, multi-select or drag and drop, and no speculative parent
 * field anywhere.
 *
 * <p><b>Delete is a second target, never a second meaning for the first.</b> A
 * row is a pair: the label selects, and the control beside it removes. The two
 * never share a gesture, so choosing a body cannot destroy it. See
 * {@link #onBodyDeleted} for why it is not confirmed.
 *
 * <p><b>The other commands live behind one overflow, inline.</b> The panel is
 * 220 dp wide, so more targets on the row would leave the label nothing, and a
 * persistent command column is the desktop shape this product does not have.
 * The overflow grows a strip out BENEATH its row — pushing the rows below
 * rather than standing over them, so it never partially covers another live
 * control — and at most one row is open at a time. Delete stays exactly where
 * {@code UI-OWNER-45} put it and its path is untouched.
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

    /**
     * Whether the mode allows the Stage 018A object commands at all right now.
     *
     * <p>The same rule and the same reason as {@link #deletionAvailable}: all
     * four are refused below JNI while sculpting and while a sketch is open,
     * because the scene holds still there. A control that must fail is worse
     * than an absent one, so the overflow is withdrawn — and the guard below
     * JNI stays regardless, because removing a control is not removing a guard.
     */
    private boolean commandsAvailable = true;

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

            // The overflow, between the label and Delete. It opens this row's
            // command strip inline and nothing else: it mutates no model fact,
            // so it is drawn whatever the mode allows, and the commands inside
            // it are what the mode governs.
            final String bodyLabel = row.getText().toString();
            final ImageView more = EditorControlStyles.iconButton(context, R.id.object_row_more,
                    R.drawable.ic_object_more,
                    context.getString(R.string.object_commands, bodyLabel));
            more.setTag(Long.valueOf(objectId));
            more.setVisibility(commandsAvailable ? VISIBLE : GONE);
            more.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    // A second tap on the same row closes it, and opening one
                    // row closes whichever other row was open: at most one
                    // strip stands at a time.
                    final boolean wasOpen = expandedBodyId == objectId;
                    expandedBodyId = wasOpen ? NO_BODY : objectId;
                    renaming = false;
                    refreshFromNative();
                }
            });
            pair.addView(more, EditorControlStyles.iconButtonParams(context, gap));

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

            // The strip, when this row is the open one. It is a SIBLING beneath
            // the row rather than a surface over it, so the rows below move
            // down and nothing is covered.
            if (expandedBodyId == objectId && commandsAvailable) {
                final View expansion;
                if (renaming) {
                    expansion = buildRenameEditor(objectId, bodyLabel);
                } else if (mirrorChoosing) {
                    expansion = buildMirrorPlaneChooser(objectId, bodyLabel);
                } else {
                    expansion = buildCommandStrip(objectId, bodyLabel);
                }
                final LinearLayout.LayoutParams expansionParams =
                        EditorControlStyles.rowParams(gap);
                expansionParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
                list.addView(expansion, expansionParams);
            }
        }
        if (!bodyStillPresent(written)) {
            // The open row is gone -- deleted, or replaced by a load. Nothing
            // may stay open over a body that is not in the list.
            expandedBodyId = NO_BODY;
            renaming = false;
            mirrorChoosing = false;
        }
    }

    /** Whether the row whose strip is open is still one of the first {@code written} bodies. */
    private boolean bodyStillPresent(int written) {
        if (expandedBodyId == NO_BODY) {
            return true;
        }
        for (int i = 0; i < written; i++) {
            if (idBuffer[i] == expandedBodyId) {
                return true;
            }
        }
        return false;
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
    /**
     * Shows or withdraws every row's overflow, according to whether the object
     * commands are possible at all right now.
     *
     * <p>Rebuilds rather than toggling visibility in place, because withdrawing
     * the overflow must also close any strip standing open under it — a strip
     * whose commands the mode refuses is exactly the control that cannot
     * succeed.
     */
    void showObjectCommandsAvailable(boolean available) {
        if (commandsAvailable == available) {
            return;
        }
        commandsAvailable = available;
        if (!available) {
            expandedBodyId = NO_BODY;
            renaming = false;
            mirrorChoosing = false;
        }
        refreshFromNative();
    }

    /** Whether the row overflow is currently offered, for verification. */
    boolean objectCommandsAvailable() {
        return commandsAvailable;
    }

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

    /**
     * Which body has its command strip open, or {@link #NO_BODY}.
     *
     * <p>At most ONE at a time, and it is remembered across the rebuild every
     * refresh performs — otherwise the strip would close the moment the command
     * inside it changed native state and the list re-read it. It is view state
     * and nothing else: no {@code ObjectId} lives here as truth, and closing
     * the surface changes no model fact.
     */
    private long expandedBodyId = NO_BODY;

    /** Whether the open strip has been replaced by the inline rename editor. */
    private boolean renaming;

    /**
     * Whether the open strip has been replaced by the mirror plane chooser.
     *
     * <p>View state and nothing else: no plane is remembered anywhere, no
     * default is pre-selected, and closing the chooser changes no model fact.
     * Mutually exclusive with {@link #renaming} because both REPLACE the same
     * one strip, and two surfaces in one row is exactly the partial covering
     * this product forbids.
     */
    private boolean mirrorChoosing;

    private static final long NO_BODY = Long.MIN_VALUE;

    /**
     * Builds one row's inline command strip.
     *
     * <p><b>Why inline and not more icons on the row.</b> The Objects panel is
     * 220 dp wide. More 48 dp targets beside the label would leave the label
     * nothing, and a persistent command column is the desktop shape this
     * product does not have. So the row keeps two targets — the label, which
     * selects, and Delete, which removes, exactly as {@code UI-OWNER-45} left
     * them — plus one overflow that grows the rest out beneath it. The strip
     * pushes the rows below it rather than standing over them, so it never
     * partially covers another live control.
     *
     * <p><b>Why TWO lines.</b> Five 48 dp targets in a row are 256 dp and the
     * panel offers 192 dp of content width, so the strip wraps: Rename,
     * Show/Hide and Lock on the first line, Duplicate and Mirror on the second.
     * Wrapping is what keeps every target at the 48 dp floor — the alternative
     * is shrinking the drawn boxes, which the floor exists to prevent.
     *
     * <p>Every control in it is a 48 dp hit area with a content description
     * naming the act and the body. The two toggles change their GLYPH with the
     * state as well as their words, so what is hidden and what is locked is
     * readable without relying on colour.
     *
     * <p><b>Mirror is absent for a body it could not reflect.</b> Native code
     * answers per row, so an Imported Mesh, a CAD Body and a body carrying a
     * sculpt mesh simply have no Mirror control — a control that must fail is
     * worse than an absent one. The domain guard below JNI stays regardless.
     */
    private View buildCommandStrip(final long objectId, final String label) {
        final Context context = getContext();
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);

        final LinearLayout strip = new LinearLayout(context);
        strip.setId(R.id.object_row_commands);
        strip.setOrientation(VERTICAL);
        strip.setTag(Long.valueOf(objectId));

        final LinearLayout first = commandLine(context);
        final LinearLayout second = commandLine(context);
        strip.addView(first, EditorControlStyles.rowParams(0));
        strip.addView(second, EditorControlStyles.rowParams(gap));

        final ImageView rename = EditorControlStyles.iconButton(context, R.id.object_row_rename,
                R.drawable.ic_object_rename, context.getString(R.string.rename_body, label));
        rename.setTag(Long.valueOf(objectId));
        rename.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                renaming = true;
                mirrorChoosing = false;
                refreshFromNative();
            }
        });
        first.addView(rename, EditorControlStyles.iconButtonParams(context, 0));

        final boolean visible = NativeViewport.sceneBodyVisible(objectId);
        final ImageView visibility = EditorControlStyles.iconButton(context,
                R.id.object_row_visibility,
                visible ? R.drawable.ic_object_visible : R.drawable.ic_object_hidden,
                context.getString(visible ? R.string.hide_body : R.string.show_body, label));
        visibility.setTag(Long.valueOf(objectId));
        visibility.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onVisibilityToggled(objectId, label);
            }
        });
        first.addView(visibility, EditorControlStyles.iconButtonParams(context, gap));

        final boolean locked = NativeViewport.sceneBodyLocked(objectId);
        final ImageView lock = EditorControlStyles.iconButton(context, R.id.object_row_lock,
                locked ? R.drawable.ic_object_locked : R.drawable.ic_object_unlocked,
                context.getString(locked ? R.string.unlock_body : R.string.lock_body, label));
        lock.setTag(Long.valueOf(objectId));
        lock.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onLockToggled(objectId, label);
            }
        });
        first.addView(lock, EditorControlStyles.iconButtonParams(context, gap));

        final ImageView duplicate = EditorControlStyles.iconButton(context,
                R.id.object_row_duplicate, R.drawable.ic_object_duplicate,
                context.getString(R.string.duplicate_body, label));
        duplicate.setTag(Long.valueOf(objectId));
        duplicate.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onDuplicated(objectId, label);
            }
        });
        second.addView(duplicate, EditorControlStyles.iconButtonParams(context, 0));

        if (NativeViewport.sceneBodyCanMirror(objectId)) {
            final ImageView mirror = EditorControlStyles.iconButton(context,
                    R.id.object_row_mirror, R.drawable.ic_object_mirror,
                    context.getString(R.string.mirror_body, label));
            mirror.setTag(Long.valueOf(objectId));
            mirror.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    // Opening the chooser is not the act. Nothing is created,
                    // no history step is recorded and no ObjectId is minted
                    // until a plane is picked.
                    mirrorChoosing = true;
                    renaming = false;
                    refreshFromNative();
                }
            });
            second.addView(mirror, EditorControlStyles.iconButtonParams(context, gap));
        }
        return strip;
    }

    /** One horizontal line of the command strip. */
    private LinearLayout commandLine(Context context) {
        final LinearLayout line = new LinearLayout(context);
        line.setOrientation(HORIZONTAL);
        line.setGravity(Gravity.CENTER_VERTICAL);
        return line;
    }

    /**
     * Builds the compact mirror plane chooser that REPLACES the command strip.
     *
     * <p>Three chips — {@code XY}, {@code XZ}, {@code YZ} — sharing the strip's
     * width, each 48 dp tall and each carrying a content description naming the
     * world axis it reflects, because two letters read aloud say nothing on
     * their own. The labels are the product's existing workplane vocabulary and
     * are not a second naming of the same three planes.
     *
     * <p><b>Choosing a plane is the whole act.</b> There is no preview and no
     * confirmation: the command commits, the chooser closes and the reflection
     * becomes the selected object. Before that, nothing has happened at all —
     * System Back closes the chooser and mutates nothing, exactly as it does
     * over the rename editor, so there is no half-finished Mirror to abandon.
     */
    private View buildMirrorPlaneChooser(final long objectId, final String label) {
        final Context context = getContext();
        final LinearLayout chooser = new LinearLayout(context);
        chooser.setId(R.id.object_mirror_planes);
        chooser.setOrientation(HORIZONTAL);
        chooser.setGravity(Gravity.CENTER_VERTICAL);
        chooser.setTag(Long.valueOf(objectId));
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);

        final int[] ids = {R.id.object_mirror_plane_xy, R.id.object_mirror_plane_xz,
                           R.id.object_mirror_plane_yz};
        final int[] labels = {R.string.mirror_plane_xy, R.string.mirror_plane_xz,
                              R.string.mirror_plane_yz};
        final int[] descriptions = {R.string.mirror_plane_xy_description,
                                    R.string.mirror_plane_xz_description,
                                    R.string.mirror_plane_yz_description};
        final int[] planes = {NativeViewport.MIRROR_PLANE_XY, NativeViewport.MIRROR_PLANE_XZ,
                              NativeViewport.MIRROR_PLANE_YZ};
        for (int i = 0; i < ids.length; i++) {
            final int plane = planes[i];
            final String planeLabel = context.getString(labels[i]);
            final TextView chip =
                    EditorControlStyles.actionChip(context, ids[i], planeLabel);
            chip.setContentDescription(context.getString(descriptions[i]));
            chip.setTag(Long.valueOf(objectId));
            chip.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    onMirrored(objectId, label, plane, planeLabel);
                }
            });
            chooser.addView(chip, EditorControlStyles.evenShare(i == 0 ? 0 : gap));
        }
        return chooser;
    }

    /**
     * Builds the inline rename editor that REPLACES the command strip.
     *
     * <p>A field and a commit control, not a dialog and not a property sheet:
     * the name being changed is one short string and the row it belongs to is
     * right above it. IME Done commits, so the keyboard's own primary key does
     * the obvious thing; System Back closes the editor and changes nothing,
     * because it is not a transaction until it is committed.
     *
     * <p>The field starts at the body's CURRENT stored name, or empty for a
     * body that has none — an empty field for a body labelled from its
     * ObjectId is honest, because that label is derived and is not a name the
     * user could edit.
     */
    private View buildRenameEditor(final long objectId, final String label) {
        final Context context = getContext();
        final LinearLayout editor = new LinearLayout(context);
        editor.setOrientation(HORIZONTAL);
        editor.setGravity(Gravity.CENTER_VERTICAL);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);

        final EditText field = new EditText(context);
        field.setId(R.id.object_rename_field);
        field.setTag(Long.valueOf(objectId));
        field.setSingleLine(true);
        field.setInputType(InputType.TYPE_CLASS_TEXT);
        field.setImeOptions(EditorInfo.IME_ACTION_DONE);
        field.setText(NativeViewport.sceneBodyName(objectId));
        field.setSelectAllOnFocus(true);
        field.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_value));
        field.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        field.setBackgroundResource(R.drawable.bg_field);
        final int padX = EditorControlStyles.dimen(context, R.dimen.field_padding_horizontal);
        final int padY = EditorControlStyles.dimen(context, R.dimen.field_padding_vertical);
        field.setPadding(padX, padY, padX, padY);
        field.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
        field.setContentDescription(context.getString(R.string.object_rename_field, label));
        field.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    onRenamed(objectId, v.getText().toString());
                    return true;
                }
                return false;
            }
        });
        editor.addView(field,
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        final ImageView commit = EditorControlStyles.iconButton(context,
                R.id.object_rename_commit, R.drawable.ic_object_rename,
                context.getString(R.string.object_rename_commit));
        commit.setTag(Long.valueOf(objectId));
        commit.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onRenamed(objectId, field.getText().toString());
            }
        });
        editor.addView(commit, EditorControlStyles.iconButtonParams(context, gap));
        return editor;
    }

    /**
     * Closes the row command strip and the rename editor, changing no model
     * fact.
     *
     * <p>Called by System Back, which is why it reports whether there was
     * anything to close: Back is one step in every phase, and consuming it when
     * nothing was open would swallow the step that should have left the surface.
     */
    boolean dismissRowCommands() {
        if (expandedBodyId == NO_BODY) {
            return false;
        }
        expandedBodyId = NO_BODY;
        renaming = false;
        mirrorChoosing = false;
        refreshFromNative();
        return true;
    }

    /** Which body has its command strip open, for verification. {@code null} when none. */
    Long expandedRow() {
        return expandedBodyId == NO_BODY ? null : Long.valueOf(expandedBodyId);
    }

    private void onVisibilityToggled(long objectId, String label) {
        final Context context = getContext();
        final boolean wantVisible = !NativeViewport.sceneBodyVisible(objectId);
        final int status = NativeViewport.sceneSetBodyVisible(objectId, wantVisible);
        if (status != NativeViewport.OBJCMD_OK) {
            host.showStatus(context.getString(R.string.status_body_command_failed),
                    R.attr.fsTextError);
            return;
        }
        host.onNativeStateChanged();
        host.showStatus(context.getString(
                wantVisible ? R.string.status_body_shown : R.string.status_body_hidden, label),
                R.attr.fsTextSecondary);
    }

    private void onLockToggled(long objectId, String label) {
        final Context context = getContext();
        final boolean wantLocked = !NativeViewport.sceneBodyLocked(objectId);
        final int status = NativeViewport.sceneSetBodyLocked(objectId, wantLocked);
        if (status != NativeViewport.OBJCMD_OK) {
            host.showStatus(context.getString(R.string.status_body_command_failed),
                    R.attr.fsTextError);
            return;
        }
        host.onNativeStateChanged();
        host.showStatus(context.getString(
                wantLocked ? R.string.status_body_locked : R.string.status_body_unlocked, label),
                R.attr.fsTextSecondary);
    }

    private void onDuplicated(long objectId, String label) {
        final Context context = getContext();
        final int status = NativeViewport.sceneDuplicateBody(objectId);
        if (status != NativeViewport.OBJCMD_OK) {
            // The face-supported refusal gets its own words because it is the
            // only one a user can reason about: the object sits on another
            // object's face, so a copy would have nowhere of its own to be.
            host.showStatus(status == NativeViewport.OBJCMD_REFUSED_FACE_SUPPORTED_CAD
                            ? context.getString(R.string.status_body_duplicate_refused_face, label)
                            : context.getString(R.string.status_body_command_failed),
                    R.attr.fsTextError);
            return;
        }
        // The copy is the active body now, and it is what the strip should not
        // still be open over.
        expandedBodyId = NO_BODY;
        renaming = false;
        mirrorChoosing = false;
        host.onNativeStateChanged();
        host.showStatus(context.getString(R.string.status_body_duplicated, label),
                R.attr.fsTextSecondary);
    }

    /**
     * Reflects one object across one principal world plane.
     *
     * <p><b>Unconfirmed, deliberately</b>, on exactly Duplicate's terms: it
     * destroys nothing, it is one Undo away, and the history capsule is on the
     * same screen.
     *
     * <p>The reflection becomes the active body, so the strip must not stay
     * open over the row it was started from — the same rule Duplicate follows.
     */
    private void onMirrored(long objectId, String label, int plane, String planeLabel) {
        final Context context = getContext();
        final int status = NativeViewport.sceneMirrorBody(objectId, plane);
        if (status != NativeViewport.OBJCMD_OK) {
            // The representation refusal gets its own words because it is the
            // only one a user can reason about: this object is not a shape the
            // reflection could be carried by.
            host.showStatus(status == NativeViewport.OBJCMD_REFUSED_NOT_MIRRORABLE
                            ? context.getString(
                                    R.string.status_body_mirror_refused_representation)
                            : context.getString(R.string.status_body_command_failed),
                    R.attr.fsTextError);
            return;
        }
        expandedBodyId = NO_BODY;
        renaming = false;
        mirrorChoosing = false;
        host.onNativeStateChanged();
        host.showStatus(context.getString(R.string.status_body_mirrored, label, planeLabel),
                R.attr.fsTextSecondary);
    }

    private void onRenamed(long objectId, String requested) {
        final Context context = getContext();
        final int status = NativeViewport.sceneRenameBody(objectId, requested);
        if (status != NativeViewport.OBJCMD_OK) {
            host.showStatus(context.getString(
                    status == NativeViewport.OBJCMD_REFUSED_INVALID_NAME
                            ? R.string.status_body_rename_refused_empty
                            : R.string.status_body_command_failed),
                    R.attr.fsTextError);
            // The editor stays open on a refusal, holding what was typed, so
            // the user can correct it rather than start again.
            return;
        }
        expandedBodyId = NO_BODY;
        renaming = false;
        mirrorChoosing = false;
        host.onNativeStateChanged();
        host.showStatus(context.getString(R.string.status_body_renamed,
                BodyLabels.of(context, objectId)), R.attr.fsTextSecondary);
    }
}
