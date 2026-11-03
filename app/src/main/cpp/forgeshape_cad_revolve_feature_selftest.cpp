#include "forgeshape_cad_revolve_feature_selftest.h"

#include "forgeshape_cad_from_primitive.h"
#include "forgeshape_mass_properties.h"


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

// On a face's canonical XY: a Construction line along v at u = 0 (the axis)
// and a 0.4 x 0.4 square from u = 0.4 to 0.8 centred on v = 0 (the profile).
CadSketch ringSketch(SketchEntityId* axisId, SketchEntityId* profileId) {
    CadSketch sketch;
    SketchLine axis;
    axis.start = SketchPoint{0.0, -1.0};
    axis.end = SketchPoint{0.0, 1.0};
    addSketchEntity(&sketch, axis, axisId, SketchEntityRole::Construction);
    SketchRectangle square;
    square.center = SketchPoint{0.6, 0.0};
    square.width = 0.4;
    square.height = 0.4;
    addSketchEntity(&sketch, square, profileId);
    return sketch;
}

// A 4 x 4 x 1 block and a full-turn ring revolved on its top.
CadBodyState ringOnBlock(CadFeatureOperation operation) {
    CadBodyState state;
    cadBaseSketch(state) = rectSketch(0.0, 0.0, 4.0, 4.0);
    state.extrude = oneSide(1.0);
    SketchEntityId axisId = kNoSketchEntity;
    SketchEntityId profileId = kNoSketchEntity;
    CadSketch sketch = ringSketch(&axisId, &profileId);
    RevolveFeature revolve;
    revolve.profileEntityId = profileId;
    revolve.axis = CadSketchEdgeRef{axisId, 0u};
    revolve.angleDegrees = 360.0;
    appendCadLaterRevolveWithSketch(&state, operation, topOf(state, kCadFeatureId),
                                    std::move(sketch), revolve);
    return state;
}

}  // namespace

void runCadRevolveFeatureSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                                   std::string* performance) {
    if (out == nullptr) return;
    Checks r{out};

    // --- RV_01..02: the later revolve -----------------------------------------------
    {
        const Regen add = regen(ringOnBlock(CadFeatureOperation::Add));
        const Regen cut = regen(ringOnBlock(CadFeatureOperation::Cut));
        // The ring is symmetric about the face: half of it stands above the
        // top and half is inside the block.
        const double above = add.mesh.volume - 16.0;
        const double below = 16.0 - cut.mesh.volume;
        const double halfRing = kPi * 0.6 * 0.16;  // Pappus, half a turn
        r.check("RV_01_a_revolve_add_adds_the_half_of_the_ring_above_the_face",
                add.why == CadStatus::Ok && std::fabs(above - halfRing) < 0.01 * halfRing);
        r.check("RV_01_a_revolve_cut_removes_the_half_inside_the_part",
                cut.why == CadStatus::Ok && near(above, below, 1e-6));
    }
    {
        CadBodyState mixed = ringOnBlock(CadFeatureOperation::Add);
        mixed.laterFeatures[0].extrude.depth = 0.5;
        r.check("RV_02_a_revolve_carries_no_extrusion",
                validateCadBodyState(mixed) == CadStatus::RevolvePayloadMismatch);
        CadBodyState patterned = ringOnBlock(CadFeatureOperation::Add);
        CadFeaturePattern pattern;
        pattern.patternId = patterned.nextFeatureId++;
        pattern.kind = CadPatternKind::Mirror;
        pattern.featureIds = {patterned.laterFeatures[0].featureId};
        patterned.patterns.push_back(pattern);
        r.check("RV_02_a_pattern_of_a_revolve_is_refused",
                validateCadBodyState(patterned) == CadStatus::PatternTargetUnsupported);
    }

    // --- RV_03: CADB v15 and the fingerprint ----------------------------------------
    {
        const CadBodyState state = ringOnBlock(CadFeatureOperation::Cut);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state), &why);
        uint16_t version = 0;
        uint64_t length = 0;
        const size_t base = cadPayloadAt(bytes, &version, &length);
        r.check("RV_03_a_later_revolve_is_written_cadb_v15",
                base != 0 && version == kCadSectionVersionV15);
        ProjectDocument decoded;
        r.check("RV_03_v15_round_trips_the_revolve_exactly",
                decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                        && sameCadBodyState(decoded.cad.bodies[0].state, state));
        std::vector<uint8_t> older = bytes;
        if (base != 0) {
            const size_t header = base - kForgeSectionHeaderBytes;
            older[header + 4] = static_cast<uint8_t>(kCadSectionVersionV14 & 0xFF);
            older[header + 5] = static_cast<uint8_t>(kCadSectionVersionV14 >> 8);
        }
        ProjectDocument scratch;
        r.check("RV_03_the_same_section_declared_v14_is_refused",
                base != 0 && decodeProject(older.data(), older.size(), &scratch)
                                     != ProjectCodecStatus::Ok);
        CadBodyState turned = state;
        turned.laterFeatures[0].revolve.angleDegrees = 180.0;
        r.check("RV_03_the_fingerprint_reads_the_revolve",
                fingerprintOf(state) != 0u && fingerprintOf(state) != fingerprintOf(turned));
    }

    // --- PR_01..03: a primitive becomes a CAD part (`CAD-PRIMITIVES-R1`) -----------
    {
        auto measure = [](const PrimitiveSpec& spec, MassProperties* m) {
            CadBodyState state;
            if (cadBodyStateFromPrimitive(spec, &state) != CadStatus::Ok) return false;
            const Regen g = regen(state);
            return g.why == CadStatus::Ok
                   && computeMassProperties(g.mesh.mesh.vertices.data(),
                                            static_cast<uint32_t>(g.mesh.mesh.vertices.size()),
                                            g.mesh.mesh.indices.data(),
                                            static_cast<uint32_t>(g.mesh.mesh.indices.size()),
                                            mat4Identity(), m)
                   && m->closed;
        };
        MassProperties box, cylinder, sphere, cone;
        BoxDimensionsMeters b;
        b.width = 2.0;
        b.height = 1.0;
        b.depth = 3.0;
        const bool boxOk = measure(PrimitiveSpec::of(b), &box);
        r.check("PR_01_a_box_part_is_the_box_exactly_where_it_stood",
                boxOk && near(box.volume, 6.0, 1e-5) && near(box.boundsMin.x, -1.0, 1e-6)
                        && near(box.boundsMax.y, 0.5, 1e-6) && near(box.boundsMin.z, -1.5, 1e-6)
                        && near(box.centroid.y, 0.0, 1e-5));
        CylinderDimensionsMeters c;
        c.diameter = 1.0;
        c.height = 2.0;
        const bool cylOk = measure(PrimitiveSpec::of(c), &cylinder);
        r.check("PR_01_a_cylinder_part_stands_on_the_y_axis",
                cylOk && near(cylinder.volume, circleArea(0.5) * 2.0, 1e-5)
                        && near(cylinder.boundsMax.y - cylinder.boundsMin.y, 2.0, 1e-6)
                        && near(cylinder.boundsMax.x, 0.5, 1e-6));
        SphereDimensionsMeters sd;
        sd.diameter = 2.0;
        ConeDimensionsMeters cd;
        cd.bottomDiameter = 2.0;
        cd.height = 3.0;
        const bool sphOk = measure(PrimitiveSpec::of(sd), &sphere);
        const bool coneOk = measure(PrimitiveSpec::of(cd), &cone);
        r.check("PR_02_a_sphere_and_a_cone_are_revolved_parts_of_their_size",
                sphOk && std::fabs(sphere.volume - 4.0 / 3.0 * kPi) < 0.03 * 4.0 / 3.0 * kPi
                        && near(sphere.boundsMax.y, 1.0, 1e-5) && coneOk
                        && std::fabs(cone.volume - kPi * 3.0 / 3.0) < 0.03 * kPi
                        && near(cone.boundsMax.y, 1.5, 1e-5) && near(cone.boundsMin.y, -1.5, 1e-5));
        CapsuleDimensionsMeters cap;
        CadBodyState none;
        r.check("PR_02_a_capsule_has_no_cad_part_yet",
                cadBodyStateFromPrimitive(PrimitiveSpec::of(cap), &none)
                        == CadStatus::PrimitiveNotConvertible
                        && !primitiveHasCadPart(PrimitiveSpec::of(cap)));
    }
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        SceneObject& primitive = scene.addBody();
        const ObjectId primitiveId = primitive.objectId();
        TransformValues placed;
        placed.positionX = 1.0;
        placed.positionY = 2.0;
        placed.positionZ = 3.0;
        placed.rotationY = 30.0;
        primitive.transform().setValues(placed);
        primitive.setName("Bracket");
        publishSceneObject(primitive);
        const size_t bodies = scene.bodyCount();
        const size_t depth = history.undoDepth();
        ObjectId partId = kNoObject;
        const CadStatus converted = convertPrimitiveToCadPart(scene, history, primitiveId, &partId);
        const SceneObject* part = scene.findBody(partId);
        r.check("PR_03_the_act_replaces_the_primitive_with_a_part_as_one_step",
                converted == CadStatus::Ok && part != nullptr && part->cadOrNull() != nullptr
                        && scene.findBody(primitiveId) == nullptr && scene.bodyCount() == bodies
                        && history.undoDepth() == depth + 1 && part->name() == "Bracket"
                        && near(part->transform().values().positionX, 1.0)
                        && near(part->transform().values().rotationY, 30.0)
                        && scene.activeBodyId() == partId);
        const bool undone = history.undo();
        r.check("PR_03_one_undo_brings_the_same_primitive_back",
                undone && scene.findBody(primitiveId) != nullptr
                        && scene.findBody(primitiveId)->constructionOrNull() != nullptr
                        && scene.findBody(partId) == nullptr);
    }

    // --- RV_04: the session ---------------------------------------------------------
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
        bool drawn = block != kNoObject && farCap(scene, block, &frame, &ref)
                     && d.beginFace(frame, ref, &staged);
        SketchEntityId axisId = kNoSketchEntity;
        SketchEntityId profileId = kNoSketchEntity;
        if (drawn) {
            // A round profile beside the axis (a torus), then the axis, each
            // drawn and typed exactly; the axis is then made Construction.
            SketchCircle round;
            round.center = SketchPoint{0.6, 0.0};
            round.radius = 0.2;
            profileId = d.place(SketchTool::Circle, SketchPoint{0.6, 0.0}, SketchPoint{1.2, 0.0},
                                round);
            SketchLine axis;
            axis.start = SketchPoint{0.0, -1.0};
            axis.end = SketchPoint{0.0, 1.0};
            axisId = d.place(SketchTool::Line, SketchPoint{0.0, -0.6}, SketchPoint{0.0, 0.6}, axis);
            if (axisId != kNoSketchEntity) {
                d.sketch.setTool(SketchTool::Select);
                d.tap(SketchPoint{0.0, 0.0});
                if (d.sketch.selectedEntityId() == axisId) {
                    d.sketch.toggleSelectionConstruction();
                }
            }
            drawn = axisId != kNoSketchEntity && profileId != kNoSketchEntity;
        }
        const bool finished = drawn && d.sketch.finish() == CadStatus::Ok
                              && d.sketch.beginRevolve() == CadStatus::Ok
                              && d.sketch.setRevolveAxis(CadSketchEdgeRef{axisId, 0u})
                                         == CadStatus::Ok
                              && d.sketch.setRevolveAngle(360.0) == CadStatus::Ok;
        r.check("RV_04_a_revolve_on_a_parts_face_offers_add_and_cut",
                finished && d.sketch.operationAvailable(CadFeatureOperation::Add)
                        && d.sketch.operationAvailable(CadFeatureOperation::Cut)
                        && d.sketch.setOperation(CadFeatureOperation::Cut) == CadStatus::Ok);
        ObjectId id = kNoObject;
        const CadStatus committed =
                finished ? d.sketch.commit(scene, history, &id) : CadStatus::RegenerationFailed;
        std::shared_ptr<const CadBodyMesh> mesh;
        const CadBody* body = block != kNoObject ? scene.findBody(block)->cadOrNull() : nullptr;
        r.check("RV_04_the_commit_revolves_into_the_same_part",
                committed == CadStatus::Ok && id == block && body != nullptr
                        && body->regenerated(&mesh) == CadStatus::Ok && mesh->volume < 4.0
                        && !body->state().laterFeatures.empty()
                        && body->state().laterFeatures.back().kind == CadFeatureKind::Revolve
                        && body->state().laterFeatures.back().operation == CadFeatureOperation::Cut);
    }
    if (performance != nullptr) {
        char line[96];
        std::snprintf(line, sizeof(line), "revolve_feature_session_us=%.0f",
                      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                              .count());
        *performance = line;
    }
}

}  // namespace forgeshape
