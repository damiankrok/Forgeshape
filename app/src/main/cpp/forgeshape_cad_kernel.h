// The CAD boolean kernel seam (`CAD-VERTICAL-SLICE-R1`, GATE-KERNEL).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer, no
// camera. This header is the ONLY thing the rest of ForgeShape knows about the
// geometry kernel. Every type below is ForgeShape's own; the implementation is
// the one translation unit that includes a vendored Manifold header
// (third_party/manifold, Apache-2.0, pinned at 3.5.4), so replacing the kernel
// is a change to one file and not a change to the CAD domain.
//
// What the kernel is for, and what it is not
// ------------------------------------------
// A CAD Body's truth is its feature chain -- sketches, regions, extents and
// operations (`forgeshape_cad_body.h`). The kernel never sees that truth. It is
// handed DERIVED closed solids in the body's local binary64 metres and returns
// a derived closed solid; the chain regenerates through it every time, and no
// kernel output is ever stored, serialized or read back as authored data.
//
// Every solid carries ONE FACE TAG PER TRIANGLE. The caller mints the tags
// (an index into its own table of semantic faces), the kernel carries them
// through the boolean, and a triangle of the result therefore still says which
// semantic face it came from. That is what lets a tap on a triangle of an
// Add/Cut result resolve to a stable `(feature, face token)` rather than to a
// triangle index, which is never identity.
//
// Fail closed, never repair
// -------------------------
// An input that is not a closed, finite, positively-oriented 2-manifold is
// refused by name. In particular a solid whose triangles wind INWARD (negative
// volume) is `InvertedInput`: the kernel would read it as a hole and a union
// would quietly subtract, which is exactly the silent inversion the gate
// forbids. A result that is not a valid manifold, is non-finite or exceeds the
// bound is refused too. Nothing throws: the vendored library is built without
// its debug/assert paths, and every status is a value.
//
// Deterministic
// -------------
// The kernel runs single-threaded (`MANIFOLD_PAR=-1`). The same inputs in the
// same order give the same result, and the output is canonicalised here --
// triangles grouped by ascending face tag, vertices renumbered in first-use
// order -- so the derived mesh a body publishes is a pure function of its
// authored chain.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_workplane.h"

namespace forgeshape {

// A closed solid in body-local metres. `positions` is xyz triples; `indices`
// is three per triangle, counter-clockwise seen from OUTSIDE; `faceTags` is one
// per triangle.
struct CadSolid {
    std::vector<double> positions;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> faceTags;

    uint32_t vertexCount() const { return static_cast<uint32_t>(positions.size() / 3u); }
    uint32_t triangleCount() const { return static_cast<uint32_t>(indices.size() / 3u); }
    bool empty() const { return indices.empty(); }
};

enum class CadKernelStatus : uint8_t {
    Ok,
    // Malformed arrays: an index out of range, a tag count that is not one per
    // triangle, a position count that is not a multiple of three.
    InvalidInput,
    // A coordinate is NaN or infinite.
    NonFinite,
    // The input is not a closed 2-manifold (an open edge, an edge on more than
    // two triangles).
    NotManifold,
    // The input winds inward: its signed volume is not positive.
    InvertedInput,
    // The result would exceed kMaxCadKernelTriangles.
    ResultTooLarge,
    // The kernel reported any other error.
    KernelError,
};

const char* cadKernelStatusName(CadKernelStatus status);

enum class CadBooleanOp : uint8_t {
    Union,
    Difference,
};

// What the operation rules measure about a solid. Derived, never stored.
struct CadSolidMeasure {
    double volume = 0.0;
    // Connected shells. A union that ADDS a shell is a disjoint Add.
    uint32_t components = 0;
    uint32_t triangles = 0;
};

// The largest result the kernel may return. Far above anything a phone sketch
// produces (a 32-segment circle cut through a box is a few hundred triangles);
// it exists so no chain of edits can grow a body without bound.
constexpr uint32_t kMaxCadKernelTriangles = 262144;

// Checks one solid and measures it. Ok means: well-formed arrays, finite, a
// closed 2-manifold and a strictly positive volume.
CadKernelStatus cadKernelValidateSolid(const CadSolid& solid, CadSolidMeasure* outMeasure);

// Measures a solid the kernel itself produced (no orientation refusal). An
// empty solid measures zero everywhere.
CadKernelStatus cadKernelMeasure(const CadSolid& solid, CadSolidMeasure* outMeasure);

// ONE boolean: `a op b`. Both inputs are validated first. The result is
// canonical (see the file comment) and may be EMPTY -- a difference that
// removes everything -- which is a rule decision for the caller, not a kernel
// failure. Writes nothing on any refusal.
CadKernelStatus cadKernelBoolean(const CadSolid& a, const CadSolid& b, CadBooleanOp op,
                                 CadSolid* out);

// Triangulates one planar region: `loops[0]` is the outer boundary and every
// further loop is a hole, each a simple polygon in (u, v) without a repeated
// closing vertex, in any orientation. Indices address the loops' vertices
// concatenated in order, and every triangle is counter-clockwise in (u, v).
// Used by the cap generator for a region with holes; a simple polygon still
// goes through the product's own ear clipper, so a pre-existing body's mesh
// does not move by one bit.
CadKernelStatus cadKernelTriangulateRegion(const std::vector<std::vector<SketchPoint>>& loops,
                                           std::vector<uint32_t>* outIndices);

// "manifold-3.5.4": what the evidence and the diagnostics name.
const char* cadKernelIdentity();

}  // namespace forgeshape
