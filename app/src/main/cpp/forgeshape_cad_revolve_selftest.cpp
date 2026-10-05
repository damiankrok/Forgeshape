#include "forgeshape_cad_revolve_selftest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_kernel.h"
#include "forgeshape_cad_revolve.h"
#include "forgeshape_camera.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_input.h"
#include "forgeshape_project_bootstrap.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_region.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool ok) { out->push_back(ArrangementSelfTestCheck{name, ok}); }
};

SketchEntityId add(CadSketch* sketch, SketchEntity::Payload payload) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, std::move(payload), &id);
    return id;
}

SketchRectangle rectangle(double cu, double cv, double w, double h) {
    SketchRectangle r;
    r.center = SketchPoint{cu, cv};
    r.width = w;
    r.height = h;
    return r;
}

SketchLine line(double u0, double v0, double u1, double v1) {
    SketchLine l;
    l.start = SketchPoint{u0, v0};
    l.end = SketchPoint{u1, v1};
    return l;
}

SketchCircle circle(double cu, double cv, double r) {
    SketchCircle c;
    c.center = SketchPoint{cu, cv};
    c.radius = r;
    return c;
}

// The canonical case: a 1 x 1 m square from u 1..2, v 0..1 (entity 1) and a
// separate Line on u = 0 from v -1 to v 2 (entity 2) as the axis.
CadSketch offsetSketch() {
    CadSketch s;
    add(&s, rectangle(1.5, 0.5, 1.0, 1.0));
    add(&s, line(0.0, -1.0, 0.0, 2.0));
    return s;
}

RevolveFeature loopRevolve(SketchEntityId profile, CadSketchEdgeRef axis, double degrees,
                           RevolveDirection direction = RevolveDirection::Positive,
                           std::vector<SketchEntityId> holes = {},
                           std::vector<ProfileRegionRef> additional = {}) {
    RevolveFeature r;
    r.profileEntityId = profile;
    r.profileHoleIds = std::move(holes);
    r.additionalRegions = std::move(additional);
    r.axis = axis;
    r.angleDegrees = degrees;
    r.direction = direction;
    return r;
}

// Pappus over the polygonal path the mesh takes: every point of the area at
// radius r sweeps a regular polygon of `revolveSegmentCount` chords, and the
// ruled wedge between two consecutive half-planes holds exactly
// sin(step) * A * rbar. So the mesh's volume is N * sin(angle / N) * A * rbar.
double revolvedVolume(double area, double centroidRadius, double degrees) {
    const double n = static_cast<double>(revolveSegmentCount(degrees));
    return n * std::sin(degrees * kPi / 180.0 / n) * area * centroidRadius;
}

bool near(double a, double b, double relative) {
    return std::fabs(a - b) <= relative * std::max(1.0, std::fabs(b));
}

double micros(std::chrono::steady_clock::time_point since) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - since)
            .count();
}

// V - E + F over the mesh's own index topology: 2 for a sphere-like shell,
// 0 for a torus-like one; summed over the shells of a multi-shell solid.
long eulerCharacteristic(const ConstructionMesh& mesh) {
    std::set<std::pair<uint32_t, uint32_t>> edges;
    std::set<uint32_t> used;
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        for (int k = 0; k < 3; ++k) {
            const uint32_t a = mesh.indices[t + static_cast<size_t>(k)];
            const uint32_t b = mesh.indices[t + static_cast<size_t>((k + 1) % 3)];
            edges.insert(std::make_pair(std::min(a, b), std::max(a, b)));
            used.insert(a);
        }
    }
    return static_cast<long>(used.size()) - static_cast<long>(edges.size())
           + static_cast<long>(mesh.indices.size() / 3u);
}

// How many vertices share their exact position with another: a duplicated seam
// would show up here.
size_t coincidentVertices(const ConstructionMesh& mesh) {
    std::set<std::tuple<float, float, float>> seen;
    size_t duplicates = 0;
    for (const MeshVertex& v : mesh.vertices) {
        if (!seen.insert(std::make_tuple(v.position[0], v.position[1], v.position[2])).second) {
            ++duplicates;
        }
    }
    return duplicates;
}

uint64_t indexDigest(const ConstructionMesh& mesh) {
    uint64_t h = 14695981039346656037ull;
    for (uint32_t index : mesh.indices) {
        for (int i = 0; i < 4; ++i) {
            h ^= (index >> (i * 8)) & 0xFFu;
            h *= 1099511628211ull;
        }
    }
    return h;
}

bool samePositions(const ConstructionMesh& a, const ConstructionMesh& b) {
    if (a.vertices.size() != b.vertices.size() || a.indices != b.indices) return false;
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (std::memcmp(a.vertices[i].position, b.vertices[i].position, sizeof(float) * 3) != 0) {
            return false;
        }
    }
    return true;
}

bool regenerate(const CadBodyState& state, CadBodyMesh* mesh, CadStatus* why = nullptr) {
    const CadStatus status = regenerateCadBody(state, mesh);
    if (why != nullptr) *why = status;
    return status == CadStatus::Ok;
}

// The face-table index of the first face wearing `kind`, or the table size.
size_t faceOfKind(const CadBodyMesh& mesh, CadFaceKind kind) {
    for (size_t i = 0; i < mesh.faces.size(); ++i) {
        if (mesh.faces[i].token.kind == kind) return i;
    }
    return mesh.faces.size();
}

// The mean outward normal (area-weighted) of every triangle tagged `face`.
Vec3 faceNormal(const CadBodyMesh& mesh, size_t face) {
    Vec3 sum{0.0f, 0.0f, 0.0f};
    for (size_t t = 0; t < mesh.triangleFace.size(); ++t) {
        if (mesh.triangleFace[t] != face) continue;
        const MeshVertex& a = mesh.mesh.vertices[mesh.mesh.indices[t * 3]];
        const MeshVertex& b = mesh.mesh.vertices[mesh.mesh.indices[t * 3 + 1]];
        const MeshVertex& c = mesh.mesh.vertices[mesh.mesh.indices[t * 3 + 2]];
        const Vec3 pa{a.position[0], a.position[1], a.position[2]};
        const Vec3 pb{b.position[0], b.position[1], b.position[2]};
        const Vec3 pc{c.position[0], c.position[1], c.position[2]};
        sum = vec3Add(sum, vec3Cross(vec3Sub(pb, pa), vec3Sub(pc, pa)));
    }
    return sum;
}

ProjectDocument documentFor(const CadBodyState& state) {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;
    ProjectBodyPlacement placement;
    placement.objectId = 1;
    document.scene.bodies.push_back(placement);
    document.hasCad = true;
    ProjectCadBody body;
    body.objectId = 1;
    body.state = state;
    document.cad.bodies.push_back(std::move(body));
    return document;
}

size_t cadPayloadAt(const std::vector<uint8_t>& bytes, uint16_t* outVersion = nullptr,
                    uint64_t* outPayloadBytes = nullptr) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            if (outVersion != nullptr) {
                *outVersion = static_cast<uint16_t>(bytes[offset + 4] | (bytes[offset + 5] << 8));
            }
            if (outPayloadBytes != nullptr) *outPayloadBytes = payloadBytes;
            return offset + kForgeSectionHeaderBytes;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
    return 0;
}

uint16_t cadVersionOf(const std::vector<uint8_t>& bytes) {
    uint16_t version = 0;
    return cadPayloadAt(bytes, &version) != 0 ? version : 0;
}

// The bytes with one byte of the CADB payload replaced and the section CRC
// repaired, so only the decoder's own rule can refuse them.
std::vector<uint8_t> patchedCadByte(std::vector<uint8_t> bytes, size_t payloadOffset, uint8_t value) {
    uint64_t payloadBytes = 0;
    const size_t base = cadPayloadAt(bytes, nullptr, &payloadBytes);
    if (base == 0 || payloadOffset >= payloadBytes) return {};
    bytes[base + payloadOffset] = value;
    const uint32_t crc = crc32IsoHdlc(&bytes[base], static_cast<size_t>(payloadBytes));
    std::memcpy(&bytes[base - kForgeSectionHeaderBytes + 16], &crc, 4);
    return bytes;
}

ProjectCodecStatus decodeStatus(const std::vector<uint8_t>& bytes, ProjectDocument* out = nullptr) {
    ProjectDocument scratch;
    if (bytes.empty()) return ProjectCodecStatus::Truncated;
    return decodeProject(bytes.data(), bytes.size(), out != nullptr ? out : &scratch);
}

std::string sha(const std::vector<uint8_t>& bytes) {
    return bytes.empty() ? std::string("unavailable") : projectFixtureSha256Hex(bytes);
}

// --- the four v7 fixtures (DATA_PACKAGE_SPEC.md §7h; the independent builder
// constructs the same bytes from the text) ------------------------------------

CadBodyState fullFixtureState() {
    return makeCadRevolveBodyState(offsetSketch(), loopRevolve(1, CadSketchEdgeRef{2, 0}, 360.0));
}

CadBodyState partialFixtureState() {
    return makeCadRevolveBodyState(
            offsetSketch(),
            loopRevolve(1, CadSketchEdgeRef{2, 0}, 90.0, RevolveDirection::Negative));
}

std::vector<uint8_t> fullFixtureBytes() { return encodeProjectV1(documentFor(fullFixtureState())); }
std::vector<uint8_t> partialFixtureBytes() {
    return encodeProjectV1(documentFor(partialFixtureState()));
}
std::vector<uint8_t> badAxisFixtureBytes() {
    CadBodyState state = fullFixtureState();
    state.revolve.axis = CadSketchEdgeRef{7, 0};
    return encodeProjectV1Unchecked(documentFor(state));
}

// The payload offset of the base feature's KIND byte in a one-body document
// whose one sketch carries `entityBytes` of entities: body id 8, two marks 8,
// sketch count 4, sketch id 4, placement 1, plane 1, nextEntityId 4, entity
// count 4, the entities, feature count 4, feature id 4.
size_t baseKindOffset(size_t entityBytes) {
    return 4 + 8 + 8 + 4 + 4 + 1 + 1 + 4 + 4 + entityBytes + 4 + 4;
}

// offsetSketch's entities: a rectangle (id 4, kind 1, four f64) and a line
// (id 4, kind 1, four f64).
constexpr size_t kOffsetSketchEntityBytes = (4 + 1 + 32) + (4 + 1 + 32);

std::vector<uint8_t> badKindFixtureBytes() {
    return patchedCadByte(fullFixtureBytes(), baseKindOffset(kOffsetSketchEntityBytes), 9);
}

// The digests the independent PowerShell builder produces for the same four
// documents (scripts/build-forge-corpus.ps1, `cad_*_v7.forge`).
constexpr const char* kFullFixtureSha =
        "1e6d021408bf3959659cef1fb119d3e5a4ced0442289b3ab5bb583fdcde5d450";
constexpr const char* kPartialFixtureSha =
        "7b72037a923f8c767a8694cc6e080e0ef8a8eb624ee926e02f288e5541b2fdd2";
constexpr const char* kBadAxisFixtureSha =
        "8be1bee7210c7286e72c8ba94a44f8724bda1400feff6589ef64b3ce30d5ecd1";
constexpr const char* kBadKindFixtureSha =
        "404e648523be5069334321bf20cf2abf077ed0ba0b6bb48cc728c30ab56a9fb5";

// ---------------------------------------------------------------------------
// A session over XY driven through the gesture path the product uses: each
// entity drawn by a real drag and then TYPED exact, like MULTIFACE's driver.
// ---------------------------------------------------------------------------

struct Driver {
    static constexpr int kW = 1000;
    static constexpr int kH = 1000;
    SketchSession sketch;
    CameraController camera;

    bool begin() {
        if (sketch.begin(Workplane::XY) != CadStatus::Ok) return false;
        camera.setViewport(kW, kH);
        const SketchFrame& f = sketch.frame();
        camera.frameSketchView(f.origin, f.u, f.v, f.n);
        return true;
    }
    bool at(TouchAction action, float x, float y, int32_t id = 7) {
        TouchPointer p{id, x, y};
        return sketch.onTouch(action, action == TouchAction::Up ? id : -1, &p, 1, camera.snapshot(),
                              kW, kH);
    }
    bool place(SketchTool tool, SketchEntity::Payload exact) {
        sketch.setTool(tool);
        float x0, y0, x1, y1;
        if (!sketch.sketchToScreen(camera.snapshot(), SketchPoint{0.13, 0.27}, kW, kH, &x0, &y0)
            || !sketch.sketchToScreen(camera.snapshot(), SketchPoint{0.61, 0.83}, kW, kH, &x1, &y1)) {
            return false;
        }
        const size_t before = sketch.sketch().entities.size();
        bool ok = at(TouchAction::Down, x0, y0);
        for (int step = 1; step <= 4; ++step) {
            const float t = step / 4.0f;
            ok &= at(TouchAction::Move, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
        }
        ok &= at(TouchAction::Up, x1, y1);
        if (!ok || sketch.sketch().entities.size() != before + 1u) return false;
        return sketch.replaceEntity(sketch.sketch().entities.back().id(), std::move(exact))
               == CadStatus::Ok;
    }
    bool finishOn(const CadSketch& source) {
        if (!begin()) return false;
        for (const SketchEntity& entity : source.entities) {
            SketchTool tool = SketchTool::Line;
            if (entity.rectangle() != nullptr) tool = SketchTool::Rectangle;
            if (entity.circle() != nullptr) tool = SketchTool::Circle;
            if (!place(tool, entity.payload())) return false;
        }
        return sketch.finish() == CadStatus::Ok;
    }
    // A Ready tap at a sketch point, through the gesture path.
    bool tapAt(const SketchPoint& p) {
        float x = 0.0f;
        float y = 0.0f;
        if (!sketch.sketchToScreen(camera.snapshot(), p, kW, kH, &x, &y)) return false;
        at(TouchAction::Down, x, y);
        at(TouchAction::Up, x, y);
        return true;
    }
    // The revolve view, exactly as the product installs it after an axis.
    bool takeRevolveView() {
        CameraController::Pose pose;
        if (!sketch.revolveViewPose(camera.capturePose(), &pose)) return false;
        camera.restorePose(pose);
        return true;
    }
};

// ---------------------------------------------------------------------------
// REV-01..05: the solid and its exact parameters
// ---------------------------------------------------------------------------

void testSolids(Checks& r) {
    const CadBodyState full = fullFixtureState();
    CadBodyMesh fullMesh;
    CadStatus why = CadStatus::Ok;
    const bool fullOk = regenerate(full, &fullMesh, &why);
    r.check("REV_01_a_square_offset_from_a_line_axis_revolves_360_into_one_valid_solid",
            validateCadBodyState(full) == CadStatus::Ok && fullOk && fullMesh.components == 1u
                    && near(fullMesh.volume, revolvedVolume(1.0, 1.5, 360.0), 1e-9)
                    && fullMesh.mesh.indices.size() / 3u == 256u
                    && fullMesh.mesh.vertices.size() == 128u);

    CadSketch touching;
    const SketchEntityId square = add(&touching, rectangle(0.5, 0.5, 1.0, 1.0));
    CadBodyMesh cylinder;
    const CadBodyState cylinderState =
            makeCadRevolveBodyState(touching, loopRevolve(square, CadSketchEdgeRef{square, 3}, 360.0));
    r.check("REV_02_a_square_touching_its_axis_revolves_360_into_a_solid_cylinder",
            regenerate(cylinderState, &cylinder) && cylinder.components == 1u
                    && near(cylinder.volume, revolvedVolume(1.0, 0.5, 360.0), 1e-9)
                    && eulerCharacteristic(cylinder.mesh) == 2);

    const CadBodyState quarter = makeCadRevolveBodyState(
            offsetSketch(), loopRevolve(1, CadSketchEdgeRef{2, 0}, 90.0));
    CadBodyMesh quarterMesh;
    const bool quarterOk = regenerate(quarter, &quarterMesh);
    const size_t startCap = faceOfKind(quarterMesh, CadFaceKind::CapPlane);
    const size_t endCap = faceOfKind(quarterMesh, CadFaceKind::CapFar);
    size_t startTriangles = 0;
    size_t endTriangles = 0;
    for (uint32_t tag : quarterMesh.triangleFace) {
        startTriangles += tag == startCap ? 1u : 0u;
        endTriangles += tag == endCap ? 1u : 0u;
    }
    r.check("REV_03_a_90_degree_partial_sweep_has_a_start_cap_and_an_end_cap_and_a_valid_solid",
            quarterOk && quarterMesh.components == 1u
                    && near(quarterMesh.volume, revolvedVolume(1.0, 1.5, 90.0), 1e-9)
                    && startCap < quarterMesh.faces.size() && endCap < quarterMesh.faces.size()
                    && startTriangles == 2u && endTriangles == 2u
                    && faceOfKind(fullMesh, CadFaceKind::CapPlane) == fullMesh.faces.size());

    bool exact = true;
    for (double degrees : {180.0, 37.5, 90.0, 360.0}) {
        const CadBodyState state = makeCadRevolveBodyState(
                offsetSketch(), loopRevolve(1, CadSketchEdgeRef{2, 0}, degrees));
        CadBodyMesh mesh;
        exact = exact && regenerate(state, &mesh) && state.revolve.angleDegrees == degrees
                && near(mesh.volume, revolvedVolume(1.0, 1.5, degrees), 1e-9);
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state));
        ProjectDocument back;
        exact = exact && decodeStatus(bytes, &back) == ProjectCodecStatus::Ok
                && back.cad.bodies.size() == 1u
                && back.cad.bodies[0].state.revolve.angleDegrees == degrees;
    }
    r.check("REV_04_180_and_37_5_degrees_are_stored_regenerated_and_round_tripped_exactly", exact);

    const CadBodyState flipped = makeCadRevolveBodyState(
            offsetSketch(), loopRevolve(1, CadSketchEdgeRef{2, 0}, 90.0, RevolveDirection::Negative));
    CadBodyMesh flippedMesh;
    const bool flippedOk = regenerate(flipped, &flippedMesh);
    // The axis is +v on XY, the square on the -u side's left normal... the
    // sweep rotates the square about v: one sense ends on +z, the other on -z.
    float zMin[2] = {1e9f, 1e9f};
    float zMax[2] = {-1e9f, -1e9f};
    const CadBodyMesh* meshes[2] = {&quarterMesh, &flippedMesh};
    for (int m = 0; m < 2; ++m) {
        for (const MeshVertex& v : meshes[m]->mesh.vertices) {
            zMin[m] = std::min(zMin[m], v.position[2]);
            zMax[m] = std::max(zMax[m], v.position[2]);
        }
    }
    r.check("REV_05_flip_keeps_the_exact_magnitude_and_sweeps_the_other_way",
            flippedOk && flipped.revolve.angleDegrees == quarter.revolve.angleDegrees
                    && near(flippedMesh.volume, quarterMesh.volume, 1e-12)
                    && near(zMin[0], -zMax[1], 1e-5) && near(zMax[0], -zMin[1], 1e-5)
                    && (zMax[0] > 0.5f) != (zMax[1] > 0.5f));
}

// ---------------------------------------------------------------------------
// REV-06..11: the axis's semantic identity and the side rule
// ---------------------------------------------------------------------------

void testAxis(Checks& r) {
    // REV-06: a Line axis is (line, 0); the square moving keeps it resolving to
    // the same line, never to a nearer edge.
    {
        CadBodyState state = fullFixtureState();
        CadBodyMesh before;
        regenerate(state, &before);
        CadSketch& sketch = findCadSketchRecord(state, kBaseCadSketchId)->sketch;
        replaceSketchEntity(&sketch, 1, rectangle(2.5, 0.5, 1.0, 1.0));
        RevolveAxis2D axis;
        CadBodyMesh after;
        r.check("REV_06_a_line_axis_is_named_by_line_and_edge_0_and_survives_a_profile_edit",
                state.revolve.axis.entityId == 2u && state.revolve.axis.edgeLocalIndex == 0u
                        && resolveRevolveAxis(sketch, state.revolve.axis, &axis) == CadStatus::Ok
                        && axis.start.u == 0.0 && axis.start.v == -1.0 && axis.dv == 1.0
                        && regenerate(state, &after)
                        && near(after.volume, revolvedVolume(1.0, 2.5, 360.0), 1e-9)
                        && resolveRevolveAxis(sketch, CadSketchEdgeRef{2, 1}, nullptr)
                                   == CadStatus::RevolveAxisUnresolved);
    }
    // REV-07: a Polyline SEGMENT axis: segment 1 of an open polyline, by index.
    {
        CadSketch s;
        const SketchEntityId square = add(&s, rectangle(1.5, 0.5, 1.0, 1.0));
        SketchPolyline poly;
        poly.vertices = {SketchPoint{-1.0, -2.0}, SketchPoint{0.0, -1.0}, SketchPoint{0.0, 3.0},
                         SketchPoint{-1.0, 4.0}};
        const SketchEntityId polyline = add(&s, poly);
        const CadBodyState state =
                makeCadRevolveBodyState(s, loopRevolve(square, CadSketchEdgeRef{polyline, 1}, 360.0));
        CadBodyMesh mesh;
        RevolveAxis2D axis;
        r.check("REV_07_a_polyline_segment_axis_is_named_by_its_index_and_resolves_exactly",
                resolveRevolveAxis(s, CadSketchEdgeRef{polyline, 1}, &axis) == CadStatus::Ok
                        && axis.start.u == 0.0 && axis.start.v == -1.0 && axis.end.v == 3.0
                        && regenerate(state, &mesh)
                        && near(mesh.volume, revolvedVolume(1.0, 1.5, 360.0), 1e-9)
                        && resolveRevolveAxis(s, CadSketchEdgeRef{polyline, 3}, nullptr)
                                   == CadStatus::RevolveAxisUnresolved);
    }
    // REV-08: a Rectangle EDGE axis: the square's own right side (edge 1) and
    // its left side (edge 3) both give the cylinder; edge 4 does not exist.
    {
        CadSketch s;
        const SketchEntityId square = add(&s, rectangle(0.5, 0.5, 1.0, 1.0));
        CadBodyMesh right;
        CadBodyMesh left;
        RevolveAxis2D edge1;
        RevolveAxis2D edge3;
        r.check("REV_08_a_rectangle_edge_axis_is_named_by_its_counter_clockwise_edge_index",
                resolveRevolveAxis(s, CadSketchEdgeRef{square, 1}, &edge1) == CadStatus::Ok
                        && edge1.start.u == 1.0 && edge1.start.v == 0.0 && edge1.end.v == 1.0
                        && resolveRevolveAxis(s, CadSketchEdgeRef{square, 3}, &edge3) == CadStatus::Ok
                        && edge3.start.u == 0.0 && edge3.start.v == 1.0 && edge3.end.v == 0.0
                        && regenerate(makeCadRevolveBodyState(
                                              s, loopRevolve(square, CadSketchEdgeRef{square, 1}, 360.0)),
                                      &right)
                        && regenerate(makeCadRevolveBodyState(
                                              s, loopRevolve(square, CadSketchEdgeRef{square, 3}, 360.0)),
                                      &left)
                        && near(right.volume, left.volume, 1e-12)
                        && resolveRevolveAxis(s, CadSketchEdgeRef{square, 4}, nullptr)
                                   == CadStatus::RevolveAxisUnresolved);
    }
    // REV-09: a Circle, an Arc and a Spline are refused as axes, by name.
    {
        CadSketch s;
        const SketchEntityId square = add(&s, rectangle(3.0, 0.5, 1.0, 1.0));
        const SketchEntityId c = add(&s, circle(-2.0, 0.0, 0.5));
        SketchArc arc;
        arc.start = SketchPoint{-1.0, 3.0};
        arc.mid = SketchPoint{-0.5, 3.5};
        arc.end = SketchPoint{0.0, 3.0};
        const SketchEntityId a = add(&s, arc);
        SketchSpline spline;
        spline.points = {SketchPoint{-1.0, -3.0}, SketchPoint{0.0, -2.5}, SketchPoint{1.0, -3.0}};
        const SketchEntityId sp = add(&s, spline);
        bool refused = true;
        for (SketchEntityId curve : {c, a, sp}) {
            refused = refused
                      && resolveRevolveAxis(s, CadSketchEdgeRef{curve, 0}, nullptr)
                                 == CadStatus::RevolveAxisNotStraight
                      && validateCadBodyState(makeCadRevolveBodyState(
                                 s, loopRevolve(square, CadSketchEdgeRef{curve, 0}, 360.0)))
                                 == CadStatus::RevolveAxisNotStraight
                      && sketchEntityStraightEdges(*findSketchEntity(s, curve)).empty();
        }
        r.check("REV_09_a_circle_arc_or_spline_is_refused_as_an_axis_by_name", refused);
    }
    // REV-10: an area with material on both sides of the axis is refused --
    // on its vertices, and on a TRUE circle that dips across between samples.
    {
        CadSketch crossing;
        const SketchEntityId square = add(&crossing, rectangle(0.2, 0.5, 1.0, 1.0));
        const SketchEntityId axis = add(&crossing, line(0.0, -1.0, 0.0, 2.0));
        CadBodyMesh none;
        CadStatus regen = CadStatus::Ok;
        regenerate(makeCadRevolveBodyState(crossing, loopRevolve(square, CadSketchEdgeRef{axis, 0}, 90.0)),
                   &none, &regen);
        // A circle whose cardinal vertex sits a hair OFF the axis while the
        // true circle crosses it: centre 0.9999999 from the axis, radius 1.
        // Its tessellated vertices are all clear; the exact arc test is not.
        CadSketch dip;
        const SketchEntityId disk = add(&dip, circle(1.0, 0.0, 1.0 + 4.0e-6));
        const SketchEntityId dipAxis = add(&dip, line(0.0, -3.0, 0.0, 3.0));
        CadSketch tangent;
        const SketchEntityId exactDisk = add(&tangent, circle(1.0, 0.0, 1.0));
        const SketchEntityId tangentAxis = add(&tangent, line(0.0, -3.0, 0.0, 3.0));
        CadBodyMesh touchingMesh;
        r.check("REV_10_an_area_crossing_the_axis_is_refused_by_name_on_the_true_curve_too",
                validateCadBodyState(makeCadRevolveBodyState(
                        crossing, loopRevolve(square, CadSketchEdgeRef{axis, 0}, 360.0)))
                                == CadStatus::RevolveProfileCrossesAxis
                        && regen == CadStatus::RevolveProfileCrossesAxis
                        && validateCadBodyState(makeCadRevolveBodyState(
                                   dip, loopRevolve(disk, CadSketchEdgeRef{dipAxis, 0}, 360.0)))
                                   == CadStatus::RevolveProfileCrossesAxis
                        && regenerate(makeCadRevolveBodyState(
                                              tangent, loopRevolve(exactDisk, CadSketchEdgeRef{tangentAxis, 0},
                                                                   360.0)),
                                      &touchingMesh)
                        && touchingMesh.components == 1u);
    }
    // REV-11: the axis entity deleted -- refused by name, never retargeted to
    // the square's own nearest edge.
    {
        CadBodyState state = fullFixtureState();
        CadSketch& sketch = findCadSketchRecord(state, kBaseCadSketchId)->sketch;
        removeSketchEntity(&sketch, 2);
        CadBodyMesh mesh;
        CadStatus regen = CadStatus::Ok;
        regenerate(state, &mesh, &regen);
        r.check("REV_11_a_deleted_axis_edge_fails_by_name_and_is_never_retargeted",
                validateCadBodyState(state) == CadStatus::RevolveAxisUnresolved
                        && regen == CadStatus::RevolveAxisUnresolved
                        && state.revolve.axis.entityId == 2u);
    }
}

// ---------------------------------------------------------------------------
// REV-12..16: what is selected
// ---------------------------------------------------------------------------

void testSelections(Checks& r) {
    // REV-12: PlanarFaces. A 2 x 1 rectangle cut in half by a line running
    // side to side (two T-junctions): the right half is one atomic face.
    {
        CadSketch s;
        add(&s, rectangle(2.0, 0.5, 2.0, 1.0));
        add(&s, line(2.0, 0.0, 2.0, 1.0));
        const SketchEntityId axis = add(&s, line(0.0, -1.0, 0.0, 2.0));
        const SketchArrangement arrangement = deriveSketchArrangement(s);
        size_t right = arrangement.faces.size();
        for (size_t i = 0; i < arrangement.faces.size(); ++i) {
            std::vector<PlanarProfileComponent> shape;
            if (mergePlanarFaceSelection(arrangement, {i}, &shape) == CadStatus::Ok
                && shape.size() == 1u
                && sketchPointStrictlyInside(SketchPoint{2.5, 0.5}, shape[0].outer.polygon)) {
                right = i;
            }
        }
        RevolveFeature revolve = loopRevolve(kNoSketchEntity, CadSketchEdgeRef{axis, 0}, 360.0);
        revolve.selection = CadSelectionKind::PlanarFaces;
        if (right < arrangement.faces.size()) revolve.planarFaces = {arrangement.faces[right].ref};
        const CadBodyState state = makeCadRevolveBodyState(s, revolve);
        CadBodyMesh mesh;
        r.check("REV_12_a_planar_face_selection_revolves_its_exact_atomic_face",
                arrangement.status == ArrangementStatus::Ok && right < arrangement.faces.size()
                        && validateCadBodyState(state) == CadStatus::Ok && regenerate(state, &mesh)
                        && near(mesh.volume, revolvedVolume(1.0, 2.5, 360.0), 1e-9)
                        && cadBodyStateUsesPlanarFaces(state));
    }
    // REV-13: LoopRegions, the legacy-shaped selection.
    {
        const CadBodyState state = fullFixtureState();
        r.check("REV_13_a_loop_region_selection_revolves",
                state.revolve.selection == CadSelectionKind::LoopRegions
                        && !cadBodyStateUsesPlanarFaces(state)
                        && validateCadBodyState(state) == CadStatus::Ok);
    }
    // REV-14: a region with a hole -- a 2 x 2 square around a 0.4 m circle,
    // revolved about a line: a full turn encloses a toroidal void (two shells
    // of the one manifold), a partial turn opens it into a bored channel.
    {
        CadSketch s;
        const SketchEntityId outer = add(&s, rectangle(2.0, 0.0, 2.0, 2.0));
        const SketchEntityId hole = add(&s, circle(2.0, 0.0, 0.4));
        const SketchEntityId axis = add(&s, line(0.0, -3.0, 0.0, 3.0));
        CadBodyMesh fullHole;
        CadBodyMesh quarterHole;
        const SketchRegionExtraction regions = extractSketchRegions(s);
        double area = 0.0;
        for (const SketchRegion& region : regions.regions) {
            if (region.outerAnchorId == outer) area = region.area;
        }
        CadSolidMeasure measure;
        CadSolid solid;
        CadFeatureGeometry g;
        const CadBodyState fullState =
                makeCadRevolveBodyState(s, loopRevolve(outer, CadSketchEdgeRef{axis, 0}, 360.0,
                                                       RevolveDirection::Positive, {hole}));
        const bool built = buildCadFeatureGeometry(fullState, kCadFeatureId, &g) == CadStatus::Ok
                           && appendCadFeatureSolid(g, 0u, &solid) == CadStatus::Ok
                           && cadKernelValidateSolid(solid, &measure) == CadKernelStatus::Ok;
        r.check("REV_14_a_region_with_a_hole_revolves_into_a_closed_manifold_full_and_partial",
                regenerate(fullState, &fullHole) && built
                        && near(fullHole.volume, revolvedVolume(area, 2.0, 360.0), 1e-9)
                        && eulerCharacteristic(fullHole.mesh) == 0
                        && regenerate(makeCadRevolveBodyState(
                                              s, loopRevolve(outer, CadSketchEdgeRef{axis, 0}, 90.0,
                                                             RevolveDirection::Positive, {hole})),
                                      &quarterHole)
                        && quarterHole.components == 1u
                        && near(quarterHole.volume, revolvedVolume(area, 2.0, 90.0), 1e-9));
    }
    // REV-15: two separate squares on the same side: one New Body of two
    // shells, deterministic.
    {
        CadSketch s;
        const SketchEntityId a = add(&s, rectangle(1.5, 0.5, 1.0, 1.0));
        const SketchEntityId b = add(&s, rectangle(1.5, 3.5, 1.0, 1.0));
        const SketchEntityId axis = add(&s, line(0.0, -1.0, 0.0, 5.0));
        const CadBodyState state = makeCadRevolveBodyState(
                s, loopRevolve(a, CadSketchEdgeRef{axis, 0}, 360.0, RevolveDirection::Positive, {},
                               {ProfileRegionRef{b, {}}}));
        CadBodyMesh first;
        CadBodyMesh second;
        r.check("REV_15_separate_areas_on_one_side_make_one_deterministic_multi_shell_new_body",
                regenerate(state, &first) && regenerate(state, &second) && first.components == 2u
                        && near(first.volume, 2.0 * revolvedVolume(1.0, 1.5, 360.0), 1e-9)
                        && samePositions(first.mesh, second.mesh));
    }
    // REV-16: squares on OPPOSITE sides whose sweeps meet need a union: refused
    // by name at a full turn; clear of each other below half a turn.
    {
        CadSketch s;
        const SketchEntityId a = add(&s, rectangle(1.5, 0.5, 1.0, 1.0));
        const SketchEntityId b = add(&s, rectangle(-1.5, 0.5, 1.0, 1.0));
        const SketchEntityId axis = add(&s, line(0.0, -1.0, 0.0, 2.0));
        const auto state = [&](double degrees) {
            return makeCadRevolveBodyState(
                    s, loopRevolve(a, CadSketchEdgeRef{axis, 0}, degrees, RevolveDirection::Positive,
                                   {}, {ProfileRegionRef{b, {}}}));
        };
        CadBodyMesh quarterMesh;
        // Opposite sides that do NOT meet when mirrored are legal at any angle.
        CadSketch apart;
        const SketchEntityId c = add(&apart, rectangle(1.5, 0.5, 1.0, 1.0));
        const SketchEntityId d = add(&apart, rectangle(-1.5, 4.5, 1.0, 1.0));
        const SketchEntityId apartAxis = add(&apart, line(0.0, -1.0, 0.0, 6.0));
        CadBodyMesh apartMesh;
        r.check("REV_16_overlapping_sweeps_from_opposite_sides_are_refused_by_name_never_unioned",
                validateCadBodyState(state(360.0)) == CadStatus::RevolveComponentsOverlap
                        && validateCadBodyState(state(180.0)) == CadStatus::RevolveComponentsOverlap
                        && regenerate(state(90.0), &quarterMesh) && quarterMesh.components == 2u
                        && regenerate(makeCadRevolveBodyState(
                                              apart, loopRevolve(c, CadSketchEdgeRef{apartAxis, 0}, 360.0,
                                                                 RevolveDirection::Positive, {},
                                                                 {ProfileRegionRef{d, {}}})),
                                      &apartMesh)
                        && apartMesh.components == 2u);
    }
}

// ---------------------------------------------------------------------------
// REV-17..20: commit, Undo / Redo, edit and cancel
// ---------------------------------------------------------------------------

void testHistory(Checks& r) {
    // REV-17: through the session's own New Body commit in a live project.
    ConstructionScene scene;  // a live project: the default Box
    ConstructionHistory history(scene);
    Driver d;
    bool ready = d.finishOn(offsetSketch()) && d.sketch.selectionChosen();
    ready = ready && d.sketch.beginRevolve() == CadStatus::Ok
            && d.sketch.setRevolveAxis(CadSketchEdgeRef{2, 0}) == CadStatus::Ok
            && d.sketch.setRevolveAngle(37.5) == CadStatus::Ok;
    const CadBodyState staged = ready ? d.sketch.candidateState() : CadBodyState{};
    ObjectId id = kNoObject;
    const CadStatus committed = ready ? d.sketch.commit(scene, history, &id) : CadStatus::NotSketching;
    const SceneObject* body = scene.findBody(id);
    const CadBodyState created = body != nullptr && body->cadOrNull() != nullptr
                                         ? body->cadOrNull()->state()
                                         : CadBodyState{};
    const size_t bodies = scene.bodyCount();
    const bool undone = history.undo() && scene.findBody(id) == nullptr
                        && scene.bodyCount() == bodies - 1u;
    const bool redone = history.redo() && scene.findBody(id) != nullptr
                        && sameCadBodyState(scene.findBody(id)->cadOrNull()->state(), created);
    r.check("REV_17_a_revolve_commits_as_one_new_body_one_undo_and_redo_restores_it_exactly",
            committed == CadStatus::Ok && body != nullptr && created.baseKind == CadFeatureKind::Revolve
                    && created.revolve.angleDegrees == 37.5
                    && sameCadSketchEdgeRef(created.revolve.axis, CadSketchEdgeRef{2, 0})
                    && sameCadBodyState(created, staged) && history.undoDepth() == 1u && undone
                    && redone && !d.sketch.active());

    // REV-18: reopen the Revolve (staged in Ready, kind kept) and set 180.
    SceneObject* object = scene.findBody(id);
    CadBody* cad = object != nullptr ? object->cadOrNull() : nullptr;
    const SketchFrame frame{Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}};
    SketchSession edit;
    const CadStatus reopened = cad != nullptr
                                       ? edit.beginEditFeature(id, cad->state(), kCadFeatureId, frame, true)
                                       : CadStatus::NotCadBody;
    const bool staysRevolve = reopened == CadStatus::Ok
                              && edit.featureKind() == CadFeatureKind::Revolve
                              && edit.revolveParameters().angleDegrees == 37.5
                              && !edit.revolveAxisPicking() && edit.endRevolve() == CadStatus::InvalidFeatureKind;
    const size_t depthBefore = history.undoDepth();
    const bool angled = edit.setRevolveAngle(180.0) == CadStatus::Ok
                        && edit.commitEdit(scene, history) == CadStatus::Ok;
    const CadBodyState afterAngle = scene.findBody(id)->cadOrNull()->state();
    r.check("REV_18_reopening_a_revolve_shows_a_revolve_and_an_angle_edit_is_one_exact_step",
            staysRevolve && angled && afterAngle.revolve.angleDegrees == 180.0
                    && afterAngle.baseKind == CadFeatureKind::Revolve
                    && history.undoDepth() == depthBefore + 1u);

    // REV-19: change the axis to the square's own left edge (u = 1): the
    // square then touches its axis and the solid is a thick-walled cylinder.
    SketchSession axisEdit;
    const bool axisChanged =
            axisEdit.beginEditFeature(id, afterAngle, kCadFeatureId, frame, true) == CadStatus::Ok
            && axisEdit.beginRevolveAxisPick() == CadStatus::Ok && axisEdit.revolveAxisPicking()
            && axisEdit.setRevolveAxis(CadSketchEdgeRef{1, 3}) == CadStatus::Ok
            && axisEdit.commitEdit(scene, history) == CadStatus::Ok;
    const CadBodyState afterAxis = scene.findBody(id)->cadOrNull()->state();
    CadBodyMesh axisMesh;
    r.check("REV_19_an_axis_edit_to_another_straight_edge_is_one_exact_step",
            axisChanged && sameCadSketchEdgeRef(afterAxis.revolve.axis, CadSketchEdgeRef{1, 3})
                    && afterAxis.revolve.angleDegrees == 180.0 && regenerate(afterAxis, &axisMesh)
                    && near(axisMesh.volume, revolvedVolume(1.0, 0.5, 180.0), 1e-9)
                    && history.undo()
                    && sameCadBodyState(scene.findBody(id)->cadOrNull()->state(), afterAngle)
                    && history.redo());

    // REV-20: a cancelled edit costs nothing -- no step, no id mark moved.
    const CadBodyState beforeCancel = scene.findBody(id)->cadOrNull()->state();
    const size_t depth = history.undoDepth();
    SketchSession cancelled;
    const bool changedThenCancelled =
            cancelled.beginEditFeature(id, beforeCancel, kCadFeatureId, frame, true) == CadStatus::Ok
            && cancelled.setRevolveAngle(45.0) == CadStatus::Ok
            && cancelled.flipRevolveDirection() == CadStatus::Ok;
    cancelled.cancel();
    const CadBodyState afterCancel = scene.findBody(id)->cadOrNull()->state();
    r.check("REV_20_a_cancelled_revolve_edit_changes_nothing_and_burns_no_id",
            changedThenCancelled && sameCadBodyState(afterCancel, beforeCancel)
                    && history.undoDepth() == depth
                    && afterCancel.nextSketchId == beforeCancel.nextSketchId
                    && afterCancel.nextFeatureId == beforeCancel.nextFeatureId
                    && !cancelled.active());
}

// ---------------------------------------------------------------------------
// REV-21..25: the mesh itself
// ---------------------------------------------------------------------------

void testMesh(Checks& r, std::string* perf) {
    CadBodyMesh torus;
    regenerate(fullFixtureState(), &torus);
    r.check("REV_21_a_full_turn_closes_its_seam_with_no_duplicated_vertex_and_torus_topology",
            !torus.mesh.vertices.empty() && coincidentVertices(torus.mesh) == 0u
                    && eulerCharacteristic(torus.mesh) == 0 && torus.components == 1u);

    // REV-22: both caps of both senses face OUT of the sweep, and the solid is
    // positive.
    bool caps = true;
    for (RevolveDirection direction : {RevolveDirection::Positive, RevolveDirection::Negative}) {
        const CadBodyState state =
                makeCadRevolveBodyState(offsetSketch(), loopRevolve(1, CadSketchEdgeRef{2, 0}, 90.0, direction));
        CadBodyMesh mesh;
        caps = caps && regenerate(state, &mesh) && mesh.volume > 0.0;
        RevolveAxis2D axis;
        resolveRevolveAxis(cadBaseSketch(state), state.revolve.axis, &axis);
        CadFrame64 placement;
        placement.u = DVec3{1, 0, 0};
        placement.v = DVec3{0, 1, 0};
        placement.n = DVec3{0, 0, 1};
        const RevolveFrame f = revolveFrame(placement, axis, direction);
        // The square is on the axis's -u side? No: the axis runs +v at u = 0,
        // its left normal is -u, and the square is at +u -- side -1. A point
        // of the square at theta = 0 moves along -(tangent) for side -1.
        const Vec3 tangent0 = vec3Scale(vec3FromDVec3(f.tangent), -1.0f);
        const float a = static_cast<float>(90.0 * kPi / 180.0);
        const Vec3 radial0 = vec3Scale(vec3FromDVec3(f.radial), -1.0f);
        const Vec3 tangentEnd = vec3Add(vec3Scale(tangent0, std::cos(a)), vec3Scale(radial0, -std::sin(a)));
        const Vec3 start = faceNormal(mesh, faceOfKind(mesh, CadFaceKind::CapPlane));
        const Vec3 end = faceNormal(mesh, faceOfKind(mesh, CadFaceKind::CapFar));
        caps = caps && vec3Dot(start, tangent0) < 0.0f && vec3Dot(end, tangentEnd) > 0.0f;
    }
    r.check("REV_22_partial_caps_wind_outward_for_both_senses_and_the_solid_is_positive", caps);

    // REV-23: the PRODUCTION kernel judges it -- the published measure is the
    // kernel's own, and the same solid wound inward is refused by it.
    {
        CadFeatureGeometry g;
        CadSolid solid;
        CadSolidMeasure measure;
        const bool built = buildCadFeatureGeometry(fullFixtureState(), kCadFeatureId, &g) == CadStatus::Ok
                           && appendCadFeatureSolid(g, 0u, &solid) == CadStatus::Ok
                           && cadKernelValidateSolid(solid, &measure) == CadKernelStatus::Ok;
        CadSolid inverted = solid;
        for (size_t t = 0; t + 2 < inverted.indices.size(); t += 3) {
            std::swap(inverted.indices[t + 1], inverted.indices[t + 2]);
        }
        r.check("REV_23_the_revolve_is_held_to_the_production_kernels_validity_answer",
                built && near(torus.volume, measure.volume, 1e-12)
                        && torus.components == measure.components
                        && cadKernelValidateSolid(inverted, nullptr) == CadKernelStatus::InvertedInput);
    }

    // REV-24: deterministic: the same state gives the same mesh, bit for bit,
    // and the same index digest.
    CadBodyMesh again;
    regenerate(fullFixtureState(), &again);
    char digest[64];
    std::snprintf(digest, sizeof(digest), "%016llx",
                  static_cast<unsigned long long>(indexDigest(torus.mesh)));
    r.check("REV_24_the_derived_topology_and_mesh_are_deterministic",
            samePositions(torus.mesh, again.mesh) && indexDigest(torus.mesh) == indexDigest(again.mesh)
                    && torus.triangleFace == again.triangleFace);

    // REV-25: bounded cost: a 32-gon swept a full turn (2048 triangles) and
    // the canonical square, twenty regenerations each.
    CadSketch disk;
    const SketchEntityId c = add(&disk, circle(2.0, 0.0, 0.5));
    const SketchEntityId axis = add(&disk, line(0.0, -3.0, 0.0, 3.0));
    const CadBodyState torusDisk =
            makeCadRevolveBodyState(disk, loopRevolve(c, CadSketchEdgeRef{axis, 0}, 360.0));
    double worstDisk = 0.0;
    double worstSquare = 0.0;
    size_t triangles = 0;
    for (int run = 0; run < 20; ++run) {
        CadBodyMesh mesh;
        auto t0 = std::chrono::steady_clock::now();
        regenerate(torusDisk, &mesh);
        worstDisk = std::max(worstDisk, micros(t0));
        triangles = mesh.mesh.indices.size() / 3u;
        t0 = std::chrono::steady_clock::now();
        regenerate(fullFixtureState(), &mesh);
        worstSquare = std::max(worstSquare, micros(t0));
    }
    char line[256];
    std::snprintf(line, sizeof(line),
                  "revolve_disk_max_us=%.0f revolve_square_max_us=%.0f revolve_disk_triangles=%zu "
                  "revolve_index_digest=%s",
                  worstDisk, worstSquare, triangles, digest);
    *perf += line;
    r.check("REV_25_revolve_regeneration_is_bounded",
            triangles == 2048u && worstDisk < 250000.0 && worstSquare < 250000.0);
}

// ---------------------------------------------------------------------------
// The session: axis pick, refusal, handle drag, Flip, first project
// ---------------------------------------------------------------------------

void testSession(Checks& r) {
    // The first project (New Project -> CAD): finish, revolve, a TAP on the
    // line picks the axis, the default 360 commits through the bootstrap.
    {
        ConstructionScene empty{NoProjectTag{}};
        ConstructionHistory history(empty);
        SculptSession sculpt;
        Driver d;
        const bool ready = d.finishOn(offsetSketch()) && d.sketch.selectionChosen();
        const bool entered = ready && d.sketch.revolveAvailable()
                             && d.sketch.beginRevolve() == CadStatus::Ok
                             && d.sketch.featureKind() == CadFeatureKind::Revolve
                             && d.sketch.revolveAxisPicking()
                             && d.sketch.evaluateCandidate().status == CadStatus::RevolveNeedsAxis;
        // A tap ON the line (u = 0, v = 0.5) through the gesture path.
        const bool picked = entered && d.tapAt(SketchPoint{0.0, 0.5})
                            && d.sketch.lastTapOutcome() == SketchTapOutcome::Resolved
                            && !d.sketch.revolveAxisPicking()
                            && sameCadSketchEdgeRef(d.sketch.revolveParameters().axis,
                                                    CadSketchEdgeRef{2, 0});
        const bool previewed = picked && d.sketch.evaluateCandidate().status == CadStatus::Ok
                               && d.sketch.evaluateCandidate().mesh != nullptr
                               && near(d.sketch.evaluateCandidate().mesh->volume,
                                       revolvedVolume(1.0, 1.5, 360.0), 1e-9);
        FirstProjectReport report;
        const CadStatus committed = previewed
                                            ? commitFirstCadProject(d.sketch, empty, sculpt, history, &report)
                                            : CadStatus::NotSketching;
        const SceneObject* body = empty.findBody(report.bodyId);
        r.check("REV_S01_new_project_cad_revolve_picks_the_axis_by_a_tap_and_creates_the_first_project",
                committed == CadStatus::Ok && empty.hasProject() && body != nullptr && body->isCad()
                        && body->cadOrNull()->state().baseKind == CadFeatureKind::Revolve
                        && body->cadOrNull()->state().revolve.angleDegrees == 360.0
                        && history.undoDepth() == 0u);
    }
    // A tap on a CIRCLE while picking the axis is refused by name, and the
    // axis stays unchosen; a tap on the line then picks it.
    {
        CadSketch s;
        add(&s, rectangle(1.5, 0.5, 1.0, 1.0));
        add(&s, line(0.0, -1.0, 0.0, 2.0));
        add(&s, circle(-2.0, 0.5, 0.5));
        Driver d;
        bool ok = d.finishOn(s) && d.sketch.selectProfile(1) == CadStatus::Ok
                  && d.sketch.beginRevolve() == CadStatus::Ok;
        ok = ok && d.tapAt(SketchPoint{-1.5, 0.5});
        const bool refused = ok && d.sketch.lastStatus() == CadStatus::RevolveAxisNotStraight
                             && d.sketch.revolveAxisPicking()
                             && d.sketch.revolveParameters().axis.entityId == kNoSketchEntity;
        const bool picked = refused && d.tapAt(SketchPoint{0.0, 1.5}) && !d.sketch.revolveAxisPicking();
        r.check("REV_S02_a_curved_axis_tap_is_refused_by_name_and_a_straight_one_picks",
                refused && picked && d.sketch.revolveParameters().axis.entityId == 2u);
    }
    // The ring handle: grabbed on Down (consumed -- the camera sees nothing),
    // dragged round the ring, the angle follows in whole degrees; a second
    // finger cancels back to the angle the finger found.
    {
        Driver d;
        bool ok = d.finishOn(offsetSketch()) && d.sketch.beginRevolve() == CadStatus::Ok
                  && d.sketch.setRevolveAxis(CadSketchEdgeRef{2, 0}) == CadStatus::Ok
                  && d.takeRevolveView();
        SketchSession::RevolveRing ring;
        ok = ok && d.sketch.revolveRing(&ring);
        const auto screen = [&](double degrees, float* x, float* y) {
            const double a = degrees * kPi / 180.0;
            const Vec3 p = vec3Add(ring.centre,
                                   vec3Add(vec3Scale(ring.radial, ring.radius * static_cast<float>(std::cos(a))),
                                           vec3Scale(ring.tangent, ring.radius * static_cast<float>(std::sin(a)))));
            return projectWorldToScreen(d.camera.snapshot(), p, Driver::kW, Driver::kH, x, y);
        };
        float hx = 0.0f;
        float hy = 0.0f;
        ok = ok && projectWorldToScreen(d.camera.snapshot(), ring.handle, Driver::kW, Driver::kH, &hx, &hy);
        const bool grabbed = ok && d.at(TouchAction::Down, hx, hy)
                             && !sketchEventReachesCamera(SketchSessionState::Ready, true, 1,
                                                          d.sketch.readyTapArmed());
        bool followed = grabbed;
        for (double degrees = 350.0; followed && degrees >= 269.0; degrees -= 10.0) {
            float x = 0.0f;
            float y = 0.0f;
            followed = screen(degrees, &x, &y) && d.at(TouchAction::Move, x, y);
        }
        float ex = 0.0f;
        float ey = 0.0f;
        followed = followed && screen(270.0, &ex, &ey) && d.at(TouchAction::Move, ex, ey)
                   && d.sketch.revolveDragging();
        const double dragged = d.sketch.revolveParameters().angleDegrees;
        const bool released = d.at(TouchAction::Up, ex, ey) && !d.sketch.revolveDragging();
        // A second drag cancelled by a second finger: the angle goes back.
        d.at(TouchAction::Down, 0.0f, 0.0f);  // re-anchor the ring read below
        d.at(TouchAction::Up, 0.0f, 0.0f);
        SketchSession::RevolveRing ring2;
        d.sketch.revolveRing(&ring2);
        float h2x = 0.0f;
        float h2y = 0.0f;
        projectWorldToScreen(d.camera.snapshot(), ring2.handle, Driver::kW, Driver::kH, &h2x, &h2y);
        d.at(TouchAction::Down, h2x, h2y);
        float mx = 0.0f;
        float my = 0.0f;
        screen(200.0, &mx, &my);
        d.at(TouchAction::Move, mx, my);
        TouchPointer two[2] = {TouchPointer{7, mx, my}, TouchPointer{8, 10.0f, 10.0f}};
        const bool secondFinger = !d.sketch.onTouch(TouchAction::PointerDown, 8, two, 2,
                                                    d.camera.snapshot(), Driver::kW, Driver::kH);
        r.check("REV_S03_the_ring_handle_drags_the_angle_without_the_camera_and_a_second_finger_cancels",
                grabbed && followed && released && dragged == 270.0
                        && d.sketch.evaluateCandidate().status == CadStatus::Ok && secondFinger
                        && d.sketch.revolveParameters().angleDegrees == 270.0
                        && !d.sketch.revolveDragging());
        // Flip: the sense reverses and the exact angle stays.
        const double before = d.sketch.revolveParameters().angleDegrees;
        const RevolveDirection sense = d.sketch.revolveParameters().direction;
        r.check("REV_S04_flip_reverses_the_sense_and_keeps_the_exact_angle",
                d.sketch.flipRevolveDirection() == CadStatus::Ok
                        && d.sketch.revolveParameters().angleDegrees == before
                        && d.sketch.revolveParameters().direction != sense
                        && d.sketch.evaluateCandidate().status == CadStatus::Ok);
        // Typed angles: refused, never clamped.
        const bool typed = d.sketch.setRevolveAngle(0.0) == CadStatus::RevolveAngleInvalid
                           && d.sketch.setRevolveAngle(400.0) == CadStatus::RevolveAngleInvalid
                           && d.sketch.setRevolveAngle(-90.0) == CadStatus::RevolveAngleInvalid
                           && d.sketch.revolveParameters().angleDegrees == before
                           && d.sketch.setRevolveAngle(37.5) == CadStatus::Ok
                           && d.sketch.revolveParameters().angleDegrees == 37.5;
        // Back to the extrusion keeps the selection; Revolve again keeps the axis.
        const bool roundTrip = d.sketch.endRevolve() == CadStatus::Ok
                               && d.sketch.featureKind() == CadFeatureKind::Extrude
                               && d.sketch.selectionChosen()
                               && d.sketch.evaluateCandidate().status == CadStatus::Ok
                               && d.sketch.beginRevolve() == CadStatus::Ok
                               && !d.sketch.revolveAxisPicking()
                               && d.sketch.revolveParameters().angleDegrees == 37.5;
        r.check("REV_S05_typed_angles_are_exact_or_refused_and_extrude_instead_keeps_the_selection",
                typed && roundTrip);
    }
    // A tap beside the ring -- on the square, off the handle -- is still the
    // area's: it toggles the square out, and the candidate says so.
    {
        Driver d;
        bool ok = d.finishOn(offsetSketch()) && d.sketch.beginRevolve() == CadStatus::Ok
                  && d.sketch.setRevolveAxis(CadSketchEdgeRef{2, 0}) == CadStatus::Ok
                  && d.takeRevolveView();
        const size_t before = d.sketch.selectedAreaCount();
        ok = ok && d.tapAt(SketchPoint{1.5, 0.5});
        r.check("REV_S06_a_profile_tap_beside_the_ring_still_toggles_the_area",
                ok && before == 1u && d.sketch.selectedAreaCount() == 0u
                        && d.sketch.lastTapOutcome() == SketchTapOutcome::Resolved
                        && !d.sketch.revolveDragging());
    }
}

// ---------------------------------------------------------------------------
// REV-FMT: CADB v7
// ---------------------------------------------------------------------------

void testFormat(Checks& r) {
    // FMT-02: a document every one of whose bodies is an Extrude writes the
    // version it always wrote -- v1 for the plain square, v6 for a shared
    // sketch -- and never v7.
    {
        CadSketch s;
        const SketchEntityId square = add(&s, rectangle(0.0, 0.0, 2.0, 2.0));
        ExtrudeFeature extrude;
        extrude.profileEntityId = square;
        const CadBodyState plain = makeCadBodyState(s, extrude);
        CadBodyState shared = plain;
        appendCadLaterFeature(&shared, CadFeatureOperation::Add, kBaseCadSketchId, extrude);
        const std::vector<uint8_t> plainBytes = encodeProjectV1(documentFor(plain));
        r.check("REV_FMT_02_an_extrude_only_document_still_writes_its_v1_to_v6_bytes",
                cadVersionOf(plainBytes) == kCadSectionVersion
                        && !cadBodyStateUsesRevolve(plain) && cadBodyStateLegacyRepresentable(plain)
                        && cadVersionOf(encodeProjectV1Unchecked(documentFor(shared)))
                                   == kCadSectionVersionV6);
    }
    // FMT-03 / FMT-04: full and partial round-trip exactly through v7.
    const std::vector<uint8_t> full = fullFixtureBytes();
    const std::vector<uint8_t> partial = partialFixtureBytes();
    for (int which = 0; which < 2; ++which) {
        const std::vector<uint8_t>& bytes = which == 0 ? full : partial;
        const CadBodyState state = which == 0 ? fullFixtureState() : partialFixtureState();
        ProjectDocument back;
        CadBodyMesh mesh;
        const bool ok = !bytes.empty() && cadVersionOf(bytes) == kCadSectionVersionV7
                        && decodeStatus(bytes, &back) == ProjectCodecStatus::Ok
                        && back.cad.bodies.size() == 1u
                        && sameCadBodyState(back.cad.bodies[0].state, state)
                        && encodeProjectV1(back) == bytes
                        && regenerate(back.cad.bodies[0].state, &mesh);
        r.check(which == 0 ? "REV_FMT_03_a_full_revolve_round_trips_exactly_through_cadb_v7"
                           : "REV_FMT_04_a_partial_reversed_revolve_round_trips_exactly_through_cadb_v7",
                ok);
    }
    // FMT-05 / FMT-06: an unresolved axis and an unknown kind are refused by
    // name, and nothing is written.
    {
        ProjectDocument out;
        const ProjectCodecStatus badAxis = decodeStatus(badAxisFixtureBytes(), &out);
        ProjectDocument out2;
        const std::vector<uint8_t> badKind = badKindFixtureBytes();
        const ProjectCodecStatus unknownKind = decodeStatus(badKind, &out2);
        r.check("REV_FMT_05_a_v7_revolve_with_an_unresolved_axis_is_refused",
                badAxis == ProjectCodecStatus::InvalidSemanticValue && out.cad.bodies.empty());
        r.check("REV_FMT_06_an_unknown_feature_kind_is_refused_by_name",
                !badKind.empty() && unknownKind == ProjectCodecStatus::InvalidSemanticValue
                        && out2.cad.bodies.empty());
    }
    // FMT-07: the production encoder writes the bytes the independent builder
    // constructs from DATA_PACKAGE_SPEC.md §7h.
    r.check("REV_FMT_07_every_v7_fixture_matches_the_independent_corpus_digest",
            sha(full) == kFullFixtureSha && sha(partial) == kPartialFixtureSha
                    && sha(badAxisFixtureBytes()) == kBadAxisFixtureSha
                    && sha(badKindFixtureBytes()) == kBadKindFixtureSha);
    // FMT-08: one payload per kind -- a Revolve carrying extrude values, or an
    // Extrude carrying revolve values, is refused; the codec writes neither.
    {
        CadBodyState revolveWithDepth = fullFixtureState();
        revolveWithDepth.extrude.depth = 2.0;
        CadBodyState extrudeWithAngle = makeCadBodyState(offsetSketch(), ExtrudeFeature{});
        extrudeWithAngle.extrude.profileEntityId = 1;
        extrudeWithAngle.revolve.angleDegrees = 90.0;
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> refused = encodeProjectV1(documentFor(revolveWithDepth), &why);
        r.check("REV_FMT_08_a_payload_that_contradicts_its_kind_is_refused_never_read_as_either",
                validateCadBodyState(revolveWithDepth) == CadStatus::RevolvePayloadMismatch
                        && validateCadBodyState(extrudeWithAngle) == CadStatus::RevolvePayloadMismatch
                        && refused.empty() && why == ProjectCodecStatus::InvalidSemanticValue);
    }
    // FMT-09: a document with an Extrude body AND a Revolve body writes v7 for
    // both, and both read back exactly.
    {
        ProjectDocument document = documentFor(fullFixtureState());
        document.scene.nextObjectId = 3;
        ProjectBodyPlacement second;
        second.objectId = 2;
        document.scene.bodies.push_back(second);
        ProjectCadBody extrudeBody;
        extrudeBody.objectId = 2;
        CadSketch s;
        const SketchEntityId square = add(&s, rectangle(5.0, 0.0, 1.0, 1.0));
        ExtrudeFeature extrude;
        extrude.profileEntityId = square;
        extrudeBody.state = makeCadBodyState(s, extrude);
        document.cad.bodies.push_back(extrudeBody);
        const std::vector<uint8_t> bytes = encodeProjectV1(document);
        ProjectDocument back;
        r.check("REV_FMT_09_a_mixed_extrude_and_revolve_document_writes_v7_and_reads_back_exactly",
                cadVersionOf(bytes) == kCadSectionVersionV7
                        && decodeStatus(bytes, &back) == ProjectCodecStatus::Ok
                        && back.cad.bodies.size() == 2u
                        && sameCadBodyState(back.cad.bodies[0].state, fullFixtureState())
                        && sameCadBodyState(back.cad.bodies[1].state, extrudeBody.state)
                        && encodeProjectV1(back) == bytes);
    }
    // The fingerprint follows the Revolve's truth.
    {
        ConstructionScene a{NoProjectTag{}};
        ConstructionScene b{NoProjectTag{}};
        ConstructionHistory ha(a);
        ConstructionHistory hb(b);
        SculptSession sculpt;
        ProjectDocument da = documentFor(fullFixtureState());
        ProjectDocument db = documentFor(partialFixtureState());
        const bool loaded = loadProjectDocument(da, a, sculpt, ha) == ProjectCodecStatus::Ok
                            && loadProjectDocument(db, b, sculpt, hb) == ProjectCodecStatus::Ok;
        r.check("REV_FMT_10_the_project_fingerprint_moves_with_the_revolve_angle_and_direction",
                loaded && projectSemanticFingerprint(a, ProjectKind::Construction)
                                  != projectSemanticFingerprint(b, ProjectKind::Construction));
    }
}

}  // namespace

std::string cadRevolveFixtureDigests() {
    return std::string("cad_revolve_full_v7=") + sha(fullFixtureBytes())
           + " cad_revolve_partial_v7=" + sha(partialFixtureBytes())
           + " cad_revolve_bad_axis_v7=" + sha(badAxisFixtureBytes())
           + " cad_bad_feature_kind_v7=" + sha(badKindFixtureBytes());
}

void runCadRevolveSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    if (out == nullptr) {
        return;
    }
    Checks r{out};
    std::string perf;
    testSolids(r);
    testAxis(r);
    testSelections(r);
    testHistory(r);
    testMesh(r, &perf);
    testSession(r);
    testFormat(r);
    if (performance != nullptr) {
        *performance = perf;
    }
}

}  // namespace forgeshape
