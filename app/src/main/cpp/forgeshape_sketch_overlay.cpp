#include "forgeshape_sketch_overlay.h"

#include "forgeshape_gizmo.h"

namespace forgeshape {

bool sketchOverlayStyleWeights(SketchOverlayStyle style, ViewportBackground background,
                               SketchOverlayStyleWeights* out) {
    if (out == nullptr) {
        return false;
    }
    // The two levels every style is built out of, so a weight is always stated
    // relative to the ground it is drawn on rather than as a literal grey.
    const float neutral = gizmoNeutralLevel(background);
    float highlight[3] = {0.0f, 0.0f, 0.0f};
    gizmoHighlightColor(background, highlight);

    SketchOverlayStyleWeights weights;
    bool mapped = false;
    // Deliberately no `default:`. A style that reaches the renderer with no
    // mapping is invisible rather than loud, so the ONE cheap guard against
    // that happening again is the compiler's own -Wswitch on this switch,
    // beside the exhaustive case in the gizmo self-test.
    switch (style) {
        case SketchOverlayStyle::GridMinor:
            weights.neutralLevel = neutral;
            weights.alpha = kGizmoAxisAlpha * 0.22f;
            mapped = true;
            break;
        case SketchOverlayStyle::GridMajor:
            weights.neutralLevel = neutral;
            weights.alpha = kGizmoAxisAlpha * 0.45f;
            mapped = true;
            break;
        case SketchOverlayStyle::Axes:
            weights.neutralLevel = neutral;
            weights.alpha = kGizmoAxisAlpha * 0.9f;
            mapped = true;
            break;
        case SketchOverlayStyle::Entities:
            // Entities read at full weight in a level that stands off the grid:
            // the neutral pulled toward the highlight's own level.
            weights.neutralLevel = neutral * 0.35f + highlight[0] * 0.65f;
            weights.alpha = kGizmoAxisAlpha;
            mapped = true;
            break;
        case SketchOverlayStyle::Dimension:
            // A technical-drawing annotation, not geometry: it stands at the
            // entity level so it is never mistaken for the grid, and just under
            // the entity WEIGHT so it never out-reads the stroke it measures.
            // Its vertices all carry the emphasis tag, so the colour it is
            // actually drawn in is the highlight the gizmo already owns and no
            // new hue enters the viewport for it.
            weights.neutralLevel = neutral * 0.35f + highlight[0] * 0.65f;
            weights.alpha = kGizmoAxisAlpha * 0.85f;
            mapped = true;
            break;
    }
    if (!mapped) {
        // A code that is not a style at all. The renderer skips such a range
        // rather than recording a draw nothing could see.
        return false;
    }
    *out = weights;
    return true;
}

}  // namespace forgeshape
