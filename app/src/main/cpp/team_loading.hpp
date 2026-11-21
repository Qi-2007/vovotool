#pragma once
// Resume the existing team-page coroutine after the main page is ready.
// Its initialization and native eligibility checks remain in the original.
std::atomic<bool> teamLoadingEnabled{false};
std::atomic<int> teamLoadingStatus{0},teamLoadingDuration{0},teamLoadingElapsed{0};
std::atomic<unsigned long> teamLoadingCalls{0},teamLoadingSkipped{0},teamLoadingWaiting{0};
bool(*originalTeamLoading)(void*,void*)=nullptr;
bool teamLoadingHook(void* iterator,void* method){
    teamLoadingCalls++;
    P owner=0;float saved=0;bool shorten=false;
    if(teamLoadingStatus==1&&enabled&&teamLoadingEnabled&&windowFocused&&
       classIs((P)iterator,"GDMACLLPFAL")&&read<int>((P)iterator+0x20)==1){
        owner=read<P>((P)iterator+0x18);
        if(classIs(owner,"BFIFKIIDFKN")){
            float elapsed=read<float>(owner+0x6a4,-1);saved=read<float>(owner+0x610,-1);
            const bool valid=std::isfinite(saved)&&saved>=0&&saved<=60&&std::isfinite(elapsed)&&elapsed>=0&&elapsed<=120;
            if(valid){teamLoadingDuration=static_cast<int>(saved*1000);teamLoadingElapsed=static_cast<int>(elapsed*1000);}
            shorten=valid&&owner==topUiContext()&&contextPageIs(owner,u"InLevelMainPage")&&
                read<int>(owner+0x1b4,-1)==0&&read<int>(owner+0x1e4,-1)==3;
            if(shorten){write<float>(owner+0x610,0);teamLoadingSkipped++;}
            else teamLoadingWaiting++;
        }
    }
    bool result=originalTeamLoading(iterator,method);
    if(shorten)write<float>(owner+0x610,saved);
    return result;
}
void installTeamLoading(P address){
    for(const auto& pin:profile::teamLoadingPins){unsigned char bytes[16]{};
        if(!readMemory(address+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)){teamLoadingStatus=-1;return;}}
    if(hookFunction((void*)(address+profile::teamLoadingMoveNext),(void*)teamLoadingHook,(void**)&originalTeamLoading)){
        teamLoadingStatus=-2;return;
    }
    teamLoadingStatus=1;
}
