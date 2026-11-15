#pragma once
// Captured Android pointer events are pixel deltas. The game's native mouse
// refresh divides its source/cache by DPI for a different input path. Restore
// a consistent pixel basis after refresh while retaining per-axis settings.
struct LookFactors {float source=0,x=0,y=0;};
bool pixelLookFactors(float speed,float horizontal,float vertical,LookFactors& out){
    if(!std::isfinite(speed)||speed<=0||speed>1000||!std::isfinite(horizontal)||!std::isfinite(vertical)||
       horizontal<0||vertical<0||horizontal>64||vertical>64)return false;
    out.source=speed/10.f; // the pinned InputManager constructor uses speed 10
    out.x=out.source*horizontal;out.y=out.source*vertical;return true;
}
std::atomic<bool> lookVerified{false},lookRefreshRequested{false};
std::atomic<int> lookStatus{0},lookFixes{0},lookRecalculations{0},lookWriter{0};
P lookOwner=0;
void(*originalLookRefresh)(void*,void*)=nullptr;
void(*originalLookHorizontal)(void*,int,bool,void*)=nullptr;
void(*originalLookVertical)(void*,int,bool,void*)=nullptr;
void(*originalLookSpeed)(void*,float,void*)=nullptr;
float(*lookAxisSetting)(void*,bool,bool,void*)=nullptr;
bool correctLookFactors(P target,int writer){
    if(!lookVerified||!enabled||!applied||target!=input||!classIs(target,"MHCNPJOGAGH")||read<int>(target+0x1a4)!=2)return false;
    LookFactors out;
    if(!pixelLookFactors(read<float>(target+0x12c),lookAxisSetting((void*)target,false,false,nullptr),
                         lookAxisSetting((void*)target,false,true,nullptr),out)){lookStatus=-3;return false;}
    float source[2]{},cache[2]{};readMemory(target+0x14c,source,sizeof(source));readMemory(target+0x170,cache,sizeof(cache));
    lookOwner=target;lookWriter=writer;
    if(source[0]!=out.source||source[1]!=out.source||cache[0]!=out.x||cache[1]!=out.y){
        write<float>(target+0x14c,out.source);write<float>(target+0x150,out.source);
        write<float>(target+0x170,out.x);write<float>(target+0x174,out.y);lookFixes++;
        __android_log_print(ANDROID_LOG_INFO,"vovotool","Look pixel basis restored after writer %d: source %.7g/%.7g -> %.7g, cache %.7g/%.7g -> %.7g/%.7g",writer,source[0],source[1],out.source,cache[0],cache[1],out.x,out.y);
    }
    lookStatus=2;return true;
}
void lookRefreshHook(void* self,void* method){originalLookRefresh(self,method);if((P)self==input){lookRecalculations++;correctLookFactors((P)self,1);}}
void lookHorizontalHook(void* self,int value,bool save,void* method){originalLookHorizontal(self,value,save,method);if((P)self==input){lookRecalculations++;correctLookFactors((P)self,2);}}
void lookVerticalHook(void* self,int value,bool save,void* method){originalLookVertical(self,value,save,method);if((P)self==input){lookRecalculations++;correctLookFactors((P)self,3);}}
void lookSpeedHook(void* self,float value,void* method){originalLookSpeed(self,value,method);if((P)self==input){lookRecalculations++;correctLookFactors((P)self,4);}}
void updateLookFactors(){
    if(!lookVerified||!enabled||!applied||read<int>(input+0x1a4)!=2)return;
    if(lookRefreshRequested.exchange(false))lookRefreshHook((void*)input,nullptr);
    else if(lookOwner!=input)correctLookFactors(input,0);
}
void restoreLookFactors(){if(lookVerified&&originalLookRefresh){originalLookRefresh((void*)input,nullptr);lookOwner=0;lookStatus=1;}}
void installLookFactors(P address){
    for(const auto& pin:profile::lookPins){unsigned char bytes[16]{};if(!readMemory(address+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)){lookStatus=-1;return;}}
    lookAxisSetting=(decltype(lookAxisSetting))(address+profile::lookAxisSetting);
    if(hookFunction((void*)(address+profile::lookRefresh),(void*)lookRefreshHook,(void**)&originalLookRefresh)||
       hookFunction((void*)(address+profile::lookHorizontal),(void*)lookHorizontalHook,(void**)&originalLookHorizontal)||
       hookFunction((void*)(address+profile::lookVertical),(void*)lookVerticalHook,(void**)&originalLookVertical)||
       hookFunction((void*)(address+profile::lookSpeed),(void*)lookSpeedHook,(void**)&originalLookSpeed)){lookStatus=-2;return;}
    lookVerified=true;lookStatus=1;
}
