#include "lsposed_native.hpp"
#include "profile.hpp"
#include "protocol.hpp"
#include <jni.h>
#include <android/log.h>
#include <arpa/inet.h>
#include <link.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include "render_probe.hpp"

namespace {
using P=uintptr_t;
std::atomic<P> base{0};
std::atomic<int> stage{0},cursor{1000},inputMode{-1};
std::atomic<bool> enabled{false},captureEnabled{true},captured{false},windowFocused{false};
std::atomic<float> mouseScale{0.02f};
std::atomic<unsigned long> ticks{0},mouseFrames{0},motionEvents{0};
std::atomic<unsigned long> keyEvents{0},keyboardFrames{0};
std::atomic<unsigned long> escapeDispatches{0};
std::atomic<int64_t> suppressUntil{0};
HookFunction hookFunction=nullptr;
void(*originalTick)(void*,void*)=nullptr;
void(*originalMouse)(void*,void*,void*)=nullptr;
void(*originalKeyboard)(void*,void*,void*)=nullptr;
bool(*originalEscapeRoute)(void*,void*)=nullptr;
void(*originalDeviceSetter)(void*,int,bool,void*)=nullptr;
bool(*originalModeSwitch)(void*,int,int,void*)=nullptr;
std::atomic<bool> focusGuard{true};
std::atomic<int64_t> focusWarmUntil{0};
std::atomic<unsigned long> applyCalls{0},deviceRequests{0},modeRequests{0},blockedDevices{0},blockedModes{0};
std::atomic<int> lastDevice{-1},lastMode{-1},lastSwitchCaller{0};
using DeviceSetter=void(*)(void*,int,bool,void*);
using ControllerSetter=void(*)(void*,bool,void*);
using MapsSetter=void(*)(void*,int,void*);
std::mutex inputMutex,proxyMutex,installMutex,keyMutex;
unsigned heldKeys=0,pendingKeys=0;
constexpr int specialKeyCodes[]={13,27,271}; // Unity/Rewired Return, Escape, KeypadEnter.
float relativeX=0,relativeY=0,scrollY=0,scrollX=0;
unsigned buttons=0,pendingButtons=0;
int udp=-1; sockaddr_in endpoint{}; ProxyPacket packet{};
int64_t lastProxy=0;
bool applied=false;
P manager=0,input=0,binding=0,platform=0,keyboard=0,mouse=0,mouseSource=0,keyboardSource=0;
struct Baseline { int device=0,mode=0; uint8_t disableKeyboard=1,disableMouse=1,keyboard=0,mouse=0; } baseline;
int64_t nowMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void log(const char* s){__android_log_write(ANDROID_LOG_INFO,"vovotool",s);}
P clean(P p){return p&0x00ffffffffffffffULL;}
bool readMemory(P p,void* dst,size_t n){
    if(clean(p)<4096)return false;
    iovec local{dst,n},remote{reinterpret_cast<void*>(clean(p)),n};
    return process_vm_readv(getpid(),&local,1,&remote,1,0)==static_cast<ssize_t>(n);
}
template<class T>T read(P p,T fallback={}){T v;return readMemory(p,&v,sizeof(v))?v:fallback;}
template<class T>void write(P p,T value){std::memcpy(reinterpret_cast<void*>(clean(p)),&value,sizeof(value));}
bool classIs(P object,const char* name){
    P klass=read<P>(object),text=read<P>(klass+0x50);char actual[80]{};
    const size_t length=std::strlen(name)+1;
    return length<sizeof(actual)&&readMemory(text,actual,length)&&std::memcmp(actual,name,length)==0;
}
#include "uid_visibility.hpp"
#include "crafting.hpp"
#include "expedition.hpp"
#include "daily_claim.hpp"
#include "graphics.hpp"
#include "photo.hpp"
bool ready(P b){
    if((read<uint8_t>(b+0x158099bc)&1)==0)return false;
    P root=read<P>(b+profile::singletonRoot);
    P m=read<P>(root+0x8bc0),i=read<P>(root+0x8e20),s=read<P>(root+0x1030),v=read<P>(root+0xf48);
    P p=read<P>(v+0x88),k=read<P>(s+0x58),mou=read<P>(s+0x30),bind=read<P>(i+0x20);
    if(!classIs(m,"BOMCNPDGJOP")||!classIs(i,"MHCNPJOGAGH")||!classIs(bind,"JGAPEPKBJJP")||
       !classIs(v,"ConfigVars")||!classIs(p,"PlatformVars")||!classIs(k,"Keyboard")||!classIs(mou,"Mouse"))return false;
    P source=read<P>(mou+0x148),keys=read<P>(k+0x100);
    if(!classIs(source,"UnityUnifiedMouseSource")||!classIs(keys,"UnityUnifiedKeyboardSource")||!classIs(read<P>(bind+0xa0),"Player"))return false;
    manager=m;input=i;binding=bind;platform=p;keyboard=k;mouse=mou;mouseSource=source;keyboardSource=keys;return true;
}
void apply(P b){
    applyCalls++;
    if(!applied){
        baseline={read<int>(manager+0x370),read<int>(input+0x1a4),read<uint8_t>(platform+0x10),read<uint8_t>(platform+0x11),read<uint8_t>(keyboard+0xcd),read<uint8_t>(mouse+0xcd)};
    }
    write<uint8_t>(platform+0x10,0);write<uint8_t>(platform+0x11,0);
    auto setController=reinterpret_cast<ControllerSetter>(b+profile::setController);
    setController(reinterpret_cast<void*>(keyboard),true,nullptr);
    setController(reinterpret_cast<void*>(mouse),true,nullptr);
    originalDeviceSetter(reinterpret_cast<void*>(manager),1,true,nullptr);
    write<int>(input+0x1a4,2);
    reinterpret_cast<MapsSetter>(b+profile::selectMaps)(reinterpret_cast<void*>(binding),2,nullptr);
    applied=true;stage=2;log("PC UI and Rewired keyboard/mouse rule set applied");
}
void restore(P b){
    if(!applied)return;
    write<int>(input+0x1a4,baseline.mode);
    reinterpret_cast<MapsSetter>(b+profile::selectMaps)(reinterpret_cast<void*>(binding),baseline.mode,nullptr);
    originalDeviceSetter(reinterpret_cast<void*>(manager),baseline.device,true,nullptr);
    reinterpret_cast<ControllerSetter>(b+profile::setController)(reinterpret_cast<void*>(keyboard),baseline.keyboard!=0,nullptr);
    reinterpret_cast<ControllerSetter>(b+profile::setController)(reinterpret_cast<void*>(mouse),baseline.mouse!=0,nullptr);
    write<uint8_t>(platform+0x10,baseline.disableKeyboard);write<uint8_t>(platform+0x11,baseline.disableMouse);
    applied=false;stage=3;captured=false;log("Runtime input baseline restored");
}
bool pcModeLocked(){return focusGuard&&enabled&&stage==2&&applied&&read<int>(manager+0x370,-1)==1&&read<int>(input+0x1a4,-1)==2;}
void deviceSetterHook(void* self,int device,bool refresh,void* method){
    deviceRequests++;lastDevice=device;
    if(reinterpret_cast<P>(self)==manager&&device!=1&&pcModeLocked()){
        blockedDevices++;log("Automatic non-PC UI device change suppressed");return;
    }
    originalDeviceSetter(self,device,refresh,method);
}
bool modeSwitchHook(void* self,int device,int mode,void* method){
    modeRequests++;lastDevice=device;lastMode=mode;
    lastSwitchCaller=static_cast<int>(reinterpret_cast<P>(__builtin_return_address(0))-base.load());
    if(reinterpret_cast<P>(self)==input&&pcModeLocked()&&(device!=1||mode!=2||nowMs()<focusWarmUntil)){
        blockedModes++;
        __android_log_print(ANDROID_LOG_INFO,"vovotool","Automatic input/UI rebuild suppressed: device=%d mode=%d caller=0x%x",device,mode,lastSwitchCaller.load());
        return false;
    }
    __android_log_print(ANDROID_LOG_INFO,"vovotool","Native input/UI switch: device=%d mode=%d caller=0x%x",device,mode,lastSwitchCaller.load());
    return originalModeSwitch(self,device,mode,method);
}
void heartbeat(){
    const auto now=nowMs();if(now-lastProxy<100)return;lastProxy=now;
    std::lock_guard<std::mutex> guard(proxyMutex);
    if(udp<0||endpoint.sin_port==0)return;
    packet.sequence++;
    packet.enabled=enabled&&applied&&captureEnabled&&captured&&windowFocused&&cursor==-1?1:0;
    sendto(udp,&packet,sizeof(packet),MSG_DONTWAIT,reinterpret_cast<sockaddr*>(&endpoint),sizeof(endpoint));
}
bool escapeRouteHook(void* self,void* method){
    const P caller=reinterpret_cast<P>(__builtin_return_address(0));
    // Only the main-page Cancel action's branch uses the shared in-game Back path.
    // Other callers retain the actual controller-mode result and its side effects.
    if(enabled&&stage==2&&windowFocused&&inputMode==2&&reinterpret_cast<P>(self)==input&&
       caller==base.load()+profile::escapeRouteCall+4){
        escapeDispatches++;
        log("Escape action routed to the original in-game Back handler");
        return true;
    }
    return originalEscapeRoute(self,method);
}
void tickHook(void* self,void* method){
    originalTick(self,method);ticks++;
    static thread_local bool entered=false;if(entered)return;
    entered=true;
    const P b=base.load();
    try {
        if(b)updateGraphics(b);
        if(b&&ready(b)&&reinterpret_cast<P>(self)==manager){
            if(enabled){if(!applied||read<int>(manager+0x370)!=1||read<int>(input+0x1a4)!=2)apply(b);}
            else restore(b);
            cursor=read<int>(manager+0x340,1000);inputMode=read<int>(input+0x1a4,-1);
            updateUidVisibility(b);
            updateCrafting(b);
            updateExpedition(b);
            updateDaily(b);
            heartbeat();
        }
    }catch(...){enabled=false;stage=-3;log("Native input call failed; module disabled for this process");}
    entered=false;
}
void mouseHook(void* self,void* data,void* method){
    originalMouse(self,data,method);
    if(!enabled||!applied||reinterpret_cast<P>(self)!=mouseSource)return;
    const P axes=read<P>(reinterpret_cast<P>(data)+0x30);
    if(!classIs(axes,"Single[]")||read<int>(axes+0x18)!=4)return;
    mouseFrames++;
    float values[4]{};if(!readMemory(axes+0x20,values,sizeof(values)))return;
    if(captureEnabled&&captured&&windowFocused&&cursor==-1){
        std::lock_guard<std::mutex> guard(inputMutex);
        values[0]=relativeX;values[1]=relativeY;values[2]=scrollY;values[3]=scrollX;
        relativeX=relativeY=scrollY=scrollX=0;
        P buttonsArray=read<P>(reinterpret_cast<P>(data)+0x10);
        if(classIs(buttonsArray,"Boolean[]")&&read<int>(buttonsArray+0x18)==7){
            const unsigned current=buttons|pendingButtons;pendingButtons=0;
            for(unsigned n=0;n<7;n++)write<uint8_t>(buttonsArray+0x20+n,(current>>n)&1);
        }
        write<float>(axes+0x28,values[2]);write<float>(axes+0x2c,values[3]);
    }else if(nowMs()<suppressUntil){values[0]=values[1]=0;}
    const float scale=mouseScale.load();
    for(unsigned n=0;n<2;n++)if(std::isfinite(values[n]))write<float>(axes+0x20+n*4,values[n]*scale);
}
void keyboardHook(void* self,void* data,void* method){
    originalKeyboard(self,data,method);
    if(!enabled||!applied||reinterpret_cast<P>(self)!=keyboardSource)return;
    P keys=read<P>(reinterpret_cast<P>(data)+0x10),map=read<P>(keyboard+0x108);
    if(!classIs(keys,"Boolean[]")||!classIs(map,"Int32[]"))return;
    int count=read<int>(keys+0x18),length=read<int>(map+0x18);
    if(count!=132||length<=271||length>512)return;
    std::lock_guard<std::mutex> guard(keyMutex);
    if(!windowFocused){heldKeys=pendingKeys=0;}
    const unsigned pressed=heldKeys|pendingKeys;
    pendingKeys=0;
    for(unsigned n=0;n<3;n++){
        int index=read<int>(map+0x20+specialKeyCodes[n]*4,-1);
        if(index>=0&&index<count)write<uint8_t>(keys+0x20+index,(pressed>>n)&1);
    }
    keyboardFrames++;
}
void install(P address){
    std::lock_guard<std::mutex> guard(installMutex);
    if(base||!hookFunction)return;
    for(const auto& pin:profile::pins){unsigned char bytes[16];
        if(!readMemory(address+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)!=0){stage=-1;log("Version/code fingerprint mismatch; no hooks installed");return;}}
    if(hookFunction(reinterpret_cast<void*>(address+profile::setDevice),reinterpret_cast<void*>(deviceSetterHook),reinterpret_cast<void**>(&originalDeviceSetter))!=0){stage=-2;log("UI device guard installation failed");return;}
    if(hookFunction(reinterpret_cast<void*>(address+profile::switchInputMode),reinterpret_cast<void*>(modeSwitchHook),reinterpret_cast<void**>(&originalModeSwitch))!=0){stage=-2;log("Input mode guard installation failed");return;}
    if(hookFunction(reinterpret_cast<void*>(address+profile::uidTextSetter),reinterpret_cast<void*>(uidTextHook),reinterpret_cast<void**>(&originalUidText))!=0){stage=-2;log("UID text hook installation failed");return;}
    if(hookFunction(reinterpret_cast<void*>(address+profile::textPopulateMesh),reinterpret_cast<void*>(uidMeshHook),reinterpret_cast<void**>(&originalTextMesh))!=0){stage=-2;log("UID discovery hook installation failed");return;}
    if(hookFunction(reinterpret_cast<void*>(address+profile::escapeRoutePredicate),reinterpret_cast<void*>(escapeRouteHook),reinterpret_cast<void**>(&originalEscapeRoute))!=0){stage=-2;log("Escape route hook installation failed");return;}
    if(hookFunction(reinterpret_cast<void*>(address+profile::keyboardUpdate),reinterpret_cast<void*>(keyboardHook),reinterpret_cast<void**>(&originalKeyboard))!=0){stage=-2;log("Keyboard hook installation failed");return;}
    if(hookFunction(reinterpret_cast<void*>(address+profile::mouseUpdate),reinterpret_cast<void*>(mouseHook),reinterpret_cast<void**>(&originalMouse))!=0){stage=-2;log("Mouse hook installation failed");return;}
    if(hookFunction(reinterpret_cast<void*>(address+profile::tick),reinterpret_cast<void*>(tickHook),reinterpret_cast<void**>(&originalTick))!=0){stage=-2;log("Tick hook installation failed");return;}
    base=address;stage=1;installPhoto(address);installGraphics(address);log("Native hooks installed for pinned 7.2 ARM64 library");
}
int findModule(dl_phdr_info* info,size_t,void*){
    const char* name=info->dlpi_name;
    if(name&&std::strstr(name,"libyuanshen.so")){install(info->dlpi_addr);return 1;}return 0;
}
void loaded(const char* name,void*){if(name&&std::strstr(name,"libyuanshen.so"))dl_iterate_phdr(findModule,nullptr);}
}

extern "C" __attribute__((visibility("default"),used)) ModuleLoaded native_init(const NativeAPIEntries* api){
    if(!api||!api->hook_func)return nullptr;
    hookFunction=api->hook_func;dl_iterate_phdr(findModule,nullptr);return loaded;
}
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*,void*){log("JNI loaded");return JNI_VERSION_1_6;}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configure(JNIEnv* env,jclass,jboolean active,jfloat scale,jboolean capture,jint port,jstring token,jfloat gain){
    enabled=active;captureEnabled=capture;
    if(std::isfinite(scale)&&scale>=0.0001f&&scale<=2)mouseScale=scale;
    std::lock_guard<std::mutex> guard(proxyMutex);
    endpoint={};
    if(port<=0||port>65535||!token)return;
    const char* text=env->GetStringUTFChars(token,nullptr);
    if(std::strlen(text)==32){
        bool valid=true;
        for(unsigned n=0;n<16;n++){char piece[3]={text[n*2],text[n*2+1],0};char* end=nullptr;unsigned long value=std::strtoul(piece,&end,16);if(end!=piece+2){valid=false;break;}packet.token[n]=value;}
        if(valid){
            endpoint.sin_family=AF_INET;endpoint.sin_addr.s_addr=htonl(INADDR_LOOPBACK);endpoint.sin_port=htons(port);
            if(udp<0)udp=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
            packet.gain=std::isfinite(gain)&&gain>=0.05f&&gain<=2?gain:0.25f;
        }
    }
    env->ReleaseStringUTFChars(token,text);
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_state(JNIEnv* env,jclass){
    jint values[]={stage.load(),inputMode.load(),cursor.load(),enabled.load()?1:0,static_cast<jint>(ticks.load()),static_cast<jint>(mouseFrames.load()),static_cast<jint>(motionEvents.load()),static_cast<jint>(keyEvents.load()),static_cast<jint>(keyboardFrames.load()),static_cast<jint>(escapeDispatches.load())};
    jintArray result=env->NewIntArray(10);env->SetIntArrayRegion(result,0,10,values);return result;
}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_setCaptureState(JNIEnv*,jclass,jboolean cap,jboolean focus){
    if(windowFocused.exchange(focus)!=static_cast<bool>(focus)&&focus)focusWarmUntil=nowMs()+400;
    if(!focus){std::lock_guard<std::mutex> guard(keyMutex);heldKeys=pendingKeys=0;craftKeyHeld=false;dailyKeyHeld=false;expeditionKeyHeld=false;}
    if(captured.exchange(cap)!=static_cast<bool>(cap)){
        std::lock_guard<std::mutex> guard(inputMutex);relativeX=relativeY=scrollX=scrollY=0;buttons=pendingButtons=0;suppressUntil=nowMs()+150;
    }
}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_motion(JNIEnv*,jclass,jfloat x,jfloat y,jfloat vertical,jfloat horizontal,jint mask){
    if(!captured||!captureEnabled||!windowFocused||!std::isfinite(x)||!std::isfinite(y))return;
    std::lock_guard<std::mutex> guard(inputMutex);
    relativeX+=x;relativeY+=y;scrollY+=vertical;scrollX+=horizontal;
    pendingButtons|=static_cast<unsigned>(mask)&~buttons;buttons=mask;motionEvents++;
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_qi_vovotool_NativeBridge_key(JNIEnv*,jclass,jint code,jboolean down){
    if(!enabled||stage!=2||!windowFocused)return false;
    unsigned mask=code==66?1:code==111?2:code==160?4:0;
    if(!mask)return false;
    std::lock_guard<std::mutex> guard(keyMutex);
    if(down){pendingKeys|=mask&~heldKeys;heldKeys|=mask;}else heldKeys&=~mask;
    keyEvents++;
    return true;
}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureUid(JNIEnv*,jclass,jboolean hide){uidRequested=hide;}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureCraft(JNIEnv*,jclass,jboolean active){craftEnabled=active;}
extern "C" JNIEXPORT jboolean JNICALL Java_com_qi_vovotool_NativeBridge_craftKey(JNIEnv*,jclass,jboolean down){
    if(!down){craftKeyHeld=false;return true;}
    if(!enabled||!craftEnabled||stage!=2||!windowFocused)return false;
    if(!craftKeyHeld.exchange(true))requestCrafting();
    return true;
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_craftState(JNIEnv* env,jclass){
    jint values[]={craftStatus.load(),craftEnabled.load()?1:0,static_cast<jint>(craftRequests.load()),static_cast<jint>(craftDispatches.load()),static_cast<jint>(craftOpened.load()),static_cast<jint>(craftRejected.load())};
    jintArray result=env->NewIntArray(6);env->SetIntArrayRegion(result,0,6,values);return result;
}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureDaily(JNIEnv*,jclass,jboolean active){dailyEnabled=active;}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureExpedition(JNIEnv*,jclass,jboolean active){expeditionEnabled=active;}
extern "C" JNIEXPORT jboolean JNICALL Java_com_qi_vovotool_NativeBridge_expeditionKey(JNIEnv*,jclass,jboolean down){
    if(!down){expeditionKeyHeld=false;return true;}
    if(!enabled||!expeditionEnabled||stage!=2||!windowFocused)return false;
    if(!expeditionKeyHeld.exchange(true))requestExpedition();
    return true;
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_expeditionState(JNIEnv* env,jclass){
    jint values[]={expeditionStatus.load(),expeditionEnabled.load()?1:0,static_cast<jint>(expeditionRequests.load()),static_cast<jint>(expeditionDispatches.load()),static_cast<jint>(expeditionOpened.load()),static_cast<jint>(expeditionRejected.load())};
    jintArray result=env->NewIntArray(6);env->SetIntArrayRegion(result,0,6,values);return result;
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_qi_vovotool_NativeBridge_dailyKey(JNIEnv*,jclass,jboolean down){
    if(!down){dailyKeyHeld=false;return true;}
    if(!enabled||!dailyEnabled||stage!=2||!windowFocused)return false;
    if(!dailyKeyHeld.exchange(true))requestDaily();
    return true;
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_dailyState(JNIEnv* env,jclass){
    jint values[]={dailyStatus.load(),dailyEnabled.load()?1:0,static_cast<jint>(dailyRequests.load()),static_cast<jint>(dailyCalls.load()),static_cast<jint>(dailyConfirmed.load()),static_cast<jint>(dailyRejected.load())};
    jintArray result=env->NewIntArray(6);env->SetIntArrayRegion(result,0,6,values);return result;
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_uidState(JNIEnv* env,jclass){
    jint values[]={uidStatus.load(),uidRequested.load()?1:0,uidApplied.load()?1:0,static_cast<jint>(uidDiscoveries.load()),static_cast<jint>(uidWrites.load())};
    jintArray result=env->NewIntArray(5);env->SetIntArrayRegion(result,0,5,values);return result;
}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureFocusGuard(JNIEnv*,jclass,jboolean active){focusGuard=active;}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_focusState(JNIEnv* env,jclass){
    jint values[]={static_cast<jint>(applyCalls.load()),static_cast<jint>(deviceRequests.load()),static_cast<jint>(modeRequests.load()),static_cast<jint>(blockedDevices.load()),static_cast<jint>(blockedModes.load()),lastDevice.load(),lastMode.load(),lastSwitchCaller.load(),focusGuard.load()?1:0};
    jintArray result=env->NewIntArray(9);env->SetIntArrayRegion(result,0,9,values);return result;
}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureGraphics(JNIEnv*,jclass,jboolean active,jboolean resolution,jint w,jint h,jint nw,jint nh,jint fps,jboolean effects,jint vegetation,jint path){
    GraphicsConfig c;c.active=active;c.resolution=resolution;c.effects=effects;c.nativeWidth=nw;c.nativeHeight=nh;c.width=w;c.height=h;c.fps=fps;c.vegetation=vegetation;
    if(nw<1280||nh<720||nw>8192||nh>8192)c.active=false;
    if(w<1280||h<720||w>nw||h>nh||w%4||h%4||std::abs((float(w)/h)/(float(nw)/nh)-1)>.1f)c.resolution=false;
    if(fps!=0&&fps!=30&&fps!=45&&fps!=60&&fps!=90&&fps!=120&&fps!=144&&fps!=165)c.fps=0;
    c.vegetation=std::clamp(int(vegetation),10,100);
    c.path=std::clamp(int(path),0,3);
    std::lock_guard<std::mutex> lock(graphicsMutex);graphicsRequested=c;graphicsConfigured=true;
}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_requestRenderCapture(JNIEnv*,jclass,jlong token){renderprobe::request(token);}
extern "C" JNIEXPORT jstring JNICALL Java_com_qi_vovotool_NativeBridge_renderProbe(JNIEnv* env,jclass){return env->NewStringUTF(renderprobe::report().c_str());}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_graphicsState(JNIEnv* env,jclass){
    std::lock_guard<std::mutex> lock(graphicsMutex);
    jint values[]={graphicsError.load(),graphicsVerified?1:0,graphicsFrozen?1:0,graphicsApplied.active?1:0,
        graphicsFrozen&&!(graphicsApplied==graphicsRequested)?1:0,graphicsHooks.load(),graphicsWidth.load(),graphicsHeight.load(),graphicsFps.load(),
        static_cast<jint>(graphicsSwaps.load()),static_cast<jint>(graphicsImages.load()),static_cast<jint>(graphicsFrames.load()),static_cast<jint>(graphicsAreas.load()),static_cast<jint>(graphicsViews.load()),static_cast<jint>(graphicsScissors.load()),static_cast<jint>(graphicsQuality.load()),static_cast<jint>(graphicsFailures.load()),graphicsApplied.vegetation,graphicsApplied.effects?1:0,graphicsApplied.resolution?1:0,static_cast<jint>(graphicsEffectImages.load())};
    jintArray result=env->NewIntArray(21);env->SetIntArrayRegion(result,0,21,values);return result;
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_photoState(JNIEnv* env,jclass){
    jint values[]={photoStatus.load(),photoRequests.load(),photoReads.load(),photoAdjusted.load(),photoWidth.load(),photoHeight.load(),photoReadWidth.load(),photoReadHeight.load(),photoTextureWidth.load(),photoTextureHeight.load()};
    jintArray result=env->NewIntArray(10);env->SetIntArrayRegion(result,0,10,values);return result;
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_renderState(JNIEnv* env,jclass){
    jint values[]={renderOutputWidth.load(),renderOutputHeight.load(),graphicsSceneWidth.load(),graphicsSceneHeight.load(),
        static_cast<jint>(renderGameScale.load()*1000000),static_cast<jint>(renderAppliedScale.load()*1000000),static_cast<jint>(renderScaleCalls.load()),renderScaleStatus.load(),
        renderReadbackWidth.load(),renderReadbackHeight.load(),static_cast<jint>(renderReadbacks.load()),static_cast<jint>(renderReadbackErrors.load())};
    jintArray result=env->NewIntArray(12);env->SetIntArrayRegion(result,0,12,values);return result;
}
