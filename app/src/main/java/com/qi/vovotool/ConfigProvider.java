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
    static volatile int[] lastState=new int[0];
    static volatile int[] lastUidState=new int[0];
    static volatile int[] lastCraftState=new int[0],lastDailyState=new int[0],lastQrState=new int[0];
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
        MatrixCursor result=new MatrixCursor(new String[]{"enabled","capture","mouse_scale","touchpad_gain","proxy_port","proxy_token","hide_uid","focus_guard","portable_craft","craft_key","craft_meta","portable_daily","daily_key","daily_meta","qr_login"});
        result.addRow(new Object[]{p.getBoolean("enabled",true)?1:0,p.getBoolean("capture",true)?1:0,p.getFloat("mouse_scale",0.02f),p.getFloat("touchpad_gain",0.25f),RootProxyService.port,RootProxyService.token,p.getBoolean("hide_uid",true)?1:0,p.getBoolean("focus_guard",true)?1:0,p.getBoolean("portable_craft",true)?1:0,p.getInt("craft_key",Shortcut.DEFAULT_KEY),p.getInt("craft_meta",0)&Shortcut.META_MASK,p.getBoolean("portable_daily",true)?1:0,p.getInt("daily_key",Shortcut.DEFAULT_DAILY_KEY),p.getInt("daily_meta",0)&Shortcut.META_MASK,p.getBoolean("qr_login",true)?1:0});
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
            result.putIntArray("focusState",lastFocusState);result.putIntArray("windowState",lastWindowState);
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
