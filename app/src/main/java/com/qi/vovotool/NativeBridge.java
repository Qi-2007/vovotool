package com.qi.vovotool;

import android.content.Context;

public final class NativeBridge {
    private static boolean loaded;
    private NativeBridge() {}
    public static synchronized void load(Context context) throws Exception {
        if (loaded) return;
        try { System.loadLibrary("gipc"); }
        catch (UnsatisfiedLinkError first) {
            Context module = context.createPackageContext(Settings.PACKAGE, Context.CONTEXT_IGNORE_SECURITY);
            System.load(module.getApplicationInfo().nativeLibraryDir + "/libgipc.so");
        }
        loaded = true;
    }
    public static native void configure(boolean enabled, float scale, boolean capture, int port, String token, float touchpadGain);
    public static native int[] state();
    public static native void setCaptureState(boolean captured, boolean focused);
    public static native void motion(float x, float y, float vertical, float horizontal, int buttons);
    public static native boolean key(int code, boolean down);
    public static native void configureUid(boolean hidden);
    public static native int[] uidState();
    public static native void configureFocusGuard(boolean active);
    public static native int[] focusState();
}
