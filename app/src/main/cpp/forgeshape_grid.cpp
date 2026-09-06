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
    // Every palette follows the same three rules, which is what makes the grid
    // read as one thing in three appearances rather than as three grids:
    //
    //   1. weight comes from ALPHA, not from a brighter colour, so a line never
    //      competes with the shaded model for the eye;
    //   2. the two axes are separated by a faint warm/cool lean rather than by
    //      saturated red and blue — enough to tell X from Z, far short of
    //      turning the floor into a diagram;
    //   3. every value is authored against its own background, because the
    //      contrast a line needs is a function of the ground under it.
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

    // The three dark grounds lift the lines LIGHTER than the floor and differ
    // only in how much weight that takes. A lighter ground needs more alpha to
    // say the same thing, which is why these are three palettes rather than
    // one: the same alpha that is a whisper over #26282A is invisible over
    // #3C3F41. The two LIGHT grounds (UI-PREF-R1) invert the direction and
    // nothing else: lines sink DARKER than the paper, still by alpha, still
    // with a warm X and a cool Z, and the three dark palettes are untouched.

    // Over #302E2B. Warm-neutral lines over a warm ground.
    static const Palette kWarmGraphite = {
        {0.72f, 0.70f, 0.66f, 0.13f},
        {0.78f, 0.76f, 0.71f, 0.26f},
        {0.85f, 0.55f, 0.35f, 0.50f},
        {0.45f, 0.62f, 0.85f, 0.50f},
    };

    // Over #26282A. Cool-neutral lines over a cool ground.
    static const Palette kNeutralCharcoal = {
        {0.66f, 0.70f, 0.74f, 0.14f},
        {0.72f, 0.76f, 0.80f, 0.28f},
        {0.85f, 0.55f, 0.35f, 0.50f},
        {0.45f, 0.62f, 0.85f, 0.50f},
    };

    // Over #3C3F41. The lightest ground, so the alphas are raised: at the two
    // darker grounds' values the floor would read as unlined.
    static const Palette kLightCharcoal = {
        {0.80f, 0.82f, 0.84f, 0.18f},
        {0.86f, 0.88f, 0.90f, 0.34f},
        {0.90f, 0.60f, 0.40f, 0.56f},
        {0.52f, 0.70f, 0.92f, 0.56f},
    };

    // Over #EDE7DC. Warm-neutral ink over cream; the darkest line is still
    // blended at well under half weight so the paper stays paper.
    static const Palette kWarmLight = {
        {0.36f, 0.33f, 0.28f, 0.16f},
        {0.30f, 0.27f, 0.22f, 0.30f},
        {0.72f, 0.30f, 0.14f, 0.55f},
        {0.16f, 0.38f, 0.72f, 0.55f},
    };

    // Over #E4E8EC. Cool-neutral ink over steel paper.
    static const Palette kCoolLight = {
        {0.28f, 0.32f, 0.37f, 0.16f},
        {0.22f, 0.26f, 0.31f, 0.30f},
        {0.72f, 0.30f, 0.14f, 0.55f},
        {0.14f, 0.36f, 0.72f, 0.55f},
    };

    const Palette* selected = &kWarmGraphite;
    switch (background) {
        case ViewportBackground::WarmGraphite: selected = &kWarmGraphite; break;
        case ViewportBackground::NeutralCharcoal: selected = &kNeutralCharcoal; break;
        case ViewportBackground::LightCharcoal: selected = &kLightCharcoal; break;
        case ViewportBackground::WarmLight: selected = &kWarmLight; break;
        case ViewportBackground::CoolLight: selected = &kCoolLight; break;
    }
    const Palette& palette = *selected;

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
