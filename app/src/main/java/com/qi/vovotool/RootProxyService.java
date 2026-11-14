package com.qi.vovotool;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.SecureRandom;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

public final class RootProxyService extends Service {
    static volatile RootProxyService instance;
    static volatile int port;
    static volatile String token="";
    static volatile String status="未启动";
    private final AtomicBoolean starting=new AtomicBoolean();
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private volatile java.lang.Process process;
    private volatile boolean stopped;

    @Override public void onCreate(){
        super.onCreate();instance=this;
        NotificationManager manager=getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("input","触控板兼容",NotificationManager.IMPORTANCE_LOW));
        PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,MainActivity.class),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        Notification notification=new Notification.Builder(this,"input").setSmallIcon(android.R.drawable.ic_menu_manage).setContentTitle("沃沃头 · 触控板兼容").setContentText("仅在游戏捕获指针时转换触控板输入").setContentIntent(open).setOngoing(true).build();
        startForeground(10,notification);
    }
    @Override public int onStartCommand(Intent intent,int flags,int startId){ensureHelper();return START_NOT_STICKY;}
    void ensureHelper(){
        if(stopped || port!=0 || !starting.compareAndSet(false,true))return;
        worker.execute(()->{
            StringBuilder generated=new StringBuilder();byte[] random=new byte[16];new SecureRandom().nextBytes(random);
            for(byte value:random)generated.append(String.format(java.util.Locale.ROOT,"%02x",value&255));
            String secret=generated.toString();
            status="等待 root 授权或辅助进程启动";
            try {
                String binary=getApplicationInfo().nativeLibraryDir+"/libtouchpad_proxy.so";
                String command="'"+binary.replace("'","'\\''")+"' --token "+secret;
                java.lang.Process child=new ProcessBuilder("su","-c",command).redirectErrorStream(true).start();process=child;
                try(BufferedReader reader=new BufferedReader(new InputStreamReader(child.getInputStream()))){
                    String line;
                    while(!stopped && (line=reader.readLine())!=null){
                        if(line.startsWith("READY ")){
                            int number=Integer.parseInt(line.substring(6).trim());
                            if(number>0&&number<=65535){token=secret;port=number;status="已就绪，等待游戏输入";}
                        }else if(!line.trim().isEmpty())status=line.length()>160?line.substring(0,160):line;
                    }
                }
                child.waitFor();
                if(!stopped && port!=0)status="待游戏重新连接";
            }catch(Exception e){if(!stopped)status="启动失败："+e.getClass().getSimpleName();}
            finally{port=0;token="";process=null;starting.set(false);}
        });
    }
    private void stopProxy(){
        final int destination=port;final String secret=token;port=0;token="";
        if(destination<=0||secret.length()!=32)return;
        try(DatagramSocket socket=new DatagramSocket()){
            ByteBuffer data=ByteBuffer.allocate(32).order(ByteOrder.LITTLE_ENDIAN);data.putInt(0x47495043);
            for(int i=0;i<16;i++)data.put((byte)Integer.parseInt(secret.substring(i*2,i*2+2),16));
            data.putInt(0).putFloat(.25f).putInt(2);
            byte[] bytes=data.array();socket.send(new DatagramPacket(bytes,bytes.length,InetAddress.getByName("127.0.0.1"),destination));
        }catch(Exception ignored){}
    }
    @Override public void onDestroy(){
        stopped=true;stopProxy();if(process!=null)process.destroy();worker.shutdownNow();
        instance=null;status="已停止";super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent){return null;}
}
