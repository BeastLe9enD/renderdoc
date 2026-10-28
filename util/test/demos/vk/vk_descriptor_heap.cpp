/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "vk_test.h"

RD_TEST(VK_Descriptor_Heap, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Test of EXT_descriptor_heap based bindings with set/binding mappings and push data.";

  VkPhysicalDeviceDescriptorHeapFeaturesEXT heapFeatures = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT,
  };
  VkPhysicalDeviceDescriptorHeapPropertiesEXT heapProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT,
  };

  std::string pixel = R"EOSHADER(
#version 460 core

layout(location = 0) in v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
} vertIn;

layout(location = 0, index = 0) out vec4 Color;

layout(set = 0, binding = 0, std140) uniform ubo_block
{
  vec4 col;
} ubo;

layout(set = 0, binding = 1) uniform texture2D tex;
layout(set = 0, binding = 2) uniform sampler samp;

layout(set = 0, binding = 3, std140) uniform push_block
{
  vec4 tint;
} push;

void main()
{
  Color = ubo.col * textureLod(sampler2D(tex, samp), vertIn.uv.xy, 0.0) + push.tint;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
    devExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

    vmaBDA = true;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(devVersion < VK_MAKE_VERSION(1, 4, 0))
    {
      Avail = "Vulkan 1.4 is required";
      return;
    }

    getPhysFeatures2(&heapFeatures);
    getPhysProperties2(&heapProps);

    if(!heapFeatures.descriptorHeap)
      Avail = "Feature 'descriptorHeap' not available";

    heapFeatures.descriptorHeapCaptureReplay = VK_FALSE;
    heapFeatures.pNext = (void *)devInfoNext;
    devInfoNext = &heapFeatures;

    static VkPhysicalDeviceVulkan12Features vk12Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };

    getPhysFeatures2(&vk12Features);

    if(!vk12Features.bufferDeviceAddress)
      Avail = "Feature 'bufferDeviceAddress' not available";

    vk12Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    vk12Features.bufferDeviceAddress = VK_TRUE;
    vk12Features.pNext = (void *)devInfoNext;
    devInfoNext = &vk12Features;

    static VkPhysicalDeviceVulkan14Features vk14Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
    };

    getPhysFeatures2(&vk14Features);

    if(!vk14Features.maintenance5)
      Avail = "Feature 'maintenance5' not available";

    vk14Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
    vk14Features.maintenance5 = VK_TRUE;
    vk14Features.pNext = (void *)devInfoNext;
    devInfoNext = &vk14Features;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    const VkDeviceSize descAlign =
        std::max({heapProps.bufferDescriptorAlignment, heapProps.imageDescriptorAlignment,
                  heapProps.samplerDescriptorAlignment, (VkDeviceSize)16});
    const VkDeviceSize descStride = AlignUp(
        std::max({heapProps.bufferDescriptorSize, heapProps.imageDescriptorSize}), descAlign);
    const VkDeviceSize samplerStride = AlignUp(heapProps.samplerDescriptorSize, descAlign);

    // resource heap layout: two sets of [UBO, texture] descriptors, then the reserved range
    const uint32_t uboSlot = 0, texSlot = 1, slotsPerSet = 2, numSets = 2;
    const VkDeviceSize resourceUserSize =
        AlignUp(descStride * slotsPerSet * numSets, heapProps.resourceHeapAlignment);
    const VkDeviceSize resourceHeapSize = resourceUserSize + heapProps.minResourceHeapReservedRange;

    // sampler heap layout: two samplers then the reserved range
    const VkDeviceSize samplerUserSize =
        AlignUp(samplerStride * numSets, heapProps.samplerHeapAlignment);
    const VkDeviceSize samplerHeapSize = samplerUserSize + heapProps.minSamplerHeapReservedRange;

    const VkBufferUsageFlags heapUsage =
        VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    AllocatedBuffer resourceHeap(this, vkh::BufferCreateInfo(resourceHeapSize, heapUsage),
                                 VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}),
                                 (uint32_t)heapProps.resourceHeapAlignment);
    AllocatedBuffer samplerHeap(this, vkh::BufferCreateInfo(samplerHeapSize, heapUsage),
                                VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}),
                                (uint32_t)heapProps.samplerHeapAlignment);

    // two UBOs with different colours
    Vec4f uboData[2][64] = {};
    uboData[0][0] = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    uboData[1][0] = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);

    AllocatedBuffer ubo(
        this,
        vkh::BufferCreateInfo(sizeof(uboData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}), 256);
    ubo.upload(uboData);

    // two textures, cleared to different colours each frame
    AllocatedImage tex[2] = {
        AllocatedImage(
            this,
            vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY})),
        AllocatedImage(
            this,
            vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY})),
    };

    // write the descriptors directly into the heaps
    {
      byte *resPtr = resourceHeap.map();
      byte *sampPtr = samplerHeap.map();

      for(uint32_t s = 0; s < numSets; s++)
      {
        VkDeviceAddressRangeEXT uboRange = {ubo.address + s * sizeof(uboData[0]), sizeof(uboData[0])};

        VkImageViewCreateInfo viewInfo =
            vkh::ImageViewCreateInfo(tex[s].image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM);

        VkImageDescriptorInfoEXT imgInfo = {VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT};
        imgInfo.pView = &viewInfo;
        imgInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkResourceDescriptorInfoEXT resInfos[2] = {};
        resInfos[0].sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
        resInfos[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        resInfos[0].data.pAddressRange = &uboRange;
        resInfos[1].sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
        resInfos[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        resInfos[1].data.pImage = &imgInfo;

        VkHostAddressRangeEXT resDests[2] = {
            {resPtr + (s * slotsPerSet + uboSlot) * descStride,
             (size_t)heapProps.bufferDescriptorSize},
            {resPtr + (s * slotsPerSet + texSlot) * descStride, (size_t)heapProps.imageDescriptorSize},
        };

        CHECK_VKR(vkWriteResourceDescriptorsEXT(device, 2, resInfos, resDests));

        VkSamplerCreateInfo sampInfo =
            vkh::SamplerCreateInfo(s == 0 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
                                   s == 0 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);

        VkHostAddressRangeEXT sampDest = {sampPtr + s * samplerStride,
                                          (size_t)heapProps.samplerDescriptorSize};

        CHECK_VKR(vkWriteSamplerDescriptorsEXT(device, 1, &sampInfo, &sampDest));
      }

      resourceHeap.unmap();
      samplerHeap.unmap();
    }

    // map the shader's set/binding pairs into the heaps. The UBO and texture are indexed by a value
    // in push data so we can switch between descriptor 'sets' per draw, and the tint is read
    // directly from push data
    VkDescriptorSetAndBindingMappingEXT mappings[4] = {};
    for(VkDescriptorSetAndBindingMappingEXT &m : mappings)
    {
      m.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
      m.descriptorSet = 0;
      m.bindingCount = 1;
      m.resourceMask = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    }

    // push data layout: [0] uint index, [16] vec4 tint
    mappings[0].firstBinding = 0;
    mappings[0].source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;
    mappings[0].sourceData.pushIndex.heapOffset = uint32_t(uboSlot * descStride);
    mappings[0].sourceData.pushIndex.pushOffset = 0;
    mappings[0].sourceData.pushIndex.heapIndexStride = uint32_t(descStride * slotsPerSet);

    mappings[1].firstBinding = 1;
    mappings[1].source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;
    mappings[1].sourceData.pushIndex.heapOffset = uint32_t(texSlot * descStride);
    mappings[1].sourceData.pushIndex.pushOffset = 0;
    mappings[1].sourceData.pushIndex.heapIndexStride = uint32_t(descStride * slotsPerSet);

    mappings[2].firstBinding = 2;
    mappings[2].source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;
    mappings[2].sourceData.pushIndex.samplerHeapOffset = 0;
    mappings[2].sourceData.pushIndex.samplerPushOffset = 0;
    mappings[2].sourceData.pushIndex.samplerHeapIndexStride = uint32_t(samplerStride);

    mappings[3].firstBinding = 3;
    mappings[3].source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT;
    mappings[3].sourceData.pushDataOffset = 16;

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = {
        VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT,
    };
    mappingInfo.mappingCount = ARRAY_COUNT(mappings);
    mappingInfo.pMappings = mappings;

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = VK_NULL_HANDLE;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.stages[1].pNext = &mappingInfo;

    VkPipelineCreateFlags2CreateInfo flags2 = {VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO};
    flags2.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;
    pipeCreateInfo.pNext = &flags2;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    VkBindHeapInfoEXT resourceBind = {VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    resourceBind.heapRange = {resourceHeap.address, resourceHeapSize};
    resourceBind.reservedRangeOffset = resourceUserSize;
    resourceBind.reservedRangeSize = heapProps.minResourceHeapReservedRange;

    VkBindHeapInfoEXT samplerBind = {VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    samplerBind.heapRange = {samplerHeap.address, samplerHeapSize};
    samplerBind.reservedRangeOffset = samplerUserSize;
    samplerBind.reservedRangeSize = heapProps.minSamplerHeapReservedRange;

    struct PushData
    {
      uint32_t index;
      uint32_t pad[3];
      Vec4f tint;
    };

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      for(uint32_t s = 0; s < numSets; s++)
      {
        vkh::cmdPipelineBarrier(
            cmd,
            {
                vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tex[s].image),
            });

        vkCmdClearColorImage(cmd, tex[s].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             s == 0 ? vkh::ClearColorValue(0.0f, 1.0f, 0.0f, 1.0f)
                                    : vkh::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f),
                             1, vkh::ImageSubresourceRange());

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tex[s].image),
                 });
      }

      vkCmdBindResourceHeapEXT(cmd, &resourceBind);
      vkCmdBindSamplerHeapEXT(cmd, &samplerBind);

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      VkViewport v = mainWindow->viewport;
      v.width /= 2.0f;

      // first draw: green texture * white UBO, no tint
      {
        setMarker(cmd, "Set 0");

        PushData pushData = {};
        pushData.index = 0;
        pushData.tint = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);

        VkPushDataInfoEXT pushInfo = {VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT};
        pushInfo.offset = 0;
        pushInfo.data = {&pushData, sizeof(pushData)};
        vkCmdPushDataEXT(cmd, &pushInfo);

        vkCmdSetViewport(cmd, 0, 1, &v);
        vkCmdDraw(cmd, 3, 1, 0, 0);
      }

      // second draw: white texture * blue UBO, red tint
      {
        setMarker(cmd, "Set 1");

        PushData pushData = {};
        pushData.index = 1;
        pushData.tint = Vec4f(1.0f, 0.0f, 0.0f, 0.0f);

        VkPushDataInfoEXT pushInfo = {VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT};
        pushInfo.offset = 0;
        pushInfo.data = {&pushData, sizeof(pushData)};
        vkCmdPushDataEXT(cmd, &pushInfo);

        v.x += v.width;
        vkCmdSetViewport(cmd, 0, 1, &v);
        vkCmdDraw(cmd, 3, 1, 0, 0);
      }

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
