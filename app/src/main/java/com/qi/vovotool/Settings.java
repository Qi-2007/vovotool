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
    private static long lastWakeup=-5000;
    private Settings() {}
    public static SharedPreferences preferences(Context context) { return context.getSharedPreferences("settings", Context.MODE_PRIVATE); }
    static int developerLevel(SharedPreferences p){return DeveloperMode.normalize(p.getInt(DeveloperMode.LEVEL_KEY,0));}
    static boolean effective(SharedPreferences p,String key,boolean fallback){
        return DeveloperMode.allowed(developerLevel(p),key)&&p.getBoolean(key,fallback);
    }
    static synchronized void requestConfigurationWakeup(Context context){
        // Application.attach is too early for an activity launch. Retry from the
        // game's resumed Activity instead, without consuming the wakeup cooldown.
        if(!(context instanceof android.app.Activity))return;
        long now=android.os.SystemClock.elapsedRealtime();if(now-lastWakeup<5000)return;lastWakeup=now;
        try{
            Intent intent=new Intent().setClassName(PACKAGE,PACKAGE+".ConfigurationBootstrapActivity")
                .addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
            context.startActivity(intent);
        }catch(RuntimeException error){Log.w("vovotool","Configuration bootstrap deferred until game resume",error);}
    }
    public static void grantGameAccess(Context context) {
        try { context.grantUriPermission(GAME,URI,Intent.FLAG_GRANT_READ_URI_PERMISSION); }
        catch (RuntimeException error) { Log.w("vovotool","Cannot grant the scoped game configuration URI",error); }
    }
}
