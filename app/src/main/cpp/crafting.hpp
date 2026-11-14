#pragma once
// Open the original SynthesisPage through the same named-page route as Fufu.
std::atomic<bool> craftEnabled{true},craftPending{false},craftKeyHeld{false};
std::atomic<int> craftStatus{0};
std::atomic<unsigned long> craftRequests{0},craftDispatches{0},craftOpened{0},craftRejected{0};
std::atomic<int64_t> craftRequestedAt{0};
int64_t craftLastDispatch=0,craftAwaitUntil=0;

P topUiContext(){
    const P stack=read<P>(manager+0x200);
    if(!classIs(stack,"Stack`1"))return 0;
    const P items=read<P>(stack+0x18);
    const int count=read<int>(stack+0x24),capacity=read<int>(items+0x18);
    if(count<1||count>32||capacity<count||capacity>128)return 0;
    return read<P>(items+0x20+(count-1)*sizeof(P));
}
bool contextPageIs(P context,const char16_t* expected){
    if(!context)return false;
    const P descriptor=read<P>(context+0x130);
    if(!classIs(descriptor,"DJJPPIKOCAC"))return false;
    std::u16string name;
    return readManagedText(read<P>(descriptor+0x10),name)&&name==expected;
}
void requestCrafting(){
    craftRequests++;
    if(craftPending||nowMs()-craftRequestedAt.load()<800)return;
    craftRequestedAt=nowMs();craftStatus=1;craftPending=true;
}
void updateCrafting(P b){
    if(craftAwaitUntil){
        if(contextPageIs(topUiContext(),u"SynthesisPage")){
            craftAwaitUntil=0;craftStatus=3;craftOpened++;log("Portable SynthesisPage opened through the native page dispatcher");
        }else if(nowMs()>craftAwaitUntil){craftAwaitUntil=0;craftStatus=-3;craftRejected++;}
    }
    if(!craftPending.exchange(false))return;
    const int64_t now=nowMs();
    if(!craftEnabled||!enabled||!windowFocused||stage!=2||cursor!=-1||
       !contextPageIs(topUiContext(),u"InLevelMainPage")||now-craftRequestedAt.load()>1500||
       craftAwaitUntil||now-craftLastDispatch<1200){
        craftStatus=-1;craftRejected++;log("Portable synthesis request refused outside the focused main page");return;
    }
    auto makeString=reinterpret_cast<void*(*)(void*,const char16_t*,void*)>(b+profile::stringFromUtf16);
    void* page=makeString(nullptr,u"SynthesisPage",nullptr);
    if(!page){craftStatus=-2;craftRejected++;return;}
    using OpenPage=bool(*)(void*,void*,void*,void*,void*);
    craftLastDispatch=now;craftDispatches++;
    bool accepted=reinterpret_cast<OpenPage>(b+profile::openPageByName)(page,nullptr,nullptr,nullptr,nullptr);
    if(accepted){craftStatus=2;craftAwaitUntil=now+8000;}
    else {craftStatus=-2;craftRejected++;log("Native page dispatcher declined the synthesis request");}
}
