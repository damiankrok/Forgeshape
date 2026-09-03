// The semantic face topology of a CAD extrusion (`CAD-A3`, `ARCH-OWNER-13`).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer. It turns
// a `CadBodyState` into a bounded set of PLANAR faces, each with a stable
// SEMANTIC identity (`CadFaceToken` -- feature lineage, never a triangle index)
// and a deterministic plane FRAME in the body's own local space. A sketch can
// then be supported by one of these faces and stay associated across a supported
// parent edit, a save/open, an undo and a regeneration.
//
// Which faces exist
// -----------------
// An extrusion has two caps and one planar side per profile edge:
//
//   * `CapPlane` -- the cap lying on the producing sketch's support plane;
//   * `CapFar`   -- the cap at the far end of the extrusion;
//   * `Side`     -- one per profile edge, identified by (edgeEntityId,
//                   edgeLocalIndex). A rectangle has four, a closed polyline n,
//                   a line chain one per line. A CIRCLE's side is cylindrical
//                   and is NOT eligible for a 2D sketch, though its ranges are
//                   still reported so a tap on it resolves cleanly.
//
// The frame
// ---------
// Each face carries `origin`, `u`, `v`, `n` -- a right-handed orthonormal frame
// (`u x v = n`) with `n` the OUTWARD normal, in the body's LOCAL space. A child
// sketch authors on its own canonical local XY; `cadFaceFrameMatrix` maps that
// onto the producer's face, and the child's world model is the producer's world
// model times that matrix. The frame is derived deterministically from the
// authored state, so it does not flip after a save, an edit, an undo or a
// regeneration.
//
// The lineage signature
// ---------------------
// `cadTopologySignature` hashes the SET of face tokens a body exposes. A
// supported edit -- a rectangle's size, a circle's radius, a depth, a direction
// -- does not change that set and so keeps a reference valid; a change that
// removed the referenced face changes the signature and the reference fails
// closed rather than retargeting to the nearest face.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_math.h"
#include "forgeshape_sketch.h"

namespace forgeshape {

// One planar face of an extrusion, in the body's LOCAL space.
struct CadFace {
    CadFaceToken token{};
    // Right-handed orthonormal, `n` outward. `u x v = n`.
    Vec3 origin{0.0f, 0.0f, 0.0f};
    Vec3 u{1.0f, 0.0f, 0.0f};
    Vec3 v{0.0f, 1.0f, 0.0f};
    Vec3 n{0.0f, 0.0f, 1.0f};
    // Whether a 2D sketch may be supported here. Both caps are eligible; a side
    // is eligible unless it is a circle's cylindrical side.
    bool eligible = true;
};

// The matrix mapping a child's local XY-space onto this face: columns u, v, n,
// translation origin. Composing the producer's world model with it places the
// child's local space physically on the face.
inline Mat4 cadFaceFrameMatrix(const CadFace& face) {
    return mat4FromBasis(face.u, face.v, face.n, face.origin);
}

// Enumerates every planar face of the body's current extrusion, in a
// deterministic order: `CapPlane`, `CapFar`, then one `Side` per profile edge in
// profile-edge order. Returns the CAD status of the underlying validation
// (`Ok` and a filled list on success), and nothing on any refusal.
CadStatus enumerateCadFaces(const CadBodyState& state, std::vector<CadFace>* out);

// The triangle-index RANGES of the body's generated mesh, each tagged with the
// semantic face token it belongs to, in the exact order `generateCadMesh`
// emits them. Transient: it exists so a rendered-triangle pick can resolve to a
// semantic face, and it is NEVER persisted. `eligible` mirrors the face's.
struct CadFaceRange {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    CadFaceToken token{};
    bool eligible = true;
};

CadStatus cadFaceRanges(const CadBodyState& state, std::vector<CadFaceRange>* out);

// Resolves one semantic token to its current face frame. Fails closed
// (`ProfileNotFound`) when the token names no face of the current topology --
// which is what makes a stale reference refuse rather than retarget.
CadStatus resolveCadFace(const CadBodyState& state, const CadFaceToken& token, CadFace* out);

// A deterministic signature of the body's face TOPOLOGY (the set of tokens),
// independent of the sizes, the depth and the direction. Two states with the
// same set of faces share it; a state whose profile changed structure does not.
// Zero when the state has no closed profile.
uint64_t cadTopologySignature(const CadBodyState& state);

}  // namespace forgeshape
