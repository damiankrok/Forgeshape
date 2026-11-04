// The desktop shell's interface layer (`DESKTOP-R1`): one pipeline, one
// single-channel atlas and one growing vertex/index buffer pair per frame in
// flight, drawn last in the main pass. Compiled into the desktop build only;
// the Android build's renderer has none of it (see forgeshape_ui_frame.h).
#if !defined(__ANDROID__)

#include <cstring>

#include "forgeshape_log.h"
#include "forgeshape_renderer.h"

namespace forgeshape {

namespace {

const uint32_t kUiVertSpv[] =
#include "ui.vert.spv.inc"
    ;
const uint32_t kUiFragSpv[] =
#include "ui.frag.spv.inc"
    ;

constexpr const char* kTag = "ForgeShape";

struct UiPush {
    float viewport[2];
};

VkShaderModule makeModule(VkDevice device, const uint32_t* code, size_t bytes) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = bytes;
    info.pCode = code;
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

}  // namespace

bool Renderer::createUiDeviceResources() {
    uiVertShader_ = makeModule(device_, kUiVertSpv, sizeof(kUiVertSpv));
    uiFragShader_ = makeModule(device_, kUiFragSpv, sizeof(kUiFragSpv));
    if (uiVertShader_ == VK_NULL_HANDLE || uiFragShader_ == VK_NULL_HANDLE) {
        FS_LOG_ERROR(kTag, "FORGESHAPE_NATIVE_VIEWPORT_FAIL:ui_shader_modules");
        return false;
    }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &uiSetLayout_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &uiDescriptorPool_) != VK_SUCCESS) {
        return false;
    }
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = uiDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &uiSetLayout_;
    if (vkAllocateDescriptorSets(device_, &allocInfo, &uiSet_) != VK_SUCCESS) {
        return false;
    }

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.size = sizeof(UiPush);
    VkPipelineLayoutCreateInfo pipelineLayout{};
    pipelineLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayout.setLayoutCount = 1;
    pipelineLayout.pSetLayouts = &uiSetLayout_;
    pipelineLayout.pushConstantRangeCount = 1;
    pipelineLayout.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(device_, &pipelineLayout, nullptr, &uiPipelineLayout_) != VK_SUCCESS) {
        return false;
    }

    VkSamplerCreateInfo sampler{};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // NEAREST: every glyph and icon is rasterised at the exact pixel size it is
    // drawn at and placed on whole pixels, so filtering could only blur it.
    sampler.magFilter = VK_FILTER_NEAREST;
    sampler.minFilter = VK_FILTER_NEAREST;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.maxLod = 0.0f;
    if (vkCreateSampler(device_, &sampler, nullptr, &uiSampler_) != VK_SUCCESS) {
        return false;
    }
    uiAtlasReady_ = false;
    uiAtlasRevision_ = 0;
    return true;
}

void Renderer::destroyUiDeviceResources() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    for (int i = 0; i < 2; ++i) {
        if (uiVertexBuffers_[i] != VK_NULL_HANDLE) vkDestroyBuffer(device_, uiVertexBuffers_[i], nullptr);
        if (uiVertexMemory_[i] != VK_NULL_HANDLE) vkFreeMemory(device_, uiVertexMemory_[i], nullptr);
        if (uiIndexBuffers_[i] != VK_NULL_HANDLE) vkDestroyBuffer(device_, uiIndexBuffers_[i], nullptr);
        if (uiIndexMemory_[i] != VK_NULL_HANDLE) vkFreeMemory(device_, uiIndexMemory_[i], nullptr);
        uiVertexBuffers_[i] = VK_NULL_HANDLE;
        uiVertexMemory_[i] = VK_NULL_HANDLE;
        uiIndexBuffers_[i] = VK_NULL_HANDLE;
        uiIndexMemory_[i] = VK_NULL_HANDLE;
        uiVertexCapacity_[i] = 0;
        uiIndexCapacity_[i] = 0;
    }
    if (uiAtlasView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, uiAtlasView_, nullptr);
    if (uiAtlasImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, uiAtlasImage_, nullptr);
    if (uiAtlasMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, uiAtlasMemory_, nullptr);
    uiAtlasView_ = VK_NULL_HANDLE;
    uiAtlasImage_ = VK_NULL_HANDLE;
    uiAtlasMemory_ = VK_NULL_HANDLE;
    uiAtlasWidth_ = 0;
    uiAtlasHeight_ = 0;
    uiAtlasReady_ = false;
    if (uiSampler_ != VK_NULL_HANDLE) vkDestroySampler(device_, uiSampler_, nullptr);
    if (uiPipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, uiPipelineLayout_, nullptr);
    if (uiDescriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, uiDescriptorPool_, nullptr);
    if (uiSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, uiSetLayout_, nullptr);
    if (uiVertShader_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, uiVertShader_, nullptr);
    if (uiFragShader_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, uiFragShader_, nullptr);
    uiSampler_ = VK_NULL_HANDLE;
    uiPipelineLayout_ = VK_NULL_HANDLE;
    uiDescriptorPool_ = VK_NULL_HANDLE;
    uiSet_ = VK_NULL_HANDLE;
    uiSetLayout_ = VK_NULL_HANDLE;
    uiVertShader_ = VK_NULL_HANDLE;
    uiFragShader_ = VK_NULL_HANDLE;
}

bool Renderer::createUiPipeline() {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = uiVertShader_;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = uiFragShader_;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(UiVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[6]{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(UiVertex, x))};
    attributes[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(UiVertex, u))};
    attributes[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT,
                     static_cast<uint32_t>(offsetof(UiVertex, localX))};
    attributes[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT,
                     static_cast<uint32_t>(offsetof(UiVertex, halfWidth))};
    attributes[4] = {4, 0, VK_FORMAT_R32G32B32_SFLOAT,
                     static_cast<uint32_t>(offsetof(UiVertex, radius))};
    attributes[5] = {5, 0, VK_FORMAT_R8G8B8A8_UNORM,
                     static_cast<uint32_t>(offsetof(UiVertex, color))};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 6;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blend;

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depthStencil;
    info.pColorBlendState = &colorBlend;
    info.pDynamicState = &dynamic;
    info.layout = uiPipelineLayout_;
    info.renderPass = renderPass_;
    info.subpass = 0;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &uiPipeline_)
        != VK_SUCCESS) {
        FS_LOG_ERROR(kTag, "FORGESHAPE_NATIVE_VIEWPORT_FAIL:ui_pipeline");
        return false;
    }
    return true;
}

void Renderer::destroyUiPipeline() {
    if (uiPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, uiPipeline_, nullptr);
        uiPipeline_ = VK_NULL_HANDLE;
    }
}

// Uploads the atlas when its revision moved: a whole-image copy through a
// one-time staging buffer, on the upload command buffer the other one-time
// copies use. A glyph cache that grew is a handful of these per session.
bool Renderer::syncUiAtlas() {
    const UiFramePtr frame = uiFrame_;
    if (!frame || !frame->atlas || frame->atlas->pixels.empty()) {
        return true;
    }
    const UiAtlasImage& atlas = *frame->atlas;
    if (uiAtlasReady_ && atlas.revision == uiAtlasRevision_) {
        return true;
    }
    if (atlas.pixels.size() != static_cast<size_t>(atlas.width) * atlas.height) {
        return false;
    }
    if (uiAtlasImage_ == VK_NULL_HANDLE || uiAtlasWidth_ != atlas.width
        || uiAtlasHeight_ != atlas.height) {
        vkDeviceWaitIdle(device_);
        if (uiAtlasView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, uiAtlasView_, nullptr);
        if (uiAtlasImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, uiAtlasImage_, nullptr);
        if (uiAtlasMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, uiAtlasMemory_, nullptr);
        uiAtlasView_ = VK_NULL_HANDLE;
        uiAtlasImage_ = VK_NULL_HANDLE;
        uiAtlasMemory_ = VK_NULL_HANDLE;

        VkImageCreateInfo image{};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D;
        image.format = VK_FORMAT_R8_UNORM;
        image.extent = {atlas.width, atlas.height, 1};
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device_, &image, nullptr, &uiAtlasImage_) != VK_SUCCESS) {
            return false;
        }
        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(device_, uiAtlasImage_, &req);
        uint32_t type = 0;
        if (!findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &type)) {
            return false;
        }
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = type;
        if (vkAllocateMemory(device_, &alloc, nullptr, &uiAtlasMemory_) != VK_SUCCESS
            || vkBindImageMemory(device_, uiAtlasImage_, uiAtlasMemory_, 0) != VK_SUCCESS) {
            return false;
        }
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = uiAtlasImage_;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = VK_FORMAT_R8_UNORM;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &view, nullptr, &uiAtlasView_) != VK_SUCCESS) {
            return false;
        }
        uiAtlasWidth_ = atlas.width;
        uiAtlasHeight_ = atlas.height;
        uiAtlasReady_ = false;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = uiSampler_;
        imageInfo.imageView = uiAtlasView_;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = uiSet_;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    const VkDeviceSize bytes = atlas.pixels.size();
    if (!createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &staging, &stagingMemory)) {
        return false;
    }
    void* mapped = nullptr;
    vkMapMemory(device_, stagingMemory, 0, bytes, 0, &mapped);
    std::memcpy(mapped, atlas.pixels.data(), static_cast<size_t>(bytes));
    vkUnmapMemory(device_, stagingMemory);

    // The atlas may still be sampled by a frame in flight.
    vkDeviceWaitIdle(device_);
    vkResetCommandBuffer(uploadCommandBuffer_, 0);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(uploadCommandBuffer_, &begin);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = uiAtlasReady_ ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                         : VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = uiAtlasImage_;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(uploadCommandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {atlas.width, atlas.height, 1};
    vkCmdCopyBufferToImage(uploadCommandBuffer_, staging, uiAtlasImage_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier toShader = toTransfer;
    toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(uploadCommandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toShader);
    vkEndCommandBuffer(uploadCommandBuffer_);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &uploadCommandBuffer_;
    vkResetFences(device_, 1, &uploadFence_);
    const VkResult submitted = vkQueueSubmit(graphicsQueue_, 1, &submit, uploadFence_);
    if (submitted == VK_SUCCESS) {
        vkWaitForFences(device_, 1, &uploadFence_, VK_TRUE, UINT64_MAX);
    }
    vkDestroyBuffer(device_, staging, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
    if (submitted != VK_SUCCESS) {
        return false;
    }
    uiAtlasReady_ = true;
    uiAtlasRevision_ = atlas.revision;
    return true;
}

void Renderer::recordUiDraw(VkCommandBuffer cmd) {
    const UiFramePtr frame = uiFrame_;
    if (!frame || frame->indices.empty() || frame->commands.empty() || !uiAtlasReady_
        || uiPipeline_ == VK_NULL_HANDLE) {
        return;
    }
    const uint32_t slot = currentFrame_ % 2;
    const VkDeviceSize vertexBytes = frame->vertices.size() * sizeof(UiVertex);
    const VkDeviceSize indexBytes = frame->indices.size() * sizeof(uint32_t);
    const auto grow = [this](VkDeviceSize need, VkBufferUsageFlags usage, VkBuffer* buffer,
                             VkDeviceMemory* memory, VkDeviceSize* capacity) {
        if (*capacity >= need) {
            return true;
        }
        if (*buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, *buffer, nullptr);
        if (*memory != VK_NULL_HANDLE) vkFreeMemory(device_, *memory, nullptr);
        *buffer = VK_NULL_HANDLE;
        *memory = VK_NULL_HANDLE;
        VkDeviceSize size = 64 * 1024;
        while (size < need) size *= 2;
        if (!createBuffer(size, usage,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          buffer, memory)) {
            *capacity = 0;
            return false;
        }
        *capacity = size;
        return true;
    };
    if (!grow(vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &uiVertexBuffers_[slot],
              &uiVertexMemory_[slot], &uiVertexCapacity_[slot])
        || !grow(indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &uiIndexBuffers_[slot],
                 &uiIndexMemory_[slot], &uiIndexCapacity_[slot])) {
        return;
    }
    void* mapped = nullptr;
    vkMapMemory(device_, uiVertexMemory_[slot], 0, vertexBytes, 0, &mapped);
    std::memcpy(mapped, frame->vertices.data(), static_cast<size_t>(vertexBytes));
    vkUnmapMemory(device_, uiVertexMemory_[slot]);
    vkMapMemory(device_, uiIndexMemory_[slot], 0, indexBytes, 0, &mapped);
    std::memcpy(mapped, frame->indices.data(), static_cast<size_t>(indexBytes));
    vkUnmapMemory(device_, uiIndexMemory_[slot]);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipelineLayout_, 0, 1, &uiSet_,
                            0, nullptr);
    UiPush push{{static_cast<float>(swapchainExtent_.width),
                 static_cast<float>(swapchainExtent_.height)}};
    vkCmdPushConstants(cmd, uiPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UiPush), &push);
    VkViewport viewport{0.0f, 0.0f, static_cast<float>(swapchainExtent_.width),
                        static_cast<float>(swapchainExtent_.height), 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &uiVertexBuffers_[slot], &offset);
    vkCmdBindIndexBuffer(cmd, uiIndexBuffers_[slot], 0, VK_INDEX_TYPE_UINT32);
    const int32_t maxW = static_cast<int32_t>(swapchainExtent_.width);
    const int32_t maxH = static_cast<int32_t>(swapchainExtent_.height);
    for (const UiDrawCommand& command : frame->commands) {
        if (command.indexCount == 0
            || command.firstIndex + command.indexCount > frame->indices.size()) {
            continue;
        }
        int32_t x0 = command.clipX < 0 ? 0 : command.clipX;
        int32_t y0 = command.clipY < 0 ? 0 : command.clipY;
        int32_t x1 = command.clipX + static_cast<int32_t>(command.clipWidth);
        int32_t y1 = command.clipY + static_cast<int32_t>(command.clipHeight);
        x1 = x1 > maxW ? maxW : x1;
        y1 = y1 > maxH ? maxH : y1;
        if (x1 <= x0 || y1 <= y0) {
            continue;
        }
        VkRect2D scissor{{x0, y0}, {static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0)}};
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdDrawIndexed(cmd, command.indexCount, 1, command.firstIndex, 0, 0);
    }
}

}  // namespace forgeshape

#endif  // !__ANDROID__
