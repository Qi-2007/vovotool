#pragma once
// The Android EasyTouch update uses real touches even in PC UI. Let its
// existing GetMouseTouch implementation feed the map's original gestures.
bool mapMouseSession=false;
std::atomic<int> mapMouseStatus{0};
std::atomic<unsigned long> mapMouseFrames{0},mapMouseConverted{0},mapMouseDowns{0},mapMouseUps{0},mapRealTouchFrames{0};
void(*originalMapTouches)(void*,bool,int,void*)=nullptr;
bool(*mapMouseHeld)(int)=nullptr;
bool(*mapMouseDown)(int)=nullptr;
bool(*mapMouseUp)(int)=nullptr;
bool useMapMouse(bool allowed,int realCount,bool recentTouch,bool down,bool held,bool up,bool& session){
    if(!allowed||realCount!=0||recentTouch){session=false;return false;}
    if(down)session=true;
    if(session&&(down||held||up))return true;
    session=false;return false;
}
void mapTouchesHook(void* self,bool realTouch,int count,void* method){
    const bool allowed=mapMouseStatus==1&&enabled&&applied&&windowFocused&&cursor!=-1&&
        read<int>(input+0x1a4)==2&&read<P>(input+0x40)==(P)self&&classIs((P)self,"EasyTouch")&&
        contextPageIs(topUiContext(),u"InLevelMapPage");
    bool down=false,held=false,up=false;
    if(allowed){
        mapMouseFrames++;if(count>0)mapRealTouchFrames++;
        down=mapMouseDown(0);held=mapMouseHeld(0);up=mapMouseUp(0);
        if(down)mapMouseDowns++;if(up)mapMouseUps++;
    }
    if(useMapMouse(allowed,count,nowMs()<touchLookUntil,down,held,up,mapMouseSession)){
        const uint8_t remote=read<uint8_t>((P)self+0x29);
        // enableRemote would otherwise force the real-touch branch again.
        write<uint8_t>((P)self+0x29,0);
        originalMapTouches(self,false,1,method);
        write<uint8_t>((P)self+0x29,remote);
        mapMouseConverted++;return;
    }
    originalMapTouches(self,realTouch,count,method);
}
void installMapMouse(P address){
    for(const auto& pin:profile::mapMousePins){unsigned char bytes[16]{};
        if(!readMemory(address+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)){mapMouseStatus=-1;return;}}
    mapMouseHeld=(decltype(mapMouseHeld))(address+profile::mapMouseHeld);
    mapMouseDown=(decltype(mapMouseDown))(address+profile::mapMouseDown);
    mapMouseUp=(decltype(mapMouseUp))(address+profile::mapMouseUp);
    if(hookFunction((void*)(address+profile::mapUpdateTouches),(void*)mapTouchesHook,(void**)&originalMapTouches)){
        mapMouseStatus=-2;return;
    }
    mapMouseStatus=1;
}
