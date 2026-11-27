#include <atomic>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
using P=uintptr_t;
std::atomic<bool> enabled{true};
std::map<P,unsigned char> memory;
std::map<P,std::string> classes;
int64_t clockMs=100;
int64_t nowMs(){return clockMs;}
void log(const char*){}
bool readMemory(P p,void* dst,size_t n){
    for(size_t i=0;i<n;i++)if(!memory.count(p+i))return false;
    for(size_t i=0;i<n;i++)static_cast<unsigned char*>(dst)[i]=memory[p+i];
    return true;
}
template<class T>T read(P p,T fallback={}){T value;return readMemory(p,&value,sizeof(value))?value:fallback;}
template<class T>void write(P p,T value){for(size_t i=0;i<sizeof(value);i++)memory[p+i]=reinterpret_cast<unsigned char*>(&value)[i];}
bool classIs(P p,const char* name){return classes.count(p)&&classes[p]==name;}
void setter(void*,uint32_t,bool,void*);
namespace profile {
struct Pin{P rva;unsigned char bytes[16];};
constexpr Pin regionUnlockPins[]={{0x100,{}}};
constexpr P singletonRoot=0x200,regionUnlockSetLocal=0x100;
}
#include "region_unlock.hpp"
int calls=0;bool accept=true;
void setter(void* owner,uint32_t id,bool active,void* method){
    assert(owner==(void*)0x2000&&active&&method==nullptr);
    assert(id==47||id==48||id==7058);calls++;
    write<int>(0x4038,read<int>(0x4038)+1); // set_Item changes version, not count.
    if(accept)for(int n=0;n<3;n++)if(regionUnlockIds[n]==id)write<uint8_t>(0x502c+n*16,1);
}
int main(){
    const P b=reinterpret_cast<P>(setter)-profile::regionUnlockSetLocal;
    for(int i=0;i<16;i++)memory[b+0x100+i]=0;
    write<P>(b+profile::singletonRoot,0x1000);write<P>(0x9c40,0x2000);write<P>(0x2070,0x3000);
    classes={{0x2000,"HBIJFOJIFAH"},{0x3000,"OGOCCLDFCLI"}};
    write<P>(0x3010,0x4000);write<P>(0x4010,0x5000);
    write<int>(0x4038,1579);write<int>(0x403c,3);write<int>(0x5018,3); // relogin: version exceeds capacity
    for(int n=0;n<3;n++){
        write<int>(0x5020+n*16,1);write<uint32_t>(0x5028+n*16,regionUnlockIds[n]);write<uint8_t>(0x502c+n*16,n==0?1:0);
    }
    installRegionUnlock(b);assert(regionUnlockVerified);
    requestRegionUnlock(false,1);updateRegionUnlock(b);assert(calls==0);
    requestRegionUnlock(true,2);updateRegionUnlock(b);assert(calls==3&&regionUnlockStatus==2);
    assert(read<int>(0x4038)==1582&&read<int>(0x403c)==3);
    for(auto& value:regionUnlockValues)assert(value==1);
    requestRegionUnlock(true,2);updateRegionUnlock(b);assert(calls==3); // repeated polling is consumed once
    requestRegionUnlock(true,3);requestRegionUnlock(false,0);updateRegionUnlock(b);assert(calls==3&&regionUnlockStatus==-4);
    requestRegionUnlock(true,3);updateRegionUnlock(b);assert(calls==3); // cancellation cannot replay
    requestRegionUnlock(true,4);enabled=false;updateRegionUnlock(b);assert(calls==3&&regionUnlockStatus==-4);enabled=true;
    requestRegionUnlock(true,5);clockMs+=60001;updateRegionUnlock(b);assert(calls==3&&regionUnlockStatus==-5);
    requestRegionUnlock(true,6);classes[0x2000]="WrongClass";updateRegionUnlock(b);assert(calls==3&&regionUnlockPending==6);
    classes[0x2000]="HBIJFOJIFAH";write<int>(0x403c,4);updateRegionUnlock(b);assert(calls==3); // invalid dictionary
    write<int>(0x403c,3);write<uint32_t>(0x5048,999);updateRegionUnlock(b);assert(calls==3); // missing 7058: no partial writes
    write<uint32_t>(0x5048,7058);write<uint8_t>(0x504c,2);updateRegionUnlock(b);assert(calls==3); // invalid bool
    write<uint8_t>(0x504c,0);updateRegionUnlock(b);assert(calls==6&&regionUnlockStatus==2); // cache becomes ready
    accept=false;write<uint8_t>(0x503c,0);requestRegionUnlock(true,7);updateRegionUnlock(b);assert(calls==9&&regionUnlockStatus==-3);
    memory[b+0x100]=1;regionUnlockVerified=false;installRegionUnlock(b);assert(!regionUnlockVerified&&regionUnlockStatus==-1);
    requestRegionUnlock(true,8);updateRegionUnlock(b);assert(calls==9&&regionUnlockStatus==-1);
    // A new login refreshes state values and version while retaining the cache.
    memory[b+0x100]=0;installRegionUnlock(b);accept=true;
    for(int n=0;n<3;n++)write<uint8_t>(0x502c+n*16,0);
    write<int>(0x4038,3000);
    requestRegionUnlock(true,9);updateRegionUnlock(b);assert(calls==12&&regionUnlockStatus==2);
    for(auto& value:regionUnlockValues)assert(value==1);
    assert(read<int>(0x4038)==3003&&read<int>(0x403c)==3);
    puts("PASS: relogin/version growth, exact IDs, one-shot requests, level/master cancellation, expiry, classes, cache layout, missing states, readback and fingerprints");
}
