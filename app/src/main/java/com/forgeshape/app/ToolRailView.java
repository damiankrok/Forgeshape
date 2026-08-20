package com.forgeshape.app;

import android.content.Context;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
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
        final String glyph;
        final String label;
        /** The caller's meaning for this entry: a {@code TOOL_*} constant in
         *  Sculpt, an {@code EditorUiState.CONSTRUCTION_TOOL_*} in Construction. */
        final int key;
        final boolean reserved;

        Entry(int viewId, String glyph, String label, int key, boolean reserved) {
            this.viewId = viewId;
            this.glyph = glyph;
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
        setBackground(EditorControlStyles.chromeOverlay(context));
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
        final LinearLayout item = new LinearLayout(context);
        item.setId(entry.viewId);
        item.setOrientation(VERTICAL);
        item.setGravity(Gravity.CENTER);
        item.setBackground(EditorControlStyles.controlBackground(context, false));

        if (!compact) {
            final TextView glyph = new TextView(context);
            glyph.setText(entry.glyph);
            glyph.setGravity(Gravity.CENTER);
            glyph.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                    EditorControlStyles.dimen(context, R.dimen.rail_glyph_size));
            glyph.setTextColor(context.getColor(
                    entry.reserved ? R.color.text_disabled : R.color.text_primary));
            item.addView(glyph, new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        }

        final TextView label = new TextView(context);
        label.setText(entry.label);
        label.setGravity(Gravity.CENTER);
        label.setSingleLine(true);
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.rail_label_size));
        label.setTextColor(context.getColor(
                entry.reserved ? R.color.text_disabled : R.color.text_primary));
        item.addView(label, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        if (entry.reserved) {
            // Visible, readable and inert. The content description says why,
            // so a screen reader is not left to infer it from a grey label.
            item.setEnabled(false);
            item.setContentDescription(
                    context.getString(R.string.reserved_not_implemented, entry.label));
            final android.graphics.drawable.GradientDrawable shape =
                    new android.graphics.drawable.GradientDrawable();
            shape.setColor(context.getColor(R.color.control_surface));
            shape.setCornerRadius(EditorControlStyles.dimen(context, R.dimen.control_corner));
            shape.setStroke(EditorControlStyles.dimen(context, R.dimen.control_border_width),
                    context.getColor(R.color.text_disabled));
            item.setBackground(shape);
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
            item.setBackground(EditorControlStyles.controlBackground(getContext(), active));
            item.setActivated(active);
            paintLabels((LinearLayout) item, active);
        }
    }

    private void paintLabels(LinearLayout item, boolean active) {
        final int color = getContext().getColor(
                active ? R.color.text_primary : R.color.text_secondary);
        for (int i = 0; i < item.getChildCount(); i++) {
            final View child = item.getChildAt(i);
            if (child instanceof TextView) {
                ((TextView) child).setTextColor(color);
            }
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
}
