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
    // Theme
    // -----------------------------------------------------------------------

    /**
     * Resolves one semantic role against the theme the Activity applied.
     *
     * <p>This is the only way a colour reaches Java in this package. A caller
     * asks for a <b>role</b> — {@code R.attr.fsTextError}, not a colour — and
     * whichever of the two themes is in force answers. That is what stops a
     * second theme costing a second component tree: there is no
     * {@code if (light)} anywhere, and adding a third would touch
     * {@code attrs.xml} and {@code themes.xml} and nothing else.
     *
     * <p>Views built from resources need none of this — a background or a colour
     * state list carrying {@code ?attr/} is resolved by the platform when the
     * Context inflates it. This exists for the handful of places that must set a
     * colour imperatively: text roles, and the two brush sliders, which are
     * drawn onto a Canvas rather than composed from drawables.
     *
     * @param attrRes one of the {@code R.attr.fs*} roles declared in attrs.xml
     */
    static int themeColor(Context context, int attrRes) {
        final TypedValue value = new TypedValue();
        if (!context.getTheme().resolveAttribute(attrRes, value, true)) {
            // A role with no answer is a theme that forgot it, which is a build
            // mistake rather than something to paint around. Magenta is chosen
            // to be impossible to mistake for a designed colour.
            return 0xFFFF00FF;
        }
        if (value.type >= TypedValue.TYPE_FIRST_COLOR_INT
                && value.type <= TypedValue.TYPE_LAST_COLOR_INT) {
            return value.data;
        }
        return context.getColor(value.resourceId);
    }

    // -----------------------------------------------------------------------
    // Typography
    // -----------------------------------------------------------------------

    /**
     * Applies one typographic role.
     *
     * @param sizeRes  one of the {@code text_*} dimensions, which are named by
     *                 role rather than by size
     * @param colorAttr one of the {@code R.attr.fs*} colour roles
     * @param medium   whether this role carries the medium weight; reserved for
     *                 type that names a surface or states an exact value, so
     *                 weight stays a hierarchy signal instead of decoration
     */
    private static void applyRole(TextView view, int sizeRes, int colorAttr, boolean medium) {
        final Context context = view.getContext();
        view.setTextSize(TypedValue.COMPLEX_UNIT_PX, dimen(context, sizeRes));
        view.setTextColor(themeColor(context, colorAttr));
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
        applyRole(view, R.dimen.text_display, R.attr.fsTextPrimary, true);
        return view;
    }

    /** A surface's own name: toolbar context, inspector title, option title. */
    static TextView titleText(Context context, int id, CharSequence text) {
        final TextView view = new TextView(context);
        view.setId(id);
        view.setText(text);
        applyRole(view, R.dimen.text_title, R.attr.fsTextPrimary, true);
        return view;
    }

    /** Secondary prose: a status line, an option's one-line description. */
    static TextView captionText(Context context, int id, CharSequence text) {
        final TextView view = new TextView(context);
        view.setId(id);
        view.setText(text);
        applyRole(view, R.dimen.text_caption, R.attr.fsTextSecondary, false);
        applyWrappedLeading(view);
        return view;
    }

    static TextView sectionLabel(Context context, CharSequence text) {
        final TextView label = new TextView(context);
        label.setText(text);
        applyRole(label, R.dimen.text_label, R.attr.fsTextSecondary, true);
        label.setAllCaps(true);
        label.setLetterSpacing(0.08f);
        return label;
    }

    static TextView fieldLabel(Context context, CharSequence text) {
        final TextView label = new TextView(context);
        label.setText(text);
        applyRole(label, R.dimen.text_label, R.attr.fsTextSecondary, false);
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
        chip.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        return chip;
    }

    /**
     * A row in a list of things the scene holds: one Construction Body.
     *
     * <p>Left-aligned and borderless at rest, which is the difference between a
     * <i>list</i> and a column of buttons. Centred, boxed rows made a scene of
     * one body look like a second Add Body sitting above the real one; the eye
     * reads a left edge as content and a centred pill as a control.
     *
     * <p>The ACTIVE row is unchanged — {@link #setChipActive} still gives it the
     * tinted fill, the thicker accent border and the brightened label — so the
     * one shape in the list is the answer to the only question the surface asks.
     */
    static TextView listRow(Context context, int id, CharSequence text) {
        final TextView row = chip(context, id, text);
        row.setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
        row.setBackgroundResource(R.drawable.bg_list_row);
        return row;
    }

    /**
     * A secondary action that sits beneath content it adds to: Add Body.
     *
     * <p>Quieter than the content above it on purpose. A filled, bordered
     * <i>Add Body</i> beside borderless rows outweighed the list it belongs to —
     * the panel's own subject read as less important than the button that
     * appends to it. Secondary label, no box until pressed.
     */
    static TextView secondaryActionChip(Context context, int id, CharSequence text) {
        final TextView chip = chip(context, id, text);
        chip.setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
        chip.setTextColor(themeColor(context, R.attr.fsTextSecondary));
        chip.setBackgroundResource(R.drawable.bg_list_row);
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

    /**
     * The same repaint for a {@link #listRow}, whose resting background is
     * borderless rather than a box.
     *
     * <p>Separate from {@link #setChipActive} because that one restores
     * {@code bg_control}, which would quietly re-box a row the first time it was
     * deselected — the list would then look different before and after the user
     * had ever touched it.
     */
    static void setListRowActive(TextView row, boolean active) {
        row.setBackgroundResource(
                active ? R.drawable.bg_control_active : R.drawable.bg_list_row);
        row.setActivated(active);
    }

    /** A chip that names something the product does not have yet. */
    static void setChipReserved(TextView chip, CharSequence reason) {
        chip.setEnabled(false);
        chip.setTextColor(themeColor(chip.getContext(), R.attr.fsTextDisabled));
        chip.setBackgroundResource(R.drawable.bg_control_reserved);
        chip.setContentDescription(reason);
    }

    /**
     * A primary commit button.
     *
     * <p>Its label reads {@code fsTextOnPrimary}, not {@code fsTextPrimary}:
     * the two are the same on a dark theme, where a primary button is a dark
     * blue block carrying the same near-white as everything else, and they are
     * not on a light one, where the block is a solid accent that needs white on
     * it while body text is near-black.
     */
    static TextView primaryButton(Context context, int id, CharSequence text) {
        final TextView button = chip(context, id, text);
        button.setBackgroundResource(R.drawable.bg_primary);
        button.setTextColor(themeColor(context, R.attr.fsTextOnPrimary));
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
     * An icon-only control: Display, Objects, Hide UI, the restore chip, the
     * inspector toggle.
     *
     * <p>Square at the 44 dp touch floor, so an icon that reads at 20 dp is
     * still reachable with a fingertip. The content description is mandatory
     * and is the only name this control has.
     *
     * <p><b>Borderless at rest.</b> The touch target is unchanged — only the box
     * is gone. Three or four of these sit in a row on an already-opaque toolbar,
     * and drawing a filled, stroked rectangle around each one put competing
     * shapes across the top of the model for glyphs that read perfectly well on
     * the strip itself. Pressed feedback and the active box are both kept, so
     * the control still answers a touch immediately and a control whose panel is
     * open is the only one in the row wearing a shape.
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
        button.setBackgroundResource(R.drawable.bg_icon_button);
        button.setClickable(true);
        button.setFocusable(true);
        return button;
    }

    /** Marks an icon-only control active, background and tint together. */
    static void setIconButtonActive(ImageView button, boolean active) {
        button.setBackgroundResource(
                active ? R.drawable.bg_control_active : R.drawable.bg_icon_button);
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
