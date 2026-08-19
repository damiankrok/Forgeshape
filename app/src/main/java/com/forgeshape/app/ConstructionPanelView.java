package com.forgeshape.app;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The Construction properties editor.
 *
 * <p>A compact overlay above the viewport with two independent sections:
 *
 * <ul>
 *   <li><b>Shape</b> — which primitive the one Construction object is (Box,
 *       Cylinder, Sphere, Cone or Capsule) and that primitive's exact
 *       parameters, with <i>Apply Shape</i>.
 *       These define what the object <i>is</i>, and applying them republishes its
 *       mesh.</li>
 *   <li><b>Position and Rotation</b> — with <i>Apply Transform</i>. These define
 *       where the object <i>sits</i>, and applying them publishes no mesh at
 *       all.</li>
 * </ul>
 *
 * <p>Both share one mm/cm/m display unit, which governs every length: the box's
 * width/height/depth, the cylinder's diameter/height, the sphere's diameter, the
 * cone's bottom diameter/height, the capsule's diameter/total height, and the
 * position. Rotation is always degrees and is never converted.
 *
 * <p><b>This view is presentation and input only.</b> It holds field text, the
 * selected display unit and a <i>draft</i> primitive kind; it holds no dimension
 * and no transform. The authoritative values are native doubles — meters for
 * lengths, degrees for angles — and the only way this class can change anything
 * is to submit a complete section at once through {@link NativeViewport} and
 * accept the verdict. In particular, moving the shape selector changes nothing
 * but which fields are on screen: the object's kind changes only when Apply
 * Shape is pressed, and then through the native method belonging to that
 * primitive alone.
 *
 * <p>Built from plain framework views (no Compose, no AndroidX, no design
 * system) and laid out in code, because the project ships no resource layouts.
 */
final class ConstructionPanelView extends LinearLayout {

    /** Field order within every section. */
    private static final int X = 0;
    private static final int Y = 1;
    private static final int Z = 2;

    /**
     * Two-parameter primitive field order. Slot 0 is always the diameter and
     * slot 1 always the length along the axis, whichever primitive it is — so a
     * row's slots never change meaning, only their labels do.
     */
    private static final int DIAMETER = 0;
    private static final int AXIAL = 1;

    private static final String[] BOX_LABELS = {"Width", "Height", "Depth"};
    private static final String[] CYLINDER_LABELS = {"Diameter", "Height"};
    private static final String[] SPHERE_LABELS = {"Diameter"};
    private static final String[] CONE_LABELS = {"Diameter", "Height"};
    private static final String[] CAPSULE_LABELS = {"Diameter", "Total Height"};
    private static final String[] POSITION_LABELS = {"Pos X", "Pos Y", "Pos Z"};
    private static final String[] ROTATION_LABELS = {"Rot X", "Rot Y", "Rot Z"};

    private static final int COLOR_PANEL = 0xF0141A22;
    private static final int COLOR_FIELD = 0xFF232C38;
    private static final int COLOR_LABEL = 0xFF9AA7B8;
    private static final int COLOR_SECTION = 0xFF6E8CAE;
    private static final int COLOR_TEXT = 0xFFF2F5F9;
    private static final int COLOR_OK = 0xFF7BD88F;
    private static final int COLOR_NEUTRAL = 0xFF9AA7B8;
    private static final int COLOR_ERROR = 0xFFFF8A7A;
    private static final int COLOR_ACCENT = 0xFF3E6FA8;

    private final EditText[] boxFields = new EditText[3];
    private final EditText[] cylinderFields = new EditText[2];
    private final EditText[] sphereFields = new EditText[1];
    private final EditText[] coneFields = new EditText[2];
    private final EditText[] capsuleFields = new EditText[2];
    private final EditText[] positionFields = new EditText[3];
    private final EditText[] rotationFields = new EditText[3];
    private final RadioButton[] kindButtons = new RadioButton[5];
    private View boxRow;
    private View cylinderRow;
    private View sphereRow;
    private View coneRow;
    private View capsuleRow;
    private Button resumeSculptButton;
    private TextView positionSectionLabel;
    private final TextView statusLine;

    /** Reused across reads; native fills these with authoritative values. */
    private final double[] nativePrimitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
    private final double[] nativeTransform = new double[6];
    private final double[] nativeSculpt = new double[NativeViewport.SCULPT_STATE_SIZE];

    /** The view that gets focus back once editing is done. */
    private final View viewportView;

    /**
     * Told after the product mode changed, so the host can swap which panel is
     * on screen. The panel itself owns no mode: it asks native code to freeze,
     * and then reports that something happened.
     */
    private final Runnable onModeChanged;

    /**
     * The unit every length field is currently written in. Presentation state,
     * process-scoped: it survives home/resume with the Activity and resets to
     * meters only when the process restarts.
     */
    private LengthUnit unit = LengthUnit.METERS;

    /**
     * The primitive the panel is currently showing fields for.
     *
     * <p><b>Draft only.</b> It is not the object's kind: it decides which fields
     * are visible and which parameters Apply Shape will submit. Native code is
     * not told about it until then.
     */
    private int draftKind = NativeViewport.PRIMITIVE_BOX;

    /** Set while this class drives a RadioGroup, so it does not react to itself. */
    private boolean settingSelectionProgrammatically;

    ConstructionPanelView(Context context, View viewportView, Runnable onModeChanged) {
        super(context);
        this.viewportView = viewportView;
        this.onModeChanged = onModeChanged;

        setOrientation(VERTICAL);
        setBackground(panelBackground());
        setPadding(dp(14), dp(8), dp(14), dp(8));
        // The panel is opaque to touch: see onTouchEvent.
        setClickable(true);

        addView(buildShapeSelectorRow(context));
        boxRow = buildFieldRow(context, BOX_LABELS, boxFields, dp(2));
        addView(boxRow);
        cylinderRow = buildFieldRow(context, CYLINDER_LABELS, cylinderFields, dp(2));
        addView(cylinderRow);
        sphereRow = buildFieldRow(context, SPHERE_LABELS, sphereFields, dp(2));
        addView(sphereRow);
        coneRow = buildFieldRow(context, CONE_LABELS, coneFields, dp(2));
        addView(coneRow);
        capsuleRow = buildFieldRow(context, CAPSULE_LABELS, capsuleFields, dp(2));
        addView(capsuleRow);
        addView(buildUnitAndApplyShapeRow(context));

        positionSectionLabel = sectionLabel(context, "Position");
        addView(positionSectionLabel, sectionParams(dp(8)));
        addView(buildFieldRow(context, POSITION_LABELS, positionFields, dp(2)));

        addView(sectionLabel(context, "Rotation (degrees)"), sectionParams(dp(6)));
        addView(buildFieldRow(context, ROTATION_LABELS, rotationFields, dp(2)));
        addView(buildApplyTransformRow(context));
        addView(buildModeRow(context));

        statusLine = new TextView(context);
        statusLine.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12.0f);
        statusLine.setTextColor(COLOR_NEUTRAL);
        statusLine.setPadding(0, dp(6), 0, 0);
        addView(statusLine);

        refreshFromNative();
        setStatus("Shape and placement — edit, then Apply.", COLOR_NEUTRAL);
    }

    // -----------------------------------------------------------------------
    // Construction of the view tree
    // -----------------------------------------------------------------------

    private TextView sectionLabel(Context context, String text) {
        final TextView label = new TextView(context);
        label.setText(text);
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12.0f);
        label.setTextColor(COLOR_SECTION);
        return label;
    }

    private LinearLayout.LayoutParams sectionParams(int topMargin) {
        final LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = topMargin;
        return params;
    }

    /**
     * The primitive selector.
     *
     * <p>Choosing a primitive here only swaps which parameter fields are shown.
     * It is a draft: the object keeps being whatever it already is until Apply
     * Shape submits the selected kind together with its parameters.
     */
    private View buildShapeSelectorRow(Context context) {
        // A column, not a row: five primitives beside a "Shape" caption would
        // overflow a phone's width and clip the last button off the screen. The
        // caption gets its own line and the selector gets the whole width.
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(VERTICAL);
        row.setLayoutParams(sectionParams(0));

        row.addView(sectionLabel(context, "Shape"));

        final RadioGroup group = new RadioGroup(context);
        group.setOrientation(HORIZONTAL);
        // Index is the kind: PRIMITIVE_BOX, PRIMITIVE_CYLINDER, PRIMITIVE_SPHERE,
        // PRIMITIVE_CONE, PRIMITIVE_CAPSULE.
        final String[] labels = {"Box", "Cylinder", "Sphere", "Cone", "Capsule"};
        for (int i = 0; i < labels.length; i++) {
            final RadioButton button = new RadioButton(context);
            button.setId(kindViewId(i));
            button.setText(labels[i]);
            button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11.0f);
            button.setTextColor(COLOR_TEXT);
            button.setContentDescription("Shape " + labels[i]);
            kindButtons[i] = button;
            // Equal shares of the width rather than wrap-content, so the row
            // cannot overflow and clip a button that has to stay tappable.
            final RadioGroup.LayoutParams params =
                    new RadioGroup.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
            group.addView(button, params);
        }
        settingSelectionProgrammatically = true;
        group.check(kindViewId(draftKind));
        settingSelectionProgrammatically = false;
        group.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup radioGroup, int checkedId) {
                if (settingSelectionProgrammatically) {
                    return;
                }
                for (int i = 0; i < labels.length; i++) {
                    if (checkedId == kindViewId(i)) {
                        onDraftKindSelected(i);
                        return;
                    }
                }
            }
        });
        row.addView(group, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        return row;
    }

    private View buildFieldRow(Context context, String[] labels, EditText[] out, int topMargin) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        row.setLayoutParams(sectionParams(topMargin));

        for (int i = 0; i < labels.length; i++) {
            final LinearLayout column = new LinearLayout(context);
            column.setOrientation(VERTICAL);

            final TextView label = new TextView(context);
            label.setText(labels[i]);
            label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10.0f);
            label.setTextColor(COLOR_LABEL);
            column.addView(label);

            final EditText field = new EditText(context);
            // A numeric keyboard, but with a key listener that also accepts a
            // comma decimal separator and a minus sign. setRawInputType is what
            // keeps both: it tells the IME what to show without replacing the
            // key listener the way setInputType would. The minus sign matters
            // twice over here — a negative coordinate or angle is an ordinary
            // value that must be typeable, and a negative dimension must be
            // typeable so it can be visibly refused rather than unreachable.
            field.setKeyListener(DigitsKeyListener.getInstance("0123456789.,-"));
            field.setRawInputType(InputType.TYPE_CLASS_NUMBER
                    | InputType.TYPE_NUMBER_FLAG_DECIMAL
                    | InputType.TYPE_NUMBER_FLAG_SIGNED);
            field.setSingleLine(true);
            field.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15.0f);
            field.setTextColor(COLOR_TEXT);
            field.setBackground(fieldBackground());
            field.setPadding(dp(9), dp(6), dp(9), dp(6));
            field.setImeOptions(i == labels.length - 1 ? EditorInfo.IME_ACTION_DONE
                                                       : EditorInfo.IME_ACTION_NEXT);
            field.setContentDescription(labels[i]);
            out[i] = field;

            final LinearLayout.LayoutParams fieldParams =
                    new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT);
            fieldParams.topMargin = dp(2);
            column.addView(field, fieldParams);

            // Equal columns, so the panel stays legible at any viewport width.
            final LinearLayout.LayoutParams columnParams =
                    new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
            if (i > 0) {
                columnParams.leftMargin = dp(8);
            }
            row.addView(column, columnParams);
        }
        return row;
    }

    private View buildUnitAndApplyShapeRow(Context context) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setLayoutParams(sectionParams(dp(6)));

        // One shared unit for every length in the panel: an object is not
        // measured in three different units at once, and neither is its position.
        final RadioGroup group = new RadioGroup(context);
        group.setOrientation(HORIZONTAL);
        final LengthUnit[] units = LengthUnit.values();
        for (int i = 0; i < units.length; i++) {
            final RadioButton button = new RadioButton(context);
            button.setId(unitViewId(i));
            button.setText(units[i].label());
            button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14.0f);
            button.setTextColor(COLOR_TEXT);
            button.setContentDescription("Unit " + units[i].label());
            final RadioGroup.LayoutParams params =
                    new RadioGroup.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT);
            if (i > 0) {
                params.leftMargin = dp(4);
            }
            group.addView(button, params);
        }
        settingSelectionProgrammatically = true;
        group.check(unitViewId(unit.ordinal()));
        settingSelectionProgrammatically = false;
        group.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup radioGroup, int checkedId) {
                if (settingSelectionProgrammatically) {
                    return;
                }
                for (int i = 0; i < units.length; i++) {
                    if (checkedId == unitViewId(i)) {
                        onUnitSelected(units[i]);
                        return;
                    }
                }
            }
        });
        row.addView(group, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        row.addView(applyButton(context, "Apply Shape", new OnClickListener() {
            @Override
            public void onClick(View v) {
                onApplyShape();
            }
        }));
        return row;
    }

    private View buildApplyTransformRow(Context context) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        row.setGravity(Gravity.END | Gravity.CENTER_VERTICAL);
        row.setLayoutParams(sectionParams(dp(6)));
        row.addView(applyButton(context, "Apply Transform", new OnClickListener() {
            @Override
            public void onClick(View v) {
                onApplyTransform();
            }
        }));
        return row;
    }

    /**
     * The mode row.
     *
     * <p>It has a row of its own rather than sharing the placement Apply's,
     * because these two buttons do not edit the object at all — they change
     * which of its two representations is being edited. Putting them beside
     * <i>Apply Transform</i> would both crowd the row and suggest they are the
     * same kind of act.
     */
    private View buildModeRow(Context context) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        row.setGravity(Gravity.START | Gravity.CENTER_VERTICAL);
        row.setLayoutParams(sectionParams(dp(8)));

        row.addView(applyButton(context, "Freeze to Sculpt", new OnClickListener() {
            @Override
            public void onClick(View v) {
                onFreezeToSculpt();
            }
        }));

        // Resuming is a different act from freezing and therefore a different
        // button: Freeze starts from the Construction shape and discards any
        // sculpt work, Resume goes back to the sculpt work exactly as it was.
        // Collapsing the two into one control would make which of those happens
        // depend on hidden state, which is precisely the confusion worth paying
        // a second button to avoid.
        resumeSculptButton = applyButton(context, "Resume Sculpt", new OnClickListener() {
            @Override
            public void onClick(View v) {
                onResumeSculpt();
            }
        });
        final LinearLayout.LayoutParams resumeParams =
                new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT);
        resumeParams.leftMargin = dp(10);
        row.addView(resumeSculptButton, resumeParams);
        return row;
    }

    // -----------------------------------------------------------------------
    // Freeze to Sculpt
    // -----------------------------------------------------------------------

    /**
     * Asks native code to copy the object's current Construction mesh into a
     * Frozen Sculpt Mesh and enter Sculpt Mode.
     *
     * <p>This is an explicit act with a visible consequence, which is exactly
     * why it is a button and not a side effect of anything else. It changes no
     * dimension, no primitive kind and no placement — the Construction Source is
     * only read, and it is still here, unchanged, when Sculpt Mode is left.
     *
     * <p>Freezing again is how a later Construction change is adopted, and it
     * discards the sculpted vertices; nothing does that on the user's behalf.
     */
    private void onFreezeToSculpt() {
        if (NativeViewport.freezeToSculpt() != NativeViewport.SCULPT_OK) {
            setStatus("Could not freeze this shape for sculpting. Object unchanged.", COLOR_ERROR);
            return;
        }
        setStatus("Frozen for sculpting — the Construction " + describeDraftKind().toLowerCase()
                + " is kept.", COLOR_OK);
        finishEditing();
        if (onModeChanged != null) {
            onModeChanged.run();
        }
    }

    /**
     * Returns to the Frozen Sculpt Mesh exactly as it was left, without
     * freezing again — so nothing that has been sculpted is lost by having
     * looked at the Construction Source.
     */
    private void onResumeSculpt() {
        if (NativeViewport.enterSculptMode() != NativeViewport.SCULPT_OK) {
            setStatus("Nothing has been frozen yet — press Freeze to Sculpt first.", COLOR_ERROR);
            return;
        }
        finishEditing();
        if (onModeChanged != null) {
            onModeChanged.run();
        }
    }

    private Button applyButton(Context context, String text, OnClickListener listener) {
        final Button button = new Button(context);
        button.setText(text);
        button.setAllCaps(false);
        button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14.0f);
        button.setTextColor(COLOR_TEXT);
        button.setBackground(applyBackground());
        button.setPadding(dp(18), dp(5), dp(18), dp(5));
        button.setContentDescription(text);
        button.setOnClickListener(listener);
        return button;
    }

    /** Stable, panel-local view ids. */
    private static int unitViewId(int unitOrdinal) {
        return 0x7F550001 + unitOrdinal;
    }

    private static int kindViewId(int kind) {
        return 0x7F550101 + kind;
    }

    // -----------------------------------------------------------------------
    // Touch boundary
    // -----------------------------------------------------------------------

    /**
     * Swallows every touch that lands on the panel and is not taken by one of
     * its controls.
     *
     * <p>Without this, an unclaimed touch inside the panel would fall through to
     * the {@code SurfaceView} underneath and orbit the camera. Touches outside
     * the panel's bounds never reach this method, so the rest of the viewport
     * navigates and picks exactly as before.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }

    // -----------------------------------------------------------------------
    // Draft primitive kind
    // -----------------------------------------------------------------------

    /**
     * Swaps which primitive's fields are on screen.
     *
     * <p>Pure presentation: no native call is made, so the object's kind, its
     * parameters, its transform, the mesh revision and the GPU upload count are
     * all untouched. The object becomes a cylinder only when Apply Shape submits
     * one.
     */
    private void onDraftKindSelected(int kind) {
        if (kind == draftKind) {
            return;
        }
        draftKind = kind;
        applyDraftKindVisibility();
        setStatus(describeDraftKind() + " selected — press Apply Shape to change the object.",
                COLOR_NEUTRAL);
    }

    private void applyDraftKindVisibility() {
        // Exactly one parameter row is on screen at a time, and it is always the
        // one belonging to the drafted kind, so no field on screen can be read
        // as another primitive's parameter.
        boxRow.setVisibility(draftKind == NativeViewport.PRIMITIVE_BOX ? VISIBLE : GONE);
        cylinderRow.setVisibility(draftKind == NativeViewport.PRIMITIVE_CYLINDER ? VISIBLE : GONE);
        sphereRow.setVisibility(draftKind == NativeViewport.PRIMITIVE_SPHERE ? VISIBLE : GONE);
        coneRow.setVisibility(draftKind == NativeViewport.PRIMITIVE_CONE ? VISIBLE : GONE);
        capsuleRow.setVisibility(draftKind == NativeViewport.PRIMITIVE_CAPSULE ? VISIBLE : GONE);
    }

    private String describeDraftKind() {
        switch (draftKind) {
            case NativeViewport.PRIMITIVE_CYLINDER: return "Cylinder";
            case NativeViewport.PRIMITIVE_SPHERE: return "Sphere";
            case NativeViewport.PRIMITIVE_CONE: return "Cone";
            case NativeViewport.PRIMITIVE_CAPSULE: return "Capsule";
            default: return "Box";
        }
    }

    // -----------------------------------------------------------------------
    // Display unit
    // -----------------------------------------------------------------------

    /**
     * Re-expresses every length on screen in another unit.
     *
     * <p>Pure presentation: no native call is made, so the authoritative
     * parameters, the authoritative transform, the mesh revision and the GPU
     * upload count are all untouched. The conversion is an exact decimal point
     * shift, so switching back and forth reproduces the original digits.
     *
     * <p>Both primitives' fields are converted, including the hidden one, so the
     * inactive draft does not silently change meaning while it is off screen.
     * Rotation is deliberately not converted — degrees are degrees in every
     * length unit.
     *
     * <p>A field whose text is not a number is left exactly as the user typed
     * it; silently rewriting or discarding it would be worse than showing it
     * unconverted, and Apply will report it as invalid anyway.
     */
    private void onUnitSelected(LengthUnit next) {
        if (next == unit) {
            return;
        }
        final LengthUnit previous = unit;
        unit = next;
        int converted = 0;
        for (EditText field : boxFields) {
            converted += convertField(field, previous, next);
        }
        for (EditText field : cylinderFields) {
            converted += convertField(field, previous, next);
        }
        for (EditText field : sphereFields) {
            converted += convertField(field, previous, next);
        }
        for (EditText field : coneFields) {
            converted += convertField(field, previous, next);
        }
        for (EditText field : capsuleFields) {
            converted += convertField(field, previous, next);
        }
        for (EditText field : positionFields) {
            converted += convertField(field, previous, next);
        }
        positionSectionLabel.setText(positionSectionTitle());
        if (converted == boxFields.length + cylinderFields.length + sphereFields.length
                + coneFields.length + capsuleFields.length + positionFields.length) {
            setStatus("Showing " + next.label() + " — display only, nothing applied.",
                    COLOR_NEUTRAL);
        } else {
            setStatus("Showing " + next.label() + " — a field is not a number and was left as "
                    + "typed.", COLOR_ERROR);
        }
    }

    /** Returns 1 when the field was converted, 0 when its text was not a number. */
    private int convertField(EditText field, LengthUnit from, LengthUnit to) {
        try {
            final BigDecimal value = LengthUnit.parse(field.getText().toString());
            field.setText(LengthUnit.present(to.convertFrom(from, value)));
            return 1;
        } catch (NumberFormatException notANumber) {
            return 0;
        }
    }

    private String positionSectionTitle() {
        return "Position (" + unit.label() + ")";
    }

    // -----------------------------------------------------------------------
    // Reading native truth
    // -----------------------------------------------------------------------

    /**
     * Rewrites every field, and the primitive selector, from the authoritative
     * native state: lengths in the currently selected display unit, rotation in
     * degrees.
     *
     * <p>This is the only source of the numbers and the kind shown at startup
     * and after a resume; nothing about the object's shape, size or placement is
     * stored or defaulted on the Java side. It also resets the draft kind to
     * what the object actually is, so the selector can never be left claiming a
     * shape the object is not.
     */
    void refreshFromNative() {
        NativeViewport.constructionPrimitive(nativePrimitive);
        final int nativeKind = (int) nativePrimitive[0];
        draftKind = nativeKind >= 0 && nativeKind < kindButtons.length
                ? nativeKind
                : NativeViewport.PRIMITIVE_BOX;
        settingSelectionProgrammatically = true;
        kindButtons[draftKind].setChecked(true);
        settingSelectionProgrammatically = false;
        applyDraftKindVisibility();

        for (int i = 0; i < boxFields.length; i++) {
            boxFields[i].setText(unit.format(nativePrimitive[NativeViewport.PRIMITIVE_BOX_WIDTH + i]));
        }
        cylinderFields[DIAMETER].setText(
                unit.format(nativePrimitive[NativeViewport.PRIMITIVE_CYLINDER_DIAMETER]));
        cylinderFields[AXIAL].setText(
                unit.format(nativePrimitive[NativeViewport.PRIMITIVE_CYLINDER_DIAMETER + 1]));
        sphereFields[DIAMETER].setText(
                unit.format(nativePrimitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER]));
        coneFields[DIAMETER].setText(
                unit.format(nativePrimitive[NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER]));
        coneFields[AXIAL].setText(
                unit.format(nativePrimitive[NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER + 1]));
        capsuleFields[DIAMETER].setText(
                unit.format(nativePrimitive[NativeViewport.PRIMITIVE_CAPSULE_DIAMETER]));
        capsuleFields[AXIAL].setText(
                unit.format(nativePrimitive[NativeViewport.PRIMITIVE_CAPSULE_DIAMETER + 1]));

        NativeViewport.boxTransform(nativeTransform);
        for (int i = 0; i < 3; i++) {
            positionFields[i].setText(unit.format(nativeTransform[i]));
            // Degrees are not a length: they are presented as-is, in every unit.
            rotationFields[i].setText(
                    LengthUnit.present(BigDecimal.valueOf(nativeTransform[3 + i])));
        }

        positionSectionLabel.setText(positionSectionTitle());
        // Resume only exists once there is something to resume. Its visibility
        // is read from native state rather than remembered here, so it cannot
        // offer a sculpt mesh that does not exist.
        NativeViewport.sculptState(nativeSculpt);
        resumeSculptButton.setVisibility(
                nativeSculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0 ? VISIBLE : GONE);
        // The fields no longer hold whatever a previous message complained
        // about, so leaving that message on screen would describe text that is
        // not there any more. Callers that have something to say overwrite this.
        setStatus("Showing the current " + describeDraftKind().toLowerCase() + " in "
                + unit.label() + ".", COLOR_NEUTRAL);
    }

    // -----------------------------------------------------------------------
    // Apply Shape
    // -----------------------------------------------------------------------

    /**
     * Parses the selected primitive's parameters, converts them to meters and
     * submits them together with the kind as one atomic native request.
     *
     * <p>Nothing is submitted unless every relevant field parses and is
     * positive: a request native code would refuse for a reason the UI already
     * knows about is not worth making, and a partial submission does not exist.
     * Native validation remains the final authority — invalid values are never
     * clamped or repaired here.
     *
     * <p>The transform is not part of this request and is never disturbed by it.
     */
    private void onApplyShape() {
        final Integer result = submitDraftShape();
        if (result == null) {
            return;  // a field was reported; nothing was submitted
        }
        switch (result) {
            case NativeViewport.APPLY_APPLIED:
                refreshFromNative();
                setStatus("Shape applied — " + describeShape(), COLOR_OK);
                finishEditing();
                break;
            case NativeViewport.APPLY_UNCHANGED:
                refreshFromNative();
                setStatus("Shape unchanged — already " + describeShape(), COLOR_NEUTRAL);
                finishEditing();
                break;
            case NativeViewport.APPLY_REJECTED_NOT_POSITIVE:
                setStatus("Rejected: every dimension must be greater than 0. Object unchanged.",
                        COLOR_ERROR);
                break;
            case NativeViewport.APPLY_REJECTED_NOT_FINITE:
                setStatus("Rejected: a value is not a finite length. Object unchanged.",
                        COLOR_ERROR);
                break;
            case NativeViewport.APPLY_REJECTED_NOT_REPRESENTABLE:
                setStatus("Rejected: a value is too large or too small to build. Object unchanged.",
                        COLOR_ERROR);
                break;
            case NativeViewport.APPLY_REJECTED_RELATION:
                // Today the only relation in the product is the capsule's, and
                // naming it is far more useful than "invalid dimensions".
                setStatus("Rejected: Total Height cannot be less than Diameter — the two "
                        + "rounded ends alone are that tall. Object unchanged.", COLOR_ERROR);
                break;
            default:
                setStatus("Rejected: ForgeShape could not build that shape. Object unchanged.",
                        COLOR_ERROR);
                break;
        }
    }

    /**
     * Reads the drafted primitive's own fields and submits them through that
     * primitive's own native method.
     *
     * <p>Each branch reads exactly the fields that primitive has and calls the
     * method that takes exactly those parameters, so there is no point at which
     * a value is carried in a slot whose meaning depends on a separate kind.
     *
     * @return the {@code APPLY_*} status, or {@code null} when a field was
     *         reported and nothing was submitted
     */
    private Integer submitDraftShape() {
        if (draftKind == NativeViewport.PRIMITIVE_SPHERE) {
            final BigDecimal diameter =
                    readField(sphereFields[DIAMETER], SPHERE_LABELS[DIAMETER], true);
            if (diameter == null) {
                return null;
            }
            return NativeViewport.applyConstructionSphere(unit.toMeters(diameter).doubleValue());
        }
        if (draftKind == NativeViewport.PRIMITIVE_CYLINDER) {
            final BigDecimal[] values = readAxialPair(cylinderFields, CYLINDER_LABELS, "Cylinder");
            if (values == null) {
                return null;
            }
            return NativeViewport.applyConstructionCylinder(
                    unit.toMeters(values[DIAMETER]).doubleValue(),
                    unit.toMeters(values[AXIAL]).doubleValue());
        }
        if (draftKind == NativeViewport.PRIMITIVE_CONE) {
            final BigDecimal[] values = readAxialPair(coneFields, CONE_LABELS, "Cone");
            if (values == null) {
                return null;
            }
            return NativeViewport.applyConstructionCone(
                    unit.toMeters(values[DIAMETER]).doubleValue(),
                    unit.toMeters(values[AXIAL]).doubleValue());
        }
        if (draftKind == NativeViewport.PRIMITIVE_CAPSULE) {
            final BigDecimal[] values = readAxialPair(capsuleFields, CAPSULE_LABELS, "Capsule");
            if (values == null) {
                return null;
            }
            // The relation totalHeight >= diameter is deliberately NOT checked
            // here. Native validation owns it, exactly as it owns positivity for
            // the values it can already name a problem with: this panel refuses
            // only what it can describe from the text alone.
            return NativeViewport.applyConstructionCapsule(
                    unit.toMeters(values[DIAMETER]).doubleValue(),
                    unit.toMeters(values[AXIAL]).doubleValue());
        }
        final BigDecimal[] displayed = new BigDecimal[3];
        for (int i = 0; i < 3; i++) {
            displayed[i] = readField(boxFields[i], BOX_LABELS[i], true);
            if (displayed[i] == null) {
                return null;
            }
        }
        return NativeViewport.applyConstructionBox(unit.toMeters(displayed[X]).doubleValue(),
                unit.toMeters(displayed[Y]).doubleValue(),
                unit.toMeters(displayed[Z]).doubleValue());
    }

    /**
     * Reads a diameter-plus-axial-length pair from one primitive's own row.
     *
     * <p>Shared by the cylinder, the cone and the capsule because all three
     * present exactly that pair, in exactly those two slots. It reads fields and
     * reports on them; it decides nothing about the shape, and the caller still
     * hands the values to that primitive's own native method, so the typed
     * boundary is untouched.
     *
     * @return the two values, or {@code null} when a field was reported
     */
    private BigDecimal[] readAxialPair(EditText[] fields, String[] labels, String primitive) {
        final BigDecimal diameter = readField(fields[DIAMETER], labels[DIAMETER], true);
        if (diameter == null) {
            return null;
        }
        final BigDecimal axial =
                readField(fields[AXIAL], primitive + " " + labels[AXIAL], true);
        if (axial == null) {
            return null;
        }
        final BigDecimal[] values = new BigDecimal[2];
        values[DIAMETER] = diameter;
        values[AXIAL] = axial;
        return values;
    }

    // -----------------------------------------------------------------------
    // Apply Transform
    // -----------------------------------------------------------------------

    /**
     * Parses all six placement values and submits them as one atomic native
     * request: position converted from the display unit to meters, rotation
     * passed through as degrees.
     *
     * <p>Zero and negative are valid for all six — a coordinate is a place and
     * an angle is a direction, neither is a size — so the only thing refused
     * here is text that is not a number at all.
     *
     * <p>The shape is not part of this request and is never disturbed by it.
     */
    private void onApplyTransform() {
        final BigDecimal[] position = new BigDecimal[3];
        final BigDecimal[] rotation = new BigDecimal[3];
        for (int i = 0; i < 3; i++) {
            position[i] = readField(positionFields[i], POSITION_LABELS[i], false);
            if (position[i] == null) {
                return;
            }
        }
        for (int i = 0; i < 3; i++) {
            rotation[i] = readField(rotationFields[i], ROTATION_LABELS[i], false);
            if (rotation[i] == null) {
                return;
            }
        }

        switch (NativeViewport.applyBoxTransform(
                unit.toMeters(position[X]).doubleValue(),
                unit.toMeters(position[Y]).doubleValue(),
                unit.toMeters(position[Z]).doubleValue(),
                rotation[X].doubleValue(), rotation[Y].doubleValue(), rotation[Z].doubleValue())) {
            case NativeViewport.APPLY_APPLIED:
                refreshFromNative();
                setStatus("Transform applied — " + describeTransform(), COLOR_OK);
                finishEditing();
                break;
            case NativeViewport.APPLY_UNCHANGED:
                refreshFromNative();
                setStatus("Transform unchanged — already " + describeTransform(), COLOR_NEUTRAL);
                finishEditing();
                break;
            case NativeViewport.APPLY_REJECTED_NOT_REPRESENTABLE:
                setStatus("Rejected: a value is too large to place. Transform unchanged.",
                        COLOR_ERROR);
                break;
            default:
                setStatus("Rejected: a value is not a finite number. Transform unchanged.",
                        COLOR_ERROR);
                break;
        }
    }

    // -----------------------------------------------------------------------
    // Field reading and reporting
    // -----------------------------------------------------------------------

    /**
     * Parses one field, reporting and focusing it on failure.
     *
     * @param positive whether the value must be greater than zero. True for a
     *                 dimension, false for a coordinate or an angle.
     * @return the parsed value, or {@code null} when the field was reported
     */
    private BigDecimal readField(EditText field, String name, boolean positive) {
        final String raw = field.getText().toString();
        final BigDecimal value;
        try {
            value = LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            fail(field, raw.trim().isEmpty()
                    ? name + " is empty — enter a number."
                    : name + " is not a number: \"" + raw.trim() + "\".");
            return null;
        }
        if (positive && value.signum() <= 0) {
            fail(field, name + " must be greater than 0 " + unit.label() + ".");
            return null;
        }
        return value;
    }

    /** Describes the authoritative shape in the selected display unit. */
    private String describeShape() {
        NativeViewport.constructionPrimitive(nativePrimitive);
        final int kind = (int) nativePrimitive[0];
        if (kind == NativeViewport.PRIMITIVE_SPHERE) {
            return "sphere " + unit.format(nativePrimitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER])
                    + " " + unit.label() + " dia";
        }
        if (kind == NativeViewport.PRIMITIVE_CYLINDER) {
            return describeAxialPair("cylinder", NativeViewport.PRIMITIVE_CYLINDER_DIAMETER, "");
        }
        if (kind == NativeViewport.PRIMITIVE_CONE) {
            return describeAxialPair("cone", NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER, "");
        }
        if (kind == NativeViewport.PRIMITIVE_CAPSULE) {
            return describeAxialPair("capsule", NativeViewport.PRIMITIVE_CAPSULE_DIAMETER,
                    " total");
        }
        return "box " + unit.format(nativePrimitive[NativeViewport.PRIMITIVE_BOX_WIDTH]) + " x "
                + unit.format(nativePrimitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 1]) + " x "
                + unit.format(nativePrimitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 2]) + " "
                + unit.label();
    }

    /**
     * Describes a diameter-plus-axial-length primitive from the array native
     * code just filled. {@code axialSuffix} is what makes a capsule's height
     * read as its <i>total</i> height rather than its middle.
     */
    private String describeAxialPair(String name, int diameterSlot, String axialSuffix) {
        return name + " " + unit.format(nativePrimitive[diameterSlot]) + " dia x "
                + unit.format(nativePrimitive[diameterSlot + 1]) + " " + unit.label()
                + axialSuffix;
    }

    /** Describes the authoritative transform in the selected display unit. */
    private String describeTransform() {
        NativeViewport.boxTransform(nativeTransform);
        return unit.format(nativeTransform[0]) + ", " + unit.format(nativeTransform[1]) + ", "
                + unit.format(nativeTransform[2]) + " " + unit.label() + " @ "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[3])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[4])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[5])) + " deg";
    }

    private void fail(EditText field, String message) {
        setStatus(message, COLOR_ERROR);
        field.requestFocus();
        field.selectAll();
    }

    private void setStatus(String message, int color) {
        statusLine.setTextColor(color);
        statusLine.setText(message);
    }

    /**
     * Hands focus and the keyboard back to the viewport once an edit has landed,
     * so the next touch navigates instead of typing.
     */
    private void finishEditing() {
        clearFocusIn(boxFields);
        clearFocusIn(cylinderFields);
        clearFocusIn(sphereFields);
        clearFocusIn(coneFields);
        clearFocusIn(capsuleFields);
        clearFocusIn(positionFields);
        clearFocusIn(rotationFields);
        final InputMethodManager ime =
                (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.hideSoftInputFromWindow(getWindowToken(), 0);
        }
        if (viewportView != null) {
            viewportView.requestFocus();
        }
    }

    private void clearFocusIn(EditText[] fields) {
        for (EditText field : fields) {
            field.clearFocus();
        }
    }

    // -----------------------------------------------------------------------
    // Styling helpers
    // -----------------------------------------------------------------------

    private int dp(int value) {
        return Math.round(getResources().getDisplayMetrics().density * value);
    }

    private GradientDrawable panelBackground() {
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(COLOR_PANEL);
        shape.setStroke(dp(1), 0xFF2C3644);
        return shape;
    }

    private GradientDrawable fieldBackground() {
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(COLOR_FIELD);
        shape.setCornerRadius(dp(6));
        shape.setStroke(dp(1), 0xFF3A4757);
        return shape;
    }

    private GradientDrawable applyBackground() {
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(COLOR_ACCENT);
        shape.setCornerRadius(dp(6));
        shape.setStroke(dp(1), Color.WHITE & 0x40FFFFFF);
        return shape;
    }
}
