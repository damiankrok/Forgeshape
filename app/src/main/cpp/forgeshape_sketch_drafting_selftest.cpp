#include "forgeshape_sketch_drafting_selftest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_camera.h"
#include "forgeshape_history.h"
#include "forgeshape_input.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_dimension.h"
#include "forgeshape_sketch_drafting.h"
#include "forgeshape_sketch_overlay.h"
#include "forgeshape_sketch_region.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_sketch_snap.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool ok) { out->push_back(ArrangementSelfTestCheck{name, ok}); }
};

// --- sketch builders ---------------------------------------------------------

SketchEntityId add(CadSketch* sketch, SketchEntity::Payload payload,
                   SketchEntityRole role = SketchEntityRole::Regular) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, std::move(payload), &id, role);
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
    return SketchLine{SketchPoint{u0, v0}, SketchPoint{u1, v1}};
}

SketchCircle circle(double cu, double cv, double r) {
    SketchCircle c;
    c.center = SketchPoint{cu, cv};
    c.radius = r;
    return c;
}

SketchArc arc(double u0, double v0, double u1, double v1, double u2, double v2) {
    return SketchArc{SketchPoint{u0, v0}, SketchPoint{u1, v1}, SketchPoint{u2, v2}};
}

SketchPolyline polyline(std::vector<SketchPoint> points, bool closed = false) {
    SketchPolyline p;
    p.vertices = std::move(points);
    p.closed = closed;
    return p;
}

SketchSpline spline(std::vector<SketchPoint> points) {
    SketchSpline s;
    s.points = std::move(points);
    return s;
}

bool samePoint(const SketchPoint& a, const SketchPoint& b) { return a.u == b.u && a.v == b.v; }

bool near(double a, double b, double tolerance = 1e-9) { return std::fabs(a - b) <= tolerance; }

bool nearPoint(const SketchPoint& a, const SketchPoint& b, double tolerance = 1e-9) {
    return near(a.u, b.u, tolerance) && near(a.v, b.v, tolerance);
}

const SketchEntity* entityOf(const CadSketch& sketch, SketchEntityId id) {
    return findSketchEntity(sketch, id);
}

double valueOf(const CadSketch& sketch, SketchDimensionId id) {
    const SketchDimension* d = findSketchDimension(sketch, id);
    double value = NAN;
    if (d != nullptr) sketchDimensionValue(sketch, *d, &value);
    return value;
}

SketchDimensionId addDim(CadSketch* sketch, SketchDimensionKind kind, SketchDimensionMode mode,
                         CadSketchEdgeRef first, CadSketchEdgeRef second = CadSketchEdgeRef{},
                         CadStatus* why = nullptr) {
    SketchDimensionId id = kNoSketchDimension;
    const CadStatus status = addSketchDimension(sketch, kind, mode, first, second, &id);
    if (why != nullptr) *why = status;
    return id;
}

// The distance from `p` to a curve's derived polyline.
double distanceToCurve(const SketchEntity& entity, const SketchPoint& p) {
    return sketchEntityDistance(entity, p);
}

// --- documents ---------------------------------------------------------------

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

// One payload byte replaced and the section CRC repaired, so only the
// decoder's own rule can refuse the result.
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

ExtrudeFeature profileExtrude(SketchEntityId profile) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = profile;
    return extrude;
}

// --- the five v8 fixtures (DATA_PACKAGE_SPEC.md §7i; the independent builder
// constructs the same bytes from the text) -----------------------------------

CadBodyState constructionFixtureState() {
    CadSketch s;
    add(&s, rectangle(0.0, 0.0, 2.0, 1.0));
    add(&s, line(-2.0, 0.0, 2.0, 0.0), SketchEntityRole::Construction);
    add(&s, circle(0.0, 0.0, 1.5), SketchEntityRole::Construction);
    return makeCadBodyState(s, profileExtrude(1));
}

CadSketch drivingFixtureSketch() {
    CadSketch s;
    add(&s, rectangle(0.0, 0.0, 2.0, 1.0));
    add(&s, circle(3.0, 0.0, 0.5));
    add(&s, line(-1.0, -2.0, 1.0, -2.0));
    addDim(&s, SketchDimensionKind::RectangleWidth, SketchDimensionMode::Driving, {1, 0});
    addDim(&s, SketchDimensionKind::RectangleHeight, SketchDimensionMode::Driving, {1, 0});
    addDim(&s, SketchDimensionKind::CircleDiameter, SketchDimensionMode::Driving, {2, 0});
    addDim(&s, SketchDimensionKind::LineLength, SketchDimensionMode::Driving, {3, 0});
    addDim(&s, SketchDimensionKind::LineAngle, SketchDimensionMode::Driving, {3, 0});
    return s;
}

CadBodyState drivingFixtureState() { return makeCadBodyState(drivingFixtureSketch(), profileExtrude(1)); }

CadBodyState referenceFixtureState() {
    CadSketch s;
    add(&s, rectangle(0.0, 0.0, 2.0, 1.0));
    add(&s, arc(3.0, 0.0, 4.0, 1.0, 5.0, 0.0));
    add(&s, polyline({{0.0, -2.0}, {2.0, -2.0}, {2.0, -3.0}}));
    add(&s, line(0.0, 2.0, 2.0, 3.0));
    const SketchDimensionMode ref = SketchDimensionMode::Reference;
    addDim(&s, SketchDimensionKind::ArcRadius, ref, {2, 0});
    addDim(&s, SketchDimensionKind::ArcSweep, ref, {2, 0});
    addDim(&s, SketchDimensionKind::EdgeLength, ref, {3, 1});
    addDim(&s, SketchDimensionKind::EdgeLength, ref, {1, 2});
    addDim(&s, SketchDimensionKind::LineHorizontal, ref, {4, 0});
    addDim(&s, SketchDimensionKind::LineVertical, ref, {4, 0});
    addDim(&s, SketchDimensionKind::EdgeAngle, ref, {3, 0}, {3, 1});
    // Dimension 8 was made and deleted: its id is burned, the mark is 9.
    addDim(&s, SketchDimensionKind::LineLength, ref, {4, 0});
    removeSketchDimension(&s, 8);
    return makeCadBodyState(s, profileExtrude(1));
}

std::vector<uint8_t> constructionFixtureBytes() {
    return encodeProjectV1(documentFor(constructionFixtureState()));
}
std::vector<uint8_t> drivingFixtureBytes() { return encodeProjectV1(documentFor(drivingFixtureState())); }
std::vector<uint8_t> referenceFixtureBytes() {
    return encodeProjectV1(documentFor(referenceFixtureState()));
}
std::vector<uint8_t> badRefFixtureBytes() {
    CadBodyState state = drivingFixtureState();
    cadBaseSketch(state).dimensions[2].first.entityId = 9;
    return encodeProjectV1Unchecked(documentFor(state));
}
std::vector<uint8_t> conflictFixtureBytes() {
    CadBodyState state = drivingFixtureState();
    SketchDimension radius;
    radius.id = 6;
    radius.kind = SketchDimensionKind::CircleRadius;
    radius.mode = SketchDimensionMode::Driving;
    radius.first = CadSketchEdgeRef{2, 0};
    cadBaseSketch(state).dimensions.push_back(radius);
    cadBaseSketch(state).nextDimensionId = 7;
    return encodeProjectV1Unchecked(documentFor(state));
}

// The digests the independent PowerShell builder produces for the same five
// documents (scripts/build-forge-corpus.ps1, `cad_*_v8.forge`).
constexpr const char* kConstructionFixtureSha =
        "553b7e094cddaf8b65e26c4b642c67c4920646de52ab7bedaade2413b547b776";
constexpr const char* kDrivingFixtureSha =
        "d0a560450063c91f0b4473b5932b05d6e66b8245537da44e34d3434a2bcdfddf";
constexpr const char* kReferenceFixtureSha =
        "cd8ad8620b3f9386d64789651014df57ea24db854a33f4651a107dd411aaf258";
constexpr const char* kBadRefFixtureSha =
        "a0e1388b6b63157ff66e735fd9e1cf6e888990f9d660bb1095686183f3c96f74";
constexpr const char* kConflictFixtureSha =
        "9ca1afac3749b5f5bc6131b11df973b2687ecaa635df22a487291f963cd9f8fd";
// A v7 fixture that predates this stage (`cad_revolve_full_v7`): a build that
// knows v8 must still write it byte for byte.
constexpr const char* kRevolveFullV7Sha =
        "1e6d021408bf3959659cef1fb119d3e5a4ced0442289b3ab5bb583fdcde5d450";

// --- a touch driver over a session -------------------------------------------

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
    bool screen(const SketchPoint& p, float* x, float* y) {
        return sketch.sketchToScreen(camera.snapshot(), p, kW, kH, x, y);
    }
    // One entity through the drawing gesture, then typed exactly.
    SketchEntityId place(SketchTool tool, SketchEntity::Payload exact) {
        sketch.setTool(tool);
        float x0, y0, x1, y1;
        if (!screen(SketchPoint{-7.13, -7.27}, &x0, &y0) || !screen(SketchPoint{-6.11, -6.17}, &x1, &y1)) {
            return kNoSketchEntity;
        }
        const size_t before = sketch.sketch().entities.size();
        bool ok = at(TouchAction::Down, x0, y0);
        for (int step = 1; step <= 4; ++step) {
            const float t = step / 4.0f;
            ok &= at(TouchAction::Move, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
        }
        ok &= at(TouchAction::Up, x1, y1);
        if (!ok || sketch.sketch().entities.size() != before + 1u) return kNoSketchEntity;
        const SketchEntityId id = sketch.sketch().entities.back().id();
        return sketch.replaceEntity(id, std::move(exact)) == CadStatus::Ok ? id : kNoSketchEntity;
    }
    bool tap(const SketchPoint& p) {
        float x = 0.0f;
        float y = 0.0f;
        if (!screen(p, &x, &y)) return false;
        at(TouchAction::Down, x, y);
        at(TouchAction::Up, x, y);
        return true;
    }
    double worldPerUnit() {
        float perPixel = 0.0f;
        worldMetersPerPixel(camera.snapshot(), sketch.frame().origin, kH, &perPixel);
        return static_cast<double>(perPixel) * gizmoPixelsPerReferenceUnit();
    }
};

double microsSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
}

// ---------------------------------------------------------------------------
// DR-CON: construction geometry
// ---------------------------------------------------------------------------

void testConstruction(Checks& r) {
    CadSketch regular;
    const SketchEntityId rect = add(&regular, rectangle(0.0, 0.0, 2.0, 1.0));
    r.check("DR_CON_01_a_regular_closed_rectangle_creates_a_profile",
            extractClosedProfiles(regular).profiles.size() == 1u
                    && deriveSketchArrangement(regular).faces.size() == 1u);

    CadSketch built = regular;
    setSketchEntitiesRole(&built, {rect}, SketchEntityRole::Construction);
    const SketchArrangement none = deriveSketchArrangement(built);
    r.check("DR_CON_02_the_same_rectangle_as_construction_creates_no_profile_and_no_cell",
            extractClosedProfiles(built).profiles.empty() && none.status == ArrangementStatus::Ok
                    && none.faces.empty() && validateCadSketch(built) == CadStatus::Ok
                    && validateCadBodyState(makeCadBodyState(built, profileExtrude(rect)))
                               != CadStatus::Ok);

    CadSketch back = built;
    const bool toggled = sketchRoleToggleTarget(back, {rect}) == SketchEntityRole::Regular
                         && setSketchEntitiesRole(&back, {rect}, SketchEntityRole::Regular)
                                    == CadStatus::Ok;
    r.check("DR_CON_03_toggling_construction_back_to_regular_restores_the_profile_with_its_id",
            toggled && extractClosedProfiles(back).profiles.size() == 1u
                    && extractClosedProfiles(back).profiles[0].anchorEntityId == rect
                    && sameCadSketch(back, regular));

    // A construction centre line as a Revolve axis: the square revolves about
    // it, and the line splits no cell because it is not material.
    {
        CadSketch s;
        const SketchEntityId square = add(&s, rectangle(1.5, 0.5, 1.0, 1.0));
        const SketchEntityId axis = add(&s, line(0.0, -1.0, 0.0, 2.0), SketchEntityRole::Construction);
        RevolveFeature revolve;
        revolve.profileEntityId = square;
        revolve.axis = CadSketchEdgeRef{axis, 0};
        revolve.angleDegrees = 360.0;
        const CadBodyState state = makeCadRevolveBodyState(s, revolve);
        CadBodyMesh mesh;
        r.check("DR_CON_04_a_construction_line_is_a_valid_revolve_axis",
                validateCadBodyState(state) == CadStatus::Ok
                        && regenerateCadBody(state, &mesh) == CadStatus::Ok && mesh.volume > 0.0
                        && deriveSketchArrangement(s).faces.size() == 1u);
    }
    // A construction line as a Mirror axis.
    {
        CadSketch s;
        const SketchEntityId c = add(&s, circle(2.0, 1.0, 0.5));
        const SketchEntityId axis = add(&s, line(0.0, -3.0, 0.0, 3.0), SketchEntityRole::Construction);
        std::vector<SketchEntityId> made;
        const CadStatus why = applySketchMirror(&s, {c}, CadSketchEdgeRef{axis, 0}, &made);
        const SketchEntity* mirrored = made.size() == 1u ? entityOf(s, made[0]) : nullptr;
        r.check("DR_CON_05_a_construction_line_is_a_valid_mirror_axis",
                why == CadStatus::Ok && mirrored != nullptr && mirrored->circle() != nullptr
                        && samePoint(mirrored->circle()->center, SketchPoint{-2.0, 1.0}));
    }
    // Every kind carries its role through `CADB` v8 and back.
    {
        CadSketch s;
        const SketchEntityRole c = SketchEntityRole::Construction;
        add(&s, rectangle(0.0, 0.0, 2.0, 1.0));
        add(&s, line(5.0, 0.0, 6.0, 0.0), c);
        add(&s, polyline({{5.0, 1.0}, {6.0, 1.0}, {6.0, 2.0}}), c);
        add(&s, rectangle(5.0, 4.0, 1.0, 1.0), c);
        add(&s, circle(8.0, 0.0, 0.5), c);
        add(&s, arc(8.0, 2.0, 9.0, 3.0, 10.0, 2.0), c);
        add(&s, spline({{8.0, 5.0}, {9.0, 6.0}, {10.0, 5.0}}), c);
        const CadBodyState state = makeCadBodyState(s, profileExtrude(1));
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state));
        ProjectDocument out;
        bool roles = decodeStatus(bytes, &out) == ProjectCodecStatus::Ok && out.cad.bodies.size() == 1u;
        if (roles) {
            const CadSketch& read = cadBaseSketch(out.cad.bodies[0].state);
            roles = read.entities.size() == 7u && !read.entities[0].construction();
            for (size_t i = 1; roles && i < read.entities.size(); ++i) {
                roles = read.entities[i].construction();
            }
            roles = roles && sameCadBodyState(out.cad.bodies[0].state, state);
        }
        r.check("DR_CON_06_every_entity_kind_persists_its_role_through_cadb_v8",
                cadVersionOf(bytes) == kCadSectionVersionV8 && roles);
    }
}

// ---------------------------------------------------------------------------
// DR-DIM: retained dimensions
// ---------------------------------------------------------------------------

void testDimensions(Checks& r) {
    const SketchDimensionMode drive = SketchDimensionMode::Driving;
    const SketchDimensionMode ref = SketchDimensionMode::Reference;
    {
        CadSketch s;
        const SketchEntityId l = add(&s, line(0.0, 0.0, 3.0, 4.0));
        const SketchDimensionId d = addDim(&s, SketchDimensionKind::LineLength, drive, {l, 0});
        const bool read5 = near(valueOf(s, d), 5.0);
        const CadStatus set = applySketchDimensionValue(&s, d, 10.0);
        const SketchLine* after = entityOf(s, l)->line();
        r.check("DR_DIM_01_line_length_driving_keeps_p0_and_direction_exactly",
                read5 && set == CadStatus::Ok && samePoint(after->start, SketchPoint{0.0, 0.0})
                        && samePoint(after->end, SketchPoint{6.0, 8.0}) && near(valueOf(s, d), 10.0)
                        && applySketchDimensionValue(&s, d, 0.0) == CadStatus::SketchDimensionValueInvalid
                        && applySketchDimensionValue(&s, d, -2.0) == CadStatus::SketchDimensionValueInvalid
                        && applySketchDimensionValue(&s, d, NAN) == CadStatus::SketchDimensionValueInvalid
                        && samePoint(entityOf(s, l)->line()->end, SketchPoint{6.0, 8.0}));
    }
    {
        CadSketch s;
        const SketchEntityId l = add(&s, line(1.0, 1.0, 4.0, 5.0));
        const SketchDimensionId d = addDim(&s, SketchDimensionKind::LineAngle, drive, {l, 0});
        const bool up = applySketchDimensionValue(&s, d, 90.0) == CadStatus::Ok
                        && samePoint(entityOf(s, l)->line()->end, SketchPoint{1.0, 6.0})
                        && near(valueOf(s, d), 90.0);
        const bool back = applySketchDimensionValue(&s, d, 180.0) == CadStatus::Ok
                          && samePoint(entityOf(s, l)->line()->end, SketchPoint{-4.0, 1.0})
                          && near(valueOf(s, d), 180.0);
        const bool skew = applySketchDimensionValue(&s, d, 30.0) == CadStatus::Ok
                          && near(valueOf(s, d), 30.0, 1e-12)
                          && near(std::hypot(entityOf(s, l)->line()->end.u - 1.0,
                                             entityOf(s, l)->line()->end.v - 1.0), 5.0, 1e-12);
        r.check("DR_DIM_02_line_angle_driving_keeps_p0_and_length_in_the_canonical_interval",
                up && back && skew
                        && applySketchDimensionValue(&s, d, -180.0) == CadStatus::SketchDimensionValueInvalid
                        && applySketchDimensionValue(&s, d, 180.5) == CadStatus::SketchDimensionValueInvalid
                        && samePoint(entityOf(s, l)->line()->start, SketchPoint{1.0, 1.0}));
    }
    {
        CadSketch s;
        const SketchEntityId rc = add(&s, rectangle(1.0, 2.0, 4.0, 2.0));
        const SketchDimensionId w = addDim(&s, SketchDimensionKind::RectangleWidth, drive, {rc, 0});
        const SketchDimensionId h = addDim(&s, SketchDimensionKind::RectangleHeight, drive, {rc, 0});
        const bool width = applySketchDimensionValue(&s, w, 3.0) == CadStatus::Ok
                           && entityOf(s, rc)->rectangle()->width == 3.0
                           && entityOf(s, rc)->rectangle()->height == 2.0
                           && samePoint(entityOf(s, rc)->rectangle()->center, SketchPoint{1.0, 2.0});
        r.check("DR_DIM_03_rectangle_width_driving_keeps_centre_and_height", width);
        const bool height = applySketchDimensionValue(&s, h, 0.75) == CadStatus::Ok
                            && entityOf(s, rc)->rectangle()->height == 0.75
                            && entityOf(s, rc)->rectangle()->width == 3.0
                            && samePoint(entityOf(s, rc)->rectangle()->center, SketchPoint{1.0, 2.0});
        r.check("DR_DIM_04_rectangle_height_driving_keeps_centre_and_width", height);
    }
    {
        CadSketch s;
        const SketchEntityId c = add(&s, circle(1.0, 1.0, 2.0));
        const SketchDimensionId rd = addDim(&s, SketchDimensionKind::CircleRadius, drive, {c, 0});
        r.check("DR_DIM_05_circle_radius_driving_keeps_the_centre",
                applySketchDimensionValue(&s, rd, 2.5) == CadStatus::Ok
                        && entityOf(s, c)->circle()->radius == 2.5
                        && samePoint(entityOf(s, c)->circle()->center, SketchPoint{1.0, 1.0}));
        CadSketch t;
        const SketchEntityId c2 = add(&t, circle(1.0, 1.0, 2.0));
        const SketchDimensionId dd = addDim(&t, SketchDimensionKind::CircleDiameter, drive, {c2, 0});
        r.check("DR_DIM_06_circle_diameter_driving_writes_the_same_radius",
                valueOf(t, dd) == 4.0 && applySketchDimensionValue(&t, dd, 3.0) == CadStatus::Ok
                        && entityOf(t, c2)->circle()->radius == 1.5 && valueOf(t, dd) == 3.0);
        CadStatus conflict = CadStatus::Ok;
        addDim(&t, SketchDimensionKind::CircleRadius, drive, {c2, 0}, {}, &conflict);
        CadStatus asReference = CadStatus::Ok;
        addDim(&t, SketchDimensionKind::CircleRadius, ref, {c2, 0}, {}, &asReference);
        CadStatus duplicate = CadStatus::Ok;
        addDim(&t, SketchDimensionKind::CircleRadius, ref, {c2, 0}, {}, &duplicate);
        r.check("DR_DIM_07_radius_and_diameter_cannot_both_drive_one_circle",
                conflict == CadStatus::SketchDimensionConflict && asReference == CadStatus::Ok
                        && duplicate == CadStatus::SketchDimensionConflict
                        && t.dimensions.size() == 2u);
    }
    {
        CadSketch s;
        const SketchEntityId l = add(&s, line(1.0, 2.0, 4.0, -2.0));
        const SketchDimensionId hz = addDim(&s, SketchDimensionKind::LineHorizontal, ref, {l, 0});
        const SketchDimensionId vt = addDim(&s, SketchDimensionKind::LineVertical, ref, {l, 0});
        CadStatus drivenH = CadStatus::Ok;
        addDim(&s, SketchDimensionKind::LineHorizontal, drive, {l, 0}, {}, &drivenH);
        r.check("DR_DIM_08_line_horizontal_and_vertical_references_are_exact_and_read_only",
                valueOf(s, hz) == 3.0 && valueOf(s, vt) == 4.0
                        && drivenH == CadStatus::SketchDimensionInvalid
                        && applySketchDimensionValue(&s, hz, 1.0) == CadStatus::SketchDimensionReadOnly);
    }
    {
        CadSketch s;
        const SketchEntityId p = add(&s, polyline({{0.0, 0.0}, {3.0, 4.0}, {3.0, 10.0}}));
        const SketchDimensionId a = addDim(&s, SketchDimensionKind::EdgeLength, ref, {p, 0});
        const SketchDimensionId b = addDim(&s, SketchDimensionKind::EdgeLength, ref, {p, 1});
        CadStatus past = CadStatus::Ok;
        addDim(&s, SketchDimensionKind::EdgeLength, ref, {p, 2}, {}, &past);
        r.check("DR_DIM_09_a_polyline_segment_length_reference_is_exact",
                valueOf(s, a) == 5.0 && valueOf(s, b) == 6.0
                        && past == CadStatus::SketchDimensionInvalid);
    }
    {
        CadSketch s;
        const SketchEntityId a = add(&s, arc(1.0, 0.0, 0.0, 1.0, -1.0, 0.0));
        const SketchDimensionId radius = addDim(&s, SketchDimensionKind::ArcRadius, ref, {a, 0});
        const SketchDimensionId sweep = addDim(&s, SketchDimensionKind::ArcSweep, ref, {a, 0});
        CadStatus drivenArc = CadStatus::Ok;
        addDim(&s, SketchDimensionKind::ArcRadius, drive, {a, 0}, {}, &drivenArc);
        r.check("DR_DIM_10_an_arc_radius_reference_is_exact",
                near(valueOf(s, radius), 1.0, 1e-12) && drivenArc == CadStatus::SketchDimensionInvalid);
        r.check("DR_DIM_11_an_arc_sweep_reference_is_exact",
                near(valueOf(s, sweep), 180.0, 1e-9));
    }
    {
        CadSketch s;
        const SketchEntityId a = add(&s, line(0.0, 0.0, 1.0, 0.0));
        const SketchEntityId b = add(&s, line(0.0, 0.0, 1.0, 1.0));
        const SketchEntityId rc = add(&s, rectangle(5.0, 5.0, 2.0, 1.0));
        const SketchDimensionId ab = addDim(&s, SketchDimensionKind::EdgeAngle, ref, {a, 0}, {b, 0});
        const SketchDimensionId corner =
                addDim(&s, SketchDimensionKind::EdgeAngle, ref, {rc, 0}, {rc, 1});
        CadStatus self = CadStatus::Ok;
        addDim(&s, SketchDimensionKind::EdgeAngle, ref, {a, 0}, {a, 0}, &self);
        CadStatus spl = CadStatus::Ok;
        const SketchEntityId sp = add(&s, spline({{8.0, 0.0}, {9.0, 1.0}, {10.0, 0.0}}));
        addDim(&s, SketchDimensionKind::EdgeAngle, ref, {a, 0}, {sp, 0}, &spl);
        r.check("DR_DIM_12_the_angle_between_two_straight_edges_is_exact",
                near(valueOf(s, ab), 45.0, 1e-12) && near(valueOf(s, corner), 90.0, 1e-12)
                        && self == CadStatus::SketchDimensionInvalid
                        && spl == CadStatus::SketchDimensionInvalid
                        && applicableSketchDimensionKinds(s, CadSketchEdgeRef{sp, 0}).empty());
    }
    {
        CadSketch s;
        const SketchEntityId l = add(&s, line(0.0, 0.0, 3.0, 4.0));
        const SketchDimensionId d = addDim(&s, SketchDimensionKind::LineLength, drive, {l, 0});
        const CadSketch before = s;
        const bool removed = removeSketchDimension(&s, d) == CadStatus::Ok && s.dimensions.empty();
        r.check("DR_DIM_13_deleting_a_dimension_leaves_the_geometry_unchanged",
                removed && s.entities.size() == 1u
                        && sameSketchEntity(s.entities[0], before.entities[0])
                        && s.nextDimensionId == before.nextDimensionId
                        && removeSketchDimension(&s, d) == CadStatus::SketchDimensionInvalid);
    }
    // DR-DIM-14: ids, the high-water mark, Cancel and Undo through the product's
    // own session commit.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        Driver d;
        bool ok = d.begin();
        const SketchEntityId rect = ok ? d.place(SketchTool::Rectangle, rectangle(0.0, 0.0, 2.0, 1.0))
                                       : kNoSketchEntity;
        ok = ok && rect != kNoSketchEntity
             && d.sketch.setModifyMode(SketchModifyMode::Dimension) == CadStatus::Ok
             && d.sketch.setDimensionTarget(CadSketchEdgeRef{rect, 0}) == CadStatus::Ok;
        SketchDimensionId first = 0, second = 0, third = 0;
        ok = ok && d.sketch.addDimension(SketchDimensionKind::RectangleWidth, drive, &first) == CadStatus::Ok
             && d.sketch.addDimension(SketchDimensionKind::RectangleHeight, ref, &second) == CadStatus::Ok
             && d.sketch.removeDimension(first) == CadStatus::Ok
             && d.sketch.addDimension(SketchDimensionKind::RectangleWidth, ref, &third) == CadStatus::Ok;
        const bool ids = first == 1u && second == 2u && third == 3u
                         && d.sketch.sketch().nextDimensionId == 4u;
        ObjectId id = kNoObject;
        ok = ok && d.sketch.finish() == CadStatus::Ok
             && d.sketch.commit(scene, history, &id) == CadStatus::Ok;
        const SceneObject* body = scene.findBody(id);
        const CadBodyState committed = body != nullptr ? body->cadOrNull()->state() : CadBodyState{};
        const bool stored = cadBaseSketch(committed).nextDimensionId == 4u
                            && cadBaseSketch(committed).dimensions.size() == 2u;
        // A staged edit that adds a dimension and is CANCELLED burns nothing.
        SketchSession edit;
        const SketchFrame frame{Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}};
        bool cancelled = edit.beginEdit(id, committed, frame) == CadStatus::Ok
                         && edit.setModifyMode(SketchModifyMode::Dimension) == CadStatus::Ok
                         && edit.setDimensionTarget(CadSketchEdgeRef{rect, 0}) == CadStatus::Ok;
        SketchDimensionId burned = 0;
        cancelled = cancelled
                    && edit.addDimension(SketchDimensionKind::EdgeLength, ref, &burned) == CadStatus::Ok
                    && burned == 4u;
        edit.cancel();
        const bool untouched = scene.findBody(id) != nullptr
                               && sameCadBodyState(scene.findBody(id)->cadOrNull()->state(), committed);
        // A committed edit that adds one, then Undo / Redo.
        SketchSession again;
        SketchDimensionId fourth = 0;
        bool edited = again.beginEdit(id, committed, frame) == CadStatus::Ok
                      && again.setModifyMode(SketchModifyMode::Dimension) == CadStatus::Ok
                      && again.setDimensionTarget(CadSketchEdgeRef{rect, 0}) == CadStatus::Ok
                      && again.addDimension(SketchDimensionKind::EdgeLength, ref, &fourth) == CadStatus::Ok
                      && again.finish() == CadStatus::Ok && again.commitEdit(scene, history) == CadStatus::Ok;
        const CadBodyState afterEdit = scene.findBody(id)->cadOrNull()->state();
        const bool undone = history.undo()
                            && sameCadBodyState(scene.findBody(id)->cadOrNull()->state(), committed);
        const bool redone = history.redo()
                            && sameCadBodyState(scene.findBody(id)->cadOrNull()->state(), afterEdit);
        // A forward edit may never LOWER the mark.
        CadBodyState lowered = afterEdit;
        cadBaseSketch(lowered).nextDimensionId = 2u;
        cadBaseSketch(lowered).dimensions.clear();
        const CadStatus refusedLower = scene.findBody(id)->cadOrNull()->applyState(lowered);
        r.check("DR_DIM_14_dimension_ids_high_water_cancel_and_undo_follow_the_branch_rule",
                ok && ids && stored && cancelled && untouched && edited && fourth == 4u
                        && cadBaseSketch(afterEdit).nextDimensionId == 5u && undone && redone
                        && refusedLower == CadStatus::HighWaterInvalid);
    }
    {
        CadSketch s;
        const SketchEntityId a = add(&s, line(0.0, 0.0, 2.0, 0.0));
        const SketchEntityId b = add(&s, line(0.0, 0.0, 0.0, 2.0));
        const SketchEntityId c = add(&s, circle(5.0, 5.0, 1.0));
        addDim(&s, SketchDimensionKind::LineLength, drive, {a, 0});
        addDim(&s, SketchDimensionKind::CircleRadius, drive, {c, 0});
        addDim(&s, SketchDimensionKind::EdgeAngle, ref, {a, 0}, {b, 0});
        CadSketch one = s;
        uint32_t removed = 0;
        const bool circleGone = deleteSketchEntities(&one, {c}, &removed) == CadStatus::Ok
                                && removed == 1u && one.dimensions.size() == 2u
                                && one.entities.size() == 2u;
        CadSketch blocked = s;
        const bool refused = deleteSketchEntities(&blocked, {a}, &removed)
                                     == CadStatus::SketchDimensionDependency
                             && sameCadSketch(blocked, s);
        CadSketch both = s;
        const bool pair = deleteSketchEntities(&both, {a, b}, &removed) == CadStatus::Ok
                          && removed == 2u && both.dimensions.size() == 1u
                          && both.dimensions[0].kind == SketchDimensionKind::CircleRadius;
        r.check("DR_DIM_15_deleting_an_entity_removes_only_the_dimensions_that_measure_only_it",
                circleGone && refused && pair);
    }
    {
        CadSketch s;
        const SketchEntityId h = add(&s, line(0.0, 0.0, 4.0, 0.0));
        add(&s, line(2.0, -1.0, 2.0, 1.0));
        addDim(&s, SketchDimensionKind::LineLength, ref, {h, 0});
        SketchTrimPlan plan;
        r.check("DR_DIM_16_trimming_a_dimensioned_entity_is_refused_by_name",
                planSketchTrim(s, SketchPoint{1.0, 0.0}, 0.05, &plan)
                        == CadStatus::SketchDimensionDependency);
    }
    {
        CadSketch s;
        const SketchEntityId l = add(&s, line(0.0, 0.0, 1.0, 0.0));
        add(&s, line(3.0, -1.0, 3.0, 1.0));
        CadSketch referenced = s;
        const SketchDimensionId refLength = addDim(&referenced, SketchDimensionKind::LineLength, ref, {l, 0});
        addDim(&s, SketchDimensionKind::LineLength, drive, {l, 0});
        SketchExtendPlan locked;
        SketchExtendPlan free;
        const bool lockedRefused = planSketchExtend(s, SketchPoint{0.95, 0.0}, 0.05, &locked)
                                   == CadStatus::SketchDimensionLocked;
        const bool referenceFollows = planSketchExtend(referenced, SketchPoint{0.95, 0.0}, 0.05, &free)
                                              == CadStatus::Ok
                                      && valueOf(free.result, refLength) == 3.0;
        r.check("DR_DIM_17_extend_against_a_driving_length_is_refused_and_a_reference_follows",
                lockedRefused && referenceFollows);
    }
    // The annotations themselves: built for every kind, deterministic, and on
    // the stated side.
    {
        const CadSketch s = referenceFixtureState().sketches[0].sketch;
        bool all = true;
        for (const SketchDimension& d : s.dimensions) {
            SketchDimensionAnnotation a1;
            SketchDimensionAnnotation a2;
            all = all && buildSketchDimensionAnnotation(s, d, 0.01, &a1)
                  && buildSketchDimensionAnnotation(s, d, 0.01, &a2) && !a1.segments.empty()
                  && a1.segments.size() % 2u == 0u && a1.segments.size() == a2.segments.size()
                  && samePoint(a1.label, a2.label);
        }
        const CadSketch dr = drivingFixtureSketch();
        SketchDimensionAnnotation width;
        const bool below = buildSketchDimensionAnnotation(dr, dr.dimensions[0], 0.01, &width)
                           && width.label.v < -0.5 && near(width.value, 2.0);
        SketchDimensionAnnotation zoomed;
        const bool scales = buildSketchDimensionAnnotation(dr, dr.dimensions[0], 0.02, &zoomed)
                            && zoomed.label.v < width.label.v;
        r.check("DR_DIM_18_every_kind_builds_a_deterministic_annotation_on_its_stated_side",
                all && below && scales);
    }
    // One entity's labels never claim one box: a line's Length beside its
    // Angle at every small angle (where the bisector rule would put both on
    // the same side), and an arc's Radius beside its Sweep (which share the
    // middle ray). A label box is taken as 90 x 48 reference units -- wider
    // than a "(R 0.812 m)" chip and as tall as the 48 dp floor.
    {
        const double unit = 0.01;
        const auto apart = [&](const SketchPoint& a, const SketchPoint& b) {
            return std::fabs(a.u - b.u) >= 90.0 * unit || std::fabs(a.v - b.v) >= 48.0 * unit;
        };
        bool lines = true;
        for (double length : {0.9, 1.2, 2.0, 4.0}) {
            for (int degrees = -44; degrees <= 44; ++degrees) {
                const double t = static_cast<double>(degrees) * 3.14159265358979323846 / 180.0;
                CadSketch s;
                const SketchEntityId l =
                        add(&s, line(0.0, 0.0, length * std::cos(t), length * std::sin(t)));
                const SketchDimensionId len = addDim(&s, SketchDimensionKind::LineLength, drive, {l, 0});
                const SketchDimensionId ang = addDim(&s, SketchDimensionKind::LineAngle, drive, {l, 0});
                SketchDimensionAnnotation a;
                SketchDimensionAnnotation b;
                lines = lines && buildSketchDimensionAnnotation(s, *findSketchDimension(s, len), unit, &a)
                        && buildSketchDimensionAnnotation(s, *findSketchDimension(s, ang), unit, &b)
                        && apart(a.label, b.label);
            }
        }
        bool arcs = true;
        for (double radius : {0.5, 0.8, 1.5}) {
            for (int middle = 0; middle < 360; middle += 15) {
                const double m = static_cast<double>(middle) * 3.14159265358979323846 / 180.0;
                const double half = 0.9;  // a 103-degree arc
                CadSketch s;
                const SketchEntityId e = add(
                        &s, arc(radius * std::cos(m - half), radius * std::sin(m - half),
                                radius * std::cos(m), radius * std::sin(m),
                                radius * std::cos(m + half), radius * std::sin(m + half)));
                const SketchDimensionId rad = addDim(&s, SketchDimensionKind::ArcRadius, ref, {e, 0});
                const SketchDimensionId swp = addDim(&s, SketchDimensionKind::ArcSweep, ref, {e, 0});
                SketchDimensionAnnotation a;
                SketchDimensionAnnotation b;
                arcs = arcs && buildSketchDimensionAnnotation(s, *findSketchDimension(s, rad), unit, &a)
                       && buildSketchDimensionAnnotation(s, *findSketchDimension(s, swp), unit, &b)
                       && apart(a.label, b.label);
            }
        }
        r.check("DR_DIM_19_one_entitys_labels_stand_apart_line_length_and_angle_arc_radius_and_sweep",
                lines && arcs);
    }
    // Every label names the point it stands OFF from, on the geometry it
    // measures, and stands away from it: the chrome pushes the drawn box along
    // `label - attach` until the whole box clears it, so a label can never own
    // taps on its own stroke.
    {
        const double unit = 0.01;
        CadSketch s;
        const SketchEntityId v = add(&s, line(0.0, 0.0, 0.0, 2.0));
        const SketchEntityId steep = add(&s, line(3.0, 0.0, 3.5, 2.0));
        const SketchEntityId shallow = add(&s, line(5.0, 0.0, 7.0, 0.3));
        const SketchEntityId box = add(&s, rectangle(10.0, 0.0, 2.0, 1.0));
        const SketchEntityId disk = add(&s, circle(15.0, 0.0, 0.8));
        const SketchEntityId bow = add(&s, arc(19.0, 0.0, 20.0, 1.0, 21.0, 0.0));
        struct Expect {
            SketchDimensionKind kind;
            CadSketchEdgeRef first;
            SketchPoint attach;
        };
        const std::vector<Expect> expect = {
            {SketchDimensionKind::LineLength, {v, 0}, SketchPoint{0.0, 1.0}},
            {SketchDimensionKind::LineAngle, {steep, 0}, SketchPoint{NAN, NAN}},
            {SketchDimensionKind::LineAngle, {shallow, 0}, SketchPoint{NAN, NAN}},
            {SketchDimensionKind::RectangleWidth, {box, 0}, SketchPoint{10.0, -0.5}},
            {SketchDimensionKind::RectangleHeight, {box, 0}, SketchPoint{11.0, 0.0}},
            {SketchDimensionKind::CircleRadius, {disk, 0}, SketchPoint{NAN, NAN}},
            {SketchDimensionKind::CircleDiameter, {disk, 0}, SketchPoint{NAN, NAN}},
            {SketchDimensionKind::ArcRadius, {bow, 0}, SketchPoint{20.0, 1.0}},
            {SketchDimensionKind::ArcSweep, {bow, 0}, SketchPoint{NAN, NAN}},
        };
        bool all = true;
        for (const Expect& e : expect) {
            const SketchDimensionId id = addDim(&s, e.kind, ref, e.first);
            SketchDimensionAnnotation a;
            const bool built = id != kNoSketchDimension
                               && buildSketchDimensionAnnotation(s, *findSketchDimension(s, id), unit, &a);
            const double away = built ? std::hypot(a.label.u - a.attach.u, a.label.v - a.attach.v) : 0.0;
            bool on = built && std::isfinite(a.attach.u) && std::isfinite(a.attach.v) && away > 10.0 * unit;
            if (on && std::isfinite(e.attach.u)) {
                on = nearPoint(a.attach, e.attach, 1e-9);
            }
            if (on && e.kind == SketchDimensionKind::CircleRadius) {
                on = near(std::hypot(a.attach.u - 15.0, a.attach.v), 0.8, 1e-9);
            }
            if (on && e.kind == SketchDimensionKind::LineAngle) {
                // Square off the line for a small angle, on the arc for a large one.
                const SketchLine& l = *findSketchEntity(s, e.first.entityId)->line();
                const double lu = l.end.u - l.start.u;
                const double lv = l.end.v - l.start.v;
                const double cross = (a.attach.u - l.start.u) * lv - (a.attach.v - l.start.v) * lu;
                on = e.first.entityId == shallow ? near(cross, 0.0, 1e-9) : true;
            }
            all = all && on;
        }
        r.check("DR_DIM_20_every_label_stands_off_a_point_on_the_geometry_it_measures", all);
    }
}

// ---------------------------------------------------------------------------
// DR-SNAP
// ---------------------------------------------------------------------------

void testSnaps(Checks& r) {
    const std::vector<SketchPoint> none;
    // Two lines crossing at (1, 0): A from (0, 0) to (3, 0), B from (1, -1) to (1, 1).
    CadSketch cross;
    add(&cross, line(0.0, 0.0, 3.0, 0.0));
    add(&cross, line(1.0, -1.0, 1.0, 1.0));
    const SketchSnapCandidates c = collectSketchSnapCandidates(cross);
    {
        // (1, 0.5) is 0.5 from B's end (1, 1) AND from the crossing (1, 0):
        // outside the inner aperture, the endpoint wins by priority.
        const SketchSnapResult s = snapSketchPoint(c, SketchPoint{1.0, 0.5}, 0.6, 0.25, none, nullptr);
        r.check("DR_SNAP_01_an_endpoint_outranks_an_intersection_at_the_same_distance",
                s.kind == SketchSnapKind::Endpoint && samePoint(s.point, SketchPoint{1.0, 1.0}));
    }
    {
        const SketchSnapResult s = snapSketchPoint(c, SketchPoint{1.05, 0.02}, 0.3, 0.25, none, nullptr);
        r.check("DR_SNAP_02_the_crossing_of_two_lines_is_an_exact_intersection_snap",
                c.intersectionsDerived && c.intersections.size() == 1u
                        && s.kind == SketchSnapKind::Intersection
                        && samePoint(s.point, SketchPoint{1.0, 0.0}));
    }
    {
        const SketchSnapResult s = snapSketchPoint(c, SketchPoint{1.52, 0.03}, 0.3, 0.25, none, nullptr);
        CadSketch r2;
        add(&r2, rectangle(0.0, 0.0, 4.0, 2.0));
        const SketchSnapResult edge =
                snapSketchPoint(collectSketchSnapCandidates(r2), SketchPoint{0.04, -0.98}, 0.3, 0.25,
                                none, nullptr);
        r.check("DR_SNAP_03_a_line_and_a_rectangle_edge_offer_their_exact_midpoint",
                s.kind == SketchSnapKind::Midpoint && samePoint(s.point, SketchPoint{1.5, 0.0})
                        && edge.kind == SketchSnapKind::Midpoint
                        && samePoint(edge.point, SketchPoint{0.0, -1.0}));
    }
    {
        CadSketch s;
        add(&s, circle(5.0, 5.0, 1.0));
        add(&s, arc(9.0, 0.0, 10.0, 1.0, 11.0, 0.0));
        add(&s, rectangle(-5.0, 5.0, 2.0, 2.0));
        const SketchSnapCandidates sc = collectSketchSnapCandidates(s);
        const SketchSnapResult a = snapSketchPoint(sc, SketchPoint{5.02, 5.01}, 0.3, 0.25, none, nullptr);
        const SketchSnapResult b = snapSketchPoint(sc, SketchPoint{10.03, 0.02}, 0.3, 0.25, none, nullptr);
        const SketchSnapResult d = snapSketchPoint(sc, SketchPoint{-5.02, 4.97}, 0.3, 0.25, none, nullptr);
        r.check("DR_SNAP_04_circle_arc_and_rectangle_centres_are_exact",
                a.kind == SketchSnapKind::Center && samePoint(a.point, SketchPoint{5.0, 5.0})
                        && b.kind == SketchSnapKind::Center && nearPoint(b.point, SketchPoint{10.0, 0.0}, 1e-12)
                        && d.kind == SketchSnapKind::Center && samePoint(d.point, SketchPoint{-5.0, 5.0}));
    }
    {
        CadSketch s;
        add(&s, circle(5.0, 5.0, 1.0));
        const SketchSnapResult o = snapSketchPoint(collectSketchSnapCandidates(s), SketchPoint{0.03, -0.02},
                                                   0.3, 0.25, none, nullptr);
        r.check("DR_SNAP_05_the_origin_is_an_exact_snap",
                o.kind == SketchSnapKind::Origin && o.point.u == 0.0 && o.point.v == 0.0);
    }
    {
        CadSketch s;
        add(&s, line(2.0, 3.0, 2.0, 9.0));
        const SketchSnapCandidates sc = collectSketchSnapCandidates(s);
        const SketchSnapResult h = snapSketchPoint(sc, SketchPoint{5.1, 3.04}, 0.2, 0.5, none, nullptr);
        r.check("DR_SNAP_06_a_horizontal_guide_snaps_v_exactly_and_records_nothing",
                h.kind == SketchSnapKind::HorizontalGuide && h.horizontalGuide && h.point.v == 3.0
                        && h.point.u == 5.0 && samePoint(h.horizontalSource, SketchPoint{2.0, 3.0})
                        && s.dimensions.empty());
        const SketchSnapResult v = snapSketchPoint(sc, SketchPoint{2.03, 15.2}, 0.2, 0.5, none, nullptr);
        r.check("DR_SNAP_07_a_vertical_guide_snaps_u_exactly",
                v.kind == SketchSnapKind::VerticalGuide && v.verticalGuide && v.point.u == 2.0
                        && v.point.v == 15.0);
    }
    {
        CadSketch s;
        add(&s, line(0.0, 0.0, 1.0, 0.0));
        add(&s, line(2.0, 0.0, 3.0, 0.0));
        CadSketch reversed;
        reversed.entities = {s.entities[1], s.entities[0]};
        reversed.nextEntityId = 3;
        const SketchSnapResult a = snapSketchPoint(collectSketchSnapCandidates(s), SketchPoint{1.5, 0.3},
                                                   0.6, 0.25, none, nullptr);
        const SketchSnapResult b = snapSketchPoint(collectSketchSnapCandidates(reversed),
                                                   SketchPoint{1.5, 0.3}, 0.6, 0.25, none, nullptr);
        r.check("DR_SNAP_08_an_exact_tie_keeps_the_earlier_entity_whatever_the_storage_order",
                a.kind == SketchSnapKind::Endpoint && samePoint(a.point, SketchPoint{1.0, 0.0})
                        && b.kind == a.kind && samePoint(b.point, a.point));
    }
    {
        CadSketch s;
        add(&s, line(0.0, 0.0, 4.0, 0.0), SketchEntityRole::Construction);
        add(&s, circle(2.0, 2.0, 0.5), SketchEntityRole::Construction);
        const SketchSnapCandidates sc = collectSketchSnapCandidates(s);
        const SketchSnapResult end = snapSketchPoint(sc, SketchPoint{3.97, 0.02}, 0.3, 0.25, none, nullptr);
        const SketchSnapResult mid = snapSketchPoint(sc, SketchPoint{2.03, 0.02}, 0.3, 0.25, none, nullptr);
        const SketchSnapResult centre = snapSketchPoint(sc, SketchPoint{2.02, 1.98}, 0.3, 0.25, none, nullptr);
        r.check("DR_SNAP_09_construction_geometry_takes_part_in_every_snap",
                end.kind == SketchSnapKind::Endpoint && mid.kind == SketchSnapKind::Midpoint
                        && centre.kind == SketchSnapKind::Center);
    }
    {
        CadSketch s;
        add(&s, line(0.0, 0.0, 2.0, 0.0));
        add(&s, line(1.0, 0.0, 3.0, 0.0));
        const SketchSnapCandidates sc = collectSketchSnapCandidates(s);
        const SketchSnapResult q = snapSketchPoint(sc, SketchPoint{1.5, 0.01}, 0.05, 0.25, none, nullptr);
        r.check("DR_SNAP_10_an_ambiguous_overlap_invents_no_intersection",
                !sc.intersectionsDerived && sc.intersections.empty()
                        && q.kind != SketchSnapKind::Intersection);
    }
    {
        // A drag's end never snaps back onto its own start, and the inner
        // aperture puts an exact centre under the finger ahead of a far
        // intersection.
        CadSketch s;
        add(&s, rectangle(0.0, 0.0, 1.0, 1.0));
        add(&s, circle(0.5, 0.0, 0.3));
        const SketchSnapCandidates sc = collectSketchSnapCandidates(s);
        const SketchSnapResult atCentre = snapSketchPoint(sc, SketchPoint{0.01, 0.0}, 0.7, 0.25, none, nullptr);
        const SketchPoint start{0.5, 0.0};
        const SketchSnapResult notStart =
                snapSketchPoint(sc, SketchPoint{0.52, 0.01}, 0.7, 0.25, none, nullptr, &start);
        r.check("DR_SNAP_11_the_inner_aperture_and_the_drag_start_rule_hold",
                atCentre.kind == SketchSnapKind::Center && samePoint(atCentre.point, SketchPoint{0.0, 0.0})
                        && !samePoint(notStart.point, start));
    }
}

// ---------------------------------------------------------------------------
// DR-TRIM
// ---------------------------------------------------------------------------

void testTrim(Checks& r) {
    CadSketch ladder;
    const SketchEntityId rail = add(&ladder, line(0.0, 0.0, 4.0, 0.0));
    add(&ladder, line(1.0, -1.0, 1.0, 1.0));
    add(&ladder, line(3.0, -1.0, 3.0, 1.0));
    {
        SketchTrimPlan plan;
        const CadStatus why = planSketchTrim(ladder, SketchPoint{0.5, 0.01}, 0.1, &plan);
        const SketchEntity* kept = entityOf(plan.result, rail);
        r.check("DR_TRIM_01_a_line_end_piece_up_to_its_nearest_intersection_is_removed",
                why == CadStatus::Ok && kept != nullptr && kept->line() != nullptr
                        && samePoint(kept->line()->start, SketchPoint{1.0, 0.0})
                        && samePoint(kept->line()->end, SketchPoint{4.0, 0.0})
                        && plan.createdIds.empty() && plan.result.entities.size() == 3u
                        && plan.removed.size() == 2u);
    }
    {
        SketchTrimPlan plan;
        const CadStatus why = planSketchTrim(ladder, SketchPoint{2.0, 0.01}, 0.1, &plan);
        const SketchEntity* first = entityOf(plan.result, rail);
        const SketchEntity* second = plan.createdIds.size() == 1u ? entityOf(plan.result, plan.createdIds[0])
                                                                  : nullptr;
        r.check("DR_TRIM_02_a_line_interval_between_two_intersections_splits_it_in_two",
                why == CadStatus::Ok && first != nullptr && second != nullptr
                        && samePoint(first->line()->start, SketchPoint{0.0, 0.0})
                        && samePoint(first->line()->end, SketchPoint{1.0, 0.0})
                        && samePoint(second->line()->start, SketchPoint{3.0, 0.0})
                        && samePoint(second->line()->end, SketchPoint{4.0, 0.0})
                        && plan.createdIds[0] == ladder.nextEntityId);
    }
    {
        CadSketch s;
        const SketchEntityId p = add(&s, polyline({{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}}));
        add(&s, line(2.0, -1.0, 2.0, 1.0));
        SketchTrimPlan plan;
        const CadStatus why = planSketchTrim(s, SketchPoint{1.0, 0.02}, 0.1, &plan);
        const SketchEntity* kept = entityOf(plan.result, p);
        r.check("DR_TRIM_03_a_polyline_segment_interval_is_removed_keeping_vertex_order",
                why == CadStatus::Ok && kept != nullptr && kept->polyline() != nullptr
                        && kept->polyline()->vertices.size() == 3u
                        && samePoint(kept->polyline()->vertices[0], SketchPoint{2.0, 0.0})
                        && samePoint(kept->polyline()->vertices[1], SketchPoint{4.0, 0.0})
                        && samePoint(kept->polyline()->vertices[2], SketchPoint{4.0, 4.0}));
    }
    {
        CadSketch s;
        const SketchEntityId rc = add(&s, rectangle(0.0, 0.0, 4.0, 2.0), SketchEntityRole::Construction);
        add(&s, line(0.0, -2.0, 0.0, 2.0));
        SketchTrimPlan plan;
        const CadStatus why = planSketchTrim(s, SketchPoint{-1.0, -0.99}, 0.1, &plan);
        size_t lines = 0;
        bool roles = true;
        for (SketchEntityId id : plan.createdIds) {
            const SketchEntity* e = entityOf(plan.result, id);
            lines += (e != nullptr && e->line() != nullptr) ? 1u : 0u;
            roles = roles && e != nullptr && e->construction();
        }
        r.check("DR_TRIM_04_a_trimmed_rectangle_becomes_lines_and_says_so",
                why == CadStatus::Ok && plan.rectangleConverted && entityOf(plan.result, rc) == nullptr
                        && lines == 4u && plan.createdIds.size() == 4u);
        r.check("DR_TRIM_09_every_piece_inherits_the_targets_construction_role", why == CadStatus::Ok && roles);
    }
    {
        CadSketch s;
        const SketchEntityId c = add(&s, circle(0.0, 0.0, 1.0));
        add(&s, line(-2.0, 0.0, 2.0, 0.0));
        SketchTrimPlan plan;
        const CadStatus why = planSketchTrim(s, SketchPoint{0.0, 0.99}, 0.1, &plan);
        const SketchEntity* a = plan.createdIds.size() == 1u ? entityOf(plan.result, plan.createdIds[0])
                                                             : nullptr;
        SketchPoint centre;
        double radius = 0.0;
        double start = 0.0;
        double sweep = 0.0;
        const bool geometry = a != nullptr && a->arc() != nullptr
                              && arcGeometry(*a->arc(), &centre, &radius, &start, &sweep) == CadStatus::Ok;
        r.check("DR_TRIM_05_a_trimmed_circle_becomes_an_arc_on_the_same_circle",
                why == CadStatus::Ok && entityOf(plan.result, c) == nullptr && geometry
                        && nearPoint(centre, SketchPoint{0.0, 0.0}, 1e-12) && near(radius, 1.0, 1e-12)
                        && near(std::fabs(sweep), kPi, 1e-9) && a->arc()->mid.v < -0.99);
    }
    {
        CadSketch s;
        const SketchEntityId a = add(&s, arc(-1.0, 0.0, 0.0, 1.0, 1.0, 0.0));
        add(&s, line(0.0, -2.0, 0.0, 2.0));
        SketchTrimPlan plan;
        const CadStatus why = planSketchTrim(s, SketchPoint{-0.7071, 0.7071}, 0.05, &plan);
        const SketchEntity* kept = entityOf(plan.result, a);
        SketchPoint centre;
        double radius = 0.0;
        r.check("DR_TRIM_06_an_arc_interval_is_removed_and_the_rest_stays_an_arc",
                why == CadStatus::Ok && kept != nullptr && kept->arc() != nullptr
                        && nearPoint(kept->arc()->start, SketchPoint{0.0, 1.0}, 1e-9)
                        && samePoint(kept->arc()->end, SketchPoint{1.0, 0.0})
                        && arcGeometry(*kept->arc(), &centre, &radius, nullptr, nullptr) == CadStatus::Ok
                        && near(radius, 1.0, 1e-9));
    }
    {
        CadSketch s;
        const SketchEntityId h = add(&s, line(-2.0, 0.0, 2.0, 0.0));
        const SketchEntityId sp = add(&s, spline({{-1.0, -1.0}, {0.0, 1.0}, {1.0, -1.0}}));
        SketchTrimPlan plan;
        const CadStatus why = planSketchTrim(s, SketchPoint{0.0, 0.01}, 0.1, &plan);
        const SketchEntity* left = entityOf(plan.result, h);
        const SketchEntity* right = plan.createdIds.size() == 1u ? entityOf(plan.result, plan.createdIds[0])
                                                                 : nullptr;
        const SketchEntity* curve = entityOf(plan.result, sp);
        r.check("DR_TRIM_07_a_spline_cuts_a_line_at_its_exact_crossings",
                why == CadStatus::Ok && left != nullptr && right != nullptr && curve != nullptr
                        && near(left->line()->end.v, 0.0, 1e-9) && near(right->line()->start.v, 0.0, 1e-9)
                        && distanceToCurve(*curve, left->line()->end) < 1e-3
                        && distanceToCurve(*curve, right->line()->start) < 1e-3
                        && left->line()->end.u < 0.0 && right->line()->start.u > 0.0);
        SketchTrimPlan refused;
        r.check("DR_TRIM_08_a_spline_target_is_refused_by_name",
                planSketchTrim(s, SketchPoint{0.0, 0.98}, 0.1, &refused) == CadStatus::TrimSplineUnsupported);
    }
    {
        CadSketch s;
        const SketchEntityId h = add(&s, line(0.0, 0.0, 4.0, 0.0));
        add(&s, line(2.0, -1.0, 2.0, 1.0));
        addDim(&s, SketchDimensionKind::LineLength, SketchDimensionMode::Reference, {h, 0});
        SketchTrimPlan plan;
        const CadSketch before = s;
        r.check("DR_TRIM_10_a_dimensioned_target_refuses_and_nothing_moves",
                planSketchTrim(s, SketchPoint{1.0, 0.0}, 0.1, &plan) == CadStatus::SketchDimensionDependency
                        && sameCadSketch(s, before));
    }
}

// ---------------------------------------------------------------------------
// DR-EXT
// ---------------------------------------------------------------------------

void testExtend(Checks& r) {
    {
        CadSketch s;
        const SketchEntityId l = add(&s, line(0.0, 0.0, 1.0, 0.0));
        add(&s, line(3.0, -1.0, 3.0, 1.0));
        add(&s, line(5.0, -1.0, 5.0, 1.0));
        SketchExtendPlan plan;
        const CadStatus why = planSketchExtend(s, SketchPoint{0.9, 0.01}, 0.2, &plan);
        r.check("DR_EXT_01_a_line_extends_to_its_nearest_forward_intersection_exactly",
                why == CadStatus::Ok && plan.atEnd
                        && samePoint(entityOf(plan.result, l)->line()->start, SketchPoint{0.0, 0.0})
                        && nearPoint(entityOf(plan.result, l)->line()->end, SketchPoint{3.0, 0.0}, 1e-12));
    }
    {
        CadSketch s;
        const SketchEntityId p = add(&s, polyline({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}));
        add(&s, line(-2.0, 4.0, 3.0, 4.0));
        SketchExtendPlan plan;
        const CadStatus why = planSketchExtend(s, SketchPoint{1.01, 0.95}, 0.2, &plan);
        const SketchEntity* e = entityOf(plan.result, p);
        CadSketch closed;
        add(&closed, polyline({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}, true));
        SketchExtendPlan refused;
        r.check("DR_EXT_02_an_open_polyline_end_segment_extends_and_a_closed_one_is_refused",
                why == CadStatus::Ok && e->polyline()->vertices.size() == 3u
                        && nearPoint(e->polyline()->vertices[2], SketchPoint{1.0, 4.0}, 1e-12)
                        && samePoint(e->polyline()->vertices[0], SketchPoint{0.0, 0.0})
                        && planSketchExtend(closed, SketchPoint{1.0, 0.5}, 0.2, &refused)
                                   == CadStatus::ExtendUnsupported);
    }
    {
        CadSketch s;
        const SketchEntityId a = add(&s, arc(1.0, 0.0, 0.0, 1.0, -1.0, 0.0));
        add(&s, line(-2.0, -0.5, 2.0, -0.5));
        SketchExtendPlan plan;
        const CadStatus why = planSketchExtend(s, SketchPoint{-0.99, 0.05}, 0.2, &plan);
        const SketchEntity* e = entityOf(plan.result, a);
        SketchPoint centre;
        double radius = 0.0;
        double start = 0.0;
        double sweep = 0.0;
        const bool circleKept = e != nullptr && e->arc() != nullptr
                                && arcGeometry(*e->arc(), &centre, &radius, &start, &sweep) == CadStatus::Ok
                                && nearPoint(centre, SketchPoint{0.0, 0.0}, 1e-9) && near(radius, 1.0, 1e-9);
        r.check("DR_EXT_03_an_arc_extends_along_its_own_circle_to_the_target",
                why == CadStatus::Ok && circleKept && samePoint(e->arc()->start, SketchPoint{1.0, 0.0})
                        && nearPoint(e->arc()->end, SketchPoint{-std::sqrt(3.0) / 2.0, -0.5}, 1e-9)
                        && near(sweep, kPi + kPi / 6.0, 1e-9));
    }
    {
        CadSketch s;
        const SketchEntityId l = add(&s, line(0.0, 0.0, 1.0, 0.0));
        const SketchEntityId sp = add(&s, spline({{3.0, -1.0}, {3.2, 0.0}, {3.0, 1.0}}));
        SketchExtendPlan plan;
        const CadStatus why = planSketchExtend(s, SketchPoint{0.95, 0.0}, 0.2, &plan);
        const SketchEntity* e = entityOf(plan.result, l);
        r.check("DR_EXT_04_a_spline_is_a_valid_extend_target",
                why == CadStatus::Ok && e != nullptr && near(e->line()->end.v, 0.0, 1e-12)
                        && e->line()->end.u > 3.0 && e->line()->end.u < 3.3
                        && distanceToCurve(*entityOf(plan.result, sp), e->line()->end) < 1e-3);
    }
    {
        CadSketch s;
        add(&s, line(0.0, 0.0, 1.0, 0.0));
        add(&s, line(-3.0, -1.0, -3.0, 1.0));
        SketchExtendPlan plan;
        CadSketch c;
        add(&c, circle(0.0, 0.0, 1.0));
        SketchExtendPlan circlePlan;
        r.check("DR_EXT_05_no_forward_target_and_an_unsupported_kind_are_refused_by_name",
                planSketchExtend(s, SketchPoint{0.95, 0.0}, 0.2, &plan) == CadStatus::ExtendNoTarget
                        && planSketchExtend(c, SketchPoint{1.0, 0.0}, 0.2, &circlePlan)
                                   == CadStatus::ExtendUnsupported);
    }
    {
        CadSketch s;
        const SketchEntityId l = add(&s, line(0.0, 0.0, 1.0, 0.0));
        add(&s, line(3.0, -1.0, 3.0, 1.0));
        addDim(&s, SketchDimensionKind::LineLength, SketchDimensionMode::Driving, {l, 0});
        SketchExtendPlan plan;
        const CadSketch before = s;
        r.check("DR_EXT_06_a_driving_length_locks_extend",
                planSketchExtend(s, SketchPoint{0.95, 0.0}, 0.2, &plan) == CadStatus::SketchDimensionLocked
                        && sameCadSketch(s, before));
    }
}

// ---------------------------------------------------------------------------
// DR-OFF
// ---------------------------------------------------------------------------

void testOffset(Checks& r) {
    std::vector<SketchEntity::Payload> out;
    {
        const SketchEntity l(1u, line(0.0, 0.0, 2.0, 0.0));
        const bool plus = offsetSketchEntity(l, 1.0, &out) == CadStatus::Ok
                          && std::get<SketchLine>(out[0]).start.v == 1.0
                          && std::get<SketchLine>(out[0]).end.v == 1.0
                          && std::get<SketchLine>(out[0]).end.u == 2.0;
        const bool minus = offsetSketchEntity(l, -1.0, &out) == CadStatus::Ok
                           && std::get<SketchLine>(out[0]).start.v == -1.0;
        double d = 0.0;
        const bool side = sketchOffsetDistanceAt(l, SketchPoint{1.0, 0.75}, &d) && d == 0.75;
        r.check("DR_OFF_01_a_line_offsets_to_either_side_exactly", plus && minus && side
                                                                       && offsetSketchEntity(l, 0.0, &out)
                                                                                  == CadStatus::OffsetInvalid);
    }
    {
        const SketchEntity c(1u, circle(1.0, 1.0, 2.0));
        const bool out1 = offsetSketchEntity(c, 0.5, &out) == CadStatus::Ok
                          && std::get<SketchCircle>(out[0]).radius == 2.5;
        const bool in1 = offsetSketchEntity(c, -0.5, &out) == CadStatus::Ok
                         && std::get<SketchCircle>(out[0]).radius == 1.5
                         && samePoint(std::get<SketchCircle>(out[0]).center, SketchPoint{1.0, 1.0});
        r.check("DR_OFF_02_a_circle_offsets_concentrically_in_and_out",
                out1 && in1 && offsetSketchEntity(c, -2.0, &out) == CadStatus::OffsetInvalid);
    }
    {
        const SketchEntity a(1u, arc(1.0, 0.0, 0.0, 1.0, -1.0, 0.0));
        const bool ok = offsetSketchEntity(a, 1.0, &out) == CadStatus::Ok;
        SketchPoint centre;
        double radius = 0.0;
        double start = 0.0;
        double sweep = 0.0;
        const bool kept = ok && arcGeometry(std::get<SketchArc>(out[0]), &centre, &radius, &start, &sweep)
                                        == CadStatus::Ok
                          && near(radius, 2.0, 1e-12) && near(start, 0.0, 1e-12) && near(sweep, kPi, 1e-9)
                          && samePoint(std::get<SketchArc>(out[0]).start, SketchPoint{2.0, 0.0});
        r.check("DR_OFF_03_an_arc_offsets_concentrically_keeping_its_angles",
                kept && offsetSketchEntity(a, -1.0, &out) == CadStatus::OffsetInvalid);
    }
    {
        const SketchEntity rc(1u, rectangle(1.0, 1.0, 4.0, 2.0));
        const bool grow = offsetSketchEntity(rc, 0.5, &out) == CadStatus::Ok
                          && std::get<SketchRectangle>(out[0]).width == 5.0
                          && std::get<SketchRectangle>(out[0]).height == 3.0
                          && samePoint(std::get<SketchRectangle>(out[0]).center, SketchPoint{1.0, 1.0});
        r.check("DR_OFF_04_a_rectangle_offsets_about_its_centre",
                grow && offsetSketchEntity(rc, -0.5, &out) == CadStatus::Ok
                        && std::get<SketchRectangle>(out[0]).height == 1.0
                        && offsetSketchEntity(rc, -1.0, &out) == CadStatus::OffsetInvalid);
    }
    {
        const SketchEntity p(1u, polyline({{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}}));
        const bool ok = offsetSketchEntity(p, 0.5, &out) == CadStatus::Ok;
        const SketchPolyline* o = ok ? &std::get<SketchPolyline>(out[0]) : nullptr;
        r.check("DR_OFF_05_a_polyline_offsets_with_exact_miter_joins",
                o != nullptr && o->vertices.size() == 3u
                        && samePoint(o->vertices[0], SketchPoint{0.0, 0.5})
                        && samePoint(o->vertices[1], SketchPoint{1.5, 0.5})
                        && samePoint(o->vertices[2], SketchPoint{1.5, 2.0}));
        const SketchEntity sharp(2u, polyline({{0.0, 0.0}, {2.0, 0.0}, {0.0, 0.2}}));
        r.check("DR_OFF_06_a_corner_beyond_the_miter_limit_is_refused",
                offsetSketchEntity(sharp, 0.1, &out) == CadStatus::OffsetMiterLimit);
        const SketchEntity zig(3u, polyline({{0.0, 0.0}, {3.0, 0.0}, {3.0, 1.0}, {1.5, 1.0}, {1.5, -1.0}}));
        r.check("DR_OFF_07_an_offset_that_crosses_itself_is_refused",
                offsetSketchEntity(zig, 0.3, &out) == CadStatus::OffsetSelfIntersecting);
    }
    {
        const SketchEntity sp(1u, spline({{0.0, 0.0}, {1.0, 1.0}, {2.0, 0.0}}));
        r.check("DR_OFF_08_a_spline_offset_is_refused_by_name",
                offsetSketchEntity(sp, 0.2, &out) == CadStatus::OffsetSplineUnsupported
                        && !sketchEntityOffsettable(sp));
    }
    {
        CadSketch s;
        const SketchEntityId c = add(&s, circle(0.0, 0.0, 1.0), SketchEntityRole::Construction);
        addDim(&s, SketchDimensionKind::CircleRadius, SketchDimensionMode::Driving, {c, 0});
        std::vector<SketchEntityId> made;
        const bool ok = applySketchOffset(&s, c, 0.5, &made) == CadStatus::Ok && made.size() == 1u;
        r.check("DR_OFF_09_an_offset_inherits_the_sources_role",
                ok && entityOf(s, made[0])->construction()
                        && entityOf(s, made[0])->circle()->radius == 1.5);
        r.check("DR_OFF_10_an_offset_copies_no_dimension",
                ok && s.dimensions.size() == 1u && s.dimensions[0].first.entityId == c
                        && sketchDimensionsReferencing(s, made[0]).empty());
    }
}

// ---------------------------------------------------------------------------
// DR-MIR
// ---------------------------------------------------------------------------

void testMirror(Checks& r) {
    CadSketch s;
    const SketchEntityId axis = add(&s, line(0.0, -10.0, 0.0, 10.0));
    const SketchEntityId l = add(&s, line(1.0, 0.0, 2.0, 1.0));
    const SketchEntityId p = add(&s, polyline({{1.0, 2.0}, {3.0, 2.0}, {3.0, 4.0}}));
    const SketchEntityId c = add(&s, circle(4.0, 1.0, 0.5));
    const SketchEntityId a = add(&s, arc(5.0, 0.0, 6.0, 1.0, 7.0, 0.0));
    const SketchEntityId sp = add(&s, spline({{1.0, 5.0}, {2.0, 6.5}, {3.0, 5.0}}));
    const SketchEntityId rc = add(&s, rectangle(6.0, 5.0, 2.0, 1.0));
    const CadSketchEdgeRef ax{axis, 0};
    std::vector<SketchMirrorPiece> pieces;
    const auto one = [&](SketchEntityId id) -> const SketchEntity::Payload* {
        pieces.clear();
        if (mirrorSketchEntities(s, {id}, ax, &pieces) != CadStatus::Ok || pieces.size() != 1u) {
            return nullptr;
        }
        return &pieces[0].payload;
    };
    const SketchEntity::Payload* m = one(l);
    r.check("DR_MIR_01_a_line_reflects_exactly",
            m != nullptr && samePoint(std::get<SketchLine>(*m).start, SketchPoint{-1.0, 0.0})
                    && samePoint(std::get<SketchLine>(*m).end, SketchPoint{-2.0, 1.0}));
    m = one(p);
    r.check("DR_MIR_02_a_polyline_reflects_keeping_its_vertex_order",
            m != nullptr && std::get<SketchPolyline>(*m).vertices.size() == 3u
                    && samePoint(std::get<SketchPolyline>(*m).vertices[0], SketchPoint{-1.0, 2.0})
                    && samePoint(std::get<SketchPolyline>(*m).vertices[2], SketchPoint{-3.0, 4.0}));
    m = one(c);
    r.check("DR_MIR_03_a_circle_reflects_its_centre",
            m != nullptr && samePoint(std::get<SketchCircle>(*m).center, SketchPoint{-4.0, 1.0})
                    && std::get<SketchCircle>(*m).radius == 0.5);
    m = one(a);
    {
        double sweepA = 0.0;
        double sweepM = 0.0;
        double radiusM = 0.0;
        const bool ok = m != nullptr
                        && arcGeometry(*entityOf(s, a)->arc(), nullptr, nullptr, nullptr, &sweepA) == CadStatus::Ok
                        && arcGeometry(std::get<SketchArc>(*m), nullptr, &radiusM, nullptr, &sweepM)
                                   == CadStatus::Ok;
        r.check("DR_MIR_04_an_arc_reflects_its_three_points_and_its_sense_flips",
                ok && samePoint(std::get<SketchArc>(*m).start, SketchPoint{-5.0, 0.0})
                        && samePoint(std::get<SketchArc>(*m).mid, SketchPoint{-6.0, 1.0})
                        && near(radiusM, 1.0, 1e-12) && near(sweepM, -sweepA, 1e-12));
    }
    m = one(sp);
    r.check("DR_MIR_05_a_spline_reflects_its_authored_points_exactly",
            m != nullptr && samePoint(std::get<SketchSpline>(*m).points[1], SketchPoint{-2.0, 6.5})
                    && samePoint(std::get<SketchSpline>(*m).points[2], SketchPoint{-3.0, 5.0}));
    m = one(rc);
    r.check("DR_MIR_06_an_axis_aligned_mirror_keeps_a_rectangle_a_rectangle",
            m != nullptr && std::holds_alternative<SketchRectangle>(*m)
                    && samePoint(std::get<SketchRectangle>(*m).center, SketchPoint{-6.0, 5.0}));
    {
        CadSketch t;
        const SketchEntityId diag = add(&t, line(0.0, 0.0, 1.0, 1.0));
        const SketchEntityId box = add(&t, rectangle(3.0, 0.0, 2.0, 1.0));
        std::vector<SketchMirrorPiece> lines;
        const CadStatus why = mirrorSketchEntities(t, {box}, CadSketchEdgeRef{diag, 0}, &lines);
        bool swapped = lines.size() == 4u;
        for (size_t k = 0; swapped && k < 4u; ++k) {
            const SketchLine* e = std::get_if<SketchLine>(&lines[k].payload);
            swapped = e != nullptr;
        }
        // Across u = v a point (u, v) lands on (v, u): the first corner (2, -0.5).
        r.check("DR_MIR_07_a_slanted_mirror_turns_a_rectangle_into_four_lines",
                why == CadStatus::Ok && swapped
                        && nearPoint(std::get<SketchLine>(lines[0].payload).start, SketchPoint{-0.5, 2.0}, 1e-12));
    }
    {
        CadSketch t = s;
        setSketchEntitiesRole(&t, {axis}, SketchEntityRole::Construction);
        std::vector<SketchEntityId> made;
        const CadStatus why = applySketchMirror(&t, {c, l}, ax, &made);
        r.check("DR_MIR_08_a_construction_axis_mirrors_and_is_never_itself_mirrored",
                why == CadStatus::Ok && made.size() == 2u
                        && applySketchMirror(&t, {axis}, ax, &made) == CadStatus::MirrorNothingSelected);
    }
    {
        CadSketch t = s;
        setSketchEntitiesRole(&t, {c}, SketchEntityRole::Construction);
        std::vector<SketchEntityId> made;
        const size_t before = t.entities.size();
        const CadStatus why = applySketchMirror(&t, {l, c, sp}, ax, &made);
        r.check("DR_MIR_09_a_multi_selection_mirrors_every_entity_in_one_act_keeping_roles",
                why == CadStatus::Ok && made.size() == 3u && t.entities.size() == before + 3u
                        && entityOf(t, made[1])->construction() && !entityOf(t, made[0])->construction());
        CadSketch curve = s;
        std::vector<SketchEntityId> none;
        r.check("DR_MIR_11_a_curve_is_never_a_mirror_axis",
                applySketchMirror(&curve, {l}, CadSketchEdgeRef{c, 0}, &none) == CadStatus::MirrorAxisInvalid
                        && applySketchMirror(&curve, {l}, CadSketchEdgeRef{a, 0}, &none)
                                   == CadStatus::MirrorAxisInvalid
                        && applySketchMirror(&curve, {l}, CadSketchEdgeRef{sp, 0}, &none)
                                   == CadStatus::MirrorAxisInvalid);
    }
    {
        CadSketch t = s;
        addDim(&t, SketchDimensionKind::LineLength, SketchDimensionMode::Driving, {l, 0});
        std::vector<SketchEntityId> made;
        const CadStatus why = applySketchMirror(&t, {l}, ax, &made);
        r.check("DR_MIR_10_a_mirror_copies_no_dimension",
                why == CadStatus::Ok && t.dimensions.size() == 1u
                        && sketchDimensionsReferencing(t, made[0]).empty());
    }
}

// ---------------------------------------------------------------------------
// DR-SES: the session -- multi-selection, modify modes, overlay, history
// ---------------------------------------------------------------------------

void testSession(Checks& r) {
    Driver d;
    bool ok = d.begin();
    const SketchEntityId rect = ok ? d.place(SketchTool::Rectangle, rectangle(0.0, 0.0, 2.0, 1.0)) : 0;
    const SketchEntityId c = ok ? d.place(SketchTool::Circle, circle(4.0, 0.0, 0.5)) : 0;
    const SketchEntityId l = ok ? d.place(SketchTool::Line, line(0.0, -3.0, 0.0, 3.0)) : 0;
    ok = ok && rect != 0 && c != 0 && l != 0;
    // Multi-selection: a still tap replaces, Select multiple adds and removes,
    // empty space clears.
    d.sketch.setTool(SketchTool::Select);
    d.tap(SketchPoint{1.0, 0.25});
    const bool single = d.sketch.selectedEntityIds().size() == 1u && d.sketch.selectedEntityId() == rect;
    d.sketch.setMultiSelect(true);
    d.tap(SketchPoint{4.5, 0.0});
    d.tap(SketchPoint{0.0, 2.5});
    const bool three = d.sketch.selectedEntityIds().size() == 3u && d.sketch.selectedEntityId() == 0;
    d.tap(SketchPoint{4.5, 0.0});
    const bool removed = d.sketch.selectedEntityIds().size() == 2u && !d.sketch.entitySelected(c);
    d.tap(SketchPoint{-8.0, 8.0});
    const bool cleared = d.sketch.selectedEntityIds().empty();
    r.check("DR_SES_01_select_multiple_adds_and_removes_and_empty_space_clears",
            ok && single && three && removed && cleared);

    // The construction act on a MIXED selection is one deterministic answer.
    d.sketch.toggleSelect(rect);
    d.sketch.toggleSelect(l);
    SketchEntityRole applied = SketchEntityRole::Regular;
    const bool made = d.sketch.toggleSelectionConstruction(&applied) == CadStatus::Ok
                      && applied == SketchEntityRole::Construction;
    const bool profilesGone = extractClosedProfiles(d.sketch.sketch()).profiles.size() == 1u;  // the circle
    const SketchOverlayPtr overlay = d.sketch.overlay(0.01f);
    const bool dashed = overlay->ranges.size() == kSketchOverlayRangesPerFrame
                        && overlay->ranges[5].style == SketchOverlayStyle::Construction
                        && overlay->ranges[5].vertexCount > 8u;
    applied = SketchEntityRole::Construction;
    const bool back = d.sketch.toggleSelectionConstruction(&applied) == CadStatus::Ok
                      && applied == SketchEntityRole::Regular
                      && extractClosedProfiles(d.sketch.sketch()).profiles.size() == 2u;
    r.check("DR_SES_02_make_construction_dashes_and_make_regular_restores_in_one_act",
            made && profilesGone && dashed && back);

    // A modify mode owns the tap: Trim removes the line's piece above the
    // rectangle, and a drawing tool ends the mode.
    d.sketch.clearSelection();
    d.sketch.setMultiSelect(false);
    const bool trimMode = d.sketch.setModifyMode(SketchModifyMode::Trim) == CadStatus::Ok;
    d.tap(SketchPoint{0.0, 2.0});
    const SketchEntity* trimmed = findSketchEntity(d.sketch.sketch(), l);
    const bool trimmedOk = trimmed != nullptr && trimmed->line() != nullptr
                           && samePoint(trimmed->line()->end, SketchPoint{0.0, 0.5})
                           && d.sketch.selectedEntityIds().empty();
    d.sketch.setTool(SketchTool::Line);
    r.check("DR_SES_03_a_trim_tap_means_trim_only_and_a_drawing_tool_ends_the_mode",
            trimMode && trimmedOk && d.sketch.modifyMode() == SketchModifyMode::None);

    // Dimension mode: a tap targets, the kinds are offered, one is added and
    // shows under Selected; Off hides it.
    const bool dimMode = d.sketch.setModifyMode(SketchModifyMode::Dimension) == CadStatus::Ok;
    d.tap(SketchPoint{4.5, 0.0});
    const std::vector<SketchDimensionKind> kinds = d.sketch.dimensionTargetKinds();
    SketchDimensionId diameter = 0;
    const bool added = kinds.size() == 2u && kinds[0] == SketchDimensionKind::CircleRadius
                       && d.sketch.addDimension(SketchDimensionKind::CircleDiameter,
                                                SketchDimensionMode::Driving, &diameter)
                                  == CadStatus::Ok;
    const bool shown = d.sketch.visibleDimensionAnnotations(0.01).size() == 1u;
    d.sketch.setDimensionVisibility(SketchDimensionVisibility::Off);
    const bool hidden = d.sketch.visibleDimensionAnnotations(0.01).empty();
    d.sketch.setDimensionVisibility(SketchDimensionVisibility::All);
    const bool edited = d.sketch.applyDimensionValue(diameter, 3.0) == CadStatus::Ok
                        && findSketchEntity(d.sketch.sketch(), c)->circle()->radius == 1.5;
    r.check("DR_SES_04_dimension_mode_targets_by_tap_and_visibility_selects_what_is_drawn",
            dimMode && added && shown && hidden && edited);

    // Offset: the source from the tap, the distance typed, Confirm creates.
    d.sketch.setTool(SketchTool::Select);
    const bool offsetMode = d.sketch.setModifyMode(SketchModifyMode::Offset) == CadStatus::Ok;
    d.tap(SketchPoint{5.5, 0.0});
    const size_t before = d.sketch.sketch().entities.size();
    std::vector<SketchEntityId> offsetIds;
    const bool offset = offsetMode && d.sketch.offsetSource() == c
                        && d.sketch.setOffsetDistance(0.25) == CadStatus::Ok
                        && d.sketch.offsetPreviewStatus() == CadStatus::Ok
                        && d.sketch.confirmOffset(&offsetIds) == CadStatus::Ok
                        && d.sketch.sketch().entities.size() == before + 1u
                        && d.sketch.modifyMode() == SketchModifyMode::None
                        && findSketchEntity(d.sketch.sketch(), offsetIds[0])->circle()->radius == 1.75;
    r.check("DR_SES_05_offset_confirm_creates_one_entity_and_cancel_creates_none",
            offset && d.sketch.setModifyMode(SketchModifyMode::Offset) == CadStatus::Ok
                    && d.sketch.setModifyMode(SketchModifyMode::None) == CadStatus::Ok
                    && d.sketch.sketch().entities.size() == before + 1u);

    // Mirror: needs a selection; the axis by tap; Confirm creates.
    d.sketch.clearSelection();
    const bool needsSelection =
            d.sketch.setModifyMode(SketchModifyMode::Mirror) == CadStatus::MirrorNothingSelected;
    d.sketch.select(c);
    const bool mirrorMode = d.sketch.setModifyMode(SketchModifyMode::Mirror) == CadStatus::Ok;
    d.tap(SketchPoint{0.0, -2.0});
    CadSketchEdgeRef axisRef;
    std::vector<SketchEntityId> mirrored;
    const bool mirrorOk = mirrorMode && d.sketch.mirrorAxis(&axisRef) && axisRef.entityId == l
                          && d.sketch.confirmMirror(&mirrored) == CadStatus::Ok && mirrored.size() == 1u
                          && findSketchEntity(d.sketch.sketch(), mirrored[0])->circle()->center.u == -4.0;
    r.check("DR_SES_06_mirror_needs_a_selection_takes_the_axis_by_tap_and_confirms_once",
            needsSelection && mirrorOk);

    // Delete with dimensions, through the session.
    d.sketch.select(c);
    const bool deleted = d.sketch.deleteSelected() == CadStatus::Ok
                         && d.sketch.lastDeletedDimensionCount() == 1u
                         && d.sketch.sketch().dimensions.empty();
    r.check("DR_SES_07_delete_removes_the_entitys_own_dimensions_and_reports_the_count", deleted);
    d.sketch.cancel();
}

// ---------------------------------------------------------------------------
// DR-FMT: CADB v8
// ---------------------------------------------------------------------------

void testFormat(Checks& r) {
    // FMT-01/02/03: a state with no drafting truth keeps every older writer.
    {
        CadSketch s;
        add(&s, rectangle(1.5, 0.5, 1.0, 1.0));
        add(&s, line(0.0, -1.0, 0.0, 2.0));
        RevolveFeature revolve;
        revolve.profileEntityId = 1;
        revolve.axis = CadSketchEdgeRef{2, 0};
        revolve.angleDegrees = 360.0;
        const std::vector<uint8_t> v7 = encodeProjectV1(documentFor(makeCadRevolveBodyState(s, revolve)));
        CadSketch plainSketch;
        add(&plainSketch, rectangle(0.0, 0.0, 2.0, 2.0));
        const CadBodyState plain = makeCadBodyState(plainSketch, profileExtrude(1));
        CadBodyState shared = plain;
        appendCadLaterFeature(&shared, CadFeatureOperation::Add, kBaseCadSketchId, profileExtrude(1));
        r.check("DR_FMT_01_a_pre_existing_fixture_encodes_byte_identically",
                sha(v7) == kRevolveFullV7Sha && !cadBodyStateUsesDrafting(plain)
                        && cadBodyStateLegacyRepresentable(plain));
        r.check("DR_FMT_02_a_plain_revolve_still_writes_exact_v7", cadVersionOf(v7) == kCadSectionVersionV7);
        r.check("DR_FMT_03_a_plain_v6_document_still_writes_exact_v6",
                cadVersionOf(encodeProjectV1Unchecked(documentFor(shared))) == kCadSectionVersionV6
                        && cadVersionOf(encodeProjectV1(documentFor(plain))) == kCadSectionVersion);
    }
    const auto roundTrips = [](const CadBodyState& state, const std::vector<uint8_t>& bytes) {
        ProjectDocument back;
        return !bytes.empty() && cadVersionOf(bytes) == kCadSectionVersionV8
               && decodeStatus(bytes, &back) == ProjectCodecStatus::Ok && back.cad.bodies.size() == 1u
               && sameCadBodyState(back.cad.bodies[0].state, state) && encodeProjectV1(back) == bytes;
    };
    r.check("DR_FMT_04_construction_roles_write_v8_and_round_trip",
            roundTrips(constructionFixtureState(), constructionFixtureBytes()));
    r.check("DR_FMT_05_driving_dimensions_write_v8_and_round_trip",
            roundTrips(drivingFixtureState(), drivingFixtureBytes()));
    r.check("DR_FMT_06_reference_dimensions_and_a_burned_id_round_trip",
            roundTrips(referenceFixtureState(), referenceFixtureBytes())
                    && cadBaseSketch(referenceFixtureState()).nextDimensionId == 9u);
    {
        ProjectDocument out;
        r.check("DR_FMT_07_a_dimension_naming_a_missing_entity_is_refused",
                decodeStatus(badRefFixtureBytes(), &out) == ProjectCodecStatus::InvalidSemanticValue
                        && out.cad.bodies.empty());
    }
    {
        CadBodyState dup = drivingFixtureState();
        cadBaseSketch(dup).dimensions[1].id = 1;
        const std::vector<uint8_t> bytes = encodeProjectV1Unchecked(documentFor(dup));
        CadBodyState low = drivingFixtureState();
        cadBaseSketch(low).nextDimensionId = 5;
        r.check("DR_FMT_08_a_duplicate_dimension_id_and_a_low_high_water_are_refused",
                decodeStatus(bytes) == ProjectCodecStatus::InvalidSemanticValue
                        && decodeStatus(encodeProjectV1Unchecked(documentFor(low)))
                                   == ProjectCodecStatus::InvalidSemanticValue);
    }
    r.check("DR_FMT_09_conflicting_driving_dimensions_are_refused",
            decodeStatus(conflictFixtureBytes()) == ProjectCodecStatus::InvalidSemanticValue
                    && validateCadBodyState(drivingFixtureState()) == CadStatus::Ok);
    {
        // Payload offsets: body count 4, object id 8, two marks 8, sketch count
        // 4, sketch id 4, placement 1, plane 1, nextEntityId 4, entity count 4,
        // then entity 0: id 4, kind 1, ROLE.
        const size_t roleOffset = 4 + 8 + 8 + 4 + 4 + 1 + 1 + 4 + 4 + 4 + 1;
        // Driving fixture: rectangle 38, circle 30, line 38 bytes of entities,
        // then nextDimensionId 4, count 4, dimension 0: id 4, KIND.
        const size_t kindOffset = 4 + 8 + 8 + 4 + 4 + 1 + 1 + 4 + 4 + 106 + 4 + 4 + 4;
        const size_t modeOffset = kindOffset + 1;
        r.check("DR_FMT_10_an_unknown_role_kind_or_mode_code_is_refused_by_name",
                decodeStatus(patchedCadByte(constructionFixtureBytes(), roleOffset, 7))
                                == ProjectCodecStatus::InvalidSemanticValue
                        && decodeStatus(patchedCadByte(drivingFixtureBytes(), kindOffset, 99))
                                   == ProjectCodecStatus::InvalidSemanticValue
                        && decodeStatus(patchedCadByte(drivingFixtureBytes(), modeOffset, 3))
                                   == ProjectCodecStatus::InvalidSemanticValue
                        && decodeStatus(patchedCadByte(drivingFixtureBytes(), kindOffset, 1))
                                   == ProjectCodecStatus::InvalidSemanticValue);
    }
    r.check("DR_FMT_11_every_v8_fixture_matches_the_independent_corpus_digest",
            sha(constructionFixtureBytes()) == kConstructionFixtureSha
                    && sha(drivingFixtureBytes()) == kDrivingFixtureSha
                    && sha(referenceFixtureBytes()) == kReferenceFixtureSha
                    && sha(badRefFixtureBytes()) == kBadRefFixtureSha
                    && sha(conflictFixtureBytes()) == kConflictFixtureSha);
    {
        // The fingerprint moves with a role and with a dimension, and a state
        // with neither keeps the one it always had.
        ConstructionScene a{NoProjectTag{}};
        ConstructionScene b{NoProjectTag{}};
        ConstructionScene c{NoProjectTag{}};
        ConstructionHistory ha(a);
        ConstructionHistory hb(b);
        ConstructionHistory hc(c);
        SculptSession sculpt;
        CadBodyState plainState = constructionFixtureState();
        for (SketchEntity& e : cadBaseSketch(plainState).entities) e.setRole(SketchEntityRole::Regular);
        const bool loaded =
                loadProjectDocument(documentFor(constructionFixtureState()), a, sculpt, ha) == ProjectCodecStatus::Ok
                && loadProjectDocument(documentFor(drivingFixtureState()), b, sculpt, hb) == ProjectCodecStatus::Ok;
        CadBodyState noDims = drivingFixtureState();
        cadBaseSketch(noDims).dimensions.pop_back();
        const bool loadedC = loadProjectDocument(documentFor(noDims), c, sculpt, hc) == ProjectCodecStatus::Ok;
        r.check("DR_FMT_12_the_fingerprint_moves_with_roles_and_dimensions",
                loaded && loadedC
                        && projectSemanticFingerprint(b, ProjectKind::Construction)
                                   != projectSemanticFingerprint(c, ProjectKind::Construction)
                        && cadVersionOf(encodeProjectV1(documentFor(plainState))) != kCadSectionVersionV8);
    }
}

// ---------------------------------------------------------------------------
// Performance (bounded single shots; no startup loop)
// ---------------------------------------------------------------------------

std::string measure() {
    char buffer[512];
    // 100 entities: 50 horizontal and 50 vertical lines crossing.
    CadSketch grid;
    for (int i = 0; i < 50; ++i) {
        add(&grid, line(0.0, 0.1 * i, 6.0, 0.1 * i));
        add(&grid, line(0.1 * i + 0.05, -0.5, 0.1 * i + 0.05, 5.5));
    }
    auto t0 = std::chrono::steady_clock::now();
    const SketchSnapCandidates candidates = collectSketchSnapCandidates(grid);
    const double collect = microsSince(t0);
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        snapSketchPoint(candidates, SketchPoint{2.03 + 0.001 * i, 1.97}, 0.05, 0.25, {}, nullptr);
    }
    const double query = microsSince(t0) / 100.0;
    t0 = std::chrono::steady_clock::now();
    SketchTrimPlan trim;
    planSketchTrim(grid, SketchPoint{2.0, 0.1}, 0.02, &trim);
    const double trimUs = microsSince(t0);
    const SketchArrangement shared = deriveSketchArrangement(cadSketchAllCurvesView(grid));
    t0 = std::chrono::steady_clock::now();
    SketchTrimPlan cachedTrim;
    planSketchTrim(grid, SketchPoint{2.0, 0.1}, 0.02, &cachedTrim, &shared);
    const double trimCachedUs = microsSince(t0);
    CadSketch extendSketch = grid;
    const SketchEntityId stub = add(&extendSketch, line(-2.0, 2.53, -1.0, 2.53));
    t0 = std::chrono::steady_clock::now();
    SketchExtendPlan extendPlan;
    const CadStatus extendWhy = planSketchExtend(extendSketch, SketchPoint{-1.01, 2.53}, 0.02, &extendPlan);
    const double extendUs = microsSince(t0);
    CadSketch mirror;
    add(&mirror, line(-1.0, -10.0, -1.0, 10.0));
    for (int i = 0; i < 99; ++i) add(&mirror, circle(0.2 * i, 0.0, 0.05));
    std::vector<SketchEntityId> mirrorIds;
    for (const SketchEntity& e : mirror.entities) mirrorIds.push_back(e.id());
    t0 = std::chrono::steady_clock::now();
    std::vector<SketchMirrorPiece> pieces;
    mirrorSketchEntities(mirror, mirrorIds, CadSketchEdgeRef{1, 0}, &pieces);
    const double mirrorUs = microsSince(t0);
    std::vector<SketchPoint> zig;
    for (int i = 0; i <= 100; ++i) zig.push_back(SketchPoint{0.1 * i, (i % 2) * 0.1});
    std::vector<SketchEntity::Payload> offsetOut;
    t0 = std::chrono::steady_clock::now();
    offsetSketchEntity(SketchEntity(1u, polyline(zig)), 0.01, &offsetOut);
    const double offsetUs = microsSince(t0);
    CadSketch dims;
    for (int i = 0; i < 100; ++i) {
        const SketchEntityId l = add(&dims, line(0.0, 0.2 * i, 1.0 + 0.01 * i, 0.2 * i));
        addDim(&dims, SketchDimensionKind::LineLength, SketchDimensionMode::Reference, {l, 0});
    }
    t0 = std::chrono::steady_clock::now();
    size_t segments = 0;
    for (const SketchDimension& d : dims.dimensions) {
        SketchDimensionAnnotation a;
        if (buildSketchDimensionAnnotation(dims, d, 0.01, &a)) segments += a.segments.size();
    }
    const double dimsUs = microsSince(t0);
    // Near the arrangement cap: 64 x 64 crossing lines (4096 crossings).
    CadSketch cap;
    for (int i = 0; i < 64; ++i) {
        add(&cap, line(0.0, 0.1 * i, 7.0, 0.1 * i));
        add(&cap, line(0.1 * i + 0.05, -0.5, 0.1 * i + 0.05, 7.0));
    }
    t0 = std::chrono::steady_clock::now();
    const SketchSnapCandidates capCandidates = collectSketchSnapCandidates(cap);
    const double capCollect = microsSince(t0);
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        snapSketchPoint(capCandidates, SketchPoint{3.03 + 0.001 * i, 2.97}, 0.05, 0.25, {}, nullptr);
    }
    const double capQuery = microsSince(t0) / 100.0;
    std::snprintf(buffer, sizeof(buffer),
                  "drafting_snap100_collect_us=%.0f drafting_snap100_query_us=%.1f "
                  "drafting_snap_cap_collect_us=%.0f drafting_snap_cap_query_us=%.1f "
                  "drafting_snap_cap_intersections=%zu drafting_trim100_us=%.0f "
                  "drafting_trim100_cached_us=%.0f drafting_extend100_us=%.0f extend_ok=%d "
                  "drafting_mirror100_us=%.0f drafting_offset100_us=%.0f "
                  "drafting_dims100_us=%.0f drafting_dims100_segments=%zu",
                  collect, query, capCollect, capQuery, capCandidates.intersections.size(), trimUs,
                  trimCachedUs, extendUs,
                  extendWhy == CadStatus::Ok && extendPlan.target == stub ? 1 : 0, mirrorUs,
                  offsetUs, dimsUs, segments);
    return std::string(buffer);
}

}  // namespace

std::string sketchDraftingFixtureDigests() {
    return std::string("cad_construction_v8=") + sha(constructionFixtureBytes())
           + " cad_dimension_driving_v8=" + sha(drivingFixtureBytes())
           + " cad_dimension_reference_v8=" + sha(referenceFixtureBytes())
           + " cad_bad_dimension_ref_v8=" + sha(badRefFixtureBytes())
           + " cad_dimension_conflict_v8=" + sha(conflictFixtureBytes());
}

void runSketchDraftingSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                                std::string* performance) {
    if (out == nullptr) {
        return;
    }
    Checks r{out};
    testConstruction(r);
    testDimensions(r);
    testSnaps(r);
    testTrim(r);
    testExtend(r);
    testOffset(r);
    testMirror(r);
    testSession(r);
    testFormat(r);
    if (performance != nullptr) {
        *performance = measure();
    }
}

}  // namespace forgeshape
