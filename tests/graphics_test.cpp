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
template<class T>T read(P,T fallback={}){return fallback;}
template<class T>void write(P,T){}
bool readMemory(P,void*,size_t){return false;}
int (*hookFunction)(void*,void*,void**)=nullptr;
namespace profile {
struct Pin{P rva;unsigned char bytes[16];};constexpr Pin graphicsPins[]={{0,{}}};
constexpr P graphicsQualityFloat=0,graphicsVulkanInitialize=0;
}
#include "graphics.hpp"
VkFramebufferCreateInfo seenFrame{};
VkResult fakeFrame(VkDevice,const VkFramebufferCreateInfo* p,const VkAllocationCallbacks*,VkFramebuffer* out){seenFrame=*p;*out=(VkFramebuffer)300;return VK_SUCCESS;}
void fakeBegin(VkCommandBuffer,const VkRenderPassBeginInfo*,VkSubpassContents){}
void fakeEnd(VkCommandBuffer){}
VkResult fakeReset(VkCommandBuffer,VkCommandBufferResetFlags){return VK_SUCCESS;}
int main(){
    graphicsVerified=true;
    GraphicsConfig c;c.active=true;c.resolution=true;c.nativeWidth=3200;c.nativeHeight=2000;c.width=2240;c.height=1400;
    auto s=chooseRenderSize(3200,2000,true,c);assert(s.newWidth==2240&&s.newHeight==1400);
    assert(!chooseRenderSize(1024,1024,true,c).changed());
    assert(!chooseRenderSize(64,40,true,c).changed());
    c.active=false;assert(!chooseRenderSize(3200,2000,true,c).changed());c.active=true;
    graphicsError=-4;assert(!chooseRenderSize(3200,2000,true,c).changed());graphicsError=0;
    auto rect=scaleRenderRect({{0,0},{3200,2000}},s);assert(rect.extent.width==2240&&rect.extent.height==1400);
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
    puts("PASS: resolution selection, effect targets, clipping, native pass-through, quality limits, frozen settings, attachment agreement, command cleanup");
}
