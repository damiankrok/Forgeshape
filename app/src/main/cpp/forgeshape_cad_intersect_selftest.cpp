#include "forgeshape_cad_intersect_selftest.h"

#include "forgeshape_measure.h"
#include "forgeshape_sketch_region.h"

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

}  // namespace

void runCadIntersectSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    Checks r{out};
    // The 4 x 4 x 1 block with a round boss tool (r 0.5) from its top face,
    // grown INTO the block 0.4: an Intersect keeps only the disc under it.
    const auto intersected = [](double depth, double radius, ExtrudeDirection direction) {
        CadBodyState state = blockState(1.0);
        appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Intersect,
                                        topOf(state, kCadFeatureId),
                                        circleSketch(0.0, 0.0, radius), oneSide(depth, direction));
        return state;
    };

    // --- IX_01: the kernel keeps the common material -----------------------------
    {
        const CadBodyState state = intersected(0.4, 0.5, ExtrudeDirection::AgainstNormal);
        const Regen g = regen(state);
        r.check("IX_01_an_intersect_keeps_only_the_material_its_tool_shares_with_the_body",
                volumeIs(g, circleArea(0.5) * 0.4));
        r.check("IX_01_the_operation_is_named_and_indexed_after_cut",
                std::strcmp(cadFeatureOperationName(CadFeatureOperation::Intersect), "Intersect")
                                == 0
                        && cadFeatureOperationIndex(CadFeatureOperation::Intersect) == 3
                        && kCadFeatureOperationCount == 4 && cadBodyStateUsesIntersect(state)
                        && !cadBodyStateUsesIntersect(blockState(1.0)));
        // A tool deeper than the block keeps the whole column under it.
        const Regen deep = regen(intersected(5.0, 0.5, ExtrudeDirection::AgainstNormal));
        r.check("IX_01_a_tool_through_the_block_keeps_the_whole_column",
                volumeIs(deep, circleArea(0.5) * 1.0));
    }

    // --- IX_02: refusals by name --------------------------------------------------
    {
        // Grown OUT of the block, the tool shares nothing with it.
        const Regen away = regen(intersected(0.4, 0.5, ExtrudeDirection::AlongNormal));
        r.check("IX_02_a_tool_that_shares_nothing_is_refused_by_name",
                away.why == CadStatus::IntersectRemovesBody && away.failed == 2u);
        // A tool holding the whole block changes nothing.
        const Regen whole = regen(intersected(5.0, 4.0, ExtrudeDirection::AgainstNormal));
        r.check("IX_02_a_tool_that_holds_the_whole_body_is_refused_by_name",
                whole.why == CadStatus::IntersectNoEffect && whole.failed == 2u);
        // A pattern repeats an Add or a Cut, never an Intersect.
        CadBodyState patterned = intersected(0.4, 0.5, ExtrudeDirection::AgainstNormal);
        CadFeaturePattern pattern;
        pattern.patternId = patterned.nextFeatureId++;
        pattern.kind = CadPatternKind::Linear;
        pattern.featureIds = {2u};
        pattern.count = 2;
        pattern.spacing = 1.0;
        patterned.patterns.push_back(pattern);
        r.check("IX_02_a_pattern_of_an_intersect_is_refused",
                validateCadBodyState(patterned) == CadStatus::PatternTargetUnsupported);
        r.check("IX_02_the_status_names_exist",
                std::strcmp(cadStatusName(CadStatus::IntersectRemovesBody), "IntersectRemovesBody")
                                == 0
                        && std::strcmp(cadStatusName(CadStatus::IntersectNoEffect),
                                       "IntersectNoEffect")
                                   == 0);
    }

    // --- IX_03: CADB v16 ------------------------------------------------------------
    {
        const CadBodyState state = intersected(0.4, 0.5, ExtrudeDirection::AgainstNormal);
        ProjectCodecStatus encoded = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(state), &encoded);
        uint16_t version = 0;
        uint64_t length = 0;
        const size_t at = encoded == ProjectCodecStatus::Ok ? cadPayloadAt(bytes, &version, &length)
                                                            : 0;
        ProjectDocument decoded;
        const ProjectCodecStatus back = decodeProject(bytes.data(), bytes.size(), &decoded);
        r.check("IX_03_an_intersect_is_written_as_cadb_v16_and_reads_back",
                at != 0 && version == kCadSectionVersionV16 && back == ProjectCodecStatus::Ok
                        && decoded.cad.bodies.size() == 1
                        && cadBodyStateUsesIntersect(decoded.cad.bodies[0].state)
                        && volumeIs(regen(decoded.cad.bodies[0].state), circleArea(0.5) * 0.4));
        // A Cut in its place keeps the version it always had.
        CadBodyState cut = state;
        cut.laterFeatures[0].operation = CadFeatureOperation::Cut;
        ProjectCodecStatus cutWhy = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> cutBytes = encodeProjectV1(documentFor(cut), &cutWhy);
        uint16_t cutVersion = 0;
        cadPayloadAt(cutBytes, &cutVersion, &length);
        r.check("IX_03_a_project_without_an_intersect_keeps_its_version",
                cutVersion != 0 && cutVersion < kCadSectionVersionV16);
        // The same bytes under a lower version are refused: code 4 is v16's.
        std::vector<uint8_t> lowered = bytes;
        bool refusedLow = false;
        if (at != 0) {
            const size_t versionAt = at - kForgeSectionHeaderBytes + 4;
            lowered[versionAt] = static_cast<uint8_t>(kCadSectionVersionV15 & 0xFF);
            lowered[versionAt + 1] = static_cast<uint8_t>(kCadSectionVersionV15 >> 8);
            ProjectDocument ignored;
            refusedLow = decodeProject(lowered.data(), lowered.size(), &ignored)
                         != ProjectCodecStatus::Ok;
        }
        // And a v16 section with no Intersect is a second encoding, refused.
        std::vector<uint8_t> raised = cutBytes;
        bool refusedHigh = false;
        const size_t cutAt = cadPayloadAt(cutBytes, &cutVersion, &length);
        if (cutAt != 0) {
            const size_t versionAt = cutAt - kForgeSectionHeaderBytes + 4;
            raised[versionAt] = static_cast<uint8_t>(kCadSectionVersionV16 & 0xFF);
            raised[versionAt + 1] = static_cast<uint8_t>(kCadSectionVersionV16 >> 8);
            ProjectDocument ignored;
            refusedHigh = decodeProject(raised.data(), raised.size(), &ignored)
                          != ProjectCodecStatus::Ok;
        }
        r.check("IX_03_code_4_below_v16_and_v16_without_an_intersect_are_refused",
                refusedLow && refusedHigh);
        r.check("IX_03_the_fingerprint_tells_an_intersect_from_a_cut",
                fingerprintOf(state) != 0u && fingerprintOf(state) != fingerprintOf(cut));
    }

    // --- IX_04: the session -----------------------------------------------------------
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
        const bool placed = began
                            && d.place(SketchTool::Circle, SketchPoint{0.0, 0.0},
                                       SketchPoint{0.6, 0.0}, circleAt(0.0, 0.0, 0.3))
                                       != kNoSketchEntity;
        const CadStatus finished = placed ? d.sketch.finish() : CadStatus::RegenerationFailed;
        const bool offered = finished == CadStatus::Ok
                             && d.sketch.operationAvailable(CadFeatureOperation::Intersect);
        const CadStatus op = d.sketch.setOperation(CadFeatureOperation::Intersect);
        r.check("IX_04_a_face_sketch_offers_intersect_and_grows_it_into_the_body",
                offered && op == CadStatus::Ok
                        && d.sketch.operation() == CadFeatureOperation::Intersect
                        && d.sketch.extrude().direction == ExtrudeDirection::AgainstNormal);
        const CadStatus ex = d.sketch.setExtrude(0.5, d.sketch.extrude().direction);
        const uint32_t depth = static_cast<uint32_t>(history.undoDepth());
        ObjectId id = kNoObject;
        const CadStatus committed = ex == CadStatus::Ok ? d.sketch.commit(scene, history, &id)
                                                        : CadStatus::RegenerationFailed;
        r.check("IX_04_the_commit_keeps_the_common_disc_as_one_undo_step",
                committed == CadStatus::Ok && id == block
                        && near(bodyVolume(scene, block), circleArea(0.3) * 0.5, 1e-6)
                        && static_cast<uint32_t>(history.undoDepth()) == depth + 1u);
        // A world-plane sketch has no body to intersect.
        Driver w;
        const bool world = w.beginWorld(Workplane::XY)
                           && w.place(SketchTool::Circle, SketchPoint{0.0, 0.0},
                                      SketchPoint{0.6, 0.0}, circleAt(0.0, 0.0, 0.3))
                                      != kNoSketchEntity
                           && w.sketch.finish() == CadStatus::Ok;
        r.check("IX_04_a_world_sketch_does_not_offer_intersect",
                world && !w.sketch.operationAvailable(CadFeatureOperation::Intersect)
                        && w.sketch.setOperation(CadFeatureOperation::Intersect)
                                   != CadStatus::Ok);
    }
    // --- MS_01..03: measuring a distance (`CAD-MEASURE-R1`) -----------------------
    {
        MeasureState m;
        const bool none = !measureResult(m).complete;
        measureRecordPoint(&m, Vec3{1.0f, 2.0f, 3.0f}, true);
        const bool one = m.count == 1 && !measureResult(m).complete && m.snappedA;
        measureRecordPoint(&m, Vec3{4.0f, 6.0f, 3.0f}, false);
        const MeasureResult two = measureResult(m);
        r.check("MS_01_two_points_measure_the_distance_and_its_components",
                none && one && m.count == 2 && two.complete && near(two.distance, 5.0, 1e-9)
                        && near(two.dx, 3.0, 1e-9) && near(two.dy, 4.0, 1e-9) && two.dz == 0.0);
        measureRecordPoint(&m, Vec3{0.0f, 0.0f, 0.0f}, false);
        r.check("MS_01_a_third_point_starts_a_new_measurement",
                m.count == 1 && !measureResult(m).complete && m.a.x == 0.0f && !m.snappedA);
        // The overlay: a cross per point and the line between them.
        MeasureState both;
        measureRecordPoint(&both, Vec3{0.0f, 0.0f, 0.0f}, false);
        const size_t oneCross = measureOverlay(both, 0.1f)->vertices.size();
        measureRecordPoint(&both, Vec3{1.0f, 0.0f, 0.0f}, false);
        const SketchOverlayPtr drawn = measureOverlay(both, 0.1f);
        r.check("MS_02_the_overlay_draws_each_point_and_the_line",
                oneCross == 6u && drawn->vertices.size() == 14u && drawn->ranges.size() == 1u
                        && drawn->revision != measureOverlay(MeasureState{}, 0.1f)->revision);
        // A tap near a box corner snaps to it; one in the middle of a face does not.
        CameraController camera;
        camera.setViewport(1000, 1000);
        camera.frameSketchView(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f},
                               Vec3{0.0f, 0.0f, 1.0f});
        const MeshVertex quad[4] = {{{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 0.0f},
                                    {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 0.0f},
                                    {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 0.0f},
                                    {{-1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 0.0f}};
        const uint32_t tris[6] = {0, 1, 2, 0, 2, 3};
        MeshValidation validation;
        const RuntimeMeshPtr mesh = createRuntimeMesh(1, 1, quad, 4, tris, 6, &validation, false);
        float cx = 0.0f;
        float cy = 0.0f;
        const bool projected = projectWorldToScreen(camera.snapshot(), Vec3{1.0f, 1.0f, 0.0f}, 1000,
                                                    1000, &cx, &cy);
        bool snapped = false;
        const Vec3 corner = mesh ? measureSnapPoint(camera.snapshot(), 1000, 1000, cx - 5.0f,
                                                    cy + 5.0f, *mesh, mat4Identity(), 0,
                                                    Vec3{0.99f, 0.99f, 0.0f}, &snapped)
                                 : Vec3{};
        bool centred = true;
        float mx = 0.0f;
        float my = 0.0f;
        projectWorldToScreen(camera.snapshot(), Vec3{0.2f, -0.1f, 0.0f}, 1000, 1000, &mx, &my);
        const Vec3 middle = mesh ? measureSnapPoint(camera.snapshot(), 1000, 1000, mx, my, *mesh,
                                                    mat4Identity(), 0, Vec3{0.2f, -0.1f, 0.0f},
                                                    &centred)
                                 : Vec3{};
        // Two picks on square faces read 90 degrees; one without a normal reads none.
        MeasureState square;
        measureRecordPoint(&square, Vec3{0.0f, 0.0f, 1.0f}, false, Vec3{0.0f, 0.0f, 1.0f});
        measureRecordPoint(&square, Vec3{1.0f, 0.0f, 0.5f}, false, Vec3{1.0f, 0.0f, 0.0f});
        MeasureState unknown;
        measureRecordPoint(&unknown, Vec3{0.0f, 0.0f, 0.0f}, false);
        measureRecordPoint(&unknown, Vec3{1.0f, 0.0f, 0.0f}, false, Vec3{1.0f, 0.0f, 0.0f});
        Vec3 n;
        const bool normal = mesh && measureTriangleNormal(*mesh, mat4Identity(), 0, &n);
        r.check("MS_03_two_square_faces_read_ninety_degrees_and_a_triangle_knows_its_normal",
                measureResult(square).hasAngle && near(measureResult(square).angleDegrees, 90.0, 1e-4)
                        && !measureResult(unknown).hasAngle && normal && near(n.z, 1.0, 1e-6));
        r.check("MS_03_a_tap_beside_a_corner_measures_the_corner_exactly",
                projected && snapped && corner.x == 1.0f && corner.y == 1.0f && !centred
                        && middle.x == 0.2f && middle.y == -0.1f);
    }
    // --- RF_01..04: a revolved part's flat faces (`CAD-REVOLVE-FACES-R1`) ---------
    {
        // A cylinder r 1, h 2: a 1 x 2 rectangle of four lines on XY whose last
        // line lies on the Y axis, revolved a full turn about it.
        CadSketch profile;
        SketchEntityId ignored = kNoSketchEntity;
        SketchEntityId axisId = kNoSketchEntity;
        const SketchPoint corner[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 2.0}, {0.0, 2.0}};
        for (int k = 0; k < 4; ++k) {
            SketchLine line;
            line.start = corner[k];
            line.end = corner[(k + 1) % 4];
            addSketchEntity(&profile, line, k == 3 ? &axisId : &ignored);
        }
        const SketchRegionExtraction regions = extractSketchRegions(profile);
        RevolveFeature revolve;
        revolve.profileEntityId =
                regions.regions.empty() ? kNoSketchEntity : regions.regions.front().outerAnchorId;
        revolve.axis = CadSketchEdgeRef{axisId, 0u};
        revolve.angleDegrees = kMaxRevolveAngleDegrees;
        const CadBodyState cylinder = makeCadRevolveBodyState(profile, revolve);
        std::vector<CadFeatureGeometry> chain;
        const CadStatus built = buildCadChainGeometry(cylinder, &chain);
        std::vector<CadFeatureFace> flat;
        int curvedOrOnAxis = 0;
        if (built == CadStatus::Ok && !chain.empty()) {
            for (const CadFeatureFace& face : chain.front().faces) {
                if (face.eligible) flat.push_back(face);
                else ++curvedOrOnAxis;
            }
        }
        r.check("RF_01_the_discs_a_revolve_sweeps_are_flat_supports_and_its_walls_are_not",
                built == CadStatus::Ok && flat.size() == 2u && curvedOrOnAxis == 2
                        && !flat[0].lineageEligible && !flat[1].lineageEligible);
        // The top disc: the flat face whose normal points up the axis.
        CadFeatureSupport top;
        top.featureId = kCadFeatureId;
        for (const CadFeatureFace& face : flat) {
            if (face.frame.n.y > 0.5) top.face = face.token;
        }
        top.lineageToken = cadFeatureTopologySignature(cylinder, kCadFeatureId);
        const double cylinderVolume = 2.0 * disk(1.0);
        CadBodyState drilled = cylinder;
        CadFeatureHole hole;
        hole.support = top;
        hole.diameter = 0.4;
        CadFrame64 topFrame;
        const bool framed = resolveCadFeatureSupportFrame(chain, top, &topFrame);
        if (framed) {
            // At the axis: (0, 2, 0) in the body, in the face's own frame.
            const DVec3 d = dvec3Sub(DVec3{0.0, 2.0, 0.0}, topFrame.origin);
            hole.u = dvec3Dot(d, topFrame.u);
            hole.v = dvec3Dot(d, topFrame.v);
        }
        appendCadFeatureHole(&drilled, hole);
        const Regen bored = regen(drilled);
        r.check("RF_02_a_hole_drills_through_a_revolved_parts_top_disc",
                framed && near(topFrame.n.y, 1.0, 1e-9) && bored.why == CadStatus::Ok
                        && bored.mesh.volume < cylinderVolume - 0.2
                        && bored.mesh.volume > cylinderVolume - 0.3);
        // A later Cut drawn on the top disc.
        CadBodyState cut = cylinder;
        appendCadLaterFeatureWithSketch(&cut, CadFeatureOperation::Cut, top,
                                        circleSketch(0.0, 0.0, 0.3),
                                        oneSide(0.5, ExtrudeDirection::AgainstNormal));
        const Regen pocket = regen(cut);
        r.check("RF_02_a_later_cut_on_the_disc_pockets_the_revolved_part",
                pocket.why == CadStatus::Ok && near(pocket.mesh.volume,
                                                    cylinderVolume - circleArea(0.3) * 0.5, 1e-6));
        // CADB v17 carries it; a plain revolve keeps its version.
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(cut), &why);
        uint16_t version = 0;
        uint64_t length = 0;
        const size_t at = why == ProjectCodecStatus::Ok ? cadPayloadAt(bytes, &version, &length) : 0;
        ProjectDocument decoded;
        const bool back = decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                          && decoded.cad.bodies.size() == 1
                          && sameCadBodyState(decoded.cad.bodies[0].state, cut);
        const std::vector<uint8_t> plainBytes = encodeProjectV1(documentFor(cylinder), &why);
        uint16_t plainVersion = 0;
        cadPayloadAt(plainBytes, &plainVersion, &length);
        bool refusedLow = false;
        if (at != 0) {
            std::vector<uint8_t> lowered = bytes;
            const size_t versionAt = at - kForgeSectionHeaderBytes + 4;
            lowered[versionAt] = static_cast<uint8_t>(kCadSectionVersionV16 & 0xFF);
            lowered[versionAt + 1] = static_cast<uint8_t>(kCadSectionVersionV16 >> 8);
            ProjectDocument ignoredDoc;
            refusedLow = decodeProject(lowered.data(), lowered.size(), &ignoredDoc)
                         != ProjectCodecStatus::Ok;
        }
        r.check("RF_03_a_revolved_part_built_on_is_cadb_v17_and_reads_back",
                version == kCadSectionVersionV17 && back && plainVersion != 0
                        && plainVersion < kCadSectionVersionV17 && refusedLow
                        && cadBodyStateBuildsOnRevolve(cut) && !cadBodyStateBuildsOnRevolve(cylinder));
        // Still refused: a pattern of the revolve itself, and a sketch on its
        // curved wall.
        CadBodyState patterned = cut;
        CadFeaturePattern pattern;
        pattern.patternId = patterned.nextFeatureId++;
        pattern.featureIds = {kCadFeatureId};
        pattern.count = 2;
        pattern.spacing = 3.0;
        patterned.patterns.push_back(pattern);
        CadFeatureSupport wall = top;
        for (const CadFeatureFace& face : chain.empty() ? std::vector<CadFeatureFace>{}
                                                        : chain.front().faces) {
            if (!face.eligible) wall.face = face.token;
        }
        CadBodyState onWall = cylinder;
        appendCadLaterFeatureWithSketch(&onWall, CadFeatureOperation::Cut, wall,
                                        circleSketch(0.0, 0.0, 0.2),
                                        oneSide(0.2, ExtrudeDirection::AgainstNormal));
        r.check("RF_04_a_pattern_of_the_revolve_and_a_sketch_on_a_curved_face_stay_refused",
                validateCadBodyState(patterned) == CadStatus::PatternTargetUnsupported
                        && validateCadBodyState(onWall) != CadStatus::Ok);
    }
    if (performance != nullptr) {
        char line[96];
        std::snprintf(line, sizeof(line), "intersect_session_us=%.0f",
                      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                              .count());
        *performance = line;
    }
}

}  // namespace forgeshape
