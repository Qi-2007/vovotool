package com.qi.vovotool;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.view.Display;

/** HyperOS detection and physical display capability, shared by the module UI and PowerKeeper. */
final class HyperOs {
    private static Boolean detected;
    private static int panelMax;
    private HyperOs() {}
    static boolean detected(){
        Boolean cached=detected;if(cached!=null)return cached.booleanValue();
        boolean value=detect();detected=Boolean.valueOf(value);return value;
    }
    private static boolean detect(){
        // HyperOS 1.0+ exposes ro.mi.os.version.name; MIUI 14 and earlier only have the MIUI code.
        if(property("ro.mi.os.version.name").length()>0)return true;
        try{
            if(Integer.parseInt(property("ro.miui.ui.version.code"))>=816)return true;
        }catch(NumberFormatException ignored){}
        String incremental=android.os.Build.VERSION.INCREMENTAL;
        return incremental!=null&&incremental.startsWith("OS");
    }
    private static String property(String key){
        try{
            Class<?> properties=Class.forName("android.os.SystemProperties");
            Object value=properties.getMethod("get",String.class).invoke(null,key);
            return value instanceof String?(String)value:"";
        }catch(Throwable error){return "";}
    }
    /** Highest supported refresh rate in Hz, or 0 when the display cannot be queried. */
    static int panelMaxHz(Context context){
        if(context==null)return 0;
        int cached=panelMax;if(cached>0)return cached;
        int value=0;
        try{
            DisplayManager manager=(DisplayManager)context.getSystemService(Context.DISPLAY_SERVICE);
            Display display=manager==null?null:manager.getDisplay(Display.DEFAULT_DISPLAY);
            if(display!=null){
                for(Display.Mode mode:display.getSupportedModes())value=Math.max(value,Math.round(mode.getRefreshRate()));
                if(value<=0)value=Math.round(display.getMode().getRefreshRate());
            }
        }catch(Throwable ignored){}
        if(value>0)panelMax=value;
        return value;
    }
}
