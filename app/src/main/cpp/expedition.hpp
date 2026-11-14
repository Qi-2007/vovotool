#pragma once
// ExplorationPage's original handler accepts a nullable return action and area string.
std::atomic<bool> expeditionEnabled{true},expeditionPending{false},expeditionKeyHeld{false};
std::atomic<int> expeditionStatus{0};
std::atomic<unsigned long> expeditionRequests{0},expeditionDispatches{0},expeditionOpened{0},expeditionRejected{0};
std::atomic<int64_t> expeditionRequestedAt{0};
int64_t expeditionLastDispatch=0,expeditionAwaitUntil=0;

void requestExpedition(){
    expeditionRequests++;
    if(expeditionPending||nowMs()-expeditionRequestedAt.load()<800)return;
    expeditionRequestedAt=nowMs();expeditionStatus=1;expeditionPending=true;
}
void updateExpedition(P b){
    if(expeditionAwaitUntil){
        if(contextPageIs(topUiContext(),u"ExplorationPage")){
            expeditionAwaitUntil=0;expeditionStatus=3;expeditionOpened++;log("Portable ExplorationPage opened through the original expedition handler");
        }else if(nowMs()>expeditionAwaitUntil){expeditionAwaitUntil=0;expeditionStatus=-3;expeditionRejected++;}
    }
    if(!expeditionPending.exchange(false))return;
    const int64_t now=nowMs();
    if(!expeditionEnabled||!enabled||!windowFocused||stage!=2||cursor!=-1||
       !contextPageIs(topUiContext(),u"InLevelMainPage")||now-expeditionRequestedAt.load()>1500||
       expeditionAwaitUntil||craftAwaitUntil||now-expeditionLastDispatch<1200){
        expeditionStatus=-1;expeditionRejected++;return;
    }
    using OpenExpedition=bool(*)(void*,void*);
    expeditionLastDispatch=now;expeditionDispatches++;
    // A null area follows the game's TryParse-failed branch and selects its default area.
    bool accepted=reinterpret_cast<OpenExpedition>(b+profile::openExpedition)(nullptr,nullptr);
    if(accepted){expeditionStatus=2;expeditionAwaitUntil=now+8000;}
    else {expeditionStatus=-2;expeditionRejected++;log("Native expedition handler declined the page request");}
}
