#include "forgeshape_renderer.h"

#include <android/log.h>

#include <algorithm>
#include <chrono>
#include <cstring>

#include "forgeshape_matcap.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"

// SPIR-V produced ahead of time by the NDK-provided glslc (see CMakeLists.txt).
static const uint32_t kSurfaceVertSpv[] =
#include "surface.vert.spv.inc"
    ;
static const uint32_t kSurfaceFragSpv[] =
#include "surface.frag.spv.inc"
    ;
static const uint32_t kGridVertSpv[] =
#include "grid.vert.spv.inc"
    ;
static const uint32_t kGizmoVertSpv[] =
#include "gizmo.vert.spv.inc"
    ;
static const uint32_t kGizmoFragSpv[] =
#include "gizmo.frag.spv.inc"
    ;
static const uint32_t kGridFragSpv[] =
#include "grid.frag.spv.inc"
    ;

#define FS_TAG "ForgeShape"
#define FS_LOGI(...) __android_log_print(ANDROID_LOG_INFO, FS_TAG, __VA_ARGS__)
#define FS_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, FS_TAG, __VA_ARGS__)
#define FS_FAIL(reason)                                                    \
    do {                                                                   \
        __android_log_print(ANDROID_LOG_ERROR, FS_TAG,                     \
                            "FORGESHAPE_NATIVE_VIEWPORT_FAIL:%s", reason); \
    } while (0)

#define FS_VK_CHECK(expr, reason)     \
    do {                              \
        VkResult _r = (expr);         \
        if (_r != VK_SUCCESS) {       \
            FS_LOGE("%s -> VkResult=%d", reason, (int)_r); \
            FS_FAIL(reason);          \
            return false;             \
        }                             \
    } while (0)

namespace forgeshape {
namespace {

// The complete per-draw uniform block, shared by both stages.
//
// It is EXACTLY 128 bytes, which is the minimum maxPushConstantsSize the Vulkan
// specification guarantees. Some implementations expose 256, but ForgeShape
// targets the guarantee, so this struct must not grow: the next thing that
// needs per-draw uniform data belongs in a descriptor, not here.
//
// The layout is mirrored, member for member and offset for offset, by
// shaders/surface.vert and shaders/surface.frag. A static_assert below pins the
// size so a careless addition fails the build rather than the device.
struct SurfacePush {
    float mvp[16];  // offset 0

    // The upper-left 3x3 of (view * model), by ROWS, so the vertex shader
    // transforms a normal with three dot products. Valid as a plain matrix
    // rather than an inverse-transpose only because both factors are rigid —
    // see the note in surface.vert.
    //
    // Each row's w component is otherwise dead weight forced by std430's
    // 16-byte vec3 alignment, so `normalRow0.w` carries the shading model
    // rather than costing a fifth 16-byte slot the budget does not have. Rows
    // 1 and 2 keep a zero w.
    float normalRow0[4];  // offset 64, w = ShadingModel as a float
    float normalRow1[4];  // offset 80
    float normalRow2[4];  // offset 96

    // rgb is the tint colour, a is how much of it to mix in. The renderer is
    // told only WHETHER to draw the highlight; which object is selected is
    // owned by SelectionController and never reaches this file.
    float selectionTint[4];  // offset 112
};

static_assert(sizeof(SurfacePush) == 128,
              "the push constant block must stay inside the guaranteed 128-byte budget");

// The grid's per-draw uniform block, mirrored by shaders/grid.vert.
//
// Also exactly 128 bytes, and also at the guaranteed minimum rather than at
// whatever a particular device happens to expose. There is no model matrix and
// no normal matrix here because there is nothing to place or to light: the grid
// IS world space and it takes no light. What fills the budget instead is one
// colour per GridLineTier, which is what keeps the tier in the vertex buffer and
// the appearance in the push constants — so switching Dark to Light re-uploads
// nothing at all.
struct GridPush {
    float viewProj[16];    // offset 0
    float minorColor[4];   // offset 64
    float majorColor[4];   // offset 80
    float axisXColor[4];   // offset 96
    float axisZColor[4];   // offset 112
};

static_assert(sizeof(GridPush) == 128,
              "the grid push constant block must stay inside the guaranteed 128-byte budget");

// The gizmo per-draw uniform block, mirrored by shaders/gizmo.vert.
//
// Also exactly 128 bytes and also at the guaranteed minimum. There is ONE matrix
// and no model/normal split, because a handle has no normal and takes no light.
// That leaves precisely four vec4s, and the gizmo needs five values plus three
// colours — so the three axis colours carry the extra scalars in their
// otherwise-wasted w components, exactly the way the surface block carries its
// shading model. The packing is documented once, in gizmo.vert, and written once,
// here. Highlighting a grabbed handle is one float and no upload.
struct GizmoPush {
    float mvp[16];        // offset 0
    // rgb = axis colour; w = neutral grey level (the uniform-scale cube)
    float axisXColor[4];  // offset 64
    // rgb = axis colour; w = which HANDLE is held, as a GizmoHandle code
    float axisYColor[4];  // offset 80
    // rgb = axis colour; w = alpha multiplier for the handles NOT held
    float axisZColor[4];  // offset 96
    // rgb = the colour a held handle is drawn in; w = base alpha
    float highlight[4];   // offset 112
};

static_assert(sizeof(GizmoPush) == 128,
              "the gizmo push constant block must stay inside the guaranteed 128-byte budget");

// How far the grid is pushed AWAY from the eye in depth, in NDC, to settle the
// one case where it is exactly coplanar with real geometry: a Construction
// Plane sitting at world y = 0, which is where the grid lives.
//
// Applied in the vertex shader rather than through VkPipelineRasterizationState's
// depth bias, because that is defined for POLYGON fragments and the grid is a
// line list — enabling it would look like the fix and do nothing. Mirrored by
// shaders/grid.vert, which is the only consumer.
//
// The sign matters: a POSITIVE nudge makes the grid lose every tie under
// VK_COMPARE_OP_LESS, so a coplanar Plane always wins and the result is
// deterministic rather than a per-pixel coin toss that shimmers as the camera
// moves. Nudging the GRID rather than the Plane is equally deliberate: the
// domain Plane keeps the y its ConstructionTransform says and its 4-vertex /
// 6-index topology, and no Construction parameter is moved for a presentation
// problem.
//
// 1e-4 of the [0,1] depth range is far above the float depth buffer's
// resolvable difference anywhere the model realistically sits, and far below
// anything geometrically visible: at the default 8.2 m framing it hides the
// grid within roughly a millimetre of a surface.
constexpr float kGridDepthNudge = 1.0e-4f;

// The selection HUE, unchanged. What changed at UI-R1C1 is only how much of it
// is mixed in, and that is no longer a constant: it comes per body, per frame,
// from forgeshape_selection_pulse.h — the peak of an acknowledgement pulse when
// a body has just become selected, and kSelectionRestingAlpha for as long as it
// stays selected. A not-selected body mixes nothing, which is why alpha 0 and
// the colour below are all a body needs to disappear from the highlight.
const float kSelectionTintRgb[3] = {1.00f, 0.62f, 0.10f};

}  // namespace

// Seeded with the camera's own defaults so the very first recorded frame is
// valid even if the viewport thread has not pushed a snapshot yet.
Renderer::Renderer() : camera_(CameraController().snapshot()) {}

Renderer::~Renderer() {
    detachSurface();
    destroyInstance();
}

// ---------------------------------------------------------------------------
// Instance
// ---------------------------------------------------------------------------

bool Renderer::createInstance() {
    if (instance_ != VK_NULL_HANDLE) {
        return true;
    }

    uint32_t loaderVersion = VK_API_VERSION_1_0;
    auto enumerateVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (enumerateVersion != nullptr && enumerateVersion(&loaderVersion) != VK_SUCCESS) {
        loaderVersion = VK_API_VERSION_1_0;
    }
    FS_LOGI("Vulkan loader instance version: %u.%u.%u",
            VK_VERSION_MAJOR(loaderVersion), VK_VERSION_MINOR(loaderVersion),
            VK_VERSION_PATCH(loaderVersion));

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ForgeShape";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 3, 0);
    appInfo.pEngineName = "ForgeShapeNative";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 3, 0);
    appInfo.apiVersion = (loaderVersion >= VK_API_VERSION_1_1) ? VK_API_VERSION_1_1
                                                               : VK_API_VERSION_1_0;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;

    FS_VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance_), "vkCreateInstance");
    apiVersion_ = appInfo.apiVersion;
    FS_LOGI("Vulkan instance created (requested API %u.%u)",
            VK_VERSION_MAJOR(apiVersion_), VK_VERSION_MINOR(apiVersion_));
    return true;
}

// The Vulkan result codes forgeshape_render_recovery.h names without including
// Vulkan. Asserted here, where both worlds are visible, so the policy can stay
// Vulkan-free without the two ever drifting apart.
static_assert(kVkSuccessCode == static_cast<int>(VK_SUCCESS), "VK_SUCCESS code drift");
static_assert(kVkSuboptimalCode == static_cast<int>(VK_SUBOPTIMAL_KHR),
              "VK_SUBOPTIMAL_KHR code drift");
static_assert(kVkErrorOutOfDateCode == static_cast<int>(VK_ERROR_OUT_OF_DATE_KHR),
              "VK_ERROR_OUT_OF_DATE_KHR code drift");
static_assert(kVkErrorSurfaceLostCode == static_cast<int>(VK_ERROR_SURFACE_LOST_KHR),
              "VK_ERROR_SURFACE_LOST_KHR code drift");
static_assert(kVkErrorDeviceLostCode == static_cast<int>(VK_ERROR_DEVICE_LOST),
              "VK_ERROR_DEVICE_LOST code drift");

void Renderer::destroyInstance() {
    destroyDeviceScopedResources();

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        FS_LOGI("Vulkan instance destroyed");
    }
}

void Renderer::destroyDeviceScopedResources() {
    if (device_ != VK_NULL_HANDLE) {
        // A full device idle is right here for both callers: at process
        // teardown every queue is about to disappear with the device, and after
        // a device loss the wait returns immediately with an error rather than
        // blocking. Destruction is the one thing that stays lawful on a lost
        // device, which is why this whole block is safe to run on one.
        vkDeviceWaitIdle(device_);

        destroyMatCapResources();
        destroyMeshResources();
        destroyGridResources();
        destroyGizmoResources();
        destroySketchOverlayResources();
        if (vertShader_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vertShader_, nullptr);
        if (fragShader_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, fragShader_, nullptr);
        if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        if (gridVertShader_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, gridVertShader_, nullptr);
        if (gridFragShader_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, gridFragShader_, nullptr);
        if (gridPipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, gridPipelineLayout_, nullptr);
        }
        if (gizmoVertShader_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, gizmoVertShader_, nullptr);
        }
        if (gizmoFragShader_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, gizmoFragShader_, nullptr);
        }
        if (gizmoPipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, gizmoPipelineLayout_, nullptr);
        }

        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (imageAvailable_[i] != VK_NULL_HANDLE) vkDestroySemaphore(device_, imageAvailable_[i], nullptr);
            if (inFlightFences_[i] != VK_NULL_HANDLE) vkDestroyFence(device_, inFlightFences_[i], nullptr);
            imageAvailable_[i] = VK_NULL_HANDLE;
            inFlightFences_[i] = VK_NULL_HANDLE;
        }
        if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);

        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
        vertShader_ = VK_NULL_HANDLE;
        fragShader_ = VK_NULL_HANDLE;
        pipelineLayout_ = VK_NULL_HANDLE;
        gridVertShader_ = VK_NULL_HANDLE;
        gridFragShader_ = VK_NULL_HANDLE;
        gridPipelineLayout_ = VK_NULL_HANDLE;
        gizmoVertShader_ = VK_NULL_HANDLE;
        gizmoFragShader_ = VK_NULL_HANDLE;
        gizmoPipelineLayout_ = VK_NULL_HANDLE;
        commandPool_ = VK_NULL_HANDLE;
    }
}

bool Renderer::rebuildDeviceAfterLoss() {
    // The window reference is the one thing that must outlive the teardown.
    // detachSurface releases the renderer's own reference, so an extra one is
    // taken here and handed straight back to attachSurface, which takes
    // ownership of it. Without this the renderer would have to ask the Android
    // layer to re-deliver a Surface it never actually lost.
    ANativeWindow* window = window_;
    if (window == nullptr) {
        // No surface to come back to. Not a failure: the next attach builds a
        // fresh device anyway, because the teardown below clears device_.
        detachSurface();
        destroyDeviceScopedResources();
        FS_LOGI("FORGESHAPE_%s:no_surface_to_restore", kRenderDeviceRebuiltToken);
        return true;
    }
    ANativeWindow_acquire(window);

    detachSurface();
    destroyDeviceScopedResources();

    // attachSurface takes ownership of the reference acquired above and, with
    // device_ now null, walks the whole first-attach path: physical device,
    // logical device, command pool, descriptors, shaders, sync objects, upload
    // objects, MatCap, grid, gizmo, swapchain.
    const bool attached = attachSurface(window);
    if (!attached) {
        FS_LOGE("FORGESHAPE_%s:rebuild_attach_failed", kRenderRestartRequiredToken);
        return false;
    }
    // Nothing re-uploads geometry here on purpose. destroyMeshResources cleared
    // the per-body upload record, so the next frame's syncScene sees every body
    // as new and mirrors the CPU truth that was never touched.
    FS_LOGI("FORGESHAPE_%s:attempt=%d completed=%d", kRenderDeviceRebuiltToken,
            recovery_.deviceRebuildAttempts(), recovery_.deviceRebuildsCompleted() + 1);
    return true;
}

bool Renderer::handleFrameResult(int vkResultCode, const char* where) {
    const RenderFailureKind kind = classifyRenderResult(vkResultCode, expectSuboptimal_);
    if (kind == RenderFailureKind::None) {
        return true;
    }

    const RenderRecoveryAction action = recovery_.onFailure(kind);
    switch (action) {
        case RenderRecoveryAction::Continue:
            return true;

        case RenderRecoveryAction::RebuildSwapchain:
            needsSwapchainRebuild_ = true;
            return true;

        case RenderRecoveryAction::RebuildDevice: {
            FS_LOGE("FORGESHAPE_%s:%s attempt=%d", kRenderDeviceLostToken, where,
                    recovery_.deviceRebuildAttempts());
            const bool rebuilt = rebuildDeviceAfterLoss();
            recovery_.onDeviceRebuildFinished(rebuilt);
            if (!rebuilt) {
                FS_LOGE("FORGESHAPE_%s:%s", kRenderRestartRequiredToken, where);
                return false;
            }
            return true;
        }

        case RenderRecoveryAction::StopRestartRequired:
            FS_LOGE("FORGESHAPE_%s:%s:%s", kRenderRestartRequiredToken, where,
                    renderFailureKindName(kind));
            return false;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Surface attach / detach
// ---------------------------------------------------------------------------

bool Renderer::attachSurface(ANativeWindow* window) {
    if (window == nullptr) {
        FS_FAIL("attachSurface_null_window");
        return false;
    }
    detachSurface();

    window_ = window;  // takes ownership of the caller's ANativeWindow reference

    VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = window_;
    FS_VK_CHECK(vkCreateAndroidSurfaceKHR(instance_, &surfaceInfo, nullptr, &surface_),
                "vkCreateAndroidSurfaceKHR");
    FS_LOGI("Android Vulkan surface created (window %dx%d)",
            ANativeWindow_getWidth(window_), ANativeWindow_getHeight(window_));

    if (device_ == VK_NULL_HANDLE) {
        if (!pickPhysicalDeviceAndQueues()) return false;
        if (!createLogicalDevice()) return false;
        if (!createCommandPool()) return false;
        // The descriptor set layout must exist before the pipeline layout that
        // references it, and the MatCap upload needs the staging buffer and
        // upload command buffer that createMeshUploadObjects provides — so the
        // two halves of the sampler setup sit on either side of it.
        if (!createDescriptorResources()) return false;
        if (!createShaderModules()) return false;
        if (!createSyncObjects()) return false;
        if (!createMeshUploadObjects()) return false;
        if (!createMatCapResources()) return false;
        // After the upload objects, because the grid borrows the same staging
        // buffer and upload command buffer for its single, one-time copy.
        if (!createGridResources()) return false;
        // And the gizmo, on the same borrowed staging path and for the same
        // one-time copy. It is device-scoped, so a HOME/resume does not
        // regenerate it.
        if (!createGizmoResources()) return false;
    } else {
        // Reused device: confirm the new surface is still presentable.
        VkBool32 supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, presentQueueFamily_, surface_, &supported);
        if (supported != VK_TRUE) {
            FS_FAIL("surface_not_presentable_on_existing_queue");
            return false;
        }
    }

    presentedThisSession_ = false;
    return createSwapchainDependents();
}

void Renderer::detachSurface() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    destroySwapchainDependents();

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
        FS_LOGI("Android Vulkan surface destroyed");
    }
    if (window_ != nullptr) {
        ANativeWindow_release(window_);
        window_ = nullptr;
        FS_LOGI("ANativeWindow released");
    }
}

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------

bool Renderer::pickPhysicalDeviceAndQueues() {
    uint32_t count = 0;
    FS_VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
    if (count == 0) {
        FS_FAIL("no_vulkan_physical_device");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    FS_VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "vkEnumeratePhysicalDevices");

    for (VkPhysicalDevice candidate : devices) {
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

        uint32_t graphics = UINT32_MAX;
        uint32_t present = UINT32_MAX;
        for (uint32_t i = 0; i < familyCount; ++i) {
            if (families[i].queueCount == 0) continue;
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if (graphics == UINT32_MAX) graphics = i;
            }
            VkBool32 supported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_, &supported);
            if (supported == VK_TRUE && present == UINT32_MAX) present = i;
            if (graphics != UINT32_MAX && present != UINT32_MAX &&
                (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supported == VK_TRUE) {
                graphics = i;
                present = i;
                break;
            }
        }
        if (graphics == UINT32_MAX || present == UINT32_MAX) continue;

        // Also require a swapchain-capable device.
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extCount, exts.data());
        bool hasSwapchain = false;
        for (const auto& e : exts) {
            if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        if (!hasSwapchain) continue;

        physicalDevice_ = candidate;
        graphicsQueueFamily_ = graphics;
        presentQueueFamily_ = present;
        break;
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        FS_FAIL("no_suitable_physical_device");
        return false;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties_);
    FS_LOGI("Physical device selected: %s (type=%d, deviceApi=%u.%u.%u, driver=%u)",
            props.deviceName, (int)props.deviceType,
            VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion),
            VK_VERSION_PATCH(props.apiVersion), props.driverVersion);
    FS_LOGI("Queue families: graphics=%u present=%u", graphicsQueueFamily_, presentQueueFamily_);
    return true;
}

bool Renderer::createLogicalDevice() {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfos[2]{};
    uint32_t queueInfoCount = 1;

    queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfos[0].queueFamilyIndex = graphicsQueueFamily_;
    queueInfos[0].queueCount = 1;
    queueInfos[0].pQueuePriorities = &priority;

    if (presentQueueFamily_ != graphicsQueueFamily_) {
        queueInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[1].queueFamilyIndex = presentQueueFamily_;
        queueInfos[1].queueCount = 1;
        queueInfos[1].pQueuePriorities = &priority;
        queueInfoCount = 2;
    }

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = queueInfoCount;
    deviceInfo.pQueueCreateInfos = queueInfos;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    FS_VK_CHECK(vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);
    FS_LOGI("Logical device and queues created");
    return true;
}

bool Renderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily_;
    FS_VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_), "vkCreateCommandPool");
    return true;
}

// ---------------------------------------------------------------------------
// Buffers / shaders / sync
// ---------------------------------------------------------------------------

bool Renderer::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties,
                              uint32_t* outIndex) const {
    for (uint32_t i = 0; i < memoryProperties_.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (memoryProperties_.memoryTypes[i].propertyFlags & properties) == properties) {
            *outIndex = i;
            return true;
        }
    }
    return false;
}

bool Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags properties, VkBuffer* outBuffer,
                            VkDeviceMemory* outMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    FS_VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, outBuffer), "vkCreateBuffer");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device_, *outBuffer, &req);

    uint32_t memoryType = 0;
    if (!findMemoryType(req.memoryTypeBits, properties, &memoryType)) {
        FS_FAIL("no_suitable_memory_type");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = req.size;
    allocInfo.memoryTypeIndex = memoryType;
    FS_VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, outMemory), "vkAllocateMemory");
    FS_VK_CHECK(vkBindBufferMemory(device_, *outBuffer, *outMemory, 0), "vkBindBufferMemory");
    return true;
}

// ---------------------------------------------------------------------------
// Dynamic mesh: device-local buffers + a bounded, reused staging upload
// ---------------------------------------------------------------------------
//
// Ownership: every VkBuffer/VkDeviceMemory below is created, written and
// destroyed here, on the render thread, and nowhere else. The CPU side only
// publishes revisions into MeshStore; it never touches Vulkan.
//
// In-flight safety: before the mesh buffers are overwritten or retired, this
// code waits on the renderer's OWN frame fences (all kMaxFramesInFlight of
// them). After that wait no submitted frame can still be reading them, so a
// transfer cannot race a draw and a retired buffer cannot be freed too early.
// It deliberately does not call vkDeviceWaitIdle or vkQueueWaitIdle.

bool Renderer::createMeshUploadObjects() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    FS_VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &uploadCommandBuffer_),
                "vkAllocateCommandBuffers(mesh_upload)");

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;  // starts unsignalled
    FS_VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &uploadFence_),
                "vkCreateFence(mesh_upload)");
    FS_LOGI("Mesh upload objects created (device-local target, host-visible staging)");
    return true;
}

void Renderer::destroyBodyResources(BodyRenderResources& body) {
    if (body.vertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, body.vertexBuffer, nullptr);
    if (body.vertexMemory != VK_NULL_HANDLE) vkFreeMemory(device_, body.vertexMemory, nullptr);
    if (body.indexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, body.indexBuffer, nullptr);
    if (body.indexMemory != VK_NULL_HANDLE) vkFreeMemory(device_, body.indexMemory, nullptr);
    body.vertexBuffer = VK_NULL_HANDLE;
    body.vertexMemory = VK_NULL_HANDLE;
    body.vertexCapacityBytes = 0;
    body.indexBuffer = VK_NULL_HANDLE;
    body.indexMemory = VK_NULL_HANDLE;
    body.indexCapacityBytes = 0;
    body.indexCount = 0;
    body.uploadedRevision = kNoMeshRevision;
    body.failedRevision = kNoMeshRevision;
    // The GPU no longer holds anything derived from the cache, so the cache
    // must not claim it does: the next sync has to rebuild and re-upload.
    body.renderMesh.invalidate();
}

BodyRenderResources& Renderer::resourcesFor(ObjectId objectId) {
    // Keyed by stable ObjectId, so a body keeps its own buffers for as long as
    // the scene names it, no matter how many other bodies are added around it.
    // A body the scene stops naming is released by releaseBodiesAbsentFromScene
    // and simply re-uploads here if it ever comes back.
    return bodies_[objectId];
}

void Renderer::destroyMeshResources() {
    if (device_ == VK_NULL_HANDLE) return;

    for (auto& entry : bodies_) {
        destroyBodyResources(entry.second);
    }
    bodies_.clear();

    if (stagingBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, stagingBuffer_, nullptr);
    if (stagingMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, stagingMemory_, nullptr);
    if (uploadFence_ != VK_NULL_HANDLE) vkDestroyFence(device_, uploadFence_, nullptr);
    if (uploadCommandBuffer_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &uploadCommandBuffer_);
    }

    stagingBuffer_ = VK_NULL_HANDLE;
    stagingMemory_ = VK_NULL_HANDLE;
    stagingCapacityBytes_ = 0;
    uploadFence_ = VK_NULL_HANDLE;
    uploadCommandBuffer_ = VK_NULL_HANDLE;
    meshUploadDiagnostics().setLiveBufferObjects(0);
}

bool Renderer::waitForMeshBuffersIdle(uint64_t timeoutNanoseconds) {
    // Every frame that could reference the mesh buffers was submitted with one
    // of these fences, so waiting for all of them is exactly "no in-flight
    // frame still uses the mesh". Renderer-owned fences only.
    VkFence fences[kMaxFramesInFlight];
    uint32_t count = 0;
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (inFlightFences_[i] != VK_NULL_HANDLE) {
            fences[count++] = inFlightFences_[i];
        }
    }
    if (count == 0) {
        return true;
    }
    const VkResult r = vkWaitForFences(device_, count, fences, VK_TRUE, timeoutNanoseconds);
    if (r == VK_SUCCESS) {
        return true;
    }
    if (r == VK_TIMEOUT) {
        // Only reachable for a caller that supplied a deadline, and for that
        // caller this is "not yet" rather than a failure -- it asks again on
        // the next frame. With the default there is no deadline to expire.
        return false;
    }
    FS_LOGE("vkWaitForFences(mesh idle) -> %d", (int)r);
    FS_FAIL("mesh_wait_frames_idle");
    return false;
}

bool Renderer::ensureStagingCapacity(VkDeviceSize bytes) {
    if (bytes <= stagingCapacityBytes_ && stagingBuffer_ != VK_NULL_HANDLE) {
        return true;  // reuse
    }

    uint64_t grown = 0;
    if (!growCapacityBytes(static_cast<uint64_t>(stagingCapacityBytes_),
                           static_cast<uint64_t>(bytes), &grown)) {
        FS_FAIL("mesh_staging_capacity_overflow");
        return false;
    }

    // The staging buffer is only ever touched between fenced uploads, and the
    // caller has already waited for the previous upload fence, so replacing it
    // here is safe.
    if (stagingBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, stagingBuffer_, nullptr);
    if (stagingMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, stagingMemory_, nullptr);
    stagingBuffer_ = VK_NULL_HANDLE;
    stagingMemory_ = VK_NULL_HANDLE;
    stagingCapacityBytes_ = 0;

    const VkMemoryPropertyFlags hostVisible =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!createBuffer(static_cast<VkDeviceSize>(grown), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      hostVisible, &stagingBuffer_, &stagingMemory_)) {
        return false;
    }
    stagingCapacityBytes_ = static_cast<VkDeviceSize>(grown);
    meshUploadDiagnostics().recordStagingGrow(grown);
    return true;
}

bool Renderer::ensureMeshCapacity(BodyRenderResources& body, VkDeviceSize vertexBytes,
                                  VkDeviceSize indexBytes, bool* outGrew) {
    const bool vertexFits = body.vertexBuffer != VK_NULL_HANDLE && vertexBytes <= body.vertexCapacityBytes;
    const bool indexFits = body.indexBuffer != VK_NULL_HANDLE && indexBytes <= body.indexCapacityBytes;
    if (vertexFits && indexFits) {
        if (outGrew != nullptr) *outGrew = false;
        return true;  // same-topology (or smaller) update: reuse, no recreation
    }

    uint64_t newVertexCapacity = body.vertexCapacityBytes;
    uint64_t newIndexCapacity = body.indexCapacityBytes;
    if (!vertexFits &&
        !growCapacityBytes(body.vertexCapacityBytes, vertexBytes, &newVertexCapacity)) {
        FS_FAIL("mesh_vertex_capacity_overflow");
        return false;
    }
    if (!indexFits && !growCapacityBytes(body.indexCapacityBytes, indexBytes, &newIndexCapacity)) {
        FS_FAIL("mesh_index_capacity_overflow");
        return false;
    }

    // A buffer is only ever retired after every in-flight frame has finished
    // with it; the caller guarantees that before calling in.
    if (!vertexFits) {
        if (body.vertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, body.vertexBuffer, nullptr);
        if (body.vertexMemory != VK_NULL_HANDLE) vkFreeMemory(device_, body.vertexMemory, nullptr);
        body.vertexBuffer = VK_NULL_HANDLE;
        body.vertexMemory = VK_NULL_HANDLE;
        body.vertexCapacityBytes = 0;
        if (!createBuffer(static_cast<VkDeviceSize>(newVertexCapacity),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &body.vertexBuffer, &body.vertexMemory)) {
            return false;
        }
        body.vertexCapacityBytes = static_cast<VkDeviceSize>(newVertexCapacity);
        ++meshBufferGrowCount_;
        meshUploadDiagnostics().recordBufferGrow();
    }
    if (!indexFits) {
        if (body.indexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, body.indexBuffer, nullptr);
        if (body.indexMemory != VK_NULL_HANDLE) vkFreeMemory(device_, body.indexMemory, nullptr);
        body.indexBuffer = VK_NULL_HANDLE;
        body.indexMemory = VK_NULL_HANDLE;
        body.indexCapacityBytes = 0;
        if (!createBuffer(static_cast<VkDeviceSize>(newIndexCapacity),
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &body.indexBuffer, &body.indexMemory)) {
            return false;
        }
        body.indexCapacityBytes = static_cast<VkDeviceSize>(newIndexCapacity);
        ++meshBufferGrowCount_;
        meshUploadDiagnostics().recordBufferGrow();
    }

    if (outGrew != nullptr) *outGrew = true;
    return true;
}

bool Renderer::uploadRenderMesh(BodyRenderResources& body, ObjectId objectId,
                                const RenderMeshData& mesh, MeshRevision sourceRevision) {
    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(mesh.vertexBytes());
    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(mesh.indexBytes());
    if (vertexBytes == 0 || indexBytes == 0) {
        FS_FAIL("mesh_upload_empty");
        return false;
    }

    // 1. No in-flight frame may still be reading the buffers we are about to
    //    write to or retire.
    if (!waitForMeshBuffersIdle()) {
        return false;
    }

    // 2. Reuse the device-local allocation when it is already big enough.
    bool grew = false;
    if (!ensureMeshCapacity(body, vertexBytes, indexBytes, &grew)) {
        return false;
    }

    // 3. Stage on the host, then copy on the device.
    const VkDeviceSize stagingNeeded = vertexBytes + indexBytes;
    if (!ensureStagingCapacity(stagingNeeded)) {
        return false;
    }

    void* mapped = nullptr;
    FS_VK_CHECK(vkMapMemory(device_, stagingMemory_, 0, stagingNeeded, 0, &mapped),
                "vkMapMemory(mesh_staging)");
    auto* bytes = static_cast<unsigned char*>(mapped);
    std::memcpy(bytes, mesh.vertices.data(), static_cast<size_t>(vertexBytes));
    std::memcpy(bytes + vertexBytes, mesh.indices.data(), static_cast<size_t>(indexBytes));
    vkUnmapMemory(device_, stagingMemory_);

    FS_VK_CHECK(vkResetCommandBuffer(uploadCommandBuffer_, 0), "vkResetCommandBuffer(mesh_upload)");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    FS_VK_CHECK(vkBeginCommandBuffer(uploadCommandBuffer_, &begin), "vkBeginCommandBuffer(mesh_upload)");

    VkBufferCopy vertexCopy{};
    vertexCopy.srcOffset = 0;
    vertexCopy.dstOffset = 0;
    vertexCopy.size = vertexBytes;
    vkCmdCopyBuffer(uploadCommandBuffer_, stagingBuffer_, body.vertexBuffer, 1, &vertexCopy);

    VkBufferCopy indexCopy{};
    indexCopy.srcOffset = vertexBytes;
    indexCopy.dstOffset = 0;
    indexCopy.size = indexBytes;
    vkCmdCopyBuffer(uploadCommandBuffer_, stagingBuffer_, body.indexBuffer, 1, &indexCopy);

    // The transfer must be visible to vertex/index fetch of every later frame.
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
    vkCmdPipelineBarrier(uploadCommandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

    FS_VK_CHECK(vkEndCommandBuffer(uploadCommandBuffer_), "vkEndCommandBuffer(mesh_upload)");

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &uploadCommandBuffer_;

    FS_VK_CHECK(vkResetFences(device_, 1, &uploadFence_), "vkResetFences(mesh_upload)");
    FS_VK_CHECK(vkQueueSubmit(graphicsQueue_, 1, &submit, uploadFence_), "vkQueueSubmit(mesh_upload)");
    // Renderer-owned upload fence, not a queue/device idle: the next upload may
    // reuse this command buffer and this staging memory only once it is done.
    FS_VK_CHECK(vkWaitForFences(device_, 1, &uploadFence_, VK_TRUE, UINT64_MAX),
                "vkWaitForFences(mesh_upload)");

    body.indexCount = mesh.indexCount();
    body.uploadedRevision = sourceRevision;
    body.uploadedShading = mesh.shading;

    MeshUploadDiagnostics& diagnostics = meshUploadDiagnostics();
    // The recorded counts are the RENDER counts — what the GPU actually holds.
    // The source counts are recorded separately and logged beside them, because
    // they are different numbers about different things and confusing them
    // would make every capacity and topology diagnostic misleading.
    diagnostics.recordUpload(sourceRevision, mesh.vertexCount(), mesh.indexCount(),
                             body.vertexCapacityBytes, body.indexCapacityBytes, /*reusedCapacity=*/!grew);
    diagnostics.recordSourceCounts(mesh.sourceVertexCount, mesh.sourceIndexCount);
    // Two live buffer objects per BODY at most: within a body an old buffer is
    // destroyed in the same step that replaces it, so the total is a direct
    // count of bodies that hold geometry, not a leak indicator that grows on
    // its own.
    uint32_t liveBuffers = 0;
    for (const auto& entry : bodies_) {
        if (entry.second.vertexBuffer != VK_NULL_HANDLE) ++liveBuffers;
        if (entry.second.indexBuffer != VK_NULL_HANDLE) ++liveBuffers;
    }
    diagnostics.setLiveBufferObjects(liveBuffers);

    const MeshGpuStats stats = diagnostics.snapshot();
    // The historical prefix is preserved verbatim, including the counts in
    // positions 2 and 3, so existing evidence tooling keeps parsing. Those two
    // are now the RENDER counts; `src=` names the authoritative ones and
    // `shading=` says which derivation produced the difference.
    // `body=` is new in Stage 017 and is appended rather than inserted, so the
    // historical prefix keeps parsing: with several Bodies in the scene an
    // upload line is otherwise ambiguous about which one it describes, which is
    // exactly the evidence "editing A did not re-upload B" depends on.
    FS_LOGI("FORGESHAPE_MESH_UPLOAD_OK:%llu:%u:%u:%s vcap=%llu icap=%llu scap=%llu "
            "grows=%llu sgrows=%llu uploads=%llu src=%u:%u shading=%s body=%llu",
            (unsigned long long)sourceRevision, mesh.vertexCount(), mesh.indexCount(),
            grew ? "grow" : "reuse", (unsigned long long)stats.vertexCapacityBytes,
            (unsigned long long)stats.indexCapacityBytes,
            (unsigned long long)stats.stagingCapacityBytes,
            (unsigned long long)stats.bufferGrowCount,
            (unsigned long long)stats.stagingGrowCount,
            (unsigned long long)stats.uploadCount, mesh.sourceVertexCount, mesh.sourceIndexCount,
            surfaceShadingName(mesh.shading), (unsigned long long)objectId);
    return true;
}

// How long the opportunistic mesh-resource release may wait for the in-flight
// frames, in nanoseconds. 100 ms: far above a frame, so the release lands on the
// frame the body left; far below anything a user would call a freeze, and
// bounded so a fence that will never signal cannot hang the render thread.
constexpr uint64_t kMeshReleaseWaitNanoseconds = 100ull * 1000ull * 1000ull;

void Renderer::releaseBodiesAbsentFromScene() {
    if (bodies_.empty() || device_ == VK_NULL_HANDLE) {
        return;
    }
    bool waited = false;
    for (auto it = bodies_.begin(); it != bodies_.end();) {
        bool present = false;
        for (const SceneDrawItem& item : scene_) {
            if (item.objectId == it->first) {
                present = true;
                break;
            }
        }
        if (present) {
            ++it;
            continue;
        }
        // Once, and only when something is actually going to be destroyed: a
        // buffer a submitted frame still references may not be freed, and the
        // renderer's own in-flight fences are exactly that question.
        //
        // BOUNDED, unlike the wait a capacity grow performs. This runs whenever
        // a body leaves the snapshot -- a delete, an undo, and every project
        // load that changes ObjectIds -- and it is opportunistic: there is
        // always a next frame to try again on. An unbounded wait here would
        // stake the render thread on a fence that may never signal, because a
        // failed `vkQueueSubmit` leaves that frame's fence reset with nothing
        // left to signal it; `surfaceDestroyed` then blocks on the render
        // thread and the Activity never tears down.
        //
        // The deadline is generous against a ~16 ms frame, so in the steady
        // state it is never approached and the release happens on the frame the
        // body left. It is a ceiling, not a budget.
        if (!waited) {
            if (!waitForMeshBuffersIdle(kMeshReleaseWaitNanoseconds)) {
                // Not idle yet, or not idle at all. Leave every entry resident
                // and ask again next frame rather than free something a frame
                // in flight may still be reading.
                return;
            }
            waited = true;
        }
        FS_LOGI("FORGESHAPE_MESH_RESOURCES_RELEASED:%llu", (unsigned long long)it->first);
        destroyBodyResources(it->second);
        it = bodies_.erase(it);
    }
    if (!waited) {
        return;
    }
    uint32_t liveBuffers = 0;
    for (const auto& entry : bodies_) {
        if (entry.second.vertexBuffer != VK_NULL_HANDLE) ++liveBuffers;
        if (entry.second.indexBuffer != VK_NULL_HANDLE) ++liveBuffers;
    }
    meshUploadDiagnostics().setLiveBufferObjects(liveBuffers);
}

void Renderer::syncScene() {
    // Bodies the scene no longer names first, so nothing below is uploading
    // beside resources for objects that are gone. The diagnostic imported
    // preview REPLACES the scene rather than joining it, so while one is on
    // screen the project's own bodies are released here and re-uploaded when it
    // is cleared -- correct by the same route an Undo takes, and a debug-only
    // path nothing in the product reaches.
    releaseBodiesAbsentFromScene();
    // One independent pass per body. Nothing here is shared between bodies
    // except the transient staging buffer, so whether body B does any work is
    // decided entirely by B's own revision and the surface shading.
    for (const SceneDrawItem& item : scene_) {
        syncBody(item);
    }
}

double Renderer::consumeFrameDeltaSeconds() {
    const auto now = std::chrono::steady_clock::now();
    if (!haveFrameTime_) {
        haveFrameTime_ = true;
        lastFrameTime_ = now;
        return 0.0;
    }
    const double seconds =
        std::chrono::duration<double>(now - lastFrameTime_).count();
    lastFrameTime_ = now;
    return seconds;
}

void Renderer::advanceSelectionFeedback(double deltaSeconds) {
    // Reduced motion is read per frame from the same display snapshot that
    // carries the shading model and the viewport background. It is a plain bool
    // by the time it gets here; what an Android animator scale is stays above
    // JNI, where it belongs.
    const bool motionEnabled = !display_.reducedMotion;

    for (const SceneDrawItem& item : scene_) {
        auto found = bodies_.find(item.objectId);
        if (found == bodies_.end()) {
            continue;  // nothing uploaded for this body yet; nothing to tint
        }
        BodyRenderResources& body = found->second;
        body.selectionAlpha = advanceSelectionPulse(body.selectionPulse, item.selected,
                                                    deltaSeconds, motionEnabled);
    }
}

void Renderer::syncBody(const SceneDrawItem& item) {
    // The snapshot already holds this body's newest published revision; a
    // revision published between two frames is coalesced away, which is what
    // keeps this bounded.
    const RuntimeMeshPtr& mesh = item.mesh;
    if (!mesh) {
        return;
    }

    BodyRenderResources& body = resourcesFor(item.objectId);
    const SurfaceShading shading = display_.surface;

    // THE per-frame, per-body gate. On a steady frame both comparisons match
    // and this function does nothing at all: no normal generation, no
    // allocation, no buffer traffic. Camera motion, a rotation, an inspector
    // toggle, a unit switch, a selection change and a Studio<->MatCap change
    // all land here and stop.
    //
    // It is also what makes body independence structural: editing body A mints
    // a revision in A's OWN MeshStore, so B's cached revision still equals B's
    // published revision and B returns here without rebuilding or uploading.
    if ((mesh->revision() == body.uploadedRevision && shading == body.uploadedShading) ||
        (mesh->revision() == body.failedRevision && shading == body.uploadedShading)) {
        return;
    }

    // Rebuild the render-only derived geometry. This is where normals come
    // from; it never touches the authoritative RuntimeMesh, which stays exactly
    // as MeshStore published it and remains what CPU picking reads.
    bool buildFailed = false;
    body.renderMesh.refresh(*mesh, shading, &buildFailed);
    if (buildFailed || !body.renderMesh.valid()) {
        body.failedRevision = mesh->revision();
        body.uploadedShading = shading;  // do not retry this pair every frame
        meshUploadDiagnostics().recordFailure();
        FS_LOGE("FORGESHAPE_RENDER_MESH_FAIL:%llu body=%llu",
                (unsigned long long)mesh->revision(), (unsigned long long)item.objectId);
        return;
    }

    if (!uploadRenderMesh(body, item.objectId, body.renderMesh.data(), mesh->revision())) {
        body.failedRevision = mesh->revision();  // fail closed, keep the last good mesh
        body.uploadedShading = shading;
        meshUploadDiagnostics().recordFailure();
        FS_LOGE("FORGESHAPE_MESH_UPLOAD_FAIL:%llu body=%llu",
                (unsigned long long)mesh->revision(), (unsigned long long)item.objectId);
        return;
    }

    // Proof material for "no accidental per-frame rebuild".
    //
    // `rebuilds` and `frame` are the two numbers that settle it: this line is
    // emitted once per accepted geometry or shading change, so a session in
    // which `frame` climbs by thousands while `rebuilds` does not move at all
    // is a direct measurement rather than an argument.
    //
    // RenderMeshCache's own skipped-refresh counter is deliberately NOT logged
    // here: the gate above returns before refresh() is ever called on a steady
    // frame, so in production that counter would read zero forever and say
    // nothing. It counts direct calls, which is what the self-tests make.
    // `rebuilds` is this BODY's own rebuild count, so the measurement stays
    // exactly as strong as it was with one object: a session in which `frame`
    // climbs by thousands while a body's `rebuilds` does not move is direct
    // evidence that nothing rebuilt it.
    FS_LOGI("FORGESHAPE_RENDER_MESH_BUILD:%llu src=%u:%u render=%u:%u shading=%s "
            "rebuilds=%llu frame=%llu ms=%.3f body=%llu",
            (unsigned long long)mesh->revision(), body.renderMesh.data().sourceVertexCount,
            body.renderMesh.data().sourceIndexCount, body.renderMesh.data().vertexCount(),
            body.renderMesh.data().indexCount(), surfaceShadingName(body.renderMesh.shading()),
            (unsigned long long)body.renderMesh.rebuildCount(), (unsigned long long)frameIndex_,
            body.renderMesh.lastRebuildMillis(), (unsigned long long)item.objectId);
}

bool Renderer::createShaderModules() {
    VkShaderModuleCreateInfo vertInfo{};
    vertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertInfo.codeSize = sizeof(kSurfaceVertSpv);
    vertInfo.pCode = kSurfaceVertSpv;
    FS_VK_CHECK(vkCreateShaderModule(device_, &vertInfo, nullptr, &vertShader_), "vkCreateShaderModule(vert)");

    VkShaderModuleCreateInfo fragInfo{};
    fragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragInfo.codeSize = sizeof(kSurfaceFragSpv);
    fragInfo.pCode = kSurfaceFragSpv;
    FS_VK_CHECK(vkCreateShaderModule(device_, &fragInfo, nullptr, &fragShader_), "vkCreateShaderModule(frag)");

    // ONE range covering both stages. The vertex stage reads the MVP and the
    // normal rows, the fragment stage reads the normal rows' packed shading
    // model and the selection tint; declaring a single shared range is simpler
    // than two overlapping ones and is what both shaders' blocks describe.
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(SurfacePush);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    FS_VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_), "vkCreatePipelineLayout");

    // --- the grid's own shaders and layout ---------------------------------
    VkShaderModuleCreateInfo gridVertInfo{};
    gridVertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    gridVertInfo.codeSize = sizeof(kGridVertSpv);
    gridVertInfo.pCode = kGridVertSpv;
    FS_VK_CHECK(vkCreateShaderModule(device_, &gridVertInfo, nullptr, &gridVertShader_),
                "vkCreateShaderModule(grid_vert)");

    VkShaderModuleCreateInfo gridFragInfo{};
    gridFragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    gridFragInfo.codeSize = sizeof(kGridFragSpv);
    gridFragInfo.pCode = kGridFragSpv;
    FS_VK_CHECK(vkCreateShaderModule(device_, &gridFragInfo, nullptr, &gridFragShader_),
                "vkCreateShaderModule(grid_frag)");

    VkPushConstantRange gridRange{};
    gridRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    gridRange.offset = 0;
    gridRange.size = sizeof(GridPush);

    // setLayoutCount = 0, deliberately. The grid consults no sampler: it has no
    // normal, takes no light and ignores the shading model entirely, so it must
    // not be able to reach the MatCap even by accident.
    VkPipelineLayoutCreateInfo gridLayoutInfo{};
    gridLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    gridLayoutInfo.setLayoutCount = 0;
    gridLayoutInfo.pushConstantRangeCount = 1;
    gridLayoutInfo.pPushConstantRanges = &gridRange;
    FS_VK_CHECK(vkCreatePipelineLayout(device_, &gridLayoutInfo, nullptr, &gridPipelineLayout_),
                "vkCreatePipelineLayout(grid)");

    // --- the gizmo's own shaders and layout --------------------------------
    VkShaderModuleCreateInfo gizmoVertInfo{};
    gizmoVertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    gizmoVertInfo.codeSize = sizeof(kGizmoVertSpv);
    gizmoVertInfo.pCode = kGizmoVertSpv;
    FS_VK_CHECK(vkCreateShaderModule(device_, &gizmoVertInfo, nullptr, &gizmoVertShader_),
                "vkCreateShaderModule(gizmo_vert)");

    VkShaderModuleCreateInfo gizmoFragInfo{};
    gizmoFragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    gizmoFragInfo.codeSize = sizeof(kGizmoFragSpv);
    gizmoFragInfo.pCode = kGizmoFragSpv;
    FS_VK_CHECK(vkCreateShaderModule(device_, &gizmoFragInfo, nullptr, &gizmoFragShader_),
                "vkCreateShaderModule(gizmo_frag)");

    VkPushConstantRange gizmoRange{};
    gizmoRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    gizmoRange.offset = 0;
    gizmoRange.size = sizeof(GizmoPush);

    // setLayoutCount = 0, exactly as the grid's is and for the same reason: a
    // handle consults no sampler, so it must not be able to reach the MatCap
    // even by accident.
    VkPipelineLayoutCreateInfo gizmoLayoutInfo{};
    gizmoLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    gizmoLayoutInfo.setLayoutCount = 0;
    gizmoLayoutInfo.pushConstantRangeCount = 1;
    gizmoLayoutInfo.pPushConstantRanges = &gizmoRange;
    FS_VK_CHECK(vkCreatePipelineLayout(device_, &gizmoLayoutInfo, nullptr, &gizmoPipelineLayout_),
                "vkCreatePipelineLayout(gizmo)");

    FS_LOGI("Shader modules created (SPIR-V: vert %zu bytes, frag %zu bytes, push %zu bytes; "
            "grid vert %zu bytes, grid frag %zu bytes, grid push %zu bytes; "
            "gizmo vert %zu bytes, gizmo frag %zu bytes, gizmo push %zu bytes)",
            sizeof(kSurfaceVertSpv), sizeof(kSurfaceFragSpv), sizeof(SurfacePush),
            sizeof(kGridVertSpv), sizeof(kGridFragSpv), sizeof(GridPush),
            sizeof(kGizmoVertSpv), sizeof(kGizmoFragSpv), sizeof(GizmoPush));
    return true;
}

// ---------------------------------------------------------------------------
// World reference grid
// ---------------------------------------------------------------------------

bool Renderer::createGridResources() {
    // Generated on the stack from a compile-time constant count, uploaded, and
    // then forgotten. There is no cache to invalidate and no revision to
    // follow, because there is no input that can change: the grid's spacing,
    // extent and tiers are constants of forgeshape_grid.h.
    GridVertex vertices[kGridVertexCount];
    const int written = generateGridVertices(vertices, kGridVertexCount);
    if (written != kGridVertexCount) {
        FS_FAIL("grid_generate_incomplete");
        return false;
    }
    const VkDeviceSize bytes = sizeof(GridVertex) * static_cast<VkDeviceSize>(written);

    if (!createBuffer(bytes,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &gridVertexBuffer_,
                      &gridVertexMemory_)) {
        FS_FAIL("grid_vertex_buffer");
        return false;
    }

    // Shares the mesh path's staging buffer and upload command buffer, which is
    // why this runs after createMeshUploadObjects. It is the same kind of
    // transient scratch used inside one upload, and a second copy of it for
    // 2.6 KiB written once would be pure duplication.
    if (!ensureStagingCapacity(bytes)) {
        return false;
    }
    void* mapped = nullptr;
    FS_VK_CHECK(vkMapMemory(device_, stagingMemory_, 0, bytes, 0, &mapped),
                "vkMapMemory(grid_staging)");
    std::memcpy(mapped, vertices, static_cast<size_t>(bytes));
    vkUnmapMemory(device_, stagingMemory_);

    FS_VK_CHECK(vkResetCommandBuffer(uploadCommandBuffer_, 0), "vkResetCommandBuffer(grid_upload)");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    FS_VK_CHECK(vkBeginCommandBuffer(uploadCommandBuffer_, &begin),
                "vkBeginCommandBuffer(grid_upload)");

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = bytes;
    vkCmdCopyBuffer(uploadCommandBuffer_, stagingBuffer_, gridVertexBuffer_, 1, &copy);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vkCmdPipelineBarrier(uploadCommandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &barrier, 0, nullptr, 0,
                         nullptr);
    FS_VK_CHECK(vkEndCommandBuffer(uploadCommandBuffer_), "vkEndCommandBuffer(grid_upload)");

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &uploadCommandBuffer_;
    FS_VK_CHECK(vkResetFences(device_, 1, &uploadFence_), "vkResetFences(grid_upload)");
    FS_VK_CHECK(vkQueueSubmit(graphicsQueue_, 1, &submit, uploadFence_),
                "vkQueueSubmit(grid_upload)");
    FS_VK_CHECK(vkWaitForFences(device_, 1, &uploadFence_, VK_TRUE, UINT64_MAX),
                "vkWaitForFences(grid_upload)");

    gridVertexCount_ = static_cast<uint32_t>(written);
    // Logged ONCE per device, deliberately not per frame and deliberately not
    // per toggle. A second occurrence of this line in a session log is direct
    // evidence that something re-uploaded a constant.
    FS_LOGI("FORGESHAPE_GRID_UPLOAD_OK lines=%d vertices=%u bytes=%llu spacing=%.2f "
            "major=%d extent=%.1f",
            kGridLineCount, gridVertexCount_, (unsigned long long)bytes,
            kGridMinorSpacingMeters, kGridMajorEveryNMinor, kGridHalfExtentMeters);
    return true;
}

void Renderer::destroyGridResources() {
    if (device_ == VK_NULL_HANDLE) return;
    if (gridVertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, gridVertexBuffer_, nullptr);
        gridVertexBuffer_ = VK_NULL_HANDLE;
    }
    if (gridVertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, gridVertexMemory_, nullptr);
        gridVertexMemory_ = VK_NULL_HANDLE;
    }
    gridVertexCount_ = 0;
}

// ---------------------------------------------------------------------------
// Construction Move / Rotate gizmo
// ---------------------------------------------------------------------------

bool Renderer::createGizmoResources() {
    // Generated on the stack from a compile-time constant count, uploaded, and
    // then forgotten — exactly like the grid. There is no cache to invalidate
    // and no revision to follow, because the geometry is authored in a canonical
    // reference-unit space that nothing can move: where the gizmo IS and how big
    // it looks are a matrix, computed per frame, per drag, for free.
    GizmoVertex vertices[kGizmoVertexCount];
    const int written = generateGizmoVertices(vertices, kGizmoVertexCount);
    if (written != kGizmoVertexCount) {
        FS_FAIL("gizmo_generate_incomplete");
        return false;
    }
    const VkDeviceSize bytes = sizeof(GizmoVertex) * static_cast<VkDeviceSize>(written);

    if (!createBuffer(bytes,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &gizmoVertexBuffer_,
                      &gizmoVertexMemory_)) {
        FS_FAIL("gizmo_vertex_buffer");
        return false;
    }

    // Shares the mesh path's staging buffer and upload command buffer, which is
    // why this runs after createMeshUploadObjects — the same transient scratch
    // the grid borrows, for the same one-time few-kilobyte copy.
    if (!ensureStagingCapacity(bytes)) {
        return false;
    }
    void* mapped = nullptr;
    FS_VK_CHECK(vkMapMemory(device_, stagingMemory_, 0, bytes, 0, &mapped),
                "vkMapMemory(gizmo_staging)");
    std::memcpy(mapped, vertices, static_cast<size_t>(bytes));
    vkUnmapMemory(device_, stagingMemory_);

    FS_VK_CHECK(vkResetCommandBuffer(uploadCommandBuffer_, 0), "vkResetCommandBuffer(gizmo_upload)");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    FS_VK_CHECK(vkBeginCommandBuffer(uploadCommandBuffer_, &begin),
                "vkBeginCommandBuffer(gizmo_upload)");

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = bytes;
    vkCmdCopyBuffer(uploadCommandBuffer_, stagingBuffer_, gizmoVertexBuffer_, 1, &copy);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vkCmdPipelineBarrier(uploadCommandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &barrier, 0, nullptr, 0,
                         nullptr);
    FS_VK_CHECK(vkEndCommandBuffer(uploadCommandBuffer_), "vkEndCommandBuffer(gizmo_upload)");

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &uploadCommandBuffer_;
    FS_VK_CHECK(vkResetFences(device_, 1, &uploadFence_), "vkResetFences(gizmo_upload)");
    FS_VK_CHECK(vkQueueSubmit(graphicsQueue_, 1, &submit, uploadFence_),
                "vkQueueSubmit(gizmo_upload)");
    FS_VK_CHECK(vkWaitForFences(device_, 1, &uploadFence_, VK_TRUE, UINT64_MAX),
                "vkWaitForFences(gizmo_upload)");

    gizmoVertexCount_ = static_cast<uint32_t>(written);
    // Logged ONCE per device. A second occurrence of this line in a session log
    // is direct evidence that a drag, an orbit or a mode switch re-uploaded
    // geometry it must never touch.
    FS_LOGI("FORGESHAPE_GIZMO_UPLOAD_OK vertices=%u bytes=%llu move=[%d,%d) rotate=[%d,%d) "
            "scale=[%d,%d)",
            gizmoVertexCount_, (unsigned long long)bytes, kGizmoMoveFirstVertex,
            kGizmoMoveFirstVertex + kGizmoMoveVertexCount, kGizmoRotateFirstVertex,
            kGizmoRotateFirstVertex + kGizmoRotateVertexCount, kGizmoScaleFirstVertex,
            kGizmoScaleFirstVertex + kGizmoScaleVertexCount);
    return true;
}

void Renderer::destroyGizmoResources() {
    if (device_ == VK_NULL_HANDLE) return;
    if (gizmoVertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, gizmoVertexBuffer_, nullptr);
        gizmoVertexBuffer_ = VK_NULL_HANDLE;
    }
    if (gizmoVertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, gizmoVertexMemory_, nullptr);
        gizmoVertexMemory_ = VK_NULL_HANDLE;
    }
    gizmoVertexCount_ = 0;
}

// ---------------------------------------------------------------------------
// Sketch overlay
// ---------------------------------------------------------------------------

void Renderer::destroySketchOverlayResources() {
    if (device_ == VK_NULL_HANDLE) return;
    if (sketchVertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, sketchVertexBuffer_, nullptr);
        sketchVertexBuffer_ = VK_NULL_HANDLE;
    }
    if (sketchVertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, sketchVertexMemory_, nullptr);
        sketchVertexMemory_ = VK_NULL_HANDLE;
    }
    sketchVertexCapacityBytes_ = 0;
    sketchVertexCount_ = 0;
    sketchUploadedOnce_ = false;
    sketchRanges_.clear();
}

bool Renderer::syncSketchOverlay() {
    if (device_ == VK_NULL_HANDLE) {
        return true;
    }
    if (!sketchOverlay_ || sketchOverlay_->vertices.empty()) {
        // No sketch: nothing is drawn and nothing is transferred. The buffer
        // is kept for the next sketch rather than freed per cancel.
        sketchVertexCount_ = 0;
        sketchRanges_.clear();
        if (sketchOverlay_) {
            sketchUploadedRevision_ = sketchOverlay_->revision;
            sketchUploadedOnce_ = true;
        }
        return true;
    }
    const SketchOverlay& overlay = *sketchOverlay_;
    if (sketchUploadedOnce_ && overlay.revision == sketchUploadedRevision_
        && sketchVertexCount_ == overlay.vertices.size()) {
        return true;  // the frame draws what the device already holds
    }
    if (overlay.vertices.size() > kMaxSketchOverlayVertices) {
        FS_LOGE("FORGESHAPE_SKETCH_OVERLAY_FAIL:too_large vertices=%u",
                (unsigned)overlay.vertices.size());
        sketchVertexCount_ = 0;
        return false;
    }
    const VkDeviceSize bytes =
        sizeof(GizmoVertex) * static_cast<VkDeviceSize>(overlay.vertices.size());

    // No in-flight frame may still be reading the buffer about to be written
    // or retired -- the same rule every mesh upload follows.
    if (!waitForMeshBuffersIdle()) {
        return false;
    }
    if (sketchVertexBuffer_ == VK_NULL_HANDLE || bytes > sketchVertexCapacityBytes_) {
        uint64_t grown = 0;
        if (!growCapacityBytes(static_cast<uint64_t>(sketchVertexCapacityBytes_),
                               static_cast<uint64_t>(bytes), &grown)) {
            FS_FAIL("sketch_overlay_capacity_overflow");
            return false;
        }
        if (sketchVertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, sketchVertexBuffer_, nullptr);
        if (sketchVertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, sketchVertexMemory_, nullptr);
        sketchVertexBuffer_ = VK_NULL_HANDLE;
        sketchVertexMemory_ = VK_NULL_HANDLE;
        sketchVertexCapacityBytes_ = 0;
        if (!createBuffer(static_cast<VkDeviceSize>(grown),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &sketchVertexBuffer_,
                          &sketchVertexMemory_)) {
            FS_FAIL("sketch_overlay_vertex_buffer");
            return false;
        }
        sketchVertexCapacityBytes_ = static_cast<VkDeviceSize>(grown);
    }

    // The same borrowed staging path the gizmo and every mesh use: host
    // staging, one fenced copy, one barrier to vertex input.
    if (!ensureStagingCapacity(bytes)) {
        return false;
    }
    void* mapped = nullptr;
    FS_VK_CHECK(vkMapMemory(device_, stagingMemory_, 0, bytes, 0, &mapped),
                "vkMapMemory(sketch_staging)");
    std::memcpy(mapped, overlay.vertices.data(), static_cast<size_t>(bytes));
    vkUnmapMemory(device_, stagingMemory_);

    FS_VK_CHECK(vkResetCommandBuffer(uploadCommandBuffer_, 0), "vkResetCommandBuffer(sketch_upload)");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    FS_VK_CHECK(vkBeginCommandBuffer(uploadCommandBuffer_, &begin),
                "vkBeginCommandBuffer(sketch_upload)");
    VkBufferCopy copy{};
    copy.size = bytes;
    vkCmdCopyBuffer(uploadCommandBuffer_, stagingBuffer_, sketchVertexBuffer_, 1, &copy);
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vkCmdPipelineBarrier(uploadCommandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &barrier, 0, nullptr, 0,
                         nullptr);
    FS_VK_CHECK(vkEndCommandBuffer(uploadCommandBuffer_), "vkEndCommandBuffer(sketch_upload)");
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &uploadCommandBuffer_;
    FS_VK_CHECK(vkResetFences(device_, 1, &uploadFence_), "vkResetFences(sketch_upload)");
    FS_VK_CHECK(vkQueueSubmit(graphicsQueue_, 1, &submit, uploadFence_),
                "vkQueueSubmit(sketch_upload)");
    FS_VK_CHECK(vkWaitForFences(device_, 1, &uploadFence_, VK_TRUE, UINT64_MAX),
                "vkWaitForFences(sketch_upload)");

    sketchVertexCount_ = static_cast<uint32_t>(overlay.vertices.size());
    sketchRanges_ = overlay.ranges;
    sketchUploadedRevision_ = overlay.revision;
    sketchUploadedOnce_ = true;
    return true;
}

void Renderer::recordSketchOverlayDraw(VkCommandBuffer cmd) {
    if (sketchVertexCount_ == 0 || sketchVertexBuffer_ == VK_NULL_HANDLE ||
        gizmoPipeline_ == VK_NULL_HANDLE || sketchRanges_.empty()) {
        return;
    }
    const Mat4 viewProj = mat4Multiply(camera_.proj, camera_.view);
    if (!mat4Finite(viewProj)) {
        return;
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gizmoPipeline_);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &sketchVertexBuffer_, &offset);

    for (const SketchOverlayRange& range : sketchRanges_) {
        if (range.vertexCount == 0 || range.firstVertex + range.vertexCount > sketchVertexCount_) {
            continue;
        }
        // The vertices are WORLD positions, so the model is the identity and
        // the gizmo push carries the view-projection alone. The packed scalars
        // then decide the weight of this range -- see gizmo.vert for what each
        // slot means.
        GizmoPush push{};
        std::memcpy(push.mvp, viewProj.m, sizeof(push.mvp));
        gizmoAxisColor(display_.background, GizmoAxis::X, push.axisXColor);
        gizmoAxisColor(display_.background, GizmoAxis::Y, push.axisYColor);
        gizmoAxisColor(display_.background, GizmoAxis::Z, push.axisZColor);
        gizmoHighlightColor(display_.background, push.highlight);
        const float neutral = gizmoNeutralLevel(display_.background);
        // Nothing in the overlay is dimmed relative to its neighbours: the
        // held-handle mechanism is reused only to pick the highlight colour
        // for an emphasised line (handle tag 1), never to fade the rest.
        push.axisZColor[3] = 1.0f;
        push.axisYColor[3] = 1.0f;
        switch (range.style) {
            case SketchOverlayStyle::GridMinor:
                push.axisXColor[3] = neutral;
                push.highlight[3] = kGizmoAxisAlpha * 0.22f;
                break;
            case SketchOverlayStyle::GridMajor:
                push.axisXColor[3] = neutral;
                push.highlight[3] = kGizmoAxisAlpha * 0.45f;
                break;
            case SketchOverlayStyle::Axes:
                push.axisXColor[3] = neutral;
                push.highlight[3] = kGizmoAxisAlpha * 0.9f;
                break;
            case SketchOverlayStyle::Entities:
                // Entities read at full weight in a level that stands off the
                // grid: the neutral pulled toward the highlight's own level.
                push.axisXColor[3] = neutral * 0.35f + push.highlight[0] * 0.65f;
                push.highlight[3] = kGizmoAxisAlpha;
                break;
        }
        vkCmdPushConstants(cmd, gizmoPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GizmoPush), &push);
        vkCmdDraw(cmd, range.vertexCount, 1, range.firstVertex, 0);
    }
}

// ---------------------------------------------------------------------------
// MatCap image and the one descriptor set
// ---------------------------------------------------------------------------

bool Renderer::createDescriptorResources() {
    // Exactly one binding exists in the whole renderer: the MatCap sampler.
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    FS_VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_),
                "vkCreateDescriptorSetLayout(matcap)");

    // One set, allocated once, never updated again after the MatCap is
    // uploaded: the image is immutable for the life of the device, so there is
    // no per-frame descriptor traffic and no need for per-frame-in-flight
    // copies.
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    FS_VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_),
                "vkCreateDescriptorPool(matcap)");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout_;
    FS_VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet_),
                "vkAllocateDescriptorSets(matcap)");
    return true;
}

bool Renderer::createMatCapResources() {
    // The asset is COMPUTED, not loaded: there is no image file, no decoder and
    // no third-party dependency. See forgeshape_matcap.h for provenance.
    std::vector<uint8_t> texels;
    generateMatCap(&texels);
    if (texels.size() != kMatCapByteSize) {
        FS_FAIL("matcap_generation_size");
        return false;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {kMatCapSize, kMatCapSize, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    FS_VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &matcapImage_), "vkCreateImage(matcap)");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, matcapImage_, &requirements);
    uint32_t memoryType = 0;
    if (!findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        &memoryType)) {
        FS_FAIL("matcap_memory_type");
        return false;
    }
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = requirements.size;
    alloc.memoryTypeIndex = memoryType;
    FS_VK_CHECK(vkAllocateMemory(device_, &alloc, nullptr, &matcapMemory_), "vkAllocateMemory(matcap)");
    FS_VK_CHECK(vkBindImageMemory(device_, matcapImage_, matcapMemory_, 0), "vkBindImageMemory(matcap)");

    // Reuse the mesh path's staging buffer and upload command buffer: this runs
    // once, at device creation, long before any mesh upload, so there is no
    // contention and no second staging allocation.
    if (!ensureStagingCapacity(kMatCapByteSize)) {
        return false;
    }
    void* mapped = nullptr;
    FS_VK_CHECK(vkMapMemory(device_, stagingMemory_, 0, kMatCapByteSize, 0, &mapped),
                "vkMapMemory(matcap_staging)");
    std::memcpy(mapped, texels.data(), kMatCapByteSize);
    vkUnmapMemory(device_, stagingMemory_);

    FS_VK_CHECK(vkResetCommandBuffer(uploadCommandBuffer_, 0), "vkResetCommandBuffer(matcap)");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    FS_VK_CHECK(vkBeginCommandBuffer(uploadCommandBuffer_, &begin), "vkBeginCommandBuffer(matcap)");

    // UNDEFINED -> TRANSFER_DST_OPTIMAL, copy, then -> SHADER_READ_ONLY_OPTIMAL.
    // The image never changes again, so this is the only layout transition it
    // will ever undergo.
    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = matcapImage_;
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(uploadCommandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageOffset = {0, 0, 0};
    copy.imageExtent = {kMatCapSize, kMatCapSize, 1};
    vkCmdCopyBufferToImage(uploadCommandBuffer_, stagingBuffer_, matcapImage_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier toSampled = toTransfer;
    toSampled.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toSampled.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSampled.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toSampled.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(uploadCommandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toSampled);

    FS_VK_CHECK(vkEndCommandBuffer(uploadCommandBuffer_), "vkEndCommandBuffer(matcap)");

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &uploadCommandBuffer_;
    FS_VK_CHECK(vkResetFences(device_, 1, &uploadFence_), "vkResetFences(matcap)");
    FS_VK_CHECK(vkQueueSubmit(graphicsQueue_, 1, &submit, uploadFence_), "vkQueueSubmit(matcap)");
    FS_VK_CHECK(vkWaitForFences(device_, 1, &uploadFence_, VK_TRUE, UINT64_MAX),
                "vkWaitForFences(matcap)");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = matcapImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    FS_VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &matcapImageView_),
                "vkCreateImageView(matcap)");

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    // CLAMP_TO_EDGE matters: a normal exactly on the silhouette lands on the
    // rim of the disc, and wrapping there would sample the opposite side of the
    // sphere and put a bright seam around every object.
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;  // a MatCap lookup has no surface gradient to filter
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.maxLod = 0.0f;
    FS_VK_CHECK(vkCreateSampler(device_, &samplerInfo, nullptr, &matcapSampler_),
                "vkCreateSampler(matcap)");

    VkDescriptorImageInfo imageBinding{};
    imageBinding.sampler = matcapSampler_;
    imageBinding.imageView = matcapImageView_;
    imageBinding.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageBinding;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    FS_LOGI("FORGESHAPE_MATCAP_READY:%ux%u:%u bytes (generated, ForgeShape-owned)", kMatCapSize,
            kMatCapSize, kMatCapByteSize);
    return true;
}

void Renderer::destroyMatCapResources() {
    if (device_ == VK_NULL_HANDLE) return;

    if (matcapSampler_ != VK_NULL_HANDLE) vkDestroySampler(device_, matcapSampler_, nullptr);
    if (matcapImageView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, matcapImageView_, nullptr);
    if (matcapImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, matcapImage_, nullptr);
    if (matcapMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, matcapMemory_, nullptr);
    // Freeing the pool frees the set allocated from it; the set must not be
    // freed separately.
    if (descriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
    }

    matcapSampler_ = VK_NULL_HANDLE;
    matcapImageView_ = VK_NULL_HANDLE;
    matcapImage_ = VK_NULL_HANDLE;
    matcapMemory_ = VK_NULL_HANDLE;
    descriptorPool_ = VK_NULL_HANDLE;
    descriptorSet_ = VK_NULL_HANDLE;
    descriptorSetLayout_ = VK_NULL_HANDLE;
}

bool Renderer::createSyncObjects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        FS_VK_CHECK(vkCreateSemaphore(device_, &semInfo, nullptr, &imageAvailable_[i]), "vkCreateSemaphore");
        FS_VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]), "vkCreateFence");
    }
    return true;
}

// ---------------------------------------------------------------------------
// Swapchain-dependent objects
// ---------------------------------------------------------------------------

VkFormat Renderer::selectDepthFormat() const {
    const VkFormat candidates[] = {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D16_UNORM,
    };
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

bool Renderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    FS_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps),
                "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX || extent.height == UINT32_MAX) {
        extent.width = static_cast<uint32_t>(ANativeWindow_getWidth(window_));
        extent.height = static_cast<uint32_t>(ANativeWindow_getHeight(window_));
    }

    // ------------------------------------------------------------------------
    // THE orientation convention (there is exactly one, see ARCHITECTURE.md).
    //
    // ForgeShape always renders in Android window orientation: the swapchain
    // image is the size of the window the user sees, and the presentation
    // engine -- not this renderer -- performs any display rotation. That keeps
    // one coordinate space from the SurfaceView through the camera viewport and
    // projection aspect to the swapchain image and to picking.
    //
    // The alternative, pre-rotation, requires imageExtent in the display's
    // PRE-transform (panel) space, which is the transpose of the window when the
    // transform is 90 or 270 degrees. Declaring preTransform = currentTransform
    // while passing the window-space extent -- what this renderer did before --
    // makes SurfaceFlinger rotate a 2400x1080 buffer into a 1080x2400 layout and
    // then stretch it back to the 2400x1080 window, an anisotropic scale of
    // (2400/1080, 1080/2400). That was the rotated-landscape defect: the extent
    // space, not the projection, was the inconsistent convention.
    //
    // Identity is a supported transform on every Android presentation engine
    // this targets, and requesting it costs one compositor rotation, which is
    // what every non-pre-rotated Android application already pays.
    VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    if ((caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0) {
        // No identity available: fall back to the engine's own transform rather
        // than fail to present. The extent stays in the space that transform
        // maps FROM, so the image is never stretched; a 90/270 surface would
        // then be presented rotated, which is visible and diagnosable from
        // FORGESHAPE_SURFACE_CONFIG below.
        preTransform = caps.currentTransform;
        if (preTransform & (VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR |
                            VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR |
                            VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR |
                            VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR)) {
            std::swap(extent.width, extent.height);
        }
    }

    if (extent.width == 0 || extent.height == 0) {
        FS_LOGI("Swapchain skipped: zero-sized surface");
        return false;
    }

    uint32_t formatCount = 0;
    FS_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr),
                "vkGetPhysicalDeviceSurfaceFormatsKHR");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    FS_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data()),
                "vkGetPhysicalDeviceSurfaceFormatsKHR");
    if (formats.empty()) {
        FS_FAIL("no_surface_formats");
        return false;
    }

    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM || f.format == VK_FORMAT_B8G8R8A8_UNORM) {
            chosen = f;
            break;
        }
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface_;
    info.minImageCount = imageCount;
    info.imageFormat = chosen.format;
    info.imageColorSpace = chosen.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform = preTransform;
    info.compositeAlpha = compositeAlpha;
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR;  // always supported
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    uint32_t families[2] = {graphicsQueueFamily_, presentQueueFamily_};
    if (graphicsQueueFamily_ != presentQueueFamily_) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = families;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    // Durable, non-per-frame orientation diagnostic: one line per swapchain
    // creation. Everything needed to audit the orientation convention end to
    // end -- Android window size, what the presentation engine reports, and
    // what this renderer chose -- is in this single token, so a rotation
    // regression never needs temporary instrumentation again.
    FS_LOGI("FORGESHAPE_SURFACE_CONFIG window=%dx%d currentExtent=%ux%u "
            "currentTransform=0x%x supportedTransforms=0x%x chosenExtent=%ux%u preTransform=0x%x",
            ANativeWindow_getWidth(window_), ANativeWindow_getHeight(window_),
            caps.currentExtent.width, caps.currentExtent.height,
            static_cast<unsigned>(caps.currentTransform),
            static_cast<unsigned>(caps.supportedTransforms), extent.width, extent.height,
            static_cast<unsigned>(info.preTransform));

    FS_VK_CHECK(vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_), "vkCreateSwapchainKHR");
    swapchainFormat_ = chosen.format;
    swapchainExtent_ = extent;
    // Deliberately rendering unrotated into a rotated surface makes the
    // presentation engine report VK_SUBOPTIMAL_KHR forever. That is the chosen
    // convention working as intended, not a stale swapchain, so the frame loop
    // must not treat it as a rebuild request -- doing so rebuilds the swapchain
    // every single frame for as long as the device is rotated.
    expectSuboptimal_ = (preTransform != caps.currentTransform);

    uint32_t actualCount = 0;
    FS_VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, nullptr), "vkGetSwapchainImagesKHR");
    swapchainImages_.resize(actualCount);
    FS_VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, swapchainImages_.data()),
                "vkGetSwapchainImagesKHR");

    swapchainImageViews_.resize(actualCount);
    for (uint32_t i = 0; i < actualCount; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        FS_VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &swapchainImageViews_[i]),
                    "vkCreateImageView(swapchain)");
    }

    FS_LOGI("Swapchain created: format=%d extent=%ux%u images=%u presentMode=FIFO",
            (int)swapchainFormat_, swapchainExtent_.width, swapchainExtent_.height, actualCount);
    return true;
}

bool Renderer::createDepthResources() {
    depthFormat_ = selectDepthFormat();
    if (depthFormat_ == VK_FORMAT_UNDEFINED) {
        FS_FAIL("no_supported_depth_format");
        return false;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = depthFormat_;
    imageInfo.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    FS_VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &depthImage_), "vkCreateImage(depth)");

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device_, depthImage_, &req);
    uint32_t memoryType = 0;
    if (!findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryType)) {
        FS_FAIL("no_device_local_memory_for_depth");
        return false;
    }
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = req.size;
    allocInfo.memoryTypeIndex = memoryType;
    FS_VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &depthMemory_), "vkAllocateMemory(depth)");
    FS_VK_CHECK(vkBindImageMemory(device_, depthImage_, depthMemory_, 0), "vkBindImageMemory(depth)");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    FS_VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &depthImageView_), "vkCreateImageView(depth)");

    FS_LOGI("Depth resources created: format=%d extent=%ux%u",
            (int)depthFormat_, swapchainExtent_.width, swapchainExtent_.height);
    return true;
}

bool Renderer::createRenderPass() {
    VkAttachmentDescription attachments[2]{};
    attachments[0].format = swapchainFormat_;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = depthFormat_;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    FS_VK_CHECK(vkCreateRenderPass(device_, &info, nullptr, &renderPass_), "vkCreateRenderPass");
    FS_LOGI("Render pass created (color + depth)");
    return true;
}

bool Renderer::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        VkImageView views[2] = {swapchainImageViews_[i], depthImageView_};
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPass_;
        info.attachmentCount = 2;
        info.pAttachments = views;
        info.width = swapchainExtent_.width;
        info.height = swapchainExtent_.height;
        info.layers = 1;
        FS_VK_CHECK(vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]), "vkCreateFramebuffer");
    }
    return true;
}

bool Renderer::createPipeline() {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertShader_;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragShader_;
    stages[1].pName = "main";

    // The vertex format is RenderVertex, not MeshVertex: what the GPU holds is
    // the derived render mesh, whose extra normal channel is the whole point of
    // the derivation. MeshVertex remains the authoritative CPU format and is
    // never handed to Vulkan directly any more.
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(RenderVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[3]{};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(RenderVertex, position);
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(RenderVertex, normal);
    attributes[2].location = 2;
    attributes[2].binding = 0;
    attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[2].offset = offsetof(RenderVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{0.0f, 0.0f, static_cast<float>(swapchainExtent_.width),
                        static_cast<float>(swapchainExtent_.height), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, swapchainExtent_};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    // Canonical ForgeShape convention (forgeshape_picking.h): triangles are
    // counter-clockwise seen from outside the solid in right-handed world space.
    // CPU picking uses the same convention, so what is pickable is exactly what
    // is drawn — and that agreement is the point of stating it in one place.
    //
    // COUNTER_CLOCKWISE, and the reason is worth writing down because the
    // opposite was shipped and looked plausible. Vulkan classifies a triangle by
    // the sign of its signed area in framebuffer coordinates, and the
    // projection's Y flip (mat4Perspective's negated m[5]) is already what
    // carries a view-space counter-clockwise triangle to the sign Vulkan calls
    // COUNTER_CLOCKWISE. Naming CLOCKWISE here "to account for the Y flip"
    // double-counts it: the flip is in the matrix, not in this enum.
    //
    // Getting this backwards does not blank the viewport, which is what made it
    // survive review — a closed solid still fills exactly the same silhouette.
    // It quietly draws the FAR walls instead of the near ones, so every convex
    // primitive renders as the inside of itself and reads as a concave interior
    // corner. Measured on the default box: with BACK/CLOCKWISE the three visible
    // faces were -X, -Z and -Y at luminance 0.387 / 0.333 / 0.264; the three
    // that should be visible are +Y, +Z and +X at 0.832 / 0.559 / 0.310.
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

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
    info.layout = pipelineLayout_;
    info.renderPass = renderPass_;
    info.subpass = 0;

    FS_VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_),
                "vkCreateGraphicsPipelines");
    FS_LOGI("Graphics pipeline created (depth test LESS, depth write on, "
            "cull BACK, frontFace COUNTER_CLOCKWISE)");
    return true;
}

bool Renderer::createGridPipeline() {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = gridVertShader_;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = gridFragShader_;
    stages[1].pName = "main";

    // GridVertex, not RenderVertex: a grid line has a position and a TIER, and
    // no normal and no colour. Sharing the surface pipeline's vertex format
    // would mean carrying 24 dead bytes per vertex to describe a floor.
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(GridVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[2]{};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(GridVertex, position);
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32_SFLOAT;
    attributes[1].offset = offsetof(GridVertex, tier);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkViewport viewport{0.0f, 0.0f, static_cast<float>(swapchainExtent_.width),
                        static_cast<float>(swapchainExtent_.height), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, swapchainExtent_};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // FILL, even though every primitive here is a line.
    //
    // polygonMode describes how POLYGONS are rasterized and says nothing about
    // a LINE_LIST, so the only thing naming VK_POLYGON_MODE_LINE would achieve
    // is requiring the `fillModeNonSolid` device feature — which ForgeShape does
    // not request and must not start requesting to draw a floor.
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    // No culling: a line has no facing, and a floor is looked at from above and
    // from below equally often.
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    // 1.0 exactly. Anything wider needs the `wideLines` device feature, which
    // ForgeShape does not request and must not start requesting for a grid.
    raster.lineWidth = 1.0f;
    // depthBiasEnable is deliberately LEFT OFF, and the coplanar case is
    // settled in the vertex shader instead. Vulkan's depth bias is defined for
    // POLYGON fragments; a line primitive is not one, so enabling it here would
    // read as the fix for the Plane-at-y=0 case while doing nothing at all.
    // See kGridDepthNudge and shaders/grid.vert.

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    // Tested, so the model occludes the grid and the grid never punches through
    // it. NOT written, so a translucent line leaves the depth buffer exactly as
    // the bodies left it: the grid contributes nothing that a later draw could
    // be occluded by, which is what keeps it a reference rather than geometry.
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;

    // The one blended pipeline in ForgeShape. The grid's whole visual contract
    // is "present but never competing", and that is an alpha, not a colour —
    // see the palette rules in forgeshape_grid.cpp.
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    // The ALPHA channel is deliberately left alone: KEEP what is already in the
    // attachment (1.0, from the render pass clear) rather than replacing it
    // with the line's blend weight.
    //
    // This is not a detail. The swapchain image is what the Android compositor
    // presents, and it honours that alpha — so writing a line's own 0.14 into
    // the destination would punch the viewport 86 % transparent along every
    // grid line and composite the window background through it. The colour
    // channels are blended, which is where the subtlety belongs; the surface
    // stays opaque.
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

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
    info.layout = gridPipelineLayout_;
    info.renderPass = renderPass_;
    info.subpass = 0;

    FS_VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &gridPipeline_),
                "vkCreateGraphicsPipelines(grid)");
    FS_LOGI("Grid pipeline created (LINE_LIST, polygonMode FILL, cull NONE, depth test LESS, "
            "depth write OFF, vertex depth nudge %.5f, alpha blend, no descriptor set)",
            kGridDepthNudge);
    return true;
}

bool Renderer::createGizmoPipeline() {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = gizmoVertShader_;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = gizmoFragShader_;
    stages[1].pName = "main";

    // GizmoVertex: a position, a COLOUR TAG and a HANDLE code — and no normal
    // and no colour. The two tags are separate because a plane handle borrows
    // its hue from the axis perpendicular to it, so holding that axis must not
    // also light the plane.
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(GizmoVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[3]{};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(GizmoVertex, position);
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32_SFLOAT;
    attributes[1].offset = offsetof(GizmoVertex, axis);
    attributes[2].location = 2;
    attributes[2].binding = 0;
    attributes[2].format = VK_FORMAT_R32_SFLOAT;
    attributes[2].offset = offsetof(GizmoVertex, handle);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkViewport viewport{0.0f, 0.0f, static_cast<float>(swapchainExtent_.width),
                        static_cast<float>(swapchainExtent_.height), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, swapchainExtent_};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // FILL and lineWidth 1.0, for exactly the reasons spelled out on the grid
    // pipeline: polygonMode says nothing about a LINE_LIST, and anything wider
    // than 1.0 would require the `wideLines` device feature ForgeShape does not
    // request and must not start requesting to draw a handle.
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    // Depth test OFF, and this is the one place the gizmo differs from the grid
    // on purpose. A handle is a CONTROL: it has to be visible and grabbable even
    // where it passes through the body it moves, and a pivot marker buried
    // inside a solid would be a control the user can see the effect of and never
    // reach. That is the "always on top" the tool contract permits.
    depthStencil.depthTestEnable = VK_FALSE;
    // Depth write OFF as well, so the buffer is left exactly as the bodies and
    // the grid left it. The gizmo contributes nothing a later draw could be
    // occluded by, which is what keeps model depth semantics intact — see
    // recordGizmoDraw for where it sits in the pass.
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // Straight (non-premultiplied) alpha, matching what shaders/gizmo.frag
    // writes, so a dimmed axis is the same colour drawn more faintly rather than
    // a different colour.
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

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
    info.layout = gizmoPipelineLayout_;
    info.renderPass = renderPass_;
    info.subpass = 0;

    FS_VK_CHECK(
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &gizmoPipeline_),
        "vkCreateGraphicsPipelines(gizmo)");
    FS_LOGI("Gizmo pipeline created (LINE_LIST, cull NONE, depth test OFF, depth write OFF, "
            "alpha blend, no descriptor set)");
    return true;
}

bool Renderer::createCommandBuffers() {
    commandBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = commandPool_;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    FS_VK_CHECK(vkAllocateCommandBuffers(device_, &info, commandBuffers_.data()), "vkAllocateCommandBuffers");

    renderFinished_.resize(framebuffers_.size(), VK_NULL_HANDLE);
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (auto& sem : renderFinished_) {
        FS_VK_CHECK(vkCreateSemaphore(device_, &semInfo, nullptr, &sem), "vkCreateSemaphore(renderFinished)");
    }
    imagesInFlight_.assign(framebuffers_.size(), VK_NULL_HANDLE);
    return true;
}

bool Renderer::createSwapchainDependents() {
    if (!createSwapchain()) return false;
    if (!createDepthResources()) return false;
    if (!createRenderPass()) return false;
    if (!createFramebuffers()) return false;
    if (!createPipeline()) return false;
    if (!createGridPipeline()) return false;
    if (!createGizmoPipeline()) return false;
    if (!createCommandBuffers()) return false;
    needsSwapchainRebuild_ = false;
    return true;
}

void Renderer::destroySwapchainDependents() {
    if (device_ == VK_NULL_HANDLE) return;

    for (auto& sem : renderFinished_) {
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(device_, sem, nullptr);
    }
    renderFinished_.clear();
    imagesInFlight_.clear();

    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(device_, commandPool_, static_cast<uint32_t>(commandBuffers_.size()),
                             commandBuffers_.data());
        commandBuffers_.clear();
    }
    if (gizmoPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, gizmoPipeline_, nullptr);
        gizmoPipeline_ = VK_NULL_HANDLE;
    }
    if (gridPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, gridPipeline_, nullptr);
        gridPipeline_ = VK_NULL_HANDLE;
    }
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    for (auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, nullptr);
    }
    framebuffers_.clear();
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depthMemory_, nullptr);
        depthMemory_ = VK_NULL_HANDLE;
    }
    for (auto view : swapchainImageViews_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    currentFrame_ = 0;
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

bool Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    FS_VK_CHECK(vkResetCommandBuffer(cmd, 0), "vkResetCommandBuffer");

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    FS_VK_CHECK(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer");

    // The viewport background, read from the display settings this frame was
    // given. It is a presentation value and nothing more: the clear value is
    // written into the render pass on every frame anyway, so switching it
    // rebuilds no geometry, mints no revision, re-uploads nothing, and does not
    // touch the swapchain, the pipeline, the descriptor set or any GPU buffer.
    float background[3];
    viewportBackgroundColor(display_.background, background);

    VkClearValue clears[2]{};
    clears[0].color = {{background[0], background[1], background[2], 1.0f}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = renderPass_;
    rp.framebuffer = framebuffers_[imageIndex];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = swapchainExtent_;
    rp.clearValueCount = 2;
    rp.pClearValues = clears;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    // The MatCap sampler. Bound once for the whole pass: the set is immutable
    // and shared by every body, so it does not belong inside the per-body loop.
    if (descriptorSet_ != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                                &descriptorSet_, 0, nullptr);
    }

    // One draw per Construction Body, each with its OWN model transform, its
    // own buffers and its own selection state. Before the first mesh revision
    // has been uploaded for a body there is nothing to draw for it; the pass
    // still clears and presents, so the viewport never stalls.
    for (const SceneDrawItem& item : scene_) {
        recordBodyDraw(cmd, item);
    }

    // LAST, and that ordering is load-bearing. Every body has now written depth,
    // so the depth-tested, depth-biased grid is correctly occluded by the model
    // and loses cleanly to a Construction Plane lying on its own plane at y = 0.
    // Drawing it first would work for opaque bodies and fail for exactly the
    // coplanar case the bias exists to settle.
    recordGridDraw(cmd);

    // And the gizmo after the grid, so a handle is never lost behind a floor
    // line. It is depth-test-off and depth-write-off, so it reads nothing from
    // the depth buffer and leaves it exactly as the bodies and the grid did.
    recordGizmoDraw(cmd);

    // And the sketch overlay last, on the gizmo's pipeline and terms: a plane
    // grid and the lines being drawn must read over the bodies they are drawn
    // against, and they leave the depth buffer untouched.
    recordSketchOverlayDraw(cmd);

    vkCmdEndRenderPass(cmd);
    FS_VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
    return true;
}

void Renderer::recordBodyDraw(VkCommandBuffer cmd, const SceneDrawItem& item) {
    auto found = bodies_.find(item.objectId);
    if (found == bodies_.end()) {
        return;  // nothing uploaded for this body yet
    }
    const BodyRenderResources& body = found->second;
    if (body.vertexBuffer == VK_NULL_HANDLE || body.indexBuffer == VK_NULL_HANDLE ||
        body.indexCount == 0) {
        return;
    }

    // The mesh vertices are this body's LOCAL geometry and never move; where the
    // body appears comes from its own derived model transform, and where the
    // viewer stands comes from the camera snapshot. The renderer composes the
    // two and owns neither.
    const Mat4 modelView = mat4Multiply(camera_.view, item.model);
    const Mat4 mvp = mat4Multiply(camera_.proj, modelView);

    SurfacePush push{};
    std::memcpy(push.mvp, mvp.m, sizeof(push.mvp));

    // The view-space NORMAL matrix, by ROWS. Storage is column-major
    // (m[column * 4 + row]), so row r is {m[0*4+r], m[1*4+r], m[2*4+r]}.
    //
    // This is view * (R * S^-1) and NOT view * model. The two are the same
    // matrix for every unscaled body, and they part company the moment a body
    // carries a non-uniform scale: the model stretches a normal the same way it
    // stretches a position, which tilts it the wrong way on every face that is
    // not perpendicular to a scaled axis. The scene hands the correct one over
    // as `normalModel` — the inverse transpose of the model's upper-left 3x3,
    // built from the authoritative scale rather than inverted numerically.
    //
    // The view factor still needs no inverse-transpose of its own, because a
    // look-at matrix is rigid and is its own. See forgeshape_transform.h.
    const Mat4 viewNormal = mat4Multiply(camera_.view, item.normalModel);
    for (int row = 0; row < 3; ++row) {
        float* dst = (row == 0) ? push.normalRow0 : (row == 1) ? push.normalRow1 : push.normalRow2;
        dst[0] = viewNormal.m[0 * 4 + row];
        dst[1] = viewNormal.m[1 * 4 + row];
        dst[2] = viewNormal.m[2 * 4 + row];
        dst[3] = 0.0f;
    }
    // Packed into row 0's otherwise-dead w to stay inside the guaranteed
    // 128-byte push constant budget. Switching shading model is exactly this
    // one float: no geometry is rebuilt, no revision is minted and no buffer is
    // touched.
    push.normalRow0[3] = static_cast<float>(shadingModelIndex(display_.shading));

    // Per body, not per frame: only the selected body is tinted, and how
    // strongly is this body's own pulse state, advanced once per frame by
    // advanceSelectionFeedback. The renderer is still never told WHICH object
    // is selected — the snapshot carries a plain bool per item and identity
    // stays with SelectionController. No push-constant byte was added for this:
    // `selectionTint.a` has always been there.
    std::memcpy(push.selectionTint, kSelectionTintRgb, sizeof(kSelectionTintRgb));
    push.selectionTint[3] = body.selectionAlpha;

    vkCmdPushConstants(cmd, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(SurfacePush), &push);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &body.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, body.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, body.indexCount, 1, 0, 0, 0);
}

void Renderer::recordGridDraw(VkCommandBuffer cmd) {
    // The whole cost of "Grid off" is this early return. Nothing is rebuilt,
    // nothing is freed and nothing is re-uploaded when the grid is hidden, which
    // is why toggling it can be proven to touch no body's revision, render mesh
    // or GPU buffer (R1C2-03, R1C2-06, R1C2-19).
    if (!display_.gridVisible || gridPipeline_ == VK_NULL_HANDLE ||
        gridVertexBuffer_ == VK_NULL_HANDLE || gridVertexCount_ == 0) {
        return;
    }

    // No model matrix. The grid's vertices ARE world space — the one thing in
    // the renderer with no placement, because a floor that could be moved would
    // be a Construction Body, and it must never become one.
    //
    // Both projections work here for free and neither is touched: whatever the
    // camera snapshot says about perspective or orthographic is already in
    // camera_.proj, and this composes it exactly as recordBodyDraw does.
    const Mat4 viewProj = mat4Multiply(camera_.proj, camera_.view);

    GridPush push{};
    std::memcpy(push.viewProj, viewProj.m, sizeof(push.viewProj));
    gridLineColor(display_.background, GridLineTier::Minor, push.minorColor);
    gridLineColor(display_.background, GridLineTier::Major, push.majorColor);
    gridLineColor(display_.background, GridLineTier::AxisX, push.axisXColor);
    gridLineColor(display_.background, GridLineTier::AxisZ, push.axisZColor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline_);
    vkCmdPushConstants(cmd, gridPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GridPush),
                       &push);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &gridVertexBuffer_, &offset);
    // ONE draw call, non-indexed, for the whole grid.
    vkCmdDraw(cmd, gridVertexCount_, 1, 0, 0);

    // The surface pipeline is rebound by the next frame's recording, which
    // always starts from vkCmdBindPipeline(pipeline_). Nothing after this point
    // in the pass draws a body.
}

void Renderer::recordGizmoDraw(VkCommandBuffer cmd) {
    // The whole cost of "no gizmo" is this early return: no gizmo state was
    // pushed, or the shell says there is no active Transform context, or the
    // camera could not produce a scale. Nothing is rebuilt, nothing is freed and
    // nothing is re-uploaded when the gizmo is absent — which is why showing and
    // hiding it can be proven to touch no body's revision or GPU buffer.
    if (!gizmo_.visible || gizmoPipeline_ == VK_NULL_HANDLE ||
        gizmoVertexBuffer_ == VK_NULL_HANDLE || gizmoVertexCount_ == 0 ||
        !(gizmo_.worldPerReferenceUnit > 0.0f)) {
        return;
    }

    // Canonical gizmo space -> world -> clip. The canonical vertices are
    // authored in REFERENCE UNITS, so one uniform scale by the world length of a
    // reference unit at the pivot depth is exactly what keeps the handles a
    // near-constant size on screen at any zoom.
    //
    // The rotation in this matrix is the gizmo BASIS the session decided —
    // identity in World space, the body orientation in Local — and it is taken
    // from the snapshot rather than recomputed, so the drawn handles and the
    // hit-tested handles cannot point different ways. The body own SCALE is
    // deliberately absent: stretching a body must not stretch the instrument
    // used to stretch it, and the snapshot carries no scale for it to reach.
    const float scale = gizmo_.worldPerReferenceUnit;
    Mat4 model = gizmo_.orientation;
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            model.m[column * 4 + row] *= scale;
        }
    }
    model.m[12] = gizmo_.pivot.x;
    model.m[13] = gizmo_.pivot.y;
    model.m[14] = gizmo_.pivot.z;
    model.m[15] = 1.0f;

    const Mat4 viewProj = mat4Multiply(camera_.proj, camera_.view);
    const Mat4 mvp = mat4Multiply(viewProj, model);
    if (!mat4Finite(mvp)) {
        return;
    }

    GizmoPush push{};
    std::memcpy(push.mvp, mvp.m, sizeof(push.mvp));
    gizmoAxisColor(display_.background, GizmoAxis::X, push.axisXColor);
    gizmoAxisColor(display_.background, GizmoAxis::Y, push.axisYColor);
    gizmoAxisColor(display_.background, GizmoAxis::Z, push.axisZColor);
    gizmoHighlightColor(display_.background, push.highlight);
    // The packed scalars. See GizmoPush and gizmo.vert: the alpha slots of the
    // three axis colours are the only bytes left inside the guaranteed budget.
    push.axisXColor[3] = gizmoNeutralLevel(display_.background);
    push.axisYColor[3] = static_cast<float>(gizmoHandleCode(gizmo_.activeHandle));
    push.axisZColor[3] = kGizmoIdleAxisAlphaScale;
    push.highlight[3] = kGizmoAxisAlpha;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gizmoPipeline_);
    vkCmdPushConstants(cmd, gizmoPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GizmoPush),
                       &push);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &gizmoVertexBuffer_, &offset);
    // A RANGE of the one buffer, not a second buffer and not a re-upload:
    // switching Move to Rotate to Scale changes two integers on this call and
    // nothing else in the whole renderer.
    int firstVertex = 0;
    int vertexCount = 0;
    if (!gizmoVertexRange(gizmo_.mode, &firstVertex, &vertexCount)) {
        return;
    }
    const uint32_t first = static_cast<uint32_t>(firstVertex);
    const uint32_t count = static_cast<uint32_t>(vertexCount);
    if (first + count > gizmoVertexCount_) {
        return;
    }
    vkCmdDraw(cmd, count, 1, first, 0);

    // Nothing after this point in the pass draws anything, and the next frame's
    // recording starts from vkCmdBindPipeline(pipeline_) again — so this pass
    // leaves no pipeline, blend, depth or scissor state behind for a body draw
    // to inherit.
}

bool Renderer::drawFrame() {
    if (surface_ == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE) {
        return true;  // nothing to present yet
    }

    if (needsSwapchainRebuild_ || swapchain_ == VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        destroySwapchainDependents();
        if (!createSwapchainDependents()) {
            needsSwapchainRebuild_ = true;
            return true;  // retry on a later tick (e.g. zero-sized surface)
        }
        FS_LOGI("Swapchain rebuilt");
    }

    // Mirror each body's newest published CPU mesh revision onto the GPU before
    // this frame is recorded. No-op for any body whose revision did not change.
    syncScene();
    // And the sketch overlay, on the same revision-gated terms.
    syncSketchOverlay();

    // Selection feedback is presentation and rides entirely on the frame loop
    // that was going to run anyway: no Java animator, no invalidate, no
    // geometry, no revision and no upload. It is advanced OUTSIDE syncScene's
    // revision gate on purpose — a pulse has to keep decaying on the frames
    // where nothing was published, which is nearly all of them.
    advanceSelectionFeedback(consumeFrameDeltaSeconds());

    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                             imageAvailable_[currentFrame_], VK_NULL_HANDLE,
                                             &imageIndex);
#ifndef NDEBUG
    if (injectDeviceLossOnce_) {
        // The seam, and the whole of it: from here down every line is the real
        // device-loss path. Nothing about the handling is special-cased for the
        // injection, which is what makes the test a test of the product.
        injectDeviceLossOnce_ = false;
        acquire = VK_ERROR_DEVICE_LOST;
    }
#endif
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
        // OUT_OF_DATE, SURFACE_LOST, DEVICE_LOST and anything else. Every one of
        // them means this frame has no image to draw into, so the frame is
        // abandoned and `imageIndex` is never read. What HAPPENS about it — a
        // swapchain rebuild, a whole device rebuild, or stopping for good — is
        // the recovery policy's decision, not this function's.
        return handleFrameResult(static_cast<int>(acquire), "acquire");
    }
    if (acquire == VK_SUBOPTIMAL_KHR && !expectSuboptimal_) {
        // Suboptimal-but-usable: this frame is still drawn, and the swapchain is
        // rebuilt before the next one. Rebuild only when it is NOT the expected
        // consequence of the identity-pre-transform convention; a real size
        // change arrives as OUT_OF_DATE or as an explicit requestResize().
        needsSwapchainRebuild_ = true;
    }

    if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight_[imageIndex] = inFlightFences_[currentFrame_];

    if (!recordCommandBuffer(commandBuffers_[imageIndex], imageIndex)) {
        return false;
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imageAvailable_[currentFrame_];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffers_[imageIndex];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinished_[imageIndex];

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
    // Not FS_VK_CHECK: a lost device very often surfaces here rather than at
    // acquire, and treating it as a flat unrecoverable failure would throw away
    // a device the policy is willing to rebuild.
    const VkResult submitted = vkQueueSubmit(graphicsQueue_, 1, &submit,
                                             inFlightFences_[currentFrame_]);
    if (submitted != VK_SUCCESS) {
        return handleFrameResult(static_cast<int>(submitted), "submit");
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished_[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &imageIndex;

    VkResult presented = vkQueuePresentKHR(presentQueue_, &present);
    if (presented != VK_SUCCESS && presented != VK_SUBOPTIMAL_KHR) {
        // Same reasoning as the submit above: OUT_OF_DATE is routine, and
        // DEVICE_LOST is recoverable rather than fatal. The frame counter and
        // the first-present token are deliberately not advanced on this path —
        // nothing was presented.
        return handleFrameResult(static_cast<int>(presented), "present");
    }
    if (presented == VK_SUBOPTIMAL_KHR && !expectSuboptimal_) {
        needsSwapchainRebuild_ = true;
    }

    ++frameIndex_;
    // Reaching here means an image really was presented: every non-success
    // result returned above. After a device rebuild `attachSurface` clears this
    // flag, so the token appears a second time — which is the honest report
    // that the viewport came back, not a duplicate startup.
    if (!presentedThisSession_) {
        presentedThisSession_ = true;
        FS_LOGI("First frame submitted and presented (frame #%llu, %ux%u)",
                (unsigned long long)frameIndex_, swapchainExtent_.width, swapchainExtent_.height);
        __android_log_print(ANDROID_LOG_INFO, FS_TAG, "FORGESHAPE_NATIVE_VIEWPORT_OK");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
    return true;
}

}  // namespace forgeshape
