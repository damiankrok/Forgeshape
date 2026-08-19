// Stable native object identity.
//
// This lives in its own header so that the mesh layer and the selection layer
// can both name an ObjectId without either depending on the other. The identity
// RULES are still owned by forgeshape_selection.h; this file only carries the
// type and the reserved values, so that a published mesh revision can record
// which object it belongs to without dragging camera/input types along.
#pragma once

#include <cstdint>

namespace forgeshape {

// Deliberately an opaque integer minted by the selection layer and nothing else.
// It is NOT a pointer, NOT a list index, NOT a Vulkan or renderer handle and NOT
// a display name, so it stays valid when GPU buffers are recreated, when the
// Surface is destroyed and rebuilt, and when geometry is re-uploaded.
using ObjectId = uint64_t;

// Reserved: "nothing is selected".
constexpr ObjectId kNoObject = 0;

// The one selectable object in the current foundation: the bootstrap demo
// object. Every mesh revision published so far belongs to it, which is exactly
// why replacing the mesh cannot change what is selected.
constexpr ObjectId kDemoCubeObjectId = 1;

// Since Stage 007 that same identity is the product Construction box. It is
// deliberately the SAME value: the object did not change, only what generates
// its geometry did, so nothing about selection had to be re-established.
constexpr ObjectId kConstructionBoxObjectId = kDemoCubeObjectId;

}  // namespace forgeshape
