package org.libsdl.app;

import android.view.InputDevice;
import android.view.MotionEvent;
import java.util.ArrayList;
import java.util.HashSet;

/**
 * Shares SDL's existing handheld controller instead of creating player two.
 * Phones without one (e.g. OnePlus Open) get a virtual pad registered
 * straight with SDL's Android joystick driver; it isn't an InputDevice, so
 * SDLJoystickHandler's hot-plug poll never sees or removes it.
 */
public final class FableTouchInput {
    // Far above real InputDevice ids. Axes: 0 LX, 1 LY, 2 RX, 3 RY, 4 LT,
    // 5 RT, matching SDL's generated Android mapping for axis_mask 0x3f.
    private static final int VIRTUAL_ID = 0x0F2B7001;
    private static boolean virtualAdded;
    private static final boolean[] triggerHeld = new boolean[2];

    private static int device() {
        for (int id : InputDevice.getDeviceIds()) {
            if (SDLControllerManager.isDeviceSDLJoystick(id)) return id;
        }
        if (!virtualAdded) {
            // Buttons 0..14: A B X Y Back Guide Start L3 R3 LB RB Up Down Left Right.
            SDLControllerManager.nativeAddJoystick(VIRTUAL_ID, "Fable Touch Controller",
                "fable2-touch-controller", 0, 0, 0x7fff, 6, 0x3f, 0,
                false, false, false, false);
            virtualAdded = true;
        }
        // Triggers rest at -1; SDL starts axes at 0, which reads as half-pulled.
        for (int t = 0; t < 2; ++t)
            if (!triggerHeld[t]) SDLControllerManager.onNativeJoy(VIRTUAL_ID, 4 + t, -1f);
        return VIRTUAL_ID;
    }
    private static final int[] VIRTUAL_AXES = {
        MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_RX, MotionEvent.AXIS_RY,
        MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER };
    public static void button(int keycode, boolean down) {
        int id = device();
        if (id < 0) return;
        SDLControllerManager.pollInputDevices();
        if (down) SDLControllerManager.onNativePadDown(id, keycode, 0);
        else SDLControllerManager.onNativePadUp(id, keycode, 0);
    }
    private static void axis(int id, int androidAxis, float value) {
        if (id == VIRTUAL_ID) {
            for (int i = 0; i < VIRTUAL_AXES.length; ++i) {
                if (VIRTUAL_AXES[i] == androidAxis) {
                    SDLControllerManager.onNativeJoy(id, i, Math.max(-1f, Math.min(1f, value)));
                    return;
                }
            }
            return;
        }
        InputDevice pad = InputDevice.getDevice(id);
        if (pad == null) return;
        ArrayList<InputDevice.MotionRange> ranges = new ArrayList<>(pad.getMotionRanges());
        ranges.sort(new SDLJoystickHandler.RangeComparator());
        HashSet<Integer> seen = new HashSet<>();
        int index = 0;
        for (InputDevice.MotionRange range : ranges) {
            int axis = range.getAxis();
            if ((range.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0
                || !seen.add(axis) || axis == MotionEvent.AXIS_HAT_X || axis == MotionEvent.AXIS_HAT_Y) continue;
            if (axis == androidAxis) {
                SDLControllerManager.onNativeJoy(id, index, Math.max(-1f, Math.min(1f, value)));
                return;
            }
            ++index;
        }
    }
    public static void stick(boolean right, float x, float y) {
        int id = device();
        if (id < 0) return;
        SDLControllerManager.pollInputDevices();
        InputDevice pad = InputDevice.getDevice(id);
        boolean rx = right && (pad == null || pad.getMotionRange(MotionEvent.AXIS_RX, InputDevice.SOURCE_JOYSTICK) != null
            && pad.getMotionRange(MotionEvent.AXIS_RY, InputDevice.SOURCE_JOYSTICK) != null);
        axis(id, right ? (rx ? MotionEvent.AXIS_RX : MotionEvent.AXIS_Z) : MotionEvent.AXIS_X, x);
        axis(id, right ? (rx ? MotionEvent.AXIS_RY : MotionEvent.AXIS_RZ) : MotionEvent.AXIS_Y, y);
    }
    public static void trigger(boolean right, boolean down) {
        triggerHeld[right ? 1 : 0] = down;
        int id = device();
        if (id < 0) return;
        InputDevice pad = InputDevice.getDevice(id);
        int primary = right ? MotionEvent.AXIS_RTRIGGER : MotionEvent.AXIS_LTRIGGER;
        int fallback = right ? MotionEvent.AXIS_GAS : MotionEvent.AXIS_BRAKE;
        int axis = pad == null || pad.getMotionRange(primary, InputDevice.SOURCE_JOYSTICK) != null ? primary : fallback;
        axis(id, axis, down ? 1f : -1f);
    }
}
