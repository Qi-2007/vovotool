#pragma once
// The local open-state entry publishes the game's normal local region refresh.
// Requests arrive from the config thread; all game calls run from tickHook.
constexpr uint32_t regionUnlockIds[]={47,48,7058};
std::atomic<bool> regionUnlockAllowed{false};
std::atomic<int64_t> regionUnlockSeen{0},regionUnlockPending{0},regionUnlockRequestedAt{0};
std::atomic<int> regionUnlockStatus{0},regionUnlockRequests{0};
std::atomic<int> regionUnlockValues[3]{{-1},{-1},{-1}};
std::atomic<bool> regionUnlockVerified{false};

void requestRegionUnlock(bool allowed,int64_t token){
    regionUnlockAllowed=allowed;
    if(!allowed){if(regionUnlockPending.exchange(0))regionUnlockStatus=-4;return;}
    if(token<=0||token<=regionUnlockSeen.load())return;
    regionUnlockSeen=token;regionUnlockRequests++;regionUnlockRequestedAt=nowMs();regionUnlockStatus=1;
    regionUnlockPending=token;
}

void installRegionUnlock(P b){
    for(const auto& pin:profile::regionUnlockPins){
        unsigned char bytes[16]{};
        if(!readMemory(b+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)!=0){regionUnlockStatus=-1;return;}
    }
    regionUnlockVerified=true;
}

bool readRegionUnlockValues(P state,int (&values)[3]){
    const P dictionary=read<P>(state+0x10),entries=read<P>(dictionary+0x10);
    const int count=read<int>(dictionary+0x38,-1),capacity=read<int>(entries+0x18,-1);
    if(count<0||count>capacity||capacity>5000)return false;
    for(int n=0;n<3;n++)values[n]=-1;
    for(int i=0;i<count;i++){
        const P entry=entries+0x20+i*16;
        if(read<int>(entry,-1)<0)continue;
        const uint32_t key=read<uint32_t>(entry+8);
        for(int n=0;n<3;n++)if(key==regionUnlockIds[n]){
            int value=read<uint8_t>(entry+12,255);
            if(value>1)return false;
            values[n]=value;
        }
    }
    return values[0]>=0&&values[1]>=0&&values[2]>=0;
}

void updateRegionUnlock(P b){
    if(!regionUnlockPending.load())return;
    if(!regionUnlockAllowed||!enabled){regionUnlockPending=0;regionUnlockStatus=-4;return;}
    if(nowMs()-regionUnlockRequestedAt.load()>60000){regionUnlockPending=0;regionUnlockStatus=-5;return;}
    if(!regionUnlockVerified){regionUnlockPending=0;regionUnlockStatus=-1;return;}
    // Leave the request queued through loading until the local state cache exists.
    const P root=read<P>(b+profile::singletonRoot),owner=read<P>(root+0x8c40),state=read<P>(owner+0x70);
    if(!classIs(owner,"HBIJFOJIFAH")||!classIs(state,"OGOCCLDFCLI"))return;
    int values[3];
    if(!readRegionUnlockValues(state,values))return;
    if(!regionUnlockPending.exchange(0))return;
    using SetOpenState=void(*)(void*,uint32_t,bool,void*);
    auto set=reinterpret_cast<SetOpenState>(b+profile::regionUnlockSetLocal);
    for(uint32_t id:regionUnlockIds)set(reinterpret_cast<void*>(owner),id,true,nullptr);
    if(read<P>(root+0x8c40)!=owner||read<P>(owner+0x70)!=state||!readRegionUnlockValues(state,values)){
        regionUnlockStatus=-2;return;
    }
    for(int n=0;n<3;n++)regionUnlockValues[n]=values[n];
    regionUnlockStatus=values[0]&&values[1]&&values[2]?2:-3;
    if(regionUnlockStatus==2)log("Local region open states 47, 48 and 7058 applied and read back");
}
