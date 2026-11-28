#include "lsposed_native.hpp"
// 一份构建适配双服：默认国服表，运行时按进程名切换到国际服表。
#include "profile.hpp"
#include "profile_os.hpp"
#include "apply_os.inc"
#include "input_offsets.hpp"
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
bool inputBaselineSaved=false;
int64_t inputApplyRetryAt=0;
P manager=0,input=0,binding=0,platform=0,keyboard=0,mouse=0,mouseSource=0,keyboardSource=0;
P probedKeyboard=0,probedMouse=0;  // 国际服 probe 直接定位到的 Keyboard/Mouse 对象
struct Baseline { int device=0,mode=0; uint8_t disableKeyboard=1,disableMouse=1,keyboard=0,mouse=0; bool controllerStateKnown=false; } baseline;
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
    P klass=read<P>(object);char actual[80]{};
    const size_t length=std::strlen(name)+1;
    if(length>=sizeof(actual))return false;
    // IL2CPP 类名偏移两服不同：国服 klass+0x50，国际服 klass+0x10。都试一遍。
    const int nameOffsets[2]={0x50,0x10};
    for(int i=0;i<2;i++){
        const P text=read<P>(klass+nameOffsets[i]);
        if(readMemory(text,actual,length)&&std::memcmp(actual,name,length)==0)return true;
    }
    return false;
}
// UI 页面栈深：世界场景=1，打开任意 UI 页面>1。
// 栈偏移两服不同：国际服 manager+0x2A8，国服 manager+0x200（均为 Stack`1）。
// 不依赖 runtimeIsOS（此时尚未声明），两个偏移都试，谁命中用谁。
// 返回 -1 表示栈不可读（此时不要据此改写 cursor，保留游戏原值）。
int uiPageDepth(){
    if(!manager)return -1;
    const int offsets[2]={0x2a8,0x200};
    for(int i=0;i<2;i++){
        const P stack=read<P>(manager+offsets[i],0);
        if(stack<0x10000||!classIs(stack,"Stack`1"))continue;
        const int size=read<int>(stack+0x24,-1);
        if(size>=0&&size<=32)return size;
    }
    return -1;
}
// 是否真正在大世界：国际服栈深=1 且栈元素类名匹配大世界 HUD。
// 加载/登录页栈深也为 1，但栈元素是别的类名，必须排除，否则会被误判为大世界而独占鼠标。
bool inWorld(){
    if(!manager)return false;
    const P stack=read<P>(manager+0x2a8,0);   // 国际服偏移
    if(stack<0x10000||!classIs(stack,"Stack`1"))return false;
    if(read<int>(stack+0x24,-1)!=1)return false;
    // stack+0x18 指向元素数组，+0x20 是第一个元素
    const P arr=read<P>(stack+0x18,0);
    if(arr<0x10000)return false;
    const P elem=read<P>(arr+0x20,0);
    return classIs(elem,"BFIFKIIDFKN");
}
// 在 holder 对象里找一个偏移，使 holder+offset 指向 name 类的实例；找不到返回 0。
int locateSlot(P holder,int low,int high,const char* name){
    if(holder<0x10000)return 0;
    for(int offset=low;offset<=high;offset+=8){
        const P value=read<P>(holder+offset);
        if(value>0x10000&&classIs(value,name))return offset;
    }
    return 0;
}
// 国际服专用：从单例根把整条链路探出来。每一项都要过类名核对，探不到就当没就绪，
// 宁可功能不起来也不往错地址写。
bool probeLayout(P root){
    auto& o=inoff::active;
    o.rootManager=locateSlot(root,0x4000,0x10000,"BOMCNPDGJOP");
    o.rootInput=locateSlot(root,0x4000,0x10000,"MHCNPJOGAGH");
    o.rootConfig=locateSlot(root,0x400,0x3000,"ConfigVars");
    o.rootSources=0;o.sourcesKeyboard=0;o.sourcesMouse=0;
    for(int offset=0x400;offset<=0x3000&&!o.rootSources;offset+=8){
        const P holder=read<P>(root+offset);
        if(holder<0x10000)continue;
        const int keyboard=locateSlot(holder,0,0x200,"Keyboard");
        if(!keyboard)continue;
        o.rootSources=offset;o.sourcesKeyboard=keyboard;o.sourcesMouse=locateSlot(holder,0,0x200,"Mouse");
    }
    const P manager=o.rootManager?read<P>(root+o.rootManager):0;
    const P input=o.rootInput?read<P>(root+o.rootInput):0;
    const P config=o.rootConfig?read<P>(root+o.rootConfig):0;
    const P sources=o.rootSources?read<P>(root+o.rootSources):0;
    P keyboard=o.sourcesKeyboard&&sources?read<P>(sources+o.sourcesKeyboard):0;
    P mouse=o.sourcesMouse&&sources?read<P>(sources+o.sourcesMouse):0;
    // 国际服：Mouse/Keyboard 常是 root 的直接字段（如 root+0x5a80/0x5a88），
    // 不在 holder 内。holder 方式找不到时，直接从 root 扫。
    if(!classIs(mouse,"Mouse")){
        const int mo=locateSlot(root,0x4000,0x10000,"Mouse");
        if(mo){o.sourcesMouse=mo;mouse=read<P>(root+mo);}
    }
    if(!classIs(keyboard,"Keyboard")){
        const int ko=locateSlot(root,0x4000,0x10000,"Keyboard");
        if(ko){o.sourcesKeyboard=ko;keyboard=read<P>(root+ko);}
        o.rootSources=0;  // 标记：keyboard/mouse 走缓存，不再经 holder
    }
    probedKeyboard=keyboard;probedMouse=mouse;
    o.configPlatform=locateSlot(config,0,0x200,"PlatformVars");
    o.inputBinding=locateSlot(input,0,0x200,"JGAPEPKBJJP");
    o.mouseSource=locateSlot(mouse,0,0x200,"UnityUnifiedMouseSource");
    o.keyboardSource=locateSlot(keyboard,0,0x200,"UnityUnifiedKeyboardSource");
    o.keyboardMap=locateSlot(keyboard,0,0x200,"Int32[]");
    if(manager&&o.deviceType){const int v=read<int>(manager+o.deviceType,-1);if(v<0||v>2)o.deviceType=0;}
    if(input&&o.inputMode){const int v=read<int>(input+o.inputMode,-1);if(v<0||v>2)o.inputMode=0;}
    // mouseSource/keyboardSource 只用于相对坐标键鼠输入，国际服结构不同探不到；
    // 它们不是"显示隐藏 PC 选项"的必要条件，探不到就置 0 让对应 hook 安全跳过。
    if(!classIs(read<P>(mouse+o.mouseSource), "UnityUnifiedMouseSource")) o.mouseSource=0;
    if(!classIs(read<P>(keyboard+o.keyboardSource), "UnityUnifiedKeyboardSource")) o.keyboardSource=0;
    const bool ok=o.rootManager&&o.rootInput&&o.rootConfig&&classIs(keyboard,"Keyboard")&&
                  classIs(mouse,"Mouse")&&o.deviceType&&o.inputMode;
    if(ok){
        // 国际服：探测 cursor 字段（PC 鼠标模式下值为 -1），供 Java 层 pointer capture 判定。
        // 国服 cursor 是硬编码 0x340，国际服布局不同，这里运行时扫 manager 找 -1。
        if(runtimeIsOS&&manager&&!o.cursor){
            // 国际服真 cursor 字段 manager+0x300：视角模式=-1，UI 面板=非-1。
            // 不能扫第一个 -1：manager+0x2fc 是恒定 -1 的无关字段。
            o.cursor=0x300;
        }
        inoff::probed=true;
        log("Input chain probed for this build");
    }else{
        __android_log_print(ANDROID_LOG_INFO,"vovotool",
            "Input chain probe incomplete: mgr=%d inp=%d src=%d cfg=%d kb=%d mou=%d mouseSrc=%d kbdSrc=%d dev=%d mode=%d",
            o.rootManager?1:0,o.rootInput?1:0,o.rootSources?1:0,o.rootConfig?1:0,
            o.sourcesKeyboard?1:0,o.sourcesMouse?1:0,o.mouseSource?1:0,o.keyboardSource?1:0,
            o.deviceType?1:0,o.inputMode?1:0);
    }
    return ok;
}
#include "uid_visibility.hpp"
#include "crafting.hpp"
#include "expedition.hpp"
#include "daily_claim.hpp"
#include "graphics.hpp"
#include "photo.hpp"
#include "look_sensitivity.hpp"
#include "touch_look.hpp"
#include "map_mouse.hpp"
#include "team_loading.hpp"
#include "avatar_fade.hpp"
#include "avatar_switch.hpp"
#include "region_unlock.hpp"
bool ready(P b){
    if((read<uint8_t>(b+profile::readyFlag)&1)==0)return false;
    P root=read<P>(b+profile::singletonRoot);
    if(root<0x10000)return false;
    if(runtimeIsOS && !inoff::probed){
        // 游戏初期 Unity 对象链尚未建好，失败要重试；节流避免每帧全表扫描。
        static int64_t nextProbeAt=0;
        if(nowMs()<nextProbeAt)return false;
        nextProbeAt=nowMs()+2000;
        if(!probeLayout(root))return false;
    }
    const auto& o=inoff::active;
    P m=o.rootManager?read<P>(root+o.rootManager):0;
    P i=o.rootInput?read<P>(root+o.rootInput):0;
    P s=o.rootSources?read<P>(root+o.rootSources):0;
    P v=o.rootConfig?read<P>(root+o.rootConfig):0;
    P p=v&&o.configPlatform?read<P>(v+o.configPlatform):0;
    P k=runtimeIsOS&&probedKeyboard?probedKeyboard:(s&&o.sourcesKeyboard?read<P>(s+o.sourcesKeyboard):0);
    P mou=runtimeIsOS&&probedMouse?probedMouse:(s&&o.sourcesMouse?read<P>(s+o.sourcesMouse):0);
    P bind=i&&o.inputBinding?read<P>(i+o.inputBinding):0;
    if(!classIs(m,"BOMCNPDGJOP")||!classIs(i,"MHCNPJOGAGH")||!classIs(bind,"JGAPEPKBJJP")||
       !classIs(v,"ConfigVars")||!classIs(p,"PlatformVars")||!classIs(k,"Keyboard")||!classIs(mou,"Mouse"))return false;
    P source=o.mouseSource?read<P>(mou+o.mouseSource):0,keys=o.keyboardSource?read<P>(k+o.keyboardSource):0;
    if(!classIs(source,"UnityUnifiedMouseSource")){source=0;inoff::active.mouseSource=0;}
    if(!classIs(keys,"UnityUnifiedKeyboardSource")){keys=0;inoff::active.keyboardSource=0;}
    const P player=read<P>(bind+0xa0);
    if(!runtimeIsOS&&!classIs(player,"Player"))return false;  // Player 校验是国服偏移，国际服跳过
    (void)player;
    manager=m;input=i;binding=bind;platform=p;keyboard=k;mouse=mou;mouseSource=source;keyboardSource=keys;return true;
}
void apply(P b){
    if(!applied&&nowMs()<inputApplyRetryAt)return;
    applyCalls++;
    if(!applied){
        if(!inputBaselineSaved){
            const auto& o=inoff::active;
            baseline={o.deviceType?read<int>(manager+o.deviceType):-1,
                      o.inputMode?read<int>(input+o.inputMode):-1,
                      static_cast<uint8_t>(o.platformDisableKeyboard?read<uint8_t>(platform+o.platformDisableKeyboard):0),
                      static_cast<uint8_t>(o.platformDisableMouse?read<uint8_t>(platform+o.platformDisableMouse):0),
                      static_cast<uint8_t>(o.keyboardCd?read<uint8_t>(keyboard+o.keyboardCd):0),
                      static_cast<uint8_t>(o.mouseCd?read<uint8_t>(mouse+o.mouseCd):0)};
            baseline.controllerStateKnown=(o.keyboardCd!=0)&&(o.mouseCd!=0);
            inputBaselineSaved=true;
        }
        // A main page created for mobile retains its controls and cursor policy
        // if we only change device/mode fields. Explicit activation in-world
        // must use the complete native transition before locking PC mode.
        if(contextPageIs(topUiContext(),u"InLevelMainPage")){
            if(!originalModeSwitch((void*)input,1,2,nullptr)){
                inputApplyRetryAt=nowMs()+1000;stage=1;log("Waiting for native in-world PC transition");return;
            }
            if(!ready(b)){stage=1;return;}
            log("Explicit in-world PC activation completed through native UI transition");
        }
    }
    const auto& layout=inoff::active;
    if(layout.platformDisableKeyboard)write<uint8_t>(platform+layout.platformDisableKeyboard,0);
    if(layout.platformDisableMouse)write<uint8_t>(platform+layout.platformDisableMouse,0);
    if(!runtimeIsOS){
        auto setController=reinterpret_cast<ControllerSetter>(b+profile::setController);
        setController(reinterpret_cast<void*>(keyboard),true,nullptr);
        setController(reinterpret_cast<void*>(mouse),true,nullptr);
        reinterpret_cast<MapsSetter>(b+profile::selectMaps)(reinterpret_cast<void*>(binding),2,nullptr);
    }else{
        // 国际服：keyboard controller 启用是安全的（崩溃定位显示 mouse 那次才 pc=0）。
        if(keyboard)reinterpret_cast<ControllerSetter>(b+profile::setController)(reinterpret_cast<void*>(keyboard),true,nullptr);
        // selectMaps 切换真正的键鼠控制方案（用经类名校验的 binding）——键盘响应必需。
        if(binding)reinterpret_cast<MapsSetter>(b+profile::selectMaps)(reinterpret_cast<void*>(binding),2,nullptr);
        // setController(mouse) 用经 classIs("Mouse") 校验的对象（keyboard+0x148），激活鼠标 controller。
        if(keyboard){
            const P mo=read<P>(keyboard+0x148);
            if(classIs(mo,"Mouse"))reinterpret_cast<ControllerSetter>(b+profile::setController)(reinterpret_cast<void*>(mo),true,nullptr);
        }
    }
    if(originalDeviceSetter)originalDeviceSetter(reinterpret_cast<void*>(manager),1,true,nullptr);
    if(inoff::active.inputMode)write<int>(input+inoff::active.inputMode,2);
    applied=true;stage=2;
    log(runtimeIsOS?"OS PC UI applied (keyboard enabled, mouse source unresolved)":"PC UI and Rewired keyboard/mouse rule set applied");
}
void restore(P b){
    if(!applied)return;
    if(inoff::active.inputMode)write<int>(input+inoff::active.inputMode,baseline.mode);
    if(!runtimeIsOS&&baseline.controllerStateKnown){
        // 国服：keyboardCd/mouseCd 偏移已验证，基线是真实 controller 状态，精确还原。
        reinterpret_cast<MapsSetter>(b+profile::selectMaps)(reinterpret_cast<void*>(binding),baseline.mode,nullptr);
        reinterpret_cast<ControllerSetter>(b+profile::setController)(reinterpret_cast<void*>(mouse),baseline.mouse!=0,nullptr);
    }
    if(originalDeviceSetter)originalDeviceSetter(reinterpret_cast<void*>(manager),baseline.device,true,nullptr);
    // 国际服 layout 未定位 keyboardCd/mouseCd（=0），baseline.keyboard/mouse 恒为 0；
    // 若仍按 (baseline.keyboard!=0) 调用会无条件关闭键盘 controller，破坏游戏原状态。
    // deviceType/inputMode 已还原为手机值，游戏据此切回触控，controller 不再额外改动。
    if(!runtimeIsOS||baseline.controllerStateKnown){
        reinterpret_cast<ControllerSetter>(b+profile::setController)(reinterpret_cast<void*>(keyboard),baseline.keyboard!=0,nullptr);
    }else{
        log("OS restore keeps controller state (cd offsets unavailable)");
    }
    if(inoff::active.platformDisableKeyboard)write<uint8_t>(platform+inoff::active.platformDisableKeyboard,baseline.disableKeyboard);
    if(inoff::active.platformDisableMouse)write<uint8_t>(platform+inoff::active.platformDisableMouse,baseline.disableMouse);
    restoreLookFactors();applied=false;inputBaselineSaved=false;inputApplyRetryAt=0;stage=3;captured=false;log("Runtime input baseline restored");
}
bool pcModeLocked(){return focusGuard&&enabled&&stage==2&&applied&&
    read<int>(manager+inoff::active.deviceType,-1)==1&&read<int>(input+inoff::active.inputMode,-1)==2;}
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
            if(enabled){if(!applied||read<int>(manager+inoff::active.deviceType)!=1||read<int>(input+inoff::active.inputMode)!=2)apply(b);}
            else restore(b);
            updateLookFactors();
            updateAvatarFade(b);
            {
                const int rawCursor=read<int>(manager+inoff::active.cursor,1000);
                const int depth=uiPageDepth();
                // 世界场景(栈深1)且栈顶是大世界 HUD 才视为视角模式(-1)；
                // 加载/登录页栈深也为1但栈顶是别的类名，必须给非-1 以免鼠标被独占。
                // 栈不可读(depth<0)时退回游戏原值。
                if(runtimeIsOS){
                    cursor=(inWorld())?-1:(depth>1?55:55);
                }else{
                    cursor=(depth==1)?-1:(depth>1?55:rawCursor);
                }
                inputMode=read<int>(input+inoff::active.inputMode,-1);
            }
            updateUidVisibility(b);
            updateCrafting(b);
            updateExpedition(b);
            updateDaily(b);
            updateRegionUnlock(b);
            heartbeat();
        }
    }catch(...){enabled=false;stage=-3;log("Native input call failed; module disabled for this process");}
    entered=false;
}
void mouseHook(void* self,void* data,void* method){
    originalMouse(self,data,method);
    if(!enabled||!applied)return;
    // 不用探测到的 mouseSource（静态探测不稳定）：用 self 的真实类名校验。
    if(runtimeIsOS){ if(!classIs(reinterpret_cast<P>(self),"UnityUnifiedMouseSource"))return; }
    else if(reinterpret_cast<P>(self)!=mouseSource)return;
    // 轴数组偏移：国际服 data+0x20，国服 data+0x30（实测布局不同）。
    const int axesOff=runtimeIsOS?0x20:0x30;
    const int buttonsOff=runtimeIsOS?0x30:0x10;
    const P axes=read<P>(reinterpret_cast<P>(data)+axesOff);
    if(!classIs(axes,"Single[]")||read<int>(axes+0x18)!=4)return;
    mouseFrames++;
    float values[4]{};if(!readMemory(axes+0x20,values,sizeof(values)))return;
    if(captureEnabled&&windowFocused&&cursor==-1){
        std::lock_guard<std::mutex> guard(inputMutex);
        values[0]=relativeX;values[1]=relativeY;values[2]=scrollY;values[3]=scrollX;
        relativeX=relativeY=scrollY=scrollX=0;
        P buttonsArray=read<P>(reinterpret_cast<P>(data)+buttonsOff);
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
    if(!enabled||!applied)return;
    if(runtimeIsOS){ if(!classIs(reinterpret_cast<P>(self),"UnityUnifiedKeyboardSource"))return; }
    else if(reinterpret_cast<P>(self)!=keyboardSource)return;
    // 国际服按键表 data+0x30，国服 data+0x10；均为 Boolean[] len=132。
    const int keysOffset=runtimeIsOS?0x30:0x10;
    P keys=read<P>(reinterpret_cast<P>(data)+keysOffset);
    if(!classIs(keys,"Boolean[]")||read<int>(keys+0x18)!=132)return;
    const int count=read<int>(keys+0x18);
    P map=inoff::active.keyboardMap?read<P>(keyboard+inoff::active.keyboardMap):0;
    const bool haveMap=classIs(map,"Int32[]")&&read<int>(map+0x18)>271;
    std::lock_guard<std::mutex> guard(keyMutex);
    if(!windowFocused){heldKeys=pendingKeys=0;}
    const unsigned pressed=heldKeys|pendingKeys;
    pendingKeys=0;
    for(unsigned n=0;n<3;n++){
        int index=haveMap?read<int>(map+0x20+specialKeyCodes[n]*4,-1):specialKeyCodes[n];
        if(index>=0&&index<count)write<uint8_t>(keys+0x20+index,(pressed>>n)&1);
    }
    keyboardFrames++;
}
void install(P address){
    std::lock_guard<std::mutex> guard(installMutex);
    if(base||!hookFunction)return;
    for(int pi=0;pi<profile::pinsCount;++pi){const auto& pin=profile::pins[pi];unsigned char bytes[16]={};
        if(!readMemory(address+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)!=0){stage=-1;log("Version/code fingerprint mismatch; no hooks installed");return;}}
    // 国际服表里未定位的条目是 0，跳过不装，不能拿 base+0 去 hook
    auto hook=[&](uintptr_t offset,void* replacement,void** original)->bool{
        if(!offset){log("Hook skipped: target unresolved for this build");return true;}
        return hookFunction(reinterpret_cast<void*>(address+offset),replacement,original)==0;
    };
#ifdef VOVO_NO_HOOKS
    base=address;stage=1;log("DIAG build: no hooks installed");return;
#endif
#ifndef VOVO_NO_UI
    if(!hook(profile::setDevice,reinterpret_cast<void*>(deviceSetterHook),reinterpret_cast<void**>(&originalDeviceSetter))){stage=-2;log("UI device guard installation failed");return;}
#endif
#ifndef VOVO_NO_UI
    if(!hook(profile::switchInputMode,reinterpret_cast<void*>(modeSwitchHook),reinterpret_cast<void**>(&originalModeSwitch))){stage=-2;log("Input mode guard installation failed");return;}
#endif
#ifndef VOVO_NO_UID
    if(!hook(profile::uidTextSetter,reinterpret_cast<void*>(uidTextHook),reinterpret_cast<void**>(&originalUidText))){stage=-2;log("UID text hook installation failed");return;}
#endif
#ifndef VOVO_NO_UIDMESH
    if(!hook(profile::textPopulateMesh,reinterpret_cast<void*>(uidMeshHook),reinterpret_cast<void**>(&originalTextMesh))){stage=-2;log("UID discovery hook installation failed");return;}
#endif
#ifndef VOVO_NO_ESCAPE
    if(!hook(profile::escapeRoutePredicate,reinterpret_cast<void*>(escapeRouteHook),reinterpret_cast<void**>(&originalEscapeRoute))){stage=-2;log("Escape route hook installation failed");return;}
#endif
#ifndef VOVO_NO_INPUT
    if(!hook(profile::keyboardUpdate,reinterpret_cast<void*>(keyboardHook),reinterpret_cast<void**>(&originalKeyboard))){stage=-2;log("Keyboard hook installation failed");return;}
#endif
#ifndef VOVO_NO_MOUSE
    if(!hook(profile::mouseUpdate,reinterpret_cast<void*>(mouseHook),reinterpret_cast<void**>(&originalMouse))){stage=-2;log("Mouse hook installation failed");return;}
#endif
#ifndef VOVO_NO_TICK
    if(!hook(profile::tick,reinterpret_cast<void*>(tickHook),reinterpret_cast<void**>(&originalTick))){stage=-2;log("Tick hook installation failed");return;}
#endif
    base=address;stage=1;
    if(runtimeIsOS){
        // 国际服：核心 + 画质 + 反角色虚化（国际服在 avatar_fade.hpp 内按类名+因子直接处理）。
        // 地图鼠标 hook 用国服偏移会崩溃，仍不启用。
        installGraphics(address);
        installAvatarFade(address);
        log("OS build: core + graphics + avatarFade installed");
    }else{
        installLookFactors(address);installTouchLook(address);installMapMouse(address);installTeamLoading(address);installAvatarFade(address);installAvatarSwitch(address);installPhoto(address);installGraphics(address);installRegionUnlock(address);
    }
    log("Native hooks installed for pinned 7.2 ARM64 library");
}
int findModule(dl_phdr_info* info,size_t,void*){
    const char* name=info->dlpi_name;
    if(name&&std::strstr(name,"libyuanshen.so")){install(info->dlpi_addr);return 1;}return 0;
}
void loaded(const char* name,void*){if(name&&std::strstr(name,"libyuanshen.so"))dl_iterate_phdr(findModule,nullptr);}
}

static bool runningAsOS(){
    // /proc/self/cmdline 在游戏主进程里就是包名。
    FILE* f=fopen("/proc/self/cmdline","rb");if(!f)return false;
    char buf[256]={};size_t n=fread(buf,1,sizeof(buf)-1,f);fclose(f);
    std::string cmd(buf,n>0?n:0);
    bool os=cmd.find("com.miHoYo.GenshinImpact")!=std::string::npos;
    log(os?"Runtime profile: OS (GenshinImpact)":"Runtime profile: CN (Yuanshen)");
    return os;
}
extern "C" __attribute__((visibility("default"),used)) ModuleLoaded native_init(const NativeAPIEntries* api){
    if(!api||!api->hook_func)return nullptr;
    runtimeIsOS=runningAsOS();
    if(runtimeIsOS){apply_os_profile();inoff::apply_os_layout();}
    hookFunction=api->hook_func;dl_iterate_phdr(findModule,nullptr);return loaded;
}
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*,void*){log("JNI loaded");return JNI_VERSION_1_6;}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureRegionUnlock(JNIEnv*,jclass,jboolean allowed,jlong token){
    requestRegionUnlock(allowed,token);
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_regionUnlockState(JNIEnv* env,jclass){
    jint values[]={regionUnlockStatus.load(),regionUnlockRequests.load(),regionUnlockValues[0].load(),regionUnlockValues[1].load(),regionUnlockValues[2].load()};
    jintArray result=env->NewIntArray(5);env->SetIntArrayRegion(result,0,5,values);return result;
}
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
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_noteTouch(JNIEnv*,jclass,jint action){
    touchLookUntil=action==3?0:nowMs()+150;
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_touchLookState(JNIEnv* env,jclass){
    jint values[]={touchLookStatus.load(),static_cast<jint>(touchLookCalls.load()),static_cast<jint>(touchLookCorrections.load())};
    jintArray result=env->NewIntArray(3);env->SetIntArrayRegion(result,0,3,values);return result;
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_mapMouseState(JNIEnv* env,jclass){
    jint values[]={mapMouseStatus.load(),static_cast<jint>(mapMouseFrames.load()),static_cast<jint>(mapMouseConverted.load()),
        static_cast<jint>(mapMouseDowns.load()),static_cast<jint>(mapMouseUps.load()),static_cast<jint>(mapRealTouchFrames.load())};
    jintArray result=env->NewIntArray(6);env->SetIntArrayRegion(result,0,6,values);return result;
}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_setCaptureState(JNIEnv*,jclass,jboolean cap,jboolean focus){
    if(windowFocused.exchange(focus)!=static_cast<bool>(focus)&&focus)focusWarmUntil=nowMs()+400;
    if(!focus){std::lock_guard<std::mutex> guard(keyMutex);heldKeys=pendingKeys=0;craftKeyHeld=false;dailyKeyHeld=false;expeditionKeyHeld=false;}
    if(captured.exchange(cap)!=static_cast<bool>(cap)){
        std::lock_guard<std::mutex> guard(inputMutex);relativeX=relativeY=scrollX=scrollY=0;buttons=pendingButtons=0;suppressUntil=nowMs()+150;
    }
}
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_motion(JNIEnv*,jclass,jfloat x,jfloat y,jfloat vertical,jfloat horizontal,jint mask){
    if(!captureEnabled||!windowFocused||!std::isfinite(x)||!std::isfinite(y))return;
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
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureTeamLoading(JNIEnv*,jclass,jboolean active){teamLoadingEnabled=active;}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_teamLoadingState(JNIEnv* env,jclass){
    jint values[]={teamLoadingStatus.load(),teamLoadingEnabled?1:0,static_cast<jint>(teamLoadingCalls.load()),
        static_cast<jint>(teamLoadingSkipped.load()),static_cast<jint>(teamLoadingWaiting.load()),teamLoadingDuration.load(),teamLoadingElapsed.load()};
    jintArray result=env->NewIntArray(7);env->SetIntArrayRegion(result,0,7,values);return result;
}
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
extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_requestLookRefresh(JNIEnv*,jclass,jlong token){static jlong previous=0;if(token&&token!=previous){previous=token;lookRefreshRequested=true;}}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_lookState(JNIEnv* env,jclass){
    const P p=input;auto pack=[](float v){return std::isfinite(v)&&std::abs(v)<1000?jint(v*1000000):jint(-2147483647);};
    jint values[]={lookStatus.load(),lookFixes.load(),lookRecalculations.load(),lookWriter.load(),pack(read<float>(p+0x14c)),pack(read<float>(p+0x150)),pack(read<float>(p+0x170)),pack(read<float>(p+0x174)),pack(read<float>(p+0x12c)),read<int>(p+0x1a4,-1),lookVerified?1:0,lookRefreshRequested?1:0};
    jintArray result=env->NewIntArray(12);env->SetIntArrayRegion(result,0,12,values);return result;
}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_renderState(JNIEnv* env,jclass){
    jint values[]={renderOutputWidth.load(),renderOutputHeight.load(),graphicsSceneWidth.load(),graphicsSceneHeight.load(),
        static_cast<jint>(renderGameScale.load()*1000000),static_cast<jint>(renderAppliedScale.load()*1000000),static_cast<jint>(renderScaleCalls.load()),renderScaleStatus.load(),
        renderReadbackWidth.load(),renderReadbackHeight.load(),static_cast<jint>(renderReadbacks.load()),static_cast<jint>(renderReadbackErrors.load())};
    jintArray result=env->NewIntArray(12);env->SetIntArrayRegion(result,0,12,values);return result;
}

extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureAvatarFade(JNIEnv*,jclass,jboolean active){avatarFadeEnabled=active;}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_avatarFadeState(JNIEnv* env,jclass){
    jint values[]={avatarFadeStatus.load(),avatarFadeEnabled?1:0,avatarFadeCount.load(),static_cast<jint>(avatarFadeCalls.load()),
        static_cast<jint>(avatarFadeOverrides.load()),static_cast<jint>(avatarFadeRestores.load()),avatarFadeRequested.load(),avatarFadeOutput.load()};
    jintArray result=env->NewIntArray(8);env->SetIntArrayRegion(result,0,8,values);return result;
}

extern "C" JNIEXPORT void JNICALL Java_com_qi_vovotool_NativeBridge_configureAvatarSwitch(JNIEnv*,jclass,jboolean active){avatarSwitchEnabled=active;}
extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_avatarSwitchState(JNIEnv* env,jclass){
    jint values[]={avatarSwitchStatus.load(),avatarSwitchEnabled?1:0,static_cast<jint>(avatarSwitchCalls.load()),static_cast<jint>(avatarSwitchOverrides.load()),static_cast<jint>(avatarSwitchAccepted.load()),avatarSwitchLastFail.load(),static_cast<jint>(avatarSwitchMessages.load()),avatarSwitchMessageFail.load(),avatarSwitchMessageCaller.load(),static_cast<jint>(avatarSwitchRetCalls.load()),avatarSwitchRetCode.load(),avatarSwitchRetCaller.load(),static_cast<jint>(avatarSwitchRequests.load()),static_cast<jint>(avatarSwitchSent.load())};
    jintArray result=env->NewIntArray(14);env->SetIntArrayRegion(result,0,14,values);return result;
}

extern "C" JNIEXPORT jintArray JNICALL Java_com_qi_vovotool_NativeBridge_avatarSwitchTraceState(JNIEnv* env,jclass){
    jint values[14]={avatarSwitchTraceMask.load(),avatarSwitchTraceReason.load()};
    values[12]=avatarSwitchToastCaller.load();values[13]=static_cast<jint>(avatarSwitchToasts.load());
    for(int i=0;i<10;i++)values[i+2]=static_cast<jint>(avatarSwitchExitCounts[i].load());
    jintArray result=env->NewIntArray(14);env->SetIntArrayRegion(result,0,14,values);return result;
}
