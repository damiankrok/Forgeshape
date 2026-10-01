package com.forgeshape.app;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Paint;
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
 * The world-anchored extrude HUD (`CAD-UX-S1`, `CAD-EXT-R1`,
 * `CAD-VERTICAL-SLICE-R1`, `CAD-FOUNDATION-C1`, `CAD-FOUNDATION-C2`).
 *
 * <p>The <b>arrow</b> along the extrusion normal and the <b>dimension leader</b>
 * beside its shaft are drawn by the renderer from the sketch overlay's own line
 * list, because they are geometry-shaped. What is <b>not</b> geometry-shaped is
 * this: the exact distance, which has to be legible and editable by typing;
 * which extent is being authored; what the extrusion does to material; and the
 * Flip that reverses which side the solid grows on. Those are chrome,
 * positioned over the viewport at the points native projects.
 *
 * <p><b>Two annotations</b> (`CAD-FOUNDATION-C2`). The exact VALUE stands ABOVE
 * its leader, rotated to it and kept upright
 * ({@link CadHudPresentation#layoutLeader}). The extent, the operation and, in
 * One Side, Flip are ONE ACTION PANEL: a rigid plate of glyphs built once at its
 * reference size and scaled as a unit by the camera-attached multiplier,
 * anchored just past the arrow's drawn point and shown WHOLE or not at all
 * ({@link CadHudPresentation#layoutPanel}). Its icons never drift apart, never
 * disappear one by one, and never shrink independently.
 *
 * <p><b>What is drawn and what is touched are two facts.</b> The plate is a
 * drawing: it is not clickable and its {@code setScale} therefore scales
 * nothing a finger aims at. The panel's touch target is ONE invisible group
 * proxy — never scaled, at least 48 dp each way and covering the plate — which
 * opens the ACTION PALETTE: ordinary, readable screen chrome holding the extent
 * choices, the operations native offers now, and Flip, each a full 48 dp
 * target, with Tool Labels captions when that preference is on. Three 48 dp
 * proxies centred on three shrunken glyphs would overlap and make a touch
 * choose between icons by distance; one proxy never has to.
 *
 * <p><b>Holds no semantics.</b> The depth, the extent, the operation, which
 * operations can be chosen, whether the candidate would be refused, and every
 * anchor are read back from native on every refresh, and every act here is
 * submitted whole. The only state this class keeps is whether its palette is
 * open, which editor is open and whether captions are drawn — presentation,
 * which a desktop adapter would replace along with this file without anything
 * below JNI noticing.
 *
 * <p><b>Three operations, and only what can be chosen is offered.</b> The
 * palette's operation row holds exactly the operations native says are
 * available now, and is absent when there is nothing else to choose; the
 * panel's operation glyph always states the operation in force, in its colour
 * AND its shape, and wears an error ring when the candidate would be refused.
 *
 * <p><b>Two states, and only one is ever shown.</b> While a sketch is in its
 * Ready state the extrude HUD stands on the arrow; over a committed CAD Body
 * with no session open, a single <i>Edit Sketch</i> control stands on the
 * body's own sketch. Neither is drawn when native says its anchor does not
 * project on screen — a control with nowhere honest to stand is hidden rather
 * than placed at a guess.
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

    /** The extent choices, in {@link CadHudPresentation#EXTENTS} order. */
    private static final int[] EXTENT_CHOICE_IDS = {
            R.id.cad_extrude_extent_one_side, R.id.cad_extrude_extent_symmetric,
            R.id.cad_extrude_extent_two_sides};
    /** The operation choices, in {@link CadHudPresentation#OPERATIONS} order. */
    private static final int[] OPERATION_CHOICE_IDS = {
            R.id.cad_extrude_operation_new_body, R.id.cad_extrude_operation_add,
            R.id.cad_extrude_operation_cut};

    /**
     * One palette control: a container whose box is at least the 48 dp hit
     * rectangle, holding the glyph and, with Tool Labels on, a one-word caption
     * under it.
     *
     * <p>The CONTAINER is the control — it carries the id, the click, the
     * selected and activated state, the background and the accessible name —
     * and its two children only draw, duplicating its state so the tint follows
     * without anything walking them. What was last applied is remembered only
     * so a refresh on every gesture sample re-lays nothing out unless the
     * glyph, the caption or the tint actually changed.
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

    /** The value text's gap above its leader, px. */
    private final float valueGapPx;
    /** The glyph at scale 1.0, px: the panel's reference glyph and every palette glyph. */
    private final int referenceGlyphPx;

    /** The primary value, above its leader. */
    private final TextView reading;

    /**
     * The action panel's plate: a DRAWING, built at its reference size and
     * scaled as one unit. Not clickable, so its scale is never a hit area.
     */
    private final LinearLayout plate;
    private final ImageView extentGlyph;
    private final ImageView operationGlyph;
    private final ImageView flipGlyph;
    /** The panel's ONE touch target: invisible, unscaled, at least 48 dp each way. */
    private final View panelProxy;

    /** The action palette: the extent row, the operation row and Flip. */
    private final LinearLayout actionsPalette;
    private final LinearLayout operationRow;
    private final IconControl[] extentChoices = new IconControl[EXTENT_CHOICE_IDS.length];
    private final IconControl[] operationChoices = new IconControl[OPERATION_CHOICE_IDS.length];
    private final IconControl flip;

    private final LinearLayout editor;
    private final EditText field;

    /**
     * The SECOND side's own value, standing at the second arrow.
     *
     * <p>Drawn in Two Sides alone, because that is the only mode with two
     * distances to state. Symmetric has two arrows and ONE distance, so a second
     * number beside the first would be the same value written twice.
     */
    private final TextView secondReading;
    private final LinearLayout secondEditor;
    private final EditText secondField;

    /** The retained-sketch control, shown over a committed CAD Body instead. */
    private final IconControl editSketch;

    /** Whether the action palette is open. Presentation only; never a domain value. */
    private boolean paletteOpen;
    /** Whether palette controls draw their caption. Application preference; default off. */
    private boolean toolLabels;
    /** How many glyphs the plate currently holds, or -1 before the first refresh. */
    private int appliedPanelIcons = -1;
    /** The operation glyph's applied tint and refusal ring, for change detection. */
    private ColorStateList appliedOperationTint;
    private boolean appliedOperationRefused;
    /** The proxy size last applied, px. */
    private int appliedHitWidth;
    private int appliedHitHeight;
    /** The panel glyph's drawn size last refresh, px. Verification. */
    private int lastGlyphPx;
    /** The value text size last applied, in sp. Verification and change detection. */
    private float lastValueTextSp;
    /** The primary leader's value layout at the last refresh. Verification. */
    private CadHudPresentation.LeaderLayout lastLayout = new CadHudPresentation.LeaderLayout();
    /** The panel layout at the last refresh. Verification. */
    private CadHudPresentation.PanelLayout lastPanel = new CadHudPresentation.PanelLayout();
    /** Whether the whole annotation collapsed at the last refresh. Verification. */
    private boolean lastCollapsed;

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

    /** The refusal last described, so a steady one is not re-worded per frame. */
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
        valueGapPx = CadHudPresentation.VALUE_GAP_DP * density;
        referenceGlyphPx = CadHudPresentation.glyphPx(1.0, density);
        // The editors keep the ordinary gap between a field and its Apply.
        final int editorGap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);

        // --- The primary value, above its leader ---------------------------
        reading = valueText(context, R.id.cad_extrude_depth_value);
        reading.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openEditor();
            }
        });
        addView(reading, wrapParams());

        // --- The action panel: one plate, one proxy -------------------------
        // The plate is laid out ONCE at its reference size (glyphs, gaps and
        // padding in fixed dp) and only ever scaled as a whole, about its
        // top-left corner so the shared placement arithmetic centres the scaled
        // box. Nothing inside it is ever re-laid out by zoom, so its internal
        // spacing cannot drift.
        plate = new LinearLayout(context);
        plate.setId(R.id.cad_extrude_panel_plate);
        plate.setOrientation(LinearLayout.HORIZONTAL);
        plate.setGravity(Gravity.CENTER_VERTICAL);
        plate.setBackgroundResource(R.drawable.bg_hud_panel);
        final int platePad = Math.round(CadHudPresentation.PANEL_PAD_DP * density);
        plate.setPadding(platePad, platePad, platePad, platePad);
        plate.setClickable(false);
        plate.setFocusable(false);
        // The proxy is the one accessible thing; the plate is how it is drawn.
        plate.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        final int plateGap = Math.round(CadHudPresentation.PANEL_GAP_DP * density);
        extentGlyph = plateGlyph(context, R.id.cad_extrude_extent,
                CadHudPresentation.extentIcon(NativeViewport.EXTENT_ONE_SIDE), 0);
        operationGlyph = plateGlyph(context, R.id.cad_extrude_operation,
                CadHudPresentation.operationIcon(NativeViewport.OPERATION_NEW_BODY), plateGap);
        flipGlyph = plateGlyph(context, R.id.cad_extrude_flip_glyph, R.drawable.ic_extrude_flip,
                plateGap);
        extentGlyph.setImageTintList(contentTint);
        flipGlyph.setImageTintList(contentTint);
        addView(plate, wrapParams());

        panelProxy = new View(context);
        panelProxy.setId(R.id.cad_extrude_panel);
        panelProxy.setClickable(true);
        panelProxy.setFocusable(true);
        panelProxy.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                togglePalette();
            }
        });
        addView(panelProxy, new LayoutParams(hitPx, hitPx));

        // --- The action palette ---------------------------------------------
        // Ordinary screen chrome, grown out of the panel that opens it, and
        // added AFTER the panel so it draws over it where they meet. Three rows,
        // one per question: how far (extent), what to material (operation), and
        // which side (Flip, One Side only).
        actionsPalette = new LinearLayout(context);
        actionsPalette.setId(R.id.cad_extrude_actions_palette);
        actionsPalette.setOrientation(LinearLayout.VERTICAL);
        EditorControlStyles.applyFloatingSurface(actionsPalette);
        final int pad = EditorControlStyles.dimen(context, R.dimen.cad_hud_capsule_padding);
        actionsPalette.setPadding(pad, pad, pad, pad);
        // Clickable with no listener, so a touch between two controls is
        // consumed rather than orbiting the camera under the palette.
        actionsPalette.setClickable(true);

        final LinearLayout extentRow = paletteRow(context, R.id.cad_extrude_extent_palette);
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
                    closePalette();
                    CadExtrudeCanvasView.this.actions.onExtrudeExtentRequested(mode);
                }
            });
            extentRow.addView(extentChoices[i].view, EditorControlStyles.wrap(0));
        }
        actionsPalette.addView(extentRow, rowParams(0));

        operationRow = paletteRow(context, R.id.cad_extrude_operation_palette);
        for (int i = 0; i < OPERATION_CHOICE_IDS.length; i++) {
            final int chosen = CadHudPresentation.OPERATIONS[i];
            operationChoices[i] = iconControl(context, OPERATION_CHOICE_IDS[i],
                    CadHudPresentation.operationIcon(chosen),
                    CadHudPresentation.operationCaption(chosen), R.drawable.bg_hud_member);
            operationChoices[i].tint = operationTints[i];
            operationChoices[i].view.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    closePalette();
                    CadExtrudeCanvasView.this.actions.onExtrudeOperationRequested(chosen);
                }
            });
            operationRow.addView(operationChoices[i].view, EditorControlStyles.wrap(0));
        }
        actionsPalette.addView(operationRow, rowParams(pad));

        // Flip is an ACT, not a choice: one tap reverses the side and closes
        // the palette. The same act exists as a chip in the precision panel,
        // and both write exactly the same native direction.
        flip = iconControl(context, R.id.cad_extrude_flip, R.drawable.ic_extrude_flip,
                R.string.cad_hud_caption_flip, R.drawable.bg_hud_member);
        flip.view.setContentDescription(context.getString(R.string.cad_hud_flip_description));
        flip.view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                closePalette();
                CadExtrudeCanvasView.this.actions.onExtrudeFlipRequested();
            }
        });
        actionsPalette.addView(flip.view, rowParams(pad));
        closedPalette(actionsPalette);
        addView(actionsPalette, wrapParams());

        // --- The primary value's editor ------------------------------------
        // Screen-aligned and readable, at the value's own anchor: typing is not
        // a drawing act, so the editor does not rotate with the leader.
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

        // --- The SECOND side, above its own leader -------------------------
        secondReading = valueText(context, R.id.cad_extrude_second_value);
        secondReading.setVisibility(GONE);
        secondReading.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openSecondEditor();
            }
        });
        addView(secondReading, wrapParams());

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
        setAnnotationVisible(false, false);
    }

    // -----------------------------------------------------------------------
    // Construction helpers
    // -----------------------------------------------------------------------

    private static LayoutParams wrapParams() {
        return new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private static LinearLayout.LayoutParams rowParams(int topMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = topMargin;
        return params;
    }

    /** One row of the action palette: its members stand edge to edge. */
    private static LinearLayout paletteRow(Context context, int id) {
        final LinearLayout row = new LinearLayout(context);
        row.setId(id);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        return row;
    }

    /**
     * One glyph on the plate at its REFERENCE size, with the reference gap
     * before it. Only the plate's scale ever changes what it measures on screen.
     */
    private ImageView plateGlyph(Context context, int id, int iconRes, int leftMargin) {
        final ImageView glyph = new ImageView(context);
        glyph.setId(id);
        glyph.setScaleType(ImageView.ScaleType.FIT_CENTER);
        glyph.setImageResource(iconRes);
        final int inset = Math.round(referenceGlyphPx * 0.12f);
        glyph.setPadding(inset, inset, inset, inset);
        glyph.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        final LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(referenceGlyphPx, referenceGlyphPx);
        params.leftMargin = leftMargin;
        plate.addView(glyph, params);
        return glyph;
    }

    /**
     * Withdraws the palette while keeping it laid out.
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

    /** One palette control. Its caption is applied on refresh. */
    private IconControl iconControl(Context context, int id, int iconRes, int captionRes,
                                    int background) {
        final LinearLayout view = new LinearLayout(context);
        view.setId(id);
        view.setOrientation(LinearLayout.VERTICAL);
        view.setGravity(Gravity.CENTER);
        view.setBackgroundResource(background);
        // The HIT rectangle, reached by the box and never by the glyph.
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
        view.addView(glyph, new LinearLayout.LayoutParams(referenceGlyphPx, referenceGlyphPx));

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
        applyIcon(control);
        return control;
    }

    /**
     * The exact value: TEXT standing above its leader, inside a transparent
     * touch proxy of at least 48 dp.
     *
     * <p>No pill and no background: the view's box is the proxy, it is rotated
     * with the leader so it covers the text it holds, and it paints nothing but
     * the text. A soft halo in the floating tone keeps the digits readable over
     * the model the way a drawing's dimension text is set clear of its lines.
     */
    private TextView valueText(Context context, int id) {
        final TextView text = new TextView(context);
        text.setId(id);
        text.setGravity(Gravity.CENTER);
        text.setSingleLine(true);
        text.setIncludeFontPadding(false);
        text.setTextSize(TypedValue.COMPLEX_UNIT_SP, CadHudPresentation.VALUE_TEXT_BASE_SP);
        text.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        EditorControlStyles.applyMediumWeight(text);
        text.setShadowLayer(3.0f * density, 0.0f, 0.0f,
                EditorControlStyles.themeColor(context, R.attr.fsSurfaceFloating));
        final int padH = Math.round(4.0f * density);
        text.setPadding(padH, 0, padH, 0);
        text.setMinimumHeight(hitPx);
        text.setMinimumWidth(hitPx);
        text.setClickable(true);
        text.setFocusable(true);
        return text;
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
     * Draws one palette control's glyph, with or without its caption, inside
     * its unchanged 48 dp hit rectangle. With a caption the one short word sits
     * UNDER the glyph and the pair is centred in the box by its gravity.
     */
    private void applyIcon(IconControl control) {
        final ColorStateList tint = control.tint != null ? control.tint : contentTint;
        final boolean captioned = CadHudPresentation.captionShown(toolLabels, false);
        if (control.appliedIcon == control.iconRes && control.appliedLabels == captioned
                && control.appliedTint == tint && control.appliedCaption == control.captionRes) {
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
        if (control.appliedLabels != captioned || control.appliedCaption != control.captionRes) {
            if (captioned) {
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
            control.appliedLabels = captioned;
            control.appliedCaption = control.captionRes;
        }
    }

    /** Swaps a palette control's state shape only when it actually changes. */
    private static void setControlBackground(IconControl control, int background) {
        if (control.appliedBackground != background) {
            control.view.setBackgroundResource(background);
            control.appliedBackground = background;
        }
    }

    /**
     * Marks one palette choice chosen or not, in every channel at once.
     *
     * <p>The chosen one is the only item in its row wearing a SHAPE — the
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
     * Shows or hides captions on every palette control (Tool Labels).
     *
     * <p>An application preference the workspace reads from
     * {@link AppPreferencesStore} and hands over; this view stores the answer
     * only to draw with it. The next refresh applies it, which the caller
     * triggers.
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
     * the value follows a live arrow drag, the panel follows an orbit and both
     * follow the camera-attached scale.
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
        if (tool[NativeViewport.CAD_EXTRUDE_ON_SCREEN] == 0.0 || !anchorSpace.ready()) {
            // Behind the camera: hidden, deterministically. A control for an
            // anchor nobody can see would be a control pointing at nothing.
            hide();
            return;
        }
        sketchBodyId = NativeViewport.NO_OBJECT;
        editSketch.view.setVisibility(GONE);
        final Context context = getContext();
        final LengthUnit unit = host.uiState().displayUnit();
        final int extent = extentMode();
        final double scale = tool[NativeViewport.CAD_EXTRUDE_SCALE];
        lastGlyphPx = CadHudPresentation.glyphPx(scale, density);
        applyValueTextSize(CadHudPresentation.valueTextSp(scale));
        // A live drag rewrites the value under the user; an editor or the
        // palette open over it would act on a state the arrow has already left.
        if (tool[NativeViewport.CAD_EXTRUDE_DRAGGING] != 0.0) {
            closeEditor();
            closeSecondEditor();
            closePalette();
        }

        final int iconCount = CadHudPresentation.panelIconCount(extent);
        bindPanel(context, extent, iconCount);

        shownDepth = tool[NativeViewport.CAD_EXTRUDE_DEPTH];
        reading.setText(unit.formatWithUnit(shownDepth));
        reading.setContentDescription(context.getString(
                extent == NativeViewport.EXTENT_SYMMETRIC ? R.string.extrude_each_side_description
                        : extent == NativeViewport.EXTENT_TWO_SIDES
                                ? R.string.extrude_side_a_description
                                : R.string.extrude_depth_description,
                unit.formatWithUnit(shownDepth)));

        setVisibility(VISIBLE);
        final CadHudPresentation.LeaderLayout layout = layoutOn(reading,
                NativeViewport.CAD_EXTRUDE_LEADER_ON_SCREEN,
                NativeViewport.CAD_EXTRUDE_LEADER_START_X);
        lastLayout = layout;
        final CadHudPresentation.PanelLayout panel = layoutPanel(scale);
        // The whole annotation collapses together or not at all: the value
        // never outlives the panel beside it, nor the panel the value.
        lastCollapsed = CadHudPresentation.annotationCollapsed(
                tool[NativeViewport.CAD_EXTRUDE_CLAMP] == NativeViewport.CAD_EXTRUDE_CLAMP_LOW,
                layout.valueVisible ? layout.visibleLength : 0.0f,
                reading.getPaint().measureText(reading.getText().toString()));
        if (lastCollapsed) {
            panel.visible = false;
            layout.valueVisible = false;
        }
        lastPanel = panel;
        if (!layout.valueVisible && !panel.visible) {
            hide();
            return;
        }
        if (layout.valueVisible) {
            anchorX = layout.valueX;
            anchorY = layout.valueY;
        } else {
            closeEditor();
        }
        if (editorOpen()) {
            setAnnotationVisible(false, false);
            closePalette();
            placeAt(editor, anchorX, anchorY);
        } else {
            reading.setVisibility(layout.valueVisible ? VISIBLE : GONE);
            if (layout.valueVisible) {
                placeValue(reading, layout);
            }
            placePanel(panel);
        }

        // The second value exists in Two Sides alone, above its OWN leader, and
        // only where native says that leader projects.
        final boolean secondShown = !lastCollapsed
                && CadHudPresentation.secondValuePresent(extent)
                && tool[NativeViewport.CAD_EXTRUDE_SECOND_ON_SCREEN] != 0.0;
        if (!secondShown) {
            closeSecondEditor();
            secondReading.setVisibility(GONE);
            return;
        }
        shownSecond = tool[NativeViewport.CAD_EXTRUDE_NEGATIVE];
        secondReading.setText(unit.formatWithUnit(shownSecond));
        secondReading.setContentDescription(context.getString(
                R.string.extrude_side_b_description, unit.formatWithUnit(shownSecond)));
        final CadHudPresentation.LeaderLayout second = layoutOn(secondReading,
                NativeViewport.CAD_EXTRUDE_SECOND_LEADER_ON_SCREEN,
                NativeViewport.CAD_EXTRUDE_SECOND_LEADER_START_X);
        if (!second.valueVisible) {
            closeSecondEditor();
            secondReading.setVisibility(GONE);
            return;
        }
        secondAnchorX = second.valueX;
        secondAnchorY = second.valueY;
        if (secondEditorOpen()) {
            secondReading.setVisibility(GONE);
            placeAt(secondEditor, secondAnchorX, secondAnchorY);
        } else {
            secondReading.setVisibility(VISIBLE);
            placeValue(secondReading, second);
        }
    }

    /** Applies the value text size to both values, only when it changes. */
    private void applyValueTextSize(float sp) {
        if (sp == lastValueTextSp) {
            return;
        }
        reading.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        secondReading.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        lastValueTextSp = sp;
    }

    /**
     * Lays one value out above the leader native projected: slot {@code onScreen}
     * says whether it projects, {@code startX..startX+3} are its start x, y and
     * end x, y.
     */
    private CadHudPresentation.LeaderLayout layoutOn(TextView value, int onScreen, int startX) {
        if (tool[onScreen] == 0.0) {
            return new CadHudPresentation.LeaderLayout();
        }
        anchorSpace.measureUnderParent(value);
        final Paint.FontMetrics metrics = value.getPaint().getFontMetrics();
        final float textH = metrics.descent - metrics.ascent;
        return CadHudPresentation.layoutLeader(
                (float) tool[startX], (float) tool[startX + 1],
                (float) tool[startX + 2], (float) tool[startX + 3],
                anchorSpace.viewportWidth(), anchorSpace.viewportHeight(), textH, valueGapPx);
    }

    /**
     * The panel against the arrow's drawn point: its screen direction is from
     * the shaft's middle (the label anchor) to that point — the line the panel
     * stands on and turns with.
     */
    private CadHudPresentation.PanelLayout layoutPanel(double scale) {
        final boolean head = tool[NativeViewport.CAD_EXTRUDE_HEAD_ON_SCREEN] != 0.0;
        final float headX = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_X];
        final float headY = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_Y];
        // The plate's OWN measured reference box (its glyph count already
        // bound): per-child pixel rounding makes it differ from the dp sum by
        // a pixel or two, and the proxy must cover what is actually drawn.
        anchorSpace.measureUnderParent(plate);
        return CadHudPresentation.layoutPanel(head, headX, headY,
                headX - (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X],
                headY - (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y],
                plate.getMeasuredWidth(), plate.getMeasuredHeight(), scale, density,
                anchorSpace.viewportWidth(), anchorSpace.viewportHeight());
    }

    /**
     * Binds the panel's glyphs and the palette's rows to what native says now:
     * the extent in force, the operation in force (colour AND shape, with an
     * error ring when the candidate would be refused), Flip in One Side alone,
     * and in the palette exactly the operations that can be chosen.
     */
    private void bindPanel(Context context, int extent, int iconCount) {
        final int current = (int) tool[NativeViewport.CAD_EXTRUDE_OPERATION];
        final int available = (int) tool[NativeViewport.CAD_EXTRUDE_OPERATIONS_AVAILABLE];
        final int status = (int) tool[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS];
        final boolean refused = status != NativeViewport.CAD_OK;

        // The plate. Its glyph count is its only layout-changing fact.
        if (appliedPanelIcons != iconCount) {
            flipGlyph.setVisibility(iconCount == 3 ? VISIBLE : GONE);
            appliedPanelIcons = iconCount;
        }
        final int extentCaption = CadHudPresentation.extentCaption(extent);
        extentGlyph.setImageResource(CadHudPresentation.extentIcon(extent));
        extentGlyph.setContentDescription(context.getString(
                R.string.cad_hud_extent_state_description, context.getString(extentCaption)));
        final int operationCaption = CadHudPresentation.operationCaption(current);
        operationGlyph.setImageResource(CadHudPresentation.operationIcon(current));
        final ColorStateList operationTint = operationTints[operationIndex(current)];
        if (appliedOperationTint != operationTint) {
            operationGlyph.setImageTintList(operationTint);
            appliedOperationTint = operationTint;
        }
        if (appliedOperationRefused != refused) {
            // Colour is never the only carrier: the ring is a SHAPE, and the
            // accessible name says why.
            if (refused) {
                operationGlyph.setBackgroundResource(R.drawable.bg_hud_glyph_invalid);
            } else {
                operationGlyph.setBackground(null);
            }
            appliedOperationRefused = refused;
        }
        String operationDescription = context.getString(
                R.string.cad_hud_operation_fixed_description, context.getString(operationCaption));
        if (refused) {
            operationDescription = context.getString(R.string.cad_hud_operation_invalid_description,
                    operationDescription, refusalReason(context, status));
        }
        operationGlyph.setContentDescription(operationDescription);
        plate.setActivated(paletteOpen);
        panelProxy.setActivated(paletteOpen);
        String panelDescription = context.getString(R.string.cad_hud_panel_description,
                context.getString(extentCaption), context.getString(operationCaption));
        if (refused) {
            panelDescription = context.getString(R.string.cad_hud_operation_invalid_description,
                    panelDescription, refusalReason(context, status));
        }
        panelProxy.setContentDescription(panelDescription);

        // The palette.
        for (int i = 0; i < extentChoices.length; i++) {
            final int mode = CadHudPresentation.EXTENTS[i];
            markChoice(extentChoices[i], mode == extent,
                    CadHudPresentation.extentDescription(mode));
            applyIcon(extentChoices[i]);
        }
        // Only what can be chosen NOW is in the row; the rest is absent, not
        // disabled — and with nothing else to choose the row itself is absent,
        // because a row offering exactly the current answer chooses nothing.
        operationRow.setVisibility(CadHudPresentation.operationBadgeHasChoice(current, available)
                ? VISIBLE : GONE);
        for (int i = 0; i < operationChoices.length; i++) {
            final int offered = CadHudPresentation.OPERATIONS[i];
            operationChoices[i].view.setVisibility(
                    CadHudPresentation.operationOffered(offered, available) ? VISIBLE : GONE);
            markChoice(operationChoices[i], offered == current,
                    CadHudPresentation.operationDescription(offered));
            applyIcon(operationChoices[i]);
        }
        // Flip belongs to One Side alone: the other two reach both sides
        // already, so there is no side left for it to choose. Absent rather
        // than shown and refused.
        flip.view.setVisibility(CadHudPresentation.flipPresent(extent) ? VISIBLE : GONE);
        applyIcon(flip);
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
        closePalette();
        setAnnotationVisible(false, false);
        secondReading.setVisibility(GONE);
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
        final int glyphPx = CadHudPresentation.loneGlyphPx(bodyAnchor[2], density);
        final ViewGroup.LayoutParams params = editSketch.glyph.getLayoutParams();
        if (params.width != glyphPx) {
            params.width = glyphPx;
            params.height = glyphPx;
            editSketch.glyph.setLayoutParams(params);
        }
        applyIcon(editSketch);
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
     * Stands the panel WHOLE where the layout says — the plate scaled as one
     * unit, the proxy unscaled on the same centre — or withdraws both; then
     * hangs the open palette, if any, from the panel.
     */
    private void placePanel(CadHudPresentation.PanelLayout panel) {
        if (!panel.visible) {
            plate.setVisibility(GONE);
            panelProxy.setVisibility(GONE);
            closePalette();
            return;
        }
        plate.setVisibility(VISIBLE);
        panelProxy.setVisibility(VISIBLE);
        // Pivot at the plate's own centre: scale and rotation then both leave
        // that centre where the unscaled box's centre is placed, so the drawn
        // plate is centred on the layout's point at every scale and angle.
        final float pivotX = plate.getMeasuredWidth() * 0.5f;
        final float pivotY = plate.getMeasuredHeight() * 0.5f;
        if (plate.getPivotX() != pivotX || plate.getPivotY() != pivotY) {
            plate.setPivotX(pivotX);
            plate.setPivotY(pivotY);
        }
        if (plate.getScaleX() != panel.scale) {
            plate.setScaleX(panel.scale);
            plate.setScaleY(panel.scale);
        }
        if (plate.getRotation() != panel.rotation) {
            plate.setRotation(panel.rotation);
        }
        // Unclamped: the layout already proved the turned plate's covering box
        // fits, and a clamp on the UNSCALED box would move a shrunken plate off
        // the centre its proxy stands on.
        anchorSpace.placeCentred(plate, panel.centreX, panel.centreY,
                plate.getMeasuredWidth(), plate.getMeasuredHeight());
        final int hitW = (int) panel.hitWidth;
        final int hitH = (int) panel.hitHeight;
        if (hitW != appliedHitWidth || hitH != appliedHitHeight) {
            final ViewGroup.LayoutParams params = panelProxy.getLayoutParams();
            params.width = hitW;
            params.height = hitH;
            panelProxy.setLayoutParams(params);
            appliedHitWidth = hitW;
            appliedHitHeight = hitH;
        }
        anchorSpace.place(panelProxy, panel.centreX, panel.centreY, hitW, hitH);
        if (!paletteOpen) {
            return;
        }
        anchorSpace.measureUnderParent(actionsPalette);
        final float top = panel.centreY - panel.hitHeight * 0.5f;
        final float centreY = CadHudPresentation.paletteCentreY(top, panel.hitHeight,
                actionsPalette.getMeasuredHeight(), paletteGap, anchorSpace.viewportHeight());
        // Centred on the panel that opened it: an anchored surface grows out of
        // its invoking control. The shared clamp keeps it on screen.
        anchorSpace.place(actionsPalette, panel.centreX, centreY,
                actionsPalette.getMeasuredWidth(), actionsPalette.getMeasuredHeight());
    }

    /** Rotates a value to its leader and centres its proxy on the layout's point. */
    private void placeValue(TextView value, CadHudPresentation.LeaderLayout layout) {
        anchorSpace.measureUnderParent(value);
        if (value.getRotation() != layout.rotation) {
            value.setRotation(layout.rotation);
        }
        anchorSpace.place(value, layout.valueX, layout.valueY, value.getMeasuredWidth(),
                value.getMeasuredHeight());
    }

    /** Shows or withdraws the primary value and the panel. */
    private void setAnnotationVisible(boolean value, boolean panel) {
        reading.setVisibility(value ? VISIBLE : GONE);
        plate.setVisibility(panel ? VISIBLE : GONE);
        panelProxy.setVisibility(panel ? VISIBLE : GONE);
    }

    /** The primary leader's value layout at the last refresh; verification. */
    CadHudPresentation.LeaderLayout lastLeaderLayout() {
        return lastLayout;
    }

    /** The action panel's layout at the last refresh; verification. */
    CadHudPresentation.PanelLayout lastPanelLayout() {
        return lastPanel;
    }

    /** Whether the whole annotation collapsed at the last refresh; verification. */
    boolean lastAnnotationCollapsed() {
        return lastCollapsed;
    }

    /** The value text size last applied, in sp; verification. */
    float lastValueTextSp() {
        return lastValueTextSp;
    }

    /**
     * Centres one child on an anchor at its natural size. The conversion from
     * the viewport pixels native reports into this container's own translation
     * space, and the clamp against the real viewport, are the shared contract
     * in {@link ViewportAnchorSpace}.
     */
    private void placeAt(View shown, float x, float y) {
        anchorSpace.measureAndPlace(shown, x, y, 1.0f);
    }

    // -----------------------------------------------------------------------
    // Palette
    // -----------------------------------------------------------------------

    /** Opens the action palette, closing any editor, or closes it again. */
    private void togglePalette() {
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0) {
            return;
        }
        closeEditor();
        closeSecondEditor();
        paletteOpen = !paletteOpen;
        actionsPalette.setVisibility(paletteOpen ? VISIBLE : INVISIBLE);
        refreshFromNative();
    }

    /** Closes the action palette. Changes nothing but what is drawn. */
    private void closePalette() {
        if (!paletteOpen) {
            return;
        }
        paletteOpen = false;
        closedPalette(actionsPalette);
        plate.setActivated(false);
        panelProxy.setActivated(false);
    }

    /** Whether the action palette is open; verification. */
    boolean actionPaletteOpen() {
        return paletteOpen;
    }

    /** The panel glyph's drawn size at the last refresh, in pixels; verification. */
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
        closePalette();
        final LengthUnit unit = host.uiState().displayUnit();
        // Seeded with the CURRENT depth and selected whole, so the first
        // keystroke replaces rather than appends — the rule every exact-value
        // field in the product follows.
        field.setText(unit.format(shownDepth));
        field.selectAll();
        setAnnotationVisible(false, false);
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
        closePalette();
        final LengthUnit unit = host.uiState().displayUnit();
        secondField.setText(unit.format(shownSecond));
        secondField.selectAll();
        secondReading.setVisibility(GONE);
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
        closePalette();
        setAnnotationVisible(false, false);
        secondReading.setVisibility(GONE);
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
