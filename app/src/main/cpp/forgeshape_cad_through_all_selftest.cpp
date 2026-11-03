#include "forgeshape_cad_through_all_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_extrude_tool.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_camera.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool passed) { out->push_back(ArrangementSelfTestCheck{name, passed}); }
};

bool near(double a, double b, double tolerance = 1e-9) { return std::fabs(a - b) <= tolerance; }

double disk(double r) { return 0.5 * r * r * 32.0 * std::sin(2.0 * kPi / 32.0); }

ExtrudeFeature oneSide(double depth, ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = 1;
    extrude.depth = depth;
    extrude.direction = direction;
    return extrude;
}

CadSketch rectSketch(double cu, double cv, double w, double h) {
    CadSketch sketch;
    SketchRectangle r;
    r.center = SketchPoint{cu, cv};
    r.width = w;
    r.height = h;
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(&sketch, r, &id);
    return sketch;
}

CadSketch circleSketch(double cu, double cv, double radius) {
    CadSketch sketch;
    SketchCircle c;
    c.center = SketchPoint{cu, cv};
    c.radius = radius;
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(&sketch, c, &id);
    return sketch;
}


// A 4 x 4 x `height` block on XY: x, y in [-2, 2], z in [0, height].
CadBodyState blockState(double height = 1.0) {
    CadBodyState state;
    cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
    state.extrude = oneSide(height);
    return state;
}

CadFeatureSupport supportOn(const CadBodyState& state, uint32_t featureId, CadFaceToken token) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face = token;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

CadFeatureSupport topOf(const CadBodyState& state, uint32_t featureId) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face.kind = CadFaceKind::CapFar;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

ExtrudeFeature throughAll(ExtrudeDirection direction) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = 1;
    extrude.extent = ExtrudeExtentMode::ThroughAll;
    extrude.depth = 0.0;
    extrude.direction = direction;
    return extrude;
}

// The 4 x 4 x `height` block with a Through All round cut (r 0.3) from its top.
CadBodyState drilledBlock(double height, ExtrudeDirection direction = ExtrudeDirection::AgainstNormal) {
    CadBodyState state;
    cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
    state.extrude = oneSide(height);
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut, topOf(state, kCadFeatureId),
                                    circleSketch(0.0, 0.0, 0.3), throughAll(direction));
    return state;
}

struct Regen {
    CadStatus why = CadStatus::RegenerationFailed;
    uint32_t failed = 0;
    CadBodyMesh mesh;
};

Regen regen(const CadBodyState& state) {
    Regen g;
    CadRegenerationReport report;
    g.why = validateCadBodyState(state);
    if (g.why == CadStatus::Ok) {
        g.why = regenerateCadBody(state, &g.mesh, &report);
        g.failed = report.failedFeatureId;
    }
    return g;
}

bool volumeIs(const Regen& g, double volume) {
    return g.why == CadStatus::Ok && near(g.mesh.volume, volume, 1e-7);
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

size_t cadPayloadAt(const std::vector<uint8_t>& bytes, uint16_t* version, uint64_t* length) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            *version = static_cast<uint16_t>(bytes[offset + 4] | (bytes[offset + 5] << 8));
            *length = payloadBytes;
            return offset + kForgeSectionHeaderBytes;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
    return 0;
}


// One session, one camera framed on it, real Down / Move / Up samples.
struct Driver {
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
        if (sketch.begin(plane) != CadStatus::Ok) return false;
        frameCamera();
        return true;
    }
    bool beginFace(const SketchFrame& frame, const TopoRef& ref, const CadBodyState* producer) {
        if (sketch.beginOnFace(frame, ref, producer) != CadStatus::Ok) return false;
        frameCamera();
        return true;
    }
    bool at(TouchAction action, float x, float y, int32_t id = 7) {
        TouchPointer p{id, x, y};
        return sketch.onTouch(action, action == TouchAction::Up ? id : -1, &p, 1, camera.snapshot(),
                              kW, kH);
    }
    bool screenOf(const SketchPoint& point, float* x, float* y) {
        return sketch.sketchToScreen(camera.snapshot(), point, kW, kH, x, y);
    }
    bool worldToScreen(const Vec3& world, float* x, float* y) {
        return projectWorldToScreen(camera.snapshot(), world, kW, kH, x, y);
    }
    // A stroke through sketch points, sampled finely enough to travel.
    bool stroke(const std::vector<SketchPoint>& points) {
        std::vector<std::pair<float, float>> screen;
        for (const SketchPoint& p : points) {
            float x = 0.0f;
            float y = 0.0f;
            if (!screenOf(p, &x, &y)) return false;
            screen.emplace_back(x, y);
        }
        bool ok = at(TouchAction::Down, screen[0].first, screen[0].second);
        for (size_t i = 1; i < screen.size(); ++i) {
            for (int k = 1; k <= 8; ++k) {
                const float t = k / 8.0f;
                ok &= at(TouchAction::Move,
                         screen[i - 1].first + (screen[i].first - screen[i - 1].first) * t,
                         screen[i - 1].second + (screen[i].second - screen[i - 1].second) * t);
            }
        }
        ok &= at(TouchAction::Up, screen.back().first, screen.back().second);
        return ok;
    }
    bool tap(const SketchPoint& p) {
        float x = 0.0f;
        float y = 0.0f;
        if (!screenOf(p, &x, &y)) return false;
        at(TouchAction::Down, x, y);
        return at(TouchAction::Up, x, y);
    }
    // Draws one entity through the gesture path, then TYPES its exact geometry.
    SketchEntityId place(SketchTool tool, const SketchPoint& from, const SketchPoint& to,
                         SketchEntity::Payload exact) {
        sketch.setTool(tool);
        const size_t before = sketch.sketch().entities.size();
        if (!stroke({from, to}) || sketch.sketch().entities.size() != before + 1u) {
            return kNoSketchEntity;
        }
        const SketchEntityId id = sketch.sketch().entities.back().id();
        return sketch.replaceEntity(id, std::move(exact)) == CadStatus::Ok ? id : kNoSketchEntity;
    }
    // The oblique view Finish leads to (`cadFeatureViewPose`, the product rule).
    bool tilt() {
        CadExtrudeAnchors anchors;
        CameraController::Pose pose;
        if (!sketch.extrudeAnchors(&anchors)
            || cadFeatureViewPose(camera.capturePose(), nullptr, sketch.frame(), anchors, &pose)
                       == CadFeatureViewSource::Unavailable) {
            return false;
        }
        camera.restorePose(pose);
        return true;
    }
    // Grabs the primary arrow near its tip and drags it to the axis point
    // `signedTarget` metres from the base along the axis it had at the Down.
    // Ends with Up unless `keepDown`.
    bool dragArrowTo(double signedTarget, bool keepDown = false) {
        CadExtrudeAnchors anchors;
        if (!sketch.extrudeAnchors(&anchors)) return false;
        const Vec3 grab = vec3Add(anchors.base, vec3Scale(anchors.axis,
                                                          static_cast<float>(anchors.depth * 0.97)));
        // The drag is RELATIVE (the depth moves by what the finger moved along
        // the axis), so the target is offset by where the grab stood.
        const Vec3 target = vec3Add(
                anchors.base,
                vec3Scale(anchors.axis, static_cast<float>(signedTarget - anchors.depth * 0.03)));
        float gx = 0.0f, gy = 0.0f, tx = 0.0f, ty = 0.0f;
        if (!worldToScreen(grab, &gx, &gy) || !worldToScreen(target, &tx, &ty)) return false;
        at(TouchAction::Down, gx, gy);
        if (!sketch.extrudeManipulator().capturing()) return false;
        for (int k = 1; k <= 12; ++k) {
            const float t = k / 12.0f;
            at(TouchAction::Move, gx + (tx - gx) * t, gy + (ty - gy) * t);
        }
        if (!keepDown) at(TouchAction::Up, tx, ty);
        return true;
    }
};

SketchRectangle rectangleAt(double cu, double cv, double w, double h) {
    SketchRectangle r;
    r.center = SketchPoint{cu, cv};
    r.width = w;
    r.height = h;
    return r;
}

SketchCircle circleAt(double cu, double cv, double radius) {
    SketchCircle c;
    c.center = SketchPoint{cu, cv};
    c.radius = radius;
    return c;
}

double circleArea(double radius) {
    return 0.5 * polygonSignedAreaTwice(circleProfilePolygon(circleAt(0.0, 0.0, radius)));
}

double bodyVolume(const ConstructionScene& scene, ObjectId id) {
    const SceneObject* object = scene.findBody(id);
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    std::shared_ptr<const CadBodyMesh> mesh;
    if (body == nullptr || body->regenerated(&mesh) != CadStatus::Ok || mesh == nullptr) return -1.0;
    return mesh->volume;
}

// The far cap of a body's first feature, in world space, and its reference.
bool farCap(const ConstructionScene& scene, ObjectId id, SketchFrame* out, TopoRef* ref) {
    const SceneObject* object = scene.findBody(id);
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr) return false;
    CadFaceToken token;
    token.kind = CadFaceKind::CapFar;
    CadFace face;
    Mat4 world;
    if (resolveCadFeatureFace(body->state(), kCadFeatureId, token, &face) != CadStatus::Ok
        || !scene.resolveWorldModel(id, &world)) {
        return false;
    }
    const Mat4 fw = mat4Multiply(world, cadFaceFrameMatrix(face));
    out->origin = mat4TransformPoint(fw, Vec3{0.0f, 0.0f, 0.0f});
    out->u = vec3Normalize(mat4TransformDirection(fw, Vec3{1.0f, 0.0f, 0.0f}));
    out->v = vec3Normalize(mat4TransformDirection(fw, Vec3{0.0f, 1.0f, 0.0f}));
    out->n = vec3Normalize(mat4TransformDirection(fw, Vec3{0.0f, 0.0f, 1.0f}));
    ref->producerObjectId = id;
    ref->producerLocalFeatureId = kCadFeatureId;
    ref->face = token;
    ref->lineageToken = cadFeatureTopologySignature(body->state(), kCadFeatureId);
    return true;
}

// A 2 x 2 x 1 block committed through the real session.
ObjectId makeBlock(ConstructionScene& scene, ConstructionHistory& history) {
    Driver d;
    if (!d.beginWorld(Workplane::XY)
        || d.place(SketchTool::Rectangle, SketchPoint{-0.5, -0.5}, SketchPoint{0.5, 0.5},
                   rectangleAt(0.0, 0.0, 2.0, 2.0))
                   == kNoSketchEntity
        || d.sketch.finish() != CadStatus::Ok
        || d.sketch.setExtrude(1.0, ExtrudeDirection::AlongNormal) != CadStatus::Ok) {
        return kNoObject;
    }
    ObjectId id = kNoObject;
    return d.sketch.commit(scene, history, &id) == CadStatus::Ok ? id : kNoObject;
}

}  // namespace

void runCadThroughAllSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    if (out == nullptr) return;
    Checks r{out};

    // --- TA_01..03: the derived distance --------------------------------------
    r.check("TA_01_a_through_all_cut_goes_right_through_the_part",
            volumeIs(regen(drilledBlock(1.0)), 16.0 - disk(0.3)));
    r.check("TA_02_through_all_follows_an_upstream_size_edit",
            volumeIs(regen(drilledBlock(2.5)), 40.0 - disk(0.3) * 2.5));
    {
        // A boss first, then a Through All cut from the boss's top: through
        // the boss AND the block under it.
        CadBodyState state;
        cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
        state.extrude = oneSide(1.0);
        appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Add, topOf(state, kCadFeatureId),
                                        rectSketch(0.0, 0.0, 0.8, 0.8), oneSide(0.5));
        appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut, topOf(state, 2u),
                                        circleSketch(0.0, 0.0, 0.3),
                                        throughAll(ExtrudeDirection::AgainstNormal));
        r.check("TA_03_through_all_from_a_boss_reaches_through_everything_below",
                volumeIs(regen(state), 16.0 + 0.32 - disk(0.3) * 1.5));
    }

    // --- TA_04..05: refusals by name ------------------------------------------
    {
        CadBodyState base;
        cadBaseSketch(base) = rectSketch(0.0, 0.0, 4.0, 4.0);
        base.extrude = throughAll(ExtrudeDirection::AlongNormal);
        CadBodyState add;
        cadBaseSketch(add) = rectSketch(0.0, 0.0, 4.0, 4.0);
        add.extrude = oneSide(1.0);
        appendCadLaterFeatureWithSketch(&add, CadFeatureOperation::Add, topOf(add, kCadFeatureId),
                                        circleSketch(0.0, 0.0, 0.3),
                                        throughAll(ExtrudeDirection::AlongNormal));
        CadBodyState stored = drilledBlock(1.0);
        stored.laterFeatures[0].extrude.depth = 0.5;  // Through All stores no depth
        r.check("TA_04_the_base_feature_cannot_go_through_all",
                validateCadBodyState(base) == CadStatus::ThroughAllUnsupported);
        r.check("TA_04_an_add_cannot_go_through_all",
                validateCadBodyState(add) == CadStatus::ThroughAllUnsupported);
        r.check("TA_04_through_all_stores_no_depth",
                validateCadBodyState(stored) == CadStatus::InvalidExtrudeExtent);
        const Regen outward = regen(drilledBlock(1.0, ExtrudeDirection::AlongNormal));
        r.check("TA_05_a_cut_pointing_out_of_the_part_reaches_nothing_and_says_so",
                outward.why == CadStatus::ThroughAllReachesNothing);
        // The pure transitions: into Through All no distance survives, out of
        // it the resolved distance does, and a typed depth makes it One Side.
        const ExtrudeFeature in =
                extrudeFeatureWithExtent(oneSide(0.4, ExtrudeDirection::AgainstNormal),
                                         ExtrudeExtentMode::ThroughAll, ExtrudeDirection::AlongNormal);
        const ExtrudeFeature typed =
                extrudeFeatureWithPrimary(in, 0.7, ExtrudeDirection::AgainstNormal);
        // Every mode crosses the index boundary (JNI, the History) and back.
        bool indices = true;
        for (int i = 0; i < kExtrudeExtentModeCount; ++i) {
            ExtrudeExtentMode mode = ExtrudeExtentMode::OneSide;
            indices = indices && extrudeExtentModeFromIndex(i, &mode)
                      && extrudeExtentModeIndex(mode) == i;
        }
        ExtrudeExtentMode none = ExtrudeExtentMode::OneSide;
        r.check("TA_05_every_extent_mode_round_trips_its_index",
                indices && !extrudeExtentModeFromIndex(kExtrudeExtentModeCount, &none));
        r.check("TA_05_transitions_keep_the_side_and_never_invent_a_distance",
                in.extent == ExtrudeExtentMode::ThroughAll && in.depth == 0.0
                        && in.direction == ExtrudeDirection::AgainstNormal
                        && extrudeFeatureCanonical(in) && typed.extent == ExtrudeExtentMode::OneSide
                        && typed.depth == 0.7 && extrudePositiveDistance(in) == 0.0);
    }

    // --- TA_06: CADB v12 ----------------------------------------------------------
    {
        const CadBodyState state = drilledBlock(1.0);
        CadBodyState plain = state;
        plain.laterFeatures[0].extrude = oneSide(1.01, ExtrudeDirection::AgainstNormal);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state), &why);
        const std::vector<uint8_t> plainBytes = encodeProjectV1(documentFor(plain), &why);
        uint16_t version = 0, plainVersion = 0;
        uint64_t length = 0, plainLength = 0;
        const size_t base = cadPayloadAt(bytes, &version, &length);
        cadPayloadAt(plainBytes, &plainVersion, &plainLength);
        r.check("TA_06_a_through_all_body_is_written_cadb_v12_and_only_it",
                base != 0 && version == kCadSectionVersionV12 && plainVersion < kCadSectionVersionV9);
        ProjectDocument decoded;
        r.check("TA_06_v12_round_trips_the_extent_exactly",
                decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                        && decoded.cad.bodies.size() == 1u
                        && sameCadBodyState(decoded.cad.bodies[0].state, state));
        // The same bytes declaring v11 are refused: code 4 has no v11 meaning.
        std::vector<uint8_t> older = bytes;
        if (base != 0) {
            const size_t header = base - kForgeSectionHeaderBytes;
            older[header + 4] = static_cast<uint8_t>(kCadSectionVersionV11 & 0xFF);
            older[header + 5] = static_cast<uint8_t>(kCadSectionVersionV11 >> 8);
        }
        ProjectDocument scratch;
        r.check("TA_06_the_same_section_declared_v11_is_refused",
                base != 0 && decodeProject(older.data(), older.size(), &scratch)
                                     != ProjectCodecStatus::Ok);
    }

    // --- TA_07..09: the session -------------------------------------------------
    const auto t0 = std::chrono::steady_clock::now();
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        const ObjectId block = makeBlock(scene, history);
        const CadBodyState staged = block != kNoObject
                                            ? scene.findBody(block)->cadOrNull()->state()
                                            : CadBodyState{};
        Driver d;
        SketchFrame frame;
        TopoRef ref;
        const bool ready = block != kNoObject && farCap(scene, block, &frame, &ref)
                           && d.beginFace(frame, ref, &staged)
                           && d.place(SketchTool::Circle, SketchPoint{0.0, 0.0},
                                      SketchPoint{0.6, 0.0}, circleAt(0.0, 0.0, 0.3))
                                      != kNoSketchEntity
                           && d.sketch.finish() == CadStatus::Ok && d.tilt();
        r.check("TA_07_through_all_is_offered_only_once_the_operation_is_cut",
                ready && !d.sketch.throughAllAvailable()
                        && d.sketch.setExtrudeExtent(ExtrudeExtentMode::ThroughAll)
                                   == CadStatus::ThroughAllUnsupported
                        && d.sketch.setOperation(CadFeatureOperation::Cut) == CadStatus::Ok
                        && d.sketch.throughAllAvailable());
        const CadStatus set = d.sketch.setExtrudeExtent(ExtrudeExtentMode::ThroughAll);
        const ExtrudeFeature shown = d.sketch.displayExtrude();
        CadExtrudeAnchors anchors;
        r.check("TA_08_the_arrow_and_value_read_where_the_cut_reaches",
                set == CadStatus::Ok && extrudeIsThroughAll(d.sketch.extrude())
                        && shown.extent == ExtrudeExtentMode::OneSide && shown.depth > 1.0
                        && shown.depth < 1.05 && d.sketch.extrudeAnchors(&anchors)
                        && near(anchors.depth, shown.depth, 1e-4)
                        && d.sketch.evaluateCandidate().status == CadStatus::Ok);
        // Grabbing the arrow and dragging sets a depth; a second finger
        // gives Through All back.
        const bool grabbed = d.dragArrowTo(-0.5, true);
        const bool becameDepth = d.sketch.extrude().extent == ExtrudeExtentMode::OneSide;
        TouchPointer two[2] = {{7, 500.0f, 500.0f}, {8, 600.0f, 600.0f}};
        d.sketch.onTouch(TouchAction::PointerDown, 8, two, 2, d.camera.snapshot(), Driver::kW,
                         Driver::kH);
        r.check("TA_08_a_drag_sets_a_depth_and_a_second_finger_restores_through_all",
                grabbed && becameDepth && extrudeIsThroughAll(d.sketch.extrude()));
        // Leaving Cut keeps where it reached, as One Side.
        CadFeatureOperation before = d.sketch.operation();
        (void)before;
        ObjectId id = kNoObject;
        const CadStatus committed = d.sketch.commit(scene, history, &id);
        std::shared_ptr<const CadBodyMesh> mesh;
        const CadBody* body = scene.findBody(block)->cadOrNull();
        r.check("TA_09_commit_is_one_step_and_the_cut_goes_through",
                committed == CadStatus::Ok && body->regenerated(&mesh) == CadStatus::Ok
                        && near(mesh->volume, 4.0 - circleArea(0.3), 1e-7)
                        && extrudeIsThroughAll(body->state().laterFeatures.back().extrude));
    }
    if (performance != nullptr) {
        char line[96];
        std::snprintf(line, sizeof(line), "through_all_session_us=%.0f",
                      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                              .count());
        *performance = line;
    }
}

}  // namespace forgeshape
