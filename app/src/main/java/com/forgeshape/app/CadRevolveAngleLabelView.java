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

/**
 * The Revolve angle beside its ring (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`).
 *
 * <p>The ring itself — the axis, the arc from the profile to the current angle
 * and its handle — is world geometry the renderer draws from the sketch
 * overlay, so it foreshortens and turns with the part. What is not geometry is
 * the NUMBER: it has to be legible and editable by typing, so it is this small
 * chrome label standing at the point native projects halfway round the arc,
 * on the {@link SketchDimensionLabelView} pattern a fourth time.
 *
 * <p>Holds no angle: the value is read back from native on every refresh and a
 * typed one is submitted whole in degrees, exactly as typed. Hidden whenever
 * native says the anchor does not project, there is no axis yet, or no revolve
 * is open — a label with nowhere honest to stand is not drawn at a guess.
 */
final class CadRevolveAngleLabelView extends FrameLayout {

    /** Told that an exact angle was typed; the workspace owns what it means. */
    interface OnRevolveAngleAction {
        void onRevolveAngleEntered(double degrees);
    }

    private final OnRevolveAngleAction actions;
    private final InspectorHost host;
    private final ViewportAnchorSpace anchorSpace;
    private final double[] state = new double[NativeViewport.REVOLVE_STATE_SIZE];

    private final TextView reading;
    private final LinearLayout editor;
    private final EditText field;
    private float anchorX;
    private float anchorY;

    CadRevolveAngleLabelView(Context context, InspectorHost host, ViewportAnchorSpace anchorSpace,
                             OnRevolveAngleAction actions) {
        super(context);
        this.host = host;
        this.anchorSpace = anchorSpace;
        this.actions = actions;
        setId(R.id.cad_revolve_angle_label);
        setVisibility(GONE);

        reading = EditorControlStyles.chip(context, R.id.cad_revolve_angle_value, "");
        reading.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openEditor();
            }
        });
        addView(reading, new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        editor = new LinearLayout(context);
        editor.setId(R.id.cad_revolve_angle_editor);
        editor.setOrientation(LinearLayout.HORIZONTAL);
        editor.setGravity(Gravity.CENTER_VERTICAL);
        editor.setBackgroundResource(R.drawable.bg_surface_context);
        final int pad = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        editor.setPadding(pad, pad, pad, pad);
        editor.setVisibility(GONE);

        field = new EditText(context);
        field.setId(R.id.field_revolve_angle_canvas);
        field.setSingleLine(true);
        field.setBackgroundResource(R.drawable.bg_field);
        // Signed accepted so a mistyped value is REPORTED by name below JNI
        // rather than impossible to enter; the range is native's to judge.
        field.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL
                | InputType.TYPE_NUMBER_FLAG_SIGNED);
        field.setKeyListener(DigitsKeyListener.getInstance("0123456789.,-"));
        field.setImeOptions(EditorInfo.IME_ACTION_DONE);
        field.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        field.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        field.setMinimumWidth(EditorControlStyles.dimen(context, R.dimen.sketch_dimension_field));
        field.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
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
                R.id.apply_revolve_angle, context.getString(R.string.apply));
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
    }

    /** Re-reads native and places the label, or withdraws it. */
    void refreshFromNative() {
        NativeViewport.cadRevolveToolState(state);
        if (!CadRevolvePresentation.labelShown(state)) {
            close();
            return;
        }
        final double degrees = state[NativeViewport.REVOLVE_ANGLE];
        final String text = CadRevolvePresentation.label(degrees);
        reading.setText(text);
        reading.setContentDescription(
                getContext().getString(R.string.revolve_angle_description, text));
        anchorX = (float) state[NativeViewport.REVOLVE_LABEL_X];
        anchorY = (float) state[NativeViewport.REVOLVE_LABEL_Y];
        setVisibility(VISIBLE);
        anchorSpace.measureAndPlace(this, anchorX, anchorY, 1.0f);
    }

    /** Whether the angle editor is open, for verification. */
    boolean editorOpen() {
        return editor.getVisibility() == VISIBLE;
    }

    void openEditor() {
        if (getVisibility() != VISIBLE) {
            return;
        }
        field.setText(CadRevolvePresentation.formatDegrees(state[NativeViewport.REVOLVE_ANGLE]));
        field.selectAll();
        reading.setVisibility(GONE);
        editor.setVisibility(VISIBLE);
        anchorSpace.measureAndPlace(this, anchorX, anchorY, 1.0f);
        field.requestFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.showSoftInput(field, InputMethodManager.SHOW_IMPLICIT);
        }
    }

    /** Closes the editor without submitting. The angle is unchanged. */
    void closeEditor() {
        if (editor.getVisibility() != VISIBLE) {
            return;
        }
        editor.setVisibility(GONE);
        reading.setVisibility(VISIBLE);
        field.clearFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.hideSoftInputFromWindow(getWindowToken(), 0);
        }
    }

    private void close() {
        closeEditor();
        setVisibility(GONE);
    }

    private void submit() {
        final Context context = getContext();
        final String raw = field.getText().toString();
        final double degrees = CadRevolvePresentation.parseDegrees(raw);
        if (Double.isNaN(degrees)) {
            host.showStatus(raw.trim().isEmpty()
                    ? context.getString(R.string.field_empty, context.getString(R.string.label_angle))
                    : context.getString(R.string.field_not_a_number,
                            context.getString(R.string.label_angle), raw.trim()),
                    R.attr.fsTextError);
            field.requestFocus();
            return;
        }
        actions.onRevolveAngleEntered(degrees);
    }

    static FrameLayout.LayoutParams anchoredParams() {
        return new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }
}
