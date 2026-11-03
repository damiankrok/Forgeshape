#include "forgeshape_sketch_knife_undo_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_extrude_tool.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_camera.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {

namespace {

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool passed) { out->push_back(ArrangementSelfTestCheck{name, passed}); }
};

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

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

// A face sketch on the block's top, holding one circle of radius 0.5, finished.
bool circleOnTop(Driver& d, const ConstructionScene& scene, ObjectId block,
                 const CadBodyState& staged) {
    SketchFrame frame;
    TopoRef ref;
    return farCap(scene, block, &frame, &ref) && d.beginFace(frame, ref, &staged)
           && d.place(SketchTool::Circle, SketchPoint{0.0, 0.0}, SketchPoint{0.6, 0.0},
                      circleAt(0.0, 0.0, 0.5))
                      != kNoSketchEntity
           && d.sketch.finish() == CadStatus::Ok && d.sketch.selectionChosen() && d.tilt();
}

// ---------------------------------------------------------------------------
// A. Push-through: dragging the arrow through the face makes it a Cut
// ---------------------------------------------------------------------------

void testPushThrough(Checks& c) {
    ConstructionScene scene;
    ConstructionHistory history(scene);
    const ObjectId block = makeBlock(scene, history);
    c.check("SKU_PT_00_a_2x2x1_block_is_committed_through_the_session",
            block != kNoObject && near(bodyVolume(scene, block), 4.0, 1e-9));
    if (block == kNoObject) return;
    const CadBodyState staged = scene.findBody(block)->cadOrNull()->state();
    const double disk = circleArea(0.5);

    {
        Driver d;
        const bool ready = circleOnTop(d, scene, block, staged);
        c.check("SKU_PT_01_a_face_sketch_starts_as_new_body_pointing_out_of_the_face",
                ready && d.sketch.operation() == CadFeatureOperation::NewBody
                        && d.sketch.extrude().direction == ExtrudeDirection::AlongNormal
                        && d.sketch.operationAvailable(CadFeatureOperation::Cut));
        const double depth0 = d.sketch.extrude().depth;
        // Into the body, 0.4 below the face, with the finger still down.
        const bool grabbed = ready && d.dragArrowTo(-0.4, true);
        const bool active = d.sketch.pushThroughActive();
        c.check("SKU_PT_02_dragging_through_the_face_switches_to_cut_into_the_body",
                grabbed && active && d.sketch.operation() == CadFeatureOperation::Cut
                        && d.sketch.extrude().direction == ExtrudeDirection::AgainstNormal
                        && near(d.sketch.extrude().depth, 0.4, 0.02)
                        && d.sketch.evaluateCandidate().status == CadStatus::Ok);
        // A second finger cancels: everything the drag started from returns.
        TouchPointer two[2] = {{7, 500.0f, 500.0f}, {8, 600.0f, 600.0f}};
        d.sketch.onTouch(TouchAction::PointerDown, 8, two, 2, d.camera.snapshot(), Driver::kW,
                         Driver::kH);
        c.check("SKU_PT_03_a_second_pointer_restores_the_operation_side_and_depth",
                grabbed && !d.sketch.extrudeManipulator().capturing()
                        && d.sketch.operation() == CadFeatureOperation::NewBody
                        && d.sketch.extrude().direction == ExtrudeDirection::AlongNormal
                        && d.sketch.extrude().depth == depth0);
        // Through, released, then pulled back out again: an outward New Body.
        const bool in = d.dragArrowTo(-0.4);
        const bool cutIn = in && d.sketch.operation() == CadFeatureOperation::Cut;
        const bool out = cutIn && d.dragArrowTo(-0.3);  // the arrow now points down: -0.3 is up
        c.check("SKU_PT_04_pulling_a_cut_back_out_through_the_face_restores_new_body",
                cutIn && out && d.sketch.operation() == CadFeatureOperation::NewBody
                        && d.sketch.extrude().direction == ExtrudeDirection::AlongNormal
                        && near(d.sketch.extrude().depth, 0.3, 0.02));
        d.sketch.cancel();
    }
    {
        // The outward operation the user chose is the one a pull-out returns to.
        Driver d;
        const bool ready = circleOnTop(d, scene, block, staged)
                           && d.sketch.setOperation(CadFeatureOperation::Add) == CadStatus::Ok;
        const bool in = ready && d.dragArrowTo(-0.25)
                        && d.sketch.operation() == CadFeatureOperation::Cut;
        const bool out = in && d.dragArrowTo(-0.2);
        c.check("SKU_PT_05_an_add_pushed_in_is_a_cut_and_pulled_out_is_an_add_again",
                in && out && d.sketch.operation() == CadFeatureOperation::Add
                        && d.sketch.extrude().direction == ExtrudeDirection::AlongNormal);
        d.sketch.cancel();
    }
    {
        // Commit after a push-through: ONE step, the SAME body, the exact pocket.
        Driver d;
        const size_t undoBefore = history.undoDepth();
        const bool pushed = circleOnTop(d, scene, block, staged) && d.dragArrowTo(-0.4);
        const double depth = d.sketch.extrude().depth;
        ObjectId committed = kNoObject;
        const CadStatus why = pushed ? d.sketch.commit(scene, history, &committed)
                                     : CadStatus::NotSketching;
        c.check("SKU_PT_06_committing_the_pushed_cut_pockets_the_same_body_in_one_step",
                pushed && why == CadStatus::Ok && committed == block
                        && history.undoDepth() == undoBefore + 1u
                        && near(bodyVolume(scene, block), 4.0 - disk * depth, 1e-6)
                        && depth > 0.3 && depth < 0.5);
    }
    {
        // A world-plane sketch has no material to push into: the drag clamps
        // at the millimetre floor and stays a New Body.
        Driver d;
        const bool ready = d.beginWorld(Workplane::XY)
                           && d.place(SketchTool::Circle, SketchPoint{5.0, 0.0},
                                      SketchPoint{5.6, 0.0}, circleAt(5.0, 0.0, 0.5))
                                      != kNoSketchEntity
                           && d.sketch.finish() == CadStatus::Ok && d.tilt();
        const bool dragged = ready && d.dragArrowTo(-0.4, true);
        const bool inactive = !d.sketch.pushThroughActive();
        d.at(TouchAction::Up, 0.0f, 0.0f);
        c.check("SKU_PT_07_a_world_plane_drag_never_crosses_and_stays_new_body",
                dragged && inactive && d.sketch.operation() == CadFeatureOperation::NewBody
                        && d.sketch.extrude().direction == ExtrudeDirection::AlongNormal
                        && near(d.sketch.extrude().depth, kCadExtrudeMinDragDepthMeters, 1e-12));
        d.sketch.cancel();
    }
    {
        // An EDIT keeps the side and the operation the user chose.
        Driver d;
        const CadBodyState now = scene.findBody(block)->cadOrNull()->state();
        const uint32_t cutId = now.laterFeatures.empty() ? 0u : now.laterFeatures.back().featureId;
        SketchFrame top;
        TopoRef unused;
        const bool editing = cutId != 0u && farCap(scene, block, &top, &unused)
                             && d.sketch.beginEditFeature(block, now, cutId, top, true)
                                        == CadStatus::Ok;
        if (editing) d.frameCamera();
        const bool tilted = editing && d.tilt();
        const bool dragged = tilted && d.dragArrowTo(-0.2, true);
        const bool inactive = !d.sketch.pushThroughActive();
        d.at(TouchAction::Up, 0.0f, 0.0f);
        c.check("SKU_PT_08_an_edit_of_a_feature_is_never_pushed_through",
                dragged && inactive && d.sketch.operation() == CadFeatureOperation::Cut
                        && d.sketch.extrude().direction == ExtrudeDirection::AgainstNormal);
        d.sketch.cancel();
    }
}

// ---------------------------------------------------------------------------
// B. In-sketch Undo / Redo
// ---------------------------------------------------------------------------

void testSketchUndo(Checks& c) {
    Driver d;
    const bool began = d.beginWorld(Workplane::XY);
    c.check("SKU_UN_01_a_fresh_sketch_has_nothing_to_undo_or_redo",
            began && !d.sketch.canUndoSketchEdit() && !d.sketch.canRedoSketchEdit()
                    && d.sketch.sketchUndoDepth() == 0u);
    d.sketch.setTool(SketchTool::Rectangle);
    const bool drew = d.stroke({SketchPoint{-1.0, -1.0}, SketchPoint{1.0, 1.0}});
    const CadSketch one = d.sketch.sketch();
    c.check("SKU_UN_02_one_drawn_rectangle_is_one_entry",
            drew && one.entities.size() == 1u && d.sketch.sketchUndoDepth() == 1u
                    && d.sketch.canUndoSketchEdit());
    d.sketch.setTool(SketchTool::Circle);
    const bool drew2 = d.stroke({SketchPoint{3.0, 0.0}, SketchPoint{3.5, 0.0}});
    const CadSketch two = d.sketch.sketch();
    const bool undid = d.sketch.undoSketchEdit();
    c.check("SKU_UN_03_undo_takes_back_the_last_act_exactly",
            drew2 && two.entities.size() == 2u && undid && sameCadSketch(d.sketch.sketch(), one)
                    && d.sketch.canRedoSketchEdit());
    const bool redid = d.sketch.redoSketchEdit();
    c.check("SKU_UN_04_redo_restores_it_exactly",
            redid && sameCadSketch(d.sketch.sketch(), two) && !d.sketch.canRedoSketchEdit());
    d.sketch.undoSketchEdit();
    d.sketch.undoSketchEdit();
    c.check("SKU_UN_05_undo_walks_back_to_the_empty_sketch_and_stops",
            d.sketch.sketch().entities.empty() && !d.sketch.undoSketchEdit()
                    && d.sketch.sketchRedoDepth() == 2u);
    d.sketch.redoSketchEdit();
    d.sketch.setTool(SketchTool::Line);
    d.stroke({SketchPoint{-2.0, 2.0}, SketchPoint{2.0, 2.0}});
    c.check("SKU_UN_06_a_new_act_after_undo_clears_redo",
            d.sketch.sketch().entities.size() == 2u && !d.sketch.canRedoSketchEdit()
                    && d.sketch.sketchUndoDepth() == 2u);
    // A Delete is one entry and Undo brings the entity back with its id.
    const SketchEntityId lineId = d.sketch.sketch().entities.back().id();
    const CadSketch beforeDelete = d.sketch.sketch();
    const bool selected = d.sketch.select(lineId);
    const bool deleted = selected && d.sketch.deleteSelected() == CadStatus::Ok;
    const bool back = deleted && d.sketch.undoSketchEdit();
    c.check("SKU_UN_07_a_delete_is_one_entry_and_undo_restores_the_same_entity",
            back && sameCadSketch(d.sketch.sketch(), beforeDelete)
                    && findSketchEntity(d.sketch.sketch(), lineId) != nullptr);
    // A typed value is one entry.
    const size_t depth = d.sketch.sketchUndoDepth();
    const bool typed = d.sketch.applyLineLength(lineId, 1.5) == CadStatus::Ok;
    const bool typedBack = typed && d.sketch.sketchUndoDepth() == depth + 1u
                           && d.sketch.undoSketchEdit()
                           && sameCadSketch(d.sketch.sketch(), beforeDelete);
    c.check("SKU_UN_08_a_typed_length_is_one_entry", typedBack);
    // A polyline being placed gives back its last vertex first, recording nothing.
    d.sketch.setTool(SketchTool::Polyline);
    const size_t depthBeforeRun = d.sketch.sketchUndoDepth();
    d.tap(SketchPoint{-3.0, -3.0});
    d.tap(SketchPoint{-2.0, -3.0});
    d.tap(SketchPoint{-2.0, -2.0});
    const bool running = d.sketch.polylineInProgress();
    const bool popped = d.sketch.undoSketchEdit() && d.sketch.polylineInProgress()
                        && d.sketch.sketchUndoDepth() == depthBeforeRun;
    c.check("SKU_UN_09_undo_inside_a_polyline_takes_back_the_last_vertex",
            running && popped && d.sketch.sketch().entities.size() == 2u);
    d.sketch.setTool(SketchTool::Select);  // ends the run as an open polyline: one entry
    c.check("SKU_UN_10_ending_a_run_is_one_entry",
            d.sketch.sketch().entities.size() == 3u
                    && d.sketch.sketchUndoDepth() == depthBeforeRun + 1u);
    // Ready refuses; Back to Sketch keeps the branch.
    d.sketch.undoSketchEdit();
    const size_t kept = d.sketch.sketchUndoDepth();
    d.sketch.setTool(SketchTool::Rectangle);
    d.stroke({SketchPoint{4.0, 4.0}, SketchPoint{5.0, 5.0}});
    const bool finished = d.sketch.finish() == CadStatus::Ok;
    const bool refusedInReady = finished && !d.sketch.canUndoSketchEdit()
                                && !d.sketch.undoSketchEdit();
    d.sketch.backToEditing();
    c.check("SKU_UN_11_ready_has_no_sketch_undo_and_back_to_sketch_keeps_the_branch",
            refusedInReady && d.sketch.canUndoSketchEdit()
                    && d.sketch.sketchUndoDepth() == kept + 1u);
    // The cap: oldest first, never the newest.
    for (int i = 0; i < 70; ++i) {
        d.sketch.setTool(SketchTool::Line);
        d.stroke({SketchPoint{-4.0 + 0.1 * i, -4.0}, SketchPoint{-4.0 + 0.1 * i, -3.5}});
    }
    c.check("SKU_UN_12_the_branch_is_capped_at_its_bound",
            d.sketch.sketchUndoDepth() == kMaxSketchUndoEntries);
    // Cancel ends the branch with the session.
    d.sketch.cancel();
    const bool again = d.beginWorld(Workplane::XY);
    c.check("SKU_UN_13_cancel_and_a_new_sketch_start_with_an_empty_branch",
            again && d.sketch.sketchUndoDepth() == 0u && d.sketch.sketchRedoDepth() == 0u);
    d.sketch.cancel();

    // Commit after an Undo commits what the sketch NOW holds, as ONE step.
    ConstructionScene scene;
    ConstructionHistory history(scene);
    Driver e;
    e.beginWorld(Workplane::XY);
    e.place(SketchTool::Rectangle, SketchPoint{-0.5, -0.5}, SketchPoint{0.5, 0.5},
            rectangleAt(0.0, 0.0, 2.0, 2.0));
    e.place(SketchTool::Circle, SketchPoint{0.0, 0.0}, SketchPoint{0.4, 0.0},
            circleAt(0.0, 0.0, 0.4));
    // Undo the typed circle and its gesture: the circle is gone.
    e.sketch.undoSketchEdit();
    e.sketch.undoSketchEdit();
    const bool oneLeft = e.sketch.sketch().entities.size() == 1u;
    const size_t undoBefore = history.undoDepth();
    ObjectId id = kNoObject;
    const bool committed = oneLeft && e.sketch.finish() == CadStatus::Ok
                           && e.sketch.commit(scene, history, &id) == CadStatus::Ok;
    c.check("SKU_UN_14_finish_after_undo_commits_the_undone_sketch_as_one_step",
            committed && history.undoDepth() == undoBefore + 1u
                    && near(bodyVolume(scene, id), 4.0, 1e-9)
                    && e.sketch.sketchUndoDepth() == 0u);
}

// ---------------------------------------------------------------------------
// C. The knife
// ---------------------------------------------------------------------------

void testKnife(Checks& c) {
    Driver d;
    d.beginWorld(Workplane::XY);
    const bool held = d.sketch.setTool(SketchTool::Trim);
    c.check("SKU_KN_01_the_knife_on_the_rail_is_the_trim_mode",
            held && d.sketch.tool() == SketchTool::Trim
                    && d.sketch.modifyMode() == SketchModifyMode::Trim
                    && sketchToolIndex(SketchTool::Trim) == 7 && kSketchToolCount == 8);
    d.sketch.setTool(SketchTool::Line);
    c.check("SKU_KN_02_a_drawing_tool_puts_the_knife_down",
            d.sketch.tool() == SketchTool::Line && d.sketch.modifyMode() == SketchModifyMode::None);
    d.sketch.setModifyMode(SketchModifyMode::Trim);
    const bool palette = d.sketch.tool() == SketchTool::Trim;
    d.sketch.setModifyMode(SketchModifyMode::None);
    c.check("SKU_KN_03_trim_from_the_palette_holds_the_knife_and_leaving_it_selects",
            palette && d.sketch.tool() == SketchTool::Select
                    && d.sketch.modifyMode() == SketchModifyMode::None);

    // A horizontal line crossed by three verticals: x = -1, 0, 1.
    const SketchEntityId h = d.place(SketchTool::Line, SketchPoint{-2.0, 0.0}, SketchPoint{2.0, 0.1},
                                     SketchLine{SketchPoint{-2.0, 0.0}, SketchPoint{2.0, 0.0}});
    std::vector<SketchEntityId> verticals;
    for (int i = -1; i <= 1; ++i) {
        verticals.push_back(d.place(SketchTool::Line, SketchPoint{i * 1.0, -1.0},
                                    SketchPoint{i * 1.0 + 0.1, 1.0},
                                    SketchLine{SketchPoint{i * 1.0, -1.0},
                                               SketchPoint{i * 1.0, 1.0}}));
    }
    const bool drawn = h != kNoSketchEntity && verticals[0] != kNoSketchEntity
                       && verticals[1] != kNoSketchEntity && verticals[2] != kNoSketchEntity
                       && d.sketch.sketch().entities.size() == 4u;
    // Swipe across the TOP halves of all three verticals in one stroke.
    d.sketch.setTool(SketchTool::Trim);
    const size_t depth = d.sketch.sketchUndoDepth();
    const CadSketch beforeSwipe = d.sketch.sketch();
    const auto t0 = std::chrono::steady_clock::now();
    const bool swiped = drawn && d.stroke({SketchPoint{-1.5, 0.6}, SketchPoint{1.5, 0.6}});
    const double swipeUs = std::chrono::duration<double, std::micro>(
                                   std::chrono::steady_clock::now() - t0).count();
    // Every vertical now reaches only from y = -1 up to the horizontal line.
    bool topsGone = swiped && d.sketch.lastKnifeTrimCount() == 3u;
    size_t topReach = 0;
    for (const SketchEntity& e : d.sketch.sketch().entities) {
        if (const SketchLine* line = e.line()) {
            if (std::fabs(line->start.u - line->end.u) < 1e-9) {
                topReach += std::max(line->start.v, line->end.v) > 1e-9 ? 1u : 0u;
            }
        }
    }
    topsGone = topsGone && topReach == 0u;
    c.check("SKU_KN_04_one_swipe_cuts_every_interval_it_crosses_back_to_the_intersection",
            topsGone);
    c.check("SKU_KN_05_the_swipe_is_one_undo_entry_and_undo_restores_every_cut",
            topsGone && d.sketch.sketchUndoDepth() == depth + 1u && d.sketch.undoSketchEdit()
                    && sameCadSketch(d.sketch.sketch(), beforeSwipe));
    // A stroke that runs BESIDE a line, never crossing it, cuts nothing --
    // even where a still tap at the same distance would (the tap tolerance is
    // 24 reference units, about 0.62 m in this framing, and the knife's reach
    // 0.6 of a quarter of it, about 0.09 m; this stroke is 0.12 m off).
    const bool beside = d.stroke({SketchPoint{-1.8, 0.12}, SketchPoint{-1.2, 0.12}});
    c.check("SKU_KN_06_a_stroke_beside_a_line_without_crossing_it_cuts_nothing",
            beside && d.sketch.lastKnifeTrimCount() == 0u
                    && sameCadSketch(d.sketch.sketch(), beforeSwipe));
    // A still tap is the ordinary Trim tap: the interval under the finger.
    const bool tapped = d.tap(SketchPoint{-0.5, 0.0});
    size_t pieces = 0;
    for (const SketchEntity& e : d.sketch.sketch().entities) {
        if (const SketchLine* line = e.line()) {
            if (std::fabs(line->start.v) < 1e-9 && std::fabs(line->end.v) < 1e-9) ++pieces;
        }
    }
    c.check("SKU_KN_07_a_still_tap_with_the_knife_trims_the_interval_under_it",
            tapped && d.sketch.sketch().entities.size() == 5u && pieces == 2u);
    // The knife does not outlive its sketch.
    d.sketch.cancel();
    const bool again = d.beginWorld(Workplane::XY);
    c.check("SKU_KN_08_a_new_sketch_starts_with_the_rectangle_not_the_knife",
            again && d.sketch.tool() == SketchTool::Rectangle
                    && d.sketch.modifyMode() == SketchModifyMode::None);
    d.sketch.cancel();
    c.check("SKU_KN_09_a_three_cut_swipe_is_bounded_in_time", swipeUs < 2.0e6);
}

}  // namespace

void runSketchKnifeUndoSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                                 std::string* performance) {
    if (out == nullptr) return;
    Checks c{out};
    const auto t0 = std::chrono::steady_clock::now();
    testPushThrough(c);
    testSketchUndo(c);
    testKnife(c);
    if (performance != nullptr) {
        char line[96];
        std::snprintf(line, sizeof(line), "sketch_knife_undo_us=%.0f",
                      std::chrono::duration<double, std::micro>(
                              std::chrono::steady_clock::now() - t0).count());
        *performance = line;
    }
}

}  // namespace forgeshape
