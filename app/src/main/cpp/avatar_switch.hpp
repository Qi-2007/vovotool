#pragma once
#include <cstdio>
std::atomic<unsigned> switchFlowLogs{0};
void switchFlowLog(const char* kind,int a,int b,int c){
    if(switchFlowLogs.fetch_add(1)>=1000)return;
    char text[160];std::snprintf(text,sizeof(text),"switchflow t=%lld %s a=%d b=%d c=%d",static_cast<long long>(nowMs()),kind,a,b,c);log(text);
}
void(*originalSwitchInput)(void*,uint64_t,uint32_t,bool,void*)=nullptr;
void switchInputTrace(void* self,uint64_t guid,uint32_t id,bool special,void* method){
    switchFlowLog("input",id,special,static_cast<int>((P)__builtin_return_address(0)-base.load()));
    originalSwitchInput(self,guid,id,special,method);
}
void(*originalSwitchResponse)(void*,void*,void*)=nullptr;
void switchResponseTrace(void* self,void* response,void* method);
float(*originalSwitchCooldown)(void*,void*)=nullptr;
std::atomic<unsigned> switchCooldownLogs{0};
float switchCooldownTrace(void* self,void* method);
std::atomic<bool> avatarSwitchEnabled{false};
std::atomic<int> avatarSwitchStatus{0},avatarSwitchLastFail{0};
std::atomic<unsigned long> avatarSwitchCalls{0},avatarSwitchOverrides{0},avatarSwitchAccepted{0};
std::atomic<int> avatarSwitchMessageFail{-1},avatarSwitchMessageCaller{0},avatarSwitchRetCode{0},avatarSwitchRetCaller{0};
std::atomic<unsigned long> avatarSwitchMessages{0},avatarSwitchRetCalls{0};
float switchCooldownTrace(void* self,void* method){
    float value=originalSwitchCooldown(self,method);
    bool zero=avatarSwitchStatus==1&&enabled&&avatarSwitchEnabled&&windowFocused&&
        classIs((P)self,"HBIJFOJIFAH")&&contextPageIs(topUiContext(),u"InLevelMainPage");
    if(value>0&&switchCooldownLogs.fetch_add(1)<24)switchFlowLog("cooldown",static_cast<int>(value),zero,static_cast<int>((P)__builtin_return_address(0)-base.load()));
    return zero?0.f:value;
}
void(*originalAvatarSwitchMessage)(int,void*)=nullptr;
void avatarSwitchMessageHook(int fail,void* method){
    avatarSwitchMessages++;avatarSwitchMessageFail=fail;
    avatarSwitchMessageCaller=(P)__builtin_return_address(0)-base.load();
    originalAvatarSwitchMessage(fail,method);
}
void switchResponseTrace(void* self,void* response,void* method){
    int code=classIs((P)response,"JDOIEGICOAD")?read<int>((P)response+0x18):-9999;
    avatarSwitchRetCalls++;avatarSwitchRetCode=code;
    avatarSwitchRetCaller=(P)__builtin_return_address(0)-base.load();
    switchFlowLog("response",code,0,avatarSwitchRetCaller.load());
    originalSwitchResponse(self,response,method);
}
struct AvatarSwitchTrace {unsigned mask=0;uint64_t guid=0;};
thread_local AvatarSwitchTrace* avatarSwitchTrace=nullptr;
std::atomic<int> avatarSwitchTraceMask{0},avatarSwitchTraceReason{0};
std::atomic<unsigned long> avatarSwitchExitCounts[10]{};
bool(*originalSwitchUi)(void*,int,void*)=nullptr;
P(*originalSwitchData)(void*,uint64_t,void*)=nullptr;
P(*originalSwitchCurrent)(void*,void*)=nullptr;
P(*originalSwitchTarget)(void*,uint32_t,void*)=nullptr;
struct SwitchVec3 {float x,y,z;};
void(*originalSwitchSend)(void*,SwitchVec3,uint64_t,uint32_t,bool,void*)=nullptr;
bool switchUiTrace(void* self,int type,void* method){bool r=originalSwitchUi(self,type,method);if(avatarSwitchTrace&&type==9){avatarSwitchTrace->mask|=1;if(r)avatarSwitchTrace->mask|=2;}return r;}
P switchDataTrace(void* self,uint64_t guid,void* method){P r=originalSwitchData(self,guid,method);if(avatarSwitchTrace){avatarSwitchTrace->mask|=4;if(r)avatarSwitchTrace->mask|=8;}return r;}
P switchCurrentTrace(void* self,void* method){P r=originalSwitchCurrent(self,method);if(avatarSwitchTrace&&!(avatarSwitchTrace->mask&16)){avatarSwitchTrace->mask|=16;if(r){avatarSwitchTrace->mask|=32;if(read<uint64_t>(r+0x558)==avatarSwitchTrace->guid)avatarSwitchTrace->mask|=64;}}return r;}
P switchTargetTrace(void* self,uint32_t id,void* method){P r=originalSwitchTarget(self,id,method);if(avatarSwitchTrace&&!(avatarSwitchTrace->mask&128)){avatarSwitchTrace->mask|=128;if(r){avatarSwitchTrace->mask|=256;if(read<uint8_t>(r+0x38b)&1)avatarSwitchTrace->mask|=512;}}return r;}
void switchSendTrace(void* self,SwitchVec3 position,uint64_t guid,uint32_t id,bool special,void* method){if(avatarSwitchTrace)avatarSwitchTrace->mask|=4096;switchFlowLog("send",id,special,0);originalSwitchSend(self,position,guid,id,special,method);}
int avatarSwitchExit(unsigned mask,bool result){
    if(result)return 0;
    if(mask&2)return 1;
    if((mask&4)&&!(mask&8))return 2;
    if((mask&16)&&!(mask&32))return 3;
    if(mask&64)return 4;
    if((mask&128)&&!(mask&256))return 5;
    if((mask&256)&&!(mask&512))return 6;
    if((mask&1024)&&!(mask&2048))return 7;
    if((mask&2048)&&!(mask&4096))return 8;
    return 9;
}
std::atomic<int> avatarSwitchToastCaller{0};std::atomic<unsigned long> avatarSwitchToasts{0};
void(*originalSwitchToast)(void*,bool,void*)=nullptr;
void(*originalSwitchWarning)(void*,bool,bool,void*)=nullptr;
void(*originalSwitchInfo)(void*,bool,int,void*,void*)=nullptr;
void recordSwitchToast(P text,P caller){std::u16string value;if(readManagedText(text,value)&&value.find(u"冷却")!=std::u16string::npos){avatarSwitchToasts++;avatarSwitchToastCaller=caller-base.load();}}
void switchToastHook(void* text,bool b,void* method){recordSwitchToast((P)text,(P)__builtin_return_address(0));originalSwitchToast(text,b,method);}
void switchWarningHook(void* text,bool a,bool b,void* method){recordSwitchToast((P)text,(P)__builtin_return_address(0));originalSwitchWarning(text,a,b,method);}
void switchInfoHook(void* text,bool a,int kind,void* cb,void* method){recordSwitchToast((P)text,(P)__builtin_return_address(0));originalSwitchInfo(text,a,kind,cb,method);}
using AvatarSwitchCheck=bool(*)(void*,void*,void*,int*,bool,bool,bool,int,int,void*);
AvatarSwitchCheck originalAvatarSwitch=nullptr;
bool avatarSwitchHook(void* entity,void* target,void* data,int* fail,bool ignoreState,bool ignoreCD,bool special,int nyx,int specialType,void* method){
    avatarSwitchCalls++;
    if(avatarSwitchStatus==1&&enabled&&avatarSwitchEnabled&&windowFocused&&
       contextPageIs(topUiContext(),u"InLevelMainPage")&&classIs((P)entity,"PECHLEKMHDF")&&
       classIs((P)target,"PECHLEKMHDF")&&classIs((P)data,"LOPOKBINJOP")&&!ignoreCD){
        ignoreCD=true;avatarSwitchOverrides++;
    }
    bool result=originalAvatarSwitch(entity,target,data,fail,ignoreState,ignoreCD,special,nyx,specialType,method);
    avatarSwitchLastFail=fail?read<int>((P)fail,-1):-1;
    if(avatarSwitchTrace){avatarSwitchTrace->mask|=1024;if(result)avatarSwitchTrace->mask|=2048;}
    if(result)avatarSwitchAccepted++;
    return result;
}
using AvatarSwitchRequest=bool(*)(void*,uint64_t,uint32_t,bool,bool,bool,bool,bool,bool,void*);
AvatarSwitchRequest originalAvatarSwitchRequest=nullptr;
std::atomic<unsigned long> avatarSwitchRequests{0},avatarSwitchSent{0};
bool avatarSwitchRequestHook(void* self,uint64_t guid,uint32_t id,bool a,bool b,bool c,bool d,bool e,bool f,void* method){
    avatarSwitchRequests++;
    const bool bypass=avatarSwitchStatus==1&&enabled&&avatarSwitchEnabled&&windowFocused&&classIs((P)self,"HBIJFOJIFAH")&&contextPageIs(topUiContext(),u"InLevelMainPage");
    float saved=read<float>((P)self+0x1e8);
    if(bypass){
        write<float>((P)self+0x1e8,-1000000.f);
        // MADILIEPJJA feeds both ignoreChangeCD and the native request's +0x29 bool.
        // Changing only the nested check leaves the request flag false.
        e=true;
    }
    AvatarSwitchTrace trace{0,guid};auto* previous=avatarSwitchTrace;avatarSwitchTrace=&trace;
    bool result=originalAvatarSwitchRequest(self,guid,id,a,b,c,d,e,f,method);
    avatarSwitchTrace=previous;int reason=avatarSwitchExit(trace.mask,result);avatarSwitchTraceMask=trace.mask;avatarSwitchTraceReason=reason;avatarSwitchExitCounts[reason]++;
    if(reason!=4)switchFlowLog("exit",reason,trace.mask,id);
    if(bypass)write<float>((P)self+0x1e8,saved);
    if(result)avatarSwitchSent++;
    return result;
}
bool(*originalAvatarSwitchReady)(void*,void*)=nullptr;
bool avatarSwitchReadyHook(void* self,void* method){
    if(avatarSwitchStatus==1&&enabled&&avatarSwitchEnabled&&windowFocused&&classIs((P)self,"HBIJFOJIFAH")&&contextPageIs(topUiContext(),u"InLevelMainPage"))return true;
    return originalAvatarSwitchReady(self,method);
}
void installAvatarSwitch(P address){
    for(const auto& pin:profile::avatarSwitchPins){unsigned char bytes[16]{};
        if(!readMemory(address+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)){avatarSwitchStatus=-1;return;}}
    if(hookFunction((void*)(address+profile::avatarSwitchCheck),(void*)avatarSwitchHook,(void**)&originalAvatarSwitch)){
        avatarSwitchStatus=-2;return;
    }
    // Leave ShowChangeAvatarFailMessage unpatched: its lazy-init branch jumps
    // back into the entry patch span. Responses are traced at BEGNBEFMFCI instead.
    if(hookFunction((void*)(address+profile::avatarSwitchRequest),(void*)avatarSwitchRequestHook,(void**)&originalAvatarSwitchRequest)){
        avatarSwitchStatus=-2;return;
    }
    if(hookFunction((void*)(address+profile::avatarSwitchReady),(void*)avatarSwitchReadyHook,(void**)&originalAvatarSwitchReady)){
        avatarSwitchStatus=-2;return;
    }
    if(hookFunction((void*)(address+profile::switchTraceUi),(void*)switchUiTrace,(void**)&originalSwitchUi)||
       hookFunction((void*)(address+profile::switchTraceData),(void*)switchDataTrace,(void**)&originalSwitchData)||
       hookFunction((void*)(address+profile::switchTraceCurrent),(void*)switchCurrentTrace,(void**)&originalSwitchCurrent)||
       hookFunction((void*)(address+profile::switchTraceTarget),(void*)switchTargetTrace,(void**)&originalSwitchTarget)||
       hookFunction((void*)(address+profile::switchTraceSend),(void*)switchSendTrace,(void**)&originalSwitchSend)){
        avatarSwitchStatus=-2;return;
    }
    if(hookFunction((void*)(address+profile::switchTraceToast),(void*)switchToastHook,(void**)&originalSwitchToast)||
       hookFunction((void*)(address+profile::switchTraceWarning),(void*)switchWarningHook,(void**)&originalSwitchWarning)||
       hookFunction((void*)(address+profile::switchTraceInfo),(void*)switchInfoHook,(void**)&originalSwitchInfo)){
        avatarSwitchStatus=-2;return;
    }
    if(hookFunction((void*)(address+profile::switchTraceInput),(void*)switchInputTrace,(void**)&originalSwitchInput)||
       hookFunction((void*)(address+profile::switchTraceResponse),(void*)switchResponseTrace,(void**)&originalSwitchResponse)||
       hookFunction((void*)(address+profile::switchTraceCooldown),(void*)switchCooldownTrace,(void**)&originalSwitchCooldown)){
        avatarSwitchStatus=-2;return;
    }
    avatarSwitchStatus=1;
    log("Switch CT v3 installed: request flag and cooldown getter; native failure message untouched");
}
