#include "forgeshape_cad_feature_selftest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_kernel.h"
#include "forgeshape_cad_v6_selftest.h"
#include "forgeshape_camera.h"
#include "forgeshape_history.h"
#include "forgeshape_math.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_arrangement_selftest.h"
#include "forgeshape_sketch_region.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_support_chooser.h"

namespace forgeshape {
namespace {

struct Recorder {
    CadFeatureSelfTestResult* out;
    int max;
    int n = 0;
    void check(const char* name, bool ok) {
        if (n < max) {
            out[n].name = name;
            out[n].passed = ok;
            ++n;
        }
    }
};

std::string g_performance = "not measured";

// ---------------------------------------------------------------------------
// Kernel-gate fixtures: closed solids built directly, so the gate proves the
// KERNEL SEAM before any CAD domain code is allowed to rely on it.
// ---------------------------------------------------------------------------

// An axis-aligned box, outward counter-clockwise, one tag per face
// (tagBase + 0..5: -Z, +Z, -Y, +X, +Y, -X). `inverted` winds every triangle
// inward, which is the input the gate must refuse rather than read as a hole.
CadSolid box(double x0, double y0, double z0, double x1, double y1, double z1, uint32_t tagBase,
             bool inverted = false) {
    CadSolid s;
    const double v[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                            {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    for (const auto& p : v) {
        s.positions.insert(s.positions.end(), {p[0], p[1], p[2]});
    }
    const uint32_t f[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
                              {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
    for (uint32_t i = 0; i < 6; ++i) {
        const uint32_t a = f[i][0], b = f[i][1], c = f[i][2], d = f[i][3];
        if (inverted) {
            s.indices.insert(s.indices.end(), {a, c, b, a, d, c});
        } else {
            s.indices.insert(s.indices.end(), {a, b, c, a, c, d});
        }
        s.faceTags.push_back(tagBase + i);
        s.faceTags.push_back(tagBase + i);
    }
    return s;
}

// A square prism with a square hole through it along Z: the tool profile
// "containing a hole" the gate asks for, built as a closed solid with inner
// walls wound to face the hole.
CadSolid squareTube(double outer, double inner, double z0, double z1, uint32_t tagBase) {
    CadSolid s;
    const double o = outer * 0.5;
    const double in = inner * 0.5;
    // 0..3 outer bottom, 4..7 inner bottom, 8..11 outer top, 12..15 inner top.
    const double ring[2][4][2] = {{{-o, -o}, {o, -o}, {o, o}, {-o, o}},
                                  {{-in, -in}, {in, -in}, {in, in}, {-in, in}}};
    for (int level = 0; level < 2; ++level) {
        const double z = level == 0 ? z0 : z1;
        for (int r = 0; r < 2; ++r) {
            for (int k = 0; k < 4; ++k) {
                s.positions.insert(s.positions.end(), {ring[r][k][0], ring[r][k][1], z});
            }
        }
    }
    auto tri = [&s](uint32_t a, uint32_t b, uint32_t c, uint32_t tag) {
        s.indices.insert(s.indices.end(), {a, b, c});
        s.faceTags.push_back(tag);
    };
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t k1 = (k + 1u) % 4u;
        // Top annulus (+Z, CCW from above) and bottom annulus (reversed).
        tri(8 + k, 8 + k1, 12 + k1, tagBase + 0);
        tri(8 + k, 12 + k1, 12 + k, tagBase + 0);
        tri(0 + k, 4 + k1, 0 + k1, tagBase + 1);
        tri(0 + k, 4 + k, 4 + k1, tagBase + 1);
        // Outer walls face out; inner walls face into the hole.
        tri(0 + k, 0 + k1, 8 + k1, tagBase + 2 + k);
        tri(0 + k, 8 + k1, 8 + k, tagBase + 2 + k);
        tri(4 + k, 12 + k1, 4 + k1, tagBase + 6 + k);
        tri(4 + k, 12 + k, 12 + k1, tagBase + 6 + k);
    }
    return s;
}

bool nearRel(double a, double b, double rel = 1e-9) {
    return std::fabs(a - b) <= rel * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}

bool hasTag(const CadSolid& s, uint32_t tag) {
    for (uint32_t t : s.faceTags) {
        if (t == tag) {
            return true;
        }
    }
    return false;
}

bool sameSolid(const CadSolid& a, const CadSolid& b) {
    if (a.indices != b.indices || a.faceTags != b.faceTags
        || a.positions.size() != b.positions.size()) {
        return false;
    }
    for (size_t i = 0; i < a.positions.size(); ++i) {
        uint64_t x = 0;
        uint64_t y = 0;
        std::memcpy(&x, &a.positions[i], sizeof(x));
        std::memcpy(&y, &b.positions[i], sizeof(y));
        if (x != y) {
            return false;
        }
    }
    return true;
}

bool tagsAscending(const CadSolid& s) {
    for (size_t i = 1; i < s.faceTags.size(); ++i) {
        if (s.faceTags[i] < s.faceTags[i - 1]) {
            return false;
        }
    }
    return true;
}

void runKernelGate(Recorder& r) {
    const CadSolid base = box(-1, -1, 0, 1, 1, 1, 100);
    CadSolidMeasure mBase;
    r.check("CADVS_K00_base_box_valid_volume_4",
            cadKernelValidateSolid(base, &mBase) == CadKernelStatus::Ok && nearRel(mBase.volume, 4.0)
                    && mBase.components == 1u);

    // K01 union with an overlapping box.
    {
        CadSolid out;
        CadSolidMeasure m;
        const bool ok = cadKernelBoolean(base, box(0.5, -0.5, 0.5, 1.5, 0.5, 1.5, 200),
                                         CadBooleanOp::Union, &out) == CadKernelStatus::Ok
                        && cadKernelMeasure(out, &m) == CadKernelStatus::Ok;
        r.check("CADVS_K01_union_overlapping_box_volume_4_75_one_shell",
                ok && nearRel(m.volume, 4.75) && m.components == 1u && hasTag(out, 100)
                        && hasTag(out, 203) && tagsAscending(out));
    }
    // K02 cut by a through prism: a hole, one shell, volume minus the column.
    {
        CadSolid out;
        CadSolidMeasure m;
        const bool ok = cadKernelBoolean(base, box(-0.25, -0.25, -0.5, 0.25, 0.25, 1.5, 300),
                                         CadBooleanOp::Difference, &out) == CadKernelStatus::Ok
                        && cadKernelMeasure(out, &m) == CadKernelStatus::Ok;
        r.check("CADVS_K02_cut_through_prism_volume_3_75_hole_walls_tagged",
                ok && nearRel(m.volume, 3.75) && m.components == 1u && hasTag(out, 302)
                        && !hasTag(out, 300) && !hasTag(out, 301));
    }
    // K03 blind cut flush with the top face: a pocket with a floor, the top
    // face still present, the tool's own top face absent (the opening is open).
    {
        CadSolid out;
        CadSolidMeasure m;
        const bool ok = cadKernelBoolean(base, box(-0.25, -0.25, 0.5, 0.25, 0.25, 1.0, 400),
                                         CadBooleanOp::Difference, &out) == CadKernelStatus::Ok
                        && cadKernelMeasure(out, &m) == CadKernelStatus::Ok;
        r.check("CADVS_K03_blind_cut_flush_top_pocket_floor_open_mouth",
                ok && nearRel(m.volume, 3.875) && m.components == 1u && hasTag(out, 400)
                        && !hasTag(out, 401) && hasTag(out, 101));
    }
    // K04 a tool profile containing a hole: union and difference are both
    // mathematically valid and both produce exactly the expected volume.
    {
        const CadSolid tube = squareTube(1.0, 0.5, 0.5, 1.5, 500);
        CadSolidMeasure mt;
        const bool tubeOk = cadKernelValidateSolid(tube, &mt) == CadKernelStatus::Ok
                            && nearRel(mt.volume, 0.75);
        CadSolid u;
        CadSolid d;
        CadSolidMeasure mu;
        CadSolidMeasure md;
        const bool ok = tubeOk
                        && cadKernelBoolean(base, tube, CadBooleanOp::Union, &u) == CadKernelStatus::Ok
                        && cadKernelBoolean(base, tube, CadBooleanOp::Difference, &d)
                                   == CadKernelStatus::Ok
                        && cadKernelMeasure(u, &mu) == CadKernelStatus::Ok
                        && cadKernelMeasure(d, &md) == CadKernelStatus::Ok;
        // Overlap of the tube with the base: z 0.5..1.0, area 1 - 0.25 = 0.75.
        r.check("CADVS_K04_tool_with_hole_union_and_cut_exact",
                ok && nearRel(mu.volume, 4.0 + 0.75 - 0.375) && nearRel(md.volume, 4.0 - 0.375)
                        && mu.components == 1u && md.components == 1u);
    }
    // K05 a disjoint union is DETECTABLE: it adds a shell. The R1 rule refuses
    // it as a disjoint Add; the kernel's job is only to make that visible.
    {
        CadSolid out;
        CadSolidMeasure m;
        const bool ok = cadKernelBoolean(base, box(3, 3, 3, 4, 4, 4, 600), CadBooleanOp::Union, &out)
                                == CadKernelStatus::Ok
                        && cadKernelMeasure(out, &m) == CadKernelStatus::Ok;
        r.check("CADVS_K05_disjoint_union_adds_a_shell", ok && m.components == 2u
                                                              && nearRel(m.volume, 5.0));
    }
    // K06 a disjoint cut changes nothing: detectable as "no intersection".
    {
        CadSolid out;
        CadSolidMeasure m;
        const bool ok = cadKernelBoolean(base, box(3, 3, 3, 4, 4, 4, 700),
                                         CadBooleanOp::Difference, &out) == CadKernelStatus::Ok
                        && cadKernelMeasure(out, &m) == CadKernelStatus::Ok;
        r.check("CADVS_K06_disjoint_cut_volume_unchanged", ok && nearRel(m.volume, 4.0)
                                                               && m.components == 1u);
    }
    // K07 coplanar / touching faces have ONE named deterministic result: a
    // union of face-touching solids MERGES into one shell of the summed volume,
    // and a difference with a face-touching tool removes nothing.
    {
        CadSolid u;
        CadSolid d;
        CadSolidMeasure mu;
        CadSolidMeasure md;
        const CadSolid touching = box(1, -1, 0, 2, 1, 1, 800);
        const bool ok = cadKernelBoolean(base, touching, CadBooleanOp::Union, &u) == CadKernelStatus::Ok
                        && cadKernelBoolean(base, touching, CadBooleanOp::Difference, &d)
                                   == CadKernelStatus::Ok
                        && cadKernelMeasure(u, &mu) == CadKernelStatus::Ok
                        && cadKernelMeasure(d, &md) == CadKernelStatus::Ok;
        r.check("CADVS_K07_touching_union_merges_touching_cut_removes_nothing",
                ok && mu.components == 1u && nearRel(mu.volume, 6.0) && nearRel(md.volume, 4.0));
        // A flush boss on the top face is the everyday case of the same rule.
        CadSolid boss;
        CadSolidMeasure mb;
        const bool bossOk = cadKernelBoolean(base, box(-0.25, -0.25, 1.0, 0.25, 0.25, 1.5, 900),
                                             CadBooleanOp::Union, &boss) == CadKernelStatus::Ok
                            && cadKernelMeasure(boss, &mb) == CadKernelStatus::Ok;
        r.check("CADVS_K07b_flush_boss_union_one_shell_no_internal_face",
                bossOk && mb.components == 1u && nearRel(mb.volume, 4.125) && !hasTag(boss, 900));
    }
    // K08 a solid wound INWARD is refused by name, never read as a hole that
    // would turn a union into a subtraction.
    {
        const CadSolid inverted = box(0.5, -0.5, 0.5, 1.5, 0.5, 1.5, 1000, /*inverted=*/true);
        CadSolid out;
        out.indices.push_back(42);  // sentinel: a refusal must not write
        CadSolidMeasure m;
        r.check("CADVS_K08_inverted_input_refused_not_subtracted",
                cadKernelValidateSolid(inverted, &m) == CadKernelStatus::InvertedInput
                        && cadKernelBoolean(base, inverted, CadBooleanOp::Union, &out)
                                   == CadKernelStatus::InvertedInput
                        && out.indices.size() == 1u && out.indices[0] == 42u);
    }
    // K09 determinism: the same operation twice is bit-identical, canonical
    // order included.
    {
        CadSolid a1;
        CadSolid a2;
        const CadSolid tool = box(-0.3, -0.3, -0.5, 0.3, 0.3, 0.6, 1100);
        const bool ok = cadKernelBoolean(base, tool, CadBooleanOp::Difference, &a1) == CadKernelStatus::Ok
                        && cadKernelBoolean(base, tool, CadBooleanOp::Difference, &a2)
                                   == CadKernelStatus::Ok;
        r.check("CADVS_K09_repeated_operation_bit_identical", ok && sameSolid(a1, a2)
                                                                  && tagsAscending(a1));
    }
    // K10 an open (non-manifold) input is refused, and so are malformed arrays
    // and non-finite coordinates -- never published, never a crash.
    {
        CadSolid open = box(0, 0, 0, 1, 1, 1, 1200);
        open.indices.resize(open.indices.size() - 3u);
        open.faceTags.pop_back();
        CadSolid badTags = box(0, 0, 0, 1, 1, 1, 1300);
        badTags.faceTags.pop_back();
        CadSolid nan = box(0, 0, 0, 1, 1, 1, 1400);
        nan.positions[0] = std::nan("");
        CadSolid out;
        CadSolidMeasure m;
        r.check("CADVS_K10_open_malformed_nonfinite_inputs_refused",
                cadKernelValidateSolid(open, &m) == CadKernelStatus::NotManifold
                        && cadKernelValidateSolid(badTags, &m) == CadKernelStatus::InvalidInput
                        && cadKernelValidateSolid(nan, &m) == CadKernelStatus::NonFinite
                        && cadKernelBoolean(base, open, CadBooleanOp::Union, &out)
                                   == CadKernelStatus::NotManifold
                        && out.empty());
    }
    // K11 a cut that removes everything is an EMPTY result, not a failure: the
    // R1 rule refuses it as `CutRemovesBody`, and the kernel reports it plainly.
    {
        CadSolid out;
        CadSolidMeasure m;
        const bool ok = cadKernelBoolean(base, box(-2, -2, -1, 2, 2, 2, 1500),
                                         CadBooleanOp::Difference, &out) == CadKernelStatus::Ok
                        && cadKernelMeasure(out, &m) == CadKernelStatus::Ok;
        r.check("CADVS_K11_total_cut_is_empty_result", ok && out.empty() && m.volume == 0.0
                                                           && m.components == 0u);
    }
    // K12 region triangulation with a hole covers exactly outer minus hole,
    // counter-clockwise, in either input orientation.
    {
        std::vector<std::vector<SketchPoint>> loops = {
                {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}},
                {{-0.25, -0.25}, {0.25, -0.25}, {0.25, 0.25}, {-0.25, 0.25}}};
        std::vector<uint32_t> ccw;
        std::vector<uint32_t> reversedIdx;
        auto area = [](const std::vector<std::vector<SketchPoint>>& ls,
                       const std::vector<uint32_t>& idx) {
            std::vector<SketchPoint> all;
            for (const auto& l : ls) {
                all.insert(all.end(), l.begin(), l.end());
            }
            double a = 0.0;
            for (size_t t = 0; t + 2 < idx.size(); t += 3) {
                const SketchPoint& p0 = all[idx[t]];
                const SketchPoint& p1 = all[idx[t + 1]];
                const SketchPoint& p2 = all[idx[t + 2]];
                a += 0.5 * ((p1.u - p0.u) * (p2.v - p0.v) - (p2.u - p0.u) * (p1.v - p0.v));
            }
            return a;
        };
        const bool okA = cadKernelTriangulateRegion(loops, &ccw) == CadKernelStatus::Ok;
        std::vector<std::vector<SketchPoint>> reversed = loops;
        std::reverse(reversed[0].begin(), reversed[0].end());
        std::reverse(reversed[1].begin(), reversed[1].end());
        const bool okB = cadKernelTriangulateRegion(reversed, &reversedIdx) == CadKernelStatus::Ok;
        r.check("CADVS_K12_region_triangulation_with_hole_exact_area_any_orientation",
                okA && okB && nearRel(area(loops, ccw), 3.75) && nearRel(area(reversed, reversedIdx), 3.75));
    }
    r.check("CADVS_K13_kernel_identity_pinned",
            std::string(cadKernelIdentity()) == "manifold-3.5.4");
}

// ===========================================================================
// The CAD domain on top of the gate: regions with holes, their extrusion, the
// retained feature chain and its operations, the sketch session, the `CADB`
// v5 codec and the acceptance model's timings. Every case builds its own
// sketch, state, body, scene, history, session and camera; nothing reads the
// process-scoped `sketchSession()` or any other live state.
// ===========================================================================

constexpr double kPi = 3.14159265358979323846;

std::string g_digests = "not measured";

bool nearf(float a, float b, float tol = 1e-5f) { return std::fabs(a - b) <= tol; }

bool sameDoubleBits(double a, double b) {
    uint64_t x = 0;
    uint64_t y = 0;
    std::memcpy(&x, &a, sizeof(x));
    std::memcpy(&y, &b, sizeof(y));
    return x == y;
}

double microsSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
}

double medianOf(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    return values[values.size() / 2u];
}

// --- sketch fixtures --------------------------------------------------------

SketchRectangle rectangleAt(double cu, double cv, double w, double h) {
    SketchRectangle rectangle;
    rectangle.center = SketchPoint{cu, cv};
    rectangle.width = w;
    rectangle.height = h;
    return rectangle;
}

SketchCircle circleAt(double cu, double cv, double radius) {
    SketchCircle circle;
    circle.center = SketchPoint{cu, cv};
    circle.radius = radius;
    return circle;
}

SketchEntityId addRect(CadSketch* sketch, double cu, double cv, double w, double h) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, rectangleAt(cu, cv, w, h), &id);
    return id;
}

SketchEntityId addCircle(CadSketch* sketch, double cu, double cv, double radius) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, circleAt(cu, cv, radius), &id);
    return id;
}

SketchEntityId addClosedPolyline(CadSketch* sketch, std::vector<SketchPoint> points) {
    SketchPolyline polyline;
    polyline.vertices = std::move(points);
    polyline.closed = true;
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, std::move(polyline), &id);
    return id;
}

CadSketch rectSketch(double cu, double cv, double w, double h) {
    CadSketch sketch;
    addRect(&sketch, cu, cv, w, h);
    return sketch;
}

CadSketch circleSketch(double cu, double cv, double radius) {
    CadSketch sketch;
    addCircle(&sketch, cu, cv, radius);
    return sketch;
}

// The area of the polygon a circle of this radius is extruded as -- the
// 32-gon the product derives, measured -- never pi r^2.
double circleArea(double radius) {
    return 0.5 * polygonSignedAreaTwice(circleProfilePolygon(circleAt(0.0, 0.0, radius)));
}

ProfileRegionRef regionRef(SketchEntityId outer, std::vector<SketchEntityId> holes = {}) {
    ProfileRegionRef ref;
    ref.outerAnchorId = outer;
    ref.holeAnchorIds = std::move(holes);
    return ref;
}

// --- extrusions and states --------------------------------------------------

ExtrudeFeature oneSide(double depth, ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = 1;
    extrude.depth = depth;
    extrude.direction = direction;
    return extrude;
}

ExtrudeFeature symmetricExtent(double perSide) {
    ExtrudeFeature extrude = oneSide(perSide);
    extrude.extent = ExtrudeExtentMode::Symmetric;
    return extrude;
}

ExtrudeFeature twoSidesExtent(double a, double b) {
    ExtrudeFeature extrude = oneSide(a);
    extrude.extent = ExtrudeExtentMode::TwoSides;
    extrude.secondDistance = b;
    return extrude;
}

// The acceptance model's base: a 4 x 3 rectangle (id 1) around a centred
// circle of radius 0.8 (id 2), the RING chosen -- the rectangle with its hole.
CadBodyState regionHoleState(ExtrudeFeature extrude = oneSide(1.0)) {
    CadBodyState state;
    addRect(&cadBaseSketch(state), 0.0, 0.0, 4.0, 3.0);
    addCircle(&cadBaseSketch(state), 0.0, 0.0, 0.8);
    state.extrude = extrude;
    setExtrudeRegions(&state.extrude, {regionRef(1, {2})});
    return state;
}

// A 2 x 2 x depth block on XY, the chain cases' base.
CadBodyState blockState(double depth = 1.0) {
    CadBodyState state;
    addRect(&cadBaseSketch(state), 0.0, 0.0, 2.0, 2.0);
    state.extrude = oneSide(depth);
    return state;
}

// A cap of an EARLIER feature, at that feature's own lineage -- exactly what a
// sketch placed on it records.
CadFeatureSupport capSupport(const CadBodyState& state, uint32_t featureId,
                             CadFaceKind kind = CadFaceKind::CapFar) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face.kind = kind;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

// Appends one later feature, standing on a cap of `supportFeatureId`, with the
// next feature id -- a new retained sketch in the table and a feature naming it.
CadBodyState withFeature(CadBodyState state, CadFeatureOperation operation,
                         uint32_t supportFeatureId, CadSketch sketch, ExtrudeFeature extrude,
                         CadFaceKind kind = CadFaceKind::CapFar) {
    appendCadLaterFeatureWithSketch(&state, operation, capSupport(state, supportFeatureId, kind),
                                    std::move(sketch), std::move(extrude));
    return state;
}

// The sketch record the i-th LATER feature extrudes (`CAD-V6-S1`): where its
// support and its entities now live.
CadSketchRecord& laterRecord(CadBodyState& state, size_t i) {
    return *findCadSketchRecord(state, state.laterFeatures[i].sketchId);
}
const CadSketchRecord& laterRecord(const CadBodyState& state, size_t i) {
    return *findCadSketchRecord(state, state.laterFeatures[i].sketchId);
}

// The three chain fixtures the codec cases also persist.
CadBodyState featureAddState() {
    return withFeature(blockState(), CadFeatureOperation::Add, kCadFeatureId,
                       rectSketch(0.0, 0.0, 0.8, 0.8), oneSide(0.5));
}

CadBodyState featureCutState() {
    return withFeature(blockState(), CadFeatureOperation::Cut, kCadFeatureId,
                       circleSketch(0.0, 0.0, 0.3), oneSide(0.5, ExtrudeDirection::AgainstNormal));
}

// The owner's acceptance model: the ring, a Symmetric boss on its far cap
// beside the hole, and a blind round pocket on the other side of the hole.
CadBodyState chainAddOnlyState() {
    return withFeature(regionHoleState(), CadFeatureOperation::Add, kCadFeatureId,
                       rectSketch(1.4, 0.0, 0.6, 0.6), symmetricExtent(0.25));
}

CadBodyState featureChainState() {
    return withFeature(chainAddOnlyState(), CadFeatureOperation::Cut, kCadFeatureId,
                       circleSketch(-1.4, 0.0, 0.3),
                       oneSide(0.5, ExtrudeDirection::AgainstNormal));
}

// --- mesh measurements ------------------------------------------------------

// Every undirected edge on exactly two triangles, once in each direction.
bool watertight(const ConstructionMesh& mesh) {
    if (mesh.indices.empty() || mesh.indices.size() % 3u != 0u) {
        return false;
    }
    std::map<std::pair<uint32_t, uint32_t>, int> directed;
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const uint32_t a = mesh.indices[t];
        const uint32_t b = mesh.indices[t + 1];
        const uint32_t c = mesh.indices[t + 2];
        if (a == b || b == c || a == c) {
            return false;
        }
        ++directed[{a, b}];
        ++directed[{b, c}];
        ++directed[{c, a}];
    }
    for (const auto& entry : directed) {
        if (entry.second != 1) {
            return false;
        }
        const auto twin = directed.find({entry.first.second, entry.first.first});
        if (twin == directed.end() || twin->second != 1) {
            return false;
        }
    }
    return true;
}

// Divergence theorem over the float render mesh: positive exactly when every
// face winds counter-clockwise seen from outside.
double meshSignedVolume(const ConstructionMesh& mesh) {
    double six = 0.0;
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const float* a = mesh.vertices[mesh.indices[t]].position;
        const float* b = mesh.vertices[mesh.indices[t + 1]].position;
        const float* c = mesh.vertices[mesh.indices[t + 2]].position;
        const double bx = b[0], by = b[1], bz = b[2];
        const double cx = c[0], cy = c[1], cz = c[2];
        six += a[0] * (by * cz - bz * cy) - a[1] * (bx * cz - bz * cx) + a[2] * (bx * cy - by * cx);
    }
    return six / 6.0;
}

bool meshFinite(const ConstructionMesh& mesh) {
    if (mesh.vertices.empty()) {
        return false;
    }
    for (const MeshVertex& v : mesh.vertices) {
        for (int c = 0; c < 3; ++c) {
            if (!std::isfinite(v.position[c])) {
                return false;
            }
        }
    }
    for (uint32_t i : mesh.indices) {
        if (i >= mesh.vertices.size()) {
            return false;
        }
    }
    return true;
}

void meshBounds(const ConstructionMesh& mesh, float lo[3], float hi[3]) {
    for (int c = 0; c < 3; ++c) {
        lo[c] = 1e30f;
        hi[c] = -1e30f;
    }
    for (const MeshVertex& v : mesh.vertices) {
        for (int c = 0; c < 3; ++c) {
            lo[c] = std::fmin(lo[c], v.position[c]);
            hi[c] = std::fmax(hi[c], v.position[c]);
        }
    }
}

// Connected shells, counted on the index topology alone -- independent of the
// `components` the regeneration reports about itself.
uint32_t meshComponents(const ConstructionMesh& mesh) {
    std::vector<uint32_t> parent(mesh.vertices.size());
    std::iota(parent.begin(), parent.end(), 0u);
    auto find = [&parent](uint32_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    std::vector<uint8_t> used(mesh.vertices.size(), 0u);
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const uint32_t a = find(mesh.indices[t]);
        const uint32_t b = find(mesh.indices[t + 1]);
        const uint32_t c = find(mesh.indices[t + 2]);
        parent[b] = a;
        parent[find(c)] = a;
        used[mesh.indices[t]] = used[mesh.indices[t + 1]] = used[mesh.indices[t + 2]] = 1u;
    }
    uint32_t roots = 0;
    for (uint32_t i = 0; i < parent.size(); ++i) {
        if (used[i] != 0u && find(i) == i) {
            ++roots;
        }
    }
    return roots;
}

Vec3 vertexAt(const ConstructionMesh& mesh, uint32_t index) {
    const float* p = mesh.vertices[index].position;
    return Vec3{p[0], p[1], p[2]};
}

// One tag per triangle, every tag naming a face of the table.
bool faceTableConsistent(const CadBodyMesh& m) {
    if (m.faces.empty() || m.triangleFace.size() * 3u != m.mesh.indices.size()) {
        return false;
    }
    for (uint32_t tag : m.triangleFace) {
        if (tag >= m.faces.size()) {
            return false;
        }
    }
    return true;
}

bool sameMeshBits(const ConstructionMesh& a, const ConstructionMesh& b) {
    if (a.vertices.size() != b.vertices.size() || a.indices != b.indices) {
        return false;
    }
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (std::memcmp(a.vertices[i].position, b.vertices[i].position, sizeof(float) * 3u) != 0) {
            return false;
        }
    }
    return true;
}

bool sameBodyMesh(const CadBodyMesh& a, const CadBodyMesh& b) {
    if (!sameMeshBits(a.mesh, b.mesh) || a.triangleFace != b.triangleFace
        || a.faces.size() != b.faces.size() || !sameDoubleBits(a.volume, b.volume)
        || a.components != b.components) {
        return false;
    }
    for (size_t i = 0; i < a.faces.size(); ++i) {
        if (a.faces[i].featureId != b.faces[i].featureId
            || !sameCadFaceToken(a.faces[i].token, b.faces[i].token)
            || a.faces[i].eligible != b.faces[i].eligible) {
            return false;
        }
    }
    return true;
}

// The index in the face table of a feature's cap, or UINT32_MAX.
uint32_t capIndex(const CadBodyMesh& m, uint32_t featureId, CadFaceKind kind) {
    for (uint32_t i = 0; i < m.faces.size(); ++i) {
        if (m.faces[i].featureId == featureId && m.faces[i].token.kind == kind) {
            return i;
        }
    }
    return 0xFFFFFFFFu;
}

bool meshHasFace(const CadBodyMesh& m, uint32_t index) {
    return std::find(m.triangleFace.begin(), m.triangleFace.end(), index) != m.triangleFace.end();
}

// Whether the face has triangles and every one of their vertices lies at z.
bool faceAtZ(const CadBodyMesh& m, uint32_t index, float z) {
    bool any = false;
    for (size_t t = 0; t < m.triangleFace.size(); ++t) {
        if (m.triangleFace[t] != index) {
            continue;
        }
        any = true;
        for (size_t k = 0; k < 3; ++k) {
            if (!nearf(vertexAt(m.mesh, m.mesh.indices[t * 3u + k]).z, z)) {
                return false;
            }
        }
    }
    return any;
}

struct Regen {
    CadStatus why = CadStatus::RegenerationFailed;
    CadRegenerationReport report;
    CadBodyMesh mesh;
};

Regen regen(const CadBodyState& state) {
    Regen g;
    g.why = regenerateCadBody(state, &g.mesh, &g.report);
    return g;
}

// A closed, outward, finite, single-shell solid of exactly this volume.
bool solidOk(const Regen& g, double volume) {
    return g.why == CadStatus::Ok && g.report.status == CadStatus::Ok
           && g.report.failedFeatureId == 0u && g.mesh.components == 1u
           && nearRel(g.mesh.volume, volume) && watertight(g.mesh.mesh) && meshFinite(g.mesh.mesh)
           && faceTableConsistent(g.mesh) && meshSignedVolume(g.mesh.mesh) > 0.0
           && nearRel(meshSignedVolume(g.mesh.mesh), volume, 1e-5)
           && meshComponents(g.mesh.mesh) == 1u;
}

// A refusal by name, blamed on the feature that refused, writing nothing.
bool refusedBy(const CadBodyState& state, CadStatus why, uint32_t featureId) {
    CadBodyMesh untouched;
    untouched.components = 99u;
    CadRegenerationReport report;
    const CadStatus status = regenerateCadBody(state, &untouched, &report);
    return status == why && report.status == why && report.failedFeatureId == featureId
           && untouched.components == 99u && untouched.mesh.indices.empty();
}

// ---------------------------------------------------------------------------
// A. Regions (forgeshape_sketch_region.h)
// ---------------------------------------------------------------------------

void testRegions(Recorder& r) {
    const double ringArea = 12.0 - circleArea(0.8);
    {
        CadSketch s;
        addRect(&s, 0.5, 0.25, 2.0, 1.0);
        const SketchRegionExtraction x = extractSketchRegions(s);
        r.check("CADVS_REG_01_single_rectangle_is_one_region_without_holes",
                x.loops.profiles.size() == 1u && x.regions.size() == 1u
                        && x.regions[0].outerAnchorId == 1u && x.regions[0].holeAnchorIds.empty()
                        && x.regions[0].holeLoops.empty() && x.regions[0].depth == 0u
                        && x.regions[0].status == CadStatus::Ok && nearRel(x.regions[0].area, 2.0)
                        && x.parent.size() == 1u && x.parent[0] == -1
                        && validateRegionSelection(x, {regionRef(1)}) == CadStatus::Ok);
    }
    {
        CadSketch s;
        addCircle(&s, -0.5, 0.5, 0.75);
        const SketchRegionExtraction x = extractSketchRegions(s);
        r.check("CADVS_REG_02_single_circle_is_one_region_of_the_32gon_area",
                x.regions.size() == 1u && x.regions[0].outerAnchorId == 1u
                        && x.regions[0].holeAnchorIds.empty() && x.loops.profiles[0].fromCircle
                        && x.loops.profiles[0].polygon.size() == kSketchCircleSegments
                        && nearRel(x.regions[0].area, circleArea(0.75), 1e-12)
                        && nearRel(circleArea(0.75),
                                   0.5 * 32.0 * 0.75 * 0.75 * std::sin(2.0 * kPi / 32.0), 1e-12)
                        && validateRegionSelection(x, {regionRef(1)}) == CadStatus::Ok);
    }
    {
        CadSketch s;
        addRect(&s, 0.0, 0.0, 4.0, 3.0);
        addCircle(&s, 0.0, 0.0, 0.8);
        const SketchRegionExtraction x = extractSketchRegions(s);
        const bool two = x.regions.size() == 2u && x.loops.profiles.size() == 2u;
        r.check("CADVS_REG_03_rectangle_around_circle_is_exactly_ring_and_disk",
                two && x.regions[0].outerAnchorId == 1u
                        && x.regions[0].holeAnchorIds == std::vector<SketchEntityId>{2u}
                        && x.regions[0].holeLoops == std::vector<uint32_t>{1u}
                        && x.regions[1].outerAnchorId == 2u && x.regions[1].holeAnchorIds.empty()
                        && x.regions[0].depth == 0u && x.regions[1].depth == 1u
                        && x.parent[0] == -1 && x.parent[1] == 0 && x.loopContains(0, 1)
                        && !x.loopsConflict(0, 1) && x.loops.rejections.empty());
        r.check("CADVS_REG_04_ring_and_disk_areas_are_exact",
                two && nearRel(x.regions[0].area, ringArea, 1e-12)
                        && nearRel(x.regions[1].area, circleArea(0.8), 1e-12)
                        && x.regions[0].status == CadStatus::Ok
                        && x.regions[1].status == CadStatus::Ok);
        bool labelOnMaterial = false;
        if (two) {
            const std::vector<std::vector<SketchPoint>> ring = sketchRegionLoops(x, x.regions[0]);
            labelOnMaterial = ring.size() == 2u
                              && sketchPointStrictlyInside(x.regions[0].interiorPoint, ring[0])
                              && !sketchPointStrictlyInside(x.regions[0].interiorPoint, ring[1])
                              && std::fabs(x.regions[0].centroid.u) < 1e-9
                              && std::fabs(x.regions[0].centroid.v) < 1e-9;
        }
        r.check("CADVS_REG_05_ring_label_point_is_on_material_while_its_centroid_is_in_the_hole",
                labelOnMaterial);
        // `CAD-FOUNDATION-C1`: the ring and its own disk together are legal and
        // mean their union -- one solid rectangle -- rather than a refusal.
        const std::vector<SketchRegionComponent> both =
                mergeSelectedRegions(x, {regionRef(1, {2}), regionRef(2)});
        r.check("CADVS_REG_06_ring_or_disk_alone_is_valid_and_both_together_mean_their_union",
                two && validateRegionSelection(x, {regionRef(1, {2})}) == CadStatus::Ok
                        && validateRegionSelection(x, {regionRef(2)}) == CadStatus::Ok
                        && validateRegionSelection(x, {regionRef(1, {2}), regionRef(2)})
                                   == CadStatus::Ok
                        && both.size() == 1u && both[0].outerAnchorId == 1u
                        && both[0].holeLoops.empty() && nearRel(both[0].area, 12.0, 1e-12)
                        && validateRegionSelection(x, {}) == CadStatus::AmbiguousProfile);
        r.check("CADVS_REG_07_nested_loops_are_no_longer_refused_as_nested",
                extractClosedProfiles(s).rejections.empty()
                        && extractClosedProfiles(s).profiles.size() == 2u);
    }
    {
        CadSketch s;
        addRect(&s, -2.0, 0.0, 1.0, 1.0);
        addRect(&s, 2.0, 0.0, 1.0, 1.0);
        const SketchRegionExtraction x = extractSketchRegions(s);
        r.check("CADVS_REG_08_two_disjoint_rectangles_are_two_selectable_regions",
                x.regions.size() == 2u && x.regions[0].holeAnchorIds.empty()
                        && x.regions[1].holeAnchorIds.empty() && x.parent[0] == -1
                        && x.parent[1] == -1 && !x.loopsConflict(0, 1)
                        && validateRegionSelection(x, {regionRef(1), regionRef(2)}) == CadStatus::Ok
                        && validateRegionSelection(x, {}) == CadStatus::AmbiguousProfile);
    }
    {
        // rect > circle 1 > circle 2: even/odd without a second rule.
        CadSketch s;
        addRect(&s, 0.0, 0.0, 4.0, 3.0);
        addCircle(&s, 0.0, 0.0, 1.2);
        addCircle(&s, 0.0, 0.0, 0.5);
        const SketchRegionExtraction x = extractSketchRegions(s);
        const bool three = x.regions.size() == 3u;
        r.check("CADVS_REG_09_deep_nesting_gives_each_loop_only_its_direct_child_as_hole",
                three && x.regions[0].holeAnchorIds == std::vector<SketchEntityId>{2u}
                        && x.regions[1].holeAnchorIds == std::vector<SketchEntityId>{3u}
                        && x.regions[2].holeAnchorIds.empty() && x.parent[0] == -1
                        && x.parent[1] == 0 && x.parent[2] == 1 && x.regions[2].depth == 2u
                        && nearRel(x.regions[0].area, 12.0 - circleArea(1.2), 1e-12)
                        && nearRel(x.regions[1].area, circleArea(1.2) - circleArea(0.5), 1e-12));
        // Even/odd through the union: the outer ring with the island is two
        // components; the outer ring with its own middle ring is one ring
        // whose hole is the innermost disk; the middle ring with its own disk
        // is one solid disk.
        const std::vector<SketchRegionComponent> island =
                mergeSelectedRegions(x, {regionRef(1, {2}), regionRef(3)});
        const std::vector<SketchRegionComponent> outerTwo =
                mergeSelectedRegions(x, {regionRef(1, {2}), regionRef(2, {3})});
        const std::vector<SketchRegionComponent> innerTwo =
                mergeSelectedRegions(x, {regionRef(2, {3}), regionRef(3)});
        r.check("CADVS_REG_10_nested_selections_union_by_even_odd_over_the_nesting_tree",
                three
                        && validateRegionSelection(x, {regionRef(1, {2}), regionRef(3)})
                                   == CadStatus::Ok
                        && validateRegionSelection(x, {regionRef(1, {2}), regionRef(2, {3})})
                                   == CadStatus::Ok
                        && validateRegionSelection(x, {regionRef(2, {3}), regionRef(3)})
                                   == CadStatus::Ok
                        && island.size() == 2u && island[0].outerAnchorId == 1u
                        && island[0].holeAnchorIds == std::vector<SketchEntityId>{2u}
                        && island[1].outerAnchorId == 3u && island[1].holeLoops.empty()
                        && outerTwo.size() == 1u && outerTwo[0].outerAnchorId == 1u
                        && outerTwo[0].holeAnchorIds == std::vector<SketchEntityId>{3u}
                        && nearRel(outerTwo[0].area, 12.0 - circleArea(0.5), 1e-12)
                        && innerTwo.size() == 1u && innerTwo[0].outerAnchorId == 2u
                        && innerTwo[0].holeLoops.empty()
                        && nearRel(innerTwo[0].area, circleArea(1.2), 1e-12));
    }
    {
        const std::vector<SketchPoint> ccw = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 1.0},
                                              {1.0, 1.0}, {1.0, 2.0}, {0.0, 2.0}};
        const std::vector<SketchPoint> cw(ccw.rbegin(), ccw.rend());
        CadSketch a;
        CadSketch b;
        addClosedPolyline(&a, ccw);
        addClosedPolyline(&b, cw);
        const SketchRegionExtraction xa = extractSketchRegions(a);
        const SketchRegionExtraction xb = extractSketchRegions(b);
        bool same = xa.regions.size() == 1u && xb.regions.size() == 1u;
        if (same) {
            std::vector<SketchPoint> pa = xa.loops.profiles[0].polygon;
            std::vector<SketchPoint> pb = xb.loops.profiles[0].polygon;
            auto byUV = [](const SketchPoint& p, const SketchPoint& q) {
                return p.u < q.u || (p.u == q.u && p.v < q.v);
            };
            std::sort(pa.begin(), pa.end(), byUV);
            std::sort(pb.begin(), pb.end(), byUV);
            bool sameVertices = pa.size() == pb.size();
            for (size_t i = 0; sameVertices && i < pa.size(); ++i) {
                sameVertices = pa[i].u == pb[i].u && pa[i].v == pb[i].v;
            }
            same = sameVertices && nearRel(xa.regions[0].area, 3.0, 1e-12)
                   && nearRel(xb.regions[0].area, 3.0, 1e-12)
                   && polygonSignedAreaTwice(xa.loops.profiles[0].polygon) > 0.0
                   && polygonSignedAreaTwice(xb.loops.profiles[0].polygon) > 0.0
                   && xa.regions[0].interiorPoint.u == xb.regions[0].interiorPoint.u
                   && xa.regions[0].interiorPoint.v == xb.regions[0].interiorPoint.v;
        }
        r.check("CADVS_REG_11_clockwise_and_counter_clockwise_polylines_are_one_region", same);
        CadBodyState sa;
        cadBaseSketch(sa) = a;
        sa.extrude = oneSide(1.0);
        CadBodyState sb;
        cadBaseSketch(sb) = b;
        sb.extrude = oneSide(1.0);
        const Regen ga = regen(sa);
        const Regen gb = regen(sb);
        r.check("CADVS_REG_12_either_winding_extrudes_outward_with_the_same_volume",
                solidOk(ga, 3.0) && solidOk(gb, 3.0));
    }
    {
        CadSketch s;
        addClosedPolyline(&s, {{0.0, 0.0}, {2.0, 2.0}, {2.0, 0.0}, {0.0, 1.0}});
        addRect(&s, 5.0, 5.0, 1.0, 1.0);
        const SketchRegionExtraction x = extractSketchRegions(s);
        r.check("CADVS_REG_13_bowtie_is_rejected_self_intersecting_and_yields_no_region",
                x.loops.rejections.size() == 1u && x.loops.rejections[0].anchorEntityId == 1u
                        && x.loops.rejections[0].why == CadStatus::SelfIntersectingProfile
                        && x.regions.size() == 1u && x.regions[0].outerAnchorId == 2u
                        && validateRegionSelection(x, {regionRef(1)}) == CadStatus::ProfileNotFound);
    }
    {
        // Tangent from inside: the circle's exact +U vertex lands on the edge.
        CadSketch s;
        addRect(&s, 0.0, 0.0, 4.0, 3.0);
        addCircle(&s, 1.25, 0.0, 0.75);
        const SketchRegionExtraction x = extractSketchRegions(s);
        r.check("CADVS_REG_14_circle_touching_the_rectangle_edge_is_not_a_hole",
                x.regions.size() == 2u && x.regions[0].holeAnchorIds.empty()
                        && x.regions[1].holeAnchorIds.empty() && x.loopsConflict(0, 1)
                        && x.parent[0] == -1 && x.parent[1] == -1
                        && validateRegionSelection(x, {regionRef(1)}) == CadStatus::Ok
                        && validateRegionSelection(x, {regionRef(2)}) == CadStatus::Ok);
        r.check("CADVS_REG_15_selecting_touching_regions_together_is_refused",
                x.regions.size() == 2u
                        && validateRegionSelection(x, {regionRef(1), regionRef(2)})
                                   == CadStatus::OverlappingRegions);
    }
    {
        CadSketch s;
        addRect(&s, 0.0, 0.0, 4.0, 3.0);
        addCircle(&s, -0.3, 0.0, 0.5);
        addCircle(&s, 0.3, 0.0, 0.5);
        const SketchRegionExtraction x = extractSketchRegions(s);
        r.check("CADVS_REG_16_overlapping_holes_make_the_outer_region_unselectable",
                x.regions.size() == 3u
                        && x.regions[0].holeAnchorIds == std::vector<SketchEntityId>({2u, 3u})
                        && x.regions[0].status == CadStatus::OverlappingHoles
                        && x.regions[1].status == CadStatus::Ok
                        && x.regions[2].status == CadStatus::Ok && x.loopsConflict(1, 2)
                        && validateRegionSelection(x, {regionRef(1, {2, 3})})
                                   == CadStatus::OverlappingHoles
                        && validateRegionSelection(x, {regionRef(2)}) == CadStatus::Ok
                        && validateRegionSelection(x, {regionRef(2), regionRef(3)})
                                   == CadStatus::OverlappingRegions);
    }
    {
        CadSketch s;
        addRect(&s, 0.0, 0.0, 2.0, 2.0);
        addRect(&s, 0.0, 0.0, 2.0, 2.0);
        const SketchRegionExtraction x = extractSketchRegions(s);
        r.check("CADVS_REG_17_two_coincident_rectangles_cannot_be_selected_together",
                x.regions.size() == 2u && x.loopsConflict(0, 1)
                        && x.regions[0].holeAnchorIds.empty() && x.regions[1].holeAnchorIds.empty()
                        && validateRegionSelection(x, {regionRef(1)}) == CadStatus::Ok
                        && validateRegionSelection(x, {regionRef(2)}) == CadStatus::Ok
                        && validateRegionSelection(x, {regionRef(1), regionRef(2)})
                                   == CadStatus::OverlappingRegions);
    }
    {
        CadSketch s;
        addRect(&s, 0.0, 0.0, 4.0, 3.0);
        addCircle(&s, 0.0, 0.0, 0.8);
        const SketchRegionExtraction x = extractSketchRegions(s);
        CadBodyState filled = regionHoleState();
        filled.extrude.profileHoleIds.clear();
        CadBodyState extraHole = regionHoleState();
        extraHole.extrude.profileHoleIds = {2u, 7u};
        r.check("CADVS_REG_18_stored_holes_that_differ_from_the_derived_ones_are_a_mismatch",
                validateRegionSelection(x, {regionRef(1)}) == CadStatus::ProfileRegionMismatch
                        && validateRegionSelection(x, {regionRef(1, {2, 7})})
                                   == CadStatus::ProfileRegionMismatch
                        && validateRegionSelection(x, {regionRef(1, {7})})
                                   == CadStatus::ProfileRegionMismatch
                        && validateRegionSelection(x, {regionRef(7)}) == CadStatus::ProfileNotFound
                        && validateCadBodyState(filled) == CadStatus::ProfileRegionMismatch
                        && validateCadBodyState(extraHole) == CadStatus::ProfileRegionMismatch
                        && validateCadBodyState(regionHoleState()) == CadStatus::Ok);
    }
    {
        CadSketch pair;
        addRect(&pair, -2.0, 0.0, 1.0, 1.0);
        addRect(&pair, 2.0, 0.0, 1.0, 1.0);
        const SketchRegionExtraction xp = extractSketchRegions(pair);
        CadSketch holes;
        addRect(&holes, 0.0, 0.0, 4.0, 3.0);
        addCircle(&holes, -1.0, 0.0, 0.4);
        addCircle(&holes, 1.0, 0.0, 0.4);
        const SketchRegionExtraction xh = extractSketchRegions(holes);
        CadBodyState backwards;
        cadBaseSketch(backwards) = pair;
        backwards.extrude = oneSide(1.0);
        backwards.extrude.profileEntityId = 2;
        backwards.extrude.additionalRegions = {regionRef(1)};
        r.check("CADVS_REG_19_non_canonical_order_is_a_mismatch_never_re_sorted",
                validateRegionSelection(xp, {regionRef(2), regionRef(1)})
                                == CadStatus::ProfileRegionMismatch
                        && validateRegionSelection(xp, {regionRef(1), regionRef(1)})
                                   == CadStatus::ProfileRegionMismatch
                        && validateRegionSelection(xh, {regionRef(1, {3, 2})})
                                   == CadStatus::ProfileRegionMismatch
                        && validateRegionSelection(xh, {regionRef(1, {2, 2, 3})})
                                   == CadStatus::ProfileRegionMismatch
                        && validateRegionSelection(xh, {regionRef(1, {2, 3})}) == CadStatus::Ok
                        && validateCadBodyState(backwards) == CadStatus::ProfileRegionMismatch);
    }
    {
        CadSketch s;
        addRect(&s, 0.0, 0.0, 4.0, 3.0);
        addCircle(&s, 0.0, 0.0, 0.8);
        const std::vector<ProfileRegionRef> stored = {regionRef(1, {2})};
        const bool resized = replaceSketchEntity(&s, 1, rectangleAt(0.0, 0.0, 5.0, 4.0))
                                     == CadStatus::Ok
                             && replaceSketchEntity(&s, 2, circleAt(0.25, 0.0, 1.0)) == CadStatus::Ok;
        const SketchRegionExtraction x = extractSketchRegions(s);
        r.check("CADVS_REG_20_resizing_keeps_region_identity_and_the_stored_selection",
                resized && x.regions.size() == 2u && x.regions[0].outerAnchorId == 1u
                        && x.regions[0].holeAnchorIds == std::vector<SketchEntityId>{2u}
                        && x.regions[1].outerAnchorId == 2u
                        && validateRegionSelection(x, stored) == CadStatus::Ok
                        && nearRel(x.regions[0].area, 20.0 - circleArea(1.0), 1e-12));
        const bool moved = replaceSketchEntity(&s, 2, circleAt(6.0, 0.0, 0.5)) == CadStatus::Ok;
        const SketchRegionExtraction y = extractSketchRegions(s);
        r.check("CADVS_REG_21_a_hole_moved_out_makes_the_stored_selection_a_mismatch",
                moved && validateRegionSelection(y, stored) == CadStatus::ProfileRegionMismatch
                        && validateRegionSelection(y, {regionRef(1)}) == CadStatus::Ok);
    }
    {
        // Drawn inner-first, and an entity list held in a permuted order.
        CadSketch drawn;
        addCircle(&drawn, -1.0, 0.0, 0.4);
        addRect(&drawn, 0.0, 0.0, 4.0, 3.0);
        addCircle(&drawn, 1.0, 0.0, 0.4);
        const SketchRegionExtraction xd = extractSketchRegions(drawn);
        CadSketch permuted;
        permuted.entities = {SketchEntity(3, rectangleAt(0.0, 0.0, 4.0, 3.0)),
                             SketchEntity(1, circleAt(-1.0, 0.0, 0.4)),
                             SketchEntity(2, circleAt(1.0, 0.0, 0.4))};
        permuted.nextEntityId = 4;
        const SketchRegionExtraction xq = extractSketchRegions(permuted);
        auto anchorsAscend = [](const SketchRegionExtraction& x) {
            bool ok = !x.regions.empty() && x.regions.size() == x.loops.profiles.size();
            for (size_t i = 0; ok && i < x.regions.size(); ++i) {
                ok = x.regions[i].outerAnchorId == x.loops.profiles[i].anchorEntityId
                     && (i == 0 || x.regions[i - 1].outerAnchorId < x.regions[i].outerAnchorId);
            }
            return ok;
        };
        r.check("CADVS_REG_22_regions_ascend_by_anchor_whatever_the_insertion_order",
                validateCadSketch(permuted) == CadStatus::Ok && anchorsAscend(xd) && anchorsAscend(xq)
                        && xd.regions.size() == 3u && xq.regions.size() == 3u
                        && xd.regions[1].outerAnchorId == 2u
                        && xd.regions[1].holeAnchorIds == std::vector<SketchEntityId>({1u, 3u})
                        && xq.regions[2].outerAnchorId == 3u
                        && xq.regions[2].holeAnchorIds == std::vector<SketchEntityId>({1u, 2u}));
    }
    {
        CadSketch s;
        addRect(&s, 0.0, 0.0, 4.0, 3.0);
        addCircle(&s, 0.0, 0.0, 0.8);
        const SketchRegionExtraction x = extractSketchRegions(s);
        SketchEntityId atCentre = 99;
        SketchEntityId atRing = 99;
        SketchEntityId outside = 99;
        SketchEntityId onEdge = 99;
        r.check("CADVS_REG_23_region_at_picks_the_disk_inside_and_the_ring_between",
                sketchRegionAt(x, SketchPoint{0.0, 0.0}, &atCentre) && atCentre == 2u
                        && sketchRegionAt(x, SketchPoint{1.5, 0.0}, &atRing) && atRing == 1u
                        && sketchRegionAt(x, SketchPoint{-0.3, 1.2}, &atRing) && atRing == 1u
                        && !sketchRegionAt(x, SketchPoint{5.0, 5.0}, &outside) && outside == 99u
                        && !sketchRegionAt(x, SketchPoint{2.0, 0.0}, &onEdge) && onEdge == 99u);

        bool hatchOk = x.regions.size() == 2u;
        bool splitAtZero = false;
        if (hatchOk) {
            const std::vector<std::vector<SketchPoint>> ring = sketchRegionLoops(x, x.regions[0]);
            const std::vector<SketchPoint> segments = sketchRegionHatch(x, x.regions[0], 0.25);
            hatchOk = ring.size() == 2u && !segments.empty() && segments.size() % 2u == 0u;
            int atZero = 0;
            for (size_t i = 0; hatchOk && i + 1 < segments.size(); i += 2) {
                const SketchPoint& a = segments[i];
                const SketchPoint& b = segments[i + 1];
                const SketchPoint mid{(a.u + b.u) * 0.5, a.v};
                hatchOk = a.v == b.v && a.u < b.u && sketchPointStrictlyInside(mid, ring[0])
                          && !sketchPointStrictlyInside(mid, ring[1]);
                if (a.v == 0.0) {
                    ++atZero;
                }
            }
            splitAtZero = atZero == 2;
            const std::vector<SketchPoint> disk = sketchRegionHatch(x, x.regions[1], 0.25);
            const std::vector<SketchPoint> diskPolygon = x.loops.profiles[1].polygon;
            hatchOk = hatchOk && !disk.empty();
            for (size_t i = 0; hatchOk && i + 1 < disk.size(); i += 2) {
                hatchOk = sketchPointStrictlyInside(
                        SketchPoint{(disk[i].u + disk[i + 1].u) * 0.5, disk[i].v}, diskPolygon);
            }
            hatchOk = hatchOk && sketchRegionHatch(x, x.regions[0], 0.0).empty()
                      && sketchRegionHatch(x, x.regions[0], std::nan("")).empty();
        }
        r.check("CADVS_REG_24_ring_hatch_leaves_the_hole_empty", hatchOk && splitAtZero);
    }
    {
        CadSketch s;
        addRect(&s, -2.0, 0.0, 1.0, 1.0);
        addRect(&s, 2.0, 0.0, 1.0, 1.0);
        const SketchRegionExtraction x = extractSketchRegions(s);
        bool toggles = x.regions.size() == 2u;
        if (toggles) {
            std::vector<ProfileRegionRef> sel = toggleRegionSelection({}, x.regions[1]);
            toggles = sel.size() == 1u && sel[0].outerAnchorId == 2u;
            sel = toggleRegionSelection(sel, x.regions[0]);
            toggles = toggles && sel.size() == 2u && sel[0].outerAnchorId == 1u
                      && sel[1].outerAnchorId == 2u;
            sel = toggleRegionSelection(sel, x.regions[1]);
            toggles = toggles && sel.size() == 1u && sel[0].outerAnchorId == 1u;
        }
        ExtrudeFeature e;
        setExtrudeRegions(&e, {regionRef(3), regionRef(1, {5, 2})});
        const std::vector<ProfileRegionRef> back = extrudeRegions(e);
        const bool canonical = e.profileEntityId == 1u
                               && e.profileHoleIds == std::vector<SketchEntityId>({2u, 5u})
                               && e.additionalRegions.size() == 1u
                               && e.additionalRegions[0].outerAnchorId == 3u && back.size() == 2u
                               && sameProfileRegionRef(back[0], regionRef(1, {2, 5}))
                               && !extrudeSelectsSingleSimpleProfile(e);
        ExtrudeFeature single;
        setExtrudeRegions(&single, {regionRef(4)});
        ExtrudeFeature none = e;
        setExtrudeRegions(&none, {});
        r.check("CADVS_REG_25_toggle_and_store_keep_one_canonical_selection",
                toggles && canonical && extrudeSelectsSingleSimpleProfile(single)
                        && none.profileEntityId == kNoSketchEntity && none.profileHoleIds.empty()
                        && none.additionalRegions.empty() && extrudeRegions(none).empty());
    }
    {
        SketchRegionExtraction out;
        CadBodyState unchosen = regionHoleState();
        unchosen.extrude.profileEntityId = kNoSketchEntity;
        unchosen.extrude.profileHoleIds.clear();
        CadBodyState orphanHoles = regionHoleState();
        orphanHoles.extrude.profileEntityId = kNoSketchEntity;
        r.check("CADVS_REG_26_feature_rule_accepts_the_ring_and_names_what_is_missing",
                validateCadFeatureGeometry(cadBaseSketch(regionHoleState()), regionHoleState().extrude, &out)
                                == CadStatus::Ok
                        && out.regions.size() == 2u
                        && validateCadFeatureGeometry(cadBaseSketch(unchosen), unchosen.extrude)
                                   == CadStatus::ProfileNotFound
                        && validateCadFeatureGeometry(cadBaseSketch(orphanHoles), orphanHoles.extrude)
                                   == CadStatus::ProfileRegionMismatch);
    }
}

// ---------------------------------------------------------------------------
// B. Extruding regions with holes
// ---------------------------------------------------------------------------

void testExtrusion(Recorder& r) {
    const double ringArea = 12.0 - circleArea(0.8);
    {
        const CadBodyState state = regionHoleState();
        const Regen g = regen(state);
        const CadBodyMesh& m = g.mesh;
        const bool ok = g.why == CadStatus::Ok && g.report.status == CadStatus::Ok
                        && g.report.failedFeatureId == 0u;
        r.check("CADVS_EXT_01_ring_prism_regenerates", ok && validateCadBodyState(state) == CadStatus::Ok);
        r.check("CADVS_EXT_02_ring_prism_is_watertight_every_edge_twice_opposite",
                ok && watertight(m.mesh));
        const double floatVolume = meshSignedVolume(m.mesh);
        r.check("CADVS_EXT_03_ring_prism_volume_is_exact_and_winding_outward",
                ok && m.volume > 0.0 && nearRel(m.volume, ringArea * 1.0) && floatVolume > 0.0
                        && nearRel(floatVolume, ringArea, 1e-5) && m.components == 1u
                        && meshComponents(m.mesh) == 1u);
        r.check("CADVS_EXT_04_ring_prism_is_finite_and_indexed_in_range", ok && meshFinite(m.mesh));
        bool table = ok && faceTableConsistent(m) && m.faces.size() == 38u
                     && m.faces[0].token.kind == CadFaceKind::CapPlane
                     && m.faces[1].token.kind == CadFaceKind::CapFar && m.faces[0].eligible
                     && m.faces[1].eligible;
        for (uint32_t k = 0; table && k < 4u; ++k) {
            const CadMeshFace& f = m.faces[2u + k];
            table = f.token.kind == CadFaceKind::Side && f.token.edgeEntityId == 1u
                    && f.token.edgeLocalIndex == k && f.eligible && f.featureId == kCadFeatureId;
        }
        for (uint32_t k = 0; table && k < kSketchCircleSegments; ++k) {
            const CadMeshFace& f = m.faces[6u + k];
            table = f.token.kind == CadFaceKind::Side && f.token.edgeEntityId == 2u
                    && f.token.edgeLocalIndex == k && !f.eligible;
        }
        r.check("CADVS_EXT_05_ring_face_table_is_caps_outer_sides_then_curved_hole_walls", table);
        std::vector<CadFaceRange> ranges;
        std::vector<CadFaceRange> direct;
        bool tiled = ok && cadFaceRangesFromMesh(m, &ranges) == CadStatus::Ok
                     && ranges.size() == m.faces.size()
                     && cadFaceRanges(state, &direct) == CadStatus::Ok
                     && direct.size() == ranges.size();
        uint32_t cursor = 0;
        for (size_t i = 0; tiled && i < ranges.size(); ++i) {
            tiled = ranges[i].firstIndex == cursor && ranges[i].indexCount > 0u
                    && sameCadFaceToken(ranges[i].token, m.faces[i].token)
                    && ranges[i].featureId == kCadFeatureId
                    && ranges[i].eligible == m.faces[i].eligible
                    && direct[i].firstIndex == ranges[i].firstIndex
                    && direct[i].indexCount == ranges[i].indexCount;
            cursor += ranges[i].indexCount;
        }
        r.check("CADVS_EXT_06_ring_face_ranges_tile_the_index_buffer_one_per_face",
                tiled && cursor == m.mesh.indices.size());
        float lo[3];
        float hi[3];
        meshBounds(m.mesh, lo, hi);
        r.check("CADVS_EXT_07_ring_prism_bounds_are_the_rectangle_and_the_depth",
                ok && nearf(lo[0], -2.0f) && nearf(hi[0], 2.0f) && nearf(lo[1], -1.5f)
                        && nearf(hi[1], 1.5f) && nearf(lo[2], 0.0f) && nearf(hi[2], 1.0f)
                        && faceAtZ(m, 0u, 0.0f) && faceAtZ(m, 1u, 1.0f));
        // Walls: an outer wall faces away from the axis, a hole wall into it.
        bool walls = ok && faceTableConsistent(m);
        for (size_t t = 0; walls && t < m.triangleFace.size(); ++t) {
            const uint32_t tag = m.triangleFace[t];
            if (tag < 2u) {
                continue;
            }
            const Vec3 a = vertexAt(m.mesh, m.mesh.indices[t * 3u]);
            const Vec3 b = vertexAt(m.mesh, m.mesh.indices[t * 3u + 1u]);
            const Vec3 c = vertexAt(m.mesh, m.mesh.indices[t * 3u + 2u]);
            const Vec3 n = vec3Cross(vec3Sub(b, a), vec3Sub(c, a));
            const float radial = n.x * (a.x + b.x + c.x) + n.y * (a.y + b.y + c.y);
            walls = tag < 6u ? radial > 0.0f : radial < 0.0f;
        }
        r.check("CADVS_EXT_08_outer_walls_face_out_and_hole_walls_face_into_the_hole", walls);
    }
    {
        struct SpanCase {
            ExtrudeFeature extrude;
            float lo;
            float hi;
            float planeCap;
            float farCap;
        };
        const SpanCase cases[3] = {
                {symmetricExtent(0.5), -0.5f, 0.5f, -0.5f, 0.5f},
                {twoSidesExtent(0.75, 0.25), -0.25f, 0.75f, -0.25f, 0.75f},
                {oneSide(1.0, ExtrudeDirection::AgainstNormal), -1.0f, 0.0f, 0.0f, -1.0f}};
        bool spans[3] = {false, false, false};
        for (int c = 0; c < 3; ++c) {
            const Regen g = regen(regionHoleState(cases[c].extrude));
            float lo[3];
            float hi[3];
            meshBounds(g.mesh.mesh, lo, hi);
            spans[c] = solidOk(g, ringArea * static_cast<double>(cases[c].hi - cases[c].lo))
                       && nearf(lo[2], cases[c].lo) && nearf(hi[2], cases[c].hi)
                       && faceAtZ(g.mesh, 0u, cases[c].planeCap)
                       && faceAtZ(g.mesh, 1u, cases[c].farCap);
        }
        r.check("CADVS_EXT_09_symmetric_ring_spans_minus_to_plus_the_distance", spans[0]);
        r.check("CADVS_EXT_10_two_sides_ring_spans_minus_b_to_plus_a", spans[1]);
        r.check("CADVS_EXT_11_one_side_against_ring_spans_minus_depth_to_the_plane", spans[2]);
    }
    {
        // Every body any earlier version made: R0's mesh, rebuilt by hand.
        const CadBodyState legacy = blockState(1.0);
        const Regen g = regen(legacy);
        const std::vector<SketchPoint> polygon = rectangleProfilePolygon(rectangleAt(0.0, 0.0, 2.0, 2.0));
        std::vector<uint32_t> cap;
        const bool capOk = triangulateSimplePolygon(polygon, &cap) == CadStatus::Ok && cap.size() == 6u;
        const uint32_t n = 4u;
        std::vector<uint32_t> indices;
        for (size_t t = 0; capOk && t < cap.size(); t += 3) {
            indices.insert(indices.end(), {n + cap[t], n + cap[t + 1], n + cap[t + 2]});
        }
        for (size_t t = 0; capOk && t < cap.size(); t += 3) {
            indices.insert(indices.end(), {cap[t], cap[t + 2], cap[t + 1]});
        }
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t j = (i + 1u) % n;
            indices.insert(indices.end(), {i, j, n + j, i, n + j, n + i});
        }
        bool r0 = capOk && g.why == CadStatus::Ok && g.mesh.mesh.vertices.size() == 8u
                  && g.mesh.mesh.indices.size() == 36u
                  && g.mesh.mesh.indices.size() == cadExtrusionIndexCount(4)
                  && g.mesh.mesh.indices == indices;
        for (uint32_t i = 0; r0 && i < n; ++i) {
            const Vec3 lower = vertexAt(g.mesh.mesh, i);
            const Vec3 upper = vertexAt(g.mesh.mesh, n + i);
            r0 = lower.x == static_cast<float>(polygon[i].u) && lower.y == static_cast<float>(polygon[i].v)
                 && lower.z == 0.0f && upper.x == lower.x && upper.y == lower.y && upper.z == 1.0f;
        }
        r.check("CADVS_EXT_12_legacy_rectangle_is_r0s_mesh_8_vertices_36_indices", r0);
        ConstructionMesh generated;
        r.check("CADVS_EXT_13_generate_cad_mesh_is_the_regeneration_without_its_table",
                generateCadMesh(legacy, &generated) == CadStatus::Ok
                        && sameMeshBits(generated, g.mesh.mesh));
        std::vector<CadFaceRange> ranges;
        const bool rangesOk = g.why == CadStatus::Ok
                              && cadFaceRangesFromMesh(g.mesh, &ranges) == CadStatus::Ok
                              && ranges.size() == 6u;
        const CadFaceKind kinds[6] = {CadFaceKind::CapFar, CadFaceKind::CapPlane, CadFaceKind::Side,
                                      CadFaceKind::Side, CadFaceKind::Side, CadFaceKind::Side};
        bool r0Ranges = rangesOk
                        && g.mesh.triangleFace
                                   == std::vector<uint32_t>({1, 1, 0, 0, 2, 2, 3, 3, 4, 4, 5, 5})
                        && nearRel(g.mesh.volume, 4.0) && g.mesh.components == 1u;
        for (uint32_t i = 0; r0Ranges && i < 6u; ++i) {
            r0Ranges = ranges[i].firstIndex == i * 6u && ranges[i].indexCount == 6u
                       && ranges[i].token.kind == kinds[i] && ranges[i].eligible
                       && (i < 2u || (ranges[i].token.edgeEntityId == 1u
                                      && ranges[i].token.edgeLocalIndex == i - 2u));
        }
        r.check("CADVS_EXT_14_legacy_rectangle_face_ranges_are_r0s", r0Ranges);
    }
    {
        // The disk of a nested sketch is a single region without holes: R0's
        // path, bit for bit the cylinder a lone circle makes.
        CadBodyState disk = regionHoleState();
        setExtrudeRegions(&disk.extrude, {regionRef(2)});
        CadBodyState lone;
        addCircle(&cadBaseSketch(lone), 0.0, 0.0, 0.8);
        lone.extrude = oneSide(1.0);
        const Regen gd = regen(disk);
        const Regen gl = regen(lone);
        r.check("CADVS_EXT_15_disk_of_a_nested_sketch_is_the_lone_cylinder_bit_for_bit",
                gd.why == CadStatus::Ok && gl.why == CadStatus::Ok
                        && sameMeshBits(gd.mesh.mesh, gl.mesh.mesh)
                        && gd.mesh.triangleFace == gl.mesh.triangleFace
                        && gd.mesh.mesh.vertices.size() == cadExtrusionVertexCount(32)
                        && nearRel(gd.mesh.volume, circleArea(0.8), 1e-12));
    }
    {
        CadBodyState two;
        addRect(&cadBaseSketch(two), -2.0, 0.0, 1.0, 1.0);
        addRect(&cadBaseSketch(two), 2.0, 0.0, 1.0, 1.0);
        two.extrude = oneSide(1.0);
        setExtrudeRegions(&two.extrude, {regionRef(1), regionRef(2)});
        const Regen g = regen(two);
        bool sides = g.why == CadStatus::Ok && g.mesh.faces.size() == 10u;
        for (uint32_t k = 0; sides && k < 4u; ++k) {
            sides = g.mesh.faces[2u + k].token.edgeEntityId == 1u
                    && g.mesh.faces[6u + k].token.edgeEntityId == 2u;
        }
        r.check("CADVS_EXT_16_two_disjoint_regions_extrude_as_two_closed_components",
                sides && g.mesh.components == 2u && meshComponents(g.mesh.mesh) == 2u
                        && watertight(g.mesh.mesh) && nearRel(g.mesh.volume, 2.0)
                        && meshSignedVolume(g.mesh.mesh) > 0.0 && faceTableConsistent(g.mesh));
    }
}

// ---------------------------------------------------------------------------
// C. Operations and the retained chain
// ---------------------------------------------------------------------------

void testChain(Recorder& r) {
    const CadBodyState base = blockState();
    const CadBodyState add = featureAddState();
    const CadBodyState cut = featureCutState();
    const double pocket = circleArea(0.3);
    {
        const Regen g = regen(add);
        float lo[3];
        float hi[3];
        meshBounds(g.mesh.mesh, lo, hi);
        r.check("CADVS_OPS_01_add_on_the_far_cap_unions_one_shell_of_4_32",
                validateCadBodyState(add) == CadStatus::Ok && solidOk(g, 4.0 + 0.32)
                        && nearf(hi[2], 1.5f) && nearf(lo[2], 0.0f) && g.mesh.faces.size() == 12u
                        && g.mesh.faces[6].featureId == 2u);
        const uint32_t baseFar = capIndex(g.mesh, kCadFeatureId, CadFaceKind::CapFar);
        const uint32_t bossFar = capIndex(g.mesh, 2u, CadFaceKind::CapFar);
        const uint32_t bossPlane = capIndex(g.mesh, 2u, CadFaceKind::CapPlane);
        std::vector<CadFaceRange> ranges;
        bool bossRange = cadFaceRangesFromMesh(g.mesh, &ranges) == CadStatus::Ok;
        bool found = false;
        for (const CadFaceRange& range : ranges) {
            found |= range.featureId == 2u && range.token.kind == CadFaceKind::CapFar
                     && range.eligible;
        }
        r.check("CADVS_OPS_02_add_keeps_both_top_faces_and_merges_away_its_flush_bottom",
                g.why == CadStatus::Ok && baseFar < g.mesh.faces.size()
                        && bossFar < g.mesh.faces.size() && bossPlane < g.mesh.faces.size()
                        && faceAtZ(g.mesh, baseFar, 1.0f) && faceAtZ(g.mesh, bossFar, 1.5f)
                        && !meshHasFace(g.mesh, bossPlane) && bossRange && found);
    }
    {
        const Regen g = regen(cut);
        float lo[3];
        float hi[3];
        meshBounds(g.mesh.mesh, lo, hi);
        const uint32_t floor = capIndex(g.mesh, 2u, CadFaceKind::CapFar);
        const uint32_t mouth = capIndex(g.mesh, 2u, CadFaceKind::CapPlane);
        bool pocketFaces = g.why == CadStatus::Ok && g.mesh.faces.size() == 6u + 34u;
        for (const CadMeshFace& f : g.mesh.faces) {
            pocketFaces = pocketFaces && (f.featureId != 2u || !f.eligible);
        }
        r.check("CADVS_OPS_03_blind_cut_removes_exactly_the_pocket",
                solidOk(g, 4.0 - pocket * 0.5) && nearf(lo[2], 0.0f) && nearf(hi[2], 1.0f));
        r.check("CADVS_OPS_04_cut_leaves_an_ineligible_floor_and_an_open_mouth",
                pocketFaces && floor < g.mesh.faces.size() && mouth < g.mesh.faces.size()
                        && faceAtZ(g.mesh, floor, 0.5f) && !meshHasFace(g.mesh, mouth));
    }
    {
        // Add, then a Cut standing on the Add's own far cap.
        const CadBodyState chain = withFeature(add, CadFeatureOperation::Cut, 2u,
                                               circleSketch(0.0, 0.0, 0.3),
                                               oneSide(0.25, ExtrudeDirection::AgainstNormal));
        const Regen g = regen(chain);
        float lo[3];
        float hi[3];
        meshBounds(g.mesh.mesh, lo, hi);
        const uint32_t floor = capIndex(g.mesh, 3u, CadFaceKind::CapFar);
        r.check("CADVS_OPS_05_add_then_cut_on_the_add_regenerates_in_order",
                validateCadBodyState(chain) == CadStatus::Ok
                        && solidOk(g, 4.0 + 0.32 - pocket * 0.25) && nearf(hi[2], 1.5f)
                        && g.mesh.faces.size() == 6u + 6u + 34u && floor < g.mesh.faces.size()
                        && faceAtZ(g.mesh, floor, 1.25f));
    }
    r.check("CADVS_OPS_06_add_wholly_inside_is_refused_add_no_effect",
            refusedBy(withFeature(base, CadFeatureOperation::Add, 1u, rectSketch(0.0, 0.0, 0.8, 0.8),
                                  oneSide(0.5, ExtrudeDirection::AgainstNormal)),
                      CadStatus::AddNoEffect, 2u));
    r.check("CADVS_OPS_07_add_away_from_the_body_is_refused_add_disjoint_by_feature_2",
            refusedBy(withFeature(base, CadFeatureOperation::Add, 1u, rectSketch(5.0, 0.0, 0.8, 0.8),
                                  oneSide(0.5)),
                      CadStatus::AddDisjoint, 2u));
    r.check("CADVS_OPS_08_cut_entirely_outside_is_refused_cut_no_intersection",
            refusedBy(withFeature(base, CadFeatureOperation::Cut, 1u, circleSketch(5.0, 0.0, 0.3),
                                  oneSide(0.5, ExtrudeDirection::AgainstNormal)),
                      CadStatus::CutNoIntersection, 2u));
    r.check("CADVS_OPS_09_cut_through_everything_is_refused_cut_removes_body",
            refusedBy(withFeature(base, CadFeatureOperation::Cut, 1u, rectSketch(0.0, 0.0, 3.0, 3.0),
                                  oneSide(2.0, ExtrudeDirection::AgainstNormal)),
                      CadStatus::CutRemovesBody, 2u));
    {
        CadBodyState newBody = add;
        newBody.laterFeatures[0].operation = CadFeatureOperation::NewBody;
        CadBodyState unknown = add;
        unknown.laterFeatures[0].operation = static_cast<CadFeatureOperation>(7);
        r.check("CADVS_OPS_10_new_body_or_an_unknown_code_as_a_later_feature_is_refused",
                validateCadBodyState(newBody) == CadStatus::InvalidFeatureOperation
                        && refusedBy(newBody, CadStatus::InvalidFeatureOperation, 2u)
                        && validateCadBodyState(unknown) == CadStatus::InvalidFeatureOperation);
    }
    {
        CadBodyState seven = add;
        laterRecord(seven, 0).featureSupport.featureId = 7;
        CadBodyState itself = add;
        laterRecord(itself, 0).featureSupport.featureId = 2;
        r.check("CADVS_OPS_11_support_naming_no_earlier_feature_is_refused",
                validateCadBodyState(seven) == CadStatus::FeatureSupportInvalid
                        && refusedBy(seven, CadStatus::FeatureSupportInvalid, 2u)
                        && validateCadBodyState(itself) == CadStatus::FeatureSupportInvalid);
    }
    {
        CadBodyState stale = add;
        laterRecord(stale, 0).featureSupport.lineageToken ^= 1u;
        r.check("CADVS_OPS_12_stale_lineage_is_refused_never_retargeted",
                validateCadBodyState(stale) == CadStatus::FeatureSupportInvalid
                        && refusedBy(stale, CadStatus::FeatureSupportInvalid, 2u));
    }
    {
        CadBodyState noSuchFace = add;
        laterRecord(noSuchFace, 0).featureSupport.face = CadFaceToken{CadFaceKind::Side, 9u, 0u};
        CadBodyState cylinder;
        addCircle(&cadBaseSketch(cylinder), 0.0, 0.0, 1.0);
        cylinder.extrude = oneSide(1.0);
        CadBodyState onCurve = withFeature(cylinder, CadFeatureOperation::Add, 1u,
                                           rectSketch(0.0, 0.0, 0.1, 0.1), oneSide(0.1));
        laterRecord(onCurve, 0).featureSupport.face = CadFaceToken{CadFaceKind::Side, 1u, 0u};
        r.check("CADVS_OPS_13_unknown_face_and_curved_side_supports_are_refused",
                validateCadBodyState(noSuchFace) == CadStatus::FeatureSupportInvalid
                        && validateCadBodyState(onCurve) == CadStatus::FeatureSupportInvalid);
    }
    {
        CadBodyState tilted = add;
        laterRecord(tilted, 0).sketch.plane = Workplane::XZ;
        CadBodyState crossBody = add;
        laterRecord(crossBody, 0).sketch.hasFaceSupport = true;
        laterRecord(crossBody, 0).sketch.faceSupport.producerObjectId = 5;
        r.check("CADVS_OPS_14_a_later_sketch_is_canonical_xy_with_no_topo_ref",
                validateCadBodyState(tilted) == CadStatus::FeatureSupportInvalid
                        && validateCadBodyState(crossBody) == CadStatus::FeatureSupportInvalid);
    }
    {
        const CadBodyState onPocket = withFeature(cut, CadFeatureOperation::Add, 2u,
                                                  rectSketch(0.0, 0.0, 0.1, 0.1), oneSide(0.1),
                                                  CadFaceKind::CapPlane);
        std::vector<CadFace> cutFaces;
        bool noneEligible = enumerateCadFeatureFaces(cut, 2u, &cutFaces) == CadStatus::Ok
                            && cutFaces.size() == 34u;
        for (const CadFace& f : cutFaces) {
            noneEligible = noneEligible && !f.eligible;
        }
        r.check("CADVS_OPS_15_a_sketch_on_a_cut_face_is_refused_the_pocket_is_ineligible",
                noneEligible && validateCadBodyState(onPocket) == CadStatus::FeatureSupportInvalid
                        && refusedBy(onPocket, CadStatus::FeatureSupportInvalid, 3u));
    }
    {
        // Feature 2 cuts the whole top half away, so feature 1's far cap no
        // longer carries material where feature 3 stands.
        const CadBodyState halfCut = withFeature(base, CadFeatureOperation::Cut, 1u,
                                                 rectSketch(0.0, 0.0, 3.0, 3.0),
                                                 oneSide(0.5, ExtrudeDirection::AgainstNormal));
        const CadBodyState lost = withFeature(halfCut, CadFeatureOperation::Add, 1u,
                                              rectSketch(0.0, 0.0, 0.5, 0.5), oneSide(0.5));
        const Regen half = regen(halfCut);
        const Regen boss = regen(add);
        CadFace far;
        const bool farOk = resolveCadFeatureFace(base, kCadFeatureId,
                                                 CadFaceToken{CadFaceKind::CapFar, 0u, 0u}, &far)
                           == CadStatus::Ok;
        r.check("CADVS_OPS_16_a_support_face_an_earlier_cut_carved_away_is_support_face_lost",
                solidOk(half, 2.0) && validateCadBodyState(lost) == CadStatus::Ok
                        && refusedBy(lost, CadStatus::SupportFaceLost, 3u));
        r.check("CADVS_OPS_17_mesh_carries_face_answers_the_same_question_on_the_float_mesh",
                farOk && boss.why == CadStatus::Ok && cadMeshCarriesFace(boss.mesh, far)
                        && half.why == CadStatus::Ok && !cadMeshCarriesFace(half.mesh, far));
    }
    {
        CadBodyState deeper = add;
        deeper.extrude.depth = 2.0;
        CadBodyState wider = add;
        const bool widened = replaceSketchEntity(&cadBaseSketch(wider), 1, rectangleAt(0.0, 0.0, 3.0, 3.0))
                             == CadStatus::Ok;
        const Regen g = regen(deeper);
        float lo[3];
        float hi[3];
        meshBounds(g.mesh.mesh, lo, hi);
        std::vector<CadFace> bossFaces;
        const bool framed = enumerateCadFeatureFaces(deeper, 2u, &bossFaces) == CadStatus::Ok
                            && bossFaces.size() == 6u && nearf(bossFaces[1].origin.z, 2.5f);
        r.check("CADVS_OPS_18_a_depth_edit_upstream_carries_the_add_to_the_new_cap",
                cadFeatureTopologySignature(deeper, 1u) == cadFeatureTopologySignature(add, 1u)
                        && solidOk(g, 8.0 + 0.32) && nearf(hi[2], 2.5f) && framed);
        r.check("CADVS_OPS_19_a_size_edit_upstream_keeps_the_support_attached",
                widened && validateCadBodyState(wider) == CadStatus::Ok
                        && solidOk(regen(wider), 9.0 + 0.32));
    }
    {
        CadBodyState reshaped = add;
        const bool replaced = replaceSketchEntity(&cadBaseSketch(reshaped), 1, circleAt(0.0, 0.0, 1.0))
                              == CadStatus::Ok;
        CadBody body(ObjectId{42});
        const bool applied = body.applyState(add) == CadStatus::Ok;
        const CadStatus refused = body.applyState(reshaped);
        r.check("CADVS_OPS_20_a_structural_edit_upstream_refuses_the_dependent_feature",
                replaced && cadFeatureTopologySignature(reshaped, 1u)
                                    != cadFeatureTopologySignature(add, 1u)
                        && validateCadBodyState(reshaped) == CadStatus::FeatureSupportInvalid
                        && refusedBy(reshaped, CadStatus::FeatureSupportInvalid, 2u) && applied
                        && refused == CadStatus::FeatureSupportInvalid
                        && sameCadBodyState(body.state(), add));
    }
    {
        const Regen a = regen(featureChainState());
        const Regen b = regen(featureChainState());
        r.check("CADVS_OPS_21_regenerating_twice_is_bit_identical_mesh_and_face_table",
                a.why == CadStatus::Ok && b.why == CadStatus::Ok && sameBodyMesh(a.mesh, b.mesh));
    }
    {
        CadBody body(ObjectId{77});
        bool changed = false;
        const bool applied = body.applyState(add, &changed) == CadStatus::Ok && changed;
        std::shared_ptr<const CadBodyMesh> before;
        const bool cached = body.regenerated(&before) == CadStatus::Ok && before != nullptr;
        const uint64_t updates = body.updateCount();
        const uint64_t rejected = body.rejectedUpdateCount();
        CadBodyState lump = add;
        laterRecord(lump, 0).sketch = rectSketch(5.0, 0.0, 0.8, 0.8);
        bool lumpChanged = true;
        const CadStatus why = body.applyState(lump, &lumpChanged);
        std::shared_ptr<const CadBodyMesh> after;
        const bool stillCached = body.regenerated(&after) == CadStatus::Ok;
        r.check("CADVS_OPS_22_apply_state_of_a_failing_chain_changes_neither_state_nor_cache",
                applied && cached && why == CadStatus::AddDisjoint && !lumpChanged
                        && sameCadBodyState(body.state(), add) && stillCached
                        && after.get() == before.get() && body.updateCount() == updates
                        && body.rejectedUpdateCount() == rejected + 1u);
        bool sameChanged = true;
        r.check("CADVS_OPS_23_an_identical_request_is_ok_unchanged_and_uncounted",
                body.applyState(add, &sameChanged) == CadStatus::Ok && !sameChanged
                        && body.updateCount() == updates);
        const CadBodyState captured = body.captureState();
        body.restoreState(base);
        std::shared_ptr<const CadBodyMesh> baseMesh;
        const bool restoredBase = sameCadBodyState(body.state(), base)
                                  && body.regenerated(&baseMesh) == CadStatus::Ok
                                  && nearRel(baseMesh->volume, 4.0);
        body.restoreState(captured);
        std::shared_ptr<const CadBodyMesh> again;
        const bool restoredAdd = body.regenerated(&again) == CadStatus::Ok && before != nullptr
                                 && sameBodyMesh(*again, *before);
        r.check("CADVS_OPS_24_capture_and_restore_are_bit_exact_and_count_nothing",
                sameCadBodyState(captured, add) && restoredBase
                        && sameCadBodyState(body.state(), add) && restoredAdd
                        && body.updateCount() == updates);
    }
    {
        CadBodyState crowded = add;
        while (cadFeatureCount(crowded) <= kMaxCadFeatures) {
            CadFeature extra = crowded.laterFeatures[0];
            extra.featureId = crowded.nextFeatureId++;
            crowded.laterFeatures.push_back(extra);
        }
        r.check("CADVS_OPS_25_more_than_the_feature_bound_is_too_many_features",
                cadFeatureCount(crowded) == kMaxCadFeatures + 1u
                        && validateCadBodyState(crowded) == CadStatus::TooManyFeatures
                        && regen(crowded).why == CadStatus::TooManyFeatures);
        CadBodyState repeated = add;
        repeated.laterFeatures.push_back(repeated.laterFeatures[0]);
        CadBodyState baseId = add;
        baseId.laterFeatures[0].featureId = kCadFeatureId;
        r.check("CADVS_OPS_26_feature_ids_not_strictly_ascending_are_refused",
                validateCadBodyState(repeated) == CadStatus::TooManyFeatures
                        && validateCadBodyState(baseId) == CadStatus::TooManyFeatures);
    }
    {
        // Exactly the bound: a tower of fifteen flush Adds, each on the one below.
        CadBodyState tower = base;
        for (uint32_t k = 2; k <= kMaxCadFeatures; ++k) {
            tower = withFeature(tower, CadFeatureOperation::Add, k - 1u, rectSketch(0.0, 0.0, 0.8, 0.8),
                                oneSide(0.125));
        }
        const Regen g = regen(tower);
        float lo[3];
        float hi[3];
        meshBounds(g.mesh.mesh, lo, hi);
        r.check("CADVS_OPS_27_a_chain_of_exactly_the_bound_regenerates",
                cadFeatureCount(tower) == kMaxCadFeatures
                        && validateCadBodyState(tower) == CadStatus::Ok
                        && solidOk(g, 4.0 + 15.0 * 0.64 * 0.125) && nearf(hi[2], 1.0f + 15.0f * 0.125f));
    }
    {
        std::vector<CadFace> bossFaces;
        bool boss = enumerateCadFeatureFaces(add, 2u, &bossFaces) == CadStatus::Ok
                    && bossFaces.size() == 6u;
        for (const CadFace& f : bossFaces) {
            boss = boss && f.eligible;
        }
        boss = boss && nearf(bossFaces[0].origin.z, 1.0f) && nearf(bossFaces[0].n.z, -1.0f)
               && nearf(bossFaces[1].origin.z, 1.5f) && nearf(bossFaces[1].n.z, 1.0f);
        r.check("CADVS_OPS_28_an_add_exposes_eligible_faces_placed_by_its_support", boss);
        r.check("CADVS_OPS_29_the_base_signature_is_the_legacy_one_and_ignores_later_features",
                cadFeatureTopologySignature(add, 1u) == cadTopologySignature(base)
                        && cadTopologySignature(add) == cadTopologySignature(base)
                        && cadFeatureTopologySignature(add, 2u) != 0u
                        && cadFeatureTopologySignature(add, 7u) == 0u);
    }
}

// ---------------------------------------------------------------------------
// D. The sketch session: regions, operations, commit, edit
// ---------------------------------------------------------------------------

// One session, one camera framed on it, real Down/Move/Up samples.
struct SessionDriver {
    static constexpr int kW = 1000;
    static constexpr int kH = 1000;
    SketchSession sketch;
    CameraController camera;

    void frameCamera() {
        camera.setViewport(kW, kH);
        const SketchFrame& f = sketch.frame();
        camera.frameSketchView(f.origin, f.u, f.v, f.n);
    }
    bool beginWorld(Workplane plane) {
        if (sketch.begin(plane) != CadStatus::Ok) {
            return false;
        }
        frameCamera();
        return true;
    }
    bool beginFace(const SketchFrame& frame, const TopoRef& ref, const CadBodyState* producer) {
        if (sketch.beginOnFace(frame, ref, producer) != CadStatus::Ok) {
            return false;
        }
        frameCamera();
        return true;
    }
    bool at(TouchAction action, float x, float y) {
        TouchPointer p{7, x, y};
        return sketch.onTouch(action, action == TouchAction::Up ? 7 : -1, &p, 1, camera.snapshot(),
                              kW, kH);
    }
    bool screenOf(const SketchPoint& point, float* x, float* y) {
        return sketch.sketchToScreen(camera.snapshot(), point, kW, kH, x, y);
    }
    bool drag(SketchTool tool, const SketchPoint& from, const SketchPoint& to) {
        sketch.setTool(tool);
        float x0, y0, x1, y1;
        if (!screenOf(from, &x0, &y0) || !screenOf(to, &x1, &y1)) {
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
    // Draws one entity through the gesture path, then TYPES its exact
    // geometry -- a typed value is never snapped -- so the case is exact.
    SketchEntityId place(SketchTool tool, const SketchPoint& from, const SketchPoint& to,
                         SketchEntity::Payload exact) {
        const size_t before = sketch.sketch().entities.size();
        if (!drag(tool, from, to) || sketch.sketch().entities.size() != before + 1u) {
            return kNoSketchEntity;
        }
        const SketchEntityId id = sketch.sketch().entities.back().id();
        return sketch.replaceEntity(id, std::move(exact)) == CadStatus::Ok ? id : kNoSketchEntity;
    }
    bool toggleAt(const SketchPoint& point) {
        float x, y;
        return screenOf(point, &x, &y) && sketch.toggleRegionAt(camera.snapshot(), x, y, kW, kH);
    }
};

// A CAD body's feature cap resolved into WORLD space, and the reference a
// sketch placed on it records -- exactly what the shell hands the session.
bool worldCapFrame(const ConstructionScene& scene, ObjectId bodyId, uint32_t featureId,
                   CadFaceKind kind, SketchFrame* out, TopoRef* outRef) {
    const SceneObject* object = scene.findBody(bodyId);
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr) {
        return false;
    }
    CadFaceToken token;
    token.kind = kind;
    CadFace face;
    Mat4 world;
    if (resolveCadFeatureFace(body->state(), featureId, token, &face) != CadStatus::Ok
        || !scene.resolveWorldModel(bodyId, &world)) {
        return false;
    }
    const Mat4 faceWorld = mat4Multiply(world, cadFaceFrameMatrix(face));
    out->origin = mat4TransformPoint(faceWorld, Vec3{0.0f, 0.0f, 0.0f});
    out->u = vec3Normalize(mat4TransformDirection(faceWorld, Vec3{1.0f, 0.0f, 0.0f}));
    out->v = vec3Normalize(mat4TransformDirection(faceWorld, Vec3{0.0f, 1.0f, 0.0f}));
    out->n = vec3Normalize(mat4TransformDirection(faceWorld, Vec3{0.0f, 0.0f, 1.0f}));
    outRef->producerObjectId = bodyId;
    outRef->producerLocalFeatureId = featureId;
    outRef->face = token;
    outRef->lineageToken = cadFeatureTopologySignature(body->state(), featureId);
    return true;
}

CadBodyState bodyStateOf(const ConstructionScene& scene, ObjectId id) {
    const SceneObject* object = scene.findBody(id);
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    return body != nullptr ? body->state() : CadBodyState{};
}

double bodyVolumeOf(const ConstructionScene& scene, ObjectId id) {
    const SceneObject* object = scene.findBody(id);
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    std::shared_ptr<const CadBodyMesh> mesh;
    if (body == nullptr || body->regenerated(&mesh) != CadStatus::Ok || mesh == nullptr) {
        return -1.0;
    }
    return mesh->volume;
}

// ---------------------------------------------------------------------------
// `CAD-FOUNDATION-C1`: a selection is the UNION of its atomic regions
// ---------------------------------------------------------------------------

// The owner's case: rectangle O (id 1) holding two disjoint circles A (id 2)
// and B (id 3). Atomic regions: O minus A minus B, disk A, disk B.
CadSketch unionSketch() {
    CadSketch sketch;
    addRect(&sketch, 0.0, 0.0, 4.0, 3.0);
    addCircle(&sketch, -1.0, 0.0, 0.4);
    addCircle(&sketch, 1.0, 0.0, 0.4);
    return sketch;
}

std::vector<ProfileRegionRef> unionSelection(bool o, bool a, bool b) {
    std::vector<ProfileRegionRef> selection;
    if (o) selection.push_back(regionRef(1, {2, 3}));
    if (a) selection.push_back(regionRef(2));
    if (b) selection.push_back(regionRef(3));
    return selection;
}

CadBodyState unionState(bool o, bool a, bool b, double depth = 1.0) {
    CadBodyState state;
    cadBaseSketch(state) = unionSketch();
    state.extrude = oneSide(depth);
    setExtrudeRegions(&state.extrude, unionSelection(o, a, b));
    return state;
}

bool componentIs(const SketchRegionComponent& c, SketchEntityId outer,
                 std::vector<SketchEntityId> holes) {
    return c.outerAnchorId == outer && c.holeAnchorIds == holes
           && c.holeLoops.size() == c.holeAnchorIds.size();
}

// Whether any face of the body is a wall of the loop anchored at `entity`.
bool bodyHasSideOf(const CadBodyMesh& m, SketchEntityId entity) {
    for (const CadMeshFace& face : m.faces) {
        if (face.token.kind == CadFaceKind::Side && face.token.edgeEntityId == entity) {
            return true;
        }
    }
    return false;
}

void testRegionUnion(Recorder& r) {
    const double disk = circleArea(0.4);
    const CadSketch sketch = unionSketch();
    const SketchRegionExtraction x = extractSketchRegions(sketch);
    const bool shaped = x.regions.size() == 3u && x.parent[0] == -1 && x.parent[1] == 0
                        && x.parent[2] == 0
                        && x.regions[0].holeAnchorIds == std::vector<SketchEntityId>({2u, 3u});
    r.check("CADFC1_REG_00_rectangle_and_two_circles_are_three_atomic_regions", shaped);

    // The whole truth table: every non-empty subset validates, and merges into
    // exactly the canonical components, in ascending outer-anchor order.
    struct Row {
        const char* name;
        bool o, a, b;
        std::vector<std::pair<SketchEntityId, std::vector<SketchEntityId>>> expect;
        double area;
    };
    const Row rows[] = {
            {"CADFC1_REG_01_O_is_the_rectangle_with_holes_A_and_B", true, false, false,
             {{1u, {2u, 3u}}}, 12.0 - 2.0 * disk},
            {"CADFC1_REG_02_A_is_disk_A", false, true, false, {{2u, {}}}, disk},
            {"CADFC1_REG_03_B_is_disk_B", false, false, true, {{3u, {}}}, disk},
            {"CADFC1_REG_04_O_plus_A_is_the_rectangle_with_only_hole_B", true, true, false,
             {{1u, {3u}}}, 12.0 - disk},
            {"CADFC1_REG_05_O_plus_B_is_the_rectangle_with_only_hole_A", true, false, true,
             {{1u, {2u}}}, 12.0 - disk},
            {"CADFC1_REG_06_A_plus_B_is_two_disjoint_disks", false, true, true,
             {{2u, {}}, {3u, {}}}, 2.0 * disk},
            {"CADFC1_REG_07_O_plus_A_plus_B_is_the_solid_rectangle", true, true, true,
             {{1u, {}}}, 12.0},
    };
    for (const Row& row : rows) {
        const std::vector<ProfileRegionRef> selection = unionSelection(row.o, row.a, row.b);
        const std::vector<SketchRegionComponent> merged = mergeSelectedRegions(x, selection);
        bool same = shaped && validateRegionSelection(x, selection) == CadStatus::Ok
                    && merged.size() == row.expect.size();
        double area = 0.0;
        for (size_t i = 0; same && i < merged.size(); ++i) {
            same = componentIs(merged[i], row.expect[i].first, row.expect[i].second);
            area += merged[i].area;
        }
        r.check(row.name, same && nearRel(area, row.area, 1e-12));
    }

    // Geometry: the merged components are what is extruded -- exact volumes,
    // one closed shell per component, and no wall left on an absorbed loop.
    {
        const Regen oa = regen(unionState(true, true, false));
        const Regen ob = regen(unionState(true, false, true));
        const Regen all = regen(unionState(true, true, true));
        const Regen ab = regen(unionState(false, true, true));
        const Regen o = regen(unionState(true, false, false));
        r.check("CADFC1_GEO_01_O_plus_A_extrudes_to_the_exact_volume_with_one_hole",
                solidOk(oa, 12.0 - disk) && !bodyHasSideOf(oa.mesh, 2u)
                        && bodyHasSideOf(oa.mesh, 3u) && oa.mesh.faces.size() == 2u + 4u + 32u);
        r.check("CADFC1_GEO_02_O_plus_B_extrudes_to_the_exact_volume_with_one_hole",
                solidOk(ob, 12.0 - disk) && !bodyHasSideOf(ob.mesh, 3u)
                        && bodyHasSideOf(ob.mesh, 2u) && ob.mesh.faces.size() == 2u + 4u + 32u);
        r.check("CADFC1_GEO_03_all_three_extrude_to_the_solid_rectangle_volume",
                solidOk(all, 12.0) && !bodyHasSideOf(all.mesh, 2u) && !bodyHasSideOf(all.mesh, 3u)
                        && all.mesh.faces.size() == 2u + 4u);
        r.check("CADFC1_GEO_04_A_plus_B_stays_two_disjoint_components_in_one_body",
                ab.why == CadStatus::Ok && ab.mesh.components == 2u
                        && nearRel(ab.mesh.volume, 2.0 * disk) && watertight(ab.mesh.mesh)
                        && meshComponents(ab.mesh.mesh) == 2u);
        r.check("CADFC1_GEO_05_O_alone_is_unchanged_the_ring_with_two_holes",
                solidOk(o, 12.0 - 2.0 * disk) && bodyHasSideOf(o.mesh, 2u)
                        && bodyHasSideOf(o.mesh, 3u));
        // The kernel sees ONE closed shell for a merged component: no internal
        // double wall that a later boolean would have to reconcile.
        CadFeatureGeometry g;
        CadSolid solid;
        CadSolidMeasure measure;
        const CadBodyState oaState = unionState(true, true, false);
        const bool built = buildCadFeatureGeometry(oaState, kCadFeatureId, &g) == CadStatus::Ok
                           && appendCadFeatureSolid(g, 0u, &solid) == CadStatus::Ok;
        r.check("CADFC1_GEO_06_a_merged_component_is_one_kernel_valid_shell_with_no_shared_wall",
                built && g.components.size() == 1u && g.chosen.size() == 2u
                        && cadKernelValidateSolid(solid, &measure) == CadKernelStatus::Ok
                        && measure.components == 1u && nearRel(measure.volume, 12.0 - disk, 1e-9));
    }

    // Persistence: the durable form is unchanged -- the atomic list -- and it
    // still refuses by name when the nesting under it changes.
    {
        CadBodyState edited = unionState(true, true, false);
        addCircle(&cadBaseSketch(edited), 0.0, 1.0, 0.3);  // a new loop inside O
        r.check("CADFC1_PER_01_a_nesting_edit_under_a_union_selection_is_ProfileRegionMismatch",
                validateCadBodyState(unionState(true, true, false)) == CadStatus::Ok
                        && validateCadBodyState(edited) == CadStatus::ProfileRegionMismatch);
        const CadBodyState stored = unionState(true, true, false);
        r.check("CADFC1_PER_02_the_stored_selection_is_the_atomic_list_not_a_merged_boundary",
                stored.extrude.profileEntityId == 1u
                        && stored.extrude.profileHoleIds == std::vector<SketchEntityId>({2u, 3u})
                        && stored.extrude.additionalRegions.size() == 1u
                        && stored.extrude.additionalRegions[0].outerAnchorId == 2u
                        && stored.extrude.additionalRegions[0].holeAnchorIds.empty());
    }

    // Touching and crossing loops are still refused by the existing rule: a
    // circle straddling the rectangle's edge is its own region, and choosing
    // both cannot be merged without guessing.
    {
        CadSketch crossing;
        addRect(&crossing, 0.0, 0.0, 4.0, 3.0);
        addCircle(&crossing, 2.0, 0.0, 0.5);
        const SketchRegionExtraction cx = extractSketchRegions(crossing);
        r.check("CADFC1_REG_08_touching_or_crossing_loops_stay_refused_OverlappingRegions",
                cx.regions.size() == 2u && cx.loopsConflict(0, 1)
                        && validateRegionSelection(cx, {regionRef(1), regionRef(2)})
                                   == CadStatus::OverlappingRegions);
    }

    // Add and Cut read the SAME merged semantics: a later feature whose sketch
    // is a square around a circle, with the square AND the disk chosen, adds or
    // removes the whole square.
    {
        CadSketch tool;
        addRect(&tool, 0.0, 0.0, 0.8, 0.8);
        addCircle(&tool, 0.0, 0.0, 0.2);
        ExtrudeFeature merged = oneSide(0.5);
        setExtrudeRegions(&merged, {regionRef(1, {2}), regionRef(2)});
        ExtrudeFeature ringOnly = oneSide(0.5);
        setExtrudeRegions(&ringOnly, {regionRef(1, {2})});
        ExtrudeFeature mergedCut = oneSide(0.5, ExtrudeDirection::AgainstNormal);
        setExtrudeRegions(&mergedCut, {regionRef(1, {2}), regionRef(2)});
        const Regen add = regen(withFeature(blockState(), CadFeatureOperation::Add, kCadFeatureId,
                                            tool, merged));
        const Regen addRing = regen(withFeature(blockState(), CadFeatureOperation::Add,
                                                kCadFeatureId, tool, ringOnly));
        const Regen cut = regen(withFeature(blockState(), CadFeatureOperation::Cut, kCadFeatureId,
                                            tool, mergedCut));
        r.check("CADFC1_OPS_01_a_merged_selection_feeds_Add_the_whole_square",
                solidOk(add, 4.0 + 0.64 * 0.5)
                        && solidOk(addRing, 4.0 + (0.64 - circleArea(0.2)) * 0.5));
        r.check("CADFC1_OPS_02_a_merged_selection_feeds_Cut_the_whole_square",
                cut.why == CadStatus::Ok && nearRel(cut.mesh.volume, 4.0 - 0.64 * 0.5, 1e-9));
    }

    // The session: nothing is guessed, every tap toggles exactly the region
    // under the finger, the preview IS the commit, and reopening the sketch
    // finds the same atomic selection.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        SessionDriver s;
        const bool began = s.beginWorld(Workplane::XY);
        const SketchEntityId o =
                began ? s.place(SketchTool::Rectangle, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0},
                                rectangleAt(0.0, 0.0, 4.0, 3.0))
                      : kNoSketchEntity;
        const SketchEntityId a = s.place(SketchTool::Circle, SketchPoint{-1.0, 0.0},
                                         SketchPoint{-0.6, 0.0}, circleAt(-1.0, 0.0, 0.4));
        const SketchEntityId b = s.place(SketchTool::Circle, SketchPoint{1.0, 0.0},
                                         SketchPoint{1.4, 0.0}, circleAt(1.0, 0.0, 0.4));
        const bool finished = s.sketch.finish() == CadStatus::Ok;
        r.check("CADFC1_SES_01_three_regions_and_nothing_chosen_when_ambiguous",
                o == 1u && a == 2u && b == 3u && finished
                        && s.sketch.regions().regions.size() == 3u
                        && s.sketch.extrude().profileEntityId == kNoSketchEntity);
        const bool tapO = s.toggleAt(SketchPoint{0.0, 1.0});
        const bool onlyO = tapO && s.sketch.regionSelected(o) && !s.sketch.regionSelected(a)
                           && !s.sketch.regionSelected(b);
        const bool tapA = s.toggleAt(SketchPoint{-1.0, 0.0});
        const bool oPlusA = tapA && s.sketch.regionSelected(o) && s.sketch.regionSelected(a)
                            && !s.sketch.regionSelected(b);
        r.check("CADFC1_SES_02_tap_O_then_tap_A_keeps_O_and_adds_A",
                onlyO && oPlusA
                        && s.sketch.extrude().profileHoleIds == std::vector<SketchEntityId>({a, b}));
        const CadCandidateEvaluation preview = s.sketch.evaluateCandidate();
        r.check("CADFC1_SES_03_the_preview_is_the_rectangle_with_only_hole_B",
                preview.valid && preview.status == CadStatus::Ok && preview.mesh != nullptr
                        && preview.mesh->components == 1u
                        && nearRel(preview.mesh->volume, 12.0 - disk)
                        && !bodyHasSideOf(*preview.mesh, a) && bodyHasSideOf(*preview.mesh, b));
        // J3: a tap on A again removes A and ONLY A.
        const bool tapAOff = s.toggleAt(SketchPoint{-1.0, 0.0});
        const bool backToO = tapAOff && s.sketch.regionSelected(o) && !s.sketch.regionSelected(a);
        const bool tapAOn = s.toggleAt(SketchPoint{-1.0, 0.0});
        r.check("CADFC1_SES_04_deselect_then_reselect_changes_only_the_tapped_region",
                backToO && tapAOn && s.sketch.regionSelected(o) && s.sketch.regionSelected(a));
        const SketchFrame frame = s.sketch.frame();
        const CadCandidateEvaluation before = s.sketch.evaluateCandidate();
        ObjectId id = kNoObject;
        const bool committed = s.sketch.commit(scene, history, &id) == CadStatus::Ok;
        const CadBodyState stored = committed ? bodyStateOf(scene, id) : CadBodyState{};
        const Regen again = regen(stored);
        r.check("CADFC1_SES_05_commit_is_the_previewed_union_in_one_body_one_step",
                committed && before.mesh != nullptr && again.why == CadStatus::Ok
                        && sameBodyMesh(*before.mesh, again.mesh) && history.undoDepth() == 1u
                        && nearRel(bodyVolumeOf(scene, id), 12.0 - disk)
                        && stored.extrude.additionalRegions.size() == 1u
                        && stored.extrude.additionalRegions[0].outerAnchorId == a);
        SessionDriver reopened;
        const bool reopenedOk = committed
                                && reopened.sketch.beginEdit(id, stored, frame) == CadStatus::Ok
                                && reopened.sketch.finish() == CadStatus::Ok;
        r.check("CADFC1_SES_06_reopening_the_sketch_keeps_O_plus_A",
                reopenedOk && reopened.sketch.regionSelected(o) && reopened.sketch.regionSelected(a)
                        && !reopened.sketch.regionSelected(b));
        reopened.sketch.cancel();
    }
    // A tap that cannot be merged is refused BY NAME and drops nothing.
    {
        SessionDriver s;
        const bool began = s.beginWorld(Workplane::XY);
        const SketchEntityId rect =
                began ? s.place(SketchTool::Rectangle, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0},
                                rectangleAt(0.0, 0.0, 4.0, 3.0))
                      : kNoSketchEntity;
        const SketchEntityId straddle = s.place(SketchTool::Circle, SketchPoint{2.0, 0.0},
                                                SketchPoint{2.5, 0.0}, circleAt(2.0, 0.0, 0.5));
        const bool finished = s.sketch.finish() == CadStatus::Ok;
        const CadStatus first = s.sketch.toggleRegion(rect);
        const CadStatus second = s.sketch.toggleRegion(straddle);
        // `CAD-V6-S2`: a circle straddling the rectangle's edge CUTS both, so
        // the sketch now selects planar faces, and a WHOLE split loop names no
        // face: choosing either by its anchor is refused by name, nothing
        // chosen. (The unmergeable planar tap -- a pinch -- is CADV6S2_SES_*.)
        r.check("CADFC1_SES_07_S2_a_straddling_circle_puts_the_sketch_on_faces_split_loops_refuse",
                finished && s.sketch.selectionKind() == CadSelectionKind::PlanarFaces
                        && s.sketch.planarFaceCount() == 3u
                        && first == CadStatus::PlanarFaceUnresolved
                        && second == CadStatus::PlanarFaceUnresolved
                        && s.sketch.lastStatus() == CadStatus::PlanarFaceUnresolved
                        && s.sketch.selectedAreaCount() == 0u);
        s.sketch.cancel();
    }
}

void testSession(Recorder& r) {
    const double ringArea = 12.0 - circleArea(0.8);
    const double pocket = circleArea(0.3);
    ConstructionScene scene;  // a live project: the default Box
    ConstructionHistory history(scene);
    const size_t bodiesBefore = scene.bodyCount();
    const ObjectId nextIdBefore = scene.nextObjectId();

    // --- a world-plane sketch whose loops enclose two regions ---------------
    SessionDriver world;
    const bool began = world.beginWorld(Workplane::XY);
    const SketchEntityId rectId =
            began ? world.place(SketchTool::Rectangle, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0},
                                rectangleAt(0.0, 0.0, 4.0, 3.0))
                  : kNoSketchEntity;
    const SketchEntityId circleId =
            rectId != kNoSketchEntity
                    ? world.place(SketchTool::Circle, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 0.0},
                                  circleAt(0.0, 0.0, 0.8))
                    : kNoSketchEntity;
    const CadStatus finished = world.sketch.finish();
    r.check("CADVS_SES_01_rectangle_and_circle_drawn_on_xy_finish_ready",
            began && rectId == 1u && circleId == 2u && finished == CadStatus::Ok
                    && world.sketch.state() == SketchSessionState::Ready);
    r.check("CADVS_SES_02_two_regions_and_nothing_chosen_for_the_user",
            world.sketch.regions().regions.size() == 2u
                    && world.sketch.extrude().profileEntityId == kNoSketchEntity
                    && world.sketch.extrude().profileHoleIds.empty()
                    && world.sketch.extrude().additionalRegions.empty()
                    && !world.sketch.regionSelected(1) && !world.sketch.regionSelected(2));
    ObjectId refusedId = 123;
    const CadStatus ambiguous = world.sketch.commit(scene, history, &refusedId);
    const CadCandidateEvaluation unchosen = world.sketch.evaluateCandidate();
    r.check("CADVS_SES_03_commit_and_evaluation_refuse_ambiguous_profile_and_mint_nothing",
            ambiguous == CadStatus::AmbiguousProfile && refusedId == kNoObject && unchosen.valid
                    && unchosen.status == CadStatus::AmbiguousProfile && unchosen.mesh == nullptr
                    && scene.bodyCount() == bodiesBefore && scene.nextObjectId() == nextIdBefore
                    && history.undoDepth() == 0u
                    && world.sketch.state() == SketchSessionState::Ready);
    const CadStatus pickRing = world.sketch.toggleRegion(rectId);
    r.check("CADVS_SES_04_toggling_the_rectangle_selects_the_ring_with_its_hole",
            pickRing == CadStatus::Ok && world.sketch.extrude().profileEntityId == rectId
                    && world.sketch.extrude().profileHoleIds == std::vector<SketchEntityId>{circleId}
                    && world.sketch.extrude().additionalRegions.empty()
                    && world.sketch.regionSelected(rectId) && !world.sketch.regionSelected(circleId));
    // `CAD-FOUNDATION-C1`: the tap is a PURE toggle. The disk joins the ring
    // and the ring stays; the candidate is their union, the solid rectangle.
    const CadStatus pickDisk = world.sketch.toggleRegion(circleId);
    const CadCandidateEvaluation unionEval = world.sketch.evaluateCandidate();
    r.check("CADVS_SES_05_toggling_the_circle_adds_the_disk_and_keeps_the_ring",
            pickDisk == CadStatus::Ok && world.sketch.extrude().profileEntityId == rectId
                    && world.sketch.extrude().profileHoleIds == std::vector<SketchEntityId>{circleId}
                    && world.sketch.extrude().additionalRegions.size() == 1u
                    && world.sketch.extrude().additionalRegions[0].outerAnchorId == circleId
                    && world.sketch.regionSelected(rectId) && world.sketch.regionSelected(circleId)
                    && unionEval.valid && unionEval.status == CadStatus::Ok
                    && unionEval.mesh != nullptr && unionEval.mesh->components == 1u
                    && nearRel(unionEval.mesh->volume, 12.0));
    const CadStatus dropDisk = world.sketch.toggleRegion(circleId);
    const bool ringOnly = dropDisk == CadStatus::Ok
                          && world.sketch.extrude().profileEntityId == rectId
                          && world.sketch.extrude().additionalRegions.empty()
                          && world.sketch.regionSelected(rectId)
                          && !world.sketch.regionSelected(circleId);
    const CadStatus drop = world.sketch.toggleRegion(rectId);
    const bool cleared = ringOnly && drop == CadStatus::Ok
                         && world.sketch.extrude().profileEntityId == kNoSketchEntity;
    const bool tappedRing = world.toggleAt(SketchPoint{1.5, 0.0});
    r.check("CADVS_SES_06_toggling_off_then_tapping_the_ring_in_the_canvas_selects_it",
            cleared && tappedRing && world.sketch.extrude().profileEntityId == rectId
                    && world.sketch.extrude().profileHoleIds == std::vector<SketchEntityId>{circleId});
    const bool missed = !world.toggleAt(SketchPoint{3.0, 0.0});
    r.check("CADVS_SES_07_a_tap_outside_every_region_changes_nothing",
            missed && world.sketch.extrude().profileEntityId == rectId);
    const CadCandidateEvaluation first = world.sketch.evaluateCandidate();
    const CadCandidateEvaluation second = world.sketch.evaluateCandidate();
    r.check("CADVS_SES_08_an_unchanged_candidate_is_evaluated_once_same_mesh",
            first.valid && first.status == CadStatus::Ok && first.mesh != nullptr
                    && second.mesh.get() == first.mesh.get() && second.revision == first.revision
                    && first.revision == world.sketch.candidateRevision()
                    && first.operation == CadFeatureOperation::NewBody
                    && first.targetBodyId == kNoObject && nearRel(first.mesh->volume, ringArea));
    const CadStatus deeper = world.sketch.setExtrude(1.25, ExtrudeDirection::AlongNormal);
    const CadCandidateEvaluation third = world.sketch.evaluateCandidate();
    r.check("CADVS_SES_09_a_changed_candidate_gets_a_new_revision_and_a_new_mesh",
            deeper == CadStatus::Ok && third.revision > first.revision && third.mesh != nullptr
                    && third.mesh.get() != first.mesh.get()
                    && nearRel(third.mesh->volume, ringArea * 1.25));
    const bool newBodyOnly = world.sketch.operationAvailable(CadFeatureOperation::NewBody)
                             && !world.sketch.operationAvailable(CadFeatureOperation::Add)
                             && !world.sketch.operationAvailable(CadFeatureOperation::Cut);
    const CadStatus wantAdd = world.sketch.setOperation(CadFeatureOperation::Add);
    const CadStatus wantCut = world.sketch.setOperation(CadFeatureOperation::Cut);
    r.check("CADVS_SES_10_a_world_plane_sketch_offers_new_body_only",
            newBodyOnly && wantAdd == CadStatus::OperationNeedsTarget
                    && wantCut == CadStatus::OperationNeedsTarget
                    && world.sketch.operation() == CadFeatureOperation::NewBody
                    && world.sketch.operationTargetId() == kNoObject);
    const CadStatus depthBack = world.sketch.setExtrude(1.0, ExtrudeDirection::AlongNormal);
    ObjectId bodyId = kNoObject;
    const CadStatus created = world.sketch.commit(scene, history, &bodyId);
    const SceneObject* body = scene.findBody(bodyId);
    r.check("CADVS_SES_11_commit_new_body_creates_one_body_in_one_undo_step",
            depthBack == CadStatus::Ok && created == CadStatus::Ok && bodyId != kNoObject
                    && body != nullptr && body->isCad() && scene.bodyCount() == bodiesBefore + 1u
                    && history.undoDepth() == 1u && !world.sketch.active()
                    && scene.activeBodyId() == bodyId);
    r.check("CADVS_SES_12_the_committed_body_is_exactly_the_authored_ring",
            sameCadBodyState(bodyStateOf(scene, bodyId), regionHoleState()));
    if (body == nullptr) {
        return;  // every later case needs the body; their absence is itself the failure
    }

    // --- Add on the body's own far cap ---------------------------------------
    SketchFrame capFrame;
    TopoRef capRef;
    const bool framed = worldCapFrame(scene, bodyId, kCadFeatureId, CadFaceKind::CapFar, &capFrame,
                                      &capRef);
    const CadBodyState ringState = bodyStateOf(scene, bodyId);
    const TransformValues placementBefore = body->transform().values();
    const MeshRevision revisionBefore = body->meshStore().currentRevision();
    const size_t undoBeforeAdd = history.undoDepth();
    {
        SessionDriver bare;
        const bool bareBegan = framed && bare.beginFace(capFrame, capRef, nullptr);
        const bool offersNewBodyOnly = bareBegan
                                       && bare.sketch.operationAvailable(CadFeatureOperation::NewBody)
                                       && !bare.sketch.operationAvailable(CadFeatureOperation::Add)
                                       && bare.sketch.setOperation(CadFeatureOperation::Add)
                                                  == CadStatus::OperationNeedsTarget;
        bare.sketch.cancel();
        r.check("CADVS_SES_13_a_face_sketch_without_a_staged_producer_cannot_add",
                offersNewBodyOnly && !bare.sketch.active());
    }
    SessionDriver add;
    const bool addBegan = framed && add.beginFace(capFrame, capRef, &ringState);
    r.check("CADVS_SES_14_a_face_sketch_on_a_cad_body_offers_new_body_add_and_cut",
            addBegan && nearf(capFrame.origin.z, 1.0f) && nearf(capFrame.n.z, 1.0f)
                    && add.sketch.operationAvailable(CadFeatureOperation::NewBody)
                    && add.sketch.operationAvailable(CadFeatureOperation::Add)
                    && add.sketch.operationAvailable(CadFeatureOperation::Cut)
                    && add.sketch.operationTargetId() == kNoObject);
    const SketchEntityId boss =
            addBegan ? add.place(SketchTool::Rectangle, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0},
                                 rectangleAt(1.4, 0.0, 0.8, 0.8))
                     : kNoSketchEntity;
    const bool addReady = boss == 1u && add.sketch.finish() == CadStatus::Ok
                          && add.sketch.extrude().profileEntityId == 1u
                          && add.sketch.setOperation(CadFeatureOperation::Add) == CadStatus::Ok
                          && add.sketch.setExtrude(0.5, ExtrudeDirection::AlongNormal) == CadStatus::Ok;
    const CadCandidateEvaluation addPreview = add.sketch.evaluateCandidate();
    r.check("CADVS_SES_15_the_add_candidate_targets_the_producer_and_previews_the_union",
            addReady && add.sketch.operationTargetId() == bodyId
                    && addPreview.status == CadStatus::Ok && addPreview.targetBodyId == bodyId
                    && addPreview.operation == CadFeatureOperation::Add && addPreview.mesh != nullptr
                    && nearRel(addPreview.mesh->volume, ringArea + 0.32)
                    && addPreview.mesh->components == 1u
                    && add.sketch.candidateState().laterFeatures.size() == 1u);
    ObjectId addOut = kNoObject;
    const CadStatus addCommitted =
            addReady ? add.sketch.commit(scene, history, &addOut) : CadStatus::NotSketching;
    body = scene.findBody(bodyId);
    const CadBodyState afterAdd = bodyStateOf(scene, bodyId);
    CadBodyState baseOfAdd = afterAdd;
    baseOfAdd.laterFeatures.clear();
    baseOfAdd.sketches.resize(1);
    baseOfAdd.nextSketchId = kBaseCadSketchId + 1u;
    baseOfAdd.nextFeatureId = kCadFeatureId + 1u;
    const bool addShape = afterAdd.laterFeatures.size() == 1u
                          && afterAdd.laterFeatures[0].featureId == 2u
                          && afterAdd.laterFeatures[0].operation == CadFeatureOperation::Add
                          && laterRecord(afterAdd, 0).featureSupport.featureId == kCadFeatureId
                          && laterRecord(afterAdd, 0).featureSupport.face.kind == CadFaceKind::CapFar
                          && laterRecord(afterAdd, 0).featureSupport.lineageToken == capRef.lineageToken
                          && laterRecord(afterAdd, 0).sketch.plane == Workplane::XY
                          && !laterRecord(afterAdd, 0).sketch.hasFaceSupport
                          && afterAdd.laterFeatures[0].extrude.depth == 0.5
                          && sameCadBodyState(baseOfAdd, ringState);
    r.check("CADVS_SES_16_the_add_commit_grows_the_same_body_in_place",
            addCommitted == CadStatus::Ok && addOut == bodyId
                    && scene.bodyCount() == bodiesBefore + 1u && addShape && !add.sketch.active());
    r.check("CADVS_SES_17_the_add_keeps_the_placement_costs_one_undo_and_republishes",
            body != nullptr && sameConstructionPlacement(body->transform().values(), placementBefore)
                    && history.undoDepth() == undoBeforeAdd + 1u
                    && body->meshStore().currentRevision() != revisionBefore
                    && nearRel(bodyVolumeOf(scene, bodyId), ringArea + 0.32));
    const bool undone = history.undo();
    const bool undoRestored = undone && sameCadBodyState(bodyStateOf(scene, bodyId), ringState)
                              && scene.bodyCount() == bodiesBefore + 1u
                              && nearRel(bodyVolumeOf(scene, bodyId), ringArea);
    r.check("CADVS_SES_18_undo_restores_the_base_only_body_exactly", undoRestored);
    const bool redone = history.redo();
    r.check("CADVS_SES_19_redo_restores_the_add_exactly",
            redone && sameCadBodyState(bodyStateOf(scene, bodyId), afterAdd)
                    && nearRel(bodyVolumeOf(scene, bodyId), ringArea + 0.32));

    // --- Cut on the same cap, beside the hole on the other side --------------
    SketchFrame cutFrame;
    TopoRef cutRef;
    const bool cutFramed = worldCapFrame(scene, bodyId, kCadFeatureId, CadFaceKind::CapFar,
                                         &cutFrame, &cutRef);
    SessionDriver cut;
    const bool cutBegan = cutFramed && cut.beginFace(cutFrame, cutRef, &afterAdd);
    const SketchEntityId hole =
            cutBegan ? cut.place(SketchTool::Circle, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 0.0},
                                 circleAt(-1.4, 0.0, 0.3))
                     : kNoSketchEntity;
    const bool cutChosen = hole == 1u && cut.sketch.finish() == CadStatus::Ok
                           && cut.sketch.extrude().direction == ExtrudeDirection::AlongNormal
                           && cut.sketch.setOperation(CadFeatureOperation::Cut) == CadStatus::Ok;
    // A NEW Cut on a face starts pointing INTO the body (face normals point
    // out), through the one writer: a direction, never a negative depth.
    const bool cutPointsIn = cutChosen
                             && cut.sketch.extrude().direction == ExtrudeDirection::AgainstNormal
                             && cut.sketch.extrude().depth > 0.0
                             && cut.sketch.extrude().extent == ExtrudeExtentMode::OneSide;
    const bool addPointsOutAgain =
            cutPointsIn && cut.sketch.setOperation(CadFeatureOperation::Add) == CadStatus::Ok
            && cut.sketch.extrude().direction == ExtrudeDirection::AlongNormal
            && cut.sketch.setOperation(CadFeatureOperation::Cut) == CadStatus::Ok
            && cut.sketch.extrude().direction == ExtrudeDirection::AgainstNormal;
    r.check("CADVS_SES_31_choosing_cut_on_a_face_points_the_extrusion_into_the_body",
            cutPointsIn && addPointsOutAgain);
    const bool cutReady = cutChosen && cut.sketch.setExtrude(0.5, cut.sketch.extrude().direction)
                                               == CadStatus::Ok
                          && cut.sketch.extrude().direction == ExtrudeDirection::AgainstNormal;
    const size_t undoBeforeCut = history.undoDepth();
    ObjectId cutOut = kNoObject;
    const CadStatus cutCommitted =
            cutReady ? cut.sketch.commit(scene, history, &cutOut) : CadStatus::NotSketching;
    const CadBodyState afterCut = bodyStateOf(scene, bodyId);
    r.check("CADVS_SES_20_the_cut_commit_appends_feature_3_to_the_same_body",
            cutFramed && cutRef.lineageToken == capRef.lineageToken
                    && cutCommitted == CadStatus::Ok && cutOut == bodyId
                    && afterCut.laterFeatures.size() == 2u
                    && afterCut.laterFeatures[1].featureId == 3u
                    && afterCut.laterFeatures[1].operation == CadFeatureOperation::Cut
                    && afterCut.laterFeatures[1].extrude.direction == ExtrudeDirection::AgainstNormal
                    && sameCadFeature(afterCut.laterFeatures[0], afterAdd.laterFeatures[0])
                    && history.undoDepth() == undoBeforeCut + 1u
                    && scene.bodyCount() == bodiesBefore + 1u);
    r.check("CADVS_SES_21_the_cut_body_regenerates_to_the_expected_volume",
            nearRel(bodyVolumeOf(scene, bodyId), ringArea + 0.32 - pocket * 0.5));

    // --- an Add over the hole touches nothing: refused, nothing moves -------
    {
        SketchFrame frame;
        TopoRef ref;
        const bool lumpFramed = worldCapFrame(scene, bodyId, kCadFeatureId, CadFaceKind::CapFar,
                                              &frame, &ref);
        const SceneConstructionState sceneBefore = captureSceneConstructionState(scene);
        const size_t undoBefore = history.undoDepth();
        const size_t redoBefore = history.redoDepth();
        SessionDriver lump;
        const bool lumpBegan = lumpFramed && lump.beginFace(frame, ref, &afterCut);
        const SketchEntityId over =
                lumpBegan ? lump.place(SketchTool::Rectangle, SketchPoint{0.0, 0.0},
                                       SketchPoint{1.0, 1.0}, rectangleAt(0.0, 0.0, 0.8, 0.8))
                          : kNoSketchEntity;
        const bool lumpReady = over == 1u && lump.sketch.finish() == CadStatus::Ok
                               && lump.sketch.setOperation(CadFeatureOperation::Add) == CadStatus::Ok
                               && lump.sketch.setExtrude(0.5, ExtrudeDirection::AlongNormal)
                                          == CadStatus::Ok;
        const CadCandidateEvaluation preview = lump.sketch.evaluateCandidate();
        ObjectId lumpOut = 77;
        const CadStatus why = lumpReady ? lump.sketch.commit(scene, history, &lumpOut)
                                        : CadStatus::NotSketching;
        r.check("CADVS_SES_22_an_add_standing_over_the_hole_is_previewed_as_disjoint",
                lumpReady && preview.status == CadStatus::AddDisjoint
                        && preview.failedFeatureId == 4u && preview.mesh == nullptr);
        r.check("CADVS_SES_23_the_disjoint_add_commit_is_refused_and_changes_nothing",
                why == CadStatus::AddDisjoint && lumpOut == kNoObject
                        && lump.sketch.state() == SketchSessionState::Ready
                        && lump.sketch.lastStatus() == CadStatus::AddDisjoint
                        && sameSceneConstructionState(captureSceneConstructionState(scene),
                                                      sceneBefore)
                        && sameCadBodyState(bodyStateOf(scene, bodyId), afterCut)
                        && history.undoDepth() == undoBefore && history.redoDepth() == redoBefore
                        && scene.bodyCount() == bodiesBefore + 1u);
        lump.sketch.cancel();
        r.check("CADVS_SES_24_cancel_after_a_refusal_costs_the_project_nothing",
                !lump.sketch.active()
                        && sameSceneConstructionState(captureSceneConstructionState(scene),
                                                      sceneBefore)
                        && history.undoDepth() == undoBefore);
    }

    // --- edit the Add feature in place ---------------------------------------
    {
        SketchFrame frame;
        TopoRef ref;
        const bool editFramed = worldCapFrame(scene, bodyId, kCadFeatureId, CadFaceKind::CapFar,
                                              &frame, &ref);
        const CadBodyState beforeEdit = bodyStateOf(scene, bodyId);
        SessionDriver edit;
        const CadStatus missing = edit.sketch.beginEditFeature(bodyId, beforeEdit, 9u, frame, true);
        const bool missingRefused = missing == CadStatus::ProfileNotFound && !edit.sketch.active();
        const CadStatus opened = edit.sketch.beginEditFeature(bodyId, beforeEdit, 2u, frame, true);
        edit.frameCamera();
        r.check("CADVS_SES_25_edit_feature_opens_ready_on_the_staged_add",
                editFramed && missingRefused && opened == CadStatus::Ok
                        && edit.sketch.state() == SketchSessionState::Ready
                        && edit.sketch.editingFeatureId() == 2u
                        && edit.sketch.editingBodyId() == bodyId
                        && edit.sketch.operation() == CadFeatureOperation::Add
                        && !edit.sketch.operationAvailable(CadFeatureOperation::NewBody)
                        && edit.sketch.operationAvailable(CadFeatureOperation::Add)
                        && edit.sketch.operationAvailable(CadFeatureOperation::Cut)
                        && edit.sketch.operationTargetId() == bodyId
                        && edit.sketch.extrude().depth == 0.5
                        && edit.sketch.setOperation(CadFeatureOperation::NewBody)
                                   == CadStatus::InvalidFeatureOperation);
        const size_t undoBeforeEdit = history.undoDepth();
        const CadStatus deepen = edit.sketch.setExtrude(0.75, ExtrudeDirection::AlongNormal);
        const CadStatus editCommitted = edit.sketch.commitEdit(scene, history);
        const CadBodyState afterEdit = bodyStateOf(scene, bodyId);
        r.check("CADVS_SES_26_commit_edit_rewrites_feature_2_in_the_same_body_as_one_step",
                deepen == CadStatus::Ok && editCommitted == CadStatus::Ok && !edit.sketch.active()
                        && scene.bodyCount() == bodiesBefore + 1u
                        && afterEdit.laterFeatures.size() == 2u
                        && afterEdit.laterFeatures[0].featureId == 2u
                        && afterEdit.laterFeatures[0].extrude.depth == 0.75
                        && sameCadFeature(afterEdit.laterFeatures[1], beforeEdit.laterFeatures[1])
                        && history.undoDepth() == undoBeforeEdit + 1u
                        && nearRel(bodyVolumeOf(scene, bodyId), ringArea + 0.64 * 0.75 - pocket * 0.5));
        const bool undoneEdit = history.undo();
        r.check("CADVS_SES_27_undo_of_the_edit_restores_the_previous_chain",
                undoneEdit && sameCadBodyState(bodyStateOf(scene, bodyId), beforeEdit));
        const bool redoneEdit = history.redo();
        r.check("CADVS_SES_28_redo_of_the_edit_restores_the_edited_chain",
                redoneEdit && sameCadBodyState(bodyStateOf(scene, bodyId), afterEdit));
    }

    // --- a structural edit of the base the Add stands on is refused ----------
    {
        const CadBodyState beforeBaseEdit = bodyStateOf(scene, bodyId);
        const SceneConstructionState sceneBefore = captureSceneConstructionState(scene);
        const size_t undoBefore = history.undoDepth();
        const WorkplaneFrame xy = workplaneFrame(Workplane::XY);
        const SketchFrame baseFrame{Vec3{0.0f, 0.0f, 0.0f}, xy.uAxis, xy.vAxis, xy.normal};
        SessionDriver baseEdit;
        const CadStatus opened =
                baseEdit.sketch.beginEditFeature(bodyId, beforeBaseEdit, kCadFeatureId, baseFrame, false);
        baseEdit.frameCamera();
        const bool newBodyOnly = opened == CadStatus::Ok
                                 && baseEdit.sketch.operationAvailable(CadFeatureOperation::NewBody)
                                 && !baseEdit.sketch.operationAvailable(CadFeatureOperation::Add)
                                 && !baseEdit.sketch.operationAvailable(CadFeatureOperation::Cut);
        const bool reshaped = baseEdit.sketch.replaceEntity(1, circleAt(0.0, 0.0, 2.5)) == CadStatus::Ok
                              && baseEdit.sketch.finish() == CadStatus::Ok
                              && baseEdit.sketch.extrude().profileEntityId == 1u;
        const CadStatus why = baseEdit.sketch.commitEdit(scene, history);
        r.check("CADVS_SES_29_editing_the_base_offers_new_body_only",
                newBodyOnly && baseEdit.sketch.editingFeatureId() == kCadFeatureId);
        r.check("CADVS_SES_30_a_structural_base_edit_under_a_later_feature_is_refused",
                reshaped && why == CadStatus::FeatureSupportInvalid
                        && baseEdit.sketch.state() == SketchSessionState::Ready
                        && sameCadBodyState(bodyStateOf(scene, bodyId), beforeBaseEdit)
                        && sameSceneConstructionState(captureSceneConstructionState(scene),
                                                      sceneBefore)
                        && history.undoDepth() == undoBefore);
        baseEdit.sketch.cancel();
    }
}

// ---------------------------------------------------------------------------
// E. Persistence: `CADB` v5
// ---------------------------------------------------------------------------

// A one-body document exactly as `forgeshape_cad_selftest.cpp`'s
// `extentDocumentFor` builds one: through a scene, so the identity, the
// placement and the allocator are what the product captures.
ProjectDocument cadDocumentFor(const CadBodyState& state) {
    ConstructionScene scene((NoProjectTag()));
    CadStatus why = CadStatus::Ok;
    SceneObject* object = scene.addCadBody(state, &why);
    if (object == nullptr) {
        return ProjectDocument{};
    }
    publishSceneObject(*object);
    return captureProjectDocument(scene, ProjectKind::Construction);
}

bool cadbSectionVersion(const std::vector<uint8_t>& bytes, uint16_t* out) {
    for (size_t i = 0; i + 8 <= bytes.size(); ++i) {
        if (bytes[i] == 'C' && bytes[i + 1] == 'A' && bytes[i + 2] == 'D' && bytes[i + 3] == 'B') {
            *out = static_cast<uint16_t>(bytes[i + 4] | (bytes[i + 5] << 8));
            return true;
        }
    }
    return false;
}

// Where the `CADB` payload starts, and the CRC repair that makes a
// byte-patched file well-formed again -- so only the SEMANTIC check can refuse
// it. The same two helpers the CAD suite uses for its corrupt fixtures.
size_t cadPayloadOffset(const std::vector<uint8_t>& bytes) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            return offset + kForgeSectionHeaderBytes;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
    return 0;
}

void repairCadPayloadCrc(std::vector<uint8_t>* bytes) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes->size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &(*bytes)[offset + 8], 8);
        if (std::memcmp(&(*bytes)[offset], kSectionTagCad, 4) == 0) {
            const uint32_t crc = crc32IsoHdlc(&(*bytes)[offset + kForgeSectionHeaderBytes],
                                              static_cast<size_t>(payloadBytes));
            std::memcpy(&(*bytes)[offset + 16], &crc, 4);
            return;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
}

uint32_t u32At(const std::vector<uint8_t>& bytes, size_t at) {
    uint32_t value = 0xFFFFFFFFu;
    if (at + 4u <= bytes.size()) {
        std::memcpy(&value, &bytes[at], 4);
    }
    return value;
}

uint64_t u64At(const std::vector<uint8_t>& bytes, size_t at) {
    uint64_t value = ~0ull;
    if (at + 8u <= bytes.size()) {
        std::memcpy(&value, &bytes[at], 8);
    }
    return value;
}

double f64At(const std::vector<uint8_t>& bytes, size_t at) {
    double value = std::nan("");
    if (at + 8u <= bytes.size()) {
        std::memcpy(&value, &bytes[at], 8);
    }
    return value;
}

std::vector<uint8_t> patched(std::vector<uint8_t> bytes, size_t at, const void* value, size_t size) {
    if (at == 0 || at + size > bytes.size()) {
        return std::vector<uint8_t>();
    }
    std::memcpy(&bytes[at], value, size);
    repairCadPayloadCrc(&bytes);
    return bytes;
}

// Refused as a semantic value, with nothing written to the destination.
bool refusedSemantic(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return false;
    }
    ProjectDocument out;
    const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &out);
    return why == ProjectCodecStatus::InvalidSemanticValue && !out.hasCad && out.cad.bodies.empty()
           && out.scene.bodies.empty();
}

uint64_t fingerprintOf(const CadBodyState& state) {
    ConstructionScene scene((NoProjectTag()));
    SceneObject* object = scene.addCadBody(state);
    if (object == nullptr) {
        return 0;
    }
    publishSceneObject(*object);
    return projectSemanticFingerprint(scene, ProjectKind::Construction);
}

void testPersistence(Recorder& r) {
    const CadBodyState states[4] = {regionHoleState(), featureAddState(), featureCutState(),
                                    featureChainState()};
    const char* const names[4] = {"region_hole", "feature_add", "feature_cut", "feature_chain"};
    std::vector<uint8_t> bytes[4];
    bool atV5[4] = {false, false, false, false};
    bool decodes[4] = {false, false, false, false};
    bool stable[4] = {false, false, false, false};
    std::string digests;
    for (int i = 0; i < 4; ++i) {
        const ProjectDocument document = cadDocumentFor(states[i]);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        bytes[i] = encodeProjectV1(document, &why);
        uint16_t version = 0;
        atV5[i] = why == ProjectCodecStatus::Ok && !bytes[i].empty()
                  && document.scene.bodies.size() == 1u
                  && document.scene.bodies[0].objectId == kFirstBodyObjectId
                  && document.scene.nextObjectId == kFirstBodyObjectId + 1u
                  && document.scene.activeObjectId == kFirstBodyObjectId
                  && cadbSectionVersion(bytes[i], &version) && version == kCadSectionVersionV5;
        ProjectDocument decoded;
        const ProjectCodecStatus decodedWhy =
                bytes[i].empty() ? ProjectCodecStatus::Truncated
                                 : decodeProject(bytes[i].data(), bytes[i].size(), &decoded);
        decodes[i] = decodedWhy == ProjectCodecStatus::Ok && decoded.hasCad
                     && decoded.cad.bodies.size() == 1u
                     && sameCadBodyState(decoded.cad.bodies[0].state, states[i])
                     && sameProjectDocument(document, decoded);
        stable[i] = decodes[i] && encodeProjectV1(decoded) == bytes[i]
                    && validateProjectDocument(decoded) == ProjectCodecStatus::Ok;
        digests += std::string(i == 0 ? "" : " ") + names[i] + "=" + projectFixtureSha256Hex(bytes[i]);
    }
    r.check("CADVS_IO_01_region_hole_writes_cadb_v5", atV5[0]);
    r.check("CADVS_IO_02_region_hole_decodes_to_the_same_state", decodes[0]);
    r.check("CADVS_IO_03_region_hole_re_encodes_byte_identical_and_validates", stable[0]);
    r.check("CADVS_IO_04_feature_add_writes_cadb_v5", atV5[1]);
    r.check("CADVS_IO_05_feature_add_decodes_to_the_same_state", decodes[1]);
    r.check("CADVS_IO_06_feature_add_re_encodes_byte_identical_and_validates", stable[1]);
    r.check("CADVS_IO_07_feature_cut_writes_cadb_v5", atV5[2]);
    r.check("CADVS_IO_08_feature_cut_decodes_to_the_same_state", decodes[2]);
    r.check("CADVS_IO_09_feature_cut_re_encodes_byte_identical_and_validates", stable[2]);
    r.check("CADVS_IO_10_feature_chain_writes_cadb_v5", atV5[3]);
    r.check("CADVS_IO_11_feature_chain_decodes_to_the_same_state", decodes[3]);
    r.check("CADVS_IO_12_feature_chain_re_encodes_byte_identical_and_validates", stable[3]);

    // The v5 record at the offsets DATA_PACKAGE_SPEC.md §7f fixes. The one-body
    // record from the payload start: bodyCount u32 | objectId u64 | plane u8 |
    // supportKind u8 | nextEntityId u32 | profileEntityId u32 | extent u8 |
    // direction u8 | depth f64 | secondDistance f64 | entityCount u32 |
    // entities | REGIONS | laterFeatureCount u32 | features (56-byte fixed
    // record, entities, REGIONS).
    constexpr size_t kFirstEntity = 4 + 8 + 1 + 1 + 4 + 4 + 1 + 1 + 8 + 8 + 4;
    constexpr size_t kRectangleRecord = 4 + 1 + 4 * 8;
    constexpr size_t kCircleRecord = 4 + 1 + 3 * 8;
    constexpr size_t kAddLaterCount = kFirstEntity + kRectangleRecord + 4 + 4;
    constexpr size_t kAddFeatureId = kAddLaterCount + 4;
    constexpr size_t kAddOperation = kAddFeatureId + 4;
    constexpr size_t kAddSupportFeature = kAddOperation + 1;
    constexpr size_t kAddFaceKind = kAddSupportFeature + 4;
    constexpr size_t kAddToolEntity = kAddFeatureId + 56;
    constexpr size_t kAddToolCentreU = kAddToolEntity + 4 + 1;
    constexpr size_t kAddPayloadBytes = kAddToolEntity + kRectangleRecord + 4 + 4;
    constexpr size_t kHoleCount = kFirstEntity + kRectangleRecord + kCircleRecord;
    constexpr size_t kHoleAnchor = kHoleCount + 4;
    constexpr size_t kHolePayloadBytes = kHoleAnchor + 4 + 4 + 4;
    const size_t addAt = cadPayloadOffset(bytes[1]);
    const size_t holeAt = cadPayloadOffset(bytes[0]);
    const bool layout =
            addAt != 0 && holeAt != 0
            && u64At(bytes[1], addAt - kForgeSectionHeaderBytes + 8) == kAddPayloadBytes
            && u64At(bytes[0], holeAt - kForgeSectionHeaderBytes + 8) == kHolePayloadBytes
            && u32At(bytes[1], addAt) == 1u && u32At(bytes[1], addAt + kAddLaterCount) == 1u
            && u32At(bytes[1], addAt + kAddFeatureId) == 2u && bytes[1][addAt + kAddOperation] == 2u
            && u32At(bytes[1], addAt + kAddSupportFeature) == 1u
            && bytes[1][addAt + kAddFaceKind] == 2u
            && u32At(bytes[1], addAt + kAddToolEntity) == 1u
            && bytes[1][addAt + kAddToolEntity + 4] == 3u
            && f64At(bytes[1], addAt + kAddToolCentreU) == 0.0
            && u32At(bytes[0], holeAt + kHoleCount) == 1u
            && u32At(bytes[0], holeAt + kHoleAnchor) == 2u
            && u32At(bytes[0], holeAt + kHoleAnchor + 4) == 0u
            && u32At(bytes[0], holeAt + kHoleAnchor + 8) == 0u;
    r.check("CADVS_IO_13_the_v5_record_sits_at_the_specified_offsets", layout);

    const uint8_t badOperationCode = 9;
    const uint32_t noSuchFeature = 7;
    const uint32_t baseFeatureId = 1;
    const uint32_t noSuchHole = 7;
    const double farAway = 5.0;
    const std::vector<uint8_t> badOperation =
            layout ? patched(bytes[1], addAt + kAddOperation, &badOperationCode, 1) : std::vector<uint8_t>();
    const std::vector<uint8_t> badSupport =
            layout ? patched(bytes[1], addAt + kAddSupportFeature, &noSuchFeature, 4)
                   : std::vector<uint8_t>();
    const std::vector<uint8_t> badFeatureId =
            layout ? patched(bytes[1], addAt + kAddFeatureId, &baseFeatureId, 4) : std::vector<uint8_t>();
    const std::vector<uint8_t> badHole =
            layout ? patched(bytes[0], holeAt + kHoleAnchor, &noSuchHole, 4) : std::vector<uint8_t>();
    const std::vector<uint8_t> disjointAdd =
            layout ? patched(bytes[1], addAt + kAddToolCentreU, &farAway, 8) : std::vector<uint8_t>();
    r.check("CADVS_IO_14_an_unknown_operation_code_is_refused_as_a_semantic_value",
            refusedSemantic(badOperation));
    r.check("CADVS_IO_15_a_support_naming_no_earlier_feature_is_refused",
            refusedSemantic(badSupport));
    r.check("CADVS_IO_16_a_later_feature_wearing_the_base_id_is_refused",
            refusedSemantic(badFeatureId));
    r.check("CADVS_IO_17_a_stored_hole_the_sketch_does_not_derive_is_refused",
            refusedSemantic(badHole));
    r.check("CADVS_IO_18_a_v5_file_whose_add_would_be_disjoint_is_refused_on_load",
            refusedSemantic(disjointAdd));
    const char* const badNames[5] = {"bad_operation", "bad_support", "bad_feature_id", "bad_hole",
                                     "disjoint_add"};
    const std::vector<uint8_t>* const bad[5] = {&badOperation, &badSupport, &badFeatureId, &badHole,
                                               &disjointAdd};
    for (int i = 0; i < 5; ++i) {
        digests += std::string(" ") + badNames[i] + "="
                   + (bad[i]->empty() ? std::string("unavailable") : projectFixtureSha256Hex(*bad[i]));
    }
    g_digests = digests;

    uint16_t legacyVersion = 0;
    const std::vector<uint8_t> legacy = encodeProjectV1(cadDocumentFor(blockState()));
    CadBodyState disk = regionHoleState();
    setExtrudeRegions(&disk.extrude, {regionRef(2)});
    uint16_t diskVersion = 0;
    const std::vector<uint8_t> diskBytes = encodeProjectV1(cadDocumentFor(disk));
    r.check("CADVS_IO_19_a_body_without_holes_or_features_still_writes_its_old_version",
            !legacy.empty() && cadbSectionVersion(legacy, &legacyVersion)
                    && legacyVersion == kCadSectionVersion && !diskBytes.empty()
                    && cadbSectionVersion(diskBytes, &diskVersion)
                    && diskVersion == kCadSectionVersion);
    // `CAD-FOUNDATION-C1`: a union selection is the SAME v5 record the codec
    // always wrote -- the atomic list -- and it round-trips byte-identically.
    {
        const CadBodyState unionBody = unionState(true, true, false);
        uint16_t unionVersion = 0;
        const std::vector<uint8_t> unionBytes = encodeProjectV1(cadDocumentFor(unionBody));
        ProjectDocument back;
        const bool decoded = !unionBytes.empty()
                             && decodeProject(unionBytes.data(), unionBytes.size(), &back)
                                        == ProjectCodecStatus::Ok
                             && back.hasCad && back.cad.bodies.size() == 1u;
        r.check("CADFC1_PER_03_a_union_selection_round_trips_as_the_unchanged_v5_record",
                decoded && cadbSectionVersion(unionBytes, &unionVersion) && unionVersion == 5u
                        && sameCadBodyState(back.cad.bodies[0].state, unionBody)
                        && encodeProjectV1(back) == unionBytes);
    }
    const uint64_t baseFingerprint = fingerprintOf(blockState());
    const uint64_t addFingerprint = fingerprintOf(featureAddState());
    r.check("CADVS_IO_20_a_later_feature_moves_the_project_fingerprint",
            baseFingerprint != 0u && addFingerprint != 0u && baseFingerprint != addFingerprint
                    && addFingerprint != fingerprintOf(featureCutState())
                    && addFingerprint == fingerprintOf(featureAddState()));
    bool hex = true;
    std::vector<std::string> seen;
    for (int i = 0; i < 4; ++i) {
        const std::string h = bytes[i].empty() ? std::string() : projectFixtureSha256Hex(bytes[i]);
        hex = hex && h.size() == 64u
              && h.find_first_not_of("0123456789abcdef") == std::string::npos
              && std::find(seen.begin(), seen.end(), h) == seen.end();
        seen.push_back(h);
    }
    r.check("CADVS_IO_21_the_four_v5_digests_are_distinct_sha256", hex);

    // The corpus `scripts/build-forge-corpus.ps1` CONSTRUCTS from
    // DATA_PACKAGE_SPEC.md §7f, sharing no line with this codec: the same bytes
    // from two implementations is the format being a specification.
    const char* const committed[9] = {
            "c6d269425cfa02d749e00b6fe922edf1d6896e6d488a1bad52efbfb59ece91f5",  // cad_region_hole_v5
            "2cb25af694e26a1d186374ec7e691d500201068eeb0606e08dc486c2d637f264",  // cad_feature_add_v5
            "9f4efdb626fea351790a0063308b422954aa49f1162e199027d5d04b43b66cbc",  // cad_feature_cut_v5
            "a4a65681d08c093a9179d5c6be22a56fe2d7b2347670d2231229c2ef0230f10f",  // cad_feature_chain_v5
            "b04b776b29189ffe7262b5ba95f9d106e5a5f37fa6f6c8a8566a5b31de002a23",  // cad_bad_operation_v5
            "79436a25e7177fa818b7ac4dac1476254658c9e09f1c3a49b0f042f560b738cd",  // cad_bad_feature_ref_v5
            "7d50931ddf3f7a734454beaed51576225aae0cc1d990b94ba5bc3ab8d7c4e951",  // cad_bad_feature_order_v5
            "17d27c5c46b9b9f7d606d278d524aff6cfbc48647504497825c8438f2493f7a0",  // cad_bad_region_v5
            nullptr};
    const std::vector<uint8_t>* const encoded[8] = {&bytes[0],   &bytes[1],     &bytes[2],
                                                    &bytes[3],   &badOperation, &badSupport,
                                                    &badFeatureId, &badHole};
    bool corpus = true;
    for (int i = 0; i < 8; ++i) {
        corpus = corpus && !encoded[i]->empty()
                 && projectFixtureSha256Hex(*encoded[i]) == committed[i];
    }
    r.check("CADVS_IO_22_every_v5_fixture_matches_the_independent_corpus_digest", corpus);
    r.check("CADVS_IO_23_the_lineage_tokens_are_the_spec_s_format_values",
            cadFeatureTopologySignature(blockState(), kCadFeatureId) == 0x958F78AF1C70BAA1ull
                    && cadFeatureTopologySignature(regionHoleState(), kCadFeatureId)
                               == 0x937BA1514FAF7381ull);
}

// ---------------------------------------------------------------------------
// F. The acceptance model's timings
// ---------------------------------------------------------------------------

void measurePerformance(Recorder& r) {
    const CadBodyState base = regionHoleState();
    const CadBodyState withAdd = chainAddOnlyState();
    const CadBodyState chain = featureChainState();
    const ProjectDocument document = cadDocumentFor(chain);
    std::vector<double> baseUs;
    std::vector<double> addUs;
    std::vector<double> chainUs;
    std::vector<double> codecUs;
    bool ok = !document.cad.bodies.empty();
    uint32_t triangles = 0;
    for (int run = 0; run < 5; ++run) {
        CadBodyMesh mesh;
        auto t0 = std::chrono::steady_clock::now();
        ok = ok && regenerateCadBody(base, &mesh) == CadStatus::Ok;
        baseUs.push_back(microsSince(t0));
        t0 = std::chrono::steady_clock::now();
        ok = ok && regenerateCadBody(withAdd, &mesh) == CadStatus::Ok;
        addUs.push_back(microsSince(t0));
        t0 = std::chrono::steady_clock::now();
        ok = ok && regenerateCadBody(chain, &mesh) == CadStatus::Ok;
        chainUs.push_back(microsSince(t0));
        triangles = static_cast<uint32_t>(mesh.mesh.indices.size() / 3u);
        t0 = std::chrono::steady_clock::now();
        const std::vector<uint8_t> bytes = encodeProjectV1(document);
        ProjectDocument back;
        ok = ok && !bytes.empty()
             && decodeProject(bytes.data(), bytes.size(), &back) == ProjectCodecStatus::Ok;
        codecUs.push_back(microsSince(t0));
    }
    char line[256];
    std::snprintf(line, sizeof(line),
                  "region_base_us=%.0f add_us=%.0f add_cut_us=%.0f codec_roundtrip_us=%.0f "
                  "triangles=%u",
                  medianOf(baseUs), medianOf(addUs), medianOf(chainUs), medianOf(codecUs), triangles);
    g_performance = line;
    r.check("CADVS_PERF_01_acceptance_model_regenerates_and_round_trips_every_run",
            ok && triangles > 0u && std::isfinite(medianOf(chainUs)) && medianOf(chainUs) >= 0.0);
}

// ---------------------------------------------------------------------------
// `CAD-V6-S1-C1`: how long a CadFeatureId and a CadSketchId live
// ---------------------------------------------------------------------------
//
// Every case drives the product's own paths -- a face sketch through the
// session, one commit inside one `ScopedConstructionEdit`, and the one
// `ConstructionHistory` -- over a scene of its own, opened the way Open opens
// a project: one body and an EMPTY history.

struct IdLifetimeRig {
    ConstructionScene scene{NoProjectTag{}};
    ConstructionHistory history{scene};
    ObjectId bodyId = kNoObject;

    bool open(const CadBodyState& state) {
        SceneObject* body = scene.addCadBody(state);
        if (body == nullptr) {
            return false;
        }
        publishSceneObject(*body);
        bodyId = body->objectId();
        history.clear();
        return true;
    }
    CadBodyState state() const { return bodyStateOf(scene, bodyId); }
    uint64_t fingerprint() const {
        return projectSemanticFingerprint(scene, ProjectKind::Construction);
    }
    std::vector<uint8_t> bytes() const {
        return encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction));
    }
    // One later feature through the product path: a sketch on `onFeature`'s
    // far cap, ONE entity typed exactly, Finish, the operation, the depth, one
    // commit. `out` receives the session's candidate just before the commit.
    CadStatus feature(uint32_t onFeature, CadFeatureOperation operation,
                      SketchEntity::Payload exact, double depth, ExtrudeDirection direction,
                      bool cancelInsteadOfCommit = false, CadBodyState* outCandidate = nullptr) {
        SketchFrame frame;
        TopoRef ref;
        if (!worldCapFrame(scene, bodyId, onFeature, CadFaceKind::CapFar, &frame, &ref)) {
            return CadStatus::ProfileNotFound;
        }
        const CadBodyState producer = state();
        SessionDriver s;
        if (!s.beginFace(frame, ref, &producer)) {
            return CadStatus::NotSketching;
        }
        const SketchTool tool = std::holds_alternative<SketchCircle>(exact) ? SketchTool::Circle
                                                                           : SketchTool::Rectangle;
        const SketchEntityId id =
                s.place(tool, SketchPoint{0.0, 0.0}, SketchPoint{0.5, 0.5}, std::move(exact));
        CadStatus why = id == kNoSketchEntity ? CadStatus::ProfileNotFound : s.sketch.finish();
        if (why == CadStatus::Ok) why = s.sketch.setOperation(operation);
        if (why == CadStatus::Ok) why = s.sketch.setExtrude(depth, direction);
        if (why == CadStatus::Ok && outCandidate != nullptr) {
            *outCandidate = s.sketch.candidateState();
        }
        if (why == CadStatus::Ok && cancelInsteadOfCommit) {
            s.sketch.cancel();
            return s.sketch.active() ? CadStatus::NotSketching : CadStatus::Ok;
        }
        if (why == CadStatus::Ok) {
            ObjectId out = kNoObject;
            why = s.sketch.commit(scene, history, &out);
        }
        if (s.sketch.active()) {
            s.sketch.cancel();
        }
        return why;
    }
    CadStatus add(uint32_t onFeature, SketchEntity::Payload exact, double depth = 0.25) {
        return feature(onFeature, CadFeatureOperation::Add, std::move(exact), depth,
                       ExtrudeDirection::AlongNormal);
    }
    CadStatus cut(uint32_t onFeature, SketchEntity::Payload exact, double depth = 0.5) {
        return feature(onFeature, CadFeatureOperation::Cut, std::move(exact), depth,
                       ExtrudeDirection::AgainstNormal);
    }
    // A domain edit as ONE transaction -- what `commitEdit` does with a
    // staged candidate.
    CadStatus apply(const CadBodyState& requested) {
        SceneObject* object = scene.findBody(bodyId);
        CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
        if (body == nullptr) {
            return CadStatus::NotCadBody;
        }
        CadStatus why = CadStatus::Ok;
        {
            ScopedConstructionEdit edit(history);
            why = body->applyState(requested);
            if (why == CadStatus::Ok) {
                publishSceneObject(*object);
            }
        }
        return why;
    }
};

// The later feature `k` (0-based) of `state`, or a zero record.
CadFeature laterAt(const CadBodyState& state, size_t k) {
    return k < state.laterFeatures.size() ? state.laterFeatures[k] : CadFeature{};
}

void testIdLifetimeBefore(Recorder& r) {
    // IDL-B01..B03 on one rig: Add, Undo, look at the redo side, new branch.
    IdLifetimeRig rig;
    const bool opened = rig.open(blockState());
    const CadBodyState s0 = rig.state();
    const size_t undo0 = rig.history.undoDepth();
    const CadStatus added = opened ? rig.add(kCadFeatureId, rectangleAt(0.0, 0.0, 0.5, 0.5))
                                   : CadStatus::NotCadBody;
    const CadBodyState s1 = rig.state();
    const bool addMinted2 = added == CadStatus::Ok && s1.laterFeatures.size() == 1u
                            && laterAt(s1, 0).featureId == 2u && laterAt(s1, 0).sketchId == 2u
                            && s1.nextFeatureId == 3u && s1.nextSketchId == 3u
                            && rig.history.undoDepth() == undo0 + 1u;
    const bool undone = rig.history.undo();
    const CadBodyState afterUndo = rig.state();
    const bool rewound = undone && sameCadBodyState(afterUndo, s0) && afterUndo.nextFeatureId == 2u
                         && afterUndo.nextSketchId == 2u && afterUndo.laterFeatures.empty()
                         && afterUndo.sketches.size() == 1u;
    // IDL-B02: the redo side still holds feature 2 / sketch 2 -- a Redo puts
    // back exactly that identity -- so "nothing can still point at the undone
    // feature" was not true while the redo step stands.
    const bool redoStands = rig.history.canRedo() && rig.history.redoDepth() == 1u;
    const bool redone = rig.history.redo();
    const bool redoIsTheSameIdentity = redone && sameCadBodyState(rig.state(), s1);
    const bool undoneAgain = rig.history.undo() && sameCadBodyState(rig.state(), s0)
                             && rig.history.redoDepth() == 1u;
    r.check("CADV6C1_IDL_B02_BEFORE_after_undo_the_redo_step_still_holds_feature_2_and_sketch_2",
            addMinted2 && redoStands && redoIsTheSameIdentity && undoneAgain);
    // IDL-B01: a new edit after Undo is handed the SAME ids again.
    const CadStatus branched = rig.cut(kCadFeatureId, circleAt(0.0, 0.0, 0.3));
    const CadBodyState s2 = rig.state();
    const bool reminted = branched == CadStatus::Ok && s2.laterFeatures.size() == 1u
                          && laterAt(s2, 0).featureId == 2u && laterAt(s2, 0).sketchId == 2u
                          && laterAt(s2, 0).operation == CadFeatureOperation::Cut
                          && s2.nextFeatureId == 3u && s2.nextSketchId == 3u
                          && !sameCadBodyState(s2, s1);
    r.check("CADV6C1_IDL_B01_BEFORE_undo_rewinds_both_high_water_marks_and_the_next_feature_re_mints_2",
            addMinted2 && rewound && reminted);
    // IDL-B03: that commit empties the redo stack, and no walk of the history
    // reaches the abandoned Add again.
    const bool redoCleared = rig.history.redoDepth() == 0u && !rig.history.canRedo()
                             && !rig.history.redo() && sameCadBodyState(rig.state(), s2);
    bool abandonedUnreachable = true;
    while (rig.history.undo()) {
        abandonedUnreachable &= !sameCadBodyState(rig.state(), s1);
    }
    const bool backAtStart = sameCadBodyState(rig.state(), s0);
    while (rig.history.redo()) {
        abandonedUnreachable &= !sameCadBodyState(rig.state(), s1);
    }
    r.check("CADV6C1_IDL_B03_BEFORE_the_new_branch_commit_clears_redo_and_the_old_add_is_unreachable",
            reminted && redoCleared && abandonedUnreachable && backAtStart
                    && sameCadBodyState(rig.state(), s2));

    // IDL-B04: a cancelled edit. The session's candidate mints ids of its own;
    // cancelling it writes nothing to the body and records nothing. And an
    // open Construction edit that applied a minted state and was cancelled
    // puts the high-water marks back, leaving the redo side alone.
    {
        IdLifetimeRig c;
        const bool cOpened = c.open(blockState());
        const CadBodyState before = c.state();
        CadBodyState candidate;
        const CadStatus cancelled =
                cOpened ? c.feature(kCadFeatureId, CadFeatureOperation::Add,
                                    rectangleAt(0.0, 0.0, 0.5, 0.5), 0.25,
                                    ExtrudeDirection::AlongNormal, /*cancel=*/true, &candidate)
                        : CadStatus::NotCadBody;
        const bool sessionCancel = cancelled == CadStatus::Ok
                                   && laterAt(candidate, 0).featureId == 2u
                                   && candidate.nextFeatureId == 3u && candidate.nextSketchId == 3u
                                   && sameCadBodyState(c.state(), before)
                                   && c.history.undoDepth() == 0u && c.history.redoDepth() == 0u;
        // A redo step to protect, then an edit opened, used and cancelled.
        const bool redoArmed = c.add(kCadFeatureId, rectangleAt(0.0, 0.0, 0.5, 0.5)) == CadStatus::Ok
                               && c.history.undo() && c.history.redoDepth() == 1u;
        CadBody* body = c.scene.findBody(c.bodyId)->cadOrNull();
        const bool opened2 = c.history.beginEdit();
        const bool applied = body->applyState(candidate) == CadStatus::Ok
                             && body->state().nextFeatureId == 3u;
        c.history.cancelEdit();
        const bool editCancel = redoArmed && opened2 && applied
                                && sameCadBodyState(c.state(), before)
                                && c.state().nextFeatureId == 2u && c.state().nextSketchId == 2u
                                && c.history.undoDepth() == 0u && c.history.redoDepth() == 1u;
        r.check("CADV6C1_IDL_B04_BEFORE_a_cancelled_edit_burns_no_id_and_records_no_step",
                sessionCancel && editCancel);
    }

    // IDL-B05: the saved bytes and the fingerprint after Add then Undo.
    {
        IdLifetimeRig f;
        const bool fOpened = f.open(blockState());
        const std::vector<uint8_t> saved = f.bytes();
        const uint64_t savedFingerprint = f.fingerprint();
        uint16_t savedVersion = 0;
        const bool legacy = cadbSectionVersion(saved, &savedVersion) && savedVersion == 1u;
        const bool addOk = fOpened && f.add(kCadFeatureId, rectangleAt(0.0, 0.0, 0.5, 0.5))
                                              == CadStatus::Ok;
        const bool moved = f.fingerprint() != savedFingerprint && f.bytes() != saved;
        const bool undoOk = f.history.undo();
        r.check("CADV6C1_IDL_B05_BEFORE_undo_back_to_the_saved_state_is_byte_and_fingerprint_equal",
                legacy && addOk && moved && undoOk && f.fingerprint() == savedFingerprint
                        && f.bytes() == saved);
    }
}

// Walks the whole reachable history -- every Undo, then every Redo -- and
// asks `ok` of the rig's body at each state. Ends where it began.
template <typename Predicate>
bool everyReachableState(IdLifetimeRig& rig, Predicate ok) {
    const CadBodyState start = rig.state();
    const size_t redoAtStart = rig.history.redoDepth();
    bool all = ok(rig.state());
    while (rig.history.undo()) {
        all &= ok(rig.state());
    }
    while (rig.history.redo()) {
        all &= ok(rig.state());
    }
    // Back to where the walk began: undo the redo steps that stood at start.
    for (size_t i = 0; i < redoAtStart; ++i) {
        rig.history.undo();
    }
    return all && rig.history.redoDepth() == redoAtStart && sameCadBodyState(rig.state(), start);
}

// How many features, and how many sketch records, carry `id`.
size_t featuresWithId(const CadBodyState& s, uint32_t id) {
    size_t n = id == kCadFeatureId ? 1u : 0u;
    for (const CadFeature& f : s.laterFeatures) n += f.featureId == id ? 1u : 0u;
    return n;
}
size_t sketchesWithId(const CadBodyState& s, CadSketchId id) {
    size_t n = 0;
    for (const CadSketchRecord& record : s.sketches) n += record.sketchId == id ? 1u : 0u;
    return n;
}

// The pinned contract (`CAD-V6-S1-C1`, the `CadSketchId` comment): an id is
// unique along ONE FORWARD HISTORY BRANCH. A committed edit never lowers a
// high-water mark; Undo restores the snapshot, marks included; the edit that
// re-mints an id only the redo side held is the commit that clears redo.
void testIdLifetime(Recorder& r) {
    const SketchEntity::Payload boss = rectangleAt(0.0, 0.0, 0.5, 0.5);

    // --- IDL-01: a committed deletion never hands its ids on --------------
    {
        IdLifetimeRig d;
        const bool built = d.open(blockState())
                           && d.add(kCadFeatureId, rectangleAt(-0.5, -0.5, 0.5, 0.5)) == CadStatus::Ok
                           && d.add(kCadFeatureId, rectangleAt(0.5, 0.5, 0.5, 0.5)) == CadStatus::Ok
                           && d.state().nextFeatureId == 4u && d.state().nextSketchId == 4u;
        // The deletion S2's command will make: the last feature and its
        // sketch leave, the marks stay.
        CadBodyState deleted = d.state();
        deleted.laterFeatures.pop_back();
        deleted.sketches.pop_back();
        const size_t undoBefore = d.history.undoDepth();
        const bool committed = d.apply(deleted) == CadStatus::Ok
                               && d.history.undoDepth() == undoBefore + 1u
                               && sameCadBodyState(d.state(), deleted);
        // A deletion that hands the ids BACK is refused below every caller.
        CadBodyState handsBack = deleted;
        handsBack.nextFeatureId = 3u;
        handsBack.nextSketchId = 3u;
        const bool lowerRefused = validateCadBodyState(handsBack) == CadStatus::Ok
                                  && d.apply(handsBack) == CadStatus::HighWaterInvalid
                                  && sameCadBodyState(d.state(), deleted)
                                  && d.history.undoDepth() == undoBefore + 1u;
        CadBodyState onlySketchLower = deleted;
        onlySketchLower.nextSketchId = 3u;
        const bool sketchLowerRefused = d.apply(onlySketchLower) == CadStatus::HighWaterInvalid;
        const bool next = d.add(kCadFeatureId, rectangleAt(0.5, -0.5, 0.5, 0.5)) == CadStatus::Ok
                          && laterAt(d.state(), 1).featureId == 4u
                          && laterAt(d.state(), 1).sketchId == 4u && featuresWithId(d.state(), 3u) == 0u
                          && sketchesWithId(d.state(), 3u) == 0u;
        r.check("CADV6C1_IDL_01_a_committed_deletion_never_hands_its_ids_on_and_lowering_a_mark_is_refused",
                built && committed && lowerRefused && sketchLowerRefused && next);
    }

    // --- IDL-02 .. IDL-04 on one rig --------------------------------------
    {
        IdLifetimeRig u;
        const bool opened = u.open(blockState());
        const CadBodyState s0 = u.state();
        const bool added = opened && u.add(kCadFeatureId, boss) == CadStatus::Ok;
        const CadBodyState s1 = u.state();
        const bool undone = u.history.undo();
        const CadBodyState back = u.state();
        r.check("CADV6C1_IDL_02_undo_restores_the_snapshot_bit_exactly_high_water_marks_included",
                added && undone && sameCadBodyState(back, s0) && back.nextFeatureId == 2u
                        && back.nextSketchId == 2u && cadBodyStateLegacyRepresentable(back));
        // Redo: while undone, id 2 lives ONLY in the redo snapshot; Redo puts
        // back exactly that identity, once, and the forward branch continues
        // from its marks.
        const bool absentWhileUndone = featuresWithId(back, 2u) == 0u && sketchesWithId(back, 2u) == 0u
                                       && u.history.redoDepth() == 1u;
        const bool redone = u.history.redo();
        const CadBodyState again = u.state();
        const bool exactOnce = redone && sameCadBodyState(again, s1) && featuresWithId(again, 2u) == 1u
                               && sketchesWithId(again, 2u) == 1u
                               && laterAt(again, 0).operation == CadFeatureOperation::Add;
        r.check("CADV6C1_IDL_03_redo_restores_feature_2_and_sketch_2_exactly_and_never_beside_another_2",
                absentWhileUndone && exactOnce && u.history.redoDepth() == 0u);
        // Undo, then a DIFFERENT edit: it is handed 2 / 2, and the same call
        // that mints them empties the redo stack.
        const bool undoneAgain = u.history.undo() && u.history.redoDepth() == 1u;
        const size_t undoBefore = u.history.undoDepth();
        const bool branched = u.cut(kCadFeatureId, circleAt(0.0, 0.0, 0.3)) == CadStatus::Ok;
        const CadBodyState s2 = u.state();
        const bool newIdentity = branched && laterAt(s2, 0).featureId == 2u
                                 && laterAt(s2, 0).sketchId == 2u
                                 && laterAt(s2, 0).operation == CadFeatureOperation::Cut
                                 && findCadSketchRecord(s2, 2u) != nullptr
                                 && findCadSketchRecord(s2, 2u)->sketch.entities.size() == 1u
                                 && findCadSketchRecord(s2, 2u)->sketch.entities[0].circle() != nullptr
                                 && u.history.redoDepth() == 0u && !u.history.canRedo()
                                 && u.history.undoDepth() == undoBefore + 1u;
        const bool neverTheOldAdd = everyReachableState(u, [&s1](const CadBodyState& s) {
            return !sameCadBodyState(s, s1)
                   && (s.laterFeatures.empty()
                       || laterAt(s, 0).operation == CadFeatureOperation::Cut);
        });
        r.check("CADV6C1_IDL_04_undo_then_a_new_edit_re_mints_2_in_the_call_that_clears_redo",
                undoneAgain && newIdentity && neverTheOldAdd);
    }

    // --- IDL-05: two levels of Undo ---------------------------------------
    {
        IdLifetimeRig t;
        const bool built = t.open(blockState())
                           && t.add(kCadFeatureId, rectangleAt(0.0, 0.0, 1.0, 1.0)) == CadStatus::Ok
                           && t.add(2u, rectangleAt(0.0, 0.0, 0.4, 0.4)) == CadStatus::Ok;
        const CadBodyState s3 = t.state();
        const bool chain = built && laterAt(s3, 1).featureId == 3u && laterAt(s3, 1).sketchId == 3u
                           && findCadSketchRecord(s3, 3u)->featureSupport.featureId == 2u;
        const bool twice = t.history.undo() && t.history.undo() && t.history.redoDepth() == 2u
                           && t.state().nextFeatureId == 2u && t.state().nextSketchId == 2u;
        const bool first = t.add(kCadFeatureId, rectangleAt(0.5, 0.5, 0.5, 0.5)) == CadStatus::Ok
                           && laterAt(t.state(), 0).featureId == 2u
                           && laterAt(t.state(), 0).sketchId == 2u && t.history.redoDepth() == 0u;
        const bool second = t.add(kCadFeatureId, rectangleAt(-0.5, -0.5, 0.5, 0.5)) == CadStatus::Ok
                            && laterAt(t.state(), 1).featureId == 3u
                            && laterAt(t.state(), 1).sketchId == 3u
                            && t.state().nextFeatureId == 4u && t.state().nextSketchId == 4u;
        const bool neverTheOldChain = everyReachableState(t, [&s3](const CadBodyState& s) {
            for (const CadSketchRecord& record : s.sketches) {
                if (record.hasFeatureSupport && record.featureSupport.featureId == 2u) {
                    return false;  // the abandoned feature 3 stood on feature 2
                }
            }
            return !sameCadBodyState(s, s3);
        });
        r.check("CADV6C1_IDL_05_two_undos_rewind_to_2_and_the_new_branch_mints_2_then_3",
                chain && twice && first && second && neverTheOldChain);
    }

    // --- IDL-06: cancelled and refused edits burn nothing -----------------
    {
        IdLifetimeRig c;
        const bool opened = c.open(blockState());
        const std::vector<uint8_t> saved = c.bytes();
        const uint64_t savedFingerprint = c.fingerprint();
        CadBodyState candidate;
        const bool sessionCancelled =
                opened
                && c.feature(kCadFeatureId, CadFeatureOperation::Add, boss, 0.25,
                             ExtrudeDirection::AlongNormal, /*cancel=*/true, &candidate)
                           == CadStatus::Ok
                && laterAt(candidate, 0).featureId == 2u;
        // An Add pointed INTO the body gains nothing and is refused by name.
        const CadStatus refused = c.feature(kCadFeatureId, CadFeatureOperation::Add, boss, 0.25,
                                            ExtrudeDirection::AgainstNormal);
        CadBody* body = c.scene.findBody(c.bodyId)->cadOrNull();
        const bool editOpened = c.history.beginEdit();
        const bool editApplied = body->applyState(candidate) == CadStatus::Ok;
        c.history.cancelEdit();
        const bool nothing = sameCadBodyState(c.state(), blockState()) && c.history.undoDepth() == 0u
                             && c.history.redoDepth() == 0u && c.bytes() == saved
                             && c.fingerprint() == savedFingerprint;
        const bool next = c.add(kCadFeatureId, boss) == CadStatus::Ok
                          && laterAt(c.state(), 0).featureId == 2u
                          && laterAt(c.state(), 0).sketchId == 2u;
        r.check("CADV6C1_IDL_06_cancelled_and_refused_edits_burn_no_id_and_change_no_byte",
                sessionCancelled && refused == CadStatus::AddNoEffect && editOpened && editApplied
                        && nothing && next);
    }

    // --- IDL-07: save and reopen ------------------------------------------
    {
        const auto reopen = [](const std::vector<uint8_t>& bytes, IdLifetimeRig* into) {
            ProjectDocument document;
            return !bytes.empty()
                   && decodeProject(bytes.data(), bytes.size(), &document) == ProjectCodecStatus::Ok
                   && document.cad.bodies.size() == 1u && into->open(document.cad.bodies[0].state);
        };
        // (a) saved after an Add: reopened, the next feature is 3.
        IdLifetimeRig a;
        const bool aBuilt = a.open(blockState()) && a.add(kCadFeatureId, boss) == CadStatus::Ok;
        IdLifetimeRig aBack;
        const bool aOk = aBuilt && reopen(a.bytes(), &aBack) && sameCadBodyState(aBack.state(), a.state())
                         && aBack.add(2u, rectangleAt(0.0, 0.0, 0.2, 0.2)) == CadStatus::Ok
                         && laterAt(aBack.state(), 1).featureId == 3u
                         && laterAt(aBack.state(), 1).sketchId == 3u;
        // (b) saved after Add + Undo: the file is the pre-Add file and carries
        // no trace of the undone branch, so the reopened project mints 2.
        IdLifetimeRig b;
        const bool bBuilt = b.open(blockState());
        const std::vector<uint8_t> preAdd = b.bytes();
        const bool bUndone = bBuilt && b.add(kCadFeatureId, boss) == CadStatus::Ok && b.history.undo();
        IdLifetimeRig bBack;
        const bool bOk = bUndone && b.bytes() == preAdd && reopen(b.bytes(), &bBack)
                         && bBack.add(kCadFeatureId, boss) == CadStatus::Ok
                         && laterAt(bBack.state(), 0).featureId == 2u
                         && laterAt(bBack.state(), 0).sketchId == 2u;
        // (c) saved after a committed deletion: v6 carries the marks, so the
        // reopened project still never re-mints the deleted ids.
        IdLifetimeRig c;
        bool cOk = c.open(blockState()) && c.add(kCadFeatureId, rectangleAt(-0.5, -0.5, 0.5, 0.5)) == CadStatus::Ok
                   && c.add(kCadFeatureId, rectangleAt(0.5, 0.5, 0.5, 0.5)) == CadStatus::Ok;
        CadBodyState deleted = c.state();
        deleted.laterFeatures.pop_back();
        deleted.sketches.pop_back();
        cOk = cOk && c.apply(deleted) == CadStatus::Ok;
        const std::vector<uint8_t> deletedBytes = c.bytes();
        uint16_t version = 0;
        IdLifetimeRig cBack;
        cOk = cOk && cadbSectionVersion(deletedBytes, &version) && version == kCadSectionVersionV6
              && reopen(deletedBytes, &cBack) && cBack.state().nextFeatureId == 4u
              && cBack.state().nextSketchId == 4u
              && cBack.add(kCadFeatureId, rectangleAt(0.5, -0.5, 0.5, 0.5)) == CadStatus::Ok
              && laterAt(cBack.state(), 1).featureId == 4u && laterAt(cBack.state(), 1).sketchId == 4u;
        r.check("CADV6C1_IDL_07_a_reopened_project_continues_from_the_marks_its_file_states", aOk && bOk && cOk);
    }

    // --- IDL-08: Undo back to the saved state is clean --------------------
    {
        IdLifetimeRig f;
        const bool opened = f.open(blockState());
        const std::vector<uint8_t> saved = f.bytes();
        const uint64_t savedFingerprint = f.fingerprint();
        const bool roundTrip = opened && f.add(kCadFeatureId, boss) == CadStatus::Ok
                               && f.fingerprint() != savedFingerprint && f.history.undo();
        const bool clean = roundTrip && f.fingerprint() == savedFingerprint && f.bytes() == saved;
        // The rejected model, measured: the same state with the marks kept at
        // 3 / 3 by a non-undoable floor is not legacy-shaped, so its file is
        // CADB v6 and its fingerprint is not the saved one -- Undo to a saved
        // project would read unsaved for invisible allocator metadata.
        CadBodyState floored = blockState();
        floored.nextFeatureId = 3u;
        floored.nextSketchId = 3u;
        IdLifetimeRig m;
        uint16_t flooredVersion = 0;
        const bool floorDirty = validateCadBodyState(floored) == CadStatus::Ok
                                && !cadBodyStateLegacyRepresentable(floored) && m.open(floored)
                                && cadbSectionVersion(m.bytes(), &flooredVersion)
                                && flooredVersion == kCadSectionVersionV6
                                && m.fingerprint() != savedFingerprint && m.bytes() != saved;
        r.check("CADV6C1_IDL_08_add_then_undo_is_byte_and_fingerprint_equal_to_the_save_a_floor_would_not_be",
                clean && floorDirty);
    }

    // --- IDL-09: a sketch two features share ------------------------------
    {
        // Sketch 2 on the block's far cap holds two squares, A (entity 1) and
        // B (entity 2). Feature 2 adds A; the step under test makes feature 3
        // CUT B out of the SAME sketch.
        CadSketch pair;
        addRect(&pair, -0.5, 0.0, 0.4, 0.4);
        addRect(&pair, 0.5, 0.0, 0.4, 0.4);
        CadBodyState start = withFeature(blockState(), CadFeatureOperation::Add, kCadFeatureId, pair,
                                         oneSide(0.25));
        CadBodyState sharing = start;
        ExtrudeFeature cutB = oneSide(0.5, ExtrudeDirection::AgainstNormal);
        cutB.profileEntityId = 2;
        const uint32_t cutId = appendCadLaterFeature(&sharing, CadFeatureOperation::Cut, 2u, cutB);
        IdLifetimeRig s;
        const bool shared = s.open(start) && cutId == 3u && s.apply(sharing) == CadStatus::Ok
                            && laterAt(s.state(), 1).sketchId == 2u
                            && !cadBodyStateLegacyRepresentable(s.state());
        const bool undoRedo = s.history.undo() && sameCadBodyState(s.state(), start)
                              && s.history.redo() && sameCadBodyState(s.state(), sharing)
                              && s.history.undo();
        // The new branch re-mints feature 3 -- and a NEW sketch 3 for it. The
        // re-used feature id inherits nothing of the shared reference.
        const bool branched = s.add(kCadFeatureId, circleAt(0.0, 0.6, 0.2)) == CadStatus::Ok;
        const CadBodyState now = s.state();
        const CadSketchRecord* two = findCadSketchRecord(now, 2u);
        const bool noRetarget = branched && laterAt(now, 1).featureId == 3u
                                && laterAt(now, 1).sketchId == 3u
                                && laterAt(now, 1).operation == CadFeatureOperation::Add
                                && laterAt(now, 0).sketchId == 2u && two != nullptr
                                && sameCadSketch(two->sketch, pair) && sketchesWithId(now, 2u) == 1u
                                && s.history.redoDepth() == 0u
                                && nearRel(bodyVolumeOf(s.scene, s.bodyId),
                                           4.0 + 0.16 * 0.25 + circleArea(0.2) * 0.25);
        const bool neverSharedAgain = everyReachableState(s, [](const CadBodyState& state) {
            return state.laterFeatures.size() < 2u
                   || laterAt(state, 1).sketchId != laterAt(state, 0).sketchId;
        });
        r.check("CADV6C1_IDL_09_a_shared_sketch_survives_undo_redo_and_a_re_minted_feature_never_inherits_it",
                shared && undoRedo && noRetarget && neverSharedAgain);
    }

    // --- IDL-10: a feature-face support cannot be retargeted --------------
    {
        IdLifetimeRig k;
        const bool opened = k.open(blockState());
        // Feature 2: a 1 x 1 square boss. Feature 3 stands on ITS far cap.
        // Body B, a New Body on the same cap, carries a TopoRef to it.
        const bool chain = opened
                           && k.add(kCadFeatureId, rectangleAt(0.0, 0.0, 1.0, 1.0)) == CadStatus::Ok
                           && k.add(2u, rectangleAt(0.0, 0.0, 0.4, 0.4)) == CadStatus::Ok;
        const CadBodyState s3 = k.state();
        const CadSketchRecord onTwo = *findCadSketchRecord(s3, 3u);
        const CadFeature three = laterAt(s3, 1);
        const uint64_t squareLineage = cadFeatureTopologySignature(s3, 2u);
        const bool dependent =
                chain
                && k.feature(2u, CadFeatureOperation::NewBody, rectangleAt(0.3, 0.3, 0.2, 0.2), 0.25,
                             ExtrudeDirection::AlongNormal)
                           == CadStatus::Ok
                && k.scene.bodyCount() == 2u && onTwo.featureSupport.featureId == 2u
                && onTwo.featureSupport.lineageToken == squareLineage;
        const bool rewound = k.history.undo() && k.history.undo() && k.history.undo()
                             && k.scene.bodyCount() == 1u && k.history.redoDepth() == 3u
                             && k.state().laterFeatures.empty();
        // The new branch's feature 2 is a CIRCLE boss: same id, other shape.
        const bool branched = k.add(kCadFeatureId, circleAt(0.0, 0.0, 0.4)) == CadStatus::Ok;
        const CadBodyState now = k.state();
        const bool reminted = branched && laterAt(now, 0).featureId == 2u
                              && cadFeatureTopologySignature(now, 2u) != squareLineage
                              && k.history.redoDepth() == 0u && !k.history.redo()
                              && k.scene.bodyCount() == 1u;
        // Nothing reachable still stands on the abandoned square.
        const bool noStaleSupport = everyReachableState(k, [squareLineage](const CadBodyState& s) {
            for (const CadSketchRecord& record : s.sketches) {
                if (record.hasFeatureSupport && record.featureSupport.featureId == 2u
                    && record.featureSupport.lineageToken == squareLineage) {
                    return false;
                }
            }
            return validateCadBodyState(s) == CadStatus::Ok;
        });
        // Grafting the abandoned feature 3 onto the new branch -- which no
        // product path can do -- is refused by its lineage for this shape.
        CadBodyState graft = now;
        CadSketchRecord graftedRecord = onTwo;
        graftedRecord.sketchId = graft.nextSketchId++;
        graft.sketches.push_back(graftedRecord);
        CadFeature graftedFeature = three;
        graftedFeature.featureId = graft.nextFeatureId++;
        graftedFeature.sketchId = graftedRecord.sketchId;
        graft.laterFeatures.push_back(graftedFeature);
        const bool graftRefused = validateCadBodyState(graft) == CadStatus::FeatureSupportInvalid;
        // For a SAME-shape re-mint the lineage token cannot tell the two
        // features apart, which is why the guarantee is structural: the redo
        // step that could have put feature 3 back is gone in the very call
        // that minted the new feature 2 (asserted above), and no product path
        // copies a record from one branch into another.
        CadBodyState sameShape = withFeature(blockState(), CadFeatureOperation::Add, kCadFeatureId,
                                             rectSketch(0.0, 0.0, 1.0, 1.0), oneSide(0.25));
        const bool lineageBlind = cadFeatureTopologySignature(sameShape, 2u) == squareLineage;
        r.check("CADV6C1_IDL_10_a_feature_face_support_is_never_retargeted_by_a_re_minted_feature_id",
                dependent && rewound && reminted && noStaleSupport && graftRefused && lineageBlind);
    }
}

// ---------------------------------------------------------------------------
// `CAD-V6-S2`: planar faces at runtime
// ---------------------------------------------------------------------------

// PF-S1's reference sketches, entity ids 1.. in placement order.
CadSketch lensSketchAt(double cu, double r) {
    CadSketch s;
    addRect(&s, 0.0, 0.0, 4.0, 3.0);
    addCircle(&s, cu, 0.0, r);
    return s;
}

CadSketch protrusionSketch() {
    CadSketch s;
    addRect(&s, 0.0, 0.0, 4.0, 3.0);
    for (const auto& seg : {std::pair<SketchPoint, SketchPoint>{{2.0, -0.5}, {3.0, -0.5}},
                            std::pair<SketchPoint, SketchPoint>{{3.0, -0.5}, {3.0, 0.5}},
                            std::pair<SketchPoint, SketchPoint>{{3.0, 0.5}, {2.0, 0.5}}}) {
        SketchLine line;
        line.start = seg.first;
        line.end = seg.second;
        SketchEntityId id = kNoSketchEntity;
        addSketchEntity(&s, line, &id);
    }
    return s;
}

// The derived face of `sketch` with this area (the lens, a crescent, ...).
bool faceWithArea(const CadSketch& sketch, double area, PlanarFaceRef* out, size_t* outIndex = nullptr) {
    const SketchArrangement a = deriveSketchArrangement(sketch);
    for (size_t i = 0; i < a.faces.size(); ++i) {
        if (std::fabs(a.faces[i].area - area) < 1e-6) {
            *out = a.faces[i].ref;
            if (outIndex != nullptr) *outIndex = i;
            return true;
        }
    }
    return false;
}

// The derived face of `sketch` whose interior contains `point`.
bool faceAtPoint(const CadSketch& sketch, SketchPoint point, PlanarFaceRef* out,
                 size_t* outIndex = nullptr) {
    const SketchArrangement a = deriveSketchArrangement(sketch);
    for (size_t i = 0; i < a.faces.size(); ++i) {
        std::vector<PlanarProfileComponent> shape;
        if (mergePlanarFaceSelection(a, {i}, &shape) != CadStatus::Ok || shape.size() != 1u) continue;
        bool inside = sketchPointStrictlyInside(point, shape[0].outer.polygon);
        for (const PlanarProfileLoop& hole : shape[0].holes) {
            inside = inside && !sketchPointStrictlyInside(point, hole.polygon);
        }
        if (inside) {
            *out = a.faces[i].ref;
            if (outIndex != nullptr) *outIndex = i;
            return true;
        }
    }
    return false;
}

ExtrudeFeature facesExtrude(std::vector<PlanarFaceRef> faces, double depth,
                            ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature e = oneSide(depth, direction);
    e.profileEntityId = kNoSketchEntity;
    e.selection = CadSelectionKind::PlanarFaces;
    std::sort(faces.begin(), faces.end(), [](const PlanarFaceRef& a, const PlanarFaceRef& b) {
        return comparePlanarFaceRef(a, b) < 0;
    });
    e.planarFaces = std::move(faces);
    return e;
}

constexpr double kS2Pi = 3.14159265358979323846;
const double kLens = kS2Pi * 0.125;  // half of a 0.5 m disk: the lens of PF-S1-01

// The polygon area the extruder sees: the tessellated lens.
double polygonAreaOf(const PlanarProfileComponent& c) { return c.area; }

std::vector<CadFeatureFace> sideFaces(const CadFeatureGeometry& g) {
    return std::vector<CadFeatureFace>(g.faces.begin() + 2, g.faces.end());
}

void testPlanarRuntime(Recorder& r) {
    const CadSketch lens = lensSketchAt(2.0, 0.5);
    PlanarFaceRef lensRef;
    const bool lensFound = faceAtPoint(lens, SketchPoint{1.8, 0.0}, &lensRef);
    const CadBodyState lensBody = makeCadBodyState(lens, facesExtrude({lensRef}, 1.0));

    // --- tokens ------------------------------------------------------------
    {
        // The protrusion sketch's big face: rectangle side 1.1 is split at the
        // two T-junctions, and two of its three pieces bound this face.
        const CadSketch sketch = protrusionSketch();
        PlanarFaceRef big;
        const bool found = faceWithArea(sketch, 12.0, &big);
        CadFeatureGeometry g;
        const CadStatus why = buildCadFeatureGeometry(
                makeCadBodyState(sketch, facesExtrude({big}, 1.0)), kCadFeatureId, &g);
        std::vector<CadFaceToken> side11;
        bool wholeLegacy = true;
        for (const CadFeatureFace& face : sideFaces(g)) {
            if (face.token.edgeEntityId == 1u && face.token.edgeLocalIndex == 1u) {
                side11.push_back(face.token);
            } else if (face.token.edgeEntityId == 1u) {
                // Sides 1.0, 1.2, 1.3 are whole: the legacy token, byte for byte.
                CadFaceToken legacy;
                legacy.kind = CadFaceKind::Side;
                legacy.edgeEntityId = 1u;
                legacy.edgeLocalIndex = face.token.edgeLocalIndex;
                wholeLegacy = wholeLegacy && !face.token.fragment
                              && cadFaceTokenCode(face.token)
                                         == ((2ull << 56) | (1ull << 16) | face.token.edgeLocalIndex)
                              && sameCadFaceToken(face.token, legacy);
            }
        }
        // Side 1.1 is split into THREE pieces at the two T-junctions, and the
        // rectangle's face is bounded by all three (the protrusion stands
        // outside it): three sides, three distinct fragment tokens.
        bool twoDistinct = side11.size() == 3u;
        for (size_t i = 0; twoDistinct && i < side11.size(); ++i) {
            twoDistinct = side11[i].fragment && (cadFaceTokenCode(side11[i]) >> 56) == 0x03u;
            for (size_t j = i + 1; twoDistinct && j < side11.size(); ++j) {
                twoDistinct = !sameCadFaceToken(side11[i], side11[j])
                              && cadFaceTokenCode(side11[i]) != cadFaceTokenCode(side11[j]);
            }
        }
        r.check("CADV6S2_TOK_01_the_pieces_of_one_rectangle_side_wear_distinct_fragment_tokens",
                found && why == CadStatus::Ok && twoDistinct);
        CadFaceToken wholeSide;
        wholeSide.kind = CadFaceKind::Side;
        wholeSide.edgeEntityId = 1u;
        wholeSide.edgeLocalIndex = 1u;
        r.check("CADV6S2_TOK_02_whole_edges_keep_the_legacy_token_and_a_piece_never_equals_the_whole",
                wholeLegacy && side11.size() == 3u && !sameCadFaceToken(side11[0], wholeSide)
                        && !sameCadFaceToken(wholeSide, side11[1])
                        && !sameCadFaceToken(side11[2], wholeSide));
        // Deterministic and canonical: a second derivation lists the same
        // tokens in the same order.
        CadFeatureGeometry again;
        buildCadFeatureGeometry(makeCadBodyState(sketch, facesExtrude({big}, 1.0)), kCadFeatureId,
                                &again);
        bool same = again.faces.size() == g.faces.size() && again.signature == g.signature;
        for (size_t i = 0; same && i < g.faces.size(); ++i) {
            same = sameCadFaceToken(again.faces[i].token, g.faces[i].token);
        }
        r.check("CADV6S2_TOK_03_fragment_tokens_and_the_signature_are_deterministic", same);
    }
    {
        // Straight piece eligible, curved piece never.
        CadFeatureGeometry g;
        const CadStatus why = buildCadFeatureGeometry(lensBody, kCadFeatureId, &g);
        bool straightEligible = false;
        bool curvedIneligible = false;
        for (const CadFeatureFace& face : sideFaces(g)) {
            if (face.token.edgeEntityId == 1u) straightEligible = face.eligible && face.token.fragment;
            if (face.token.edgeEntityId == 2u) curvedIneligible = !face.eligible && face.token.fragment;
        }
        r.check("CADV6S2_FACE_05_a_straight_fragment_side_is_eligible_a_curved_one_never",
                lensFound && why == CadStatus::Ok && g.faces.size() == 4u && straightEligible
                        && curvedIneligible);
    }

    // --- fragment supports (S2-FACE-02 .. 04) --------------------------------
    {
        CadFeatureGeometry base;
        buildCadFeatureGeometry(lensBody, kCadFeatureId, &base);
        CadFaceToken straight;
        for (const CadFeatureFace& face : sideFaces(base)) {
            if (face.token.edgeEntityId == 1u) straight = face.token;
        }
        CadFeatureSupport support;
        support.featureId = kCadFeatureId;
        support.face = straight;
        support.lineageToken = base.signature;
        CadBodyState withAdd = lensBody;
        const uint32_t added = appendCadLaterFeatureWithSketch(
                &withAdd, CadFeatureOperation::Add, support, rectSketch(0.0, 0.0, 0.3, 0.3),
                oneSide(0.2));
        const Regen g = regen(withAdd);
        const std::vector<uint8_t> bytes = encodeProjectV1(cadDocumentFor(withAdd));
        ProjectDocument back;
        const bool decoded = !bytes.empty()
                             && decodeProject(bytes.data(), bytes.size(), &back) == ProjectCodecStatus::Ok
                             && back.cad.bodies.size() == 1u;
        uint16_t version = 0;
        r.check("CADV6S2_FACE_02_a_sketch_on_a_straight_fragment_side_regenerates_and_round_trips",
                added == 2u && validateCadBodyState(withAdd) == CadStatus::Ok && g.why == CadStatus::Ok
                        && g.mesh.components == 1u && g.mesh.volume > 0.3 * 0.3 * 0.2
                        && !cadBodyStateLegacyRepresentable(withAdd) && cadbSectionVersion(bytes, &version)
                        && version == kCadSectionVersionV6 && decoded
                        && sameCadBodyState(back.cad.bodies[0].state, withAdd)
                        && encodeProjectV1(back) == bytes
                        && regen(back.cad.bodies[0].state).why == CadStatus::Ok);
        // Topology-preserving: the circle slides and still crosses the same
        // side twice -- same token, same lineage, still attached.
        CadBodyState moved = withAdd;
        CadSketch& movedBase = cadBaseSketch(moved);
        const bool replaced =
                replaceSketchEntity(&movedBase, 2u, circleAt(1.95, 0.1, 0.55)) == CadStatus::Ok;
        CadFeatureGeometry movedBaseGeometry;
        const CadStatus movedWhy = buildCadFeatureGeometry(moved, kCadFeatureId, &movedBaseGeometry);
        bool sameStraight = false;
        for (const CadFeatureFace& face : sideFaces(movedBaseGeometry)) {
            if (face.token.edgeEntityId == 1u) sameStraight = sameCadFaceToken(face.token, straight);
        }
        r.check("CADV6S2_FACE_03_a_topology_preserving_move_keeps_the_fragment_token_and_support",
                replaced && movedWhy == CadStatus::Ok && sameStraight
                        && movedBaseGeometry.signature == base.signature
                        && validateCadBodyState(moved) == CadStatus::Ok
                        && regen(moved).why == CadStatus::Ok);
        // Topology-changing: a line cutting the lens side between the circle's
        // two crossings -- the lens no longer exists, nothing is re-bound.
        CadBodyState split = withAdd;
        SketchLine cutLine;
        cutLine.start = SketchPoint{1.0, 0.0};
        cutLine.end = SketchPoint{3.0, 0.0};
        SketchEntityId lineId = kNoSketchEntity;
        addSketchEntity(&cadBaseSketch(split), cutLine, &lineId);
        CadBody body(1);
        const CadStatus applied = body.applyState(withAdd);
        const CadStatus refused = body.applyState(split);
        r.check("CADV6S2_FACE_04_a_topology_changing_edit_fails_closed_and_the_body_stands",
                applied == CadStatus::Ok && refused == CadStatus::PlanarFaceUnresolved
                        && sameCadBodyState(body.state(), withAdd));
        // The SAME fragment on another face: the outer-circle cell also runs
        // along side 1.1 between the crossings (the other way). Re-selecting
        // the base to that cell keeps the token but changes the lineage: the
        // support refuses, never retargets.
        PlanarFaceRef cap;
        const SketchArrangement arrangement = deriveSketchArrangement(lens);
        for (const AtomicPlanarFace& face : arrangement.faces) {
            if (!samePlanarFaceRef(face.ref, lensRef) && face.area < 1.0) cap = face.ref;
        }
        CadBodyState other = withAdd;
        other.extrude = facesExtrude({cap}, 1.0);
        r.check("CADV6S2_FACE_06_the_same_fragment_on_another_face_is_a_new_lineage_and_refuses",
                !cap.outer.empty() && validateCadBodyState(other) == CadStatus::FeatureSupportInvalid);
    }

    // --- regeneration: New Body, union, disjoint, pinch -----------------------
    {
        const Regen lensSolid = regen(lensBody);
        std::vector<PlanarProfileComponent> components;
        size_t lensIndex = 0;
        faceAtPoint(lens, SketchPoint{1.8, 0.0}, &lensRef, &lensIndex);
        const SketchArrangement a = deriveSketchArrangement(lens);
        mergePlanarFaceSelection(a, {lensIndex}, &components);
        r.check("CADV6S2_REG_01_the_lens_extrudes_as_a_new_body_to_its_tessellated_area",
                lensSolid.why == CadStatus::Ok && lensSolid.mesh.components == 1u
                        && components.size() == 1u
                        && nearRel(lensSolid.mesh.volume, polygonAreaOf(components[0]) * 1.0, 1e-9)
                        && lensSolid.mesh.volume < kLens && lensSolid.mesh.volume > 0.99 * kLens
                        && watertight(lensSolid.mesh.mesh));
        // Adjacent: lens + the rectangle's other cell share the arc; their
        // union is the whole rectangle, exactly, with no wall on the arc.
        std::vector<size_t> both;
        for (size_t i = 0; i < a.faces.size(); ++i) {
            if (a.faces[i].area > 1.0 || i == lensIndex) both.push_back(i);
        }
        std::vector<PlanarFaceRef> refs;
        for (size_t i : both) refs.push_back(a.faces[i].ref);
        const CadBodyState united = makeCadBodyState(lens, facesExtrude(refs, 1.0));
        const Regen unitedSolid = regen(united);
        CadFeatureGeometry g;
        buildCadFeatureGeometry(united, kCadFeatureId, &g);
        bool noArc = true;
        for (const CadFeatureFace& face : sideFaces(g)) noArc = noArc && face.token.edgeEntityId != 2u;
        r.check("CADV6S2_REG_02_adjacent_faces_merge_into_one_component_with_no_internal_wall",
                both.size() == 2u && solidOk(unitedSolid, 12.0) && g.planarComponents.size() == 1u
                        && noArc && g.faces.size() == 2u + 6u);
        // Disjoint: two separate lenses on opposite sides of the rectangle.
        CadSketch twoLenses;
        addRect(&twoLenses, 0.0, 0.0, 4.0, 3.0);
        addCircle(&twoLenses, 2.0, 0.0, 0.5);
        addCircle(&twoLenses, -2.0, 0.0, 0.5);
        const SketchArrangement t = deriveSketchArrangement(twoLenses);
        std::vector<PlanarFaceRef> lenses;
        PlanarFaceRef left;
        PlanarFaceRef right;
        if (faceAtPoint(twoLenses, SketchPoint{-1.8, 0.0}, &left)
            && faceAtPoint(twoLenses, SketchPoint{1.8, 0.0}, &right)) {
            lenses = {left, right};
        }
        const CadBodyState disjoint = makeCadBodyState(twoLenses, facesExtrude(lenses, 1.0));
        const Regen disjointSolid = regen(disjoint);
        CadFeatureGeometry dg;
        buildCadFeatureGeometry(disjoint, kCadFeatureId, &dg);
        CadFeatureGeometry dg2;
        buildCadFeatureGeometry(disjoint, kCadFeatureId, &dg2);
        const bool ordered = dg.planarComponents.size() == 2u
                             && compareFragmentRef(dg.planarComponents[0].outer.fragments[0],
                                                   dg.planarComponents[1].outer.fragments[0]) < 0
                             && dg.signature == dg2.signature;
        r.check("CADV6S2_REG_03_disjoint_faces_are_two_components_in_canonical_order",
                lenses.size() == 2u && disjointSolid.why == CadStatus::Ok
                        && disjointSolid.mesh.components == 2u && ordered);
        // A pinch: two cells meeting at one corner only.
        CadSketch pinch;
        addRect(&pinch, 0.5, 0.5, 1.0, 1.0);
        addRect(&pinch, 1.5, 1.5, 1.0, 1.0);
        SketchLine through;
        through.start = SketchPoint{0.5, -0.5};
        through.end = SketchPoint{0.5, 1.5};
        SketchEntityId throughId = kNoSketchEntity;
        addSketchEntity(&pinch, through, &throughId);
        const SketchArrangement pa = deriveSketchArrangement(pinch);
        std::vector<PlanarFaceRef> corner;
        for (const AtomicPlanarFace& face : pa.faces) {
            if (std::fabs(face.area - 0.5) < 1e-9) {
                // The right half of the first square touches the second at (1, 1).
                std::vector<PlanarProfileComponent> shape;
                size_t i = static_cast<size_t>(&face - pa.faces.data());
                mergePlanarFaceSelection(pa, {i}, &shape);
                bool right = false;
                for (const SketchPoint& p : shape[0].outer.polygon) right = right || p.u > 0.99;
                if (right) corner.push_back(face.ref);
            }
            if (std::fabs(face.area - 1.0) < 1e-9) corner.push_back(face.ref);
        }
        const CadBodyState pinched = makeCadBodyState(pinch, facesExtrude(corner, 1.0));
        r.check("CADV6S2_REG_04_a_pinched_union_is_refused_by_name",
                corner.size() == 2u && validateCadBodyState(pinched) == CadStatus::OverlappingRegions
                        && regen(pinched).why == CadStatus::OverlappingRegions);
        // A spline anywhere keeps the sketch off faces, by name.
        r.check("CADV6S2_REG_05_the_arrangement_refusals_map_to_their_v6_names",
                cadStatusForArrangement(ArrangementStatus::UnsupportedCurve)
                                == CadStatus::PlanarFaceUnsupportedCurve
                        && cadStatusForArrangement(ArrangementStatus::AmbiguousOverlap)
                                   == CadStatus::PlanarFaceAmbiguousOverlap
                        && cadStatusForArrangement(ArrangementStatus::CapExceeded)
                                   == CadStatus::PlanarFaceCapExceeded
                        && cadStatusForArrangement(ArrangementStatus::PinchedSelection)
                                   == CadStatus::OverlappingRegions);
    }

    // --- the requires-PlanarFaces predicate ------------------------------------
    {
        CadSketch nested;
        addRect(&nested, 0.0, 0.0, 4.0, 3.0);
        addCircle(&nested, -1.0, 0.0, 0.5);
        addCircle(&nested, 1.0, 0.0, 0.5);
        CadSketch dangling;
        addRect(&dangling, 0.0, 0.0, 4.0, 3.0);
        SketchLine stub;
        stub.start = SketchPoint{2.0, 0.0};
        stub.end = SketchPoint{3.0, 0.0};
        SketchEntityId stubId = kNoSketchEntity;
        addSketchEntity(&dangling, stub, &stubId);
        const auto requires = [](const CadSketch& sketch) {
            return sketchRequiresPlanarFaces(deriveSketchArrangement(sketch),
                                             extractSketchRegions(sketch));
        };
        r.check("CADV6S2_SES_00_faces_are_required_exactly_where_a_crossing_cuts_an_area",
                requires(lens) && requires(protrusionSketch()) && !requires(nested)
                        && !requires(dangling) && !requires(rectSketch(0.0, 0.0, 2.0, 2.0)));
    }

    // --- the session (S2-01 .. S2-10) -----------------------------------------
    const auto finishedOn = [](SessionDriver& s, const CadSketch& sketch) {
        if (!s.beginWorld(Workplane::XY)) return false;
        for (const SketchEntity& entity : sketch.entities) {
            SketchTool tool = SketchTool::Line;
            if (entity.rectangle() != nullptr) tool = SketchTool::Rectangle;
            if (entity.circle() != nullptr) tool = SketchTool::Circle;
            if (s.place(tool, SketchPoint{0.0, 0.0}, SketchPoint{0.5, 0.5}, entity.payload())
                == kNoSketchEntity) {
                return false;
            }
        }
        return s.sketch.finish() == CadStatus::Ok;
    };
    const auto faceIndexAt = [](SessionDriver& s, double area) {
        for (size_t i = 0; i < s.sketch.planarFaceCount(); ++i) {
            double a = 0.0;
            if (s.sketch.planarFaceInfo(i, nullptr, &a) && std::fabs(a - area) < 1e-6) return i;
        }
        return s.sketch.planarFaceCount();
    };
    const auto tapFace = [](SessionDriver& s, size_t index) {
        SketchPoint p;
        return index < s.sketch.planarFaceCount() && s.sketch.planarFaceInfo(index, &p, nullptr)
               && s.toggleAt(p);
    };
    {
        ConstructionScene scene((NoProjectTag()));
        ConstructionHistory history(scene);
        SessionDriver s;
        const bool finished = finishedOn(s, lens);
        const size_t li = faceIndexAt(s, kLens);
        bool each = s.sketch.selectionKind() == CadSelectionKind::PlanarFaces
                    && s.sketch.planarFaceCount() == 3u
                    && s.sketch.evaluateCandidate().status == CadStatus::AmbiguousProfile;
        // Every cell is selectable on its own, and a tap is a pure toggle.
        for (size_t i = 0; each && i < 3u; ++i) {
            each = tapFace(s, i) && s.sketch.selectedAreaCount() == 1u && s.sketch.planarFaceSelected(i)
                   && tapFace(s, i) && s.sketch.selectedAreaCount() == 0u;
        }
        const bool lensTapped = tapFace(s, li) && s.sketch.selectedAreaCount() == 1u;
        const CadCandidateEvaluation preview = s.sketch.evaluateCandidate();
        ObjectId id = kNoObject;
        const CadStatus committed = s.sketch.commit(scene, history, &id);
        const CadBodyState stored = bodyStateOf(scene, id);
        r.check("CADV6S2_SES_01_circle_crossing_rectangle_three_cells_each_selectable_lens_new_body",
                finished && each && lensTapped && preview.status == CadStatus::Ok
                        && committed == CadStatus::Ok && id != kNoObject
                        && stored.extrude.selection == CadSelectionKind::PlanarFaces
                        && stored.extrude.planarFaces.size() == 1u
                        && samePlanarFaceRef(stored.extrude.planarFaces[0], lensRef)
                        && preview.mesh != nullptr
                        && nearRel(bodyVolumeOf(scene, id), preview.mesh->volume)
                        && history.undoDepth() == 1u);
        // S2-08: reopening the feature finds the exact refs and previews the
        // committed solid until edited.
        SessionDriver reopened;
        const bool began = reopened.sketch.beginEdit(id, stored, s.sketch.frame()) == CadStatus::Ok
                           && reopened.sketch.finish() == CadStatus::Ok;
        const CadCandidateEvaluation again = reopened.sketch.evaluateCandidate();
        r.check("CADV6S2_SES_08_reopening_keeps_the_exact_face_refs_and_previews_the_committed_solid",
                began && reopened.sketch.selectionKind() == CadSelectionKind::PlanarFaces
                        && reopened.sketch.selectedAreaCount() == 1u
                        && samePlanarFaceRef(reopened.sketch.extrude().planarFaces[0], lensRef)
                        && again.status == CadStatus::Ok && again.mesh != nullptr
                        && nearRel(again.mesh->volume, bodyVolumeOf(scene, id)));
        // S2-09: a topology-preserving edit keeps the refs and regenerates.
        const bool edited =
                reopened.sketch.state() == SketchSessionState::Ready
                && (reopened.sketch.backToEditing(), true)
                && reopened.sketch.replaceEntity(2u, circleAt(1.95, 0.1, 0.55)) == CadStatus::Ok
                && reopened.sketch.finish() == CadStatus::Ok && reopened.sketch.selectedAreaCount() == 1u
                && !reopened.sketch.selectionLost();
        const double before = bodyVolumeOf(scene, id);
        const CadStatus editCommitted = edited ? reopened.sketch.commitEdit(scene, history)
                                               : CadStatus::NotSketching;
        r.check("CADV6S2_SES_09_a_topology_preserving_edit_keeps_the_selection_and_regenerates",
                edited && editCommitted == CadStatus::Ok && bodyVolumeOf(scene, id) > before
                        && history.undoDepth() == 2u);
        // S2-10: a topology-changing edit loses the ref by name; nothing is
        // re-bound and the body stands at its last valid state.
        const CadBodyState standing = bodyStateOf(scene, id);
        SessionDriver lost;
        const bool reopenedLost =
                lost.sketch.beginEdit(id, standing, s.sketch.frame()) == CadStatus::Ok
                && lost.sketch.replaceEntity(2u, circleAt(0.0, 0.0, 0.5)) == CadStatus::Ok
                && lost.sketch.finish() == CadStatus::Ok;
        r.check("CADV6S2_SES_10_a_topology_changing_edit_loses_the_ref_by_name_and_changes_nothing",
                reopenedLost && lost.sketch.selectionLost() && !lost.sketch.selectionChosen()
                        && lost.sketch.evaluateCandidate().status == CadStatus::PlanarFaceUnresolved
                        && lost.sketch.commitEdit(scene, history) == CadStatus::PlanarFaceUnresolved
                        && sameCadBodyState(bodyStateOf(scene, id), standing)
                        && history.undoDepth() == 2u);
        lost.sketch.cancel();
    }
    {
        // S2-02: the T-junction protrusion extrudes as a New Body.
        ConstructionScene scene((NoProjectTag()));
        ConstructionHistory history(scene);
        SessionDriver s;
        const bool finished = finishedOn(s, protrusionSketch());
        const bool tapped = tapFace(s, faceIndexAt(s, 1.0));
        ObjectId id = kNoObject;
        const CadStatus committed = s.sketch.setExtrude(0.5, ExtrudeDirection::AlongNormal) == CadStatus::Ok
                                            ? s.sketch.commit(scene, history, &id)
                                            : CadStatus::NotSketching;
        r.check("CADV6S2_SES_02_the_t_junction_protrusion_is_selectable_and_extrudes",
                finished && s.sketch.planarFaceCount() == 0u && tapped && committed == CadStatus::Ok
                        && nearRel(bodyVolumeOf(scene, id), 0.5));
    }
    {
        // S2-03: two crossing circles, three faces, the lens alone.
        CadSketch circles;
        addCircle(&circles, -0.4, 0.0, 1.0);
        addCircle(&circles, 0.4, 0.0, 1.0);
        SessionDriver s;
        const bool finished = finishedOn(s, circles);
        const bool lensOnly = s.toggleAt(SketchPoint{0.0, 0.0}) && s.sketch.selectedAreaCount() == 1u;
        double area = 0.0;
        for (size_t i = 0; i < s.sketch.planarFaceCount(); ++i) {
            if (s.sketch.planarFaceSelected(i)) s.sketch.planarFaceInfo(i, nullptr, &area);
        }
        const bool crescents = s.toggleAt(SketchPoint{-1.2, 0.0}) && s.toggleAt(SketchPoint{1.2, 0.0})
                               && s.sketch.selectedAreaCount() == 3u
                               && s.sketch.evaluateCandidate().status == CadStatus::Ok;
        r.check("CADV6S2_SES_03_two_crossing_circles_three_faces_lens_and_crescents_independent",
                finished && s.sketch.planarFaceCount() == 3u && lensOnly
                        && std::fabs(area - 1.585347) < 1e-5 && crescents);
        s.sketch.cancel();
    }
    {
        // S2-06 / S2-07: a crossing sketch on the block's own cap drives Add
        // and Cut into the SAME body.
        IdLifetimeRig rig;
        const bool opened = rig.open(blockState(1.0));
        const auto onCap = [&](CadFeatureOperation op, double depth, ExtrudeDirection dir,
                               CadStatus* outWhy) {
            SketchFrame f;
            TopoRef t;
            if (!worldCapFrame(rig.scene, rig.bodyId, kCadFeatureId, CadFaceKind::CapFar, &f, &t)) {
                return false;
            }
            const CadBodyState producer = rig.state();
            SessionDriver s;
            if (!s.beginFace(f, t, &producer)) return false;
            // A 1 x 1 square crossed by a 0.3 circle on its right side.
            if (s.place(SketchTool::Rectangle, {0.0, 0.0}, {0.5, 0.5}, rectangleAt(0.0, 0.0, 1.0, 1.0))
                        == kNoSketchEntity
                || s.place(SketchTool::Circle, {-0.2, 0.2}, {0.3, 0.2}, circleAt(0.5, 0.0, 0.3))
                           == kNoSketchEntity
                || s.sketch.finish() != CadStatus::Ok || s.sketch.planarFaceCount() != 3u) {
                return false;
            }
            // Merge the lens with the square's other cell: the whole square.
            for (size_t i = 0; i < s.sketch.planarFaceCount(); ++i) {
                double a = 0.0;
                SketchPoint p;
                s.sketch.planarFaceInfo(i, &p, &a);
                if (p.u < 0.5 + 1e-9 && std::fabs(p.v) < 0.5) {
                    if (s.sketch.togglePlanarFace(i) != CadStatus::Ok) return false;
                }
            }
            *outWhy = s.sketch.setOperation(op);
            if (*outWhy == CadStatus::Ok) *outWhy = s.sketch.setExtrude(depth, dir);
            ObjectId out = kNoObject;
            if (*outWhy == CadStatus::Ok) *outWhy = s.sketch.commit(rig.scene, rig.history, &out);
            if (s.sketch.active()) s.sketch.cancel();
            return true;
        };
        CadStatus addWhy = CadStatus::NotSketching;
        const bool addRan = opened && onCap(CadFeatureOperation::Add, 0.5, ExtrudeDirection::AlongNormal,
                                            &addWhy);
        const double afterAdd = bodyVolumeOf(rig.scene, rig.bodyId);
        r.check("CADV6S2_SES_06_a_merged_face_selection_adds_into_the_same_body",
                addRan && addWhy == CadStatus::Ok && rig.scene.bodyCount() == 1u
                        && rig.state().laterFeatures.size() == 1u
                        && rig.state().laterFeatures[0].extrude.selection == CadSelectionKind::PlanarFaces
                        && rig.state().laterFeatures[0].extrude.planarFaces.size() == 2u
                        && nearRel(afterAdd, 4.0 + 1.0 * 0.5));
        CadStatus cutWhy = CadStatus::NotSketching;
        const bool cutRan = onCap(CadFeatureOperation::Cut, 0.25, ExtrudeDirection::AgainstNormal,
                                  &cutWhy);
        r.check("CADV6S2_SES_07_a_merged_face_selection_cuts_the_same_body",
                cutRan && cutWhy == CadStatus::Ok && rig.scene.bodyCount() == 1u
                        && rig.state().laterFeatures.size() == 2u
                        && rig.state().laterFeatures[1].operation == CadFeatureOperation::Cut
                        && bodyVolumeOf(rig.scene, rig.bodyId) < afterAdd);
    }
    {
        // Legacy stays legacy: a rectangle around two circles finishes on
        // loop regions and writes its legacy CADB version.
        SessionDriver s;
        CadSketch nested;
        addRect(&nested, 0.0, 0.0, 4.0, 3.0);
        addCircle(&nested, -1.0, 0.0, 0.5);
        const bool finished = finishedOn(s, nested);
        r.check("CADV6S2_SES_11_a_nested_sketch_stays_on_loop_regions",
                finished && s.sketch.selectionKind() == CadSelectionKind::LoopRegions
                        && s.sketch.planarFaceCount() == 0u && s.sketch.regions().regions.size() == 2u);
        s.sketch.cancel();
    }

    // --- a shared sketch validates atomically ----------------------------------
    {
        // Sketch 2 on the block's cap: a square crossed by a circle. Feature 2
        // ADDS the lens; feature 3 CUTS the square's other cell -- one sketch.
        CadSketch shared;
        addRect(&shared, 0.0, 0.0, 1.0, 1.0);
        addCircle(&shared, 0.5, 0.0, 0.3);
        const SketchArrangement a = deriveSketchArrangement(shared);
        PlanarFaceRef lensCell;
        PlanarFaceRef squareCell;
        for (const AtomicPlanarFace& face : a.faces) {
            if (face.area < 0.2 && face.area > 0.1) lensCell = face.ref;
            if (face.area > 0.8) squareCell = face.ref;
        }
        CadBodyState state = withFeature(blockState(1.0), CadFeatureOperation::Add, kCadFeatureId,
                                         shared, facesExtrude({lensCell}, 0.25));
        appendCadLaterFeature(&state, CadFeatureOperation::Cut, 2u,
                              facesExtrude({squareCell}, 0.25, ExtrudeDirection::AgainstNormal));
        CadBody body(1);
        const CadStatus built = body.applyState(state);
        // An edit that keeps the lens but merges the square's cell away (a
        // second circle that crosses only the square) loses feature 3's face.
        CadBodyState edited = state;
        addCircle(&findCadSketchRecord(edited, 2u)->sketch, -0.5, 0.0, 0.2);
        CadRegenerationReport report;
        CadBodyMesh mesh;
        const CadStatus editWhy = regenerateCadBody(edited, &mesh, &report);
        const CadStatus refused = body.applyState(edited);
        r.check("CADV6S2_SHR_01_a_shared_sketch_edit_that_loses_one_feature_s_face_refuses_whole",
                built == CadStatus::Ok && editWhy == CadStatus::PlanarFaceUnresolved
                        && report.failedFeatureId == 3u && refused == CadStatus::PlanarFaceUnresolved
                        && sameCadBodyState(body.state(), state));
    }

    // --- the stale support chooser ------------------------------------------
    {
        IdLifetimeRig rig;
        const bool opened = rig.open(blockState(1.0))
                            && rig.add(kCadFeatureId, rectangleAt(0.0, 0.0, 0.5, 0.5)) == CadStatus::Ok;
        ChosenSupport chosen;
        chosen.kind = ChosenSupport::Kind::Face;
        SketchFrame tapFrame;
        const bool framed = opened
                            && worldCapFrame(rig.scene, rig.bodyId, 2u, CadFaceKind::CapFar, &tapFrame,
                                             &chosen.faceRef);
        chosen.worldFrame = tapFrame;
        ChosenSupport fresh;
        const CadStatus now = refreshChosenSupport(rig.scene, chosen, &fresh);
        // Undo removes feature 2: the stored selection must not begin a sketch.
        const bool undone = rig.history.undo();
        ChosenSupport stale;
        const CadStatus afterUndo = refreshChosenSupport(rig.scene, chosen, &stale);
        // Redo brings it back: valid again. Moving the producer: the frame is
        // recomputed from where the face is NOW, never the tap's.
        const bool redone = rig.history.redo();
        TransformValues shifted;
        shifted.positionX = 3.0;
        rig.scene.findBody(rig.bodyId)->transform().setValues(shifted);
        ChosenSupport movedFresh;
        const CadStatus afterMove = refreshChosenSupport(rig.scene, chosen, &movedFresh);
        r.check("CADV6S2_CHOOSER_01_a_stale_support_refuses_and_a_moved_one_is_re_framed",
                framed && now == CadStatus::Ok && undone && afterUndo != CadStatus::Ok && redone
                        && afterMove == CadStatus::Ok
                        && std::fabs(movedFresh.worldFrame.origin.x - (tapFrame.origin.x + 3.0f)) < 1e-4f);
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// CAD-FOUNDATION-C2 planar-face preflight: BEFORE reproductions
// ---------------------------------------------------------------------------
//
// These cases ENCODE THE CURRENT BEHAVIOUR of the region model for the OWNER's
// two sketches -- a circle crossing a rectangle, and a line-built protrusion
// closed against a rectangle's edge -- plus two crossing circles and the
// supported nested case. They are labelled BEFORE on purpose: the region model
// is loop NESTING, not a planar arrangement (`forgeshape_sketch_region.h`), so
// a crossing never splits a loop and a bounded cell whose boundary is made of
// pieces of two source curves is not a region at all. A later planar-face
// stage (artifacts/cad-foundation-c2/PLANAR_FACE_MODEL_PROPOSAL.md) is
// expected to flip the BEFORE checks it supersedes -- deliberately, by name --
// and keep PF-04 exactly as it is.

namespace {

SketchEntityId addLine(CadSketch* sketch, SketchPoint a, SketchPoint b) {
    SketchLine line;
    line.start = a;
    line.end = b;
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, line, &id);
    return id;
}

// Every edge of the loop comes from its own anchor entity: the loop is ONE
// source curve, never pieces of two.
bool loopIsOneSourceCurve(const ClosedProfile& loop) {
    for (SketchEntityId owner : loop.edgeEntityId) {
        if (owner != loop.anchorEntityId) {
            return false;
        }
    }
    return !loop.edgeEntityId.empty();
}

const ClosedProfile* loopOf(const SketchRegionExtraction& x, SketchEntityId anchor) {
    for (const ClosedProfile& loop : x.loops.profiles) {
        if (loop.anchorEntityId == anchor) {
            return &loop;
        }
    }
    return nullptr;
}

SketchEntityId regionAt(const SketchRegionExtraction& x, double u, double v) {
    SketchEntityId id = kNoSketchEntity;
    return sketchRegionAt(x, SketchPoint{u, v}, &id) ? id : kNoSketchEntity;
}

bool hasRejection(const SketchRegionExtraction& x, SketchEntityId anchor, CadStatus why) {
    for (const ProfileRejection& rejection : x.loops.rejections) {
        if (rejection.anchorEntityId == anchor && rejection.why == why) {
            return true;
        }
    }
    return false;
}

void testPlanarFaceBefore(Recorder& r) {
    // --- PF-01: a rectangle crossed by one circle ------------------------
    // Rectangle 4 x 3 about the origin; circle r = 0.5 at (2, 0) straddles the
    // right edge x = 2 at (2, +-0.5). A planar arrangement has THREE bounded
    // faces: the rectangle minus the lens, the lens, the cap outside.
    {
        CadSketch sketch;
        const SketchEntityId rect = addRect(&sketch, 0.0, 0.0, 4.0, 3.0);
        const SketchEntityId circle = addCircle(&sketch, 2.0, 0.0, 0.5);
        const SketchRegionExtraction x = extractSketchRegions(sketch);
        const bool eachValidAlone =
                validateCadSketch(sketch) == CadStatus::Ok
                && extractSketchRegions(rectSketch(0.0, 0.0, 4.0, 3.0)).regions.size() == 1u
                && extractSketchRegions(circleSketch(2.0, 0.0, 0.5)).regions.size() == 1u;
        r.check("CADFC2_PF_01a_BEFORE_rectangle_and_crossing_circle_are_each_valid", eachValidAlone);
        // The crossings are derivable (x = 2, y = +-sqrt(0.5^2 - 0^2)) but never
        // computed: the rectangle is still its 4 corners, the circle its own
        // 32-gon, and each loop is ONE source curve.
        const ClosedProfile* rl = loopOf(x, rect);
        const ClosedProfile* cl = loopOf(x, circle);
        const bool unsplit = rl != nullptr && cl != nullptr && rl->polygon.size() == 4u
                             && cl->polygon.size() == static_cast<size_t>(kSketchCircleSegments)
                             && loopIsOneSourceCurve(*rl) && loopIsOneSourceCurve(*cl);
        r.check("CADFC2_PF_01b_BEFORE_no_intersection_splits_either_loop", unsplit);
        const bool twoRegions = x.regions.size() == 2u && x.loopsConflict(0, 1)
                                && x.regions[0].status == CadStatus::Ok
                                && x.regions[1].status == CadStatus::Ok
                                && x.regions[0].holeAnchorIds.empty()
                                && x.regions[1].holeAnchorIds.empty();
        r.check("CADFC2_PF_01c_BEFORE_two_whole_loop_regions_not_three_faces", twoRegions);
        // A tap resolves to the smallest WHOLE loop around the point: the lens
        // and the cap outside the rectangle are both "the circle", so neither
        // cell can be chosen on its own.
        const bool taps = regionAt(x, -1.0, 0.0) == rect && regionAt(x, 1.8, 0.0) == circle
                          && regionAt(x, 2.3, 0.0) == circle;
        r.check("CADFC2_PF_01d_BEFORE_lens_and_outer_cap_both_tap_to_the_whole_circle", taps);
        const bool refusal = validateRegionSelection(x, {regionRef(rect), regionRef(circle)})
                                     == CadStatus::OverlappingRegions
                             && validateRegionSelection(x, {regionRef(rect)}) == CadStatus::Ok
                             && std::fabs(x.regions[0].area - 12.0) < 1e-9;
        r.check("CADFC2_PF_01e_BEFORE_both_is_OverlappingRegions_rectangle_alone_ignores_circle",
                refusal);
        // Through the session: Finish succeeds, nothing is chosen (two
        // regions), a lens tap selects the whole circle.
        SessionDriver s;
        const bool began = s.beginWorld(Workplane::XY);
        const SketchEntityId sr =
                began ? s.place(SketchTool::Rectangle, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0},
                                rectangleAt(0.0, 0.0, 4.0, 3.0))
                      : kNoSketchEntity;
        const SketchEntityId sc = s.place(SketchTool::Circle, SketchPoint{2.0, 0.0},
                                          SketchPoint{2.5, 0.0}, circleAt(2.0, 0.0, 0.5));
        const bool finished = s.sketch.finish() == CadStatus::Ok;
        const bool ambiguous = s.sketch.evaluateCandidate().status == CadStatus::AmbiguousProfile;
        s.toggleAt(SketchPoint{1.8, 0.0});
        // FLIPPED by `CAD-V6-S2`, as this stage said it would be: the session
        // now finishes on the THREE atomic faces, and a tap inside the lens
        // chooses the lens alone -- never the whole circle.
        double lensArea = 0.0;
        for (size_t i = 0; i < s.sketch.planarFaceCount(); ++i) {
            double area = 0.0;
            if (s.sketch.planarFaceSelected(i) && s.sketch.planarFaceInfo(i, nullptr, &area)) {
                lensArea = area;
            }
        }
        r.check("CADFC2_PF_01f_S2_session_finishes_on_three_faces_and_a_lens_tap_takes_the_lens",
                sr != kNoSketchEntity && sc != kNoSketchEntity && finished && ambiguous
                        && s.sketch.selectionKind() == CadSelectionKind::PlanarFaces
                        && s.sketch.planarFaceCount() == 3u && s.sketch.selectedAreaCount() == 1u
                        && std::fabs(lensArea - 3.14159265358979 * 0.125) < 1e-9);
        s.sketch.cancel();
    }

    // --- PF-02: a line-built protrusion closed against the rectangle ------
    // Three lines (2,-0.5)->(3,-0.5)->(3,0.5)->(2,0.5) whose ends lie ON the
    // rectangle's right edge, mid-edge. A planar arrangement has TWO faces:
    // the rectangle and the 1 x 1 protrusion.
    {
        CadSketch sketch;
        const SketchEntityId rect = addRect(&sketch, 0.0, 0.0, 4.0, 3.0);
        const SketchEntityId l0 = addLine(&sketch, SketchPoint{2.0, -0.5}, SketchPoint{3.0, -0.5});
        addLine(&sketch, SketchPoint{3.0, -0.5}, SketchPoint{3.0, 0.5});
        addLine(&sketch, SketchPoint{3.0, 0.5}, SketchPoint{2.0, 0.5});
        const SketchRegionExtraction x = extractSketchRegions(sketch);
        // A rectangle edge is never a chain node, so the chain's two ends have
        // degree 1: OpenProfile, anchored by its smallest line.
        r.check("CADFC2_PF_02a_BEFORE_protrusion_chain_is_OpenProfile_T_junction_not_a_node",
                validateCadSketch(sketch) == CadStatus::Ok && x.regions.size() == 1u
                        && x.regions[0].outerAnchorId == rect
                        && hasRejection(x, l0, CadStatus::OpenProfile));
        r.check("CADFC2_PF_02b_BEFORE_protrusion_is_not_tappable",
                regionAt(x, 2.5, 0.0) == kNoSketchEntity && regionAt(x, 1.0, 0.0) == rect);
        // The variant whose end segments CROSS the edge is open the same way.
        CadSketch crossing;
        addRect(&crossing, 0.0, 0.0, 4.0, 3.0);
        const SketchEntityId c0 =
                addLine(&crossing, SketchPoint{1.5, -0.5}, SketchPoint{3.0, -0.5});
        addLine(&crossing, SketchPoint{3.0, -0.5}, SketchPoint{3.0, 0.5});
        addLine(&crossing, SketchPoint{3.0, 0.5}, SketchPoint{1.5, 0.5});
        const SketchRegionExtraction cx = extractSketchRegions(crossing);
        r.check("CADFC2_PF_02c_BEFORE_crossing_protrusion_chain_is_OpenProfile_too",
                cx.regions.size() == 1u && hasRejection(cx, c0, CadStatus::OpenProfile));
        // Drawn CLOSED as one polyline through the edge, it is a whole loop
        // that conflicts with the rectangle: PF-01's behaviour, not a face.
        CadSketch closed;
        const SketchEntityId cr = addRect(&closed, 0.0, 0.0, 4.0, 3.0);
        const SketchEntityId cp = addClosedPolyline(
                &closed, {SketchPoint{1.5, -0.5}, SketchPoint{3.0, -0.5}, SketchPoint{3.0, 0.5},
                          SketchPoint{1.5, 0.5}});
        const SketchRegionExtraction px = extractSketchRegions(closed);
        r.check("CADFC2_PF_02d_BEFORE_a_closed_crossing_polyline_is_a_conflicting_whole_loop",
                px.regions.size() == 2u && px.loopsConflict(0, 1)
                        && regionAt(px, 1.8, 0.0) == cp
                        && validateRegionSelection(px, {regionRef(cr), regionRef(cp)})
                                   == CadStatus::OverlappingRegions);
        // Through the session: Finish succeeds on the rectangle ALONE and
        // auto-selects it; the open protrusion is reported only as a
        // rejection, so the user sees one region and no reason.
        SessionDriver s;
        const bool began = s.beginWorld(Workplane::XY);
        const SketchEntityId sr =
                began ? s.place(SketchTool::Rectangle, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0},
                                rectangleAt(0.0, 0.0, 4.0, 3.0))
                      : kNoSketchEntity;
        SketchLine a;
        a.start = SketchPoint{2.0, -0.5};
        a.end = SketchPoint{3.0, -0.5};
        SketchLine b;
        b.start = SketchPoint{3.0, -0.5};
        b.end = SketchPoint{3.0, 0.5};
        SketchLine c;
        c.start = SketchPoint{3.0, 0.5};
        c.end = SketchPoint{2.0, 0.5};
        const bool lines =
                s.place(SketchTool::Line, SketchPoint{2.2, -0.5}, SketchPoint{2.8, -0.5}, a)
                        != kNoSketchEntity
                && s.place(SketchTool::Line, SketchPoint{3.0, -0.3}, SketchPoint{3.0, 0.3}, b)
                           != kNoSketchEntity
                && s.place(SketchTool::Line, SketchPoint{2.8, 0.5}, SketchPoint{2.2, 0.5}, c)
                           != kNoSketchEntity;
        const bool finished = s.sketch.finish() == CadStatus::Ok;
        const size_t regions = s.sketch.regions().regions.size();
        const bool tapped = s.toggleAt(SketchPoint{2.5, 0.0});
        // FLIPPED by `CAD-V6-S2`: the protrusion closed by T-junctions is a
        // face, the session finishes on TWO faces (the region model still sees
        // one loop), nothing is guessed, and the protrusion is selectable.
        double protrusionArea = 0.0;
        for (size_t i = 0; i < s.sketch.planarFaceCount(); ++i) {
            double area = 0.0;
            if (s.sketch.planarFaceSelected(i) && s.sketch.planarFaceInfo(i, nullptr, &area)) {
                protrusionArea = area;
            }
        }
        r.check("CADFC2_PF_02e_S2_session_finishes_on_two_faces_and_the_protrusion_is_selectable",
                sr != kNoSketchEntity && lines && s.sketch.sketch().entities.size() == 4u
                        && finished && regions == 1u
                        && s.sketch.selectionKind() == CadSelectionKind::PlanarFaces
                        && s.sketch.planarFaceCount() == 2u && tapped
                        && s.sketch.selectedAreaCount() == 1u && std::fabs(protrusionArea - 1.0) < 1e-9);
        s.sketch.cancel();
    }

    // --- PF-03: two crossing circles ---------------------------------------
    // r = 1 at (-0.4, 0) and (0.4, 0). A planar arrangement has THREE bounded
    // faces: two crescents and the lens.
    {
        CadSketch sketch;
        const SketchEntityId a = addCircle(&sketch, -0.4, 0.0, 1.0);
        const SketchEntityId b = addCircle(&sketch, 0.4, 0.0, 1.0);
        const SketchRegionExtraction x = extractSketchRegions(sketch);
        r.check("CADFC2_PF_03a_BEFORE_two_crossing_circles_are_two_conflicting_loops",
                x.regions.size() == 2u && x.loopsConflict(0, 1)
                        && loopIsOneSourceCurve(*loopOf(x, a))
                        && loopIsOneSourceCurve(*loopOf(x, b)));
        // Equal areas: the lens resolves to the first loop by anchor order --
        // an ordering accident, not a face.
        r.check("CADFC2_PF_03b_BEFORE_the_lens_taps_to_a_whole_circle_by_anchor_order",
                regionAt(x, 0.0, 0.0) == a && regionAt(x, -1.2, 0.0) == a
                        && regionAt(x, 1.2, 0.0) == b);
        r.check("CADFC2_PF_03c_BEFORE_both_circles_is_OverlappingRegions",
                validateRegionSelection(x, {regionRef(a), regionRef(b)})
                        == CadStatus::OverlappingRegions);
    }

    // --- PF-04: the supported nested case stays exactly as it is ------------
    {
        CadSketch sketch;
        const SketchEntityId o = addRect(&sketch, 0.0, 0.0, 4.0, 3.0);
        const SketchEntityId a = addCircle(&sketch, -1.0, 0.0, 0.5);
        const SketchEntityId b = addCircle(&sketch, 1.0, 0.0, 0.5);
        const SketchRegionExtraction x = extractSketchRegions(sketch);
        const SketchRegion* outer = findSketchRegion(x, o);
        const bool nested = x.regions.size() == 3u && outer != nullptr
                            && outer->holeAnchorIds == std::vector<SketchEntityId>{a, b}
                            && !x.loopsConflict(0, 1) && !x.loopsConflict(0, 2)
                            && regionAt(x, 0.0, 0.0) == o && regionAt(x, -1.0, 0.0) == a
                            && regionAt(x, 1.0, 0.0) == b;
        r.check("CADFC2_PF_04a_nested_rectangle_and_two_circles_is_three_regions", nested);
        const bool union3 =
                validateRegionSelection(x, {regionRef(o, {a, b}), regionRef(a), regionRef(b)})
                == CadStatus::Ok;
        r.check("CADFC2_PF_04b_nested_union_of_all_three_is_still_valid", union3);
    }
}

}  // namespace

int runCadFeatureSelfTests(CadFeatureSelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};
    // The kernel gate first: nothing below may rely on the seam before it passed.
    runKernelGate(r);
    testRegions(r);
    testRegionUnion(r);
    testPlanarFaceBefore(r);
    testExtrusion(r);
    testChain(r);
    testSession(r);
    testPersistence(r);
    testIdLifetimeBefore(r);
    testIdLifetime(r);
    testPlanarRuntime(r);
    measurePerformance(r);
    // The planar arrangement (`CAD-PLANAR-FACE-PF-S1`): derived-only, wired to
    // nothing yet, so it rides in this suite rather than a startup token of
    // its own. Its cap-sketch timing joins this suite's performance line.
    std::vector<ArrangementSelfTestCheck> arrangement;
    std::string arrangementPerformance;
    runSketchArrangementSelfTests(&arrangement, &arrangementPerformance);
    for (const ArrangementSelfTestCheck& check : arrangement) {
        r.check(check.name, check.passed);
    }
    g_performance += " " + arrangementPerformance;
    // The retained sketch table, the selection variant and `CADB` v6
    // (`CAD-V6-S1`): model and persistence only, wired to no session, JNI or
    // UI path, so -- like the arrangement -- they ride in this suite.
    std::vector<CadV6SelfTestCheck> v6;
    std::string v6Digests;
    runCadV6SelfTests(&v6, &v6Digests);
    for (const CadV6SelfTestCheck& check : v6) {
        r.check(check.name, check.passed);
    }
    g_digests += " " + v6Digests;
    return r.n;
}

const char* cadFeaturePerformanceReport() {
    return g_performance.c_str();
}

const char* cadFeatureFixtureDigests() {
    return g_digests.c_str();
}

}  // namespace forgeshape
