package com.forgeshape.app;

import android.content.Context;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The resting scene control: which body is being edited, and how to add one.
 *
 * <p>This is the workspace's main entry to the scene, and it is deliberately a
 * <b>capsule rather than a panel</b>. What the user needs at rest is two facts
 * and two acts — which body is current, that there are others, that the list can
 * be opened, and that a new body can be created — and all four fit in a control
 * the width of a body name. The list itself is a surface the user asks for and
 * dismisses; it is not something the workspace pays for permanently.
 *
 * <p><b>It holds no scene state.</b> The label is written from
 * {@link NativeViewport#sceneActiveBodyId()} on every refresh, and neither
 * control changes anything by itself: the expand control opens the one
 * {@link ObjectsSectionView} in the product, and {@code +} opens the Add
 * Primitive palette. Which body is active lives below JNI and is read back, so
 * this capsule can never disagree with the viewport or with the precision
 * surface.
 *
 * <p><b>{@code +} is an anchor, not an act.</b> It creates nothing on its own —
 * tapping it opens the palette, and a body exists only once a shape has been
 * chosen. A {@code +} that silently appended a default box is what made
 * creation feel like an administrative operation rather than a choice.
 *
 * <p><b>And in Sculpt there is no {@code +} at all.</b> {@code sceneAddBody()}
 * refuses while sculpting — that is the domain's rule and it is correct — but
 * the control was drawn anyway, so a user could tap it, be shown six shapes,
 * choose one, and only then be told no. A path that must fail is worse than an
 * absent one, and it is worse than a disabled one too: what a greyed {@code +}
 * would say is "not now", which is exactly as much as its absence says, at the
 * cost of a dead control in the resting workspace. The scene stays reachable —
 * the body name still opens the list — because seeing what the scene holds is
 * as true in Sculpt as anywhere else. See {@link #showCreationAvailable}.
 */
final class ObjectsCapsuleView extends LinearLayout {

    /** Told which of the capsule's two acts was invoked. */
    interface OnObjectsCapsuleAction {
        /** Open (or close) the scene list, grown from this capsule. */
        void onObjectsRequested();

        /** Open (or close) the Add Primitive palette, grown from the {@code +}. */
        void onAddPrimitiveRequested();
    }

    private final TextView activeBody;
    private final ImageView add;

    ObjectsCapsuleView(Context context, final OnObjectsCapsuleAction actions) {
        super(context);
        setId(R.id.objects_capsule);
        setOrientation(HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);
        // TIER 1 — a floating control group standing ON the model, exactly like
        // the toolbar's capsules and the Tool Rail. Clickable so the capsule
        // itself swallows any touch its two controls did not take: reaching for
        // the scene must never orbit the camera behind it.
        EditorControlStyles.applyFloatingSurface(this);
        setClickable(true);
        final int pad = EditorControlStyles.dimen(context, R.dimen.toolbar_group_padding);
        setPadding(pad, pad, pad, pad);

        // The whole label is the expand target, not a separate chevron beside
        // it: the body name is the largest thing in the capsule and the one a
        // thumb reaches for, and a name that says which body is current while
        // being inert would be a label pretending to be a control.
        activeBody = EditorControlStyles.chip(context, R.id.objects_capsule_active, "");
        activeBody.setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
        // A member of this capsule, so it carries the capsule's own concentric
        // corner rather than the 10 dp control corner — which left a crescent of
        // capsule showing at the label's leading end whenever the scene list was
        // open and the label was lit.
        EditorControlStyles.asCapsuleMember(activeBody, R.drawable.bg_capsule_control);
        activeBody.setMaxWidth(
                EditorControlStyles.dimen(context, R.dimen.objects_capsule_max_label_width));
        activeBody.setEllipsize(TextUtils.TruncateAt.END);
        activeBody.setCompoundDrawablesRelativeWithIntrinsicBounds(
                R.drawable.ic_objects, 0, 0, 0);
        activeBody.setCompoundDrawablePadding(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small));
        activeBody.setCompoundDrawableTintList(EditorControlStyles.contentTint(context));
        activeBody.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onObjectsRequested();
            }
        });
        addView(activeBody, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        add = EditorControlStyles.iconButton(context, R.id.objects_capsule_add,
                R.drawable.ic_add, context.getString(R.string.add_primitive));
        add.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onAddPrimitiveRequested();
            }
        });
        addView(add, EditorControlStyles.iconButtonParams(context,
                EditorControlStyles.dimen(context, R.dimen.toolbar_gap)));
    }

    /**
     * Rewrites the label from native scene truth.
     *
     * <p>Called from the one refresh the workspace runs in <b>every</b> mode, so
     * a viewport pick, an Objects row, a new body and a mode change all end at
     * the same fact.
     */
    void refreshFromNative() {
        final String body = BodyLabels.ofActive(getContext());
        activeBody.setText(body);
        activeBody.setContentDescription(
                getContext().getString(R.string.objects_capsule_active, body));
    }

    /** Draws the expand control as active while the scene list is open. */
    void showObjectsOpen(boolean open) {
        EditorControlStyles.setCapsuleMemberActive(activeBody, open);
    }

    /**
     * Shows or withdraws the {@code +}, according to whether creation is
     * possible at all right now.
     *
     * <p>GONE rather than disabled. The scene is still reachable beside it, so
     * nothing is lost: what is removed is a control that could only ever refuse.
     * The capsule simply narrows to its label, which is the honest shape of what
     * it offers in Sculpt.
     */
    void showCreationAvailable(boolean available) {
        add.setVisibility(available ? VISIBLE : GONE);
    }

    /** Whether the {@code +} is currently offered, for verification. */
    boolean creationAvailable() {
        return add.getVisibility() == VISIBLE;
    }

    /** Draws {@code +} as active while the Add Primitive palette is open. */
    void showAddOpen(boolean open) {
        EditorControlStyles.setIconButtonActive(add, open);
        add.setContentDescription(getContext().getString(
                open ? R.string.add_primitive_close : R.string.add_primitive));
    }

    /** The {@code +}, which is what the Add Primitive palette is anchored to. */
    View addControl() {
        return add;
    }

    /**
     * Swallows every touch the capsule's own controls did not take.
     *
     * <p>The same rule every chrome surface follows. The capsule sits low over
     * the model, which is exactly where a Sculpt stroke is most likely to
     * begin.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }
}
