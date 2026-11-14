package com.qi.vovotool;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

public final class XposedEntry implements IXposedHookLoadPackage {
    private static boolean installed;
    @Override public void handleLoadPackage(XC_LoadPackage.LoadPackageParam load) {
        if(!Settings.GAME.equals(load.packageName) || !Settings.GAME.equals(load.processName))return;
        XposedHelpers.findAndHookMethod(Application.class,"attach",Context.class,new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam param) {
                if(installed)return;
                Context context=(Context)param.args[0];
                try {
                    PackageInfo info=context.getPackageManager().getPackageInfo(Settings.GAME,0);
                    if(!Settings.VERSION.equals(info.versionName)) {
                        XposedBridge.log("vovotool: unsupported game version "+info.versionName);
                        Bundle report=new Bundle();report.putIntArray("state",new int[]{-10});
                        context.getContentResolver().call(Settings.URI,"report",null,report);return;
                    }
                    NativeBridge.load(context);installJavaHooks(context.getClassLoader());QrLoginBridge.install(context.getClassLoader());installed=true;
                    XposedBridge.log("vovotool: module loaded for "+info.versionName);
                } catch(Throwable e) { XposedBridge.log(e); }
            }
        });
    }
    private static void installJavaHooks(ClassLoader loader) throws Exception {
        Class<?> owner=XposedHelpers.findClass("com.miHoYo.GetMobileInfo.MainActivity",loader);
        while(owner!=null){
            try {
                java.lang.reflect.Method dispatch=owner.getDeclaredMethod("dispatchKeyEvent",KeyEvent.class);
                XposedBridge.hookMethod(dispatch,new XC_MethodHook(){
                    @Override protected void beforeHookedMethod(MethodHookParam p){
                        if(CaptureController.onKey((Activity)p.thisObject,(KeyEvent)p.args[0]))p.setResult(true);
                    }
                });
                break;
            }catch(NoSuchMethodException e){owner=owner.getSuperclass();}
        }
        Class<?> gameActivity=XposedHelpers.findClass("com.miHoYo.GetMobileInfo.MainActivity",loader);
        XposedBridge.hookMethod(inheritedMethod(gameActivity,"dispatchTouchEvent",MotionEvent.class),new XC_MethodHook(){
            @Override protected void beforeHookedMethod(MethodHookParam p){
                if(CaptureController.onTouch((Activity)p.thisObject,(MotionEvent)p.args[0]))p.setResult(true);
            }
        });
        XposedBridge.hookMethod(inheritedMethod(gameActivity,"onWindowFocusChanged",boolean.class),new XC_MethodHook(){
            @Override protected void beforeHookedMethod(MethodHookParam p){CaptureController.onWindowFocus((Activity)p.thisObject,(Boolean)p.args[0]);}
        });
        XposedHelpers.findAndHookMethod(Activity.class,"onResume",new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam p){
                Activity a=(Activity)p.thisObject;
                if(Settings.GAME.equals(a.getPackageName()) && a.getClass().getName().contains("MainActivity"))CaptureController.connect(a);
            }
        });
        XposedHelpers.findAndHookMethod(Activity.class,"onDestroy",new XC_MethodHook(){
            @Override protected void afterHookedMethod(MethodHookParam p){CaptureController.disconnect((Activity)p.thisObject);}
        });
        XC_MethodHook motion=new XC_MethodHook(){
            @Override protected void beforeHookedMethod(MethodHookParam p){if(CaptureController.onMotion((MotionEvent)p.args[0]))p.setResult(true);}
        };
        XposedHelpers.findAndHookMethod(ViewGroup.class,"dispatchCapturedPointerEvent",MotionEvent.class,motion);
        XposedHelpers.findAndHookMethod(View.class,"dispatchCapturedPointerEvent",MotionEvent.class,motion);
    }
    private static java.lang.reflect.Method inheritedMethod(Class<?> owner,String name,Class<?>... types) throws NoSuchMethodException {
        while(owner!=null){try{return owner.getDeclaredMethod(name,types);}catch(NoSuchMethodException e){owner=owner.getSuperclass();}}
        throw new NoSuchMethodException(name);
    }
}
