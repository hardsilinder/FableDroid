package org.fable2.recomp;

import android.app.Activity;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/** Drag touch controls into place, resize or hide them, then save. */
public final class TouchLayoutActivity extends Activity {
    private TouchControls controls;
    private TextView selectedLabel;
    private Button smaller, larger, hide;
    private Settings settings;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        settings = new Settings(this);
        FrameLayout root = new FrameLayout(this);
        root.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
            new int[] { 0xFF2B4A3A, 0xFF1A2E33, 0xFF0B1A1C }));
        controls = new TouchControls(this, true);
        if (state != null && state.getString("layout") != null) controls.applyLayout(state.getString("layout"));
        root.addView(controls, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT));

        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setBackground(Ui.rounded(this, 0xE6132B2E, 16, Ui.GOLD_DARK, 1));
        int p = Ui.dp(this, 10);
        bar.setPadding(p, p, p, p);
        selectedLabel = Ui.text(this, "Drag a control to move it", 15, Ui.IVORY, true);
        selectedLabel.setPadding(Ui.dp(this, 6), 0, Ui.dp(this, 12), 0);
        bar.addView(selectedLabel);
        smaller = add(bar, "Smaller", v -> resize(1 / 1.15f));
        larger = add(bar, "Larger", v -> resize(1.15f));
        hide = add(bar, "Hide", v -> {
            TouchControls.Control c = controls.selected();
            if (c == null || c.key == TouchControls.TOGGLE) return;
            c.hidden = !c.hidden; controls.refreshSelected(); updateBar(c);
        });
        add(bar, "Reset", v -> { controls.resetLayout(); updateBar(controls.selected()); });
        add(bar, "Cancel", v -> finish());
        Button save = Ui.primary(this, "Save");
        save.setTextSize(15);
        save.setMinHeight(Ui.dp(this, 44));
        save.setPadding(Ui.dp(this, 20), 0, Ui.dp(this, 20), 0);
        save.setOnClickListener(v -> { settings.setTouchLayout(controls.layout()); finish(); });
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.leftMargin = Ui.dp(this, 8);
        bar.addView(save, lp);
        FrameLayout.LayoutParams barParams = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        barParams.topMargin = Ui.dp(this, 72);   // below the shoulder buttons, clear of the D-pad
        root.addView(bar, barParams);
        controls.setSelectionListener(this::updateBar);
        updateBar(null);
        setContentView(root);
        hideSystemBars();
    }

    private Button add(LinearLayout bar, String label, View.OnClickListener l) {
        Button b = Ui.secondary(this, label);
        b.setMinHeight(Ui.dp(this, 44));
        b.setOnClickListener(l);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.leftMargin = Ui.dp(this, 8);
        bar.addView(b, lp);
        return b;
    }

    private void resize(float factor) {
        TouchControls.Control c = controls.selected();
        if (c == null) return;
        c.size = Math.max(0.5f, Math.min(2.5f, c.size * factor));
        controls.refreshSelected();
        updateBar(c);
    }

    private void updateBar(TouchControls.Control c) {
        boolean has = c != null;
        selectedLabel.setText(has ? c.label + "  ·  " + Math.round(c.size * 100) + "%" : "Drag a control to move it");
        smaller.setEnabled(has); larger.setEnabled(has);
        hide.setEnabled(has && c.key != TouchControls.TOGGLE);
        hide.setText(has && c.hidden ? "Show" : "Hide");
    }

    @Override protected void onSaveInstanceState(Bundle out) {
        super.onSaveInstanceState(out);
        out.putString("layout", controls.layout());
    }

    private void hideSystemBars() {
        WindowInsetsController c = getWindow().getInsetsController();
        if (c == null) return;
        c.hide(WindowInsets.Type.systemBars());
        c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }
}
