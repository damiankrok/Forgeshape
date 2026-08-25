package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

/**
 * The one question ForgeShape asks before the user starts working: which
 * representation is this model going to begin in?
 *
 * <p>Exactly two answers, because the product has exactly two representations.
 * <b>Construction / CAD</b> begins from an exact primitive whose dimensions are
 * typed. <b>Sculpt</b> begins on a mesh that is already frozen and ready for a
 * brush. Neither is a document, a template or a saved project: there is no
 * persistence anywhere behind this, and both answers end in the ordinary
 * workspace on the ordinary scene.
 *
 * <p><b>It is not a launcher screen.</b> The scrim is partial and the Vulkan
 * viewport keeps rendering behind it, so the first thing ForgeShape shows is
 * still the model. That is the viewport-first rule applied to the one moment
 * where it would be easiest to break.
 *
 * <p><b>It promises nothing that does not exist.</b> Sketch and Extrude have
 * reserved homes in the Tool Rail and are named nowhere here, because a start
 * screen that offered them would be advertising a feature the product does not
 * have.
 *
 * <p><b>It owns no state and makes no native call.</b> It reports which option
 * was pressed; {@link EditorWorkspaceView} owns what that means and drives the
 * existing product entry points to get there.
 */
final class StartChooserView extends FrameLayout {

    /** Told which way the user chose to begin. */
    interface OnStartFlowChosen {
        void onConstructionStartChosen();

        void onSculptStartChosen();
    }

    /**
     * The panel itself, which is also the scroll container.
     *
     * <p>One view rather than a panel inside a scroller: a shadow is drawn
     * outside its own bounds, so a raised panel wrapped in a scroller would
     * have that scroller clip exactly the shadow. Making the scroller the
     * panel means a window too short for the question can still reach both
     * answers, which is the same rule the Tool Rail follows.
     */
    private final ScrollView panel;

    StartChooserView(Context context, final OnStartFlowChosen listener) {
        super(context);
        setId(R.id.start_chooser);
        setBackgroundResource(R.drawable.bg_chooser_scrim);
        EditorControlStyles.allowChildShadows(this);

        panel = new ScrollView(context);
        panel.setId(R.id.start_chooser_panel);
        panel.setBackgroundResource(R.drawable.bg_chooser_panel);
        panel.setElevation(EditorControlStyles.dimen(context, R.dimen.elevation_chooser));
        final int pad = EditorControlStyles.dimen(context, R.dimen.chooser_padding);
        panel.setPadding(pad, pad, pad, pad);
        panel.setClipToPadding(false);

        final LinearLayout content = new LinearLayout(context);
        content.setOrientation(LinearLayout.VERTICAL);
        panel.addView(content, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        content.addView(EditorControlStyles.displayText(context,
                context.getString(R.string.new_project)),
                EditorControlStyles.rowParams(0));

        content.addView(EditorControlStyles.captionText(context, View.NO_ID,
                context.getString(R.string.start_prompt)),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));

        content.addView(buildOption(context, R.id.start_option_construction,
                        R.drawable.ic_start_construction,
                        context.getString(R.string.start_construction_title),
                        context.getString(R.string.start_construction_description),
                        new OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                listener.onConstructionStartChosen();
                            }
                        }),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.section_gap)));

        content.addView(buildOption(context, R.id.start_option_sculpt,
                        R.drawable.ic_start_sculpt,
                        context.getString(R.string.start_sculpt_title),
                        context.getString(R.string.start_sculpt_description),
                        new OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                listener.onSculptStartChosen();
                            }
                        }),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));

        final LayoutParams params = new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.CENTER;
        addView(panel, params);
    }

    /**
     * One option: an icon, what it is, and one line of what it does.
     *
     * <p>The description is the working part. "Sculpt" alone does not say that
     * the exact source is kept and reachable, and that is precisely the thing a
     * user has to trust before choosing it.
     */
    private View buildOption(Context context, int id, int iconRes, CharSequence title,
                             CharSequence description, OnClickListener onChosen) {
        final LinearLayout option = new LinearLayout(context);
        option.setId(id);
        option.setOrientation(LinearLayout.HORIZONTAL);
        // TOP, not CENTER_VERTICAL. Centred against a three-line description the
        // icon floated beside the second line of prose with nothing to relate
        // to, and the title it actually names sat above it. Aligned to the top
        // it reads with the title, and the two options line up with each other
        // even though their descriptions wrap to different heights.
        option.setGravity(Gravity.TOP);
        option.setBackgroundResource(R.drawable.bg_chooser_card);
        final int pad = EditorControlStyles.dimen(context, R.dimen.chooser_option_padding);
        option.setPadding(pad, pad, pad, pad);
        option.setClickable(true);
        option.setFocusable(true);
        // The title is the whole control's name and the description follows it,
        // so a screen reader gets the same two facts a sighted user gets and in
        // the same order.
        option.setContentDescription(title + ". " + description);
        option.setOnClickListener(onChosen);

        final View optionIcon = EditorControlStyles.icon(context, iconRes,
                R.dimen.chooser_option_icon_size);
        // Optically centred on the TITLE's line rather than sitting on its cap
        // height: a 28 dp glyph top-aligned against 15 sp type reads as riding
        // slightly high, and this is the one place in the product where an icon
        // is paired with a heading instead of with a label beneath it.
        final LinearLayout.LayoutParams iconParams =
                (LinearLayout.LayoutParams) optionIcon.getLayoutParams();
        iconParams.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        option.addView(optionIcon, iconParams);

        final LinearLayout text = new LinearLayout(context);
        text.setOrientation(LinearLayout.VERTICAL);
        text.addView(EditorControlStyles.titleText(context, View.NO_ID, title),
                EditorControlStyles.rowParams(0));
        text.addView(EditorControlStyles.captionText(context, View.NO_ID, description),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        final LinearLayout.LayoutParams textParams = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        textParams.leftMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        option.addView(text, textParams);
        return option;
    }

    /**
     * Caps the panel's width and keeps it off the window edges.
     *
     * <p>Done here rather than with a fixed width because the answer depends on
     * the window: a phone gets the window minus its margins, and anything wider
     * gets the cap, so the two options never end up an arm apart on a tablet.
     * Idempotent, so the second measure pass of a traversal changes nothing.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final Context context = getContext();
        final int margin = EditorControlStyles.dimen(context, R.dimen.chooser_padding);
        final int cap = EditorControlStyles.dimen(context, R.dimen.chooser_max_width);
        final int available = MeasureSpec.getSize(widthMeasureSpec) - 2 * margin;
        final int wanted = Math.min(cap, available);
        final ViewGroup.LayoutParams params = panel.getLayoutParams();
        if (wanted > 0 && params.width != wanted) {
            params.width = wanted;
            panel.setLayoutParams(params);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Swallows every touch the two options did not take.
     *
     * <p>Nothing behind this may be operated while the question is open — not
     * the chrome, and not the viewport, whose camera would otherwise orbit
     * under a scrim the user is reading.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }
}
