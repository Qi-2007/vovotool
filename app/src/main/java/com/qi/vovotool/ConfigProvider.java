package com.qi.vovotool;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.Process;

public final class ConfigProvider extends ContentProvider {
    static volatile long lastReport;
    static volatile int[] lastGraphicsState=new int[0];
    static volatile int[] lastRenderState=new int[0];
    static volatile int[] lastPhotoState=new int[0];
    static volatile String lastRenderProbe="{}";
    static volatile long renderCaptureToken;
    static volatile long lookRefreshToken;
    static volatile int[] lastLookState=new int[0];
    static volatile int[] lastTouchLookState=new int[0];
    static volatile int[] lastAvatarFadeState=new int[0];
    static volatile int[] lastTeamLoadingState=new int[0];
    static volatile int[] lastMapMouseState=new int[0];
    static volatile int[] lastState=new int[0];
    static volatile int[] lastUidState=new int[0];
    static volatile int[] lastCraftState=new int[0],lastDailyState=new int[0],lastQrState=new int[0],lastExpeditionState=new int[0];
    static volatile int[] lastFocusState=new int[0],lastWindowState=new int[0];
    @Override public boolean onCreate(){Settings.grantGameAccess(getContext());return true;}
    private void checkCaller(){
        int uid=Binder.getCallingUid();if(uid==Process.myUid())return;
        String[] packages=getContext().getPackageManager().getPackagesForUid(uid);
        if(packages!=null)for(String name:packages)if(Settings.GAME.equals(name))return;
        throw new SecurityException("Only the scoped game may read module configuration");
    }
    @Override public Cursor query(Uri uri,String[] projection,String selection,String[] args,String sort){
        checkCaller();
        RootProxyService service=RootProxyService.instance;
        if(service!=null)service.ensureHelper();
        SharedPreferences p=Settings.preferences(getContext());
        MatrixCursor result=new MatrixCursor(new String[]{"enabled","capture","mouse_scale","touchpad_gain","proxy_port","proxy_token","hide_uid","focus_guard","portable_craft","craft_key","craft_meta","portable_daily","daily_key","daily_meta","qr_login","portable_expedition","expedition_key","expedition_meta","graphics_enabled","render_resolution","render_width","render_height","target_fps","effect_enhance","veg_percent","render_path","render_capture","look_refresh","skip_team_loading","disable_character_fade"});
        result.addRow(new Object[]{p.getBoolean("enabled",true)?1:0,p.getBoolean("capture",true)?1:0,p.getFloat("mouse_scale",0.02f),p.getFloat("touchpad_gain",0.25f),RootProxyService.port,RootProxyService.token,p.getBoolean("hide_uid",true)?1:0,p.getBoolean("focus_guard",true)?1:0,p.getBoolean("portable_craft",true)?1:0,p.getInt("craft_key",Shortcut.DEFAULT_KEY),p.getInt("craft_meta",0)&Shortcut.META_MASK,p.getBoolean("portable_daily",true)?1:0,p.getInt("daily_key",Shortcut.DEFAULT_DAILY_KEY),p.getInt("daily_meta",0)&Shortcut.META_MASK,p.getBoolean("qr_login",true)?1:0,p.getBoolean("portable_expedition",true)?1:0,p.getInt("expedition_key",Shortcut.DEFAULT_EXPEDITION_KEY),p.getInt("expedition_meta",0)&Shortcut.META_MASK,p.getBoolean("graphics_enabled",false)?1:0,p.getBoolean("render_resolution",false)?1:0,p.getInt("render_width",0),p.getInt("render_height",0),p.getInt("target_fps",0),p.getBoolean("effect_enhance",false)?1:0,p.getInt("veg_percent",100),p.getInt("render_path",3),renderCaptureToken,lookRefreshToken,p.getBoolean("skip_team_loading",true)?1:0,p.getBoolean("disable_character_fade",true)?1:0});
        return result;
    }
    @Override public Bundle call(String method,String arg,Bundle extras){
        if("diagnostics".equals(method) && (Binder.getCallingUid()==0 || Binder.getCallingUid()==Process.myUid())){
            Bundle result=new Bundle();result.putLong("lastReport",lastReport);result.putIntArray("state",lastState);
            result.putString("rootStatus",RootProxyService.status);result.putInt("proxyPort",RootProxyService.port);
            result.putIntArray("uidState",lastUidState);
            result.putIntArray("craftState",lastCraftState);
            result.putIntArray("dailyState",lastDailyState);
            result.putIntArray("qrState",lastQrState);
            result.putIntArray("expeditionState",lastExpeditionState);
            result.putIntArray("graphicsState",lastGraphicsState);
            result.putIntArray("renderState",lastRenderState);
            result.putIntArray("photoState",lastPhotoState);
            result.putIntArray("lookState",lastLookState);
            result.putIntArray("touchLookState",lastTouchLookState);
            result.putIntArray("mapMouseState",lastMapMouseState);
            result.putIntArray("teamLoadingState",lastTeamLoadingState);
            result.putIntArray("avatarFadeState",lastAvatarFadeState);
            result.putIntArray("focusState",lastFocusState);result.putIntArray("windowState",lastWindowState);
            return result;
        }
        if("set_avatar_fade".equals(method)&&(Binder.getCallingUid()==0||Binder.getCallingUid()==Process.myUid())){
            if(!"true".equals(arg)&&!"false".equals(arg))throw new IllegalArgumentException("Expected true or false");
            Settings.preferences(getContext()).edit().putBoolean("disable_character_fade",Boolean.parseBoolean(arg)).commit();return Bundle.EMPTY;
        }
        if("set_skip_team_loading".equals(method)&&(Binder.getCallingUid()==0||Binder.getCallingUid()==Process.myUid())){
            if(!"true".equals(arg)&&!"false".equals(arg))throw new IllegalArgumentException("Expected true or false");
            Settings.preferences(getContext()).edit().putBoolean("skip_team_loading",Boolean.parseBoolean(arg)).commit();return Bundle.EMPTY;
        }
        if("set_enabled".equals(method)&&(Binder.getCallingUid()==0||Binder.getCallingUid()==Process.myUid())){
            if(!"true".equals(arg)&&!"false".equals(arg))throw new IllegalArgumentException("Expected true or false");
            Settings.preferences(getContext()).edit().putBoolean("enabled",Boolean.parseBoolean(arg)).commit();return Bundle.EMPTY;
        }
        if("recalculate_look".equals(method)&&(Binder.getCallingUid()==0||Binder.getCallingUid()==Process.myUid())){
            lookRefreshToken=System.currentTimeMillis();Bundle result=new Bundle();result.putLong("token",lookRefreshToken);return result;
        }
        if("graphics_config".equals(method)&&(Binder.getCallingUid()==0||Binder.getCallingUid()==Process.myUid())){
            SharedPreferences p=Settings.preferences(getContext());Bundle result=new Bundle();
            for(String key:GraphicsSettings.KEYS){
                if(!p.contains(key))continue;
                if("graphics_enabled".equals(key)||"render_resolution".equals(key)||"effect_enhance".equals(key))result.putBoolean(key,p.getBoolean(key,false));
                else result.putInt(key,p.getInt(key,0));
            }result.putInt("render_path",p.getInt("render_path",3));return result;
        }
        if("set_graphics".equals(method)&&(Binder.getCallingUid()==0||Binder.getCallingUid()==Process.myUid())&&extras!=null){
            SharedPreferences p=Settings.preferences(getContext());int[] n=GraphicsSettings.screen(getContext());
            int w=extras.getInt("render_width",p.getInt("render_width",n[0]&~3)),h=extras.getInt("render_height",p.getInt("render_height",n[1]&~3));
            int fps=extras.getInt("target_fps",p.getInt("target_fps",0)),veg=extras.getInt("veg_percent",p.getInt("veg_percent",100));
            boolean res=extras.getBoolean("render_resolution",p.getBoolean("render_resolution",false));
            String error=GraphicsSettings.validate(w,h,n[0],n[1],fps,veg,res);if(error!=null)throw new IllegalArgumentException(error);
            p.edit().putBoolean("graphics_enabled",extras.getBoolean("graphics_enabled",p.getBoolean("graphics_enabled",false))).putBoolean("render_resolution",res).putInt("render_width",w).putInt("render_height",h).putInt("target_fps",fps).putInt("veg_percent",veg).putBoolean("effect_enhance",extras.getBoolean("effect_enhance",p.getBoolean("effect_enhance",false))).commit();return Bundle.EMPTY;
        }
        if(("set_render_path".equals(method)||"capture_render".equals(method)||"render_probe".equals(method))&&(Binder.getCallingUid()==0||Binder.getCallingUid()==Process.myUid())){
            Bundle result=new Bundle();
            if("set_render_path".equals(method)){
                int path=Integer.parseInt(arg);if(path<0||path>3)throw new IllegalArgumentException("Expected 0, 1, 2 or 3");
                Settings.preferences(getContext()).edit().putInt("render_path",path).commit();
            }else if("capture_render".equals(method)){renderCaptureToken=System.currentTimeMillis();lastRenderProbe="{}";result.putLong("token",renderCaptureToken);}
            else result.putString("probe",lastRenderProbe);
            return result;
        }
        if("set_focus_guard".equals(method)&&(Binder.getCallingUid()==0||Binder.getCallingUid()==Process.myUid())){
            if(!"true".equals(arg)&&!"false".equals(arg))throw new IllegalArgumentException("Expected true or false");
            Settings.preferences(getContext()).edit().putBoolean("focus_guard",Boolean.parseBoolean(arg)).apply();return Bundle.EMPTY;
        }
        if("set_hide_uid".equals(method) && (Binder.getCallingUid()==0 || Binder.getCallingUid()==Process.myUid())){
            if(!"true".equals(arg)&&!"false".equals(arg))throw new IllegalArgumentException("Expected true or false");
            Settings.preferences(getContext()).edit().putBoolean("hide_uid",Boolean.parseBoolean(arg)).apply();return Bundle.EMPTY;
        }
        checkCaller();
        if("report".equals(method)&&extras!=null){int[] fade=extras.getIntArray("avatarFadeState");if(fade!=null&&fade.length==8)lastAvatarFadeState=fade;}
        if("report".equals(method)&&extras!=null){int[] team=extras.getIntArray("teamLoadingState");if(team!=null&&team.length==7)lastTeamLoadingState=team;}
        if("report".equals(method)&&extras!=null){int[] look=extras.getIntArray("lookState");if(look!=null&&look.length==12)lastLookState=look;}
        if("report".equals(method)&&extras!=null){int[] touch=extras.getIntArray("touchLookState");if(touch!=null&&touch.length==3)lastTouchLookState=touch;}
        if("report".equals(method)&&extras!=null){int[] map=extras.getIntArray("mapMouseState");if(map!=null&&map.length==6)lastMapMouseState=map;}
        if("report".equals(method)&&extras!=null){String probe=extras.getString("renderProbe");if(probe!=null&&probe.length()<=700000)lastRenderProbe=probe;}
        if("report".equals(method)&&extras!=null){int[] render=extras.getIntArray("renderState");if(render!=null&&render.length==12)lastRenderState=render;int[] photo=extras.getIntArray("photoState");if(photo!=null&&photo.length==10)lastPhotoState=photo;}
        if("report".equals(method)&&extras!=null){int[] graphics=extras.getIntArray("graphicsState");if(graphics!=null&&graphics.length==21)lastGraphicsState=graphics;}
        if("report".equals(method)&&extras!=null){int[] expedition=extras.getIntArray("expeditionState");if(expedition!=null&&expedition.length==6)lastExpeditionState=expedition;}
        if("report".equals(method)&&extras!=null){int[] qr=extras.getIntArray("qrState");if(qr!=null&&qr.length==5)lastQrState=qr;}
        if("report".equals(method) && extras!=null){int[] state=extras.getIntArray("state");if(state!=null&&state.length<=10){lastState=state;lastReport=System.currentTimeMillis();}}
        if("report".equals(method) && extras!=null){int[] uid=extras.getIntArray("uidState");if(uid!=null&&uid.length==5)lastUidState=uid;}
        if("report".equals(method)&&extras!=null){int[] daily=extras.getIntArray("dailyState");if(daily!=null&&daily.length==6)lastDailyState=daily;}
        if("report".equals(method) && extras!=null){int[] craft=extras.getIntArray("craftState");if(craft!=null&&craft.length==6)lastCraftState=craft;}
        if("report".equals(method) && extras!=null){
            int[] focus=extras.getIntArray("focusState"),window=extras.getIntArray("windowState");
            if(focus!=null&&focus.length==9)lastFocusState=focus;
            if(window!=null&&window.length==10)lastWindowState=window;
        }
        return Bundle.EMPTY;
    }
    @Override public String getType(Uri uri){return "vnd.android.cursor.item/vnd.giinput.config";}
    @Override public Uri insert(Uri uri,ContentValues v){throw new UnsupportedOperationException();}
    @Override public int update(Uri uri,ContentValues v,String s,String[] a){throw new UnsupportedOperationException();}
    @Override public int delete(Uri uri,String s,String[] a){throw new UnsupportedOperationException();}
}
