package com.forgeshape.app;

import android.content.Context;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The canvas editor of one hole (`CAD-HOLE-R1`): the kind (Simple,
 * Counterbore, Countersink), the diameter, Through All or a typed depth, the
 * counterbore's or the cone's own two values, and where on the face it stands
 * — then Apply, Delete (an existing hole) and Cancel.
 *
 * <p>It holds no truth. What it shows comes from native (the seeds of a new
 * hole, the values of a committed one), and Apply hands every value to native,
 * which judges the whole body and makes the act ONE Undo. Rows a kind does not
 * use are ABSENT rather than disabled, and the depth field is absent while
 * Through All is chosen, because there is no depth to type then.
 */
final class HoleEditorPanel extends LinearLayout {

    /** What the panel asks of its host. */
    interface Listener {
        /** The display unit lengths are typed in. */
        LengthUnit holeUnit();

        /** Reports a field that does not parse, as a verdict. */
        void holeFieldRefused(String message);

        /** Applies the request; true closes the panel. */
        boolean holeApply(Request request);

        /** Deletes the hole being edited; true closes the panel. */
        boolean holeDelete();

        /** The panel closed without applying anything. */
        void holeCancelled();

        /**
         * Begins placing from two edges of the face, the hole standing at
         * (u, v) now (`CAD-TO-FACE-R1`): the two distances, or null when
         * native refused (the host has said why).
         */
        double[] holeEdgeSeed(double u, double v);

        /** Ends placing from edges. */
        void holeEdgesReleased();
    }

    /** Every value one Apply sends, in native's units (metres, degrees). */
    static final class Request {
        int kind;
        double diameter;
        boolean throughAll;
        double depth;
        double boreDiameter;
        double boreDepth;
        double sinkDiameter;
        double sinkAngle;
        double u;
        double v;
        /** Placed from two edges: {@link #edgeA} and {@link #edgeB} in, (u, v) unused. */
        boolean byEdges;
        double edgeA;
        double edgeB;
    }

    private final Listener listener;
    private final TextView[] kindChips = new TextView[3];
    private final EditText diameterField;
    private final TextView throughChip;
    private final TextView blindChip;
    private final EditText depthField;
    private final LinearLayout boreRow;
    private final EditText boreDiameterField;
    private final EditText boreDepthField;
    private final LinearLayout sinkRow;
    private final EditText sinkDiameterField;
    private final EditText sinkAngleField;
    private final EditText uField;
    private final EditText vField;
    private final LinearLayout positionRow;
    private final LinearLayout positionRow2;
    private final TextView pointChip;
    private final TextView edgesChip;
    private final LinearLayout edgeRowA;
    private final LinearLayout edgeRowB;
    private final EditText edgeAField;
    private final EditText edgeBField;
    private final TextView deleteButton;

    private int kind = NativeViewport.HOLE_SIMPLE;
    private boolean throughAll = true;
    private boolean editing;
    private boolean byEdges;

    HoleEditorPanel(Context context, Listener listener) {
        super(context);
        this.listener = listener;
        setId(R.id.cad_canvas_hole_editor);
        setOrientation(VERTICAL);
        EditorControlStyles.applyContextSurface(this);
        final int pad = EditorControlStyles.dimen(context, R.dimen.cad_hud_editor_padding);
        setPadding(pad, pad, pad, pad);
        setClickable(true);
        setVisibility(GONE);
        final int gap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);

        final LinearLayout kindRow = row(context, 0);
        final int[] kindIds = {R.id.hole_kind_simple, R.id.hole_kind_counterbore,
                R.id.hole_kind_countersink};
        for (int k = 0; k < 3; k++) {
            final int chosen = k;
            kindChips[k] = EditorControlStyles.chip(context, kindIds[k],
                    context.getString(HolePresentation.kindName(k)));
            kindChips[k].setContentDescription(
                    context.getString(HolePresentation.kindDescription(k)));
            kindChips[k].setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    chooseKind(chosen);
                }
            });
            kindRow.addView(kindChips[k], EditorControlStyles.wrap(k == 0 ? 0 : gap));
        }

        final LinearLayout sizeRow = row(context, gap);
        sizeRow.addView(label(context, R.string.hole_label_diameter), EditorControlStyles.wrap(0));
        diameterField = field(context, R.id.field_hole_diameter, false);
        sizeRow.addView(diameterField, EditorControlStyles.wrap(gap));

        final LinearLayout depthRow = row(context, gap);
        throughChip = EditorControlStyles.chip(context, R.id.hole_through_all,
                context.getString(R.string.hole_through_all));
        throughChip.setContentDescription(context.getString(R.string.hole_through_all_description));
        throughChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                chooseThroughAll(true);
            }
        });
        depthRow.addView(throughChip, EditorControlStyles.wrap(0));
        blindChip = EditorControlStyles.chip(context, R.id.hole_blind,
                context.getString(R.string.hole_blind));
        blindChip.setContentDescription(context.getString(R.string.hole_blind_description));
        blindChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                chooseThroughAll(false);
            }
        });
        depthRow.addView(blindChip, EditorControlStyles.wrap(gap));
        depthField = field(context, R.id.field_hole_depth, false);
        depthRow.addView(depthField, EditorControlStyles.wrap(gap));

        boreRow = row(context, gap);
        boreRow.addView(label(context, R.string.hole_label_bore_diameter), EditorControlStyles.wrap(0));
        boreDiameterField = field(context, R.id.field_hole_bore_diameter, false);
        boreRow.addView(boreDiameterField, EditorControlStyles.wrap(gap));
        boreRow.addView(label(context, R.string.hole_label_bore_depth),
                EditorControlStyles.wrap(gap * 2));
        boreDepthField = field(context, R.id.field_hole_bore_depth, false);
        boreRow.addView(boreDepthField, EditorControlStyles.wrap(gap));

        sinkRow = row(context, gap);
        sinkRow.addView(label(context, R.string.hole_label_sink_diameter), EditorControlStyles.wrap(0));
        sinkDiameterField = field(context, R.id.field_hole_sink_diameter, false);
        sinkRow.addView(sinkDiameterField, EditorControlStyles.wrap(gap));
        sinkRow.addView(label(context, R.string.hole_label_sink_angle),
                EditorControlStyles.wrap(gap * 2));
        sinkAngleField = field(context, R.id.field_hole_sink_angle, false);
        sinkRow.addView(sinkAngleField, EditorControlStyles.wrap(gap));

        // Where on the face: two signed numbers in the face's own frame, so a
        // tap's position can be made exact.
        // How it is placed: at a point of the face, or by its distances from
        // two of the face's edges, which then carry it when the part changes.
        final LinearLayout placeRow = row(context, gap);
        pointChip = EditorControlStyles.chip(context, R.id.hole_place_point,
                context.getString(R.string.hole_place_point));
        pointChip.setContentDescription(context.getString(R.string.hole_place_point_description));
        pointChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                choosePlacement(false);
            }
        });
        placeRow.addView(pointChip, EditorControlStyles.wrap(0));
        edgesChip = EditorControlStyles.chip(context, R.id.hole_place_edges,
                context.getString(R.string.hole_place_edges));
        edgesChip.setContentDescription(context.getString(R.string.hole_place_edges_description));
        edgesChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                choosePlacement(true);
            }
        });
        placeRow.addView(edgesChip, EditorControlStyles.wrap(gap));

        positionRow = row(context, gap);
        positionRow.addView(label(context, R.string.hole_label_position), EditorControlStyles.wrap(0));
        positionRow.addView(label(context, R.string.hole_label_u), EditorControlStyles.wrap(gap));
        uField = field(context, R.id.field_hole_u, true);
        positionRow.addView(uField, EditorControlStyles.wrap(gap));
        // The second coordinate on its own row, so the docked editor stays
        // narrow beside the trailing controls (`CAD-UI-DOCK-R1`).
        positionRow2 = row(context, gap);
        positionRow2.addView(label(context, R.string.hole_label_v), EditorControlStyles.wrap(0));
        vField = field(context, R.id.field_hole_v, true);
        positionRow2.addView(vField, EditorControlStyles.wrap(gap));

        // From edges: the two highlighted edges, one row each.
        edgeRowA = row(context, gap);
        edgeRowA.addView(label(context, R.string.hole_label_edge_a), EditorControlStyles.wrap(0));
        edgeAField = field(context, R.id.field_hole_edge_a, false);
        edgeRowA.addView(edgeAField, EditorControlStyles.wrap(gap));
        edgeRowB = row(context, gap);
        edgeRowB.addView(label(context, R.string.hole_label_edge_b), EditorControlStyles.wrap(0));
        edgeBField = field(context, R.id.field_hole_edge_b, false);
        edgeRowB.addView(edgeBField, EditorControlStyles.wrap(gap));

        final LinearLayout actions = row(context, gap);
        final TextView apply = EditorControlStyles.primaryButton(context, R.id.apply_hole,
                context.getString(R.string.apply));
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                submit();
            }
        });
        actions.addView(apply, EditorControlStyles.wrap(0));
        deleteButton = EditorControlStyles.actionChip(context, R.id.delete_hole,
                context.getString(R.string.hole_delete));
        deleteButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (HoleEditorPanel.this.listener.holeDelete()) {
                    close();
                }
            }
        });
        actions.addView(deleteButton, EditorControlStyles.wrap(gap));
        final TextView cancel = EditorControlStyles.actionChip(context, R.id.cancel_hole,
                context.getString(R.string.hole_cancel));
        cancel.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                close();
                HoleEditorPanel.this.listener.holeCancelled();
            }
        });
        actions.addView(cancel, EditorControlStyles.wrap(gap));

        addView(kindRow);
        addView(sizeRow);
        addView(depthRow);
        addView(boreRow);
        addView(sinkRow);
        addView(placeRow);
        addView(positionRow);
        addView(positionRow2);
        addView(edgeRowA);
        addView(edgeRowB);
        addView(actions);
    }

    /** Opens the panel for a NEW Simple Through All hole, seeded by native. */
    void openNew(double diameter, double u, double v) {
        editing = false;
        kind = NativeViewport.HOLE_SIMPLE;
        throughAll = true;
        writeDefaults(diameter);
        final LengthUnit unit = listener.holeUnit();
        uField.setText(unit.format(u));
        vField.setText(unit.format(v));
        byEdges = false;
        deleteButton.setVisibility(GONE);
        refresh();
        setVisibility(VISIBLE);
    }

    /** Opens the panel over a COMMITTED hole's values; Apply edits it. */
    void openExisting(double[] info) {
        editing = true;
        kind = (int) info[NativeViewport.HOLE_INFO_KIND];
        throughAll = info[NativeViewport.HOLE_INFO_THROUGH_ALL] != 0.0;
        final double diameter = info[NativeViewport.HOLE_INFO_DIAMETER];
        writeDefaults(diameter);
        final LengthUnit unit = listener.holeUnit();
        if (!throughAll) {
            depthField.setText(unit.format(info[NativeViewport.HOLE_INFO_DEPTH]));
        }
        if (kind == NativeViewport.HOLE_COUNTERBORE) {
            boreDiameterField.setText(unit.format(info[NativeViewport.HOLE_INFO_BORE_DIAMETER]));
            boreDepthField.setText(unit.format(info[NativeViewport.HOLE_INFO_BORE_DEPTH]));
        } else if (kind == NativeViewport.HOLE_COUNTERSINK) {
            sinkDiameterField.setText(unit.format(info[NativeViewport.HOLE_INFO_SINK_DIAMETER]));
            sinkAngleField.setText(CadRevolvePresentation.formatDegrees(
                    info[NativeViewport.HOLE_INFO_SINK_ANGLE]));
        }
        uField.setText(unit.format(info[NativeViewport.HOLE_INFO_U]));
        vField.setText(unit.format(info[NativeViewport.HOLE_INFO_V]));
        byEdges = false;
        deleteButton.setVisibility(VISIBLE);
        refresh();
        setVisibility(VISIBLE);
        // A hole placed from edges reopens on them, highlighted.
        if (info.length > NativeViewport.HOLE_INFO_BY_EDGES
                && info[NativeViewport.HOLE_INFO_BY_EDGES] != 0.0) {
            choosePlacement(true);
        }
    }

    boolean isOpen() {
        return getVisibility() == VISIBLE;
    }

    boolean isEditing() {
        return editing;
    }

    void close() {
        final boolean wasOpen = isOpen();
        setVisibility(GONE);
        for (EditText field : new EditText[] {diameterField, depthField, boreDiameterField,
                boreDepthField, sinkDiameterField, sinkAngleField, uField, vField, edgeAField,
                edgeBField}) {
            field.clearFocus();
        }
        if (byEdges || wasOpen) {
            byEdges = false;
            listener.holeEdgesReleased();
        }
    }

    /** Whether the hole is being placed from two edges. */
    boolean placedFromEdges() {
        return byEdges;
    }

    /**
     * At a point, or from the two nearest edges at the distances it stands
     * from them now, so switching moves nothing; a refusal keeps the point.
     */
    private void choosePlacement(boolean edges) {
        if (edges == byEdges) {
            refresh();
            return;
        }
        if (!edges) {
            byEdges = false;
            listener.holeEdgesReleased();
            refresh();
            return;
        }
        final Double u = parse(uField, true);
        final Double v = parse(vField, true);
        if (u == null || v == null) {
            refuse(u == null ? R.string.hole_label_u : R.string.hole_label_v,
                    u == null ? uField : vField);
            return;
        }
        final double[] seeded = listener.holeEdgeSeed(u, v);
        if (seeded == null) {
            return;
        }
        final LengthUnit unit = listener.holeUnit();
        edgeAField.setText(unit.format(seeded[0]));
        edgeBField.setText(unit.format(seeded[1]));
        byEdges = true;
        refresh();
    }

    // -------------------------------------------------------------------------

    /** Every size field at the value a hole of {@code diameter} opens with. */
    private void writeDefaults(double diameter) {
        final LengthUnit unit = listener.holeUnit();
        diameterField.setText(unit.format(diameter));
        depthField.setText(unit.format(HolePresentation.defaultDepth(diameter)));
        boreDiameterField.setText(unit.format(HolePresentation.defaultBoreDiameter(diameter)));
        boreDepthField.setText(unit.format(HolePresentation.defaultBoreDepth(diameter)));
        sinkDiameterField.setText(unit.format(HolePresentation.defaultSinkDiameter(diameter)));
        sinkAngleField.setText(CadRevolvePresentation.formatDegrees(
                HolePresentation.DEFAULT_SINK_ANGLE));
    }

    private void chooseKind(int chosen) {
        kind = chosen;
        refresh();
    }

    private void chooseThroughAll(boolean through) {
        throughAll = through;
        refresh();
    }

    private void refresh() {
        final Context context = getContext();
        for (int k = 0; k < 3; k++) {
            EditorControlStyles.setChipActive(kindChips[k], k == kind);
        }
        EditorControlStyles.setChipActive(throughChip, throughAll);
        EditorControlStyles.setChipActive(blindChip, !throughAll);
        depthField.setVisibility(throughAll ? GONE : VISIBLE);
        EditorControlStyles.setChipActive(pointChip, !byEdges);
        EditorControlStyles.setChipActive(edgesChip, byEdges);
        positionRow.setVisibility(byEdges ? GONE : VISIBLE);
        positionRow2.setVisibility(byEdges ? GONE : VISIBLE);
        edgeRowA.setVisibility(byEdges ? VISIBLE : GONE);
        edgeRowB.setVisibility(byEdges ? VISIBLE : GONE);
        boreRow.setVisibility(HolePresentation.boreShown(kind) ? VISIBLE : GONE);
        sinkRow.setVisibility(HolePresentation.sinkShown(kind) ? VISIBLE : GONE);
        final String unit = listener.holeUnit().label();
        describe(diameterField, R.string.hole_label_diameter, unit);
        describe(depthField, R.string.hole_label_depth, unit);
        describe(boreDiameterField, R.string.hole_label_bore_diameter, unit);
        describe(boreDepthField, R.string.hole_label_bore_depth, unit);
        describe(sinkDiameterField, R.string.hole_label_sink_diameter, unit);
        describe(sinkAngleField, R.string.hole_label_sink_angle,
                context.getString(R.string.hole_degrees));
        describe(uField, R.string.hole_label_u, unit);
        describe(vField, R.string.hole_label_v, unit);
        describe(edgeAField, R.string.hole_label_edge_a, unit);
        describe(edgeBField, R.string.hole_label_edge_b, unit);
    }

    private void describe(EditText field, int labelRes, String unit) {
        final Context context = getContext();
        field.setContentDescription(context.getString(R.string.hole_field_description,
                context.getString(labelRes), unit));
    }

    private void submit() {
        final Request request = new Request();
        request.kind = kind;
        request.throughAll = throughAll;
        final Double diameter = parse(diameterField, true);
        if (diameter == null) {
            refuse(R.string.hole_label_diameter, diameterField);
            return;
        }
        request.diameter = diameter;
        if (!throughAll) {
            final Double depth = parse(depthField, true);
            if (depth == null) {
                refuse(R.string.hole_label_depth, depthField);
                return;
            }
            request.depth = depth;
        }
        if (HolePresentation.boreShown(kind)) {
            final Double bore = parse(boreDiameterField, true);
            if (bore == null) {
                refuse(R.string.hole_label_bore_diameter, boreDiameterField);
                return;
            }
            final Double boreDepth = parse(boreDepthField, true);
            if (boreDepth == null) {
                refuse(R.string.hole_label_bore_depth, boreDepthField);
                return;
            }
            request.boreDiameter = bore;
            request.boreDepth = boreDepth;
        }
        if (HolePresentation.sinkShown(kind)) {
            final Double sink = parse(sinkDiameterField, true);
            if (sink == null) {
                refuse(R.string.hole_label_sink_diameter, sinkDiameterField);
                return;
            }
            final Double angle = parse(sinkAngleField, false);
            if (angle == null) {
                refuse(R.string.hole_label_sink_angle, sinkAngleField);
                return;
            }
            request.sinkDiameter = sink;
            request.sinkAngle = angle;
        }
        if (byEdges) {
            final Double a = parse(edgeAField, true);
            if (a == null) {
                refuse(R.string.hole_label_edge_a, edgeAField);
                return;
            }
            final Double b = parse(edgeBField, true);
            if (b == null) {
                refuse(R.string.hole_label_edge_b, edgeBField);
                return;
            }
            request.byEdges = true;
            request.edgeA = a;
            request.edgeB = b;
        } else {
            final Double u = parse(uField, true);
            if (u == null) {
                refuse(R.string.hole_label_u, uField);
                return;
            }
            final Double v = parse(vField, true);
            if (v == null) {
                refuse(R.string.hole_label_v, vField);
                return;
            }
            request.u = u;
            request.v = v;
        }
        if (listener.holeApply(request)) {
            close();
        }
    }

    private Double parse(EditText field, boolean length) {
        try {
            final BigDecimal typed = LengthUnit.parse(field.getText().toString());
            return length ? listener.holeUnit().toMeters(typed).doubleValue() : typed.doubleValue();
        } catch (NumberFormatException notANumber) {
            return null;
        }
    }

    private void refuse(int labelRes, EditText field) {
        final Context context = getContext();
        final String raw = field.getText().toString().trim();
        listener.holeFieldRefused(raw.isEmpty()
                ? context.getString(R.string.field_empty, context.getString(labelRes))
                : context.getString(R.string.field_not_a_number, context.getString(labelRes), raw));
        field.requestFocus();
    }

    private static LinearLayout row(Context context, int topMargin) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        final LayoutParams params = new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = topMargin;
        row.setLayoutParams(params);
        return row;
    }

    private static TextView label(Context context, int textRes) {
        final TextView label = new TextView(context);
        label.setSingleLine(true);
        label.setText(textRes);
        label.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextSecondary));
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_label));
        EditorControlStyles.applyMediumWeight(label);
        return label;
    }

    private static EditText field(Context context, int id, boolean signed) {
        final EditText made = new EditText(context);
        made.setId(id);
        made.setSingleLine(true);
        made.setBackgroundResource(R.drawable.bg_field);
        final int padH = EditorControlStyles.dimen(context, R.dimen.field_padding_horizontal);
        final int padV = EditorControlStyles.dimen(context, R.dimen.field_padding_vertical);
        made.setPadding(padH, padV, padH, padV);
        int type = InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL;
        if (signed) {
            type |= InputType.TYPE_NUMBER_FLAG_SIGNED;
        }
        made.setInputType(type);
        made.setKeyListener(DigitsKeyListener.getInstance(signed ? "0123456789.-" : "0123456789."));
        made.setImeOptions(EditorInfo.IME_ACTION_DONE);
        made.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        made.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        made.setMinimumWidth(EditorControlStyles.dimen(context, R.dimen.cad_canvas_field));
        made.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
        return made;
    }
}
