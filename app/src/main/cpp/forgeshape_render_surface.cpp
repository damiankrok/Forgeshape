#include "forgeshape_render_surface.h"

namespace forgeshape {

#if defined(__ANDROID__)

const char* platformSurfaceExtensionName() { return VK_KHR_ANDROID_SURFACE_EXTENSION_NAME; }

const char* platformSurfaceLabel() { return "Android"; }

void platformWindowAcquire(PlatformWindow window) {
    if (window != nullptr) {
        ANativeWindow_acquire(window);
    }
}

void platformWindowRelease(PlatformWindow window) {
    if (window != nullptr) {
        ANativeWindow_release(window);
    }
}

bool platformWindowSize(PlatformWindow window, uint32_t* width, uint32_t* height) {
    if (window == nullptr) {
        return false;
    }
    const int w = ANativeWindow_getWidth(window);
    const int h = ANativeWindow_getHeight(window);
    *width = w > 0 ? static_cast<uint32_t>(w) : 0u;
    *height = h > 0 ? static_cast<uint32_t>(h) : 0u;
    return w > 0 && h > 0;
}

VkResult platformCreateSurface(VkInstance instance, PlatformWindow window, VkSurfaceKHR* out) {
    VkAndroidSurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    info.window = window;
    return vkCreateAndroidSurfaceKHR(instance, &info, nullptr, out);
}

#elif defined(_WIN32)

const char* platformSurfaceExtensionName() { return VK_KHR_WIN32_SURFACE_EXTENSION_NAME; }

const char* platformSurfaceLabel() { return "Win32"; }

void platformWindowAcquire(PlatformWindow) {}

void platformWindowRelease(PlatformWindow) {}

bool platformWindowSize(PlatformWindow window, uint32_t* width, uint32_t* height) {
    if (window == nullptr || window->hwnd == nullptr) {
        return false;
    }
    RECT rect{};
    if (!GetClientRect(window->hwnd, &rect)) {
        return false;
    }
    const LONG w = rect.right - rect.left;
    const LONG h = rect.bottom - rect.top;
    *width = w > 0 ? static_cast<uint32_t>(w) : 0u;
    *height = h > 0 ? static_cast<uint32_t>(h) : 0u;
    return w > 0 && h > 0;
}

VkResult platformCreateSurface(VkInstance instance, PlatformWindow window, VkSurfaceKHR* out) {
    if (window == nullptr || window->hwnd == nullptr) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkWin32SurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.hinstance = window->instance;
    info.hwnd = window->hwnd;
    return vkCreateWin32SurfaceKHR(instance, &info, nullptr, out);
}

#else

// Any other desktop build is the automated check's: a headless surface, which
// presents nowhere and whose size is whatever the shell states.
const char* platformSurfaceExtensionName() { return VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME; }

const char* platformSurfaceLabel() { return "headless"; }

void platformWindowAcquire(PlatformWindow) {}

void platformWindowRelease(PlatformWindow) {}

bool platformWindowSize(PlatformWindow window, uint32_t* width, uint32_t* height) {
    if (window == nullptr) {
        return false;
    }
    *width = window->width;
    *height = window->height;
    return window->width > 0 && window->height > 0;
}

VkResult platformCreateSurface(VkInstance instance, PlatformWindow window, VkSurfaceKHR* out) {
    if (window == nullptr) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    // An extension entry point, so it is asked of the instance rather than
    // linked: the loader exports only core and WSI functions.
    auto create = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT"));
    if (create == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    VkHeadlessSurfaceCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
    return create(instance, &info, nullptr, out);
}

#endif

}  // namespace forgeshape
