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

RD_TEST(VK_Untyped_Pointers, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests shaders using untyped pointers from VK_KHR_shader_untyped_pointers, including "
      "untyped variables of each buffer storage class and reinterpreting buffer memory.";

  std::string vertex = R"EOSHADER(
               OpCapability Shader
               OpCapability UntypedPointersKHR
               OpExtension "SPV_KHR_storage_buffer_storage_class"
               OpExtension "SPV_KHR_untyped_pointers"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %Position %Color %UV %vertOut %gl_Position

               OpName %main "main"
               OpName %Position "Position"
               OpName %Color "Color"
               OpName %UV "UV"
               OpName %vertOut "vertOut"
               OpName %SSBOWords "SSBOWords"
               OpMemberName %SSBOWords 0 "words"
               OpName %SSBOFloats "SSBOFloats"
               OpMemberName %SSBOFloats 0 "values"
               OpName %vssbo "vssbo"

               OpDecorate %Position Location 0
               OpDecorate %Color Location 1
               OpDecorate %UV Location 2
               OpDecorate %vertOut Location 0
               OpDecorate %gl_Position BuiltIn Position

               OpDecorate %uint_rtarr ArrayStride 4
               OpDecorate %SSBOWords Block
               OpMemberDecorate %SSBOWords 0 Offset 0

               OpDecorate %v4_rtarr ArrayStride 16
               OpDecorate %SSBOFloats Block
               OpMemberDecorate %SSBOFloats 0 Offset 0

               OpDecorate %vssbo DescriptorSet 0
               OpDecorate %vssbo Binding 1
               OpDecorate %vssbo NonWritable

       %void = OpTypeVoid
     %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
    %v3float = OpTypeVector %float 3
    %v4float = OpTypeVector %float 4
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
     %uint_1 = OpConstant %uint 1
    %float_1 = OpConstant %float 1

 %uint_rtarr = OpTypeRuntimeArray %uint
   %v4_rtarr = OpTypeRuntimeArray %v4float
  %SSBOWords = OpTypeStruct %uint_rtarr
 %SSBOFloats = OpTypeStruct %v4_rtarr

  %uptr_SSBO = OpTypeUntypedPointerKHR StorageBuffer
%ptr_Input_v2float = OpTypePointer Input %v2float
%ptr_Input_v3float = OpTypePointer Input %v3float
%ptr_Input_v4float = OpTypePointer Input %v4float
%ptr_Output_v4float = OpTypePointer Output %v4float

   %Position = OpVariable %ptr_Input_v3float Input
      %Color = OpVariable %ptr_Input_v4float Input
         %UV = OpVariable %ptr_Input_v2float Input
    %vertOut = OpVariable %ptr_Output_v4float Output
%gl_Position = OpVariable %ptr_Output_v4float Output
      %vssbo = OpUntypedVariableKHR %uptr_SSBO StorageBuffer %SSBOWords

       %main = OpFunction %void None %voidfn
      %entry = OpLabel
        %pos = OpLoad %v3float %Position

               ; read the scale reinterpreting the uint words: ((SSBOFloats)vssbo).values[1].x
     %sc_ptr = OpUntypedAccessChainKHR %uptr_SSBO %SSBOFloats %vssbo %uint_0 %uint_1 %uint_0
      %scale = OpLoad %float %sc_ptr

     %scaled = OpVectorTimesScalar %v3float %pos %scale
         %px = OpCompositeExtract %float %scaled 0
         %py = OpCompositeExtract %float %scaled 1
         %pz = OpCompositeExtract %float %scaled 2
       %pos4 = OpCompositeConstruct %v4float %px %py %pz %float_1
               OpStore %gl_Position %pos4

        %col = OpLoad %v4float %Color
               OpStore %vertOut %col

               OpReturn
               OpFunctionEnd
)EOSHADER";

  std::string pixel = R"EOSHADER(
               OpCapability Shader
               OpCapability UntypedPointersKHR
               OpExtension "SPV_KHR_storage_buffer_storage_class"
               OpExtension "SPV_KHR_untyped_pointers"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %Color
               OpExecutionMode %main OriginUpperLeft

               OpName %main "main"
               OpName %Color "Color"
               OpName %UBOData "UBOData"
               OpMemberName %UBOData 0 "tint"
               OpMemberName %UBOData 1 "unused"
               OpName %SSBOWords "SSBOWords"
               OpMemberName %SSBOWords 0 "words"
               OpName %SSBOFloats "SSBOFloats"
               OpMemberName %SSBOFloats 0 "values"
               OpName %OutData "OutData"
               OpMemberName %OutData 0 "firstword"
               OpMemberName %OutData 1 "result"
               OpName %PushData "PushData"
               OpMemberName %PushData 0 "index"
               OpName %SSBOVec "SSBOVec"
               OpMemberName %SSBOVec 0 "col"
               OpName %ubo "ubo"
               OpName %ssbo "ssbo"
               OpName %outbuf "outbuf"
               OpName %push "push"
               OpName %bufarray "bufarray"

               OpDecorate %Color Location 0

               OpDecorate %UBOData Block
               OpMemberDecorate %UBOData 0 Offset 0
               OpMemberDecorate %UBOData 1 Offset 16

               OpDecorate %uint_rtarr ArrayStride 4
               OpDecorate %SSBOWords Block
               OpMemberDecorate %SSBOWords 0 Offset 0

               OpDecorate %v4_rtarr ArrayStride 16
               OpDecorate %SSBOFloats Block
               OpMemberDecorate %SSBOFloats 0 Offset 0

               OpDecorate %OutData Block
               OpMemberDecorate %OutData 0 Offset 0
               OpMemberDecorate %OutData 1 Offset 16

               OpDecorate %PushData Block
               OpMemberDecorate %PushData 0 Offset 0

               OpDecorate %SSBOVec Block
               OpMemberDecorate %SSBOVec 0 Offset 0

               OpDecorate %ubo DescriptorSet 0
               OpDecorate %ubo Binding 0
               OpDecorate %ssbo DescriptorSet 0
               OpDecorate %ssbo Binding 1
               OpDecorate %ssbo NonWritable
               OpDecorate %outbuf DescriptorSet 0
               OpDecorate %outbuf Binding 2
               OpDecorate %bufarray DescriptorSet 0
               OpDecorate %bufarray Binding 3
               OpDecorate %bufarray NonWritable

       %void = OpTypeVoid
     %voidfn = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
     %uint_1 = OpConstant %uint 1
     %uint_4 = OpConstant %uint 4

 %uint_rtarr = OpTypeRuntimeArray %uint
   %v4_rtarr = OpTypeRuntimeArray %v4float
    %UBOData = OpTypeStruct %v4float %v4float
  %SSBOWords = OpTypeStruct %uint_rtarr
 %SSBOFloats = OpTypeStruct %v4_rtarr
    %OutData = OpTypeStruct %uint %v4float
   %PushData = OpTypeStruct %uint
    %SSBOVec = OpTypeStruct %v4float
 %SSBOVecArr = OpTypeArray %SSBOVec %uint_4

%uptr_Uniform = OpTypeUntypedPointerKHR Uniform
  %uptr_SSBO = OpTypeUntypedPointerKHR StorageBuffer
  %uptr_Push = OpTypeUntypedPointerKHR PushConstant
%ptr_Output_v4float = OpTypePointer Output %v4float

        %ubo = OpUntypedVariableKHR %uptr_Uniform Uniform %UBOData
       %ssbo = OpUntypedVariableKHR %uptr_SSBO StorageBuffer %SSBOWords
     %outbuf = OpUntypedVariableKHR %uptr_SSBO StorageBuffer %OutData
       %push = OpUntypedVariableKHR %uptr_Push PushConstant %PushData
   %bufarray = OpUntypedVariableKHR %uptr_SSBO StorageBuffer %SSBOVecArr
      %Color = OpVariable %ptr_Output_v4float Output

       %main = OpFunction %void None %voidfn
      %entry = OpLabel

               ; tint = ubo.tint
   %tint_ptr = OpUntypedAccessChainKHR %uptr_Uniform %UBOData %ubo %uint_0
       %tint = OpLoad %v4float %tint_ptr

               ; reinterpret the uint words as vec4s: scale = ((SSBOFloats)ssbo).values[1]
  %scale_ptr = OpUntypedAccessChainKHR %uptr_SSBO %SSBOFloats %ssbo %uint_0 %uint_1
      %scale = OpLoad %v4float %scale_ptr

               ; index = push.index
    %idx_ptr = OpUntypedAccessChainKHR %uptr_Push %PushData %push %uint_0
        %idx = OpLoad %uint %idx_ptr

               ; add = bufarray[index].col
    %add_ptr = OpUntypedAccessChainKHR %uptr_SSBO %SSBOVecArr %bufarray %idx %uint_0
        %add = OpLoad %v4float %add_ptr

        %mul = OpFMul %v4float %tint %scale
        %res = OpFAdd %v4float %mul %add
               OpStore %Color %res

               ; copy the first word to the output, reading the memory with its declared type
   %word_ptr = OpUntypedAccessChainKHR %uptr_SSBO %SSBOWords %ssbo %uint_0 %uint_0
       %word = OpLoad %uint %word_ptr
  %first_ptr = OpUntypedAccessChainKHR %uptr_SSBO %OutData %outbuf %uint_0
               OpStore %first_ptr %word

               ; write the result out through an untyped pointer
    %out_ptr = OpUntypedAccessChainKHR %uptr_SSBO %OutData %outbuf %uint_1
               OpStore %out_ptr %res

               OpReturn
               OpFunctionEnd
)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);

    // the shaders use the StorageBuffer storage class, which is core in 1.1
    optDevExts.push_back(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME);

    features.fragmentStoresAndAtomics = VK_TRUE;
    features.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceShaderUntypedPointersFeaturesKHR untypedFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR,
    };

    getPhysFeatures2(&untypedFeatures);

    if(!untypedFeatures.shaderUntypedPointers)
    {
      Avail = "Feature 'shaderUntypedPointers' not available";
      return;
    }

    devInfoNext = &untypedFeatures;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {setlayout}, {
                         vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i)),
                     }));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::spvasm, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::spvasm, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    Vec4f uboData[2] = {
        Vec4f(0.5f, 0.25f, 0.75f, 1.0f),
        Vec4f(),
    };

    AllocatedBuffer ubo(this,
                        vkh::BufferCreateInfo(sizeof(uboData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    ubo.upload(uboData);

    // the shaders declare this as an array of uints, but read the second 16 bytes as a vec4 of
    // floats. The first word is copied to the output buffer as-is.
    uint32_t ssboData[8] = {
        0x12345678, 0, 0, 0, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000,
    };

    AllocatedBuffer ssbo(
        this,
        vkh::BufferCreateInfo(sizeof(ssboData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    ssbo.upload(ssboData);

    AllocatedBuffer outbuf(this,
                           vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                           VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    // each element of the descriptor array is bound to a different 256-byte region, which is
    // sufficient for any minStorageBufferOffsetAlignment
    const VkDeviceSize arrayStride = 256;
    std::vector<Vec4f> arrayData(4 * arrayStride / sizeof(Vec4f));
    for(size_t i = 0; i < 4; i++)
      arrayData[i * arrayStride / sizeof(Vec4f)] = Vec4f(0.0f, 0.25f * float(i), 0.0f, 0.0f);

    AllocatedBuffer arraybuf(
        this,
        vkh::BufferCreateInfo(arrayData.size() * sizeof(Vec4f),
                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    arraybuf.upload(arrayData.data(), arrayData.size() * sizeof(Vec4f));

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            {vkh::DescriptorBufferInfo(ubo.buffer)}),
                    vkh::WriteDescriptorSet(descset, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(ssbo.buffer)}),
                    vkh::WriteDescriptorSet(descset, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(outbuf.buffer)}),
                    vkh::WriteDescriptorSet(
                        descset, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        {
                            vkh::DescriptorBufferInfo(arraybuf.buffer, arrayStride * 0, arrayStride),
                            vkh::DescriptorBufferInfo(arraybuf.buffer, arrayStride * 1, arrayStride),
                            vkh::DescriptorBufferInfo(arraybuf.buffer, arrayStride * 2, arrayStride),
                            vkh::DescriptorBufferInfo(arraybuf.buffer, arrayStride * 3, arrayStride),
                        }),
                });

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdFillBuffer(cmd, outbuf.buffer, 0, VK_WHOLE_SIZE, 0);

      vkh::cmdPipelineBarrier(cmd, {},
                              {
                                  vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                                           VK_ACCESS_SHADER_WRITE_BIT, outbuf.buffer),
                              });

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});

      Vec4i push = {2, 0, 0, 0};
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i), &push);

      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      setMarker(cmd, "Draw");
      vkCmdDraw(cmd, 3, 1, 0, 0);

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
