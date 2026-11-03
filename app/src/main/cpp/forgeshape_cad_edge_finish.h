// Chamfer and Fillet of a CAD Body's straight edges (`CAD-EDGE-FINISH-R1`).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer, no
// camera. Everything here is DERIVED from a `CadBodyState`: which edges a
// feature's own extrusion has, where each one runs, which of them can be
// finished, and the closed tool solid a Chamfer or a Fillet subtracts. Nothing
// here is stored, serialized or read back; the authored truth is the
// `CadEdgeFinish` list (forgeshape_cad_body.h).
//
// An edge is found on the feature's OWN prism (`appendCadFeatureSolid`, the
// same solid regeneration feeds the kernel), never on the boolean result: a
// triangle of the result is never identity. Two prism triangles that share a
// mesh edge and carry different face tags put that mesh edge on the semantic
// edge between those two faces; a STRAIGHT edge is one whose pieces are all
// collinear, and only such an edge between two FLAT faces may be finished.
//
// The tool is a prism along the edge whose cross-section is the corner the
// finish removes, widened OUTSIDE the material so the kernel never sees a face
// coplanar with one of the edge's own faces:
//
//     Chamfer: the corner E cut back by the chord between the two setback
//              points A = E + t*inA and B = E + t*inB (t = distance);
//     Fillet:  the corner E cut back by the circular arc of radius r tangent to
//              both faces at A and B (t = r / tan(alpha / 2)), sampled at the
//              circle density every curve in the product uses (32 per turn).
//
// At an end where the edge leaves the material (a convex corner of the part)
// the tool runs on past the end, so the finish reaches the corner cleanly; at an
// end where material continues (the edge meets another solid) it stops flush,
// so nothing beyond the edge is cut.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_kernel.h"
#include "forgeshape_history.h"
#include "forgeshape_math.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// One edge of one feature's own extrusion, in body-local binary64.
struct CadFeatureEdge {
    CadEdgeRef ref{};
    // The edge runs start -> end; `direction` is the unit vector between them.
    DVec3 start{0.0, 0.0, 0.0};
    DVec3 end{0.0, 0.0, 0.0};
    DVec3 direction{1.0, 0.0, 0.0};
    double length = 0.0;
    // The two faces' outward normals and, in each face, the unit direction
    // perpendicular to the edge pointing AWAY from it into the face.
    DVec3 normalA{0.0, 0.0, 1.0};
    DVec3 normalB{0.0, 0.0, 1.0};
    DVec3 inwardA{1.0, 0.0, 0.0};
    DVec3 inwardB{0.0, 1.0, 0.0};
    // The material's interior angle between the two faces, in degrees.
    double angleDegrees = 90.0;
    // How far each face reaches away from the edge (what a setback must fit).
    double widthA = 0.0;
    double widthB = 0.0;
    bool straight = false;
    bool convex = false;
    // Whether a Chamfer or a Fillet may stand on it, and if not, why.
    bool eligible = false;
    CadStatus why = CadStatus::EdgeNotEligible;
};

// Every edge of the feature `featureId`'s own extrusion, eligible or not, in a
// deterministic order (by face table index pairs). Fails closed by the chain's
// own reason when the feature does not derive, `EdgeNotFound` for a feature id
// the chain lacks.
CadStatus enumerateCadFeatureEdges(const CadBodyState& state, uint32_t featureId,
                                   std::vector<CadFeatureEdge>* out);

// Every ELIGIBLE edge of every feature a finish may target, in chain order.
CadStatus enumerateCadFinishableEdges(const CadBodyState& state,
                                      std::vector<CadFeatureEdge>* out);

// Resolves one edge reference exactly, or refuses by name: `EdgeNotFound` when
// the feature or either face is missing or the faces do not meet,
// `EdgeFinishTargetUnsupported` when the feature cuts or revolves, and the
// edge's own `why` when it exists but is not eligible. Never the nearest edge.
CadStatus resolveCadFinishEdge(const CadBodyState& state, const CadEdgeRef& ref,
                               CadFeatureEdge* out);

// The setback t along each face: the distance for a Chamfer, r / tan(alpha/2)
// for a Fillet of radius r. Zero for a degenerate angle.
double cadEdgeFinishSetback(CadEdgeFinishKind kind, double distance, double angleDegrees);

// The finish list's own rules, without the kernel: ids, kinds, distances, the
// edge lists (non-empty, bounded, canonical, distinct) and every edge resolving
// eligible with a setback that fits both faces.
CadStatus validateCadEdgeFinishes(const CadBodyState& state, uint32_t* outFailedId = nullptr);

// The closed, outward-wound tool solid one finish subtracts along one edge,
// every triangle tagged `tag`. `body` is the solid the tool will be subtracted
// from: it decides, per end, whether the tool runs on past the edge.
CadStatus buildCadEdgeFinishTool(const CadFeatureEdge& edge, CadEdgeFinishKind kind,
                                 double distance, const CadSolid& body, uint32_t tag,
                                 CadSolid* out);

// The eligible edges of ONE planar face of `featureId`'s own extrusion: what
// "finish every edge of this face" means. Empty, with Ok, for a face none of
// whose edges may be finished.
CadStatus cadFinishableEdgesOfFace(const CadBodyState& state, uint32_t featureId,
                                   const CadFaceToken& face, std::vector<CadEdgeRef>* out);

// A readable default distance for a new finish over `edges`: a 1/2/5 x 10^k
// value near a fifth of the narrowest face the setback must fit in, so the
// first Apply always succeeds. Zero when no edge resolves.
double suggestCadEdgeFinishDistance(const CadBodyState& state, CadEdgeFinishKind kind,
                                    const std::vector<CadEdgeRef>& edges);
// The same over edges already resolved.
double suggestCadEdgeFinishDistance(CadEdgeFinishKind kind,
                                    const std::vector<CadFeatureEdge>& edges);

// Resolves several refs with ONE enumeration per feature: what a per-frame
// read of an offer needs. Fails closed on the first ref that does not resolve
// eligible, by that ref's own reason.
CadStatus resolveCadFinishEdges(const CadBodyState& state, const std::vector<CadEdgeRef>& refs,
                                std::vector<CadFeatureEdge>* out);

// --- the three user acts, each ONE `ScopedConstructionEdit` (one Undo) -----
//
// Each judges the whole requested state through `CadBody::applyState`, which
// validates and regenerates it and writes nothing unless all of it passes, so a
// refusal leaves the body and the history exactly as they were.

// Adds a finish of `kind` and `distance` over `edges` to body `bodyId`.
CadStatus addCadEdgeFinishToBody(ConstructionScene& scene, ConstructionHistory& history,
                                 ObjectId bodyId, CadEdgeFinishKind kind, double distance,
                                 std::vector<CadEdgeRef> edges, uint32_t* outFinishId = nullptr);

// Changes finish `finishId`'s kind and distance; its edges stay.
CadStatus editCadEdgeFinishOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                  ObjectId bodyId, uint32_t finishId, CadEdgeFinishKind kind,
                                  double distance);

// Removes finish `finishId`. Its id is never minted again on this branch.
CadStatus deleteCadEdgeFinishOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                    ObjectId bodyId, uint32_t finishId);

// Whether a point lies inside a closed solid (ray parity). Derived geometry
// only; used to decide where a tool may run past its edge.
bool cadSolidContainsPoint(const CadSolid& solid, const DVec3& point);

// Whether `edge` -- an edge of one feature's OWN prism -- is a real OUTSIDE
// convex edge of `body`, the part as it stands (`CAD-EDGE-FINISH-R2`): at
// samples along its middle, the space just beyond each of its two faces is
// empty and the corner between them is material. False where two features
// meet -- a boss's foot on the block it stands on, a face flush with a
// neighbour's, an edge another feature swallowed -- where cutting the
// prism's corner would carve a groove into the part instead of rounding it.
bool cadEdgeIsOutsideEdgeOfBody(const CadFeatureEdge& edge, const CadSolid& body);

// A regenerated body's mesh as a solid for containment questions.
CadSolid cadSolidFromBodyMesh(const CadBodyMesh& mesh);

}  // namespace forgeshape
