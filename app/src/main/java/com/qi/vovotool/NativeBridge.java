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
    public static native void configureCraft(boolean enabled);
    public static native boolean craftKey(boolean down);
    public static native int[] craftState();
    public static native void configureExpedition(boolean enabled);
    public static native boolean expeditionKey(boolean down);
    public static native int[] expeditionState();
    public static native void configureDaily(boolean enabled);
    public static native boolean dailyKey(boolean down);
    public static native int[] dailyState();
    public static native void configureFocusGuard(boolean active);
    public static native int[] focusState();
    public static native void configureGraphics(boolean enabled,boolean resolution,int width,int height,int nativeWidth,int nativeHeight,int fps,boolean effects,int vegetation,int path);
    public static native void requestRenderCapture(long token);
    public static native String renderProbe();
    public static native int[] graphicsState();
    public static native int[] renderState();
    public static native int[] photoState();
    public static native int[] lookState();
    public static native void requestLookRefresh(long token);
    public static native void noteTouch(int action);
    public static native int[] touchLookState();
    public static native int[] mapMouseState();
    public static native void configureTeamLoading(boolean enabled);
    public static native int[] teamLoadingState();
}
