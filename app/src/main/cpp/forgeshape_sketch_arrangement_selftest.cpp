#include "forgeshape_sketch_arrangement_selftest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <utility>

#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_region.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool ok) { out->push_back(ArrangementSelfTestCheck{name, ok}); }
};

// --- sketch builders (ids minted by the sketch, in call order) -------------

SketchEntityId add(CadSketch* sketch, SketchEntity::Payload payload) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, std::move(payload), &id);
    return id;
}

SketchEntityId rect(CadSketch* s, double cu, double cv, double w, double h) {
    SketchRectangle r;
    r.center = SketchPoint{cu, cv};
    r.width = w;
    r.height = h;
    return add(s, r);
}

SketchEntityId circle(CadSketch* s, double cu, double cv, double radius) {
    SketchCircle c;
    c.center = SketchPoint{cu, cv};
    c.radius = radius;
    return add(s, c);
}

SketchEntityId line(CadSketch* s, double u0, double v0, double u1, double v1) {
    SketchLine l;
    l.start = SketchPoint{u0, v0};
    l.end = SketchPoint{u1, v1};
    return add(s, l);
}

SketchEntityId arc(CadSketch* s, SketchPoint a, SketchPoint m, SketchPoint b) {
    SketchArc x;
    x.start = a;
    x.mid = m;
    x.end = b;
    return add(s, x);
}

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

// Every entity a face's outer cycle is made of.
std::vector<SketchEntityId> outerEntities(const AtomicPlanarFace& face) {
    std::vector<SketchEntityId> ids;
    for (const FragmentRef& f : face.ref.outer) {
        if (std::find(ids.begin(), ids.end(), f.sourceEntityId) == ids.end()) {
            ids.push_back(f.sourceEntityId);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

// True when a cycle is a WHOLE source loop: every fragment runs source start
// to source end (what a v5 whole-loop region can name).
bool wholeLoop(const FragmentCycle& cycle) {
    for (const FragmentRef& f : cycle) {
        if (f.startCut.kind == ArrangementCutKind::Intersection
            || f.endCut.kind == ArrangementCutKind::Intersection) {
            return false;
        }
    }
    return true;
}

size_t fragmentsOf(const SketchArrangement& a, SketchEntityId id, uint32_t local) {
    size_t n = 0;
    for (const ArrangementFragment& f : a.fragments) {
        if (f.ref.sourceEntityId == id && f.ref.sourceEdgeLocalIndex == local) ++n;
    }
    return n;
}

bool sameArrangement(const SketchArrangement& x, const SketchArrangement& y) {
    if (x.status != y.status || x.faces.size() != y.faces.size()
        || x.fragments.size() != y.fragments.size() || x.nodes.size() != y.nodes.size()) {
        return false;
    }
    for (size_t i = 0; i < x.faces.size(); ++i) {
        if (!samePlanarFaceRef(x.faces[i].ref, y.faces[i].ref)) return false;
    }
    for (size_t i = 0; i < x.fragments.size(); ++i) {
        if (compareFragmentRef(x.fragments[i].ref, y.fragments[i].ref) != 0
            || x.fragments[i].startNode != y.fragments[i].startNode
            || x.fragments[i].endNode != y.fragments[i].endNode
            || x.fragments[i].boundsFace != y.fragments[i].boundsFace) {
            return false;
        }
    }
    for (size_t i = 0; i < x.nodes.size(); ++i) {
        if (x.nodes[i].u != y.nodes[i].u || x.nodes[i].v != y.nodes[i].v) return false;
    }
    return true;
}

bool allResolve(const SketchArrangement& from, const SketchArrangement& in) {
    for (const AtomicPlanarFace& face : from.faces) {
        if (!resolvePlanarFaceRef(in, face.ref, nullptr)) return false;
    }
    return true;
}

double lensArea(double r, double d) {
    return 2.0 * r * r * std::acos(d / (2.0 * r)) - 0.5 * d * std::sqrt(4.0 * r * r - d * d);
}

// The OWNER's circle crossing a rectangle's right edge.
CadSketch crossingCircleSketch(double cu, double cv, double radius) {
    CadSketch s;
    rect(&s, 0.0, 0.0, 4.0, 3.0);
    circle(&s, cu, cv, radius);
    return s;
}

// --- the matrix ------------------------------------------------------------

void testCrossingCircleRectangle(Checks& c) {
    const CadSketch s = crossingCircleSketch(2.0, 0.0, 0.5);
    const SketchArrangement a = deriveSketchArrangement(s);
    const double half = 0.5 * kPi * 0.25;
    bool areas = a.faces.size() == 3u;
    int lens = -1;
    for (size_t i = 0; areas && i < a.faces.size(); ++i) {
        const double area = a.faces[i].area;
        areas = near(area, half, 1e-9) || near(area, 12.0 - half, 1e-9);
        // The lens is the half-disk INSIDE the rectangle: both entities, and
        // its rectangle fragment is walked forward (x < 2 is left of the
        // up-going right side).
        if (outerEntities(a.faces[i]) == std::vector<SketchEntityId>{1u, 2u} && near(area, half, 1e-9)) {
            for (const FragmentRef& f : a.faces[i].ref.outer) {
                if (f.sourceEntityId == 1u && !f.reversed) lens = static_cast<int>(i);
            }
        }
    }
    c.check("PFS1_01a_crossing_circle_rectangle_is_three_bounded_faces",
            a.status == ArrangementStatus::Ok && areas && a.stats.crossings == 2u);
    bool distinct = true;
    for (size_t i = 0; i + 1 < a.faces.size(); ++i) {
        distinct = distinct && comparePlanarFaceRef(a.faces[i].ref, a.faces[i + 1].ref) < 0;
    }
    c.check("PFS1_01b_three_faces_have_distinct_canonical_refs", distinct);
    c.check("PFS1_01c_the_lens_is_made_of_rectangle_AND_circle_fragments", lens >= 0);
    bool noWholeLoop = true;
    for (const AtomicPlanarFace& f : a.faces) {
        noWholeLoop = noWholeLoop && !wholeLoop(f.ref.outer);
    }
    c.check("PFS1_01d_no_face_is_a_whole_source_loop_alias", noWholeLoop);
    // The rectangle's right side is split twice, the circle twice.
    c.check("PFS1_01e_right_side_three_fragments_circle_two",
            fragmentsOf(a, 1u, 1u) == 3u && fragmentsOf(a, 2u, 0u) == 2u
                    && fragmentsOf(a, 1u, 0u) == 1u);
}

void testProtrusion(Checks& c) {
    CadSketch s;
    const SketchEntityId r = rect(&s, 0.0, 0.0, 4.0, 3.0);
    line(&s, 2.0, -0.5, 3.0, -0.5);
    line(&s, 3.0, -0.5, 3.0, 0.5);
    line(&s, 3.0, 0.5, 2.0, 0.5);
    line(&s, 3.0, 0.5, 3.5, 1.0);   // dangling off a protrusion corner
    line(&s, -1.0, 0.0, -0.5, 0.0); // floating inside the rectangle
    const SketchArrangement a = deriveSketchArrangement(s);
    bool rectangle = false;
    bool protrusion = false;
    for (const AtomicPlanarFace& f : a.faces) {
        if (near(f.area, 12.0, 1e-9) && f.ref.holes.empty()) rectangle = true;
        if (near(f.area, 1.0, 1e-9)) {
            // Made of the rectangle's right side (walked reversed: the
            // protrusion is right of the up-going side) and the three lines.
            bool usesSide = false;
            for (const FragmentRef& x : f.ref.outer) {
                usesSide = usesSide || (x.sourceEntityId == r && x.sourceEdgeLocalIndex == 1u
                                        && x.reversed);
            }
            protrusion = usesSide && f.ref.outer.size() == 4u;
        }
    }
    c.check("PFS1_02a_T_junctions_split_the_rectangle_side",
            a.status == ArrangementStatus::Ok && a.stats.tJunctions == 2u
                    && fragmentsOf(a, r, 1u) == 3u);
    c.check("PFS1_02b_protrusion_sketch_is_two_bounded_faces",
            a.faces.size() == 2u && rectangle && protrusion);
    c.check("PFS1_02c_dangling_lines_bound_no_face", a.stats.prunedFragments == 2u);
}

void testTwoCircles(Checks& c) {
    CadSketch s;
    circle(&s, -0.4, 0.0, 1.0);
    circle(&s, 0.4, 0.0, 1.0);
    const SketchArrangement a = deriveSketchArrangement(s);
    const double lens = lensArea(1.0, 0.8);
    int lenses = 0;
    int crescents = 0;
    for (const AtomicPlanarFace& f : a.faces) {
        if (near(f.area, lens, 1e-9)) ++lenses;
        if (near(f.area, kPi - lens, 1e-9)) ++crescents;
    }
    c.check("PFS1_03a_two_crossing_circles_are_lens_and_two_crescents",
            a.status == ArrangementStatus::Ok && a.faces.size() == 3u && lenses == 1
                    && crescents == 2 && a.stats.crossings == 2u);
}

void testNested(Checks& c) {
    CadSketch s;
    const SketchEntityId o = rect(&s, 0.0, 0.0, 4.0, 3.0);
    const SketchEntityId x = circle(&s, -1.0, 0.0, 0.5);
    const SketchEntityId y = circle(&s, 1.0, 0.0, 0.5);
    const SketchArrangement a = deriveSketchArrangement(s);
    const SketchRegionExtraction regions = extractSketchRegions(s);
    // Each arrangement face corresponds to exactly one v5 region: its outer
    // cycle is the region's outer loop, its holes the region's holes, and
    // every fragment is a whole source edge.
    bool matches = a.status == ArrangementStatus::Ok && a.faces.size() == regions.regions.size();
    for (const AtomicPlanarFace& f : a.faces) {
        const std::vector<SketchEntityId> outer = outerEntities(f);
        bool found = false;
        for (const SketchRegion& region : regions.regions) {
            if (outer != std::vector<SketchEntityId>{region.outerAnchorId}) continue;
            std::vector<SketchEntityId> holes;
            for (const FragmentCycle& h : f.ref.holes) holes.push_back(h.front().sourceEntityId);
            std::sort(holes.begin(), holes.end());
            found = holes == region.holeAnchorIds;
        }
        matches = matches && found && wholeLoop(f.ref.outer);
        for (const FragmentCycle& h : f.ref.holes) matches = matches && wholeLoop(h);
    }
    c.check("PFS1_04a_nested_faces_are_exactly_the_v5_regions_of_whole_loops", matches);
    bool areas = false;
    for (const AtomicPlanarFace& f : a.faces) {
        if (outerEntities(f) == std::vector<SketchEntityId>{o}) {
            areas = f.ref.holes.size() == 2u && near(f.area, 12.0 - 2.0 * kPi * 0.25, 1e-9);
        }
    }
    c.check("PFS1_04b_outer_face_carries_both_disks_as_holes",
            areas && x != y && a.stats.crossings == 0u && a.stats.tJunctions == 0u);
}

void testLineLine(Checks& c) {
    CadSketch s;
    line(&s, 0.0, 0.0, 2.0, 2.0);
    line(&s, 0.0, 2.0, 2.0, 0.0);
    const SketchArrangement a = deriveSketchArrangement(s);
    bool cuts = a.fragments.size() == 4u;
    for (const ArrangementFragment& f : a.fragments) {
        const ArrangementCut& cut = f.ref.startCut.kind == ArrangementCutKind::Intersection
                                            ? f.ref.startCut : f.ref.endCut;
        cuts = cuts && cut.kind == ArrangementCutKind::Intersection && cut.ordinal == 0u
               && cut.partnerEntityId == (f.ref.sourceEntityId == 1u ? 2u : 1u);
    }
    c.check("PFS1_05a_line_line_crossing_is_one_node_four_fragments",
            a.status == ArrangementStatus::Ok && a.stats.crossings == 1u && a.nodes.size() == 5u
                    && cuts && a.faces.empty());
}

void testThreeAtOnePoint(Checks& c) {
    // Two lines crossing exactly ON the rectangle's right side: three curves,
    // one node, and the side cut ONCE, named by its smallest partner.
    CadSketch s;
    const SketchEntityId r = rect(&s, 0.0, 0.0, 4.0, 3.0);
    const SketchEntityId l0 = line(&s, 1.0, -1.0, 3.0, 1.0);
    line(&s, 1.0, 1.0, 3.0, -1.0);
    const SketchArrangement a = deriveSketchArrangement(s);
    bool named = false;
    for (const ArrangementFragment& f : a.fragments) {
        if (f.ref.sourceEntityId == r && f.ref.sourceEdgeLocalIndex == 1u
            && f.ref.endCut.kind == ArrangementCutKind::Intersection) {
            named = f.ref.endCut.partnerEntityId == l0 && f.ref.endCut.ordinal == 0u;
        }
    }
    size_t atMeet = 0;
    for (const SketchPoint& p : a.nodes) {
        if (std::fabs(p.u - 2.0) <= 1e-9 && std::fabs(p.v) <= 1e-9) ++atMeet;
    }
    c.check("PFS1_05b_three_curves_at_one_point_are_one_node_and_one_named_cut",
            a.status == ArrangementStatus::Ok && atMeet == 1u && fragmentsOf(a, r, 1u) == 2u
                    && named && a.faces.size() == 1u && near(a.faces[0].area, 12.0, 1e-9));
}

void testSegmentCircle(Checks& c) {
    CadSketch s;
    const SketchEntityId l = line(&s, -2.0, 0.0, 2.0, 0.0);
    circle(&s, 0.0, 0.0, 1.0);
    const SketchArrangement a = deriveSketchArrangement(s);
    // The line's middle fragment runs from its first cut (x = -1, ordinal 0)
    // to its second (x = +1, ordinal 1), in the line's own direction.
    bool ordered = false;
    for (const ArrangementFragment& f : a.fragments) {
        if (f.ref.sourceEntityId == l && f.ref.startCut.kind == ArrangementCutKind::Intersection
            && f.ref.endCut.kind == ArrangementCutKind::Intersection) {
            ordered = f.ref.startCut.ordinal == 0u && f.ref.endCut.ordinal == 1u;
        }
    }
    c.check("PFS1_06a_segment_circle_two_ordered_cuts_two_half_disks",
            a.status == ArrangementStatus::Ok && a.stats.crossings == 2u && ordered
                    && fragmentsOf(a, l, 0u) == 3u && a.faces.size() == 2u
                    && near(a.faces[0].area, 0.5 * kPi, 1e-9) && near(a.faces[1].area, 0.5 * kPi, 1e-9));
}

void testTangents(Checks& c) {
    {
        CadSketch s;
        line(&s, -2.0, 1.0, 2.0, 1.0);
        circle(&s, 0.0, 0.0, 1.0);
        const SketchArrangement a = deriveSketchArrangement(s);
        c.check("PFS1_07a_tangent_segment_circle_makes_no_node_and_no_bogus_face",
                a.status == ArrangementStatus::Ok && a.stats.tangents == 1u
                        && a.stats.crossings == 0u && a.faces.size() == 1u
                        && near(a.faces[0].area, kPi, 1e-9));
    }
    {
        CadSketch s;
        rect(&s, 0.0, 0.0, 2.0, 2.0);
        circle(&s, 0.0, 0.0, 1.0);
        const SketchArrangement a = deriveSketchArrangement(s);
        bool ring = false;
        for (const AtomicPlanarFace& f : a.faces) {
            ring = ring || (f.ref.holes.size() == 1u && near(f.area, 4.0 - kPi, 1e-9));
        }
        c.check("PFS1_07b_inscribed_circle_is_disk_plus_rectangle_with_hole",
                a.status == ArrangementStatus::Ok && a.stats.tangents == 4u && a.faces.size() == 2u
                        && ring);
    }
    {
        CadSketch s;
        circle(&s, -1.0, 0.0, 1.0);
        circle(&s, 1.0, 0.0, 1.0);
        const SketchArrangement a = deriveSketchArrangement(s);
        c.check("PFS1_08a_externally_tangent_circles_have_no_lens",
                a.status == ArrangementStatus::Ok && a.stats.tangents == 1u && a.faces.size() == 2u
                        && near(a.faces[0].area, kPi, 1e-9) && near(a.faces[1].area, kPi, 1e-9));
    }
    {
        CadSketch s;
        circle(&s, 0.0, 0.0, 2.0);
        circle(&s, 1.0, 0.0, 1.0);
        const SketchArrangement a = deriveSketchArrangement(s);
        bool areas = a.faces.size() == 2u;
        for (const AtomicPlanarFace& f : a.faces) {
            areas = areas && (near(f.area, kPi, 1e-9) || near(f.area, 3.0 * kPi, 1e-9));
        }
        c.check("PFS1_08b_internally_tangent_circles_have_no_lens",
                a.status == ArrangementStatus::Ok && a.stats.tangents == 1u && areas);
    }
}

void testArcs(Checks& c) {
    const SketchPoint e{1.0, 0.0}, top{0.0, 1.0}, w{-1.0, 0.0};
    {
        // Upper semicircle; a vertical line crosses the full circle twice but
        // the ARC once: the sweep filters the lower root.
        CadSketch s;
        arc(&s, e, top, w);
        line(&s, 0.5, -2.0, 0.5, 2.0);
        const SketchArrangement a = deriveSketchArrangement(s);
        c.check("PFS1_09a_segment_arc_crossing_filtered_by_sweep",
                a.status == ArrangementStatus::Ok && a.stats.crossings == 1u);
    }
    {
        CadSketch s;
        arc(&s, e, top, w);
        line(&s, -1.0, 0.0, 1.0, 0.0);
        const SketchArrangement a = deriveSketchArrangement(s);
        c.check("PFS1_09b_arc_closed_by_a_line_is_a_half_disk_face",
                a.status == ArrangementStatus::Ok && a.stats.endpointCoincidences == 2u
                        && a.faces.size() == 1u && near(a.faces[0].area, 0.5 * kPi, 1e-9));
    }
    {
        CadSketch up;
        arc(&up, e, top, w);
        circle(&up, 0.0, 1.0, 0.5);
        CadSketch down;
        arc(&down, e, top, w);
        circle(&down, 0.0, -1.0, 0.5);
        const SketchArrangement a = deriveSketchArrangement(up);
        const SketchArrangement b = deriveSketchArrangement(down);
        c.check("PFS1_09c_circle_arc_crossings_filtered_by_sweep",
                a.status == ArrangementStatus::Ok && a.stats.crossings == 2u
                        && b.status == ArrangementStatus::Ok && b.stats.crossings == 0u);
    }
    {
        CadSketch s;
        arc(&s, e, top, w);
        arc(&s, SketchPoint{2.0, 0.0}, SketchPoint{1.0, 1.0}, SketchPoint{0.0, 0.0});
        const SketchArrangement a = deriveSketchArrangement(s);
        c.check("PFS1_09d_arc_arc_crossing_filtered_by_sweep",
                a.status == ArrangementStatus::Ok && a.stats.crossings == 1u);
    }
}

void testTolerance(Checks& c) {
    c.check("PFS1_10a_coincidence_tolerance_is_pinned_at_one_micrometre",
            kSketchCoincidenceMeters == 1.0e-6);
    const auto tSketch = [](double gap) {
        CadSketch s;
        rect(&s, 0.0, 0.0, 4.0, 2.0);
        line(&s, 3.0, 0.0, 2.0 + gap, 0.0);
        return deriveSketchArrangement(s);
    };
    const SketchArrangement inside = tSketch(0.5e-6);
    const SketchArrangement outside = tSketch(2.0e-6);
    c.check("PFS1_10b_endpoint_within_tolerance_splits_the_touched_side_once",
            inside.status == ArrangementStatus::Ok && inside.stats.tJunctions == 1u
                    && fragmentsOf(inside, 1u, 1u) == 2u);
    c.check("PFS1_10c_endpoint_outside_tolerance_does_not",
            outside.status == ArrangementStatus::Ok && outside.stats.tJunctions == 0u
                    && fragmentsOf(outside, 1u, 1u) == 1u);
    CadSketch corner;
    rect(&corner, 0.0, 0.0, 4.0, 2.0);
    line(&corner, 3.0, 1.0, 2.0, 1.0);
    const SketchArrangement k = deriveSketchArrangement(corner);
    c.check("PFS1_10d_endpoint_on_an_authored_endpoint_splits_nothing",
            k.status == ArrangementStatus::Ok && k.stats.tJunctions == 0u
                    && fragmentsOf(k, 1u, 1u) == 1u && fragmentsOf(k, 1u, 2u) == 1u);
}

void testDeterminism(Checks& c) {
    CadSketch s = crossingCircleSketch(2.0, 0.0, 0.5);
    line(&s, 2.0, -1.0, 3.0, -1.0);
    line(&s, 3.0, -1.0, 3.0, 1.2);
    line(&s, 3.0, 1.2, 2.0, 1.2);
    arc(&s, SketchPoint{-2.0, 0.5}, SketchPoint{-2.5, 0.0}, SketchPoint{-2.0, -0.5});
    const SketchArrangement base = deriveSketchArrangement(s);
    bool permuted = base.status == ArrangementStatus::Ok && base.faces.size() >= 4u;
    CadSketch shuffled = s;
    for (int round = 0; round < 6; ++round) {
        std::rotate(shuffled.entities.begin(), shuffled.entities.begin() + 1,
                    shuffled.entities.end());
        if (round % 2 == 1) std::reverse(shuffled.entities.begin(), shuffled.entities.end());
        permuted = permuted && sameArrangement(base, deriveSketchArrangement(shuffled));
    }
    c.check("PFS1_11a_input_order_changes_no_node_fragment_or_face", permuted);
    bool repeated = true;
    for (int i = 0; i < 100; ++i) {
        repeated = repeated && sameArrangement(base, deriveSketchArrangement(s));
    }
    c.check("PFS1_12a_one_hundred_derivations_are_tuple_identical", repeated);
}

void testEdits(Checks& c) {
    const SketchArrangement before = deriveSketchArrangement(crossingCircleSketch(2.0, 0.0, 0.5));
    // Still crossing the same right side twice, same ordinals.
    const SketchArrangement moved = deriveSketchArrangement(crossingCircleSketch(2.1, 0.3, 0.6));
    c.check("PFS1_13a_a_move_keeping_partners_and_ordinals_keeps_every_ref",
            before.faces.size() == 3u && moved.faces.size() == 3u && allResolve(before, moved)
                    && allResolve(moved, before));
    // Pulled clear of the rectangle: the intersections are gone.
    const SketchArrangement apart = deriveSketchArrangement(crossingCircleSketch(3.0, 0.0, 0.5));
    size_t lost = 0;
    for (const AtomicPlanarFace& f : before.faces) {
        size_t index = 99;
        if (!resolvePlanarFaceRef(apart, f.ref, &index) && index == 99) ++lost;
    }
    c.check("PFS1_14a_losing_an_intersection_loses_every_affected_ref_with_no_fallback",
            apart.status == ArrangementStatus::Ok && apart.faces.size() == 2u && lost == 3u);
    // Now crossing the TOP side instead: different partners, no alias.
    const SketchArrangement top = deriveSketchArrangement(crossingCircleSketch(0.0, 1.5, 0.5));
    size_t stillThere = 0;
    for (const AtomicPlanarFace& f : before.faces) {
        if (resolvePlanarFaceRef(top, f.ref, nullptr)) ++stillThere;
    }
    c.check("PFS1_14b_a_different_crossing_resolves_none_of_the_old_refs",
            top.faces.size() == 3u && stillThere == 0u);
}

void testRefusals(Checks& c) {
    {
        CadSketch s;
        rect(&s, 0.0, 0.0, 4.0, 3.0);
        SketchSpline spline;
        spline.points = {SketchPoint{-3.0, 0.0}, SketchPoint{0.0, 2.0}, SketchPoint{3.0, 0.0}};
        add(&s, spline);
        const SketchArrangement a = deriveSketchArrangement(s);
        // `CAD-V6-S2-CORRECTION-FILL-HUD-R1`: no longer refused. The spline's
        // two spans are source edges 3.0 and 3.1 and are cut where they cross
        // the rectangle.
        c.check("PFS1_15a_a_spline_is_intersected_span_by_span_since_the_fill_correction",
                a.status == ArrangementStatus::Ok && fragmentsOf(a, 2u, 0u) > 1u
                        && fragmentsOf(a, 2u, 1u) > 1u && !a.faces.empty());
    }
    {
        CadSketch s;
        line(&s, 0.0, 0.0, 2.0, 0.0);
        line(&s, 1.0, 0.0, 3.0, 0.0);
        CadSketch touching;
        line(&touching, 0.0, 0.0, 2.0, 0.0);
        line(&touching, 2.0, 0.0, 3.0, 0.0);
        c.check("PFS1_16a_overlapping_collinear_segments_are_AmbiguousOverlap",
                deriveSketchArrangement(s).status == ArrangementStatus::AmbiguousOverlap
                        && deriveSketchArrangement(touching).status == ArrangementStatus::Ok);
    }
    {
        CadSketch same;
        circle(&same, 0.0, 0.0, 1.0);
        circle(&same, 0.0, 0.0, 1.0);
        CadSketch shared;
        rect(&shared, 0.0, 0.0, 2.0, 2.0);
        rect(&shared, 2.0, 0.5, 2.0, 1.0);
        CadSketch arcs;
        arc(&arcs, SketchPoint{1.0, 0.0}, SketchPoint{0.0, 1.0}, SketchPoint{-1.0, 0.0});
        arc(&arcs, SketchPoint{0.0, 1.0}, SketchPoint{-1.0, 0.0}, SketchPoint{0.0, -1.0});
        CadSketch halves;
        arc(&halves, SketchPoint{1.0, 0.0}, SketchPoint{0.0, 1.0}, SketchPoint{-1.0, 0.0});
        arc(&halves, SketchPoint{-1.0, 0.0}, SketchPoint{0.0, -1.0}, SketchPoint{1.0, 0.0});
        const SketchArrangement h = deriveSketchArrangement(halves);
        c.check("PFS1_16b_coincident_circles_shared_sides_and_overlapping_arcs_are_refused",
                deriveSketchArrangement(same).status == ArrangementStatus::AmbiguousOverlap
                        && deriveSketchArrangement(shared).status == ArrangementStatus::AmbiguousOverlap
                        && deriveSketchArrangement(arcs).status == ArrangementStatus::AmbiguousOverlap);
        c.check("PFS1_16c_two_arcs_meeting_end_to_end_on_one_circle_are_one_disk",
                h.status == ArrangementStatus::Ok && h.faces.size() == 1u
                        && near(h.faces[0].area, kPi, 1e-9));
    }
}

// A grid of n horizontal and n vertical lines: n*n crossings, (n-1)^2 cells.
CadSketch gridSketch(int n) {
    CadSketch s;
    for (int i = 0; i < n; ++i) line(&s, -1.0, i, static_cast<double>(n), i);
    for (int j = 0; j < n; ++j) line(&s, j, -1.0, j, static_cast<double>(n));
    return s;
}

void testCaps(Checks& c, std::string* performance) {
    // 64 x 64 is exactly kMaxArrangementContacts crossings.
    const CadSketch atCap = gridSketch(64);
    std::vector<long long> micros;
    SketchArrangement first;
    bool same = true;
    constexpr int kRuns = 20;
    for (int run = 0; run < kRuns; ++run) {
        const auto t0 = std::chrono::steady_clock::now();
        SketchArrangement a = deriveSketchArrangement(atCap);
        micros.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - t0)
                                 .count());
        if (run == 0) {
            first = std::move(a);
        } else {
            same = same && sameArrangement(first, a);
        }
    }
    c.check("PFS1_17a_the_at_cap_grid_derives_every_cell_deterministically",
            first.status == ArrangementStatus::Ok && first.stats.crossings == kMaxArrangementContacts
                    && first.faces.size() == 63u * 63u && same);
    c.check("PFS1_17b_one_line_past_the_contact_cap_is_CapExceeded",
            deriveSketchArrangement(gridSketch(65)).status == ArrangementStatus::CapExceeded);
    // The largest LEGAL sketch: 256 polylines of 256 vertices. Refused by name
    // after bounded work, never derived quadratically.
    CadSketch biggest;
    for (int p = 0; p < static_cast<int>(kMaxSketchEntities); ++p) {
        SketchPolyline poly;
        for (int k = 0; k < static_cast<int>(kMaxPolylineVertices); ++k) {
            poly.vertices.push_back(SketchPoint{k * 0.01, p * 0.01 + (k % 2) * 0.001});
        }
        add(&biggest, poly);
    }
    const auto t0 = std::chrono::steady_clock::now();
    const SketchArrangement big = deriveSketchArrangement(biggest);
    const long long bigMicros = std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::steady_clock::now() - t0)
                                        .count();
    c.check("PFS1_17c_the_largest_legal_sketch_is_refused_CapExceeded_after_bounded_work",
            biggest.entities.size() == kMaxSketchEntities
                    && big.status == ArrangementStatus::CapExceeded && big.faces.empty());
    // 256 disjoint rectangles: exactly the source-edge cap, every one a face.
    CadSketch rects;
    for (int i = 0; i < 256; ++i) rect(&rects, (i % 16) * 3.0, (i / 16) * 3.0, 1.0, 1.0);
    const SketchArrangement r = deriveSketchArrangement(rects);
    c.check("PFS1_17d_the_source_edge_cap_is_reachable_and_exact",
            r.status == ArrangementStatus::Ok && r.stats.sourceEdges == kMaxArrangementSourceEdges
                    && r.faces.size() == 256u);
    std::vector<long long> sorted = micros;
    std::sort(sorted.begin(), sorted.end());
    char text[160];
    std::snprintf(text, sizeof(text),
                  "arrangement_cap_us=%lld/%lld runs=%d cells=%zu largest_legal_refusal_us=%lld",
                  sorted[sorted.size() / 2], sorted.back(), kRuns, first.faces.size(), bigMicros);
    *performance = text;
}

}  // namespace

void runSketchArrangementSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                                   std::string* performance) {
    Checks c{out};
    testCrossingCircleRectangle(c);
    testProtrusion(c);
    testTwoCircles(c);
    testNested(c);
    testLineLine(c);
    testThreeAtOnePoint(c);
    testSegmentCircle(c);
    testTangents(c);
    testArcs(c);
    testTolerance(c);
    testDeterminism(c);
    testEdits(c);
    testRefusals(c);
    testCaps(c, performance);
}

}  // namespace forgeshape
