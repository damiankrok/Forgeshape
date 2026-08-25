#include "forgeshape_input.h"

#include <cmath>

namespace forgeshape {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

}  // namespace

const char* pointerToolTypeName(PointerToolType tool) {
    switch (tool) {
        case PointerToolType::Finger: return "finger";
        case PointerToolType::Stylus: return "stylus";
        case PointerToolType::Eraser: return "eraser";
        case PointerToolType::Mouse:  return "mouse";
        case PointerToolType::Unknown: break;
    }
    return "unknown";
}

PointerToolType pointerToolTypeFromCode(int32_t code) {
    // A closed switch, not a cast. Casting an out-of-range int onto the enum
    // would produce a value no switch anywhere handles, which is exactly the
    // failure the Unknown fallback exists to prevent.
    switch (code) {
        case 1: return PointerToolType::Finger;
        case 2: return PointerToolType::Stylus;
        case 3: return PointerToolType::Eraser;
        case 4: return PointerToolType::Mouse;
        default: return PointerToolType::Unknown;
    }
}

int32_t pointerToolTypeCode(PointerToolType tool) {
    return static_cast<int32_t>(tool);
}

float sanitizePointerPressure(float raw) {
    if (!std::isfinite(raw)) {
        return kPointerPressureDefault;
    }
    if (raw < kPointerPressureMin) {
        return kPointerPressureMin;
    }
    if (raw > kPointerPressureMax) {
        return kPointerPressureMax;
    }
    return raw;
}

float sanitizePointerTiltRadians(float raw) {
    if (!std::isfinite(raw)) {
        return kPointerTiltNoneRadians;
    }
    if (raw < kPointerTiltNoneRadians) {
        // A negative lean is not a direction: direction lives in the
        // orientation angle, and the magnitude is unsigned by definition.
        return kPointerTiltNoneRadians;
    }
    if (raw > kPointerTiltMaxRadians) {
        return kPointerTiltMaxRadians;
    }
    return raw;
}

float sanitizePointerTiltOrientationRadians(float raw) {
    if (!std::isfinite(raw)) {
        return 0.0f;
    }
    // Wrapped rather than clamped: an orientation is periodic, so a value just
    // past +pi is very nearly -pi and must not collapse onto the opposite side.
    float wrapped = std::fmod(raw, kTwoPi);
    if (wrapped <= -kPi) {
        wrapped += kTwoPi;
    } else if (wrapped > kPi) {
        wrapped -= kTwoPi;
    }
    // fmod of a huge magnitude can still land marginally outside after the
    // single correction above; fold once more rather than loop.
    if (wrapped <= -kPi) {
        wrapped += kTwoPi;
    } else if (wrapped > kPi) {
        wrapped -= kTwoPi;
    }
    return wrapped;
}

void sanitizePointerTilt(float rawTiltRadians, float rawOrientationRadians,
                         float* outTiltRadians, float* outOrientationRadians) {
    const float tilt = sanitizePointerTiltRadians(rawTiltRadians);
    const float orientation = (tilt == kPointerTiltNoneRadians)
                                  ? 0.0f
                                  : sanitizePointerTiltOrientationRadians(rawOrientationRadians);
    if (outTiltRadians != nullptr) {
        *outTiltRadians = tilt;
    }
    if (outOrientationRadians != nullptr) {
        *outOrientationRadians = orientation;
    }
}

}  // namespace forgeshape
