#include "forgeshape_cad_r2_selftest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_sketch_inventory.h"
#include "forgeshape_cad_timeline.h"
#include "forgeshape_camera.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_input.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_support_chooser.h"

namespace forgeshape {

namespace {

constexpr int kW = 1000;
constexpr int kH = 1000;

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool ok) {
        out->push_back(ArrangementSelfTestCheck{name, ok});
        if (!ok) {
            std::printf("CADR2_CHECK_FAILED %s\n", name);
        }
    }
};

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

SketchRectangle rect(double cu, double cv, double w, double h) {
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

SketchLine lineOf(double u0, double v0, double u1, double v1) {
    SketchLine l;
    l.start = SketchPoint{u0, v0};
    l.end = SketchPoint{u1, v1};
    return l;
}

SketchEntityId add(CadSketch* sketch, SketchEntity::Payload payload) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, std::move(payload), &id);
    return id;
}

ExtrudeFeature oneSide(SketchEntityId profile, double depth,
                       ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = profile;
    extrude.depth = depth;
    extrude.direction = direction;
    return extrude;
}

ExtrudeFeature faceExtrude(std::vector<PlanarFaceRef> faces, double depth) {
    ExtrudeFeature e;
    e.selection = CadSelectionKind::PlanarFaces;
    e.planarFaces = std::move(faces);
    e.profileEntityId = kNoSketchEntity;
    e.depth = depth;
    return e;
}

CadFaceToken cap(CadFaceKind kind) {
    CadFaceToken token;
    token.kind = kind;
    return token;
}

CadFeatureSupport faceSupport(const CadBodyState& state, uint32_t featureId, CadFaceToken token) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face = token;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

// The shop body: a 2 x 2 x 1 block on XY (feature 1, sketch 1), a 0.6 m boss
// Added 0.5 m on its top at (-0.5, 0.5) (feature 2, sketch 2), and a 0.6 m
// pocket Cut 0.4 m into its top at (0.5, -0.5) (feature 3, sketch 3).
// Volume 4 + 0.18 - 0.144 = 4.036 m^3. Legacy-representable (`CADB` v5).
constexpr double kShopVolume = 4.0 + 0.18 - 0.144;

CadBodyState shopState() {
    CadSketch base;
    const SketchEntityId baseRect = add(&base, rect(0.0, 0.0, 2.0, 2.0));
    CadBodyState state = makeCadBodyState(base, oneSide(baseRect, 1.0));
    CadSketch boss;
    const SketchEntityId bossRect = add(&boss, rect(-0.5, 0.5, 0.6, 0.6));
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Add,
                                    faceSupport(state, kCadFeatureId, cap(CadFaceKind::CapFar)),
                                    std::move(boss), oneSide(bossRect, 0.5));
    CadSketch pocket;
    const SketchEntityId pocketRect = add(&pocket, rect(0.5, -0.5, 0.6, 0.6));
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut,
                                    faceSupport(state, kCadFeatureId, cap(CadFaceKind::CapFar)),
                                    std::move(pocket),
                                    oneSide(pocketRect, 0.4, ExtrudeDirection::AgainstNormal));
    return state;
}

double volumeOf(const CadBodyState& state) {
    CadBodyMesh mesh;
    if (regenerateCadBody(state, &mesh) != CadStatus::Ok) return -1.0;
    return mesh.volume;
}

double bodyVolume(const ConstructionScene& scene, ObjectId id) {
    const SceneObject* body = scene.findBody(id);
    return body != nullptr && body->cadOrNull() != nullptr ? volumeOf(body->cadOrNull()->state())
                                                           : -1.0;
}

const CadBodyState* bodyState(const ConstructionScene& scene, ObjectId id) {
    const SceneObject* body = scene.findBody(id);
    return body != nullptr && body->cadOrNull() != nullptr ? &body->cadOrNull()->state() : nullptr;
}

const CadSketchInventoryRow* rowFor(const CadSketchInventory& inventory, CadSketchId id) {
    for (const CadSketchInventoryRow& row : inventory.rows) {
        if (row.sketchId == id) return &row;
    }
    return nullptr;
}

// Sketch-local (a, b) of a body-local point, through the sketch's own
// placement: what the user sees under the finger.
SketchPoint localOn(const CadFrame64& frame, double x, double y, double z) {
    const DVec3 d{x - frame.origin.x, y - frame.origin.y, z - frame.origin.z};
    return SketchPoint{d.x * frame.u.x + d.y * frame.u.y + d.z * frame.u.z,
                       d.x * frame.v.x + d.y * frame.v.y + d.z * frame.v.z};
}

SketchFrame worldFrameOf(const CadFrame64& f) {
    SketchFrame frame;
    frame.origin = vec3FromDVec3(f.origin);
    frame.u = vec3FromDVec3(f.u);
    frame.v = vec3FromDVec3(f.v);
    frame.n = vec3FromDVec3(f.n);
    return frame;
}

// The sketch session driven through the gestures the product uses: an entity
// is drawn by a real drag and then TYPED exact (the OSS driver).
struct Driver {
    SketchSession sketch;
    CameraController camera;

    void frameOnSession() {
        camera.setViewport(kW, kH);
        const SketchFrame& f = sketch.frame();
        camera.frameSketchView(f.origin, f.u, f.v, f.n);
    }
    bool at(TouchAction action, float x, float y) {
        TouchPointer p{7, x, y};
        return sketch.onTouch(action, action == TouchAction::Up ? 7 : -1, &p, 1, camera.snapshot(),
                              kW, kH);
    }
    bool place(SketchTool tool, SketchEntity::Payload exact) {
        const SketchPoint from{0.13, 0.27};
        const SketchPoint to{0.61, 0.83};
        sketch.setTool(tool);
        float x0, y0, x1, y1;
        if (!sketch.sketchToScreen(camera.snapshot(), from, kW, kH, &x0, &y0)
            || !sketch.sketchToScreen(camera.snapshot(), to, kW, kH, &x1, &y1)) {
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
};

// A camera looking straight down -n at `origin`.
CameraSnapshot lookAlong(const Vec3& origin, Vec3 n, Vec3 seedU) {
    n = vec3Normalize(n);
    Vec3 u = vec3Sub(seedU, vec3Scale(n, vec3Dot(seedU, n)));
    u = vec3Normalize(u);
    const Vec3 v = vec3Cross(n, u);
    CameraController camera;
    camera.setViewport(kW, kH);
    camera.frameSketchView(origin, u, v, n);
    return camera.snapshot();
}

// The support chooser exactly as New Sketch drives it.
bool pickAt(const ConstructionScene& scene, const CameraSnapshot& camera, const Vec3& world,
            ChosenSupport* out) {
    float x = 0.0f;
    float y = 0.0f;
    if (!projectWorldToScreen(camera, world, kW, kH, &x, &y)) return false;
    SupportChooser& chooser = supportChooser();
    chooser.begin(/*allowFaces=*/true);
    const ChosenSupport picked = chooser.select(camera, x, y, kW, kH, scene);
    chooser.cancel();
    if (picked.kind != ChosenSupport::Kind::Face) return false;
    return refreshChosenSupport(scene, picked, out) == CadStatus::Ok;
}

// A one-body scene holding `state` at the identity placement.
struct Shop {
    ConstructionScene scene{NoProjectTag{}};
    ConstructionHistory history{scene};
    ObjectId id = kNoObject;

    explicit Shop(const CadBodyState& state) {
        SceneObject* body = scene.addCadBody(state);
        if (body != nullptr) {
            id = body->objectId();
            publishSceneObject(*body);
        }
    }
    const CadBodyState& state() const { return *bodyState(scene, id); }
    uint64_t fingerprint() const {
        return projectSemanticFingerprint(scene, ProjectKind::Construction);
    }
};

// New Sketch on the face under `world` (seen down -n), a w x w square centred
// on `world`, then Keep Sketch. Returns the kept id or kNoCadSketch.
CadSketchId keepSquareOn(Shop& shop, const Vec3& world, const Vec3& n, double w,
                         CadStatus* outWhy = nullptr) {
    ChosenSupport chosen;
    const Vec3 seed = std::fabs(n.x) > 0.9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    if (!pickAt(shop.scene, lookAlong(world, n, seed), world, &chosen)) {
        if (outWhy) *outWhy = CadStatus::FeatureSupportInvalid;
        std::printf("CADR2_PICK_MISSED at (%.2f, %.2f, %.2f)\n", world.x, world.y, world.z);
        return kNoCadSketch;
    }
    Driver d;
    CadStatus why = d.sketch.beginOnFace(chosen.worldFrame, chosen.faceRef, &shop.state());
    if (why == CadStatus::Ok) {
        d.frameOnSession();
        const SketchFrame& f = d.sketch.frame();
        const Vec3 rel = vec3Sub(world, f.origin);
        const double a = vec3Dot(rel, f.u);
        const double b = vec3Dot(rel, f.v);
        if (!d.place(SketchTool::Rectangle, rect(a, b, w, w))) why = CadStatus::ProfileNotFound;
    }
    CadSketchId kept = kNoCadSketch;
    if (why == CadStatus::Ok) {
        why = d.sketch.canKeepSketch() ? d.sketch.commitSketch(shop.scene, shop.history, &kept)
                                       : CadStatus::NotSketching;
    }
    if (outWhy) *outWhy = why;
    if (why != CadStatus::Ok) {
        std::printf("CADR2_KEEP_REFUSED %s at (%.2f, %.2f, %.2f)\n", cadStatusName(why), world.x,
                    world.y, world.z);
    }
    return kept;
}

// A grid sketch: an `n` x `n` square of side `n` metres centred on the origin,
// split into unit cells by n - 1 vertical and n - 1 horizontal lines whose
// ends lie ON the outline (T-junctions), so the arrangement has n^2 cells.
CadSketch gridSketch(int n) {
    CadSketch s;
    const double half = n * 0.5;
    add(&s, rect(0.0, 0.0, n, n));
    for (int i = 1; i < n; ++i) {
        const double c = -half + i;
        add(&s, lineOf(c, -half, c, half));
        add(&s, lineOf(-half, c, half, c));
    }
    return s;
}

// The arrangement face whose interior holds (u, v), by its own area-weighted
// test: the face whose merged one-face shape contains the point.
int cellAt(const SketchArrangement& arrangement, double u, double v) {
    for (size_t i = 0; i < arrangement.faces.size(); ++i) {
        std::vector<PlanarProfileComponent> shape;
        if (mergePlanarFaces(arrangement, {i}, &shape) != ArrangementStatus::Ok
            || shape.size() != 1u) {
            continue;
        }
        const std::vector<SketchPoint>& poly = shape[0].outer.polygon;
        bool inside = false;
        for (size_t a = 0, b = poly.size() - 1; a < poly.size(); b = a++) {
            if (((poly[a].v > v) != (poly[b].v > v))
                && (u < (poly[b].u - poly[a].u) * (v - poly[a].v) / (poly[b].v - poly[a].v)
                                + poly[a].u)) {
                inside = !inside;
            }
        }
        if (inside) return static_cast<int>(i);
    }
    return -1;
}

std::vector<PlanarFaceRef> refsOf(const SketchArrangement& arrangement, std::vector<int> cells) {
    std::sort(cells.begin(), cells.end());
    std::vector<PlanarFaceRef> refs;
    for (int c : cells) refs.push_back(arrangement.faces[static_cast<size_t>(c)].ref);
    return refs;
}

// The cells of a grid of n, by (column, row) from the lower left.
int gridCell(const SketchArrangement& arrangement, int n, int column, int row) {
    return cellAt(arrangement, -n * 0.5 + column + 0.5, -n * 0.5 + row + 0.5);
}

// ===========================================================================
// R2-SK: the inventory and the lifecycle of a retained sketch
// ===========================================================================

void testInventory(Checks& r) {
    const CadBodyState shop = shopState();
    const CadSketchInventory inv = buildCadSketchInventoryFor(shop);
    bool ordered = inv.rows.size() == 3u && inv.nextSketchId == 4u;
    for (size_t i = 0; ordered && i < inv.rows.size(); ++i) {
        const CadSketchInventoryRow& row = inv.rows[i];
        ordered = row.sketchId == i + 1u && row.ordinal == i + 1u && row.consumers.size() == 1u
                  && row.consumers[0] == i + 1u && row.state == CadSketchRowState::Ok
                  && row.root == (i == 0u) && (row.acts & kCadSketchActEdit) != 0u;
    }
    r.check("R2_SK_01_inventory_lists_every_sketch_by_id_with_its_consumer_in_chain_order", ordered);

    const bool supports = inv.rows.size() == 3u
                          && inv.rows[0].support == CadSketchSupportKind::WorldPlane
                          && inv.rows[0].plane == Workplane::XY
                          && inv.rows[1].support == CadSketchSupportKind::FeatureFace
                          && inv.rows[1].supportFeatureId == kCadFeatureId
                          && inv.rows[1].supportFace.kind == CadFaceKind::CapFar
                          && inv.rows[2].support == CadSketchSupportKind::FeatureFace
                          && inv.rows[0].entityCount == 1u;
    r.check("R2_SK_02_inventory_states_where_each_sketch_stands", supports);

    const bool deletes = inv.rows.size() == 3u
                         && (inv.rows[0].acts & kCadSketchActDelete) == 0u
                         && inv.rows[0].deleteRefusal == CadStatus::SketchIsRoot
                         && (inv.rows[1].acts & kCadSketchActDelete) == 0u
                         && inv.rows[1].deleteRefusal == CadStatus::SketchInUse
                         && (inv.rows[2].acts & kCadSketchActReuseCut) != 0u;
    r.check("R2_SK_03_root_and_consumed_sketches_offer_no_delete_and_say_why", deletes);

    // The inventory's ordinal is the timeline's, sketch for sketch, and the
    // body is a legacy (v5) one -- the inventory needs nothing v6 to read it.
    const CadTimeline timeline = buildCadTimelineFor(shop);
    bool sameOrdinals = cadBodyStateLegacyRepresentable(shop);
    for (const CadTimelineRow& row : timeline.rows) {
        if (row.kind != CadTimelineRowKind::Sketch) continue;
        const CadSketchInventoryRow* mine = rowFor(inv, row.id);
        sameOrdinals = sameOrdinals && mine != nullptr && mine->ordinal == row.ordinal;
    }
    // A revolved body: Edit offered, reuse refused by the R1 rule's own name.
    CadSketch revolveSketch;
    const SketchEntityId square = add(&revolveSketch, rect(1.5, 0.5, 1.0, 1.0));
    add(&revolveSketch, lineOf(0.0, -1.0, 0.0, 2.0));
    RevolveFeature revolve;
    revolve.profileEntityId = square;
    revolve.axis = CadSketchEdgeRef{2, 0};
    revolve.angleDegrees = 360.0;
    const CadSketchInventory revolved =
            buildCadSketchInventoryFor(makeCadRevolveBodyState(revolveSketch, revolve));
    r.check("R2_SK_04_legacy_and_revolve_bodies_read_with_timeline_ordinals_and_named_refusals",
            sameOrdinals && revolved.rows.size() == 1u
                    && (revolved.rows[0].acts & kCadSketchActEdit) != 0u
                    && (revolved.rows[0].acts & kCadSketchActReuseAdd) == 0u
                    && revolved.rows[0].reuseRefusal == CadStatus::RevolveLaterFeatureUnsupported);
}

void testKeepAndEdit(Checks& r, double* keepMicros) {
    Shop shop(shopState());
    const uint64_t before = shop.fingerprint();
    const size_t undoBefore = shop.history.undoDepth();

    // R2-SK-05: a sketch on the base's top, kept without a feature.
    const auto t0 = std::chrono::steady_clock::now();
    CadStatus why = CadStatus::Ok;
    const CadSketchId kept = keepSquareOn(shop, Vec3{0.5f, 0.5f, 1.0f}, Vec3{0, 0, 1}, 0.4, &why);
    *keepMicros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                          .count();
    const CadBodyState& after = shop.state();
    const CadSketchInventory inv = buildCadSketchInventoryFor(after);
    const CadSketchInventoryRow* row = rowFor(inv, kept);
    r.check("R2_SK_05_keep_sketch_on_a_body_face_retains_an_unused_sketch_and_no_feature",
            why == CadStatus::Ok && kept == 4u && after.sketches.size() == 4u
                    && cadFeatureCount(after) == 3u && after.nextSketchId == 5u
                    && near(bodyVolume(shop.scene, shop.id), kShopVolume, 1e-9)
                    && shop.history.undoDepth() == undoBefore + 1u && row != nullptr
                    && row->state == CadSketchRowState::Unused && row->consumers.empty()
                    && row->support == CadSketchSupportKind::FeatureFace
                    && (row->acts & kCadSketchActDelete) != 0u
                    && (row->acts & kCadSketchActReuseAdd) != 0u && shop.fingerprint() != before
                    && !cadBodyStateLegacyRepresentable(after));

    // R2-SK-06 (BLOCKED-DESIGN-DECISION reproducer): a second ROOT sketch on a
    // world plane is refused by the table rule; a world-plane session offers no
    // Keep Sketch and refuses one.
    {
        CadBodyState twoRoots = shopState();
        CadSketch extra;
        add(&extra, rect(0.0, 0.0, 0.5, 0.5));
        const CadSketchId id = addCadSketchRecord(&twoRoots, extra, nullptr);
        SketchSession world;
        world.begin(Workplane::XZ);
        r.check("R2_SK_06_a_world_plane_retained_sketch_in_an_existing_body_is_refused_by_name",
                id != kNoCadSketch
                        && validateCadBodyState(twoRoots) == CadStatus::SketchSupportInvalid
                        && !world.canKeepSketch()
                        && world.commitSketch(shop.scene, shop.history) == CadStatus::NotSketching);
    }

    // R2-SK-07: an empty sketch is not kept; an open Construction edit refuses.
    {
        ChosenSupport chosen;
        const bool picked = pickAt(shop.scene, lookAlong(Vec3{0.6f, 0.6f, 1.0f}, Vec3{0, 0, 1},
                                                         Vec3{1, 0, 0}),
                                   Vec3{0.6f, 0.6f, 1.0f}, &chosen);
        Driver d;
        const bool begun = picked && d.sketch.beginOnFace(chosen.worldFrame, chosen.faceRef,
                                                          &shop.state()) == CadStatus::Ok;
        const size_t depth = shop.history.undoDepth();
        const bool emptyRefused = begun && !d.sketch.canKeepSketch()
                                  && d.sketch.commitSketch(shop.scene, shop.history)
                                             == CadStatus::NoClosedProfile;
        d.frameOnSession();
        const bool placed = d.place(SketchTool::Rectangle, rect(0.0, 0.0, 0.1, 0.1));
        shop.history.beginEdit();
        const bool editRefused = d.sketch.commitSketch(shop.scene, shop.history)
                                 == CadStatus::RefusedEditInProgress;
        shop.history.commitEdit();
        d.sketch.cancel();
        r.check("R2_SK_07_keep_sketch_refuses_an_empty_sketch_and_an_open_edit_recording_nothing",
                emptyRefused && placed && editRefused && shop.history.undoDepth() == depth
                        && shop.state().sketches.size() == 4u);
    }

    // R2-SK-08: the unused sketch, edited SKETCH-ONLY: Finish is refused, no
    // Revolve is offered, Keep Sketch replaces its entities as one Undo.
    if (findCadSketchRecord(shop.state(), kept) == nullptr) {
        r.check("R2_SK_08_an_unused_sketch_edits_sketch_only_and_keep_commits_one_undo", false);
    } else {
        CadFrame64 placement;
        cadSketchPlacement(shop.state(), kept, &placement);
        Driver d;
        const CadStatus begun = d.sketch.beginEditSketch(shop.id, shop.state(), kept,
                                                         worldFrameOf(placement));
        const CadSketch* stored = &findCadSketchRecord(shop.state(), kept)->sketch;
        const SketchEntityId entity = stored->entities.front().id();
        const SketchRectangle old = *stored->entities.front().rectangle();
        const bool only = begun == CadStatus::Ok && d.sketch.sketchOnly()
                          && d.sketch.editingSketchId() == kept && d.sketch.canKeepSketch()
                          && d.sketch.finish() == CadStatus::NotSketching
                          && !d.sketch.revolveAvailable();
        const bool replaced =
                d.sketch.replaceEntity(entity, rect(old.center.u, old.center.v, 0.5, 0.3))
                == CadStatus::Ok;
        const size_t depth = shop.history.undoDepth();
        CadSketchId id = kNoCadSketch;
        const CadStatus committed = d.sketch.commitSketch(shop.scene, shop.history, &id);
        if (committed != CadStatus::Ok) {
            std::printf("CADR2_SK08 begun=%s commit=%s only=%d replaced=%d\n", cadStatusName(begun),
                        cadStatusName(committed), only ? 1 : 0, replaced ? 1 : 0);
        }
        const auto widthOf = [&]() {
            const CadSketchRecord* rec = findCadSketchRecord(shop.state(), kept);
            return rec != nullptr && !rec->sketch.entities.empty()
                                   && rec->sketch.entities.front().rectangle() != nullptr
                           ? *rec->sketch.entities.front().rectangle()
                           : SketchRectangle{};
        };
        const SketchRectangle now = widthOf();
        if (committed == CadStatus::Ok) shop.history.undo();
        const SketchRectangle undone = widthOf();
        if (committed == CadStatus::Ok) shop.history.redo();
        r.check("R2_SK_08_an_unused_sketch_edits_sketch_only_and_keep_commits_one_undo",
                only && replaced && committed == CadStatus::Ok && id == kept
                        && shop.history.undoDepth() == depth + 1u && now.width == 0.5
                        && now.height == 0.3 && undone.width == 0.4
                        && cadFeatureCount(shop.state()) == 3u);
    }
}

void testReuseAndDelete(Checks& r, double* reuseMicros) {
    Shop shop(shopState());
    const CadSketchId kept = keepSquareOn(shop, Vec3{0.5f, 0.5f, 1.0f}, Vec3{0, 0, 1}, 0.4);
    CadFrame64 placement;
    cadSketchPlacement(shop.state(), kept, &placement);
    const SketchFrame frame = worldFrameOf(placement);

    // R2-SK-09: reuse as an Add: Ready at once, Back to Sketch refused, no copy.
    const double v0 = bodyVolume(shop.scene, shop.id);
    Driver d;
    const auto t0 = std::chrono::steady_clock::now();
    const CadStatus begun = d.sketch.beginReuseSketch(shop.id, shop.state(), kept,
                                                      CadFeatureOperation::Add, frame);
    const bool ready = begun == CadStatus::Ok && d.sketch.state() == SketchSessionState::Ready
                       && d.sketch.reuseSketchId() == kept && !d.sketch.canReturnToSketch()
                       && !d.sketch.operationAvailable(CadFeatureOperation::NewBody)
                       && d.sketch.operationAvailable(CadFeatureOperation::Cut)
                       && !d.sketch.revolveAvailable() && !d.sketch.canKeepSketch();
    d.sketch.backToEditing();
    const bool stayed = d.sketch.state() == SketchSessionState::Ready
                        && d.sketch.lastStatus() == CadStatus::SketchInUse;
    d.sketch.setExtrude(0.3, d.sketch.extrude().direction);
    const CadCandidateEvaluation& preview = d.sketch.evaluateCandidate();
    const double previewVolume = preview.mesh != nullptr ? preview.mesh->volume : -1.0;
    ObjectId outId = kNoObject;
    const CadStatus committed = d.sketch.commit(shop.scene, shop.history, &outId);
    *reuseMicros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                           .count();
    const CadBodyState& afterAdd = shop.state();
    const double addVolume = 0.4 * 0.4 * 0.3;
    r.check("R2_SK_09_reuse_stages_a_new_add_on_the_existing_sketch_by_id_with_no_copy",
            ready && stayed && committed == CadStatus::Ok && outId == shop.id
                    && cadFeatureCount(afterAdd) == 4u && afterAdd.sketches.size() == 4u
                    && afterAdd.laterFeatures.back().sketchId == kept
                    && afterAdd.laterFeatures.back().operation == CadFeatureOperation::Add
                    && near(previewVolume, bodyVolume(shop.scene, shop.id), 0.0)
                    && near(bodyVolume(shop.scene, shop.id) - v0, addVolume, 1e-9));

    // R2-SK-10: the same sketch again, as a Cut into the material: two
    // consumers, one record, Delete refused.
    Driver c;
    const CadStatus cutBegun = c.sketch.beginReuseSketch(shop.id, shop.state(), kept,
                                                         CadFeatureOperation::Cut, frame);
    const bool intoMaterial = c.sketch.extrude().direction == ExtrudeDirection::AgainstNormal;
    c.sketch.setExtrude(0.2, c.sketch.extrude().direction);
    const CadStatus cut = c.sketch.commit(shop.scene, shop.history, &outId);
    const CadSketchInventory inv = buildCadSketchInventoryFor(shop.state());
    const CadSketchInventoryRow* row = rowFor(inv, kept);
    // The Add stands 0.3 above z = 1 and the Cut removes z in [0.8, 1]: the
    // Add's own volume stays, the block loses the 0.2 under the footprint.
    r.check("R2_SK_10_one_sketch_drives_an_add_and_a_cut_and_cannot_be_deleted_while_used",
            cutBegun == CadStatus::Ok && intoMaterial && cut == CadStatus::Ok && row != nullptr
                    && row->consumers.size() == 2u && row->consumers[0] == 4u
                    && row->consumers[1] == 5u && row->deleteRefusal == CadStatus::SketchInUse
                    && shop.state().sketches.size() == 4u
                    && near(bodyVolume(shop.scene, shop.id) - v0, addVolume - 0.4 * 0.4 * 0.2,
                            1e-9));

    // R2-SK-11: editing the SHARED sketch regenerates both consumers in one Finish.
    {
        Driver e;
        const CadStatus begunEdit = e.sketch.beginEditSketch(shop.id, shop.state(), kept, frame);
        const bool viaFirst = begunEdit == CadStatus::Ok && e.sketch.editingFeatureId() == 4u
                              && !e.sketch.sketchOnly();
        const SketchEntityId entity = e.sketch.sketch().entities.front().id();
        const SketchRectangle old =
                *e.sketch.sketch().entities.front().rectangle();
        e.sketch.replaceEntity(entity, rect(old.center.u, old.center.v, 0.5, 0.5));
        const CadStatus finished = e.sketch.finish();
        const CadStatus committed2 = e.sketch.commitEdit(shop.scene, shop.history);
        r.check("R2_SK_11_editing_a_shared_sketch_regenerates_every_consumer_in_one_step",
                viaFirst && finished == CadStatus::Ok && committed2 == CadStatus::Ok
                        && near(bodyVolume(shop.scene, shop.id) - v0,
                                0.5 * 0.5 * 0.3 - 0.5 * 0.5 * 0.2, 1e-9));
    }

    // R2-SK-12: a shared-sketch edit that breaks a consumer is refused whole,
    // naming the FIRST failing feature; nothing is written.
    {
        const CadBodyState stateBefore = shop.state();
        const uint64_t fingerprint = shop.fingerprint();
        Driver e;
        e.sketch.beginEditSketch(shop.id, shop.state(), kept, frame);
        const SketchEntityId entity = e.sketch.sketch().entities.front().id();
        // Far off the block: the Add becomes disjoint (feature 4 fails first).
        e.sketch.replaceEntity(entity, rect(40.0, 40.0, 0.5, 0.5));
        e.sketch.finish();
        const CadCandidateEvaluation evaluation = e.sketch.evaluateCandidate();  // a copy: cancel clears it
        const CadTimeline staged =
                buildCadTimeline(e.sketch.candidateState(),
                                 CadRegenerationReport{evaluation.status, evaluation.failedFeatureId},
                                 true, e.sketch.editingFeatureId());
        bool laterNotRegenerated = false;
        for (const CadTimelineRow& t : staged.rows) {
            if (t.kind == CadTimelineRowKind::Feature && t.id == 5u) {
                laterNotRegenerated = t.state == CadTimelineRowState::NotRegenerated;
            }
        }
        const CadStatus refused = e.sketch.commitEdit(shop.scene, shop.history);
        e.sketch.cancel();
        r.check("R2_SK_12_a_breaking_shared_edit_names_the_first_failure_and_writes_nothing",
                evaluation.status == CadStatus::AddDisjoint && evaluation.failedFeatureId == 4u
                        && laterNotRegenerated && refused == CadStatus::AddDisjoint
                        && sameCadBodyState(shop.state(), stateBefore)
                        && shop.fingerprint() == fingerprint);
    }

    // Reuse refusals: New Body, an unknown id, a full chain.
    {
        Driver e;
        const CadStatus newBody = e.sketch.beginReuseSketch(shop.id, shop.state(), kept,
                                                            CadFeatureOperation::NewBody, frame);
        const CadStatus unknown = e.sketch.beginReuseSketch(shop.id, shop.state(), 77u,
                                                            CadFeatureOperation::Add, frame);
        CadBodyState full = shop.state();
        while (cadFeatureCount(full) < kMaxCadFeatures) {
            appendCadLaterFeature(&full, CadFeatureOperation::Add, kept, oneSide(1, 0.01));
        }
        r.check("R2_SK_13_reuse_refuses_new_body_unknown_ids_and_a_full_chain_by_name",
                newBody == CadStatus::InvalidFeatureOperation
                        && unknown == CadStatus::SketchNotFound && !e.sketch.active()
                        && cadSketchReuseRefusal(full, kept) == CadStatus::TooManyFeatures);
    }
}

void testDelete(Checks& r, double* deleteMicros) {
    Shop shop(shopState());
    const CadSketchId first = keepSquareOn(shop, Vec3{0.5f, 0.5f, 1.0f}, Vec3{0, 0, 1}, 0.3);
    const CadSketchId second = keepSquareOn(shop, Vec3{-0.5f, -0.5f, 1.0f}, Vec3{0, 0, 1}, 0.3);
    const CadBodyState withBoth = shop.state();
    const uint64_t fingerprintBoth = shop.fingerprint();
    const size_t depth = shop.history.undoDepth();

    const auto t0 = std::chrono::steady_clock::now();
    const CadStatus deleted = deleteCadSketch(shop.scene, shop.history, shop.id, first);
    *deleteMicros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                            .count();
    r.check("R2_SK_14_delete_removes_only_an_unused_sketch_as_one_step_and_keeps_the_mark",
            first == 4u && second == 5u && deleted == CadStatus::Ok
                    && shop.history.undoDepth() == depth + 1u
                    && findCadSketchRecord(shop.state(), first) == nullptr
                    && findCadSketchRecord(shop.state(), second) != nullptr
                    && shop.state().nextSketchId == 6u
                    && near(bodyVolume(shop.scene, shop.id), kShopVolume, 1e-9));

    shop.history.undo();
    const bool undone = sameCadBodyState(shop.state(), withBoth)
                        && shop.fingerprint() == fingerprintBoth;
    shop.history.redo();
    const bool redone = findCadSketchRecord(shop.state(), first) == nullptr;
    r.check("R2_SK_15_undo_restores_the_exact_id_and_redo_removes_it_again", undone && redone);

    const CadSketchId third = keepSquareOn(shop, Vec3{0.6f, 0.6f, 1.0f}, Vec3{0, 0, 1}, 0.2);
    r.check("R2_SK_16_a_sketch_kept_after_a_delete_takes_a_fresh_id_never_the_freed_one",
            third == 6u && findCadSketchRecord(shop.state(), first) == nullptr);

    const size_t before = shop.history.undoDepth();
    const uint64_t fp = shop.fingerprint();
    const CadStatus root = deleteCadSketch(shop.scene, shop.history, shop.id, kBaseCadSketchId);
    const CadStatus used = deleteCadSketch(shop.scene, shop.history, shop.id, 2u);
    const CadStatus unknown = deleteCadSketch(shop.scene, shop.history, shop.id, 99u);
    const CadStatus notCad = deleteCadSketch(shop.scene, shop.history, 12345u, second);
    shop.history.beginEdit();
    const CadStatus busy = deleteCadSketch(shop.scene, shop.history, shop.id, second);
    shop.history.commitEdit();
    r.check("R2_SK_17_delete_refuses_root_used_unknown_non_cad_and_busy_recording_nothing",
            root == CadStatus::SketchIsRoot && used == CadStatus::SketchInUse
                    && unknown == CadStatus::SketchNotFound && notCad == CadStatus::NotCadBody
                    && busy == CadStatus::RefusedEditInProgress
                    && shop.history.undoDepth() == before && shop.fingerprint() == fp);

    // Corrupted ids: an inventory over a state whose feature names a missing
    // sketch still lists every record it holds; validation names the fault.
    CadBodyState broken = shopState();
    broken.laterFeatures.back().sketchId = 42u;
    const CadSketchInventory inv = buildCadSketchInventoryFor(broken);
    CadBodyState duplicate = shopState();
    duplicate.sketches[2].sketchId = duplicate.sketches[1].sketchId;
    r.check("R2_SK_18_corrupted_ids_are_listed_and_refused_never_crash",
            inv.rows.size() == 3u && validateCadBodyState(broken) == CadStatus::SketchNotFound
                    && validateCadBodyState(duplicate) != CadStatus::Ok
                    && rowFor(inv, 3u) != nullptr && rowFor(inv, 3u)->consumers.empty());
}

// ===========================================================================
// R2-HIST: staged edits of the exact feature, first failures, Cancel
// ===========================================================================

void testHistory(Checks& r) {
    Shop shop(shopState());
    CadFrame64 pocketFrame;
    cadSketchPlacement(shop.state(), 3u, &pocketFrame);

    // R2-HIST-01: the Cut by its own id, staged with its own values.
    Driver d;
    const CadStatus begun = d.sketch.beginEditFeature(shop.id, shop.state(), 3u,
                                                      worldFrameOf(pocketFrame), true);
    r.check("R2_HIST_01_an_edit_opens_the_exact_feature_by_id_with_its_own_values",
            begun == CadStatus::Ok && d.sketch.editingFeatureId() == 3u
                    && d.sketch.operation() == CadFeatureOperation::Cut
                    && d.sketch.extrude().depth == 0.4
                    && d.sketch.extrude().direction == ExtrudeDirection::AgainstNormal
                    && d.sketch.state() == SketchSessionState::Ready);

    // R2-HIST-02: a typed depth on the LATER feature, one Undo.
    const size_t depth = shop.history.undoDepth();
    d.sketch.setExtrude(0.3, d.sketch.extrude().direction);
    const CadStatus committed = d.sketch.commitEdit(shop.scene, shop.history);
    const double afterEdit = bodyVolume(shop.scene, shop.id);
    shop.history.undo();
    const double undone = bodyVolume(shop.scene, shop.id);
    shop.history.redo();
    r.check("R2_HIST_02_a_typed_depth_on_a_later_cut_is_one_step_and_exact",
            committed == CadStatus::Ok && shop.history.undoDepth() == depth + 1u
                    && near(afterEdit, kShopVolume + 0.36 * 0.1, 1e-9)
                    && near(undone, kShopVolume, 1e-9));

    // R2-HIST-03: the boss Add as Two Sides (0.5 up, 0.1 down into the block):
    // the part inside the block adds nothing, so the volume holds, and the
    // extent is the stored one.
    {
        CadFrame64 bossFrame;
        cadSketchPlacement(shop.state(), 2u, &bossFrame);
        Driver e;
        e.sketch.beginEditFeature(shop.id, shop.state(), 2u, worldFrameOf(bossFrame), true);
        const double before = bodyVolume(shop.scene, shop.id);
        const CadStatus extent = e.sketch.setExtrudeExtent(ExtrudeExtentMode::TwoSides);
        e.sketch.setExtrudeSide(true, 0.5);
        e.sketch.setExtrudeSide(false, 0.1);
        const CadStatus ok = e.sketch.commitEdit(shop.scene, shop.history);
        const CadFeature& boss = shop.state().laterFeatures[0];
        r.check("R2_HIST_03_two_sides_on_a_later_add_commits_its_exact_distances",
                extent == CadStatus::Ok && ok == CadStatus::Ok
                        && boss.extrude.extent == ExtrudeExtentMode::TwoSides
                        && extrudePositiveDistance(boss.extrude) == 0.5
                        && extrudeNegativeDistance(boss.extrude) == 0.1
                        && near(bodyVolume(shop.scene, shop.id), before, 1e-9));
    }

    // R2-HIST-04/05: an upstream base edit that strands the pocket outside the
    // block is refused naming the Cut; Cancel restores byte- and fingerprint-
    // exactly, with no history step.
    {
        const CadBodyState stateBefore = shop.state();
        const uint64_t fingerprint = shop.fingerprint();
        const size_t depthBefore = shop.history.undoDepth();
        CadFrame64 baseFrame;
        cadSketchPlacement(shop.state(), 1u, &baseFrame);
        Driver e;
        e.sketch.beginEditFeature(shop.id, shop.state(), kCadFeatureId, worldFrameOf(baseFrame),
                                  false);
        const SketchEntityId entity = e.sketch.sketch().entities.front().id();
        // 0.4 wide: u in [-0.2, 0.2]; the pocket (u in [0.2, 0.8]) now misses.
        e.sketch.replaceEntity(entity, rect(0.0, 0.0, 0.4, 2.0));
        e.sketch.finish();
        const CadCandidateEvaluation& evaluation = e.sketch.evaluateCandidate();
        const CadTimeline staged = buildCadTimeline(
                e.sketch.candidateState(),
                CadRegenerationReport{evaluation.status, evaluation.failedFeatureId}, true,
                e.sketch.editingFeatureId());
        const CadSketchInventory stagedInv = buildCadSketchInventory(
                e.sketch.candidateState(),
                CadRegenerationReport{evaluation.status, evaluation.failedFeatureId});
        bool failedRow = false;
        for (const CadTimelineRow& t : staged.rows) {
            if (t.kind == CadTimelineRowKind::Feature && t.id == 3u) {
                failedRow = t.state == CadTimelineRowState::Failed;
            }
        }
        const CadStatus refused = e.sketch.commitEdit(shop.scene, shop.history);
        r.check("R2_HIST_04_an_upstream_edit_names_the_first_failing_downstream_feature",
                evaluation.failedFeatureId == 3u && evaluation.status != CadStatus::Ok
                        && failedRow && refused == evaluation.status
                        && rowFor(stagedInv, 3u) != nullptr
                        && rowFor(stagedInv, 3u)->state == CadSketchRowState::Failed);
        e.sketch.cancel();
        r.check("R2_HIST_05_cancel_restores_the_exact_body_and_fingerprint_with_no_step",
                sameCadBodyState(shop.state(), stateBefore) && shop.fingerprint() == fingerprint
                        && shop.history.undoDepth() == depthBefore && !e.sketch.active());
    }

    // R2-HIST-06: inventory states agree with the timeline's sketch rows, for a
    // committed body carrying a consumed, a shared and an unused sketch.
    {
        Shop more(shopState());
        const CadSketchId unused = keepSquareOn(more, Vec3{0.5f, 0.5f, 1.0f}, Vec3{0, 0, 1}, 0.3);
        const CadTimeline timeline = buildCadTimelineFor(more.state());
        const CadSketchInventory inv = buildCadSketchInventoryFor(more.state());
        bool agree = unused != kNoCadSketch;
        for (const CadTimelineRow& t : timeline.rows) {
            if (t.kind != CadTimelineRowKind::Sketch) continue;
            const CadSketchInventoryRow* row = rowFor(inv, t.id);
            const bool unusedBoth = t.state == CadTimelineRowState::Unused
                                    && row != nullptr && row->state == CadSketchRowState::Unused;
            const bool okBoth = t.state == CadTimelineRowState::Ok && row != nullptr
                                && row->state == CadSketchRowState::Ok;
            agree = agree && (unusedBoth || okBoth);
        }
        // Rebuilding the inventory twice gives the same rows (pure, no cache).
        const CadSketchInventory again = buildCadSketchInventoryFor(more.state());
        bool same = again.rows.size() == inv.rows.size();
        for (size_t i = 0; same && i < inv.rows.size(); ++i) {
            same = again.rows[i].sketchId == inv.rows[i].sketchId
                   && again.rows[i].acts == inv.rows[i].acts
                   && again.rows[i].state == inv.rows[i].state;
        }
        r.check("R2_HIST_06_inventory_and_timeline_states_agree_and_reads_are_idempotent",
                agree && same);
    }
}

// ===========================================================================
// R2-PF: planar-face selections
// ===========================================================================

double cellsVolume(const CadSketch& sketch, const std::vector<PlanarFaceRef>& refs, double depth,
                   uint32_t* outComponents, CadStatus* outWhy) {
    CadBodyState state = makeCadBodyState(sketch, faceExtrude(refs, depth));
    CadBodyMesh mesh;
    const CadStatus why = regenerateCadBody(state, &mesh);
    if (outWhy) *outWhy = why;
    if (outComponents) *outComponents = mesh.components;
    return why == CadStatus::Ok ? mesh.volume : -1.0;
}

void testPlanarFaces(Checks& r, double* gridMicros) {
    // A 6 x 6 grid: 36 unit cells (one rectangle, ten lines, T-junctions).
    const int n = 6;
    const CadSketch grid = gridSketch(n);
    const auto t0 = std::chrono::steady_clock::now();
    const SketchArrangement arrangement = deriveSketchArrangement(grid);
    *gridMicros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                          .count();
    r.check("R2_PF_01_a_gridded_rectangle_derives_one_cell_per_unit_square",
            arrangement.status == ArrangementStatus::Ok && arrangement.faces.size() == 36u);

    // 24 cells (four full rows): one component, exact polygonal volume.
    std::vector<int> rows;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < n; ++column) rows.push_back(gridCell(arrangement, n, column, row));
    uint32_t components = 0;
    CadStatus why = CadStatus::Ok;
    const double rowsVolume = cellsVolume(grid, refsOf(arrangement, rows), 0.5, &components, &why);
    r.check("R2_PF_02_twenty_four_edge_connected_cells_extrude_as_one_exact_component",
            why == CadStatus::Ok && components == 1u && near(rowsVolume, 24.0 * 0.5, 1e-9));

    // All 36: one component; and two separate rows: two components.
    std::vector<int> all;
    for (int i = 0; i < 36; ++i) all.push_back(i);
    const double allVolume = cellsVolume(grid, refsOf(arrangement, all), 0.5, &components, &why);
    const bool allOk = why == CadStatus::Ok && components == 1u && near(allVolume, 18.0, 1e-9);
    std::vector<int> twoRows;
    for (int column = 0; column < n; ++column) {
        twoRows.push_back(gridCell(arrangement, n, column, 0));
        twoRows.push_back(gridCell(arrangement, n, column, 2));
    }
    const double twoVolume = cellsVolume(grid, refsOf(arrangement, twoRows), 0.5, &components, &why);
    r.check("R2_PF_03_all_cells_and_two_disjoint_rows_give_one_and_two_components",
            allOk && why == CadStatus::Ok && components == 2u && near(twoVolume, 6.0, 1e-9));

    // Point touch: two diagonal cells meet only at a node -> two components.
    const std::vector<int> diagonal{gridCell(arrangement, n, 1, 1), gridCell(arrangement, n, 2, 2)};
    const double diagonalVolume =
            cellsVolume(grid, refsOf(arrangement, diagonal), 1.0, &components, &why);
    r.check("R2_PF_04_cells_meeting_only_at_a_point_are_separate_components_never_merged",
            why == CadStatus::Ok && components == 2u && near(diagonalVolume, 2.0, 1e-9));

    // Pinched inner holes: everything but two diagonal inner cells that touch
    // at a node -> one component with two holes, exact area 34.
    std::vector<int> pinched;
    const int holeA = gridCell(arrangement, n, 2, 2);
    const int holeB = gridCell(arrangement, n, 3, 3);
    for (int i = 0; i < 36; ++i)
        if (i != holeA && i != holeB) pinched.push_back(i);
    std::vector<size_t> pinchedIdx(pinched.begin(), pinched.end());
    std::vector<PlanarProfileComponent> merged;
    const CadStatus mergeWhy = mergePlanarFaceSelection(arrangement, pinchedIdx, &merged);
    const double pinchedVolume =
            cellsVolume(grid, refsOf(arrangement, pinched), 1.0, &components, &why);
    r.check("R2_PF_05_a_selection_whose_two_unchosen_cells_touch_at_a_node_is_one_solid_with_holes",
            mergeWhy == CadStatus::Ok && merged.size() == 1u && merged[0].holes.size() == 2u
                    && why == CadStatus::Ok && components == 1u && near(pinchedVolume, 34.0, 1e-9));

    // Permutation: the union of a selection does not depend on the order the
    // cells were named in.
    std::vector<size_t> forward;
    for (int c : rows) forward.push_back(static_cast<size_t>(c));
    std::vector<size_t> backward(forward.rbegin(), forward.rend());
    std::vector<size_t> shuffled = forward;
    for (size_t i = 0; i + 3 < shuffled.size(); i += 4) std::swap(shuffled[i], shuffled[i + 3]);
    std::vector<PlanarProfileComponent> a, b, c;
    const bool sameUnion = mergePlanarFaceSelection(arrangement, forward, &a) == CadStatus::Ok
                           && mergePlanarFaceSelection(arrangement, backward, &b) == CadStatus::Ok
                           && mergePlanarFaceSelection(arrangement, shuffled, &c) == CadStatus::Ok
                           && a.size() == b.size() && a.size() == c.size() && a.size() == 1u
                           && a[0].outer.polygon.size() == b[0].outer.polygon.size()
                           && a[0].area == b[0].area && a[0].area == c[0].area;
    r.check("R2_PF_06_the_union_of_a_selection_is_independent_of_its_order", sameUnion);

    // Crossing circles: three cells (two crescents and a lens); together one
    // solid whose volume is the sum of the three cells (chordal tolerance).
    {
        CadSketch circles;
        add(&circles, circleAt(-0.4, 0.0, 1.0));
        add(&circles, circleAt(0.4, 0.0, 1.0));
        const SketchArrangement crossing = deriveSketchArrangement(circles);
        double cellArea = 0.0;
        std::vector<int> every;
        for (size_t i = 0; i < crossing.faces.size(); ++i) {
            cellArea += crossing.faces[i].area;
            every.push_back(static_cast<int>(i));
        }
        const double v = crossing.faces.size() == 3u
                                 ? cellsVolume(circles, refsOf(crossing, every), 1.0, &components,
                                               &why)
                                 : -1.0;
        // A chord polygon of N >= 32 segments loses at most 1 - sin(2pi/N)/(2pi/N)
        // of a disk: 0.65 %; 1 % bounds it.
        r.check("R2_PF_07_crossing_circles_split_into_three_cells_that_union_to_one_solid",
                crossing.status == ArrangementStatus::Ok && crossing.faces.size() == 3u
                        && why == CadStatus::Ok && components == 1u && v > 0.0
                        && near(v, cellArea, 0.01 * cellArea) && v <= cellArea);
    }

    // Two circles tangent from outside: no node at the touch, two cells, two
    // components when both are chosen.
    {
        CadSketch tangent;
        add(&tangent, circleAt(-0.5, 0.0, 0.5));
        add(&tangent, circleAt(0.5, 0.0, 0.5));
        const SketchArrangement touching = deriveSketchArrangement(tangent);
        std::vector<int> both{0, 1};
        const double v = touching.faces.size() == 2u
                                 ? cellsVolume(tangent, refsOf(touching, both), 1.0, &components,
                                               &why)
                                 : -1.0;
        r.check("R2_PF_08_tangent_circles_stay_two_cells_and_two_components",
                touching.status == ArrangementStatus::Ok && touching.faces.size() == 2u
                        && why == CadStatus::Ok && components == 2u && v > 0.0);
    }

    // Nested loops (loop regions): a ring around a disk; the ring alone.
    {
        CadSketch nested;
        const SketchEntityId outer = add(&nested, rect(0.0, 0.0, 2.0, 2.0));
        const SketchEntityId inner = add(&nested, circleAt(0.0, 0.0, 0.5));
        ExtrudeFeature ring = oneSide(outer, 1.0);
        ring.profileHoleIds = {inner};
        const double v = volumeOf(makeCadBodyState(nested, ring));
        const double disk = 0.5 * polygonSignedAreaTwice(circleProfilePolygon(circleAt(0, 0, 0.5)));
        r.check("R2_PF_09_a_ring_around_a_nested_disk_extrudes_the_ring_alone",
                near(v, 4.0 - disk, 1e-9));
    }

    // Reverse winding: a clockwise and a counter-clockwise closed polyline are
    // the same area and the same solid.
    {
        CadSketch ccw;
        SketchPolyline p;
        p.vertices = {{0, 0}, {2, 0}, {2, 1}, {0, 1}};
        p.closed = true;
        const SketchEntityId a = add(&ccw, p);
        CadSketch cw;
        std::reverse(p.vertices.begin(), p.vertices.end());
        const SketchEntityId b = add(&cw, p);
        r.check("R2_PF_10_reversed_winding_extrudes_the_same_outward_solid",
                near(volumeOf(makeCadBodyState(ccw, oneSide(a, 1.0))), 2.0, 1e-12)
                        && near(volumeOf(makeCadBodyState(cw, oneSide(b, 1.0))), 2.0, 1e-12));
    }

    // Determinism: the same sketch derives the same refs, and regenerating
    // twice gives the same status, triangle count and volume.
    {
        const SketchArrangement again = deriveSketchArrangement(grid);
        bool sameRefs = again.faces.size() == arrangement.faces.size();
        for (size_t i = 0; sameRefs && i < again.faces.size(); ++i) {
            sameRefs = samePlanarFaceRef(again.faces[i].ref, arrangement.faces[i].ref);
        }
        const CadBodyState state = makeCadBodyState(grid, faceExtrude(refsOf(arrangement, rows), 0.5));
        CadBodyMesh m1, m2;
        const bool same = regenerateCadBody(state, &m1) == CadStatus::Ok
                          && regenerateCadBody(state, &m2) == CadStatus::Ok
                          && m1.mesh.indices == m2.mesh.indices && m1.volume == m2.volume;
        r.check("R2_PF_11_repeated_derivation_and_regeneration_are_bit_identical",
                sameRefs && same);
    }

    // A spline across a rectangle: the arrangement cuts the rectangle into
    // cells on the curve itself.
    {
        CadSketch splined;
        add(&splined, rect(0.0, 0.0, 2.0, 2.0));
        SketchSpline spline;
        spline.points = {{-1.5, -0.4}, {-0.5, 0.3}, {0.5, -0.3}, {1.5, 0.4}};
        add(&splined, spline);
        const SketchArrangement cut = deriveSketchArrangement(splined);
        double total = 0.0;
        for (const AtomicPlanarFace& f : cut.faces) total += f.area;
        r.check("R2_PF_12_a_spline_crossing_a_rectangle_splits_it_without_losing_area",
                cut.status == ArrangementStatus::Ok && cut.faces.size() == 2u
                        && near(total, 4.0, 1e-6));
    }
}

// ===========================================================================
// R2-SUP: support faces
// ===========================================================================

void testSupport(Checks& r) {
    Shop shop(shopState());
    // The Cut's pocket floor (z = 0.6) and its -x wall (x = 0.2) take a kept sketch.
    CadStatus floorWhy = CadStatus::Ok;
    const CadSketchId onFloor =
            keepSquareOn(shop, Vec3{0.5f, -0.5f, 0.6f}, Vec3{0, 0, 1}, 0.2, &floorWhy);
    const CadSketchRecord* floorRecord = findCadSketchRecord(shop.state(), onFloor);
    r.check("R2_SUP_01_a_cut_pocket_floor_carries_a_kept_sketch",
            floorWhy == CadStatus::Ok && floorRecord != nullptr
                    && floorRecord->featureSupport.featureId == 3u
                    && floorRecord->featureSupport.face.kind == CadFaceKind::CapFar);

    ChosenSupport wall;
    const bool wallPicked = pickAt(shop.scene, lookAlong(Vec3{0.2f, -0.5f, 0.8f}, Vec3{1, 0, 1},
                                                         Vec3{0, 1, 0}),
                                   Vec3{0.2f, -0.5f, 0.8f}, &wall);
    r.check("R2_SUP_02_a_cut_straight_wall_is_an_eligible_support",
            wallPicked && wall.faceRef.producerLocalFeatureId == 3u
                    && wall.faceRef.face.kind == CadFaceKind::Side);

    // A circular pocket's wall is curved: never eligible.
    {
        CadBodyState round = shopState();
        CadSketch hole;
        const SketchEntityId c = add(&hole, circleAt(-0.5, -0.5, 0.2));
        appendCadLaterFeatureWithSketch(&round, CadFeatureOperation::Cut,
                                        faceSupport(round, kCadFeatureId, cap(CadFaceKind::CapFar)),
                                        std::move(hole),
                                        oneSide(c, 0.3, ExtrudeDirection::AgainstNormal));
        std::vector<CadFace> faces;
        bool curvedIneligible = enumerateCadFeatureFaces(round, 4u, &faces) == CadStatus::Ok;
        bool sawSide = false;
        for (const CadFace& f : faces) {
            if (f.token.kind == CadFaceKind::Side) {
                sawSide = true;
                curvedIneligible = curvedIneligible && !f.eligible;
            }
        }
        r.check("R2_SUP_03_a_curved_cut_wall_is_never_an_eligible_support",
                curvedIneligible && sawSide);
    }

    // A partial Revolve's caps have no exact frame in R1: ineligible, and a
    // retained sketch cannot be kept on a revolved body at all.
    {
        CadSketch s;
        const SketchEntityId square = add(&s, rect(1.5, 0.5, 1.0, 1.0));
        add(&s, lineOf(0.0, -1.0, 0.0, 2.0));
        RevolveFeature revolve;
        revolve.profileEntityId = square;
        revolve.axis = CadSketchEdgeRef{2, 0};
        revolve.angleDegrees = 90.0;
        const CadBodyState quarter = makeCadRevolveBodyState(s, revolve);
        std::vector<CadFace> faces;
        const CadStatus enumerated = enumerateCadFeatureFaces(quarter, kCadFeatureId, &faces);
        bool none = true;
        for (const CadFace& f : faces) none = none && !f.eligible;
        CadBodyState withSketch = quarter;
        CadSketch extra;
        add(&extra, rect(0.0, 0.0, 0.2, 0.2));
        CadFeatureSupport onCap;
        onCap.featureId = kCadFeatureId;
        onCap.face = cap(CadFaceKind::CapPlane);
        onCap.lineageToken = cadFeatureTopologySignature(quarter, kCadFeatureId);
        addCadSketchRecord(&withSketch, extra, &onCap);
        r.check("R2_SUP_04_partial_revolve_caps_stay_ineligible_and_refuse_a_sketch",
                (enumerated != CadStatus::Ok || none)
                        && validateCadBodyState(withSketch) != CadStatus::Ok);
    }

    // A size-only edit upstream keeps a kept sketch attached (same lineage);
    // a structural one (the boss rectangle becomes a circle) is refused while a
    // sketch stands on the boss -- never retargeted.
    {
        Shop s(shopState());
        const CadSketchId onBoss =
                keepSquareOn(s, Vec3{-0.5f, 0.5f, 1.5f}, Vec3{0, 0, 1}, 0.2);
        CadBodyState grown = s.state();
        CadSketchRecord* boss = cadFeatureSketchRecord(grown, 2u);
        const SketchEntityId bossRect = boss->sketch.entities.front().id();
        replaceSketchEntity(&boss->sketch, bossRect, rect(-0.5, 0.5, 0.7, 0.7));
        CadBody& body = *s.scene.findBody(s.id)->cadOrNull();
        const CadStatus sized = body.applyState(grown);
        const CadSketchInventory inv = buildCadSketchInventoryFor(s.state());
        CadBodyState rounded = s.state();
        CadSketchRecord* boss2 = cadFeatureSketchRecord(rounded, 2u);
        replaceSketchEntity(&boss2->sketch, bossRect, circleAt(-0.5, 0.5, 0.3));
        const CadBodyState kept = s.state();
        const CadStatus structural = body.applyState(rounded);
        // The same edit through the staged session: the PREVIEW already names
        // it, and the commit refuses by that name.
        CadFrame64 bossFrame;
        cadSketchPlacement(s.state(), 2u, &bossFrame);
        Driver e;
        e.sketch.beginEditFeature(s.id, s.state(), 2u, worldFrameOf(bossFrame), false);
        e.sketch.replaceEntity(bossRect, circleAt(-0.5, 0.5, 0.3));
        e.sketch.finish();
        const CadStatus preview = e.sketch.evaluateCandidate().status;
        const CadStatus commit = e.sketch.commitEdit(s.scene, s.history);
        e.sketch.cancel();
        r.check("R2_SUP_05_a_size_edit_keeps_a_kept_sketch_attached_a_structural_edit_is_refused",
                onBoss != kNoCadSketch && sized == CadStatus::Ok && rowFor(inv, onBoss) != nullptr
                        && rowFor(inv, onBoss)->state == CadSketchRowState::Unused
                        && structural == CadStatus::FeatureSupportInvalid
                        && preview == CadStatus::FeatureSupportInvalid
                        && commit == CadStatus::FeatureSupportInvalid
                        && sameCadBodyState(s.state(), kept));
    }

    // A stale chooser: a face picked, then the feature that made it undone --
    // the confirm-time refresh refuses and never retargets.
    {
        Shop s(shopState());
        const CadSketchId first = keepSquareOn(s, Vec3{0.5f, 0.5f, 1.0f}, Vec3{0, 0, 1}, 0.2);
        // Reuse it as an Add so a new face (its top, z = 1.1) exists.
        CadFrame64 placement;
        cadSketchPlacement(s.state(), first, &placement);
        Driver d;
        d.sketch.beginReuseSketch(s.id, s.state(), first, CadFeatureOperation::Add,
                                  worldFrameOf(placement));
        d.sketch.setExtrude(0.1, d.sketch.extrude().direction);
        ObjectId out = kNoObject;
        const CadStatus added = d.sketch.commit(s.scene, s.history, &out);
        ChosenSupport onTop;
        const bool picked = pickAt(s.scene, lookAlong(Vec3{0.5f, 0.5f, 1.1f}, Vec3{0, 0, 1},
                                                      Vec3{1, 0, 0}),
                                   Vec3{0.5f, 0.5f, 1.1f}, &onTop);
        s.history.undo();  // the Add (feature 4) is gone
        ChosenSupport refreshed;
        const CadStatus stale = refreshChosenSupport(s.scene, onTop, &refreshed);
        r.check("R2_SUP_06_a_support_picked_before_an_undo_is_refused_on_confirm_not_retargeted",
                added == CadStatus::Ok && picked && onTop.faceRef.producerLocalFeatureId == 4u
                        && stale != CadStatus::Ok);
    }
}

// ===========================================================================
// R2-DATA: persistence
// ===========================================================================

std::vector<uint8_t> encodeScene(const ConstructionScene& scene) {
    return encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction));
}

void testData(Checks& r) {
    Shop shop(shopState());
    const std::vector<uint8_t> savedLegacy = encodeScene(shop.scene);
    const uint64_t savedFingerprint = shop.fingerprint();
    const CadSketchId kept = keepSquareOn(shop, Vec3{0.5f, 0.5f, 1.0f}, Vec3{0, 0, 1}, 0.4);
    CadFrame64 placement;
    cadSketchPlacement(shop.state(), kept, &placement);
    Driver d;
    d.sketch.beginReuseSketch(shop.id, shop.state(), kept, CadFeatureOperation::Add,
                              worldFrameOf(placement));
    ObjectId out = kNoObject;
    d.sketch.commit(shop.scene, shop.history, &out);
    const CadSketchId unused = keepSquareOn(shop, Vec3{-0.6f, -0.6f, 1.0f}, Vec3{0, 0, 1}, 0.2);

    // Save -> decode -> reload -> regenerate -> re-encode.
    const std::vector<uint8_t> bytes = encodeScene(shop.scene);
    ProjectDocument back;
    const ProjectCodecStatus decoded = decodeProject(bytes.data(), bytes.size(), &back);
    ConstructionScene reloaded{NoProjectTag{}};
    ConstructionHistory reloadedHistory(reloaded);
    SculptSession sculpt;
    const ProjectCodecStatus loaded =
            decoded == ProjectCodecStatus::Ok
                    ? loadProjectDocument(back, reloaded, sculpt, reloadedHistory)
                    : decoded;
    const CadBodyState* again = bodyState(reloaded, shop.id);
    const CadSketchInventory before = buildCadSketchInventoryFor(shop.state());
    const CadSketchInventory after =
            again != nullptr ? buildCadSketchInventoryFor(*again) : CadSketchInventory{};
    bool sameInventory = before.rows.size() == after.rows.size() && before.rows.size() == 5u;
    for (size_t i = 0; sameInventory && i < before.rows.size(); ++i) {
        sameInventory = before.rows[i].sketchId == after.rows[i].sketchId
                        && before.rows[i].consumers == after.rows[i].consumers
                        && before.rows[i].state == after.rows[i].state;
    }
    r.check("R2_DATA_01_kept_shared_and_unused_sketches_survive_save_reopen_and_reencode",
            unused != kNoCadSketch && decoded == ProjectCodecStatus::Ok
                    && loaded == ProjectCodecStatus::Ok && again != nullptr
                    && sameCadBodyState(*again, shop.state()) && sameInventory
                    && near(volumeOf(*again), bodyVolume(shop.scene, shop.id), 0.0)
                    && encodeScene(reloaded) == bytes);

    // Undo everything: the project is byte- and fingerprint-equal to the
    // legacy save (CADB v5 again); Redo all the way is the v6 project again.
    const uint64_t forward = shop.fingerprint();
    while (shop.history.undoDepth() > 0u) shop.history.undo();
    const bool legacyAgain = encodeScene(shop.scene) == savedLegacy
                             && shop.fingerprint() == savedFingerprint;
    while (shop.history.redoDepth() > 0u) shop.history.redo();
    r.check("R2_DATA_02_undo_to_the_save_is_byte_equal_and_redo_restores_the_fingerprint",
            legacyAgain && shop.fingerprint() == forward && encodeScene(shop.scene) == bytes);

    // Corrupt input fails closed and the live project does not move.
    std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + bytes.size() / 2);
    ProjectDocument junk;
    const ProjectCodecStatus truncatedWhy =
            decodeProject(truncated.data(), truncated.size(), &junk);
    ProjectDocument dangling = back;
    dangling.cad.bodies[0].state.laterFeatures.back().sketchId = 77u;
    const uint64_t liveBefore = shop.fingerprint();
    const ProjectCodecStatus danglingWhy =
            loadProjectDocument(dangling, shop.scene, sculpt, shop.history);
    r.check("R2_DATA_03_truncated_bytes_and_a_dangling_sketch_id_fail_closed_leaving_the_project",
            truncatedWhy != ProjectCodecStatus::Ok && danglingWhy != ProjectCodecStatus::Ok
                    && shop.fingerprint() == liveBefore);

    // Delete of the unused sketch round-trips, and the freed id is not re-used
    // by the reopened project either (the mark is persisted in v6).
    const CadStatus deleted = deleteCadSketch(shop.scene, shop.history, shop.id, unused);
    const std::vector<uint8_t> deletedBytes = encodeScene(shop.scene);
    ProjectDocument deletedBack;
    const bool deletedRound =
            decodeProject(deletedBytes.data(), deletedBytes.size(), &deletedBack)
                    == ProjectCodecStatus::Ok
            && sameCadBodyState(deletedBack.cad.bodies[0].state, shop.state())
            && deletedBack.cad.bodies[0].state.nextSketchId == unused + 1u;
    r.check("R2_DATA_04_a_deleted_sketch_stays_deleted_and_its_id_stays_retired_across_save",
            deleted == CadStatus::Ok && deletedRound);
}

}  // namespace

void runCadR2SelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    Checks r{out};
    double keepUs = 0.0;
    double reuseUs = 0.0;
    double deleteUs = 0.0;
    double gridUs = 0.0;
    testInventory(r);
    testKeepAndEdit(r, &keepUs);
    testReuseAndDelete(r, &reuseUs);
    testDelete(r, &deleteUs);
    testHistory(r);
    testPlanarFaces(r, &gridUs);
    testSupport(r);
    testData(r);
    // Inventory read cost on a 16-sketch body (the cap), median of 9.
    CadBodyState big = shopState();
    {
        Shop s(big);
        while (s.state().sketches.size() < kMaxCadSketches) {
            const double x = -0.8 + 0.1 * static_cast<double>(s.state().sketches.size());
            if (keepSquareOn(s, Vec3{static_cast<float>(x), 0.85f, 1.0f}, Vec3{0, 0, 1}, 0.05)
                == kNoCadSketch) {
                break;
            }
        }
        big = s.state();
    }
    std::vector<double> samples;
    for (int i = 0; i < 9; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        const CadSketchInventory inv = buildCadSketchInventoryFor(big);
        samples.push_back(std::chrono::duration<double, std::micro>(
                                  std::chrono::steady_clock::now() - t0)
                                  .count());
        (void)inv;
    }
    std::sort(samples.begin(), samples.end());
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  "cad_r2_keep_us=%.0f cad_r2_reuse_us=%.0f cad_r2_delete_us=%.0f "
                  "cad_r2_grid36_arrangement_us=%.0f cad_r2_inventory%zu_us=%.0f/%.0f",
                  keepUs, reuseUs, deleteUs, gridUs, big.sketches.size(), samples[4],
                  samples.back());
    *performance = buffer;
}

}  // namespace forgeshape
