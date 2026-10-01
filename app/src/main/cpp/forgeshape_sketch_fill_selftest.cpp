#include "forgeshape_sketch_fill_selftest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <utility>

#include "forgeshape_cad_body.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_region.h"

namespace forgeshape {

namespace {

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

SketchEntityId spline(CadSketch* s, std::vector<SketchPoint> points) {
    SketchSpline x;
    x.points = std::move(points);
    return add(s, x);
}

// The closed spline-and-line loop every case below reuses: a three-point
// spline from (-1, 0) through (0, 2) to (1, 0), closed by the line back.
void splineLoop(CadSketch* s, double du = 0.0, double dv = 0.0, double peak = 2.0) {
    spline(s, {SketchPoint{-1.0 + du, 0.0 + dv}, SketchPoint{0.0 + du, peak + dv},
               SketchPoint{1.0 + du, 0.0 + dv}});
    line(s, 1.0 + du, 0.0 + dv, -1.0 + du, 0.0 + dv);
}

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

double faceAreaSum(const SketchArrangement& a) {
    double sum = 0.0;
    for (const AtomicPlanarFace& face : a.faces) sum += face.area;
    return sum;
}

// Every face's ref distinct (the list is sorted, so strictly ascending).
bool refsDistinct(const SketchArrangement& a) {
    for (size_t i = 1; i < a.faces.size(); ++i) {
        if (comparePlanarFaceRef(a.faces[i - 1].ref, a.faces[i].ref) >= 0) return false;
    }
    return true;
}

bool sameFaces(const SketchArrangement& x, const SketchArrangement& y) {
    if (x.status != y.status || x.faces.size() != y.faces.size()) return false;
    for (size_t i = 0; i < x.faces.size(); ++i) {
        if (!samePlanarFaceRef(x.faces[i].ref, y.faces[i].ref)) return false;
    }
    return true;
}

bool allResolve(const SketchArrangement& from, const SketchArrangement& in) {
    for (const AtomicPlanarFace& face : from.faces) {
        if (!resolvePlanarFaceRef(in, face.ref, nullptr)) return false;
    }
    return true;
}

// A fragment of `entity` whose cut names `partner` as the crossing curve.
bool cutBy(const SketchArrangement& a, SketchEntityId entity, SketchEntityId partner) {
    for (const ArrangementFragment& f : a.fragments) {
        if (f.ref.sourceEntityId != entity) continue;
        if ((f.ref.startCut.kind == ArrangementCutKind::Intersection
             && f.ref.startCut.partnerEntityId == partner)
            || (f.ref.endCut.kind == ArrangementCutKind::Intersection
                && f.ref.endCut.partnerEntityId == partner)) {
            return true;
        }
    }
    return false;
}

// Some face's boundary carries a fragment of this entity.
bool faceUses(const SketchArrangement& a, SketchEntityId entity) {
    for (const AtomicPlanarFace& face : a.faces) {
        for (const FragmentRef& f : face.ref.outer) {
            if (f.sourceEntityId == entity) return true;
        }
    }
    return false;
}

// A body extruding the given faces 1 m, through the PlanarFaces path.
CadBodyState facesBody(const CadSketch& sketch, const SketchArrangement& a,
                       const std::vector<size_t>& indices, double depth = 1.0) {
    ExtrudeFeature e;
    e.profileEntityId = kNoSketchEntity;
    e.depth = depth;
    e.direction = ExtrudeDirection::AlongNormal;
    e.selection = CadSelectionKind::PlanarFaces;
    for (size_t i : indices) e.planarFaces.push_back(a.faces[i].ref);
    std::sort(e.planarFaces.begin(), e.planarFaces.end(),
              [](const PlanarFaceRef& x, const PlanarFaceRef& y) {
                  return comparePlanarFaceRef(x, y) < 0;
              });
    return makeCadBodyState(sketch, e);
}

struct Solid {
    CadStatus why = CadStatus::RegenerationFailed;
    uint32_t components = 0;
    double volume = 0.0;
};

Solid solidOf(const CadBodyState& state) {
    Solid s;
    CadBodyMesh mesh;
    s.why = regenerateCadBody(state, &mesh);
    s.components = mesh.components;
    s.volume = mesh.volume;
    return s;
}

// The solid is built from the curves' chords, which fall inside a boundary
// convex toward the material and outside one concave toward it, so its volume
// differs from the exact face area times the depth by a small fraction.
bool chordedVolume(const Solid& s, double exactArea, double depth) {
    const double exact = exactArea * depth;
    return s.why == CadStatus::Ok && s.volume > 0.0 && std::fabs(s.volume - exact) <= 0.02 * exact;
}

SketchSelectionModeDecision decide(const CadSketch& s) {
    return decideSketchSelectionMode(deriveSketchArrangement(s), extractSketchRegions(s));
}

// The spline whose middle span rises to an INTERIOR maximum: its tangents at
// (-1, 0.5) and (1, 0.5) point up and down, so the top lies inside span 1 and
// is no authored point.
const std::vector<SketchPoint> kHump = {SketchPoint{-2.0, 0.0}, SketchPoint{-1.0, 0.5},
                                        SketchPoint{1.0, 0.5}, SketchPoint{2.0, 0.0}};

// That maximum, found on span 1's own parameter by golden section.
SketchPoint interiorPeak() {
    SketchSpline s;
    s.points = kHump;
    SketchBezierSpan span;
    sketchSplineSpan(s, 1u, &span);
    double lo = 0.0;
    double hi = 1.0;
    const double g = 0.6180339887498949;
    for (int i = 0; i < 200; ++i) {
        const double a = hi - g * (hi - lo);
        const double b = lo + g * (hi - lo);
        if (sketchBezierPoint(span, a).v > sketchBezierPoint(span, b).v) {
            hi = b;
        } else {
            lo = a;
        }
    }
    return sketchBezierPoint(span, 0.5 * (lo + hi));
}

// ---------------------------------------------------------------------------
// FILL-01 .. FILL-06: the cells
// ---------------------------------------------------------------------------

void testCells(Checks& c) {
    // FILL-01: a spline closed by one line is ONE bounded face, and nothing
    // crossed, so the sketch stays on the legacy loop model.
    {
        CadSketch s;
        splineLoop(&s);
        const SketchArrangement a = deriveSketchArrangement(s);
        const SketchRegionExtraction regions = extractSketchRegions(s);
        const SketchSelectionModeDecision mode = decideSketchSelectionMode(a, regions);
        ExtrudeFeature legacy;
        legacy.profileEntityId = 1u;
        legacy.depth = 1.0;
        const Solid loop = solidOf(makeCadBodyState(s, legacy));
        const Solid face = solidOf(facesBody(s, a, {0}));
        c.check("FILL_01a_a_spline_closed_by_a_line_is_one_bounded_face",
                a.status == ArrangementStatus::Ok && a.faces.size() == 1u
                        && a.stats.endpointCoincidences >= 2u && a.faces[0].area > 1.0
                        && regions.regions.size() == 1u);
        c.check("FILL_01b_it_stays_LoopRegions_and_extrudes_through_both_paths",
                mode.status == CadStatus::Ok && mode.kind == CadSelectionKind::LoopRegions
                        && loop.why == CadStatus::Ok && loop.components == 1u
                        && chordedVolume(face, a.faces[0].area, 1.0)
                        && near(loop.volume, face.volume, 0.02 * face.volume));
    }
    // FILL-02: the loop crossing a 4 x 2 rectangle's top side twice: the
    // rectangle less the loop's lower part, that lower part, and the bump
    // above the rectangle -- three cells, each its own ref, each toggleable,
    // and all three together one solid.
    {
        CadSketch s;
        rect(&s, 0.0, 0.0, 4.0, 2.0);
        splineLoop(&s);
        const SketchArrangement a = deriveSketchArrangement(s);
        double inside = 0.0;
        size_t bump = a.faces.size();
        for (size_t i = 0; i < a.faces.size(); ++i) {
            // The bump is bounded by the spline and a piece of the top side
            // alone: neither the closing line (3) nor a whole rectangle side.
            bool line3 = false;
            bool wholeSide = false;
            for (const FragmentRef& f : a.faces[i].ref.outer) {
                line3 = line3 || f.sourceEntityId == 3u;
                wholeSide = wholeSide
                            || (f.sourceEntityId == 1u
                                && f.startCut.kind == ArrangementCutKind::SourceStart
                                && f.endCut.kind == ArrangementCutKind::SourceEnd);
            }
            if (line3 || wholeSide) {
                inside += a.faces[i].area;
            } else {
                bump = i;
            }
        }
        c.check("FILL_02a_a_spline_loop_crossing_a_rectangle_twice_makes_three_cells",
                a.status == ArrangementStatus::Ok && a.faces.size() == 3u && a.stats.crossings == 2u
                        && refsDistinct(a) && cutBy(a, 2u, 1u) && cutBy(a, 1u, 2u));
        c.check("FILL_02b_the_cells_inside_the_rectangle_tile_it_exactly",
                bump < a.faces.size() && near(inside, 8.0, 1e-9) && a.faces[bump].area > 0.1);
        bool each = true;
        for (size_t i = 0; i < a.faces.size(); ++i) {
            const Solid one = solidOf(facesBody(s, a, {i}));
            each = each && one.components == 1u && chordedVolume(one, a.faces[i].area, 1.0);
        }
        c.check("FILL_02c_every_cell_extrudes_alone", each);
        const Solid all = solidOf(facesBody(s, a, {0, 1, 2}));
        c.check("FILL_02d_all_three_union_into_one_solid_with_no_internal_wall",
                all.components == 1u && chordedVolume(all, faceAreaSum(a), 1.0)
                        && decide(s).kind == CadSelectionKind::PlanarFaces);
    }
    // FILL-03: the loop crossing a circle four times: two lunes outside the
    // loop, the disk's part inside it, and the loop cut into the parts below
    // and above the disk -- five cells, every crossing a spline-circle one.
    {
        CadSketch s;
        circle(&s, 0.0, 1.0, 0.6);
        splineLoop(&s);
        const SketchArrangement a = deriveSketchArrangement(s);
        std::vector<size_t> all(a.faces.size());
        for (size_t i = 0; i < all.size(); ++i) all[i] = i;
        const CadBodyState state = facesBody(s, a, all);
        c.check("FILL_03a_a_spline_crossing_a_circle_derives_all_five_cells",
                a.status == ArrangementStatus::Ok && a.faces.size() == 5u && a.stats.crossings == 4u
                        && refsDistinct(a) && cutBy(a, 1u, 2u) && cutBy(a, 2u, 1u));
        c.check("FILL_03b_no_whole_loop_overlap_refusal_the_union_is_one_solid",
                validateCadBodyState(state) == CadStatus::Ok && solidOf(state).components == 1u
                        && decide(s).status == CadStatus::Ok
                        && decide(s).kind == CadSelectionKind::PlanarFaces);
    }
    // FILL-04: an arc crossing the loop twice splits it into two cells; the
    // arc's free ends dangle and bound nothing.
    {
        CadSketch s;
        arc(&s, SketchPoint{-2.0, 1.2}, SketchPoint{0.0, 0.6}, SketchPoint{2.0, 1.2});
        splineLoop(&s);
        const SketchArrangement a = deriveSketchArrangement(s);
        c.check("FILL_04_a_spline_crossing_an_arc_derives_both_cells",
                a.status == ArrangementStatus::Ok && a.faces.size() == 2u && a.stats.crossings == 2u
                        && cutBy(a, 2u, 1u) && cutBy(a, 1u, 2u) && a.stats.prunedFragments == 2u
                        && validateCadBodyState(facesBody(s, a, {0})) == CadStatus::Ok
                        && validateCadBodyState(facesBody(s, a, {1})) == CadStatus::Ok);
    }
    // FILL-05: two spline-and-line loops crossing each other: a lens and two
    // crescents, split where the two SPLINES cross.
    {
        CadSketch s;
        splineLoop(&s);
        splineLoop(&s, 0.5, 0.5);
        const SketchArrangement a = deriveSketchArrangement(s);
        c.check("FILL_05_two_spline_loops_crossing_split_into_atomic_cells",
                a.status == ArrangementStatus::Ok && a.faces.size() == 3u && refsDistinct(a)
                        && cutBy(a, 1u, 3u) && cutBy(a, 3u, 1u)
                        && solidOf(facesBody(s, a, {0, 1, 2})).components == 1u);
    }
    // FILL-06: the OWNER's stress case, rebuilt deterministically:
    //   rectangle 4 x 3 at the origin (entity 1);
    //   circle A, r 0.8 at (-0.8, 0) (2), and circle B, r 0.8 at (0, 0) (3),
    //     overlapping each other inside the rectangle;
    //   circle C, r 0.6 at (2, 0) (4), crossing the rectangle's right side;
    //   a spline (5) from (0.5, -1.0) down through (1.2, -2.2) to (1.9, -1.0),
    //     closed by a line (6), crossing the rectangle's bottom side twice.
    // The bounded cells: A only, A and B, B only (3); C inside and C outside
    // the rectangle (2); the loop above and below the bottom side (2); and the
    // rectangle's remainder, with A u B as its one hole (1). Eight.
    {
        CadSketch s;
        rect(&s, 0.0, 0.0, 4.0, 3.0);
        circle(&s, -0.8, 0.0, 0.8);
        circle(&s, 0.0, 0.0, 0.8);
        circle(&s, 2.0, 0.0, 0.6);
        spline(&s, {SketchPoint{0.5, -1.0}, SketchPoint{1.2, -2.2}, SketchPoint{1.9, -1.0}});
        line(&s, 1.9, -1.0, 0.5, -1.0);
        const SketchArrangement a = deriveSketchArrangement(s);
        size_t holed = 0;
        for (const AtomicPlanarFace& face : a.faces) holed += face.ref.holes.size();
        // Inside the rectangle exactly: total minus C's outer half disk and the
        // spline loop's part below the bottom side (both exact faces).
        const double halfDisk = 0.5 * 3.14159265358979323846 * 0.36;
        double belowBottom = -1.0;
        for (const AtomicPlanarFace& face : a.faces) {
            bool usesSpline = false;
            bool usesLine = false;
            for (const FragmentRef& f : face.ref.outer) {
                usesSpline = usesSpline || f.sourceEntityId == 5u;
                usesLine = usesLine || f.sourceEntityId == 6u;
            }
            // The part below: spline and rectangle, never the closing line.
            if (usesSpline && !usesLine) belowBottom = face.area;
        }
        const double insideRectangle = faceAreaSum(a) - halfDisk - belowBottom;
        c.check("FILL_06a_the_owner_stress_sketch_derives_exactly_eight_bounded_cells",
                a.status == ArrangementStatus::Ok && a.faces.size() == 8u && refsDistinct(a)
                        && holed == 1u && a.stats.crossings == 6u);
        c.check("FILL_06b_the_unbounded_exterior_is_absent_the_rectangle_is_tiled_exactly",
                belowBottom > 0.0 && near(insideRectangle, 12.0, 1e-6));
        // Every cell alone, and every pair: a valid selection or the faces
        // touch at a single point -- never the loop model's overlap names.
        bool singles = true;
        bool pairs = true;
        uint32_t merged = 0;
        uint32_t pinched = 0;
        for (size_t i = 0; i < a.faces.size(); ++i) {
            singles = singles && validateCadBodyState(facesBody(s, a, {i})) == CadStatus::Ok;
            for (size_t j = i + 1; j < a.faces.size(); ++j) {
                const CadStatus why = validateCadBodyState(facesBody(s, a, {i, j}));
                if (why == CadStatus::Ok) {
                    ++merged;
                } else if (why == CadStatus::PlanarFacesTouchAtPoint) {
                    ++pinched;
                } else {
                    pairs = false;
                }
                pairs = pairs && why != CadStatus::OverlappingRegions
                        && why != CadStatus::OverlappingHoles;
            }
        }
        c.check("FILL_06c_every_cell_alone_is_a_valid_selection", singles);
        c.check("FILL_06d_no_pair_of_cells_returns_a_legacy_overlap_status",
                pairs && merged + pinched == 28u && merged > 0u);
        std::vector<size_t> all(a.faces.size());
        for (size_t i = 0; i < all.size(); ++i) all[i] = i;
        const Solid whole = solidOf(facesBody(s, a, all));
        c.check("FILL_06e_all_eight_cells_union_into_one_solid",
                whole.components == 1u && chordedVolume(whole, faceAreaSum(a), 1.0));
        const SketchSelectionModeDecision mode = decide(s);
        c.check("FILL_06f_the_stress_sketch_is_PlanarFaces_with_no_refusal",
                mode.status == CadStatus::Ok && mode.kind == CadSelectionKind::PlanarFaces);
    }
}

// ---------------------------------------------------------------------------
// FILL-07 .. FILL-09: identity
// ---------------------------------------------------------------------------

void testIdentity(Checks& c) {
    CadSketch s;
    rect(&s, 0.0, 0.0, 4.0, 2.0);
    splineLoop(&s);
    const SketchArrangement base = deriveSketchArrangement(s);
    // FILL-07: entity order in the vector is not identity.
    {
        CadSketch reversed = s;
        std::reverse(reversed.entities.begin(), reversed.entities.end());
        CadSketch rotated = s;
        std::rotate(rotated.entities.begin(), rotated.entities.begin() + 1, rotated.entities.end());
        c.check("FILL_07_permuting_the_entity_vector_derives_identical_face_refs",
                sameFaces(base, deriveSketchArrangement(reversed))
                        && sameFaces(base, deriveSketchArrangement(rotated)));
    }
    // FILL-08: moving the spline's authored points while it still crosses the
    // same side the same two times keeps every ref.
    {
        CadSketch moved = s;
        SketchSpline curve;
        curve.points = {SketchPoint{-1.05, 0.02}, SketchPoint{0.1, 2.15}, SketchPoint{0.95, -0.03}};
        SketchLine closing;
        closing.start = SketchPoint{0.95, -0.03};
        closing.end = SketchPoint{-1.05, 0.02};
        const bool edited = replaceSketchEntity(&moved, 2u, curve) == CadStatus::Ok
                            && replaceSketchEntity(&moved, 3u, closing) == CadStatus::Ok;
        const SketchArrangement after = deriveSketchArrangement(moved);
        c.check("FILL_08_a_topology_preserving_spline_edit_keeps_every_ref",
                edited && after.status == ArrangementStatus::Ok && after.faces.size() == 3u
                        && allResolve(base, after) && allResolve(after, base));
    }
    // FILL-09: lowering the peak inside the rectangle removes both crossings;
    // the bump and the split cells are gone and resolve to nothing.
    {
        CadSketch lowered = s;
        SketchSpline curve;
        curve.points = {SketchPoint{-1.0, 0.0}, SketchPoint{0.0, 0.8}, SketchPoint{1.0, 0.0}};
        const bool edited = replaceSketchEntity(&lowered, 2u, curve) == CadStatus::Ok;
        const SketchArrangement after = deriveSketchArrangement(lowered);
        bool noneResolve = true;
        for (const AtomicPlanarFace& face : base.faces) {
            noneResolve = noneResolve && !resolvePlanarFaceRef(after, face.ref, nullptr);
        }
        const CadBodyState stale = facesBody(s, base, {0});
        CadBodyState staleOnLowered = stale;
        cadBaseSketch(staleOnLowered) = lowered;
        c.check("FILL_09_removing_an_intersection_fails_every_affected_ref_with_no_rebind",
                edited && after.status == ArrangementStatus::Ok && after.faces.size() == 2u
                        && noneResolve
                        && validateCadBodyState(staleOnLowered) == CadStatus::PlanarFaceUnresolved);
    }
}

// ---------------------------------------------------------------------------
// FILL-10 .. FILL-12: contacts that are not crossings, and refusals
// ---------------------------------------------------------------------------

void testContacts(Checks& c) {
    const SketchPoint peak = interiorPeak();
    // FILL-10: a line and a circle each TANGENT to a span's interior: a touch,
    // never a node, never a zero-area face.
    {
        CadSketch s;
        spline(&s, kHump);
        line(&s, 2.0, 0.0, -2.0, 0.0);
        line(&s, -3.0, peak.v, 3.0, peak.v);
        const SketchArrangement withLine = deriveSketchArrangement(s);
        CadSketch d;
        spline(&d, kHump);
        line(&d, 2.0, 0.0, -2.0, 0.0);
        circle(&d, peak.u, peak.v + 0.3, 0.3);
        const SketchArrangement withCircle = deriveSketchArrangement(d);
        bool noSliver = true;
        for (const AtomicPlanarFace& face : withLine.faces) noSliver = noSliver && face.area > 0.1;
        for (const AtomicPlanarFace& face : withCircle.faces) noSliver = noSliver && face.area > 0.1;
        c.check("FILL_10a_a_line_tangent_to_a_span_is_a_touch_not_a_node",
                peak.v > 0.5 && withLine.status == ArrangementStatus::Ok
                        && withLine.stats.tangents >= 1u && withLine.stats.crossings == 0u
                        && withLine.faces.size() == 1u && !cutBy(withLine, 1u, 3u));
        c.check("FILL_10b_a_circle_tangent_to_a_span_is_a_touch_and_no_sliver_face_exists",
                withCircle.status == ArrangementStatus::Ok && withCircle.stats.tangents >= 1u
                        && withCircle.stats.crossings == 0u && withCircle.faces.size() == 2u
                        && noSliver);
    }
    // FILL-11: a spline whose two ENDS stand on the rectangle's top side: two
    // T-junctions split that side, and the rectangle splits into two cells.
    {
        CadSketch s;
        rect(&s, 0.0, 0.0, 4.0, 2.0);
        spline(&s, {SketchPoint{-1.0, 1.0}, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}});
        const SketchArrangement a = deriveSketchArrangement(s);
        c.check("FILL_11_a_spline_ending_on_a_side_splits_it_at_two_T_junctions",
                a.status == ArrangementStatus::Ok && a.stats.tJunctions == 2u
                        && a.faces.size() == 2u && near(faceAreaSum(a), 8.0, 1e-9)
                        && faceUses(a, 2u) && cutBy(a, 1u, 2u));
    }
    // FILL-12: shared stretches and a span meeting itself are refused by
    // name, deterministically.
    {
        CadSketch twin;
        spline(&twin, {SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}, SketchPoint{2.0, 0.0}});
        spline(&twin, {SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}, SketchPoint{2.0, 0.0}});
        CadSketch reversedTwin;
        spline(&reversedTwin, {SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}, SketchPoint{2.0, 0.0}});
        spline(&reversedTwin, {SketchPoint{2.0, 0.0}, SketchPoint{1.0, 1.0}, SketchPoint{0.0, 0.0}});
        CadSketch straight;
        spline(&straight, {SketchPoint{0.0, 0.0}, SketchPoint{2.0, 0.0}});
        line(&straight, 0.5, 0.0, 3.0, 0.0);
        CadSketch looped;
        spline(&looped, {SketchPoint{-11.0, -6.0}, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 0.0},
                         SketchPoint{12.0, -6.0}});
        const ArrangementStatus t1 = deriveSketchArrangement(twin).status;
        const ArrangementStatus t2 = deriveSketchArrangement(reversedTwin).status;
        const ArrangementStatus t3 = deriveSketchArrangement(straight).status;
        const ArrangementStatus t4 = deriveSketchArrangement(looped).status;
        c.check("FILL_12a_coincident_splines_and_a_straight_span_on_a_line_are_AmbiguousOverlap",
                t1 == ArrangementStatus::AmbiguousOverlap && t2 == ArrangementStatus::AmbiguousOverlap
                        && t3 == ArrangementStatus::AmbiguousOverlap
                        && deriveSketchArrangement(twin).status == t1);
        c.check("FILL_12b_a_span_that_loops_on_itself_is_SelfIntersectingCurve",
                t4 == ArrangementStatus::SelfIntersectingCurve
                        && cadStatusForArrangement(t4) == CadStatus::SelfIntersectingProfile
                        && deriveSketchArrangement(looped).status == t4);
    }
}

// ---------------------------------------------------------------------------
// The mode decision (S2CORR-16, S2CORR-18)
// ---------------------------------------------------------------------------

void testDecision(Checks& c) {
    // Legacy-exact sketches stay on the loop model and its legacy writer.
    {
        CadSketch one;
        rect(&one, 0.0, 0.0, 2.0, 2.0);
        CadSketch nested;
        rect(&nested, 0.0, 0.0, 4.0, 4.0);
        circle(&nested, -1.0, 0.0, 0.5);
        circle(&nested, 1.0, 0.0, 0.5);
        CadSketch loop;
        splineLoop(&loop);
        const SketchSelectionModeDecision a = decide(one);
        const SketchSelectionModeDecision b = decide(nested);
        const SketchSelectionModeDecision d = decide(loop);
        c.check("FILL_MODE_01_legacy_exact_sketches_stay_LoopRegions",
                a.status == CadStatus::Ok && a.kind == CadSelectionKind::LoopRegions
                        && b.status == CadStatus::Ok && b.kind == CadSelectionKind::LoopRegions
                        && d.status == CadStatus::Ok && d.kind == CadSelectionKind::LoopRegions);
    }
    // Crossing topology is PlanarFaces, splines included.
    {
        CadSketch crossing;
        rect(&crossing, 0.0, 0.0, 4.0, 3.0);
        circle(&crossing, 2.0, 0.0, 0.5);
        CadSketch splined;
        rect(&splined, 0.0, 0.0, 4.0, 2.0);
        splineLoop(&splined);
        c.check("FILL_MODE_02_crossing_topology_is_PlanarFaces_splines_included",
                decide(crossing).kind == CadSelectionKind::PlanarFaces
                        && decide(splined).kind == CadSelectionKind::PlanarFaces
                        && decide(splined).status == CadStatus::Ok);
    }
    // An arrangement that cannot be derived for a sketch whose loops cross is
    // REFUSED by the arrangement's own name -- never read as whole loops.
    {
        CadSketch s;
        rect(&s, 0.0, 0.0, 4.0, 3.0);
        circle(&s, 2.0, 0.0, 0.5);
        line(&s, -1.0, -1.5, 1.0, -1.5);  // along the bottom side: a shared stretch
        const SketchArrangement a = deriveSketchArrangement(s);
        const SketchRegionExtraction regions = extractSketchRegions(s);
        const SketchSelectionModeDecision mode = decideSketchSelectionMode(a, regions);
        c.check("FILL_MODE_03_a_failed_arrangement_over_crossing_loops_is_refused_by_name",
                a.status == ArrangementStatus::AmbiguousOverlap && !sketchLoopsAreExact(regions)
                        && mode.status == CadStatus::PlanarFaceAmbiguousOverlap
                        && mode.status != CadStatus::OverlappingRegions);
    }
    // ... while the same failure over loops that do not cross keeps the loop
    // model, exactly as every earlier build read that sketch.
    {
        CadSketch s;
        rect(&s, 0.0, 0.0, 4.0, 3.0);
        line(&s, -1.0, -1.5, 1.0, -1.5);
        const SketchSelectionModeDecision mode = decide(s);
        c.check("FILL_MODE_04_a_failed_arrangement_over_exact_loops_keeps_LoopRegions",
                deriveSketchArrangement(s).status == ArrangementStatus::AmbiguousOverlap
                        && mode.status == CadStatus::Ok
                        && mode.kind == CadSelectionKind::LoopRegions);
    }
}

// ---------------------------------------------------------------------------
// FILL-13: bounded work
// ---------------------------------------------------------------------------

void testBounds(Checks& c, std::string* performance) {
    // Four 16-point zigzag splines across eight lines: hundreds of spline
    // crossings, measured over a few runs (never a startup benchmark).
    CadSketch s;
    for (int k = 0; k < 4; ++k) {
        std::vector<SketchPoint> points;
        for (int i = 0; i < 16; ++i) {
            points.push_back(SketchPoint{-4.0 + i * 0.5 + k * 0.1, (i % 2 != 0 ? 2.0 : -2.0) + k * 0.05});
        }
        spline(&s, points);
    }
    for (int j = 0; j < 8; ++j) line(&s, -5.0, -1.75 + j * 0.5, 5.0, -1.75 + j * 0.5);
    constexpr int kRuns = 3;
    std::vector<long long> micros;
    SketchArrangement first;
    bool same = true;
    for (int run = 0; run < kRuns; ++run) {
        const auto t0 = std::chrono::steady_clock::now();
        SketchArrangement a = deriveSketchArrangement(s);
        micros.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - t0)
                                 .count());
        if (run == 0) {
            first = std::move(a);
        } else {
            same = same && sameFaces(first, a);
        }
    }
    c.check("FILL_13a_a_dense_spline_sketch_derives_deterministically_within_the_caps",
            first.status == ArrangementStatus::Ok && first.stats.crossings > 400u
                    && first.faces.size() > 400u && same);
    // The largest spline a sketch may carry, eight times over: bounded.
    CadSketch biggest;
    for (int k = 0; k < 8; ++k) {
        std::vector<SketchPoint> points;
        for (uint32_t i = 0; i < kMaxSplinePoints; ++i) {
            points.push_back(SketchPoint{-8.0 + i * 0.5 + k * 0.07, (i % 2 != 0 ? 3.0 : -3.0) + k * 0.03});
        }
        spline(&biggest, points);
    }
    const auto t0 = std::chrono::steady_clock::now();
    const SketchArrangement big = deriveSketchArrangement(biggest);
    const long long bigMicros = std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::steady_clock::now() - t0)
                                        .count();
    c.check("FILL_13b_eight_maximal_splines_derive_with_bounded_work",
            big.status == ArrangementStatus::Ok && big.stats.sourceEdges == 8u * (kMaxSplinePoints - 1u)
                    && !big.faces.empty());
    std::sort(micros.begin(), micros.end());
    char text[160];
    std::snprintf(text, sizeof(text), "fill_spline_us=%lld/%lld runs=%d cells=%zu max_splines_us=%lld",
                  micros[micros.size() / 2], micros.back(), kRuns, first.faces.size(), bigMicros);
    *performance = text;
}

}  // namespace

void runSketchFillSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    Checks c{out};
    testCells(c);
    testIdentity(c);
    testContacts(c);
    testDecision(c);
    testBounds(c, performance);
}

}  // namespace forgeshape
