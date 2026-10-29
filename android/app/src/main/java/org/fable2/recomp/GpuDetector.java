package org.fable2.recomp;

import android.opengl.*;
import android.util.Log;

/** Queries the system GLES driver, before any custom Vulkan driver is loaded. */
final class GpuDetector {
    private static String cached;

    static synchronized String renderer() {
        if (cached != null) return cached;
        // EGL state is thread-local. Never disturb SDL's or the UI's context.
        String[] result = { "" };
        Thread probe = new Thread(() -> result[0] = probe(), "GpuDetect");
        probe.start();
        try { probe.join(); }
        catch (InterruptedException e) { Thread.currentThread().interrupt(); return ""; }
        cached = result[0];
        Log.i("FableGpu", "System renderer: " + (cached.isEmpty() ? "unknown" : cached));
        return cached;
    }

    private static String probe() {
        EGLDisplay display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        if (display == EGL14.EGL_NO_DISPLAY) return "";
        EGLContext context = EGL14.EGL_NO_CONTEXT;
        EGLSurface surface = EGL14.EGL_NO_SURFACE;
        boolean initialized = false;
        try {
            int[] version = new int[2];
            if (!EGL14.eglInitialize(display, version, 0, version, 1)) return "";
            initialized = true;
            EGLConfig[] configs = new EGLConfig[1];
            int[] count = new int[1];
            int[] attributes = { EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT, EGL14.EGL_NONE };
            if (!EGL14.eglChooseConfig(display, attributes, 0, configs, 0, 1, count, 0)
                    || count[0] == 0) return "";
            context = EGL14.eglCreateContext(display, configs[0], EGL14.EGL_NO_CONTEXT,
                new int[] { EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE }, 0);
            if (context == EGL14.EGL_NO_CONTEXT) return "";
            surface = EGL14.eglCreatePbufferSurface(display, configs[0],
                new int[] { EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE }, 0);
            if (surface == EGL14.EGL_NO_SURFACE || !EGL14.eglMakeCurrent(display, surface, surface, context)) return "";
            String renderer = GLES20.glGetString(GLES20.GL_RENDERER);
            return renderer == null ? "" : renderer;
        } catch (RuntimeException e) {
            Log.w("FableGpu", "GPU detection failed; using System by default", e);
            return "";
        } finally {
            if (initialized) {
                EGL14.eglMakeCurrent(display, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
                if (surface != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(display, surface);
                if (context != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(display, context);
                EGL14.eglTerminate(display);
            }
            EGL14.eglReleaseThread();
        }
    }
}
