package com.forgeshape.app;

import android.content.Context;
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
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The canvas-first control of the extrusion (`CAD-UX-S1`).
 *
 * <p>The <b>arrow</b> along the extrusion normal is drawn by the renderer from
 * the sketch overlay's own line list, because it is geometry-shaped and belongs
 * in the same space as the profile it grows out of. What is <b>not</b>
 * geometry-shaped is this: the exact depth, which has to be legible at any zoom
 * and editable by typing; the Flip that reverses which side the solid grows on;
 * and the badge that says the operation is <i>New Body</i>. Those are chrome,
 * positioned over the viewport at the anchor native reports — the same pattern
 * {@link SketchDimensionLabelView} and {@link BodyDimensionLabelsView} already
 * use, a third time.
 *
 * <p><b>Holds no semantics.</b> The depth, the direction, the profile and the
 * anchor are all read back from native on every refresh and every value typed
 * here is submitted whole. There is no draft direction, no draft depth and no
 * cached anchor in this class: a desktop adapter would replace this file and
 * nothing below JNI would notice.
 *
 * <p><b>Camera-attached, not screen-pinned.</b> The cluster is drawn at the
 * multiplier native derives from the camera, so it visibly shrinks as the
 * camera pulls back from the work and grows as it comes in, saturating at both
 * ends. Its controls are authored at {@code R.dimen.cad_canvas_control} 60dp
 * and the multiplier's floor is 0.80, so the smallest a live control can ever
 * be drawn is exactly the 48dp interactive floor.
 *
 * <p><b>Two states, and only one is ever shown.</b> While a sketch is in its
 * Ready state the extrude cluster stands at the arrow; over a committed CAD
 * Body with no session open, a single <i>Edit Sketch</i> chip stands on the
 * body's own sketch, because a sketch survives its extrusion and the UI must
 * not suggest that Extrude consumed it. Neither is drawn when native says its
 * anchor does not project on screen — a control with nowhere honest to stand is
 * hidden rather than placed at a guess.
 *
 * <p><b>Three extents, one model</b> (`CAD-EXT-R1`). One Side, Symmetric and
 * Two Sides are three ways of authoring the same two distances, and this class
 * holds none of them: the selector sends a mode, each value field sends ONE
 * side's distance, and everything drawn is re-read from native on the next
 * refresh. Flip belongs to One Side alone and is ABSENT in the other two —
 * Symmetric already reaches both sides and Two Sides states both explicitly, so
 * there is no side left for it to choose, and native refuses it there too.
 *
 * <p><b>Add and Cut do not exist</b> and are not drawn here, inert or
 * otherwise. The badge names what this extrusion does and nothing more.
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

        void onCanvasEditSketchRequested(long bodyId);
    }

    private final InspectorHost host;
    private final OnCanvasExtrudeAction actions;
    /** The ONE viewport-anchor conversion; see {@link ViewportAnchorSpace}. */
    private final ViewportAnchorSpace anchorSpace;
    private final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
    private final float[] bodyAnchor = new float[3];

    /** The extrude cluster: the extent selector, the value, Flip and the badge. */
    private final LinearLayout cluster;
    private final TextView reading;
    private final LinearLayout editor;
    private final EditText field;
    private final TextView flip;
    private final TextView operation;
    private final TextView extentOneSide;
    private final TextView extentSymmetric;
    private final TextView extentTwoSides;

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

    /** The retained-sketch chip, shown over a committed CAD Body instead. */
    private final TextView editSketch;

    /** The body the Edit Sketch chip currently stands on, or 0. */
    private long sketchBodyId;
    /** The depth the reading last showed, in metres. Display only. */
    private double shownDepth;
    /** The second side's distance last shown, in metres. Display only. */
    private double shownSecond;
    private float anchorX;
    private float anchorY;
    private float secondAnchorX;
    private float secondAnchorY;

    CadExtrudeCanvasView(Context context, InspectorHost host, ViewportAnchorSpace anchorSpace,
                         OnCanvasExtrudeAction actions) {
        super(context);
        this.host = host;
        this.anchorSpace = anchorSpace;
        this.actions = actions;
        setId(R.id.cad_extrude_canvas);
        setVisibility(GONE);

        final int pad = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        final int control = EditorControlStyles.dimen(context, R.dimen.cad_canvas_control);

        cluster = new LinearLayout(context);
        cluster.setOrientation(LinearLayout.HORIZONTAL);
        cluster.setGravity(Gravity.CENTER_VERTICAL);
        cluster.setBackgroundResource(R.drawable.bg_surface_context);
        cluster.setPadding(pad, pad, pad, pad);

        // The extent selector, FIRST in the cluster: which combination of the
        // two distances is being authored is the question the value beside it
        // answers, so it is read before the number rather than after it.
        extentOneSide = extentChip(context, R.id.cad_extrude_extent_one_side,
                R.string.extent_one_side, R.string.extent_one_side_description,
                NativeViewport.EXTENT_ONE_SIDE, control);
        extentSymmetric = extentChip(context, R.id.cad_extrude_extent_symmetric,
                R.string.extent_symmetric, R.string.extent_symmetric_description,
                NativeViewport.EXTENT_SYMMETRIC, control);
        extentTwoSides = extentChip(context, R.id.cad_extrude_extent_two_sides,
                R.string.extent_two_sides, R.string.extent_two_sides_description,
                NativeViewport.EXTENT_TWO_SIDES, control);
        cluster.addView(extentOneSide, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        final LinearLayout.LayoutParams extentParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        extentParams.leftMargin = pad;
        cluster.addView(extentSymmetric, extentParams);
        final LinearLayout.LayoutParams twoParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        twoParams.leftMargin = pad;
        cluster.addView(extentTwoSides, twoParams);

        reading = EditorControlStyles.chip(context, R.id.cad_extrude_depth_value, "");
        reading.setMinimumHeight(control);
        reading.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openEditor();
            }
        });
        final LinearLayout.LayoutParams readingParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        readingParams.leftMargin = pad;
        cluster.addView(reading, readingParams);

        // Flip is ONE tap next to the geometry, which is the whole point of it
        // being here: the same act exists as a chip in the precision panel, and
        // both write exactly the same native direction.
        flip = EditorControlStyles.chip(context, R.id.cad_extrude_flip,
                context.getString(R.string.extrude_flip));
        flip.setMinimumHeight(control);
        flip.setMinimumWidth(control);
        flip.setContentDescription(context.getString(R.string.extrude_flip) + ". "
                + context.getString(R.string.extrude_flip_description));
        flip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                CadExtrudeCanvasView.this.actions.onExtrudeFlipRequested();
            }
        });
        final LinearLayout.LayoutParams flipParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        flipParams.leftMargin = pad;
        cluster.addView(flip, flipParams);

        // A LABEL and not a control: there is exactly one operation and there is
        // nothing here to choose between, so this states what is happening
        // rather than offering an Add and a Cut that do not exist.
        operation = EditorControlStyles.titleText(context, R.id.cad_extrude_operation,
                context.getString(R.string.extrude_operation_new_body));
        operation.setContentDescription(
                context.getString(R.string.extrude_operation_new_body_description));
        final LinearLayout.LayoutParams operationParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        operationParams.leftMargin = pad;
        cluster.addView(operation, operationParams);

        addView(cluster, new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        editor = new LinearLayout(context);
        editor.setId(R.id.cad_extrude_depth_editor);
        editor.setOrientation(LinearLayout.HORIZONTAL);
        editor.setGravity(Gravity.CENTER_VERTICAL);
        editor.setBackgroundResource(R.drawable.bg_surface_context);
        editor.setPadding(pad, pad, pad, pad);
        editor.setVisibility(GONE);

        field = distanceField(context, R.id.field_cad_extrude_depth, control);
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
        editor.addView(field, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        final TextView apply = EditorControlStyles.primaryButton(context,
                R.id.apply_cad_extrude_depth, context.getString(R.string.apply));
        apply.setMinimumHeight(control);
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                submit();
            }
        });
        final LinearLayout.LayoutParams applyParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        applyParams.leftMargin = pad;
        editor.addView(apply, applyParams);

        addView(editor, new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // The SECOND side's cluster, at the second arrow. Its own anchor, its
        // own editor, and exactly one value: Two Sides is the only mode with a
        // second distance to state.
        secondCluster = new LinearLayout(context);
        secondCluster.setOrientation(LinearLayout.HORIZONTAL);
        secondCluster.setGravity(Gravity.CENTER_VERTICAL);
        secondCluster.setBackgroundResource(R.drawable.bg_surface_context);
        secondCluster.setPadding(pad, pad, pad, pad);
        secondCluster.setVisibility(GONE);
        secondReading = EditorControlStyles.chip(context, R.id.cad_extrude_second_value, "");
        secondReading.setMinimumHeight(control);
        secondReading.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openSecondEditor();
            }
        });
        secondCluster.addView(secondReading, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        addView(secondCluster, new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        secondEditor = new LinearLayout(context);
        secondEditor.setId(R.id.cad_extrude_second_editor);
        secondEditor.setOrientation(LinearLayout.HORIZONTAL);
        secondEditor.setGravity(Gravity.CENTER_VERTICAL);
        secondEditor.setBackgroundResource(R.drawable.bg_surface_context);
        secondEditor.setPadding(pad, pad, pad, pad);
        secondEditor.setVisibility(GONE);
        secondField = distanceField(context, R.id.field_cad_extrude_second, control);
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
        secondEditor.addView(secondField, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        final TextView applySecond = EditorControlStyles.primaryButton(context,
                R.id.apply_cad_extrude_second, context.getString(R.string.apply));
        applySecond.setMinimumHeight(control);
        applySecond.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                submitSecond();
            }
        });
        final LinearLayout.LayoutParams applySecondParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        applySecondParams.leftMargin = pad;
        secondEditor.addView(applySecond, applySecondParams);
        addView(secondEditor, new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        editSketch = EditorControlStyles.secondaryActionChip(context, R.id.cad_canvas_edit_sketch,
                context.getString(R.string.edit_sketch));
        editSketch.setMinimumHeight(control);
        editSketch.setVisibility(GONE);
        editSketch.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (sketchBodyId != NativeViewport.NO_OBJECT) {
                    CadExtrudeCanvasView.this.actions.onCanvasEditSketchRequested(sketchBodyId);
                }
            }
        });
        addView(editSketch, new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    /**
     * One exact-distance field. Both sides get an identical one, because both
     * take the same kind of value and a second set of rules for the second side
     * would be a second answer to what a distance is.
     */
    private EditText distanceField(Context context, int id, int control) {
        final EditText made = new EditText(context);
        made.setId(id);
        made.setSingleLine(true);
        made.setBackgroundResource(R.drawable.bg_field);
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
        made.setMinimumHeight(control);
        return made;
    }

    /** One extent chip. Holds no state: the tap sends a mode and nothing else. */
    private TextView extentChip(Context context, int id, int labelRes, int descriptionRes,
                                final int mode, int control) {
        final TextView chip = EditorControlStyles.chip(context, id, context.getString(labelRes));
        chip.setMinimumHeight(control);
        chip.setContentDescription(
                context.getString(labelRes) + ". " + context.getString(descriptionRes));
        chip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                closeEditor();
                closeSecondEditor();
                actions.onExtrudeExtentRequested(mode);
            }
        });
        return chip;
    }

    /**
     * Re-reads native and shows the extrude cluster, the retained-sketch chip,
     * or neither.
     *
     * <p>Called on every state change and on every viewport gesture sample, so
     * the value follows a live arrow drag and the anchor follows an orbit.
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
        editSketch.setVisibility(GONE);
        final Context context = getContext();
        final LengthUnit unit = host.uiState().displayUnit();
        final int extent = (int) tool[NativeViewport.CAD_EXTRUDE_EXTENT];
        final boolean twoSides = extent == NativeViewport.EXTENT_TWO_SIDES;
        EditorControlStyles.setChipActive(extentOneSide,
                extent == NativeViewport.EXTENT_ONE_SIDE);
        EditorControlStyles.setChipActive(extentSymmetric,
                extent == NativeViewport.EXTENT_SYMMETRIC);
        EditorControlStyles.setChipActive(extentTwoSides, twoSides);
        // Flip belongs to One Side alone: the other two reach both sides
        // already, so there is no side left for it to choose. Absent rather
        // than shown and refused.
        flip.setVisibility(extent == NativeViewport.EXTENT_ONE_SIDE ? VISIBLE : GONE);

        shownDepth = tool[NativeViewport.CAD_EXTRUDE_DEPTH];
        reading.setText(unit.formatWithUnit(shownDepth));
        reading.setContentDescription(context.getString(
                extent == NativeViewport.EXTENT_SYMMETRIC ? R.string.extrude_each_side_description
                        : twoSides ? R.string.extrude_side_a_description
                                   : R.string.extrude_depth_description,
                unit.formatWithUnit(shownDepth)));
        // A live drag rewrites the value under the user; an editor open over it
        // would submit a number the arrow has already left behind.
        if (tool[NativeViewport.CAD_EXTRUDE_DRAGGING] != 0.0) {
            closeEditor();
            closeSecondEditor();
        }
        cluster.setVisibility(editorOpen() ? GONE : VISIBLE);
        setVisibility(VISIBLE);
        anchorX = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X];
        anchorY = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        place(editorOpen() ? editor : cluster,
                (float) tool[NativeViewport.CAD_EXTRUDE_SCALE]);

        // The second value exists in Two Sides alone, and only where native
        // says its anchor projects. Symmetric draws two ARROWS and one number,
        // because the two sides are one distance.
        final boolean secondShown =
                twoSides && tool[NativeViewport.CAD_EXTRUDE_SECOND_ON_SCREEN] != 0.0;
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
        placeAt(secondEditorOpen() ? secondEditor : secondCluster,
                (float) tool[NativeViewport.CAD_EXTRUDE_SCALE], secondAnchorX, secondAnchorY);
    }

    private void showRetainedSketchChip() {
        closeEditor();
        cluster.setVisibility(GONE);
        // The chip belongs to a committed CAD Body with no session open, and to
        // nothing else. Every condition below is one `sketchBeginEdit` would
        // refuse, so the control is absent rather than shown and then refused.
        final long body = NativeViewport.sceneActiveBodyId();
        if (body == NativeViewport.NO_OBJECT
                || !NativeViewport.cadBodySketchAnchor(body, bodyAnchor)) {
            hide();
            return;
        }
        sketchBodyId = body;
        final Context context = getContext();
        editSketch.setContentDescription(context.getString(R.string.edit_sketch_canvas_description,
                BodyLabels.of(context, body)));
        editSketch.setVisibility(VISIBLE);
        setVisibility(VISIBLE);
        anchorX = bodyAnchor[0];
        anchorY = bodyAnchor[1];
        place(editSketch, bodyAnchor[2]);
    }

    /** Centres one child on the primary anchor at the camera-attached scale. */
    private void place(View shown, float scale) {
        placeAt(shown, scale, anchorX, anchorY);
    }

    /**
     * Centres one child on an anchor at the camera-attached scale.
     *
     * <p>The scale and the translation are applied to the CHILD rather than to
     * this container, because `CAD-EXT-R1` gave the cluster a sibling standing
     * at a different anchor and one container cannot be in two places. The
     * measured box is kept inside this view, because a value half outside the
     * window is one the user can neither read nor tap.
     */
    private void placeAt(View shown, float scale, float x, float y) {
        float k = scale;
        if (!(k > 0.0f) || Float.isNaN(k)) {
            k = 1.0f;
        }
        shown.setPivotX(0.0f);
        shown.setPivotY(0.0f);
        shown.setScaleX(k);
        shown.setScaleY(k);
        // The camera-attached multiplier is `CAD-UX-S1`'s and is untouched here:
        // it decides how BIG the cluster is drawn, and this decides only WHERE
        // that box is centred. The conversion from the viewport pixels native
        // reports into this container's own translation space, and the clamp
        // against the real viewport rather than this padded container
        // (`UI3D-F-006`), are the shared contract in ViewportAnchorSpace.
        anchorSpace.measureAndPlace(shown, x, y, k);
    }

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

    /** The body the retained-sketch chip stands on, or 0. For verification. */
    long retainedSketchBodyId() {
        return sketchBodyId;
    }

    private void openEditor() {
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0) {
            return;
        }
        final LengthUnit unit = host.uiState().displayUnit();
        // Seeded with the CURRENT depth and selected whole, so the first
        // keystroke replaces rather than appends — the rule every exact-value
        // field in the product follows.
        field.setText(unit.format(shownDepth));
        field.selectAll();
        cluster.setVisibility(GONE);
        editor.setVisibility(VISIBLE);
        place(editor, (float) tool[NativeViewport.CAD_EXTRUDE_SCALE]);
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
                || extentMode() != NativeViewport.EXTENT_TWO_SIDES) {
            return;
        }
        final LengthUnit unit = host.uiState().displayUnit();
        secondField.setText(unit.format(shownSecond));
        secondField.selectAll();
        secondCluster.setVisibility(GONE);
        secondEditor.setVisibility(VISIBLE);
        placeAt(secondEditor, (float) tool[NativeViewport.CAD_EXTRUDE_SCALE], secondAnchorX,
                secondAnchorY);
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
        cluster.setVisibility(GONE);
        secondCluster.setVisibility(GONE);
        editSketch.setVisibility(GONE);
        sketchBodyId = NativeViewport.NO_OBJECT;
        setVisibility(GONE);
    }

    /**
     * Parses the field and submits it as METRES.
     *
     * <p>An unparseable value is reported by name and the field keeps focus so
     * it can be corrected; a value the domain refuses is reported the same way.
     * In neither case does one authored value move — a typed depth is refused,
     * never clamped, which is the one place it differs from a drag.
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
     *
     * <p>In One Side the SIDE the value lands on is the side the solid is
     * already on, so the positive selector below means "the primary side"
     * there; the workspace resolves it against the direction native reports,
     * which is what keeps this class free of a second model of the extrusion.
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
     * Layout params for the host: the whole overlay, with each cluster placed
     * inside it by translation.
     *
     * <p>The container itself is never clickable and paints nothing, so a touch
     * that misses every chip reaches the viewport exactly as it did before —
     * the same arrangement {@link BodyDimensionLabelsView} uses to stand three
     * labels at three anchors.
     */
    static FrameLayout.LayoutParams anchoredParams() {
        return new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
    }
}
