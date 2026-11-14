#pragma once
// HUD text and Paimon UID group visibility. Account data is never modified.
std::atomic<bool> uidRequested{false},uidApplied{false};
std::atomic<int> uidStatus{0};
std::atomic<unsigned long> uidDiscoveries{0},uidWrites{0};
void(*originalUidText)(void*,void*,void*)=nullptr;
void(*originalTextMesh)(void*,void*,void*)=nullptr;
// SetActive may synchronously re-enter Text hooks through Unity lifecycle callbacks.
std::recursive_mutex uidMutex;
struct UidTarget {
    P object=0,identity=0;
    P group=0,groupIdentity=0;
    std::u16string baseline;
    bool dirty=false,aborted=false;
    bool groupOnly=false,groupOriginalActive=false,groupDirty=false,groupAborted=false;
};
// A menu opening must not replace the HUD's saved text or pending restoration.
UidTarget uidTargets[16];
int64_t uidLastUpdate=0;

bool readManagedText(P object,std::u16string& text){
    if(!object){text.clear();return true;}
    if(!classIs(object,"String"))return false;
    int length=read<int>(object+0x10,-1);
    if(length<0||length>64)return false;
    char16_t characters[65]{};
    if(length&&!readMemory(object+0x14,characters,length*sizeof(char16_t)))return false;
    text.assign(characters,length);return true;
}
bool looksLikeUid(const std::u16string& text){
    size_t start=0;
    if(text.size()>=3&&text.compare(0,3,u"UID")==0){
        start=3;
        while(start<text.size()&&(text[start]==u' '||text[start]==u':'||text[start]==u'\u00a0'))start++;
    }
    const size_t digits=text.size()-start;
    if(digits<6||digits>20)return false;
    for(size_t i=start;i<text.size();i++)if(text[i]<u'0'||text[i]>u'9')return false;
    return true;
}
bool validUidTarget(const UidTarget& target){
    return target.identity&&classIs(target.object,"Text")&&read<P>(target.object+0x10)==target.identity;
}
UidTarget* findUidTarget(P object){
    for(auto& target:uidTargets)if(target.object==object&&validUidTarget(target))return &target;
    return nullptr;
}
bool validUidGroup(const UidTarget& target){
    return target.groupIdentity&&classIs(target.group,"GameObject")&&read<P>(target.group+0x10)==target.groupIdentity;
}
P resolveUidGroup(P object,P b){
    using Getter=P(*)(void*,void*);
    const auto componentObject=reinterpret_cast<Getter>(b+profile::componentGameObject);
    const auto componentTransform=reinterpret_cast<Getter>(b+profile::componentTransform);
    const auto nameGetter=reinterpret_cast<Getter>(b+profile::objectName);
    const auto parentGetter=reinterpret_cast<Getter>(b+profile::transformParent);
    constexpr const char16_t* path[]={u"PlayerID",u"Layout",u"UID",u"GrpPlayerCard",u"Right",u"GrpProfile",u"PlayerProfilePage",u"Pages",u"Canvas"};
    P transform=componentTransform(reinterpret_cast<void*>(object),nullptr);
    P groupTransform=0;
    for(size_t depth=0;depth<sizeof(path)/sizeof(path[0]);depth++){
        if(!(classIs(transform,"RectTransform")||classIs(transform,"Transform"))||!read<P>(transform+0x10))return 0;
        std::u16string name;
        if(!readManagedText(nameGetter(reinterpret_cast<void*>(transform),nullptr),name)||name!=path[depth])return 0;
        if(depth==2)groupTransform=transform;
        transform=parentGetter(reinterpret_cast<void*>(transform),nullptr);
    }
    return componentObject(reinterpret_cast<void*>(groupTransform),nullptr);
}
bool discoverUid(P object,P value){
    if(!classIs(object,"Text")||!read<P>(object+0x10))return false;
    std::u16string source;
    if(!readManagedText(value,source)||!looksLikeUid(source))return false;
    const P b=base.load();if(!b)return false;
    auto nameGetter=reinterpret_cast<void*(*)(void*,void*)>(b+profile::objectName);
    P name=reinterpret_cast<P>(nameGetter(reinterpret_cast<void*>(object),nullptr));
    std::u16string actualName;
    if(!readManagedText(name,actualName)||(actualName!=u"TxtUID"&&actualName!=u"PlayerID"))return false;
    const P group=actualName==u"PlayerID"?resolveUidGroup(object,b):0;
    // PlayerID is only a discovery anchor. Refuse an unverified parent path
    // rather than rewriting its text as a separate fallback.
    if(actualName==u"PlayerID"&&(!classIs(group,"GameObject")||!read<P>(group+0x10)))return false;
    std::lock_guard<std::recursive_mutex> guard(uidMutex);
    if(findUidTarget(object))return true;
    for(auto& target:uidTargets){
        if(validUidTarget(target)||validUidGroup(target))continue;
        target=UidTarget{};target.object=object;target.identity=read<P>(object+0x10);
        if(actualName==u"TxtUID")target.baseline=source;
        if(classIs(group,"GameObject")&&read<P>(group+0x10)){
            target.groupOnly=true;
            target.group=group;target.groupIdentity=read<P>(group+0x10);
            auto activeSelf=reinterpret_cast<bool(*)(void*,void*)>(b+profile::objectActiveSelf);
            target.groupOriginalActive=activeSelf(reinterpret_cast<void*>(group),nullptr);
            log("Paimon UID frame group verified");
        }
        uidDiscoveries++;
        log(actualName==u"PlayerID"?"Paimon UID group discovered via PlayerID":"HUD TxtUID text discovered");
        return true;
    }
    return false;
}
void summarizeUidVisibility(bool hide){
    bool found=false,failed=false,dirty=false;
    for(const auto& target:uidTargets){
        if(!target.groupOnly&&validUidTarget(target)){found=true;failed|=target.aborted;dirty|=target.dirty;}
        if(validUidGroup(target)){found=true;failed|=target.groupAborted;dirty|=target.groupDirty;}
    }
    uidApplied=hide&&found&&!failed;
    if(failed)uidStatus=-2;
    else if(hide)uidStatus=found?2:1;
    else if(!found)uidStatus=0;
    else if(!dirty&&uidStatus!=3)uidStatus=0;
}
// Runs on the game thread with uidMutex held. Call the original setter so our
// own clearing/restoring never replaces the application's saved baseline.
bool applyUidVisibility(UidTarget& target,P b,bool hide){
    if(target.aborted)return false;
    std::u16string observed;
    if(!readManagedText(read<P>(target.object+0xd8),observed)){target.aborted=true;return false;}
    if(hide){
        if(!observed.empty()){
            target.baseline=observed;target.dirty=true;
            originalUidText(reinterpret_cast<void*>(target.object),nullptr,nullptr);uidWrites++;
        }
        if(!readManagedText(read<P>(target.object+0xd8),observed)||!observed.empty())target.aborted=true;
    }else if(target.dirty){
        auto makeString=reinterpret_cast<void*(*)(void*,const char16_t*,void*)>(b+profile::stringFromUtf16);
        void* restored=makeString(nullptr,target.baseline.c_str(),nullptr);
        if(!restored){target.aborted=true;return false;}
        originalUidText(reinterpret_cast<void*>(target.object),restored,nullptr);uidWrites++;
        if(!readManagedText(read<P>(target.object+0xd8),observed)||observed!=target.baseline)target.aborted=true;
        else {target.dirty=false;uidStatus=3;}
    }
    return !target.aborted;
}
void updateUidGroup(UidTarget& target,P b,bool hide){
    if(!validUidGroup(target)){
        target.group=target.groupIdentity=0;target.groupDirty=false;target.groupAborted=false;return;
    }
    auto activeSelf=reinterpret_cast<bool(*)(void*,void*)>(b+profile::objectActiveSelf);
    auto setActive=reinterpret_cast<void(*)(void*,bool,void*)>(b+profile::objectSetActive);
    void* group=reinterpret_cast<void*>(target.group);
    const bool active=activeSelf(group,nullptr);
    if(hide&&!target.groupAborted){
        if(active){
            if(!target.groupDirty)target.groupOriginalActive=active;
            // Claim restoration before calling game code, even if verification fails.
            target.groupDirty=true;setActive(group,false,nullptr);uidWrites++;
            target.groupAborted=activeSelf(group,nullptr);
        }
    }else if(!hide&&target.groupDirty){
        setActive(group,target.groupOriginalActive,nullptr);uidWrites++;
        if(activeSelf(group,nullptr)==target.groupOriginalActive){
            target.groupDirty=false;target.groupAborted=false;uidStatus=3;
        }else target.groupAborted=true;
    }
}
void uidTextHook(void* self,void* value,void* method){
    const P object=reinterpret_cast<P>(self);
    bool known=false;
    {std::lock_guard<std::recursive_mutex> guard(uidMutex);known=findUidTarget(object)!=nullptr;}
    if(!known)known=discoverUid(object,reinterpret_cast<P>(value));
    if(known){
        std::lock_guard<std::recursive_mutex> guard(uidMutex);
        auto* target=findUidTarget(object);known=target&&!target->groupOnly;
    }
    bool hide=false;
    if(known){
        std::u16string source;
        const bool readable=readManagedText(reinterpret_cast<P>(value),source);
        std::lock_guard<std::recursive_mutex> guard(uidMutex);
        if(auto* target=findUidTarget(object)){
            if(readable){
                target->baseline=source;
                hide=enabled&&uidRequested&&!target->aborted;
                target->dirty=hide&&!source.empty();
            }else target->aborted=true;
        }
    }
    originalUidText(self,hide?nullptr:value,method);
    if(known){
        std::lock_guard<std::recursive_mutex> guard(uidMutex);
        if(auto* target=findUidTarget(object)){
            if(hide){
                std::u16string observed;
                if(!readManagedText(read<P>(object+0xd8),observed)||!observed.empty())target->aborted=true;
                uidWrites++;
            }
        }
        summarizeUidVisibility(enabled&&uidRequested);
    }
}
void uidMeshHook(void* self,void* helper,void* method){
    const P object=reinterpret_cast<P>(self);
    bool known=false;
    {std::lock_guard<std::recursive_mutex> guard(uidMutex);known=findUidTarget(object)!=nullptr;}
    if(!known)known=discoverUid(object,read<P>(object+0xd8));
    if(known){
        std::lock_guard<std::recursive_mutex> guard(uidMutex);
        const bool hide=enabled&&uidRequested;
        if(auto* target=findUidTarget(object);target&&!target->groupOnly)applyUidVisibility(*target,base.load(),hide);
        summarizeUidVisibility(hide);
    }
    originalTextMesh(self,helper,method);
}
void updateUidVisibility(P b){
    const bool hide=enabled&&uidRequested;
    const int64_t now=nowMs();if(now-uidLastUpdate<200)return;uidLastUpdate=now;
    std::lock_guard<std::recursive_mutex> guard(uidMutex);
    for(auto& target:uidTargets){
        if(validUidTarget(target)){if(!target.groupOnly)applyUidVisibility(target,b,hide);}
        else {target.object=target.identity=0;target.baseline.clear();target.dirty=false;target.aborted=false;}
        // SetActive invokes Unity lifecycle callbacks; defer this until Tick,
        // outside Text.OnPopulateMesh's canvas rebuild.
        updateUidGroup(target,b,hide);
    }
    summarizeUidVisibility(hide);
}
