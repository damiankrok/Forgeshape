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

    /**
     * Applies the medium weight to a view that is not one of the named roles.
     *
     * <p>Exists so the family string stays in this file. The two brush readouts
     * are the only callers: they are a role of one, and giving them their own
     * {@code applyRole} entry would put a size and a colour here that only one
     * control uses — but a second copy of {@code "sans-serif-medium"} in a view
     * constructor is exactly the drift this class was made to stop.
     */
    static void applyMediumWeight(TextView view) {
        view.setTypeface(MEDIUM);
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

    /**
     * A heading over a group of rows: Position, Rotation, Display unit, Objects.
     *
     * <p><b>Not upper-cased, and that is a correctness fix rather than a taste
     * one.</b> A section heading can carry a unit — "Position (m)" — and
     * {@code setAllCaps} rendered that as "POSITION (M)". In SI, {@code m} is
     * the metre and {@code M} is not a unit at all; the nearest thing it
     * suggests is the mega- prefix, so a CAD panel was writing the wrong symbol
     * for its own authoritative unit. A transformation that can change what a
     * symbol MEANS has no business being applied to a string the product did not
     * choose character by character, and the next unit to appear in a heading
     * would have inherited the same bug silently.
     *
     * <p>What separates a heading from a field caption is now weight, tracking
     * and the section gap above it, which is what was doing the work anyway —
     * 11 sp upper-case with 0.08 tracking is also the least legible type in the
     * product, and headings are the one thing a user scans rather than reads.
     */
    static TextView sectionLabel(Context context, CharSequence text) {
        final TextView label = new TextView(context);
        label.setText(text);
        applyRole(label, R.dimen.text_label, R.attr.fsTextSecondary, true);
        // Kept, and reduced: tracking still separates a heading from the caption
        // under it, and at sentence case it does not have to work as hard.
        label.setLetterSpacing(0.04f);
        return label;
    }

    static TextView fieldLabel(Context context, CharSequence text) {
        final TextView label = new TextView(context);
        label.setText(text);
        applyRole(label, R.dimen.text_label, R.attr.fsTextSecondary, false);
        return label;
    }

    /**
     * The status and instructional line, in its own quiet capsule.
     *
     * <p>The capsule is the point. This line is <b>persistent</b> — it always
     * says something, and most of what it says is a standing hint rather than a
     * verdict — so given the weight of a full-width toolbar it read as a
     * permanent banner across the top of the model. In a small translucent
     * capsule sized to its own text it reads as an aside: legible when looked
     * at, ignorable when not. A real verdict still stands out, because a verdict
     * changes the text COLOUR against the same quiet ground.
     */
    static TextView statusText(Context context, int id) {
        final TextView status = captionText(context, id, "");
        status.setBackgroundResource(R.drawable.bg_status_pill);
        final int padH = dimen(context, R.dimen.chip_padding_horizontal);
        final int padV = dimen(context, R.dimen.row_gap_small);
        status.setPadding(padH, padV, padH, padV);
        status.setElevation(dimen(context, R.dimen.elevation_floating));
        // A surface over the viewport, so it swallows its own touches like every
        // other one. See controlGroup.
        status.setClickable(true);
        return status;
    }

    // -----------------------------------------------------------------------
    // Surfaces and depth
    // -----------------------------------------------------------------------

    // Three material tiers, and every surface in the product is exactly one of
    // them. The tier decides tone, opacity and depth TOGETHER, because those
    // three are one claim about what kind of thing a surface is — which is what
    // stops every panel reading as the same flat card.
    //
    // None of them blurs. The viewport is a SurfaceView and the platform cannot
    // blur what is behind one, so "glass" here is tone plus opacity plus a soft
    // shadow. That is a look the platform actually keeps; a blur that silently
    // degrades to a grey slab is worse than one that was never promised.

    /** Base chrome that is part of the window frame rather than over the model. */
    static void applyChromeSurface(View view) {
        view.setBackgroundResource(R.drawable.bg_chrome_surface);
    }

    /**
     * TIER 1 — a floating control group standing ON the viewport: the toolbar's
     * control capsules, the Tool Rail, the brush controls.
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
     * TIER 2 — an expanded context surface: the Display popover, the Objects
     * panel, the start chooser's panel.
     *
     * <p>Opaque where Tier 1 is translucent, because this one carries a body of
     * content that has to be read rather than glanced at.
     */
    static void applyContextSurface(View view) {
        view.setBackgroundResource(R.drawable.bg_surface_context);
        view.setElevation(dimen(view.getContext(), R.dimen.elevation_floating));
    }

    /**
     * A capsule holding two or three controls that belong together.
     *
     * <p>This is what replaced the Global Toolbar's full-width strip. A bar
     * spanning the window with a hairline under it reads as an Android app bar
     * whatever colour it is painted; two small groups with the model between and
     * behind them read as chrome belonging to a viewport. The padding is what
     * keeps the 48 dp controls inside from touching the capsule's own edge.
     */
    /**
     * Restyles a control so it nests correctly inside a floating capsule.
     *
     * <p>A capsule's members have to be CONCENTRIC with it, or the straight edge
     * of a small-cornered box leaves a crescent of capsule showing at each end
     * of the group — which is what an active icon control, the Objects capsule's
     * body label and the recessed Export well all did before UI-R4B, and which
     * reads as a rendering fault rather than as a control. See
     * {@code radius_control_inset}.
     *
     * <p>It is a restyle rather than a second set of factories because the
     * control is the same control: same size, same touch target, same content
     * tint, same states. Only its corner belongs to its host.
     *
     * @param resting the capsule form of this control's resting background
     */
    static void asCapsuleMember(View control, int resting) {
        control.setBackgroundResource(resting);
    }

    /**
     * Marks a capsule member active, fill-led and concentric with its host.
     *
     * <p>Separate from {@link #setChipActive} for the same reason
     * {@link #setListRowActive} is: that one restores {@code bg_control}, which
     * would quietly re-box a capsule member the first time it was deselected.
     */
    static void setCapsuleMemberActive(View control, boolean active) {
        control.setBackgroundResource(active
                ? R.drawable.bg_capsule_control_active : R.drawable.bg_capsule_control);
        control.setActivated(active);
    }

    static LinearLayout controlGroup(Context context) {
        final LinearLayout group = new LinearLayout(context);
        group.setOrientation(LinearLayout.HORIZONTAL);
        group.setGravity(Gravity.CENTER_VERTICAL);
        applyFloatingSurface(group);
        final int pad = dimen(context, R.dimen.toolbar_group_padding);
        group.setPadding(pad, pad, pad, pad);
        // The capsule is now the surface, so the capsule is what swallows a
        // touch its own controls did not take — a reach for Display may not
        // orbit the camera behind it. Clickable with no listener is the whole
        // mechanism: it consumes, and the background is not stateful, so nothing
        // is drawn for it. The transparent container AROUND the groups
        // deliberately does not consume, because the model is genuinely visible
        // and genuinely reachable between them.
        group.setClickable(true);
        return group;
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
     * <p>The ACTIVE row is the tinted fill plus the brightened label, and
     * nothing else — see {@link #setListRowActive}. It is the one shape in the
     * list, which is the answer to the only question the surface asks.
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

    /**
     * Enables or disables a chip, label and all.
     *
     * <p>Distinct from {@link #setChipReserved}: that one names something the
     * product does not have, and this one names something this product DOES
     * have that cannot succeed in the state the user is in right now — a
     * support plane that is fixed because the sketch already carries geometry.
     * The chip stays visible, because the state it reports is real information;
     * it simply cannot be pressed while it would be refused.
     */
    static void setChipEnabled(TextView chip, boolean enabled) {
        chip.setEnabled(enabled);
        chip.setClickable(enabled);
        chip.setAlpha(enabled ? 1.0f : 0.45f);
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
     * <p>Square at the 48 dp touch floor, so an icon that reads at 20 dp is
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

    /**
     * Marks an icon-only control active, background and tint together.
     *
     * <p>Both forms carry the capsule's own corner rather than the control
     * corner. Every icon control in the product either sits inside a floating
     * capsule — Display and Hide UI in the utility group, the precision toggle
     * in its own, the {@code +} in the Objects capsule — or stands alone in the
     * overlay, where a pill is what a lone round-ish control should be anyway.
     * At 10 dp inside a 26 dp capsule the active form showed a crescent of
     * capsule at the end of the row.
     */
    static void setIconButtonActive(ImageView button, boolean active) {
        button.setBackgroundResource(active
                ? R.drawable.bg_capsule_control_active : R.drawable.bg_icon_button);
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
