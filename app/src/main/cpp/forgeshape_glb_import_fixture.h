// A deterministic external-GLB fixture, structurally like a low-poly sculpt
// export.
//
// WHY IT EXISTS
// -------------
// GLB-IMPORT-R1 (`ARCH-OWNER-09`) widens the diagnostic reader to ordinary
// static GLB files written by other tools. The file that motivated it is the
// owner's own low-poly character, and that binary is not in this repository —
// so the compatibility target has to be reproduced rather than copied.
//
// **This is not the owner's file, and it is not a character.** It is a
// synthetic surface that carries the same STRUCTURAL features, which are the
// only part a parser can be tested against:
//
//   * GLB 2.0, one embedded BIN chunk;
//   * one scene, one node, one mesh;
//   * a node `matrix` that is non-orthogonal, non-uniform and asymmetric, so a
//     transposed or ignored matrix moves the geometry visibly;
//   * seven TRIANGLES primitives over ONE shared POSITION accessor;
//   * about two thousand vertices and well over two thousand six hundred
//     triangles;
//   * uint32 indices;
//   * no NORMAL, so the reader must generate one;
//   * COLOR_0 as normalized unsigned short VEC4, COLOR_1 as normalized
//     unsigned byte VEC4, TEXCOORD_0 as float VEC2 — all three validated and
//     ignored;
//   * one material with `doubleSided` true;
//   * `extras` on the root, the node and the mesh.
//
// DETERMINISM
// -----------
// Every coordinate is an integer divided by a power of two, and every other
// number is an exact binary fraction, so no transcendental function and no
// platform's libm can move a byte. The same build on any device produces the
// same SHA-256, which is what lets a hash be evidence rather than a hope.
//
// The surface is deliberately ASYMMETRIC on both axes: a mirrored, transposed
// or dropped node matrix changes the world bounds, and a test can say so in
// numbers.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no filesystem.
#pragma once

#include <cstdint>
#include <vector>

namespace forgeshape {

// The fixture's shape, stated here so a test asserts against the same numbers
// the builder uses rather than against a magic constant it copied.
constexpr uint32_t kNomadLikeFixtureRows = 43;
constexpr uint32_t kNomadLikeFixtureColumns = 46;
constexpr uint32_t kNomadLikeFixturePrimitives = 7;
constexpr uint32_t kNomadLikeFixtureVertices =
        kNomadLikeFixtureRows * kNomadLikeFixtureColumns;  // 1978
constexpr uint32_t kNomadLikeFixtureTriangles =
        (kNomadLikeFixtureRows - 1) * (kNomadLikeFixtureColumns - 1) * 2;  // 3780

// Builds the complete `.glb`. Never fails: it depends on nothing outside this
// translation unit.
std::vector<uint8_t> buildNomadLikeGlbFixture();

}  // namespace forgeshape
