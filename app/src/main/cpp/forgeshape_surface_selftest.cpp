#include "forgeshape_surface_selftest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "forgeshape_history.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_project_surface.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_surface.h"
#include "forgeshape_surface_authoring.h"
#include "forgeshape_camera.h"

namespace forgeshape {
namespace {

struct Checks {
    SurfaceSelfTestResult* out;
    int maxOut;
    int count = 0;
    void check(const char* name, bool ok) {
        if (count < maxOut) out[count] = SurfaceSelfTestResult{name, ok};
        ++count;
    }
};

std::string g_performance;
std::string g_digests;

// ---------------------------------------------------------------------------
// Builders
// ---------------------------------------------------------------------------

SketchEntityId add(CadSketch* sketch, SketchEntity::Payload payload) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, std::move(payload), &id);
    return id;
}

SketchRectangle rect(double cu, double cv, double w, double h) {
    SketchRectangle r;
    r.center = SketchPoint{cu, cv};
    r.width = w;
    r.height = h;
    return r;
}

SketchCircle circle(double cu, double cv, double radius) {
    SketchCircle c;
    c.center = SketchPoint{cu, cv};
    c.radius = radius;
    return c;
}

SketchLine line(double u0, double v0, double u1, double v1) {
    SketchLine l;
    l.start = SketchPoint{u0, v0};
    l.end = SketchPoint{u1, v1};
    return l;
}

CadSketch sketchOf(Workplane plane = Workplane::XY) {
    CadSketch s;
    s.plane = plane;
    return s;
}

// The 32-gon a circle is, measured -- never pi r^2.
double polygonArea(double radius) {
    return 0.5 * polygonSignedAreaTwice(circleProfilePolygon(circle(0.0, 0.0, radius)));
}

SurfaceFeature patchFeature(uint32_t sketchId, const CadSketch& sketch) {
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::PlanarPatch;
    f.section.sketchId = sketchId;
    f.regions = surfaceDefaultPatchRegions(sketch);
    return f;
}

SurfaceFeature extrudeFeature(uint32_t sketchId, std::vector<SketchEntityId> curves, double distance) {
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::ExtrudedSurface;
    f.section.sketchId = sketchId;
    f.section.curves = std::move(curves);
    f.distance = distance;
    return f;
}

SurfaceFeature revolveFeature(uint32_t sketchId, std::vector<SketchEntityId> curves, SketchEntityId axis,
                              double degrees) {
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::RevolvedSurface;
    f.section.sketchId = sketchId;
    f.section.curves = std::move(curves);
    f.axis = CadSketchEdgeRef{axis, 0};
    f.angleDegrees = degrees;
    return f;
}

SurfaceFeature loftFeature(uint32_t a, std::vector<SketchEntityId> ca, uint32_t b, std::vector<SketchEntityId> cb) {
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::LoftSurface;
    f.section.sketchId = a;
    f.section.curves = std::move(ca);
    f.sectionB.sketchId = b;
    f.sectionB.curves = std::move(cb);
    return f;
}

SurfaceFeature trimFeature(uint32_t sketchId, const CadSketch& sketch, SurfaceFeatureId target, bool keepInside) {
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::TrimSurface;
    f.section.sketchId = sketchId;
    f.regions = surfaceDefaultPatchRegions(sketch);
    f.target = target;
    f.keepInside = keepInside;
    return f;
}

SurfaceFeature stitchFeature(std::vector<SurfaceFeatureId> ids) {
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::Stitch;
    f.stitchFeatures = std::move(ids);
    return f;
}

SurfaceFeature thickenFeature(SurfaceFeatureId source, double thickness) {
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::Thicken;
    f.source = source;
    f.thickness = thickness;
    return f;
}

double patchArea(const SurfacePatch& p) {
    double area = 0.0;
    for (size_t t = 0; t + 2 < p.triangles.size(); t += 3) {
        const DVec3 a = p.positions[p.triangles[t]];
        const DVec3 b = p.positions[p.triangles[t + 1]];
        const DVec3 c = p.positions[p.triangles[t + 2]];
        const DVec3 n = dvec3Cross(dvec3Sub(b, a), dvec3Sub(c, a));
        area += 0.5 * std::sqrt(dvec3Dot(n, n));
    }
    return area;
}

bool near(double a, double b, double tolerance = 1e-9) { return std::fabs(a - b) <= tolerance; }

SurfaceStatus regen(const SurfaceBodyState& s, SurfaceBodyMesh* mesh, SurfaceRegenerationReport* report = nullptr) {
    return regenerateSurfaceBody(s, mesh, report);
}

// A 2 x 2 rectangle on XY as a planar patch.
SurfaceBodyState patchState(double w = 2.0, double h = 2.0) {
    SurfaceBodyState s;
    CadSketch sk = sketchOf();
    add(&sk, rect(0.0, 0.0, w, h));
    const uint32_t id = appendSurfaceSketch(&s, sk, 0.0);
    appendSurfaceFeature(&s, patchFeature(id, sk));
    return s;
}

// The trimmed-and-thickened chain SURF-19/20 edit: a w x w patch, a hole of
// radius 1 trimmed out of it, then 0.2 m of material.
SurfaceBodyState chainState(double w) {
    SurfaceBodyState s = patchState(w, w);
    CadSketch cutter = sketchOf();
    add(&cutter, circle(0.0, 0.0, 1.0));
    const uint32_t cutterId = appendSurfaceSketch(&s, cutter, 0.0);
    appendSurfaceFeature(&s, trimFeature(cutterId, cutter, SurfaceFeatureId{1}, false));
    appendSurfaceFeature(&s, thickenFeature(SurfaceFeatureId{2}, 0.2));
    return s;
}

// ---------------------------------------------------------------------------
// SURF-01 .. SURF-02: planar patches
// ---------------------------------------------------------------------------

void testPatches(Checks& r) {
    SurfaceBodyMesh mesh;
    const bool ok = regen(patchState(), &mesh) == SurfaceStatus::Ok;
    bool flat = ok && mesh.patches.size() == 1u;
    for (size_t i = 0; flat && i < mesh.patches[0].positions.size(); ++i) flat = mesh.patches[0].positions[i].z == 0.0;
    r.check("SURF_01_a_closed_profile_makes_one_planar_patch_of_its_area",
            flat && mesh.patches[0].shape == SurfacePatchShape::Planar && near(patchArea(mesh.patches[0]), 4.0)
                    && mesh.patches[0].edges.size() == 1u && mesh.patches[0].edges[0].closed
                    && mesh.patches[0].edges[0].points.size() == 4u && idOf(mesh.patches[0].id.feature) == 1u
                    && mesh.patches[0].id.ordinal == 0u && mesh.solid.triangleCount() == 0u);

    SurfaceBodyState holed;
    CadSketch sk = sketchOf();
    add(&sk, rect(0.0, 0.0, 4.0, 4.0));
    add(&sk, circle(0.0, 0.0, 1.0));
    const uint32_t id = appendSurfaceSketch(&holed, sk, 0.0);
    appendSurfaceFeature(&holed, patchFeature(id, sk));
    SurfaceBodyMesh holedMesh;
    const bool holedOk = regen(holed, &holedMesh) == SurfaceStatus::Ok && holedMesh.patches.size() == 1u;
    r.check("SURF_02_a_loop_inside_a_loop_is_a_hole_the_patch_keeps",
            holedOk && holedMesh.patches[0].loops.size() == 2u && holedMesh.patches[0].edges.size() == 2u
                    && near(patchArea(holedMesh.patches[0]), 16.0 - polygonArea(1.0), 1e-9)
                    && holed.features[0].regions.size() == 1u
                    && holed.features[0].regions[0].holeAnchorIds.size() == 1u);
    SurfaceBodyState none = patchState();
    none.features[0].regions.clear();
    r.check("SURF_02_a_patch_without_a_region_is_refused_by_name",
            regen(none, nullptr) == SurfaceStatus::PayloadMismatch);
}

// ---------------------------------------------------------------------------
// SURF-03 .. SURF-06: extruded and revolved surfaces
// ---------------------------------------------------------------------------

void testSweeps(Checks& r) {
    SurfaceBodyState s;
    CadSketch sk = sketchOf();
    const SketchEntityId a = add(&sk, line(1.0, 0.0, 1.0, 1.0));
    const SketchEntityId b = add(&sk, line(0.0, 0.0, 1.0, 0.0));
    const uint32_t id = appendSurfaceSketch(&s, sk, 0.0);
    appendSurfaceFeature(&s, extrudeFeature(id, {a, b}, 0.5));
    SurfaceBodyMesh mesh;
    const bool ok = regen(s, &mesh) == SurfaceStatus::Ok && mesh.patches.size() == 1u;
    bool topAt = ok;
    for (size_t i = 3; ok && i < 6; ++i) topAt = topAt && mesh.patches[0].positions[i].z == 0.5;
    r.check("SURF_03_an_open_line_chain_extrudes_to_one_ruled_patch_without_caps",
            ok && mesh.patches[0].shape == SurfacePatchShape::Ruled && mesh.patches[0].positions.size() == 6u
                    && mesh.patches[0].triangles.size() == 12u && mesh.patches[0].edges.size() == 4u
                    && topAt && near(patchArea(mesh.patches[0]), 1.0) && mesh.open
                    && mesh.patches[0].chainStraight && !mesh.patches[0].chainClosed
                    // The walk starts at the free end of the smaller id: line 1's (1, 1).
                    && mesh.patches[0].positions[0].x == 1.0 && mesh.patches[0].positions[0].y == 1.0);

    SurfaceBodyState sp;
    CadSketch splineSketch = sketchOf();
    SketchSpline spline;
    spline.points = {{0.0, 0.0}, {0.5, 0.4}, {1.0, 0.0}, {1.5, -0.3}};
    const SketchEntityId curve = add(&splineSketch, spline);
    const uint32_t spId = appendSurfaceSketch(&sp, splineSketch, 0.0);
    appendSurfaceFeature(&sp, extrudeFeature(spId, {curve}, 1.0));
    SurfaceBodyMesh spMesh;
    std::vector<SketchPoint> tess;
    tessellateSketchCurve(splineSketch.entities[0], &tess);
    r.check("SURF_04_a_spline_extrudes_through_its_own_tessellation",
            regen(sp, &spMesh) == SurfaceStatus::Ok && spMesh.patches.size() == 1u
                    && spMesh.patches[0].positions.size() == 2u * tess.size() && !spMesh.patches[0].chainStraight);
    SurfaceBodyState zero = sp;
    zero.features[0].distance = 0.0;
    SurfaceBodyState fork = s;
    fork.sketches[0].sketch = sk;
    add(&fork.sketches[0].sketch, line(1.0, 0.0, 2.0, 0.0));
    fork.features[0].section.curves = {1, 2, 3};
    r.check("SURF_04_a_zero_distance_or_a_forked_chain_is_refused",
            regen(zero, nullptr) == SurfaceStatus::DistanceInvalid
                    && regen(fork, nullptr) == SurfaceStatus::CurveChainForked);

    // Revolve: a segment parallel to the axis u = 0 sweeps a cylinder.
    SurfaceBodyState rv;
    CadSketch rs = sketchOf();
    const SketchEntityId profile = add(&rs, line(1.0, 0.0, 1.0, 1.0));
    const SketchEntityId axis = add(&rs, line(0.0, -1.0, 0.0, 2.0));
    const uint32_t rsId = appendSurfaceSketch(&rv, rs, 0.0);
    appendSurfaceFeature(&rv, revolveFeature(rsId, {profile}, axis, 360.0));
    SurfaceBodyMesh rvMesh;
    bool onRadius = regen(rv, &rvMesh) == SurfaceStatus::Ok && rvMesh.patches.size() == 1u;
    for (size_t i = 0; onRadius && i < rvMesh.patches[0].positions.size(); ++i) {
        const DVec3 p = rvMesh.patches[0].positions[i];
        onRadius = near(std::hypot(p.x, p.z), 1.0, 1e-12);
    }
    r.check("SURF_05_an_open_profile_revolves_to_an_uncapped_surface",
            onRadius && rvMesh.patches[0].shape == SurfacePatchShape::Revolved
                    && rvMesh.patches[0].positions.size() == 64u && rvMesh.patches[0].edges.size() == 2u
                    && rvMesh.patches[0].edges[0].closed && rvMesh.open);

    SurfaceBodyState partial = rv;
    partial.features[0].angleDegrees = 90.0;
    SurfaceBodyMesh pMesh;
    const bool pOk = regen(partial, &pMesh) == SurfaceStatus::Ok;
    const DVec3 end = pOk ? pMesh.patches[0].edges[1].points[0] : DVec3{};
    SurfaceBodyState apex;
    CadSketch as = sketchOf();
    const SketchEntityId cone = add(&as, line(0.0, 0.0, 1.0, 1.0));
    const SketchEntityId apexAxis = add(&as, line(0.0, -1.0, 0.0, 2.0));
    const uint32_t asId = appendSurfaceSketch(&apex, as, 0.0);
    appendSurfaceFeature(&apex, revolveFeature(asId, {cone}, apexAxis, 360.0));
    SurfaceBodyMesh apexMesh;
    SurfaceBodyState crossing = apex;
    crossing.sketches[0].sketch = sketchOf();
    add(&crossing.sketches[0].sketch, line(-1.0, 0.0, 1.0, 1.0));
    add(&crossing.sketches[0].sketch, line(0.0, -1.0, 0.0, 2.0));
    SurfaceBodyState badAngle = partial;
    badAngle.features[0].angleDegrees = 400.0;
    r.check("SURF_06_a_partial_revolve_has_two_profile_edges_and_an_on_axis_apex",
            pOk && pMesh.patches[0].positions.size() == 2u * 9u && pMesh.patches[0].edges.size() == 4u
                    && near(end.x, 0.0, 1e-12) && near(std::fabs(end.z), 1.0, 1e-12)
                    && regen(apex, &apexMesh) == SurfaceStatus::Ok
                    && apexMesh.patches[0].positions.size() == 1u + 32u
                    && regen(crossing, nullptr) == SurfaceStatus::ProfileCrossesAxis
                    && regen(badAngle, nullptr) == SurfaceStatus::AngleInvalid);
}

// ---------------------------------------------------------------------------
// SURF-07 .. SURF-10: loft
// ---------------------------------------------------------------------------

SurfaceBodyState loftState(SketchEntity::Payload a, SketchEntity::Payload b) {
    SurfaceBodyState s;
    CadSketch sa = sketchOf();
    const SketchEntityId ea = add(&sa, std::move(a));
    CadSketch sb = sketchOf();
    const SketchEntityId eb = add(&sb, std::move(b));
    const uint32_t ia = appendSurfaceSketch(&s, sa, 0.0);
    const uint32_t ib = appendSurfaceSketch(&s, sb, 1.0);
    appendSurfaceFeature(&s, loftFeature(ia, {ea}, ib, {eb}));
    return s;
}

void testLoft(Checks& r) {
    SurfaceBodyMesh open;
    const bool openOk = regen(loftState(line(-1, 0, 1, 0), line(-1, 0.5, 1, 0.5)), &open) == SurfaceStatus::Ok;
    r.check("SURF_07_open_to_open_lofts_one_ruled_patch",
            openOk && open.patches.size() == 1u && open.patches[0].shape == SurfacePatchShape::Lofted
                    && open.patches[0].edges.size() == 4u && open.patches[0].positions[2].z == 1.0);
    SurfaceBodyMesh closed;
    const bool closedOk = regen(loftState(circle(0, 0, 1.0), rect(0, 0, 2.0, 2.0)), &closed) == SurfaceStatus::Ok;
    r.check("SURF_08_closed_to_closed_lofts_at_one_sample_count",
            closedOk && closed.patches[0].positions.size() == 64u && closed.patches[0].edges.size() == 2u
                    && closed.patches[0].edges[0].closed);
    SurfaceBodyState base = loftState(circle(0, 0, 1.0), circle(0, 0, 0.5));
    SurfaceBodyMesh m1, m2, m3;
    regen(base, &m1);
    regen(base, &m2);
    SurfaceBodyState reversed = base;
    reversed.features[0].reverseB = true;
    regen(reversed, &m3);
    SurfaceBodyState offset = base;
    offset.features[0].startOffsetB = 40;
    SurfaceBodyState openOffset = loftState(line(-1, 0, 1, 0), line(-1, 0.5, 1, 0.5));
    openOffset.features[0].startOffsetB = 1;
    r.check("SURF_09_loft_correspondence_is_durable_and_deterministic",
            surfaceMeshDigest(m1) == surfaceMeshDigest(m2) && surfaceMeshDigest(m1) != surfaceMeshDigest(m3)
                    && regen(offset, nullptr) == SurfaceStatus::LoftCorrespondenceInvalid
                    && regen(openOffset, nullptr) == SurfaceStatus::LoftCorrespondenceInvalid);
    SurfaceBodyState two = base;
    add(&two.sketches[1].sketch, circle(3.0, 0.0, 0.5));
    two.features[0].sectionB.curves = {1, 2};
    SurfaceBodyState same = loftState(circle(0, 0, 1.0), circle(0, 0, 1.0));
    same.sketches[1].offset = 0.0;
    r.check("SURF_10_open_to_closed_and_ambiguous_sections_are_refused",
            regen(loftState(line(-1, 0, 1, 0), circle(0, 0, 1)), nullptr) == SurfaceStatus::LoftSectionMismatch
                    && regen(two, nullptr) == SurfaceStatus::LoftSectionCount
                    && regen(same, nullptr) == SurfaceStatus::LoftSectionsCoincide);
}

// ---------------------------------------------------------------------------
// SURF-11 .. SURF-12: trim
// ---------------------------------------------------------------------------

void testTrim(Checks& r) {
    SurfaceBodyState s = patchState(4.0, 4.0);
    CadSketch cutter = sketchOf();
    add(&cutter, circle(0.0, 0.0, 1.0));
    const uint32_t cid = appendSurfaceSketch(&s, cutter, 0.0);
    SurfaceBodyState outside = s;
    appendSurfaceFeature(&outside, trimFeature(cid, cutter, SurfaceFeatureId{1}, false));
    SurfaceBodyState inside = s;
    appendSurfaceFeature(&inside, trimFeature(cid, cutter, SurfaceFeatureId{1}, true));
    SurfaceBodyMesh om, im;
    const bool ok = regen(outside, &om) == SurfaceStatus::Ok && regen(inside, &im) == SurfaceStatus::Ok;
    r.check("SURF_11_a_coplanar_trim_cuts_exactly_and_makes_semantic_boundary_edges",
            ok && om.patches.size() == 1u && idOf(om.patches[0].id.feature) == 2u && om.patches[0].edges.size() == 2u
                    && near(patchArea(om.patches[0]), 16.0 - polygonArea(1.0), 1e-6) && im.patches.size() == 1u
                    && near(patchArea(im.patches[0]), polygonArea(1.0), 1e-6) && im.patches[0].edges.size() == 1u);

    // A trim of a non-planar surface is refused by name -- never emulated by
    // deleting triangles -- and so is a trim sketch off the patch's plane.
    SurfaceBodyState ruled;
    CadSketch ls = sketchOf();
    const SketchEntityId l = add(&ls, line(-2, 0, 2, 0));
    const uint32_t lid = appendSurfaceSketch(&ruled, ls, 0.0);
    appendSurfaceFeature(&ruled, extrudeFeature(lid, {l}, 1.0));
    const uint32_t rc = appendSurfaceSketch(&ruled, cutter, 0.0);
    appendSurfaceFeature(&ruled, trimFeature(rc, cutter, SurfaceFeatureId{1}, false));
    SurfaceRegenerationReport report;
    SurfaceBodyMesh untouched;
    untouched.openEdgeCount = 99;
    SurfaceBodyState offPlane = s;
    const uint32_t far = appendSurfaceSketch(&offPlane, cutter, 0.5);
    appendSurfaceFeature(&offPlane, trimFeature(far, cutter, SurfaceFeatureId{1}, false));
    r.check("SURF_12_an_unsupported_trim_is_refused_by_name_and_writes_nothing",
            regen(ruled, &untouched, &report) == SurfaceStatus::TrimUnsupportedTarget
                    && idOf(report.failedFeature) == 2u && untouched.openEdgeCount == 99u
                    && untouched.patches.empty() && regen(offPlane, nullptr) == SurfaceStatus::TrimNotCoplanar);
}

// ---------------------------------------------------------------------------
// SURF-13 .. SURF-15: stitch
// ---------------------------------------------------------------------------

// A 2 x 2 patch beside a tube extruded from a rectangle `w` wide: the patch's
// loop and the tube's bottom coincide when w is 2.
SurfaceBodyState capAndTube(double w) {
    SurfaceBodyState s = patchState(2.0, 2.0);
    CadSketch ts = sketchOf();
    const SketchEntityId tube = add(&ts, rect(0.0, 0.0, w, 2.0));
    const uint32_t tid = appendSurfaceSketch(&s, ts, 0.0);
    appendSurfaceFeature(&s, extrudeFeature(tid, {tube}, 1.0));
    return s;
}

void testStitch(Checks& r) {
    SurfaceBodyState s = capAndTube(2.0);
    appendSurfaceFeature(&s, stitchFeature({SurfaceFeatureId{1}, SurfaceFeatureId{2}}));
    SurfaceBodyMesh mesh;
    const bool ok = regen(s, &mesh) == SurfaceStatus::Ok;
    const std::vector<SurfaceEdgeId> open = ok ? surfaceOpenEdges(mesh) : std::vector<SurfaceEdgeId>{};
    r.check("SURF_13_coincident_boundary_edges_stitch_into_one_shell",
            ok && mesh.stitches.size() == 1u && idOf(mesh.stitches[0].a.patch.feature) == 1u
                    && idOf(mesh.stitches[0].b.patch.feature) == 2u && mesh.stitches[0].b.ordinal == 0u
                    && open.size() == 1u && idOf(open[0].patch.feature) == 2u && open[0].ordinal == 1u
                    && mesh.openEdgeCount == 1u);
    SurfaceBodyState gap = capAndTube(2.0005);
    appendSurfaceFeature(&gap, stitchFeature({SurfaceFeatureId{1}, SurfaceFeatureId{2}}));
    SurfaceBodyState far = capAndTube(3.0);
    appendSurfaceFeature(&far, stitchFeature({SurfaceFeatureId{1}, SurfaceFeatureId{2}}));
    r.check("SURF_14_a_gap_is_refused_and_never_averaged_shut",
            regen(gap, nullptr) == SurfaceStatus::StitchGapTooLarge
                    && regen(far, nullptr) == SurfaceStatus::StitchNoCompatibleEdges);
    SurfaceBodyState three = capAndTube(2.0);
    CadSketch again = sketchOf();
    add(&again, rect(0.0, 0.0, 2.0, 2.0));
    const uint32_t aid = appendSurfaceSketch(&three, again, 0.0);
    appendSurfaceFeature(&three, patchFeature(aid, again));
    appendSurfaceFeature(&three, stitchFeature({SurfaceFeatureId{1}, SurfaceFeatureId{2}, SurfaceFeatureId{3}}));
    // Two open edges with the same ends and different interiors.
    SurfaceBodyState bent;
    CadSketch bs = sketchOf();
    const SketchEntityId straight = add(&bs, line(0, 0, 1, 0));
    CadSketch arcSketch = sketchOf();
    SketchArc arc;
    arc.start = SketchPoint{0, 0};
    arc.mid = SketchPoint{0.5, 0.3};
    arc.end = SketchPoint{1, 0};
    const SketchEntityId curved = add(&arcSketch, arc);
    const uint32_t b1 = appendSurfaceSketch(&bent, bs, 0.0);
    const uint32_t b2 = appendSurfaceSketch(&bent, arcSketch, 0.0);
    appendSurfaceFeature(&bent, extrudeFeature(b1, {straight}, 1.0));
    appendSurfaceFeature(&bent, extrudeFeature(b2, {curved}, 1.0));
    appendSurfaceFeature(&bent, stitchFeature({SurfaceFeatureId{1}, SurfaceFeatureId{2}}));
    r.check("SURF_15_a_non_manifold_or_incompatible_stitch_is_refused",
            regen(three, nullptr) == SurfaceStatus::StitchNonManifold
                    && regen(bent, nullptr) == SurfaceStatus::StitchIncompatibleBoundary);
}

// ---------------------------------------------------------------------------
// SURF-16 .. SURF-18: thicken
// ---------------------------------------------------------------------------

void testThicken(Checks& r) {
    SurfaceBodyState s = patchState();
    appendSurfaceFeature(&s, thickenFeature(SurfaceFeatureId{1}, 0.5));
    SurfaceBodyState down = patchState();
    appendSurfaceFeature(&down, thickenFeature(SurfaceFeatureId{1}, -0.5));
    SurfaceBodyMesh up, under;
    const bool ok = regen(s, &up) == SurfaceStatus::Ok && regen(down, &under) == SurfaceStatus::Ok;
    CadSolidMeasure measure;
    const bool valid = ok && cadKernelValidateSolid(up.solid, &measure) == CadKernelStatus::Ok;
    double lowest = 1e9;
    for (size_t i = 2; ok && i < under.solid.positions.size(); i += 3) lowest = std::min(lowest, under.solid.positions[i]);
    r.check("SURF_16_a_planar_patch_thickens_to_a_valid_closed_solid_either_way",
            valid && near(measure.volume, 2.0, 1e-9) && up.patches.empty() && !up.render.renderBothSides
                    && near(cadSolidVolume(under.solid), 2.0, 1e-9) && lowest == -0.5);
    SurfaceBodyState wall;
    CadSketch ws = sketchOf();
    const SketchEntityId w1 = add(&ws, line(0, 0, 1, 0));
    const SketchEntityId w2 = add(&ws, line(1, 0, 1, 1));
    const uint32_t wid = appendSurfaceSketch(&wall, ws, 0.0);
    appendSurfaceFeature(&wall, extrudeFeature(wid, {w1, w2}, 1.0));
    appendSurfaceFeature(&wall, thickenFeature(SurfaceFeatureId{1}, 0.1));
    SurfaceBodyState tube;
    CadSketch cs = sketchOf();
    const SketchEntityId c = add(&cs, circle(0, 0, 1.0));
    const uint32_t cid = appendSurfaceSketch(&tube, cs, 0.0);
    appendSurfaceFeature(&tube, extrudeFeature(cid, {c}, 2.0));
    appendSurfaceFeature(&tube, thickenFeature(SurfaceFeatureId{1}, 0.1));
    SurfaceBodyMesh wm, tm;
    const bool wallOk = regen(wall, &wm) == SurfaceStatus::Ok;
    const bool tubeOk = regen(tube, &tm) == SurfaceStatus::Ok;
    r.check("SURF_17_a_ruled_surface_over_lines_or_a_circle_thickens_exactly",
            wallOk && near(cadSolidVolume(wm.solid), 0.19, 1e-9) && tubeOk
                    && near(cadSolidVolume(tm.solid), 2.0 * (polygonArea(1.1) - polygonArea(1.0)), 1e-9));
    SurfaceBodyState rv;
    CadSketch rs = sketchOf();
    const SketchEntityId p = add(&rs, line(1, 0, 1, 1));
    const SketchEntityId ax = add(&rs, line(0, -1, 0, 2));
    const uint32_t rid = appendSurfaceSketch(&rv, rs, 0.0);
    appendSurfaceFeature(&rv, revolveFeature(rid, {p}, ax, 360.0));
    appendSurfaceFeature(&rv, thickenFeature(SurfaceFeatureId{1}, 0.1));
    SurfaceBodyState lofted = loftState(circle(0, 0, 1.0), circle(0, 0, 0.5));
    appendSurfaceFeature(&lofted, thickenFeature(SurfaceFeatureId{1}, 0.1));
    SurfaceBodyState spline;
    CadSketch ss = sketchOf();
    SketchSpline sp;
    sp.points = {{0, 0}, {0.5, 0.4}, {1, 0}};
    const SketchEntityId se = add(&ss, sp);
    const uint32_t sid = appendSurfaceSketch(&spline, ss, 0.0);
    appendSurfaceFeature(&spline, extrudeFeature(sid, {se}, 1.0));
    appendSurfaceFeature(&spline, thickenFeature(SurfaceFeatureId{1}, 0.1));
    SurfaceBodyState zero = patchState();
    appendSurfaceFeature(&zero, thickenFeature(SurfaceFeatureId{1}, 0.0));
    r.check("SURF_18_an_undefined_offset_is_refused_by_name",
            regen(rv, nullptr) == SurfaceStatus::ThickenUnsupportedForSurfaceType
                    && regen(lofted, nullptr) == SurfaceStatus::ThickenUnsupportedForSurfaceType
                    && regen(spline, nullptr) == SurfaceStatus::ThickenUnsupportedForSurfaceType
                    && regen(zero, nullptr) == SurfaceStatus::ThickenInvalidThickness);
}

// ---------------------------------------------------------------------------
// SURF-19 .. SURF-24: history, persistence, determinism, openness
// ---------------------------------------------------------------------------

struct Rig {
    ConstructionScene scene{NoProjectTag{}};
    ConstructionHistory history{scene};
    ObjectId id = kNoObject;
    explicit Rig(const SurfaceBodyState& state) {
        history.beginSessionInitialization();
        SceneObject* body = scene.addSurfaceBody(state);
        if (body != nullptr) {
            id = body->objectId();
            publishSceneObject(*body);
        }
        history.endSessionInitialization();
    }
    SurfaceBody* body() {
        SceneObject* b = scene.findBody(id);
        return b != nullptr ? b->surfaceOrNull() : nullptr;
    }
    std::vector<uint8_t> bytes() const {
        return encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction));
    }
};

ProjectDocument documentFor(const SurfaceBodyState& state) {
    ConstructionScene scene((NoProjectTag()));
    SceneObject* object = scene.addSurfaceBody(state);
    if (object == nullptr) return ProjectDocument{};
    publishSceneObject(*object);
    return captureProjectDocument(scene, ProjectKind::Construction);
}

size_t sectionOffset(const std::vector<uint8_t>& bytes, const char tag[4]) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t length = 0;
        for (int i = 7; i >= 0; --i) length = (length << 8) | bytes[offset + 8u + static_cast<size_t>(i)];
        if (std::memcmp(&bytes[offset], tag, 4) == 0) return offset;
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(length);
    }
    return 0;
}

ProjectCodecStatus decodeStatus(const std::vector<uint8_t>& bytes) {
    ProjectDocument decoded;
    return decodeProject(bytes.data(), bytes.size(), &decoded);
}

// Patches a SURF payload in place and re-seals the CRC.
std::vector<uint8_t> patched(const std::vector<uint8_t>& bytes, size_t payloadOffset, uint32_t value, int width) {
    std::vector<uint8_t> copy = bytes;
    const size_t at = sectionOffset(copy, kSectionTagSurface);
    const size_t payload = at + kForgeSectionHeaderBytes;
    for (int i = 0; i < width; ++i) copy[payload + payloadOffset + static_cast<size_t>(i)] = static_cast<uint8_t>(value >> (8 * i));
    uint64_t length = 0;
    for (int i = 7; i >= 0; --i) length = (length << 8) | copy[at + 8u + static_cast<size_t>(i)];
    const uint32_t crc = crc32IsoHdlc(copy.data() + payload, static_cast<size_t>(length));
    for (int i = 0; i < 4; ++i) copy[at + 16u + static_cast<size_t>(i)] = static_cast<uint8_t>(crc >> (8 * i));
    return copy;
}

// The corpus documents (DATA_PACKAGE_SPEC.md §7k).
SurfaceBodyState corpusPatchExtrude() {
    SurfaceBodyState s = patchState();
    CadSketch ls = sketchOf();
    const SketchEntityId a = add(&ls, line(0, 0, 1, 0));
    const SketchEntityId b = add(&ls, line(1, 0, 1, 1));
    const uint32_t id = appendSurfaceSketch(&s, ls, 0.0);
    appendSurfaceFeature(&s, extrudeFeature(id, {a, b}, 0.5));
    return s;
}

SurfaceBodyState corpusLoftTrimStitch() {
    SurfaceBodyState s = patchState(4.0, 4.0);                 // 1: patch, sketch 1
    CadSketch cutter = sketchOf();
    add(&cutter, circle(0.0, 0.0, 1.0));
    const uint32_t c = appendSurfaceSketch(&s, cutter, 0.0);    // sketch 2
    appendSurfaceFeature(&s, trimFeature(c, cutter, SurfaceFeatureId{1}, false));  // 2: trim
    CadSketch ts = sketchOf();
    const SketchEntityId tube = add(&ts, rect(0.0, 0.0, 4.0, 4.0));
    const uint32_t t = appendSurfaceSketch(&s, ts, 0.0);        // sketch 3
    appendSurfaceFeature(&s, extrudeFeature(t, {tube}, 1.0));   // 3: tube
    appendSurfaceFeature(&s, stitchFeature({SurfaceFeatureId{2}, SurfaceFeatureId{3}}));  // 4
    CadSketch top = sketchOf();
    const SketchEntityId small = add(&top, circle(0.0, 0.0, 0.5));
    const uint32_t k = appendSurfaceSketch(&s, top, 2.0);       // sketch 4
    CadSketch mid = sketchOf();
    const SketchEntityId ring = add(&mid, circle(0.0, 0.0, 1.0));
    const uint32_t m = appendSurfaceSketch(&s, mid, 1.0);       // sketch 5
    appendSurfaceFeature(&s, loftFeature(m, {ring}, k, {small}));  // 5: loft
    return s;
}

void testHistoryAndFormat(Checks& r) {
    // SURF-19/20: an upstream edit regenerates downstream, and the first
    // failure is attributed by id.
    SurfaceBodyState chain = chainState(4.0);
    SurfaceBodyMesh before;
    const bool built = regen(chain, &before) == SurfaceStatus::Ok;
    SurfaceBodyState wider = chain;
    wider.sketches[0].sketch.entities[0] = SketchEntity(1, rect(0.0, 0.0, 6.0, 6.0));
    SurfaceBodyMesh after;
    r.check("SURF_19_an_upstream_edit_regenerates_every_downstream_feature",
            built && near(cadSolidVolume(before.solid), (16.0 - polygonArea(1.0)) * 0.2, 1e-9)
                    && regen(wider, &after) == SurfaceStatus::Ok
                    && near(cadSolidVolume(after.solid), (36.0 - polygonArea(1.0)) * 0.2, 1e-9));
    SurfaceBodyState tiny = chain;
    tiny.sketches[0].sketch.entities[0] = SketchEntity(1, rect(0.0, 0.0, 1.0, 1.0));
    SurfaceRegenerationReport report;
    r.check("SURF_20_the_first_failing_downstream_feature_is_named",
            regen(tiny, nullptr, &report) == SurfaceStatus::TrimRemovesPatch && idOf(report.failedFeature) == 2u
                    && report.status == SurfaceStatus::TrimRemovesPatch);

    // SURF-21: Undo/Redo restores the exact feature list.
    Rig rig(patchState());
    const SurfaceBodyState original = rig.body()->state();
    SurfaceBodyState thick = original;
    appendSurfaceFeature(&thick, thickenFeature(SurfaceFeatureId{1}, 0.3));
    bool applied = false;
    {
        ScopedConstructionEdit edit(rig.history);
        applied = rig.body()->applyState(thick) == SurfaceStatus::Ok;
        publishSceneObject(*rig.scene.findBody(rig.id));
    }
    const uint64_t thickDigest = surfaceMeshDigest(rig.body()->mesh());
    const bool undone = rig.history.undo() && sameSurfaceBodyState(rig.body()->state(), original)
                        && rig.body()->mesh().solid.triangleCount() == 0u;
    const bool redone = rig.history.redo() && sameSurfaceBodyState(rig.body()->state(), thick)
                        && surfaceMeshDigest(rig.body()->mesh()) == thickDigest;
    SurfaceBodyState lower = thick;
    lower.nextFeatureId = 1;
    r.check("SURF_21_undo_and_redo_restore_the_exact_feature_list",
            applied && undone && redone && rig.history.undoDepth() == 1u
                    && rig.body()->applyState(lower) == SurfaceStatus::HighWaterInvalid);

    // SURF-22: save and reopen.
    const SurfaceBodyState stitched = corpusLoftTrimStitch();
    SurfaceBodyMesh stitchedMesh;
    const bool corpusOk = regen(stitched, &stitchedMesh) == SurfaceStatus::Ok;
    Rig saved(stitched);
    const std::vector<uint8_t> bytes = saved.bytes();
    ProjectDocument decoded;
    const bool decodedOk = decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok;
    ConstructionScene reopened{NoProjectTag{}};
    ConstructionHistory history(reopened);
    SculptSession sculpt;
    const bool loaded = decodedOk && loadProjectDocument(decoded, reopened, sculpt, history) == ProjectCodecStatus::Ok;
    const SceneObject* back = loaded ? reopened.findBody(saved.id) : nullptr;
    r.check("SURF_22_save_and_reopen_keep_the_exact_feature_history",
            corpusOk && stitchedMesh.stitches.size() == 1u && back != nullptr && back->surfaceOrNull() != nullptr
                    && sameSurfaceBodyState(back->surfaceOrNull()->state(), stitched)
                    && surfaceMeshDigest(back->surfaceOrNull()->mesh()) == surfaceMeshDigest(stitchedMesh)
                    && encodeProjectV1(captureProjectDocument(reopened, ProjectKind::Construction)) == bytes
                    && projectSemanticFingerprint(reopened, ProjectKind::Construction)
                               == projectSemanticFingerprint(saved.scene, ProjectKind::Construction)
                    && bytes.size() > 16u && (bytes[15] & kHeaderFlagHasSurface) != 0u
                    && sectionOffset(bytes, kSectionTagSurface) != 0u);
    // Bounds: SURF payload offsets -- bodyCount 0, objectId 4, marks 12/16,
    // sketchCount 20, featureCount 24.
    const std::vector<uint8_t> simple = encodeProjectV1(documentFor(patchState()));
    ConstructionScene legacy((NoProjectTag()));
    publishSceneObject(legacy.addBody());
    const std::vector<uint8_t> legacyBytes = encodeProjectV1(captureProjectDocument(legacy, ProjectKind::Construction));
    r.check("SURF_22_counts_are_bounded_and_a_project_without_surface_writes_no_section",
            decodeStatus(simple) == ProjectCodecStatus::Ok
                    && decodeStatus(patched(simple, 0, 0, 4)) == ProjectCodecStatus::ImpossibleCount
                    && decodeStatus(patched(simple, 24, 0, 4)) == ProjectCodecStatus::ImpossibleCount
                    && decodeStatus(patched(simple, 24, 33, 4)) == ProjectCodecStatus::ImpossibleCount
                    && decodeStatus(patched(simple, 20, 33, 4)) == ProjectCodecStatus::ImpossibleCount
                    && decodeStatus(patched(simple, 4, 77, 4)) == ProjectCodecStatus::UnresolvedReference
                    && (legacyBytes[15] & kHeaderFlagHasSurface) == 0u
                    && sectionOffset(legacyBytes, kSectionTagSurface) == 0u);

    // SURF-23: determinism.
    uint64_t digests[3] = {0, 0, 0};
    for (uint64_t& d : digests) {
        SurfaceBodyMesh m;
        regen(stitched, &m);
        d = surfaceMeshDigest(m);
    }
    r.check("SURF_23_tessellation_is_deterministic", digests[0] == digests[1] && digests[1] == digests[2]);

    // SURF-24: open until something closes it.
    SurfaceBodyState lone = capAndTube(2.0);
    lone.features.erase(lone.features.begin());
    lone.features[0].id = SurfaceFeatureId{2};
    SurfaceBodyMesh loneMesh;
    const bool loneOk = regen(lone, &loneMesh) == SurfaceStatus::Ok;
    r.check("SURF_24_a_surface_stays_open_where_no_cap_or_thicken_closes_it",
            loneOk && loneMesh.open && loneMesh.openEdgeCount == 2u && loneMesh.render.renderBothSides
                    && loneMesh.solid.triangleCount() == 0u && stitchedMesh.open && stitchedMesh.openEdgeCount > 0u);

    // Surface is never a sculpt source, and a SCUL entry over one is refused.
    ConstructionMesh source;
    ProjectDocument document = captureProjectDocument(rig.scene, ProjectKind::Construction);
    document.hasSculpt = true;
    ProjectSculptBody sculpted;
    sculpted.objectId = rig.id;
    sculpted.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    sculpted.indices = {0, 1, 2};
    document.sculpt.bodies.push_back(sculpted);
    const SceneObject* object = rig.scene.findBody(rig.id);
    r.check("SURF_24_surface_is_its_own_representation_and_never_a_sculpt_source",
            object != nullptr && object->isSurface() && !buildSculptSourceMesh(*object, &source)
                    && std::strcmp(bodyRepresentationName(object->representation()), "Surface") == 0
                    && validateProjectDocument(document) == ProjectCodecStatus::UnresolvedReference);

    // The corpus.
    ProjectDocument bad = documentFor(corpusPatchExtrude());
    if (!bad.surface.bodies.empty()) {
        SurfaceBodyState& state = bad.surface.bodies[0].state;
        CadSketch cutter = sketchOf();
        add(&cutter, circle(0.0, 0.0, 0.5));
        const uint32_t cid = appendSurfaceSketch(&state, cutter, 0.0);
        appendSurfaceFeature(&state, trimFeature(cid, cutter, SurfaceFeatureId{7}, false));
    }
    const std::vector<uint8_t> a = encodeProjectV1(documentFor(corpusPatchExtrude()));
    const std::vector<uint8_t> b = encodeProjectV1(documentFor(corpusLoftTrimStitch()));
    const std::vector<uint8_t> c = encodeProjectV1Unchecked(bad);
    r.check("SURF_22_the_corpus_documents_decode_to_their_verdicts",
            decodeStatus(a) == ProjectCodecStatus::Ok && decodeStatus(b) == ProjectCodecStatus::Ok
                    && decodeStatus(c) == ProjectCodecStatus::InvalidSemanticValue);
    // scripts/build-forge-corpus.ps1 writes the same three documents from the
    // layout in DATA_PACKAGE_SPEC.md 7k alone; the two implementations must
    // agree byte for byte.
    r.check("SURF_22_the_surf_corpus_matches_the_independent_builder",
            projectFixtureSha256Hex(a) == "d4cb49a6500718e1e1482a788aa897cf28d9922421ac9e1b73bffb89c3d55e14"
                    && projectFixtureSha256Hex(b)
                               == "006374495e44da8332cae0fc07533ebface1f59fde8e12eb5ff9981c33736c8e"
                    && projectFixtureSha256Hex(c)
                               == "4a0367a4a05f432633a81cd54313db07ab4a88da87d447d85dec8b93df3f640d");
    g_digests = "surface_patch_extrude_v1=" + projectFixtureSha256Hex(a) + " surface_loft_trim_stitch_v1="
                + projectFixtureSha256Hex(b) + " surface_bad_ref_v1=" + projectFixtureSha256Hex(c);
}

// ---------------------------------------------------------------------------
// Authoring: a sketch session's Finish, Stitch, Thicken and value edits
// ---------------------------------------------------------------------------

struct Drawing {
    SketchSession sketch;
    CameraController camera;
    enum : int { kW = 1000, kH = 1000 };
    // The purpose is process-scoped; a suite gives back whatever it found.
    SurfaceSketchPurpose saved = surfaceSketchPurpose();
    bool begin(ObjectId body, double offset = 0.0) {
        surfaceSketchPurpose() = SurfaceSketchPurpose{true, body};
        if (sketch.begin(Workplane::XY) != CadStatus::Ok) return false;
        if (offset != 0.0 && sketch.setPlaneOffset(offset) != CadStatus::Ok) return false;
        camera.setViewport(kW, kH);
        const SketchFrame& f = sketch.frame();
        camera.frameSketchView(f.origin, f.u, f.v, f.n);
        return true;
    }
    bool at(TouchAction action, float x, float y) {
        TouchPointer p{7, x, y};
        return sketch.onTouch(action, action == TouchAction::Up ? 7 : -1, &p, 1, camera.snapshot(), kW, kH);
    }
    bool drag(SketchTool tool, const SketchPoint& from, const SketchPoint& to) {
        sketch.setTool(tool);
        float x0, y0, x1, y1;
        if (!sketch.sketchToScreen(camera.snapshot(), from, kW, kH, &x0, &y0)
            || !sketch.sketchToScreen(camera.snapshot(), to, kW, kH, &x1, &y1)) {
            return false;
        }
        bool ok = at(TouchAction::Down, x0, y0);
        for (int step = 1; step <= 4; ++step) {
            const float t = step / 4.0f;
            ok &= at(TouchAction::Move, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
        }
        ok &= at(TouchAction::Up, x1, y1);
        return ok;
    }
    // Draws a circle by dragging centre to rim.
    bool circleAt(double u, double v, double radius) {
        return drag(SketchTool::Circle, SketchPoint{u, v}, SketchPoint{u + radius, v});
    }
    ~Drawing() { surfaceSketchPurpose() = saved; }
};

SurfaceCreateRequest requestOf(SurfaceCreateKind kind) {
    SurfaceCreateRequest r;
    r.kind = kind;
    return r;
}

void testAuthoring(Checks& r) {
    ConstructionScene scene{NoProjectTag{}};
    ConstructionHistory history(scene);
    SculptSession sculpt;
    ObjectId body = kNoObject;

    // 1. New Project -> Surface: an open chain extruded is the first project.
    {
        Drawing d;
        const bool drawn = d.begin(kNoObject) && d.drag(SketchTool::Line, {2.0, 0.0}, {3.0, 0.0})
                           && d.drag(SketchTool::Line, {3.0, 0.0}, {3.0, 1.0})
                           && d.sketch.sketch().entities.size() == 2u;
        SurfaceCreateRequest extrude = requestOf(SurfaceCreateKind::Extrude);
        extrude.distance = 0.5;
        const bool loft = surfaceCommitSketch(d.sketch, scene, sculpt, history, requestOf(SurfaceCreateKind::Loft))
                          == SurfaceStatus::FirstFeatureInvalid;
        const SurfaceStatus why = surfaceCommitSketch(d.sketch, scene, sculpt, history, extrude, &body);
        const SceneObject* object = scene.findBody(body);
        r.check("SURF_AUTH_01_an_open_chain_sketch_becomes_the_first_surface_project",
                drawn && loft && why == SurfaceStatus::Ok && object != nullptr && object->isSurface()
                        && scene.bodyCount() == 1u && history.undoDepth() == 0u && !d.sketch.active()
                        && !surfaceSketchPurpose().active
                        && object->surfaceOrNull()->state().features[0].kind == SurfaceFeatureKind::ExtrudedSurface
                        && object->surfaceOrNull()->state().features[0].section.curves.size() == 2u);
    }
    SurfaceBody* surface = scene.findBody(body) != nullptr ? scene.findBody(body)->surfaceOrNull() : nullptr;
    if (surface == nullptr) return;

    // 2. Thicken the wall: one Undo.
    const SurfaceStatus thick = surfaceThicken(scene, history, body, SurfaceFeatureId{1}, 0.1);
    r.check("SURF_AUTH_02_thicken_is_one_transaction",
            thick == SurfaceStatus::Ok && history.undoDepth() == 1u && surface->mesh().solid.triangleCount() > 0u
                    && surfaceThicken(scene, history, body, SurfaceFeatureId{1}, 0.1) == SurfaceStatus::FeatureConsumed
                    && history.undoDepth() == 1u);

    // 3. Patch, Trim on its plane, a tube, then Stitch.
    bool built = true;
    {
        Drawing d;
        built = built && d.begin(body) && d.drag(SketchTool::Rectangle, {-1.0, -1.0}, {1.0, 1.0})
                && surfaceCommitSketch(d.sketch, scene, sculpt, history, requestOf(SurfaceCreateKind::Patch))
                           == SurfaceStatus::Ok;
    }
    bool noTarget = false;
    {
        Drawing d;
        built = built && d.begin(body, 0.5) && d.circleAt(0.0, 0.0, 0.5);
        noTarget = surfaceCommitSketch(d.sketch, scene, sculpt, history, requestOf(SurfaceCreateKind::Trim))
                   == SurfaceStatus::TrimTargetMissing;
    }
    {
        Drawing d;
        built = built && d.begin(body) && d.circleAt(0.0, 0.0, 0.5)
                && surfaceCommitSketch(d.sketch, scene, sculpt, history, requestOf(SurfaceCreateKind::Trim))
                           == SurfaceStatus::Ok;
    }
    {
        Drawing d;
        SurfaceCreateRequest tube = requestOf(SurfaceCreateKind::Extrude);
        tube.distance = 1.0;
        built = built && d.begin(body) && d.drag(SketchTool::Rectangle, {-1.0, -1.0}, {1.0, 1.0})
                && surfaceCommitSketch(d.sketch, scene, sculpt, history, tube) == SurfaceStatus::Ok;
    }
    const std::vector<SurfaceFeatureId> live = surfaceLiveFeatures(surface->mesh());
    const SurfaceStatus stitched = surfaceStitch(scene, history, body);
    r.check("SURF_AUTH_03_patch_trim_tube_and_stitch_through_the_sketch_session",
            built && noTarget && live.size() == 2u && idOf(live[0]) == 4u && idOf(live[1]) == 5u
                    && stitched == SurfaceStatus::Ok && surface->mesh().stitches.size() == 1u
                    && history.undoDepth() == 5u);

    // 4. Revolve needs exactly one Construction straight edge.
    {
        Drawing d;
        bool ok = d.begin(body) && d.drag(SketchTool::Line, {6.0, 0.0}, {6.0, 1.0})
                  && d.drag(SketchTool::Line, {5.0, -1.0}, {5.0, 2.0});
        const SurfaceStatus none =
            surfaceCommitSketch(d.sketch, scene, sculpt, history, requestOf(SurfaceCreateKind::Revolve));
        const SketchEntityId axisId = d.sketch.sketch().entities.size() == 2u ? d.sketch.sketch().entities[1].id() : 0u;
        ok = ok && d.sketch.select(axisId) && d.sketch.toggleSelectionConstruction() == CadStatus::Ok;
        d.sketch.clearSelection();
        const SurfaceStatus revolved =
            surfaceCommitSketch(d.sketch, scene, sculpt, history, requestOf(SurfaceCreateKind::Revolve));
        const SurfaceFeature* f = findSurfaceFeature(surface->state(), SurfaceFeatureId{7});
        r.check("SURF_AUTH_04_revolve_sweeps_about_the_one_construction_edge",
                ok && none == SurfaceStatus::AxisUnresolved && revolved == SurfaceStatus::Ok && f != nullptr
                        && f->kind == SurfaceFeatureKind::RevolvedSurface && f->axis.entityId == axisId
                        && f->section.curves.size() == 1u && f->angleDegrees == 360.0);
    }

    // 5. A Section, then a Loft drawn where it stands.
    {
        Drawing a;
        bool ok = a.begin(body) && a.circleAt(0.0, 4.0, 0.5);
        const bool missing = surfaceCommitSketch(a.sketch, scene, sculpt, history, requestOf(SurfaceCreateKind::Loft))
                             == SurfaceStatus::SectionMissing;
        ok = ok && surfaceCommitSketch(a.sketch, scene, sculpt, history, requestOf(SurfaceCreateKind::Section))
                           == SurfaceStatus::Ok;
        const uint32_t pending = surfacePendingSection(surface->state());
        Drawing b;
        ok = ok && b.begin(body, 1.0) && b.sketch.frame().origin.z == 1.0f && b.circleAt(0.0, 4.0, 0.3)
             && surfaceCommitSketch(b.sketch, scene, sculpt, history, requestOf(SurfaceCreateKind::Loft))
                        == SurfaceStatus::Ok;
        const SurfaceFeature& loft = surface->state().features.back();
        const SurfaceSketchRecord* second = findSurfaceSketch(surface->state(), loft.sectionB.sketchId);
        r.check("SURF_AUTH_05_a_section_then_a_loft_at_its_typed_offset",
                ok && missing && pending != 0u && loft.kind == SurfaceFeatureKind::LoftSurface
                        && loft.section.sketchId == pending && second != nullptr && second->offset == 1.0
                        && surfacePendingSection(surface->state()) == 0u);
    }

    // 6. Value edits: one Undo, and a staged failure names its feature and
    // writes nothing.
    const size_t depth = history.undoDepth();
    const uint64_t before = surfaceMeshDigest(surface->mesh());
    const double volumeBefore = cadSolidVolume(surface->mesh().solid);
    const SurfaceStatus edited = surfaceApplyValue(scene, history, body, SurfaceValueTarget::Feature, 1u, 0.8);
    const double volumeAfter = cadSolidVolume(surface->mesh().solid);
    SurfaceRegenerationReport report;
    const SurfaceBodyState committed = surface->state();
    // Moving the patch's sketch off the plane its Trim stands on.
    const uint32_t patchSketch = findSurfaceFeature(committed, SurfaceFeatureId{3})->section.sketchId;
    const SurfaceStatus moved =
        surfaceApplyValue(scene, history, body, SurfaceValueTarget::Sketch, patchSketch, 0.25, &report);
    SurfaceBodyState staged;
    surfaceStateWithValue(committed, SurfaceValueTarget::Sketch, patchSketch, 0.25, &staged);
    SurfaceRegenerationReport stagedReport;
    regenerateSurfaceBody(staged, nullptr, &stagedReport);
    const SurfaceTimeline timeline =
        buildSurfaceTimeline(staged, stagedReport, true, SurfaceValueTarget::Sketch, patchSketch);
    size_t failedRows = 0;
    size_t notRebuilt = 0;
    bool editingMarked = false;
    for (const SurfaceTimelineRow& row : timeline.rows) {
        failedRows += row.state == SurfaceTimelineRowState::Failed ? 1u : 0u;
        notRebuilt += row.state == SurfaceTimelineRowState::NotRegenerated ? 1u : 0u;
        editingMarked = editingMarked || (row.editing && !row.feature && row.id == patchSketch);
    }
    r.check("SURF_AUTH_06_a_value_edit_regenerates_downstream_as_one_undo",
            edited == SurfaceStatus::Ok && history.undoDepth() == depth + 1u
                    && near(volumeAfter, volumeBefore * 0.8 / 0.5, 1e-9) && surfaceMeshDigest(surface->mesh()) != before);
    r.check("SURF_AUTH_07_a_staged_edit_names_its_first_failure_and_writes_nothing",
            moved == SurfaceStatus::TrimNotCoplanar && idOf(report.failedFeature) == 4u
                    && sameSurfaceBodyState(surface->state(), committed) && history.undoDepth() == depth + 1u
                    && failedRows == 1u && notRebuilt == surface->state().features.size() - 4u && editingMarked
                    && timeline.failedFeature == SurfaceFeatureId{4});
    const bool undone = history.undo() && near(cadSolidVolume(surface->mesh().solid), volumeBefore, 1e-12);
    const SurfaceTimeline rows = buildSurfaceTimeline(surface->state(), SurfaceRegenerationReport{});
    bool ordered = rows.rows.size() == surface->state().sketches.size() + surface->state().features.size();
    // A sketch row stands before the first feature that reads it.
    for (size_t i = 0; ordered && i < rows.rows.size(); ++i) {
        if (!rows.rows[i].feature) continue;
        bool seen = rows.rows[i].kind == SurfaceFeatureKind::Stitch || rows.rows[i].kind == SurfaceFeatureKind::Thicken;
        for (size_t j = 0; j < i; ++j) seen = seen || (!rows.rows[j].feature && rows.rows[j].id == rows.rows[i].sketchId);
        ordered = seen;
    }
    r.check("SURF_AUTH_08_undo_restores_and_the_timeline_reads_in_construction_order", undone && ordered);
}

double micros(const std::function<void()>& work) {
    const auto t0 = std::chrono::steady_clock::now();
    work();
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
}

void measure(Checks& r) {
    SurfaceBodyState holed;
    CadSketch sk = sketchOf();
    add(&sk, rect(0.0, 0.0, 4.0, 4.0));
    add(&sk, circle(0.0, 0.0, 1.0));
    const uint32_t id = appendSurfaceSketch(&holed, sk, 0.0);
    appendSurfaceFeature(&holed, patchFeature(id, sk));
    SurfaceBodyState loft = loftState(circle(0, 0, 1.0), rect(0, 0, 2.0, 2.0));
    SurfaceBodyState stitched = corpusLoftTrimStitch();
    SurfaceBodyState thick = chainState(4.0);
    bool ok = true;
    char line[256];
    std::snprintf(line, sizeof(line), "patch_us=%.0f loft_us=%.0f trim_stitch_loft_us=%.0f trim_thicken_us=%.0f",
                  micros([&] { ok = ok && regen(holed, nullptr) == SurfaceStatus::Ok; }),
                  micros([&] { ok = ok && regen(loft, nullptr) == SurfaceStatus::Ok; }),
                  micros([&] { ok = ok && regen(stitched, nullptr) == SurfaceStatus::Ok; }),
                  micros([&] { ok = ok && regen(thick, nullptr) == SurfaceStatus::Ok; }));
    g_performance = line;
    r.check("SURF_PERF_patch_loft_stitch_and_thicken_are_measured", ok);
}

}  // namespace

int runSurfaceSelfTests(SurfaceSelfTestResult* out, int maxOut) {
    Checks r{out, maxOut};
    testPatches(r);
    testSweeps(r);
    testLoft(r);
    testTrim(r);
    testStitch(r);
    testThicken(r);
    testHistoryAndFormat(r);
    testAuthoring(r);
    // NativeViewport states these numbers; a status is APPENDED, never inserted.
    r.check("SURF_CODES_the_status_codes_java_names_are_stable",
            surfaceStatusCode(SurfaceStatus::AxisUnresolved) == 20
                    && surfaceStatusCode(SurfaceStatus::TrimNotCoplanar) == 29
                    && surfaceStatusCode(SurfaceStatus::ThickenUnsupportedForSurfaceType) == 36
                    && surfaceStatusCode(SurfaceStatus::NothingToStitch) == 47
                    && static_cast<int>(SurfaceCreateKind::Section) == 6);
    measure(r);
    return std::min(r.count, maxOut);
}

const char* surfacePerformanceReport() { return g_performance.c_str(); }

const char* surfaceFixtureDigests() { return g_digests.c_str(); }

}  // namespace forgeshape
