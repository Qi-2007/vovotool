#include <cassert>
#include <cstdio>
#include "render_probe.hpp"
using namespace renderprobe;
VkResult fakePresent(VkQueue,const VkPresentInfoKHR*){return VK_SUCCESS;}
VkResult fakeSubmit(VkQueue,uint32_t,const VkSubmitInfo*,VkFence){return VK_SUCCESS;}
VkResult fakeBegin(VkCommandBuffer,const VkCommandBufferBeginInfo*){return VK_SUCCESS;}
void resource(Id im,Id vi,Id fb,uint32_t w,uint32_t h){
    VkImageCreateInfo info{};info.extent={w,h,1};info.format=VK_FORMAT_B10G11R11_UFLOAT_PACK32;info.samples=VK_SAMPLE_COUNT_1_BIT;
    image((VkImage)im,info);VkImageViewCreateInfo v{};v.image=(VkImage)im;view((VkImageView)vi,v);
    VkImageView viewId=(VkImageView)vi;VkFramebufferCreateInfo f{};f.width=w;f.height=h;f.attachmentCount=1;f.pAttachments=&viewId;frame((VkFramebuffer)fb,f);
}
void pass(VkCommandBuffer cmd,Id fb){VkRenderPassBeginInfo p{};p.framebuffer=(VkFramebuffer)fb;p.renderArea.extent={2240,1400};beginPass(cmd,p);}
int main(){
    realPresent=fakePresent;realSubmit=fakeSubmit;realBeginCommand=fakeBegin;
    resource(1,11,111,1476,922);resource(2,22,222,2240,1400);resource(3,33,333,2560,1600);
    Pipeline geometry;geometry.depth=true;geometry.write=true;geometry.vertex=true;geometry.dynamicViewport=true;geometry.dynamicScissor=true;pipelines[8]=geometry;
    Pipeline fullscreen;fullscreen.dynamicViewport=true;fullscreen.dynamicScissor=true;pipelines[9]=fullscreen;
    descriptors[99][0]=11;
    request(1);present(nullptr,nullptr);
    auto primary=(VkCommandBuffer)1000,secondary=(VkCommandBuffer)1001,unsubmitted=(VkCommandBuffer)1002;
    for(int n=0;n<5;n++){
        beginCommand(primary,nullptr);beginCommand(secondary,nullptr);beginCommand(unsubmitted,nullptr);
        auto& c=commands[id(secondary)];c.pipeline=8;
        VkViewport vp{0,0,1476,922,0,1};VkRect2D sc{{0,0},{1476,922}};
        viewport(secondary,0,1,&vp);scissor(secondary,0,1,&sc);draw(secondary,9000,true,false);
        pass(primary,222);Event e;e.type=3;e.object=id(secondary);commands[id(primary)].events.push_back(e);endPass(primary);
        pass(primary,222);auto& p=commands[id(primary)];p.pipeline=9;p.sets[0]=99;
        vp.width=2240;vp.height=1400;sc.extent={2240,1400};viewport(primary,0,1,&vp);scissor(primary,0,1,&sc);draw(primary,3,false,false);endPass(primary);
        pass(unsubmitted,333);draw(unsubmitted,123,false,false);endPass(unsubmitted);
        VkSubmitInfo s{};s.commandBufferCount=1;s.pCommandBuffers=&primary;submit(nullptr,1,&s,{});present(nullptr,nullptr);
    }
    assert(!recording&&snapshots.size()==3);
    for(const auto& f:snapshots){
        assert(f.missing==0&&f.dropped==0&&f.passes.size()==2&&f.buffers==2);
        const auto& geo=f.passes[0];assert(geo.frame==222&&geo.calls==1&&geo.raster[0].depth&&geo.raster[0].vp.width==1476);
        const auto& post=f.passes[1];assert(post.raster[0].vp.width==2240&&!post.raster[0].depth&&post.inputs.size()==1&&post.inputs[0]==11);
    }
    auto json=report();assert(json.find("2560")==std::string::npos&&json.find("1476")!=std::string::npos);
    request(2);present(nullptr,nullptr);VkCommandBuffer missing=(VkCommandBuffer)888;VkSubmitInfo s{};s.commandBufferCount=1;s.pCommandBuffers=&missing;submit(nullptr,1,&s,{});assert(current.missing==1);
    puts("PASS: submitted primary/secondary draws, viewport versus attachment, upscale input chain, unused targets excluded, missing coverage reported");
}
