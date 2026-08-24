package com.forgeshape.app;

import android.content.Context;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The edge tool selector.
 *
 * <p>One rail class serves both modes. Its entries change — the four sculpt
 * brushes, or the Construction editors plus the reserved Sketch and Extrude —
 * but its structure never does, which is the whole point of the shell: mode and
 * tool decide content, not shape.
 *
 * <p><b>It selects; it does not decide.</b> Tapping an entry asks for a tool.
 * Which entry is drawn active comes from {@link #showActive(int)}, and the
 * caller reads that back from native state after the request. The rail can
 * therefore never claim a tool the session is not actually holding.
 *
 * <p>Reserved entries are visible and clearly disabled from day one, so the
 * shell's shape does not change when Sketch and Extrude arrive.
 *
 * <p><b>An entry owns its own gesture against the scroll container.</b> See
 * {@link RailEntryView}: the rail lives inside a {@code ScrollView} so a window
 * too short for every entry can still reach every entry, and without that
 * ownership a tap that drifted a few pixels vertically was taken by the scroll
 * container and selected nothing at all.
 */
final class ToolRailView extends LinearLayout {

    /** Told which entry was tapped. Reserved entries never call this. */
    interface OnToolSelected {
        void onToolSelected(int key);
    }

    /** One rail entry, described rather than subclassed — four tools do not
     *  need a framework, and a fifth will not either. */
    static final class Entry {
        final int viewId;
        final int iconRes;
        final String label;
        /** The caller's meaning for this entry: a {@code TOOL_*} constant in
         *  Sculpt, an {@code EditorUiState.CONSTRUCTION_TOOL_*} in Construction. */
        final int key;
        final boolean reserved;

        Entry(int viewId, int iconRes, String label, int key, boolean reserved) {
            this.viewId = viewId;
            this.iconRes = iconRes;
            this.label = label;
            this.key = key;
            this.reserved = reserved;
        }
    }

    private Entry[] entries = new Entry[0];
    private final OnToolSelected listener;
    private boolean compact;

    ToolRailView(Context context, OnToolSelected listener) {
        super(context);
        this.listener = listener;
        setId(R.id.tool_rail);
        setOrientation(VERTICAL);
        setContentDescription(context.getString(R.string.tool_rail));
        // The floating surface and its depth belong to the scroll container
        // that holds this view, not to this view: a shadow is drawn outside the
        // child's bounds, and the ScrollView wraps this rail exactly, so a
        // shadow set here would be clipped away by the very container that
        // makes the rail reachable on a short window.
        final int padding = EditorControlStyles.dimen(context, R.dimen.rail_padding);
        setPadding(padding, padding, padding, padding);
    }

    /**
     * Shortens the entries for a window that has no height to spare.
     *
     * <p>The rail keeps every entry rather than dropping any: a tool that
     * exists on a phone in portrait and vanishes in landscape is a worse
     * problem than a shorter button.
     */
    void setCompactEntries(boolean compact) {
        if (this.compact == compact) {
            return;
        }
        this.compact = compact;
        rebuild();
    }

    void setEntries(Entry[] entries) {
        this.entries = entries;
        rebuild();
    }

    private void rebuild() {
        removeAllViews();
        final Context context = getContext();
        final int gap = EditorControlStyles.dimen(context, R.dimen.rail_item_gap);
        final int width = EditorControlStyles.dimen(context, R.dimen.rail_item_width);
        final int height = compact
                ? EditorControlStyles.dimen(context, R.dimen.control_height)
                : EditorControlStyles.dimen(context, R.dimen.rail_item_height);

        for (int i = 0; i < entries.length; i++) {
            final Entry entry = entries[i];
            final View item = buildItem(context, entry);
            final LinearLayout.LayoutParams params =
                    new LinearLayout.LayoutParams(width, height);
            params.topMargin = (i == 0) ? 0 : gap;
            addView(item, params);
        }
    }

    private View buildItem(Context context, final Entry entry) {
        final RailEntryView item = new RailEntryView(context);
        item.setId(entry.viewId);
        item.setOrientation(VERTICAL);
        item.setGravity(Gravity.CENTER);
        item.setBackgroundResource(R.drawable.bg_control);

        // A compact window drops the icon rather than the label: the label is
        // the part that says which tool this is without prior learning.
        if (!compact) {
            item.addView(EditorControlStyles.icon(context, entry.iconRes,
                    R.dimen.rail_icon_size));
        }

        final TextView label = new TextView(context);
        label.setText(entry.label);
        label.setGravity(Gravity.CENTER);
        label.setSingleLine(true);
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.rail_label_size));
        // Icon and label read the same state list and both duplicate the
        // entry's state, so an entry's glyph and its caption cannot disagree
        // about whether it is active, pressed or reserved.
        label.setTextColor(EditorControlStyles.contentTint(context));
        label.setDuplicateParentStateEnabled(true);
        item.addView(label, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        if (entry.reserved) {
            // Visible, readable and inert. The content description says why,
            // so a screen reader is not left to infer it from a grey label.
            item.setEnabled(false);
            item.setContentDescription(
                    context.getString(R.string.reserved_not_implemented, entry.label));
            item.setBackgroundResource(R.drawable.bg_control_reserved);
        } else {
            item.setContentDescription(entry.label);
            item.setClickable(true);
            item.setFocusable(true);
            item.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    listener.onToolSelected(entry.key);
                }
            });
        }
        return item;
    }

    /**
     * Draws exactly one entry as active — the one whose key the caller passes,
     * having read it back from native state.
     */
    void showActive(int activeKey) {
        for (Entry entry : entries) {
            if (entry.reserved) {
                continue;
            }
            final View item = findViewById(entry.viewId);
            if (item == null) {
                continue;
            }
            final boolean active = entry.key == activeKey;
            item.setBackgroundResource(
                    active ? R.drawable.bg_control_active : R.drawable.bg_control);
            // The icon and the label follow from the entry's own state, so this
            // is the whole repaint. It is also what verification reads.
            item.setActivated(active);
        }
    }

    /**
     * Swallows every touch that lands on the rail and is not taken by an entry.
     *
     * <p>The rail stands on the viewport, so without this an unclaimed touch
     * between two entries would fall through to the {@code SurfaceView} beneath
     * and orbit the camera — or, in Sculpt Mode, deform the model by reaching
     * for a tool. Chrome owns its own gestures completely.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }

    /**
     * One rail entry, which holds its gesture against the enclosing
     * {@code ScrollView} until the gesture is clearly a scroll.
     *
     * <p>The defect this fixes: a {@code ScrollView} intercepts as soon as a
     * drag passes the platform touch slop, and it did so on a control whose job
     * is to be tapped. A finger — and much more a stylus tip resting on a
     * 64 dp target — drifts a few pixels between down and up, so tapping a tool
     * intermittently scrolled the rail by nothing and selected nothing at all.
     *
     * <p>The rule is deliberately asymmetric, because the rail's entries are
     * the reason it exists and its scrolling is only a fallback for a window
     * too short to show them:
     *
     * <ul>
     *   <li>On <b>Down</b> the entry disallows interception, so the container
     *       cannot take a gesture that has not yet proved to be a scroll.</li>
     *   <li>Once travel passes {@link #SELECTION_SLOP_MULTIPLIER} times the
     *       platform slop, interception is allowed again. The container takes
     *       the very next move, the platform delivers this entry an
     *       {@code ACTION_CANCEL}, and a cancelled press fires no click — so a
     *       real scroll provably cannot select a tool it merely passed over.</li>
     *   <li>On <b>Up</b> or <b>Cancel</b> the disallow is always released, so
     *       no gesture can leave the container permanently unable to scroll.</li>
     * </ul>
     *
     * <p>This changes nothing about viewport gesture arbitration. The entry is
     * chrome, the chrome consumes it, and the {@code SurfaceView} beneath never
     * sees any of it either way.
     */
    private static final class RailEntryView extends LinearLayout {

        /**
         * How far past the platform's own scroll threshold a drag must travel
         * before it stops being a tap.
         *
         * <p>Two rather than one: at one it is exactly the container's
         * threshold and the ambiguous band the defect lived in is unchanged.
         * The cost is stated rather than hidden — a deliberate scroll that
         * begins on an entry needs roughly twice the usual travel before it
         * takes hold, which is the right trade for a control that is tapped far
         * more often than the rail is scrolled.
         */
        private static final int SELECTION_SLOP_MULTIPLIER = 2;

        private final int selectionSlopPixels;
        private float downX;
        private float downY;
        private boolean holdingGesture;

        RailEntryView(Context context) {
            super(context);
            selectionSlopPixels = ViewConfiguration.get(context).getScaledTouchSlop()
                    * SELECTION_SLOP_MULTIPLIER;
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    downX = event.getX();
                    downY = event.getY();
                    holdingGesture = true;
                    requestHold(true);
                    break;
                case MotionEvent.ACTION_MOVE:
                    if (holdingGesture && travelledPastSelection(event)) {
                        // This is a scroll after all. Hand the gesture back;
                        // the container intercepts on the next event and the
                        // platform cancels this entry's press for us.
                        holdingGesture = false;
                        requestHold(false);
                    }
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    holdingGesture = false;
                    requestHold(false);
                    break;
                default:
                    break;
            }
            // The entry is clickable, so View's own handling decides whether
            // this resolves as a click. Nothing above changes that decision;
            // it only decides who is allowed to take the gesture away.
            return super.onTouchEvent(event);
        }

        /** Measured from the DOWN point, so a slow creeping drag still counts. */
        private boolean travelledPastSelection(MotionEvent event) {
            final float dx = event.getX() - downX;
            final float dy = event.getY() - downY;
            return Math.abs(dx) > selectionSlopPixels || Math.abs(dy) > selectionSlopPixels;
        }

        private void requestHold(boolean hold) {
            final ViewParent parent = getParent();
            if (parent != null) {
                parent.requestDisallowInterceptTouchEvent(hold);
            }
        }
    }
}
