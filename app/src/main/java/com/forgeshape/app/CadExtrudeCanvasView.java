package com.forgeshape.app;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Paint;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.content.pm.ApplicationInfo;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
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
 * <p><b>Two annotations</b> (`CAD-FOUNDATION-C2`, reshaped by
 * `CAD-V6-S2-OWNER-CORRECTION-E2E-R1`). The exact VALUE stands ABOVE its
 * leader, rotated to it and kept upright
 * ({@link CadHudPresentation#layoutLeader}). The ACTION DOCK is ONE compact
 * badge stating the operation, drawn into the quad native projected from a
 * WORLD rectangle on the extrusion axis just past the arrow's drawn point
 * ({@link CadExtrudeDockView}, {@link CadHud3dPresentation}): it foreshortens
 * and turns with the arrow, it never slides, clamps or changes side at a
 * screen edge, and native hides it whole when it cannot be drawn honestly
 * (near the axis, behind the eye, or with a corner off the viewport).
 *
 * <p><b>What is drawn and what is touched.</b> The dock is also its own ONE
 * touch target, claiming only its projected quad and the 48 dp floor square on
 * its centre. It opens the ACTION PALETTE: ordinary, readable screen chrome
 * holding the extent choices, the operations native offers now, and Flip, each
 * a full 48 dp target, with Tool Labels captions when that preference is on.
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
 * dock's badge always states the operation in force, in its colour
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
    /** The glyph at scale 1.0, px: every palette glyph. */
    private final int referenceGlyphPx;

    /** The primary value, above its leader. */
    private final TextView reading;

    /** The action dock: the one badge, drawn into native's projected quad, and its touch target. */
    private final CadExtrudeDockView dock;

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
    /** The dock's box size last applied, px. */
    private int appliedDockWidth;
    private int appliedDockHeight;
    /** The value text size last applied, in sp. Verification and change detection. */
    private float lastValueTextSp;
    /** The primary leader's value layout at the last refresh. Verification. */
    private CadHudPresentation.LeaderLayout lastLayout = new CadHudPresentation.LeaderLayout();
    /** The dock at the last refresh. Verification. */
    private CadHud3dPresentation.Dock lastDock = new CadHud3dPresentation.Dock();
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

    /** Debug build: log which HUD surface consumed a Down. Never in release. */
    private final boolean debugTouchAttribution;
    private final android.graphics.Rect hitRect = new android.graphics.Rect();

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
        debugTouchAttribution =
                (context.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;

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

        // --- The action dock: one badge, one touch target ------------------
        dock = new CadExtrudeDockView(context, hitPx);
        dock.setId(R.id.cad_extrude_panel);
        dock.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                togglePalette();
            }
        });
        addView(dock, new LayoutParams(hitPx, hitPx));

        // --- The action palette ---------------------------------------------
        // Ordinary screen chrome, grown out of the dock that opens it, and
        // added AFTER the dock so it draws over it where they meet. Three rows,
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
    /**
     * Debug-only attribution (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`): when one of
     * this HUD's views CONSUMES a Down -- which is a Down the viewport, and so
     * a sketch cell, never sees -- the debug build logs which one. Dispatch is
     * unchanged; nothing here claims or refuses a touch.
     */
    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        final boolean consumed = super.dispatchTouchEvent(event);
        if (debugTouchAttribution && event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            final String token = CadHudTouchAttribution.token(consumed,
                    hudSurfaceAt(event.getX(), event.getY()));
            if (token != null) {
                Log.i("ForgeShape", token);
            }
        }
        return consumed;
    }

    /** The topmost visible HUD surface whose hit box holds (x, y). */
    private int hudSurfaceAt(float x, float y) {
        for (int i = getChildCount() - 1; i >= 0; i--) {
            final View child = getChildAt(i);
            if (child.getVisibility() != VISIBLE) {
                continue;
            }
            child.getHitRect(hitRect);
            if (!hitRect.contains((int) x, (int) y)) {
                continue;
            }
            if (child == dock) {
                // The dock claims its shape, not its box.
                if (dock.claimsLocal(x - dock.getLeft() - dock.getTranslationX(),
                        y - dock.getTop() - dock.getTranslationY())) {
                    return CadHudTouchAttribution.DOCK;
                }
                continue;
            }
            if (child == reading || child == secondReading) {
                return CadHudTouchAttribution.VALUE;
            }
            if (child == actionsPalette) {
                return CadHudTouchAttribution.PALETTE;
            }
            if (child == editor || child == secondEditor) {
                return CadHudTouchAttribution.EDITOR;
            }
            if (child == editSketch.view) {
                return CadHudTouchAttribution.EDIT_SKETCH;
            }
        }
        return CadHudTouchAttribution.NONE;
    }

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
     * the value follows a live arrow drag, the dock follows an orbit and both
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
        applyValueTextSize(CadHudPresentation.valueTextSp(scale));
        // A live drag rewrites the value under the user; an editor or the
        // palette open over it would act on a state the arrow has already left.
        if (tool[NativeViewport.CAD_EXTRUDE_DRAGGING] != 0.0) {
            closeEditor();
            closeSecondEditor();
            closePalette();
        }

        final CadHud3dPresentation.Dock dockFrame = CadHud3dPresentation.fromToolState(tool, hitPx);
        bindDock(context, extent, dockFrame);

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
        // The value keeps its own collapse rule; the dock obeys native alone,
        // which decided in world space whether it can be drawn honestly.
        lastCollapsed = CadHudPresentation.annotationCollapsed(
                tool[NativeViewport.CAD_EXTRUDE_CLAMP] == NativeViewport.CAD_EXTRUDE_CLAMP_LOW,
                layout.valueVisible ? layout.visibleLength : 0.0f,
                reading.getPaint().measureText(reading.getText().toString()));
        if (lastCollapsed) {
            layout.valueVisible = false;
        }
        lastDock = dockFrame;
        if (!layout.valueVisible && !dockFrame.visible) {
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
            placeDock(dockFrame);
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
     * Binds the dock and the palette's rows to what native says now: the
     * operation in force on the dock (its glyph SHAPE and colour, with an error
     * ring when the candidate would be refused, and an accessible name that
     * says why), and in the palette the extent in force, exactly the
     * operations that can be chosen, and Flip in One Side alone.
     */
    private void bindDock(Context context, int extent, CadHud3dPresentation.Dock frame) {
        final int current = (int) tool[NativeViewport.CAD_EXTRUDE_OPERATION];
        final int available = (int) tool[NativeViewport.CAD_EXTRUDE_OPERATIONS_AVAILABLE];
        final int status = (int) tool[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS];
        final boolean refused = status != NativeViewport.CAD_OK;

        final int extentCaption = CadHudPresentation.extentCaption(extent);
        final int operationCaption = CadHudPresentation.operationCaption(current);
        // Colour is never the only carrier: the glyph is a SHAPE, the refusal a
        // ring, and the accessible name says both.
        dock.bind(frame, CadHudPresentation.operationIcon(current),
                operationTints[operationIndex(current)], refused);
        dock.setActivated(paletteOpen);
        String dockDescription = context.getString(R.string.cad_hud_panel_description,
                context.getString(operationCaption), context.getString(extentCaption));
        if (refused) {
            dockDescription = context.getString(R.string.cad_hud_operation_invalid_description,
                    dockDescription, refusalReason(context, status));
        }
        if (!dockDescription.contentEquals(dock.getContentDescription() == null ? ""
                : dock.getContentDescription())) {
            dock.setContentDescription(dockDescription);
        }

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
        // Nor while the spatial support chooser is up
        // (`MODELING-R1-OWNER-CORRECTION`): the viewport then belongs to
        // choosing a plane or a face, and a chip standing on the body's sketch
        // anchor would take the very tap that chooses the face under it.
        if (body == NativeViewport.NO_OBJECT || NativeViewport.supportChooserActive()
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
     * Stands the dock WHOLE over the box native's quad needs, or withdraws it;
     * then hangs the open palette, if any, from that box. The box is never
     * clamped: native already proved every corner is on the viewport, and a
     * clamp would move the badge off the geometry it belongs to.
     */
    private void placeDock(CadHud3dPresentation.Dock frame) {
        if (!frame.visible) {
            dock.setVisibility(GONE);
            closePalette();
            return;
        }
        dock.setVisibility(VISIBLE);
        final int width = (int) Math.ceil(frame.right - frame.left);
        final int height = (int) Math.ceil(frame.bottom - frame.top);
        if (width != appliedDockWidth || height != appliedDockHeight) {
            final ViewGroup.LayoutParams params = dock.getLayoutParams();
            params.width = width;
            params.height = height;
            dock.setLayoutParams(params);
            appliedDockWidth = width;
            appliedDockHeight = height;
        }
        anchorSpace.placeCentred(dock, frame.left + width * 0.5f, frame.top + height * 0.5f,
                width, height);
        if (!paletteOpen) {
            return;
        }
        anchorSpace.measureUnderParent(actionsPalette);
        final float centreY = CadHudPresentation.paletteCentreY(frame.top, frame.bottom - frame.top,
                actionsPalette.getMeasuredHeight(), paletteGap, anchorSpace.viewportHeight());
        // Grown out of the dock that opened it: an anchored surface, kept on
        // screen by the shared clamp because it is transient readable chrome
        // and not the attached instrument.
        anchorSpace.place(actionsPalette, frame.centreX, centreY,
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

    /** Shows or withdraws the primary value and the dock. */
    private void setAnnotationVisible(boolean value, boolean shown) {
        reading.setVisibility(value ? VISIBLE : GONE);
        dock.setVisibility(shown ? VISIBLE : GONE);
    }

    /** The primary leader's value layout at the last refresh; verification. */
    CadHudPresentation.LeaderLayout lastLeaderLayout() {
        return lastLayout;
    }

    /** The dock native reported at the last refresh; verification. */
    CadHud3dPresentation.Dock lastDock() {
        return lastDock;
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
        dock.setActivated(false);
    }

    /** Whether the action palette is open; verification. */
    boolean actionPaletteOpen() {
        return paletteOpen;
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
