package com.qi.vovotool;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Collections;
import java.util.Map;
import java.util.WeakHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import org.json.JSONObject;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;

/** QR authorization is exchanged and validated by the game's existing passport SDK. */
final class QrLoginBridge {
    private static final String APP_ID="4";
    private static final String ENDPOINT="https://hk4e-sdk.mihoyo.com/hk4e_cn/combo/panda/qrcode/";
    private static final Handler main=new Handler(Looper.getMainLooper());
    private static final Map<View,Session> decorated=Collections.synchronizedMap(new WeakHashMap<>());
    private static final AtomicInteger opens=new AtomicInteger();
    private static volatile Session active;
    private static volatile int installed,phase,sdkCode;
    private static volatile Object pendingManager,pendingCallback;
    private static volatile Object completedCallback;
    private static final java.util.Set<Class<?>> successHooks=Collections.synchronizedSet(new java.util.HashSet<>());
    private static Method tokenLogin;
    private static final ThreadLocal<QrDialog> qrVerification=new ThreadLocal<>();
    private QrLoginBridge() {}
    static int[] state(){return new int[]{installed,active!=null?1:0,opens.get(),phase,sdkCode};}
    private static boolean configured(Context context){
        try(Cursor c=context.getContentResolver().query(Settings.URI,null,null,null,null)){
            return c!=null&&c.moveToFirst()&&c.getInt(c.getColumnIndexOrThrow("qr_login"))!=0;
        }catch(Exception e){return false;}
    }
    static void install(ClassLoader loader){
        try{
            Class<?> manager=XposedHelpers.findClass("com.miHoYo.sdk.platform.module.login.LoginManager",loader);
            tokenLogin=manager.getDeclaredMethod("autoLogin",String.class,String.class,String.class);
            Class<?> presenter=XposedHelpers.findClass("com.miHoYo.sdk.platform.module.login.NewAutoLoginPresenter",loader);
            XposedBridge.hookAllMethods(presenter,"passportStartVerify",new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    QrDialog dialog=qrVerification.get();if(dialog==null)return;
                    try{
                        Object internal=XposedHelpers.callStaticMethod(XposedHelpers.findClass("com.miHoYo.sdk.platform.MDKInternal",loader),"passportPlatform");
                        XposedBridge.log("vovotool QR SDK passport module present="+(internal!=null));
                        if(internal==null){
                            // The legacy SDK verifies this token normally when no passport module is loaded.
                            XposedHelpers.callMethod(p.thisObject,"startVerify",p.args[0],p.args[1]);
                        }else{
                            // loginCheck() ignores the supplied QR account and checks cached credentials.
                            // Use the real token-login API with the original MDK continuation instead.
                            Class<?> continuation=XposedHelpers.findClass("com.miHoYo.sdk.platform.module.login.NewAutoLoginPresenter$passportStartVerify$1",loader);
                            Object callback=XposedHelpers.newInstance(continuation,p.thisObject,p.args[1]);
                            Object account=p.args[0];int uid=Integer.parseInt((String)XposedHelpers.callMethod(account,"getUid"));
                            String token=(String)XposedHelpers.callMethod(account,"getToken");
                            Credential credential=dialog.credential;
                            if(credential!=null&&credential.v2){
                                if(credential.mid.isEmpty())throw new IllegalStateException();
                                Object passportAccount=XposedHelpers.newInstance(XposedHelpers.findClass("com.mihoyo.platform.account.sdk.bean.Account",loader));
                                XposedHelpers.callMethod(passportAccount,"setAid",Integer.toString(uid));
                                XposedHelpers.callMethod(passportAccount,"setMid",credential.mid);
                                XposedHelpers.callMethod(passportAccount,"setTokenStr",token);
                                Class<?> types=XposedHelpers.findClass("com.mihoyo.platform.account.sdk.constant.TokenType",loader);
                                Object accountType=XposedHelpers.getStaticObjectField(types,"TOKEN_TYPE_STOKEN");
                                XposedHelpers.callMethod(passportAccount,"setTokenType",XposedHelpers.callMethod(accountType,"getType"));
                                Class<?> passportManager=XposedHelpers.findClass("com.mihoyo.platform.account.sdk.login.LoginManager",loader);
                                XposedHelpers.callMethod(XposedHelpers.getStaticObjectField(passportManager,"INSTANCE"),"loginVerify",dialog.session.activity,passportAccount,callback,false);
                            }else{
                                Class<?> porte=XposedHelpers.findClass("com.mihoyo.platform.account.sdk.Porte",loader);
                                Class<?> callbackType=XposedHelpers.findClass("com.mihoyo.platform.account.sdk.callback.ILoginCallback",loader);
                                Method login=porte.getMethod("loginByGameToken",Activity.class,int.class,String.class,Integer.class,Integer.class,String.class,boolean.class,callbackType);
                                login.invoke(XposedHelpers.getStaticObjectField(porte,"INSTANCE"),dialog.session.activity,uid,token,null,null,null,false,callback);
                            }
                        }
                        p.setResult(null);
                    }catch(Throwable e){p.setThrowable(new IllegalStateException("QR SDK token login failed"));}
                }
            });
            for(String step:new String[]{"startVerify","verifyToken","check","passportStartVerify","goRealNameOrSuccess","closeLoading"}){
                XposedBridge.hookAllMethods(presenter,step,new XC_MethodHook(){
                    @Override protected void afterHookedMethod(MethodHookParam p){
                        if(phase==4)XposedBridge.log("vovotool QR SDK step="+p.method.getName()+" failed="+p.hasThrowable());
                    }
                });
            }
            Class<?> continuation=XposedHelpers.findClass("com.miHoYo.sdk.platform.module.login.NewAutoLoginPresenter$passportStartVerify$1",loader);
            XposedBridge.hookAllMethods(continuation,"onFailed",new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    if(phase==4){sdkCode=(Integer)p.args[0];phase=-3;Session session=active;if(session!=null&&session.qr!=null)session.qr.authenticating=false;}
                }
            });
            XposedBridge.hookAllMethods(manager,"login",new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    if(p.args.length==1&&p.args[0]!=null){completedCallback=null;pendingManager=p.thisObject;pendingCallback=p.args[0];}
                }
            });
            XposedBridge.hookAllMethods(manager,"setCallback",new XC_MethodHook(){
                @Override protected void afterHookedMethod(MethodHookParam p){
                    if(p.args.length==1){completedCallback=null;pendingManager=p.thisObject;pendingCallback=p.args[0];}
                }
            });
            for(String event:new String[]{"loginResult","loginFailed","loginCancel"}){
                XposedBridge.hookAllMethods(manager,event,new XC_MethodHook(){
                    @Override protected void afterHookedMethod(MethodHookParam p){
                        Session session=active;if(session==null||session.porte!=p.thisObject)return;
                        if(p.method.getName().equals("loginResult")){
                            if(session.qr!=null&&session.qr.authenticating){phase=5;return;}
                        }
                        if(p.method.getName().equals("loginFailed")){phase=-3;sdkCode=p.args.length>0&&p.args[0] instanceof Integer?(Integer)p.args[0]:0;}
                        pendingCallback=null;session.finish();
                    }
                });
            }
            XposedHelpers.findAndHookMethod(Activity.class,"onResume",new XC_MethodHook(){
                @Override protected void afterHookedMethod(MethodHookParam p){
                    Activity activity=(Activity)p.thisObject;
                    if(!activity.getClass().getName().equals("com.miHoYo.sdk.platform.SdkActivity")||!configured(activity))return;
                    activity.getWindow().getDecorView().post(()->{
                        try{
                            Object owner=pendingManager;
                            if(owner==null)owner=XposedHelpers.callStaticMethod(manager,"getInstance");
                            Object callback=XposedHelpers.callMethod(owner,"getCallback");
                            if(callback==null||callback==completedCallback)return;
                            Session session=active;
                            if(session==null||!session.valid()||session.originalCallback!=callback){
                                if(session!=null)session.finish();
                                session=new Session(activity,owner,callback,loader);active=session;
                            }
                            if(session.qr==null||!session.qr.authenticating)decorate(activity.getWindow().getDecorView(),activity,session);
                        }catch(Throwable e){phase=-4;}
                    });
                }
            });
            installed=1;
        }catch(Throwable e){installed=-1;XposedBridge.log("vovotool QR bridge unavailable: "+e.getClass().getSimpleName());}
    }
    private static void watchSuccess(Object callback){
        for(Class<?> type=callback.getClass();type!=Object.class&&type!=null;type=type.getSuperclass()){
            if(!successHooks.add(type))continue;
            XposedBridge.hookAllMethods(type,"onSuccess",new XC_MethodHook(){
                @Override protected void afterHookedMethod(MethodHookParam p){
                    Session session=active;
                    if(p.hasThrowable()||session==null||session.originalCallback!=p.thisObject||session.qr==null||!session.qr.authenticating)return;
                    completedCallback=p.thisObject;pendingCallback=null;phase=5;
                    session.finish();
                    main.post(()->{
                        try{
                            XposedHelpers.callMethod(session.porte,"closeLoginUI");
                            Class<?> stackClass=XposedHelpers.findClass("com.mihoyo.combo.plugin.ui.UIStack",session.loader);
                            Object stack=XposedHelpers.getStaticObjectField(stackClass,"INSTANCE");
                            String page="Login.AccountListLogin";
                            if((Boolean)XposedHelpers.callMethod(stack,"contains",page)){
                                Object ui=XposedHelpers.getStaticObjectField(XposedHelpers.findClass("com.mihoyo.combo.plugin.ui.ReplaceableUIManager",session.loader),"INSTANCE");
                                XposedHelpers.callMethod(ui,"closeUserInterface",page);
                            }
                            XposedBridge.log("vovotool QR login completed; SDK login pages closed");
                        }catch(Throwable e){XposedBridge.log("vovotool QR login UI close failed: "+e.getClass().getSimpleName());}
                    });
                }
            });
        }
    }
    private static int dp(Context c,int value){return (int)(value*c.getResources().getDisplayMetrics().density+.5f);}
    private static void decorate(View root,Context context,Session session){
        if(!(root instanceof FrameLayout)||!session.valid()||!configured(context)||decorated.get(root)==session)return;
        Button button=new Button(context);button.setText("扫码登录");button.setTextColor(Color.rgb(160,124,51));button.setTextSize(13);
        FrameLayout.LayoutParams layout=new FrameLayout.LayoutParams(dp(context,104),dp(context,44),Gravity.TOP|Gravity.LEFT);
        layout.leftMargin=dp(context,12);layout.topMargin=dp(context,12);
        button.setTag("vovotool.qr.button");
        button.setOnClickListener(v->{if(session.valid()&&configured(context))session.open(context);});
        ((FrameLayout)root).addView(button,layout);decorated.put(root,session);session.button=button;
    }
    private static final class Session {
        final Activity activity;final Object porte,originalCallback;final ClassLoader loader;
        volatile boolean finished;View button;QrDialog qr;
        Session(Activity activity,Object porte,Object callback,ClassLoader loader){this.activity=activity;this.porte=porte;originalCallback=callback;this.loader=loader;watchSuccess(callback);}
        boolean valid(){return !finished&&!activity.isFinishing()&&!activity.isDestroyed();}
        void open(Context context){if(qr!=null&&(qr.isShowing()||qr.authenticating))return;qr=new QrDialog(context,this);qr.show();qr.start();}
        void finish(){
            finished=true;if(active==this)active=null;
            main.post(()->{
                if(qr!=null&&qr.isShowing())qr.dismiss();
                synchronized(decorated){
                    java.util.Iterator<Map.Entry<View,Session>> entries=decorated.entrySet().iterator();
                    while(entries.hasNext()){
                        Map.Entry<View,Session> entry=entries.next();if(entry.getValue()!=this)continue;
                        View item=entry.getKey().findViewWithTag("vovotool.qr.button");
                        if(item!=null&&item.getParent() instanceof ViewGroup)((ViewGroup)item.getParent()).removeView(item);
                        entries.remove();
                    }
                }
                button=null;
            });
        }
    }
    private static final class Credential {
        final int uid;final String token,mid;final boolean v2;
        Credential(JSONObject account)throws Exception{
            uid=Integer.parseInt(account.getString("uid"));token=account.getString("token");mid=account.optString("mid","");v2=account.optBoolean("is_v2_token",false);
            if(uid<=0||token.isEmpty())throw new IllegalStateException();
        }
    }
    private static final class QrDialog extends Dialog {
        final Session session;final AtomicInteger generation=new AtomicInteger();
        ImageView code;TextView message;boolean authenticating;Credential credential;
        QrDialog(Context context,Session session){super(context);this.session=session;}
        @Override protected void onCreate(Bundle state){
            super.onCreate(state);setTitle("扫码登录原神");
            LinearLayout layout=new LinearLayout(getContext());layout.setOrientation(LinearLayout.VERTICAL);layout.setPadding(dp(getContext(),20),dp(getContext(),12),dp(getContext(),20),dp(getContext(),12));
            code=new ImageView(getContext());layout.addView(code,new LinearLayout.LayoutParams(dp(getContext(),260),dp(getContext(),260)));
            message=new TextView(getContext());message.setText("正在生成二维码");message.setPadding(0,dp(getContext(),12),0,dp(getContext(),12));layout.addView(message);
            Button refresh=new Button(getContext());refresh.setText("刷新二维码");refresh.setOnClickListener(v->{if(!authenticating)start();});layout.addView(refresh);
            Button close=new Button(getContext());close.setText("返回账号登录");close.setOnClickListener(v->{if(!authenticating)dismiss();});layout.addView(close);setContentView(layout);
            setOnDismissListener(v->{generation.incrementAndGet();code.setImageDrawable(null);});
        }
        boolean current(int job){return job==generation.get()&&isShowing()&&session.valid();}
        void status(int job,String text){main.post(()->{if(current(job))message.setText(text);});}
        void start(){
            int job=generation.incrementAndGet();phase=1;sdkCode=0;message.setText("正在生成二维码");code.setImageDrawable(null);opens.incrementAndGet();
            Thread worker=new Thread(()->run(job),"vovotoolQrLogin");worker.setDaemon(true);worker.start();
        }
        void run(int job){
            String stage="请求二维码";
            try{
                Map<String,String> headers=sdkHeaders(session.loader);
                String device=headers.get("x-rpc-device_id");if(device==null||device.isEmpty())throw new IllegalStateException();
                JSONObject data=request("fetch",new JSONObject().put("app_id",APP_ID).put("device",device),headers);
                stage="解析二维码";
                String qrUrl=data.getString("url");android.net.Uri uri=android.net.Uri.parse(qrUrl);
                if(!"https".equals(uri.getScheme())||!"user.mihoyo.com".equals(uri.getHost()))throw new IllegalStateException();
                if(!APP_ID.equals(uri.getQueryParameter("app_id"))||!"hk4e_cn".equals(uri.getQueryParameter("biz_key")))throw new IllegalStateException();
                String ticket=uri.getQueryParameter("ticket");if(ticket==null||ticket.isEmpty())throw new IllegalStateException();
                if(!current(job))return;
                stage="绘制二维码";Bitmap bitmap=qrBitmap(qrUrl,session.loader);
                main.post(()->{if(current(job)){code.setImageBitmap(bitmap);message.setText("请用手机原神或米游社扫码，并在手机上确认登录");}else bitmap.recycle();});
                phase=2;long deadline=SystemClock.elapsedRealtime()+120000;
                while(current(job)&&SystemClock.elapsedRealtime()<deadline){
                    Thread.sleep(1500);if(!current(job))return;
                    stage="查询扫码状态";JSONObject query=request("query",new JSONObject().put("app_id",APP_ID).put("device",device).put("ticket",ticket),headers);
                    String state=query.optString("stat");
                    if("Scanned".equals(state)){phase=3;status(job,"已扫码，请在手机上确认登录");}
                    else if("Confirmed".equals(state)){
                        JSONObject payload=query.getJSONObject("payload");if(!"Account".equals(payload.optString("proto")))throw new IllegalStateException();
                        Credential credential=new Credential(new JSONObject(payload.getString("raw")));
                        XposedBridge.log("vovotool QR credential v2="+credential.v2+" hasMid="+!credential.mid.isEmpty());
                        main.post(()->authenticate(job,credential));return;
                    }
                }
                if(current(job)){phase=-2;status(job,"二维码已过期，请刷新");}
            }catch(Exception e){if(current(job)){
                Throwable cause=e;while(cause.getCause()!=null&&cause.getCause()!=cause)cause=cause.getCause();
                phase=-1;sdkCode=cause instanceof QrRequestException?((QrRequestException)cause).code:0;
                String reason=cause instanceof QrRequestException?"接口 "+sdkCode+" "+cause.getMessage():cause.getClass().getSimpleName();
                XposedBridge.log("vovotool QR "+stage+": "+reason);
                status(job,stage+"失败（"+reason+"），请刷新重试");
            }}
        }
        void authenticate(int job,Credential credential){
            if(!current(job))return;authenticating=true;phase=4;message.setText("已授权，正在通过游戏账号 SDK 验证登录");setCancelable(false);
            // The SDK treats any empty field as a request to use a cached account.
            // A nonempty display label keeps verification bound to this QR authorization.
            String accountId=Integer.toString(credential.uid);
            try{this.credential=credential;qrVerification.set(this);tokenLogin.invoke(session.porte,credential.token,accountId,accountId);dismiss();}
            catch(Exception e){authenticating=false;setCancelable(true);phase=-3;message.setText("账号 SDK 未能完成登录，请返回后重试");}
            finally{qrVerification.remove();this.credential=null;}
        }
    }
    private static final class QrRequestException extends Exception {
        final int code;QrRequestException(int code,String message){super(message);this.code=code;}
    }
    private static Map<String,String> sdkHeaders(ClassLoader loader){
        Class<?> client=XposedHelpers.findClass("com.mihoyo.combo.common.ComboNetClient",loader);
        Object companion=XposedHelpers.getStaticObjectField(client,"Companion");
        Map<?,?> common=(Map<?,?>)XposedHelpers.callMethod(companion,"getHttpHeader");
        Map<String,String> headers=new java.util.HashMap<>();
        for(String key:new String[]{"x-rpc-sys_version","x-rpc-device_model","x-rpc-device_name","x-rpc-client_type","x-rpc-channel_id","x-rpc-mdk_version","x-rpc-channel_version","x-rpc-sdk_version","x-rpc-language","x-rpc-device_id","x-rpc-app_id","x-rpc-game_biz","x-rpc-device_fp","x-rpc-lifecycle_id"}){
            Object value=common.get(key);if(value instanceof String)headers.put(key,(String)value);
        }
        XposedBridge.log("vovotool QR SDK header names="+headers.keySet());
        return headers;
    }
    private static JSONObject request(String action,JSONObject body,Map<String,String> headers)throws Exception{
        HttpURLConnection connection=(HttpURLConnection)new URL(ENDPOINT+action).openConnection();
        try{
            connection.setRequestMethod("POST");connection.setConnectTimeout(8000);connection.setReadTimeout(8000);connection.setDoOutput(true);connection.setInstanceFollowRedirects(false);
            for(Map.Entry<String,String> header:headers.entrySet())connection.setRequestProperty(header.getKey(),header.getValue());
            connection.setRequestProperty("Content-Type","application/json");byte[] bytes=body.toString().getBytes("UTF-8");connection.setFixedLengthStreamingMode(bytes.length);
            try(java.io.OutputStream output=connection.getOutputStream()){output.write(bytes);}
            int http=connection.getResponseCode();if(http!=200)throw new QrRequestException(10000+http,"HTTP "+http);
            ByteArrayOutputStream buffer=new ByteArrayOutputStream();
            try(InputStream input=connection.getInputStream()){
                byte[] chunk=new byte[4096];int count;
                while((count=input.read(chunk))!=-1){if(buffer.size()+count>65536)throw new IllegalStateException();buffer.write(chunk,0,count);}
            }
            JSONObject result=new JSONObject(buffer.toString("UTF-8"));int retcode=result.getInt("retcode");
            if(retcode!=0){
                // Only the short public error caption; never include payloads or authorization URLs.
                String caption=result.optString("message","").replaceAll("[^\\u4e00-\\u9fff，。！？、：；（） ]","");
                if(caption.length()>80)caption=caption.substring(0,80);
                throw new QrRequestException(retcode,caption);
            }
            return result.getJSONObject("data");
        }finally{connection.disconnect();}
    }

    private static Bitmap qrBitmap(String value,ClassLoader loader)throws Exception{
        Class<?> format=XposedHelpers.findClass("com.google.zxing.BarcodeFormat",loader);Object qr=format.getField("QR_CODE").get(null);
        Object writer=XposedHelpers.findClass("com.google.zxing.qrcode.QRCodeWriter",loader).getDeclaredConstructor().newInstance();
        Object matrix=writer.getClass().getMethod("encode",String.class,format,int.class,int.class).invoke(writer,value,qr,520,520);
        Method get=matrix.getClass().getMethod("get",int.class,int.class);int[] pixels=new int[520*520];
        for(int y=0;y<520;y++)for(int x=0;x<520;x++)pixels[y*520+x]=(Boolean)get.invoke(matrix,x,y)?Color.BLACK:Color.WHITE;
        return Bitmap.createBitmap(pixels,520,520,Bitmap.Config.ARGB_8888);
    }
}
