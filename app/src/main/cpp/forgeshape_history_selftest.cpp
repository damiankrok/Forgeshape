#include "forgeshape_history_selftest.h"

#include <vector>

#include "forgeshape_body_delete.h"
#include "forgeshape_construction.h"
#include "forgeshape_history.h"
#include "forgeshape_imported_mesh.h"
#include "forgeshape_mesh.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

struct Recorder {
    HistorySelfTestResult* out;
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

// Two strings compared without pulling <string> into a suite that needs
// nothing else from it. Used only to prove a status NAME is stable, which is
// what a log line and a piece of evidence depend on.
bool sameText(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) {
        return a == b;
    }
    for (; *a != '\0' && *b != '\0'; ++a, ++b) {
        if (*a != *b) {
            return false;
        }
    }
    return *a == *b;
}

// The smallest valid Imported Mesh, for the Delete cases.
//
// Four vertices and two submeshes with different `doubleSided` answers, so a
// restored body that lost its batching would show. It is built here rather
// than shared with another suite on purpose: every suite in this product builds
// the domain objects it needs, so a result never depends on another suite.
ImportedMesh historyImportedMesh() {
    const std::vector<float> positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                       0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 3.0f};
    const std::vector<float> normals{0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f};
    const std::vector<uint32_t> indices{0, 1, 2, 0, 2, 3};
    const std::vector<ImportedMeshBatch> batches{ImportedMeshBatch{0, 3, false},
                                                 ImportedMeshBatch{3, 3, true}};
    return ImportedMesh::build(positions, normals, indices, batches);
}

// Every case below builds its OWN ConstructionScene and its OWN
// ConstructionHistory, for the same reason the scene suite does: a self-test
// that reads the process-scoped scene passes or fails depending on what a live
// session or a previous suite left behind. ConstructionHistory takes its scene
// by reference precisely so this is possible.
struct Fixture {
    ConstructionScene scene;
    ConstructionHistory history{scene};

    Fixture() {
        // The startup body is published the way the product publishes it, so a
        // body under test is in exactly the state a real one would be.
        publish(scene.activeBody());
    }

    static MeshRevision publish(SceneObject& body) {
        return publishConstructionObject(body.construction(), body.meshStore());
    }

    // One typed Apply, wrapped exactly as the product wraps it.
    PrimitiveUpdateStatus applyShape(const PrimitiveSpec& spec) {
        ScopedConstructionEdit edit(history);
        SceneObject& body = scene.activeBody();
        const PrimitiveApplyResult result = applyPrimitive(body.construction(),
                                                           body.meshStore(), spec);
        if (result.status == PrimitiveUpdateStatus::Applied) {
            body.frozenSculpt().markSourceStale();
        }
        return result.status;
    }

    TransformUpdateStatus applyPlacement(const TransformValues& values) {
        ScopedConstructionEdit edit(history);
        return applyTransformValues(scene.activeBody().transform(), values).status;
    }

    // Creation, as the product performs it: one edit around a scene append and
    // that primitive's own apply.
    ObjectId addBody(const PrimitiveSpec& spec) {
        ScopedConstructionEdit edit(history);
        SceneObject& body = scene.addBody();
        applyPrimitive(body.construction(), body.meshStore(), spec);
        return body.objectId();
    }
};

TransformValues placement(double px, double py, double pz, double rx, double ry, double rz) {
    TransformValues values;
    values.positionX = px;
    values.positionY = py;
    values.positionZ = pz;
    values.rotationX = rx;
    values.rotationY = ry;
    values.rotationZ = rz;
    return values;
}

bool samePlacement(const TransformValues& a, const TransformValues& b) {
    return a.positionX == b.positionX && a.positionY == b.positionY && a.positionZ == b.positionZ
        && a.rotationX == b.rotationX && a.rotationY == b.rotationY && a.rotationZ == b.rotationZ;
}

std::vector<ObjectId> orderedIds(const ConstructionScene& scene) {
    std::vector<ObjectId> ids;
    ids.reserve(scene.bodyCount());
    for (size_t i = 0; i < scene.bodyCount(); ++i) {
        ids.push_back(scene.bodyAt(i).objectId());
    }
    return ids;
}

}  // namespace

int runHistorySelfTests(HistorySelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};

    // -----------------------------------------------------------------------
    // Empty history
    // -----------------------------------------------------------------------
    {
        Fixture f;
        r.check("empty_history_cannot_undo", !f.history.canUndo());
        r.check("empty_history_cannot_redo", !f.history.canRedo());
        r.check("empty_history_depth_zero",
                f.history.undoDepth() == 0 && f.history.redoDepth() == 0);
        r.check("empty_history_undo_is_a_no_op", !f.history.undo());
        r.check("empty_history_redo_is_a_no_op", !f.history.redo());
    }

    // -----------------------------------------------------------------------
    // One Exact Shape Apply is one step, and it is exactly reversible
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const ConstructionObjectState before = f.scene.activeBody().construction().captureState();
        // Placement is the BODY's since IMPORT-01A, so it is captured beside the
        // shape rather than inside it. The invariant under test is unchanged.
        const TransformValues beforePlacement = f.scene.activeBody().transform().values();
        const MeshRevision beforeRevision = f.scene.activeBody().meshStore().currentRevision();

        const PrimitiveUpdateStatus status =
            f.applyShape(PrimitiveSpec::forCylinder(0.8, 2.25));
        r.check("shape_apply_is_applied", status == PrimitiveUpdateStatus::Applied);
        r.check("shape_apply_is_one_step", f.history.undoDepth() == 1);
        r.check("shape_apply_clears_nothing_to_redo", f.history.redoDepth() == 0);
        r.check("shape_apply_enables_undo", f.history.canUndo() && !f.history.canRedo());

        const ConstructionObjectState after = f.scene.activeBody().construction().captureState();
        const ObjectId id = f.scene.activeBody().objectId();

        r.check("shape_undo_reports_success", f.history.undo());
        const ConstructionObjectState undone = f.scene.activeBody().construction().captureState();
        r.check("shape_undo_restores_kind", undone.kind == before.kind);
        r.check("shape_undo_restores_every_parameter", sameConstructionShape(undone, before));
        r.check("shape_undo_restores_placement", sameConstructionPlacement(f.scene.activeBody().transform().values(), beforePlacement));
        r.check("shape_undo_keeps_object_id", f.scene.activeBody().objectId() == id);
        r.check("shape_undo_republishes",
                f.scene.activeBody().meshStore().currentRevision() > beforeRevision);
        r.check("shape_undo_moves_the_step_to_redo",
                f.history.undoDepth() == 0 && f.history.redoDepth() == 1);
        r.check("shape_undo_enables_redo", f.history.canRedo() && !f.history.canUndo());

        r.check("shape_redo_reports_success", f.history.redo());
        const ConstructionObjectState redone = f.scene.activeBody().construction().captureState();
        r.check("shape_redo_restores_exact_post_state", sameConstructionShape(redone, after));
        r.check("shape_redo_keeps_object_id", f.scene.activeBody().objectId() == id);
        r.check("shape_redo_returns_the_step",
                f.history.undoDepth() == 1 && f.history.redoDepth() == 0);
    }

    // -----------------------------------------------------------------------
    // Rejection and no-op write no history
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const ConstructionObjectState before = f.scene.activeBody().construction().captureState();
        const PrimitiveUpdateStatus rejected =
            f.applyShape(PrimitiveSpec::forBox(-1.0, 1.0, 0.5));
        r.check("rejected_shape_apply_is_rejected",
                rejected == PrimitiveUpdateStatus::Rejected);
        r.check("rejected_shape_apply_writes_no_history", f.history.undoDepth() == 0);
        r.check("rejected_shape_apply_changes_nothing",
                sameConstructionShape(f.scene.activeBody().construction().captureState(), before));

        const PrimitiveUpdateStatus unchanged = f.applyShape(
            PrimitiveSpec::of(f.scene.activeBody().construction().box().dimensionsMeters()));
        r.check("identical_shape_apply_is_unchanged",
                unchanged == PrimitiveUpdateStatus::Unchanged);
        r.check("identical_shape_apply_writes_no_history", f.history.undoDepth() == 0);

        const TransformValues current = f.scene.activeBody().transform().values();
        r.check("identical_placement_apply_is_unchanged",
                f.applyPlacement(current) == TransformUpdateStatus::Unchanged);
        r.check("identical_placement_apply_writes_no_history", f.history.undoDepth() == 0);
    }

    // -----------------------------------------------------------------------
    // Exact Transform: six values, one atomic step, no mesh revision
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const TransformValues origin = f.scene.activeBody().transform().values();
        const MeshRevision beforeRevision = f.scene.activeBody().meshStore().currentRevision();
        const TransformValues moved = placement(1.5, -0.25, 3.0, 15.0, -90.0, 370.0);

        r.check("placement_apply_is_applied",
                f.applyPlacement(moved) == TransformUpdateStatus::Applied);
        r.check("placement_apply_is_one_step", f.history.undoDepth() == 1);
        r.check("placement_apply_publishes_no_revision",
                f.scene.activeBody().meshStore().currentRevision() == beforeRevision);

        r.check("placement_undo_reports_success", f.history.undo());
        r.check("placement_undo_restores_all_six",
                samePlacement(f.scene.activeBody().transform().values(), origin));
        r.check("placement_undo_publishes_no_revision",
                f.scene.activeBody().meshStore().currentRevision() == beforeRevision);

        r.check("placement_redo_reports_success", f.history.redo());
        r.check("placement_redo_restores_all_six",
                samePlacement(f.scene.activeBody().transform().values(), moved));
        // 370 degrees is stored exactly as given and must survive a round trip
        // through history without being reduced modulo 360.
        r.check("placement_history_preserves_the_rotation_convention",
                f.scene.activeBody().transform().rotationZDegrees() == 370.0);
        r.check("placement_redo_publishes_no_revision",
                f.scene.activeBody().meshStore().currentRevision() == beforeRevision);
    }

    // -----------------------------------------------------------------------
    // Many updates inside one transaction commit as exactly one step
    // (the Stage 020 drag boundary, proved without a gizmo)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const TransformValues preDrag = f.scene.activeBody().transform().values();
        r.check("transaction_opens", f.history.beginEdit());
        r.check("transaction_refuses_to_nest", !f.history.beginEdit());
        r.check("transaction_reports_in_progress", f.history.editInProgress());
        r.check("undo_is_refused_while_an_edit_is_open", !f.history.canUndo());

        TransformValues live = preDrag;
        for (int step = 1; step <= 12; ++step) {
            live.positionX = 0.1 * step;
            live.rotationY = 3.0 * step;
            applyTransformValues(f.scene.activeBody().transform(), live);
        }
        r.check("transaction_updates_move_live_state",
                f.scene.activeBody().transform().positionXMeters() == live.positionX);
        r.check("transaction_updates_write_no_history", f.history.undoDepth() == 0);

        r.check("transaction_commit_records_a_step", f.history.commitEdit());
        r.check("transaction_commit_is_exactly_one_step", f.history.undoDepth() == 1);
        r.check("transaction_commit_closes_the_edit", !f.history.editInProgress());

        r.check("transaction_undo_returns_to_pre_state",
                f.history.undo()
                    && samePlacement(f.scene.activeBody().transform().values(), preDrag));
        r.check("transaction_redo_returns_to_final_state",
                f.history.redo()
                    && samePlacement(f.scene.activeBody().transform().values(), live));
    }

    // -----------------------------------------------------------------------
    // Cancel restores the pre-state and records nothing
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.applyShape(PrimitiveSpec::forSphere(1.5));
        const size_t depthBefore = f.history.undoDepth();
        const TransformValues preDrag = f.scene.activeBody().transform().values();
        const ConstructionObjectState preState =
            f.scene.activeBody().construction().captureState();

        f.history.beginEdit();
        TransformValues live = preDrag;
        for (int step = 1; step <= 5; ++step) {
            live.positionZ = -0.4 * step;
            applyTransformValues(f.scene.activeBody().transform(), live);
        }
        f.history.cancelEdit();
        r.check("cancel_restores_the_pre_state",
                samePlacement(f.scene.activeBody().transform().values(), preDrag));
        r.check("cancel_leaves_the_shape_alone",
                sameConstructionShape(f.scene.activeBody().construction().captureState(),
                                      preState));
        r.check("cancel_records_no_step", f.history.undoDepth() == depthBefore);
        r.check("cancel_closes_the_edit", !f.history.editInProgress());
    }

    // -----------------------------------------------------------------------
    // Creation is one transaction, and a redo brings the SAME body back
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const ObjectId first = f.scene.activeBody().objectId();
        const ObjectId created = f.addBody(PrimitiveSpec::forSphere(0.9));
        r.check("creation_is_one_step", f.history.undoDepth() == 1);
        r.check("creation_selects_the_new_body", f.scene.activeBodyId() == created);
        r.check("creation_appends_in_order",
                orderedIds(f.scene) == std::vector<ObjectId>{first, created});
        r.check("creation_produces_the_chosen_primitive",
                f.scene.activeBody().construction().kind() == PrimitiveKind::Sphere);

        const ConstructionObjectState createdState =
            f.scene.activeBody().construction().captureState();
        const TransformValues createdPlacement = f.scene.activeBody().transform().values();

        r.check("creation_undo_reports_success", f.history.undo());
        r.check("creation_undo_removes_the_body", f.scene.bodyCount() == 1);
        r.check("creation_undo_leaves_no_default_box_remnant",
                f.scene.findBody(created) == nullptr);
        r.check("creation_undo_leaves_a_valid_active_body", f.scene.activeBodyId() == first);
        r.check("creation_undo_does_not_touch_the_surviving_body",
                f.scene.bodyAt(0).construction().kind() == PrimitiveKind::Box);

        r.check("creation_redo_reports_success", f.history.redo());
        r.check("creation_redo_restores_the_same_object_id",
                f.scene.bodyCount() == 2 && f.scene.findBody(created) != nullptr);
        r.check("creation_redo_restores_scene_position",
                orderedIds(f.scene) == std::vector<ObjectId>{first, created});
        r.check("creation_redo_restores_the_primitive_and_parameters",
                sameConstructionShape(f.scene.findBody(created)->construction().captureState(),
                                      createdState));
        r.check("creation_redo_restores_the_placement",
                sameConstructionPlacement(f.scene.findBody(created)->transform().values(),
                                          createdPlacement));
        r.check("creation_redo_reselects_the_restored_body",
                f.scene.activeBodyId() == created);
        r.check("creation_redo_republishes_the_body",
                f.scene.findBody(created)->meshStore().currentRevision() != kNoMeshRevision);
    }

    // -----------------------------------------------------------------------
    // The id allocator is never rolled back
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const ObjectId undone = f.addBody(PrimitiveSpec::forCone(0.7, 1.4));
        f.history.undo();
        const ObjectId minted = f.addBody(PrimitiveSpec::forPlane(2.0, 2.0));
        r.check("a_new_creation_after_an_undo_does_not_reuse_the_id", minted != undone);
        r.check("a_new_creation_after_an_undo_clears_the_redo", !f.history.canRedo());
    }

    // -----------------------------------------------------------------------
    // Multi-object isolation and chronological replay
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const ObjectId a = f.scene.activeBody().objectId();

        f.applyShape(PrimitiveSpec::forCylinder(1.1, 2.2));          // 1: A's shape
        const ConstructionObjectState aEdited =
            f.scene.findBody(a)->construction().captureState();

        const ObjectId b = f.addBody(PrimitiveSpec::forCapsule(0.6, 1.8));  // 2: create B
        f.applyPlacement(placement(4.0, 0.0, -2.0, 0.0, 45.0, 0.0));        // 3: B's placement
        const TransformValues bMoved = f.scene.findBody(b)->transform().values();

        r.check("three_edits_are_three_steps", f.history.undoDepth() == 3);

        r.check("undo_of_b_placement", f.history.undo());
        r.check("undo_of_b_placement_touches_only_b",
                sameConstructionShape(f.scene.findBody(a)->construction().captureState(), aEdited)
                    && f.scene.findBody(b)->transform().positionXMeters() == 0.0);

        r.check("undo_of_b_creation", f.history.undo());
        r.check("undo_of_b_creation_leaves_a_alone",
                f.scene.bodyCount() == 1
                    && sameConstructionShape(f.scene.findBody(a)->construction().captureState(),
                                             aEdited));

        r.check("undo_of_a_shape", f.history.undo());
        r.check("undo_of_a_shape_returns_the_default_box",
                f.scene.findBody(a)->construction().kind() == PrimitiveKind::Box);
        r.check("three_undos_empty_the_undo_stack",
                f.history.undoDepth() == 0 && f.history.redoDepth() == 3);

        r.check("redo_of_a_shape", f.history.redo());
        r.check("redo_of_a_shape_restores_a",
                sameConstructionShape(f.scene.findBody(a)->construction().captureState(),
                                      aEdited));
        r.check("redo_of_b_creation", f.history.redo());
        r.check("redo_of_b_creation_restores_the_ordered_ids",
                orderedIds(f.scene) == std::vector<ObjectId>{a, b});
        r.check("redo_of_b_placement", f.history.redo());
        r.check("redo_of_b_placement_restores_b",
                samePlacement(f.scene.findBody(b)->transform().values(), bMoved));
        r.check("three_redos_refill_the_undo_stack",
                f.history.undoDepth() == 3 && f.history.redoDepth() == 0);
    }

    // -----------------------------------------------------------------------
    // Redo invalidation, and what does NOT invalidate it
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.applyShape(PrimitiveSpec::forSphere(1.25));
        f.history.undo();
        r.check("redo_is_available_after_an_undo", f.history.canRedo());

        f.applyShape(PrimitiveSpec::forBox(-3.0, 1.0, 1.0));
        r.check("a_rejected_apply_does_not_destroy_the_redo", f.history.canRedo());
        f.applyShape(PrimitiveSpec::of(f.scene.activeBody().construction().box()
                                           .dimensionsMeters()));
        r.check("a_no_op_apply_does_not_destroy_the_redo", f.history.canRedo());
        f.history.beginEdit();
        f.history.commitEdit();
        r.check("an_empty_transaction_does_not_destroy_the_redo", f.history.canRedo());

        f.applyShape(PrimitiveSpec::forCone(1.0, 2.0));
        r.check("a_real_new_edit_destroys_the_redo",
                !f.history.canRedo() && f.history.redoDepth() == 0);
    }

    // -----------------------------------------------------------------------
    // Selection alone is not a history step
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const ObjectId a = f.scene.activeBody().objectId();
        const ObjectId b = f.addBody(PrimitiveSpec::forSphere(1.0));
        const size_t depth = f.history.undoDepth();
        {
            ScopedConstructionEdit edit(f.history);
            f.scene.setActiveBody(a);
        }
        r.check("selection_alone_records_no_step", f.history.undoDepth() == depth);
        r.check("selection_alone_leaves_the_selection_where_it_was_put",
                f.scene.activeBodyId() == a);
        {
            ScopedConstructionEdit edit(f.history);
            f.scene.setActiveBody(b);
        }
        r.check("selection_back_records_no_step", f.history.undoDepth() == depth);
    }

    // -----------------------------------------------------------------------
    // Bounded capacity, evicting the oldest first
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const size_t overflow = kConstructionHistoryCapacity + 8;
        for (size_t i = 0; i < overflow; ++i) {
            f.applyShape(PrimitiveSpec::forSphere(0.5 + 0.01 * static_cast<double>(i)));
        }
        r.check("history_is_bounded", f.history.undoDepth() == kConstructionHistoryCapacity);

        // The oldest steps are gone, so undoing every remaining one lands on the
        // state that step number `overflow - capacity` produced — NOT on the
        // startup box, which is the state the evicted steps knew about.
        const double oldestRetained =
            0.5 + 0.01 * static_cast<double>(overflow - kConstructionHistoryCapacity - 1);
        for (size_t i = 0; i < kConstructionHistoryCapacity; ++i) {
            f.history.undo();
        }
        r.check("eviction_is_deterministic_oldest_first",
                f.scene.activeBody().construction().sphere().diameterMeters() == oldestRetained);
        r.check("an_exhausted_undo_stack_stops_cleanly",
                !f.history.canUndo() && !f.history.undo());
        r.check("every_evicted_step_is_still_redoable",
                f.history.redoDepth() == kConstructionHistoryCapacity);
        for (size_t i = 0; i < kConstructionHistoryCapacity; ++i) {
            f.history.redo();
        }
        r.check("redo_after_eviction_reaches_the_final_state",
                f.scene.activeBody().construction().sphere().diameterMeters()
                    == 0.5 + 0.01 * static_cast<double>(overflow - 1));
    }

    // -----------------------------------------------------------------------
    // Sculpt separation
    // -----------------------------------------------------------------------
    {
        Fixture f;
        SceneObject& body = f.scene.activeBody();
        SculptSession session;
        session.bindTarget(&body.frozenSculpt());
        r.check("freeze_for_the_separation_case",
                session.freezeToSculpt(body.construction().generateMesh(), body.objectId()));

        const SculptRevision frozenRevision = body.frozenSculpt().mesh.revision();
        const size_t frozenVertices = body.frozenSculpt().mesh.vertexCount();
        const size_t frozenIndices = body.frozenSculpt().mesh.indexCount();
        r.check("a_freeze_writes_no_construction_history", f.history.undoDepth() == 0);
        r.check("a_freeze_leaves_the_source_current", !body.frozenSculpt().sourceStale);

        f.applyShape(PrimitiveSpec::forCylinder(1.0, 3.0));
        r.check("a_construction_edit_marks_the_sculpt_source_stale",
                body.frozenSculpt().sourceStale);
        r.check("a_construction_edit_does_not_touch_sculpt_geometry",
                body.frozenSculpt().mesh.revision() == frozenRevision
                    && body.frozenSculpt().mesh.vertexCount() == frozenVertices
                    && body.frozenSculpt().mesh.indexCount() == frozenIndices);

        f.history.undo();
        r.check("construction_undo_does_not_touch_sculpt_geometry",
                body.frozenSculpt().mesh.revision() == frozenRevision
                    && body.frozenSculpt().mesh.vertexCount() == frozenVertices
                    && body.frozenSculpt().mesh.indexCount() == frozenIndices);
        r.check("construction_undo_routes_through_the_stale_source_rule",
                body.frozenSculpt().sourceStale);

        f.history.redo();
        r.check("construction_redo_does_not_touch_sculpt_geometry",
                body.frozenSculpt().mesh.revision() == frozenRevision
                    && body.frozenSculpt().mesh.vertexCount() == frozenVertices
                    && body.frozenSculpt().mesh.indexCount() == frozenIndices);
    }

    // -----------------------------------------------------------------------
    // A restored body keeps the Frozen Sculpt Mesh it already had
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const ObjectId created = f.addBody(PrimitiveSpec::forSphere(1.0));
        SceneObject* body = f.scene.findBody(created);
        SculptSession session;
        session.bindTarget(&body->frozenSculpt());
        session.freezeToSculpt(body->construction().generateMesh(), body->objectId());
        session.enterConstruction();
        const SculptRevision frozenRevision = body->frozenSculpt().mesh.revision();
        const size_t frozenVertices = body->frozenSculpt().mesh.vertexCount();

        f.history.undo();
        f.history.redo();
        SceneObject* restored = f.scene.findBody(created);
        r.check("a_redone_body_is_the_same_object", restored != nullptr);
        r.check("a_redone_body_keeps_its_frozen_sculpt_mesh",
                restored != nullptr && restored->frozenSculpt().mesh.frozen()
                    && restored->frozenSculpt().mesh.revision() == frozenRevision
                    && restored->frozenSculpt().mesh.vertexCount() == frozenVertices);
    }

    // -----------------------------------------------------------------------
    // Restore does the least geometry work it can
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.applyPlacement(placement(2.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        ConstructionRestoreReport report;
        f.history.undo(&report);
        r.check("a_placement_undo_republishes_nothing", report.republishedBodies == 0);
        r.check("a_placement_undo_restores_one_placement", report.replacedPlacements == 1);

        f.applyShape(PrimitiveSpec::forSphere(2.0));
        ConstructionRestoreReport shapeReport;
        f.history.undo(&shapeReport);
        r.check("a_shape_undo_republishes_exactly_the_body_that_changed",
                shapeReport.republishedBodies == 1 && shapeReport.restoredBodies == 0
                    && shapeReport.removedBodies == 0);
    }

    // -----------------------------------------------------------------------
    // IMP01B-15/16/17/18/19/20/21: Delete is one real mutation, and one Undo
    // -----------------------------------------------------------------------
    //
    // Every case here drives `deleteSceneBody` -- the one production entry
    // point -- against a scene of its own, so nothing depends on what a live
    // session left behind and nothing restates a rule the operation owns.
    {
        Fixture f;
        const ObjectId first = f.scene.activeBody().objectId();
        const ObjectId second = f.addBody(PrimitiveSpec::forSphere(1.0));
        const ObjectId third = f.addBody(PrimitiveSpec::forCylinder(1.0, 2.0));
        const size_t depthBefore = f.history.undoDepth();

        // IMP01B-15: deleting a body that is NOT active leaves the selection
        // exactly where it was.
        f.scene.setActiveBody(third);
        DeleteBodyReport report;
        r.check("IMP01B_15_deleting_an_inactive_body_succeeds",
                deleteSceneBody(second, f.scene, f.history, &report) == DeleteBodyStatus::Ok);
        r.check("IMP01B_15_the_body_is_gone_from_the_scene",
                f.scene.findBody(second) == nullptr && f.scene.bodyCount() == 2);
        r.check("IMP01B_15_the_bodies_that_remain_keep_their_order",
                f.scene.bodyAt(0).objectId() == first && f.scene.bodyAt(1).objectId() == third);
        r.check("IMP01B_20_deleting_an_inactive_body_does_not_move_the_selection",
                f.scene.activeBodyId() == third);
        r.check("IMP01B_15_a_delete_is_exactly_one_history_step",
                f.history.undoDepth() == depthBefore + 1);
        r.check("IMP01B_15_the_report_names_what_left",
                report.removedBodyId == second && report.removedIndex == 1
                    && report.bodyCount == 2);

        // IMP01B-18: Undo restores the SAME object, in its own place.
        r.check("IMP01B_18_delete_undo_succeeds", f.history.undo());
        r.check("IMP01B_18_the_body_comes_back_where_it_was",
                f.scene.bodyCount() == 3 && f.scene.bodyAt(1).objectId() == second);
        r.check("IMP01B_18_and_the_selection_it_had", f.scene.activeBodyId() == third);

        // IMP01B-19: Redo removes the same body again.
        r.check("IMP01B_19_delete_redo_succeeds", f.history.redo());
        r.check("IMP01B_19_the_same_body_leaves_again",
                f.scene.findBody(second) == nullptr && f.scene.bodyCount() == 2);
    }

    // The deleted body is HELD, not rebuilt: an Imported Mesh and a Frozen
    // Sculpt Mesh are the two things a history step cannot recreate, so an undo
    // that came back with a fabricated body would come back empty.
    {
        Fixture f;
        const ObjectId first = f.scene.activeBody().objectId();
        const ObjectId sculpted = f.addBody(PrimitiveSpec::forSphere(1.0));
        SceneObject* body = f.scene.findBody(sculpted);
        SculptSession session;
        session.bindTarget(&body->frozenSculpt());
        session.freezeToSculpt(body->construction().generateMesh(), body->objectId());
        session.enterConstruction();
        const SculptRevision revision = body->frozenSculpt().mesh.revision();
        const uint32_t vertices = body->frozenSculpt().mesh.vertexCount();
        const uint32_t indices = body->frozenSculpt().mesh.indexCount();
        const MeshRevision published = body->meshStore().currentRevision();

        r.check("IMP01B_17_deleting_a_body_with_a_sculpt_mesh_succeeds",
                deleteSceneBody(sculpted, f.scene, f.history) == DeleteBodyStatus::Ok);
        r.check("IMP01B_17_and_takes_its_sculpt_state_out_of_the_scene_with_it",
                f.scene.findBody(sculpted) == nullptr);
        r.check("IMP01B_20_the_selection_falls_to_a_body_that_still_exists",
                f.scene.findBody(f.scene.activeBodyId()) != nullptr);

        f.history.undo();
        SceneObject* restored = f.scene.findBody(sculpted);
        r.check("IMP01B_18_the_deleted_body_returns", restored != nullptr);
        r.check("IMP01B_18_with_the_same_frozen_sculpt_mesh",
                restored != nullptr && restored->frozenSculpt().mesh.frozen()
                    && restored->frozenSculpt().mesh.revision() == revision
                    && restored->frozenSculpt().mesh.vertexCount() == vertices
                    && restored->frozenSculpt().mesh.indexCount() == indices);
        r.check("IMP01B_18_and_the_same_published_revision_it_already_had",
                restored != nullptr && restored->meshStore().currentRevision() == published);
        r.check("IMP01B_18_and_the_body_it_sat_beside_is_untouched",
                f.scene.findBody(first) != nullptr);
    }

    // An Imported Mesh deletes and comes back exactly as a Construction Body
    // does. This is the case a per-representation delete path would have
    // broken: its geometry is not derived from anything a step holds.
    {
        Fixture f;
        SceneObject* imported = f.scene.addImportedBody(historyImportedMesh(), "head_low");
        r.check("IMP01B_16_an_imported_body_can_be_added_for_the_case",
                imported != nullptr && imported->isImported());
        const ObjectId importedId = (imported != nullptr) ? imported->objectId() : kNoObject;
        const uint32_t importedVertices =
                (imported != nullptr) ? imported->importedOrNull()->vertexCount() : 0u;
        if (imported != nullptr) {
            publishSceneObject(*imported);
        }

        r.check("IMP01B_16_deleting_an_imported_body_succeeds",
                deleteSceneBody(importedId, f.scene, f.history) == DeleteBodyStatus::Ok);
        r.check("IMP01B_16_it_is_gone", f.scene.findBody(importedId) == nullptr);

        f.history.undo();
        SceneObject* restored = f.scene.findBody(importedId);
        r.check("IMP01B_18_the_imported_body_returns_with_its_geometry",
                restored != nullptr && restored->isImported()
                    && restored->importedOrNull() != nullptr
                    && restored->importedOrNull()->vertexCount() == importedVertices);
        r.check("IMP01B_18_and_still_carries_its_name",
                restored != nullptr && restored->name() == "head_low");

        f.history.redo();
        r.check("IMP01B_19_and_a_redo_removes_the_same_imported_body",
                f.scene.findBody(importedId) == nullptr);
    }

    // IMP01B-20: the replacement selection, both halves of the rule.
    {
        Fixture f;
        const ObjectId first = f.scene.activeBody().objectId();
        const ObjectId second = f.addBody(PrimitiveSpec::forSphere(1.0));
        const ObjectId third = f.addBody(PrimitiveSpec::forCylinder(1.0, 2.0));

        f.scene.setActiveBody(second);
        deleteSceneBody(second, f.scene, f.history);
        r.check("IMP01B_20_deleting_the_active_body_selects_the_next_one",
                f.scene.activeBodyId() == third);

        f.scene.setActiveBody(third);
        deleteSceneBody(third, f.scene, f.history);
        r.check("IMP01B_20_and_the_previous_one_when_it_was_last",
                f.scene.activeBodyId() == first);
    }

    // IMP01B-21: the last body is refused BY NAME, and the refusal costs
    // nothing at all.
    {
        Fixture f;
        const ObjectId only = f.scene.activeBody().objectId();
        const size_t depthBefore = f.history.undoDepth();
        const ObjectId nextIdBefore = f.scene.nextObjectId();
        DeleteBodyReport report;
        r.check("IMP01B_21_the_last_body_is_refused_by_name",
                deleteSceneBody(only, f.scene, f.history, &report)
                        == DeleteBodyStatus::RefusedLastBody);
        r.check("IMP01B_21_and_the_refusal_changes_nothing",
                f.scene.bodyCount() == 1 && f.scene.activeBodyId() == only
                    && f.history.undoDepth() == depthBefore
                    && f.scene.nextObjectId() == nextIdBefore);
        r.check("IMP01B_21_the_status_has_a_stable_name",
                sameText(deleteBodyStatusName(DeleteBodyStatus::RefusedLastBody),
                         "RefusedLastBody"));

        // And the other two refusals, on the same terms.
        r.check("IMP01B_21_an_unknown_id_is_refused",
                deleteSceneBody(9999, f.scene, f.history) == DeleteBodyStatus::UnknownBody);
        f.history.beginEdit();
        r.check("IMP01B_21_a_delete_under_an_open_edit_is_refused",
                deleteSceneBody(only, f.scene, f.history)
                        == DeleteBodyStatus::RefusedEditInProgress);
        f.history.commitEdit();
    }

    // The ObjectId allocator is never rolled back by a delete: a deleted id
    // comes back BY NAME through the undo, and the next creation mints a fresh
    // one rather than reusing it.
    {
        Fixture f;
        const ObjectId second = f.addBody(PrimitiveSpec::forSphere(1.0));
        const ObjectId nextBefore = f.scene.nextObjectId();
        deleteSceneBody(second, f.scene, f.history);
        r.check("IMP01B_15_a_delete_does_not_roll_the_allocator_back",
                f.scene.nextObjectId() == nextBefore);
        const ObjectId created = f.addBody(PrimitiveSpec::forCylinder(1.0, 2.0));
        r.check("IMP01B_15_and_the_next_creation_does_not_reuse_the_deleted_id",
                created != second && created >= nextBefore);
    }

    // -----------------------------------------------------------------------
    // Clearing forgets the history without moving the scene
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.applyShape(PrimitiveSpec::forSphere(1.75));
        const ConstructionObjectState state = f.scene.activeBody().construction().captureState();
        f.history.clear();
        r.check("clear_empties_both_stacks",
                !f.history.canUndo() && !f.history.canRedo());
        r.check("clear_does_not_move_the_scene",
                sameConstructionShape(f.scene.activeBody().construction().captureState(), state));
    }

    return r.n;
}

}  // namespace forgeshape
