// Platform-neutral touch event data shared by every native consumer.
//
// This header exists so the camera owner and the selection owner interpret the
// SAME event representation instead of each declaring its own. It is data only:
// no policy, no state machine, and deliberately no JNI, Android or Vulkan types.
// It is not an input framework and must not grow into one.
#pragma once

#include <cstdint>

namespace forgeshape {

// Platform-neutral mirror of the touch phases ForgeShape cares about. The JNI
// layer is responsible for translating Android's MotionEvent actions into these.
enum class TouchAction {
    Down,          // first pointer touched down
    PointerDown,   // an additional pointer touched down
    Move,          // one or more pointers moved
    PointerUp,     // one of several pointers lifted
    Up,            // the last pointer lifted
    Cancel,        // the gesture was taken away from us
};

struct TouchPointer {
    int32_t id;  // stable pointer id, NOT an array index
    float x;     // pixels, view-local, origin top-left
    float y;
};

// Largest pointer count a single touch event may carry into native code.
constexpr int kMaxTrackedPointers = 6;

}  // namespace forgeshape
