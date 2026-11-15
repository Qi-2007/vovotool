#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <cstdio>
#include <limits>
using P=uintptr_t;
std::atomic<bool> enabled{true},windowFocused{true};
unsigned char memory[0x1800]{};P top=0x800;bool mainPage=true,validIterator=true,validOwner=true;
template<class T>T read(P p,T fallback={}){if(p+sizeof(T)>sizeof(memory))return fallback;T v;std::memcpy(&v,memory+p,sizeof(v));return v;}
template<class T>void write(P p,T v){assert(p+sizeof(T)<=sizeof(memory));std::memcpy(memory+p,&v,sizeof(v));}
bool readMemory(P p,void* v,size_t n){if(p+n>sizeof(memory))return false;std::memcpy(v,memory+p,n);return true;}
bool classIs(P p,const char* n){return (p==0x100&&validIterator&&!std::strcmp(n,"GDMACLLPFAL"))||(p==0x800&&validOwner&&!std::strcmp(n,"BFIFKIIDFKN"));}
P topUiContext(){return top;}bool contextPageIs(P,const char16_t*){return mainPage;}
namespace profile {struct Pin{P rva;unsigned char bytes[16];};constexpr Pin teamLoadingPins[]={{0,{}}};constexpr P teamLoadingMoveNext=0;}
int(*hookFunction)(void*,void*,void**)=nullptr;
#include "team_loading.hpp"
float observed=0;int calls=0;void* expectedMethod=(void*)0x1234;
bool original(void* it,void* method){assert(it==(void*)0x100&&method==expectedMethod);calls++;observed=read<float>(0xe10);return calls%2;}
void run(bool skipped){float before=read<float>(0xe10);int n=calls;bool result=teamLoadingHook((void*)0x100,expectedMethod);assert(calls==n+1&&result==bool(calls%2));assert(skipped?observed==0:(std::isnan(before)?std::isnan(observed):observed==before));float after=read<float>(0xe10);assert(std::isnan(before)?std::isnan(after):before==after);}
int main(){
    originalTeamLoading=original;teamLoadingStatus=1;write<P>(0x118,0x800);write<int>(0x120,0);write<int>(0x9b4,0);write<int>(0x9e4,3);write<float>(0xe10,2);write<float>(0xea4,0);
    run(false);write<int>(0x120,1);run(true); // initialization remains intact, then skip and restore
    write<int>(0x9b4,1);run(false);write<int>(0x9b4,0);write<int>(0x9e4,2);run(false);write<int>(0x9e4,3);run(true);
    top=0x900;run(false);top=0x800;mainPage=false;run(false);mainPage=true;
    enabled=false;run(false);enabled=true;teamLoadingEnabled=false;run(false);teamLoadingEnabled=true;
    windowFocused=false;run(false);windowFocused=true;validIterator=false;run(false);validIterator=true;validOwner=false;run(false);validOwner=true;
    write<float>(0xea4,-1);run(false);write<float>(0xea4,0);write<float>(0xe10,std::numeric_limits<float>::quiet_NaN());run(false);write<float>(0xe10,2);
    teamLoadingStatus=-1;run(false);teamLoadingStatus=1;run(true);assert(teamLoadingSkipped==3);
    puts("PASS: native result/call preservation, first resume, page readiness, temporary duration restoration, config/focus/class guards and invalid values");
}
