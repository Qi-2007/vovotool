package com.qi.vovotool;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.content.Intent;
import android.util.Log;

public final class Settings {
    public static final String PACKAGE="com.qi.vovotool";
    static final String TOUCHPAD_COMPAT="touchpad_compat";
    public static final String GAME="com.miHoYo.Yuanshen";
    public static final String VERSION="7.2.0_48751704_49008628";
    public static final Uri URI=Uri.parse("content://"+PACKAGE+".config/state");
    private Settings() {}
    public static SharedPreferences preferences(Context context) { return context.getSharedPreferences("settings", Context.MODE_PRIVATE); }
    static int developerLevel(SharedPreferences p){return DeveloperMode.normalize(p.getInt(DeveloperMode.LEVEL_KEY,0));}
    static boolean effective(SharedPreferences p,String key,boolean fallback){
        return DeveloperMode.allowed(developerLevel(p),key)&&p.getBoolean(key,fallback);
    }
    public static void grantGameAccess(Context context) {
        try { context.grantUriPermission(GAME,URI,Intent.FLAG_GRANT_READ_URI_PERMISSION); }
        catch (RuntimeException error) { Log.w("vovotool","Cannot grant the scoped game configuration URI",error); }
    }
}
