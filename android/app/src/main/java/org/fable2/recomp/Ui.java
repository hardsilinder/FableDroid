package org.fable2.recomp;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.*;
import java.util.function.IntConsumer;

/**
 * Programmatic styling shared by the launcher screens: deep teal and antique
 * gold to match the app icon. Every control is focusable with a visible
 * focus ring so the launcher works with a gamepad (AYN Thor, Odin, Retroid)
 * as well as touch.
 */
final class Ui {
    static final int BACKGROUND = 0xFF0B1A1C, CARD = 0xFF132B2E, CARD_RAISED = 0xFF1B3A3E;
    static final int GOLD = 0xFFD9B26A, GOLD_DARK = 0xFF8C6E36, IVORY = 0xFFF2EAD8, MUTED = 0xFF9DB3B0;
    static final int GOOD = 0xFF7FC48A, WARN = 0xFFE3A35B, BAD = 0xFFE07A6E;

    private Ui() {}

    static int dp(Context c, float v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, c.getResources().getDisplayMetrics()));
    }

    static GradientDrawable rounded(Context c, int color, float radius, int strokeColor, float stroke) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(dp(c, radius));
        if (stroke > 0) d.setStroke(dp(c, stroke), strokeColor);
        return d;
    }

    private static Drawable states(Context c, int normal, int pressed, int border, float radius) {
        StateListDrawable s = new StateListDrawable();
        s.addState(new int[] { -android.R.attr.state_enabled }, rounded(c, 0xFF22302F, radius, 0, 0));
        s.addState(new int[] { android.R.attr.state_pressed }, rounded(c, pressed, radius, GOLD, 2));
        s.addState(new int[] { android.R.attr.state_focused }, rounded(c, pressed, radius, IVORY, 3));
        s.addState(new int[] {}, rounded(c, normal, radius, border, border == 0 ? 0 : 1));
        return s;
    }

    static TextView text(Context c, String s, float sp, int color, boolean bold) {
        TextView t = new TextView(c);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        if (bold) t.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        return t;
    }

    static TextView title(Context c, String s) {
        TextView t = text(c, s, 15, GOLD, true);
        t.setLetterSpacing(0.08f);
        t.setAllCaps(true);
        t.setPadding(0, dp(c, 4), 0, dp(c, 8));
        return t;
    }

    static Button primary(Context c, String label) {
        Button b = new Button(c);
        b.setText(label);
        b.setAllCaps(false);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
        b.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        b.setTextColor(new ColorStateList(new int[][] { { -android.R.attr.state_enabled }, {} },
            new int[] { 0xFF6B7775, 0xFF1A1206 }));
        b.setBackground(states(c, GOLD, 0xFFF0CC85, 0, 14));
        b.setStateListAnimator(null);
        b.setMinHeight(dp(c, 60));
        return b;
    }

    static Button secondary(Context c, String label) {
        Button b = new Button(c);
        b.setText(label);
        b.setAllCaps(false);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        b.setTextColor(new ColorStateList(new int[][] { { -android.R.attr.state_enabled }, {} },
            new int[] { 0xFF6B7775, IVORY }));
        b.setBackground(states(c, CARD_RAISED, 0xFF2A5257, GOLD_DARK, 12));
        b.setStateListAnimator(null);
        b.setMinHeight(dp(c, 48));
        b.setPadding(dp(c, 16), 0, dp(c, 16), 0);
        return b;
    }

    static LinearLayout card(Context c) {
        LinearLayout l = new LinearLayout(c);
        l.setOrientation(LinearLayout.VERTICAL);
        l.setBackground(rounded(c, CARD, 18, 0x332F5A5E, 1));
        int p = dp(c, 18);
        l.setPadding(p, p, p, p);
        return l;
    }

    static LinearLayout.LayoutParams fill(Context c, float topMargin) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(c, topMargin);
        return lp;
    }

    /** Label and description on the left, control on the right. */
    static LinearLayout row(Context c, String label, String description, View control) {
        LinearLayout row = new LinearLayout(c);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, dp(c, 8), 0, dp(c, 8));
        LinearLayout texts = new LinearLayout(c);
        texts.setOrientation(LinearLayout.VERTICAL);
        texts.addView(text(c, label, 16, IVORY, true));
        if (description != null) {
            TextView d = text(c, description, 13, MUTED, false);
            d.setPadding(0, dp(c, 2), dp(c, 12), 0);
            texts.addView(d);
        }
        row.addView(texts, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
        row.addView(control);
        return row;
    }

    static Switch toggle(Context c, boolean checked, java.util.function.Consumer<Boolean> changed) {
        Switch s = new Switch(c);
        s.setChecked(checked);
        s.setThumbTintList(new ColorStateList(new int[][] { { android.R.attr.state_checked }, {} },
            new int[] { GOLD, 0xFFB8C4C2 }));
        s.setTrackTintList(new ColorStateList(new int[][] { { android.R.attr.state_checked }, {} },
            new int[] { GOLD_DARK, 0xFF3A4B4C }));
        s.setBackground(states(c, Color.TRANSPARENT, 0x332A5257, Color.TRANSPARENT, 20));
        s.setPadding(dp(c, 8), dp(c, 6), dp(c, 8), dp(c, 6));
        s.setOnCheckedChangeListener((v, on) -> changed.accept(on));
        return s;
    }

    /** A row of mutually exclusive buttons. */
    static LinearLayout segmented(Context c, String[] labels, int selected, IntConsumer chosen) {
        LinearLayout group = new LinearLayout(c);
        group.setOrientation(LinearLayout.HORIZONTAL);
        Button[] buttons = new Button[labels.length];
        for (int i = 0; i < labels.length; ++i) {
            final int index = i;
            Button b = new Button(c);
            b.setText(labels[i]);
            b.setAllCaps(false);
            b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
            b.setStateListAnimator(null);
            b.setMinWidth(dp(c, 64));
            b.setMinHeight(dp(c, 42));
            b.setPadding(dp(c, 12), 0, dp(c, 12), 0);
            buttons[i] = b;
            b.setOnClickListener(v -> {
                for (int j = 0; j < buttons.length; ++j) style(c, buttons[j], j == index);
                chosen.accept(index);
            });
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
            if (i > 0) lp.leftMargin = dp(c, 6);
            group.addView(b, lp);
            style(c, b, i == selected);
        }
        return group;
    }

    private static void style(Context c, Button b, boolean selected) {
        b.setTextColor(selected ? 0xFF1A1206 : IVORY);
        b.setBackground(selected ? states(c, GOLD, 0xFFF0CC85, 0, 10) : states(c, CARD_RAISED, 0xFF2A5257, GOLD_DARK, 10));
    }

    static SeekBar slider(Context c, int max, int value, IntConsumer changed) {
        SeekBar s = new SeekBar(c);
        s.setMax(max);
        s.setProgress(value);
        s.setProgressTintList(ColorStateList.valueOf(GOLD));
        s.setThumbTintList(ColorStateList.valueOf(GOLD));
        s.setProgressBackgroundTintList(ColorStateList.valueOf(0xFF3A4B4C));
        s.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int progress, boolean user) { if (user) changed.accept(progress); }
            @Override public void onStartTrackingTouch(SeekBar bar) {}
            @Override public void onStopTrackingTouch(SeekBar bar) {}
        });
        s.setLayoutParams(new LinearLayout.LayoutParams(dp(c, 220), LinearLayout.LayoutParams.WRAP_CONTENT));
        return s;
    }
}
