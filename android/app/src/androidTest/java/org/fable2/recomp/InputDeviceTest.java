package org.fable2.recomp;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import androidx.test.runner.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.libsdl.app.SDLControllerManager;

/** Only real gamepads may be treated as the SDL controller (issue #3). */
@RunWith(AndroidJUnit4.class)
public final class InputDeviceTest {
    @Test public void onlyRealGamepadsCountAsControllers() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null) continue;
            int s = d.getSources();
            boolean accepted = SDLControllerManager.isDeviceSDLJoystick(id);
            Log.i("FableInputTest", d.getName() + " sources=0x" + Integer.toHexString(s) + " accepted=" + accepted);
            boolean dpadOnly = (s & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD
                && (s & InputDevice.SOURCE_GAMEPAD) != InputDevice.SOURCE_GAMEPAD
                && (s & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK;
            if (dpadOnly) assertFalse(d.getName() + " is not a gamepad", accepted);
            if ((s & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
                    && d.hasKeys(KeyEvent.KEYCODE_BUTTON_A)[0]) assertTrue(d.getName() + " is a gamepad", accepted);
        }
    }
}
