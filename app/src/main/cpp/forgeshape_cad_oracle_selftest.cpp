#include "forgeshape_cad_oracle_selftest.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_revolve.h"
#include "forgeshape_history.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_transform.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// Measurements, written here from the mesh alone
// ---------------------------------------------------------------------------

struct P3 {
    double x, y, z;
};

struct Measure {
    bool finite = true;
    // Every undirected edge on exactly two triangles, walked once each way:
    // closed and consistently oriented.
    bool closedOriented = true;
    double volume = 0.0;  // signed, divergence theorem about the origin
    double area = 0.0;
    P3 lo{1e300, 1e300, 1e300};
    P3 hi{-1e300, -1e300, -1e300};
    uint32_t components = 0;  // triangle connectivity through shared vertices
    uint32_t triangles = 0;
    // The same two facts by INDEX, without welding: each shell the product
    // emits is closed on its own vertex indices even where two shells meet in
    // space (point-touching cells share a vertical edge there).
    bool indexClosed = true;
    uint32_t indexComponents = 0;
    // Undirected welded edges on more than two triangles: where shells touch.
    uint32_t weldedNonManifoldEdges = 0;
};

P3 at(const std::vector<P3>& points, uint32_t i) { return points[i]; }

Measure measurePoints(const std::vector<P3>& points, const std::vector<uint32_t>& indices) {
    Measure m;
    m.triangles = static_cast<uint32_t>(indices.size() / 3u);
    for (const P3& p : points) {
        m.finite = m.finite && std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }
    // Vertices are welded by exact position, so a mesh that duplicates a
    // vertex for shading is still judged as one surface.
    std::map<std::tuple<double, double, double>, uint32_t> weld;
    std::vector<uint32_t> canon(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        const auto key = std::make_tuple(points[i].x, points[i].y, points[i].z);
        const auto found = weld.find(key);
        if (found == weld.end()) {
            const uint32_t id = static_cast<uint32_t>(weld.size());
            weld.emplace(key, id);
            canon[i] = id;
        } else {
            canon[i] = found->second;
        }
    }
    std::map<std::pair<uint32_t, uint32_t>, int> directed;
    std::vector<uint32_t> parent(weld.size());
    std::iota(parent.begin(), parent.end(), 0u);
    const auto find = [&parent](uint32_t a) {
        while (parent[a] != a) {
            parent[a] = parent[parent[a]];
            a = parent[a];
        }
        return a;
    };
    for (size_t t = 0; t + 2 < indices.size(); t += 3) {
        const P3 a = at(points, indices[t]);
        const P3 b = at(points, indices[t + 1]);
        const P3 c = at(points, indices[t + 2]);
        m.volume += (a.x * (b.y * c.z - b.z * c.y) - a.y * (b.x * c.z - b.z * c.x)
                     + a.z * (b.x * c.y - b.y * c.x)) / 6.0;
        const P3 u{b.x - a.x, b.y - a.y, b.z - a.z};
        const P3 v{c.x - a.x, c.y - a.y, c.z - a.z};
        const P3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
        m.area += 0.5 * std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        const uint32_t ia = canon[indices[t]];
        const uint32_t ib = canon[indices[t + 1]];
        const uint32_t ic = canon[indices[t + 2]];
        directed[{ia, ib}] += 1;
        directed[{ib, ic}] += 1;
        directed[{ic, ia}] += 1;
        parent[find(ia)] = find(ib);
        parent[find(ib)] = find(ic);
    }
    for (const auto& edge : directed) {
        const auto back = directed.find({edge.first.second, edge.first.first});
        m.closedOriented = m.closedOriented && edge.second == 1 && back != directed.end()
                           && back->second == 1;
        if (edge.first.first < edge.first.second) {
            const int uses = edge.second + (back != directed.end() ? back->second : 0);
            if (uses > 2) ++m.weldedNonManifoldEdges;
        }
    }
    {
        std::map<std::pair<uint32_t, uint32_t>, int> byIndex;
        std::vector<uint32_t> up(points.size());
        std::iota(up.begin(), up.end(), 0u);
        const auto root = [&up](uint32_t a) {
            while (up[a] != a) {
                up[a] = up[up[a]];
                a = up[a];
            }
            return a;
        };
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            const uint32_t a = indices[t], b = indices[t + 1], c = indices[t + 2];
            byIndex[{a, b}] += 1;
            byIndex[{b, c}] += 1;
            byIndex[{c, a}] += 1;
            up[root(a)] = root(b);
            up[root(b)] = root(c);
        }
        for (const auto& edge : byIndex) {
            const auto back = byIndex.find({edge.first.second, edge.first.first});
            m.indexClosed = m.indexClosed && edge.second == 1 && back != byIndex.end()
                            && back->second == 1;
        }
        std::vector<bool> usedIndex(points.size(), false);
        for (uint32_t i : indices) usedIndex[i] = true;
        std::vector<bool> seen(points.size(), false);
        for (uint32_t i = 0; i < points.size(); ++i) {
            if (usedIndex[i] && !seen[root(i)]) {
                seen[root(i)] = true;
                ++m.indexComponents;
            }
        }
    }
    for (const P3& p : points) {
        m.lo = {std::min(m.lo.x, p.x), std::min(m.lo.y, p.y), std::min(m.lo.z, p.z)};
        m.hi = {std::max(m.hi.x, p.x), std::max(m.hi.y, p.y), std::max(m.hi.z, p.z)};
    }
    std::vector<bool> used(weld.size(), false);
    for (uint32_t i : indices) used[canon[i]] = true;
    std::vector<bool> root(weld.size(), false);
    for (uint32_t i = 0; i < weld.size(); ++i) {
        if (used[i] && !root[find(i)]) {
            root[find(i)] = true;
            ++m.components;
        }
    }
    return m;
}

std::vector<P3> pointsOf(const ConstructionMesh& mesh) {
    std::vector<P3> out;
    out.reserve(mesh.vertices.size());
    for (const MeshVertex& v : mesh.vertices) {
        out.push_back({v.position[0], v.position[1], v.position[2]});
    }
    return out;
}

bool measureState(const CadBodyState& state, Measure* out) {
    ConstructionMesh mesh;
    if (generateCadMesh(state, &mesh) != CadStatus::Ok) return false;
    *out = measurePoints(pointsOf(mesh), mesh.indices);
    return true;
}

// ---------------------------------------------------------------------------
// Inputs (authored truth only)
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

ExtrudeFeature extrude(SketchEntityId profile, double depth,
                       ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature e;
    e.profileEntityId = profile;
    e.depth = depth;
    e.direction = direction;
    return e;
}

CadFeatureSupport support(const CadBodyState& state, uint32_t featureId, CadFaceToken token) {
    CadFeatureSupport s;
    s.featureId = featureId;
    s.face = token;
    s.lineageToken = cadFeatureTopologySignature(state, featureId);
    return s;
}

CadFaceToken capFar() {
    CadFaceToken t;
    t.kind = CadFaceKind::CapFar;
    return t;
}

CadBodyState block(double w, double h, double d) {
    CadSketch s;
    const SketchEntityId r = add(&s, rect(0.0, 0.0, w, h));
    return makeCadBodyState(s, extrude(r, d));
}

// ---------------------------------------------------------------------------
// The tolerance contracts
// ---------------------------------------------------------------------------

// Exact polygonal solids: the published positions are float32, so a value
// built from O(1) coordinates of magnitude <= 10 carries a relative error of a
// few ulp (1.2e-7 each); 1e-5 relative bounds it with margin and is still
// thousands of times tighter than any feature-sized mistake.
bool exactly(double measured, double expected) {
    return std::fabs(measured - expected) <= 1e-5 * std::max(1.0, std::fabs(expected));
}

// A chord polygon of N segments over a sweep inscribes the true curve, so it
// never exceeds the analytic value and falls short by at most the deficit
// 1 - sin(step)/step of it (the step is the angle per chord).
bool chordally(double measured, double analytic, double stepRadians) {
    const double deficit = 1.0 - std::sin(stepRadians) / stepRadians;
    // The polygon's ratio to the curve IS sin(step)/step, so a correct mesh sits
    // on the bound; float32 positions get the exact contract's 1e-5 of slack.
    return measured <= analytic * (1.0 + 1e-5)
           && measured >= analytic * (1.0 - deficit) * (1.0 - 1e-5);
}

struct Oracle {
    std::vector<ArrangementSelfTestCheck>* out;
    int passed = 0;
    const Measure* last = nullptr;
    void check(const char* id, bool ok, const char* repro, double expected, double measured) {
        out->push_back(ArrangementSelfTestCheck{id, ok});
        if (ok) {
            ++passed;
        } else if (last != nullptr) {
            std::printf("CADR2_ORACLE_DETAIL %s closed=%d components=%u area=%.9g finite=%d\n", id,
                        last->closedOriented ? 1 : 0, last->components, last->area,
                        last->finite ? 1 : 0);
        }
        if (!ok) {
            std::printf("CADR2_ORACLE_FAIL %s expected=%.9g measured=%.9g repro=%s\n", id, expected,
                        measured, repro);
        }
    }
};

bool box(const Measure& m, double x0, double y0, double z0, double x1, double y1, double z1) {
    return exactly(m.lo.x, x0) && exactly(m.lo.y, y0) && exactly(m.lo.z, z0)
           && exactly(m.hi.x, x1) && exactly(m.hi.y, y1) && exactly(m.hi.z, z1);
}

void runOracles(Oracle& o) {
    Measure m;
    o.last = &m;

    // R2-OR-01: a w x h x d box: V = whd, A = 2(wh + wd + hd), bounds, closed.
    {
        const double w = 2.0, h = 1.5, d = 0.7;
        const bool ok = measureState(block(w, h, d), &m);
        o.check("R2_OR_01_rectangle_extrude_volume_area_bounds_closed",
                ok && m.finite && m.closedOriented && m.components == 1u
                        && exactly(m.volume, w * h * d) && exactly(m.area, 2 * (w * h + w * d + h * d))
                        && box(m, -w / 2, -h / 2, 0.0, w / 2, h / 2, d),
                "block(2,1.5) depth 0.7 on XY", w * h * d, m.volume);
    }
    // R2-OR-02: a rectangular hole: V = (WH - wh)d, A = 2(WH - wh) + 2(W+H)d + 2(w+h)d.
    {
        CadSketch s;
        const SketchEntityId outer = add(&s, rect(0.0, 0.0, 3.0, 2.0));
        const SketchEntityId inner = add(&s, rect(0.5, 0.0, 1.0, 0.8));
        ExtrudeFeature e = extrude(outer, 0.5);
        e.profileHoleIds = {inner};
        const bool ok = measureState(makeCadBodyState(s, e), &m);
        const double v = (6.0 - 0.8) * 0.5;
        const double a = 2 * (6.0 - 0.8) + 2 * (3.0 + 2.0) * 0.5 + 2 * (1.0 + 0.8) * 0.5;
        o.check("R2_OR_02_rectangular_hole_volume_area_genus",
                ok && m.closedOriented && m.components == 1u && exactly(m.volume, v)
                        && exactly(m.area, a),
                "rect 3x2 with rect hole 1x0.8 at (0.5,0), depth 0.5", v, m.volume);
    }
    // R2-OR-03: two independent regions: two shells, volumes add.
    {
        CadSketch s;
        const SketchEntityId a = add(&s, rect(-2.0, 0.0, 1.0, 1.0));
        const SketchEntityId b = add(&s, rect(2.0, 0.0, 2.0, 0.5));
        ExtrudeFeature e = extrude(a, 1.0);
        ProfileRegionRef second;
        second.outerAnchorId = b;
        e.additionalRegions = {second};
        const bool ok = measureState(makeCadBodyState(s, e), &m);
        o.check("R2_OR_03_two_independent_components",
                ok && m.closedOriented && m.components == 2u && exactly(m.volume, 1.0 + 1.0),
                "rects 1x1 at (-2,0) and 2x0.5 at (2,0), depth 1", 2.0, m.volume);
    }
    // R2-OR-04: Add a boss: V = V1 + V2, A = A1 + A2 - 2 * footprint.
    CadBodyState shop = block(2.0, 2.0, 1.0);
    {
        CadSketch boss;
        const SketchEntityId r = add(&boss, rect(-0.5, 0.5, 0.6, 0.6));
        appendCadLaterFeatureWithSketch(&shop, CadFeatureOperation::Add,
                                        support(shop, kCadFeatureId, capFar()), std::move(boss),
                                        extrude(r, 0.5));
        const bool ok = measureState(shop, &m);
        const double v = 4.0 + 0.18;
        const double a = (8.0 + 8.0) + (0.72 + 4 * 0.6 * 0.5) - 2 * 0.36;
        o.check("R2_OR_04_add_box_on_box_volume_area_bounds",
                ok && m.closedOriented && m.components == 1u && exactly(m.volume, v)
                        && exactly(m.area, a) && box(m, -1, -1, 0, 1, 1, 1.5),
                "block 2x2x1 + boss 0.6 at (-0.5,0.5) height 0.5", v, m.volume);
    }
    // R2-OR-05: Cut a pocket: V = V - pocket, A = A + perimeter * depth.
    {
        CadSketch pocket;
        const SketchEntityId r = add(&pocket, rect(0.5, -0.5, 0.6, 0.6));
        appendCadLaterFeatureWithSketch(&shop, CadFeatureOperation::Cut,
                                        support(shop, kCadFeatureId, capFar()), std::move(pocket),
                                        extrude(r, 0.4, ExtrudeDirection::AgainstNormal));
        const double before = m.area;
        const bool ok = measureState(shop, &m);
        const double v = 4.0 + 0.18 - 0.144;
        o.check("R2_OR_05_cut_pocket_volume_and_wall_area",
                ok && m.closedOriented && m.components == 1u && exactly(m.volume, v)
                        && exactly(m.area, before + 4 * 0.6 * 0.4),
                "OR-04 + pocket 0.6 at (0.5,-0.5) depth 0.4", v, m.volume);
    }
    // R2-OR-06: Symmetric: V = wh * 2d, bounds -d..d.
    {
        CadBodyState s = block(1.0, 1.0, 0.3);
        s.extrude.extent = ExtrudeExtentMode::Symmetric;
        const bool ok = measureState(s, &m);
        o.check("R2_OR_06_symmetric_extent_volume_and_bounds",
                ok && m.closedOriented && exactly(m.volume, 0.6) && box(m, -0.5, -0.5, -0.3, 0.5, 0.5, 0.3),
                "block 1x1 symmetric 0.3 per side", 0.6, m.volume);
    }
    // R2-OR-07: Two Sides: V = wh(a + b), bounds -b..a.
    {
        CadBodyState s = block(1.0, 2.0, 0.5);
        s.extrude.extent = ExtrudeExtentMode::TwoSides;
        s.extrude.secondDistance = 0.25;
        const bool ok = measureState(s, &m);
        o.check("R2_OR_07_two_sides_extent_volume_and_bounds",
                ok && m.closedOriented && exactly(m.volume, 1.5) && box(m, -0.5, -1, -0.25, 0.5, 1, 0.5),
                "block 1x2 two sides A 0.5 B 0.25", 1.5, m.volume);
    }
    // R2-OR-08: reversed One Side: same volume, mirrored bounds, still outward.
    {
        CadBodyState s = block(1.0, 1.0, 0.4);
        s.extrude.direction = ExtrudeDirection::AgainstNormal;
        const bool ok = measureState(s, &m);
        o.check("R2_OR_08_reversed_direction_is_outward_with_mirrored_bounds",
                ok && m.closedOriented && m.volume > 0.0 && exactly(m.volume, 0.4)
                        && box(m, -0.5, -0.5, -0.4, 0.5, 0.5, 0.0),
                "block 1x1 depth 0.4 AgainstNormal", 0.4, m.volume);
    }
    // R2-OR-09: a full Revolve of a rectangle u in [1, 2], v in [0, 1] about
    // u = 0: polygonal (N/2) sin(2pi/N)(r2^2 - r1^2)h EXACTLY, and the
    // analytic pi(r2^2 - r1^2)h within the chordal deficit.
    {
        CadSketch s;
        const SketchEntityId square = add(&s, rect(1.5, 0.5, 1.0, 1.0));
        SketchLine axisLine;
        axisLine.start = SketchPoint{0.0, -1.0};
        axisLine.end = SketchPoint{0.0, 2.0};
        add(&s, axisLine);
        RevolveFeature r;
        r.profileEntityId = square;
        r.axis = CadSketchEdgeRef{2, 0};
        r.angleDegrees = 360.0;
        const bool ok = measureState(makeCadRevolveBodyState(s, r), &m);
        const uint32_t n = revolveSegmentCount(360.0);
        const double step = 2.0 * kPi / n;
        const double polygonal = 0.5 * n * std::sin(step) * 3.0;
        const double analytic = kPi * 3.0;
        o.check("R2_OR_09_full_revolve_polygonal_exact_and_analytic_chordal",
                ok && m.closedOriented && m.components == 1u && exactly(m.volume, polygonal)
                        && chordally(m.volume, analytic, step),
                "rect u[1,2] v[0,1] about u=0, 360 deg", analytic, m.volume);
    }
    // R2-OR-10: a 90 degree Revolve of the same rectangle: exact wedge sum and
    // theta/2 (r2^2 - r1^2)h within the deficit.
    {
        CadSketch s;
        const SketchEntityId square = add(&s, rect(1.5, 0.5, 1.0, 1.0));
        SketchLine axisLine;
        axisLine.start = SketchPoint{0.0, -1.0};
        axisLine.end = SketchPoint{0.0, 2.0};
        add(&s, axisLine);
        RevolveFeature r;
        r.profileEntityId = square;
        r.axis = CadSketchEdgeRef{2, 0};
        r.angleDegrees = 90.0;
        const bool ok = measureState(makeCadRevolveBodyState(s, r), &m);
        const uint32_t n = revolveSegmentCount(90.0);
        const double step = (kPi / 2.0) / n;
        const double polygonal = 0.5 * n * std::sin(step) * 3.0;
        const double analytic = (kPi / 2.0) / 2.0 * 3.0;
        o.check("R2_OR_10_partial_revolve_polygonal_exact_and_analytic_chordal",
                ok && m.closedOriented && exactly(m.volume, polygonal)
                        && chordally(m.volume, analytic, step),
                "rect u[1,2] v[0,1] about u=0, 90 deg", analytic, m.volume);
    }
    // R2-OR-11: a disk extruded: the 32-gon exactly, pi r^2 d chordally.
    {
        CadSketch s;
        SketchCircle c;
        c.radius = 0.8;
        const SketchEntityId id = add(&s, c);
        const bool ok = measureState(makeCadBodyState(s, extrude(id, 1.0)), &m);
        const uint32_t n = kSketchCircleSegments;
        const double step = 2.0 * kPi / n;
        const double polygon = 0.5 * n * 0.64 * std::sin(step);
        o.check("R2_OR_11_circle_extrude_polygon_exact_and_disk_chordal",
                ok && m.closedOriented && exactly(m.volume, polygon)
                        && chordally(m.volume, kPi * 0.64, step),
                "circle r 0.8 depth 1", kPi * 0.64, m.volume);
    }
    // PlanarFaces: a 4 x 3 unit grid (rectangle and five lines).
    CadSketch grid;
    add(&grid, rect(0.0, 0.0, 4.0, 3.0));
    for (int i = -1; i <= 1; ++i) {
        SketchLine l;
        l.start = SketchPoint{static_cast<double>(i), -1.5};
        l.end = SketchPoint{static_cast<double>(i), 1.5};
        add(&grid, l);
    }
    for (double y : {-0.5, 0.5}) {
        SketchLine l;
        l.start = SketchPoint{-2.0, y};
        l.end = SketchPoint{2.0, y};
        add(&grid, l);
    }
    const SketchArrangement arrangement = deriveSketchArrangement(grid);
    // The cell holding (u, v): the one whose OUTER fragments' node box
    // contains it -- read from the node coordinates, not from any area.
    const auto cellOf = [&arrangement](double u, double v) {
        for (size_t i = 0; i < arrangement.faces.size(); ++i) {
            double lu = 1e9, lv = 1e9, hu = -1e9, hv = -1e9;
            for (const FragmentRef& f : arrangement.faces[i].ref.outer) {
                for (const ArrangementFragment& frag : arrangement.fragments) {
                    if (compareFragmentRef(frag.ref, FragmentRef{f.sourceEntityId,
                                                                 f.sourceEdgeLocalIndex,
                                                                 f.startCut, f.endCut, false})
                        == 0) {
                        for (const SketchPoint& p : frag.points) {
                            lu = std::min(lu, p.u);
                            lv = std::min(lv, p.v);
                            hu = std::max(hu, p.u);
                            hv = std::max(hv, p.v);
                        }
                    }
                }
            }
            if (u > lu && u < hu && v > lv && v < hv && hu - lu < 1.5 && hv - lv < 1.5) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };
    const auto faceState = [&](std::vector<int> cells, double depth) {
        std::sort(cells.begin(), cells.end());
        ExtrudeFeature e;
        e.selection = CadSelectionKind::PlanarFaces;
        e.profileEntityId = kNoSketchEntity;
        e.depth = depth;
        for (int c : cells) {
            if (c >= 0) e.planarFaces.push_back(arrangement.faces[static_cast<size_t>(c)].ref);
        }
        return makeCadBodyState(grid, e);
    };
    // R2-OR-12: outer + holes: everything but two separated inner cells:
    // V = (12 - 2) d, one shell, genus 2 (Euler: V - E + F = 2 - 2g read as
    // area: A = 2 * 10 + perimeter(outer + holes) * d).
    {
        std::vector<int> cells;
        const int holeA = cellOf(-0.5, 0.0);
        const int holeB = cellOf(1.5, 0.0);
        for (int i = 0; i < static_cast<int>(arrangement.faces.size()); ++i) {
            if (i != holeA && i != holeB) cells.push_back(i);
        }
        const bool ok = arrangement.faces.size() == 12u && holeA >= 0 && holeB >= 0
                        && measureState(faceState(cells, 0.5), &m);
        // (-0.5, 0) is an interior hole (perimeter 4); (1.5, 0) touches the
        // outline at x = 2, so it is a NOTCH: the outline loses 1 and gains 3.
        const double v = 10.0 * 0.5;
        const double a = 2 * 10.0 + (14.0 - 1.0 + 3.0 + 4.0) * 0.5;
        o.check("R2_OR_12_planar_faces_outer_with_a_hole_and_a_notch_volume_and_area",
                ok && m.closedOriented && m.components == 1u && exactly(m.volume, v)
                        && exactly(m.area, a),
                "grid 4x3 unit cells minus hole (-0.5,0) and notch (1.5,0), depth 0.5", v, m.volume);
    }
    // R2-OR-13: point-touch split: two diagonal cells meet at a node -> two
    // shells, 2 cells of volume.
    {
        const bool ok = measureState(faceState({cellOf(-1.5, -1.0), cellOf(-0.5, 0.0)}, 1.0), &m);
        // Two shells, each closed on its own indices; in SPACE they share the
        // vertical edge over the node, which a position weld reports as the one
        // non-manifold edge (two solids touching along an edge, by design).
        o.check("R2_OR_13_point_touching_cells_split_into_two_shells",
                ok && m.indexClosed && m.indexComponents == 2u && m.weldedNonManifoldEdges == 1u
                        && exactly(m.volume, 2.0),
                "grid cells (-1.5,-1) and (-0.5,0), depth 1", 2.0, m.volume);
    }
    // R2-OR-14: a sketch on a SIDE face (x = +1, facing +x) carries an Add
    // outward: bounds grow to x = 1 + depth, volume adds exactly.
    {
        CadBodyState s = block(2.0, 2.0, 1.0);
        std::vector<CadFace> faces;
        enumerateCadFeatureFaces(s, kCadFeatureId, &faces);
        const CadFace* side = nullptr;
        for (const CadFace& f : faces) {
            if (f.token.kind == CadFaceKind::Side && f.n.x > 0.99f) side = &f;
        }
        bool ok = side != nullptr;
        if (ok) {
            CadSketch boss;
            // Centred on the side's own frame origin, whatever its (u, v) are.
            const SketchEntityId r = add(&boss, rect(0.0, 0.0, 0.4, 0.4));
            appendCadLaterFeatureWithSketch(&s, CadFeatureOperation::Add,
                                            support(s, kCadFeatureId, side->token), std::move(boss),
                                            extrude(r, 0.3));
            ok = measureState(s, &m);
        }
        o.check("R2_OR_14_add_on_a_side_face_extends_bounds_along_its_normal",
                ok && m.closedOriented && m.components == 1u && exactly(m.volume, 4.0 + 0.048)
                        && exactly(m.hi.x, 1.3),
                "block 2x2x1 + 0.4 square on the +x side, depth 0.3", 4.048, m.volume);
    }
    // R2-OR-15: downstream regeneration: widening the base to 3 keeps the
    // pocket and boss: V = 3*2*1 + 0.18 - 0.144.
    {
        CadBodyState wide = shop;
        replaceSketchEntity(&cadBaseSketch(wide), cadBaseSketch(wide).entities.front().id(),
                            rect(0.0, 0.0, 3.0, 2.0));
        const bool ok = measureState(wide, &m);
        const double v = 6.0 + 0.18 - 0.144;
        o.check("R2_OR_15_upstream_width_edit_regenerates_downstream_features",
                ok && m.closedOriented && exactly(m.volume, v) && box(m, -1.5, -1, 0, 1.5, 1, 1.5),
                "OR-05 with base width 3", v, m.volume);
    }
    // R2-OR-16: a rigid placement preserves volume and area: the world mesh
    // through the body's own transform (rotation 30/45/60 deg, translation).
    {
        ConstructionScene scene{NoProjectTag{}};
        SceneObject* body = scene.addCadBody(shop);
        bool ok = body != nullptr;
        Measure local, world;
        if (ok) {
            TransformValues t;
            t.positionX = 3.0;
            t.positionY = -2.0;
            t.positionZ = 0.5;
            t.rotationX = 30.0;
            t.rotationY = 45.0;
            t.rotationZ = 60.0;
            ok = body->transform().setValues(t) == TransformUpdateStatus::Applied;
            ConstructionMesh mesh;
            ok = ok && generateCadMesh(shop, &mesh) == CadStatus::Ok;
            Mat4 model;
            ok = ok && scene.resolveWorldModel(body->objectId(), &model);
            std::vector<P3> moved;
            for (const MeshVertex& v : mesh.vertices) {
                const Vec3 p = mat4TransformPoint(model, Vec3{v.position[0], v.position[1],
                                                             v.position[2]});
                moved.push_back({p.x, p.y, p.z});
            }
            local = measurePoints(pointsOf(mesh), mesh.indices);
            world = measurePoints(moved, mesh.indices);
        }
        o.check("R2_OR_16_rigid_placement_preserves_volume_and_area",
                ok && world.closedOriented && std::fabs(world.volume - local.volume) <= 1e-4 * local.volume
                        && std::fabs(world.area - local.area) <= 1e-4 * local.area,
                "OR-05 body at (3,-2,0.5) rot (30,45,60)", local.volume, world.volume);
    }
    // R2-OR-17: save -> load -> regenerate measures the same solid.
    {
        ProjectDocument document;
        document.kind = ProjectKind::Construction;
        document.scene.nextObjectId = 2;
        document.scene.activeObjectId = 1;
        ProjectBodyPlacement placement;
        placement.objectId = 1;
        document.scene.bodies.push_back(placement);
        document.hasCad = true;
        ProjectCadBody cad;
        cad.objectId = 1;
        cad.state = shop;
        document.cad.bodies.push_back(cad);
        const std::vector<uint8_t> bytes = encodeProjectV1(document);
        ProjectDocument back;
        Measure before, after;
        const bool ok = decodeProject(bytes.data(), bytes.size(), &back) == ProjectCodecStatus::Ok
                        && back.cad.bodies.size() == 1u && measureState(shop, &before)
                        && measureState(back.cad.bodies[0].state, &after);
        o.check("R2_OR_17_save_load_regenerates_the_identical_solid",
                ok && before.volume == after.volume && before.area == after.area
                        && before.triangles == after.triangles,
                "OR-05 through encodeProjectV1/decodeProject", before.volume, after.volume);
    }
}

}  // namespace

void runCadOracleSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* report) {
    Oracle o{out};
    o.last = nullptr;
    runOracles(o);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "CAD_R2_ORACLE_PASS=%d", o.passed);
    std::printf("%s\n", buffer);
    *report = buffer;
}

}  // namespace forgeshape
