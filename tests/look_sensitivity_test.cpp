#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <cstdio>
#include <android/log.h>
using P=uintptr_t;
std::atomic<bool> enabled{true};bool applied=true;P input=0x1000;
unsigned char memory[0x3000]{};
template<class T>T read(P p,T fallback={}){if(p+sizeof(T)>sizeof(memory))return fallback;T v;std::memcpy(&v,memory+p,sizeof(v));return v;}
template<class T>void write(P p,T v){assert(p+sizeof(T)<=sizeof(memory));std::memcpy(memory+p,&v,sizeof(v));}
bool readMemory(P p,void* out,size_t n){if(p+n>sizeof(memory))return false;std::memcpy(out,memory+p,n);return true;}
bool classIs(P p,const char* n){return p==input&&std::strcmp(n,"MHCNPJOGAGH")==0;}
namespace profile {struct Pin {P rva;unsigned char bytes[16];};constexpr Pin lookPins[]={{0,{}}};constexpr P lookAxisSetting=0,lookRefresh=0,lookHorizontal=0,lookVertical=0,lookSpeed=0;}
int(*hookFunction)(void*,void*,void**)=nullptr;
#include "look_sensitivity.hpp"
float settings[2]={.5f,.75f};int refreshes=0;bool saved=false;
float setting(void*,bool aim,bool vertical,void*){assert(!aim);return settings[vertical?1:0];}
void nativeRefresh(void* self,void*){refreshes++;P p=(P)self;float source=read<float>(p+0x12c)/40/389.00848f;write(p+0x14c,source);write(p+0x150,source);write(p+0x170,source*settings[0]);write(p+0x174,source*settings[1]);}
void nativeHorizontal(void* self,int n,bool save,void* m){saved=save;settings[0]=n/10.f;nativeRefresh(self,m);}
void nativeVertical(void* self,int n,bool save,void* m){saved=save;settings[1]=n/10.f;nativeRefresh(self,m);}
void nativeSpeed(void* self,float speed,void* m){write((P)self+0x12c,speed);nativeRefresh(self,m);}
bool near(float a,float b){return std::abs(a-b)<1e-6f;}
int main(){
    lookVerified=true;lookAxisSetting=setting;originalLookRefresh=nativeRefresh;originalLookHorizontal=nativeHorizontal;originalLookVertical=nativeVertical;originalLookSpeed=nativeSpeed;
    write(input+0x1a4,2);write(input+0x12c,10.f);
    nativeRefresh((void*)input,nullptr);assert(read<float>(input+0x170)<.001f);updateLookFactors();
    assert(read<float>(input+0x14c)==1&&read<float>(input+0x170)==.5f&&read<float>(input+0x174)==.75f);
    int before=lookFixes;updateLookFactors();assert(lookFixes==before); // no per-frame rewrite
    for(int n=0;n<5;n++)lookRefreshHook((void*)input,nullptr);
    assert(lookRecalculations==5&&read<float>(input+0x170)==.5f&&read<float>(input+0x174)==.75f); // no repeated amplification
    lookHorizontalHook((void*)input,20,true,nullptr);assert(saved&&read<float>(input+0x170)==2&&read<float>(input+0x174)==.75f);
    lookVerticalHook((void*)input,3,false,nullptr);assert(!saved&&near(read<float>(input+0x174),.3f)&&read<float>(input+0x170)==2);
    lookSpeedHook((void*)input,20,nullptr);assert(read<float>(input+0x14c)==2&&read<float>(input+0x170)==4&&near(read<float>(input+0x174),.6f));
    before=lookFixes;write(input+0x1a4,3);lookRefreshHook((void*)input,nullptr);assert(lookFixes==before&&read<float>(input+0x14c)<.01f);
    write(input+0x1a4,2);enabled=false;lookRefreshHook((void*)input,nullptr);assert(lookFixes==before);restoreLookFactors();assert(lookOwner==0&&lookStatus==1);
    enabled=true;updateLookFactors();assert(read<float>(input+0x170)==4);
    int r=refreshes;lookRefreshRequested=true;updateLookFactors();assert(refreshes==r+1&&!lookRefreshRequested);
    settings[0]=0;lookRefreshHook((void*)input,nullptr);assert(read<float>(input+0x170)==0); // retain a legitimate zero setting
    write(input+0x12c,NAN);before=lookFixes;assert(!correctLookFactors(input,0)&&lookStatus==-3&&lookFixes==before);
    puts("PASS: late DPI recalculation, repeated refresh, per-axis setting changes, speed preservation, mode/disabled restore, one-shot refresh, invalid-value guard");
}
