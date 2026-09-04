package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * The one shape every "one question before you continue" surface takes.
 *
 * <p>A partial scrim over the live viewport, one raised panel, an eyebrow with
 * the product's name, a headline, one line of prose, and a short column of
 * option cards — each an icon, a title and one line of what it does. Home, the
 * New Project chooser and the unsaved-changes question all occupy the same
 * moment and would otherwise be three copies of the same fifty lines, and a
 * second visual grammar for that moment would be a second thing to learn.
 *
 * <p><b>It is not a launcher screen.</b> The scrim is partial and the Vulkan
 * viewport keeps rendering behind it, so the first thing ForgeShape shows is
 * still the viewport — empty at Home, because no project is open, and that is
 * honest rather than a placeholder primitive standing in for one.
 *
 * <p><b>Owns no state and makes no native call.</b> A subclass reports which
 * option was pressed; {@link EditorWorkspaceView} owns what that means.
 */
abstract class ChooserSurfaceView extends FrameLayout {

    private final ScrollView panel;
    private final LinearLayout content;
    private final TextView status;

    ChooserSurfaceView(Context context, int id, int panelId, CharSequence headline,
                       CharSequence caption) {
        super(context);
        setId(id);
        setBackgroundResource(R.drawable.bg_chooser_scrim);
        EditorControlStyles.allowChildShadows(this);

        // One view that is both the panel and its scroll container: a shadow
        // is drawn outside its own bounds, so a raised panel wrapped in a
        // scroller would have that scroller clip exactly the shadow. Making
        // the scroller the panel means a window too short for the question can
        // still reach every answer, which is the rule the Tool Rail follows.
        panel = new ScrollView(context);
        panel.setId(panelId);
        panel.setBackgroundResource(R.drawable.bg_chooser_panel);
        panel.setElevation(EditorControlStyles.dimen(context, R.dimen.elevation_chooser));
        final int pad = EditorControlStyles.dimen(context, R.dimen.chooser_padding);
        panel.setPadding(pad, pad, pad, pad);
        panel.setClipToPadding(false);

        content = new LinearLayout(context);
        content.setOrientation(LinearLayout.VERTICAL);
        panel.addView(content, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        content.addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.app_name)),
                EditorControlStyles.rowParams(0));
        content.addView(EditorControlStyles.displayText(context, headline),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap_small)));
        content.addView(EditorControlStyles.captionText(context, View.NO_ID, caption),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        // A verdict line the surface can carry itself, for the one moment the
        // toolbar's status capsule is not on screen: a refused Open at Home.
        // GONE until it has something to say, exactly as the toolbar's is.
        status = EditorControlStyles.captionText(context, View.NO_ID, "");
        status.setVisibility(GONE);

        final LayoutParams params = new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.CENTER;
        addView(panel, params);
    }

    /** Adds one option card. The first card takes the section gap; the rest the row gap. */
    final View addOption(int id, int iconRes, CharSequence title, CharSequence description,
                         OnClickListener onChosen) {
        final Context context = getContext();
        final boolean first = optionCount == 0;
        optionCount++;
        final View option = buildOption(context, id, iconRes, title, description, onChosen);
        content.addView(option, EditorControlStyles.rowParams(EditorControlStyles.dimen(context,
                first ? R.dimen.section_gap : R.dimen.row_gap)));
        return option;
    }

    private int optionCount;

    /** Adds the status line under the options. Called once, after the options. */
    final void addStatusLine() {
        content.addView(status, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(getContext(), R.dimen.row_gap)));
    }

    /** A quiet secondary action under the options: the way to not answer. */
    final TextView addSecondaryAction(int id, CharSequence label, OnClickListener onClick) {
        final Context context = getContext();
        final TextView chip = EditorControlStyles.secondaryActionChip(context, id, label);
        chip.setOnClickListener(onClick);
        final LinearLayout.LayoutParams params = EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.section_gap));
        params.width = ViewGroup.LayoutParams.MATCH_PARENT;
        content.addView(chip, params);
        return chip;
    }

    /** Writes a verdict on the surface itself, or clears it with an empty message. */
    final void showStatus(CharSequence message, int colorAttr) {
        status.setText(message);
        status.setContentDescription(message);
        status.setTextColor(EditorControlStyles.themeColor(getContext(), colorAttr));
        status.setVisibility(message == null || message.length() == 0 ? GONE : VISIBLE);
    }

    /** What the surface's own status line says, for verification. */
    final CharSequence statusText() {
        return status.getVisibility() == VISIBLE ? status.getText() : "";
    }

    /**
     * One option: an icon, what it is, and one line of what it does.
     *
     * <p>The description is the working part. "Sculpt" alone does not say that
     * the exact source is kept and reachable, and that is precisely the thing a
     * user has to trust before choosing it.
     */
    private static View buildOption(Context context, int id, int iconRes, CharSequence title,
                                    CharSequence description, OnClickListener onChosen) {
        final LinearLayout option = new LinearLayout(context);
        option.setId(id);
        option.setOrientation(LinearLayout.HORIZONTAL);
        // TOP, not CENTER_VERTICAL: aligned to the top the icon reads with the
        // title, and the options line up with each other even though their
        // descriptions wrap to different heights.
        option.setGravity(Gravity.TOP);
        option.setBackgroundResource(R.drawable.bg_chooser_card);
        final int pad = EditorControlStyles.dimen(context, R.dimen.chooser_option_padding);
        option.setPadding(pad, pad, pad, pad);
        option.setClickable(true);
        option.setFocusable(true);
        // The title is the whole control's name and the description follows
        // it, so a screen reader gets the same two facts a sighted user gets
        // and in the same order.
        option.setContentDescription(title + ". " + description);
        option.setOnClickListener(onChosen);

        final View optionIcon = EditorControlStyles.icon(context, iconRes,
                R.dimen.chooser_option_icon_size);
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
     * <p>A phone gets the window minus its margins, and anything wider gets the
     * cap, so the options never end up an arm apart on a tablet. Idempotent, so
     * the second measure pass of a traversal changes nothing.
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
     * Swallows every touch the options did not take.
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
