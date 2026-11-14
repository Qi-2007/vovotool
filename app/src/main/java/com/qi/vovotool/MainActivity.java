package com.qi.vovotool;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.View;
import android.view.KeyEvent;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

public final class MainActivity extends Activity {
    private final Handler handler=new Handler(Looper.getMainLooper());
    private TextView state,rootState,uidState,craftState,dailyState;
    private EditText mouse,touchpad;
    private SharedPreferences prefs;
    private Button shortcutButton,dailyShortcutButton;
    private boolean recordingShortcut,recordingDaily;
    private int recordedRelease;
    private String craftShortcut(){return Shortcut.label(prefs.getInt("craft_key",Shortcut.DEFAULT_KEY),prefs.getInt("craft_meta",0));}
    private String dailyShortcut(){return Shortcut.label(prefs.getInt("daily_key",Shortcut.DEFAULT_DAILY_KEY),prefs.getInt("daily_meta",0));}
    private void resetShortcutButtons(){
        if(shortcutButton!=null)shortcutButton.setText("设置快捷键 · "+craftShortcut());
        if(dailyShortcutButton!=null)dailyShortcutButton.setText("设置快捷键 · "+dailyShortcut());
    }
    private void recordShortcut(boolean daily){
        resetShortcutButtons();recordingShortcut=true;recordingDaily=daily;
        (daily?dailyShortcutButton:shortcutButton).setText("请按快捷键（Esc 取消）");
    }
    private int dp(int value){return (int)(getResources().getDisplayMetrics().density*value+.5f);}
    private TextView text(LinearLayout parent,String value,int size){TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(Color.rgb(225,232,241));t.setPadding(0,dp(8),0,dp(8));parent.addView(t);return t;}
    private Button button(LinearLayout parent,String title,View.OnClickListener action){Button b=new Button(this);b.setText(title);b.setOnClickListener(action);parent.addView(b,new LinearLayout.LayoutParams(-1,dp(54)));return b;}
    private EditText number(LinearLayout parent,String title,float value){text(parent,title,17);EditText e=new EditText(this);e.setText(Float.toString(value));e.setTextColor(Color.WHITE);e.setInputType(InputType.TYPE_CLASS_NUMBER|InputType.TYPE_NUMBER_FLAG_DECIMAL);parent.addView(e);return e;}
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);prefs=Settings.preferences(this);Settings.grantGameAccess(this);
        ScrollView scroll=new ScrollView(this);scroll.setBackgroundColor(Color.rgb(19,28,42));
        LinearLayout layout=new LinearLayout(this);layout.setOrientation(LinearLayout.VERTICAL);layout.setPadding(dp(24),dp(24),dp(24),dp(24));scroll.addView(layout);setContentView(scroll);
        text(layout,"沃沃头",28);
        text(layout,"在 LSPosed 中启用本模块，作用域只勾选「原神」。当前适配国服 7.2 ARM64。",16);
        state=text(layout,"等待游戏连接",17);
        Switch enabled=new Switch(this);enabled.setText("启用 PC 界面与原生键鼠");enabled.setTextColor(Color.WHITE);enabled.setChecked(prefs.getBoolean("enabled",true));enabled.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("enabled",c).apply());layout.addView(enabled);
        Switch capture=new Switch(this);capture.setText("镜头捕获指针，菜单自动释放");capture.setTextColor(Color.WHITE);capture.setChecked(prefs.getBoolean("capture",true));capture.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("capture",c).apply());layout.addView(capture);
        Switch hideUid=new Switch(this);hideUid.setText("隐藏 UID（右下角及派蒙菜单）");hideUid.setTextColor(Color.WHITE);hideUid.setChecked(prefs.getBoolean("hide_uid",true));hideUid.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("hide_uid",c).apply());layout.addView(hideUid);
        uidState=text(layout,"等待 UID 显示组件",14);
        Switch craft=new Switch(this);craft.setText("随身合成台");craft.setTextColor(Color.WHITE);craft.setChecked(prefs.getBoolean("portable_craft",true));craft.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("portable_craft",c).apply());layout.addView(craft);
        shortcutButton=button(layout,"设置快捷键 · "+craftShortcut(),v->recordShortcut(false));
        text(layout,"支持普通按键及 Ctrl、Alt、Shift 组合键。保存后约 1 秒在游戏中生效；在大世界可操作状态下使用。",14);
        craftState=text(layout,"随身合成台已就绪",14);
        Switch daily=new Switch(this);daily.setText("随身领取日委奖励");daily.setTextColor(Color.WHITE);daily.setChecked(prefs.getBoolean("portable_daily",true));daily.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("portable_daily",c).apply());layout.addView(daily);
        dailyShortcutButton=button(layout,"设置快捷键 · "+dailyShortcut(),v->recordShortcut(true));
        text(layout,"在大世界按快捷键领取已满足条件的日委奖励。若游戏弹出确认界面，请在游戏内确认；不会自动重试。",14);
        dailyState=text(layout,"日委领取等待游戏连接",14);
        mouse=number(layout,"镜头基础倍率（默认 0.02）",prefs.getFloat("mouse_scale",.02f));
        touchpad=number(layout,"触控板独立倍率（默认 0.25）",prefs.getFloat("touchpad_gain",.25f));
        button(layout,"保存速度",v->{try{
            float m=Float.parseFloat(mouse.getText().toString()),t=Float.parseFloat(touchpad.getText().toString());
            if(!Float.isFinite(m)||m<.0001f||m>2||!Float.isFinite(t)||t<.05f||t>2)throw new IllegalArgumentException();
            prefs.edit().putFloat("mouse_scale",m).putFloat("touchpad_gain",t).apply();Toast.makeText(this,"已保存，游戏内约 1 秒后生效",Toast.LENGTH_SHORT).show();
        }catch(Exception e){Toast.makeText(this,"基础倍率范围 0.0001～2；触控板倍率范围 0.05～2",Toast.LENGTH_LONG).show();}});
        text(layout,"触控板兼容",22);
        text(layout,"外接鼠标无需启动此项。Lenovo 键盘套触控板需要 root 授权；游戏菜单继续使用系统触控板。",16);
        rootState=text(layout,"未启动",16);
        button(layout,"启动触控板兼容",v->{
            if(Build.VERSION.SDK_INT>=33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},100);
            startForegroundService(new Intent(this,RootProxyService.class));
        });
        button(layout,"停止触控板兼容",v->stopService(new Intent(this,RootProxyService.class)));
        button(layout,"打开原神",v->{Intent game=getPackageManager().getLaunchIntentForPackage(Settings.GAME);if(game!=null)startActivity(game);else Toast.makeText(this,"未找到原神",Toast.LENGTH_SHORT).show();});
        text(layout,"关闭上方开关可恢复本次进程的输入基线；完整重载界面请重新启动游戏。模块不修改游戏文件或系统分区。",14);
    }
    private final Runnable refresh=new Runnable(){public void run(){
        int[] s=ConfigProvider.lastState;boolean fresh=System.currentTimeMillis()-ConfigProvider.lastReport<4000;
        if(fresh && s.length>0){
            switch(s[0]){
                case 2:state.setText("原神已连接 · 原生键鼠已启用");break;
                case 3:state.setText("原神已连接 · 已恢复输入基线");break;
                case -10:state.setText("当前原神版本不受支持，未应用修改");break;
                case -1:state.setText("游戏代码校验失败，未应用修改");break;
                case -2:case -3:state.setText("本次加载失败，请查看 LSPosed 日志");break;
                default:state.setText("模块已连接，等待游戏完成加载");
            }
        }else state.setText("等待原神连接 · 请确认 LSPosed 已启用");
        int[] u=ConfigProvider.lastUidState;
        if(fresh&&u.length==5){
            switch(u[0]){
                case 2:uidState.setText("已发现的 UID 文字已隐藏");break;
                case 3:uidState.setText("UID 显示已恢复");break;
                case -2:uidState.setText("UID 文字校验失败，本次停止调整");break;
                case 1:uidState.setText("等待 UID 显示组件");break;
                default:uidState.setText("UID 隐藏未启用");
            }
        }else uidState.setText("等待 UID 显示组件");
        int[] craft=ConfigProvider.lastCraftState;
        if(fresh&&craft.length==6){
            if(craft[1]==0)craftState.setText("随身合成台未启用");
            else switch(craft[0]){
                case 1:case 2:craftState.setText("正在打开合成界面");break;
                case 3:craftState.setText("上次已成功打开合成界面");break;
                case -1:craftState.setText("请回到大世界可操作状态后按 "+craftShortcut());break;
                case -2:craftState.setText("游戏未接受合成界面请求");break;
                case -3:craftState.setText("等待合成界面超时");break;
                default:craftState.setText("随身合成台已就绪 · "+craftShortcut());
            }
        }else craftState.setText("随身合成台等待游戏连接");
        int[] daily=ConfigProvider.lastDailyState;
        if(fresh&&daily.length==6){
            if(daily[1]==0)dailyState.setText("随身日委领取未启用");
            else switch(daily[0]){
                case 1:dailyState.setText("正在提交日委领取");break;
                case 2:dailyState.setText("已调用原生领取，请查看游戏提示或确认界面");break;
                case 3:dailyState.setText("已确认本次日委奖励领取");break;
                case 4:dailyState.setText("本次日委奖励已领取，无需重复提交");break;
                case -1:dailyState.setText("请回到大世界可操作状态后按 "+dailyShortcut());break;
                case -2:dailyState.setText("日委模块尚未就绪，本次未执行");break;
                case -3:dailyState.setText("未确认领取结果，请查看游戏提示；未自动重试");break;
                case -4:dailyState.setText("操作过快，请稍后再试");break;
                default:dailyState.setText("日委领取已就绪 · "+dailyShortcut());
            }
        }else dailyState.setText("日委领取等待游戏连接");
        rootState.setText(RootProxyService.status);handler.postDelayed(this,1000);
    }};
    @Override public void onResume(){super.onResume();Settings.grantGameAccess(this);handler.post(refresh);}
    @Override public boolean dispatchKeyEvent(KeyEvent event){
        int code=event.getKeyCode();
        if(event.getAction()==KeyEvent.ACTION_UP && recordedRelease==code){recordedRelease=0;return true;}
        if(!recordingShortcut)return super.dispatchKeyEvent(event);
        if(event.getAction()!=KeyEvent.ACTION_DOWN || KeyEvent.isModifierKey(code))return true;
        if(code==KeyEvent.KEYCODE_ESCAPE || code==KeyEvent.KEYCODE_BACK){
            recordingShortcut=false;recordedRelease=code;resetShortcutButtons();return true;
        }
        if(!Shortcut.supported(code)){Toast.makeText(this,"请换一个普通按键或组合键",Toast.LENGTH_SHORT).show();return true;}
        String target=recordingDaily?"daily":"craft",other=recordingDaily?"craft":"daily";
        int otherDefault=recordingDaily?Shortcut.DEFAULT_KEY:Shortcut.DEFAULT_DAILY_KEY,meta=Shortcut.modifiers(event);
        if(code==prefs.getInt(other+"_key",otherDefault)&&meta==(prefs.getInt(other+"_meta",0)&Shortcut.META_MASK)){
            recordedRelease=code;Toast.makeText(this,"与另一项功能的快捷键重复，请换一个",Toast.LENGTH_SHORT).show();return true;
        }
        prefs.edit().putInt(target+"_key",code).putInt(target+"_meta",meta).apply();
        recordingShortcut=false;recordedRelease=code;resetShortcutButtons();
        Toast.makeText(this,"已保存："+(recordingDaily?dailyShortcut():craftShortcut()),Toast.LENGTH_SHORT).show();return true;
    }
    @Override public void onPause(){
        recordingShortcut=false;recordedRelease=0;
        resetShortcutButtons();
        handler.removeCallbacks(refresh);super.onPause();
    }
}
