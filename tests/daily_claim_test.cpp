// Exercises request gating and response confirmation without contacting the game/server.
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
using P=uintptr_t;
std::atomic<bool> enabled{true},windowFocused{true};
std::atomic<int> stage{2},cursor{-1};
int64_t clockValue=10000;
bool moduleValid=true,pageMain=true,rewardTaken=false;
int callbackCount=0;
int64_t nowMs(){return clockValue;}
void log(const char*){}
template<class T>T read(P address){return static_cast<T>(address==0x100?0x1000:address==0xced0?0x2000:0);}
bool classIs(P object,const char* name){return moduleValid&&object==0x2000&&std::strcmp(name,"PBICDDIHDAP")==0;}
P topUiContext(){return 1;}
bool contextPageIs(P,const char16_t*){return pageMain;}
bool rewardGetter(void* object,void*){assert(reinterpret_cast<P>(object)==0x2000);return rewardTaken;}
void callback(void* object,int index,void* method){assert(reinterpret_cast<P>(object)==0x2000&&index==0&&!method);callbackCount++;}
namespace profile {
constexpr P singletonRoot=0x100;
const P dailyRewardTaken=reinterpret_cast<P>(&rewardGetter),dailyClaimCallback=reinterpret_cast<P>(&callback);
}
#include "daily_claim.hpp"
void reset(){
 enabled=true;windowFocused=true;stage=2;cursor=-1;clockValue=10000;moduleValid=pageMain=true;rewardTaken=false;callbackCount=0;
 dailyEnabled=true;dailyPending=false;dailyBusy=false;dailyKeyHeld=false;dailyStatus=0;
 dailyRequests=0;dailyCalls=0;dailyConfirmed=0;dailyRejected=0;dailyRequestedAt=0;dailyLastCall=dailyAwaitUntil=0;dailyAwaitObject=0;
}
int main(){
 reset();requestDaily();updateDaily(0);assert(callbackCount==1&&dailyStatus==2);
 requestDaily();updateDaily(0);assert(callbackCount==1&&dailyRejected==1&&dailyBusy);
 rewardTaken=true;updateDaily(0);assert(dailyStatus==3&&dailyConfirmed==1&&!dailyBusy);
 reset();rewardTaken=true;requestDaily();updateDaily(0);assert(dailyStatus==4&&callbackCount==0);
 reset();requestDaily();windowFocused=false;updateDaily(0);windowFocused=true;updateDaily(0);assert(dailyStatus==-1&&callbackCount==0);
 reset();pageMain=false;requestDaily();updateDaily(0);assert(dailyStatus==-1&&callbackCount==0);
 reset();requestDaily();clockValue+=1501;updateDaily(0);assert(dailyStatus==-1&&callbackCount==0);
 reset();requestDaily();dailyEnabled=false;updateDaily(0);dailyEnabled=true;updateDaily(0);assert(dailyStatus==0&&!dailyBusy&&callbackCount==0);
 reset();moduleValid=false;requestDaily();updateDaily(0);assert(dailyStatus==-2&&callbackCount==0);
 reset();requestDaily();updateDaily(0);clockValue+=30001;updateDaily(0);updateDaily(0);assert(dailyStatus==-3&&callbackCount==1&&!dailyBusy);
 reset();requestDaily();updateDaily(0);rewardTaken=true;updateDaily(0);rewardTaken=false;clockValue+=100;requestDaily();updateDaily(0);assert(dailyStatus==-4&&callbackCount==1);
 reset();requestDaily();updateDaily(0);moduleValid=false;updateDaily(0);assert(dailyStatus==-2&&callbackCount==1&&!dailyBusy);
 puts("PASS: single call, busy, confirmation, already claimed, focus, menu, expiry, disable, object, timeout, cooldown");
}
