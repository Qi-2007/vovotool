#pragma once
// Measured compensation for the normal PC camera's native touch route.
// This is not a change to shared mouse factors or saved game sensitivity.
constexpr float normalTouchVerticalGain=57.29577951308232f;
std::atomic<int64_t> touchLookUntil{0};
std::atomic<int> touchLookStatus{0};
std::atomic<unsigned long> touchLookCalls{0},touchLookCorrections{0};
void(*originalTouchRotate)(void*,float,float,void*)=nullptr;
bool correctNormalTouch(bool pcWorld,bool recentTouch,bool normalCamera,float& y){
    if(!pcWorld||!recentTouch||!normalCamera||!std::isfinite(y))return false;
    const float corrected=y*normalTouchVerticalGain;
    if(!std::isfinite(corrected))return false;
    y=corrected;return true;
}
void touchRotateHook(void* self,float x,float y,void* method){
    touchLookCalls++;
    const bool pcWorld=touchLookStatus==1&&enabled&&applied&&windowFocused&&cursor==-1&&
        read<int>(input+0x1a4)==2&&classIs((P)self,"MonoInputEasyTouch_H4");
    if(pcWorld&&nowMs()<touchLookUntil){
        const P root=read<P>(base.load()+profile::singletonRoot);
        const P cameraManager=read<P>(root+0x9070);
        const P camera=classIs(cameraManager,"LFEMMJADHPE")?read<P>(cameraManager+0x50):0;
        const P controller=classIs(camera,"BDBIBJBCPOJ")?read<P>(camera+0x508):0;
        const P state=classIs(controller,"BGMCJLBBGJF")?read<P>(controller+0x9a8):0;
        // Select the normal controller implementation, across worlds/domains.
        // Aiming and other camera implementations retain their native input.
        const bool normal=classIs(state,"KBAKEOHEJKN")&&classIs(read<P>(state+0x1a0),"DBOLJJLIEJI");
        if(correctNormalTouch(pcWorld,true,normal,y))touchLookCorrections++;
    }
    originalTouchRotate(self,x,y,method);
}
void installTouchLook(P address){
    unsigned char bytes[16]{};
    if(!readMemory(address+profile::touchRotate,bytes,16)||
       std::memcmp(bytes,profile::touchPins[0].bytes,16)){touchLookStatus=-1;return;}
    if(hookFunction((void*)(address+profile::touchRotate),(void*)touchRotateHook,(void**)&originalTouchRotate)){
        touchLookStatus=-2;return;
    }
    touchLookStatus=1;
}
