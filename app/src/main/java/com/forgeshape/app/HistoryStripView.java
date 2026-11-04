package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The workbench's History strip ({@code WORKBENCH-RIBBON-R1}): the active
 * body's steps along the bottom edge in construction order, newest at the
 * trailing end, beside the Undo and Redo capsule — the back and forward
 * arrows the owner asked for, with no rollback marker, because a rolled-back
 * chain would need durable semantics the project does not have.
 *
 * <p>A VIEW of the same timeline read the Browser's History shows
 * ({@link FeatureHistoryPresentation.Model}), stored nowhere; a step chip is
 * the History row's own act ({@link FeatureHistoryView.OnHistoryRowChosen}),
 * so tapping a step here and tapping its row there are one path. A failed
 * step is marked by its row's mark and its error colour together; a step that
 * cannot be edited is drawn and not clickable.
 */
final class HistoryStripView extends LinearLayout {

    private final FeatureHistoryView.OnHistoryRowChosen chosen;
    private final HorizontalScrollView scroller;
    private final LinearLayout steps;

    HistoryStripView(Context context, FeatureHistoryView.OnHistoryRowChosen chosen) {
        super(context);
        this.chosen = chosen;
        setId(R.id.history_strip);
        setOrientation(HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);
        EditorControlStyles.applyFloatingSurface(this);
        final int pad = EditorControlStyles.dimen(context, R.dimen.toolbar_group_padding);
        setPadding(pad * 3, pad, pad, pad);
        // Opaque to touch like every chrome surface.
        setClickable(true);
        setContentDescription(context.getString(R.string.history_strip_description));
        final TextView title = EditorControlStyles.sectionLabel(context,
                context.getString(R.string.history_strip_title));
        addView(title, new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        scroller = new HorizontalScrollView(context);
        scroller.setId(R.id.history_strip_steps);
        scroller.setHorizontalScrollBarEnabled(false);
        final LayoutParams scrollParams = new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1.0f);
        scrollParams.leftMargin = pad * 3;
        addView(scroller, scrollParams);
        steps = new LinearLayout(context);
        steps.setOrientation(HORIZONTAL);
        steps.setGravity(Gravity.CENTER_VERTICAL);
        scroller.addView(steps, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    /** Rebuilt from one fresh read, never diffed; scrolled to the newest step. */
    void showModel(FeatureHistoryPresentation.Model model) {
        final Context context = getContext();
        steps.removeAllViews();
        final int gap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);
        int index = 0;
        for (final FeatureHistoryPresentation.Row row : model.rows) {
            final String name = FeatureHistoryText.name(context, row);
            final TextView chip = EditorControlStyles.chip(context, R.id.history_strip_step,
                    row.mark() + "  " + name);
            chip.setTag(row);
            if (row.isFailed()) {
                chip.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextError));
                EditorControlStyles.applyMediumWeight(chip);
            } else if (row.state == NativeViewport.TIMELINE_STATE_NOT_REGENERATED
                    || row.state == NativeViewport.TIMELINE_STATE_PENDING
                    || row.state == NativeViewport.TIMELINE_STATE_UNUSED) {
                chip.setAlpha(0.6f);
            }
            EditorControlStyles.setChipActive(chip, row.editing);
            if (row.editable()) {
                chip.setOnClickListener(new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        chosen.onFeatureHistoryRowChosen(row);
                    }
                });
            } else {
                chip.setClickable(false);
            }
            final LayoutParams params = new LayoutParams(LayoutParams.WRAP_CONTENT,
                    LayoutParams.WRAP_CONTENT);
            params.leftMargin = index == 0 ? 0 : gap;
            steps.addView(chip, params);
            index++;
        }
        scroller.post(new Runnable() {
            @Override
            public void run() {
                scroller.fullScroll(View.FOCUS_RIGHT);
            }
        });
    }

    /** The step chips, in order, for a test. */
    LinearLayout stepsContainer() {
        return steps;
    }
}
