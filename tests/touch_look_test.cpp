#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <cstdio>
using P=uintptr_t;
std::atomic<P> base{0};
std::atomic<bool> enabled{true},windowFocused{true};
std::atomic<int> cursor{-1};bool applied=true;P input=0x100;
unsigned char memory[0x20000]{};bool aiming=false;
template<class T>T read(P p,T fallback={}){if(p+sizeof(T)>sizeof(memory))return fallback;T v;std::memcpy(&v,memory+p,sizeof(v));return v;}
template<class T>void put(P p,T v){assert(p+sizeof(T)<=sizeof(memory));std::memcpy(memory+p,&v,sizeof(v));}
bool readMemory(P p,void* v,size_t n){if(p+n>sizeof(memory))return false;std::memcpy(v,memory+p,n);return true;}
bool classIs(P p,const char* n){
    if(p==0x200)return !std::strcmp(n,"MonoInputEasyTouch_H4");
    if(p==0x400)return !std::strcmp(n,"LFEMMJADHPE");
    if(p==0x500)return !std::strcmp(n,"BDBIBJBCPOJ");
    if(p==0x1000)return !std::strcmp(n,"BGMCJLBBGJF");
    if(p==0x2000)return !std::strcmp(n,aiming?"HMFAPGCKOCC":"KBAKEOHEJKN");
    if(p==0x3000)return !std::strcmp(n,aiming?"HDNAMHCOJHK":"DBOLJJLIEJI");
    return false;
}
int64_t nowMs(){return 100;}
namespace profile {struct Pin {P rva;unsigned char bytes[16];};constexpr P singletonRoot=0x4000,touchRotate=0;constexpr Pin touchPins[]={{0,{}}};}
int(*hookFunction)(void*,void*,void**)=nullptr;
#include "touch_look.hpp"
float lastX=0,lastY=0;int calls=0;
void original(void*,float x,float y,void*){lastX=x;lastY=y;calls++;}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
void run(){touchRotateHook((void*)0x200,.25f,.125f,nullptr);assert(lastX==.25f);}
int main(){
    put<int>(input+0x1a4,2);put<P>(profile::singletonRoot,0x6000);put<P>(0x6000+0x9070,0x400);
    put<P>(0x400+0x50,0x500);put<P>(0x500+0x508,0x1000);put<P>(0x1000+0x9a8,0x2000);put<P>(0x2000+0x1a0,0x3000);
    originalTouchRotate=original;touchLookStatus=1;touchLookUntil=200;
    run();assert(near(lastY,.125f*normalTouchVerticalGain)&&touchLookCorrections==1);
    aiming=true;run();assert(lastY==.125f&&touchLookCorrections==1);aiming=false;
    cursor=1000;run();assert(lastY==.125f);cursor=-1;
    touchLookUntil=0;run();assert(lastY==.125f);touchLookUntil=200;
    enabled=false;run();assert(lastY==.125f);enabled=true;
    windowFocused=false;run();assert(lastY==.125f);windowFocused=true;
    put<int>(input+0x1a4,1);run();assert(lastY==.125f);put<int>(input+0x1a4,2);
    touchRotateHook((void*)0x201,.25f,.125f,nullptr);assert(lastY==.125f);
    touchLookStatus=-1;run();assert(lastY==.125f&&touchLookCorrections==1);
    assert(calls==9);
    puts("PASS: normal touchscreen correction; aiming, mixed/menu cursor, mouse/no-touch, disabled, unfocused, mobile and wrong-source passthrough");
}
