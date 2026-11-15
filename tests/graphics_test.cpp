// Exercise production geometry/resource tracking without loading the game.
#include <atomic>
#include <mutex>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <android/log.h>
using P=uintptr_t;
std::atomic<P> base{0};
unsigned char fakeMemory[0x6000]{};bool emulate=false;
int64_t nowMs(){return 5000;}
template<class T>T read(P p,T fallback={}){if(!emulate||p<0x1000||p+sizeof(T)>sizeof(fakeMemory))return fallback;T value;std::memcpy(&value,fakeMemory+p,sizeof(value));return value;}
template<class T>void write(P p,T value){assert(emulate&&p>=0x1000&&p+sizeof(T)<=sizeof(fakeMemory));std::memcpy(fakeMemory+p,&value,sizeof(value));}
bool readMemory(P,void*,size_t){return false;}
bool classIs(P p,const char* n){return emulate&&((p==0x1000&&std::strcmp(n,"PostProcessLayer")==0)||(p==0x2000&&std::strcmp(n,"Camera")==0)||(p==0x3000&&std::strcmp(n,"RenderTexture")==0)||(p==0x5000&&std::strcmp(n,"Texture2D")==0));}
void* fakeMainCamera(void*){return (void*)0x2000;}
int (*hookFunction)(void*,void*,void**)=nullptr;
namespace profile {
struct Pin{P rva;unsigned char bytes[16];};constexpr Pin graphicsPins[]={{0,{}}};
constexpr P graphicsQualityFloat=0,graphicsVulkanInitialize=0;
constexpr P graphicsScreenWidth=0,graphicsScreenHeight=0,graphicsRebuildScene=0,graphicsPostProcessUpdate=0,graphicsPrepareScene=0;
constexpr P graphicsSetResolution=0;
const P graphicsMainCamera=reinterpret_cast<P>(fakeMainCamera);
constexpr Pin photoPins[]={{0,{}}};constexpr P photoRead=0,photoResize=0;
}
#include "graphics.hpp"
#include "photo.hpp"
PhotoRect seenPhoto{};const PhotoRect* seenPhotoPointer=nullptr;
int resizeCalls=0,readCalls=0;bool allowResize=true;
int fakeTextureWidth(void*){return read<int>(0x51e0);}
int fakeTextureHeight(void*){return read<int>(0x51e4);}
bool fakePhotoResize(void*,int w,int h){resizeCalls++;if(!allowResize)return false;write<int>(0x51e0,w);write<int>(0x51e4,h);return true;}
void fakePhotoRead(void*,const PhotoRect* r,int,int,bool){readCalls++;seenPhoto=*r;seenPhotoPointer=r;}
int seenResolutionW=0,seenResolutionH=0,seenRefresh=0;bool seenFullscreen=false;
void fakeSetResolution(int w,int h,bool fullscreen,int refresh){seenResolutionW=w;seenResolutionH=h;seenFullscreen=fullscreen;seenRefresh=refresh;}
VkFramebufferCreateInfo seenFrame{};
VkResult fakeFrame(VkDevice,const VkFramebufferCreateInfo* p,const VkAllocationCallbacks*,VkFramebuffer* out){seenFrame=*p;*out=(VkFramebuffer)300;return VK_SUCCESS;}
void fakeBegin(VkCommandBuffer,const VkRenderPassBeginInfo*,VkSubpassContents){}
void fakeEnd(VkCommandBuffer){}
VkResult fakeReset(VkCommandBuffer,VkCommandBufferResetFlags){return VK_SUCCESS;}
void fakeCopy(VkCommandBuffer,VkImage,VkImageLayout,VkBuffer,uint32_t,const VkBufferImageCopy*){}
VkExtent2D seenSwap{};
VkResult fakeSwap(VkDevice,const VkSwapchainCreateInfoKHR* p,const VkAllocationCallbacks*,VkSwapchainKHR* out){seenSwap=p->imageExtent;*out=(VkSwapchainKHR)900;return VK_SUCCESS;}
int preparedW=0,preparedH=0;
void fakePrepare(void*,void*){preparedW=read<int>(0x1094);preparedH=read<int>(0x1098);}
void fakeRebuild(void*,void*){
    float gain=read<uint8_t>(0x1160)?read<float>(0x1168):1.f;
    write<int>(0x40e0,int(std::floor(read<int>(0x1094)*gain))&~1);write<int>(0x40e4,int(std::floor(read<int>(0x1098)*gain))&~1);
}
int main(){
    graphicsVerified=true;photoReady=true;
    GraphicsConfig c;c.active=true;c.resolution=true;c.nativeWidth=3200;c.nativeHeight=2000;c.width=2240;c.height=1400;c.path=2;
    auto s=chooseRenderSize(3200,2000,true,c);assert(!s.changed());
    assert(!chooseRenderSize(2429,1518,false,c).changed()); // output/photo dimensions remain native
    assert(!chooseRenderSize(1476,922,true,c).changed()); // scene changes happen above Vulkan
    assert(!chooseRenderSize(1024,1024,true,c).changed());
    assert(!chooseRenderSize(64,40,true,c).changed());
    c.path=1;assert(chooseRenderSize(1476,922,true,c).newWidth==2240);
    assert(chooseRenderSize(2429,1518,false,c).newWidth==2240);
    graphicsSourceWidth=2429;graphicsSourceHeight=1518;
    assert(chooseRenderSize(2429,1518,true,c).newWidth==2240); // output and photo must now agree on the custom size
    assert(chooseRenderSize(1476,922,true,c).newWidth==2240); // scene still uses the old Vulkan path
    c.width=3200;c.height=2000;assert(chooseRenderSize(2429,1518,true,c).newWidth==3200);
    assert(chooseRenderSize(1476,922,true,c).newWidth==3200);c.width=2240;c.height=1400;
    photoReady=false;assert(!chooseRenderSize(2429,1518,false,c).changed());photoReady=true;
    c.path=3;c.width=1920;c.height=1200;renderOutputWidth=1920;renderOutputHeight=1200;
    assert(chooseRenderSize(1166,728,true,c).newWidth==1920); // native output change lowers the game's default scene dimensions
    assert(!chooseRenderSize(583,364,true,c).changed()); // half-resolution effects stay independent
    assert(!chooseRenderSize(1920,1200,false,c).changed()); // physical and logical output already agree
    c.width=2240;c.height=1400;
    c.path=0;assert(!chooseRenderSize(1476,922,true,c).changed());c.path=2;
    c.active=false;assert(!chooseRenderSize(3200,2000,true,c).changed());c.active=true;
    graphicsError=-4;assert(!chooseRenderSize(3200,2000,true,c).changed());graphicsError=0;
    s={3200,2000,2240,1400};auto rect=scaleRenderRect({{0,0},{3200,2000}},s);assert(rect.extent.width==2240&&rect.extent.height==1400);
    rect=scaleRenderRect({{3100,1950},{200,200}},s);assert(rect.extent.width==70&&rect.extent.height==35);
    rect=scaleRenderRect({{4000,2500},{10,10}},s);assert(rect.extent.width==0&&rect.extent.height==0&&rect.offset.x==2240);
    c.resolution=false;c.effects=true;graphicsSourceWidth=2429;graphicsSourceHeight=1518;
    s=chooseRenderSize(1214,759,true,c);assert(s.newWidth==2428&&s.newHeight==1518);
    s=chooseRenderSize(607,380,true,c);assert(s.newWidth==3200&&s.newHeight==2000);
    assert(!chooseRenderSize(1214,759,false,c).changed());
    assert(!chooseRenderSize(512,512,true,c).changed());
    c.effects=false;assert(!chooseRenderSize(1214,759,true,c).changed());
    assert(vegetationValue("terrainDistance",400,50)==200);
    assert(vegetationValue("terrainDistance",60,10)==50);
    assert(vegetationValue("unrelatedSetting",500,10)==500);
    assert(vegetationValue("shadowDistance",500,100)==500);
    graphicsConfigured=true;graphicsRequested=c;graphicsRequested.fps=90;assert(graphicsConfiguration().fps==90);
    graphicsRequested.fps=30;assert(graphicsConfiguration().fps==90); // pending until next process
    graphicsConfigured=false;graphicsFrozen=false;graphicsApplied={};assert(!graphicsConfiguration().active);
    graphicsConfigured=true;graphicsRequested.active=true;assert(!graphicsConfiguration().active); // failed startup read stays transparent
    realCreateFrame=fakeFrame;realBeginPass=fakeBegin;realEndPass=fakeEnd;realResetCommand=fakeReset;
    renderViews[100]={3200,2000,2240,1400};renderViews[101]=renderViews[100];
    VkImageView views[]={(VkImageView)100,(VkImageView)101};VkFramebufferCreateInfo info{};
    info.attachmentCount=2;info.pAttachments=views;info.width=3200;info.height=2000;info.layers=1;VkFramebuffer frame{};
    assert(createFrameHook(nullptr,&info,nullptr,&frame)==VK_SUCCESS&&seenFrame.width==2240);
    VkRenderPassBeginInfo pass{};pass.framebuffer=frame;pass.renderArea.extent={3200,2000};auto cmd=(VkCommandBuffer)400;
    beginPassHook(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);assert(commandRenderSize(cmd).newWidth==2240);
    endPassHook(cmd);assert(commandRenderSize(cmd).width==0);
    beginPassHook(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);resetCommandHook(cmd,0);assert(commandRenderSize(cmd).width==0);
    views[1]=(VkImageView)999;createFrameHook(nullptr,&info,nullptr,&frame);assert(seenFrame.width==3200);
    graphicsApplied=c;graphicsApplied.active=true;graphicsApplied.resolution=true;graphicsApplied.effects=false;
    realCreateSwap=fakeSwap;VkSwapchainCreateInfoKHR swap{};swap.imageExtent={2429,1518};VkSwapchainKHR chain{};
    createSwapHook(nullptr,&swap,nullptr,&chain);assert(seenSwap.width==2429&&seenSwap.height==1518);
    realCopyImageToBuffer=fakeCopy;renderImages[700]={2429,1518,2429,1518};VkBufferImageCopy region{};region.imageExtent={2429,1518,1};
    copyImageToBufferHook(nullptr,(VkImage)700,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,{},1,&region);assert(renderReadbackErrors==0&&renderReadbacks==1);
    renderImages[700]={2429,1518,1920,1200};copyImageToBufferHook(nullptr,(VkImage)700,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,{},1,&region);assert(renderReadbackErrors==1); // detect the old photo regression
    for(float gain:{.5f,.6075f,.67f,.8f,1.f})for(int target:{1280,1920,2560,3200}){
        int input=sceneInputSize(target,gain),result=int(std::floor(float(input)*gain))&~1;assert(result==target);
    }
    emulate=true;write<P>(0x1260,0x2000);write<P>(0x10b8,0x3000);write<P>(0x3010,0x4000);
    write<int>(0x10b0,2429);write<int>(0x10b4,1518);write<float>(0x1088,1.f);write<float>(0x108c,1.f);
    write<uint8_t>(0x1160,1);write<float>(0x1168,.8f);
    graphicsApplied.path=2;graphicsApplied.width=1920;graphicsApplied.height=1200;realRebuildScene=fakeRebuild;realPrepareScene=fakePrepare;
    rebuildSceneHook((void*)0x1000,nullptr);assert(read<int>(0x40e0)==1920&&read<int>(0x40e4)==1200);
    assert(read<int>(0x1094)==0&&read<int>(0x1098)==0);
    prepareSceneHook((void*)0x1000,nullptr);assert(preparedW==1920&&preparedH==1200);
    assert(read<int>(0x1094)==0&&read<int>(0x1098)==0&&read<float>(0x1088)==1.f&&read<float>(0x108c)==1.f);
    write<uint8_t>(0x10d1,1);prepareSceneHook((void*)0x1000,nullptr);assert(preparedW==0&&preparedH==0); // extra cameras stay native
    realSetResolution=fakeSetResolution;graphicsApplied.path=3;graphicsApplied.width=1920;graphicsApplied.height=1200;
    screenResolutionHook(2429,1518,false,90);assert(seenResolutionW==1920&&seenResolutionH==1200&&!seenFullscreen&&seenRefresh==90);
    graphicsApplied.active=false;screenResolutionHook(2429,1518,true,60);assert(seenResolutionW==2429&&seenResolutionH==1518&&seenFullscreen&&seenRefresh==60);graphicsApplied.active=true;
    graphicsApplied.path=1;graphicsApplied.width=2240;graphicsApplied.height=1400;graphicsApplied.active=true;graphicsApplied.resolution=true;
    renderOutputWidth=2429;renderOutputHeight=1518;graphicsWidth=2240;graphicsHeight=1400;
    write<P>(0x5010,0x5100);write<P>(0x5100,0x5300);write<int(*)(void*)>(0x5450,fakeTextureWidth);write<int(*)(void*)>(0x5458,fakeTextureHeight);
    write<int>(0x51e0,2429);write<int>(0x51e4,1518);photoResize=fakePhotoResize;realPhotoRead=fakePhotoRead;
    PhotoRect full{0,0,2429,1518};photoReadHook((void*)0x5000,&full,0,0,false);
    assert(resizeCalls==1&&readCalls==1&&seenPhoto.width==2240&&seenPhoto.height==1400&&read<int>(0x51e0)==2240);
    assert(full.width==2429&&full.height==1518); // const caller Rect unchanged
    photoReadHook((void*)0x5000,&full,0,0,false);assert(resizeCalls==1&&readCalls==2); // reused texture
    graphicsApplied.width=3200;graphicsApplied.height=2000;graphicsWidth=3200;graphicsHeight=2000;
    write<int>(0x51e0,2429);write<int>(0x51e4,1518);photoReadHook((void*)0x5000,&full,0,0,false);
    assert(resizeCalls==2&&readCalls==3&&seenPhoto.width==3200&&seenPhoto.height==2000&&read<int>(0x51e0)==3200);
    PhotoRect partial{100,100,640,480};photoReadHook((void*)0x5000,&partial,0,0,false);assert(resizeCalls==2&&seenPhotoPointer==&partial);
    graphicsApplied.active=false;photoReadHook((void*)0x5000,&full,0,0,false);assert(resizeCalls==2&&seenPhotoPointer==&full);graphicsApplied.active=true;
    write<int>(0x51e0,2429);write<int>(0x51e4,1518);allowResize=false;int before=readCalls;
    photoReadHook((void*)0x5000,&full,0,0,false);assert(photoStatus==-3&&readCalls==before); // no read into failed allocation
    puts("PASS: Vulkan sizing and cleanup, engine experiments, photo resize before read for smaller/larger output, reused texture, partial/disabled pass-through, allocation failure");
}
