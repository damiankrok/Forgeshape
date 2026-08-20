#include "forgeshape_renderer.h"

#include <android/log.h>

#include <algorithm>
#include <chrono>
#include <cstring>

#include "forgeshape_math.h"
#include "forgeshape_mesh.h"

// SPIR-V produced ahead of time by the NDK-provided glslc (see CMakeLists.txt).
static const uint32_t kCubeVertSpv[] =
#include "cube.vert.spv.inc"
    ;
static const uint32_t kCubeFragSpv[] =
#include "cube.frag.spv.inc"
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

// Fragment-stage push constant: rgb is the tint colour, a is how much of it to
// mix in. The renderer is told only whether to draw the highlight; which object
// is selected is owned by SelectionController and never reaches this file.
struct SelectionPush {
    float tint[4];
};

constexpr VkDeviceSize kSelectionPushOffset = sizeof(Mat4);

const SelectionPush kNotSelected{{0.0f, 0.0f, 0.0f, 0.0f}};
const SelectionPush kSelected{{1.00f, 0.62f, 0.10f, 0.55f}};

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

void Renderer::destroyInstance() {
    if (device_ != VK_NULL_HANDLE) {
        // Process teardown is the one place a full device idle is right: every
        // queue is about to disappear along with the device.
        vkDeviceWaitIdle(device_);

        destroyMeshResources();
        if (vertShader_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, vertShader_, nullptr);
        if (fragShader_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, fragShader_, nullptr);
        if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);

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
        commandPool_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        FS_LOGI("Vulkan instance destroyed");
    }
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
        if (!createShaderModules()) return false;
        if (!createSyncObjects()) return false;
        if (!createMeshUploadObjects()) return false;
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

void Renderer::destroyMeshResources() {
    if (device_ == VK_NULL_HANDLE) return;

    if (vertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, vertexBuffer_, nullptr);
    if (vertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, vertexMemory_, nullptr);
    if (indexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, indexBuffer_, nullptr);
    if (indexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, indexMemory_, nullptr);
    if (stagingBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, stagingBuffer_, nullptr);
    if (stagingMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, stagingMemory_, nullptr);
    if (uploadFence_ != VK_NULL_HANDLE) vkDestroyFence(device_, uploadFence_, nullptr);
    if (uploadCommandBuffer_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &uploadCommandBuffer_);
    }

    vertexBuffer_ = VK_NULL_HANDLE;
    vertexMemory_ = VK_NULL_HANDLE;
    vertexCapacityBytes_ = 0;
    indexBuffer_ = VK_NULL_HANDLE;
    indexMemory_ = VK_NULL_HANDLE;
    indexCapacityBytes_ = 0;
    stagingBuffer_ = VK_NULL_HANDLE;
    stagingMemory_ = VK_NULL_HANDLE;
    stagingCapacityBytes_ = 0;
    uploadFence_ = VK_NULL_HANDLE;
    uploadCommandBuffer_ = VK_NULL_HANDLE;
    indexCount_ = 0;
    uploadedRevision_ = kNoMeshRevision;
    meshUploadDiagnostics().setLiveBufferObjects(0);
}

bool Renderer::waitForMeshBuffersIdle() {
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
    const VkResult r = vkWaitForFences(device_, count, fences, VK_TRUE, UINT64_MAX);
    if (r != VK_SUCCESS) {
        FS_LOGE("vkWaitForFences(mesh idle) -> %d", (int)r);
        FS_FAIL("mesh_wait_frames_idle");
        return false;
    }
    return true;
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

bool Renderer::ensureMeshCapacity(VkDeviceSize vertexBytes, VkDeviceSize indexBytes,
                                  bool* outGrew) {
    const bool vertexFits = vertexBuffer_ != VK_NULL_HANDLE && vertexBytes <= vertexCapacityBytes_;
    const bool indexFits = indexBuffer_ != VK_NULL_HANDLE && indexBytes <= indexCapacityBytes_;
    if (vertexFits && indexFits) {
        if (outGrew != nullptr) *outGrew = false;
        return true;  // same-topology (or smaller) update: reuse, no recreation
    }

    uint64_t newVertexCapacity = vertexCapacityBytes_;
    uint64_t newIndexCapacity = indexCapacityBytes_;
    if (!vertexFits &&
        !growCapacityBytes(vertexCapacityBytes_, vertexBytes, &newVertexCapacity)) {
        FS_FAIL("mesh_vertex_capacity_overflow");
        return false;
    }
    if (!indexFits && !growCapacityBytes(indexCapacityBytes_, indexBytes, &newIndexCapacity)) {
        FS_FAIL("mesh_index_capacity_overflow");
        return false;
    }

    // A buffer is only ever retired after every in-flight frame has finished
    // with it; the caller guarantees that before calling in.
    if (!vertexFits) {
        if (vertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        if (vertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, vertexMemory_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
        vertexMemory_ = VK_NULL_HANDLE;
        vertexCapacityBytes_ = 0;
        if (!createBuffer(static_cast<VkDeviceSize>(newVertexCapacity),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer_, &vertexMemory_)) {
            return false;
        }
        vertexCapacityBytes_ = static_cast<VkDeviceSize>(newVertexCapacity);
        ++meshBufferGrowCount_;
        meshUploadDiagnostics().recordBufferGrow();
    }
    if (!indexFits) {
        if (indexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, indexBuffer_, nullptr);
        if (indexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, indexMemory_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
        indexMemory_ = VK_NULL_HANDLE;
        indexCapacityBytes_ = 0;
        if (!createBuffer(static_cast<VkDeviceSize>(newIndexCapacity),
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer_, &indexMemory_)) {
            return false;
        }
        indexCapacityBytes_ = static_cast<VkDeviceSize>(newIndexCapacity);
        ++meshBufferGrowCount_;
        meshUploadDiagnostics().recordBufferGrow();
    }

    if (outGrew != nullptr) *outGrew = true;
    return true;
}

bool Renderer::uploadMesh(const RuntimeMesh& mesh) {
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
    if (!ensureMeshCapacity(vertexBytes, indexBytes, &grew)) {
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
    std::memcpy(bytes, mesh.vertices(), static_cast<size_t>(vertexBytes));
    std::memcpy(bytes + vertexBytes, mesh.indices(), static_cast<size_t>(indexBytes));
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
    vkCmdCopyBuffer(uploadCommandBuffer_, stagingBuffer_, vertexBuffer_, 1, &vertexCopy);

    VkBufferCopy indexCopy{};
    indexCopy.srcOffset = vertexBytes;
    indexCopy.dstOffset = 0;
    indexCopy.size = indexBytes;
    vkCmdCopyBuffer(uploadCommandBuffer_, stagingBuffer_, indexBuffer_, 1, &indexCopy);

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

    indexCount_ = mesh.indexCount();
    uploadedRevision_ = mesh.revision();

    MeshUploadDiagnostics& diagnostics = meshUploadDiagnostics();
    diagnostics.recordUpload(mesh.revision(), mesh.vertexCount(), mesh.indexCount(),
                             vertexCapacityBytes_, indexCapacityBytes_, /*reusedCapacity=*/!grew);
    // Exactly one vertex buffer and one index buffer are ever live: an old one
    // is destroyed in the same step that replaces it.
    diagnostics.setLiveBufferObjects(
        (vertexBuffer_ != VK_NULL_HANDLE ? 1u : 0u) + (indexBuffer_ != VK_NULL_HANDLE ? 1u : 0u));

    const MeshGpuStats stats = diagnostics.snapshot();
    FS_LOGI("FORGESHAPE_MESH_UPLOAD_OK:%llu:%u:%u:%s vcap=%llu icap=%llu scap=%llu "
            "grows=%llu sgrows=%llu uploads=%llu",
            (unsigned long long)mesh.revision(), mesh.vertexCount(), mesh.indexCount(),
            grew ? "grow" : "reuse", (unsigned long long)stats.vertexCapacityBytes,
            (unsigned long long)stats.indexCapacityBytes,
            (unsigned long long)stats.stagingCapacityBytes,
            (unsigned long long)stats.bufferGrowCount,
            (unsigned long long)stats.stagingGrowCount,
            (unsigned long long)stats.uploadCount);
    return true;
}

void Renderer::syncMeshRevision() {
    // Observes only the NEWEST published revision; revisions published between
    // two frames are coalesced away, which is what keeps this bounded.
    const RuntimeMeshPtr mesh = meshStore().current();
    if (!mesh) {
        return;
    }
    if (mesh->revision() == uploadedRevision_ || mesh->revision() == failedRevision_) {
        return;
    }
    if (!uploadMesh(*mesh)) {
        failedRevision_ = mesh->revision();  // fail closed, keep drawing the last good mesh
        meshUploadDiagnostics().recordFailure();
        FS_LOGE("FORGESHAPE_MESH_UPLOAD_FAIL:%llu", (unsigned long long)mesh->revision());
    }
}

bool Renderer::createShaderModules() {
    VkShaderModuleCreateInfo vertInfo{};
    vertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertInfo.codeSize = sizeof(kCubeVertSpv);
    vertInfo.pCode = kCubeVertSpv;
    FS_VK_CHECK(vkCreateShaderModule(device_, &vertInfo, nullptr, &vertShader_), "vkCreateShaderModule(vert)");

    VkShaderModuleCreateInfo fragInfo{};
    fragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragInfo.codeSize = sizeof(kCubeFragSpv);
    fragInfo.pCode = kCubeFragSpv;
    FS_VK_CHECK(vkCreateShaderModule(device_, &fragInfo, nullptr, &fragShader_), "vkCreateShaderModule(frag)");

    VkPushConstantRange pushRanges[2]{};
    pushRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRanges[0].offset = 0;
    pushRanges[0].size = sizeof(Mat4);
    pushRanges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRanges[1].offset = static_cast<uint32_t>(kSelectionPushOffset);
    pushRanges[1].size = sizeof(SelectionPush);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 2;
    layoutInfo.pPushConstantRanges = pushRanges;
    FS_VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_), "vkCreatePipelineLayout");

    FS_LOGI("Shader modules created (SPIR-V: vert %zu bytes, frag %zu bytes)",
            sizeof(kCubeVertSpv), sizeof(kCubeFragSpv));
    return true;
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

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[2]{};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(MeshVertex, position);
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(MeshVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
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
    // The projection's Y flip mirrors that into framebuffer space, so the front
    // face is the CLOCKWISE one here. CPU picking uses the same convention, so
    // what is pickable is exactly what is drawn.
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
            "cull BACK, frontFace CLOCKWISE)");
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

    VkClearValue clears[2]{};
    clears[0].color = {{0.055f, 0.070f, 0.105f, 1.0f}};  // ForgeShape viewport background
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

    // The mesh vertices are the box's LOCAL geometry and never move; where the
    // box appears comes from the derived model transform, and where the viewer
    // stands comes from the camera snapshot. The renderer composes the two and
    // owns neither.
    const Mat4 mvp = mat4Multiply(camera_.proj, mat4Multiply(camera_.view, model_));

    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), mvp.m);

    const SelectionPush& selection = selectionHighlight_ ? kSelected : kNotSelected;
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT,
                       static_cast<uint32_t>(kSelectionPushOffset), sizeof(SelectionPush),
                       selection.tint);

    // Before the first mesh revision has been uploaded there is nothing to
    // draw; the pass still clears and presents, so the viewport never stalls.
    if (vertexBuffer_ != VK_NULL_HANDLE && indexBuffer_ != VK_NULL_HANDLE && indexCount_ > 0) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);
        vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    FS_VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
    return true;
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

    // Mirror the newest published CPU mesh revision onto the GPU before this
    // frame is recorded. No-op unless the revision actually changed.
    syncMeshRevision();

    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                             imageAvailable_[currentFrame_], VK_NULL_HANDLE,
                                             &imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        needsSwapchainRebuild_ = true;
        return true;
    } else if (acquire == VK_SUBOPTIMAL_KHR) {
        // Suboptimal-but-usable. Rebuild only when it is NOT the expected
        // consequence of the identity-pre-transform convention; a real size
        // change arrives as OUT_OF_DATE or as an explicit requestResize().
        if (!expectSuboptimal_) {
            needsSwapchainRebuild_ = true;
        }
    } else if (acquire != VK_SUCCESS) {
        FS_LOGE("vkAcquireNextImageKHR -> %d", (int)acquire);
        FS_FAIL("vkAcquireNextImageKHR");
        return false;
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
    FS_VK_CHECK(vkQueueSubmit(graphicsQueue_, 1, &submit, inFlightFences_[currentFrame_]), "vkQueueSubmit");

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished_[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &imageIndex;

    VkResult presented = vkQueuePresentKHR(presentQueue_, &present);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR ||
        (presented == VK_SUBOPTIMAL_KHR && !expectSuboptimal_)) {
        needsSwapchainRebuild_ = true;
    } else if (presented != VK_SUCCESS && presented != VK_SUBOPTIMAL_KHR) {
        FS_LOGE("vkQueuePresentKHR -> %d", (int)presented);
        FS_FAIL("vkQueuePresentKHR");
        return false;
    }

    ++frameIndex_;
    if (!presentedThisSession_ && presented != VK_ERROR_OUT_OF_DATE_KHR) {
        presentedThisSession_ = true;
        FS_LOGI("First frame submitted and presented (frame #%llu, %ux%u)",
                (unsigned long long)frameIndex_, swapchainExtent_.width, swapchainExtent_.height);
        __android_log_print(ANDROID_LOG_INFO, FS_TAG, "FORGESHAPE_NATIVE_VIEWPORT_OK");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
    return true;
}

}  // namespace forgeshape
