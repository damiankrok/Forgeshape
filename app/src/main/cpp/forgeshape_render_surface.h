// The renderer's ONE window seam (`DESKTOP-R1`).
//
// A swapchain is made for a window, and a window is the one thing the two
// shells do not share: Android hands the renderer an `ANativeWindow`, the
// Windows shell an `HWND`, and an automated desktop check a headless surface
// of a stated size (`VK_EXT_headless_surface`, which Mesa's software driver
// provides on a CI runner without a GPU). Everything the renderer needs from a
// window is the five functions below; nothing else in it names a platform.
//
// On Android `PlatformWindow` IS `ANativeWindow*`, so `Renderer::attachSurface`
// keeps its signature and the Android path is the same Vulkan calls it always
// was.
#pragma once

#if defined(__ANDROID__)
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <android/native_window.h>
#elif defined(_WIN32)
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <vulkan/vulkan.h>

#include <cstdint>

namespace forgeshape {

#if defined(__ANDROID__)

using PlatformWindow = ANativeWindow*;

#else

// A desktop window as the shell hands it over. The shell owns it and outlives
// every renderer attached to it, so the renderer never takes or drops a
// reference. On Windows it is the client area of `hwnd`; on any other desktop
// build it is a headless surface of `width` x `height` pixels.
struct DesktopWindow {
#if defined(_WIN32)
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr;
#endif
    uint32_t width = 0;
    uint32_t height = 0;
};

using PlatformWindow = DesktopWindow*;

#endif

// The instance extension this platform's surface needs beside VK_KHR_surface.
const char* platformSurfaceExtensionName();

// What the log calls this platform's surface ("Android", "Win32", "headless").
const char* platformSurfaceLabel();

// Reference handling: an Android window is reference counted, a desktop window
// is owned by its shell (both no-ops there).
void platformWindowAcquire(PlatformWindow window);
void platformWindowRelease(PlatformWindow window);

// The window's current size in pixels. False when it has none (a minimised
// Win32 window reports 0 x 0, which the swapchain treats as "skip").
bool platformWindowSize(PlatformWindow window, uint32_t* width, uint32_t* height);

// Creates the Vulkan surface for the window.
VkResult platformCreateSurface(VkInstance instance, PlatformWindow window, VkSurfaceKHR* out);

}  // namespace forgeshape
