package com.qi.vovotool;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.SystemClock;

import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/**
 * Runs inside com.miui.powerkeeper. HyperOS applies a per-game refresh rate policy
 * (DisplayFrameSetting) which pins Genshin to 60 Hz even when the game itself is
 * unlocked. The selected graphics frame limit is mirrored onto that policy so the
 * display/compositor stops capping the game.
 */
final class SystemFpsUnlock {
    private static final String CONFIG = "content://com.qi.vovotool.config/state";
    static final String OWNER = "com.miui.powerkeeper";
    private static final String DISPLAY_FRAME_SETTING = "com.miui.powerkeeper.statemachine.DisplayFrameSetting";
    private static final String[] GAME_PACKAGES = {
            "com.miHoYo.Yuanshen", "com.miHoYo.GenshinImpact"
    };
    private static final long CACHE_MS = 3000;
    private static final String USER_FPS_EXTRA = "vovotool.mUserFps";
    private static long lastRead;
    private static volatile int cachedTarget;
    private static int lastApplied = -1;
    private static boolean errorLogged;

    private SystemFpsUnlock() {}

    static void install(ClassLoader loader) {
        try {
            XposedHelpers.findAndHookMethod(DISPLAY_FRAME_SETTING, loader,
                    "setScreenEffect", String.class, int.class, int.class, new XC_MethodHook() {
                        @Override protected void beforeHookedMethod(MethodHookParam param) {
                            try {
                                Object value = param.args[0];
                                if (!(value instanceof String) || !isGame((String) value)) return;
                                int target = target();
                                if (target <= 60) return;
                                int requested = ((Integer) param.args[1]).intValue();
                                int userFps = XposedHelpers.getIntField(param.thisObject, "mUserFps");
                                param.setObjectExtra(USER_FPS_EXTRA, Integer.valueOf(userFps));
                                param.args[1] = Integer.valueOf(target);
                                // The native clamp inside setScreenEffect rewrites fps to mUserFps
                                // unless mUserFps itself is the requested value.
                                XposedHelpers.setIntField(param.thisObject, "mUserFps", target);
                                note((String) value, requested, target);
                            } catch (Throwable error) {
                                XposedBridge.log(error);
                            }
                        }

                        @Override protected void afterHookedMethod(MethodHookParam param) {
                            Object saved = param.getObjectExtra(USER_FPS_EXTRA);
                            if (!(saved instanceof Integer)) return;
                            try {
                                XposedHelpers.setIntField(param.thisObject, "mUserFps", ((Integer) saved).intValue());
                            } catch (Throwable ignored) {
                            }
                        }
                    });
            XposedBridge.log("vovotool: system frame rate hook installed");
        } catch (Throwable error) {
            XposedBridge.log("vovotool: system frame rate hook unavailable");
            XposedBridge.log(error);
        }

        try {
            XposedHelpers.findAndHookMethod(DISPLAY_FRAME_SETTING, loader,
                    "applyFpsViaPowerkeeper", int.class, new XC_MethodHook() {
                        @Override protected void beforeHookedMethod(MethodHookParam param) {
                            try {
                                Object value = XposedHelpers.getObjectField(param.thisObject, "mCurrentFgPkg");
                                if (!(value instanceof String) || !isGame((String) value)) return;
                                int target = target();
                                if (target <= 60) return;
                                int requested = ((Integer) param.args[0]).intValue();
                                if (requested < target) {
                                    param.args[0] = Integer.valueOf(target);
                                    note((String) value, requested, target);
                                }
                            } catch (Throwable error) {
                                XposedBridge.log(error);
                            }
                        }
                    });
        } catch (Throwable error) {
            XposedBridge.log("vovotool: powerkeeper fps fallback unavailable");
            XposedBridge.log(error);
        }
    }

    private static boolean isGame(String pkg) {
        for (String game : GAME_PACKAGES) if (game.equals(pkg)) return true;
        return false;
    }

    /** 0 means "leave the system policy alone"; otherwise a display rate above 60. */
    private static int target() {
        long now = SystemClock.elapsedRealtime();
        if (lastRead != 0 && now - lastRead < CACHE_MS) return cachedTarget;
        lastRead = now;
        int result = 0;
        Context context = context();
        if (context != null) {
            Cursor cursor = null;
            try {
                cursor = context.getContentResolver().query(Uri.parse(CONFIG), null, null, null, null);
                if (cursor != null && cursor.moveToFirst()) {
                    int enabled = cursor.getInt(cursor.getColumnIndexOrThrow("enabled"));
                    int unlock = cursor.getInt(cursor.getColumnIndexOrThrow("system_fps_unlock"));
                    int fps = cursor.getInt(cursor.getColumnIndexOrThrow("target_fps"));
                    if (enabled != 0 && unlock != 0) {
                        result = fps <= 0 ? 144 : Math.min(fps, 144);
                    }
                    errorLogged = false;
                }
            } catch (Throwable error) {
                // The provider may not be reachable yet (module app starting); retry on the next call.
                if (!errorLogged) {
                    errorLogged = true;
                    XposedBridge.log("vovotool: system fps configuration unavailable");
                    XposedBridge.log(error);
                }
            } finally {
                if (cursor != null) try { cursor.close(); } catch (Throwable ignored) {}
            }
        }
        cachedTarget = result;
        return result;
    }

    private static Context context() {
        try {
            Class<?> thread = Class.forName("android.app.ActivityThread");
            Object application = thread.getMethod("currentApplication").invoke(null);
            return application instanceof Context ? (Context) application : null;
        } catch (Throwable error) {
            return null;
        }
    }

    private static void note(String pkg, int requested, int target) {
        if (requested == lastApplied) return;
        lastApplied = requested;
        XposedBridge.log("vovotool: " + pkg + " system frame rate " + requested + " -> " + target);
    }
}
