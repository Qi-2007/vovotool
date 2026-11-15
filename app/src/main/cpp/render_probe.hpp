#pragma once
#include <vulkan/vulkan.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <array>
#include <cmath>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Passive API observer. Catalogs API objects, then follows recorded commands
// through successful queue submission to presentation. Never changes arguments.
namespace renderprobe {
using Id=uint64_t;
template<class T>Id id(T h){return (Id)h;}
struct Image {uint32_t w=0,h=0,format=0,usage=0,samples=1;};
struct View {Id image=0;uint32_t mip=0,aspect=0;};
struct Frame {uint32_t w=0,h=0;std::vector<Id> views;};
struct Pipeline {bool depth=false,write=false,blend=false,vertex=false,dynamicViewport=false,dynamicScissor=false;VkViewport vp{};VkRect2D sc{};};
struct Draw {Id frame=0,pipeline=0;VkViewport vp{};VkRect2D sc{};bool viewport=false,scissor=false,indexed=false,indirect=false;uint64_t elements=0;std::vector<Id> inputs;};
struct Event {int type=0;Id object=0;VkRect2D area{};Draw draw;}; // draw, begin pass, end pass, execute secondary
struct Command {uint64_t generation=0;Id frame=0,pipeline=0;VkRect2D area{};VkViewport vp{};VkRect2D sc{};bool viewport=false,scissor=false;std::array<Id,8> sets{};std::vector<Event> events;};
struct Raster {bool known=false,depth=false,write=false,blend=false,vertex=false,indexed=false,indirect=false,viewport=false,scissor=false;VkViewport vp{};VkRect2D sc{};uint64_t calls=0,elements=0;};
struct Pass {Id frame=0;VkRect2D area{};uint64_t calls=0;std::vector<Raster> raster;std::vector<Id> inputs;};
struct Snapshot {std::vector<Pass> passes;std::vector<Id> presented;unsigned submits=0,buffers=0,missing=0,dropped=0;};
std::mutex mutex;
std::unordered_map<Id,Image> images;
std::unordered_map<Id,View> views;
std::unordered_map<Id,Frame> frames;
std::unordered_map<Id,Pipeline> pipelines;
std::unordered_map<Id,std::map<uint64_t,Id>> descriptors;
std::unordered_map<Id,Command> commands;
std::unordered_map<Id,Image> swaps;
std::unordered_map<Id,std::vector<Id>> swapImages;
std::vector<Snapshot> snapshots;
Snapshot current;
std::atomic<bool> recording{false};
uint64_t generation=0,requestToken=0,completedToken=0;
bool armed=false;int remaining=0,warmup=0;
std::string result="{}";
unsigned hooks=0;
constexpr size_t maxObjects=32768,maxPasses=160,maxEvents=24000;
template<class T>void put(std::unordered_map<Id,T>& map,Id key,const T& value){if(map.size()<maxObjects||map.count(key))map[key]=value;}
void unique(std::vector<Id>& list,Id v,size_t limit=24){if(v&&list.size()<limit&&std::find(list.begin(),list.end(),v)==list.end())list.push_back(v);}
void image(VkImage h,const VkImageCreateInfo& info){std::lock_guard<std::mutex> lock(mutex);put(images,id(h),Image{info.extent.width,info.extent.height,uint32_t(info.format),info.usage,uint32_t(info.samples)});}
void view(VkImageView h,const VkImageViewCreateInfo& info){std::lock_guard<std::mutex> lock(mutex);put(views,id(h),View{id(info.image),info.subresourceRange.baseMipLevel,info.subresourceRange.aspectMask});}
void frame(VkFramebuffer h,const VkFramebufferCreateInfo& info){std::lock_guard<std::mutex> lock(mutex);Frame f{info.width,info.height,{}};if(info.pAttachments&&info.attachmentCount<=16)for(uint32_t i=0;i<info.attachmentCount;i++)f.views.push_back(id(info.pAttachments[i]));put(frames,id(h),f);}
void swap(VkSwapchainKHR h,const VkSwapchainCreateInfoKHR& info){std::lock_guard<std::mutex> lock(mutex);put(swaps,id(h),Image{info.imageExtent.width,info.imageExtent.height,uint32_t(info.imageFormat),info.imageUsage,1});}
void swapImageList(VkSwapchainKHR h,uint32_t count,const VkImage* list){
    std::lock_guard<std::mutex> lock(mutex);if(!list||count>32)return;auto s=swaps.find(id(h));if(s==swaps.end())return;
    std::vector<Id> ids;for(uint32_t i=0;i<count;i++){ids.push_back(id(list[i]));put(images,id(list[i]),s->second);}put(swapImages,id(h),ids);
}
template<class T>void destroyImage(T h){std::lock_guard<std::mutex> lock(mutex);if(!recording)images.erase(id(h));}
template<class T>void destroyView(T h){std::lock_guard<std::mutex> lock(mutex);if(!recording)views.erase(id(h));}
template<class T>void destroyFrame(T h){std::lock_guard<std::mutex> lock(mutex);if(!recording)frames.erase(id(h));}
void beginPass(VkCommandBuffer h,const VkRenderPassBeginInfo& info){if(!recording)return;std::lock_guard<std::mutex> lock(mutex);auto& c=commands[id(h)];if(c.generation!=generation){c={};c.generation=generation;}c.frame=id(info.framebuffer);c.area=info.renderArea;if(c.events.size()<maxEvents){Event e;e.type=1;e.object=c.frame;e.area=c.area;c.events.push_back(e);}else current.dropped++;}
void endPass(VkCommandBuffer h){if(!recording)return;std::lock_guard<std::mutex> lock(mutex);auto& c=commands[id(h)];if(c.generation==generation&&c.events.size()<maxEvents){Event e;e.type=2;c.events.push_back(e);}c.frame=0;}
void viewport(VkCommandBuffer h,uint32_t first,uint32_t count,const VkViewport* values){if(!recording||first||!count||!values)return;std::lock_guard<std::mutex> lock(mutex);auto& c=commands[id(h)];c.vp=values[0];c.viewport=true;}
void scissor(VkCommandBuffer h,uint32_t first,uint32_t count,const VkRect2D* values){if(!recording||first||!count||!values)return;std::lock_guard<std::mutex> lock(mutex);auto& c=commands[id(h)];c.sc=values[0];c.scissor=true;}
void draw(VkCommandBuffer h,uint64_t elements,bool indexed,bool indirect){
    if(!recording)return;std::lock_guard<std::mutex> lock(mutex);auto& c=commands[id(h)];if(c.generation!=generation||c.events.size()>=maxEvents){current.dropped++;return;}
    Draw d;d.frame=c.frame;d.pipeline=c.pipeline;d.vp=c.vp;d.sc=c.sc;d.viewport=c.viewport;d.scissor=c.scissor;d.indexed=indexed;d.indirect=indirect;d.elements=elements;
    for(Id set:c.sets){auto it=descriptors.find(set);if(it==descriptors.end())continue;for(const auto& v:it->second){auto viewIt=views.find(v.second);if(viewIt==views.end())continue;auto imageIt=images.find(viewIt->second.image);if(imageIt==images.end()||imageIt->second.w<320||imageIt->second.h<180)continue;unique(d.inputs,v.second);}}
    Event e;e.draw=std::move(d);c.events.push_back(std::move(e));
}
bool sameRaster(const Raster& a,const Raster& b){return a.known==b.known&&a.depth==b.depth&&a.write==b.write&&a.blend==b.blend&&a.vertex==b.vertex&&a.indexed==b.indexed&&a.indirect==b.indirect&&a.viewport==b.viewport&&a.scissor==b.scissor&&std::memcmp(&a.vp,&b.vp,sizeof(a.vp))==0&&std::memcmp(&a.sc,&b.sc,sizeof(a.sc))==0;}
void fold(Id handle,int& active,unsigned depth=0){
    if(depth>8){current.dropped++;return;}auto it=commands.find(handle);
    if(it==commands.end()||it->second.generation!=generation){current.missing++;return;}current.buffers++;
    for(const auto& e:it->second.events){
        if(e.type==1){if(current.passes.size()>=maxPasses){current.dropped++;active=-1;continue;}current.passes.push_back(Pass{e.object,e.area,0,{},{}});active=int(current.passes.size()-1);}
        else if(e.type==2)active=-1;
        else if(e.type==3){int inherited=active;fold(e.object,inherited,depth+1);}
        else{
            const auto& d=e.draw;
            if(active<0){if(current.passes.size()>=maxPasses){current.dropped++;continue;}current.passes.push_back(Pass{d.frame,{},0,{},{}});active=int(current.passes.size()-1);}
            auto& pass=current.passes[active];pass.calls++;
            Raster r;r.indexed=d.indexed;r.indirect=d.indirect;r.viewport=d.viewport;r.scissor=d.scissor;r.vp=d.vp;r.sc=d.sc;
            auto pi=pipelines.find(d.pipeline);if(pi!=pipelines.end()){const auto& p=pi->second;r.known=true;r.depth=p.depth;r.write=p.write;r.blend=p.blend;r.vertex=p.vertex;if(!p.dynamicViewport){r.vp=p.vp;r.viewport=true;}if(!p.dynamicScissor){r.sc=p.sc;r.scissor=true;}}
            bool merged=false;for(auto& old:pass.raster)if(sameRaster(old,r)){old.calls++;old.elements+=d.elements;merged=true;break;}
            if(!merged){r.calls=1;r.elements=d.elements;if(pass.raster.size()<20)pass.raster.push_back(r);else current.dropped++;}
            for(Id image:d.inputs)unique(pass.inputs,image,32);
        }
    }
}
void jsonImage(std::ostringstream& s,Id h,uint32_t mip=0){s<<"{\"id\":\""<<h<<"\",\"mip\":"<<mip;auto i=images.find(h);if(i!=images.end())s<<",\"width\":"<<std::max(1u,i->second.w>>std::min(mip,31u))<<",\"height\":"<<std::max(1u,i->second.h>>std::min(mip,31u))<<",\"format\":"<<i->second.format<<",\"usage\":"<<i->second.usage<<",\"samples\":"<<i->second.samples;s<<'}';}
std::string serialize(){
    std::ostringstream s;s<<"{\"schema\":1,\"request\":"<<requestToken<<",\"hookMask\":"<<hooks<<",\"frames\":[";bool comma=false;
    for(const auto& f:snapshots){if(comma)s<<',';comma=true;s<<"{\"submits\":"<<f.submits<<",\"commandBuffers\":"<<f.buffers<<",\"missingCommandBuffers\":"<<f.missing<<",\"dropped\":"<<f.dropped<<",\"presented\":[";bool c=false;for(Id i:f.presented){if(c)s<<',';c=true;jsonImage(s,i);}s<<"],\"passes\":[";c=false;
        for(const auto& p:f.passes){if(c)s<<',';c=true;s<<"{\"framebuffer\":\""<<p.frame<<"\",\"draws\":"<<p.calls<<",\"area\":["<<p.area.offset.x<<','<<p.area.offset.y<<','<<p.area.extent.width<<','<<p.area.extent.height<<"],\"attachments\":[";bool ac=false;auto fi=frames.find(p.frame);if(fi!=frames.end())for(Id vi:fi->second.views){auto v=views.find(vi);if(v==views.end())continue;if(ac)s<<',';ac=true;jsonImage(s,v->second.image,v->second.mip);}s<<"],\"raster\":[";ac=false;
            for(const auto& r:p.raster){if(ac)s<<',';ac=true;s<<"{\"calls\":"<<r.calls<<",\"elements\":"<<r.elements<<",\"pipelineKnown\":"<<r.known<<",\"depthTest\":"<<r.depth<<",\"depthWrite\":"<<r.write<<",\"blend\":"<<r.blend<<",\"vertexInput\":"<<r.vertex<<",\"indexed\":"<<r.indexed<<",\"indirect\":"<<r.indirect<<",\"viewportKnown\":"<<r.viewport<<",\"viewport\":["<<r.vp.x<<','<<r.vp.y<<','<<r.vp.width<<','<<r.vp.height<<"],\"scissorKnown\":"<<r.scissor<<",\"scissor\":["<<r.sc.offset.x<<','<<r.sc.offset.y<<','<<r.sc.extent.width<<','<<r.sc.extent.height<<"]}";}
            s<<"],\"sampledInputs\":[";ac=false;for(Id im:p.inputs){if(ac)s<<',';ac=true;auto v=views.find(im);if(v!=views.end())jsonImage(s,v->second.image,v->second.mip);else jsonImage(s,0);}s<<"]}";
        }s<<"]}";
    }s<<"]}";return s.str();
}
void request(uint64_t token){std::lock_guard<std::mutex> lock(mutex);if(!token||token==requestToken)return;requestToken=token;armed=true;remaining=3;warmup=2;recording=false;snapshots.clear();current={};result="{}";}
std::string report(){std::lock_guard<std::mutex> lock(mutex);return result;}

PFN_vkCreateGraphicsPipelines realCreatePipelines=nullptr;
PFN_vkDestroyPipeline realDestroyPipeline=nullptr;
PFN_vkUpdateDescriptorSets realUpdateDescriptors=nullptr;
PFN_vkFreeDescriptorSets realFreeDescriptors=nullptr;
PFN_vkBeginCommandBuffer realBeginCommand=nullptr;
PFN_vkCmdBindPipeline realBindPipeline=nullptr;
PFN_vkCmdBindDescriptorSets realBindDescriptors=nullptr;
PFN_vkCmdDraw realDraw=nullptr;
PFN_vkCmdDrawIndexed realDrawIndexed=nullptr;
PFN_vkCmdDrawIndirect realDrawIndirect=nullptr;
PFN_vkCmdDrawIndexedIndirect realDrawIndexedIndirect=nullptr;
PFN_vkCmdExecuteCommands realExecute=nullptr;
PFN_vkQueueSubmit realSubmit=nullptr;
PFN_vkQueuePresentKHR realPresent=nullptr;
VkResult createPipelines(VkDevice device,VkPipelineCache cache,uint32_t n,const VkGraphicsPipelineCreateInfo* info,const VkAllocationCallbacks* a,VkPipeline* out){
    VkResult r=realCreatePipelines(device,cache,n,info,a,out);if(!info||!out||n>4096)return r;std::lock_guard<std::mutex> lock(mutex);
    for(uint32_t i=0;i<n;i++)if(out[i]){const auto& p=info[i];Pipeline v;if(p.pDepthStencilState){v.depth=p.pDepthStencilState->depthTestEnable;v.write=p.pDepthStencilState->depthWriteEnable;}if(p.pVertexInputState)v.vertex=p.pVertexInputState->vertexBindingDescriptionCount>0;
        if(p.pColorBlendState&&p.pColorBlendState->pAttachments&&p.pColorBlendState->attachmentCount)for(uint32_t j=0;j<p.pColorBlendState->attachmentCount;j++)v.blend|=p.pColorBlendState->pAttachments[j].blendEnable;
        if(p.pDynamicState)for(uint32_t j=0;j<p.pDynamicState->dynamicStateCount;j++){auto d=p.pDynamicState->pDynamicStates[j];v.dynamicViewport|=d==VK_DYNAMIC_STATE_VIEWPORT||d==VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;v.dynamicScissor|=d==VK_DYNAMIC_STATE_SCISSOR||d==VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT;}
        if(p.pViewportState){if(!v.dynamicViewport&&p.pViewportState->viewportCount&&p.pViewportState->pViewports)v.vp=p.pViewportState->pViewports[0];if(!v.dynamicScissor&&p.pViewportState->scissorCount&&p.pViewportState->pScissors)v.sc=p.pViewportState->pScissors[0];}put(pipelines,id(out[i]),v);
    }return r;
}
void destroyPipeline(VkDevice d,VkPipeline p,const VkAllocationCallbacks* a){realDestroyPipeline(d,p,a);std::lock_guard<std::mutex> lock(mutex);if(!recording)pipelines.erase(id(p));}
bool sampled(VkDescriptorType t){return t==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER||t==VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE||t==VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;}
void updateDescriptors(VkDevice d,uint32_t n,const VkWriteDescriptorSet* writes,uint32_t copies,const VkCopyDescriptorSet* copy){
    realUpdateDescriptors(d,n,writes,copies,copy);std::lock_guard<std::mutex> lock(mutex);
    if(writes&&n<65536)for(uint32_t i=0;i<n;i++){const auto& w=writes[i];if(!sampled(w.descriptorType)||!w.pImageInfo||w.descriptorCount>4096)continue;if(descriptors.size()>=maxObjects&&!descriptors.count(id(w.dstSet)))continue;auto& set=descriptors[id(w.dstSet)];
        for(uint32_t j=0;j<w.descriptorCount&&set.size()<256;j++)set[(uint64_t(w.dstBinding)<<32)|(w.dstArrayElement+j)]=id(w.pImageInfo[j].imageView);}
    if(copy&&copies<4096)for(uint32_t i=0;i<copies;i++){const auto& c=copy[i];auto src=descriptors.find(id(c.srcSet));if(src==descriptors.end()||c.descriptorCount>256)continue;if(descriptors.size()>=maxObjects&&!descriptors.count(id(c.dstSet)))continue;
        for(uint32_t j=0;j<c.descriptorCount;j++){auto it=src->second.find((uint64_t(c.srcBinding)<<32)|(c.srcArrayElement+j));if(it!=src->second.end())descriptors[id(c.dstSet)][(uint64_t(c.dstBinding)<<32)|(c.dstArrayElement+j)]=it->second;}}
}
VkResult freeDescriptors(VkDevice d,VkDescriptorPool p,uint32_t n,const VkDescriptorSet* sets){VkResult r=realFreeDescriptors(d,p,n,sets);if(r==VK_SUCCESS&&sets){std::lock_guard<std::mutex> lock(mutex);for(uint32_t i=0;i<n;i++)descriptors.erase(id(sets[i]));}return r;}
VkResult beginCommand(VkCommandBuffer h,const VkCommandBufferBeginInfo* info){VkResult r=realBeginCommand(h,info);if(recording&&r==VK_SUCCESS){std::lock_guard<std::mutex> lock(mutex);if(commands.size()<8192||commands.count(id(h))){Command c;c.generation=generation;if(info&&info->pInheritanceInfo)c.frame=id(info->pInheritanceInfo->framebuffer);commands[id(h)]=std::move(c);}else current.dropped++;}return r;}
void bindPipeline(VkCommandBuffer h,VkPipelineBindPoint point,VkPipeline p){realBindPipeline(h,point,p);if(recording&&point==VK_PIPELINE_BIND_POINT_GRAPHICS){std::lock_guard<std::mutex> lock(mutex);commands[id(h)].pipeline=id(p);}}
void bindDescriptors(VkCommandBuffer h,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t n,const VkDescriptorSet* sets,uint32_t dynamicCount,const uint32_t* offsets){realBindDescriptors(h,point,layout,first,n,sets,dynamicCount,offsets);if(recording&&point==VK_PIPELINE_BIND_POINT_GRAPHICS&&sets){std::lock_guard<std::mutex> lock(mutex);auto& c=commands[id(h)];for(uint32_t i=0;i<n&&first+i<c.sets.size();i++)c.sets[first+i]=id(sets[i]);}}
void cmdDraw(VkCommandBuffer h,uint32_t vertices,uint32_t instances,uint32_t firstVertex,uint32_t firstInstance){realDraw(h,vertices,instances,firstVertex,firstInstance);draw(h,uint64_t(vertices)*instances,false,false);}
void cmdDrawIndexed(VkCommandBuffer h,uint32_t indices,uint32_t instances,uint32_t first,int32_t vertexOffset,uint32_t firstInstance){realDrawIndexed(h,indices,instances,first,vertexOffset,firstInstance);draw(h,uint64_t(indices)*instances,true,false);}
void cmdDrawIndirect(VkCommandBuffer h,VkBuffer b,VkDeviceSize offset,uint32_t count,uint32_t stride){realDrawIndirect(h,b,offset,count,stride);draw(h,count,false,true);}
void cmdDrawIndexedIndirect(VkCommandBuffer h,VkBuffer b,VkDeviceSize offset,uint32_t count,uint32_t stride){realDrawIndexedIndirect(h,b,offset,count,stride);draw(h,count,true,true);}
void execute(VkCommandBuffer h,uint32_t n,const VkCommandBuffer* children){realExecute(h,n,children);if(recording&&children&&n<=4096){std::lock_guard<std::mutex> lock(mutex);auto& c=commands[id(h)];for(uint32_t i=0;i<n&&c.events.size()<maxEvents;i++){Event e;e.type=3;e.object=id(children[i]);c.events.push_back(e);}}}
VkResult submit(VkQueue queue,uint32_t n,const VkSubmitInfo* info,VkFence fence){VkResult r=realSubmit(queue,n,info,fence);if(recording&&r==VK_SUCCESS&&info&&n<4096){std::lock_guard<std::mutex> lock(mutex);current.submits++;for(uint32_t i=0;i<n;i++)for(uint32_t j=0;j<info[i].commandBufferCount;j++){int active=-1;fold(id(info[i].pCommandBuffers[j]),active);}}return r;}
VkResult present(VkQueue queue,const VkPresentInfoKHR* info){
    VkResult r=realPresent(queue,info);std::lock_guard<std::mutex> lock(mutex);
    if(r!=VK_SUCCESS&&r!=VK_SUBOPTIMAL_KHR)return r;
    if(armed){armed=false;generation++;commands.clear();current={};recording=true;return r;}
    if(!recording)return r;
    if(info&&info->swapchainCount<=8)for(uint32_t i=0;i<info->swapchainCount;i++){auto it=swapImages.find(id(info->pSwapchains[i]));if(it!=swapImages.end()&&info->pImageIndices[i]<it->second.size())unique(current.presented,it->second[info->pImageIndices[i]]);}
    if(warmup)warmup--;else{snapshots.push_back(std::move(current));remaining--;}
    current={};
    if(!remaining){recording=false;completedToken=requestToken;result=serialize();if(result.size()>700000)result="{\"error\":\"capture exceeded Binder limit\"}";}
    return r;
}
PFN_vkVoidFunction wrap(const char* name,PFN_vkVoidFunction real){
    std::lock_guard<std::mutex> lock(mutex);
#define PROBE(N,F,S,B) if(std::strcmp(name,N)==0){if(real!=(PFN_vkVoidFunction)F){if(S&&(PFN_vkVoidFunction)S!=real)return real;S=(decltype(S))real;hooks|=B;}return (PFN_vkVoidFunction)F;}
    PROBE("vkCreateGraphicsPipelines",createPipelines,realCreatePipelines,1)
    PROBE("vkDestroyPipeline",destroyPipeline,realDestroyPipeline,2)
    PROBE("vkUpdateDescriptorSets",updateDescriptors,realUpdateDescriptors,4)
    PROBE("vkFreeDescriptorSets",freeDescriptors,realFreeDescriptors,8)
    PROBE("vkBeginCommandBuffer",beginCommand,realBeginCommand,16)
    PROBE("vkCmdBindPipeline",bindPipeline,realBindPipeline,32)
    PROBE("vkCmdBindDescriptorSets",bindDescriptors,realBindDescriptors,64)
    PROBE("vkCmdDraw",cmdDraw,realDraw,128)
    PROBE("vkCmdDrawIndexed",cmdDrawIndexed,realDrawIndexed,256)
    PROBE("vkCmdDrawIndirect",cmdDrawIndirect,realDrawIndirect,512)
    PROBE("vkCmdDrawIndexedIndirect",cmdDrawIndexedIndirect,realDrawIndexedIndirect,1024)
    PROBE("vkCmdExecuteCommands",execute,realExecute,2048)
    PROBE("vkQueueSubmit",submit,realSubmit,4096)
    PROBE("vkQueuePresentKHR",present,realPresent,8192)
#undef PROBE
    return real;
}
} // namespace renderprobe
