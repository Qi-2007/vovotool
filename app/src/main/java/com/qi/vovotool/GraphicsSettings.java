package com.qi.vovotool;

import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.Color;
import android.text.Editable;
import android.text.TextWatcher;
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
            c.getInt(c.getColumnIndexOrThrow("effect_enhance"))!=0,c.getInt(c.getColumnIndexOrThrow("veg_percent")),c.getInt(c.getColumnIndexOrThrow("render_path")));
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
    private static TextView label(Context c,LinearLayout parent,String value,int size){TextView t=new TextView(c);t.setText(value);t.setTextColor(Color.WHITE);t.setTextSize(size);int inset=Math.round(8*c.getResources().getDisplayMetrics().density);t.setPadding(0,inset,0,inset);parent.addView(t);return t;}
    private static Switch toggle(Context c,LinearLayout parent,String title,boolean checked){Switch s=new Switch(c);s.setText(title);s.setTextColor(Color.WHITE);s.setChecked(checked);parent.addView(s);return s;}
    private static EditText integer(Context c,LinearLayout parent,String title,int value){label(c,parent,title,16);EditText e=new EditText(c);e.setInputType(2);e.setTextColor(Color.WHITE);e.setText(Integer.toString(value));parent.addView(e);return e;}
    static TextView add(Context c,LinearLayout root,SharedPreferences p,MaterialUi ui){
        LinearLayout parent=ui.card(root);
        int[] n=screen(c);
        label(c,parent,"图形设置",22);
        label(c,parent,"设置会保存，重启原神后由模块自动应用。分辨率及特效增强需要 Vulkan 渲染。屏幕原生："+n[0]+"×"+n[1],14);
        Switch active=toggle(c,parent,"启用自定义图形设置",p.getBoolean("graphics_enabled",false));
        parent=ui.card(root);
        Switch resolution=toggle(c,parent,"自定义渲染分辨率",p.getBoolean("render_resolution",false));
        EditText width=integer(c,parent,"渲染宽度",p.getInt("render_width",n[0]&~3));
        EditText height=integer(c,parent,"渲染高度",p.getInt("render_height",n[1]&~3));
        int minimumPercent=Math.min(100,Math.max((1280*100+n[0]-1)/n[0],(720*100+n[1]-1)/n[1]));
        final int minimum=((minimumPercent+4)/5)*5;
        TextView scaleLabel=label(c,parent,"",16);
        SeekBar scale=new SeekBar(c);scale.setMax((100-minimum)/5);scale.setKeyProgressIncrement(1);scale.setEnabled(minimum<100);scale.setContentDescription("渲染分辨率比例");parent.addView(scale);
        label(c,parent,"宽高同比例缩放，步进 5%，范围 "+minimum+"%～100%。也可手动输入宽高。",14);
        boolean[] scaling={false};
        Runnable syncScale=()->{
            try{
                int w=Integer.parseInt(width.getText().toString().trim()),h=Integer.parseInt(height.getText().toString().trim());
                int percent=Math.max(minimum,Math.min(100,Math.round(((float)w/n[0]+(float)h/n[1])*10)*5));
                scale.setProgress((percent-minimum)/5);
                boolean exact=w==((n[0]*percent/100)&~3)&&h==((n[1]*percent/100)&~3);
                scaleLabel.setText("渲染比例："+(exact?percent+"%":"自定义")+" · "+w+"×"+h);
            }catch(NumberFormatException e){scaleLabel.setText("渲染比例：自定义（请输入宽高）");}
        };
        scale.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){onProgressChanged(s,s.getProgress(),true);}
            public void onProgressChanged(SeekBar s,int value,boolean user){if(!user)return;int percent=minimum+value*5;scaling[0]=true;
                width.setText(Integer.toString((n[0]*percent/100)&~3));height.setText(Integer.toString((n[1]*percent/100)&~3));scaling[0]=false;syncScale.run();}
        });
        TextWatcher dimensions=new TextWatcher(){public void beforeTextChanged(CharSequence s,int start,int count,int after){}public void onTextChanged(CharSequence s,int start,int before,int count){}public void afterTextChanged(Editable s){if(!scaling[0])syncScale.run();}};
        width.addTextChangedListener(dimensions);height.addTextChangedListener(dimensions);syncScale.run();
        parent=ui.card(root);
        label(c,parent,"帧率上限（实际帧率取决于设备和游戏负载）",16);
        Spinner fps=new Spinner(c);String[] names=new String[FPS.length];int selection=0;
        for(int i=0;i<FPS.length;i++){names[i]=FPS[i]==0?"跟随游戏":FPS[i]+" FPS";if(FPS[i]==p.getInt("target_fps",0))selection=i;}
        ArrayAdapter<String> adapter=new ArrayAdapter<>(c,android.R.layout.simple_spinner_item,names);adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);fps.setAdapter(adapter);fps.setSelection(selection);parent.addView(fps);
        parent=ui.card(root);
        Switch effects=toggle(c,parent,"特效增强（提高部分特效渲染尺寸）",p.getBoolean("effect_enhance",false));
        label(c,parent,"提高部分特效渲染尺寸，会增加 GPU 负载。",14);
        parent=ui.card(root);
        TextView vegLabel=label(c,parent,"植被及关联距离："+p.getInt("veg_percent",100)+"%（100% 为游戏原值）",16);
        SeekBar veg=new SeekBar(c);veg.setMax(18);veg.setProgress((p.getInt("veg_percent",100)-10)/5);parent.addView(veg);
        veg.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){}public void onProgressChanged(SeekBar s,int value,boolean user){vegLabel.setText("植被及关联距离："+(10+value*5)+"%（100% 为游戏原值）");}});
        label(c,parent,"距离设置也影响地形、阴影、视距和部分纹理预算。",14);
        parent=ui.card(root);
        label(c,parent,"保存与应用",20);
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
    static String status(int[] s,int[] render,boolean fresh){
        if(!fresh||s.length!=21)return "图形设置等待游戏连接";
        String pending=s[4]!=0?" · 新设置待重启生效":"";
        if(s[0]<0)return "图形设置部分未生效（状态 "+s[0]+"）"+pending;
        if(s[3]==0)return "本次进程跟随游戏图形设置"+pending;
        String output=s[6]>0?s[6]+"×"+s[7]:"等待渲染资源";
        String scene=""; // Managed RT size alone does not establish scene raster resolution.
        String readback=render.length==12&&render[11]>0?" · 检测到图像读回区域越界":"";
        return scene+"输出缓冲："+output+" · 帧率上限 "+(s[8]>0?Integer.toString(s[8]):"跟随游戏")+" · 距离 "+s[17]+"%"+pending+readback;
    }
}
