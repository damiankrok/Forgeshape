// The derived geometry of a CAD Body's feature chain (`CAD-VERTICAL-SLICE-R1`).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer, no
// camera. Everything here is DERIVED from a `CadBodyState` -- where each
// feature's sketch stands in the body, the planar faces each feature's own
// extrusion exposes, the closed solid it contributes -- and is regenerated
// whenever it is needed. Nothing here is stored, serialized or read back.
//
// Where a feature stands
// ----------------------
// The base feature's sketch lies on its workplane at the body's local origin,
// exactly as R0 put it. A LATER feature's sketch lies on a planar face of an
// EARLIER feature, named semantically (`CadFeatureSupport`: feature id, face
// token, lineage). The face's frame is derived from that earlier feature's OWN
// sketch and extrusion -- never from the boolean result, never from a triangle
// -- so a later feature's placement is a pure function of the chain before it,
// and a size edit upstream carries it along while a structural edit refuses.
//
// Everything is binary64 until the render mesh: a tool sketched on a cap must
// lie EXACTLY in that cap's plane for the kernel to see a flush face.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_kernel.h"
#include "forgeshape_math.h"
#include "forgeshape_sketch_region.h"

namespace forgeshape {

// A right-handed orthonormal frame in body-local metres: a point with sketch
// coordinates (a, b) at offset c along the normal is `origin + u*a + v*b + n*c`.
struct CadFrame64 {
    DVec3 origin{0.0, 0.0, 0.0};
    DVec3 u{1.0, 0.0, 0.0};
    DVec3 v{0.0, 1.0, 0.0};
    DVec3 n{0.0, 0.0, 1.0};
};

DVec3 cadFramePoint(const CadFrame64& frame, double a, double b, double c);

// One planar face of one feature's own extrusion, in body-local binary64.
struct CadFeatureFace {
    CadFaceToken token{};
    CadFrame64 frame;
    bool eligible = true;
};

// Everything derived about one feature of the chain.
struct CadFeatureGeometry {
    uint32_t featureId = kCadFeatureId;
    CadFeatureOperation operation = CadFeatureOperation::NewBody;
    // Copies, so the geometry never dangles into a state that changed.
    CadSketch sketch;
    ExtrudeFeature extrude;
    SketchRegionExtraction regions;
    // Indices into `regions.regions`, in selection (ascending anchor) order.
    std::vector<uint32_t> chosen;
    // The UNION of the chosen regions (`mergeSelectedRegions`,
    // `CAD-FOUNDATION-C1`): what is actually extruded, component by component.
    // For every selection that does not choose a region beside its own hole it
    // is exactly the chosen regions, in the same order, with the same holes.
    std::vector<SketchRegionComponent> components;
    // Sketch (u, v) at offset w along the normal -> body-local.
    CadFrame64 placement;
    // The solid spans nearOffset (-B) .. farOffset (+A) along the normal.
    double nearOffset = 0.0;
    double farOffset = 0.0;
    // The cap the extrusion grows FROM (CapPlane) and TO (CapFar).
    double planeCapOffset = 0.0;
    double farCapOffset = 0.0;
    double extrudeSign = 1.0;
    // CapPlane, CapFar, then one Side per edge: union component by component
    // (ascending outer anchor), the outer loop in polygon order and then each
    // hole (ascending anchor) in its own polygon order. A loop absorbed into
    // the union bounds nothing and so exposes no face.
    std::vector<CadFeatureFace> faces;
    // The `.forge` lineage signature of this feature's face topology
    // (DATA_PACKAGE_SPEC.md §7c, generalized in §7f). For a single region
    // without holes it is bit-identical to what `cadTopologySignature` always
    // computed.
    uint64_t signature = 0;
};

// Derives the chain's geometry in order, through `throughFeatureId` (every
// feature when omitted), validating each feature's own rule and each later
// feature's support against the features before it. Writes nothing on a
// refusal; `outFailedFeatureId` names the feature that refused.
CadStatus buildCadChainGeometry(const CadBodyState& state, std::vector<CadFeatureGeometry>* out,
                                uint32_t* outFailedFeatureId = nullptr,
                                uint32_t throughFeatureId = 0xFFFFFFFFu);

// The geometry of the one feature named, and only what it needs before it.
CadStatus buildCadFeatureGeometry(const CadBodyState& state, uint32_t featureId,
                                  CadFeatureGeometry* out);

// Appends one feature's extrusion -- every union component of the selected
// regions, holes as inner walls, one prism per component so no shared wall is
// ever emitted twice -- as a closed, outward-wound solid whose face tags are
// `tagOffset + index into geometry.faces`.
CadStatus appendCadFeatureSolid(const CadFeatureGeometry& geometry, uint32_t tagOffset,
                                CadSolid* solid);

// Whether `solid` still carries material on `frame`'s plane, facing along its
// normal: a support face an earlier Cut removed entirely does not. Geometric,
// not by tag, because a coplanar merge may legitimately re-tag a face.
bool cadSolidHasFaceOn(const CadSolid& solid, const CadFrame64& frame);

// The signed volume of a closed solid, by the divergence theorem.
double cadSolidVolume(const CadSolid& solid);

}  // namespace forgeshape
