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

        /**
         * The "Sketch here" chip standing on a planar face the user tapped:
         * begin a sketch on that face. Native re-validates the face and owns
         * the whole decision; the chip only asks.
         */
        void onCanvasSketchOnFaceRequested();

        /**
         * Apply a Chamfer or a Fillet ({@link NativeViewport#EDGE_FINISH_CHAMFER}
         * or {@code _FILLET}) of {@code meters}: over the offered edges when
         * {@code finishId} is 0, or as an edit of that committed finish.
         * Returns whether native accepted it, so the editor closes only then.
         */
        boolean onCanvasEdgeFinishApply(int kind, double meters, long bodyId, long finishId);

        /** Remove committed finish {@code finishId} of body {@code bodyId}. */
        boolean onCanvasEdgeFinishDelete(long bodyId, long finishId);

        /**
         * Apply a pattern (`CAD-PATTERN-R1`): a NEW one of {@code featureId}
         * when {@code patternId} is 0, else an edit of that committed pattern.
         * Returns whether native accepted it, so the editor closes only then.
         */
        boolean onCanvasPatternApply(long bodyId, long patternId, long featureId,
                PatternEditorPanel.Request request);

        /** Remove committed pattern {@code patternId} of body {@code bodyId}. */
        boolean onCanvasPatternDelete(long bodyId, long patternId);

        /** A pattern field did not parse: say so as a verdict. */
        void onCanvasPatternFieldRefused(String message);

        /**
         * Apply a hole (`CAD-HOLE-R1`): a NEW one on the face native captured
         * when {@code holeId} is 0, else an edit of that committed hole.
         * Returns whether native accepted it, so the editor closes only then.
         */
        boolean onCanvasHoleApply(long bodyId, long holeId, HoleEditorPanel.Request request);

        /** Remove committed hole {@code holeId} of body {@code bodyId}. */
        boolean onCanvasHoleDelete(long bodyId, long holeId);

        /**
         * Begins placing the hole from two edges of its face (`CAD-TO-FACE-R1`):
         * the two distances, or null on a refusal the host has said.
         */
        double[] onCanvasHoleEdgeSeed(long bodyId, long holeId, double u, double v);

        /** Ends placing from edges. */
        void onCanvasHoleEdgesReleased();
    }

    /**
     * Where the trailing controls stand (`CAD-UI-DOCK-R1`): the face actions
     * and the feature editors are DOCKED beside them rather than over the
     * model, so they neither cover the part being edited nor take the touches
     * that orbit it. Answers, in THIS view's coordinates, out[0] the inner edge
     * of the trailing controls (their left edge, or their right edge when they
     * stand on the left), out[1] the top a docked surface starts at, out[2] 1
     * when the controls stand on the LEFT, out[3] the lowest y a docked
     * surface may reach. False when there is no geometry yet.
     */
    interface DockAnchor {
        boolean trailingDock(View canvas, int[] out);
    }

    /** The extent choices, in {@link CadHudPresentation#EXTENTS} order. */
    private static final int[] EXTENT_CHOICE_IDS = {
            R.id.cad_extrude_extent_one_side, R.id.cad_extrude_extent_symmetric,
            R.id.cad_extrude_extent_two_sides, R.id.cad_extrude_extent_through_all,
            R.id.cad_extrude_extent_to_face};
    /** The operation choices, in {@link CadHudPresentation#OPERATIONS} order. */
    private static final int[] OPERATION_CHOICE_IDS = {
            R.id.cad_extrude_operation_new_body, R.id.cad_extrude_operation_add,
            R.id.cad_extrude_operation_cut, R.id.cad_extrude_operation_intersect};

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
    /**
     * "Sketch here": the direct sketch-on-face offer, standing just above the
     * point of the planar CAD face the last viewport tap landed on. Always
     * captioned, because it is the one control that teaches the gesture.
     */
    private final LinearLayout sketchOnFace;
    private final float[] faceOfferPoint = new float[2];
    /**
     * The Fillet / Chamfer offer (`CAD-EDGE-FINISH-R1`): two captioned buttons
     * beside the edge the finger landed by, or below the face it landed on
     * (then they finish every edge of that face), and the one editor both open.
     */
    private final LinearLayout edgeFinishBar;
    private final View edgeFillet;
    private final View edgeChamfer;
    private final TextView edgeFilletCaption;
    private final TextView edgeChamferCaption;
    private final LinearLayout edgeFinishEditor;
    private final TextView edgeFinishLabel;
    private final EditText edgeFinishField;
    private final TextView edgeFinishDelete;
    private final double[] edgeOffer = new double[NativeViewport.EDGE_OFFER_SIZE];
    private final double[] edgeFinishInfo = new double[NativeViewport.EDGE_FINISH_INFO_SIZE];
    /** The kind the open editor applies, or -1 while it is closed. */
    private int edgeEditorKind = -1;
    /** 0 for a new finish over the offer, else the committed finish edited. */
    private long edgeEditorFinishId;
    private long edgeEditorBodyId = NativeViewport.NO_OBJECT;
    private float edgeEditorX;
    private float edgeEditorY;

    /**
     * Pattern and Mirror (`CAD-PATTERN-R1`): two more captioned actions in the
     * same bar, offered when the tapped face belongs to a later Add or Cut, and
     * the one editor both open. Presentation only; native owns the pattern.
     */
    private final View patternAction;
    private final View mirrorAction;
    private final PatternEditorPanel patternPanel;
    private final double[] patternOffer = new double[NativeViewport.PATTERN_OFFER_SIZE];
    private final double[] patternInfo = new double[NativeViewport.PATTERN_INFO_SIZE];
    private final double[] patternSeeds = new double[NativeViewport.PATTERN_SEEDS_SIZE];
    private long patternBodyId = NativeViewport.NO_OBJECT;
    /** 0 for a new pattern of {@link #patternFeatureId}, else the pattern edited. */
    private long patternId;
    private long patternFeatureId;
    private float patternX;
    private float patternY;

    /**
     * Hole (`CAD-HOLE-R1`): one more captioned action in the same bar, offered
     * on any face of a CAD body a hole can be drilled into, and its editor.
     * Presentation only; native owns the hole and the face it stands on.
     */
    private final View holeAction;
    private final HoleEditorPanel holePanel;
    private final double[] holeOffer = new double[NativeViewport.HOLE_OFFER_SIZE];
    private final double[] holeBegin = new double[NativeViewport.HOLE_BEGIN_SIZE];
    private final double[] holeInfo = new double[NativeViewport.HOLE_INFO_SIZE];
    private long holeBodyId = NativeViewport.NO_OBJECT;
    /** 0 for a new hole on the captured face, else the hole edited. */
    private long holeId;
    private float holeX;
    private float holeY;

    private DockAnchor dockAnchor;
    private final android.widget.ScrollView patternScroll;
    private final android.widget.ScrollView holeScroll;
    private final int[] dockEdge = new int[4];
    /**
     * A small ring at the point the face offer, or a new hole, applies to:
     * the one thing left at the model once the actions moved to the dock. Not
     * clickable and not focusable, so every touch on it reaches the viewport.
     */
    private final View offerMarker;
    private final float[] markerPoint = new float[2];

    /** Whether the action palette is open. Presentation only; never a domain value. */
    private boolean paletteOpen;
    /** Whether palette controls draw their caption. Application preference; default off. */
    private boolean toolLabels;
    /**
     * Whether a workspace surface that offers its own Edit Sketch is open over
     * the viewport (the History surface). Read on every refresh, never cached,
     * so the chip cannot outlive the surface it would stand on.
     */
    private java.util.function.BooleanSupplier retainedChipWithdrawn = () -> false;
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

        // --- The direct sketch-on-face offer ---------------------------------
        sketchOnFace = new LinearLayout(context);
        sketchOnFace.setId(R.id.cad_canvas_sketch_on_face);
        sketchOnFace.setOrientation(LinearLayout.HORIZONTAL);
        sketchOnFace.setGravity(Gravity.CENTER_VERTICAL);
        sketchOnFace.setBackgroundResource(R.drawable.bg_hud_lone_control);
        sketchOnFace.setMinimumHeight(hitPx);
        sketchOnFace.setMinimumWidth(hitPx);
        sketchOnFace.setPadding(captionPadH, 0, captionPadH + captionPadH / 2, 0);
        sketchOnFace.setClickable(true);
        sketchOnFace.setFocusable(true);
        sketchOnFace.setElevation(EditorControlStyles.dimen(context, R.dimen.elevation_floating));
        sketchOnFace.setContentDescription(context.getString(R.string.sketch_on_face_description));
        final ImageView faceGlyph = new ImageView(context);
        faceGlyph.setImageResource(R.drawable.ic_sketch_new);
        faceGlyph.setImageTintList(contentTint);
        faceGlyph.setScaleType(ImageView.ScaleType.FIT_CENTER);
        faceGlyph.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        sketchOnFace.addView(faceGlyph, new LinearLayout.LayoutParams(referenceGlyphPx,
                referenceGlyphPx));
        final TextView faceCaption = new TextView(context);
        faceCaption.setText(R.string.sketch_on_face_caption);
        faceCaption.setSingleLine(true);
        faceCaption.setIncludeFontPadding(false);
        faceCaption.setTextColor(contentTint);
        faceCaption.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_label));
        EditorControlStyles.applyMediumWeight(faceCaption);
        faceCaption.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        final LinearLayout.LayoutParams faceCaptionParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        faceCaptionParams.leftMargin = captionPadH;
        sketchOnFace.addView(faceCaption, faceCaptionParams);
        sketchOnFace.setVisibility(GONE);
        sketchOnFace.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                sketchOnFace.setVisibility(GONE);
                CadExtrudeCanvasView.this.actions.onCanvasSketchOnFaceRequested();
            }
        });
        addView(sketchOnFace, wrapParams());

        // --- The Fillet / Chamfer offer --------------------------------------
        edgeFinishBar = new LinearLayout(context);
        edgeFinishBar.setId(R.id.cad_canvas_edge_finish_bar);
        // A vertical list docked beside the trailing controls: one captioned
        // row per act, read top to bottom like the Tool Rail beside it.
        edgeFinishBar.setOrientation(LinearLayout.VERTICAL);
        edgeFinishBar.setGravity(Gravity.START);
        edgeFinishBar.setBackgroundResource(R.drawable.bg_hud_lone_control);
        edgeFinishBar.setElevation(EditorControlStyles.dimen(context, R.dimen.elevation_floating));
        edgeFinishBar.setClickable(true);
        edgeFilletCaption = new TextView(context);
        edgeChamferCaption = new TextView(context);
        edgeFillet = captionedAction(context, R.id.cad_canvas_edge_fillet,
                R.drawable.ic_edge_fillet, edgeFilletCaption);
        edgeChamfer = captionedAction(context, R.id.cad_canvas_edge_chamfer,
                R.drawable.ic_edge_chamfer, edgeChamferCaption);
        edgeFillet.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openEdgeFinishEditor(NativeViewport.EDGE_FINISH_FILLET);
            }
        });
        edgeChamfer.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openEdgeFinishEditor(NativeViewport.EDGE_FINISH_CHAMFER);
            }
        });
        edgeFinishBar.addView(edgeFillet, EditorControlStyles.wrap(0));
        edgeFinishBar.addView(edgeChamfer, EditorControlStyles.wrap(0));
        final TextView patternCaption = new TextView(context);
        final TextView mirrorCaption = new TextView(context);
        patternAction = captionedAction(context, R.id.cad_canvas_pattern,
                R.drawable.ic_pattern_linear, patternCaption);
        mirrorAction = captionedAction(context, R.id.cad_canvas_mirror_feature,
                R.drawable.ic_pattern_mirror, mirrorCaption);
        patternCaption.setText(R.string.pattern_action);
        mirrorCaption.setText(R.string.mirror_action);
        patternAction.setContentDescription(context.getString(R.string.pattern_action_description));
        mirrorAction.setContentDescription(context.getString(R.string.mirror_action_description));
        patternAction.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openNewPattern(NativeViewport.PATTERN_LINEAR);
            }
        });
        mirrorAction.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openNewPattern(NativeViewport.PATTERN_MIRROR);
            }
        });
        final TextView holeCaption = new TextView(context);
        holeAction = captionedAction(context, R.id.cad_canvas_hole, R.drawable.ic_hole, holeCaption);
        holeCaption.setText(R.string.hole_action);
        holeAction.setContentDescription(context.getString(R.string.hole_action_description));
        holeAction.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openNewHole();
            }
        });
        edgeFinishBar.addView(holeAction, EditorControlStyles.wrap(0));
        edgeFinishBar.addView(patternAction, EditorControlStyles.wrap(0));
        edgeFinishBar.addView(mirrorAction, EditorControlStyles.wrap(0));
        edgeFinishBar.setVisibility(GONE);
        addView(edgeFinishBar, wrapParams());

        edgeFinishEditor = editorSurface(context, R.id.cad_canvas_edge_finish_editor);
        edgeFinishEditor.setElevation(
                EditorControlStyles.dimen(context, R.dimen.elevation_floating));
        // Two rows -- the value, then its acts -- so the docked editor stays
        // narrow beside the trailing controls.
        edgeFinishEditor.setOrientation(LinearLayout.VERTICAL);
        edgeFinishEditor.setGravity(Gravity.START);
        final LinearLayout edgeFinishValueRow = new LinearLayout(context);
        edgeFinishValueRow.setOrientation(LinearLayout.HORIZONTAL);
        edgeFinishValueRow.setGravity(Gravity.CENTER_VERTICAL);
        final LinearLayout edgeFinishActionRow = new LinearLayout(context);
        edgeFinishActionRow.setOrientation(LinearLayout.HORIZONTAL);
        edgeFinishActionRow.setGravity(Gravity.CENTER_VERTICAL);
        edgeFinishEditor.addView(edgeFinishValueRow);
        final LinearLayout.LayoutParams actionRowParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        actionRowParams.topMargin = editorGap;
        edgeFinishEditor.addView(edgeFinishActionRow, actionRowParams);
        edgeFinishLabel = new TextView(context);
        edgeFinishLabel.setSingleLine(true);
        edgeFinishLabel.setTextColor(contentTint);
        edgeFinishLabel.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_label));
        EditorControlStyles.applyMediumWeight(edgeFinishLabel);
        edgeFinishValueRow.addView(edgeFinishLabel, EditorControlStyles.wrap(0));
        edgeFinishField = distanceField(context, R.id.field_edge_finish_distance);
        edgeFinishField.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    submitEdgeFinish();
                    return true;
                }
                return false;
            }
        });
        edgeFinishValueRow.addView(edgeFinishField, EditorControlStyles.wrap(editorGap));
        final TextView applyFinish = applyButton(context, R.id.apply_edge_finish);
        applyFinish.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                submitEdgeFinish();
            }
        });
        edgeFinishActionRow.addView(applyFinish, EditorControlStyles.wrap(0));
        edgeFinishDelete = plainButton(context, R.id.delete_edge_finish,
                R.string.edge_finish_delete);
        edgeFinishDelete.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (edgeEditorFinishId > 0L && CadExtrudeCanvasView.this.actions
                        .onCanvasEdgeFinishDelete(edgeEditorBodyId, edgeEditorFinishId)) {
                    closeEdgeFinishEditor();
                }
            }
        });
        edgeFinishActionRow.addView(edgeFinishDelete, EditorControlStyles.wrap(editorGap));
        final TextView cancelFinish = plainButton(context, R.id.cancel_edge_finish,
                R.string.edge_finish_cancel);
        cancelFinish.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                closeEdgeFinishEditor();
                refreshFromNative();
            }
        });
        edgeFinishActionRow.addView(cancelFinish, EditorControlStyles.wrap(editorGap));
        addView(edgeFinishEditor, wrapParams());

        patternPanel = new PatternEditorPanel(context, new PatternEditorPanel.Listener() {
            @Override
            public LengthUnit patternUnit() {
                return host.uiState().displayUnit();
            }

            @Override
            public void patternFieldRefused(String message) {
                CadExtrudeCanvasView.this.actions.onCanvasPatternFieldRefused(message);
            }

            @Override
            public boolean patternApply(PatternEditorPanel.Request request) {
                return CadExtrudeCanvasView.this.actions.onCanvasPatternApply(patternBodyId,
                        patternId, patternFeatureId, request);
            }

            @Override
            public boolean patternDelete() {
                return patternId > 0L && CadExtrudeCanvasView.this.actions
                        .onCanvasPatternDelete(patternBodyId, patternId);
            }

            @Override
            public void patternCancelled() {
                hideIme();
                refreshFromNative();
            }
        });
        patternPanel.setElevation(EditorControlStyles.dimen(context, R.dimen.elevation_floating));
        // Docked editors scroll inside the room beside the trailing controls,
        // so a short (landscape) window never cuts their Apply off.
        patternScroll = dockScroll(context);
        patternScroll.addView(patternPanel);
        addView(patternScroll, wrapParams());

        holePanel = new HoleEditorPanel(context, new HoleEditorPanel.Listener() {
            @Override
            public LengthUnit holeUnit() {
                return host.uiState().displayUnit();
            }

            @Override
            public void holeFieldRefused(String message) {
                CadExtrudeCanvasView.this.actions.onCanvasPatternFieldRefused(message);
            }

            @Override
            public boolean holeApply(HoleEditorPanel.Request request) {
                return CadExtrudeCanvasView.this.actions.onCanvasHoleApply(holeBodyId, holeId,
                        request);
            }

            @Override
            public boolean holeDelete() {
                return holeId > 0L
                        && CadExtrudeCanvasView.this.actions.onCanvasHoleDelete(holeBodyId, holeId);
            }

            @Override
            public void holeCancelled() {
                hideIme();
                refreshFromNative();
            }

            @Override
            public double[] holeEdgeSeed(double u, double v) {
                return CadExtrudeCanvasView.this.actions.onCanvasHoleEdgeSeed(holeBodyId, holeId,
                        u, v);
            }

            @Override
            public void holeEdgesReleased() {
                CadExtrudeCanvasView.this.actions.onCanvasHoleEdgesReleased();
            }
        });
        holePanel.setElevation(EditorControlStyles.dimen(context, R.dimen.elevation_floating));
        holeScroll = dockScroll(context);
        holeScroll.addView(holePanel);
        addView(holeScroll, wrapParams());

        offerMarker = new View(context);
        offerMarker.setId(R.id.cad_canvas_offer_marker);
        offerMarker.setBackgroundResource(R.drawable.bg_offer_marker);
        offerMarker.setClickable(false);
        offerMarker.setFocusable(false);
        offerMarker.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        offerMarker.setVisibility(GONE);
        final int markerPx = Math.round(18f * density);
        addView(offerMarker, new FrameLayout.LayoutParams(markerPx, markerPx));
        setAnnotationVisible(false, false);
    }

    /** A transparent vertical scroller a docked editor stands in. */
    private static android.widget.ScrollView dockScroll(Context context) {
        final android.widget.ScrollView scroll = new android.widget.ScrollView(context);
        scroll.setFillViewport(false);
        scroll.setClipToPadding(false);
        scroll.setVerticalScrollBarEnabled(true);
        return scroll;
    }

    /** An icon and a caption beside it, as one 48 dp target. */
    private View captionedAction(Context context, int id, int iconRes, TextView caption) {
        final LinearLayout view = new LinearLayout(context);
        view.setId(id);
        view.setOrientation(LinearLayout.HORIZONTAL);
        view.setGravity(Gravity.CENTER_VERTICAL);
        view.setMinimumHeight(hitPx);
        view.setMinimumWidth(hitPx);
        view.setPadding(captionPadH, 0, captionPadH, 0);
        view.setBackgroundResource(R.drawable.bg_hud_member);
        view.setClickable(true);
        view.setFocusable(true);
        final ImageView glyph = new ImageView(context);
        glyph.setImageResource(iconRes);
        glyph.setImageTintList(contentTint);
        glyph.setScaleType(ImageView.ScaleType.FIT_CENTER);
        glyph.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        view.addView(glyph, new LinearLayout.LayoutParams(referenceGlyphPx, referenceGlyphPx));
        caption.setSingleLine(true);
        caption.setIncludeFontPadding(false);
        caption.setTextColor(contentTint);
        caption.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_label));
        EditorControlStyles.applyMediumWeight(caption);
        caption.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.leftMargin = captionPadH / 2;
        view.addView(caption, params);
        return view;
    }

    /** A quiet text button with the 48 dp floor. */
    private TextView plainButton(Context context, int id, int textRes) {
        final TextView button = new TextView(context);
        button.setId(id);
        button.setText(textRes);
        button.setGravity(Gravity.CENTER);
        button.setMinimumHeight(hitPx);
        button.setMinimumWidth(hitPx);
        button.setPadding(captionPadH, 0, captionPadH, 0);
        button.setBackgroundResource(R.drawable.bg_control);
        button.setTextColor(contentTint);
        button.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_label));
        EditorControlStyles.applyMediumWeight(button);
        button.setClickable(true);
        button.setFocusable(true);
        return button;
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
            if (child == editSketch.view || child == sketchOnFace || child == edgeFinishBar) {
                return CadHudTouchAttribution.EDIT_SKETCH;
            }
            if (child == edgeFinishEditor || child == patternScroll || child == holeScroll) {
                return CadHudTouchAttribution.EDITOR;
            }
        }
        return CadHudTouchAttribution.NONE;
    }

    private TextView valueText(Context context, int id) {
        // An opaque PLATE behind the value (`WORKBENCH-UX-R1`), hugging the
        // text rather than the 48 dp touch box: a glow on the pale New Body
        // preview read at well under 4.5:1, a plate in the floating surface
        // with a hairline reads at the panel's own contrast on any ground.
        final int plateFill = (EditorControlStyles.themeColor(context, R.attr.fsSurfaceFloating)
                & 0x00FFFFFF) | 0xEB000000;
        final int plateEdge = EditorControlStyles.themeColor(context, R.attr.fsChromeHairline);
        final float plateRadius = 6.0f * density;
        final float platePadX = 6.0f * density;
        final float platePadY = 2.0f * density;
        final android.graphics.Paint plate = new android.graphics.Paint(
                android.graphics.Paint.ANTI_ALIAS_FLAG);
        final android.graphics.RectF plateRect = new android.graphics.RectF();
        final TextView text = new TextView(context) {
            @Override
            protected void onDraw(android.graphics.Canvas canvas) {
                final CharSequence shown = getText();
                if (shown != null && shown.length() > 0) {
                    final float w = getPaint().measureText(shown, 0, shown.length());
                    final android.graphics.Paint.FontMetrics fm = getPaint().getFontMetrics();
                    final float h = fm.descent - fm.ascent;
                    final float cx = getWidth() * 0.5f;
                    final float cy = getHeight() * 0.5f;
                    plateRect.set(cx - w * 0.5f - platePadX, cy - h * 0.5f - platePadY,
                            cx + w * 0.5f + platePadX, cy + h * 0.5f + platePadY);
                    plate.setStyle(android.graphics.Paint.Style.FILL);
                    plate.setColor(plateFill);
                    canvas.drawRoundRect(plateRect, plateRadius, plateRadius, plate);
                    plate.setStyle(android.graphics.Paint.Style.STROKE);
                    plate.setStrokeWidth(Math.max(1.0f, density));
                    plate.setColor(plateEdge);
                    canvas.drawRoundRect(plateRect, plateRadius, plateRadius, plate);
                }
                super.onDraw(canvas);
            }
        };
        text.setId(id);
        text.setGravity(Gravity.CENTER);
        text.setSingleLine(true);
        text.setIncludeFontPadding(false);
        text.setTextSize(TypedValue.COMPLEX_UNIT_SP, CadHudPresentation.VALUE_TEXT_BASE_SP);
        text.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        EditorControlStyles.applyMediumWeight(text);
        final int padH = Math.round(8.0f * density);
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
    /**
     * Names the condition under which the retained-sketch chip steps aside.
     * The chip is viewport chrome anchored at the body's projected sketch
     * anchor; a surface grown over that point (History at a large font scale,
     * found by `CAD-R2-C1` D17) would otherwise have the chip stand on one of
     * its acts and take that act's tap.
     */
    void setRetainedChipWithdrawnWhen(java.util.function.BooleanSupplier withdrawn) {
        retainedChipWithdrawn = withdrawn != null ? withdrawn : () -> false;
    }

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
        sketchOnFace.setVisibility(GONE);
        edgeFinishBar.setVisibility(GONE);
        offerMarker.setVisibility(GONE);
        closeEdgeFinishEditor();
        closePatternEditor();
        closeHoleEditor();
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
        // Through All reads as what it is, with where it reaches now beside it.
        reading.setText(extent == NativeViewport.EXTENT_THROUGH_ALL
                ? context.getString(R.string.extrude_through_all_reading,
                        unit.formatWithUnit(shownDepth))
                : extent == NativeViewport.EXTENT_TO_FACE
                        ? context.getString(R.string.extrude_to_face_reading,
                                unit.formatWithUnit(shownDepth))
                        : unit.formatWithUnit(shownDepth));
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
        final boolean throughAllAvailable =
                tool[NativeViewport.CAD_EXTRUDE_THROUGH_ALL_AVAILABLE] != 0.0;
        final boolean toFaceAvailable = tool.length > NativeViewport.CAD_EXTRUDE_TO_FACE_AVAILABLE
                && tool[NativeViewport.CAD_EXTRUDE_TO_FACE_AVAILABLE] != 0.0;
        for (int i = 0; i < extentChoices.length; i++) {
            final int mode = CadHudPresentation.EXTENTS[i];
            extentChoices[i].view.setVisibility(
                    CadHudPresentation.extentOffered(mode, throughAllAvailable, toFaceAvailable)
                            || mode == extent
                            ? VISIBLE : GONE);
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
        // Nor while the History surface is open (`CAD-R2-C1`): it lists Edit
        // Sketch for every sketch itself, and at a large font scale it grows
        // over the anchor, where the chip would take its acts' taps.
        if (body == NativeViewport.NO_OBJECT || NativeViewport.supportChooserActive()
                || retainedChipWithdrawn.getAsBoolean()) {
            hide();
            return;
        }
        final boolean faceOffered = anchorSpace.ready()
                && NativeViewport.faceSketchOfferPoint(faceOfferPoint);
        // An open feature editor owns the dock; the face's own offer waits.
        final boolean editing = edgeEditorKind >= 0 || patternPanel.isOpen() || holePanel.isOpen();
        dockedBelow = 0;
        if (faceOffered && !editing) {
            sketchOnFace.setVisibility(VISIBLE);
            setVisibility(VISIBLE);
            // Docked beside the trailing controls (`CAD-UI-DOCK-R1`), the
            // first of the face's acts; only a marker stays on the model.
            if (placeDocked(sketchOnFace, 0)) {
                dockedBelow = sketchOnFace.getMeasuredHeight() + paletteGap;
            } else {
                placeAt(sketchOnFace, faceOfferPoint[0], faceOfferPoint[1] - hitPx);
            }
        } else {
            sketchOnFace.setVisibility(GONE);
        }
        final boolean finishShown = refreshEdgeFinishOffer();
        refreshOfferMarker(faceOffered);
        if (!NativeViewport.cadBodySketchAnchor(body, bodyAnchor)) {
            editSketch.view.setVisibility(GONE);
            sketchBodyId = NativeViewport.NO_OBJECT;
            if (!faceOffered && !finishShown) {
                hide();
            }
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
        sketchOnFace.setVisibility(GONE);
        edgeFinishBar.setVisibility(GONE);
        offerMarker.setVisibility(GONE);
        closeEdgeFinishEditor();
        closePatternEditor();
        closeHoleEditor();
        sketchBodyId = NativeViewport.NO_OBJECT;
        setVisibility(GONE);
    }

    // -----------------------------------------------------------------------
    // Fillet / Chamfer (`CAD-EDGE-FINISH-R1`)
    // -----------------------------------------------------------------------

    /**
     * Shows the offer bar, or keeps the open editor on its anchor. Returns
     * whether anything of it is shown. Native decides whether there is an offer
     * at all and re-validates it on every read.
     */
    private boolean refreshEdgeFinishOffer() {
        if (edgeEditorKind >= 0) {
            if (edgeEditorFinishId == 0L) {
                // A NEW finish follows its offer; an offer that went away
                // (an Undo, another tap) takes the editor with it.
                if (!anchorSpace.ready() || !NativeViewport.edgeFinishOfferState(edgeOffer)) {
                    closeEdgeFinishEditor();
                    edgeFinishBar.setVisibility(GONE);
                    return false;
                }
                edgeEditorX = (float) edgeOffer[NativeViewport.EDGE_OFFER_X];
                edgeEditorY = (float) edgeOffer[NativeViewport.EDGE_OFFER_Y];
            } else if (NativeViewport.cadEdgeFinishInfo(edgeEditorBodyId, edgeEditorFinishId,
                    edgeFinishInfo)
                    && edgeFinishInfo[NativeViewport.EDGE_FINISH_INFO_ON_SCREEN] != 0.0) {
                edgeEditorX = (float) edgeFinishInfo[NativeViewport.EDGE_FINISH_INFO_X];
                edgeEditorY = (float) edgeFinishInfo[NativeViewport.EDGE_FINISH_INFO_Y];
            }
            edgeFinishBar.setVisibility(GONE);
            edgeFinishEditor.setVisibility(VISIBLE);
            setVisibility(VISIBLE);
            placeEditor(edgeFinishEditor, edgeEditorX, edgeEditorY);
            return true;
        }
        if (patternPanel.isOpen()) {
            // The open pattern editor keeps its own anchor: a new pattern
            // follows the offer it came from, an edit the feature it repeats.
            if (patternId == 0L) {
                if (anchorSpace.ready() && NativeViewport.patternOfferState(patternOffer)) {
                    patternX = (float) patternOffer[NativeViewport.PATTERN_OFFER_X];
                    patternY = (float) patternOffer[NativeViewport.PATTERN_OFFER_Y];
                }
            } else if (NativeViewport.cadPatternInfo(patternBodyId, patternId, patternInfo)
                    && patternInfo[NativeViewport.PATTERN_INFO_ON_SCREEN] != 0.0) {
                patternX = (float) patternInfo[NativeViewport.PATTERN_INFO_X];
                patternY = (float) patternInfo[NativeViewport.PATTERN_INFO_Y];
            }
            edgeFinishBar.setVisibility(GONE);
            setVisibility(VISIBLE);
            placeEditor(patternScroll, patternX, patternY);
            return true;
        }
        if (holePanel.isOpen()) {
            // The open hole editor keeps its own anchor: a new hole where the
            // face was tapped, an edit where its axis meets the face.
            if (holeId > 0L && NativeViewport.cadHoleInfo(holeBodyId, holeId, holeInfo)
                    && holeInfo[NativeViewport.HOLE_INFO_ON_SCREEN] != 0.0) {
                holeX = (float) holeInfo[NativeViewport.HOLE_INFO_X];
                holeY = (float) holeInfo[NativeViewport.HOLE_INFO_Y];
            }
            edgeFinishBar.setVisibility(GONE);
            setVisibility(VISIBLE);
            placeEditor(holeScroll, holeX, holeY);
            return true;
        }
        final boolean holeOffered = anchorSpace.ready() && NativeViewport.holeOfferState(holeOffer);
        holeAction.setVisibility(holeOffered ? VISIBLE : GONE);
        final boolean patternOffered = anchorSpace.ready()
                && NativeViewport.patternOfferState(patternOffer);
        final boolean finishOffered = anchorSpace.ready()
                && NativeViewport.edgeFinishOfferState(edgeOffer);
        patternAction.setVisibility(patternOffered ? VISIBLE : GONE);
        mirrorAction.setVisibility(patternOffered ? VISIBLE : GONE);
        edgeFillet.setVisibility(finishOffered ? VISIBLE : GONE);
        edgeChamfer.setVisibility(finishOffered ? VISIBLE : GONE);
        if (!finishOffered && !patternOffered && !holeOffered) {
            edgeFinishBar.setVisibility(GONE);
            return false;
        }
        if (!finishOffered) {
            // A face whose edges cannot be finished (a pocket's wall, say) still
            // offers Hole, Pattern and Mirror, below the finger like a face's bar.
            edgeFinishBar.setVisibility(VISIBLE);
            setVisibility(VISIBLE);
            final double[] at = holeOffered ? holeOffer : patternOffer;
            if (!placeDocked(edgeFinishBar, dockedBelow)) {
                placeAt(edgeFinishBar, (float) at[holeOffered ? NativeViewport.HOLE_OFFER_X
                                : NativeViewport.PATTERN_OFFER_X],
                        (float) at[holeOffered ? NativeViewport.HOLE_OFFER_Y
                                : NativeViewport.PATTERN_OFFER_Y] + hitPx);
            }
            return true;
        }
        final Context context = getContext();
        final boolean face =
                edgeOffer[NativeViewport.EDGE_OFFER_MODE] == NativeViewport.EDGE_OFFER_FACE;
        final int count = (int) edgeOffer[NativeViewport.EDGE_OFFER_COUNT];
        edgeFilletCaption.setText(face ? R.string.edge_finish_fillet_face
                : R.string.edge_finish_fillet);
        edgeChamferCaption.setText(face ? R.string.edge_finish_chamfer_face
                : R.string.edge_finish_chamfer);
        edgeFillet.setContentDescription(face
                ? context.getString(R.string.edge_finish_fillet_face_description, count)
                : context.getString(R.string.edge_finish_fillet_description));
        edgeChamfer.setContentDescription(face
                ? context.getString(R.string.edge_finish_chamfer_face_description, count)
                : context.getString(R.string.edge_finish_chamfer_description));
        edgeFinishBar.setVisibility(VISIBLE);
        setVisibility(VISIBLE);
        // A face's bar stands BELOW the finger (its Sketch here chip is above);
        // a lone edge's stands above it.
        final float x = (float) edgeOffer[NativeViewport.EDGE_OFFER_X];
        final float y = (float) edgeOffer[NativeViewport.EDGE_OFFER_Y];
        if (!placeDocked(edgeFinishBar, dockedBelow)) {
            placeAt(edgeFinishBar, x, face ? y + hitPx : y - hitPx);
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // The dock beside the trailing controls (`CAD-UI-DOCK-R1`)
    // -----------------------------------------------------------------------

    /** How far down the dock the next docked surface starts, this refresh. */
    private int dockedBelow;

    /** Supplies where the trailing controls stand; the workspace owns them. */
    void setDockAnchor(DockAnchor anchor) {
        dockAnchor = anchor;
    }

    /**
     * Places {@code shown} in the dock beside the trailing controls, {@code
     * below} pixels under its top, no wider than the room between the
     * window's far edge and those controls. False when there is no dock yet
     * (no layout); the caller then falls back to the point it would stand at.
     */
    private boolean placeDocked(View shown, int below) {
        if (dockAnchor == null || getWidth() <= 0 || !dockAnchor.trailingDock(this, dockEdge)) {
            return false;
        }
        final int gap = paletteGap;
        final boolean onLeft = dockEdge[2] != 0;
        final int inner = dockEdge[0];
        final int room = onLeft ? getWidth() - inner - 2 * gap : inner - 2 * gap;
        final int top = dockEdge[1] + below;
        final int maxHeight = Math.max(hitPx, dockEdge[3] - top);
        if (room < hitPx) {
            return false;
        }
        // The natural width, then held to the room: a docked surface may never
        // stand on the controls it is docked beside.
        shown.measure(View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        final ViewGroup.LayoutParams params = shown.getLayoutParams();
        final int wantedWidth = shown.getMeasuredWidth() > room ? room
                : ViewGroup.LayoutParams.WRAP_CONTENT;
        // Held to the dock's height too: a docked editor scrolls (it stands in
        // a ScrollView) rather than running under the bottom row.
        final int wantedHeight = shown.getMeasuredHeight() > maxHeight ? maxHeight
                : ViewGroup.LayoutParams.WRAP_CONTENT;
        if (params != null && (params.width != wantedWidth || params.height != wantedHeight)) {
            params.width = wantedWidth;
            params.height = wantedHeight;
            shown.setLayoutParams(params);
        }
        shown.measure(View.MeasureSpec.makeMeasureSpec(room, View.MeasureSpec.AT_MOST),
                View.MeasureSpec.makeMeasureSpec(maxHeight, View.MeasureSpec.AT_MOST));
        final int width = shown.getMeasuredWidth();
        final int left = onLeft ? inner + gap : inner - gap - width;
        shown.setTranslationX(left - shown.getLeft());
        shown.setTranslationY(top - shown.getTop());
        return true;
    }

    /** An editor in the dock, its marker at the point it edits. */
    private void placeEditor(View editor, float pointX, float pointY) {
        markerPoint[0] = pointX;
        markerPoint[1] = pointY;
        if (!placeDocked(editor, 0)) {
            placeAt(editor, pointX, pointY + hitPx);
        }
    }

    /**
     * The marker at the point the dock's acts apply to: an open editor's
     * point, else the tapped face's, else a lone tapped edge's. Absent when
     * nothing is offered or the dock is not in use.
     */
    private void refreshOfferMarker(boolean faceOffered) {
        final boolean editing = edgeEditorKind >= 0 || patternPanel.isOpen() || holePanel.isOpen();
        float x;
        float y;
        if (editing) {
            x = markerPoint[0];
            y = markerPoint[1];
        } else if (faceOffered) {
            x = faceOfferPoint[0];
            y = faceOfferPoint[1];
        } else if (edgeFinishBar.getVisibility() == VISIBLE) {
            x = (float) edgeOffer[NativeViewport.EDGE_OFFER_X];
            y = (float) edgeOffer[NativeViewport.EDGE_OFFER_Y];
        } else {
            offerMarker.setVisibility(GONE);
            return;
        }
        if (dockAnchor == null || x <= 0f && y <= 0f) {
            offerMarker.setVisibility(GONE);
            return;
        }
        offerMarker.setVisibility(VISIBLE);
        placeAt(offerMarker, x, y);
    }

    /** Whether the offer marker is drawn; verification. */
    boolean offerMarkerShown() {
        return offerMarker.getVisibility() == VISIBLE;
    }

    // -----------------------------------------------------------------------
    // Pattern and Mirror (`CAD-PATTERN-R1`)
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // Hole (`CAD-HOLE-R1`)
    // -----------------------------------------------------------------------

    /** Opens the hole editor for a NEW hole where the face was tapped. */
    private void openNewHole() {
        if (!NativeViewport.holeOfferBegin(holeBegin)) {
            refreshFromNative();
            return;
        }
        closeEdgeFinishEditor();
        closePatternEditor();
        holeId = 0L;
        holeBodyId = (long) holeBegin[NativeViewport.HOLE_BEGIN_BODY];
        holeX = (float) holeBegin[NativeViewport.HOLE_BEGIN_X];
        holeY = (float) holeBegin[NativeViewport.HOLE_BEGIN_Y];
        holePanel.openNew(holeBegin[NativeViewport.HOLE_BEGIN_DIAMETER],
                holeBegin[NativeViewport.HOLE_BEGIN_U], holeBegin[NativeViewport.HOLE_BEGIN_V]);
        edgeFinishBar.setVisibility(GONE);
        setVisibility(VISIBLE);
        placeEditor(holeScroll, holeX, holeY);
    }

    /**
     * Opens the hole editor over a COMMITTED hole, from the History: its
     * values, Apply as an edit, and Delete. Returns false for a hole native
     * lacks.
     */
    boolean beginHoleEdit(long bodyId, long id) {
        if (!NativeViewport.cadHoleInfo(bodyId, id, holeInfo)) {
            return false;
        }
        closeEdgeFinishEditor();
        closePatternEditor();
        holeBodyId = bodyId;
        holeId = id;
        if (holeInfo[NativeViewport.HOLE_INFO_ON_SCREEN] != 0.0) {
            holeX = (float) holeInfo[NativeViewport.HOLE_INFO_X];
            holeY = (float) holeInfo[NativeViewport.HOLE_INFO_Y];
        } else {
            holeX = getWidth() * 0.5f;
            holeY = getHeight() * 0.3f;
        }
        holePanel.openExisting(holeInfo);
        edgeFinishBar.setVisibility(GONE);
        setVisibility(VISIBLE);
        placeEditor(holeScroll, holeX, holeY);
        return true;
    }

    /** Closes the hole editor without applying anything. */
    void closeHoleEditor() {
        if (!holePanel.isOpen()) {
            return;
        }
        holePanel.close();
        holeId = 0L;
        hideIme();
    }

    /** Whether the hole editor is open; verification. */
    boolean holeEditorOpen() {
        return holePanel.isOpen();
    }

    /** Opens the pattern editor for a NEW pattern of the offered feature. */
    private void openNewPattern(int kind) {
        if (!NativeViewport.patternOfferState(patternOffer)) {
            refreshFromNative();
            return;
        }
        closeEdgeFinishEditor();
        closeHoleEditor();
        patternId = 0L;
        patternFeatureId = (long) patternOffer[NativeViewport.PATTERN_OFFER_FEATURE];
        patternBodyId = (long) patternOffer[NativeViewport.PATTERN_OFFER_BODY];
        patternX = (float) patternOffer[NativeViewport.PATTERN_OFFER_X];
        patternY = (float) patternOffer[NativeViewport.PATTERN_OFFER_Y];
        patternPanel.openNew(kind, patternOffer);
        edgeFinishBar.setVisibility(GONE);
        setVisibility(VISIBLE);
        placeEditor(patternScroll, patternX, patternY);
    }

    /**
     * Opens the pattern editor over a COMMITTED pattern, from the History: its
     * values, Apply as an edit, and Delete. Returns false for a pattern native
     * lacks.
     */
    boolean beginPatternEdit(long bodyId, long id) {
        if (!NativeViewport.cadPatternInfo(bodyId, id, patternInfo)) {
            return false;
        }
        closeEdgeFinishEditor();
        closeHoleEditor();
        patternBodyId = bodyId;
        patternId = id;
        patternFeatureId = (long) patternInfo[NativeViewport.PATTERN_INFO_FEATURE];
        NativeViewport.cadPatternSeeds(bodyId, patternFeatureId, patternSeeds);
        if (patternInfo[NativeViewport.PATTERN_INFO_ON_SCREEN] != 0.0) {
            patternX = (float) patternInfo[NativeViewport.PATTERN_INFO_X];
            patternY = (float) patternInfo[NativeViewport.PATTERN_INFO_Y];
        } else {
            patternX = getWidth() * 0.5f;
            patternY = getHeight() * 0.3f;
        }
        patternPanel.openExisting(patternInfo, patternSeeds);
        edgeFinishBar.setVisibility(GONE);
        setVisibility(VISIBLE);
        placeEditor(patternScroll, patternX, patternY);
        return true;
    }

    /** Closes the pattern editor without applying anything. */
    void closePatternEditor() {
        if (!patternPanel.isOpen()) {
            return;
        }
        patternPanel.close();
        patternId = 0L;
        hideIme();
    }

    /** Whether the pattern editor is open; verification. */
    boolean patternEditorOpen() {
        return patternPanel.isOpen();
    }

    /** Opens the editor for a NEW finish over the offer, seeded with native's suggestion. */
    private void openEdgeFinishEditor(int kind) {
        if (!NativeViewport.edgeFinishOfferState(edgeOffer)) {
            refreshFromNative();
            return;
        }
        edgeEditorKind = kind;
        edgeEditorFinishId = 0L;
        edgeEditorBodyId = NativeViewport.sceneActiveBodyId();
        edgeEditorX = (float) edgeOffer[NativeViewport.EDGE_OFFER_X];
        edgeEditorY = (float) edgeOffer[NativeViewport.EDGE_OFFER_Y];
        final double suggested = edgeOffer[kind == NativeViewport.EDGE_FINISH_FILLET
                ? NativeViewport.EDGE_OFFER_SUGGEST_FILLET
                : NativeViewport.EDGE_OFFER_SUGGEST_CHAMFER];
        showEdgeFinishEditor(kind, suggested, false);
    }

    /**
     * Opens the editor over a COMMITTED finish, from the History: its value,
     * Apply as an edit, and Delete. Returns false for a finish native lacks.
     */
    boolean beginEdgeFinishEdit(long bodyId, long finishId) {
        if (!NativeViewport.cadEdgeFinishInfo(bodyId, finishId, edgeFinishInfo)) {
            return false;
        }
        edgeEditorKind = (int) edgeFinishInfo[NativeViewport.EDGE_FINISH_INFO_KIND];
        edgeEditorFinishId = finishId;
        edgeEditorBodyId = bodyId;
        if (edgeFinishInfo[NativeViewport.EDGE_FINISH_INFO_ON_SCREEN] != 0.0) {
            edgeEditorX = (float) edgeFinishInfo[NativeViewport.EDGE_FINISH_INFO_X];
            edgeEditorY = (float) edgeFinishInfo[NativeViewport.EDGE_FINISH_INFO_Y];
        } else {
            edgeEditorX = getWidth() * 0.5f;
            edgeEditorY = getHeight() * 0.5f;
        }
        showEdgeFinishEditor(edgeEditorKind,
                edgeFinishInfo[NativeViewport.EDGE_FINISH_INFO_DISTANCE], true);
        return true;
    }

    private void showEdgeFinishEditor(int kind, double meters, boolean editing) {
        closePatternEditor();
        closeHoleEditor();
        final Context context = getContext();
        final LengthUnit unit = host.uiState().displayUnit();
        final int labelRes = kind == NativeViewport.EDGE_FINISH_FILLET
                ? R.string.edge_finish_label_radius : R.string.edge_finish_label_distance;
        edgeFinishLabel.setText(labelRes);
        edgeFinishField.setText(meters > 0.0 ? unit.format(meters) : "");
        edgeFinishField.selectAll();
        edgeFinishField.setContentDescription(context.getString(
                R.string.edge_finish_field_description, context.getString(labelRes),
                context.getString(kind == NativeViewport.EDGE_FINISH_FILLET
                        ? R.string.edge_finish_fillet : R.string.edge_finish_chamfer),
                unit.label()));
        edgeFinishDelete.setVisibility(editing ? VISIBLE : GONE);
        edgeFinishBar.setVisibility(GONE);
        edgeFinishEditor.setVisibility(VISIBLE);
        setVisibility(VISIBLE);
        placeEditor(edgeFinishEditor, edgeEditorX, edgeEditorY);
        edgeFinishField.requestFocus();
        final InputMethodManager ime = (InputMethodManager)
                context.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.showSoftInput(edgeFinishField, InputMethodManager.SHOW_IMPLICIT);
        }
    }

    /** Closes the finish editor without applying anything. */
    void closeEdgeFinishEditor() {
        edgeEditorKind = -1;
        edgeEditorFinishId = 0L;
        if (edgeFinishEditor.getVisibility() != VISIBLE) {
            return;
        }
        edgeFinishEditor.setVisibility(GONE);
        edgeFinishField.clearFocus();
        hideIme();
    }

    /** Whether the finish editor is open; verification. */
    boolean edgeFinishEditorOpen() {
        return edgeEditorKind >= 0;
    }

    private void submitEdgeFinish() {
        if (edgeEditorKind < 0) {
            return;
        }
        final Context context = getContext();
        final int labelRes = edgeEditorKind == NativeViewport.EDGE_FINISH_FILLET
                ? R.string.edge_finish_label_radius : R.string.edge_finish_label_distance;
        final String raw = edgeFinishField.getText().toString();
        final BigDecimal typed;
        try {
            typed = LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                    ? context.getString(R.string.field_empty, context.getString(labelRes))
                    : context.getString(R.string.field_not_a_number,
                                        context.getString(labelRes), raw.trim()),
                    R.attr.fsTextError);
            edgeFinishField.requestFocus();
            return;
        }
        final double meters = host.uiState().displayUnit().toMeters(typed).doubleValue();
        if (actions.onCanvasEdgeFinishApply(edgeEditorKind, meters, edgeEditorBodyId,
                edgeEditorFinishId)) {
            closeEdgeFinishEditor();
        }
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
