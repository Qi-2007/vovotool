#pragma once
// ReadPixels receives a managed Texture2D and a Rect by reference. Resize its
// backing allocation before changing the read rectangle, including upscales.
struct PhotoRect {float x=0,y=0,width=0,height=0;};
std::atomic<int> photoRequests{0},photoReads{0},photoAdjusted{0},photoStatus{0};
std::atomic<int> photoWidth{0},photoHeight{0},photoReadWidth{0},photoReadHeight{0},photoTextureWidth{0},photoTextureHeight{0};
void(*realPhotoRead)(void*,const PhotoRect*,int,int,bool)=nullptr;
bool(*photoResize)(void*,int,int)=nullptr;
bool photoTextureSize(void* object,int& w,int& h){
    P managed=reinterpret_cast<P>(object);if(!classIs(managed,"Texture2D"))return false;
    P native=read<P>(managed+0x10),table=read<P>(native);
    auto width=read<int(*)(void*)>(table+0x150),height=read<int(*)(void*)>(table+0x158);
    if(!native||!width||!height)return false;
    w=width(reinterpret_cast<void*>(native));h=height(reinterpret_cast<void*>(native));
    return w>0&&h>0&&w<=8192&&h<=8192;
}
void photoReadHook(void* texture,const PhotoRect* rect,int dx,int dy,bool recalc){
    auto c=graphicsConfiguration();int tw=0,th=0;PhotoRect adjusted{};const PhotoRect* actual=rect;
    const int ow=renderOutputWidth.load(),oh=renderOutputHeight.load(),aw=graphicsWidth.load(),ah=graphicsHeight.load();
    bool full=rect&&rect->x==0&&rect->y==0&&ow>0&&oh>0&&std::abs(rect->width-ow)<.5f&&std::abs(rect->height-oh)<.5f;
    if(photoReady&&graphicsVerified&&c.active&&c.resolution&&c.path==1&&graphicsError>=0&&
       full&&dx==0&&dy==0&&aw==c.width&&ah==c.height&&(aw!=ow||ah!=oh)&&photoTextureSize(texture,tw,th)&&
       ((tw==ow&&th==oh)||(tw==aw&&th==ah))){
        photoRequests++;
        if(tw!=aw||th!=ah){
            if(!photoResize(texture,aw,ah)||!photoTextureSize(texture,tw,th)||tw!=aw||th!=ah){
                // Never issue a larger GPU read into the old CPU allocation.
                photoStatus=-3;return;
            }
        }
        adjusted=*rect;adjusted.width=float(aw);adjusted.height=float(ah);actual=&adjusted;
        photoWidth=aw;photoHeight=ah;photoAdjusted++;photoStatus=2;
        __android_log_print(ANDROID_LOG_INFO,"vovotool_photo","Photo full-frame read %dx%d -> %dx%d, Texture2D=%dx%d",ow,oh,aw,ah,tw,th);
    }
    if(actual){photoReadWidth=int(actual->width);photoReadHeight=int(actual->height);}
    if(photoTextureSize(texture,tw,th)){photoTextureWidth=tw;photoTextureHeight=th;}
    realPhotoRead(texture,actual,dx,dy,recalc);photoReads++;
}
void installPhoto(P address){
    for(const auto& pin:profile::photoPins){unsigned char bytes[16]{};if(!readMemory(address+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)){photoStatus=-1;return;}}
    photoResize=reinterpret_cast<decltype(photoResize)>(address+profile::photoResize);
    if(hookFunction((void*)(address+profile::photoRead),(void*)photoReadHook,(void**)&realPhotoRead)){photoStatus=-2;return;}
    photoStatus=1;photoReady=true;
}
