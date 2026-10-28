package org.fable2.recomp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.view.HapticFeedbackConstants;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;
import org.libsdl.app.FableTouchInput;

/**
 * On-screen gamepad. Each control has a stable id, a position as a fraction
 * of the screen and a size multiplier, all user-editable (TouchLayoutActivity
 * shows this view in edit mode) and saved in Settings as
 * "id:x:y:size:hidden;...".
 */
final class TouchControls extends View {
    static final int LEFT_STICK = -1, RIGHT_STICK = -2, LEFT_TRIGGER = -3, RIGHT_TRIGGER = -4, TOGGLE = -5;

    // id, label, default x, default y, base radius (at 1280x720), key.
    private static final Object[][] DEFAULTS = {
        {"lb", "LB", .065f, .075f, 29, KeyEvent.KEYCODE_BUTTON_L1},
        {"lt", "LT", .14f, .075f, 29, LEFT_TRIGGER},
        {"back", "Back", .44f, .075f, 29, KeyEvent.KEYCODE_BUTTON_SELECT},
        {"start", "Start", .52f, .075f, 29, KeyEvent.KEYCODE_BUTTON_START},
        {"hide", "Hide", .60f, .075f, 29, TOGGLE},
        {"rt", "RT", .86f, .075f, 29, RIGHT_TRIGGER},
        {"rb", "RB", .935f, .075f, 29, KeyEvent.KEYCODE_BUTTON_R1},
        {"up", "Up", .13f, .43f, 25, KeyEvent.KEYCODE_DPAD_UP},
        {"left", "Left", .085f, .51f, 25, KeyEvent.KEYCODE_DPAD_LEFT},
        {"right", "Right", .175f, .51f, 25, KeyEvent.KEYCODE_DPAD_RIGHT},
        {"down", "Down", .13f, .59f, 25, KeyEvent.KEYCODE_DPAD_DOWN},
        {"ls", "Move", .13f, .80f, 76, LEFT_STICK},
        {"l3", "L3", .24f, .92f, 25, KeyEvent.KEYCODE_BUTTON_THUMBL},
        {"rs", "Look", .72f, .80f, 76, RIGHT_STICK},
        {"r3", "R3", .80f, .92f, 25, KeyEvent.KEYCODE_BUTTON_THUMBR},
        {"y", "Y", .90f, .66f, 29, KeyEvent.KEYCODE_BUTTON_Y},
        {"x", "X", .845f, .76f, 29, KeyEvent.KEYCODE_BUTTON_X},
        {"b", "B", .955f, .76f, 29, KeyEvent.KEYCODE_BUTTON_B},
        {"a", "A", .90f, .86f, 29, KeyEvent.KEYCODE_BUTTON_A},
    };

    static final class Control {
        final String id, label; final float baseRadius; final int key;
        float fx, fy, size = 1f; boolean hidden;
        float x, y, radius, dx, dy;
        Control(String id, String label, float fx, float fy, float baseRadius, int key) {
            this.id = id; this.label = label; this.fx = fx; this.fy = fy; this.baseRadius = baseRadius; this.key = key;
        }
    }

    interface SelectionListener { void selected(Control control); }

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ArrayList<Control> controls = new ArrayList<>();
    private final HashMap<Integer, Control> fingers = new HashMap<>();
    private final HashMap<Integer, Integer> holds = new HashMap<>();
    private final boolean editing;
    private boolean visible = true, vibrate;
    private float scale = 1f, globalSize = 1f, opacity = 0.6f;
    private Control selected;
    private float grabDx, grabDy;
    private SelectionListener selectionListener;

    TouchControls(Context context, boolean editing) {
        super(context);
        this.editing = editing;
        setFocusable(false);
        setContentDescription("Touch gamepad: A B X Y, direction pad, two sticks, shoulders and Start");
        for (Object[] d : DEFAULTS)
            controls.add(new Control((String) d[0], (String) d[1], (Float) d[2], (Float) d[3], (Integer) d[4], (Integer) d[5]));
        Settings settings = new Settings(context);
        applyLayout(settings.touchLayout());
        setAppearance(settings.touchOpacity(), settings.touchScale(), settings.vibration());
    }

    void setAppearance(float opacity, float size, boolean vibrate) {
        this.opacity = opacity; this.globalSize = size; this.vibrate = vibrate;
        place();
    }

    void setSelectionListener(SelectionListener l) { selectionListener = l; }
    Control selected() { return selected; }

    void resetLayout() {
        for (int i = 0; i < DEFAULTS.length; ++i) {
            Control c = controls.get(i);
            c.fx = (Float) DEFAULTS[i][2]; c.fy = (Float) DEFAULTS[i][3]; c.size = 1f; c.hidden = false;
        }
        place();
    }

    void applyLayout(String layout) {
        for (String entry : layout.split(";")) {
            String[] p = entry.split(":");
            if (p.length != 5) continue;
            for (Control c : controls) {
                if (!c.id.equals(p[0])) continue;
                try {
                    c.fx = clamp(Float.parseFloat(p[1]), 0, 1); c.fy = clamp(Float.parseFloat(p[2]), 0, 1);
                    c.size = clamp(Float.parseFloat(p[3]), 0.5f, 2.5f); c.hidden = p[4].equals("1");
                } catch (NumberFormatException ignored) {}
            }
        }
        place();
    }

    String layout() {
        StringBuilder s = new StringBuilder();
        for (Control c : controls) {
            if (s.length() > 0) s.append(';');
            s.append(String.format(Locale.ROOT, "%s:%.4f:%.4f:%.2f:%d", c.id, c.fx, c.fy, c.size, c.hidden ? 1 : 0));
        }
        return s.toString();
    }

    private static float clamp(float v, float lo, float hi) { return Math.max(lo, Math.min(hi, v)); }

    private void place() {
        for (Control c : controls) {
            c.x = getWidth() * c.fx; c.y = getHeight() * c.fy;
            c.radius = c.baseRadius * scale * globalSize * c.size;
        }
        invalidate();
    }

    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        releaseAll();
        scale = Math.min(w / 1280f, h / 720f);
        place();
    }

    private int alpha(float a) { return Math.round(255 * clamp(a, 0, 1)); }

    @Override protected void onDraw(Canvas canvas) {
        for (Control c : controls) {
            boolean shown = editing || (visible && !c.hidden) || c.key == TOGGLE;
            if (!shown) continue;
            float a = editing && c.hidden ? 0.25f : opacity;
            boolean held = holds.containsKey(c.key) || c == selected;
            paint.setPathEffect(null);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(held ? Color.argb(alpha(a * 0.9f), 0xD9, 0xB2, 0x6A) : Color.argb(alpha(a * 0.35f), 0x30, 0x40, 0x42));
            canvas.drawCircle(c.x, c.y, c.radius, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth((c == selected ? 3f : 1.5f) * scale);
            if (editing && c.hidden) paint.setPathEffect(new DashPathEffect(new float[] { 8 * scale, 6 * scale }, 0));
            paint.setColor(Color.argb(alpha(a), 0xFF, 0xFF, 0xFF));
            canvas.drawCircle(c.x, c.y, c.radius, paint);
            paint.setPathEffect(null);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(Color.argb(alpha(Math.min(1f, a + 0.3f)), 0xFF, 0xFF, 0xFF));
            paint.setTextSize(17 * scale * Math.max(0.8f, globalSize));
            paint.setTextAlign(Paint.Align.CENTER);
            if (c.key == LEFT_STICK || c.key == RIGHT_STICK) {
                paint.setColor(Color.argb(alpha(a), 0xFF, 0xFF, 0xFF));
                canvas.drawCircle(c.x + c.dx * c.radius * .7f, c.y + c.dy * c.radius * .7f, c.radius * .3f, paint);
                canvas.drawText(c.label, c.x, c.y + c.radius + 18 * scale, paint);
            } else {
                canvas.drawText(c.key == TOGGLE && !visible && !editing ? "Touch" : c.label, c.x, c.y + 6 * scale, paint);
            }
        }
    }

    private Control hit(float x, float y) {
        Control best = null;
        double bestDistance = Double.MAX_VALUE;
        for (Control c : controls) {
            if (!editing) {
                if ((!(visible && !c.hidden) && c.key != TOGGLE) || (c.key < 0 && fingers.containsValue(c))) continue;
            }
            double d = Math.hypot(x - c.x, y - c.y);
            if (d <= c.radius * 1.15f && d < bestDistance) { best = c; bestDistance = d; }
        }
        return best;
    }

    private void updateStick(Control c, float x, float y) {
        float dx = (x - c.x) / c.radius, dy = (y - c.y) / c.radius;
        float length = (float) Math.max(1, Math.hypot(dx, dy));
        c.dx = dx / length; c.dy = dy / length;
        FableTouchInput.stick(c.key == RIGHT_STICK, c.dx, c.dy);
    }

    private void pressed(Control c, boolean down) {
        int n = holds.getOrDefault(c.key, 0) + (down ? 1 : -1);
        if (n > 0) holds.put(c.key, n); else holds.remove(c.key);
        if ((down && n != 1) || (!down && n > 0)) return;
        if (down && vibrate) performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        if (c.key > 0) FableTouchInput.button(c.key, down);
        else if (c.key == LEFT_TRIGGER || c.key == RIGHT_TRIGGER) FableTouchInput.trigger(c.key == RIGHT_TRIGGER, down);
        else if (c.key == LEFT_STICK || c.key == RIGHT_STICK) {
            if (!down) { c.dx = 0; c.dy = 0; FableTouchInput.stick(c.key == RIGHT_STICK, 0, 0); }
        }
    }

    private boolean editTouch(MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            selected = hit(event.getX(), event.getY());
            if (selected != null) { grabDx = selected.x - event.getX(); grabDy = selected.y - event.getY(); }
            if (selectionListener != null) selectionListener.selected(selected);
        } else if (action == MotionEvent.ACTION_MOVE && selected != null && getWidth() > 0) {
            selected.fx = clamp((event.getX() + grabDx) / getWidth(), 0, 1);
            selected.fy = clamp((event.getY() + grabDy) / getHeight(), 0, 1);
            place();
        }
        invalidate();
        return true;
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        if (editing) return editTouch(event);
        int action = event.getActionMasked(), i = event.getActionIndex(), id = event.getPointerId(i);
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            Control c = hit(event.getX(i), event.getY(i));
            if (c == null) return !fingers.isEmpty();
            if (c.key == TOGGLE) { releaseAll(); visible = !visible; invalidate(); return true; }
            fingers.put(id, c); pressed(c, true);
            if (c.key == LEFT_STICK || c.key == RIGHT_STICK) updateStick(c, event.getX(i), event.getY(i));
        } else if (action == MotionEvent.ACTION_MOVE) {
            for (int p = 0; p < event.getPointerCount(); ++p) {
                Control c = fingers.get(event.getPointerId(p));
                if (c != null && (c.key == LEFT_STICK || c.key == RIGHT_STICK)) updateStick(c, event.getX(p), event.getY(p));
            }
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            Control c = fingers.remove(id); if (c != null) pressed(c, false);
        } else if (action == MotionEvent.ACTION_CANCEL) releaseAll();
        invalidate();
        return true;
    }

    void releaseAll() {
        if (!editing) for (Control c : new ArrayList<>(fingers.values())) pressed(c, false);
        fingers.clear(); holds.clear(); invalidate();
    }

    void refreshSelected() { place(); }
}
