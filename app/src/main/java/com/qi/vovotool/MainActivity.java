package com.qi.vovotool;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.view.Gravity;
import android.widget.FrameLayout;
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
    private LinearLayout homePage,managePage,aboutPage;
    private FloatingNavigation navigation;
    private boolean navigationReady;
    private final LinearLayout[] categories=new LinearLayout[5];
    private CategoryTabs categoryTabs;
    private SwipePages pages,categoryPages;
    private int selectedPage,selectedCategory;
    private MaterialUi ui;
    private DeveloperMode developerMode;
    private android.app.AlertDialog developerDialog;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private TextView state,rootState,uidState,craftState,dailyState,expeditionState,graphicsState,teamLoadingState,avatarFadeState,avatarSwitchState;
    private EditText mouse,touchpad;
    private SharedPreferences prefs;
    private Button shortcutButton,dailyShortcutButton,expeditionShortcutButton;
    private Button regionUnlockButton;
    private TextView regionUnlockState;
    private long regionUnlockSubmittedAt;
    private int regionUnlockPreviousRequests;
    private boolean recordingShortcut;
    private String recordingFeature="craft";
    private int recordedRelease;
    private String craftShortcut(){return Shortcut.label(prefs.getInt("craft_key",Shortcut.DEFAULT_KEY),prefs.getInt("craft_meta",0));}
    private String dailyShortcut(){return Shortcut.label(prefs.getInt("daily_key",Shortcut.DEFAULT_DAILY_KEY),prefs.getInt("daily_meta",0));}
    private String expeditionShortcut(){return Shortcut.label(prefs.getInt("expedition_key",Shortcut.DEFAULT_EXPEDITION_KEY),prefs.getInt("expedition_meta",0));}
    private String featureShortcut(String feature){return Shortcut.label(prefs.getInt(feature+"_key",Shortcut.defaultKey(feature)),prefs.getInt(feature+"_meta",0));}
    private void resetShortcutButtons(){
        if(shortcutButton!=null)shortcutButton.setText("设置快捷键 · "+craftShortcut());
        if(dailyShortcutButton!=null)dailyShortcutButton.setText("设置快捷键 · "+dailyShortcut());
        if(expeditionShortcutButton!=null)expeditionShortcutButton.setText("设置快捷键 · "+expeditionShortcut());
    }
    private void recordShortcut(String feature){
        resetShortcutButtons();recordingShortcut=true;recordingFeature=feature;
        ("expedition".equals(feature)?expeditionShortcutButton:"daily".equals(feature)?dailyShortcutButton:shortcutButton).setText("请按快捷键（Esc 取消）");
    }
    private int dp(int value){return (int)(getResources().getDisplayMetrics().density*value+.5f);}
    private TextView text(LinearLayout parent,String value,int size){TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(ui.ink);t.setPadding(0,dp(8),0,dp(8));parent.addView(t);return t;}
    private Button button(LinearLayout parent,String title,View.OnClickListener action){Button b=new Button(this);b.setText(title);b.setOnClickListener(action);parent.addView(b,new LinearLayout.LayoutParams(-1,dp(54)));return b;}
    private EditText number(LinearLayout parent,String title,float value){text(parent,title,17);EditText e=new EditText(this);e.setText(Float.toString(value));e.setTextColor(ui.ink);e.setInputType(InputType.TYPE_CLASS_NUMBER|InputType.TYPE_NUMBER_FLAG_DECIMAL);parent.addView(e);return e;}
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);prefs=Settings.preferences(this);Settings.grantGameAccess(this);
        developerMode=new DeveloperMode(Settings.developerLevel(prefs));
        ui=new MaterialUi(this);
        buildShell();
        LinearLayout layout=card(homePage);
        text(layout,"模块状态",14);
        state=text(layout,"等待游戏连接",20);
        text(layout,"在 LSPosed 中启用本模块，作用域勾选「原神」。",14);
        button(layout,"打开原神",v->openGame());
        LinearLayout intro=card(homePage);
        text(intro,"让操作更顺手",22);
        text(intro,"画面、操控与便携工具，在管理页集中调整。",15);
        button(intro,"管理功能",v->selectPage(1));
        text(homePage,"当前适配 · 国服 7.2 / ARM64",13);
        layout=card(categories[1]);
        Switch enabled=new Switch(this);enabled.setText("启用 PC 界面与原生键鼠");enabled.setTextColor(Color.WHITE);enabled.setChecked(prefs.getBoolean("enabled",true));enabled.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("enabled",c).apply());layout.addView(enabled);
        layout=card(categories[1]);
        Switch capture=new Switch(this);capture.setText("启用自动捕获指针");capture.setTextColor(ui.ink);capture.setChecked(prefs.getBoolean("capture",true));capture.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("capture",c).apply());layout.addView(capture);
        layout=card(categories[3]);
        Switch hideUid=new Switch(this);hideUid.setText("隐藏 UID（右下角及派蒙菜单）");hideUid.setTextColor(Color.WHITE);hideUid.setChecked(prefs.getBoolean("hide_uid",true));hideUid.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("hide_uid",c).apply());layout.addView(hideUid);
        uidState=text(layout,"等待 UID 显示组件",14);
        layout=card(categories[3]);
        Switch qr=new Switch(this);qr.setText("扫码登录入口");qr.setTextColor(Color.WHITE);qr.setChecked(prefs.getBoolean("qr_login",true));qr.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("qr_login",c).apply());layout.addView(qr);
        text(layout,"在游戏登陆弹窗页左上角添加扫码登陆入口。",14);
        layout=card(categories[2]);
        Switch craft=new Switch(this);craft.setText("随身合成台");craft.setTextColor(Color.WHITE);craft.setChecked(prefs.getBoolean("portable_craft",true));craft.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("portable_craft",c).apply());layout.addView(craft);
        shortcutButton=button(layout,"设置快捷键 · "+craftShortcut(),v->recordShortcut("craft"));
        text(layout,"支持普通按键及Shift 组合键。在大世界可操作状态下打开合成台页面。",14);
        craftState=text(layout,"随身合成台已就绪",14);
        layout=card(categories[2]);
        Switch daily=new Switch(this);daily.setText("随身领取日委奖励");daily.setTextColor(Color.WHITE);daily.setChecked(prefs.getBoolean("portable_daily",true));daily.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("portable_daily",c).apply());layout.addView(daily);
        dailyShortcutButton=button(layout,"设置快捷键 · "+dailyShortcut(),v->recordShortcut("daily"));
        text(layout,"支持普通按键及Shift 组合键。在大世界可操作状态下领取已满足条件的日委奖励。",14);
        dailyState=text(layout,"日委领取等待游戏连接",14);
        layout=card(categories[2]);
        Switch expedition=new Switch(this);expedition.setText("随身探索派遣");expedition.setTextColor(Color.WHITE);expedition.setChecked(prefs.getBoolean("portable_expedition",true));expedition.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("portable_expedition",c).apply());layout.addView(expedition);
        expeditionShortcutButton=button(layout,"设置快捷键 · "+expeditionShortcut(),v->recordShortcut("expedition"));
        text(layout,"支持普通按键及Shift 组合键。在大世界可操作状态下打开原生探索派遣界面。",14);
        expeditionState=text(layout,"探索派遣等待游戏连接",14);
        layout=card(categories[3]);
        Switch teamLoading=new Switch(this);teamLoading.setText("取消打开队伍配置的读条");teamLoading.setTextColor(Color.WHITE);teamLoading.setChecked(prefs.getBoolean("skip_team_loading",true));teamLoading.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("skip_team_loading",c).apply());layout.addView(teamLoading);
        text(layout,"仅在页面就绪后跳过读条，保留游戏原本的开启限制。",14);
        teamLoadingState=text(layout,"队伍读条功能等待游戏连接",14);
        layout=card(categories[0]);
        Switch avatarFade=new Switch(this);avatarFade.setText("反角色虚化");avatarFade.setTextColor(ui.ink);avatarFade.setChecked(prefs.getBoolean("disable_character_fade",true));avatarFade.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("disable_character_fade",c).apply());layout.addView(avatarFade);
        avatarFadeState=text(layout,"反角色虚化等待游戏连接",14);
        layout=card(categories[3]);
        Switch avatarSwitch=new Switch(this);avatarSwitch.setText("取消切换角色冷却");avatarSwitch.setTextColor(Color.WHITE);avatarSwitch.setChecked(prefs.getBoolean("no_avatar_switch_cd",false));avatarSwitch.setOnCheckedChangeListener((v,c)->prefs.edit().putBoolean("no_avatar_switch_cd",c).apply());layout.addView(avatarSwitch);
        text(layout,"在大世界快速切人，并移除头像倒计时。极快连按仍可能被游戏服务器拒绝。",14);
        avatarSwitchState=text(layout,"切人冷却功能等待游戏连接",14);
        layout=card(categories[4]);
        text(layout,"红圈解锁",22);
        text(layout,"解除新手限制（47）、大世界限制（48）及至冬区域限制（7058）。",14);
        regionUnlockButton=button(layout,"解锁红圈",v->{
            Bundle result=getContentResolver().call(Settings.URI,"unlock_regions",null,null);
            if(result!=null&&result.getBoolean("accepted",false)){
                regionUnlockSubmittedAt=android.os.SystemClock.elapsedRealtime();
                int[] previous=ConfigProvider.lastRegionUnlockState;
                regionUnlockPreviousRequests=previous.length==5?previous[1]:0;
                regionUnlockButton.setEnabled(false);regionUnlockState.setText("已提交，请返回原神");
                Toast.makeText(this,"已提交，请返回原神",Toast.LENGTH_SHORT).show();
            }else Toast.makeText(this,"请先连接原神并开启操控主开关",Toast.LENGTH_SHORT).show();
        });
        text(layout,"对当前游戏执行一次，重新登录后可再次使用。",14);
        regionUnlockState=text(layout,"红圈解锁等待游戏连接",14);
        layout=card(categories[1]);
        text(layout,"镜头速度",20);
        mouse=number(layout,"镜头基础倍率（默认 0.02）",prefs.getFloat("mouse_scale",.02f));
        touchpad=number(layout,"触控板独立倍率（默认 0.25）",prefs.getFloat("touchpad_gain",.25f));
        button(layout,"保存速度",v->{try{
            float m=Float.parseFloat(mouse.getText().toString()),t=Float.parseFloat(touchpad.getText().toString());
            if(!Float.isFinite(m)||m<.0001f||m>2||!Float.isFinite(t)||t<.05f||t>2)throw new IllegalArgumentException();
            prefs.edit().putFloat("mouse_scale",m).putFloat("touchpad_gain",t).apply();Toast.makeText(this,"已保存，游戏内约 1 秒后生效",Toast.LENGTH_SHORT).show();
        }catch(Exception e){Toast.makeText(this,"基础倍率范围 0.0001～2；触控板倍率范围 0.05～2",Toast.LENGTH_LONG).show();}});
        graphicsState=GraphicsSettings.add(this,categories[0],prefs,ui);
        layout=card(categories[1]);
        text(layout,"触控板兼容",22);
        text(layout,"外接鼠标无需启动此项。仅适配 Lenovo 磁吸键盘的触控板，其他品牌请自行尝试。需要对模块进行 root 授权。",16);
        rootState=text(layout,"未启动",16);
        Switch touchpadCompat=new Switch(this);touchpadCompat.setText("启用触控板兼容");
        touchpadCompat.setChecked(prefs.getBoolean(Settings.TOUCHPAD_COMPAT,false));
        touchpadCompat.setOnCheckedChangeListener((v,checked)->{
            prefs.edit().putBoolean(Settings.TOUCHPAD_COMPAT,checked).apply();
            if(checked&&Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)
                requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},100);
            RootProxyService.reconcile(this);
        });layout.addView(touchpadCompat);
        text(layout,"开关状态会保存，开启后在启动游戏时自动连接。首次启用请授予 root 权限。",14);

        text(layout,"关闭操控主开关可恢复本次进程的输入基线；完整重载界面请重新启动游戏。模块不修改游戏文件或系统分区。",14);
        ui.styleTree(pages);
        updateDeveloperUi();
        selectCategory(saved==null?0:saved.getInt("category",0));
        selectPage(saved==null?0:saved.getInt("page",0));
    }
    private void saveDeveloperLevel(){
        prefs.edit().putInt(DeveloperMode.LEVEL_KEY,developerMode.level()).apply();
        updateDeveloperUi();
        selectCategory(selectedCategory);
    }
    private void updateDeveloperUi(){categoryTabs.setAvailableCount(DeveloperMode.visibleCategoryCount(developerMode.level()));}
    private final Runnable finishLogoTaps=()->{
        if(selectedPage!=2)return;
        int action=developerMode.finishTaps(android.os.SystemClock.elapsedRealtime());
        if(action==2){showDeveloperDialog();return;}
        if(action==1||action==3){
            saveDeveloperLevel();Toast.makeText(this,action==1?"已启用高级功能":"已恢复基础模式",Toast.LENGTH_SHORT).show();
        }
    };
    private void onLogoTap(){
        handler.removeCallbacks(finishLogoTaps);
        if(developerMode.tap(android.os.SystemClock.elapsedRealtime()))handler.postDelayed(finishLogoTaps,DeveloperMode.RESET_IDLE_MS);
    }
    private void showDeveloperDialog(){
        EditText input=new EditText(this);input.setSingleLine(true);input.setHint("请输入验证码");
        input.setInputType(InputType.TYPE_CLASS_NUMBER|InputType.TYPE_NUMBER_VARIATION_PASSWORD);
        input.setImeOptions(android.view.inputmethod.EditorInfo.IME_ACTION_DONE);
        LinearLayout content=column();content.setPadding(dp(24),dp(8),dp(24),0);content.addView(input);
        Runnable submit=()->{
            if(developerMode.submit(input.getText().toString())){
                saveDeveloperLevel();Toast.makeText(this,"已启用开发者模式",Toast.LENGTH_SHORT).show();
            }
            if(developerDialog!=null)developerDialog.dismiss();
        };
        developerDialog=new android.app.AlertDialog.Builder(this).setTitle("开发者模式").setView(content)
            .setPositiveButton("确认",(dialog,which)->submit.run()).setNegativeButton("取消",null).create();
        developerDialog.setOnDismissListener(dialog->{developerMode.cancel();developerDialog=null;});
        input.setOnEditorActionListener((v,action,event)->{
            if(action==android.view.inputmethod.EditorInfo.IME_ACTION_DONE||
                (event!=null&&(event.getKeyCode()==KeyEvent.KEYCODE_ENTER||event.getKeyCode()==KeyEvent.KEYCODE_NUMPAD_ENTER))){
                if(event==null||event.getAction()==KeyEvent.ACTION_DOWN)submit.run();return true;
            }return false;
        });
        developerDialog.setOnShowListener(dialog->{input.requestFocus();developerDialog.getWindow().setSoftInputMode(android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);});
        developerDialog.show();
    }
    private void openGame(){
        Intent game=getPackageManager().getLaunchIntentForPackage(Settings.GAME);
        if(game!=null)startActivity(game);else Toast.makeText(this,"未找到原神",Toast.LENGTH_SHORT).show();
    }
    private LinearLayout column(){LinearLayout v=new LinearLayout(this);v.setOrientation(LinearLayout.VERTICAL);return v;}
    private LinearLayout card(LinearLayout parent){return ui.card(parent);}
    private LinearLayout page(String title,String subtitle){
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setClipToPadding(false);
        LinearLayout content=column();content.setPadding(dp(24),dp(16),dp(24),dp(24));scroll.addView(content);pages.addView(scroll);
        text(content,title,32).setTypeface(null,android.graphics.Typeface.BOLD);
        text(content,subtitle,14);return content;
    }
    private void buildShell(){
        getWindow().setStatusBarColor(ui.background);getWindow().setNavigationBarColor(ui.background);
        if(!ui.dark)getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        LinearLayout shell=column();shell.setBackgroundColor(ui.background);
        shell.setOnApplyWindowInsetsListener((v,insets)->{v.setPadding(insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop(),insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom());return insets.consumeSystemWindowInsets();});
        pages=new SwipePages(this,index->selectPage(index));shell.addView(pages,new LinearLayout.LayoutParams(-1,0,1));
        homePage=page("沃沃头","为你的冒险，添一点顺手。 ");
        managePage=page("管理","按场景整理，按习惯调整。");
        String[] titles={"画面","操控","便携功能","其他功能","实验功能"};
        String[] descriptions={"分辨率、帧率与画面效果","原生键鼠、镜头速度与触控板","合成、日委奖励与探索派遣","界面显示与便捷设置",""};
        categoryTabs=new CategoryTabs(this,ui,titles,index->selectCategory(index));
        managePage.addView(categoryTabs);
        categoryPages=new SwipePages(this,index->selectCategory(index));
        categoryPages.setMinimumHeight(dp(240));
        managePage.addView(categoryPages,new LinearLayout.LayoutParams(-1,-2));
        for(int i=0;i<categories.length;i++){
            categories[i]=column();categoryPages.addView(categories[i],new FrameLayout.LayoutParams(-1,-2));
            if(!descriptions[i].isEmpty()){TextView description=text(categories[i],descriptions[i],14);description.setPadding(0,0,0,dp(16));}
        }
        aboutPage=page("关于","简单、直接，专注你的体验。");
        LinearLayout app=card(aboutPage);
        android.widget.ImageView logo=new android.widget.ImageView(this);logo.setImageDrawable(getApplicationInfo().loadIcon(getPackageManager()));app.addView(logo,new LinearLayout.LayoutParams(dp(64),dp(64)));
        logo.setContentDescription("沃沃头 Logo");logo.setOnClickListener(v->onLogoTap());
        text(app,"沃沃头",24);
        try{text(app,"版本 "+getPackageManager().getPackageInfo(getPackageName(),0).versionName+" 仅供学习研究，免费开源，禁止售卖",14);}catch(Exception ignored){}
        text(app,getPackageName(),14);
        LinearLayout info=card(aboutPage);text(info,"使用说明",20);text(info,"在 LSPosed 中启用沃沃头，作用域仅勾选原神。更新模块后，请重新启动游戏。",15);text(info,"图形设置保存后，下次启动原神生效。其他开关和快捷键通常约 1 秒生效。",15);
        text(info,"模块不修改游戏文件或系统分区。",14);
        FrameLayout navHost=new FrameLayout(this);
        navigation=new FloatingNavigation(this,ui,index->selectPage(index));
        FrameLayout.LayoutParams navParams=new FrameLayout.LayoutParams(-1,dp(72),Gravity.CENTER);
        navParams.setMargins(dp(24),dp(8),dp(24),dp(16));navHost.addView(navigation,navParams);
        navHost.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob)->{
            int width=Math.min(dp(420),Math.max(0,r-l-dp(48)));
            if(navigation.getLayoutParams().width!=width){navigation.getLayoutParams().width=width;navigation.requestLayout();}
        });
        shell.addView(navHost,new LinearLayout.LayoutParams(-1,dp(96)));
        setContentView(shell);
    }
    private void reveal(View view,boolean animate,int direction){
        view.animate().cancel();view.setAlpha(1);view.setTranslationX(0);view.setTranslationY(0);
        if(animate&&android.animation.ValueAnimator.areAnimatorsEnabled()){
            view.setAlpha(0);view.setTranslationX(dp(18)*direction);
            view.animate().alpha(1).translationX(0).setDuration(260).setInterpolator(new android.view.animation.PathInterpolator(.2f,0,0,1)).start();
        }
    }
    private void selectPage(int page){
        int target=Math.max(0,Math.min(2,page)),previous=selectedPage;
        boolean animate=navigationReady&&previous!=target;
        if(target!=2){developerMode.resetTaps();handler.removeCallbacks(finishLogoTaps);}
        recordingShortcut=false;resetShortcutButtons();selectedPage=target;
        for(int i=0;i<3;i++){
            View child=pages.getChildAt(i);child.animate().cancel();child.setAlpha(1);child.setTranslationX(0);
            child.setVisibility(i==selectedPage?View.VISIBLE:View.GONE);
        }
        if(animate)reveal(pages.getChildAt(target),true,target>previous?1:-1);
        pages.setPage(target,3);
        navigation.select(target,animate);navigationReady=true;
    }
    private void selectCategory(int category){
        int target=Math.max(0,Math.min(categories.length-1,category)),previous=selectedCategory;
        if(target>=DeveloperMode.visibleCategoryCount(developerMode.level()))target=0;
        recordingShortcut=false;resetShortcutButtons();selectedCategory=target;
        for(int i=0;i<categories.length;i++){
            categories[i].animate().cancel();categories[i].setAlpha(1);categories[i].setTranslationX(0);
            categories[i].setVisibility(i==selectedCategory?View.VISIBLE:View.GONE);
        }
        boolean animate=navigationReady&&previous!=target;
        categoryPages.setPage(target,DeveloperMode.visibleCategoryCount(developerMode.level()));
        categoryTabs.select(target,animate);
        reveal(categories[target],animate,target>previous?1:-1);
    }
    @Override protected void onSaveInstanceState(Bundle out){super.onSaveInstanceState(out);out.putInt("page",selectedPage);out.putInt("category",selectedCategory);}

    private final Runnable refresh=new Runnable(){public void run(){
        int[] s=ConfigProvider.lastState;boolean fresh=System.currentTimeMillis()-ConfigProvider.lastReport<4000;
        refreshRegionUnlock(fresh);
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
        int[] sw=ConfigProvider.lastAvatarSwitchState;
        if(fresh&&sw.length==14){
            if(sw[0]<0)avatarSwitchState.setText("切人冷却适配失败，本次未启用");
            else if(sw[1]==0)avatarSwitchState.setText("切人冷却保持原样");
            else if(!prefs.getBoolean("enabled",true))avatarSwitchState.setText("快速切人随模块主开关关闭");
            else if(sw[0]==1)avatarSwitchState.setText("快速切人已启用 · 头像倒计时已移除");
            else avatarSwitchState.setText("切人冷却功能等待加载");
        }else avatarSwitchState.setText("切人冷却功能等待游戏连接");
        int[] fade=ConfigProvider.lastAvatarFadeState;
        if(fresh&&fade.length==8){
            if(fade[0]<0)avatarFadeState.setText("反角色虚化加载失败 · "+fade[0]);
            else if(fade[1]==0)avatarFadeState.setText("角色镜头虚化保持原样");
            else avatarFadeState.setText(fade[2]>0?"反角色虚化已就绪":"等待场景角色");
        }else avatarFadeState.setText("反角色虚化等待游戏连接");
        int[] team=ConfigProvider.lastTeamLoadingState;
        if(fresh&&team.length==7){
            if(team[0]<0)teamLoadingState.setText("队伍读条功能加载失败 · "+team[0]);
            else if(team[1]==0)teamLoadingState.setText("队伍读条保持原样");
            else if(team[0]==1)teamLoadingState.setText("队伍读条已就绪 · 已跳过 "+team[3]+" 次");
            else teamLoadingState.setText("队伍读条功能等待加载");
        }else teamLoadingState.setText("队伍读条功能等待游戏连接");
        int[] u=ConfigProvider.lastUidState;
        if(fresh&&u.length==5){
            switch(u[0]){
                case 2:uidState.setText("已隐藏 UID");break;
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
        int[] expedition=ConfigProvider.lastExpeditionState;
        if(fresh&&expedition.length==6){
            if(expedition[1]==0)expeditionState.setText("随身探索派遣未启用");
            else switch(expedition[0]){
                case 1:case 2:expeditionState.setText("正在打开探索派遣界面");break;
                case 3:expeditionState.setText("上次已成功打开探索派遣界面");break;
                case -1:expeditionState.setText("请回到大世界可操作状态后按 "+expeditionShortcut());break;
                case -2:expeditionState.setText("游戏未接受探索派遣请求");break;
                case -3:expeditionState.setText("等待探索派遣界面超时");break;
                default:expeditionState.setText("探索派遣已就绪 · "+expeditionShortcut());
            }
        }else expeditionState.setText("探索派遣等待游戏连接");
        graphicsState.setText(GraphicsSettings.status(ConfigProvider.lastGraphicsState,ConfigProvider.lastRenderState,ConfigProvider.lastSystemFpsState,prefs.getBoolean("system_fps_unlock",false),fresh));
        rootState.setText(RootProxyService.status);handler.postDelayed(this,1000);
    }};
    private void refreshRegionUnlock(boolean fresh){
        int[] value=ConfigProvider.lastRegionUnlockState;
        boolean waiting=regionUnlockSubmittedAt>0&&(value.length!=5||value[1]<=regionUnlockPreviousRequests);
        if(!waiting)regionUnlockSubmittedAt=0;
        if(waiting&&android.os.SystemClock.elapsedRealtime()-regionUnlockSubmittedAt<60000){
            regionUnlockButton.setEnabled(false);regionUnlockState.setText("已提交，请返回原神");return;
        }
        boolean connected=fresh&&value.length==5;
        regionUnlockButton.setEnabled(connected&&value[0]!=1&&prefs.getBoolean("enabled",true)&&DeveloperMode.allowed(Settings.developerLevel(prefs),"unlock_regions"));
        if(waiting){regionUnlockState.setText("请求已过期，请返回原神后重试");return;}
        if(!connected){regionUnlockState.setText("红圈解锁等待游戏连接");return;}
        switch(value[0]){
            case 1:regionUnlockState.setText("等待游戏加载，请返回原神");break;
            case 2:regionUnlockState.setText("已解锁 · 47、48、7058 均已开启");break;
            case -1:regionUnlockState.setText("红圈解锁版本校验未通过");break;
            case -2:case -3:regionUnlockState.setText("未确认全部解锁，请返回原神后重试");break;
            case -4:regionUnlockState.setText("请求已取消");break;
            case -5:regionUnlockState.setText("请求已过期，请返回原神后重试");break;
            default:regionUnlockState.setText("红圈解锁已就绪");
        }
    }
    @Override public void onResume(){super.onResume();Settings.grantGameAccess(this);RootProxyService.reconcile(this);
        if(developerMode.level()!=Settings.developerLevel(prefs)){
            developerMode=new DeveloperMode(Settings.developerLevel(prefs));updateDeveloperUi();selectCategory(selectedCategory);
        }
        handler.post(refresh);
    }
    @Override public boolean dispatchKeyEvent(KeyEvent event){
        int code=event.getKeyCode();
        if(event.getAction()==KeyEvent.ACTION_UP && recordedRelease==code){recordedRelease=0;return true;}
        if(!recordingShortcut)return super.dispatchKeyEvent(event);
        if(event.getAction()!=KeyEvent.ACTION_DOWN || KeyEvent.isModifierKey(code))return true;
        if(code==KeyEvent.KEYCODE_ESCAPE || code==KeyEvent.KEYCODE_BACK){
            recordingShortcut=false;recordedRelease=code;resetShortcutButtons();return true;
        }
        if(!Shortcut.supported(code)){Toast.makeText(this,"请换一个普通按键或组合键",Toast.LENGTH_SHORT).show();return true;}
        String target=recordingFeature;int meta=Shortcut.modifiers(event);
        for(String other:new String[]{"craft","daily","expedition"}){
            if(other.equals(target))continue;
            if(code==prefs.getInt(other+"_key",Shortcut.defaultKey(other))&&meta==(prefs.getInt(other+"_meta",0)&Shortcut.META_MASK)){
                recordedRelease=code;Toast.makeText(this,"与另一项功能的快捷键重复，请换一个",Toast.LENGTH_SHORT).show();return true;
            }
        }
        prefs.edit().putInt(target+"_key",code).putInt(target+"_meta",meta).apply();
        recordingShortcut=false;recordedRelease=code;resetShortcutButtons();
        Toast.makeText(this,"已保存："+featureShortcut(target),Toast.LENGTH_SHORT).show();return true;
    }
    @Override public void onPause(){
        developerMode.resetTaps();handler.removeCallbacks(finishLogoTaps);
        if(developerDialog!=null)developerDialog.dismiss();
        recordingShortcut=false;recordedRelease=0;
        resetShortcutButtons();
        handler.removeCallbacks(refresh);super.onPause();
    }
}
