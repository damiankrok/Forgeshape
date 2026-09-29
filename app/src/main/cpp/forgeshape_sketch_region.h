// Planar sketch REGIONS: the areas a user picks to extrude, including an outer
// boundary with holes (`CAD-VERTICAL-SLICE-R1`).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer, no
// camera, no pixel. Everything here is derived from a sketch's authored
// entities and is regenerated whenever it is needed; nothing is stored except
// the SELECTION a feature makes, and that is stored by semantic identity.
//
// What a region is
// ----------------
// `extractClosedProfiles` reads every closed, simple loop out of a sketch. A
// region is what those loops enclose once nesting is taken into account:
//
//     region(L) = the interior of loop L
//                 minus the interiors of L's DIRECT children
//
// where B is a child of L when L is the smallest loop that CLEANLY contains B
// -- B lies strictly inside L and no edge of one touches or crosses an edge of
// the other. Every loop yields exactly one region, so a rectangle containing a
// circle yields two: the disk (the circle's region) and the rectangle minus the
// disk (the rectangle's region, with the circle as its hole). Deeper nesting
// follows by the same sentence: a ring around an island is the outer loop's
// region with one hole, and the island is its own region inside that hole --
// even/odd, without a second rule.
//
// Identity is semantic, never an index
// ------------------------------------
// A region is named by the anchor entity of its OUTER loop (the same anchor a
// profile always had: a rectangle, circle or polyline is its own anchor, a
// chain of lines/arcs/splines is anchored by its smallest member id), and a
// SELECTION also records the anchors of the holes it was chosen with. Nothing
// is a triangle index, a tessellation index or a list position. When the
// stored holes no longer equal the derived ones -- a loop was added inside a
// selected region, or one was removed -- the selection is refused by name
// (`ProfileRegionMismatch`) rather than silently re-read as a different area.
//
// What is refused, and what is deliberately not an arrangement
// ------------------------------------------------------------
// This is nesting, not a planar arrangement: loops never split each other. A
// loop that TOUCHES or CROSSES another is therefore never a hole and never has
// that loop as a hole; each such loop stays its own region exactly as every
// earlier version read it -- which is what keeps every v1..v4 `CADB` record
// meaning what it meant. What cannot be resolved without guessing is refused
// by name instead:
//   * a region whose holes touch or cross each other -- `OverlappingHoles`
//     (the region is listed but not selectable);
//   * a selection of two regions that overlap, touch, or share a boundary
//     loop (a region and its own hole) -- `OverlappingRegions`;
//   * self-intersecting, zero-area, open or forked loops -- refused where they
//     always were, by the loop extraction.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_sketch.h"

namespace forgeshape {

// How many regions one feature may select, and how many holes one region may
// carry. Both keep a feature -- and so a history step and a `.forge` record --
// bounded; both are far above anything a finger draws on a phone.
constexpr uint32_t kMaxProfileRegions = 16;
constexpr uint32_t kMaxRegionHoles = 64;

// A stored selection of ONE region: its outer loop's anchor and the anchors of
// the holes it was chosen with, ascending. Plain, copyable, comparable.
struct ProfileRegionRef {
    SketchEntityId outerAnchorId = kNoSketchEntity;
    std::vector<SketchEntityId> holeAnchorIds;
};

bool sameProfileRegionRef(const ProfileRegionRef& a, const ProfileRegionRef& b);

// One derived region.
struct SketchRegion {
    SketchEntityId outerAnchorId = kNoSketchEntity;
    // The direct children, ascending by anchor.
    std::vector<SketchEntityId> holeAnchorIds;
    // Indices into the extraction's `loops.profiles`.
    uint32_t outerLoop = 0;
    std::vector<uint32_t> holeLoops;
    // How many loops cleanly contain the outer loop.
    uint32_t depth = 0;
    // Outer area minus the holes' areas, square metres.
    double area = 0.0;
    // Ok, or why the region cannot be selected (`OverlappingHoles`).
    CadStatus status = CadStatus::Ok;
    // A point strictly inside the region and outside every hole: where a label
    // or a tap target belongs. Never the centroid, which for a centred hole
    // lies in the hole.
    SketchPoint interiorPoint{};
    // The region's area centroid (outer minus holes). May lie in a hole.
    SketchPoint centroid{};
};

struct SketchRegionExtraction {
    // Every valid closed loop, ascending by anchor, and every rejected one.
    ProfileExtraction loops;
    // One region per loop, in the same order.
    std::vector<SketchRegion> regions;
    // Per loop: the index of the loop that directly contains it, or -1.
    std::vector<int32_t> parent;
    // Per loop pair (row-major, loops x loops): 1 when their edges touch or
    // cross. Bounded by kMaxSketchEntities squared.
    std::vector<uint8_t> conflict;
    // Per loop pair (row-major): 1 when the first CLEANLY contains the second.
    std::vector<uint8_t> contains;

    bool loopsConflict(uint32_t a, uint32_t b) const {
        const size_t n = loops.profiles.size();
        return conflict[static_cast<size_t>(a) * n + b] != 0u;
    }
    bool loopContains(uint32_t a, uint32_t b) const {
        const size_t n = loops.profiles.size();
        return contains[static_cast<size_t>(a) * n + b] != 0u;
    }
};

// Derives the regions of a sketch. Never refuses as a whole: a loop that fails
// its own rules is in `loops.rejections`, and a region that cannot be selected
// carries its reason in `status`.
SketchRegionExtraction extractSketchRegions(const CadSketch& sketch);

const SketchRegion* findSketchRegion(const SketchRegionExtraction& extraction,
                                     SketchEntityId outerAnchorId);

// The canonical stored form of one derived region.
ProfileRegionRef sketchRegionRef(const SketchRegion& region);

// The whole rule for a stored selection against the regions of its sketch.
// Ok, or the first refusal:
//   AmbiguousProfile      empty (nothing chosen) while more than one region exists
//   ProfileNotFound       empty with no region at all, or an outer anchor that
//                         names no loop
//   TooManyRegions        more than kMaxProfileRegions, or a region with more
//                         than kMaxRegionHoles holes
//   ProfileRegionMismatch not canonical (outer anchors not strictly ascending,
//                         holes not strictly ascending) or the stored holes are
//                         not exactly the derived ones
//   OverlappingHoles      a chosen region's holes touch or cross
//   OverlappingRegions    two chosen regions overlap, touch or share a loop
CadStatus validateRegionSelection(const SketchRegionExtraction& extraction,
                                  const std::vector<ProfileRegionRef>& selection);

// The region under a sketch point: the innermost loop that strictly contains
// it (the smallest by area; ties by anchor) names it. False, writing nothing,
// when the point is inside no loop.
bool sketchRegionAt(const SketchRegionExtraction& extraction, const SketchPoint& point,
                    SketchEntityId* outOuterAnchorId);

// Adds the region to the selection, or removes it when it is already there,
// keeping canonical order. Pure; says nothing about validity.
std::vector<ProfileRegionRef> toggleRegionSelection(const std::vector<ProfileRegionRef>& selection,
                                                    const SketchRegion& region);

// The polygons of one region: the outer loop counter-clockwise first, then
// each hole in ascending anchor order, also as extracted (counter-clockwise).
// Empty when the region does not exist.
std::vector<std::vector<SketchPoint>> sketchRegionLoops(const SketchRegionExtraction& extraction,
                                                        const SketchRegion& region);

// The segments of a hatch drawn over a region: horizontal lines `spacing`
// apart in (u, v), clipped to the region by the even-odd rule, so a hole is
// visibly EMPTY. Bounded to kMaxRegionHatchLines lines. Presentation only.
constexpr uint32_t kMaxRegionHatchLines = 96;
std::vector<SketchPoint> sketchRegionHatch(const SketchRegionExtraction& extraction,
                                           const SketchRegion& region, double spacing);

// Point strictly inside a polygon by the even-odd rule; a point ON an edge is
// not strictly inside.
bool sketchPointStrictlyInside(const SketchPoint& point, const std::vector<SketchPoint>& polygon);

}  // namespace forgeshape
