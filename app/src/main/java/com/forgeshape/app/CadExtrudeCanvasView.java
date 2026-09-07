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
 * <p><b>Add and Cut do not exist</b> and are not drawn here, inert or
 * otherwise; nor are Symmetric and Two Sides. The badge names what this
 * extrusion does and nothing more.
 */
final class CadExtrudeCanvasView extends FrameLayout {

    /** Told what the user did; the workspace owns what each act means. */
    interface OnCanvasExtrudeAction {
        void onExtrudeDepthEntered(double depthMeters);

        void onExtrudeFlipRequested();

        void onCanvasEditSketchRequested(long bodyId);
    }

    private final InspectorHost host;
    private final OnCanvasExtrudeAction actions;
    private final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
    private final float[] bodyAnchor = new float[3];

    /** The extrude cluster: the value, the Flip and the operation badge. */
    private final LinearLayout cluster;
    private final TextView reading;
    private final LinearLayout editor;
    private final EditText field;
    private final TextView flip;
    private final TextView operation;

    /** The retained-sketch chip, shown over a committed CAD Body instead. */
    private final TextView editSketch;

    /** The body the Edit Sketch chip currently stands on, or 0. */
    private long sketchBodyId;
    /** The depth the reading last showed, in metres. Display only. */
    private double shownDepth;
    private float anchorX;
    private float anchorY;

    CadExtrudeCanvasView(Context context, InspectorHost host, OnCanvasExtrudeAction actions) {
        super(context);
        this.host = host;
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

        reading = EditorControlStyles.chip(context, R.id.cad_extrude_depth_value, "");
        reading.setMinimumHeight(control);
        reading.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openEditor();
            }
        });
        cluster.addView(reading, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

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

        field = new EditText(context);
        field.setId(R.id.field_cad_extrude_depth);
        field.setSingleLine(true);
        field.setBackgroundResource(R.drawable.bg_field);
        // A depth is unsigned — the other side is a DIRECTION, which Flip owns,
        // and a negative depth is refused below JNI rather than reinterpreted —
        // but the sign is left typeable so a mistyped value is reported by name
        // rather than silently impossible to enter.
        field.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL
                | InputType.TYPE_NUMBER_FLAG_SIGNED);
        field.setKeyListener(DigitsKeyListener.getInstance("0123456789.-"));
        field.setImeOptions(EditorInfo.IME_ACTION_DONE);
        field.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        field.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        field.setMinimumWidth(EditorControlStyles.dimen(context, R.dimen.cad_canvas_field));
        field.setMinimumHeight(control);
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
        shownDepth = tool[NativeViewport.CAD_EXTRUDE_DEPTH];
        reading.setText(unit.formatWithUnit(shownDepth));
        reading.setContentDescription(context.getString(R.string.extrude_depth_description,
                unit.formatWithUnit(shownDepth)));
        // A live drag rewrites the value under the user; an editor open over it
        // would submit a number the arrow has already left behind.
        if (tool[NativeViewport.CAD_EXTRUDE_DRAGGING] != 0.0) {
            closeEditor();
        }
        cluster.setVisibility(editorOpen() ? GONE : VISIBLE);
        setVisibility(VISIBLE);
        anchorX = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X];
        anchorY = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        place(editorOpen() ? editor : cluster,
                (float) tool[NativeViewport.CAD_EXTRUDE_SCALE]);
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

    /**
     * Centres one child on the anchor at the camera-attached scale.
     *
     * <p>The scale is applied to the child rather than to this container, so
     * the translation stays in unscaled pixels and the anchor is exactly where
     * native said it was. The measured box is kept inside the parent, because a
     * value half outside the window is one the user can neither read nor tap.
     */
    private void place(View shown, float scale) {
        float k = scale;
        if (!(k > 0.0f) || Float.isNaN(k)) {
            k = 1.0f;
        }
        shown.setPivotX(0.0f);
        shown.setPivotY(0.0f);
        shown.setScaleX(k);
        shown.setScaleY(k);
        shown.measure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final float width = shown.getMeasuredWidth() * k;
        final float height = shown.getMeasuredHeight() * k;
        final ViewGroup parent = (ViewGroup) getParent();
        float left = anchorX - width * 0.5f;
        float top = anchorY - height * 0.5f;
        if (parent != null) {
            left = Math.max(0.0f, Math.min(left, parent.getWidth() - width));
            top = Math.max(0.0f, Math.min(top, parent.getHeight() - height));
        }
        setTranslationX(left);
        setTranslationY(top);
    }

    /** Whether the numeric editor is open, for verification. */
    boolean editorOpen() {
        return editor.getVisibility() == VISIBLE;
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

    /** Closes the editor without submitting. The authored depth is unchanged. */
    void closeEditor() {
        if (editor.getVisibility() != VISIBLE) {
            return;
        }
        editor.setVisibility(GONE);
        field.clearFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.hideSoftInputFromWindow(getWindowToken(), 0);
        }
    }

    /** Withdraws the whole surface. */
    void hide() {
        closeEditor();
        cluster.setVisibility(GONE);
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
        final Context context = getContext();
        final String raw = field.getText().toString();
        final BigDecimal typed;
        try {
            typed = LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                    ? context.getString(R.string.field_empty,
                                        context.getString(R.string.label_depth_canvas))
                    : context.getString(R.string.field_not_a_number,
                                        context.getString(R.string.label_depth_canvas),
                                        raw.trim()),
                    R.attr.fsTextError);
            field.requestFocus();
            return;
        }
        actions.onExtrudeDepthEntered(
                host.uiState().displayUnit().toMeters(typed).doubleValue());
    }

    /** Layout params for the cluster: absolutely placed inside the overlay. */
    static FrameLayout.LayoutParams anchoredParams() {
        return new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }
}
