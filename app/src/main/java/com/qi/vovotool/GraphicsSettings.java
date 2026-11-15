package com.qi.vovotool;

import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.Color;
import android.util.DisplayMetrics;
import android.view.WindowManager;
import android.widget.*;

final class GraphicsSettings {
    static final String[] KEYS={"graphics_enabled","render_resolution","render_width","render_height","target_fps","effect_enhance","veg_percent"};
    static final int[] FPS={0,30,45,60,90,120,144,165};
    static int[] screen(Context context){
        DisplayMetrics m=new DisplayMetrics();
        ((WindowManager)context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay().getRealMetrics(m);
        return new int[]{Math.max(m.widthPixels,m.heightPixels),Math.min(m.widthPixels,m.heightPixels)};
    }
    static void configure(Context context,Cursor c){
        int[] n=screen(context);
        NativeBridge.configureGraphics(c.getInt(c.getColumnIndexOrThrow("graphics_enabled"))!=0,
            c.getInt(c.getColumnIndexOrThrow("render_resolution"))!=0,c.getInt(c.getColumnIndexOrThrow("render_width")),
            c.getInt(c.getColumnIndexOrThrow("render_height")),n[0],n[1],c.getInt(c.getColumnIndexOrThrow("target_fps")),
            c.getInt(c.getColumnIndexOrThrow("effect_enhance"))!=0,c.getInt(c.getColumnIndexOrThrow("veg_percent")));
    }
    static void load(Context context){
        try(Cursor c=context.getContentResolver().query(Settings.URI,null,null,null,null)){
            if(c!=null&&c.moveToFirst())configure(context,c);
        }catch(Exception error){android.util.Log.w("vovotool","Graphics startup configuration unavailable",error);}
    }
    static boolean validFps(int v){for(int f:FPS)if(v==f)return true;return false;}
    static String validate(int w,int h,int nw,int nh,int fps,int vegetation,boolean resolution){
        if(!validFps(fps)||vegetation<10||vegetation>100)return "帧率或植被距离不在支持范围";
        if(resolution&&(w<1280||h<720||w>nw||h>nh||w%4!=0||h%4!=0))return "分辨率须为 4 的倍数，至少 1280×720，且不超过屏幕原生尺寸";
        if(resolution&&Math.abs(((float)w/h)/((float)nw/nh)-1)>.1f)return "分辨率比例偏离屏幕原生比例超过 10%";
        return null;
    }
    private static TextView label(Context c,LinearLayout parent,String value,int size){TextView t=new TextView(c);t.setText(value);t.setTextColor(Color.WHITE);t.setTextSize(size);t.setPadding(0,14,0,14);parent.addView(t);return t;}
    private static Switch toggle(Context c,LinearLayout parent,String title,boolean checked){Switch s=new Switch(c);s.setText(title);s.setTextColor(Color.WHITE);s.setChecked(checked);parent.addView(s);return s;}
    private static EditText integer(Context c,LinearLayout parent,String title,int value){label(c,parent,title,16);EditText e=new EditText(c);e.setInputType(2);e.setTextColor(Color.WHITE);e.setText(Integer.toString(value));parent.addView(e);return e;}
    static TextView add(Context c,LinearLayout parent,SharedPreferences p){
        int[] n=screen(c);
        label(c,parent,"图形设置",22);
        label(c,parent,"设置会保存，重启原神后由模块自动应用。分辨率及特效增强需要 Vulkan 渲染。屏幕原生："+n[0]+"×"+n[1],14);
        Switch active=toggle(c,parent,"启用自定义图形设置",p.getBoolean("graphics_enabled",false));
        Switch resolution=toggle(c,parent,"自定义渲染分辨率",p.getBoolean("render_resolution",false));
        EditText width=integer(c,parent,"渲染宽度",p.getInt("render_width",n[0]&~3));
        EditText height=integer(c,parent,"渲染高度",p.getInt("render_height",n[1]&~3));
        LinearLayout presets=new LinearLayout(c);parent.addView(presets);
        for(int percent:new int[]{100,85,70}){Button b=new Button(c);b.setText(percent+"%");presets.addView(b,new LinearLayout.LayoutParams(0,-2,1));b.setOnClickListener(v->{width.setText(Integer.toString((n[0]*percent/100)&~3));height.setText(Integer.toString((n[1]*percent/100)&~3));});}
        label(c,parent,"帧率上限（实际帧率取决于设备和游戏负载）",16);
        Spinner fps=new Spinner(c);String[] names=new String[FPS.length];int selection=0;
        for(int i=0;i<FPS.length;i++){names[i]=FPS[i]==0?"跟随游戏":FPS[i]+" FPS";if(FPS[i]==p.getInt("target_fps",0))selection=i;}
        ArrayAdapter<String> adapter=new ArrayAdapter<>(c,android.R.layout.simple_spinner_item,names);adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);fps.setAdapter(adapter);fps.setSelection(selection);parent.addView(fps);
        Switch effects=toggle(c,parent,"特效增强（提高部分特效渲染尺寸）",p.getBoolean("effect_enhance",false));
        TextView vegLabel=label(c,parent,"植被及关联距离："+p.getInt("veg_percent",100)+"%（100% 为游戏原值）",16);
        SeekBar veg=new SeekBar(c);veg.setMax(18);veg.setProgress((p.getInt("veg_percent",100)-10)/5);parent.addView(veg);
        veg.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){}public void onProgressChanged(SeekBar s,int value,boolean user){vegLabel.setText("植被及关联距离："+(10+value*5)+"%（100% 为游戏原值）");}});
        label(c,parent,"距离设置与参考模块一致，也影响地形、阴影、视距和部分纹理预算。特效增强会增加 GPU 负载。",14);
        TextView status=label(c,parent,"图形设置等待游戏连接",14);
        Button save=new Button(c);save.setText("保存图形设置（下次启动原神生效）");parent.addView(save);
        save.setOnClickListener(v->{try{
            int w=Integer.parseInt(width.getText().toString().trim()),h=Integer.parseInt(height.getText().toString().trim()),f=FPS[fps.getSelectedItemPosition()],g=10+veg.getProgress()*5;
            String error=validate(w,h,n[0],n[1],f,g,resolution.isChecked());if(error!=null){Toast.makeText(c,error,Toast.LENGTH_LONG).show();return;}
            p.edit().putBoolean("graphics_enabled",active.isChecked()).putBoolean("render_resolution",resolution.isChecked()).putInt("render_width",w).putInt("render_height",h).putInt("target_fps",f).putBoolean("effect_enhance",effects.isChecked()).putInt("veg_percent",g).apply();
            Toast.makeText(c,"图形设置已保存，下次启动原神自动应用",Toast.LENGTH_LONG).show();
        }catch(NumberFormatException e){Toast.makeText(c,"请输入整数宽高",Toast.LENGTH_SHORT).show();}});
        Button reset=new Button(c);reset.setText("恢复游戏原值（下次启动生效）");parent.addView(reset);
        reset.setOnClickListener(v->{SharedPreferences.Editor editor=p.edit();for(String key:KEYS)editor.remove(key);editor.apply();active.setChecked(false);resolution.setChecked(false);effects.setChecked(false);fps.setSelection(0);veg.setProgress(18);width.setText(Integer.toString(n[0]&~3));height.setText(Integer.toString(n[1]&~3));Toast.makeText(c,"已恢复默认，下次启动原神生效",Toast.LENGTH_LONG).show();});
        return status;
    }
    static String status(int[] s,boolean fresh){
        if(!fresh||s.length!=21)return "图形设置等待游戏连接";
        String pending=s[4]!=0?" · 新设置待重启生效":"";
        if(s[0]<0)return "图形设置部分未生效（状态 "+s[0]+"）"+pending;
        if(s[3]==0)return "本次进程跟随游戏图形设置"+pending;
        String rendering=s[6]>0?s[6]+"×"+s[7]:"等待 Vulkan 渲染资源";
        return "本次渲染："+rendering+" · 帧率上限 "+(s[8]>0?Integer.toString(s[8]):"跟随游戏")+" · 距离 "+s[17]+"%"+pending;
    }
}
