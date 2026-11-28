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
    public static final String GAME_OS="com.miHoYo.GenshinImpact";
    public static final String VERSION="7.2.0_48751704_49008628";
    static boolean isGame(String name){return GAME.equals(name)||GAME_OS.equals(name);}
    public static final Uri URI=Uri.parse("content://"+PACKAGE+".config/state");
    private static long lastWakeup=-5000;
    // 区服选择与配置按区服隔离：主页选中区服存 meta，游戏/管理页各读自己的 settings。
    private static final String META="region", ACTIVE_KEY="active_game";
    private Settings() {}
    private static SharedPreferences meta(Context context){return context.getSharedPreferences(META,Context.MODE_PRIVATE);}
    /** 当前主页选中的区服；默认国服，持久化。 */
    public static String activeGame(Context context){return meta(context).getString(ACTIVE_KEY,GAME);}
    public static void setActiveGame(Context context,String game){
        if(!isGame(game))return;
        meta(context).edit().putString(ACTIVE_KEY,game).apply();
    }
    /** 按区服取配置文件：国服 settings_cn，国际服 settings_os。首次使用时迁移旧 settings。 */
    public static SharedPreferences preferencesFor(Context context,String game){
        String file=GAME.equals(game)?"settings_cn":"settings_os";
        SharedPreferences target=context.getSharedPreferences(file,Context.MODE_PRIVATE);
        SharedPreferences legacy=context.getSharedPreferences("settings",Context.MODE_PRIVATE);
        if(!target.contains("enabled")&&legacy.contains("enabled")){
            android.content.SharedPreferences.Editor e=target.edit();
            for(java.util.Map.Entry<String,?> entry:legacy.getAll().entrySet()){
                Object v=entry.getValue();
                if(v instanceof Boolean)e.putBoolean(entry.getKey(),(Boolean)v);
                else if(v instanceof Integer)e.putInt(entry.getKey(),(Integer)v);
                else if(v instanceof Float)e.putFloat(entry.getKey(),(Float)v);
                else if(v instanceof Long)e.putLong(entry.getKey(),(Long)v);
                else if(v instanceof String)e.putString(entry.getKey(),(String)v);
            }
            e.apply();
        }
        return target;
    }
    /** 主页/管理页使用的配置：跟随当前选中区服。 */
    public static SharedPreferences preferences(Context context){return preferencesFor(context,activeGame(context));}
    /** 调用方 uid 对应的游戏配置；用于 ContentProvider 按包名分流。 */
    public static SharedPreferences preferencesForCaller(Context context,int uid){
        String game=GAME;
        String[] packages=context.getPackageManager().getPackagesForUid(uid);
        if(packages!=null)for(String name:packages)if(isGame(name)){game=name;break;}
        return preferencesFor(context,game);
    }
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
        grantGameAccess(context,GAME);
        grantGameAccess(context,GAME_OS);
    }
    private static void grantGameAccess(Context context,String game) {
        try { context.grantUriPermission(game,URI,Intent.FLAG_GRANT_READ_URI_PERMISSION); }
        catch (RuntimeException error) { Log.w("vovotool","Cannot grant the scoped game configuration URI",error); }
    }
}
