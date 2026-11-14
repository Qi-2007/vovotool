#pragma once
// Reuse the original dialogue callback; it owns eligibility, confirmation and sending.
std::atomic<bool> dailyEnabled{true},dailyPending{false},dailyKeyHeld{false},dailyBusy{false};
std::atomic<int> dailyStatus{0};
std::atomic<unsigned long> dailyRequests{0},dailyCalls{0},dailyConfirmed{0},dailyRejected{0};
std::atomic<int64_t> dailyRequestedAt{0};
int64_t dailyLastCall=0,dailyAwaitUntil=0;
P dailyAwaitObject=0;
P dailyModule(P b){
    const P object=read<P>(read<P>(b+profile::singletonRoot)+0xbed0);
    return classIs(object,"PBICDDIHDAP")?object:0;
}
bool dailyRewardTaken(P b,P object){
    return reinterpret_cast<bool(*)(void*,void*)>(b+profile::dailyRewardTaken)(reinterpret_cast<void*>(object),nullptr);
}
void rejectDaily(int status){dailyStatus=status;dailyRejected++;dailyBusy=false;}
void requestDaily(){
    dailyRequests++;
    if(dailyBusy.exchange(true)){dailyRejected++;return;}
    dailyRequestedAt=nowMs();dailyStatus=1;dailyPending=true;
}
void updateDaily(P b){
    if(!dailyEnabled||!enabled){dailyPending=false;dailyBusy=false;dailyAwaitUntil=0;dailyAwaitObject=0;dailyStatus=0;return;}
    if(dailyAwaitUntil){
        const P object=dailyModule(b);
        if(!object||object!=dailyAwaitObject){dailyAwaitUntil=0;dailyAwaitObject=0;rejectDaily(-2);}
        else if(dailyRewardTaken(b,object)){
            dailyAwaitUntil=0;dailyAwaitObject=0;dailyBusy=false;dailyStatus=3;dailyConfirmed++;
            log("Daily reward state confirmed after original callback");
        }else if(nowMs()>dailyAwaitUntil){dailyAwaitUntil=0;dailyAwaitObject=0;rejectDaily(-3);}
    }
    if(!dailyPending.exchange(false))return;
    const int64_t now=nowMs();
    if(!windowFocused||stage!=2||cursor!=-1||now-dailyRequestedAt.load()>1500||
       !contextPageIs(topUiContext(),u"InLevelMainPage")){
        rejectDaily(-1);return;
    }
    if(dailyLastCall&&now-dailyLastCall<2000){rejectDaily(-4);return;}
    const P object=dailyModule(b);
    if(!object){rejectDaily(-2);return;}
    if(dailyRewardTaken(b,object)){dailyStatus=4;dailyBusy=false;return;}
    dailyLastCall=now;dailyCalls++;dailyStatus=2;dailyAwaitObject=object;dailyAwaitUntil=now+30000;
    using Callback=void(*)(void*,int,void*);
    reinterpret_cast<Callback>(b+profile::dailyClaimCallback)(reinterpret_cast<void*>(object),0,nullptr);
    log("Original daily dialogue callback invoked once; awaiting confirmation or server feedback");
}
