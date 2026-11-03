#include "forgeshape_cad_plane_selftest.h"

#include "forgeshape_cad_hole.h"

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
bool farCap(const ConstructionScene& scene, ObjectId id, SketchFrame* out, TopoRef* ref,
            uint32_t featureId = kCadFeatureId) {
    const SceneObject* object = scene.findBody(id);
    const CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr) return false;
    CadFaceToken token;
    token.kind = CadFaceKind::CapFar;
    CadFace face;
    Mat4 world;
    if (resolveCadFeatureFace(body->state(), featureId, token, &face) != CadStatus::Ok
        || !scene.resolveWorldModel(id, &world)) {
        return false;
    }
    const Mat4 fw = mat4Multiply(world, cadFaceFrameMatrix(face));
    out->origin = mat4TransformPoint(fw, Vec3{0.0f, 0.0f, 0.0f});
    out->u = vec3Normalize(mat4TransformDirection(fw, Vec3{1.0f, 0.0f, 0.0f}));
    out->v = vec3Normalize(mat4TransformDirection(fw, Vec3{0.0f, 1.0f, 0.0f}));
    out->n = vec3Normalize(mat4TransformDirection(fw, Vec3{0.0f, 0.0f, 1.0f}));
    ref->producerObjectId = id;
    ref->producerLocalFeatureId = featureId;
    ref->face = token;
    ref->lineageToken = cadFeatureTopologySignature(body->state(), featureId);
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



uint64_t fingerprintOf(const CadBodyState& state) {
    ConstructionScene scene{NoProjectTag{}};
    SceneObject* added = scene.addCadBody(state);
    return added != nullptr ? projectSemanticFingerprint(scene, ProjectKind::Construction) : 0u;
}

// A 4 x 4 x `height` block and a round cut (r 0.3) drawn on a plane `offset`
// above its top, tilted `angle`, cutting `depth` along `direction`.
CadBodyState offsetCut(double height, double offset, double angle, ExtrudeFeature extrude,
                       double cu = 0.0, double cv = 0.0) {
    CadBodyState state;
    cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
    state.extrude = oneSide(height);
    const uint32_t id = appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut,
                                                        topOf(state, kCadFeatureId),
                                                        circleSketch(cu, cv, 0.3), extrude);
    CadSketchRecord* record = cadFeatureSketchRecord(state, id);
    if (record != nullptr) {
        record->planeOffset = offset;
        record->planeAngleDegrees = angle;
    }
    return state;
}

}  // namespace

void runCadPlaneSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    if (out == nullptr) return;
    Checks r{out};

    // --- PL_01..02: a plane off the face ------------------------------------------
    r.check("PL_01_a_cut_from_a_plane_above_the_face_starts_there",
            volumeIs(regen(offsetCut(1.0, 0.5, 0.0, oneSide(1.0, ExtrudeDirection::AgainstNormal))),
                     16.0 - disk(0.3) * 0.5));
    r.check("PL_01_an_upstream_size_edit_carries_the_plane",
            volumeIs(regen(offsetCut(2.0, 0.5, 0.0, oneSide(1.0, ExtrudeDirection::AgainstNormal))),
                     32.0 - disk(0.3) * 0.5));
    {
        // Tilted 90 degrees: a vertical plane through the top face's centre
        // line; a symmetric 0.1 m cut of a circle centred ON the top edge of
        // that plane removes exactly half a 0.2 m cylinder.
        ExtrudeFeature symmetric = oneSide(0.1);
        symmetric.extent = ExtrudeExtentMode::Symmetric;
        r.check("PL_02_a_plane_tilted_off_the_face_turns_the_sketch_with_it",
                volumeIs(regen(offsetCut(1.0, 0.0, 90.0, symmetric)),
                         16.0 - disk(0.3) * 0.5 * 0.2));
        CadFrame64 face;
        const CadFrame64 tilted = cadSketchPlaneFrame(face, 0.25, 90.0);
        const DVec3 cross{tilted.u.y * tilted.v.z - tilted.u.z * tilted.v.y,
                          tilted.u.z * tilted.v.x - tilted.u.x * tilted.v.z,
                          tilted.u.x * tilted.v.y - tilted.u.y * tilted.v.x};
        r.check("PL_02_the_plane_frame_stays_right_handed_and_unit",
                near(cross.x, tilted.n.x, 1e-12) && near(cross.y, tilted.n.y, 1e-12)
                        && near(cross.z, tilted.n.z, 1e-12) && near(tilted.origin.z, 0.25, 1e-12)
                        && near(tilted.n.y, -1.0, 1e-12)
                        && sameCadBodyState(offsetCut(1.0, 0.0, 0.0, oneSide(0.5)),
                                            offsetCut(1.0, 0.0, 0.0, oneSide(0.5))));
    }

    // --- PL_03: refusals by name ---------------------------------------------------
    {
        CadBodyState root;
        cadBaseSketch(root) = rectSketch(0.0, 0.0, 4.0, 4.0);
        root.extrude = oneSide(1.0);
        root.sketches.front().planeOffset = 0.5;
        r.check("PL_03_a_root_sketch_stands_on_its_plane",
                validateCadBodyState(root) == CadStatus::SketchPlaneInvalid);
        r.check("PL_03_a_tilt_past_ninety_degrees_is_refused",
                validateCadBodyState(offsetCut(1.0, 0.0, 91.0, oneSide(0.5)))
                        == CadStatus::SketchPlaneInvalid);
        r.check("PL_03_a_non_finite_offset_is_refused",
                validateCadBodyState(offsetCut(1.0, std::nan(""), 0.0, oneSide(0.5)))
                        == CadStatus::SketchPlaneInvalid);
        // The face under the plane must still carry material.
        r.check("PL_03_the_plane_is_still_measured_from_its_face",
                regen(offsetCut(1.0, 0.5, 0.0, oneSide(1.0, ExtrudeDirection::AgainstNormal))).why
                        == CadStatus::Ok);
    }

    // --- PL_04: CADB v14 and the fingerprint ---------------------------------------
    {
        const CadBodyState state =
                offsetCut(1.0, 0.5, 15.0, oneSide(1.0, ExtrudeDirection::AgainstNormal));
        const CadBodyState plain =
                offsetCut(1.0, 0.0, 0.0, oneSide(1.0, ExtrudeDirection::AgainstNormal));
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state), &why);
        const std::vector<uint8_t> plainBytes = encodeProjectV1(documentFor(plain), &why);
        uint16_t version = 0, plainVersion = 0;
        uint64_t length = 0, plainLength = 0;
        const size_t base = cadPayloadAt(bytes, &version, &length);
        cadPayloadAt(plainBytes, &plainVersion, &plainLength);
        r.check("PL_04_a_sketch_off_its_face_is_written_cadb_v14_and_only_it",
                base != 0 && version == kCadSectionVersionV14 && plainVersion < kCadSectionVersionV9);
        ProjectDocument decoded;
        r.check("PL_04_v14_round_trips_the_plane_exactly",
                decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                        && sameCadBodyState(decoded.cad.bodies[0].state, state));
        std::vector<uint8_t> older = bytes;
        if (base != 0) {
            const size_t header = base - kForgeSectionHeaderBytes;
            older[header + 4] = static_cast<uint8_t>(kCadSectionVersionV13 & 0xFF);
            older[header + 5] = static_cast<uint8_t>(kCadSectionVersionV13 >> 8);
        }
        ProjectDocument scratch;
        r.check("PL_04_the_same_section_declared_v13_is_refused",
                base != 0 && decodeProject(older.data(), older.size(), &scratch)
                                     != ProjectCodecStatus::Ok);
        r.check("PL_04_the_fingerprint_reads_the_plane",
                fingerprintOf(state) != 0u && fingerprintOf(state) != fingerprintOf(plain)
                        && fingerprintOf(state) == fingerprintOf(state));
    }

    // --- PL_05: the session ---------------------------------------------------------
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
        const bool began = block != kNoObject && farCap(scene, block, &frame, &ref)
                           && d.beginFace(frame, ref, &staged);
        const bool available = began && d.sketch.facePlaneAvailable();
        const CadStatus moved = d.sketch.setFacePlane(0.5, 0.0);
        d.frameCamera();
        r.check("PL_05_a_face_sketch_may_stand_off_its_face_and_the_frame_moves_with_it",
                available && moved == CadStatus::Ok
                        && near(vec3Dot(vec3Sub(d.sketch.frame().origin, frame.origin), frame.n), 0.5,
                                1e-5)
                        && d.sketch.operation() != CadFeatureOperation::NewBody
                        && !d.sketch.operationAvailable(CadFeatureOperation::NewBody));
        r.check("PL_05_values_out_of_range_are_refused_not_clamped",
                d.sketch.setFacePlane(0.5, 120.0) == CadStatus::SketchPlaneInvalid
                        && near(d.sketch.facePlaneOffset(), 0.5) && d.sketch.facePlaneAngle() == 0.0);
        // Dragged far enough to be a drag on screen, then typed exactly.
        const bool placed = d.place(SketchTool::Circle, SketchPoint{0.0, 0.0}, SketchPoint{0.6, 0.0},
                                    circleAt(0.0, 0.0, 0.2))
                            != kNoSketchEntity;
        const CadStatus finished = d.sketch.finish();
        const CadStatus op = d.sketch.setOperation(CadFeatureOperation::Cut);
        const CadStatus ex = d.sketch.setExtrude(1.0, d.sketch.extrude().direction);
        const bool drawn = placed && finished == CadStatus::Ok && op == CadStatus::Ok
                           && ex == CadStatus::Ok;
        ObjectId id = kNoObject;
        const CadStatus committed = drawn ? d.sketch.commit(scene, history, &id)
                                          : CadStatus::RegenerationFailed;
        std::shared_ptr<const CadBodyMesh> mesh;
        const CadBody* body = block != kNoObject ? scene.findBody(block)->cadOrNull() : nullptr;
        const CadSketchRecord* record =
                body != nullptr && !body->state().laterFeatures.empty()
                        ? cadFeatureSketchRecord(body->state(),
                                                 body->state().laterFeatures.back().featureId)
                        : nullptr;
        r.check("PL_05_the_commit_writes_the_plane_and_cuts_from_it",
                committed == CadStatus::Ok && body->regenerated(&mesh) == CadStatus::Ok
                        && near(mesh->volume, 4.0 - circleArea(0.2) * 0.5, 1e-6) && record != nullptr
                        && near(record->planeOffset, 0.5) && record->planeAngleDegrees == 0.0);
    }
    if (performance != nullptr) {
        char line[96];
        std::snprintf(line, sizeof(line), "plane_session_us=%.0f",
                      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                              .count());
        *performance = line;
    }
}

}  // namespace forgeshape
