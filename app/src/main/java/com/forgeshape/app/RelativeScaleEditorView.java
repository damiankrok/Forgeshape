package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The exact-value editor for <b>Relative Scale</b> (Stage 020M,
 * {@code UI-OWNER-33B}).
 *
 * <p>Three multipliers and one Apply. The Apply commits
 * {@code newAbsolute = oldAbsolute × multiplier} per axis into the body's
 * stored Absolute Scale, as exactly one Construction history transaction.
 *
 * <p><b>The multiplier is temporary, and this view is the only thing that ever
 * holds one.</b> It is not project truth, not a second scale vector, not a
 * `.forge` field and not a history value: nothing below JNI stores it, which is
 * why {@link #resetToIdentity()} — called every time the surface opens — has
 * nothing to undo and simply writes 1 into all three fields. Re-opening after a
 * commit therefore starts at 1/1/1 by construction rather than by a rule
 * somebody has to remember to apply.
 *
 * <p>It is a separate editor from the placement editor, with its own Apply, for
 * the same reason the placement editor is separate from the shape editor: the
 * two commits mean different things. The placement editor states the absolute
 * scale a body HAS; this one states how much to change it BY, and merging them
 * would make one field mean two things depending on which button was pressed.
 *
 * <p><b>Presentation and input only.</b> It holds field text and no scale; the
 * authoritative Absolute Scale is a unitless double and it is native's. A
 * multiplier is unitless too, so nothing here offers a display unit — the same
 * rule that keeps the unit chips off the Scale row next door.
 */
final class RelativeScaleEditorView extends LinearLayout
        implements PropertyInspectorView.PinnedCommit {

    private static final int[] FIELD_IDS = {R.id.field_relative_scale_x,
            R.id.field_relative_scale_y, R.id.field_relative_scale_z};
    private static final int[] FIELD_LABELS = {R.string.label_relative_scale_x,
            R.string.label_relative_scale_y, R.string.label_relative_scale_z};

    /** What every field opens at, every time. Native states the same constant. */
    private static final String IDENTITY = "1";

    private final InspectorHost host;
    private final NumericPropertyRow[] fields = new NumericPropertyRow[3];
    private final TextView apply;

    /** Reused across reads; native fills the BODY_DIM_* slots. */
    private final double[] nativeState = new double[NativeViewport.BODY_DIM_SIZE];

    RelativeScaleEditorView(Context context, InspectorHost host) {
        super(context);
        this.host = host;
        setId(R.id.relative_scale_editor);
        setOrientation(VERTICAL);

        final int smallGap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        addView(EditorControlStyles.sectionLabel(context,
                        context.getString(R.string.section_relative_scale)),
                EditorControlStyles.rowParams(0));

        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        for (int i = 0; i < FIELD_IDS.length; i++) {
            fields[i] = new NumericPropertyRow(context, FIELD_IDS[i],
                    context.getString(FIELD_LABELS[i]), i == FIELD_IDS.length - 1);
            row.addView(fields[i], EditorControlStyles.evenShare(i == 0 ? 0 : gap));
        }
        addView(row, EditorControlStyles.rowParams(smallGap));

        // The reset rule stated where the user reads it, not only in the code:
        // a temporary multiplier that silently persisted would be the one thing
        // about this surface that could surprise somebody.
        final TextView note = EditorControlStyles.captionText(context, View.NO_ID,
                context.getString(R.string.relative_scale_note));
        addView(note, EditorControlStyles.rowParams(smallGap));

        apply = EditorControlStyles.primaryButton(context, R.id.apply_relative_scale,
                context.getString(R.string.apply_relative_scale));
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onApply();
            }
        });

        resetToIdentity();
    }

    /** The row that owns a field id, or null. For verification. */
    NumericPropertyRow rowFor(int fieldId) {
        for (NumericPropertyRow field : fields) {
            if (field.field().getId() == fieldId) {
                return field;
            }
        }
        return null;
    }

    @Override
    public View commitControl() {
        return apply;
    }

    /**
     * Writes 1 into all three fields.
     *
     * <p>Called every time the surface is opened. There is nothing to read from
     * native here and there never will be: a Relative Scale multiplier is not
     * stored anywhere, so the identity is the only honest starting value.
     */
    void resetToIdentity() {
        for (NumericPropertyRow field : fields) {
            field.setText(IDENTITY);
        }
    }

    // There is deliberately no `refreshFromNative()` here, unlike every other
    // body of the precision surface. This one DISPLAYS nothing native owns: a
    // multiplier is not stored anywhere, so there is nothing to re-read, and a
    // refresh triggered by an unrelated change (a body selected, a mesh
    // published) must not throw away a value the user has half typed. The
    // surface is reset when it OPENS -- `resetToIdentity()` -- and not when the
    // world moves underneath it.

    /**
     * Parses the three multipliers and submits them as one atomic request.
     *
     * <p>Only "is it a number" is checked here. Positive-and-finite is a DOMAIN
     * rule and it stays below JNI, where all three are validated before any is
     * written — so one bad multiplier changes none of them, and this layer
     * reports what native decided rather than re-implementing it.
     */
    private void onApply() {
        final BigDecimal[] multiplier = new BigDecimal[3];
        for (int i = 0; i < 3; i++) {
            multiplier[i] = readField(fields[i]);
            if (multiplier[i] == null) {
                return;
            }
        }
        final Context context = getContext();
        switch (NativeViewport.applyBodyRelativeScale(multiplier[0].doubleValue(),
                multiplier[1].doubleValue(), multiplier[2].doubleValue())) {
            case NativeViewport.APPLY_APPLIED:
                host.onNativeStateChanged();
                host.showStatus(context.getString(R.string.status_relative_scale_applied,
                        describeAbsoluteScale()), R.attr.fsTextSuccess);
                // Back to the identity at once: the multiplier has been spent,
                // and leaving "2" in the field would invite a second doubling
                // the user did not intend to ask for.
                resetToIdentity();
                host.finishEditing();
                break;
            case NativeViewport.APPLY_UNCHANGED:
                host.showStatus(context.getString(R.string.status_relative_scale_unchanged),
                        R.attr.fsTextSecondary);
                resetToIdentity();
                host.finishEditing();
                break;
            case NativeViewport.APPLY_REJECTED_NOT_POSITIVE:
                host.showStatus(context.getString(R.string.reject_dimension_not_positive),
                        R.attr.fsTextError);
                break;
            case NativeViewport.APPLY_REJECTED_LOCKED:
                host.showStatus(context.getString(R.string.reject_dimension_locked),
                        R.attr.fsTextError);
                break;
            case NativeViewport.APPLY_REJECTED_UNAVAILABLE:
                host.showStatus(context.getString(R.string.reject_dimension_unavailable),
                        R.attr.fsTextError);
                break;
            case NativeViewport.APPLY_REJECTED_NOT_REPRESENTABLE:
                host.showStatus(context.getString(R.string.reject_transform_not_representable),
                        R.attr.fsTextError);
                break;
            default:
                host.showStatus(context.getString(R.string.reject_dimension_other),
                        R.attr.fsTextError);
                break;
        }
    }

    private BigDecimal readField(NumericPropertyRow row) {
        final String raw = row.text();
        try {
            return LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                            ? getContext().getString(R.string.field_empty, row.label())
                            : getContext().getString(R.string.field_not_a_number, row.label(),
                                    raw.trim()),
                    R.attr.fsTextError);
            row.focusForCorrection();
            return null;
        }
    }

    /** The stored Absolute Scale after a commit, as a bare unitless triple. */
    private String describeAbsoluteScale() {
        NativeViewport.bodyDimensionsState(nativeState);
        final int s = NativeViewport.BODY_DIM_SCALE_X;
        return LengthUnit.present(BigDecimal.valueOf(nativeState[s])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeState[s + 1])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeState[s + 2]));
    }
}
