// Stable native object identity.
//
// Its own header so the mesh layer, the scene and the selection layer can all
// name an ObjectId without depending on one another; only the type and the
// reserved values live here.
#pragma once

#include <cstdint>

namespace forgeshape {

// Deliberately an opaque integer minted by the scene's monotonic allocator and
// nothing else -- never reused, never rolled back by Undo.
// It is NOT a pointer, NOT a list index, NOT a Vulkan or renderer handle and NOT
// a display name, so it stays valid when GPU buffers are recreated, when the
// Surface is destroyed and rebuilt, and when geometry is re-uploaded.
using ObjectId = uint64_t;

// Reserved: "nothing is selected".
constexpr ObjectId kNoObject = 0;

// The first id the scene's allocator hands out (`kFirstBodyObjectId`). The two
// names below are historical aliases of that one value -- the bootstrap mesh
// and the first Construction box were the same object -- and are kept only as
// constructor defaults; a body's real identity is minted by `ConstructionScene`.
constexpr ObjectId kDemoCubeObjectId = 1;
constexpr ObjectId kConstructionBoxObjectId = kDemoCubeObjectId;

}  // namespace forgeshape
