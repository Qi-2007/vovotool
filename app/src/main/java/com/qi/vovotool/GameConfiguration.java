package com.qi.vovotool;

import android.content.Context;
import android.database.Cursor;

/** One configuration path for initial game startup and subsequent live updates. */
final class GameConfiguration {
    private GameConfiguration() {}
    static void load(Context context){
        try(Cursor c=context.getContentResolver().query(Settings.URI,null,null,null,null)){
            if(c!=null&&c.moveToFirst())apply(context,c);
            else Settings.requestConfigurationWakeup(context);
        }catch(Exception error){Settings.requestConfigurationWakeup(context);android.util.Log.w("vovotool","Startup configuration unavailable; will retry on game resume",error);}
    }
    static void apply(Context context,Cursor c){
        NativeBridge.configure(flag(c,"enabled"),c.getFloat(c.getColumnIndexOrThrow("mouse_scale")),flag(c,"capture"),
            c.getInt(c.getColumnIndexOrThrow("proxy_port")),c.getString(c.getColumnIndexOrThrow("proxy_token")),c.getFloat(c.getColumnIndexOrThrow("touchpad_gain")));
        NativeBridge.configureAvatarSwitch(flag(c,"no_avatar_switch_cd"));
        NativeBridge.configureAvatarFade(flag(c,"disable_character_fade"));
        NativeBridge.configureTeamLoading(flag(c,"skip_team_loading"));
        NativeBridge.configureUid(flag(c,"hide_uid"));
        NativeBridge.configureFocusGuard(flag(c,"focus_guard"));
        NativeBridge.configureCraft(flag(c,"portable_craft"));
        NativeBridge.configureDaily(flag(c,"portable_daily"));
        NativeBridge.configureExpedition(flag(c,"portable_expedition"));
        GraphicsSettings.configure(context,c);
    }
    private static boolean flag(Cursor c,String key){return c.getInt(c.getColumnIndexOrThrow(key))!=0;}
}
