package com.forgeshape.app;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

/**
 * The Sculpt Mode control strip.
 *
 * <p>A compact overlay carrying exactly what this stage has: the four tools
 * (<b>Grab</b>, <b>Clay</b>, <b>Smooth</b>, <b>Inflate</b>), the <b>Radius</b>
 * and <b>Strength</b> they all share, and the way back to Construction Mode.
 * There is no per-tool setting, no symmetry, no mask and no undo, because none
 * of those exist below it.
 *
 * <p>The tool row is a plain row of buttons rather than a palette or a registry:
 * four tools do not need one, and the highlighted button is drawn from the tool
 * native code reports, not from what was last tapped.
 *
 * <p><b>This view is presentation and input only.</b> It holds no vertex, no
 * brush value and no mode: the sliders' positions are read back from native
 * state on every refresh, and every control's only effect is one call through
 * {@link NativeViewport}. Native code clamps the brush and owns the mode, so the
 * panel can never put the session into a state native code did not agree to.
 *
 * <p>It takes the Construction panel's place while Sculpt Mode is active, so the
 * shape and transform editors are not merely disabled but absent — there is
 * nothing on screen in Sculpt Mode that edits the Construction Source.
 *
 * <p>Built from plain framework views (no Compose, no AndroidX, no design
 * system) and laid out in code, matching the rest of the shell.
 */
final class SculptPanelView extends LinearLayout {

    private static final int COLOR_PANEL = 0xF0141A22;
    private static final int COLOR_LABEL = 0xFF9AA7B8;
    private static final int COLOR_SECTION = 0xFF6E8CAE;
    private static final int COLOR_TEXT = 0xFFF2F5F9;
    private static final int COLOR_NEUTRAL = 0xFF9AA7B8;
    private static final int COLOR_WARN = 0xFFE7C26B;
    private static final int COLOR_ACCENT = 0xFF3E6FA8;
    private static final int COLOR_TOOL_IDLE = 0xFF232C39;
    private static final int COLOR_TOOL_ACTIVE = 0xFF3E6FA8;

    /**
     * The tools, in the order native code numbers them. The index into this
     * array IS the {@code NativeViewport.TOOL_*} constant, so there is no
     * separate mapping table to drift.
     */
    private static final String[] TOOL_NAMES = {"Grab", "Clay", "Smooth", "Inflate"};

    /** One line per tool saying what the finger will do. */
    private static final String[] TOOL_HINTS = {
            "drag the surface with your finger",
            "deposit material outward as you drag",
            "even out roughness as you drag",
            "expand the surface outward as you drag",
    };

    private final Button[] toolButtons = new Button[TOOL_NAMES.length];

    /**
     * Slider resolution. The sliders are integer-valued controls over a
     * continuous native range, so they need a step count; 100 gives a pixel of
     * radius per step at the coarse end and is finer than a fingertip.
     */
    private static final int SLIDER_STEPS = 100;

    /**
     * The brush ranges, mirroring the native clamp limits so the slider ends
     * line up with what native code will actually accept. Native remains the
     * authority: it clamps whatever arrives, and the panel reads the result back
     * rather than assuming its own mapping was honoured.
     */
    private static final double MIN_RADIUS_PIXELS = 24.0;
    private static final double MAX_RADIUS_PIXELS = 600.0;
    private static final double MIN_STRENGTH = 0.05;
    private static final double MAX_STRENGTH = 1.0;

    private final SeekBar radiusBar;
    private final SeekBar strengthBar;
    private final TextView radiusValue;
    private final TextView strengthValue;
    private final TextView statusLine;

    /** Reused across reads; native fills it with the authoritative state. */
    private final double[] nativeState = new double[NativeViewport.SCULPT_STATE_SIZE];

    /** The view that gets focus back once a control has been used. */
    private final View viewportView;

    /** Told after a mode change, so the host can swap which panel is on screen. */
    private final Runnable onModeChanged;

    /** Set while this class drives a slider, so it does not react to itself. */
    private boolean settingSlidersProgrammatically;

    SculptPanelView(Context context, View viewportView, Runnable onModeChanged) {
        super(context);
        this.viewportView = viewportView;
        this.onModeChanged = onModeChanged;

        setOrientation(VERTICAL);
        setBackground(panelBackground());
        setPadding(dp(14), dp(8), dp(14), dp(8));
        // The panel is opaque to touch: see onTouchEvent.
        setClickable(true);

        addView(buildHeaderRow(context));
        addView(buildToolRow(context));

        radiusValue = valueLabel(context);
        radiusBar = buildSlider(context);
        addView(sliderRow(context, "Radius", radiusValue, radiusBar, dp(6)));

        strengthValue = valueLabel(context);
        strengthBar = buildSlider(context);
        addView(sliderRow(context, "Strength", strengthValue, strengthBar, dp(4)));

        statusLine = new TextView(context);
        statusLine.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12.0f);
        statusLine.setTextColor(COLOR_NEUTRAL);
        statusLine.setPadding(0, dp(6), 0, 0);
        addView(statusLine);

        refreshFromNative();
    }

    // -----------------------------------------------------------------------
    // Construction of the view tree
    // -----------------------------------------------------------------------

    private View buildHeaderRow(Context context) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setLayoutParams(rowParams(0));

        final TextView title = new TextView(context);
        title.setText("Sculpt");
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13.0f);
        title.setTextColor(COLOR_SECTION);
        row.addView(title, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT,
                1.0f));

        final Button back = new Button(context);
        back.setText("Back to Construction");
        back.setAllCaps(false);
        back.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14.0f);
        back.setTextColor(COLOR_TEXT);
        back.setBackground(buttonBackground());
        back.setPadding(dp(14), dp(5), dp(14), dp(5));
        back.setContentDescription("Back to Construction");
        back.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onBackToConstruction();
            }
        });
        row.addView(back);
        return row;
    }

    /**
     * The tool selector: one button per tool, equally weighted so the row is the
     * width of the panel and every tool is one tap away.
     *
     * <p>Tapping a button only <em>asks</em> for that tool. Which button is
     * highlighted is decided by {@link #showActiveTool()} from what native code
     * reports afterwards, so the panel cannot show a tool the session is not
     * actually holding.
     */
    private View buildToolRow(Context context) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        row.setLayoutParams(rowParams(dp(6)));

        for (int i = 0; i < TOOL_NAMES.length; ++i) {
            final int tool = i;
            final Button button = new Button(context);
            button.setText(TOOL_NAMES[i]);
            button.setAllCaps(false);
            button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13.0f);
            button.setTextColor(COLOR_TEXT);
            button.setPadding(dp(4), dp(4), dp(4), dp(4));
            button.setContentDescription("Tool " + TOOL_NAMES[i]);
            button.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    onToolSelected(tool);
                }
            });
            toolButtons[i] = button;

            final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0,
                    ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
            params.leftMargin = (i == 0) ? 0 : dp(4);
            row.addView(button, params);
        }
        return row;
    }

    private TextView valueLabel(Context context) {
        final TextView label = new TextView(context);
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12.0f);
        label.setTextColor(COLOR_TEXT);
        label.setGravity(Gravity.END);
        return label;
    }

    private SeekBar buildSlider(Context context) {
        final SeekBar bar = new SeekBar(context);
        bar.setMax(SLIDER_STEPS);
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (settingSlidersProgrammatically) {
                    return;
                }
                onBrushChanged();
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });
        return bar;
    }

    private View sliderRow(Context context, String name, TextView value, SeekBar bar,
                           int topMargin) {
        final LinearLayout column = new LinearLayout(context);
        column.setOrientation(VERTICAL);
        column.setLayoutParams(rowParams(topMargin));

        final LinearLayout labels = new LinearLayout(context);
        labels.setOrientation(HORIZONTAL);
        final TextView label = new TextView(context);
        label.setText(name);
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11.0f);
        label.setTextColor(COLOR_LABEL);
        labels.addView(label, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));
        labels.addView(value);
        column.addView(labels, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        bar.setContentDescription(name);
        column.addView(bar, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        return column;
    }

    private LinearLayout.LayoutParams rowParams(int topMargin) {
        final LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = topMargin;
        return params;
    }

    // -----------------------------------------------------------------------
    // Touch boundary
    // -----------------------------------------------------------------------

    /**
     * Swallows every touch that lands on the panel and is not taken by one of
     * its controls, exactly as the Construction panel does.
     *
     * <p>Without this, an unclaimed touch inside the panel would fall through to
     * the {@code SurfaceView} underneath — which in Sculpt Mode would mean
     * deforming the model by reaching for a slider.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }

    // -----------------------------------------------------------------------
    // Brush and mode
    // -----------------------------------------------------------------------

    /**
     * Submits both brush values together and reads back what native code
     * actually kept, so the labels always show the real brush rather than the
     * slider's opinion of it.
     *
     * <p>This changes no geometry: the brush takes effect on the next stroke,
     * and a stroke already in progress keeps the radius and the affected set it
     * captured when it started.
     */
    private void onBrushChanged() {
        NativeViewport.setSculptBrush(radiusFromSlider(), strengthFromSlider());
        NativeViewport.sculptState(nativeState);
        showBrushValues();
        final int active = activeTool();
        setStatus("Brush: radius " + describeRadius() + ", strength " + describeStrength()
                + " — " + TOOL_NAMES[active] + " will " + TOOL_HINTS[active] + ".", COLOR_NEUTRAL);
    }

    /**
     * Asks native code for a tool, then reads back which tool is actually
     * active and paints the row from that.
     *
     * <p>Changing tool touches no geometry: it publishes no revision, uploads
     * nothing, does not re-freeze and does not disturb any sculpting already
     * done. It also leaves Radius and Strength exactly as they were, because
     * both are shared by every tool.
     */
    private void onToolSelected(int tool) {
        NativeViewport.setSculptTool(tool);
        NativeViewport.sculptState(nativeState);
        showActiveTool();
        final int active = activeTool();
        setStatus(TOOL_NAMES[active] + ": " + TOOL_HINTS[active] + ".", COLOR_NEUTRAL);
        returnFocusToViewport();
    }

    private int activeTool() {
        final int tool = (int) nativeState[NativeViewport.SCULPT_TOOL];
        return (tool >= 0 && tool < TOOL_NAMES.length) ? tool : NativeViewport.TOOL_GRAB;
    }

    /** Highlights whichever tool native code says is active, and only that one. */
    private void showActiveTool() {
        final int active = activeTool();
        for (int i = 0; i < toolButtons.length; ++i) {
            toolButtons[i].setBackground(toolBackground(i == active));
        }
    }

    private void onBackToConstruction() {
        NativeViewport.enterConstructionMode();
        if (onModeChanged != null) {
            onModeChanged.run();
        }
        returnFocusToViewport();
    }

    private double radiusFromSlider() {
        final double t = (double) radiusBar.getProgress() / SLIDER_STEPS;
        return MIN_RADIUS_PIXELS + t * (MAX_RADIUS_PIXELS - MIN_RADIUS_PIXELS);
    }

    private double strengthFromSlider() {
        final double t = (double) strengthBar.getProgress() / SLIDER_STEPS;
        return MIN_STRENGTH + t * (MAX_STRENGTH - MIN_STRENGTH);
    }

    private static int sliderFor(double value, double min, double max) {
        final double t = (value - min) / (max - min);
        final int step = (int) Math.round(t * SLIDER_STEPS);
        if (step < 0) return 0;
        if (step > SLIDER_STEPS) return SLIDER_STEPS;
        return step;
    }

    // -----------------------------------------------------------------------
    // Reading native truth
    // -----------------------------------------------------------------------

    /**
     * Rewrites the sliders and the labels from authoritative native state.
     *
     * <p>This is the only source of what the panel shows, at construction, on
     * every resume and after every brush change. Nothing about the brush or the
     * mode is stored on the Java side.
     */
    void refreshFromNative() {
        NativeViewport.sculptState(nativeState);
        settingSlidersProgrammatically = true;
        radiusBar.setProgress(sliderFor(nativeState[NativeViewport.SCULPT_RADIUS_PIXELS],
                MIN_RADIUS_PIXELS, MAX_RADIUS_PIXELS));
        strengthBar.setProgress(sliderFor(nativeState[NativeViewport.SCULPT_STRENGTH],
                MIN_STRENGTH, MAX_STRENGTH));
        settingSlidersProgrammatically = false;
        showBrushValues();
        showActiveTool();

        if (nativeState[NativeViewport.SCULPT_SOURCE_STALE] != 0.0) {
            // The stale-source policy, stated plainly: the sculpt work is never
            // silently replaced, and adopting the new Construction shape is an
            // explicit Freeze the user has to ask for.
            setStatus("The Construction shape changed after this Freeze. Your sculpt is kept as "
                    + "it is — Freeze again to start from the new shape.", COLOR_WARN);
        } else {
            // Deliberately not the revision: this line is written when the panel
            // refreshes, and a stroke advances the revision many times without
            // asking the panel anything, so a number here would go stale within
            // one drag and read as if nothing had happened.
            final int active = activeTool();
            setStatus("Frozen mesh: " + (long) nativeState[NativeViewport.SCULPT_VERTEX_COUNT]
                    + " vertices. One finger on the model — " + TOOL_HINTS[active]
                    + "; elsewhere it orbits.", COLOR_NEUTRAL);
        }
    }

    private void showBrushValues() {
        radiusValue.setText(describeRadius());
        strengthValue.setText(describeStrength());
    }

    private String describeRadius() {
        return Math.round(nativeState[NativeViewport.SCULPT_RADIUS_PIXELS]) + " px";
    }

    private String describeStrength() {
        return String.format(java.util.Locale.US, "%.2f",
                nativeState[NativeViewport.SCULPT_STRENGTH]);
    }

    private void setStatus(String message, int color) {
        statusLine.setTextColor(color);
        statusLine.setText(message);
    }

    private void returnFocusToViewport() {
        if (viewportView != null) {
            viewportView.requestFocus();
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

    private GradientDrawable toolBackground(boolean active) {
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(active ? COLOR_TOOL_ACTIVE : COLOR_TOOL_IDLE);
        shape.setCornerRadius(dp(6));
        shape.setStroke(dp(1), active ? (Color.WHITE & 0x66FFFFFF) : 0xFF2C3644);
        return shape;
    }

    private GradientDrawable buttonBackground() {
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(COLOR_ACCENT);
        shape.setCornerRadius(dp(6));
        shape.setStroke(dp(1), Color.WHITE & 0x40FFFFFF);
        return shape;
    }
}
