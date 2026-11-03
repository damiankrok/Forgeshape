#include "forgeshape_cad_to_face_selftest.h"

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


ExtrudeFeature toFace(const CadFeatureSupport& target, ExtrudeDirection direction) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = 1;
    extrude.extent = ExtrudeExtentMode::ToFace;
    extrude.depth = 0.0;
    extrude.direction = direction;
    extrude.toFace = target;
    return extrude;
}

CadFeatureSupport bottomOf(const CadBodyState& state, uint32_t featureId) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face.kind = CadFaceKind::CapPlane;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

CadFaceToken sideToken(SketchEntityId entity, uint32_t index) {
    CadFaceToken token;
    token.kind = CadFaceKind::Side;
    token.edgeEntityId = entity;
    token.edgeLocalIndex = index;
    return token;
}

// A 4 x 4 x 1 block with a 0.8 x 0.8 post `postHeight` tall on its top, and a
// round boss (r 0.3, at (1.5, 1.5)) on the block's top up to the post's top.
CadBodyState postAndBoss(double postHeight) {
    CadBodyState state;
    cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
    state.extrude = oneSide(1.0);
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Add, topOf(state, kCadFeatureId),
                                    rectSketch(0.0, 0.0, 0.8, 0.8), oneSide(postHeight));
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Add, topOf(state, kCadFeatureId),
                                    circleSketch(1.5, 1.5, 0.3),
                                    toFace(topOf(state, 2u), ExtrudeDirection::AlongNormal));
    return state;
}

// A 4 x 4 x 2 block, a 2 x 2 pocket 0.5 deep from its top, and a round cut
// (r 0.3, at (1.5, 1.5)) from the block's top down to the pocket's floor.
CadBodyState pocketAndCut() {
    CadBodyState state;
    cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
    state.extrude = oneSide(2.0);
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut, topOf(state, kCadFeatureId),
                                    rectSketch(0.0, 0.0, 2.0, 2.0),
                                    oneSide(0.5, ExtrudeDirection::AgainstNormal));
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut, topOf(state, kCadFeatureId),
                                    circleSketch(1.5, 1.5, 0.3),
                                    toFace(topOf(state, 2u), ExtrudeDirection::AgainstNormal));
    return state;
}

// A 4 x 4 x 1 block with a hole (d 0.4, Through All) placed from two edges.
CadBodyState edgeHoleBlock(double width, double fromA, double fromB) {
    CadBodyState state;
    cadBaseSketch(state) = rectSketch(0.0, 0.0, width, 4.0);
    state.extrude = oneSide(1.0);
    CadFeatureHole hole;
    hole.support = topOf(state, kCadFeatureId);
    hole.diameter = 0.4;
    hole.throughAll = true;
    hole.u = 1.0;
    hole.v = 0.5;
    CadFeatureHole placed;
    if (suggestCadHoleEdges(state, hole, &placed) == CadStatus::Ok) {
        placed.edgeDistanceA = fromA;
        placed.edgeDistanceB = fromB;
        appendCadFeatureHole(&state, placed);
    }
    return state;
}

uint64_t fingerprintOf(const CadBodyState& state) {
    ConstructionScene scene{NoProjectTag{}};
    SceneObject* added = scene.addCadBody(state);
    return added != nullptr ? projectSemanticFingerprint(scene, ProjectKind::Construction) : 0u;
}

bool holeAxis(const CadBodyState& state, size_t index, DVec3* out) {
    CadFrame64 frame;
    if (index >= state.holes.size() || !cadHoleFrame(state, state.holes[index], &frame)) return false;
    *out = frame.origin;
    return true;
}

}  // namespace

void runCadToFaceSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    if (out == nullptr) return;
    Checks r{out};

    // --- TF_01..04: the derived distance ---------------------------------------
    r.check("TF_01_an_add_to_face_grows_up_to_the_named_face",
            volumeIs(regen(postAndBoss(2.0)), 16.0 + 0.64 * 2.0 + disk(0.3) * 2.0));
    r.check("TF_02_to_face_follows_an_upstream_size_edit",
            volumeIs(regen(postAndBoss(3.0)), 16.0 + 0.64 * 3.0 + disk(0.3) * 3.0));
    r.check("TF_03_a_cut_to_face_ends_on_the_pocket_floor",
            volumeIs(regen(pocketAndCut()), 32.0 - 4.0 * 0.5 - disk(0.3) * 0.5));
    {
        // Down to the body's own bottom face: the cut opens right through.
        CadBodyState state;
        cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
        state.extrude = oneSide(2.0);
        appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut, topOf(state, kCadFeatureId),
                                        circleSketch(0.0, 0.0, 0.3),
                                        toFace(bottomOf(state, kCadFeatureId),
                                               ExtrudeDirection::AgainstNormal));
        const Regen g = regen(state);
        // Open at the bottom: no triangle of the result lies on z = 0 inside
        // the circle.
        bool skin = false;
        for (size_t t = 0; g.why == CadStatus::Ok && t + 2 < g.mesh.mesh.indices.size(); t += 3) {
            double cx = 0.0, cy = 0.0, cz = 0.0;
            for (int k = 0; k < 3; ++k) {
                const float* p = g.mesh.mesh.vertices[g.mesh.mesh.indices[t + k]].position;
                cx += p[0] / 3.0;
                cy += p[1] / 3.0;
                cz += p[2] / 3.0;
            }
            skin = skin || (std::fabs(cz) < 1e-6 && cx * cx + cy * cy < 0.25 * 0.25);
        }
        r.check("TF_04_a_cut_to_the_bottom_face_opens_right_through",
                volumeIs(g, 32.0 - disk(0.3) * 2.0) && !skin);
    }

    // --- TF_05: refusals by name -------------------------------------------------
    {
        CadBodyState base;
        cadBaseSketch(base) = rectSketch(0.0, 0.0, 4.0, 4.0);
        base.extrude = toFace(topOf(base, kCadFeatureId), ExtrudeDirection::AlongNormal);
        r.check("TF_05_the_base_feature_cannot_go_to_a_face",
                validateCadBodyState(base) == CadStatus::ToFaceUnsupported);
        CadBodyState later = postAndBoss(2.0);
        later.laterFeatures[1].extrude.toFace.featureId = 3u;  // itself
        r.check("TF_05_a_feature_cannot_end_on_its_own_or_a_later_face",
                validateCadBodyState(later) == CadStatus::ToFaceTargetInvalid);
        CadBodyState stale = postAndBoss(2.0);
        stale.laterFeatures[1].extrude.toFace.lineageToken ^= 0x1ull;
        r.check("TF_05_a_stale_face_is_refused_never_retargeted",
                validateCadBodyState(stale) == CadStatus::ToFaceTargetInvalid);
        CadBodyState tilted = postAndBoss(2.0);
        tilted.laterFeatures[1].extrude.toFace =
                supportOn(tilted, 2u, sideToken(tilted.sketches[1].sketch.entities[0].id(), 0));
        r.check("TF_05_a_face_not_parallel_to_the_sketch_is_refused",
                validateCadBodyState(tilted) == CadStatus::ToFaceNotParallel);
        CadBodyState behind = postAndBoss(2.0);
        behind.laterFeatures[1].extrude.toFace = topOf(behind, kCadFeatureId);
        r.check("TF_05_a_face_on_the_sketch_plane_is_behind_it",
                validateCadBodyState(behind) == CadStatus::ToFaceBehind);
        CadBodyState stored = postAndBoss(2.0);
        stored.laterFeatures[1].extrude.depth = 0.5;
        r.check("TF_05_to_face_stores_no_depth",
                validateCadBodyState(stored) == CadStatus::InvalidExtrudeExtent);
        CadBodyState targetless = postAndBoss(2.0);
        targetless.laterFeatures[1].extrude.toFace = CadFeatureSupport{};
        CadBodyState stray = postAndBoss(2.0);
        stray.laterFeatures[0].extrude.toFace = topOf(stray, kCadFeatureId);
        r.check("TF_05_a_face_is_named_in_to_face_alone",
                validateCadBodyState(targetless) == CadStatus::InvalidExtrudeExtent
                        && validateCadBodyState(stray) == CadStatus::InvalidExtrudeExtent);
    }

    // --- TF_06: the candidates ---------------------------------------------------
    {
        const CadBodyState state = postAndBoss(2.0);
        std::vector<CadToFaceCandidate> candidates;
        const CadStatus why = cadToFaceCandidates(state, 3u, &candidates);
        bool postTop = false;
        bool blockBottom = false;
        bool ordered = true;
        for (size_t i = 0; i < candidates.size(); ++i) {
            const CadToFaceCandidate& c = candidates[i];
            postTop = postTop
                      || (c.target.featureId == 2u && c.target.face.kind == CadFaceKind::CapFar
                          && c.direction == ExtrudeDirection::AlongNormal && near(c.distance, 2.0));
            blockBottom = blockBottom
                          || (c.target.featureId == kCadFeatureId
                              && c.target.face.kind == CadFaceKind::CapPlane
                              && c.direction == ExtrudeDirection::AgainstNormal
                              && near(c.distance, 1.0));
            ordered = ordered && (i == 0 || candidates[i - 1].distance <= c.distance);
            ordered = ordered && c.distance > 0.0;
        }
        r.check("TF_06_candidates_are_the_parallel_faces_beyond_the_sketch_nearest_first",
                why == CadStatus::Ok && postTop && blockBottom && ordered);
    }

    // --- TF_07..08: CADB v13 and the fingerprint ---------------------------------
    {
        const CadBodyState state = postAndBoss(2.0);
        CadBodyState plain = state;
        plain.laterFeatures[1].extrude = oneSide(2.0);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state), &why);
        const std::vector<uint8_t> plainBytes = encodeProjectV1(documentFor(plain), &why);
        uint16_t version = 0, plainVersion = 0;
        uint64_t length = 0, plainLength = 0;
        const size_t base = cadPayloadAt(bytes, &version, &length);
        cadPayloadAt(plainBytes, &plainVersion, &plainLength);
        r.check("TF_07_a_to_face_body_is_written_cadb_v13_and_only_it",
                base != 0 && version == kCadSectionVersionV13 && plainVersion < kCadSectionVersionV9);
        ProjectDocument decoded;
        r.check("TF_07_v13_round_trips_the_face_exactly",
                decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                        && decoded.cad.bodies.size() == 1u
                        && sameCadBodyState(decoded.cad.bodies[0].state, state));
        std::vector<uint8_t> older = bytes;
        if (base != 0) {
            const size_t header = base - kForgeSectionHeaderBytes;
            older[header + 4] = static_cast<uint8_t>(kCadSectionVersionV12 & 0xFF);
            older[header + 5] = static_cast<uint8_t>(kCadSectionVersionV12 >> 8);
        }
        ProjectDocument scratch;
        r.check("TF_07_the_same_section_declared_v12_is_refused",
                base != 0 && decodeProject(older.data(), older.size(), &scratch)
                                     != ProjectCodecStatus::Ok);
        CadBodyState other = state;
        other.laterFeatures[1].extrude.toFace = topOf(other, 2u);
        other.laterFeatures[1].extrude.toFace.face.kind = CadFaceKind::CapFar;
        const uint64_t withFace = fingerprintOf(state);
        r.check("TF_08_the_fingerprint_reads_the_face",
                withFace != 0u && withFace == fingerprintOf(state)
                        && fingerprintOf(plain) != withFace && fingerprintOf(plain) != 0u);
        (void)other;
    }

    // --- HE_01..05: a hole placed from two edges ---------------------------------
    {
        CadBodyState state;
        cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
        state.extrude = oneSide(1.0);
        CadFeatureHole atPoint;
        atPoint.support = topOf(state, kCadFeatureId);
        atPoint.diameter = 0.4;
        atPoint.u = 1.0;
        atPoint.v = 0.5;
        CadFeatureHole fromEdges;
        const CadStatus suggested = suggestCadHoleEdges(state, atPoint, &fromEdges);
        std::vector<CadFeatureGeometry> chain;
        buildCadChainGeometry(state, &chain, nullptr, kCadFeatureId);
        double u = 0.0, v = 0.0;
        const CadStatus point = cadHolePoint(chain, fromEdges, &u, &v);
        r.check("HE_01_switching_to_edges_moves_nothing",
                suggested == CadStatus::Ok && fromEdges.byEdges && fromEdges.u == 0.0
                        && point == CadStatus::Ok && near(u, 1.0, 1e-9) && near(v, 0.5, 1e-9)
                        && validateCadHoleValues(fromEdges) == CadStatus::Ok);
        // The two edges are the nearest: 1.0 in from one side, 1.5 from the
        // other (the block is 4 x 4, centred).
        const double lo = std::min(fromEdges.edgeDistanceA, fromEdges.edgeDistanceB);
        const double hi = std::max(fromEdges.edgeDistanceA, fromEdges.edgeDistanceB);
        r.check("HE_01_the_seeded_edges_are_the_two_nearest",
                near(lo, 1.0, 1e-9) && near(hi, 1.5, 1e-9));
    }
    {
        const CadBodyState state = edgeHoleBlock(4.0, 0.7, 0.9);
        r.check("HE_02_a_hole_from_edges_drills_through",
                state.holes.size() == 1u && volumeIs(regen(state), 16.0 - disk(0.2)));
        // Widening the block moves the edge the hole is measured from: the
        // hole moves with it by the same amount, a point hole would not.
        // Widened symmetrically (4 -> 6 along x): each x edge moves 1.0, so
        // the hole moves exactly 1.0 along x and not at all along y.
        CadBodyState wider = state;
        const SketchEntityId rect = cadBaseSketch(wider).entities[0].id();
        const bool edited = replaceSketchEntity(&cadBaseSketch(wider), rect,
                                                rectangleAt(0.0, 0.0, 6.0, 4.0))
                            == CadStatus::Ok;
        DVec3 before{}, after{};
        const bool both = holeAxis(state, 0, &before) && edited && holeAxis(wider, 0, &after);
        r.check("HE_03_an_edge_placed_hole_follows_its_edge",
                both && near(std::fabs(after.x - before.x), 1.0, 1e-9)
                        && near(after.y, before.y, 1e-9)
                        && volumeIs(regen(wider), 24.0 - disk(0.2)));
    }
    {
        const CadBodyState good = edgeHoleBlock(4.0, 0.7, 0.9);
        CadBodyState same = good;
        same.holes[0].edgeFaceB = same.holes[0].edgeFaceA;
        CadBodyState pointToo = good;
        pointToo.holes[0].u = 0.25;
        CadBodyState parallel = good;
        // The face opposite edge A: index differs by 2 on a rectangle.
        parallel.holes[0].edgeFaceB = parallel.holes[0].edgeFaceA;
        parallel.holes[0].edgeFaceB.edgeLocalIndex = (parallel.holes[0].edgeFaceA.edgeLocalIndex + 2u) % 4u;
        CadBodyState own = good;
        own.holes[0].edgeFaceA = own.holes[0].support.face;
        CadBodyState bogus = good;
        bogus.holes[0].edgeFaceA.edgeLocalIndex = 9u;
        r.check("HE_04_one_encoding_per_placement",
                validateCadBodyState(same) != CadStatus::Ok
                        && validateCadBodyState(pointToo) != CadStatus::Ok);
        r.check("HE_04_parallel_edges_do_not_fix_a_point",
                validateCadBodyState(parallel) == CadStatus::HoleEdgesParallel);
        r.check("HE_04_an_edge_that_is_not_one_is_refused_by_name",
                validateCadBodyState(own) == CadStatus::HoleEdgeNotFound
                        && validateCadBodyState(bogus) == CadStatus::HoleEdgeNotFound);
    }
    {
        const CadBodyState state = edgeHoleBlock(4.0, 0.7, 0.9);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state), &why);
        uint16_t version = 0;
        uint64_t length = 0;
        cadPayloadAt(bytes, &version, &length);
        ProjectDocument decoded;
        const CadBodyState moved = edgeHoleBlock(4.0, 0.7, 1.1);
        r.check("HE_05_an_edge_placed_hole_round_trips_cadb_v13",
                version == kCadSectionVersionV13
                        && decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                        && sameCadBodyState(decoded.cad.bodies[0].state, state)
                        && fingerprintOf(moved) != fingerprintOf(state) && fingerprintOf(state) != 0u);
    }

    // --- TF_09..11: the session --------------------------------------------------
    const auto t0 = std::chrono::steady_clock::now();
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        const ObjectId block = makeBlock(scene, history);
        // A post (0.5 x 0.5, 1 tall) on the block's top, through the session.
        bool post = false;
        if (block != kNoObject) {
            const CadBodyState staged = scene.findBody(block)->cadOrNull()->state();
            Driver d;
            SketchFrame frame;
            TopoRef ref;
            ObjectId id = kNoObject;
            post = farCap(scene, block, &frame, &ref) && d.beginFace(frame, ref, &staged)
                   && d.place(SketchTool::Rectangle, SketchPoint{0.3, 0.3}, SketchPoint{0.7, 0.7},
                              rectangleAt(0.5, 0.5, 0.5, 0.5))
                              != kNoSketchEntity
                   && d.sketch.finish() == CadStatus::Ok
                   && d.sketch.setOperation(CadFeatureOperation::Add) == CadStatus::Ok
                   && d.sketch.setExtrude(1.0, d.sketch.extrude().direction) == CadStatus::Ok
                   && d.sketch.commit(scene, history, &id) == CadStatus::Ok;
        }
        const CadBodyState staged = post ? scene.findBody(block)->cadOrNull()->state() : CadBodyState{};
        Driver d;
        SketchFrame frame;
        TopoRef ref;
        const bool ready = post && farCap(scene, block, &frame, &ref) && d.beginFace(frame, ref, &staged)
                           && d.place(SketchTool::Circle, SketchPoint{-0.5, -0.5},
                                      SketchPoint{-0.3, -0.5}, circleAt(-0.5, -0.5, 0.2))
                                      != kNoSketchEntity
                           && d.sketch.finish() == CadStatus::Ok && d.tilt();
        r.check("TF_09_to_face_is_offered_for_an_add_or_a_cut_and_not_a_new_body",
                ready && !d.sketch.toFaceAvailable()
                        && d.sketch.setExtrudeExtent(ExtrudeExtentMode::ToFace)
                                   == CadStatus::ToFaceUnsupported
                        && d.sketch.setOperation(CadFeatureOperation::Add) == CadStatus::Ok
                        && d.sketch.toFaceAvailable());
        const CadStatus set = d.sketch.setExtrudeExtent(ExtrudeExtentMode::ToFace);
        const ExtrudeFeature shown = d.sketch.displayExtrude();
        r.check("TF_09_choosing_to_face_ends_on_the_nearest_face_the_solid_grows_toward",
                set == CadStatus::Ok && extrudeIsToFace(d.sketch.extrude())
                        && d.sketch.extrude().toFace.featureId == 2u
                        && d.sketch.extrude().toFace.face.kind == CadFaceKind::CapFar
                        && shown.extent == ExtrudeExtentMode::OneSide && near(shown.depth, 1.0, 1e-6)
                        && d.sketch.evaluateCandidate().status == CadStatus::Ok);
        // A face of another body, or none, is refused; the post's top by a
        // scene reference is taken.
        TopoRef foreign = ref;
        foreign.producerObjectId = block + 100u;
        SketchFrame postFrame;
        TopoRef postTop;
        const bool postRef = farCap(scene, block, &postFrame, &postTop, 2u);
        r.check("TF_10_a_tapped_face_retargets_only_on_the_same_body",
                d.sketch.setToFaceTargetFromTopoRef(foreign) == CadStatus::ToFaceTargetInvalid
                        && postRef
                        && d.sketch.setToFaceTargetFromTopoRef(postTop) == CadStatus::Ok
                        && d.sketch.extrude().toFace.featureId == 2u);
        // A drag sets a depth; a second finger gives To Face back.
        const bool grabbed = d.dragArrowTo(0.6, true);
        const bool becameDepth = d.sketch.extrude().extent == ExtrudeExtentMode::OneSide;
        TouchPointer two[2] = {{7, 500.0f, 500.0f}, {8, 600.0f, 600.0f}};
        d.sketch.onTouch(TouchAction::PointerDown, 8, two, 2, d.camera.snapshot(), Driver::kW,
                         Driver::kH);
        r.check("TF_10_a_drag_sets_a_depth_and_a_second_finger_restores_to_face",
                grabbed && becameDepth && extrudeIsToFace(d.sketch.extrude()));
        ObjectId id = kNoObject;
        const CadStatus committed = d.sketch.commit(scene, history, &id);
        std::shared_ptr<const CadBodyMesh> mesh;
        const CadBody* body = scene.findBody(block)->cadOrNull();
        r.check("TF_11_commit_is_one_step_and_the_boss_reaches_the_face",
                committed == CadStatus::Ok && body->regenerated(&mesh) == CadStatus::Ok
                        && near(mesh->volume, 4.0 + 0.25 + circleArea(0.2), 1e-6)
                        && extrudeIsToFace(body->state().laterFeatures.back().extrude));
    }
    if (performance != nullptr) {
        char line[96];
        std::snprintf(line, sizeof(line), "to_face_session_us=%.0f",
                      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                              .count());
        *performance = line;
    }
}

}  // namespace forgeshape
