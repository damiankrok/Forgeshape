#include "forgeshape_grid.h"

namespace forgeshape {
namespace {

// The centre index: the line that passes through the world origin. There is
// exactly one because kGridLinesPerAxis is odd by construction.
constexpr int kGridCentreIndex = kGridLinesPerAxis / 2;

}  // namespace

GridLineTier gridLineTier(int index, bool alongX) {
    if (index == kGridCentreIndex) {
        return alongX ? GridLineTier::AxisX : GridLineTier::AxisZ;
    }
    const int fromCentre = index - kGridCentreIndex;
    const int magnitude = fromCentre < 0 ? -fromCentre : fromCentre;
    return (magnitude % kGridMajorEveryNMinor == 0) ? GridLineTier::Major
                                                    : GridLineTier::Minor;
}

int generateGridVertices(GridVertex* out, int capacity) {
    if (out == nullptr || capacity < kGridVertexCount) {
        return 0;  // fail closed: a partial grid is worse than none
    }
    int written = 0;
    // Two passes, one per direction. `alongX` is which way the line RUNS: a
    // line running along X is offset along Z and spans the full extent in X.
    for (int pass = 0; pass < 2; ++pass) {
        const bool alongX = (pass == 0);
        for (int index = 0; index < kGridLinesPerAxis; ++index) {
            const float offset =
                (static_cast<float>(index - kGridCentreIndex)) * kGridMinorSpacingMeters;
            const float tier = static_cast<float>(gridLineTier(index, alongX));

            GridVertex& a = out[written++];
            GridVertex& b = out[written++];
            if (alongX) {
                a.position[0] = -kGridHalfExtentMeters;
                b.position[0] = kGridHalfExtentMeters;
                a.position[2] = offset;
                b.position[2] = offset;
            } else {
                a.position[0] = offset;
                b.position[0] = offset;
                a.position[2] = -kGridHalfExtentMeters;
                b.position[2] = kGridHalfExtentMeters;
            }
            a.position[1] = kGridPlaneY;
            b.position[1] = kGridPlaneY;
            a.tier = tier;
            b.tier = tier;
        }
    }
    return written;
}

void gridLineColor(ViewportBackground background, GridLineTier tier, float* outRgba) {
    if (outRgba == nullptr) {
        return;
    }
    // Both palettes follow the same three rules, which is what makes the grid
    // read as one thing in two appearances rather than as two grids:
    //
    //   1. weight comes from ALPHA, not from a brighter colour, so a line never
    //      competes with the shaded model for the eye;
    //   2. the two axes are separated by a faint warm/cool lean rather than by
    //      saturated red and blue — enough to tell X from Z, far short of
    //      turning the floor into a diagram;
    //   3. every value is authored against its own background, because "darker"
    //      and "lighter" are opposite instructions in the two appearances.
    //
    // The model is drawn BEFORE the grid with depth writes on, and the grid is
    // depth-tested and depth-biased away, so none of these values can ever land
    // on a model pixel. That is why the selection pulse stays exactly as
    // readable with the grid on as with it off.
    struct Palette {
        float minor[4];
        float major[4];
        float axisX[4];
        float axisZ[4];
    };

    // Over #0E121B. The lines lift off a near-black ground, so they are a cool
    // light grey held down to a low alpha.
    static const Palette kDark = {
        {0.55f, 0.60f, 0.70f, 0.14f},
        {0.60f, 0.65f, 0.75f, 0.28f},
        {0.85f, 0.55f, 0.35f, 0.50f},
        {0.45f, 0.62f, 0.85f, 0.50f},
    };

    // Over #E6E1D9. Inverted intent: the lines must sit DARKER than a warm
    // cream ground, and they are warm-neutral rather than pure grey so the page
    // does not turn into engineering paper.
    static const Palette kLight = {
        {0.25f, 0.24f, 0.22f, 0.16f},
        {0.22f, 0.21f, 0.19f, 0.30f},
        {0.55f, 0.30f, 0.15f, 0.52f},
        {0.18f, 0.34f, 0.55f, 0.52f},
    };

    const Palette& palette =
        (background == ViewportBackground::WarmLight) ? kLight : kDark;

    const float* source = palette.minor;
    switch (tier) {
        case GridLineTier::Minor: source = palette.minor; break;
        case GridLineTier::Major: source = palette.major; break;
        case GridLineTier::AxisX: source = palette.axisX; break;
        case GridLineTier::AxisZ: source = palette.axisZ; break;
    }
    for (int i = 0; i < 4; ++i) {
        outRgba[i] = source[i];
    }
}

}  // namespace forgeshape
