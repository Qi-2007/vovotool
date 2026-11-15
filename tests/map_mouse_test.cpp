#include <atomic>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
using P=uintptr_t;
std::atomic<bool> enabled{true},windowFocused{true};std::atomic<int> cursor{1000};
std::atomic<int64_t> touchLookUntil{0};bool applied=true,mapShown=true;P input=0x100;
unsigned char memory[0x1000]{};bool down=false,held=false,up=false;
template<class T>T read(P p,T fallback={}){if(p+sizeof(T)>sizeof(memory))return fallback;T v;std::memcpy(&v,memory+p,sizeof(v));return v;}
template<class T>void write(P p,T v){assert(p+sizeof(T)<=sizeof(memory));std::memcpy(memory+p,&v,sizeof(v));}
bool readMemory(P p,void* v,size_t n){if(p+n>sizeof(memory))return false;std::memcpy(v,memory+p,n);return true;}
bool classIs(P p,const char* n){return p==0x400&&!std::strcmp(n,"EasyTouch");}
int64_t nowMs(){return 100;}
P topUiContext(){return 0x800;}
bool contextPageIs(P,const char16_t*){return mapShown;}
namespace profile {struct Pin {P rva;unsigned char bytes[16];};constexpr Pin mapMousePins[]={{0,{}}};constexpr P mapUpdateTouches=0,mapMouseHeld=0,mapMouseDown=0,mapMouseUp=0;}
int(*hookFunction)(void*,void*,void**)=nullptr;
#include "map_mouse.hpp"
bool heldFn(int i){assert(i==0);return held;}bool downFn(int i){assert(i==0);return down;}bool upFn(int i){assert(i==0);return up;}
bool lastReal=true;int lastCount=-1;uint8_t lastRemote=0;
void original(void* self,bool real,int count,void*){lastReal=real;lastCount=count;lastRemote=read<uint8_t>((P)self+0x29);}
void run(int count=0){mapTouchesHook((void*)0x400,true,count,nullptr);assert(read<uint8_t>(0x429)==1);}
int main(){
    write<P>(input+0x40,0x400);write<int>(input+0x1a4,2);write<uint8_t>(0x429,1);
    originalMapTouches=original;mapMouseHeld=heldFn;mapMouseDown=downFn;mapMouseUp=upFn;mapMouseStatus=1;
    held=true;run();assert(lastReal&&lastCount==0); // held before opening the map
    down=true;run();assert(!lastReal&&lastCount==1&&lastRemote==0);
    down=false;run();assert(!lastReal&&mapMouseSession);
    held=false;up=true;run();assert(!lastReal&&lastCount==1); // preserve the native Ended phase
    up=false;run();assert(lastReal&&!mapMouseSession);
    down=held=true;run(1);assert(lastReal&&lastCount==1&&lastRemote==1&&!mapMouseSession);
    touchLookUntil=200;run();assert(lastReal&&!mapMouseSession);touchLookUntil=0;
    mapShown=false;run();assert(lastReal&&!mapMouseSession);mapShown=true;
    cursor=-1;run();assert(lastReal);cursor=1000;
    enabled=false;run();assert(lastReal);enabled=true;
    windowFocused=false;run();assert(lastReal);windowFocused=true;
    write<int>(input+0x1a4,0);run();assert(lastReal);write<int>(input+0x1a4,2);
    mapMouseStatus=-1;run();assert(lastReal);
    puts("PASS: map press/drag/release, prior held button, real-touch priority, field restoration, menu/world/focus/mode guards");
}
