package org.fable2.recomp;

import android.app.Activity;
import android.os.Bundle;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.HashMap;
import java.util.Map;

/**
 * Controller remapping. For each Xbox button, pick it and press the physical
 * button that should trigger it. Stored as physical keycode -> Xbox keycode
 * and applied by GameActivity before SDL sees the event.
 */
public final class RemapActivity extends Activity {
    private Settings settings;
    private Map<Integer, Integer> map;   // physical -> xbox
    private final Button[] rows = new Button[Settings.BUTTON_CODES.length];
    private TextView prompt;
    private int listening = -1;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        settings = new Settings(this);
        map = new HashMap<>(settings.buttonMap());
        int pad = Ui.dp(this, 28);
        LinearLayout screen = new LinearLayout(this);
        screen.setOrientation(LinearLayout.VERTICAL);
        screen.setBackgroundColor(Ui.BACKGROUND);
        screen.setPadding(pad, pad, pad, pad);
        screen.addView(Ui.text(this, "Remap controller buttons", 26, Ui.IVORY, true));
        prompt = Ui.text(this, "", 15, Ui.MUTED, false);
        screen.addView(prompt, Ui.fill(this, 6));

        LinearLayout grid = new LinearLayout(this);
        grid.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout[] columns = { Ui.card(this), Ui.card(this) };
        for (int i = 0; i < rows.length; ++i) {
            final int index = i;
            Button b = Ui.secondary(this, "");
            b.setGravity(android.view.Gravity.CENTER_VERTICAL | android.view.Gravity.START);
            b.setOnClickListener(v -> listen(index));
            rows[i] = b;
            columns[i < (rows.length + 1) / 2 ? 0 : 1].addView(b, Ui.fill(this, i == 0 || i == (rows.length + 1) / 2 ? 0 : 8));
        }
        LinearLayout.LayoutParams half = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1);
        grid.addView(columns[0], half);
        LinearLayout.LayoutParams half2 = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1);
        half2.leftMargin = Ui.dp(this, 16);
        grid.addView(columns[1], half2);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(grid);
        screen.addView(scroll, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1));

        LinearLayout actions = new LinearLayout(this);
        Button reset = Ui.secondary(this, "Reset to default");
        reset.setOnClickListener(v -> { map.clear(); listening = -1; refresh(); });
        Button cancel = Ui.secondary(this, "Cancel");
        cancel.setOnClickListener(v -> finish());
        Button save = Ui.primary(this, "Save");
        save.setTextSize(16);
        save.setMinHeight(Ui.dp(this, 48));
        save.setPadding(Ui.dp(this, 28), 0, Ui.dp(this, 28), 0);
        save.setOnClickListener(v -> { settings.setButtonMap(map); finish(); });
        actions.addView(reset);
        LinearLayout.LayoutParams gap = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT);
        gap.leftMargin = Ui.dp(this, 10);
        actions.addView(cancel, gap);
        LinearLayout.LayoutParams gap2 = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT);
        gap2.leftMargin = Ui.dp(this, 10);
        actions.addView(save, gap2);
        screen.addView(actions, Ui.fill(this, 14));
        setContentView(screen);
        refresh();
        rows[0].requestFocus();
    }

    private int effective(int physical) { return map.getOrDefault(physical, physical); }

    /** Physical button currently producing the given Xbox button, or -1. */
    private int physicalFor(int xbox) {
        for (Map.Entry<Integer, Integer> e : map.entrySet()) if (e.getValue() == xbox) return e.getKey();
        return map.containsKey(xbox) ? -1 : xbox;
    }

    private void refresh() {
        for (int i = 0; i < rows.length; ++i) {
            int xbox = Settings.BUTTON_CODES[i];
            int physical = physicalFor(xbox);
            String current = physical < 0 ? "unassigned" : Settings.buttonName(physical);
            boolean changed = physical != xbox;
            rows[i].setText(Settings.BUTTON_NAMES[i] + "    ←    " + (i == listening ? "press a button…" : current)
                + (changed && i != listening ? "  •" : ""));
            rows[i].setTextColor(i == listening ? Ui.GOLD : changed ? Ui.GOLD : Ui.IVORY);
        }
        prompt.setText(listening >= 0
            ? "Press the controller button to use for " + Settings.BUTTON_NAMES[listening] + ". Press it again on the same row to cancel."
            : "Choose an Xbox button, then press the physical button you want for it. Buttons swap so nothing is lost. Triggers and sticks are not remappable.");
    }

    private void listen(int index) { listening = listening == index ? -1 : index; refresh(); }

    private static boolean fromGamepad(KeyEvent e) {
        return e.isFromSource(InputDevice.SOURCE_GAMEPAD) || e.isFromSource(InputDevice.SOURCE_JOYSTICK)
            || KeyEvent.isGamepadButton(e.getKeyCode());
    }

    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if (listening < 0 || !fromGamepad(event)) return super.dispatchKeyEvent(event);
        int code = event.getKeyCode();
        if (event.getAction() == KeyEvent.ACTION_UP) {
            int xbox = Settings.BUTTON_CODES[listening];
            // Swap, so no Xbox button is ever left without a physical button.
            int old = physicalFor(xbox), previous = effective(code);
            map.put(code, xbox);
            if (old >= 0 && old != code) map.put(old, previous);
            map.entrySet().removeIf(e -> e.getKey().equals(e.getValue()));
            listening = -1;
            refresh();
        }
        return true;
    }
}
