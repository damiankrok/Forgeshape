// Linear, Circular and Mirror patterns of a CAD Body's later features
// (`CAD-PATTERN-R1`).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no camera. The
// authored truth is `CadBodyState::patterns` (forgeshape_cad_body.h); this
// module derives everything else from it -- the rigid transform of every
// instance, the moved tool solid, and whether a list of patterns is valid --
// and owns the three user acts. Regeneration (`regenerateCadBody`) applies the
// instances through the one kernel seam.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_kernel.h"
#include "forgeshape_history.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// One instance's rigid motion in body-local binary64: p' = R p + t, with R a
// rotation or (Mirror) a reflection. `reflects` says the winding must be
// reversed so the moved solid still winds outward.
struct CadPatternTransform {
    double r[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // row-major
    double t[3] = {0, 0, 0};
    bool reflects = false;
};

// Every instance transform of `pattern` EXCEPT the original's identity, in a
// deterministic order (Linear: row by row over the second direction, then the
// first; Circular: by increasing angle; Mirror: the one reflection). Empty for
// a pattern that does not validate.
std::vector<CadPatternTransform> cadPatternInstanceTransforms(const CadFeaturePattern& pattern);

// Moves `solid` by `transform` in place, reversing every triangle's winding
// when the transform reflects.
void transformCadSolid(const CadPatternTransform& transform, CadSolid* solid);

// The patterns' own rules, without the kernel: ids, bounds, canonical form,
// and every named feature an earlier LATER feature of the chain. `outFailedId`
// names the pattern that failed.
CadStatus validateCadFeaturePatterns(const CadBodyState& state, uint32_t* outFailedId = nullptr);

// A readable default spacing for a new Linear pattern of `featureIds`: the
// first feature's own extent along `axis` plus a quarter of it, rounded up to
// a 1/2/5 x 10^k value, so the first Apply lands the copies beside each other
// rather than on top. Zero when the feature does not derive.
double suggestCadPatternSpacing(const CadBodyState& state, const std::vector<uint32_t>& featureIds,
                                CadBodyAxis axis);

// The body-local axis a Circular pattern of `featureIds` turns about by
// default, and the point it passes through: the base feature's normal through
// the base region's centroid -- the middle of a round flange. False when the
// base does not derive.
bool suggestCadPatternCircularAxis(const CadBodyState& state, CadBodyAxis* outAxis,
                                   double outCenter[3]);

// --- the three user acts, each ONE `ScopedConstructionEdit` (one Undo) -----
//
// Each judges the whole requested state through `CadBody::applyState`, which
// validates and regenerates it and writes nothing unless all of it passes.

CadStatus addCadFeaturePatternToBody(ConstructionScene& scene, ConstructionHistory& history,
                                     ObjectId bodyId, CadFeaturePattern pattern,
                                     uint32_t* outPatternId = nullptr);

// Replaces pattern `patternId`'s values; its id stays.
CadStatus editCadFeaturePatternOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                      ObjectId bodyId, uint32_t patternId,
                                      CadFeaturePattern pattern);

CadStatus deleteCadFeaturePatternOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                        ObjectId bodyId, uint32_t patternId);

}  // namespace forgeshape
