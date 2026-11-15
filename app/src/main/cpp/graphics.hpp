#pragma once
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <algorithm>
#include <unordered_map>
#include <vector>

// Settings are frozen before the first graphics call; resources in one process
// must agree on their dimensions. The provider retains changes for next launch.
struct GraphicsConfig {
    bool active=false,resolution=false,effects=false;
    int width=0,height=0,nativeWidth=0,nativeHeight=0,fps=0,vegetation=100;
    bool operator==(const GraphicsConfig& o)const {
        return active==o.active&&resolution==o.resolution&&effects==o.effects&&width==o.width&&height==o.height&&
            nativeWidth==o.nativeWidth&&nativeHeight==o.nativeHeight&&fps==o.fps&&vegetation==o.vegetation;
    }
};
std::mutex graphicsMutex;
GraphicsConfig graphicsRequested,graphicsApplied;
bool graphicsConfigured=false,graphicsFrozen=false;
std::atomic<bool> graphicsVerified{false};
std::atomic<int> graphicsHooks{0},graphicsError{0},graphicsFps{0},graphicsWidth{0},graphicsHeight{0};
std::atomic<unsigned> graphicsSwaps{0},graphicsImages{0},graphicsFrames{0},graphicsAreas{0},graphicsViews{0},graphicsScissors{0},graphicsQuality{0},graphicsFailures{0};
std::atomic<unsigned> graphicsEffectImages{0};
std::atomic<int> graphicsSourceWidth{0},graphicsSourceHeight{0};
std::atomic<int> graphicsSceneWidth{0},graphicsSceneHeight{0};
GraphicsConfig graphicsConfiguration(){
    std::lock_guard<std::mutex> lock(graphicsMutex);
    if(!graphicsFrozen){if(graphicsConfigured)graphicsApplied=graphicsRequested;graphicsFrozen=true;}
    return graphicsApplied;
}
struct RenderSize { uint32_t width=0,height=0,newWidth=0,newHeight=0; bool changed()const{return width!=newWidth||height!=newHeight;} };
std::unordered_map<uint64_t,RenderSize> renderImages,renderViews,renderFrames,renderCommands,renderSwaps;
template<class T>uint64_t renderId(T h){return (uint64_t)h;}
template<class T>void rememberRender(std::unordered_map<uint64_t,RenderSize>& m,T h,RenderSize s){
    if(m.size()<32768||m.count(renderId(h)))m[renderId(h)]=s;
    else graphicsFailures++;
}
bool mainRenderSize(uint32_t w,uint32_t h,const GraphicsConfig& c){
    if(c.nativeWidth<=0||c.nativeHeight<=0||w<320||h<240)return false;
    float sx=float(w)/c.nativeWidth,sy=float(h)/c.nativeHeight;
    return sx>=.45f&&sx<=1.15f&&sy>=.45f&&sy<=1.15f&&std::abs(sx/sy-1)<.08f;
}
RenderSize chooseRenderSize(uint32_t w,uint32_t h,bool attachment,const GraphicsConfig& c){
    RenderSize s{w,h,w,h};
    if(!graphicsVerified||!c.active||graphicsError==-4)return s;
    if(c.resolution&&mainRenderSize(w,h,c)){s.newWidth=c.width;s.newHeight=c.height;return s;}
    if(!attachment||!c.effects)return s;
    const int tw=c.resolution?c.width:c.nativeWidth,th=c.resolution?c.height:c.nativeHeight;
    // Only the same half/quarter-size landscape targets recognized by reshook.
    const int sw=graphicsSourceWidth.load()>0?graphicsSourceWidth.load():c.nativeWidth;
    const int sh=graphicsSourceHeight.load()>0?graphicsSourceHeight.load():c.nativeHeight;
    const int sceneW=graphicsSceneWidth.load(),sceneH=graphicsSceneHeight.load();
    for(int divisor:{2,4})if((std::abs(int(w)-sw/divisor)<=3&&std::abs(int(h)-sh/divisor)<=3)||
        (sceneW>0&&std::abs(int(w)-sceneW/divisor)<=3&&std::abs(int(h)-sceneH/divisor)<=3)){
        s.newWidth=std::max(w,uint32_t(divisor==2?std::min(tw,int(w)*2):tw)&~1u);
        s.newHeight=std::max(h,uint32_t(divisor==2?std::min(th,int(h)*2):th)&~1u);break;
    }
    return s;
}
PFN_vkGetInstanceProcAddr realInstanceProc=nullptr;
PFN_vkGetDeviceProcAddr realDeviceProc=nullptr;
PFN_vkCreateSwapchainKHR realCreateSwap=nullptr;
PFN_vkGetSwapchainImagesKHR realSwapImages=nullptr;
PFN_vkCreateImage realCreateImage=nullptr;
PFN_vkCreateImageView realCreateView=nullptr;
PFN_vkCreateFramebuffer realCreateFrame=nullptr;
PFN_vkCmdBeginRenderPass realBeginPass=nullptr;
PFN_vkCmdEndRenderPass realEndPass=nullptr;
PFN_vkCmdSetViewport realViewport=nullptr;
PFN_vkCmdSetScissor realScissor=nullptr;
PFN_vkDestroyImage realDestroyImage=nullptr;
PFN_vkDestroyImageView realDestroyView=nullptr;
PFN_vkDestroyFramebuffer realDestroyFrame=nullptr;
PFN_vkDestroySwapchainKHR realDestroySwap=nullptr;
PFN_vkResetCommandBuffer realResetCommand=nullptr;
PFN_vkVoidFunction deviceProcHook(VkDevice,const char*);
uintptr_t(*realVulkanInitialize)()=nullptr;

VkResult createSwapHook(VkDevice d,const VkSwapchainCreateInfoKHR* p,const VkAllocationCallbacks* a,VkSwapchainKHR* out){
    auto c=graphicsConfiguration();auto info=*p;RenderSize s=chooseRenderSize(p->imageExtent.width,p->imageExtent.height,false,c);
    if(mainRenderSize(p->imageExtent.width,p->imageExtent.height,c)){graphicsSourceWidth=p->imageExtent.width;graphicsSourceHeight=p->imageExtent.height;}
    info.imageExtent={s.newWidth,s.newHeight};
    VkResult r=realCreateSwap(d,&info,a,out);
    if(r!=VK_SUCCESS&&s.changed()){
        graphicsFailures++;graphicsError=-4;s.newWidth=s.width;s.newHeight=s.height;
        r=realCreateSwap(d,p,a,out);
    }
    if(r==VK_SUCCESS){std::lock_guard<std::mutex> lock(graphicsMutex);rememberRender(renderSwaps,*out,s);graphicsWidth=s.newWidth;graphicsHeight=s.newHeight;if(s.changed())graphicsSwaps++;}
    return r;
}
VkResult swapImagesHook(VkDevice d,VkSwapchainKHR h,uint32_t* n,VkImage* out){
    VkResult r=realSwapImages(d,h,n,out);
    if((r==VK_SUCCESS||r==VK_INCOMPLETE)&&out&&n){std::lock_guard<std::mutex> lock(graphicsMutex);auto it=renderSwaps.find(renderId(h));
        if(it!=renderSwaps.end())for(uint32_t i=0;i<*n;i++)rememberRender(renderImages,out[i],it->second);}
    return r;
}
VkResult createImageHook(VkDevice d,const VkImageCreateInfo* p,const VkAllocationCallbacks* a,VkImage* out){
    auto c=graphicsConfiguration();auto info=*p;
    bool attachment=(p->usage&(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))!=0;
    RenderSize s{p->extent.width,p->extent.height,p->extent.width,p->extent.height};
    if((p->usage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)&&mainRenderSize(s.width,s.height,c)){
        graphicsSceneWidth=s.width;graphicsSceneHeight=s.height;
    }
    if(attachment&&p->imageType==VK_IMAGE_TYPE_2D&&p->extent.depth==1&&p->arrayLayers==1)
        s=chooseRenderSize(s.width,s.height,true,c);
    info.extent.width=s.newWidth;info.extent.height=s.newHeight;
    VkResult r=realCreateImage(d,&info,a,out);
    if(r==VK_SUCCESS){std::lock_guard<std::mutex> lock(graphicsMutex);rememberRender(renderImages,*out,s);if(s.changed()){graphicsImages++;if(!mainRenderSize(s.width,s.height,c))graphicsEffectImages++;}
        static std::unordered_map<uint64_t,bool> observed;
        uint64_t key=(uint64_t(s.width)<<32)|s.height;
        if(c.active&&attachment&&observed.size()<64&&!observed.count(key)){observed[key]=true;__android_log_print(ANDROID_LOG_INFO,"vovotool","Render attachment %ux%u -> %ux%u",s.width,s.height,s.newWidth,s.newHeight);}}
    else if(s.changed()){graphicsFailures++;graphicsError=-5;}
    return r;
}
VkResult createViewHook(VkDevice d,const VkImageViewCreateInfo* p,const VkAllocationCallbacks* a,VkImageView* out){
    VkResult r=realCreateView(d,p,a,out);
    if(r==VK_SUCCESS&&p->subresourceRange.baseMipLevel==0){std::lock_guard<std::mutex> lock(graphicsMutex);auto it=renderImages.find(renderId(p->image));if(it!=renderImages.end())rememberRender(renderViews,*out,it->second);}
    return r;
}
VkResult createFrameHook(VkDevice d,const VkFramebufferCreateInfo* p,const VkAllocationCallbacks* a,VkFramebuffer* out){
    auto info=*p;RenderSize s{p->width,p->height,p->width,p->height};bool valid=p->attachmentCount>0&&p->attachmentCount<=32;
    {
        std::lock_guard<std::mutex> lock(graphicsMutex);
        for(uint32_t i=0;valid&&i<p->attachmentCount;i++){
            auto it=renderViews.find(renderId(p->pAttachments[i]));
            if(it==renderViews.end()){valid=false;break;}
            const auto& t=it->second;
            if(i==0){s.newWidth=t.newWidth;s.newHeight=t.newHeight;}
            if(t.width!=p->width||t.height!=p->height||t.newWidth!=s.newWidth||t.newHeight!=s.newHeight)valid=false;
        }
    }
    if(!valid){s.newWidth=s.width;s.newHeight=s.height;}
    info.width=s.newWidth;info.height=s.newHeight;
    VkResult r=realCreateFrame(d,&info,a,out);
    if(r==VK_SUCCESS){std::lock_guard<std::mutex> lock(graphicsMutex);rememberRender(renderFrames,*out,s);if(s.changed())graphicsFrames++;}
    else if(s.changed()){graphicsFailures++;graphicsError=-6;}
    return r;
}
VkRect2D scaleRenderRect(VkRect2D r,RenderSize s){
    const float x=float(s.newWidth)/s.width,y=float(s.newHeight)/s.height;
    r.offset.x=std::clamp(int(std::lround(r.offset.x*x)),0,int(s.newWidth));r.offset.y=std::clamp(int(std::lround(r.offset.y*y)),0,int(s.newHeight));
    r.extent.width=std::min(uint32_t(std::lround(r.extent.width*x)),s.newWidth-std::min(uint32_t(r.offset.x),s.newWidth));
    r.extent.height=std::min(uint32_t(std::lround(r.extent.height*y)),s.newHeight-std::min(uint32_t(r.offset.y),s.newHeight));return r;
}
void beginPassHook(VkCommandBuffer cmd,const VkRenderPassBeginInfo* p,VkSubpassContents contents){
    auto info=*p;{
        std::lock_guard<std::mutex> lock(graphicsMutex);renderCommands.erase(renderId(cmd));auto it=renderFrames.find(renderId(p->framebuffer));
        if(it!=renderFrames.end()&&it->second.changed()){rememberRender(renderCommands,cmd,it->second);info.renderArea=scaleRenderRect(p->renderArea,it->second);graphicsAreas++;}
    }
    realBeginPass(cmd,&info,contents);
}
void endPassHook(VkCommandBuffer c){realEndPass(c);std::lock_guard<std::mutex> lock(graphicsMutex);renderCommands.erase(renderId(c));}
RenderSize commandRenderSize(VkCommandBuffer cmd){std::lock_guard<std::mutex> lock(graphicsMutex);auto it=renderCommands.find(renderId(cmd));return it==renderCommands.end()?RenderSize{}:it->second;}
void viewportHook(VkCommandBuffer cmd,uint32_t first,uint32_t n,const VkViewport* p){
    auto s=commandRenderSize(cmd);if(!s.width||!n||n>16){realViewport(cmd,first,n,p);return;}
    VkViewport copy[16];std::copy(p,p+n,copy);float x=float(s.newWidth)/s.width,y=float(s.newHeight)/s.height;
    for(uint32_t i=0;i<n;i++){copy[i].x*=x;copy[i].y*=y;copy[i].width*=x;copy[i].height*=y;}
    realViewport(cmd,first,n,copy);graphicsViews++;
}
void scissorHook(VkCommandBuffer cmd,uint32_t first,uint32_t n,const VkRect2D* p){
    auto s=commandRenderSize(cmd);if(!s.width||!n||n>16){realScissor(cmd,first,n,p);return;}
    VkRect2D copy[16];for(uint32_t i=0;i<n;i++)copy[i]=scaleRenderRect(p[i],s);realScissor(cmd,first,n,copy);graphicsScissors++;
}
void destroyImageHook(VkDevice d,VkImage h,const VkAllocationCallbacks* a){realDestroyImage(d,h,a);std::lock_guard<std::mutex> lock(graphicsMutex);renderImages.erase(renderId(h));}
void destroyViewHook(VkDevice d,VkImageView h,const VkAllocationCallbacks* a){realDestroyView(d,h,a);std::lock_guard<std::mutex> lock(graphicsMutex);renderViews.erase(renderId(h));}
void destroyFrameHook(VkDevice d,VkFramebuffer h,const VkAllocationCallbacks* a){realDestroyFrame(d,h,a);std::lock_guard<std::mutex> lock(graphicsMutex);renderFrames.erase(renderId(h));}
void destroySwapHook(VkDevice d,VkSwapchainKHR h,const VkAllocationCallbacks* a){realDestroySwap(d,h,a);std::lock_guard<std::mutex> lock(graphicsMutex);renderSwaps.erase(renderId(h));}
VkResult resetCommandHook(VkCommandBuffer c,VkCommandBufferResetFlags f){VkResult r=realResetCommand(c,f);if(r==VK_SUCCESS){std::lock_guard<std::mutex> lock(graphicsMutex);renderCommands.erase(renderId(c));}return r;}
PFN_vkVoidFunction interceptRenderProc(const char* name,PFN_vkVoidFunction real){
    if(!name||!real||!graphicsVerified)return real;
    std::lock_guard<std::mutex> lock(graphicsMutex);
    if(std::strcmp(name,"vkGetDeviceProcAddr")==0){realDeviceProc=(PFN_vkGetDeviceProcAddr)real;return (PFN_vkVoidFunction)deviceProcHook;}
    // Vulkan dispatch pointers are published once for this game's single device.
#define RENDER_PROC(N,F,S,B) if(std::strcmp(name,N)==0){if(real!=(PFN_vkVoidFunction)F){if(S&&(PFN_vkVoidFunction)S!=real){graphicsError=-7;return real;}S=(decltype(S))real;graphicsHooks.fetch_or(B);}return (PFN_vkVoidFunction)F;}
    RENDER_PROC("vkCreateSwapchainKHR",createSwapHook,realCreateSwap,1)
    RENDER_PROC("vkGetSwapchainImagesKHR",swapImagesHook,realSwapImages,2)
    RENDER_PROC("vkCreateImage",createImageHook,realCreateImage,4)
    RENDER_PROC("vkCreateImageView",createViewHook,realCreateView,8)
    RENDER_PROC("vkCreateFramebuffer",createFrameHook,realCreateFrame,16)
    RENDER_PROC("vkCmdBeginRenderPass",beginPassHook,realBeginPass,32)
    RENDER_PROC("vkCmdEndRenderPass",endPassHook,realEndPass,64)
    RENDER_PROC("vkCmdSetViewport",viewportHook,realViewport,128)
    RENDER_PROC("vkCmdSetScissor",scissorHook,realScissor,256)
    RENDER_PROC("vkDestroyImage",destroyImageHook,realDestroyImage,512)
    RENDER_PROC("vkDestroyImageView",destroyViewHook,realDestroyView,1024)
    RENDER_PROC("vkDestroyFramebuffer",destroyFrameHook,realDestroyFrame,2048)
    RENDER_PROC("vkDestroySwapchainKHR",destroySwapHook,realDestroySwap,4096)
    RENDER_PROC("vkResetCommandBuffer",resetCommandHook,realResetCommand,8192)
#undef RENDER_PROC
    return real;
}
PFN_vkVoidFunction instanceProcHook(VkInstance h,const char* n){return interceptRenderProc(n,realInstanceProc(h,n));}
PFN_vkVoidFunction deviceProcHook(VkDevice h,const char* n){return interceptRenderProc(n,realDeviceProc(h,n));}
uintptr_t gameVulkanInitializeHook(){
    // The game resolves its Vulkan table here. Replace its writable resolver
    // slot, without detouring Android's short/branch-sharing loader functions.
    P slot=base.load()+0x15db8460;
    auto real=read<PFN_vkGetInstanceProcAddr>(slot);
    if(real&&real!=instanceProcHook){realInstanceProc=real;write<PFN_vkGetInstanceProcAddr>(slot,instanceProcHook);}
    else if(!real)graphicsError=-3;
    return realVulkanInitialize();
}
bool(*realQualityFloat)(void*,float)=nullptr;
float vegetationValue(const char* name,float v,int percentage){
    if(!std::isfinite(v)||v<0||percentage>=100)return v;
    struct Rule{const char* name;float minimum;};
    static const Rule rules[]={
        {"terrainDistance",50},{"shaderLODDistance",0},{"lightViewDistanceRatio",.1f},
        {"terrainBlendDistance",10},{"meshDepthBlendDistance",1},{"textureStreamingBudget",16},
        {"viewDistanceRatio",0},{"shadowDistance",0},{"characterGroupHairEffectMaximumDistance",0}};
    for(const auto& r:rules)if(std::strcmp(name,r.name)==0)return std::max(r.minimum,v*percentage/100.f);
    return v;
}
bool qualityFloatHook(void* name,float v){
    auto c=graphicsConfiguration();float out=v;
    int length=read<int>(reinterpret_cast<P>(name)+0x10,-1);char key[97]{};char16_t raw[96]{};
    if(graphicsVerified&&c.active&&length>0&&length<=96&&readMemory(reinterpret_cast<P>(name)+0x14,raw,length*2)){
        bool ascii=true;for(int i=0;i<length;i++){if(raw[i]>127){ascii=false;break;}key[i]=char(raw[i]);}
        if(ascii)out=vegetationValue(key,v,c.vegetation);
    }
    bool r=realQualityFloat(name,out);if(r&&out!=v)graphicsQuality++;return r;
}
void updateGraphics(P b){
    auto c=graphicsConfiguration();
    if(!graphicsVerified)return;
    int current=read<int>(b+0x156a74d8);
    // The native setter is only three instructions; a 16-byte inline hook
    // would overwrite the adjacent function. Mirror its validated store.
    if(c.active&&c.fps&&current!=c.fps){write<int>(b+0x156a74d8,c.fps);current=c.fps;}
    graphicsFps=current;
}
void installGraphics(P address){
    for(const auto& pin:profile::graphicsPins){unsigned char bytes[16]{};
        if(!readMemory(address+pin.rva,bytes,16)||std::memcmp(bytes,pin.bytes,16)!=0){graphicsError=-1;return;}}
    if(hookFunction((void*)(address+profile::graphicsQualityFloat),(void*)qualityFloatHook,(void**)&realQualityFloat)!=0){graphicsError=-2;return;}
    if(hookFunction((void*)(address+profile::graphicsVulkanInitialize),(void*)gameVulkanInitializeHook,(void**)&realVulkanInitialize)!=0){graphicsError=-3;return;}
    graphicsVerified=true;
}
