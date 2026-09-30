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
//   * a selection of two regions whose loops touch or cross, or one of which
//     stands inside the other's material without being its own hole --
//     `OverlappingRegions`;
//   * self-intersecting, zero-area, open or forked loops -- refused where they
//     always were, by the loop extraction.
//
// A selection is the UNION of the atomic regions it names
// ---------------------------------------------------------
// (`CAD-FOUNDATION-C1`.) A region and its own hole -- the ring around a disk
// and the disk -- are two atomic regions that share the hole's loop as a
// boundary, and choosing both means the union: the ring with that hole filled.
// Nothing about the stored form changes to say so. The selection is still the
// list of chosen atomic regions, each with the holes it was chosen with, so
// `ProfileRegionMismatch` still guards a loop appearing or vanishing inside
// what was chosen; the union is DERIVED by `mergeSelectedRegions`, by one
// parity sentence over the nesting tree:
//
//     loop L bounds the union  <=>  selected(region(L)) != selected(region(parent(L)))
//                                   (outside every loop counts as unselected)
//
// Because nesting regions are pairwise disjoint by construction, no 2D boolean
// and no planar arrangement is needed, and every selection that was legal
// before -- none of which chose a region beside its own hole -- unions to
// exactly its own regions, with the same loops in the same order.
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
//   OverlappingRegions    two chosen regions' loops touch or cross, or one
//                         stands inside the other's material without being
//                         its own direct hole (a region beside its own hole is
//                         LEGAL: the selection means their union)
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

// One connected piece of a selection's UNION (`CAD-FOUNDATION-C1`): a real
// authored outer loop and the real authored loops bounding the holes left in
// it. Derived, never stored; named by loop anchors exactly as a region is.
struct SketchRegionComponent {
    SketchEntityId outerAnchorId = kNoSketchEntity;
    uint32_t outerLoop = 0;
    // Ascending loop index, which is ascending anchor.
    std::vector<uint32_t> holeLoops;
    std::vector<SketchEntityId> holeAnchorIds;
    // Outer area minus the holes' areas, square metres.
    double area = 0.0;
    // A point strictly on the component's material, and its area centroid
    // (which may lie in a hole) -- the same two answers a region carries.
    SketchPoint interiorPoint{};
    SketchPoint centroid{};
};

// The union of a selection, as its connected components in ascending outer
// anchor order, each with its holes ascending. The selection must already
// pass `validateRegionSelection`; an outer anchor naming no region is skipped.
//
//   * a component's outer loop is a selected region's loop whose parent region
//     is not selected (or which has no parent);
//   * walking down from it, a selected child region CONTINUES the component
//     (the shared loop is interior to the union and bounds nothing), and an
//     unselected child region is a HOLE of it;
//   * a selected region inside such a hole starts a component of its own.
std::vector<SketchRegionComponent> mergeSelectedRegions(
        const SketchRegionExtraction& extraction, const std::vector<ProfileRegionRef>& selection);

// The polygons of one component: outer first, then each hole, as extracted.
std::vector<std::vector<SketchPoint>> sketchComponentLoops(const SketchRegionExtraction& extraction,
                                                           const SketchRegionComponent& component);

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
// The same hatch over a union component, so an absorbed hole reads as material
// and a remaining one stays empty.
std::vector<SketchPoint> sketchComponentHatch(const SketchRegionExtraction& extraction,
                                              const SketchRegionComponent& component,
                                              double spacing);

// The same hatch over any loops -- the first the outer boundary, the rest holes
// -- by the even-odd rule: what a PlanarFaces union component (`CAD-V6-S2`),
// whose loops are fragment polygons rather than extracted profiles, is hatched
// with. Presentation only.
std::vector<SketchPoint> sketchLoopsHatch(const std::vector<std::vector<SketchPoint>>& loops,
                                          double spacing);

// Where an arrow or a label stands on loops (outer first, then holes): the
// area centroid when it lies on material, else a scanned interior point -- the
// one rule a region's and a union component's anchor follow. False for no loop.
bool sketchLoopsInteriorPoint(const std::vector<std::vector<SketchPoint>>& loops, SketchPoint* out);

// Point strictly inside a polygon by the even-odd rule; a point ON an edge is
// not strictly inside.
bool sketchPointStrictlyInside(const SketchPoint& point, const std::vector<SketchPoint>& polygon);

}  // namespace forgeshape
