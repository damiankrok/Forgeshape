package com.forgeshape.app;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The one place the Editor Workspace's controls get their look.
 *
 * <p>It exists because the two provisional panels each carried a private copy
 * of the same background, padding and colour helpers, and the copies had
 * already started to differ. Every metric and colour now comes from resources,
 * so a control's size is arguable in {@code dimens.xml} rather than buried in a
 * view constructor.
 *
 * <p><b>No colour is written in Java.</b> Backgrounds are
 * {@code res/drawable} state lists and text colours are
 * {@code res/color} state lists, which buys three things at once. Pressed and
 * active states come from the platform instead of from a repaint call, so
 * tapping a control gives feedback immediately rather than after native code
 * answers. Every instance of a background shares one parsed
 * {@code ConstantState}, so repainting the Tool Rail on a mode change no longer
 * allocates a fresh {@code GradientDrawable} per entry. And a later Light theme
 * (UI-R1B2) can supply a second set of values for the same names without any
 * view in this package learning about it.
 *
 * <p><b>Typography is five roles, not five numbers</b> — display, title, value,
 * body, caption — plus the Tool Rail's own caption size. A caller asks for the
 * role and does not choose a size.
 *
 * <p>Styling only. Nothing here reads or writes product state, makes a native
 * call, or knows what any control means.
 */
final class EditorControlStyles {

    /**
     * The medium weight, used for anything that names a surface or carries an
     * exact value.
     *
     * <p>A platform family rather than a bundled font file: ForgeShape ships no
     * asset for this, and {@code sans-serif-medium} is present on every release
     * the product supports.
     */
    private static final Typeface MEDIUM =
            Typeface.create("sans-serif-medium", Typeface.NORMAL);

    private EditorControlStyles() {
    }

    static int dp(Context context, int value) {
        return Math.round(context.getResources().getDisplayMetrics().density * value);
    }

    static int dimen(Context context, int dimenRes) {
        return context.getResources().getDimensionPixelSize(dimenRes);
    }

    /** Converts a raw window pixel extent to density-independent pixels. */
    static int toDp(Context context, int pixels) {
        final float density = context.getResources().getDisplayMetrics().density;
        return density > 0.0f ? Math.round(pixels / density) : pixels;
    }

    // -----------------------------------------------------------------------
    // Typography
    // -----------------------------------------------------------------------

    /**
     * Applies one typographic role.
     *
     * @param sizeRes one of the {@code text_*} dimensions, which are named by
     *                role rather than by size
     * @param medium  whether this role carries the medium weight; reserved for
     *                type that names a surface or states an exact value, so
     *                weight stays a hierarchy signal instead of decoration
     */
    private static void applyRole(TextView view, int sizeRes, int colorRes, boolean medium) {
        final Context context = view.getContext();
        view.setTextSize(TypedValue.COMPLEX_UNIT_PX, dimen(context, sizeRes));
        view.setTextColor(context.getColor(colorRes));
        if (medium) {
            view.setTypeface(MEDIUM);
        }
    }

    /** Extra leading, for anything that may wrap onto a second line. */
    private static void applyWrappedLeading(TextView view) {
        view.setLineSpacing(dimen(view.getContext(), R.dimen.text_line_spacing), 1.0f);
    }

    /** The chooser's headline: the only prominent type in the product. */
    static TextView displayText(Context context, CharSequence text) {
        final TextView view = new TextView(context);
        view.setText(text);
        applyRole(view, R.dimen.text_display, R.color.text_primary, true);
        return view;
    }

    /** A surface's own name: toolbar context, inspector title, option title. */
    static TextView titleText(Context context, int id, CharSequence text) {
        final TextView view = new TextView(context);
        view.setId(id);
        view.setText(text);
        applyRole(view, R.dimen.text_title, R.color.text_primary, true);
        return view;
    }

    /** Secondary prose: a status line, an option's one-line description. */
    static TextView captionText(Context context, int id, CharSequence text) {
        final TextView view = new TextView(context);
        view.setId(id);
        view.setText(text);
        applyRole(view, R.dimen.text_caption, R.color.text_secondary, false);
        applyWrappedLeading(view);
        return view;
    }

    static TextView sectionLabel(Context context, CharSequence text) {
        final TextView label = new TextView(context);
        label.setText(text);
        applyRole(label, R.dimen.text_label, R.color.text_secondary, true);
        label.setAllCaps(true);
        label.setLetterSpacing(0.08f);
        return label;
    }

    static TextView fieldLabel(Context context, CharSequence text) {
        final TextView label = new TextView(context);
        label.setText(text);
        applyRole(label, R.dimen.text_label, R.color.text_secondary, false);
        return label;
    }

    static TextView statusText(Context context, int id) {
        final TextView status = captionText(context, id, "");
        return status;
    }

    // -----------------------------------------------------------------------
    // Surfaces and depth
    // -----------------------------------------------------------------------

    /** Opaque chrome: text sits on it, so it may not be translucent. */
    static void applyChromeSurface(View view) {
        view.setBackgroundResource(R.drawable.bg_chrome_surface);
    }

    /**
     * Chrome that floats over the viewport: the rail, the brush controls, the
     * Display popover.
     *
     * <p>Elevation comes with the background because the two describe the same
     * claim — this surface is above the model. A surface that is beside the
     * model instead (the docked inspector) deliberately gets neither.
     */
    static void applyFloatingSurface(View view) {
        view.setBackgroundResource(R.drawable.bg_chrome_overlay);
        view.setElevation(dimen(view.getContext(), R.dimen.elevation_floating));
    }

    /**
     * Lets a container show its children's shadows.
     *
     * <p>A shadow is drawn outside the child's own bounds, so without this the
     * parent clips exactly the part that makes the surface look raised.
     */
    static void allowChildShadows(ViewGroup group) {
        group.setClipChildren(false);
        group.setClipToPadding(false);
    }

    // -----------------------------------------------------------------------
    // Text controls
    // -----------------------------------------------------------------------

    /**
     * A tappable pill whose state is meaningful: a unit, a primitive, a body
     * row, a display option.
     *
     * <p>{@code TextView} rather than {@code Button} so the height is exactly
     * the touch target requested and not the platform button's own minimum plus
     * insets, which is what made the previous rows wider than they looked.
     *
     * <p>At rest its label is secondary and when active it is primary, straight
     * from {@code control_content_tint} — so no caller has to remember to
     * repaint the label alongside the background.
     */
    static TextView chip(Context context, int id, CharSequence text) {
        final TextView chip = new TextView(context);
        chip.setId(id);
        chip.setText(text);
        chip.setContentDescription(text);
        chip.setGravity(Gravity.CENTER);
        chip.setSingleLine(true);
        chip.setTextSize(TypedValue.COMPLEX_UNIT_PX, dimen(context, R.dimen.text_body));
        chip.setTextColor(context.getColorStateList(R.color.control_content_tint));
        final int padding = dimen(context, R.dimen.chip_padding_horizontal);
        chip.setPadding(padding, 0, padding, 0);
        chip.setMinimumHeight(dimen(context, R.dimen.control_height));
        chip.setBackgroundResource(R.drawable.bg_control);
        chip.setClickable(true);
        chip.setFocusable(true);
        return chip;
    }

    /**
     * A chip that is an action rather than a selection: Add Body, Freeze again.
     *
     * <p>It has no active state to be in, so its label stays primary and only
     * the pressed state moves. Without this it would sit permanently dimmed
     * next to selectable chips and read as disabled.
     */
    static TextView actionChip(Context context, int id, CharSequence text) {
        final TextView chip = chip(context, id, text);
        chip.setTextColor(context.getColor(R.color.text_primary));
        return chip;
    }

    /** Repaints a chip for its selected state, background and label together. */
    static void setChipActive(TextView chip, boolean active) {
        chip.setBackgroundResource(
                active ? R.drawable.bg_control_active : R.drawable.bg_control);
        // The label follows from the state list, so this one call is the whole
        // repaint. setActivated is also what verification reads.
        chip.setActivated(active);
    }

    /** A chip that names something the product does not have yet. */
    static void setChipReserved(TextView chip, CharSequence reason) {
        chip.setEnabled(false);
        chip.setTextColor(chip.getContext().getColor(R.color.text_disabled));
        chip.setBackgroundResource(R.drawable.bg_control_reserved);
        chip.setContentDescription(reason);
    }

    /** A primary commit button. */
    static TextView primaryButton(Context context, int id, CharSequence text) {
        final TextView button = chip(context, id, text);
        button.setBackgroundResource(R.drawable.bg_primary);
        button.setTextColor(context.getColor(R.color.text_primary));
        button.setTypeface(MEDIUM);
        final int padding = dimen(context, R.dimen.control_padding_horizontal);
        button.setPadding(padding, 0, padding, 0);
        return button;
    }

    // -----------------------------------------------------------------------
    // Icons
    // -----------------------------------------------------------------------
    //
    // Every icon in the product is a local vector drawable on one 24 dp grid
    // with one 2 dp round stroke. They replaced Unicode glyphs in TextViews,
    // which were font-dependent, optically inconsistent with each other, and
    // had no disabled or active form at all.
    //
    // They are drawn white and tinted at use, so one drawable serves the idle,
    // active and disabled states and the tint comes from a state list rather
    // than from a repaint call.

    /**
     * A bare icon, for use inside a composed control.
     *
     * <p>It duplicates its parent's state, so the entry it sits in owns whether
     * it is active, pressed or disabled and nothing has to walk the entry
     * repainting children.
     */
    static ImageView icon(Context context, int iconRes, int sizeRes) {
        final ImageView icon = new ImageView(context);
        icon.setImageResource(iconRes);
        icon.setScaleType(ImageView.ScaleType.FIT_CENTER);
        icon.setImageTintList(context.getColorStateList(R.color.control_content_tint));
        icon.setDuplicateParentStateEnabled(true);
        final int size = dimen(context, sizeRes);
        icon.setLayoutParams(new LinearLayout.LayoutParams(size, size));
        return icon;
    }

    /**
     * An icon-only control: Display, Hide UI, the restore chip, the inspector
     * toggle.
     *
     * <p>Square at the 44 dp touch floor, so an icon that reads at 20 dp is
     * still reachable with a fingertip. The content description is mandatory
     * and is the only name this control has.
     */
    static ImageView iconButton(Context context, int id, int iconRes,
                                CharSequence description) {
        final ImageView button = new ImageView(context);
        button.setId(id);
        button.setImageResource(iconRes);
        button.setScaleType(ImageView.ScaleType.FIT_CENTER);
        button.setImageTintList(context.getColorStateList(R.color.control_content_tint));
        button.setContentDescription(description);
        final int inset = (dimen(context, R.dimen.icon_button_size)
                - dimen(context, R.dimen.icon_size)) / 2;
        button.setPadding(inset, inset, inset, inset);
        button.setBackgroundResource(R.drawable.bg_control);
        button.setClickable(true);
        button.setFocusable(true);
        return button;
    }

    /** Marks an icon-only control active, background and tint together. */
    static void setIconButtonActive(ImageView button, boolean active) {
        button.setBackgroundResource(
                active ? R.drawable.bg_control_active : R.drawable.bg_control);
        button.setActivated(active);
    }

    /** Square layout params at the icon-button touch size. */
    static LinearLayout.LayoutParams iconButtonParams(Context context, int leftMargin) {
        final int size = dimen(context, R.dimen.icon_button_size);
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(size, size);
        params.leftMargin = leftMargin;
        return params;
    }

    /** The tint every icon and control label reads its colour from. */
    static ColorStateList contentTint(Context context) {
        return context.getColorStateList(R.color.control_content_tint);
    }

    // -----------------------------------------------------------------------
    // Layout params
    // -----------------------------------------------------------------------

    static LinearLayout.LayoutParams rowParams(int topMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = topMargin;
        return params;
    }

    /** An equal share of a horizontal row, with a gap before all but the first. */
    static LinearLayout.LayoutParams evenShare(int leftMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        params.leftMargin = leftMargin;
        return params;
    }

    static LinearLayout.LayoutParams wrap(int leftMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.leftMargin = leftMargin;
        return params;
    }

    /** A flexible gap that pushes what follows to the far end of a row. */
    static View spacer(Context context) {
        final View spacer = new View(context);
        spacer.setLayoutParams(new LinearLayout.LayoutParams(0, 1, 1.0f));
        return spacer;
    }
}
