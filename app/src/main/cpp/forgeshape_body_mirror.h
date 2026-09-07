// Construction Mirror — the exact reflection of a Construction Body across one
// principal WORLD plane, expressed as a proper rotation (`MIRROR-01`).
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no renderer, no UI
// type and no filesystem. Like `forgeshape_body_dimensions.h` this is a pure
// function over VALUES — it takes no scene, no history, no body, no camera and
// no mesh — so the one arithmetic below has exactly one implementation and the
// command that drives it (`mirrorSceneBody`, beside the other object commands)
// only decides eligibility, identity and the transaction.
//
// Why a rotation and not a negative scale
// ---------------------------------------
// Scale in this product is strictly positive and always will be: a negative
// factor is a Mirror the transform deliberately does not have, because it
// inverts winding, flips every normal and every front-face test, and makes the
// glTF exporter's own `MirroredTransform` refusal a lie (see
// `forgeshape_transform.h` and `ARCH-OWNER-07`). So a Mirror is NOT a sign on
// `S`. It is a new body whose ORIENTATION already carries the reflection.
//
// That is possible because every Construction primitive this product makes is
// symmetric under a reflection of its OWN local X axis. Writing
//
//     Qx = diag(-1, +1, +1)          the local symmetry, never stored
//     F   = the world reflection      diag with -1 in the reflected axis
//
// and taking the source's position `p`, proper rotation `R` and positive
// scale `S`, the mirrored placement is
//
//     p' = F * p
//     R' = F * R * Qx
//     S' = S
//
// with det(R') = det(F) * det(R) * det(Qx) = (-1)(+1)(-1) = +1, so `R'` is a
// PROPER rotation and no negative factor is needed anywhere.
//
// The determinant is not the proof, though — it only says the result is a legal
// orientation. The geometric claim is the identity
//
//     Model_mirror(q)  ==  F * Model_source(Qx * q)      for every local q
//
// which follows because `Qx` and `S` are both diagonal and therefore commute:
//
//     T' + R'*S*q = F*T + F*R*Qx*S*q = F*(T + R*S*(Qx*q))
//
// It holds for ANY q with no symmetry assumption at all. What the symmetry then
// adds is that `Qx` maps each primitive's generated local vertex SET onto
// itself — the six generators are all centred on the local origin and their
// rings carry `kPrimitiveRadialSegments` (32, divisible by four) samples — so
//
//     WorldGeometry(mirror) == F * WorldGeometry(source)
//
// as a set, exactly, to floating-point tolerance. `MIRROR01-07` and
// `MIRROR01-08` assert both halves rather than the determinant alone.
//
// `Qx` is ALGEBRA and is never persisted, never reaches a `.forge` byte and
// never appears in a history step: what is stored is the ordinary nine
// authoritative values the mirrored body's transform already owns, which is why
// this stage needs no format change at all.
//
// Scope: one body, one principal world plane, one new object. There is no
// arbitrary plane, no face plane, no custom workplane, no live symmetry
// modifier, no linked instance, no multi-select, no hierarchy subtree and no
// Sculpt stroke symmetry.
#pragma once

#include "forgeshape_math.h"
#include "forgeshape_scene.h"
#include "forgeshape_transform.h"

namespace forgeshape {

// The three principal WORLD planes a body may be reflected across.
//
// Named for the plane, never for the axis it negates, because that is what the
// user picks and what the axis vocabulary in the rest of the product already
// says. The axis each one reflects is `mirrorReflectedAxis` below and is stated
// in exactly one place.
enum class MirrorPlane {
    Xy,  // reflects world Z
    Xz,  // reflects world Y
    Yz,  // reflects world X
};

const char* mirrorPlaneName(MirrorPlane plane);

// 0 X, 1 Y, 2 Z — which world axis this plane negates. THE mapping.
int mirrorReflectedAxis(MirrorPlane plane);

// The JNI/UI transport form: 0 XY, 1 XZ, 2 YZ. A transport index, never an
// enum ABI value and never anything a file stores.
bool mirrorPlaneFromIndex(int index, MirrorPlane* out);
int mirrorPlaneIndex(MirrorPlane plane);

// Why a mirrored placement could not be produced.
enum class MirrorStatus {
    Ok,
    // The plane is not one of the three.
    InvalidPlane,
    // NaN or infinity somewhere in the source placement. Refused rather than
    // sanitised: a body whose transform is not a number has no reflection.
    NotFinite,
    // The reflection is finite but a value would not survive the transform's
    // own validation, so it is refused BY NAME here rather than applied and
    // then rejected one layer down.
    NotRepresentable,
};

const char* mirrorStatusName(MirrorStatus status);

// THE mirror arithmetic. One placement in, one complete placement out.
//
// Writes nothing on any status but `Ok`, so a caller that ignores the return
// value still cannot half-apply a reflection.
//
// The returned rotation comes back through `eulerFromRotationMatrix`, the ONE
// bridge between the matrix form and the authoritative Euler degrees, with the
// SOURCE's own angles as the branch hint — so the mirrored body's numbers stay
// near the ones the user recognises instead of jumping a whole turn. No second
// Euler convention is introduced here and none may be.
//
// A position component that reflects to zero is written as +0.0 rather than
// -0.0, so a body standing exactly on the plane encodes byte-identically to one
// that was typed there.
MirrorStatus mirrorPlacement(const TransformValues& source, MirrorPlane plane,
                             TransformValues* out);

// Why a BODY may not be mirrored, as opposed to why a placement may not be.
//
// Separate from `MirrorStatus` because these are facts about the object rather
// than about arithmetic, and the UI withdraws the control for them while the
// domain still refuses by name if one is somehow reached.
enum class MirrorEligibility {
    Eligible,
    // An Imported Mesh. Its geometry IS its truth and nothing here may rewrite
    // one byte of it; reflecting the placement alone would turn every triangle
    // inside out. `MIRROR-01` is Construction only.
    NotConstruction,
    // A CAD Body. Its truth is a sketch and an extrusion, and a mirrored CAD
    // Body would have to be a mirrored SKETCH — an authored-geometry act this
    // stage does not do, and one no rotation of the body could stand in for.
    NotCad,
    // The body carries a Frozen Sculpt Mesh. Sculpt vertices are the body's own
    // truth and are not symmetric under `Qx`, so the local symmetry the whole
    // contract rests on does not hold for one. Refused rather than answered
    // with geometry that is not the reflection it claims to be.
    HasSculptTruth,
    // The body has no Construction Source at all, or its parameters are not
    // finite. Defensive: no path in the product builds one.
    NoConstructionSource,
};

const char* mirrorEligibilityName(MirrorEligibility eligibility);

// Whether this body is one `MIRROR-01` can reflect, and why not when it is not.
//
// Deliberately asked of the SceneObject rather than of a representation enum,
// because "carries a Frozen Sculpt Mesh" is a fact about the body and not about
// what makes its geometry.
MirrorEligibility mirrorEligibilityOf(const SceneObject& body);

}  // namespace forgeshape
