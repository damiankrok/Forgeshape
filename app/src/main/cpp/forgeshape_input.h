// Platform-neutral pointer event data shared by every native consumer.
//
// This header exists so the camera owner, the selection owner and the sculpt
// arbitration interpret the SAME event representation instead of each declaring
// its own. It is data only: no policy, no state machine, and deliberately no
// JNI, Android or Vulkan types. It is not an input framework and must not grow
// into one.
//
// Stylus semantics
// ----------------
// A pointer carries a tool type, a contact pressure and a tilt, because Sketch
// and Sculpt will eventually need them and because the data has to survive the
// whole Java -> JNI -> native crossing to be worth anything. Carrying them is
// ALL that happens today: no brush, no camera and no selection rule reads them,
// so a stylus and a finger that trace the same pixels produce the same result.
//
// Every field below is expressed in ForgeShape's own units and its own closed
// enum. No Android constant, axis id or Java value reaches this file; the
// Android adapter (the SurfaceView, then forgeshape_jni.cpp) is what translates.
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

// What is touching the viewport.
//
// The numeric values are ForgeShape's OWN wire codes, not any platform's: the
// Android adapter maps MotionEvent tool types onto them before they cross JNI,
// and a future adapter for another platform would map that platform's own
// vocabulary the same way. They are stable because they cross a language
// boundary as plain ints.
//
// Unknown is the fallback for anything a platform reports that ForgeShape does
// not model, including tool types that do not exist yet. It is NOT an error and
// NOT a reason to drop the pointer: an Unknown pointer is an ordinary contact
// pointer with default pressure and no tilt, and every consumer must treat it
// as one.
enum class PointerToolType : int32_t {
    Unknown = 0,
    Finger = 1,
    Stylus = 2,
    Eraser = 3,  // the stylus reversed; carried only, it switches no tool
    Mouse = 4,
};

// One past the largest valid wire code. Used to reject codes, never to iterate
// meaning into existence.
constexpr int32_t kPointerToolTypeCodeCount = 5;

// Diagnostic name. Owner-facing text, never parsed.
const char* pointerToolTypeName(PointerToolType tool);

// Wire code -> enum. Any code outside the closed set -- including a negative
// one, and including a tool type a future platform invents -- becomes Unknown.
PointerToolType pointerToolTypeFromCode(int32_t code);

// The wire code for an enum value, so the adapter and the tests agree.
int32_t pointerToolTypeCode(PointerToolType tool);

// ---------------------------------------------------------------------------
// Pressure
// ---------------------------------------------------------------------------
//
// Normalised contact pressure in [0, 1]: 0 is "touching with no measurable
// force" and 1 is the device's own idea of full force. It is deliberately NOT a
// physical unit -- no platform reports newtons, and a normalised range is what
// both Android's MotionEvent pressure axis and Apple Pencil's force/maximumForce
// ratio already are, so neither adapter has to invent a calibration.
//
// A device with no pressure sensor reports full pressure rather than none: a
// finger on a screen without a force sensor IS in full contact, and defaulting
// to 0 would make a future pressure-driven brush do nothing on most hardware.
constexpr float kPointerPressureMin = 0.0f;
constexpr float kPointerPressureMax = 1.0f;

// What a pointer carries when the platform reports nothing usable -- and the
// value every existing finger flow keeps.
constexpr float kPointerPressureDefault = 1.0f;

// Non-finite becomes the default; anything finite is clamped into range. A
// caller never has to check, and no NaN can reach a domain consumer.
float sanitizePointerPressure(float raw);

// ---------------------------------------------------------------------------
// Tilt
// ---------------------------------------------------------------------------
//
// Two angles, both in RADIANS, describing how the stylus is held:
//
//   tiltRadians            how far the stylus leans away from perpendicular.
//                          0 means straight up out of the screen; kPointerTiltMaxRadians
//                          (pi/2) means flat against the glass. Range [0, pi/2].
//
//   tiltOrientationRadians which way it leans, measured in the screen plane.
//                          Range (-pi, pi]. 0 points along screen -y (up the
//                          screen); positive turns toward screen +x.
//                          Meaningless, and reported as 0, when tiltRadians is 0.
//
// Radians and not degrees, once, everywhere, so no consumer ever has to ask.
//
// This pair is the smallest model that keeps direction: Android already reports
// exactly these two axes, and an Apple Pencil's altitude/azimuth converts into
// them with arithmetic alone (tilt = pi/2 - altitude). It is deliberately not a
// full stylus pose -- no barrel rotation, no hover distance, no button state --
// because nothing in the product needs one.
constexpr float kPointerTiltNoneRadians = 0.0f;
constexpr float kPointerTiltMaxRadians = 1.57079632679489661923f;  // pi/2

// Non-finite becomes "no tilt"; anything finite is clamped into [0, pi/2].
float sanitizePointerTiltRadians(float raw);

// Non-finite becomes 0; anything finite is wrapped into (-pi, pi] so the same
// physical direction always has the same number.
float sanitizePointerTiltOrientationRadians(float raw);

// Sanitizes the two tilt angles TOGETHER, which is what an adapter should call.
// Beyond the per-angle rules above it enforces the pairing rule: a pointer with
// no lean has no lean direction, so a zero tilt always reports a zero
// orientation. Without this a finger -- which reports a touch-ellipse
// orientation but never a tilt -- would arrive carrying a direction that means
// nothing. Either out pointer may be null.
void sanitizePointerTilt(float rawTiltRadians, float rawOrientationRadians,
                         float* outTiltRadians, float* outOrientationRadians);

// ---------------------------------------------------------------------------
// The pointer
// ---------------------------------------------------------------------------
//
// The defaults are the whole compatibility story: every existing call site
// writes TouchPointer{id, x, y} and gets a full-pressure, untilted finger, which
// is exactly what it meant before these fields existed.
struct TouchPointer {
    int32_t id;  // stable pointer id, NOT an array index
    float x;     // pixels, view-local, origin top-left
    float y;

    // Carried, never consumed. See the header comment.
    PointerToolType toolType = PointerToolType::Finger;
    float pressure = kPointerPressureDefault;         // [0, 1]
    float tiltRadians = kPointerTiltNoneRadians;      // [0, pi/2]
    float tiltOrientationRadians = 0.0f;              // (-pi, pi]
};

// Largest pointer count a single touch event may carry into native code.
constexpr int kMaxTrackedPointers = 6;

}  // namespace forgeshape
