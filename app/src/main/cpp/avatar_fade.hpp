#pragma once
// Override only the camera factor while native material data is calculated.
struct AvatarFadeTarget {P object=0,owner=0;};
AvatarFadeTarget avatarFadeTargets[2];
std::atomic<bool> avatarFadeEnabled{true};
std::atomic<int> avatarFadeStatus{0},avatarFadeRequested{1000},avatarFadeOutput{1000},avatarFadeCount{0};
std::atomic<unsigned long> avatarFadeCalls{0},avatarFadeOverrides{0},avatarFadeRestores{0};
bool avatarFadeWasActive=false;
void(*originalAvatarFadeRecalculate)(void*,void*)=nullptr;
bool avatarFadeValue(float value){return std::isfinite(value)&&value>=0&&value<=1;}
bool validAvatarFadeTarget(const AvatarFadeTarget& target){
    return target.object&&classIs(target.object,"LOFPEGCAEFG")&&read<P>(target.object+0x28)==target.owner&&classIs(target.owner,"PECHLEKMHDF");
}
void avatarFadeRecalculateHook(void* self,void* method){
    avatarFadeCalls++;
    bool override=false;float saved=0;
    if(avatarFadeStatus==1)for(const auto& target:avatarFadeTargets){
        if(target.object!=(P)self||!validAvatarFadeTarget(target))continue;
        saved=read<float>((P)self+0x184,-1);
        if(avatarFadeValue(saved)){
            override=enabled&&avatarFadeEnabled&&saved!=1;
            avatarFadeRequested=static_cast<int>(saved*1000);avatarFadeOutput=override?1000:static_cast<int>(saved*1000);
            if(override){write<float>((P)self+0x184,1);avatarFadeOverrides++;}
        }
        break;
    }
    originalAvatarFadeRecalculate(self,method);
    if(override)write<float>((P)self+0x184,saved);
}
void updateAvatarFade(P address){
    if(avatarFadeStatus!=1)return;
    const P root=read<P>(address+profile::singletonRoot),mgr=read<P>(root+0x9070);
    const P world=classIs(mgr,"LFEMMJADHPE")?read<P>(mgr+0x50):0;
    const P camera=classIs(world,"BDBIBJBCPOJ")?read<P>(world+0x508):0;
    const P binding=classIs(camera,"BGMCJLBBGJF")?read<P>(camera+0x200):0;
    const bool active=enabled&&avatarFadeEnabled;
    int count=0;
    for(int i=0;i<2;i++){
        auto& target=avatarFadeTargets[i];
        P object=classIs(binding,"PPEAKFODDID")?read<P>(binding+(i?0x80:0x20)):0;
        AvatarFadeTarget current{object,read<P>(object+0x28)};
        if(!validAvatarFadeTarget(current)){target={};continue;}
        const bool changed=target.object!=current.object||target.owner!=current.owner;
        target=current;count++;
        if(changed||active!=avatarFadeWasActive){
            avatarFadeRecalculateHook((void*)object,nullptr);
            if(!active)avatarFadeRestores++;
        }
    }
    avatarFadeCount=count;avatarFadeWasActive=active;
}
void installAvatarFade(P address){
    for(const auto& pin:profile::avatarFadePins){unsigned char bytes[16]{};
        if(!readMemory(address+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)){avatarFadeStatus=-1;return;}}
    if(hookFunction((void*)(address+profile::avatarFadeRecalculate),(void*)avatarFadeRecalculateHook,(void**)&originalAvatarFadeRecalculate)){
        avatarFadeStatus=-2;return;
    }
    avatarFadeStatus=1;
}
