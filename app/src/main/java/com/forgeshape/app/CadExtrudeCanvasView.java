package com.forgeshape.app;

import android.content.Context;
import android.content.res.ColorStateList;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The compact, world-anchored extrude HUD (`CAD-UX-S1`, `CAD-EXT-R1`,
 * `CAD-VERTICAL-SLICE-R1`).
 *
 * <p>The <b>arrow</b> along the extrusion normal is drawn by the renderer from
 * the sketch overlay's own line list, because it is geometry-shaped and belongs
 * in the same space as the profile it grows out of. What is <b>not</b>
 * geometry-shaped is this: the exact distance, which has to be legible at any
 * zoom and editable by typing; which extent is being authored; what the
 * extrusion does to material; and the Flip that reverses which side the solid
 * grows on. Those are chrome, positioned over the viewport at the anchor native
 * reports — the pattern {@link SketchDimensionLabelView} and
 * {@link BodyDimensionLabelsView} already use.
 *
 * <p><b>Why it is icon-first and small.</b> The cluster this replaces was a row
 * of 60 dp TEXT pills — "One Side | Symmetric | Two Sides | 1 m | Flip | New
 * Body" — that spanned a phone's whole viewport and stood the value about
 * 100 dp from the shaft it measures. Now it is one row: the extent button
 * (showing the CURRENT mode's glyph), the exact value, the operation badge
 * (showing the CURRENT operation's glyph) and, in One Side alone, Flip. The
 * choices the old row laid out all at once live in two small palettes that open
 * under the control that owns them and close when a choice is made.
 *
 * <p><b>The value stands ON the arrow.</b> The anchor is the shaft's midpoint,
 * and it is the VALUE's centre that is placed there — the extent button to its
 * left, the badge and Flip to its right — through the one
 * {@link ViewportAnchorSpace} conversion, offset by where the value sits inside
 * the cluster ({@link CadHudPresentation#valueCentreOffset}).
 *
 * <p><b>The glyph and the touch target are two different facts.</b> Every icon
 * control is a 48 dp hit rectangle at every camera distance; only the GLYPH
 * inside it follows the camera-attached multiplier, as
 * {@code clamp(28 x scale, 24, 32)} dp. No view is scaled any more: scaling the
 * view scaled its hit area down with the glyph, which is why the old controls
 * had to be authored at 60 dp to survive the multiplier's 0.80 floor. The value
 * is a 32 dp pill inside a 48 dp hit row, for the same reason. With the Tool
 * Labels preference on, a one-word caption sits UNDER each glyph inside the same
 * control, so the row gains height rather than turning back into wide pills.
 *
 * <p><b>Holds no semantics.</b> The depth, the extent, the operation, which
 * operations can be chosen, whether the candidate would be refused, and the
 * anchor are all read back from native on every refresh, and every act here is
 * submitted whole. There is no draft direction, no draft depth, no draft
 * operation and no cached anchor. The only state this class keeps is which of
 * its OWN palettes is open and whether captions are drawn — presentation, which
 * a desktop adapter would replace along with this file without anything below
 * JNI noticing.
 *
 * <p><b>Three operations, and only what can be chosen is offered.</b> New Body
 * makes a new body; Add and Cut change the body the sketch stands on. The
 * operation palette holds exactly the operations native says are available now
 * and the others are ABSENT, not disabled. When only New Body is available the
 * badge still stands, because it says what this extrusion does, but it is a
 * statement and not a control. When the candidate would be refused the badge
 * wears an error outline and its accessible name says why; colour is never the
 * only carrier — each operation's glyph is a different SHAPE.
 *
 * <p><b>Three extents, one model.</b> One Side, Symmetric and Two Sides are
 * three ways of authoring the same two distances. Flip belongs to One Side
 * alone and is ABSENT in the other two, and the second side's value stands at
 * its own arrow in Two Sides alone.
 *
 * <p><b>Two states, and only one is ever shown.</b> While a sketch is in its
 * Ready state the extrude HUD stands at the arrow; over a committed CAD Body
 * with no session open, a single <i>Edit Sketch</i> control stands on the
 * body's own sketch, because a sketch survives its extrusion. Neither is drawn
 * when native says its anchor does not project on screen — a control with
 * nowhere honest to stand is hidden rather than placed at a guess.
 */
final class CadExtrudeCanvasView extends FrameLayout {

    /** Told what the user did; the workspace owns what each act means. */
    interface OnCanvasExtrudeAction {
        /**
         * A distance typed for ONE side — {@link NativeViewport#EXTRUDE_SIDE_POSITIVE}
         * along the support normal, {@code _NEGATIVE} against it.
         */
        void onExtrudeSideEntered(int side, double meters);

        void onExtrudeFlipRequested();

        /** One of {@link NativeViewport#EXTENT_ONE_SIDE} and its two siblings. */
        void onExtrudeExtentRequested(int mode);

        /**
         * One of {@link NativeViewport#OPERATION_NEW_BODY}, {@code _ADD} and
         * {@code _CUT} — only ever one native said was available when the
         * palette was drawn. Native still decides; the palette only asks.
         */
        void onExtrudeOperationRequested(int operation);

        void onCanvasEditSketchRequested(long bodyId);
    }

    /** Which of this view's own palettes is open: at most one, never with an editor. */
    private static final int PALETTE_NONE = 0;
    private static final int PALETTE_EXTENT = 1;
    private static final int PALETTE_OPERATION = 2;

    /** The extent choices, in {@link CadHudPresentation#EXTENTS} order. */
    private static final int[] EXTENT_CHOICE_IDS = {
            R.id.cad_extrude_extent_one_side, R.id.cad_extrude_extent_symmetric,
            R.id.cad_extrude_extent_two_sides};
    /** The operation choices, in {@link CadHudPresentation#OPERATIONS} order. */
    private static final int[] OPERATION_CHOICE_IDS = {
            R.id.cad_extrude_operation_new_body, R.id.cad_extrude_operation_add,
            R.id.cad_extrude_operation_cut};

    /**
     * One icon control: a container whose box is the 48 dp hit rectangle,
     * holding the glyph and, with Tool Labels on, a one-word caption under it.
     *
     * <p>The CONTAINER is the control — it carries the id, the click, the
     * selected and activated state, the background and the accessible name —
     * and its two children only draw, duplicating its state so the tint follows
     * without anything walking them. The glyph is its own {@code ImageView}
     * sized exactly to the glyph, so what is drawn and what is measured are the
     * same box. What was last applied is remembered only so a refresh on every
     * gesture sample re-lays nothing out unless the glyph, the caption or the
     * tint actually changed.
     */
    private static final class IconControl {
        final LinearLayout view;
        final ImageView glyph;
        final TextView caption;
        int iconRes;
        int captionRes;
        /** A fixed tint (an operation's colour), or null for the shared content tint. */
        ColorStateList tint;

        int appliedIcon;
        int appliedCaption;
        int appliedGlyph = -1;
        boolean appliedLabels;
        ColorStateList appliedTint;
        int appliedBackground;

        IconControl(LinearLayout view, ImageView glyph, TextView caption, int iconRes,
                    int captionRes) {
            this.view = view;
            this.glyph = glyph;
            this.caption = caption;
            this.iconRes = iconRes;
            this.captionRes = captionRes;
        }
    }

    private final InspectorHost host;
    private final OnCanvasExtrudeAction actions;
    /** The ONE viewport-anchor conversion; see {@link ViewportAnchorSpace}. */
    private final ViewportAnchorSpace anchorSpace;
    private final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
    private final float[] bodyAnchor = new float[3];

    private final float density;
    private final int hitPx;
    private final int captionGap;
    private final int captionPadH;
    private final int captionPadMin;
    private final int paletteGap;
    private final ColorStateList contentTint;
    /** Each operation's glyph tint, in {@link CadHudPresentation#OPERATIONS} order. */
    private final ColorStateList[] operationTints =
            new ColorStateList[CadHudPresentation.OPERATIONS.length];

    /** The primary cluster: extent button, the exact value, the badge, Flip. */
    private final LinearLayout cluster;
    private final IconControl extentButton;
    private final TextView reading;
    private final IconControl operation;
    private final IconControl flip;

    private final LinearLayout extentPalette;
    private final IconControl[] extentChoices = new IconControl[EXTENT_CHOICE_IDS.length];
    private final LinearLayout operationPalette;
    private final IconControl[] operationChoices = new IconControl[OPERATION_CHOICE_IDS.length];

    private final LinearLayout editor;
    private final EditText field;

    /**
     * The SECOND side's own value, standing at the second arrow.
     *
     * <p>Drawn in Two Sides alone, because that is the only mode with two
     * distances to state. Symmetric has two arrows and ONE distance, so a second
     * number beside the first would be the same value written twice.
     */
    private final LinearLayout secondCluster;
    private final TextView secondReading;
    private final LinearLayout secondEditor;
    private final EditText secondField;

    /** The retained-sketch control, shown over a committed CAD Body instead. */
    private final IconControl editSketch;

    /** Which palette is open. Presentation only; never a domain value. */
    private int openPalette = PALETTE_NONE;
    /** Whether icon controls draw their caption. Application preference; default off. */
    private boolean toolLabels;
    /** Whether the operation badge is a control at the last refresh. */
    private boolean operationHasChoice;
    /** The glyph size last applied to the extrude HUD, in pixels. Verification. */
    private int lastGlyphPx;

    /** The body the Edit Sketch control currently stands on, or 0. */
    private long sketchBodyId;
    /** The depth the reading last showed, in metres. Display only. */
    private double shownDepth;
    /** The second side's distance last shown, in metres. Display only. */
    private double shownSecond;
    private float anchorX;
    private float anchorY;
    private float secondAnchorX;
    private float secondAnchorY;

    /** The refusal last described on the badge, so a steady one is not re-worded per frame. */
    private int describedStatus = NativeViewport.CAD_OK;
    private String describedReason = "";

    CadExtrudeCanvasView(Context context, InspectorHost host, ViewportAnchorSpace anchorSpace,
                         OnCanvasExtrudeAction actions) {
        super(context);
        this.host = host;
        this.anchorSpace = anchorSpace;
        this.actions = actions;
        setId(R.id.cad_extrude_canvas);
        setVisibility(GONE);

        density = context.getResources().getDisplayMetrics().density;
        hitPx = CadHudPresentation.hitPx(density);
        captionGap = EditorControlStyles.dimen(context, R.dimen.cad_hud_caption_gap);
        captionPadH = EditorControlStyles.dimen(context,
                R.dimen.cad_hud_caption_padding_horizontal);
        captionPadMin = EditorControlStyles.dimen(context,
                R.dimen.cad_hud_caption_padding_vertical_min);
        paletteGap = EditorControlStyles.dimen(context, R.dimen.overlay_anchor_gap);
        contentTint = EditorControlStyles.contentTint(context);
        for (int i = 0; i < CadHudPresentation.OPERATIONS.length; i++) {
            operationTints[i] = ColorStateList.valueOf(EditorControlStyles.themeColor(context,
                    CadHudPresentation.operationColorAttr(CadHudPresentation.OPERATIONS[i])));
        }
        // Members stand edge to edge inside a capsule (see capsule()); the
        // editors keep the ordinary gap between a field and its Apply.
        final int editorGap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);

        // --- The primary cluster -------------------------------------------
        cluster = capsule(context, R.id.cad_extrude_cluster);

        // The extent button, FIRST: which combination of the two distances is
        // being authored is the question the value beside it answers.
        extentButton = iconControl(context, R.id.cad_extrude_extent,
                CadHudPresentation.extentIcon(NativeViewport.EXTENT_ONE_SIDE),
                CadHudPresentation.extentCaption(NativeViewport.EXTENT_ONE_SIDE),
                R.drawable.bg_hud_member);
        extentButton.view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                togglePalette(PALETTE_EXTENT);
            }
        });
        cluster.addView(extentButton.view, EditorControlStyles.wrap(0));

        reading = valueChip(context, R.id.cad_extrude_depth_value);
        reading.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openEditor();
            }
        });
        cluster.addView(reading, EditorControlStyles.wrap(0));

        operation = iconControl(context, R.id.cad_extrude_operation,
                CadHudPresentation.operationIcon(NativeViewport.OPERATION_NEW_BODY),
                CadHudPresentation.operationCaption(NativeViewport.OPERATION_NEW_BODY),
                R.drawable.bg_hud_member);
        operation.view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                // performClick reaches a listener whether or not the view is
                // clickable, so the badge checks for itself that there is a
                // choice to open rather than trusting its clickable flag.
                if (operationHasChoice) {
                    togglePalette(PALETTE_OPERATION);
                }
            }
        });
        cluster.addView(operation.view, EditorControlStyles.wrap(0));

        // Flip is ONE tap next to the geometry, which is the whole point of it
        // being here: the same act exists as a chip in the precision panel, and
        // both write exactly the same native direction.
        flip = iconControl(context, R.id.cad_extrude_flip, R.drawable.ic_extrude_flip,
                R.string.cad_hud_caption_flip, R.drawable.bg_hud_member);
        flip.view.setContentDescription(context.getString(R.string.cad_hud_flip_description));
        flip.view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                closePalettes();
                CadExtrudeCanvasView.this.actions.onExtrudeFlipRequested();
            }
        });
        cluster.addView(flip.view, EditorControlStyles.wrap(0));
        addView(cluster, wrapParams());

        // --- The two palettes ----------------------------------------------
        // Added AFTER the cluster so they draw over it where they meet.
        extentPalette = capsule(context, R.id.cad_extrude_extent_palette);
        for (int i = 0; i < EXTENT_CHOICE_IDS.length; i++) {
            final int mode = CadHudPresentation.EXTENTS[i];
            extentChoices[i] = iconControl(context, EXTENT_CHOICE_IDS[i],
                    CadHudPresentation.extentIcon(mode), CadHudPresentation.extentCaption(mode),
                    R.drawable.bg_hud_member);
            extentChoices[i].view.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    closeEditor();
                    closeSecondEditor();
                    closePalettes();
                    actions.onExtrudeExtentRequested(mode);
                }
            });
            extentPalette.addView(extentChoices[i].view, EditorControlStyles.wrap(0));
        }
        closedPalette(extentPalette);
        addView(extentPalette, wrapParams());

        operationPalette = capsule(context, R.id.cad_extrude_operation_palette);
        for (int i = 0; i < OPERATION_CHOICE_IDS.length; i++) {
            final int chosen = CadHudPresentation.OPERATIONS[i];
            operationChoices[i] = iconControl(context, OPERATION_CHOICE_IDS[i],
                    CadHudPresentation.operationIcon(chosen),
                    CadHudPresentation.operationCaption(chosen), R.drawable.bg_hud_member);
            operationChoices[i].tint = operationTints[i];
            operationChoices[i].view.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    closePalettes();
                    actions.onExtrudeOperationRequested(chosen);
                }
            });
            operationPalette.addView(operationChoices[i].view, EditorControlStyles.wrap(0));
        }
        closedPalette(operationPalette);
        addView(operationPalette, wrapParams());

        // --- The primary value's editor ------------------------------------
        editor = editorSurface(context, R.id.cad_extrude_depth_editor);
        field = distanceField(context, R.id.field_cad_extrude_depth);
        field.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    submit();
                    return true;
                }
                return false;
            }
        });
        editor.addView(field, EditorControlStyles.wrap(0));
        final TextView apply = applyButton(context, R.id.apply_cad_extrude_depth);
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                submit();
            }
        });
        editor.addView(apply, EditorControlStyles.wrap(editorGap));
        addView(editor, wrapParams());

        // --- The SECOND side, at the second arrow --------------------------
        // Its own anchor, its own editor, and exactly one value: Two Sides is
        // the only mode with a second distance to state. The capsule's
        // horizontal padding matches the pill's vertical gap, so the pill's
        // corners are concentric with the capsule's on all four sides.
        secondCluster = capsule(context, View.NO_ID);
        final int groupPad = EditorControlStyles.dimen(context, R.dimen.cad_hud_capsule_padding);
        final int valuePadH = EditorControlStyles.dimen(context,
                R.dimen.cad_hud_value_capsule_padding_horizontal);
        secondCluster.setPadding(valuePadH, groupPad, valuePadH, groupPad);
        secondCluster.setVisibility(GONE);
        secondReading = valueChip(context, R.id.cad_extrude_second_value);
        secondReading.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openSecondEditor();
            }
        });
        secondCluster.addView(secondReading, EditorControlStyles.wrap(0));
        addView(secondCluster, wrapParams());

        secondEditor = editorSurface(context, R.id.cad_extrude_second_editor);
        secondField = distanceField(context, R.id.field_cad_extrude_second);
        secondField.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    submitSecond();
                    return true;
                }
                return false;
            }
        });
        secondEditor.addView(secondField, EditorControlStyles.wrap(0));
        final TextView applySecond = applyButton(context, R.id.apply_cad_extrude_second);
        applySecond.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                submitSecond();
            }
        });
        secondEditor.addView(applySecond, EditorControlStyles.wrap(editorGap));
        addView(secondEditor, wrapParams());

        // --- The retained-sketch control -----------------------------------
        // Standing ALONE on the model, so it wears its own floating surface.
        editSketch = iconControl(context, R.id.cad_canvas_edit_sketch, R.drawable.ic_edit_sketch,
                R.string.cad_hud_caption_edit_sketch, R.drawable.bg_hud_lone_control);
        editSketch.view.setElevation(EditorControlStyles.dimen(context,
                R.dimen.elevation_floating));
        editSketch.view.setVisibility(GONE);
        editSketch.view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (sketchBodyId != NativeViewport.NO_OBJECT) {
                    CadExtrudeCanvasView.this.actions.onCanvasEditSketchRequested(sketchBodyId);
                }
            }
        });
        addView(editSketch.view, wrapParams());
    }

    // -----------------------------------------------------------------------
    // Construction helpers
    // -----------------------------------------------------------------------

    private static LayoutParams wrapParams() {
        return new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    /**
     * A floating capsule holding HUD controls: the Tier 1 surface every control
     * group standing on the model wears, 2 dp off its members so their
     * {@code cad_hud_member_radius} corners are concentric with its own. Its
     * members stand edge to edge: they are borderless at rest, so adjacent
     * 48 dp targets still read as separate glyphs, and the HUD covers that much
     * less of the work.
     *
     * <p>Clickable with no listener, so a touch that lands on the capsule
     * between two controls is consumed rather than orbiting the camera under
     * the HUD; the transparent container around the capsules deliberately does
     * not consume, so the model stays reachable everywhere else.
     */
    private static LinearLayout capsule(Context context, int id) {
        final LinearLayout capsule = new LinearLayout(context);
        if (id != View.NO_ID) {
            capsule.setId(id);
        }
        capsule.setOrientation(LinearLayout.HORIZONTAL);
        capsule.setGravity(Gravity.CENTER_VERTICAL);
        EditorControlStyles.applyFloatingSurface(capsule);
        final int pad = EditorControlStyles.dimen(context, R.dimen.cad_hud_capsule_padding);
        capsule.setPadding(pad, pad, pad, pad);
        capsule.setClickable(true);
        return capsule;
    }

    /**
     * Withdraws a palette while keeping it laid out.
     *
     * <p>{@code INVISIBLE} rather than {@code GONE}: a closed palette takes no
     * touch and is not in the accessibility tree, exactly like an absent one,
     * but it is still measured and laid out with the overlay — so the moment it
     * opens it is placed from a real layout rather than from a pending one that
     * would stand it at the window origin for a frame.
     */
    private static void closedPalette(View palette) {
        palette.setVisibility(INVISIBLE);
    }

    /** One icon control. Its glyph size and caption are applied on refresh. */
    private IconControl iconControl(Context context, int id, int iconRes, int captionRes,
                                    int background) {
        final LinearLayout view = new LinearLayout(context);
        view.setId(id);
        view.setOrientation(LinearLayout.VERTICAL);
        view.setGravity(Gravity.CENTER);
        view.setBackgroundResource(background);
        // The HIT rectangle, at every camera scale, reached by the box and never
        // by the glyph: the glyph inside it is what follows the camera, and the
        // gravity centres it (with its caption, when there is one) in the box.
        view.setMinimumWidth(hitPx);
        view.setMinimumHeight(hitPx);
        view.setClickable(true);
        view.setFocusable(true);

        final ImageView glyph = new ImageView(context);
        glyph.setScaleType(ImageView.ScaleType.FIT_CENTER);
        glyph.setDuplicateParentStateEnabled(true);
        // The container is the one accessible thing; the glyph and caption are
        // how it is drawn, not further things to announce.
        glyph.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        final int reference = CadHudPresentation.glyphPx(1.0, density);
        view.addView(glyph, new LinearLayout.LayoutParams(reference, reference));

        final TextView caption = new TextView(context);
        caption.setGravity(Gravity.CENTER);
        caption.setSingleLine(true);
        caption.setIncludeFontPadding(false);
        caption.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_label));
        caption.setDuplicateParentStateEnabled(true);
        caption.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        caption.setVisibility(GONE);
        final LinearLayout.LayoutParams captionParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        captionParams.topMargin = captionGap;
        view.addView(caption, captionParams);

        final IconControl control = new IconControl(view, glyph, caption, iconRes, captionRes);
        control.appliedBackground = background;
        applyIcon(control, reference);
        return control;
    }

    /**
     * The exact value: a 32 dp pill inside a 48 dp hit row.
     *
     * <p>Primary text in the medium weight, because it is the one exact value
     * on the HUD and the weight is reserved for type that states one. The
     * background is set BEFORE the padding: an inset background reports its
     * inset as padding and would otherwise replace the chip's own.
     */
    private TextView valueChip(Context context, int id) {
        final TextView chip = new TextView(context);
        chip.setId(id);
        chip.setGravity(Gravity.CENTER);
        chip.setSingleLine(true);
        chip.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        chip.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        EditorControlStyles.applyMediumWeight(chip);
        chip.setBackgroundResource(R.drawable.bg_hud_value);
        final int padH = EditorControlStyles.dimen(context, R.dimen.chip_padding_horizontal);
        chip.setPadding(padH, 0, padH, 0);
        chip.setMinimumHeight(hitPx);
        chip.setMinimumWidth(hitPx);
        chip.setClickable(true);
        chip.setFocusable(true);
        return chip;
    }

    /** The surface an exact-value editor stands in: a field and its Apply. */
    private static LinearLayout editorSurface(Context context, int id) {
        final LinearLayout surface = new LinearLayout(context);
        surface.setId(id);
        surface.setOrientation(LinearLayout.HORIZONTAL);
        surface.setGravity(Gravity.CENTER_VERTICAL);
        EditorControlStyles.applyContextSurface(surface);
        final int pad = EditorControlStyles.dimen(context, R.dimen.cad_hud_editor_padding);
        surface.setPadding(pad, pad, pad, pad);
        surface.setClickable(true);
        surface.setVisibility(GONE);
        return surface;
    }

    private TextView applyButton(Context context, int id) {
        final TextView apply = EditorControlStyles.primaryButton(context, id,
                context.getString(R.string.apply));
        apply.setMinimumHeight(hitPx);
        return apply;
    }

    /**
     * One exact-distance field. Both sides get an identical one, because both
     * take the same kind of value and a second set of rules for the second side
     * would be a second answer to what a distance is.
     */
    private EditText distanceField(Context context, int id) {
        final EditText made = new EditText(context);
        made.setId(id);
        made.setSingleLine(true);
        made.setBackgroundResource(R.drawable.bg_field);
        // The shared exact-value field padding, set AFTER the background so the
        // platform field's own default padding cannot survive under it.
        final int padH = EditorControlStyles.dimen(context, R.dimen.field_padding_horizontal);
        final int padV = EditorControlStyles.dimen(context, R.dimen.field_padding_vertical);
        made.setPadding(padH, padV, padH, padV);
        // A distance is unsigned — which side it is on is the SIDE, never a
        // sign, and a negative one is refused below JNI rather than
        // reinterpreted — but the sign is left typeable so a mistyped value is
        // reported by name rather than silently impossible to enter.
        made.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL
                | InputType.TYPE_NUMBER_FLAG_SIGNED);
        made.setKeyListener(DigitsKeyListener.getInstance("0123456789.-"));
        made.setImeOptions(EditorInfo.IME_ACTION_DONE);
        made.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        made.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        made.setMinimumWidth(EditorControlStyles.dimen(context, R.dimen.cad_canvas_field));
        made.setMinimumHeight(hitPx);
        return made;
    }

    // -----------------------------------------------------------------------
    // Glyphs, captions and states
    // -----------------------------------------------------------------------

    /**
     * Draws one control's glyph at {@code glyphPx}, with or without its
     * caption, inside the unchanged 48 dp hit rectangle.
     *
     * <p>Only the glyph's own box changes size; the control's box is the hit
     * rectangle at every scale. With a caption the one short word sits UNDER
     * the glyph — beside it would put the row back toward the wide pills this
     * HUD replaced — and the pair is centred in the box by its gravity.
     */
    private void applyIcon(IconControl control, int glyphPx) {
        final ColorStateList tint = control.tint != null ? control.tint : contentTint;
        if (control.appliedIcon == control.iconRes && control.appliedGlyph == glyphPx
                && control.appliedLabels == toolLabels && control.appliedTint == tint
                && control.appliedCaption == control.captionRes) {
            return;
        }
        if (control.appliedIcon != control.iconRes) {
            control.glyph.setImageResource(control.iconRes);
            control.appliedIcon = control.iconRes;
        }
        if (control.appliedTint != tint) {
            control.glyph.setImageTintList(tint);
            control.caption.setTextColor(tint);
            control.appliedTint = tint;
        }
        if (control.appliedGlyph != glyphPx) {
            final ViewGroup.LayoutParams params = control.glyph.getLayoutParams();
            params.width = glyphPx;
            params.height = glyphPx;
            control.glyph.setLayoutParams(params);
            control.appliedGlyph = glyphPx;
        }
        if (control.appliedLabels != toolLabels || control.appliedCaption != control.captionRes) {
            if (toolLabels) {
                control.caption.setText(control.captionRes);
                control.caption.setVisibility(VISIBLE);
                control.view.setPadding(captionPadH, captionPadMin, captionPadH, captionPadMin);
            } else {
                // No text at all while hidden, so nothing reads a caption the
                // user was not shown.
                control.caption.setText(null);
                control.caption.setVisibility(GONE);
                control.view.setPadding(0, 0, 0, 0);
            }
            control.appliedLabels = toolLabels;
            control.appliedCaption = control.captionRes;
        }
    }

    /** Swaps a control's background only when it actually changes. */
    private static void setControlBackground(IconControl control, int background) {
        if (control.appliedBackground != background) {
            control.view.setBackgroundResource(background);
            control.appliedBackground = background;
        }
    }

    /**
     * Marks one palette choice chosen or not, in every channel at once.
     *
     * <p>The chosen one is the only item in its palette wearing a SHAPE — the
     * selected fill — so the choice reads without its colour; {@code
     * setSelected}/{@code setActivated} and the accessible name carry it for a
     * screen reader and for verification.
     */
    private void markChoice(IconControl choice, boolean chosen, int descriptionRes) {
        final Context context = getContext();
        choice.view.setSelected(chosen);
        choice.view.setActivated(chosen);
        setControlBackground(choice, chosen ? R.drawable.bg_hud_member_active
                                     : R.drawable.bg_hud_member);
        choice.view.setContentDescription(context.getString(
                chosen ? R.string.cad_hud_choice_selected_description
                       : R.string.cad_hud_choice_description,
                context.getString(choice.captionRes), context.getString(descriptionRes)));
    }

    /**
     * Shows or hides captions on every icon control (Tool Labels).
     *
     * <p>An application preference the workspace reads from
     * {@link AppPreferencesStore} and hands over; this view stores the answer
     * only to draw with it. Nothing is re-read or re-placed here: the next
     * refresh applies it, which the caller triggers.
     */
    void setToolLabelsVisible(boolean visible) {
        toolLabels = visible;
    }

    /** Whether captions are drawn; verification. */
    boolean toolLabelsVisible() {
        return toolLabels;
    }

    // -----------------------------------------------------------------------
    // Refresh
    // -----------------------------------------------------------------------

    /**
     * Re-reads native and shows the extrude HUD, the retained-sketch control,
     * or neither.
     *
     * <p>Called on every state change and on every viewport gesture sample, so
     * the value follows a live arrow drag, the anchor follows an orbit and the
     * glyphs follow the camera-attached scale.
     */
    void refreshFromNative() {
        NativeViewport.cadExtrudeToolState(tool);
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] != 0.0) {
            showExtrudeCluster();
            return;
        }
        showRetainedSketchChip();
    }

    private void showExtrudeCluster() {
        if (tool[NativeViewport.CAD_EXTRUDE_ON_SCREEN] == 0.0) {
            // Behind the camera or off screen: hidden, deterministically. An
            // edge-clamped control for an anchor nobody can see would be a
            // control pointing at nothing.
            hide();
            return;
        }
        sketchBodyId = NativeViewport.NO_OBJECT;
        editSketch.view.setVisibility(GONE);
        final Context context = getContext();
        final LengthUnit unit = host.uiState().displayUnit();
        final int extent = extentMode();
        final int glyph = CadHudPresentation.glyphPx(tool[NativeViewport.CAD_EXTRUDE_SCALE],
                density);
        lastGlyphPx = glyph;
        // A live drag rewrites the value under the user; an editor or a palette
        // open over it would act on a state the arrow has already left behind.
        if (tool[NativeViewport.CAD_EXTRUDE_DRAGGING] != 0.0) {
            closeEditor();
            closeSecondEditor();
            closePalettes();
        }

        bindExtent(context, extent, glyph);
        bindOperation(context, glyph);

        // Flip belongs to One Side alone: the other two reach both sides
        // already, so there is no side left for it to choose. Absent rather
        // than shown and refused.
        flip.view.setVisibility(CadHudPresentation.flipPresent(extent) ? VISIBLE : GONE);
        applyIcon(flip, glyph);

        shownDepth = tool[NativeViewport.CAD_EXTRUDE_DEPTH];
        reading.setText(unit.formatWithUnit(shownDepth));
        reading.setContentDescription(context.getString(
                extent == NativeViewport.EXTENT_SYMMETRIC ? R.string.extrude_each_side_description
                        : extent == NativeViewport.EXTENT_TWO_SIDES
                                ? R.string.extrude_side_a_description
                                : R.string.extrude_depth_description,
                unit.formatWithUnit(shownDepth)));

        cluster.setVisibility(editorOpen() ? GONE : VISIBLE);
        setVisibility(VISIBLE);
        anchorX = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X];
        anchorY = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        if (editorOpen()) {
            placeAt(editor, anchorX, anchorY);
        } else {
            placeCluster();
        }

        // The second value exists in Two Sides alone, and only where native
        // says its anchor projects.
        final boolean secondShown = CadHudPresentation.secondValuePresent(extent)
                && tool[NativeViewport.CAD_EXTRUDE_SECOND_ON_SCREEN] != 0.0;
        if (!secondShown) {
            closeSecondEditor();
            secondCluster.setVisibility(GONE);
            return;
        }
        shownSecond = tool[NativeViewport.CAD_EXTRUDE_NEGATIVE];
        secondReading.setText(unit.formatWithUnit(shownSecond));
        secondReading.setContentDescription(context.getString(
                R.string.extrude_side_b_description, unit.formatWithUnit(shownSecond)));
        secondCluster.setVisibility(secondEditorOpen() ? GONE : VISIBLE);
        secondAnchorX = (float) tool[NativeViewport.CAD_EXTRUDE_SECOND_LABEL_X];
        secondAnchorY = (float) tool[NativeViewport.CAD_EXTRUDE_SECOND_LABEL_Y];
        placeAt(secondEditorOpen() ? secondEditor : secondCluster, secondAnchorX, secondAnchorY);
    }

    /** The extent button shows the mode in force; the palette marks it chosen. */
    private void bindExtent(Context context, int extent, int glyph) {
        extentButton.iconRes = CadHudPresentation.extentIcon(extent);
        extentButton.captionRes = CadHudPresentation.extentCaption(extent);
        applyIcon(extentButton, glyph);
        final boolean open = openPalette == PALETTE_EXTENT;
        extentButton.view.setActivated(open);
        setControlBackground(extentButton, open ? R.drawable.bg_hud_member_active
                                         : R.drawable.bg_hud_member);
        extentButton.view.setContentDescription(context.getString(
                R.string.cad_hud_extent_button_description,
                context.getString(extentButton.captionRes)));
        for (int i = 0; i < extentChoices.length; i++) {
            final int mode = CadHudPresentation.EXTENTS[i];
            markChoice(extentChoices[i], mode == extent,
                    CadHudPresentation.extentDescription(mode));
            applyIcon(extentChoices[i], glyph);
        }
    }

    /**
     * The badge shows the operation in force, in its colour and its shape; the
     * palette holds only what native offers now, and the badge is a control
     * only when that is more than the current answer.
     */
    private void bindOperation(Context context, int glyph) {
        final int current = (int) tool[NativeViewport.CAD_EXTRUDE_OPERATION];
        final int available = (int) tool[NativeViewport.CAD_EXTRUDE_OPERATIONS_AVAILABLE];
        final int status = (int) tool[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS];
        operationHasChoice = CadHudPresentation.operationBadgeHasChoice(current, available);
        if (!operationHasChoice && openPalette == PALETTE_OPERATION) {
            closePalettes();
        }
        operation.iconRes = CadHudPresentation.operationIcon(current);
        operation.captionRes = CadHudPresentation.operationCaption(current);
        operation.tint = operationTints[operationIndex(current)];
        applyIcon(operation, glyph);
        // A statement when there is nothing else to choose: not clickable, not
        // focusable, still read aloud through its description.
        operation.view.setClickable(operationHasChoice);
        operation.view.setFocusable(operationHasChoice);
        final boolean open = openPalette == PALETTE_OPERATION;
        final boolean refused = status != NativeViewport.CAD_OK;
        operation.view.setActivated(open);
        setControlBackground(operation, refused ? R.drawable.bg_hud_invalid
                : open ? R.drawable.bg_hud_member_active : R.drawable.bg_hud_member);
        String description = context.getString(operationHasChoice
                        ? R.string.cad_hud_operation_button_description
                        : R.string.cad_hud_operation_fixed_description,
                context.getString(operation.captionRes));
        if (refused) {
            description = context.getString(R.string.cad_hud_operation_invalid_description,
                    description, refusalReason(context, status));
        }
        operation.view.setContentDescription(description);

        for (int i = 0; i < operationChoices.length; i++) {
            final int offered = CadHudPresentation.OPERATIONS[i];
            // Only what can be chosen NOW is in the palette; the rest is
            // absent, not disabled.
            operationChoices[i].view.setVisibility(
                    CadHudPresentation.operationOffered(offered, available) ? VISIBLE : GONE);
            markChoice(operationChoices[i], offered == current,
                    CadHudPresentation.operationDescription(offered));
            applyIcon(operationChoices[i], glyph);
        }
    }

    /** The index of an operation in {@link CadHudPresentation#OPERATIONS}; New Body if unknown. */
    private static int operationIndex(int operation) {
        for (int i = 0; i < CadHudPresentation.OPERATIONS.length; i++) {
            if (CadHudPresentation.OPERATIONS[i] == operation) {
                return i;
            }
        }
        return 0;
    }

    /** The user-facing reason a candidate would be refused, worded once per refusal. */
    private String refusalReason(Context context, int status) {
        if (status != describedStatus || describedReason.isEmpty()) {
            describedStatus = status;
            describedReason = CadStatusMessages.describe(context, status);
        }
        return describedReason;
    }

    private void showRetainedSketchChip() {
        closeEditor();
        closeSecondEditor();
        closePalettes();
        cluster.setVisibility(GONE);
        secondCluster.setVisibility(GONE);
        // The control belongs to a committed CAD Body with no session open, and
        // to nothing else. Every condition below is one `sketchBeginEdit` would
        // refuse, so the control is absent rather than shown and then refused.
        final long body = NativeViewport.sceneActiveBodyId();
        if (body == NativeViewport.NO_OBJECT
                || !NativeViewport.cadBodySketchAnchor(body, bodyAnchor)) {
            hide();
            return;
        }
        sketchBodyId = body;
        final Context context = getContext();
        applyIcon(editSketch, CadHudPresentation.glyphPx(bodyAnchor[2], density));
        editSketch.view.setContentDescription(context.getString(
                R.string.edit_sketch_canvas_description, BodyLabels.of(context, body)));
        editSketch.view.setVisibility(VISIBLE);
        setVisibility(VISIBLE);
        anchorX = bodyAnchor[0];
        anchorY = bodyAnchor[1];
        placeAt(editSketch.view, anchorX, anchorY);
    }

    // -----------------------------------------------------------------------
    // Placement
    // -----------------------------------------------------------------------

    /**
     * Stands the cluster so its VALUE is centred on the arrow's anchor, then
     * hangs the open palette, if any, from the box the cluster was given.
     *
     * <p>The cluster is measured first under the parent's constraint — the
     * shared rule — so the value's position inside it is known; the cluster is
     * then placed at {@code anchor + offset}, and the shared clamp keeps the
     * whole of it inside the viewport. Near an edge the clamp wins and the value
     * stands as close to the anchor as the viewport allows.
     */
    private void placeCluster() {
        anchorSpace.measureUnderParent(cluster);
        final int width = cluster.getMeasuredWidth();
        final int height = cluster.getMeasuredHeight();
        final float offset = CadHudPresentation.valueCentreOffset(width,
                startWithin(cluster, reading), reading.getMeasuredWidth());
        anchorSpace.place(cluster, anchorX + offset, anchorY, width, height);
        if (openPalette == PALETTE_NONE) {
            return;
        }
        // The cluster's box in VIEWPORT pixels, by the same clamp the placement
        // just applied, so the palette hangs from where the cluster IS.
        final float left = CadHudPresentation.clampedStart(anchorX + offset, width,
                anchorSpace.viewportWidth());
        final float top = CadHudPresentation.clampedStart(anchorY, height,
                anchorSpace.viewportHeight());
        final View opener = openPalette == PALETTE_EXTENT ? extentButton.view : operation.view;
        final LinearLayout palette =
                openPalette == PALETTE_EXTENT ? extentPalette : operationPalette;
        anchorSpace.measureUnderParent(palette);
        final float centreX =
                left + startWithin(cluster, opener) + opener.getMeasuredWidth() * 0.5f;
        final float centreY = CadHudPresentation.paletteCentreY(top, height,
                palette.getMeasuredHeight(), paletteGap, anchorSpace.viewportHeight());
        // Centred under the control that opened it: an anchored surface grows
        // out of its invoking control. The shared clamp keeps it on screen.
        anchorSpace.place(palette, centreX, centreY, palette.getMeasuredWidth(),
                palette.getMeasuredHeight());
    }

    /**
     * Where a child starts inside a horizontal row, from MEASURED widths.
     *
     * <p>Measured rather than laid out, because this runs between the measure
     * this pass just made and the layout that has not happened yet: a child's
     * {@code getLeft()} still answers the previous frame.
     */
    private static int startWithin(LinearLayout row, View child) {
        int x = row.getPaddingLeft();
        for (int i = 0; i < row.getChildCount(); i++) {
            final View each = row.getChildAt(i);
            if (each.getVisibility() == GONE) {
                continue;
            }
            final LinearLayout.LayoutParams params =
                    (LinearLayout.LayoutParams) each.getLayoutParams();
            x += params.leftMargin;
            if (each == child) {
                return x;
            }
            x += each.getMeasuredWidth() + params.rightMargin;
        }
        return x;
    }

    /**
     * Centres one child on an anchor at its natural size.
     *
     * <p>No scale: the camera-attached multiplier sizes glyphs, never boxes.
     * The conversion from the viewport pixels native reports into this
     * container's own translation space, and the clamp against the real
     * viewport rather than this padded container, are the shared contract in
     * {@link ViewportAnchorSpace}.
     */
    private void placeAt(View shown, float x, float y) {
        anchorSpace.measureAndPlace(shown, x, y, 1.0f);
    }

    // -----------------------------------------------------------------------
    // Palettes
    // -----------------------------------------------------------------------

    /** Opens one palette, closing the other and any editor, or closes it again. */
    private void togglePalette(int which) {
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0) {
            return;
        }
        closeEditor();
        closeSecondEditor();
        openPalette = openPalette == which ? PALETTE_NONE : which;
        extentPalette.setVisibility(openPalette == PALETTE_EXTENT ? VISIBLE : INVISIBLE);
        operationPalette.setVisibility(openPalette == PALETTE_OPERATION ? VISIBLE : INVISIBLE);
        refreshFromNative();
    }

    /** Closes whichever palette is open. Changes nothing but what is drawn. */
    private void closePalettes() {
        if (openPalette == PALETTE_NONE) {
            return;
        }
        openPalette = PALETTE_NONE;
        closedPalette(extentPalette);
        closedPalette(operationPalette);
        extentButton.view.setActivated(false);
        setControlBackground(extentButton, R.drawable.bg_hud_member);
        operation.view.setActivated(false);
        if (operation.appliedBackground == R.drawable.bg_hud_member_active) {
            setControlBackground(operation, R.drawable.bg_hud_member);
        }
    }

    /** Whether the extent palette is open; verification. */
    boolean extentPaletteOpen() {
        return openPalette == PALETTE_EXTENT;
    }

    /** Whether the operation palette is open; verification. */
    boolean operationPaletteOpen() {
        return openPalette == PALETTE_OPERATION;
    }

    /** The glyph size the extrude HUD last drew, in pixels; verification. */
    int lastGlyphPx() {
        return lastGlyphPx;
    }

    // -----------------------------------------------------------------------
    // Editors
    // -----------------------------------------------------------------------

    /** Whether the numeric editor is open, for verification. */
    boolean editorOpen() {
        return editor.getVisibility() == VISIBLE;
    }

    /** Whether the SECOND side's numeric editor is open, for verification. */
    boolean secondEditorOpen() {
        return secondEditor.getVisibility() == VISIBLE;
    }

    /** The extent mode native last reported, for verification. */
    int extentMode() {
        return (int) tool[NativeViewport.CAD_EXTRUDE_EXTENT];
    }

    /** The body the retained-sketch control stands on, or 0. For verification. */
    long retainedSketchBodyId() {
        return sketchBodyId;
    }

    private void openEditor() {
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0) {
            return;
        }
        closePalettes();
        final LengthUnit unit = host.uiState().displayUnit();
        // Seeded with the CURRENT depth and selected whole, so the first
        // keystroke replaces rather than appends — the rule every exact-value
        // field in the product follows.
        field.setText(unit.format(shownDepth));
        field.selectAll();
        cluster.setVisibility(GONE);
        editor.setVisibility(VISIBLE);
        placeAt(editor, anchorX, anchorY);
        field.requestFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.showSoftInput(field, InputMethodManager.SHOW_IMPLICIT);
        }
    }

    /** Opens the SECOND side's editor, seeded with its current distance. */
    private void openSecondEditor() {
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0
                || !CadHudPresentation.secondValuePresent(extentMode())) {
            return;
        }
        closePalettes();
        final LengthUnit unit = host.uiState().displayUnit();
        secondField.setText(unit.format(shownSecond));
        secondField.selectAll();
        secondCluster.setVisibility(GONE);
        secondEditor.setVisibility(VISIBLE);
        placeAt(secondEditor, secondAnchorX, secondAnchorY);
        secondField.requestFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.showSoftInput(secondField, InputMethodManager.SHOW_IMPLICIT);
        }
    }

    /** Closes the editor without submitting. The authored depth is unchanged. */
    void closeEditor() {
        if (editor.getVisibility() != VISIBLE) {
            return;
        }
        editor.setVisibility(GONE);
        field.clearFocus();
        hideIme();
    }

    /** Closes the second side's editor without submitting. */
    void closeSecondEditor() {
        if (secondEditor.getVisibility() != VISIBLE) {
            return;
        }
        secondEditor.setVisibility(GONE);
        secondField.clearFocus();
        hideIme();
    }

    private void hideIme() {
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.hideSoftInputFromWindow(getWindowToken(), 0);
        }
    }

    /** Withdraws the whole surface. */
    void hide() {
        closeEditor();
        closeSecondEditor();
        closePalettes();
        cluster.setVisibility(GONE);
        secondCluster.setVisibility(GONE);
        editSketch.view.setVisibility(GONE);
        sketchBodyId = NativeViewport.NO_OBJECT;
        setVisibility(GONE);
    }

    /**
     * Parses the primary field and submits it as METRES on the primary side.
     *
     * <p>In One Side the SIDE the value lands on is the side the solid is
     * already on, so the positive selector means "the primary side" there; the
     * workspace resolves it against the direction native reports, which is what
     * keeps this class free of a second model of the extrusion.
     */
    private void submit() {
        final int extent = extentMode();
        final int label = extent == NativeViewport.EXTENT_SYMMETRIC ? R.string.label_each_side_canvas
                : extent == NativeViewport.EXTENT_TWO_SIDES ? R.string.label_side_a_canvas
                                                            : R.string.label_depth_canvas;
        submitField(field, label, NativeViewport.EXTRUDE_SIDE_POSITIVE);
    }

    private void submitSecond() {
        submitField(secondField, R.string.label_side_b_canvas,
                NativeViewport.EXTRUDE_SIDE_NEGATIVE);
    }

    /**
     * Parses one field and submits it as METRES on one side.
     *
     * <p>An unparseable value is reported by name and the field keeps focus so
     * it can be corrected; a value the domain refuses is reported the same way.
     * In neither case does one authored value move — a typed distance is
     * refused, never clamped, which is the one place it differs from a drag.
     */
    private void submitField(EditText from, int labelRes, int side) {
        final Context context = getContext();
        final String raw = from.getText().toString();
        final BigDecimal typed;
        try {
            typed = LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                    ? context.getString(R.string.field_empty, context.getString(labelRes))
                    : context.getString(R.string.field_not_a_number,
                                        context.getString(labelRes), raw.trim()),
                    R.attr.fsTextError);
            from.requestFocus();
            return;
        }
        actions.onExtrudeSideEntered(side,
                host.uiState().displayUnit().toMeters(typed).doubleValue());
    }

    /**
     * Layout params for the host: the whole overlay, with each surface placed
     * inside it by translation.
     *
     * <p>The container itself is never clickable and paints nothing, so a touch
     * that misses every capsule reaches the viewport exactly as it did before —
     * the same arrangement {@link BodyDimensionLabelsView} uses to stand three
     * labels at three anchors.
     */
    static FrameLayout.LayoutParams anchoredParams() {
        return new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
    }
}
