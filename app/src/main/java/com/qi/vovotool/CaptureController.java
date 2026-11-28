package com.qi.vovotool;

import android.app.Activity;
import android.database.Cursor;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.InputDevice;
import android.view.View;
import android.view.SurfaceView;
import android.graphics.Rect;

public final class CaptureController {
    private static CaptureController current;
    private final Activity activity;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final HandlerThread thread=new HandlerThread("vovotoolConfig");
    private Handler background;
    private View view;
    private boolean stopped;
    private volatile boolean enabled=true, capture=true;
    private volatile int craftKey=Shortcut.DEFAULT_KEY,craftModifiers;
    private volatile int dailyKey=Shortcut.DEFAULT_DAILY_KEY,dailyModifiers;
    private volatile int expeditionKey=Shortcut.DEFAULT_EXPEDITION_KEY,expeditionModifiers;
    private int heldExpeditionKey;
    private int heldCraftKey,heldDailyKey;
    private int lastDailyNoticeRequests,lastDailyNoticeStatus;
    private long lastDailyNoticeAt;
    private String lastRenderProbe="{}";
    private volatile boolean focusGuard=true;
    private boolean windowFocused,focusTouchArmed,swallowTouch;
    private volatile boolean altHeld;            // ALT 临时释放鼠标
    private volatile boolean imeVisible;         // 软键盘弹出（聊天输入框激活）
    private volatile boolean chatOpen;           // 聊天框打开（回车触发的显式状态）
    private boolean chatFocusLeft;              // 聊天期间焦点是否曾离开游戏画面
    private long focusGainedAt;
    private volatile int focusLosses,focusGains,swallowedGestures,swallowedEvents,touchDowns,lastTouchSource,lastTouchAction;
    private long fingerDown;
    private float fingerTravel;
    private boolean tapping;

    private CaptureController(Activity activity) {
        this.activity=activity;
        windowFocused=activity.hasWindowFocus();focusTouchArmed=!windowFocused;
        thread.start(); background=new Handler(thread.getLooper());
        background.post(configLoop); main.post(captureLoop);
    }
    public static synchronized void connect(Activity activity) {
        if (!Settings.isGame(activity.getPackageName())) return;
        if (current!=null && current.activity==activity) return;
        if (current!=null) current.stop();
        current=new CaptureController(activity);
    }
    public static synchronized void disconnect(Activity activity) {
        if(current!=null && current.activity==activity){current.stop();current=null;}
    }
    private void stop() {
        stopped=true;main.removeCallbacksAndMessages(null);background.removeCallbacksAndMessages(null);
        if(view!=null && view.hasPointerCapture()) view.releasePointerCapture();
        NativeBridge.setCaptureState(false,false);thread.quitSafely();
    }
    private final Runnable configLoop=new Runnable(){public void run(){
        if(stopped)return;
        try(Cursor c=activity.getContentResolver().query(Settings.URI,null,null,null,null)) {
            if(c==null||!c.moveToFirst())throw new IllegalStateException("Module configuration provider unavailable");
            {
                enabled=c.getInt(c.getColumnIndexOrThrow("enabled"))!=0;
                capture=c.getInt(c.getColumnIndexOrThrow("capture"))!=0;
                GameConfiguration.apply(activity,c);
                focusGuard=c.getInt(c.getColumnIndexOrThrow("focus_guard"))!=0;
                craftKey=c.getInt(c.getColumnIndexOrThrow("craft_key"));
                craftModifiers=c.getInt(c.getColumnIndexOrThrow("craft_meta"));
                dailyKey=c.getInt(c.getColumnIndexOrThrow("daily_key"));
                dailyModifiers=c.getInt(c.getColumnIndexOrThrow("daily_meta"));
                expeditionKey=c.getInt(c.getColumnIndexOrThrow("expedition_key"));
                expeditionModifiers=c.getInt(c.getColumnIndexOrThrow("expedition_meta"));
                NativeBridge.requestRenderCapture(c.getLong(c.getColumnIndexOrThrow("render_capture")));
                NativeBridge.requestLookRefresh(c.getLong(c.getColumnIndexOrThrow("look_refresh")));
            }
            Bundle report=new Bundle();report.putIntArray("state",NativeBridge.state());
            report.putIntArray("regionUnlockState",NativeBridge.regionUnlockState());
            report.putIntArray("graphicsState",NativeBridge.graphicsState());
            report.putIntArray("renderState",NativeBridge.renderState());
            report.putIntArray("photoState",NativeBridge.photoState());
            report.putIntArray("lookState",NativeBridge.lookState());
            report.putIntArray("touchLookState",NativeBridge.touchLookState());
            report.putIntArray("mapMouseState",NativeBridge.mapMouseState());
            report.putIntArray("teamLoadingState",NativeBridge.teamLoadingState());
            report.putIntArray("avatarFadeState",NativeBridge.avatarFadeState());
            report.putIntArray("avatarSwitchState",NativeBridge.avatarSwitchState());
            report.putIntArray("avatarSwitchTraceState",NativeBridge.avatarSwitchTraceState());
            String probe=NativeBridge.renderProbe();if(!"{}".equals(probe)&&!probe.equals(lastRenderProbe)){report.putString("renderProbe",probe);lastRenderProbe=probe;}
            report.putIntArray("qrState",QrLoginBridge.state());
            report.putIntArray("uidState",NativeBridge.uidState());
            report.putIntArray("craftState",NativeBridge.craftState());
            report.putIntArray("expeditionState",NativeBridge.expeditionState());
            int[] daily=NativeBridge.dailyState();report.putIntArray("dailyState",daily);
            showDailyNotice(daily);
            report.putIntArray("focusState",NativeBridge.focusState());
            report.putIntArray("windowState",new int[]{focusLosses,focusGains,touchDowns,swallowedGestures,swallowedEvents,windowFocused?1:0,swallowTouch?1:0,focusGuard?1:0,lastTouchSource,lastTouchAction});
            activity.getContentResolver().call(Settings.URI,"report",null,report);
        } catch(Exception e) { main.post(()->{if(!stopped)Settings.requestConfigurationWakeup(activity);});Log.w("vovotool","Module configuration unavailable",e); }
        background.postDelayed(this,1000);
    }};
    private void showDailyNotice(int[] value){
        if(value.length!=6)return;
        boolean changed=value[2]!=lastDailyNoticeRequests||value[0]!=lastDailyNoticeStatus;
        lastDailyNoticeRequests=value[2];lastDailyNoticeStatus=value[0];
        if(!changed||value[2]==0)return;
        String message;
        switch(value[0]){
            case 4:message="日委奖励已领取，无需重复提交";break;
            case -1:message="请回到大世界可操作状态后领取日委";break;
            case -2:message="日委模块尚未就绪，本次未执行";break;
            case -3:message="未确认领取结果，请查看游戏提示；未自动重试";break;
            case -4:message="操作过快，请稍后再试";break;
            default:return;
        }
        long now=SystemClock.elapsedRealtime();if(now-lastDailyNoticeAt<1500)return;
        lastDailyNoticeAt=now;
        main.post(()->{if(!stopped&&activity.hasWindowFocus())android.widget.Toast.makeText(activity,message,android.widget.Toast.LENGTH_SHORT).show();});
    }
    private final Runnable captureLoop=new Runnable(){public void run(){
        if(stopped)return;
        try {
            View focused=activity.getWindow().getDecorView().findFocus();
            if(focused!=null && focused!=view) {
                if(view!=null && view.hasPointerCapture())view.releasePointerCapture();
                view=focused;
            }
            int[] state=NativeBridge.state();
            boolean focus=view!=null && view.isAttachedToWindow() && view.hasWindowFocus() && !activity.isFinishing();
            updateFocus(focus);
            // 软键盘是否弹出：可见显示区域高度明显小于窗口高度即认为输入法占据屏幕。
            boolean ime=false;
            try{
                Rect r=new Rect();
                View root=activity.getWindow().getDecorView();
                root.getWindowVisibleDisplayFrame(r);
                int screenH=root.getHeight();
                ime=screenH>0 && (screenH-r.bottom)>screenH/5;
            }catch(Throwable ignored){}
            imeVisible=ime;
            // 两段式焦点检测关聊天框：必须先看焦点离开游戏画面(聊天 UI 出现)，
            // 再看焦点回到 SurfaceView(聊天框关闭)，才恢复独占。
            // 避免刚打开时焦点还在 SurfaceView 就误判关闭。
            if(chatOpen){
                if(view!=null && !(view instanceof SurfaceView))chatFocusLeft=true;
                else if(chatFocusLeft && view instanceof SurfaceView){
                    chatOpen=false;chatFocusLeft=false;
                    Log.i("vovotool","Chat closed by focus return");
                }
            }
            boolean desired=enabled && capture && state[0]==2 && state[1]==2 && state[2]==-1 && focus
                    && !altHeld && !imeVisible && !chatOpen;
            if(view!=null) {
                if(desired && !view.hasPointerCapture())view.requestPointerCapture();
                else if(!desired && view.hasPointerCapture())view.releasePointerCapture();
            }
            boolean actual=view!=null && view.hasPointerCapture();
            NativeBridge.setCaptureState(actual,focus);
        } catch(Exception e) { Log.w("vovotool","Pointer capture update failed: "+e.getClass().getSimpleName()); }
        main.postDelayed(this,50);
    }};
    private void updateFocus(boolean focused){
        if(windowFocused==focused)return;
        windowFocused=focused;
        if(focused){
            focusGains++;focusGainedAt=SystemClock.uptimeMillis();
            final long gained=focusGainedAt;
            main.postDelayed(()->{if(windowFocused&&focusGainedAt==gained&&!swallowTouch)focusTouchArmed=false;},180);
        }else {focusLosses++;focusTouchArmed=true;heldCraftKey=heldDailyKey=0;}
        NativeBridge.setCaptureState(false,focused);
        Log.i("vovotool","Game window focus="+focused);
    }
    public static void onWindowFocus(Activity activity,boolean focused){
        CaptureController c=current;if(c!=null&&c.activity==activity&&!c.stopped)c.updateFocus(focused);
    }
    // 地图页(cursor=55 的 UI 页面)把鼠标绝对事件改写成触摸事件，
    // 让游戏的拖动手势识别生效（游戏只认 SOURCE_TOUCHSCREEN 的拖动）。
    // 返回 null 表示无需转换。用反射设置 mSource，兼容各 Android 版本。
    public static MotionEvent mouseToTouch(MotionEvent e){
        if(e==null||e.isFromSource(InputDevice.SOURCE_TOUCHSCREEN))return null;
        int src=e.getSource();
        if((src&0x2002)!=0x2002)return null;   // 只处理绝对鼠标 0x2002
        int[] st=NativeBridge.state();
        if(st[2]==-1)return null;              // 大世界鼠标独占时不转（转视角走另一条路）
        int action=e.getActionMasked();
        if(action!=MotionEvent.ACTION_DOWN && action!=MotionEvent.ACTION_MOVE &&
           action!=MotionEvent.ACTION_UP && action!=MotionEvent.ACTION_CANCEL)return null;
        try{
            MotionEvent t=MotionEvent.obtain(e.getDownTime(),e.getEventTime(),action,
                    e.getX(),e.getY(),e.getMetaState());
            try{
                java.lang.reflect.Field f=MotionEvent.class.getDeclaredField("mSource");
                f.setAccessible(true);f.setInt(t,InputDevice.SOURCE_TOUCHSCREEN);
            }catch(Throwable ignored){
                try{t.setSource(InputDevice.SOURCE_TOUCHSCREEN);}catch(Throwable ignored2){}
            }
            return t;
        }catch(Throwable ex){Log.w("vovotool","mouseToTouch failed: "+ex.getClass().getSimpleName());return null;}
    }

    public static boolean onTouch(Activity activity,MotionEvent event){
        CaptureController c=current;
        if(c==null||c.activity!=activity||c.stopped)return false;
        int action=event.getActionMasked();
        // 聊天框打开时任意点击(鼠标/触摸)界面外 → 延迟检查软键盘未弹、焦点不在输入框 → 恢复独占。
        // 必须放在 SOURCE_TOUCHSCREEN 过滤之前：鼠标点击(source=0x2002)走这里，不是触摸。
        if(c.chatOpen && action==MotionEvent.ACTION_UP){
            c.main.postDelayed(()->{
                if(c.stopped||!c.chatOpen)return;
                View f=activity.getWindow().getDecorView().findFocus();
                boolean editing=f!=null && f.onCheckIsTextEditor();
                if(!c.imeVisible && !editing){
                    c.chatOpen=false;c.chatFocusLeft=false;
                    Log.i("vovotool","Chat closed by outside click");
                }
            },500);
        }
        if(!event.isFromSource(InputDevice.SOURCE_TOUCHSCREEN))return false;
        c.lastTouchSource=event.getSource();c.lastTouchAction=action;
        if(action==MotionEvent.ACTION_DOWN){
            c.touchDowns++;
            boolean refocus=c.focusTouchArmed&&(!c.windowFocused||!activity.hasWindowFocus()||event.getDownTime()<=c.focusGainedAt+180);
            if(c.focusGuard&&c.enabled&&refocus){
                int[] state=NativeBridge.state();
                if(state[0]==2&&state[1]==2){c.swallowTouch=true;c.swallowedGestures++;Log.i("vovotool","Touch gesture used only to refocus the PC game window");}
            }
            c.focusTouchArmed=false;
        }
        if(!c.swallowTouch){NativeBridge.noteTouch(action);return false;}
        c.swallowedEvents++;
        if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_CANCEL)c.swallowTouch=false;
        return true;
    }
    public static boolean onMotion(MotionEvent event) {
        CaptureController c=current;
        if(c==null || c.stopped || !c.enabled || !c.capture || c.view==null)return false;
        int source=event.getSource();
        boolean pad=(source&0x100008)==0x100008;
        if(!pad && (source&0x20004)!=0x20004)return false;
        int[] state=NativeBridge.state();if(state[0]!=2 || state[2]!=-1)return false;
        int action=event.getActionMasked(),count=event.getPointerCount();
        float x=0,y=0,vertical=0,horizontal=0;
        if(action==MotionEvent.ACTION_MOVE || action==MotionEvent.ACTION_HOVER_MOVE){
            int history=event.getHistorySize();
            if(pad){
                for(int n=0;n<history;n++){x+=event.getHistoricalAxisValue(27,0,n);y+=event.getHistoricalAxisValue(28,0,n);}
                x+=event.getAxisValue(27,0);y+=event.getAxisValue(28,0);
            }else{
                for(int n=0;n<history;n++){x+=event.getHistoricalX(0,n);y+=event.getHistoricalY(0,n);}
                x+=event.getX(0);y+=event.getY(0);
            }
            c.fingerTravel+=Math.abs(x)+Math.abs(y);
            if(count!=1)x=y=0;
        }
        if(action==MotionEvent.ACTION_SCROLL){vertical=event.getAxisValue(9);horizontal=event.getAxisValue(10);}
        int buttons=event.getButtonState();
        if(pad && action==MotionEvent.ACTION_DOWN){c.fingerDown=event.getEventTime();c.fingerTravel=0;}
        if(pad && action==MotionEvent.ACTION_UP && event.getEventTime()-c.fingerDown<200 && c.fingerTravel<6){
            buttons|=1;c.tapping=true;
            c.main.postDelayed(()->{c.tapping=false;NativeBridge.motion(0,0,0,0,0);},70);
        }
        if(c.tapping)buttons|=1;
        NativeBridge.motion(x,-y,vertical,horizontal,buttons);
        return true;
    }
    public static boolean onKey(Activity activity, KeyEvent event) {
        CaptureController c=current;
        if(c==null || c.activity!=activity || c.stopped || !c.enabled || !activity.hasWindowFocus())return false;
        int action=event.getAction();
        if(action!=KeyEvent.ACTION_DOWN && action!=KeyEvent.ACTION_UP)return false;
        int code=event.getKeyCode();
        // ALT：按住期间临时释放鼠标独占，松开恢复（游戏照常处理 ALT，不消费事件）。
        if(code==KeyEvent.KEYCODE_ALT_LEFT || code==KeyEvent.KEYCODE_ALT_RIGHT){
            c.altHeld=(action==KeyEvent.ACTION_DOWN);
            Log.i("vovotool","ALT held="+c.altHeld);
            return false;
        }
        // 回车：大世界按下即视为打开聊天框，释放光标（游戏照常处理，不消费）。
        if(action==KeyEvent.ACTION_DOWN &&
                (code==KeyEvent.KEYCODE_ENTER || code==KeyEvent.KEYCODE_NUMPAD_ENTER) && !c.chatOpen){
            int[] st=NativeBridge.state();
            if(st[0]==2 && st[1]==2 && st[2]==-1){
                c.chatOpen=true;
                c.chatFocusLeft=false;
                Log.i("vovotool","Chat opened by Enter");
            }
        }
        // ESC：关闭聊天框，恢复独占。
        if(action==KeyEvent.ACTION_DOWN && code==KeyEvent.KEYCODE_ESCAPE && c.chatOpen){
            c.chatOpen=false;c.chatFocusLeft=false;
            Log.i("vovotool","Chat closed by Esc");
        }
        // Some keyboard layouts label the physical Esc key as Android Back.
        if(code==KeyEvent.KEYCODE_BACK && event.isFromSource(InputDevice.SOURCE_KEYBOARD) && event.getScanCode()==1)code=KeyEvent.KEYCODE_ESCAPE;
        if(c.heldCraftKey!=0 && code==c.heldCraftKey){
            if(action==KeyEvent.ACTION_UP){c.heldCraftKey=0;NativeBridge.craftKey(false);}
            return true;
        }
        if(c.heldDailyKey!=0 && code==c.heldDailyKey){
            if(action==KeyEvent.ACTION_UP){c.heldDailyKey=0;NativeBridge.dailyKey(false);}
            return true;
        }
        if(c.heldExpeditionKey!=0 && code==c.heldExpeditionKey){
            if(action==KeyEvent.ACTION_UP){c.heldExpeditionKey=0;NativeBridge.expeditionKey(false);}
            return true;
        }
        View focus=activity.getCurrentFocus();
        if(focus!=null && focus.onCheckIsTextEditor())return false;
        if(action==KeyEvent.ACTION_DOWN && code==c.craftKey && Shortcut.modifiers(event)==c.craftModifiers){
            boolean handled=NativeBridge.craftKey(true);
            if(handled){c.heldCraftKey=code;return true;}
        }
        if(action==KeyEvent.ACTION_DOWN && code==c.dailyKey && Shortcut.modifiers(event)==c.dailyModifiers){
            boolean handled=NativeBridge.dailyKey(true);
            if(handled){c.heldDailyKey=code;return true;}
        }
        if(action==KeyEvent.ACTION_DOWN && code==c.expeditionKey && Shortcut.modifiers(event)==c.expeditionModifiers){
            boolean handled=NativeBridge.expeditionKey(true);
            if(handled){c.heldExpeditionKey=code;return true;}
        }
        if(code!=KeyEvent.KEYCODE_ENTER && code!=KeyEvent.KEYCODE_NUMPAD_ENTER && code!=KeyEvent.KEYCODE_ESCAPE)return false;
        boolean handled=NativeBridge.key(code,action==KeyEvent.ACTION_DOWN);
        if(handled && event.getRepeatCount()==0)Log.i("vovotool","Keyboard bridge code="+code+" action="+action+" scan="+event.getScanCode());
        return handled;
    }
}
