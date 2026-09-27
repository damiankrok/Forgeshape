#include "forgeshape_cad_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_camera.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_history.h"
#include "forgeshape_picking.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_sketch_region.h"
#include "forgeshape_workplane.h"

namespace forgeshape {
namespace {

struct Recorder {
    CadSelfTestResult* out;
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

constexpr double kTol = 1e-9;

bool near(double a, double b, double tol = kTol) { return std::fabs(a - b) <= tol; }
bool nearf(float a, float b, float tol = 1e-5f) { return std::fabs(a - b) <= tol; }

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

CadSketch rectangleSketch(Workplane plane = Workplane::XY, double w = 2.0, double h = 1.0) {
    CadSketch sketch;
    sketch.plane = plane;
    SketchRectangle rectangle;
    rectangle.center = SketchPoint{0.5, 0.25};
    rectangle.width = w;
    rectangle.height = h;
    addSketchEntity(&sketch, rectangle);
    return sketch;
}

CadSketch circleSketch(Workplane plane = Workplane::XY, double r = 0.75) {
    CadSketch sketch;
    sketch.plane = plane;
    SketchCircle circle;
    circle.center = SketchPoint{-0.5, 0.5};
    circle.radius = r;
    addSketchEntity(&sketch, circle);
    return sketch;
}

// A regular n-gon polyline, closed, of the given radius.
CadSketch polygonSketch(uint32_t n, bool closed, double radius = 1.0) {
    CadSketch sketch;
    SketchPolyline polyline;
    for (uint32_t i = 0; i < n; ++i) {
        const double a = 2.0 * 3.14159265358979323846 * i / n;
        polyline.vertices.push_back(SketchPoint{radius * std::cos(a), radius * std::sin(a)});
    }
    polyline.closed = closed;
    addSketchEntity(&sketch, std::move(polyline));
    return sketch;
}

CadBodyState bodyState(const CadSketch& sketch, SketchEntityId profile, double depth = 1.5,
                       ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    CadBodyState state;
    state.sketch = sketch;
    state.extrude.profileEntityId = profile;
    state.extrude.depth = depth;
    state.extrude.direction = direction;
    return state;
}

// Every undirected edge of a closed triangle mesh is on exactly two
// triangles, traversed in opposite directions.
bool watertight(const ConstructionMesh& mesh) {
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
            return false;  // a directed edge used twice: two faces wound the same way
        }
        const auto twin = directed.find({entry.first.second, entry.first.first});
        if (twin == directed.end() || twin->second != 1) {
            return false;  // an edge with no partner: a hole
        }
    }
    return true;
}

bool trianglesNonDegenerate(const ConstructionMesh& mesh) {
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const MeshVertex& a = mesh.vertices[mesh.indices[t]];
        const MeshVertex& b = mesh.vertices[mesh.indices[t + 1]];
        const MeshVertex& c = mesh.vertices[mesh.indices[t + 2]];
        const Vec3 ab{b.position[0] - a.position[0], b.position[1] - a.position[1],
                      b.position[2] - a.position[2]};
        const Vec3 ac{c.position[0] - a.position[0], c.position[1] - a.position[1],
                      c.position[2] - a.position[2]};
        const Vec3 n = vec3Cross(ab, ac);
        if (!(vec3Dot(n, n) > 1e-18f)) {
            return false;
        }
    }
    return true;
}

// The signed volume by the divergence theorem: positive exactly when every
// face is wound counter-clockwise seen from OUTSIDE. Unlike the centroid
// heuristic below it is exact for a concave solid, whose notch faces
// legitimately look toward the centroid.
double signedVolume(const ConstructionMesh& mesh) {
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

bool canonicalWinding(const ConstructionMesh& mesh) {
    TriangleMeshView view;
    view.positions = &mesh.vertices[0].position[0];
    view.positionStride = sizeof(MeshVertex);
    view.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    view.indices = mesh.indices.data();
    view.indexCount = static_cast<uint32_t>(mesh.indices.size());
    Vec3 center{0.0f, 0.0f, 0.0f};
    for (const MeshVertex& v : mesh.vertices) {
        center = vec3Add(center, Vec3{v.position[0], v.position[1], v.position[2]});
    }
    center = vec3Scale(center, 1.0f / static_cast<float>(mesh.vertices.size()));
    return meshObeysCanonicalWinding(view, center);
}

void extents(const ConstructionMesh& mesh, float* lo, float* hi) {
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

bool sameMesh(const ConstructionMesh& a, const ConstructionMesh& b) {
    if (a.vertices.size() != b.vertices.size() || a.indices != b.indices) {
        return false;
    }
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (std::memcmp(a.vertices[i].position, b.vertices[i].position, sizeof(float) * 3) != 0) {
            return false;
        }
    }
    return true;
}

// A camera looking straight at a plane at a known viewport, as the product
// frames a sketch.
CameraController sketchCamera(Workplane plane, int w, int h) {
    CameraController camera;
    camera.setViewport(w, h);
    float yaw = 0.0f;
    float pitch = 0.0f;
    SketchSession::sketchViewAngles(plane, &yaw, &pitch);
    camera.frameWorkplane(yaw, pitch);
    return camera;
}

struct Touch {
    SketchSession& session;
    const CameraSnapshot& camera;
    int w;
    int h;

    bool at(TouchAction action, float x, float y, int32_t id = 7) const {
        TouchPointer p{id, x, y};
        return session.onTouch(action, action == TouchAction::Up ? id : -1, &p, 1, camera, w, h);
    }
    bool sketchPoint(const SketchPoint& point, float* x, float* y) const {
        return session.sketchToScreen(camera, point, w, h, x, y);
    }
    // A drag from one sketch point to another, through real Down/Move/Up.
    bool drag(const SketchPoint& from, const SketchPoint& to) const {
        float x0, y0, x1, y1;
        if (!sketchPoint(from, &x0, &y0) || !sketchPoint(to, &x1, &y1)) {
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
    bool tap(const SketchPoint& where) const {
        float x, y;
        if (!sketchPoint(where, &x, &y)) {
            return false;
        }
        return at(TouchAction::Down, x, y) && at(TouchAction::Up, x, y);
    }
};

double microseconds(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
            .count();
}

// ---------------------------------------------------------------------------
// The extrusion's EXTENT (`CAD-EXT-R1`, CADEXT-02..05, 08..11)
// ---------------------------------------------------------------------------
//
// One Side, Symmetric and Two Sides are three ways of authoring the SAME two
// distances -- one along `+N`, one along `-N` -- so almost every case below is
// written in those two numbers and never in the mode. What the mode decides is
// which of them the controls write, and the transition policy is the one place
// that is stated.


CadBodyState extentBody(ExtrudeExtentMode mode, double primary, double second,
                        ExtrudeDirection direction = ExtrudeDirection::AlongNormal,
                        Workplane plane = Workplane::XY) {
    CadBodyState state = bodyState(rectangleSketch(plane), 1, primary, direction);
    state.extrude.extent = mode;
    state.extrude.secondDistance = second;
    return state;
}

// The two offsets a mesh actually spans along the plane normal, measured off
// the generated vertices rather than read back out of the parameters -- the
// point being that the GEOMETRY is what the extent means.
bool meshSpan(const ConstructionMesh& mesh, Workplane plane, double* outNear, double* outFar) {
    if (mesh.vertices.empty()) {
        return false;
    }
    const WorkplaneFrame frame = workplaneFrame(plane);
    double lo = 1.0e30;
    double hi = -1.0e30;
    for (const MeshVertex& v : mesh.vertices) {
        const double d = static_cast<double>(v.position[0]) * frame.normal.x
                         + static_cast<double>(v.position[1]) * frame.normal.y
                         + static_cast<double>(v.position[2]) * frame.normal.z;
        lo = d < lo ? d : lo;
        hi = d > hi ? d : hi;
    }
    *outNear = lo;
    *outFar = hi;
    return true;
}

ProjectDocument extentDocumentFor(const CadBodyState& state) {
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

// The project fingerprint of a scene holding exactly this one CAD body, so a
// case can say that a real extent change moves it without building a scene by
// hand each time.
uint64_t cadStateFingerprintProbe(const CadBodyState& state) {
    ConstructionScene scene((NoProjectTag()));
    CadStatus why = CadStatus::Ok;
    SceneObject* object = scene.addCadBody(state, &why);
    if (object == nullptr) {
        return 0;
    }
    publishSceneObject(*object);
    return projectSemanticFingerprint(scene, ProjectKind::Construction);
}

// Where the `CADB` payload starts inside a file, and the CRC repair that makes
// a byte-patched fixture a well-formed file again -- so only the SEMANTIC check
// can refuse it, which is the whole point of a corrupt fixture.
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

void testExtent(Recorder& r) {
    // CADEXT-02: a default state, and every state built before this stage, is
    // One Side and reduces to exactly the depth and direction it always had.
    {
        const CadBodyState along = extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.0);
        const CadBodyState against = extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.0,
                                                ExtrudeDirection::AgainstNormal);
        ExtrudeFeature fresh;
        r.check("CADEXT_02_a_the_default_extent_is_one_side_and_reduces_to_the_legacy_pair",
                fresh.extent == ExtrudeExtentMode::OneSide
                        && near(extrudePositiveDistance(along.extrude), 1.5)
                        && near(extrudeNegativeDistance(along.extrude), 0.0)
                        && near(extrudePositiveDistance(against.extrude), 0.0)
                        && near(extrudeNegativeDistance(against.extrude), 1.5));
        ConstructionMesh alongMesh;
        ConstructionMesh againstMesh;
        double n0 = 0.0;
        double f0 = 0.0;
        double n1 = 0.0;
        double f1 = 0.0;
        const bool built = generateCadMesh(along, &alongMesh) == CadStatus::Ok
                           && generateCadMesh(against, &againstMesh) == CadStatus::Ok
                           && meshSpan(alongMesh, Workplane::XY, &n0, &f0)
                           && meshSpan(againstMesh, Workplane::XY, &n1, &f1);
        r.check("CADEXT_02_b_one_side_still_spans_zero_to_depth_on_the_chosen_side",
                built && near(n0, 0.0) && near(f0, 1.5) && near(n1, -1.5) && near(f1, 0.0)
                        && watertight(alongMesh) && watertight(againstMesh));
    }

    // CADEXT-03/04: the geometry of the two new modes, on all three world
    // planes. There is no per-axis special case and no sign inversion: the
    // solid spans exactly `-negative .. +positive` every time.
    {
        const Workplane planes[3] = {Workplane::XY, Workplane::XZ, Workplane::YZ};
        bool symmetricOk = true;
        bool twoSidesOk = true;
        for (const Workplane plane : planes) {
            ConstructionMesh mesh;
            double lo = 0.0;
            double hi = 0.0;
            const CadBodyState sym = extentBody(ExtrudeExtentMode::Symmetric, 0.75, 0.0,
                                                ExtrudeDirection::AlongNormal, plane);
            symmetricOk &= generateCadMesh(sym, &mesh) == CadStatus::Ok
                           && meshSpan(mesh, plane, &lo, &hi) && near(lo, -0.75)
                           && near(hi, 0.75) && watertight(mesh)
                           && near(extrudePositiveDistance(sym.extrude),
                                   extrudeNegativeDistance(sym.extrude));
            ConstructionMesh two;
            const CadBodyState pair = extentBody(ExtrudeExtentMode::TwoSides, 1.25, 0.5,
                                                 ExtrudeDirection::AlongNormal, plane);
            twoSidesOk &= generateCadMesh(pair, &two) == CadStatus::Ok
                          && meshSpan(two, plane, &lo, &hi) && near(lo, -0.5) && near(hi, 1.25)
                          && watertight(two);
        }
        r.check("CADEXT_03_symmetric_spans_minus_d_to_plus_d_on_every_plane", symmetricOk);
        r.check("CADEXT_04_a_two_sides_spans_minus_b_to_plus_a_on_every_plane", twoSidesOk);
    }

    // CADEXT-04 b: A and B are independent. Editing one leaves the other bit
    // for bit where it was.
    {
        const CadBodyState base = extentBody(ExtrudeExtentMode::TwoSides, 1.25, 0.5);
        const ExtrudeFeature movedA = extrudeFeatureWithSide(base.extrude, true, 2.0);
        const ExtrudeFeature movedB = extrudeFeatureWithSide(base.extrude, false, 0.125);
        r.check("CADEXT_04_b_editing_one_side_does_not_move_the_other",
                near(extrudePositiveDistance(movedA), 2.0)
                        && near(extrudeNegativeDistance(movedA), 0.5)
                        && near(extrudePositiveDistance(movedB), 1.25)
                        && near(extrudeNegativeDistance(movedB), 0.125));
    }

    // Symmetric: EITHER side writes the one shared distance, so a drag on one
    // arrow keeps the two equal without a second rule saying so.
    {
        const CadBodyState base = extentBody(ExtrudeExtentMode::Symmetric, 0.75, 0.0);
        const ExtrudeFeature viaPositive = extrudeFeatureWithSide(base.extrude, true, 1.5);
        const ExtrudeFeature viaNegative = extrudeFeatureWithSide(base.extrude, false, 1.5);
        r.check("CADEXT_03_b_either_symmetric_handle_writes_the_one_shared_distance",
                near(extrudePositiveDistance(viaPositive), 1.5)
                        && near(extrudeNegativeDistance(viaPositive), 1.5)
                        && near(extrudePositiveDistance(viaNegative), 1.5)
                        && near(extrudeNegativeDistance(viaNegative), 1.5));
    }

    // CADEXT-05: every transition of the policy, including that no transition
    // can produce a negative depth and that the side choice is deterministic.
    {
        const ExtrudeFeature oneAlong = extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.0).extrude;
        const ExtrudeFeature oneAgainst =
                extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.0, ExtrudeDirection::AgainstNormal)
                        .extrude;
        const ExtrudeFeature symmetric = extentBody(ExtrudeExtentMode::Symmetric, 0.75, 0.0).extrude;
        const ExtrudeFeature twoSides = extentBody(ExtrudeExtentMode::TwoSides, 1.25, 0.5).extrude;

        const ExtrudeFeature a = extrudeFeatureWithExtent(oneAlong, ExtrudeExtentMode::Symmetric,
                                                          ExtrudeDirection::AlongNormal);
        const ExtrudeFeature b = extrudeFeatureWithExtent(oneAlong, ExtrudeExtentMode::TwoSides,
                                                          ExtrudeDirection::AlongNormal);
        r.check("CADEXT_05_a_one_side_carries_its_depth_onto_both_sides",
                near(extrudePositiveDistance(a), 1.5) && near(extrudeNegativeDistance(a), 1.5)
                        && near(extrudePositiveDistance(b), 1.5)
                        && near(extrudeNegativeDistance(b), 1.5));

        const ExtrudeFeature c = extrudeFeatureWithExtent(symmetric, ExtrudeExtentMode::OneSide,
                                                          ExtrudeDirection::AgainstNormal);
        const ExtrudeFeature d = extrudeFeatureWithExtent(symmetric, ExtrudeExtentMode::TwoSides,
                                                          ExtrudeDirection::AlongNormal);
        r.check("CADEXT_05_b_symmetric_leaves_on_the_preferred_side_with_a_positive_depth",
                c.extent == ExtrudeExtentMode::OneSide
                        && c.direction == ExtrudeDirection::AgainstNormal && near(c.depth, 0.75)
                        && c.depth > 0.0 && near(extrudeNegativeDistance(c), 0.75)
                        && near(extrudePositiveDistance(c), 0.0)
                        && near(extrudePositiveDistance(d), 0.75)
                        && near(extrudeNegativeDistance(d), 0.75));

        const ExtrudeFeature e = extrudeFeatureWithExtent(twoSides, ExtrudeExtentMode::Symmetric,
                                                          ExtrudeDirection::AlongNormal);
        const ExtrudeFeature f = extrudeFeatureWithExtent(twoSides, ExtrudeExtentMode::Symmetric,
                                                          ExtrudeDirection::AgainstNormal);
        // The PREFERRED side's own value, on both sides -- never the average of
        // A and B, which is a number the user never typed.
        r.check("CADEXT_05_c_two_sides_to_symmetric_takes_a_side_and_never_an_average",
                near(e.depth, 1.25) && near(f.depth, 0.5)
                        && !near(e.depth, 0.5 * (1.25 + 0.5)));

        const ExtrudeFeature g = extrudeFeatureWithExtent(twoSides, ExtrudeExtentMode::OneSide,
                                                          ExtrudeDirection::AlongNormal);
        const ExtrudeFeature h = extrudeFeatureWithExtent(twoSides, ExtrudeExtentMode::OneSide,
                                                          ExtrudeDirection::AgainstNormal);
        r.check("CADEXT_05_d_two_sides_to_one_side_keeps_the_chosen_side_and_zeroes_the_other",
                near(extrudePositiveDistance(g), 1.25) && near(extrudeNegativeDistance(g), 0.0)
                        && near(extrudeNegativeDistance(h), 0.5)
                        && near(extrudePositiveDistance(h), 0.0) && g.depth > 0.0 && h.depth > 0.0);

        // A round trip through a two-sided mode gives the SIDE back, which is
        // the whole reason the preferred side is an argument.
        const ExtrudeFeature back = extrudeFeatureWithExtent(
                extrudeFeatureWithExtent(oneAgainst, ExtrudeExtentMode::Symmetric,
                                         ExtrudeDirection::AgainstNormal),
                ExtrudeExtentMode::OneSide, ExtrudeDirection::AgainstNormal);
        r.check("CADEXT_05_e_a_mode_round_trip_restores_the_one_side_state_bit_for_bit",
                back.extent == oneAgainst.extent && back.direction == oneAgainst.direction
                        && back.depth == oneAgainst.depth
                        && back.secondDistance == oneAgainst.secondDistance);

        // A transition to the mode already held changes nothing at all.
        const ExtrudeFeature same = extrudeFeatureWithExtent(twoSides, ExtrudeExtentMode::TwoSides,
                                                             ExtrudeDirection::AlongNormal);
        r.check("CADEXT_05_f_a_transition_to_the_mode_already_held_changes_nothing",
                same.depth == twoSides.depth && same.secondDistance == twoSides.secondDistance
                        && same.extent == twoSides.extent);
    }

    // The canonical form, and the refusals. One solid has exactly ONE encoding,
    // so a stated direction where there is no side to choose, and a second
    // distance where there is no second side, are refused by name.
    {
        CadBodyState crooked = extentBody(ExtrudeExtentMode::Symmetric, 0.75, 0.0);
        crooked.extrude.direction = ExtrudeDirection::AgainstNormal;
        CadBodyState stray = extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.25);
        CadBodyState empty = extentBody(ExtrudeExtentMode::TwoSides, 0.0, 0.0);
        CadBodyState zeroSymmetric = extentBody(ExtrudeExtentMode::Symmetric, 0.0, 0.0);
        CadBodyState oneZeroSide = extentBody(ExtrudeExtentMode::TwoSides, 0.0, 0.75);
        r.check("CADEXT_05_g_a_non_canonical_extent_is_refused_by_name",
                validateCadBodyState(crooked) == CadStatus::InvalidExtrudeExtent
                        && validateCadBodyState(stray) == CadStatus::InvalidExtrudeExtent);
        r.check("CADEXT_05_h_an_extrusion_with_no_extent_is_refused_never_clamped",
                validateCadBodyState(empty) == CadStatus::InvalidExtrudeDepth
                        && validateCadBodyState(zeroSymmetric) == CadStatus::InvalidExtrudeDepth
                        && validateCadBodyState(oneZeroSide) == CadStatus::Ok);
        CadBodyState nan = extentBody(ExtrudeExtentMode::TwoSides, 1.0, 0.5);
        nan.extrude.secondDistance = std::nan("");
        CadBodyState negative = extentBody(ExtrudeExtentMode::TwoSides, 1.0, -0.5);
        r.check("CADEXT_05_i_a_non_finite_or_negative_side_is_refused",
                validateCadBodyState(nan) == CadStatus::NonFinite
                        && validateCadBodyState(negative) == CadStatus::InvalidExtrudeDepth);
    }

    // CADEXT-08: `sameCadBodyState` notices a real extent change and a history
    // step therefore records one. It notices the MODE as well as the numbers.
    {
        const CadBodyState one = extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.0);
        const CadBodyState sym = extentBody(ExtrudeExtentMode::Symmetric, 1.5, 0.0);
        const CadBodyState twoA = extentBody(ExtrudeExtentMode::TwoSides, 1.25, 0.5);
        CadBodyState twoB = twoA;
        twoB.extrude.secondDistance = 0.25;
        r.check("CADEXT_08_a_state_equality_notices_the_mode_and_the_second_distance",
                !sameCadBodyState(one, sym) && !sameCadBodyState(twoA, twoB)
                        && sameCadBodyState(twoA, twoA));
        const uint64_t oneHash = cadStateFingerprintProbe(one);
        const uint64_t symHash = cadStateFingerprintProbe(sym);
        const uint64_t twoHash = cadStateFingerprintProbe(twoB);
        r.check("CADEXT_08_b_the_project_fingerprint_moves_with_a_real_extent_change",
                oneHash != symHash && symHash != twoHash);
    }

    // CADEXT-08 c: Undo/Redo restores the mode, A, B and the direction, through
    // the Construction history that already stores the whole CAD truth.
    {
        ConstructionScene scene((NoProjectTag()));
        ConstructionHistory history(scene);
        CadStatus why = CadStatus::Ok;
        SceneObject* object =
                scene.addCadBody(extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.0,
                                            ExtrudeDirection::AgainstNormal),
                                 &why);
        bool ok = object != nullptr && why == CadStatus::Ok;
        CadBodyState before;
        CadBodyState edited;
        if (ok) {
            CadBody* body = object->cadOrNull();
            ok = body != nullptr;
            if (ok) {
                before = body->state();
                CadBodyState candidate = before;
                candidate.extrude = extrudeFeatureWithExtent(before.extrude,
                                                             ExtrudeExtentMode::TwoSides,
                                                             ExtrudeDirection::AgainstNormal);
                candidate.extrude = extrudeFeatureWithSide(candidate.extrude, true, 2.5);
                {
                    ScopedConstructionEdit edit(history);
                    ok &= body->applyState(candidate) == CadStatus::Ok;
                }
                edited = body->state();
                ok &= history.undo() && sameCadBodyState(body->state(), before);
                ok &= history.redo() && sameCadBodyState(body->state(), edited);
            }
        }
        r.check("CADEXT_08_c_undo_and_redo_restore_the_whole_extent",
                ok && edited.extrude.extent == ExtrudeExtentMode::TwoSides
                        && near(extrudePositiveDistance(edited.extrude), 2.5)
                        && near(extrudeNegativeDistance(edited.extrude), 1.5));
    }

    // CADEXT-11: the semantic face topology is unchanged by an extent edit --
    // the signature takes no distance and no mode -- while the face FRAMES
    // legitimately move with the caps, which is what a dependent follows.
    {
        const CadBodyState one = extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.0);
        const CadBodyState sym = extentBody(ExtrudeExtentMode::Symmetric, 0.75, 0.0);
        const CadBodyState two = extentBody(ExtrudeExtentMode::TwoSides, 1.25, 0.5);
        std::vector<CadFace> oneFaces;
        std::vector<CadFace> symFaces;
        const bool enumerated = enumerateCadFaces(one, &oneFaces) == CadStatus::Ok
                                && enumerateCadFaces(sym, &symFaces) == CadStatus::Ok;
        bool sameTokens = enumerated && oneFaces.size() == symFaces.size();
        for (size_t i = 0; sameTokens && i < oneFaces.size(); ++i) {
            sameTokens = oneFaces[i].token.kind == symFaces[i].token.kind
                         && oneFaces[i].token.edgeEntityId == symFaces[i].token.edgeEntityId
                         && oneFaces[i].token.edgeLocalIndex == symFaces[i].token.edgeLocalIndex
                         && oneFaces[i].eligible == symFaces[i].eligible;
        }
        r.check("CADEXT_11_a_an_extent_edit_keeps_the_face_tokens_and_the_lineage_signature",
                sameTokens && cadTopologySignature(one) == cadTopologySignature(sym)
                        && cadTopologySignature(one) == cadTopologySignature(two));
        // `CapPlane` is the cap the extrusion grows FROM and `CapFar` the one it
        // grows TO. For One Side that is still the cap ON the plane; for
        // Symmetric it is the `-N` cap, at exactly `-d`.
        CadFace planeCap;
        CadFace farCap;
        const bool resolved =
                resolveCadFace(sym, CadFaceToken{CadFaceKind::CapPlane, 0, 0}, &planeCap)
                        == CadStatus::Ok
                && resolveCadFace(sym, CadFaceToken{CadFaceKind::CapFar, 0, 0}, &farCap)
                           == CadStatus::Ok;
        r.check("CADEXT_11_b_the_caps_sit_at_the_two_authored_distances",
                resolved && nearf(planeCap.origin.z, -0.75f) && nearf(farCap.origin.z, 0.75f));
    }

    // CADEXT-09: a One Side project still writes the version it always wrote,
    // and only a real extent forces v4.
    {
        uint16_t version = 0;
        const std::vector<uint8_t> legacy =
                encodeProjectV1(extentDocumentFor(extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.0)));
        const bool legacyV1 = !legacy.empty() && cadbSectionVersion(legacy, &version)
                              && version == kCadSectionVersion;
        const std::vector<uint8_t> symmetric = encodeProjectV1(
                extentDocumentFor(extentBody(ExtrudeExtentMode::Symmetric, 0.75, 0.0)));
        const bool symmetricV4 = !symmetric.empty() && cadbSectionVersion(symmetric, &version)
                                 && version == kCadSectionVersionV4;
        const std::vector<uint8_t> two = encodeProjectV1(
                extentDocumentFor(extentBody(ExtrudeExtentMode::TwoSides, 1.25, 0.5)));
        const bool twoV4 = !two.empty() && cadbSectionVersion(two, &version)
                           && version == kCadSectionVersionV4;
        r.check("CADEXT_09_a_a_one_side_project_still_writes_the_version_it_always_wrote",
                legacyV1);
        r.check("CADEXT_09_b_only_a_real_extent_forces_CADB_v4", symmetricV4 && twoV4);
    }

    // CADEXT-10: the v4 round trip is bit-exact and the writer deterministic.
    {
        bool allOk = true;
        const CadBodyState states[3] = {
                extentBody(ExtrudeExtentMode::Symmetric, 0.75, 0.0),
                extentBody(ExtrudeExtentMode::TwoSides, 1.25, 0.5),
                extentBody(ExtrudeExtentMode::TwoSides, 0.0, 0.75),
        };
        for (const CadBodyState& state : states) {
            const ProjectDocument original = extentDocumentFor(state);
            const std::vector<uint8_t> bytes = encodeProjectV1(original);
            ProjectDocument decoded;
            const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &decoded);
            allOk &= why == ProjectCodecStatus::Ok && sameProjectDocument(original, decoded)
                     && encodeProjectV1(decoded) == bytes;
        }
        r.check("CADEXT_10_a_every_v4_extent_round_trips_bit_exactly", allOk);
    }

    // CADEXT-10 b: the v4 decoder fails closed, and a refusal writes nothing.
    // The extent code, the canonical form and the distances are each checked
    // against a file built with the bad value in place.
    {
        std::vector<uint8_t> good = encodeProjectV1(
                extentDocumentFor(extentBody(ExtrudeExtentMode::TwoSides, 1.25, 0.5)));
        struct Bad {
            const char* name;
            size_t offset;   // within the CADB payload
            uint8_t byte;    // for a one-byte corruption
            bool isDouble;
            double value;
        };
        // The v4 one-body record: bodyCount u32, objectId u64, plane u8,
        // supportKind u8, nextEntityId u32, profileEntityId u32, extentCode u8,
        // directionCode u8, depth f64, secondDistance f64.
        const Bad cases[3] = {
                {"extent code 9", 22, 9, false, 0.0},
                {"direction 2 under Two Sides", 23, 2, false, 0.0},
                {"both distances zero", 24, 0, true, 0.0},
        };
        bool allRefused = true;
        for (const Bad& bad : cases) {
            std::vector<uint8_t> bytes = good;
            const size_t payload = cadPayloadOffset(bytes);
            if (payload == 0) {
                allRefused = false;
                break;
            }
            if (bad.isDouble) {
                const double zero = 0.0;
                std::memcpy(&bytes[payload + 24], &zero, sizeof(double));
                std::memcpy(&bytes[payload + 32], &zero, sizeof(double));
            } else {
                bytes[payload + bad.offset] = bad.byte;
            }
            repairCadPayloadCrc(&bytes);
            ProjectDocument decoded;
            const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &decoded);
            allRefused &= why != ProjectCodecStatus::Ok && !decoded.hasCad
                          && decoded.cad.bodies.empty();
        }
        r.check("CADEXT_10_b_a_bad_v4_extent_is_refused_and_nothing_is_written", allRefused);
    }

    // A v1/v2/v3 record decodes to One Side with a second distance of exactly
    // zero -- the legacy pair, meaning what it always meant.
    {
        const std::vector<uint8_t> legacy = encodeProjectV1(extentDocumentFor(
                extentBody(ExtrudeExtentMode::OneSide, 1.5, 0.0, ExtrudeDirection::AgainstNormal)));
        ProjectDocument decoded;
        const ProjectCodecStatus why = decodeProject(legacy.data(), legacy.size(), &decoded);
        const bool oneBody = why == ProjectCodecStatus::Ok && decoded.cad.bodies.size() == 1;
        r.check("CADEXT_09_c_a_legacy_record_decodes_to_one_side_with_no_second_distance",
                oneBody
                        && decoded.cad.bodies[0].state.extrude.extent
                                   == ExtrudeExtentMode::OneSide
                        && decoded.cad.bodies[0].state.extrude.secondDistance == 0.0
                        && decoded.cad.bodies[0].state.extrude.direction
                                   == ExtrudeDirection::AgainstNormal);
    }
}

// ---------------------------------------------------------------------------
// The independent corpus: `CADB` v4 (`CADEXT-10`)
// ---------------------------------------------------------------------------
//
// These six documents are the C++ side of the six `CADB` v4 fixtures
// `scripts/build-forge-corpus.ps1` writes from `DATA_PACKAGE_SPEC.md` §7e. The
// PowerShell builder shares no line with the codec, so a digest agreeing here
// is the format being a SPECIFICATION rather than whatever this encoder happens
// to emit. Every number is an exact binary fraction, so the two implementations
// agree byte for byte or not at all.


std::string g_symmetricDigest = "not measured";
std::string g_twoSidesDigest = "not measured";
std::string g_faceExtentDigest = "not measured";
std::string g_mixedExtentDigest = "not measured";
std::string g_badExtentDigest = "not measured";
std::string g_badTwoSidesDigest = "not measured";

TransformValues corpusPlacementAt(double tx, double ty, double tz) {
    TransformValues t;
    t.positionX = tx;
    t.positionY = ty;
    t.positionZ = tz;
    return t;
}

ProjectBodyPlacement corpusSceneBody(ObjectId id, const TransformValues& transform) {
    ProjectBodyPlacement body;
    body.objectId = id;
    body.transform = transform;
    return body;
}

CadBodyState corpusRectangleState(double w, double h, Workplane plane) {
    CadBodyState state;
    state.sketch.plane = plane;
    SketchRectangle rect;
    rect.center = SketchPoint{0.0, 0.0};
    rect.width = w;
    rect.height = h;
    addSketchEntity(&state.sketch, rect);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = 1.0;
    return state;
}

CadBodyState corpusCircleState(double radius, Workplane plane) {
    CadBodyState state;
    state.sketch.plane = plane;
    SketchCircle circle;
    circle.center = SketchPoint{0.0, 0.0};
    circle.radius = radius;
    addSketchEntity(&state.sketch, circle);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = 1.0;
    return state;
}

CadBodyState withSymmetric(CadBodyState state, double perSide) {
    state.extrude.extent = ExtrudeExtentMode::Symmetric;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    state.extrude.depth = perSide;
    state.extrude.secondDistance = 0.0;
    return state;
}

CadBodyState withTwoSides(CadBodyState state, double a, double b) {
    state.extrude.extent = ExtrudeExtentMode::TwoSides;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    state.extrude.depth = a;
    state.extrude.secondDistance = b;
    return state;
}

ProjectCadBody corpusCadBody(ObjectId id, const CadBodyState& state) {
    ProjectCadBody body;
    body.objectId = id;
    body.state = state;
    return body;
}

// The smallest v4 file there is: one Symmetric body on a world plane.
ProjectDocument cadSymmetricDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;
    document.scene.bodies.push_back(corpusSceneBody(1, TransformValues{}));
    document.hasCad = true;
    document.cad.bodies.push_back(
        corpusCadBody(1, withSymmetric(corpusRectangleState(2.0, 1.0, Workplane::XY), 0.75)));
    return document;
}

// Two independent distances, deliberately UNEQUAL, on a circle profile so the
// fixture also pins a curved-side body carrying the new extent.
ProjectDocument cadTwoSidesDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;
    document.scene.bodies.push_back(corpusSceneBody(1, TransformValues{}));
    document.hasCad = true;
    document.cad.bodies.push_back(
        corpusCadBody(1, withTwoSides(corpusCircleState(0.5, Workplane::XZ), 1.25, 0.5)));
    return document;
}

// v4 carrying a v2 support block, which is what makes a version a SUPERSET
// rather than a variant: a One Side producer with a Symmetric dependent on its
// far cap and a Two Sides dependent on one of its sides.
ProjectDocument cadFaceExtentDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 4;
    document.scene.activeObjectId = 1;
    document.scene.bodies.push_back(corpusSceneBody(1, TransformValues{}));
    document.scene.bodies.push_back(corpusSceneBody(2, TransformValues{}));
    document.scene.bodies.push_back(corpusSceneBody(3, TransformValues{}));
    document.hasCad = true;
    const CadBodyState producer = corpusRectangleState(4.0, 4.0, Workplane::XY);
    const uint64_t lineage = cadTopologySignature(producer);

    CadBodyState onCap = withSymmetric(corpusRectangleState(1.0, 1.0, Workplane::XY), 0.25);
    onCap.sketch.hasFaceSupport = true;
    onCap.sketch.faceSupport.producerObjectId = 1;
    onCap.sketch.faceSupport.producerLocalFeatureId = kCadFeatureId;
    onCap.sketch.faceSupport.face = CadFaceToken{CadFaceKind::CapFar, 0, 0};
    onCap.sketch.faceSupport.lineageToken = lineage;

    CadBodyState onSide = withTwoSides(corpusRectangleState(0.5, 0.5, Workplane::XY), 0.375, 0.125);
    onSide.sketch.hasFaceSupport = true;
    onSide.sketch.faceSupport.producerObjectId = 1;
    onSide.sketch.faceSupport.producerLocalFeatureId = kCadFeatureId;
    onSide.sketch.faceSupport.face = CadFaceToken{CadFaceKind::Side, 1, 1};
    onSide.sketch.faceSupport.lineageToken = lineage;

    document.cad.bodies.push_back(corpusCadBody(1, producer));
    document.cad.bodies.push_back(corpusCadBody(2, onCap));
    document.cad.bodies.push_back(corpusCadBody(3, onSide));
    return document;
}

// A LEGACY One Side body beside a Symmetric and a Two Sides one, with a sparse
// `CONS` next to them: the fixture that proves a v4 section still carries the
// One Side pair unchanged, on the three world planes.
ProjectDocument cadMixedExtentDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 5;
    document.scene.activeObjectId = 2;
    document.scene.bodies.push_back(corpusSceneBody(1, TransformValues{}));
    document.scene.bodies.push_back(corpusSceneBody(2, corpusPlacementAt(2.0, 0.0, 0.0)));
    document.scene.bodies.push_back(corpusSceneBody(3, corpusPlacementAt(-2.0, 0.0, 0.0)));
    document.scene.bodies.push_back(corpusSceneBody(4, corpusPlacementAt(0.0, 3.0, 0.0)));
    document.hasConstruction = true;
    ProjectConstructionBody source;
    source.objectId = 1;
    source.shape = canonicalCorpusShape(PrimitiveKind::Box);
    source.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(source);
    document.hasCad = true;
    CadBodyState oneSide = corpusRectangleState(1.5, 1.5, Workplane::XY);
    oneSide.extrude.direction = ExtrudeDirection::AgainstNormal;
    document.cad.bodies.push_back(corpusCadBody(2, oneSide));
    document.cad.bodies.push_back(
        corpusCadBody(3, withSymmetric(corpusRectangleState(1.0, 2.0, Workplane::XZ), 0.625)));
    document.cad.bodies.push_back(
        corpusCadBody(4, withTwoSides(corpusCircleState(0.75, Workplane::YZ), 1.0, 0.25)));
    return document;
}

}  // namespace

const char* cadSymmetricFixtureSha256() { return g_symmetricDigest.c_str(); }
const char* cadTwoSidesFixtureSha256() { return g_twoSidesDigest.c_str(); }
const char* cadFaceExtentFixtureSha256() { return g_faceExtentDigest.c_str(); }
const char* cadMixedExtentFixtureSha256() { return g_mixedExtentDigest.c_str(); }
const char* cadBadExtentFixtureSha256() { return g_badExtentDigest.c_str(); }
const char* cadBadTwoSidesFixtureSha256() { return g_badTwoSidesDigest.c_str(); }

namespace {

// The two CORRUPT v4 fixtures. Every length, count and CRC in both is correct,
// so only the SEMANTIC check can refuse them -- which is exactly what makes them
// worth having. They cannot come out of `encodeProjectV1`, which validates
// first, so the valid file is encoded and the bad value is written over its own
// FIXED-WIDTH field and the payload CRC redone. The PowerShell builder reaches
// the same bytes by CONSTRUCTING them with the bad value in place: two routes,
// one file.
//
// The v4 one-body record, from the start of the payload:
//   bodyCount u32 | objectId u64 | plane u8 | supportKind u8 | nextEntityId u32
//   | profileEntityId u32 | extentCode u8 | directionCode u8 | depth f64
//   | secondDistance f64 | entityCount u32 ...
constexpr size_t kV4ExtentCodeOffset = 4 + 8 + 1 + 1 + 4 + 4;
constexpr size_t kV4DepthOffset = kV4ExtentCodeOffset + 1 + 1;
constexpr size_t kV4SecondDistanceOffset = kV4DepthOffset + 8;

std::vector<uint8_t> cadBadExtentBytes() {
    std::vector<uint8_t> bytes = encodeProjectV1(cadSymmetricDocument());
    const size_t payload = cadPayloadOffset(bytes);
    if (payload == 0) {
        return std::vector<uint8_t>();
    }
    bytes[payload + kV4ExtentCodeOffset] = 9;  // no such extent
    repairCadPayloadCrc(&bytes);
    return bytes;
}

std::vector<uint8_t> cadBadTwoSidesBytes() {
    std::vector<uint8_t> bytes = encodeProjectV1(cadTwoSidesDocument());
    const size_t payload = cadPayloadOffset(bytes);
    if (payload == 0) {
        return std::vector<uint8_t>();
    }
    const double zero = 0.0;
    std::memcpy(&bytes[payload + kV4DepthOffset], &zero, sizeof(double));
    std::memcpy(&bytes[payload + kV4SecondDistanceOffset], &zero, sizeof(double));
    repairCadPayloadCrc(&bytes);
    return bytes;
}

}  // namespace

void testExtentCorpus(Recorder& r) {
    ProjectCodecStatus why = ProjectCodecStatus::Ok;
    const std::vector<uint8_t> symmetric = encodeProjectV1(cadSymmetricDocument(), &why);
    g_symmetricDigest = projectFixtureSha256Hex(symmetric);
    r.check("CADEXT_10_c_cad_symmetric_fixture_matches_the_committed_digest",
            why == ProjectCodecStatus::Ok
                    && g_symmetricDigest
                               == "6674e7225933ee2195292b6e43a091d3e327b7189ebd35fa1519191ecf92ab5a");
    const std::vector<uint8_t> twoSides = encodeProjectV1(cadTwoSidesDocument(), &why);
    g_twoSidesDigest = projectFixtureSha256Hex(twoSides);
    r.check("CADEXT_10_d_cad_two_sides_fixture_matches_the_committed_digest",
            why == ProjectCodecStatus::Ok
                    && g_twoSidesDigest
                               == "8c49e09cca8133b81ef9bbfab75549ed111dc08b4057777ed077191cf25cab47");
    const std::vector<uint8_t> faceExtent = encodeProjectV1(cadFaceExtentDocument(), &why);
    g_faceExtentDigest = projectFixtureSha256Hex(faceExtent);
    r.check("CADEXT_10_e_cad_face_extent_fixture_matches_the_committed_digest",
            why == ProjectCodecStatus::Ok
                    && g_faceExtentDigest
                               == "73739a226a30f8d015fe19663cd06062eeab7dab541f90795282ab5660510673");
    const std::vector<uint8_t> mixed = encodeProjectV1(cadMixedExtentDocument(), &why);
    g_mixedExtentDigest = projectFixtureSha256Hex(mixed);
    r.check("CADEXT_10_f_mixed_cad_extent_fixture_matches_the_committed_digest",
            why == ProjectCodecStatus::Ok
                    && g_mixedExtentDigest
                               == "32ee99fccc5be71ed15003dc5b6856f7ed9c57b764c1032394b2c0b2f814f076");
    const std::vector<uint8_t> badExtent = cadBadExtentBytes();
    g_badExtentDigest = projectFixtureSha256Hex(badExtent);
    r.check("CADEXT_10_g_cad_bad_extent_fixture_matches_the_committed_digest",
            !badExtent.empty()
                    && g_badExtentDigest
                               == "15f1cecae31287643467d4b127e92918aca9677c5a1016b1def41a30d493b995");
    const std::vector<uint8_t> badTwoSides = cadBadTwoSidesBytes();
    g_badTwoSidesDigest = projectFixtureSha256Hex(badTwoSides);
    r.check("CADEXT_10_h_cad_bad_two_sides_fixture_matches_the_committed_digest",
            !badTwoSides.empty()
                    && g_badTwoSidesDigest
                               == "aeb1b6784c546dde05a8e3227227aa19f2b396bee5d0f40981ad1e115414ea39");

    // Every VALID one opens, at the version it declares, with the extent it was
    // written with; and both corrupt ones are REFUSED, changing nothing.
    {
        ProjectDocument decoded;
        uint16_t version = 0;
        const bool symmetricOpens =
                decodeProject(symmetric.data(), symmetric.size(), &decoded) == ProjectCodecStatus::Ok
                && decoded.cad.bodies.size() == 1
                && decoded.cad.bodies[0].state.extrude.extent == ExtrudeExtentMode::Symmetric
                && cadbSectionVersion(symmetric, &version) && version == kCadSectionVersionV4;
        ProjectDocument twoDecoded;
        const bool twoOpens =
                decodeProject(twoSides.data(), twoSides.size(), &twoDecoded)
                        == ProjectCodecStatus::Ok
                && twoDecoded.cad.bodies.size() == 1
                && near(extrudePositiveDistance(twoDecoded.cad.bodies[0].state.extrude), 1.25)
                && near(extrudeNegativeDistance(twoDecoded.cad.bodies[0].state.extrude), 0.5);
        ProjectDocument faceDecoded;
        const bool faceOpens =
                decodeProject(faceExtent.data(), faceExtent.size(), &faceDecoded)
                        == ProjectCodecStatus::Ok
                && faceDecoded.cad.bodies.size() == 3
                && faceDecoded.cad.bodies[1].state.sketch.hasFaceSupport
                && faceDecoded.cad.bodies[2].state.sketch.hasFaceSupport;
        ProjectDocument mixedDecoded;
        const bool mixedOpens =
                decodeProject(mixed.data(), mixed.size(), &mixedDecoded) == ProjectCodecStatus::Ok
                && mixedDecoded.cad.bodies.size() == 3
                && mixedDecoded.cad.bodies[0].state.extrude.extent == ExtrudeExtentMode::OneSide
                && mixedDecoded.cad.bodies[0].state.extrude.direction
                           == ExtrudeDirection::AgainstNormal;
        r.check("CADEXT_10_i_every_valid_v4_fixture_opens_with_the_extent_it_was_written_with",
                symmetricOpens && twoOpens && faceOpens && mixedOpens);

        ProjectDocument refusedA;
        ProjectDocument refusedB;
        const ProjectCodecStatus whyA =
                decodeProject(badExtent.data(), badExtent.size(), &refusedA);
        const ProjectCodecStatus whyB =
                decodeProject(badTwoSides.data(), badTwoSides.size(), &refusedB);
        r.check("CADEXT_10_j_both_corrupt_v4_fixtures_are_refused_and_change_nothing",
                whyA == ProjectCodecStatus::InvalidSemanticValue && !refusedA.hasCad
                        && refusedA.cad.bodies.empty()
                        && whyB == ProjectCodecStatus::InvalidSemanticValue && !refusedB.hasCad
                        && refusedB.cad.bodies.empty());
    }
}

const char* cadPerformanceReport() { return g_performance.c_str(); }

int runCadSelfTests(CadSelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};
    testExtent(r);
    testExtentCorpus(r);

    // -----------------------------------------------------------------------
    // CADR0-01..03: the three workplane mappings
    // -----------------------------------------------------------------------
    {
        const Workplane planes[3] = {Workplane::XY, Workplane::XZ, Workplane::YZ};
        const char* names[3] = {"CADR0_01_xy_workplane_mapping", "CADR0_02_xz_workplane_mapping",
                                "CADR0_03_yz_workplane_mapping"};
        for (int i = 0; i < 3; ++i) {
            const WorkplaneFrame frame = workplaneFrame(planes[i]);
            const Vec3 cross = vec3Cross(frame.uAxis, frame.vAxis);
            const bool rightHanded = nearf(cross.x, frame.normal.x) && nearf(cross.y, frame.normal.y)
                                     && nearf(cross.z, frame.normal.z);
            const SketchPoint p{1.25, -2.5};
            const Vec3 local = workplaneToLocal(planes[i], p);
            const SketchPoint back = localToWorkplane(planes[i], local);
            // On the plane: no component along the normal; and the inverse
            // recovers the point exactly (every coordinate is an exact
            // binary fraction).
            const bool onPlane = nearf(vec3Dot(local, frame.normal), 0.0f);
            const Vec3 offset = workplaneToLocalAtOffset(planes[i], p, 0.5);
            const bool alongNormal = nearf(vec3Dot(vec3Sub(offset, local), frame.normal), 0.5f);
            r.check(names[i], rightHanded && onPlane && alongNormal && near(back.u, p.u)
                                  && near(back.v, p.v));
        }
        // The exact axes, so nothing about the mapping is a matter of taste.
        const Vec3 xy = workplaneToLocal(Workplane::XY, SketchPoint{1.0, 2.0});
        const Vec3 xz = workplaneToLocal(Workplane::XZ, SketchPoint{1.0, 2.0});
        const Vec3 yz = workplaneToLocal(Workplane::YZ, SketchPoint{1.0, 2.0});
        r.check("CADR0_01_xy_is_x_y_plus_z",
                xy.x == 1.0f && xy.y == 2.0f && xy.z == 0.0f);
        r.check("CADR0_02_xz_is_x_minus_z_plus_y",
                xz.x == 1.0f && xz.y == 0.0f && xz.z == -2.0f);
        r.check("CADR0_03_yz_is_minus_z_y_plus_x",
                yz.x == 0.0f && yz.y == 2.0f && yz.z == -1.0f);
    }

    // -----------------------------------------------------------------------
    // CADR0-04..07: entity creation and validation
    // -----------------------------------------------------------------------
    {
        CadSketch sketch;
        SketchEntityId id = kNoSketchEntity;
        r.check("CADR0_04_line_creates_with_a_minted_id",
                addSketchEntity(&sketch, SketchLine{{0.0, 0.0}, {1.0, 0.0}}, &id) == CadStatus::Ok
                        && id == 1 && sketch.nextEntityId == 2);
        r.check("CADR0_04_zero_length_line_refused",
                addSketchEntity(&sketch, SketchLine{{1.0, 1.0}, {1.0, 1.0}}) == CadStatus::ZeroLengthLine
                        && sketch.entities.size() == 1 && sketch.nextEntityId == 2);
        r.check("CADR0_04_non_finite_line_refused",
                addSketchEntity(&sketch, SketchLine{{0.0, 0.0}, {NAN, 1.0}}) == CadStatus::NonFinite);
        r.check("CADR0_04_out_of_range_line_refused",
                addSketchEntity(&sketch, SketchLine{{0.0, 0.0}, {2.0e5, 1.0}})
                        == CadStatus::OutOfRange);

        SketchPolyline open;
        open.vertices = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}};
        r.check("CADR0_05_open_polyline_creates",
                addSketchEntity(&sketch, open, &id) == CadStatus::Ok && id == 2);
        SketchPolyline dup;
        dup.vertices = {{0.0, 0.0}, {0.0, 0.0}, {1.0, 1.0}};
        r.check("CADR0_05_duplicate_consecutive_vertex_refused",
                addSketchEntity(&sketch, dup) == CadStatus::DuplicateEdge);
        SketchPolyline one;
        one.vertices = {{0.0, 0.0}};
        r.check("CADR0_05_single_vertex_polyline_refused",
                addSketchEntity(&sketch, one) == CadStatus::TooFewVertices);
        SketchPolyline closedTwo;
        closedTwo.vertices = {{0.0, 0.0}, {1.0, 0.0}};
        closedTwo.closed = true;
        r.check("CADR0_05_closed_two_vertex_polyline_refused",
                addSketchEntity(&sketch, closedTwo) == CadStatus::TooFewVertices);

        SketchRectangle rectangle;
        rectangle.center = {0.0, 0.0};
        rectangle.width = 2.0;
        rectangle.height = 0.0;
        r.check("CADR0_06_zero_height_rectangle_refused",
                addSketchEntity(&sketch, rectangle) == CadStatus::ZeroSizeRectangle);
        rectangle.height = -1.0;
        r.check("CADR0_06_negative_rectangle_refused",
                addSketchEntity(&sketch, rectangle) == CadStatus::ZeroSizeRectangle);
        rectangle.height = 1.0;
        r.check("CADR0_06_rectangle_creates",
                addSketchEntity(&sketch, rectangle, &id) == CadStatus::Ok && id == 3);

        SketchCircle circle;
        circle.center = {0.0, 0.0};
        circle.radius = 0.0;
        r.check("CADR0_07_zero_radius_circle_refused",
                addSketchEntity(&sketch, circle) == CadStatus::InvalidCircleRadius);
        circle.radius = INFINITY;
        r.check("CADR0_07_infinite_radius_circle_refused",
                addSketchEntity(&sketch, circle) == CadStatus::NonFinite);
        circle.radius = 0.5;
        r.check("CADR0_07_circle_creates",
                addSketchEntity(&sketch, circle, &id) == CadStatus::Ok && id == 4);

        r.check("CADR0_04_replace_keeps_id_and_refuses_bad_geometry",
                replaceSketchEntity(&sketch, 1, SketchLine{{0.0, 0.0}, {0.0, 3.0}}) == CadStatus::Ok
                        && replaceSketchEntity(&sketch, 1, SketchLine{{0.0, 0.0}, {0.0, 0.0}})
                                   == CadStatus::ZeroLengthLine
                        && findSketchEntity(sketch, 1)->line()->end.v == 3.0
                        && replaceSketchEntity(&sketch, 99, SketchLine{{0.0, 0.0}, {1.0, 0.0}})
                                   == CadStatus::UnknownEntity);
        r.check("CADR0_11_remove_never_reuses_an_id",
                removeSketchEntity(&sketch, 2) == CadStatus::Ok && sketch.entities.size() == 3
                        && sketch.nextEntityId == 5
                        && removeSketchEntity(&sketch, 2) == CadStatus::UnknownEntity
                        && validateCadSketch(sketch) == CadStatus::Ok);
        CadSketch bad = sketch;
        bad.nextEntityId = 2;  // an entity wears id 4: the allocator would collide
        r.check("CADR0_04_sketch_with_low_allocator_refused",
                validateCadSketch(bad) == CadStatus::UnknownEntity);
    }

    // -----------------------------------------------------------------------
    // CADR0-15..21: profiles
    // -----------------------------------------------------------------------
    {
        const ProfileExtraction rect = extractClosedProfiles(rectangleSketch());
        r.check("CADR0_16_rectangle_closes_one_ccw_profile",
                rect.profiles.size() == 1 && rect.profiles[0].polygon.size() == 4
                        && rect.profiles[0].anchorEntityId == 1 && near(rect.profiles[0].area, 2.0)
                        && polygonSignedAreaTwice(rect.profiles[0].polygon) > 0.0
                        && rect.rejections.empty());

        const ProfileExtraction circle = extractClosedProfiles(circleSketch());
        const double expectedArea = 0.5 * kSketchCircleSegments * 0.75 * 0.75
                                    * std::sin(2.0 * 3.14159265358979323846 / kSketchCircleSegments);
        r.check("CADR0_17_circle_closes_one_tessellated_profile",
                circle.profiles.size() == 1 && circle.profiles[0].fromCircle
                        && circle.profiles[0].polygon.size() == kSketchCircleSegments
                        && near(circle.profiles[0].area, expectedArea, 1e-9));

        const ProfileExtraction poly = extractClosedProfiles(polygonSketch(6, true));
        r.check("CADR0_18_closed_polyline_closes_one_profile",
                poly.profiles.size() == 1 && poly.profiles[0].polygon.size() == 6
                        && poly.rejections.empty());

        const ProfileExtraction openPoly = extractClosedProfiles(polygonSketch(6, false));
        r.check("CADR0_15_open_polyline_is_refused_by_name",
                openPoly.profiles.empty() && openPoly.rejections.size() == 1
                        && openPoly.rejections[0].why == CadStatus::OpenProfile);

        // A polyline closed by snapping its last vertex onto its first.
        {
            CadSketch sketch;
            SketchPolyline snapped;
            snapped.vertices = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}};
            addSketchEntity(&sketch, snapped);
            const ProfileExtraction e = extractClosedProfiles(sketch);
            r.check("CADR0_18_polyline_closed_by_endpoint_snap_is_a_profile",
                    e.profiles.size() == 1 && e.profiles[0].polygon.size() == 4
                            && near(e.profiles[0].area, 1.0));
        }

        // A chain of three lines closes a triangle; the anchor is the lowest id.
        {
            CadSketch sketch;
            addSketchEntity(&sketch, SketchLine{{0.0, 0.0}, {2.0, 0.0}});
            addSketchEntity(&sketch, SketchLine{{2.0, 0.0}, {0.0, 2.0}});
            addSketchEntity(&sketch, SketchLine{{0.0, 2.0}, {0.0, 0.0}});
            const ProfileExtraction e = extractClosedProfiles(sketch);
            r.check("CADR0_18_line_chain_closes_a_profile_anchored_by_lowest_id",
                    e.profiles.size() == 1 && e.profiles[0].anchorEntityId == 1
                            && e.profiles[0].polygon.size() == 3 && near(e.profiles[0].area, 2.0)
                            && e.profiles[0].memberEntityIds.size() == 3);
            // Lines given in a scrambled order and with reversed directions
            // still close the same loop.
            CadSketch scrambled;
            addSketchEntity(&scrambled, SketchLine{{0.0, 2.0}, {2.0, 0.0}});
            addSketchEntity(&scrambled, SketchLine{{0.0, 0.0}, {0.0, 2.0}});
            addSketchEntity(&scrambled, SketchLine{{2.0, 0.0}, {0.0, 0.0}});
            const ProfileExtraction s = extractClosedProfiles(scrambled);
            r.check("CADR0_18_line_chain_order_and_direction_do_not_matter",
                    s.profiles.size() == 1 && near(s.profiles[0].area, 2.0)
                            && polygonSignedAreaTwice(s.profiles[0].polygon) > 0.0);
        }
        // An open chain, and a forked one.
        {
            CadSketch sketch;
            addSketchEntity(&sketch, SketchLine{{0.0, 0.0}, {2.0, 0.0}});
            addSketchEntity(&sketch, SketchLine{{2.0, 0.0}, {0.0, 2.0}});
            const ProfileExtraction e = extractClosedProfiles(sketch);
            r.check("CADR0_15_open_line_chain_refused_by_name",
                    e.profiles.empty() && e.rejections.size() == 1
                            && e.rejections[0].why == CadStatus::OpenProfile
                            && e.rejections[0].anchorEntityId == 1);
            addSketchEntity(&sketch, SketchLine{{0.0, 2.0}, {0.0, 0.0}});
            addSketchEntity(&sketch, SketchLine{{0.0, 0.0}, {-1.0, -1.0}});
            const ProfileExtraction f = extractClosedProfiles(sketch);
            r.check("CADR0_15_forked_line_chain_refused_by_name",
                    f.profiles.empty() && f.rejections.size() == 1
                            && f.rejections[0].why == CadStatus::BranchingChain);
        }
        // Self-intersection: a bow tie.
        {
            CadSketch sketch;
            SketchPolyline bowtie;
            bowtie.vertices = {{0.0, 0.0}, {2.0, 2.0}, {2.0, 0.0}, {0.0, 2.0}};
            bowtie.closed = true;
            addSketchEntity(&sketch, bowtie);
            const ProfileExtraction e = extractClosedProfiles(sketch);
            r.check("CADR0_19_self_intersecting_loop_refused_by_name",
                    e.profiles.empty() && e.rejections.size() == 1
                            && e.rejections[0].why == CadStatus::SelfIntersectingProfile);
        }
        // Zero area: three collinear vertices.
        {
            CadSketch sketch;
            SketchPolyline flat;
            flat.vertices = {{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}};
            flat.closed = true;
            addSketchEntity(&sketch, flat);
            const ProfileExtraction e = extractClosedProfiles(sketch);
            r.check("CADR0_20_zero_area_loop_refused_by_name",
                    e.profiles.empty() && e.rejections.size() == 1
                            && e.rejections[0].why == CadStatus::ZeroAreaProfile);
        }
        // Several profiles: deterministic order by anchor id, independent of
        // insertion; and a nested one is refused as a hole.
        {
            CadSketch sketch;
            SketchCircle c;
            c.center = {5.0, 5.0};
            c.radius = 0.5;
            addSketchEntity(&sketch, c);  // id 1
            SketchRectangle rc;
            rc.center = {0.0, 0.0};
            rc.width = 1.0;
            rc.height = 1.0;
            addSketchEntity(&sketch, rc);  // id 2
            const ProfileExtraction e = extractClosedProfiles(sketch);
            r.check("CADR0_21_multiple_profiles_ordered_by_anchor_id",
                    e.profiles.size() == 2 && e.profiles[0].anchorEntityId == 1
                            && e.profiles[1].anchorEntityId == 2);
            CadBodyState none = bodyState(sketch, kNoSketchEntity);
            r.check("CADR0_21_no_chosen_profile_is_refused_not_guessed",
                    validateCadBodyState(none) == CadStatus::ProfileNotFound);
            SketchRectangle outer;
            outer.center = {5.0, 5.0};
            outer.width = 4.0;
            outer.height = 4.0;
            addSketchEntity(&sketch, outer);  // id 3, contains the circle
            // `CAD-VERTICAL-SLICE-R1` changed this contract ON PURPOSE: a loop
            // that contains another is no longer refused as a hole R0 would not
            // fill. It is a loop like any other, and what the two enclose is
            // the region extraction's answer -- the outer rectangle's region
            // carries the circle as its hole, and the circle is its own disk.
            const ProfileExtraction n = extractClosedProfiles(sketch);
            bool anyNestedRejection = false;
            for (const ProfileRejection& rej : n.rejections) {
                anyNestedRejection |= rej.why == CadStatus::NestedProfileUnsupported;
            }
            const SketchRegionExtraction regions = extractSketchRegions(sketch);
            const SketchRegion* outerRegion = findSketchRegion(regions, 3);
            r.check("CADR0_21_nested_outer_profile_is_a_region_with_the_inner_as_its_hole",
                    !anyNestedRejection && n.profiles.size() == 3 && n.profiles[0].anchorEntityId == 1
                            && n.profiles[1].anchorEntityId == 2 && n.profiles[2].anchorEntityId == 3
                            && outerRegion != nullptr && outerRegion->holeAnchorIds.size() == 1u
                            && outerRegion->holeAnchorIds[0] == 1u
                            && validateCadBodyState(bodyState(sketch, 3)) == CadStatus::ProfileRegionMismatch);
            // Two profiles that merely overlap are both kept: each is a solid.
            CadSketch overlap;
            SketchRectangle a;
            a.center = {0.0, 0.0};
            a.width = 2.0;
            a.height = 2.0;
            SketchRectangle b;
            b.center = {1.0, 1.0};
            b.width = 2.0;
            b.height = 2.0;
            addSketchEntity(&overlap, a);
            addSketchEntity(&overlap, b);
            r.check("CADR0_21_overlapping_profiles_are_both_kept",
                    extractClosedProfiles(overlap).profiles.size() == 2);
        }
        r.check("CADR0_15_no_closed_profile_refuses_regeneration",
                validateCadBodyState(bodyState(polygonSketch(5, false), 1))
                        == CadStatus::NoClosedProfile);
    }

    // -----------------------------------------------------------------------
    // CADR0-22..23: triangulation and tessellation are deterministic
    // -----------------------------------------------------------------------
    {
        // A concave L: seven ear clips of a non-convex polygon.
        std::vector<SketchPoint> ell = {{0, 0}, {3, 0}, {3, 1}, {1, 1}, {1, 3}, {0, 3}};
        std::vector<uint32_t> first;
        std::vector<uint32_t> second;
        const CadStatus a = triangulateSimplePolygon(ell, &first);
        const CadStatus b = triangulateSimplePolygon(ell, &second);
        double area = 0.0;
        bool allPositive = true;
        for (size_t t = 0; t + 2 < first.size(); t += 3) {
            const double twice = (ell[first[t + 1]].u - ell[first[t]].u)
                                         * (ell[first[t + 2]].v - ell[first[t]].v)
                                 - (ell[first[t + 1]].v - ell[first[t]].v)
                                           * (ell[first[t + 2]].u - ell[first[t]].u);
            allPositive &= twice > 0.0;
            area += twice * 0.5;
        }
        r.check("CADR0_22_concave_polygon_triangulates_deterministically",
                a == CadStatus::Ok && b == CadStatus::Ok && first == second
                        && first.size() == (ell.size() - 2) * 3 && allPositive && near(area, 5.0));
        std::vector<uint32_t> ignored;
        std::vector<SketchPoint> clockwise(ell.rbegin(), ell.rend());
        r.check("CADR0_22_clockwise_input_is_refused",
                triangulateSimplePolygon(clockwise, &ignored) == CadStatus::TriangulationFailed);
        std::vector<SketchPoint> two = {{0, 0}, {1, 0}};
        r.check("CADR0_22_too_few_vertices_refused",
                triangulateSimplePolygon(two, &ignored) == CadStatus::TriangulationFailed);

        SketchCircle circle;
        circle.center = {0.25, -0.5};
        circle.radius = 2.0;
        const std::vector<SketchPoint> ring1 = circleProfilePolygon(circle);
        const std::vector<SketchPoint> ring2 = circleProfilePolygon(circle);
        bool same = ring1.size() == ring2.size();
        for (size_t i = 0; same && i < ring1.size(); ++i) {
            same = ring1[i].u == ring2[i].u && ring1[i].v == ring2[i].v;
        }
        const uint32_t q = kSketchCircleSegments / 4;
        r.check("CADR0_23_circle_tessellation_deterministic_with_exact_cardinals",
                same && ring1.size() == kSketchCircleSegments && ring1[0].u == 2.25
                        && ring1[0].v == -0.5 && ring1[q].u == 0.25 && ring1[q].v == 1.5
                        && ring1[2 * q].u == -1.75 && ring1[3 * q].v == -2.5);
    }

    // -----------------------------------------------------------------------
    // CADR0-24..27: the extrusion
    // -----------------------------------------------------------------------
    {
        ConstructionMesh mesh;
        const CadBodyState rect = bodyState(rectangleSketch(Workplane::XY, 2.0, 1.0), 1, 1.5);
        const CadStatus why = generateCadMesh(rect, &mesh);
        float lo[3], hi[3];
        extents(mesh, lo, hi);
        r.check("CADR0_24_rectangle_extrude_is_watertight",
                why == CadStatus::Ok && mesh.vertices.size() == cadExtrusionVertexCount(4)
                        && mesh.indices.size() == cadExtrusionIndexCount(4) && watertight(mesh)
                        && trianglesNonDegenerate(mesh));
        r.check("CADR0_27_rectangle_extrude_has_canonical_winding_and_extents",
                why == CadStatus::Ok && canonicalWinding(mesh) && near(signedVolume(mesh), 3.0, 1e-5)
                        && nearf(lo[0], -0.5f)
                        && nearf(hi[0], 1.5f) && nearf(lo[1], -0.25f) && nearf(hi[1], 0.75f)
                        && nearf(lo[2], 0.0f) && nearf(hi[2], 1.5f));

        ConstructionMesh against;
        r.check("CADR0_27_against_normal_extrudes_the_other_way",
                generateCadMesh(bodyState(rectangleSketch(), 1, 1.5,
                                          ExtrudeDirection::AgainstNormal),
                                &against) == CadStatus::Ok
                        && watertight(against) && canonicalWinding(against)
                        && (extents(against, lo, hi), nearf(lo[2], -1.5f) && nearf(hi[2], 0.0f)));

        ConstructionMesh circle;
        r.check("CADR0_25_circle_extrude_is_watertight",
                generateCadMesh(bodyState(circleSketch(), 1, 0.5), &circle) == CadStatus::Ok
                        && circle.vertices.size() == cadExtrusionVertexCount(kSketchCircleSegments)
                        && circle.indices.size() == cadExtrusionIndexCount(kSketchCircleSegments)
                        && watertight(circle) && trianglesNonDegenerate(circle)
                        && canonicalWinding(circle));

        ConstructionMesh poly;
        r.check("CADR0_26_closed_polyline_extrude_is_watertight",
                generateCadMesh(bodyState(polygonSketch(7, true), 1, 2.0), &poly) == CadStatus::Ok
                        && poly.vertices.size() == 14 && watertight(poly)
                        && trianglesNonDegenerate(poly) && canonicalWinding(poly));

        // A concave profile extruded: the L again, as a closed polyline.
        {
            CadSketch sketch;
            SketchPolyline ell;
            ell.vertices = {{0, 0}, {3, 0}, {3, 1}, {1, 1}, {1, 3}, {0, 3}};
            ell.closed = true;
            addSketchEntity(&sketch, ell);
            ConstructionMesh concave;
            r.check("CADR0_26_concave_profile_extrude_is_watertight",
                    generateCadMesh(bodyState(sketch, 1, 1.0), &concave) == CadStatus::Ok
                            && watertight(concave) && trianglesNonDegenerate(concave)
                            && near(signedVolume(concave), 5.0, 1e-4));
        }

        // The other two planes: the depth runs along each plane's normal.
        ConstructionMesh xz;
        ConstructionMesh yz;
        r.check("CADR0_27_xz_and_yz_extrude_along_their_normals",
                generateCadMesh(bodyState(rectangleSketch(Workplane::XZ), 1, 1.5), &xz)
                                == CadStatus::Ok
                        && canonicalWinding(xz)
                        && (extents(xz, lo, hi), nearf(lo[1], 0.0f) && nearf(hi[1], 1.5f))
                        && generateCadMesh(bodyState(rectangleSketch(Workplane::YZ), 1, 1.5), &yz)
                                   == CadStatus::Ok
                        && canonicalWinding(yz)
                        && (extents(yz, lo, hi), nearf(lo[0], 0.0f) && nearf(hi[0], 1.5f)));

        r.check("CADR0_27_invalid_depth_refused_by_name",
                generateCadMesh(bodyState(rectangleSketch(), 1, 0.0), &mesh)
                                == CadStatus::InvalidExtrudeDepth
                        && generateCadMesh(bodyState(rectangleSketch(), 1, -1.0), &mesh)
                                   == CadStatus::InvalidExtrudeDepth
                        && generateCadMesh(bodyState(rectangleSketch(), 1, NAN), &mesh)
                                   == CadStatus::NonFinite);
        r.check("CADR0_15_open_profile_refuses_the_extrusion",
                generateCadMesh(bodyState(polygonSketch(4, false), 1, 1.0), &mesh)
                        == CadStatus::NoClosedProfile);
        r.check("CADR0_19_self_intersecting_profile_refuses_the_extrusion",
                [&] {
                    CadSketch sketch;
                    SketchPolyline bowtie;
                    bowtie.vertices = {{0.0, 0.0}, {2.0, 2.0}, {2.0, 0.0}, {0.0, 2.0}};
                    bowtie.closed = true;
                    addSketchEntity(&sketch, bowtie);
                    return generateCadMesh(bodyState(sketch, 1, 1.0), &mesh)
                           == CadStatus::NoClosedProfile;
                }());

        // Regeneration is a pure function of the state.
        ConstructionMesh once;
        ConstructionMesh again;
        r.check("CADR0_14_regeneration_is_deterministic",
                generateCadMesh(rect, &once) == CadStatus::Ok
                        && generateCadMesh(rect, &again) == CadStatus::Ok && sameMesh(once, again));
    }

    // -----------------------------------------------------------------------
    // CADR0-12..13: numeric edits on a CAD body are atomic
    // -----------------------------------------------------------------------
    {
        CadBody body(42, bodyState(rectangleSketch(Workplane::XY, 2.0, 1.0), 1, 1.5));
        bool changed = false;
        r.check("CADR0_12_rectangle_edit_changes_the_profile",
                applyCadRectangle(body, 4.0, 3.0, &changed) == CadStatus::Ok && changed
                        && body.sketch().entities[0].rectangle()->width == 4.0
                        && body.sketch().entities[0].rectangle()->height == 3.0
                        && body.sketch().entities[0].rectangle()->center.u == 0.5
                        && body.updateCount() == 1);
        r.check("CADR0_12_invalid_rectangle_edit_refused_atomically",
                applyCadRectangle(body, 0.0, 3.0, &changed) == CadStatus::ZeroSizeRectangle
                        && !changed && body.sketch().entities[0].rectangle()->width == 4.0
                        && body.updateCount() == 1);
        r.check("CADR0_12_identical_edit_changes_nothing",
                applyCadRectangle(body, 4.0, 3.0, &changed) == CadStatus::Ok && !changed
                        && body.updateCount() == 1);
        r.check("CADR0_30_depth_edit_applies_and_refuses_by_name",
                applyCadExtrude(body, 0.25, ExtrudeDirection::AgainstNormal, &changed)
                                == CadStatus::Ok
                        && changed && body.extrude().depth == 0.25
                        && applyCadExtrude(body, -1.0, ExtrudeDirection::AlongNormal, &changed)
                                   == CadStatus::InvalidExtrudeDepth
                        && body.extrude().depth == 0.25
                        && body.extrude().direction == ExtrudeDirection::AgainstNormal);
        r.check("CADR0_12_circle_edit_on_a_rectangle_body_refused",
                applyCadCircle(body, 1.0, &changed) == CadStatus::ProfileNotFound && !changed);

        CadBody round(43, bodyState(circleSketch(), 1, 1.0));
        r.check("CADR0_13_circle_edit_changes_the_radius",
                applyCadCircle(round, 2.0, &changed) == CadStatus::Ok && changed
                        && round.sketch().entities[0].circle()->radius == 2.0
                        && applyCadCircle(round, 0.0, &changed) == CadStatus::InvalidCircleRadius
                        && round.sketch().entities[0].circle()->radius == 2.0
                        && cadProfileKind(round.state()) == CadProfileKind::Circle
                        && cadProfileKind(body.state()) == CadProfileKind::Rectangle);
        // An edit that would leave the body with no closed profile is refused
        // whole: a state is applied only when it regenerates.
        CadBodyState broken = body.state();
        broken.extrude.profileEntityId = 77;
        r.check("CADR0_14_state_that_does_not_regenerate_is_refused_whole",
                body.applyState(broken, &changed) == CadStatus::ProfileNotFound && !changed
                        && body.extrude().profileEntityId == 1);
    }

    // -----------------------------------------------------------------------
    // CADR0-28..32: the project history
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        const ObjectId first = scene.activeBodyId();
        ObjectId created = kNoObject;
        {
            ScopedConstructionEdit edit(history);
            CadStatus why = CadStatus::Ok;
            SceneObject* body = scene.addCadBody(bodyState(rectangleSketch(), 1, 1.5), &why);
            created = body != nullptr ? body->objectId() : kNoObject;
            if (body != nullptr) publishSceneObject(*body);
        }
        r.check("CADR0_28_creating_a_cad_body_is_one_history_step",
                created != kNoObject && scene.bodyCount() == 2 && history.undoDepth() == 1
                        && scene.activeBodyId() == created
                        && scene.findBody(created)->isCad()
                        && scene.findBody(created)->meshStore().currentRevision()
                                   != kNoMeshRevision);
        {
            CadStatus why = CadStatus::Ok;
            const ObjectId before = scene.nextObjectId();
            ScopedConstructionEdit edit(history);
            r.check("CADR0_28_a_refused_creation_mints_nothing",
                    scene.addCadBody(bodyState(polygonSketch(4, false), 1, 1.0), &why) == nullptr
                            && why == CadStatus::NoClosedProfile && scene.nextObjectId() == before
                            && scene.bodyCount() == 2);
        }
        r.check("CADR0_28_and_records_nothing", history.undoDepth() == 1 && history.redoDepth() == 0);

        const CadBodyState authored = scene.findBody(created)->cadOrNull()->state();
        ConstructionRestoreReport report;
        r.check("CADR0_29_undo_removes_the_whole_body",
                history.undo(&report) && scene.bodyCount() == 1 && scene.findBody(created) == nullptr
                        && scene.activeBodyId() == first && report.removedBodies == 1);
        r.check("CADR0_29_redo_restores_the_same_id_state_and_mesh",
                history.redo(&report) && scene.bodyCount() == 2 && scene.findBody(created) != nullptr
                        && scene.findBody(created)->isCad()
                        && sameCadBodyState(scene.findBody(created)->cadOrNull()->state(),
                                            authored)
                        && scene.findBody(created)->meshStore().currentRevision()
                                   != kNoMeshRevision
                        && scene.activeBodyId() == created && report.restoredBodies == 1);

        // Edit the depth as one step; undo and redo it.
        SceneObject* body = scene.findBody(created);
        const MeshRevision beforeEdit = body->meshStore().currentRevision();
        {
            ScopedConstructionEdit edit(history);
            bool changed = false;
            applyCadExtrude(*body->cadOrNull(), 3.0, ExtrudeDirection::AlongNormal, &changed);
            if (changed) publishSceneObject(*body);
        }
        float lo[3], hi[3];
        ConstructionMesh regenerated;
        body->cadOrNull()->generateMesh(&regenerated);
        extents(regenerated, lo, hi);
        r.check("CADR0_30_depth_edit_is_one_step_and_regenerates",
                history.undoDepth() == 2 && body->cadOrNull()->extrude().depth == 3.0
                        && body->meshStore().currentRevision() > beforeEdit && nearf(hi[2], 3.0f));
        r.check("CADR0_30_undo_restores_the_previous_depth_and_republishes",
                history.undo(&report) && body->cadOrNull()->extrude().depth == 1.5
                        && report.republishedBodies == 1
                        && (body->cadOrNull()->generateMesh(&regenerated), extents(regenerated, lo, hi),
                            nearf(hi[2], 1.5f)));
        r.check("CADR0_30_redo_reapplies_it",
                history.redo(&report) && body->cadOrNull()->extrude().depth == 3.0);

        // Edit the rectangle as one step, with a placement set beforehand.
        TransformValues placed;
        placed.positionX = 1.0;
        placed.rotationY = 30.0;
        placed.scaleZ = 2.0;
        body->transform().setValues(placed);
        {
            ScopedConstructionEdit edit(history);
            bool changed = false;
            applyCadRectangle(*body->cadOrNull(), 5.0, 0.5, &changed);
            if (changed) publishSceneObject(*body);
        }
        r.check("CADR0_31_rectangle_edit_is_one_step",
                history.undoDepth() == 3
                        && body->cadOrNull()->sketch().entities[0].rectangle()->width == 5.0);
        r.check("CADR0_32_the_placement_survives_a_cad_edit",
                sameConstructionPlacement(body->transform().values(), placed));
        r.check("CADR0_31_undo_and_redo_restore_the_rectangle",
                history.undo() && body->cadOrNull()->sketch().entities[0].rectangle()->width == 2.0
                        && history.redo()
                        && body->cadOrNull()->sketch().entities[0].rectangle()->width == 5.0
                        && sameConstructionPlacement(body->transform().values(), placed));
        // A refused edit inside an edit scope records nothing.
        {
            ScopedConstructionEdit edit(history);
            bool changed = false;
            applyCadRectangle(*body->cadOrNull(), -1.0, 0.5, &changed);
        }
        r.check("CADR0_31_a_refused_edit_records_no_step", history.undoDepth() == 3);
    }

    // -----------------------------------------------------------------------
    // CADR0-33..36: persistence
    // -----------------------------------------------------------------------
    {
        const CadBodyState rectangleState = bodyState(rectangleSketch(Workplane::XZ, 2.0, 1.0), 1, 1.5);
        const CadBodyState circleState =
                bodyState(circleSketch(Workplane::YZ, 0.75), 1, 0.5, ExtrudeDirection::AgainstNormal);

        ConstructionScene scene;
        ConstructionHistory history(scene);
        SculptSession session;
        scene.addCadBody(rectangleState);
        scene.addCadBody(circleState);
        // A chain-of-lines body and a Construction body beside them.
        {
            CadSketch chain;
            addSketchEntity(&chain, SketchLine{{0.0, 0.0}, {2.0, 0.0}});
            addSketchEntity(&chain, SketchLine{{2.0, 0.0}, {0.0, 2.0}});
            addSketchEntity(&chain, SketchLine{{0.0, 2.0}, {0.0, 0.0}});
            scene.addCadBody(bodyState(chain, 1, 1.0));
        }
        scene.addBody();
        for (size_t i = 0; i < scene.bodyCount(); ++i) {
            publishSceneObject(scene.bodyAt(i));
        }
        TransformValues placed;
        placed.positionY = 2.5;
        placed.rotationZ = 370.0;
        scene.bodyAt(1).transform().setValues(placed);

        const ProjectDocument document = captureProjectDocument(scene, ProjectKind::Construction);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(document, &why);
        r.check("CADR0_33_a_cad_project_captures_and_encodes",
                why == ProjectCodecStatus::Ok && !bytes.empty() && document.hasCad
                        && document.cad.bodies.size() == 3 && document.hasConstruction
                        // The scene's own first Box, and the one added beside
                        // the CAD bodies: a SPARSE CONS next to a CADB.
                        && document.construction.bodies.size() == 2);

        ProjectDocument back;
        const ProjectCodecStatus decoded = decodeProject(bytes.data(), bytes.size(), &back);
        r.check("CADR0_33_the_bytes_decode_to_the_same_document",
                decoded == ProjectCodecStatus::Ok && sameProjectDocument(document, back)
                        && encodeProjectV1(back) == bytes);

        ConstructionScene loaded;
        ConstructionHistory loadedHistory(loaded);
        SculptSession loadedSession;
        ProjectLoadReport report;
        const ProjectCodecStatus loadWhy =
                loadProjectDocument(back, loaded, loadedSession, loadedHistory, &report);
        ConstructionMesh original;
        ConstructionMesh restored;
        r.check("CADR0_33_loading_rebuilds_the_rectangle_body_and_its_mesh",
                loadWhy == ProjectCodecStatus::Ok && loaded.bodyCount() == 5
                        && loaded.bodyAt(1).isCad()
                        && sameCadBodyState(loaded.bodyAt(1).cadOrNull()->state(), rectangleState)
                        && loaded.bodyAt(1).meshStore().currentRevision() != kNoMeshRevision
                        && scene.bodyAt(1).cadOrNull()->generateMesh(&original) == CadStatus::Ok
                        && loaded.bodyAt(1).cadOrNull()->generateMesh(&restored) == CadStatus::Ok
                        && sameMesh(original, restored)
                        && sameConstructionPlacement(loaded.bodyAt(1).transform().values(), placed));
        r.check("CADR0_34_loading_rebuilds_the_circle_body_and_the_chain_body",
                loadWhy == ProjectCodecStatus::Ok && loaded.bodyAt(2).isCad()
                        && sameCadBodyState(loaded.bodyAt(2).cadOrNull()->state(), circleState)
                        && loaded.bodyAt(3).isCad()
                        && loaded.bodyAt(3).cadOrNull()->sketch().entities.size() == 3
                        && loaded.bodyAt(4).hasConstructionSource()
                        && loadedHistory.undoDepth() == 0);
        r.check("CADR0_33_capturing_the_loaded_project_reproduces_the_bytes",
                loadWhy == ProjectCodecStatus::Ok
                        && encodeProjectV1(captureProjectDocument(loaded, ProjectKind::Construction))
                                   == bytes);

        // The fingerprint notices a CAD creation and a CAD edit, and returns
        // when the edit is undone -- it hashes values, never counters.
        {
            ConstructionScene fp;
            ConstructionHistory fpHistory(fp);
            const uint64_t empty = projectSemanticFingerprint(fp, ProjectKind::Construction);
            SceneObject* body = nullptr;
            {
                ScopedConstructionEdit edit(fpHistory);
                body = fp.addCadBody(rectangleState);
                if (body != nullptr) publishSceneObject(*body);
            }
            const uint64_t created = projectSemanticFingerprint(fp, ProjectKind::Construction);
            {
                ScopedConstructionEdit edit(fpHistory);
                applyCadExtrude(*body->cadOrNull(), 2.0, ExtrudeDirection::AlongNormal);
            }
            const uint64_t edited = projectSemanticFingerprint(fp, ProjectKind::Construction);
            fpHistory.undo();
            const uint64_t undone = projectSemanticFingerprint(fp, ProjectKind::Construction);
            r.check("CADR0_35_the_autosave_fingerprint_follows_cad_truth",
                    body != nullptr && empty != created && created != edited && undone == created);
        }

        // Fail closed: a document with an open profile encodes to nothing.
        {
            ProjectDocument broken = document;
            broken.cad.bodies[0].state.sketch.entities.clear();
            SketchPolyline open;
            open.vertices = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}};
            broken.cad.bodies[0].state.sketch.entities.emplace_back(1, open);
            ProjectCodecStatus encodeWhy = ProjectCodecStatus::Ok;
            r.check("CADR0_36_a_document_with_an_open_profile_is_refused",
                    encodeProjectV1(broken, &encodeWhy).empty()
                            && encodeWhy == ProjectCodecStatus::InvalidSemanticValue);
            ProjectDocument zeroDepth = document;
            zeroDepth.cad.bodies[1].state.extrude.depth = 0.0;
            r.check("CADR0_36_a_document_with_a_zero_depth_is_refused",
                    validateProjectDocument(zeroDepth) == ProjectCodecStatus::InvalidSemanticValue);
            ProjectDocument claimedTwice = document;
            ProjectConstructionBody twin;
            twin.objectId = document.cad.bodies[0].objectId;
            twin.features.push_back(ProjectFeatureRecord{});
            claimedTwice.construction.bodies.insert(claimedTwice.construction.bodies.begin(), twin);
            r.check("CADR0_36_a_body_claimed_by_cons_and_cadb_is_refused",
                    validateProjectDocument(claimedTwice) == ProjectCodecStatus::UnresolvedReference);
            ProjectDocument sculpted = document;
            sculpted.hasSculpt = true;
            ProjectSculptBody sculpt;
            sculpt.objectId = document.cad.bodies[0].objectId;
            sculpt.positions = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
            sculpt.indices = {0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3};
            sculpted.sculpt.bodies.push_back(sculpt);
            r.check("CADR0_36_a_sculpt_entry_over_a_cad_body_is_refused",
                    validateProjectDocument(sculpted) == ProjectCodecStatus::UnresolvedReference);
        }
        // Fail closed at the byte level: a plane code no plane has, with the
        // CRC recomputed so only the semantic check can catch it; and a CADB
        // payload cut short.
        {
            std::vector<uint8_t> corrupt = bytes;
            // Walk the section headers to the CADB payload.
            size_t offset = kForgeHeaderBytes;
            size_t cadHeader = 0;
            while (offset + kForgeSectionHeaderBytes <= corrupt.size()) {
                if (std::memcmp(&corrupt[offset], kSectionTagCad, 4) == 0) {
                    cadHeader = offset;
                    break;
                }
                uint64_t payloadBytes = 0;
                std::memcpy(&payloadBytes, &corrupt[offset + 8], 8);
                offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
            }
            bool refused = false;
            bool truncatedRefused = false;
            if (cadHeader != 0) {
                uint64_t payloadBytes = 0;
                std::memcpy(&payloadBytes, &corrupt[cadHeader + 8], 8);
                const size_t payload = cadHeader + kForgeSectionHeaderBytes;
                // bodyCount(4) + objectId(8), then the plane code.
                corrupt[payload + 12] = 9;
                const uint32_t crc = crc32IsoHdlc(&corrupt[payload], static_cast<size_t>(payloadBytes));
                std::memcpy(&corrupt[cadHeader + 16], &crc, 4);
                ProjectDocument ignored;
                refused = decodeProject(corrupt.data(), corrupt.size(), &ignored)
                          == ProjectCodecStatus::InvalidSemanticValue;

                std::vector<uint8_t> shortBytes = bytes;
                shortBytes.resize(payload + 20);
                // The header's fileBytes and the section's length still say
                // more; the reader must notice before it reads past the end.
                truncatedRefused = decodeProject(shortBytes.data(), shortBytes.size(), &ignored)
                                   == ProjectCodecStatus::Truncated;
            }
            r.check("CADR0_36_a_bad_workplane_code_is_refused_at_decode", refused);
            r.check("CADR0_36_a_truncated_cad_section_is_refused", truncatedRefused);
        }
        // An older reader's gate: the header flag bit is the one they refuse.
        r.check("CADR0_36_the_header_announces_the_cad_branch",
                bytes.size() > 15 && (bytes[15] & kHeaderFlagHasCad) != 0);
        // A legacy project is byte-for-byte what it was: no CAD branch costs
        // nothing.
        {
            ConstructionScene plain;
            const ProjectDocument legacy = captureProjectDocument(plain, ProjectKind::Construction);
            const std::vector<uint8_t> legacyBytes = encodeProjectV1(legacy);
            r.check("CADR0_36_a_project_without_cad_carries_no_cad_section",
                    !legacy.hasCad && legacyBytes.size() > 15
                            && (legacyBytes[15] & kHeaderFlagHasCad) == 0);
        }
    }

    // -----------------------------------------------------------------------
    // CADR0-37: export regenerates
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        SceneObject* body = scene.addCadBody(bodyState(rectangleSketch(), 1, 1.5));
        publishSceneObject(*body);
        GlbExportScene exported;
        const GlbExportStatus why =
                captureGlbExportScene(scene, ProjectKind::Construction, &exported);
        float hiZ = -1e30f;
        if (exported.bodies.size() == 2) {
            for (const RenderVertex& v : exported.bodies[1].render.vertices) {
                hiZ = std::fmax(hiZ, v.position[2]);
            }
        }
        r.check("CADR0_37_export_carries_the_regenerated_cad_mesh",
                why == GlbExportStatus::Ok && exported.bodies.size() == 2
                        && exported.bodies[1].objectId == body->objectId()
                        && exported.bodies[1].render.sourceVertexCount == cadExtrusionVertexCount(4)
                        && nearf(hiZ, 1.5f));
        applyCadExtrude(*body->cadOrNull(), 4.0, ExtrudeDirection::AlongNormal);
        GlbExportScene edited;
        hiZ = -1e30f;
        if (captureGlbExportScene(scene, ProjectKind::Construction, &edited) == GlbExportStatus::Ok
            && edited.bodies.size() == 2) {
            for (const RenderVertex& v : edited.bodies[1].render.vertices) {
                hiZ = std::fmax(hiZ, v.position[2]);
            }
        }
        // Deliberately WITHOUT republishing: the export reads the truth, not
        // the store.
        r.check("CADR0_37_export_reflects_an_edited_depth_without_a_republish", nearf(hiZ, 4.0f));
        std::vector<uint8_t> glb = exportSceneAsGlb(scene, ProjectKind::Construction);
        r.check("CADR0_37_the_glb_encodes", glb.size() > 12 && glb[0] == 'g' && glb[1] == 'l');
    }

    // -----------------------------------------------------------------------
    // CADR0-39: a CAD body does not sculpt, by name, and Delete is neutral
    // -----------------------------------------------------------------------
    {
        ConstructionScene scene;
        SceneObject* body = scene.addCadBody(bodyState(rectangleSketch(), 1, 1.5));
        ConstructionMesh source;
        r.check("CADR0_39_a_cad_body_offers_no_sculpt_source_this_stage",
                body != nullptr && !buildSculptSourceMesh(*body, &source));
    }

    // -----------------------------------------------------------------------
    // CADR0-08..11, 14: the sketch session, through real touch samples
    // -----------------------------------------------------------------------
    {
        const int w = 1080;
        const int h = 1920;
        SketchSession session;
        r.check("CADR0_14_an_inactive_session_refuses_every_act",
                session.finish() == CadStatus::NotSketching
                        && session.deleteSelected() == CadStatus::NotSketching
                        && session.overlay(0.001f)->vertices.empty());
        r.check("CADR0_08_begin_opens_an_editing_session",
                session.begin(Workplane::XY) == CadStatus::Ok
                        && session.state() == SketchSessionState::Editing
                        && session.begin(Workplane::XZ) == CadStatus::NotSketching
                        && session.plane() == Workplane::XY);
        CameraController camera = sketchCamera(Workplane::XY, w, h);
        const CameraSnapshot snapshot = camera.snapshot();
        Touch touch{session, snapshot, w, h};

        // Pixel -> sketch -> pixel is the identity through the plane.
        {
            SketchPoint p{0.75, -1.25};
            float x = 0.0f, y = 0.0f;
            SketchPoint back;
            r.check("CADR0_08_screen_and_sketch_coordinates_round_trip",
                    touch.sketchPoint(p, &x, &y)
                            && session.screenToSketch(snapshot, x, y, w, h, &back)
                            && near(back.u, p.u, 1e-3) && near(back.v, p.v, 1e-3));
        }

        // A rectangle by dragging corner to corner, on grid points.
        session.setTool(SketchTool::Rectangle);
        // The grid is view-adaptive since CAD-A3: a drag's corners land on
        // EXACT multiples of the current step (whatever that step is at this
        // zoom), which is the real invariant. The step here is deterministic
        // for this fixed test camera but the assertion does not hardcode it.
        const auto onGrid = [&](double x) {
            const double s = session.gridStep();
            return s > 0.0 && std::fabs(x / s - std::round(x / s)) < 1e-9;
        };
        r.check("CADR0_09_a_dragged_rectangle_snaps_to_the_grid_exactly",
                touch.drag(SketchPoint{-1.0, -0.4}, SketchPoint{1.0, 0.6})
                        && session.sketch().entities.size() == 1
                        && session.sketch().entities[0].rectangle() != nullptr
                        && onGrid(session.sketch().entities[0].rectangle()->center.u
                                  - session.sketch().entities[0].rectangle()->width * 0.5)
                        && onGrid(session.sketch().entities[0].rectangle()->center.u
                                  + session.sketch().entities[0].rectangle()->width * 0.5)
                        && session.sketch().entities[0].rectangle()->width > 0.0
                        && session.lastSnapKind() == SketchSnapKind::Grid
                        && session.selectedEntityId() == 1);
        // A drag that starts slightly off a grid point still lands on one.
        r.check("CADR0_09_an_off_grid_drag_lands_on_the_grid",
                touch.drag(SketchPoint{2.03, 1.04}, SketchPoint{3.02, 2.02})
                        && session.sketch().entities.size() == 2
                        && onGrid(session.sketch().entities[1].rectangle()->center.u
                                  - session.sketch().entities[1].rectangle()->width * 0.5)
                        && onGrid(session.sketch().entities[1].rectangle()->center.v
                                  - session.sketch().entities[1].rectangle()->height * 0.5)
                        && session.sketch().entities[1].rectangle()->width > 0.0);
        // Numeric edit of the first rectangle: exact, never snapped -- and it
        // puts that rectangle's corners OFF the grid, which is what lets the
        // endpoint-snap case below tell an endpoint snap from a grid snap.
        r.check("CADR0_12_a_typed_rectangle_is_exact",
                session.select(1)
                        && session.replaceEntity(1, SketchRectangle{{0.0, 0.0}, 1.234567, 0.333})
                                   == CadStatus::Ok
                        && session.sketch().entities[0].rectangle()->width == 1.234567
                        && session.replaceEntity(1, SketchRectangle{{0.0, 0.0}, 0.0, 1.0})
                                   == CadStatus::ZeroSizeRectangle
                        && session.sketch().entities[0].rectangle()->width == 1.234567);
        session.setTool(SketchTool::Circle);
        r.check("CADR0_07_a_dragged_circle_places_its_centre_and_radius",
                touch.drag(SketchPoint{-3.0, 2.0}, SketchPoint{-2.4, 2.0})
                        && session.sketch().entities.size() == 3
                        && session.sketch().entities[2].circle() != nullptr
                        && session.sketch().entities[2].circle()->radius > 0.0
                        && onGrid(session.sketch().entities[2].circle()->center.u)
                        && onGrid(session.sketch().entities[2].circle()->center.u
                                  - session.sketch().entities[2].circle()->radius));
        // Endpoint snapping: a line drawn to a point near the first
        // rectangle's off-grid corner (0.6172835, 0.1665). The grid alone
        // would have put the end at (0.5, 0.25) or (0.75, 0.25); the endpoint
        // wins, and the stored value is the corner's own exact coordinate.
        session.setTool(SketchTool::Line);
        r.check("CADR0_08_a_line_end_snaps_to_an_existing_corner_exactly",
                touch.drag(SketchPoint{-4.0, -3.0}, SketchPoint{0.62, 0.17})
                        && session.sketch().entities.size() == 4
                        && session.sketch().entities[3].line() != nullptr
                        && session.sketch().entities[3].line()->end.u == 1.234567 * 0.5
                        && session.sketch().entities[3].line()->end.v == 0.333 * 0.5
                        && session.sketch().entities[3].line()->start.u == -4.0
                        && session.lastSnapKind() == SketchSnapKind::Endpoint);
        // A tap with the line tool places nothing.
        r.check("CADR0_04_a_tap_with_the_line_tool_places_nothing",
                touch.tap(SketchPoint{-4.0, 3.0}) && session.sketch().entities.size() == 4);

        // Selection by tapping, and Delete.
        session.setTool(SketchTool::Select);
        r.check("CADR0_10_tapping_an_edge_selects_that_entity",
                touch.tap(SketchPoint{0.62, 0.0}) && session.selectedEntityId() == 1);
        r.check("CADR0_10_tapping_empty_space_clears_the_selection",
                touch.tap(SketchPoint{-4.0, 4.0}) && session.selectedEntityId() == kNoSketchEntity);
        r.check("CADR0_10_tapping_a_circle_ring_selects_it",
                touch.tap(SketchPoint{-2.5, 2.0}) && session.selectedEntityId() == 3);
        r.check("CADR0_11_delete_removes_the_selected_entity_only",
                session.deleteSelected() == CadStatus::Ok && session.sketch().entities.size() == 3
                        && findSketchEntity(session.sketch(), 3) == nullptr
                        && session.selectedEntityId() == kNoSketchEntity
                        && session.deleteSelected() == CadStatus::UnknownEntity
                        && session.sketch().nextEntityId == 5);

        // A polyline by tapping, closed by tapping its first vertex.
        session.setTool(SketchTool::Polyline);
        r.check("CADR0_05_a_tapped_polyline_closes_on_its_first_vertex",
                touch.tap(SketchPoint{-3.0, -3.0}) && touch.tap(SketchPoint{-1.0, -3.0})
                        && touch.tap(SketchPoint{-1.0, -1.0}) && session.polylineInProgress()
                        && touch.tap(SketchPoint{-3.0, -3.0}) && !session.polylineInProgress()
                        && session.sketch().entities.size() == 4
                        && session.sketch().entities[3].polyline() != nullptr
                        && session.sketch().entities[3].polyline()->closed
                        && session.sketch().entities[3].polyline()->vertices.size() == 3);
        // And one left open by changing tool.
        r.check("CADR0_05_changing_tool_ends_a_polyline_open",
                touch.tap(SketchPoint{3.0, -3.0}) && touch.tap(SketchPoint{4.0, -3.0})
                        && session.setTool(SketchTool::Select) && !session.polylineInProgress()
                        && session.sketch().entities.size() == 5
                        && !session.sketch().entities[4].polyline()->closed);

        // A second finger cancels the drag and hands the gesture on.
        session.setTool(SketchTool::Rectangle);
        {
            float x0, y0, x1, y1;
            touch.sketchPoint(SketchPoint{5.0, 5.0}, &x0, &y0);
            touch.sketchPoint(SketchPoint{6.0, 6.0}, &x1, &y1);
            const bool down = touch.at(TouchAction::Down, x0, y0);
            TouchPointer two[2] = {{7, x0, y0}, {8, x1, y1}};
            const bool handed = !session.onTouch(TouchAction::PointerDown, 8, two, 2, snapshot, w, h);
            const bool moveHanded = !session.onTouch(TouchAction::Move, -1, two, 2, snapshot, w, h);
            r.check("CADR0_14_a_second_finger_cancels_the_drag_and_navigates",
                    down && handed && moveHanded && !session.gestureActive()
                            && session.sketch().entities.size() == 5);
        }

        // The overlay carries the grid, the axes and the entities, in ranges.
        {
            const SketchOverlayPtr overlay = session.overlay(0.002f);
            // Five ranges since SKETCH-UX-R1: the dimension annotation is its
            // own, so the renderer draws it in the annotation weight rather
            // than as geometry. It is EMPTY here, because nothing is selected.
            bool ranged = overlay->ranges.size() == 5
                          && overlay->ranges[0].style == SketchOverlayStyle::GridMinor
                          && overlay->ranges[3].style == SketchOverlayStyle::Entities
                          && overlay->ranges[3].vertexCount > 0
                          && overlay->ranges[4].style == SketchOverlayStyle::Dimension
                          && overlay->vertices.size() <= kMaxSketchOverlayVertices;
            const uint64_t rev = overlay->revision;
            const SketchOverlayPtr again = session.overlay(0.002f);
            r.check("CADR0_14_the_overlay_is_ranged_and_cached_until_a_change",
                    ranged && again.get() == overlay.get() && again->revision == rev);
        }

        // Finish: this sketch closes several profiles (two rectangles, a
        // triangle polyline), and an open polyline and a line are ignored
        // as profiles but keep the sketch valid.
        const CadStatus finished = session.finish();
        r.check("CADR0_21_finish_extracts_profiles_and_leaves_the_choice_open",
                finished == CadStatus::Ok && session.state() == SketchSessionState::Ready
                        && session.profiles().profiles.size() == 3
                        && session.selectedProfileId() == kNoSketchEntity);
        ConstructionScene scene;
        ConstructionHistory history(scene);
        ObjectId created = kNoObject;
        r.check("CADR0_21_commit_with_no_chosen_profile_is_ambiguous",
                session.commit(scene, history, &created) == CadStatus::AmbiguousProfile
                        && created == kNoObject && scene.bodyCount() == 1
                        && history.undoDepth() == 0 && session.state() == SketchSessionState::Ready);
        r.check("CADR0_21_a_profile_is_chosen_by_anchor_and_a_bad_one_refused",
                session.selectProfile(99) == CadStatus::ProfileNotFound
                        && session.selectProfile(2) == CadStatus::Ok
                        && session.selectedProfileId() == 2);
        r.check("CADR0_30_extrude_depth_is_typed_exactly_and_validated",
                session.setExtrude(0.0, ExtrudeDirection::AlongNormal)
                                == CadStatus::InvalidExtrudeDepth
                        && session.setExtrude(2.125, ExtrudeDirection::AgainstNormal)
                                   == CadStatus::Ok
                        && session.extrude().depth == 2.125);
        // The extrude preview is in the overlay while Ready.
        {
            const SketchOverlayPtr overlay = session.overlay(0.002f);
            r.check("CADR0_14_the_extrude_preview_is_drawn_while_ready",
                    overlay->ranges.size() == 5 && overlay->ranges[3].vertexCount > 40);
        }
        // Back to editing keeps the sketch; finishing again re-extracts.
        session.backToEditing();
        r.check("CADR0_14_back_to_editing_keeps_the_sketch",
                session.state() == SketchSessionState::Editing
                        && session.sketch().entities.size() == 5 && session.finish() == CadStatus::Ok
                        && session.selectProfile(2) == CadStatus::Ok);

        // Commit: one body, one step, and the session is over.
        const CadBodyState candidate = session.candidateState();
        r.check("CADR0_28_commit_creates_one_cad_body_as_one_step",
                session.commit(scene, history, &created) == CadStatus::Ok && created != kNoObject
                        && scene.bodyCount() == 2 && history.undoDepth() == 1
                        && session.state() == SketchSessionState::Inactive
                        && scene.findBody(created)->isCad()
                        && sameCadBodyState(scene.findBody(created)->cadOrNull()->state(), candidate)
                        && scene.findBody(created)->cadOrNull()->extrude().depth == 2.125
                        && scene.findBody(created)->meshStore().currentRevision() != kNoMeshRevision);
        r.check("CADR0_29_undo_removes_the_committed_body",
                history.undo() && scene.bodyCount() == 1 && history.redo() && scene.bodyCount() == 2);

        // Cancel leaves the project untouched: a fresh session, entities
        // placed, cancelled -- nothing in the scene or the history moved.
        {
            const uint64_t before = projectSemanticFingerprint(scene, ProjectKind::Construction);
            SketchSession volatileSession;
            volatileSession.begin(Workplane::XZ);
            CameraController top = sketchCamera(Workplane::XZ, w, h);
            const CameraSnapshot topSnapshot = top.snapshot();
            Touch topTouch{volatileSession, topSnapshot, w, h};
            volatileSession.setTool(SketchTool::Rectangle);
            const bool drew = topTouch.drag(SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0})
                              && volatileSession.sketch().entities.size() == 1;
            volatileSession.cancel();
            r.check("CADR0_14_a_cancelled_sketch_leaves_the_project_unchanged",
                    drew && volatileSession.state() == SketchSessionState::Inactive
                            && scene.bodyCount() == 2 && history.undoDepth() == 1
                            && projectSemanticFingerprint(scene, ProjectKind::Construction) == before);
        }
        // Finishing an open-only sketch is refused by name.
        {
            SketchSession openOnly;
            openOnly.begin(Workplane::YZ);
            CameraController side = sketchCamera(Workplane::YZ, w, h);
            const CameraSnapshot sideSnapshot = side.snapshot();
            Touch sideTouch{openOnly, sideSnapshot, w, h};
            openOnly.setTool(SketchTool::Polyline);
            const bool placed = sideTouch.tap(SketchPoint{0.0, 0.0}) && sideTouch.tap(SketchPoint{1.0, 0.0})
                                && sideTouch.tap(SketchPoint{1.0, 1.0});
            const CadStatus why = openOnly.finish();
            r.check("CADR0_15_finishing_an_open_sketch_is_refused_by_name",
                    placed && why == CadStatus::OpenProfile
                            && openOnly.state() == SketchSessionState::Editing
                            && openOnly.sketch().entities.size() == 1);
            SketchSession empty;
            empty.begin(Workplane::XY);
            r.check("CADR0_15_finishing_an_empty_sketch_is_refused_by_name",
                    empty.finish() == CadStatus::NoClosedProfile);
        }
        // The view angles look along each plane's normal.
        {
            bool ok = true;
            const Workplane planes[3] = {Workplane::XY, Workplane::XZ, Workplane::YZ};
            for (Workplane plane : planes) {
                CameraController c = sketchCamera(plane, w, h);
                const CameraSnapshot s = c.snapshot();
                const Vec3 toEye = vec3Normalize(vec3Sub(s.eye, s.target));
                const WorkplaneFrame frame = workplaneFrame(plane);
                // The eye sits on the plane's positive normal (within the
                // pitch clamp for the top view), and the projection is
                // orthographic.
                ok &= vec3Dot(toEye, frame.normal) > 0.99f
                      && s.projection == ProjectionMode::Orthographic;
                // U runs right and V runs up on screen.
                float ox, oy, ux, uy, vx, vy;
                ok &= projectWorldToScreen(s, Vec3{0, 0, 0}, w, h, &ox, &oy)
                      && projectWorldToScreen(s, frame.uAxis, w, h, &ux, &uy)
                      && projectWorldToScreen(s, frame.vAxis, w, h, &vx, &vy)
                      && ux > ox && std::fabs(uy - oy) < 1.0f && vy < oy && std::fabs(vx - ox) < 1.0f;
            }
            r.check("CADR0_01_the_sketch_view_reads_u_right_and_v_up_on_every_plane", ok);
        }
        // Capture and restore of the camera around a sketch.
        {
            CameraController c;
            c.setViewport(w, h);
            const CameraController::Pose before = c.capturePose();
            c.frameWorkplane(0.0f, 0.0f);
            const bool framed = c.projectionMode() == ProjectionMode::Orthographic && c.yaw() == 0.0f;
            c.restorePose(before);
            r.check("CADR0_14_the_camera_pose_is_restored_after_a_sketch",
                    framed && c.projectionMode() == before.projection && c.yaw() == before.yaw
                            && c.pitch() == before.pitch && c.distance() == before.distance);
        }
    }

    // -----------------------------------------------------------------------
    // Performance: bounded, measured, reported
    // -----------------------------------------------------------------------
    {
        struct Case {
            const char* name;
            CadSketch sketch;
        };
        Case cases[4] = {
                {"rectangle", rectangleSketch()},
                {"circle32", circleSketch()},
                {"polyline32", polygonSketch(32, true)},
                {"polyline128", polygonSketch(128, true)},
        };
        char buffer[512];
        std::string report;
        bool allOk = true;
        for (const Case& c : cases) {
            const auto t0 = std::chrono::steady_clock::now();
            const ProfileExtraction extraction = extractClosedProfiles(c.sketch);
            const double extractUs = microseconds(t0);
            std::vector<uint32_t> tri;
            const auto t1 = std::chrono::steady_clock::now();
            const CadStatus triWhy = extraction.profiles.empty()
                                             ? CadStatus::NoClosedProfile
                                             : triangulateSimplePolygon(extraction.profiles[0].polygon, &tri);
            const double triUs = microseconds(t1);
            ConstructionMesh mesh;
            const auto t2 = std::chrono::steady_clock::now();
            const CadStatus genWhy = generateCadMesh(bodyState(c.sketch, 1, 1.0), &mesh);
            const double genUs = microseconds(t2);
            allOk &= triWhy == CadStatus::Ok && genWhy == CadStatus::Ok && watertight(mesh);
            std::snprintf(buffer, sizeof(buffer), "%s extractUs=%.1f triangulateUs=%.1f regenerateUs=%.1f v=%u i=%u; ",
                          c.name, extractUs, triUs, genUs,
                          static_cast<unsigned>(mesh.vertices.size()),
                          static_cast<unsigned>(mesh.indices.size()));
            report += buffer;
        }
        g_performance = report;
        r.check("CADR0_22_all_four_performance_cases_regenerate_watertight", allOk);
    }

    return r.n;
}

}  // namespace forgeshape
