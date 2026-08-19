// ForgeShape native Vulkan renderer (Stage 003 viewport foundation).
//
// Owns exactly what is needed to present one indexed cube with perspective and
// depth into an Android Surface. This is deliberately NOT a generic engine.
#pragma once

// Required before <vulkan/vulkan.h> to expose VkAndroidSurfaceCreateInfoKHR
// and vkCreateAndroidSurfaceKHR.
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif

#include <android/native_window.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_mesh.h"

namespace forgeshape {

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Installs the camera state used by the next recorded frame. The renderer
    // consumes the snapshot verbatim; it never interprets input or derives
    // camera pose itself.
    void setCamera(const CameraSnapshot& camera) { camera_ = camera; }

    // Installs the object's DERIVED model transform for the next recorded
    // frame. The renderer consumes it verbatim: it owns no position, no
    // rotation and no Euler convention, and it never builds this matrix itself.
    // Authoritative placement lives in ConstructionTransform.
    void setModelTransform(const Mat4& model) { model_ = model; }

    // Visual selection state only. The renderer is never told WHICH object is
    // selected: identity is owned by SelectionController.
    void setSelectionHighlight(bool selected) { selectionHighlight_ = selected; }

    // Device-independent setup: Vulkan instance only.
    bool createInstance();
    void destroyInstance();

    // Binds the renderer to a live ANativeWindow. Creates (on first call) the
    // device, buffers and pipeline layout, then the surface-dependent objects.
    bool attachSurface(ANativeWindow* window);

    // Releases every object that references the ANativeWindow. Safe to call
    // when no surface is attached.
    void detachSurface();

    bool hasSurface() const { return surface_ != VK_NULL_HANDLE; }

    void requestResize() { needsSwapchainRebuild_ = true; }

    // Renders and presents one frame. Returns false on unrecoverable failure.
    bool drawFrame();

private:
    bool pickPhysicalDeviceAndQueues();
    bool createLogicalDevice();
    bool createShaderModules();
    bool createSyncObjects();
    bool createCommandPool();

    bool createSwapchainDependents();
    void destroySwapchainDependents();

    bool createSwapchain();
    bool createDepthResources();
    bool createRenderPass();
    bool createFramebuffers();
    bool createPipeline();
    bool createCommandBuffers();

    bool recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);

    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer* outBuffer,
                      VkDeviceMemory* outMemory);
    bool findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties,
                        uint32_t* outIndex) const;
    VkFormat selectDepthFormat() const;

    // --- dynamic mesh upload (renderer-owned, render thread only) ------------
    //
    // Every Vulkan buffer create/copy/destroy for mesh geometry happens here,
    // on the render thread. The CPU mesh is published by someone else; this
    // side only observes the newest revision and mirrors it onto the GPU.
    bool createMeshUploadObjects();
    void destroyMeshResources();
    // Waits on the renderer's own frame fences (never vkDeviceWaitIdle /
    // vkQueueWaitIdle) so no in-flight frame can still reference the mesh
    // buffers that are about to be overwritten or destroyed.
    bool waitForMeshBuffersIdle();
    bool ensureStagingCapacity(VkDeviceSize bytes);
    // Reuses the existing device-local allocation when it is already big
    // enough; grows (and retires the old one) only when it is not.
    bool ensureMeshCapacity(VkDeviceSize vertexBytes, VkDeviceSize indexBytes, bool* outGrew);
    bool uploadMesh(const RuntimeMesh& mesh);
    // Called once per frame, before recording: uploads only if the store's
    // current revision differs from the uploaded one.
    void syncMeshRevision();

    // Instance-level
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties_{};
    uint32_t apiVersion_ = 0;

    // Device-level (persistent across surfaces)
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily_ = UINT32_MAX;
    uint32_t presentQueueFamily_ = UINT32_MAX;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;

    // Steady-state mesh geometry: DEVICE_LOCAL, written only through the
    // host-visible staging buffer below. Device-scoped, so a Surface swap never
    // touches them and the uploaded revision survives home/resume.
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory_ = VK_NULL_HANDLE;
    VkDeviceSize vertexCapacityBytes_ = 0;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory_ = VK_NULL_HANDLE;
    VkDeviceSize indexCapacityBytes_ = 0;
    uint32_t indexCount_ = 0;

    // Bounded, reused staging: one HOST_VISIBLE buffer that carries the vertex
    // block followed by the index block. Grown only when it is too small.
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;
    VkDeviceSize stagingCapacityBytes_ = 0;
    VkCommandBuffer uploadCommandBuffer_ = VK_NULL_HANDLE;
    VkFence uploadFence_ = VK_NULL_HANDLE;

    MeshRevision uploadedRevision_ = kNoMeshRevision;
    MeshRevision failedRevision_ = kNoMeshRevision;  // do not retry in a loop
    uint64_t meshBufferGrowCount_ = 0;

    VkShaderModule vertShader_ = VK_NULL_HANDLE;
    VkShaderModule fragShader_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    // Surface-dependent
    ANativeWindow* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{0, 0};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> renderFinished_;   // one per swapchain image
    std::vector<VkFence> imagesInFlight_;

    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;

    // Frames in flight
    static constexpr uint32_t kMaxFramesInFlight = 2;
    VkSemaphore imageAvailable_[kMaxFramesInFlight]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkFence inFlightFences_[kMaxFramesInFlight]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    uint32_t currentFrame_ = 0;

    // Camera pose/projection, produced by CameraController and pushed in by the
    // viewport thread before each frame.
    CameraSnapshot camera_{};

    // Derived object placement, produced by ConstructionTransform and pushed in
    // by the viewport thread before each frame. Identity until it is set.
    Mat4 model_ = mat4Identity();

    // Purely visual: "tint the cube because something is selected".
    bool selectionHighlight_ = false;

    bool needsSwapchainRebuild_ = false;
    bool presentedThisSession_ = false;
    uint64_t frameIndex_ = 0;
};

}  // namespace forgeshape
