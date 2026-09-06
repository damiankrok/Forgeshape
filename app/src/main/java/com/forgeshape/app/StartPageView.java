package com.forgeshape.app;

import android.content.Context;
import android.graphics.Rect;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * The shape every <b>start page</b> takes: Home, and the New Project choice
 * (`SKETCH-UX-R1` A).
 *
 * <p><b>A page, not a modal.</b> It owns the whole window, it is opaque, and
 * nothing of the editor is drawn or reachable behind it. That is the correction
 * this class exists to make: Home used to be a raised card on a partial scrim
 * over the viewport, and with no project open the thing showing through that
 * scrim was an empty viewport — so the product's first screen read as a dialog
 * floating over a broken editor rather than as the place a project begins.
 *
 * <p>The composition, top to bottom: the product mark and its name, one line of
 * subtitle, the actions as full-width rows, the page's own status line, and an
 * optional quiet action at the foot (Back, on a subpage). A decorative motif
 * sits behind the content in one corner — static, non-interactive, and
 * incapable of being mistaken for a live viewport, because it contains nothing
 * that can be touched, moved or picked.
 *
 * <p><b>Owns no state and makes no native call.</b> A subclass reports which
 * action was pressed; {@link EditorWorkspaceView} owns what that means. Whether
 * a page is on screen at all is derived from native truth on every refresh and
 * is never remembered here.
 *
 * <p>Window insets are applied to the CONTENT, never to the page: the page is
 * the window's ground and the bars draw over it, exactly as the editor's chrome
 * is inset while the viewport is not.
 */
abstract class StartPageView extends FrameLayout {

    private final LinearLayout content;
    private final TextView status;
    private final ImageView motif;
    private int actionCount;

    StartPageView(Context context, int id, int contentId, CharSequence headline,
                  CharSequence caption) {
        super(context);
        setId(id);
        setBackgroundResource(R.drawable.bg_start_page);
        // Opaque and total: nothing behind a start page may take a touch, and
        // nothing behind one is drawn either. See onTouchEvent.
        setClickable(true);
        setFocusable(true);
        EditorControlStyles.allowChildShadows(this);

        // The decorative motif, behind everything, anchored to the bottom
        // trailing corner so it never sits under the actions.
        motif = new ImageView(context);
        motif.setImageResource(R.drawable.bg_start_page_motif);
        motif.setScaleType(ImageView.ScaleType.FIT_XY);
        motif.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        // Not clickable, not focusable, and not in the accessibility tree:
        // decoration that could take a touch would be a control.
        motif.setClickable(false);
        motif.setFocusable(false);
        final LayoutParams motifParams = new LayoutParams(
                EditorControlStyles.dimen(context, R.dimen.start_page_motif_size),
                EditorControlStyles.dimen(context, R.dimen.start_page_motif_size));
        motifParams.gravity = Gravity.BOTTOM | Gravity.END;
        addView(motif, motifParams);

        // The content scrolls, so a short window can still reach every action.
        final ScrollView scroller = new ScrollView(context);
        scroller.setClipToPadding(false);
        content = new LinearLayout(context);
        content.setId(contentId);
        content.setOrientation(LinearLayout.VERTICAL);
        scroller.addView(content, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        content.addView(buildMasthead(context, headline), EditorControlStyles.rowParams(0));
        content.addView(EditorControlStyles.captionText(context, View.NO_ID, caption),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));

        status = EditorControlStyles.captionText(context, View.NO_ID, "");
        status.setVisibility(GONE);

        final LayoutParams params = new LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        addView(scroller, params);
    }

    /**
     * The product mark and its name, side by side, then the page's headline.
     *
     * <p>The mark is the product's own geometry — an extruded profile — rather
     * than a letterform: it is the one place the page says what this program
     * makes before the user has made anything.
     */
    private static View buildMasthead(Context context, CharSequence headline) {
        final LinearLayout masthead = new LinearLayout(context);
        masthead.setOrientation(LinearLayout.VERTICAL);

        final LinearLayout wordmark = new LinearLayout(context);
        wordmark.setId(R.id.start_page_wordmark);
        wordmark.setOrientation(LinearLayout.HORIZONTAL);
        wordmark.setGravity(Gravity.CENTER_VERTICAL);
        final View mark = EditorControlStyles.icon(context, R.drawable.ic_forgeshape_mark,
                R.dimen.start_page_mark_size);
        mark.setId(R.id.start_page_mark);
        wordmark.addView(mark, mark.getLayoutParams());
        final TextView name = EditorControlStyles.displayText(context,
                context.getString(R.string.app_name));
        name.setId(R.id.start_page_product_name);
        final LinearLayout.LayoutParams nameParams = EditorControlStyles.rowParams(0);
        nameParams.leftMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        wordmark.addView(name, nameParams);
        // The mark and the name are ONE thing to a screen reader; the icon on
        // its own would otherwise be announced as an unlabelled image.
        wordmark.setContentDescription(context.getString(R.string.app_name));
        mark.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        masthead.addView(wordmark, EditorControlStyles.rowParams(0));

        final TextView title = EditorControlStyles.titleText(context, R.id.start_page_headline,
                headline);
        masthead.addView(title, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.section_gap)));
        return masthead;
    }

    /**
     * Adds one action row: an icon, what it is, and one line of what it does.
     *
     * <p>Full width, because on a page there is nothing beside it to balance
     * against — a centred card's proportions were the card's, not the page's.
     */
    final View addAction(int id, int iconRes, CharSequence title, CharSequence description,
                         OnClickListener onChosen) {
        final Context context = getContext();
        final boolean first = actionCount == 0;
        actionCount++;
        final LinearLayout action = new LinearLayout(context);
        action.setId(id);
        action.setOrientation(LinearLayout.HORIZONTAL);
        action.setGravity(Gravity.TOP);
        action.setBackgroundResource(R.drawable.bg_start_page_action);
        final int pad = EditorControlStyles.dimen(context, R.dimen.chooser_option_padding);
        action.setPadding(pad, pad, pad, pad);
        action.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
        action.setClickable(true);
        action.setFocusable(true);
        action.setContentDescription(title + ". " + description);
        action.setOnClickListener(onChosen);

        final View icon = EditorControlStyles.icon(context, iconRes,
                R.dimen.chooser_option_icon_size);
        final LinearLayout.LayoutParams iconParams =
                (LinearLayout.LayoutParams) icon.getLayoutParams();
        iconParams.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        action.addView(icon, iconParams);

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
        action.addView(text, textParams);

        final LinearLayout.LayoutParams params = EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, first ? R.dimen.section_gap : R.dimen.row_gap));
        params.width = ViewGroup.LayoutParams.MATCH_PARENT;
        content.addView(action, params);
        return action;
    }

    /**
     * Adds a section heading: the name of a GROUP of rows (`UI-PREF-R1` A3).
     *
     * <p>The Settings page is the first start page with more than one group,
     * and a group needs a name the way an inspector section does — the same
     * label role, the same section gap above it, so the two kinds of grouped
     * surface read as one product.
     */
    final TextView addSectionLabel(CharSequence label, boolean first) {
        final Context context = getContext();
        final TextView heading = EditorControlStyles.sectionLabel(context, label);
        content.addView(heading, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.section_gap)));
        return heading;
    }

    /** A field caption inside a group, naming one preference among several. */
    final TextView addFieldLabel(CharSequence label) {
        final Context context = getContext();
        final TextView caption = EditorControlStyles.fieldLabel(context, label);
        content.addView(caption, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap)));
        return caption;
    }

    /** One line of secondary prose under a heading: what the group changes. */
    final TextView addCaption(CharSequence text) {
        final Context context = getContext();
        final TextView caption = EditorControlStyles.captionText(context, View.NO_ID, text);
        content.addView(caption, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));
        return caption;
    }

    /**
     * One selectable option in a group: a full-width list row.
     *
     * <p>A list row rather than a chip, because the options have real names
     * ("Neutral Charcoal", "Right-handed (default)") that a row of chips would
     * wrap or clip, and because a short list of named, mutually exclusive
     * options is how a preference is chosen in every tool that has them. The
     * row already carries the 48 dp floor as hit area through its own box. The
     * bare label is kept as the row's tag, so a caller can decorate the text
     * (a check mark) without the row remembering two strings.
     */
    final TextView addOptionRow(int id, CharSequence label, OnClickListener onChosen) {
        final Context context = getContext();
        final TextView row = EditorControlStyles.listRow(context, id, label);
        row.setTag(label);
        row.setOnClickListener(onChosen);
        final LinearLayout.LayoutParams params = EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small));
        params.width = ViewGroup.LayoutParams.MATCH_PARENT;
        content.addView(row, params);
        return row;
    }

    /** Adds the page's status line under the actions. Called once. */
    final void addStatusLine() {
        content.addView(status, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(getContext(), R.dimen.section_gap)));
    }

    /** A quiet action at the foot of the page: the way back out of a subpage. */
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

    /** Writes a verdict on the page itself, or clears it with an empty message. */
    final void showStatus(CharSequence message, int colorAttr) {
        status.setText(message);
        status.setContentDescription(message);
        status.setTextColor(EditorControlStyles.themeColor(getContext(), colorAttr));
        status.setVisibility(message == null || message.length() == 0 ? GONE : VISIBLE);
    }

    /** What the page's own status line says, for verification. */
    final CharSequence statusText() {
        return status.getVisibility() == VISIBLE ? status.getText() : "";
    }

    /**
     * Pads the CONTENT off the system bars and the window edges.
     *
     * <p>The page itself is never inset: it is the window's ground and the bars
     * draw over it, which is the same rule the viewport follows. Wide windows
     * get the content capped and centred rather than an arm's width of empty
     * row, and the page's ground still reaches every edge.
     */
    void applyContentInsets(Rect insets) {
        final Context context = getContext();
        final int margin = EditorControlStyles.dimen(context, R.dimen.start_page_padding);
        content.setPadding(margin + insets.left, margin + insets.top, margin + insets.right,
                margin + insets.bottom);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final Context context = getContext();
        final int cap = EditorControlStyles.dimen(context, R.dimen.start_page_max_width);
        final int available = MeasureSpec.getSize(widthMeasureSpec);
        final int wanted = Math.min(cap, available);
        final ViewGroup.LayoutParams params = content.getLayoutParams();
        if (wanted > 0 && params.width != wanted) {
            params.width = wanted;
            content.setLayoutParams(params);
        }
        // The motif is proportional to the window rather than a fixed block, so
        // it stays a corner mark on a phone and does not become a wall on a
        // tablet.
        final int motifSize = Math.max(
                EditorControlStyles.dimen(context, R.dimen.start_page_motif_size),
                Math.min(available, MeasureSpec.getSize(heightMeasureSpec)) / 2);
        final ViewGroup.LayoutParams motifParams = motif.getLayoutParams();
        if (motifParams.width != motifSize) {
            motifParams.width = motifSize;
            motifParams.height = motifSize;
            motif.setLayoutParams(motifParams);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Swallows every touch the actions did not take.
     *
     * <p>Nothing behind a start page is operable — the editor chrome is not
     * drawn at all while one is up, and the viewport behind it holds no project
     * to orbit.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }
}
