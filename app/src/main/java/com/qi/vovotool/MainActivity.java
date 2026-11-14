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
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

public final class MainActivity extends Activity {
    private final Handler handler=new Handler(Looper.getMainLooper());
    private TextView state,rootState,uidState;
    private EditText mouse,touchpad;
    private SharedPreferences prefs;
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
        Switch hideUid=new Switch(this);hideUid.setText("隐藏右下角 UID");hideUid.setTextColor(Color.WHITE);hideUid.setChecked(prefs.getBoolean("hide_uid",true));hideUid.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("hide_uid",c).apply());layout.addView(hideUid);
        uidState=text(layout,"等待 UID 显示组件",14);
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
                case 2:uidState.setText("右下角 UID 已隐藏");break;
                case 3:uidState.setText("UID 显示已恢复");break;
                case -2:uidState.setText("UID 文字校验失败，本次停止调整");break;
                case 1:uidState.setText("等待 UID 显示组件");break;
                default:uidState.setText("UID 隐藏未启用");
            }
        }else uidState.setText("等待 UID 显示组件");
        rootState.setText(RootProxyService.status);handler.postDelayed(this,1000);
    }};
    @Override public void onResume(){super.onResume();Settings.grantGameAccess(this);handler.post(refresh);}
    @Override public void onPause(){handler.removeCallbacks(refresh);super.onPause();}
}
