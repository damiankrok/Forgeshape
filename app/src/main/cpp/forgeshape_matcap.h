// ForgeShape MatCap asset — generated, not downloaded.
//
// Platform-independent: no JNI, no Android, no Vulkan and no UI type appears
// here. This module produces pixels; the renderer decides what to do with them.
//
// Asset provenance
// ----------------
// The one MatCap ForgeShape ships is COMPUTED AT RUNTIME by `generateMatCap()`
// from the closed-form lighting model written out in the implementation file.
// There is no image file in the repository, no downloaded texture, no
// third-party or copyrighted MatCap, and nothing was traced, sampled or
// converted from another application's asset. The formula below is the asset:
// it is original ForgeShape work, it is readable, and it is diffable.
//
// Generating rather than shipping a file is also the only option that respects
// the no-third-party-runtime-library rule — decoding a PNG or a JPEG would
// require a decoder ForgeShape is not allowed to depend on, and an uncompressed
// image checked into Git would be a binary blob with no reviewable provenance.
//
// Scope: exactly ONE preset. There is no MatCap library, no browser, no import,
// no per-object material and no roughness/metalness system. A second preset is
// a future decision, not a slot waiting to be filled.
#pragma once

#include <cstdint>
#include <vector>

namespace forgeshape {

// Edge length of the generated MatCap, in texels. It is square by definition:
// a MatCap is indexed by a unit normal's (x, y), so the domain is a disc
// inscribed in a square.
//
// 128 is chosen because the encoded function is smooth — it has no detail
// finer than a specular highlight several texels across — so a larger texture
// would store the same information at four times the cost. At RGBA8 this is
// 64 KiB, generated once per device initialization.
constexpr uint32_t kMatCapSize = 128;

// Four bytes per texel, R8G8B8A8_UNORM, alpha always 255.
constexpr uint32_t kMatCapBytesPerTexel = 4;
constexpr uint32_t kMatCapByteSize = kMatCapSize * kMatCapSize * kMatCapBytesPerTexel;

// Fills `out` with exactly kMatCapByteSize bytes of R8G8B8A8_UNORM texels.
//
// Deterministic and side-effect free: the same build always produces the same
// bytes, which is what lets a self-test assert properties of the asset rather
// than merely that it is non-empty.
void generateMatCap(std::vector<uint8_t>* out);

// The lookup a shader performs, evaluated on the CPU.
//
// `nx` and `ny` are a VIEW-SPACE unit normal's x and y. The MatCap convention
// is uv = n.xy * 0.5 + 0.5, so the texture's centre is a surface facing the
// camera and its rim is a silhouette. Exposed so self-tests can assert the
// asset's shading behaviour at named directions (facing, silhouette, key side,
// shadow side) without a GPU.
//
// Returns false for a direction outside the unit disc, which is not a normal
// any visible surface can have.
bool sampleMatCap(const std::vector<uint8_t>& texels, float nx, float ny, uint8_t outRgb[3]);

}  // namespace forgeshape
