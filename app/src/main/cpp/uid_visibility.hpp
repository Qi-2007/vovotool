#pragma once
// Display-only HUD UID text control. Account data and original text are not logged.
std::atomic<bool> uidRequested{false},uidApplied{false};
std::atomic<int> uidStatus{0};
std::atomic<unsigned long> uidDiscoveries{0},uidWrites{0};
void(*originalUidText)(void*,void*,void*)=nullptr;
void(*originalTextMesh)(void*,void*,void*)=nullptr;
std::mutex uidMutex;
P uidText=0,uidIdentity=0;
std::u16string uidBaseline;
bool uidDirty=false,uidAborted=false;
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
    if(text.size()>=4&&text.compare(0,3,u"UID")==0)return true;
    if(text.size()<6||text.size()>20)return false;
    for(char16_t c:text)if(c<u'0'||c>u'9')return false;
    return true;
}
bool validUidTarget(P object,P identity){
    return identity&&classIs(object,"Text")&&read<P>(object+0x10)==identity;
}
bool discoverUid(P object,P value){
    if(!classIs(object,"Text")||!read<P>(object+0x10))return false;
    std::u16string source;
    if(!readManagedText(value,source)||!looksLikeUid(source))return false;
    const P b=base.load();if(!b)return false;
    auto nameGetter=reinterpret_cast<void*(*)(void*,void*)>(b+profile::objectName);
    P name=reinterpret_cast<P>(nameGetter(reinterpret_cast<void*>(object),nullptr));
    std::u16string actualName;
    if(!readManagedText(name,actualName)||actualName!=u"TxtUID")return false;
    std::lock_guard<std::mutex> guard(uidMutex);
    const P identity=read<P>(object+0x10);
    if(uidText!=object||uidIdentity!=identity){
        uidText=object;uidIdentity=identity;uidBaseline=source;
        uidDirty=false;uidApplied=false;uidAborted=false;uidDiscoveries++;
        log("HUD TxtUID text discovered; original text retained in memory only");
    }
    return true;
}
void uidTextHook(void* self,void* value,void* method){
    const P object=reinterpret_cast<P>(self);
    bool known=false;
    {std::lock_guard<std::mutex> guard(uidMutex);known=object==uidText&&validUidTarget(object,uidIdentity);}
    if(!known)known=discoverUid(object,reinterpret_cast<P>(value));
    bool hide=false;
    if(known){
        std::u16string source;
        if(readManagedText(reinterpret_cast<P>(value),source)){
            std::lock_guard<std::mutex> guard(uidMutex);
            uidBaseline=source;
            hide=enabled&&uidRequested&&!uidAborted;
            uidDirty=hide&&!source.empty();uidApplied=hide;
        }
    }
    originalUidText(self,hide?nullptr:value,method);
    if(known&&hide){
        std::u16string observed;
        bool ok=readManagedText(read<P>(object+0xd8),observed)&&observed.empty();
        uidStatus=ok?2:-2;uidWrites++;
        if(!ok){std::lock_guard<std::mutex> guard(uidMutex);uidAborted=true;}
    }
}
void uidMeshHook(void* self,void* helper,void* method){
    const P object=reinterpret_cast<P>(self);
    bool known=false;
    {std::lock_guard<std::mutex> guard(uidMutex);known=object==uidText&&validUidTarget(object,uidIdentity);}
    if(!known)discoverUid(object,read<P>(object+0xd8));
    originalTextMesh(self,helper,method);
}
void updateUidVisibility(P b){
    const bool hide=enabled&&uidRequested;
    const int64_t now=nowMs();if(now-uidLastUpdate<200)return;uidLastUpdate=now;
    std::lock_guard<std::mutex> guard(uidMutex);
    if(!validUidTarget(uidText,uidIdentity)){
        uidText=uidIdentity=0;uidBaseline.clear();uidDirty=false;uidApplied=false;uidAborted=false;
        uidStatus=hide?1:0;return;
    }
    if(uidAborted)return;
    std::u16string observed;
    if(!readManagedText(read<P>(uidText+0xd8),observed)){uidStatus=-2;uidAborted=true;return;}
    if(hide){
        if(!observed.empty()){
            uidBaseline=observed;uidDirty=true;
            originalUidText(reinterpret_cast<void*>(uidText),nullptr,nullptr);uidWrites++;
        }
        if(!readManagedText(read<P>(uidText+0xd8),observed)||!observed.empty()){
            uidStatus=-2;uidAborted=true;return;
        }
        uidApplied=true;uidStatus=2;
    }else if(uidDirty){
        auto makeString=reinterpret_cast<void*(*)(void*,const char16_t*,void*)>(b+profile::stringFromUtf16);
        void* restored=makeString(nullptr,uidBaseline.c_str(),nullptr);
        if(!restored){uidStatus=-2;uidAborted=true;return;}
        originalUidText(reinterpret_cast<void*>(uidText),restored,nullptr);uidWrites++;
        if(!readManagedText(read<P>(uidText+0xd8),observed)||observed!=uidBaseline){uidStatus=-2;uidAborted=true;return;}
        uidDirty=false;uidApplied=false;uidStatus=3;
        log("HUD UID display restored");
    }else {uidApplied=false;if(uidStatus!=3)uidStatus=0;}
}
