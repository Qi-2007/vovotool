#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <limits>
#include <algorithm>
using P=uintptr_t;
std::atomic<bool> enabled{true};std::atomic<P> base{0};
unsigned char memory[0x20000]{};std::map<P,std::string> classes;
template<class T>T read(P p,T fallback={}){if(p+sizeof(T)>sizeof(memory))return fallback;T v;std::memcpy(&v,memory+p,sizeof(v));return v;}
template<class T>void write(P p,T v){assert(p+sizeof(T)<=sizeof(memory));std::memcpy(memory+p,&v,sizeof(v));}
bool readMemory(P p,void* v,size_t n){if(p+n>sizeof(memory))return false;std::memcpy(v,memory+p,n);return true;}
bool classIs(P p,const char* n){return classes.count(p)&&classes[p]==n;}
namespace profile {struct Pin{P rva;unsigned char bytes[16];};constexpr Pin avatarFadePins[]={{0,{}}};constexpr P avatarFadeRecalculate=0,singletonRoot=0x100;}
int(*hookFunction)(void*,void*,void**)=nullptr;
#include "avatar_fade.hpp"
int calls=0;float observed=0;void* lastMethod=nullptr;
void original(void* self,void* method){calls++;lastMethod=method;observed=read<float>((P)self+0x184);write<float>((P)self+0x168,std::min(read<float>((P)self+0x160),observed));}
int main(){
    write<P>(0x100,0x1000);write<P>(0xa070,0xb000);write<P>(0xb050,0xc000);write<P>(0xc508,0xe000);write<P>(0xe200,0x10000);write<P>(0x10020,0x11000);
    classes={{0xb000,"LFEMMJADHPE"},{0xc000,"BDBIBJBCPOJ"},{0xe000,"BGMCJLBBGJF"},{0x10000,"PPEAKFODDID"},{0x11000,"LOFPEGCAEFG"},{0x12000,"PECHLEKMHDF"}};
    write<P>(0x11028,0x12000);write<float>(0x11184,.4f);write<float>(0x11160,.6f);
    originalAvatarFadeRecalculate=original;avatarFadeStatus=1;avatarFadeEnabled=false;
    updateAvatarFade(0);assert(avatarFadeCount==1&&observed==.4f);
    avatarFadeEnabled=true;updateAvatarFade(0);assert(observed==1&&read<float>(0x11168)==.6f&&read<float>(0x11184)==.4f);
    int n=calls;updateAvatarFade(0);assert(calls==n);
    write<float>(0x11160,1);write<float>(0x11184,.25f);avatarFadeRecalculateHook((void*)0x11000,(void*)0x123);
    assert(observed==1&&lastMethod==(void*)0x123&&read<float>(0x11168)==1&&read<float>(0x11184)==.25f); // material output corrected before return; native source restored
    avatarFadeEnabled=false;updateAvatarFade(0);assert(observed==.25f&&read<float>(0x11168)==.25f);
    avatarFadeEnabled=true;updateAvatarFade(0);enabled=false;updateAvatarFade(0);assert(observed==.25f);enabled=true;
    write<float>(0x13184,.2f);avatarFadeRecalculateHook((void*)0x13000,nullptr);assert(observed==.2f);
    write<P>(0x11028,0x13000);avatarFadeRecalculateHook((void*)0x11000,nullptr);assert(observed==.25f);updateAvatarFade(0);assert(avatarFadeCount==0);
    write<P>(0x11028,0x12000);write<float>(0x11184,.5f);updateAvatarFade(0);assert(observed==1&&read<float>(0x11184)==.5f);
    write<float>(0x11184,std::numeric_limits<float>::quiet_NaN());avatarFadeRecalculateHook((void*)0x11000,nullptr);assert(std::isnan(observed)&&std::isnan(read<float>(0x11184)));
    write<float>(0x11184,.5f);avatarFadeEnabled=false;updateAvatarFade(0);assert(observed==.5f);
    classes[0x11000]="MonoTreeLeaf";updateAvatarFade(0);assert(avatarFadeCount==0);
    puts("PASS: pre-material correction, camera source restoration, normal fade preserved, original call/MethodInfo, toggle/master restoration, identity and value guards");
}
