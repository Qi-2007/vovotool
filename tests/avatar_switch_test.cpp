#include <atomic>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <string>
#include "profile.hpp"
using P=uintptr_t;
std::atomic<P> base{0};std::atomic<bool> enabled{true},windowFocused{true};bool mainPage=true,validClasses=true,lastCD=false,otherFailure=false;
alignas(8) unsigned char requestSelf[0x2000]{};int calls=0;
template<class T>T read(P p,T={}){T v;std::memcpy(&v,(void*)p,sizeof(v));return v;}
template<class T>void write(P p,T v){std::memcpy((void*)p,&v,sizeof(v));}
bool readMemory(P,void*,size_t){return false;}
bool readManagedText(P,std::u16string&){return false;}
int64_t nowMs(){return 1000;}void log(const char*){}
bool classIs(P p,const char* n){return validClasses&&((p==(P)requestSelf&&!std::strcmp(n,"HBIJFOJIFAH"))||(p==1&&!std::strcmp(n,"PECHLEKMHDF"))||(p==2&&!std::strcmp(n,"PECHLEKMHDF"))||(p==3&&!std::strcmp(n,"LOPOKBINJOP")));}
P topUiContext(){return 4;}bool contextPageIs(P,const char16_t*){return mainPage;}
int(*hookFunction)(void*,void*,void**)=nullptr;
#include "avatar_switch.hpp"
bool original(void* a,void* b,void* c,int* fail,bool state,bool cd,bool special,int nyx,int kind,void* method){
    calls++;assert(a==(void*)1&&b==(void*)2&&c==(void*)3&&!state&&special&&nyx==7&&kind==9&&method==(void*)11);
    lastCD=cd;*fail=otherFailure?12:cd?0:13;return *fail==0;
}
bool run(bool expectedCD){int fail=-1,n=calls;bool result=avatarSwitchHook((void*)1,(void*)2,(void*)3,&fail,false,false,true,7,9,(void*)11);assert(calls==n+1&&lastCD==expectedCD&&avatarSwitchLastFail==fail);return result;}
bool expectedRequestBypass=true,expectedRequestFlag=true,requestReturn=true;
bool requestOriginal(void* self,uint64_t guid,uint32_t id,bool a,bool b,bool c,bool d,bool e,bool f,void* method){
    assert(self==requestSelf&&guid==0x123456789ULL&&id==42&&a&&!b&&c&&!d&&e==expectedRequestFlag&&!f&&method==(void*)17);
    assert(read<float>((P)self+0x1e8)==(expectedRequestBypass?-1000000.f:123.f));return requestReturn;
}
bool readyOriginal(void* self,void*){assert(self==requestSelf);return false;}
int cooldownCalls=0;
float cooldownOriginal(void* self,void* method){assert(self==requestSelf&&method==(void*)19);cooldownCalls++;return 525.f;}
int main(){
    assert(!avatarSwitchEnabled);avatarSwitchEnabled=true;
    originalAvatarSwitch=original;avatarSwitchStatus=1;assert(run(true));
    otherFailure=true;assert(!run(true)&&avatarSwitchLastFail==12);otherFailure=false;
    avatarSwitchEnabled=false;assert(!run(false));avatarSwitchEnabled=true;
    enabled=false;assert(!run(false));enabled=true;
    windowFocused=false;assert(!run(false));windowFocused=true;
    mainPage=false;assert(!run(false));mainPage=true;
    validClasses=false;assert(!run(false));validClasses=true;
    avatarSwitchStatus=-1;assert(!run(false));
    avatarSwitchStatus=1;originalAvatarSwitchRequest=requestOriginal;originalAvatarSwitchReady=readyOriginal;
    write<float>((P)requestSelf+0x1e8,123.f);
    assert(avatarSwitchReadyHook(requestSelf,nullptr));
    auto request=[](bool inputFlag=false){bool r=avatarSwitchRequestHook(requestSelf,0x123456789ULL,42,true,false,true,false,inputFlag,false,(void*)17);assert(r==requestReturn);assert(read<float>((P)requestSelf+0x1e8)==123.f);};
    request();request(true);requestReturn=false;request();requestReturn=true;
    expectedRequestBypass=false;expectedRequestFlag=false;
    avatarSwitchEnabled=false;request();avatarSwitchEnabled=true;
    enabled=false;request();enabled=true;
    windowFocused=false;request();windowFocused=true;
    mainPage=false;request();mainPage=true;
    validClasses=false;request();validClasses=true;
    avatarSwitchStatus=-1;request();avatarSwitchStatus=1;
    avatarSwitchEnabled=false;expectedRequestFlag=true;request(true);avatarSwitchEnabled=true;
    assert(read<float>((P)requestSelf+0x1e8)==123.f);
    avatarSwitchEnabled=false;assert(!avatarSwitchReadyHook(requestSelf,nullptr));
    originalSwitchCooldown=cooldownOriginal;avatarSwitchEnabled=true;
    assert(switchCooldownTrace(requestSelf,(void*)19)==0.f&&cooldownCalls==1);
    avatarSwitchEnabled=false;assert(switchCooldownTrace(requestSelf,(void*)19)==525.f);avatarSwitchEnabled=true;
    enabled=false;assert(switchCooldownTrace(requestSelf,(void*)19)==525.f);enabled=true;
    mainPage=false;assert(switchCooldownTrace(requestSelf,(void*)19)==525.f);mainPage=true;
    windowFocused=false;assert(switchCooldownTrace(requestSelf,(void*)19)==525.f);windowFocused=true;
    validClasses=false;assert(switchCooldownTrace(requestSelf,(void*)19)==525.f);validClasses=true;
    avatarSwitchStatus=-1;assert(switchCooldownTrace(requestSelf,(void*)19)==525.f);avatarSwitchStatus=1;
    puts("PASS: request flag promotion and guards, original result/other arguments/MethodInfo, rate-field restoration and nested-check scope");
}
