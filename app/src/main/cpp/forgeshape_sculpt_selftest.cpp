#include "forgeshape_sculpt_selftest.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_construction.h"
#include "forgeshape_input.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_history.h"
#include "forgeshape_picking.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_selection.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

struct Recorder {
    SculptSelfTestResult* out;
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

// Mesh positions are float and the brush multiplies a pixel delta by a
// world-per-pixel scale, so an expected displacement carries a few float
// roundings. 1e-5 absolute is far above that floor at these sizes and far
// tighter than any real basis, sign or transform error, which would be off by a
// whole component.
constexpr float kEpsilon = 1e-5f;

bool nearly(float a, float b) { return std::fabs(a - b) <= kEpsilon; }

bool nearlyVec(const Vec3& a, const Vec3& b) {
    return nearly(a.x, b.x) && nearly(a.y, b.y) && nearly(a.z, b.z);
}

// Bit-exact, deliberately: "the copy equals the source" and "Construction did
// not change" are both statements about identical data, not about tolerance.
bool sameVertices(const std::vector<MeshVertex>& a, const std::vector<MeshVertex>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            if (a[i].position[axis] != b[i].position[axis]) {
                return false;
            }
            if (a[i].color[axis] != b[i].color[axis]) {
                return false;
            }
        }
    }
    return true;
}

bool sameIndices(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

bool allPositionsFinite(const SculptMesh& mesh) {
    for (const MeshVertex& v : mesh.vertices()) {
        if (!std::isfinite(v.position[0]) || !std::isfinite(v.position[1]) ||
            !std::isfinite(v.position[2])) {
            return false;
        }
    }
    return true;
}

// A camera at the product's own default framing, on a realistic portrait
// viewport. Nothing here restates FOV or aspect: the snapshot carries them.
constexpr int kViewportWidth = 1080;
constexpr int kViewportHeight = 2400;

CameraSnapshot defaultCamera() {
    CameraController camera;
    camera.setViewport(kViewportWidth, kViewportHeight);
    return camera.snapshot();
}

// The screen centre. The camera orbits the world origin and the test object is
// centred there, so this pixel always lands on the object.
constexpr float kCentreX = kViewportWidth * 0.5f;
constexpr float kCentreY = kViewportHeight * 0.5f;

// A sphere is the useful sculpt fixture: 482 vertices spread evenly over the
// surface, so a brush of any sensible radius captures a meaningful set with a
// real spread of falloff weights. A box's 8 corners would prove nothing about
// falloff.
ConstructionObject makeSphereObject() {
    ConstructionObject object(kConstructionBoxObjectId);
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    object.setPrimitive(PrimitiveSpec::forSphere(2.0));
    return object;
}

// The Plane fixture for PLN-17..19: the one primitive whose Frozen Sculpt Mesh
// is OPEN (4 vertices, 2 triangles, no interior), which is what those checks
// exist to prove the generic Freeze pipeline handles without any special case.
ConstructionObject makePlaneObject() {
    ConstructionObject object(kConstructionBoxObjectId);
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    object.setPrimitive(PrimitiveSpec::forPlane(2.0, 1.5));
    return object;
}

// The world-space displacement the brush should produce for a pointer travel of
// (dx, dy) pixels, derived from the camera snapshot the same way the brush does:
// screen +x is camera right, screen +y is DOWN, which is camera -up.
Vec3 expectedWorldDelta(const CameraSnapshot& camera, float dx, float dy, float worldPerPixel) {
    const Vec3 right{camera.view.m[0], camera.view.m[4], camera.view.m[8]};
    const Vec3 up{camera.view.m[1], camera.view.m[5], camera.view.m[9]};
    Vec3 delta = vec3Scale(right, dx * worldPerPixel);
    delta = vec3Add(delta, vec3Scale(up, -dy * worldPerPixel));
    return delta;
}

float lengthOf(const Vec3& v) { return std::sqrt(vec3Dot(v, v)); }

// How far `d` strays off the axis `n`, as an ABSOLUTE length in object space.
// Zero means exactly parallel (or exactly anti-parallel). This is what
// separates Clay from Inflate: Clay's total displacement stays on the axis of
// the normal the vertex started with; Inflate's leaves that axis as soon as the
// surface it is following has moved.
//
// Deliberately absolute rather than a fraction of |d|. A vertex near the brush
// rim moves only a few thousandths of a unit, while the float rounding of
// repeatedly adding a small step to a coordinate of magnitude ~1 is the same
// few times 1e-7 whatever the weight — so a RATIO would report the smallest
// displacements as wildly off-axis and be measuring float resolution rather
// than the brush. The absolute error is bounded by that accumulated rounding,
// which is what the thresholds below are set against.
float offAxisDistance(const Vec3& d, const Vec3& n) {
    return lengthOf(vec3Cross(d, n));
}

// The 1-ring neighbour average, computed straight from the adjacency — the same
// quantity Smooth moves toward, derived here independently of the brush.
bool neighborMean(const SculptMesh& mesh, uint32_t vertex, Vec3* out) {
    uint32_t count = 0;
    const uint32_t* neighbors = mesh.topology().neighbors(vertex, &count);
    if (neighbors == nullptr || count == 0 || out == nullptr) {
        return false;
    }
    Vec3 sum{0.0f, 0.0f, 0.0f};
    for (uint32_t i = 0; i < count; ++i) {
        sum = vec3Add(sum, mesh.vertexPosition(neighbors[i]));
    }
    *out = vec3Scale(sum, 1.0f / static_cast<float>(count));
    return true;
}

// Applies `steps` moves that zig-zag by `stepPixels`, so the stroke accumulates
// exactly steps * stepPixels of pointer path while the pointer stays next to
// where it went down. The three path-driven tools care only about the path
// length, so this is the cleanest way to give them a known amount of work.
int driveTravel(SculptSession& session, float centreX, float centreY, int steps,
                float stepPixels) {
    int applied = 0;
    for (int i = 0; i < steps; ++i) {
        const float x = centreX + ((i % 2 == 0) ? stepPixels : 0.0f);
        if (session.updateStroke(x, centreY)) {
            ++applied;
        }
    }
    return applied;
}

int slotOfHighestWeight(const SculptStroke& stroke) {
    int best = 0;
    for (int i = 1; i < stroke.affectedVertexCount(); ++i) {
        if (stroke.affectedVertex(i).weight > stroke.affectedVertex(best).weight) {
            best = i;
        }
    }
    return best;
}

int slotOfLowestWeight(const SculptStroke& stroke) {
    int worst = 0;
    for (int i = 1; i < stroke.affectedVertexCount(); ++i) {
        if (stroke.affectedVertex(i).weight < stroke.affectedVertex(worst).weight) {
            worst = i;
        }
    }
    return worst;
}

// A session holding a frozen 2 m sphere, ready to stroke, with the tool, radius
// and strength the caller asked for.
void prepareSession(SculptSession* session, const ConstructionObject& object, SculptTool tool,
                    float radiusPixels, float strength) {
    session->freezeToSculpt(object.generateMesh(), object.objectId());
    session->setTool(tool);
    session->setRadiusPixels(radiusPixels);
    session->setStrength(strength);
}

}  // namespace

// Builds a snapshot for an arbitrary eye position looking at the world origin.
//
// CameraController only moves by gesture, and a sidedness test needs the two
// poses a gesture is the clumsiest possible way to reach: straight down the
// -Y axis onto a Plane's canonical front, and straight up at the same sheet
// from underneath. A flat sheet is invisible edge-on, so the product's default
// oblique pose cannot express either. `up` is +Z rather than +Y precisely
// because the view direction here IS the Y axis, and mat4LookAt needs an up
// vector that is not parallel to it.
CameraSnapshot cameraAt(const Vec3& eye, ProjectionMode projection) {
    CameraSnapshot s{};
    s.eye = eye;
    s.target = Vec3{0.0f, 0.0f, 0.0f};
    s.yaw = 0.0f;
    s.pitch = 0.0f;
    s.distance = std::sqrt(vec3Dot(eye, eye));
    s.projection = projection;
    s.orthoHalfHeightMeters = s.distance * std::tan(kFovYRadians * 0.5f);
    s.view = mat4LookAt(eye, s.target, Vec3{0.0f, 0.0f, 1.0f});
    const float aspect =
        static_cast<float>(kViewportWidth) / static_cast<float>(kViewportHeight);
    s.proj = (projection == ProjectionMode::Orthographic)
                 ? mat4Orthographic(s.orthoHalfHeightMeters, aspect, kNearPlane, kFarPlane)
                 : mat4Perspective(kFovYRadians, aspect, kNearPlane, kFarPlane);
    return s;
}

// Directly above the sheet, seeing its canonical +Y front.
CameraSnapshot cameraLookingAtPlaneFront() {
    return cameraAt(Vec3{0.0f, 6.0f, 0.0f}, ProjectionMode::Perspective);
}

// Directly below the same sheet, seeing its back.
CameraSnapshot cameraLookingAtPlaneBack() {
    return cameraAt(Vec3{0.0f, -6.0f, 0.0f}, ProjectionMode::Perspective);
}

// SIDE-01..09. Kept in one function so the invariant reads as one contract
// rather than nine scattered assertions.
void runSidednessSelfTests(Recorder& r) {
    const Mat4 identity = mat4Identity();

    ConstructionObject planeObject;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    planeObject.setPrimitive(PrimitiveSpec::forPlane(2.0, 1.25));
    const ConstructionMesh planeSource = planeObject.generateMesh();

    ConstructionObject solidObject = makeSphereObject();
    const ConstructionMesh solidSource = solidObject.generateMesh();

    // --- SIDE-01: the Construction publication carries the fact at all -----
    r.check("side01_construction_plane_is_two_sided", planeSource.renderBothSides);
    r.check("side01_construction_solid_is_single_sided", !solidSource.renderBothSides);

    // --- SIDE-02: Freeze preserves it, in both directions ------------------
    SculptMesh frozenPlane;
    r.check("side02_freeze_plane_succeeds",
            frozenPlane.freezeFrom(planeSource, planeObject.objectId()));
    r.check("side02_frozen_plane_is_two_sided", frozenPlane.renderBothSides());

    SculptMesh frozenSolid;
    r.check("side06_freeze_solid_succeeds",
            frozenSolid.freezeFrom(solidSource, solidObject.objectId()));
    r.check("side06_frozen_solid_is_single_sided", !frozenSolid.renderBothSides());

    // --- SIDE-03: the frozen Plane's PUBLICATION is two-sided, and its
    // authoritative topology is still exactly 4:6. The backside is a
    // render-only duplication; it must never reach the source.
    MeshStore planeStore(planeObject.objectId());
    MeshValidation why = MeshValidation::Ok;
    r.check("side03_frozen_plane_publishes",
            publishSculptMesh(frozenPlane, planeStore, &why) != kNoMeshRevision);
    const RuntimeMeshPtr activePlane = planeStore.current();
    r.check("side03_frozen_plane_publication_is_two_sided",
            activePlane != nullptr && activePlane->renderBothSides());
    r.check("side03_frozen_plane_source_topology_is_still_4_6",
            frozenPlane.vertexCount() == 4 && frozenPlane.indexCount() == 6 &&
                activePlane != nullptr && activePlane->vertexCount() == 4 &&
                activePlane->indexCount() == 6);

    RenderMeshData planeRender;
    const bool planeRenderBuilt =
        activePlane != nullptr &&
        buildRenderMesh(activePlane->vertices(), activePlane->vertexCount(),
                        activePlane->indices(), activePlane->indexCount(),
                        SurfaceShading::Smooth, &planeRender, activePlane->renderBothSides());
    // Smooth 4:6 duplicated once = 8:12, and the source counts it reports are
    // still the undoubled ones.
    r.check("side03_frozen_plane_render_representation_is_doubled",
            planeRenderBuilt && planeRender.vertexCount() == 8 &&
                planeRender.indexCount() == 12 && planeRender.sourceVertexCount == 4 &&
                planeRender.sourceIndexCount == 6);

    MeshStore solidStore(solidObject.objectId());
    r.check("side06_frozen_solid_publishes",
            publishSculptMesh(frozenSolid, solidStore, &why) != kNoMeshRevision);
    const RuntimeMeshPtr activeSolid = solidStore.current();
    r.check("side06_frozen_solid_publication_is_single_sided",
            activeSolid != nullptr && !activeSolid->renderBothSides());
    RenderMeshData solidRender;
    const bool solidRenderBuilt =
        activeSolid != nullptr &&
        buildRenderMesh(activeSolid->vertices(), activeSolid->vertexCount(),
                        activeSolid->indices(), activeSolid->indexCount(),
                        SurfaceShading::Smooth, &solidRender, activeSolid->renderBothSides());
    r.check("side06_frozen_solid_render_representation_is_not_doubled",
            solidRenderBuilt && solidRender.indexCount() == activeSolid->indexCount());

    // --- SIDE-04: selection picking hits the frozen Plane from BOTH sides ---
    // Uses the explicit-transform overload with the published mesh's own
    // sidedness, which is exactly what the implicit overload now computes.
    const CameraSnapshot front = cameraLookingAtPlaneFront();
    const CameraSnapshot back = cameraLookingAtPlaneBack();
    {
        const bool frontFacesOnly = !(activePlane != nullptr && activePlane->renderBothSides());
        MeshStore probe(planeObject.objectId());
        publishSculptMesh(frozenPlane, probe, &why);
        // pickScene reads the process-global store, so drive the shared core
        // through the explicit overload against a locally published mesh by
        // temporarily making it the global active representation below; here we
        // assert the triangle-level answer the whole chain rests on.
        Ray frontRay{};
        Ray backRay{};
        const bool frontRayOk =
            buildPickRay(front, kCentreX, kCentreY, kViewportWidth, kViewportHeight, &frontRay);
        const bool backRayOk =
            buildPickRay(back, kCentreX, kCentreY, kViewportWidth, kViewportHeight, &backRay);
        r.check("side04_frozen_plane_pick_hits_from_front",
                frontRayOk &&
                    pickTriangleMesh(frontRay, frozenPlane.triangleView(), frontFacesOnly).hit);
        r.check("side04_frozen_plane_pick_hits_from_back",
                backRayOk &&
                    pickTriangleMesh(backRay, frozenPlane.triangleView(), frontFacesOnly).hit);
        // Teeth: with the front-face rule the back ray must miss, so the check
        // above is proving the sidedness flag and not merely that a ray hits.
        r.check("side04_back_ray_would_miss_under_the_front_face_rule",
                backRayOk && !pickTriangleMesh(backRay, frozenPlane.triangleView(), true).hit);
    }

    // --- SIDE-05: the Sculpt hit-test agrees, from both sides ---------------
    //
    // The brush radius is the product MAXIMUM here, and that is load-bearing
    // rather than lazy: a Plane's only 4 vertices are its corners, 1.18 m from
    // the centre at this size, and a brush that captures no vertex starts no
    // stroke at all (the same property Known Issues records for a frozen box).
    // At this camera distance 600 px reaches 1.73 m, so a failure here is a
    // sidedness failure and not a brush that was simply too small.
    {
        SculptStroke frontStroke;
        SculptStroke backStroke;
        r.check("side05_frozen_plane_sculpt_hits_from_front",
                frontStroke.begin(SculptTool::Grab, frozenPlane, front, kCentreX, kCentreY,
                                  kViewportWidth, kViewportHeight, identity, identity,
                                  kMaxBrushRadiusPixels));
        r.check("side05_frozen_plane_sculpt_hits_from_back",
                backStroke.begin(SculptTool::Grab, frozenPlane, back, kCentreX, kCentreY,
                                 kViewportWidth, kViewportHeight, identity, identity,
                                 kMaxBrushRadiusPixels));

        // Teeth for SIDE-06: a closed solid must still refuse a stroke started
        // from inside, which is the front-face rule the Plane is the exception
        // to. The sphere is 1 m across, so an eye 0.2 m from its centre is
        // within it.
        const CameraSnapshot inside =
            cameraAt(Vec3{0.0f, 0.2f, 0.0f}, ProjectionMode::Perspective);
        SculptStroke insideStroke;
        r.check("side06_solid_sculpt_refuses_a_stroke_from_inside",
                !insideStroke.begin(SculptTool::Grab, frozenSolid, inside, kCentreX, kCentreY,
                                    kViewportWidth, kViewportHeight, identity, identity,
                                    kMaxBrushRadiusPixels));
        // ...and that refusal is culling, not an empty viewport: the same ray
        // DOES meet the sphere's far wall once the front-face rule is dropped.
        // Without this the check above could pass for having hit nothing at all.
        Ray insideRay{};
        const bool insideRayOk = buildPickRay(inside, kCentreX, kCentreY, kViewportWidth,
                                              kViewportHeight, &insideRay);
        r.check("side06_inside_refusal_is_culling_not_an_empty_scene",
                insideRayOk &&
                    pickTriangleMesh(insideRay, frozenSolid.triangleView(), false).hit);
    }

    // --- SIDE-07 / SIDE-08: the stale-source case, through the REAL implicit
    // pickScene overload -- the one that used to ask PrimitiveKind.
    //
    // Freeze a solid, then change the Construction Source to a Plane. The active
    // representation is still the frozen solid, so it must stay single-sided;
    // asking the Source would wrongly make it two-sided. The converse case is
    // asserted straight after, so neither a hard-coded true nor a hard-coded
    // false can pass both.
    {
        // Save and restore the process-global state this exercises, so the test
        // is input-independent and leaves nothing behind for later suites.
        const TransformValues savedTransform = constructionTransform().values();
        const PrimitiveSpec savedPrimitive = constructionObject().spec();
        TransformValues atOrigin{};
        constructionTransform().setValues(atOrigin);

        Ray backRay{};
        buildPickRay(back, kCentreX, kCentreY, kViewportWidth, kViewportHeight, &backRay);

        // Case A: active = frozen SOLID, Construction Source = PLANE.
        constructionObject().setPrimitive(PrimitiveSpec::forPlane(2.0, 1.25));
        publishSculptMesh(frozenSolid, meshStore(), &why);
        const SceneHit staleSolidFromBehind =
            pickScene(front, kCentreX, kCentreY, kViewportWidth, kViewportHeight);
        // Seen from the front the solid is hit normally; the point of the case
        // is that it is hit on its NEAR surface, not its far wall.
        r.check("side07_stale_frozen_solid_is_still_hit_from_outside", staleSolidFromBehind.hit);
        // Now from inside: a single-sided solid must miss. If sidedness came
        // from the Plane Source this would wrongly hit.
        const CameraSnapshot inside =
            cameraAt(Vec3{0.0f, 0.2f, 0.0f}, ProjectionMode::Perspective);
        const SceneHit fromInside =
            pickScene(inside, kCentreX, kCentreY, kViewportWidth, kViewportHeight);
        r.check("side07_stale_frozen_solid_does_not_inherit_plane_two_sidedness",
                !fromInside.hit);

        // Case B: active = frozen PLANE, Construction Source = SOLID. The
        // mirror image, so the rule cannot be satisfied by ignoring sidedness.
        constructionObject().setPrimitive(
            PrimitiveSpec::forSphere(kDefaultSphereDiameterMeters));
        publishSculptMesh(frozenPlane, meshStore(), &why);
        const SceneHit planeFromBack =
            pickScene(back, kCentreX, kCentreY, kViewportWidth, kViewportHeight);
        r.check("side08_frozen_plane_still_picks_from_behind_under_a_solid_source",
                planeFromBack.hit);
        const SceneHit planeFromFront =
            pickScene(front, kCentreX, kCentreY, kViewportWidth, kViewportHeight);
        r.check("side08_frozen_plane_still_picks_from_the_front", planeFromFront.hit);

        constructionObject().setPrimitive(savedPrimitive);
        constructionTransform().setValues(savedTransform);
    }

    // --- SIDE-09: display settings are presentation and cannot move the fact --
    {
        // Both shading models and both projections, against one unchanged
        // published mesh. Sidedness is carried by the mesh, so none of them can
        // touch it -- and the render duplication follows the mesh, not the mode.
        RenderMeshData faceted;
        const bool built =
            activePlane != nullptr &&
            buildRenderMesh(activePlane->vertices(), activePlane->vertexCount(),
                            activePlane->indices(), activePlane->indexCount(),
                            SurfaceShading::Faceted, &faceted, activePlane->renderBothSides());
        r.check("side09_shading_change_does_not_alter_sidedness",
                built && activePlane->renderBothSides() && faceted.sourceVertexCount == 4 &&
                    faceted.sourceIndexCount == 6);
        const CameraSnapshot orthoBack =
            cameraAt(Vec3{0.0f, -6.0f, 0.0f}, ProjectionMode::Orthographic);
        Ray orthoBackRay{};
        const bool orthoOk = buildPickRay(orthoBack, kCentreX, kCentreY, kViewportWidth,
                                          kViewportHeight, &orthoBackRay);
        r.check("side09_projection_change_does_not_alter_sidedness",
                orthoOk && pickTriangleMesh(orthoBackRay, frozenPlane.triangleView(),
                                            !frozenPlane.renderBothSides())
                               .hit);
    }
}

int runSculptSelfTests(SculptSelfTestResult* out, int max) {
    Recorder r{out, max};
    if (out == nullptr || max <= 0) {
        return 0;
    }

    const CameraSnapshot camera = defaultCamera();
    const Mat4 identity = mat4Identity();

    // -----------------------------------------------------------------------
    // Brush parameter clamping and falloff
    // -----------------------------------------------------------------------
    r.check("radius_clamps_below_minimum",
            clampBrushRadiusPixels(1.0f) == kMinBrushRadiusPixels);
    r.check("radius_clamps_above_maximum",
            clampBrushRadiusPixels(9999.0f) == kMaxBrushRadiusPixels);
    r.check("radius_passes_valid_value", clampBrushRadiusPixels(150.0f) == 150.0f);
    r.check("radius_nonfinite_falls_back_to_default",
            clampBrushRadiusPixels(std::nanf("")) == kDefaultBrushRadiusPixels);
    r.check("strength_clamps_below_minimum", clampBrushStrength(0.0f) == kMinBrushStrength);
    r.check("strength_clamps_above_maximum", clampBrushStrength(5.0f) == kMaxBrushStrength);
    r.check("strength_passes_valid_value", clampBrushStrength(0.5f) == 0.5f);
    r.check("strength_nonfinite_falls_back_to_default",
            clampBrushStrength(std::nanf("")) == kDefaultBrushStrength);

    r.check("falloff_is_one_at_centre", nearly(sculptFalloff(0.0f, 1.0f), 1.0f));
    r.check("falloff_is_zero_at_rim", sculptFalloff(1.0f, 1.0f) == 0.0f);
    r.check("falloff_is_zero_outside_rim", sculptFalloff(2.0f, 1.0f) == 0.0f);
    r.check("falloff_decreases_outward", sculptFalloff(0.25f, 1.0f) > sculptFalloff(0.75f, 1.0f));
    r.check("falloff_is_positive_inside", sculptFalloff(0.75f, 1.0f) > 0.0f);
    r.check("falloff_zero_radius_moves_nothing", sculptFalloff(0.0f, 0.0f) == 0.0f);
    r.check("falloff_nonfinite_distance_is_zero",
            sculptFalloff(std::nanf(""), 1.0f) == 0.0f);

    // -----------------------------------------------------------------------
    // Freeze: the copy, the identity, and the preserved Construction Source
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();
        const PrimitiveSpec specBefore = object.spec();
        const TransformValues transformBefore = placement_.values();

        SculptSession session;
        r.check("mode_starts_as_construction", session.mode() == ProductMode::Construction);
        r.check("no_sculpt_mesh_before_freeze", !session.hasSculptMesh());
        r.check("enter_sculpt_refused_before_freeze", !session.enterSculpt());
        r.check("mode_still_construction_after_refused_enter",
                session.mode() == ProductMode::Construction);

        const bool froze = session.freezeToSculpt(source, object.objectId());
        r.check("freeze_succeeds", froze);
        r.check("freeze_enters_sculpt_mode", session.mode() == ProductMode::Sculpt);
        r.check("freeze_creates_sculpt_mesh", session.hasSculptMesh());

        const SculptMesh& mesh = session.mesh();
        r.check("freeze_copies_vertex_count", mesh.vertexCount() == kSphereVertexCount);
        r.check("freeze_copies_index_count", mesh.indexCount() == kSphereIndexCount);
        r.check("freeze_copies_vertices_exactly", sameVertices(mesh.vertices(), source.vertices));
        r.check("freeze_copies_indices_exactly", sameIndices(mesh.indices(), source.indices));
        r.check("freeze_preserves_object_id", mesh.objectId() == object.objectId());
        r.check("freeze_object_id_is_the_construction_id",
                mesh.objectId() == kConstructionBoxObjectId);
        r.check("sculpt_revision_starts_at_one", mesh.revision() == 1);

        // The Construction Source is untouched by the Freeze itself.
        r.check("freeze_leaves_primitive_kind", object.kind() == PrimitiveKind::Sphere);
        r.check("freeze_leaves_primitive_parameters",
                object.spec().sphere() != nullptr && specBefore.sphere() != nullptr &&
                    object.spec().sphere()->diameter == specBefore.sphere()->diameter);
        r.check("freeze_leaves_transform",
                placement_.values().positionX == transformBefore.positionX &&
                    placement_.values().rotationZ == transformBefore.rotationZ);
        r.check("freeze_leaves_construction_mesh_regenerable",
                sameVertices(object.generateMesh().vertices, source.vertices));

        // Storage independence: writing into the sculpt mesh must not be visible
        // anywhere on the Construction side.
        session.mesh().setVertexPosition(0, Vec3{99.0f, 99.0f, 99.0f});
        r.check("sculpt_edit_visible_in_sculpt_mesh",
                nearlyVec(session.mesh().vertexPosition(0), Vec3{99.0f, 99.0f, 99.0f}));
        r.check("sculpt_edit_does_not_reach_construction_mesh",
                sameVertices(object.generateMesh().vertices, source.vertices));
        r.check("sculpt_edit_does_not_reach_primitive_parameters",
                object.spec().sphere() != nullptr &&
                    object.spec().sphere()->diameter == specBefore.sphere()->diameter);
        r.check("sculpt_edit_alone_does_not_advance_revision", session.mesh().revision() == 1);
        r.check("advance_revision_is_monotonic", session.mesh().advanceRevision() == 2);

        // Topology cannot be touched: counts are constant across every edit.
        r.check("sculpt_edit_preserves_vertex_count",
                session.mesh().vertexCount() == kSphereVertexCount);
        r.check("sculpt_edit_preserves_indices",
                sameIndices(session.mesh().indices(), source.indices));

        // Out-of-range and non-finite writes are refused rather than corrupting.
        r.check("vertex_write_out_of_range_refused",
                !session.mesh().setVertexPosition(kSphereVertexCount, Vec3{0.0f, 0.0f, 0.0f}));
        r.check("vertex_write_nonfinite_refused",
                !session.mesh().setVertexPosition(1, Vec3{std::nanf(""), 0.0f, 0.0f}));
        r.check("refused_write_left_mesh_finite", allPositionsFinite(session.mesh()));
    }

    // -----------------------------------------------------------------------
    // Freeze fails closed on unusable source data
    // -----------------------------------------------------------------------
    {
        SculptSession session;
        ConstructionMesh empty;
        MeshValidation why = MeshValidation::Ok;
        r.check("freeze_refuses_empty_source", !session.freezeToSculpt(empty, 1, &why));
        r.check("refused_freeze_reports_reason", why != MeshValidation::Ok);
        r.check("refused_freeze_leaves_construction_mode",
                session.mode() == ProductMode::Construction);
        r.check("refused_freeze_creates_no_mesh", !session.hasSculptMesh());
    }

    // -----------------------------------------------------------------------
    // Mode switching keeps both representations
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();
        SculptSession session;
        session.freezeToSculpt(source, object.objectId());
        session.mesh().setVertexPosition(0, Vec3{7.0f, 8.0f, 9.0f});
        const SculptRevision revisionAfterEdit = session.mesh().advanceRevision();
        const uint64_t freezesAfterFirst = session.freezeCount();

        session.enterConstruction();
        r.check("enter_construction_switches_mode",
                session.mode() == ProductMode::Construction);
        r.check("enter_construction_keeps_sculpt_mesh", session.hasSculptMesh());
        r.check("enter_construction_keeps_sculpt_edits",
                nearlyVec(session.mesh().vertexPosition(0), Vec3{7.0f, 8.0f, 9.0f}));
        r.check("enter_construction_copies_nothing_into_construction",
                sameVertices(object.generateMesh().vertices, source.vertices));

        r.check("return_to_sculpt_succeeds", session.enterSculpt());
        r.check("return_to_sculpt_does_not_refreeze",
                session.freezeCount() == freezesAfterFirst);
        r.check("return_to_sculpt_restores_prior_edits",
                nearlyVec(session.mesh().vertexPosition(0), Vec3{7.0f, 8.0f, 9.0f}));
        r.check("return_to_sculpt_keeps_revision",
                session.mesh().revision() == revisionAfterEdit);

        // Stale-source policy: a Construction change marks, and never replaces.
        r.check("source_not_stale_after_freeze", !session.sourceStale());
        object.setPrimitive(PrimitiveSpec::forSphere(3.0));
        session.markSourceStale();
        r.check("construction_change_marks_source_stale", session.sourceStale());
        r.check("stale_source_does_not_replace_sculpt_mesh",
                nearlyVec(session.mesh().vertexPosition(0), Vec3{7.0f, 8.0f, 9.0f}));
        r.check("stale_source_does_not_refreeze", session.freezeCount() == freezesAfterFirst);
        session.freezeToSculpt(object.generateMesh(), object.objectId());
        r.check("explicit_refreeze_clears_stale", !session.sourceStale());
        r.check("explicit_refreeze_restarts_sculpt_revision", session.mesh().revision() == 1);
    }

    // -----------------------------------------------------------------------
    // PLN-17/18/19 — Freeze / Resume / stale-source on a Plane, the one
    // primitive whose Frozen Sculpt Mesh is OPEN. The generic Freeze pipeline
    // (forgeshape_sculpt.{h,cpp}) has no closedness assumption anywhere — this
    // proves that rather than merely asserting it.
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makePlaneObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();
        r.check("pln17_plane_source_is_open_4_6",
                source.vertices.size() == 4 && source.indices.size() == 6);

        SculptSession session;
        const bool froze = session.freezeToSculpt(source, object.objectId());
        r.check("pln17_freeze_succeeds_on_open_plane_mesh", froze);
        r.check("pln17_freeze_enters_sculpt_mode", session.mode() == ProductMode::Sculpt);

        const SculptMesh& mesh = session.mesh();
        r.check("pln17_frozen_mesh_is_still_exactly_4_6",
                mesh.vertexCount() == 4 && mesh.indexCount() == 6);
        r.check("pln17_frozen_mesh_copies_plane_vertices_exactly",
                sameVertices(mesh.vertices(), source.vertices));
        r.check("pln17_frozen_mesh_copies_plane_indices_exactly",
                sameIndices(mesh.indices(), source.indices));
        r.check("pln17_freeze_preserves_object_id", mesh.objectId() == object.objectId());

        // The Construction Source is untouched by Freeze: kind, parameters and
        // the regenerated mesh are all exactly what they were.
        r.check("pln17_freeze_leaves_primitive_kind", object.kind() == PrimitiveKind::Plane);
        r.check("pln17_freeze_leaves_plane_parameters",
                object.plane().widthMeters() == 2.0 && object.plane().depthMeters() == 1.5);
        r.check("pln17_freeze_leaves_construction_mesh_regenerable",
                sameVertices(object.generateMesh().vertices, source.vertices));

        // A real sculpt edit on the sparse 4-vertex plane still writes and
        // reads back correctly — the open topology is not a special case for
        // vertex writes either, even though a real brush STROKE is exercised
        // on a denser primitive elsewhere (a 4-vertex mesh is too sparse for a
        // realistic brush capture, per the stage's own carve-out).
        session.mesh().setVertexPosition(0, Vec3{5.0f, 5.0f, 5.0f});
        r.check("pln17_direct_vertex_edit_visible_in_sculpt_mesh",
                nearlyVec(session.mesh().vertexPosition(0), Vec3{5.0f, 5.0f, 5.0f}));
        r.check("pln17_direct_vertex_edit_does_not_reach_construction_source",
                sameVertices(object.generateMesh().vertices, source.vertices));

        // PLN-18: Back to Construction and Resume Sculpt round-trip both
        // representations without disturbing either.
        session.enterConstruction();
        r.check("pln18_back_to_construction_switches_mode",
                session.mode() == ProductMode::Construction);
        r.check("pln18_back_to_construction_keeps_frozen_plane_mesh", session.hasSculptMesh());
        r.check("pln18_back_to_construction_keeps_the_edit",
                nearlyVec(session.mesh().vertexPosition(0), Vec3{5.0f, 5.0f, 5.0f}));

        const uint64_t freezesBeforeResume = session.freezeCount();
        r.check("pln18_resume_sculpt_succeeds", session.enterSculpt());
        r.check("pln18_resume_sculpt_does_not_refreeze",
                session.freezeCount() == freezesBeforeResume);
        r.check("pln18_resume_sculpt_restores_the_edit_down_to_the_pixel",
                nearlyVec(session.mesh().vertexPosition(0), Vec3{5.0f, 5.0f, 5.0f}));
        r.check("pln18_resume_sculpt_keeps_open_topology",
                session.mesh().vertexCount() == 4 && session.mesh().indexCount() == 6);

        // PLN-19: a Construction edit after Freeze establishes the standard
        // stale-source state and does NOT touch the existing frozen mesh.
        r.check("pln19_source_not_stale_after_freeze", !session.sourceStale());
        object.setPrimitive(PrimitiveSpec::forPlane(4.0, 0.5));
        session.markSourceStale();
        r.check("pln19_plane_edit_after_freeze_marks_source_stale", session.sourceStale());
        r.check("pln19_stale_source_does_not_replace_frozen_plane_mesh",
                nearlyVec(session.mesh().vertexPosition(0), Vec3{5.0f, 5.0f, 5.0f}) &&
                    session.mesh().vertexCount() == 4);
        r.check("pln19_stale_source_does_not_auto_refreeze",
                session.freezeCount() == freezesBeforeResume);

        // Only an explicit re-Freeze is destructive, exactly as for every other
        // primitive: it clears staleness and starts a fresh sculpt revision
        // from the NEW (4.0 x 0.5) plane.
        session.freezeToSculpt(object.generateMesh(), object.objectId());
        r.check("pln19_explicit_refreeze_clears_stale_for_plane", !session.sourceStale());
        r.check("pln19_explicit_refreeze_restarts_sculpt_revision",
                session.mesh().revision() == 1);
        r.check("pln19_explicit_refreeze_reflects_the_new_plane_dimensions",
                object.plane().widthMeters() == 4.0 && object.plane().depthMeters() == 0.5);
    }

    // -----------------------------------------------------------------------
    // Publication of the sculpt representation
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        MeshStore store(object.objectId());
        SculptSession session;
        session.freezeToSculpt(object.generateMesh(), object.objectId());

        const MeshRevision first = publishSculptMesh(session.mesh(), store);
        r.check("sculpt_publish_mints_a_mesh_revision", first != kNoMeshRevision);
        r.check("sculpt_publish_keeps_object_id", store.objectId() == object.objectId());
        const RuntimeMeshPtr published = store.current();
        r.check("published_sculpt_topology_matches",
                published && published->vertexCount() == kSphereVertexCount &&
                    published->indexCount() == kSphereIndexCount);

        session.mesh().setVertexPosition(0, Vec3{1.5f, 0.0f, 0.0f});
        session.mesh().advanceRevision();
        const MeshRevision second = publishSculptMesh(session.mesh(), store);
        r.check("second_sculpt_publish_is_newer", second > first);

        // The two counters are genuinely different things: a Freeze restarts the
        // sculpt mesh's own revision at 1, while the store's keeps climbing and
        // never restarts. Comparing them after a re-freeze is what makes that
        // visible rather than merely asserted.
        session.freezeToSculpt(object.generateMesh(), object.objectId());
        publishSculptMesh(session.mesh(), store);
        r.check("freeze_restarts_the_sculpt_revision", session.mesh().revision() == 1);
        r.check("mesh_store_revision_never_restarts", store.currentRevision() > second);

        SculptSession unfrozen;
        r.check("publishing_an_unfrozen_sculpt_mesh_is_refused",
                publishSculptMesh(unfrozen.mesh(), store) == kNoMeshRevision);
    }

    // -----------------------------------------------------------------------
    // Grab: starting a stroke
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        SculptSession session;
        session.freezeToSculpt(object.generateMesh(), object.objectId());

        r.check("grab_starts_on_a_hit",
                session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                    identity, identity));
        r.check("stroke_is_active_after_begin", session.stroke().active());
        r.check("stroke_counted", session.strokeCount() == 1);
        r.check("stroke_captured_vertices", session.stroke().affectedVertexCount() > 0);
        r.check("stroke_hit_depth_is_positive", session.stroke().hitDepth() > 0.0f);
        r.check("stroke_world_per_pixel_is_positive", session.stroke().worldPerPixel() > 0.0f);
        r.check("stroke_world_radius_is_positive", session.stroke().worldRadius() > 0.0f);
        // Brush radius is authored in pixels and resolved at the hit depth.
        r.check("stroke_local_radius_is_pixels_times_scale",
                nearly(session.stroke().worldRadius(),
                       session.radiusPixels() * session.stroke().worldPerPixel()));
        session.endStroke();
        r.check("stroke_inactive_after_end", !session.stroke().active());

        // A miss must not start a stroke, and must not count one.
        r.check("grab_does_not_start_on_a_miss",
                !session.beginStroke(camera, 20.0f, 40.0f, kViewportWidth, kViewportHeight,
                                     identity, identity));
        r.check("missed_grab_is_not_counted", session.strokeCount() == 1);
        r.check("missed_grab_leaves_no_stroke", !session.stroke().active());

        // The brush exists only in Sculpt mode.
        session.enterConstruction();
        r.check("grab_refused_in_construction_mode",
                !session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                     identity, identity));
        session.enterSculpt();
    }

    // -----------------------------------------------------------------------
    // Grab: radius selects, falloff weights, and the untouched remainder
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();

        SculptSession small;
        small.freezeToSculpt(source, object.objectId());
        small.setRadiusPixels(60.0f);
        small.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                          identity);
        const int smallCount = small.stroke().affectedVertexCount();

        SculptSession large;
        large.freezeToSculpt(source, object.objectId());
        large.setRadiusPixels(240.0f);
        large.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                          identity);
        const int largeCount = large.stroke().affectedVertexCount();

        r.check("small_radius_captures_vertices", smallCount > 0);
        r.check("larger_radius_captures_strictly_more", largeCount > smallCount);
        r.check("larger_radius_is_a_larger_local_radius",
                large.stroke().worldRadius() > small.stroke().worldRadius());

        // Every captured vertex is genuinely inside the brush, and its weight is
        // exactly the documented falloff of its distance.
        bool allInside = true;
        bool allWeightsMatchFalloff = true;
        float nearestDistance = 1e30f;
        float farthestDistance = -1.0f;
        float nearestWeight = 0.0f;
        float farthestWeight = 0.0f;
        const Vec3 centre = large.stroke().localCenter();
        const float radius = large.stroke().worldRadius();
        for (int i = 0; i < largeCount; ++i) {
            const SculptStrokeVertex& v = large.stroke().affectedVertex(i);
            const Vec3 d = vec3Sub(v.basePosition, centre);
            const float distance = std::sqrt(vec3Dot(d, d));
            if (!(distance < radius)) {
                allInside = false;
            }
            if (!nearly(v.weight, sculptFalloff(distance, radius))) {
                allWeightsMatchFalloff = false;
            }
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestWeight = v.weight;
            }
            if (distance > farthestDistance) {
                farthestDistance = distance;
                farthestWeight = v.weight;
            }
        }
        r.check("every_captured_vertex_is_inside_the_radius", allInside);
        r.check("captured_weights_are_the_documented_falloff", allWeightsMatchFalloff);
        r.check("centre_weight_exceeds_edge_weight", nearestWeight > farthestWeight);
        r.check("every_captured_weight_is_positive", nearestWeight > 0.0f && farthestWeight > 0.0f);

        // Vertices outside the brush have no weight at all.
        int outsideWithWeight = 0;
        for (uint32_t i = 0; i < large.mesh().vertexCount(); ++i) {
            const Vec3 d = vec3Sub(large.mesh().vertexPosition(i), centre);
            if (std::sqrt(vec3Dot(d, d)) >= radius && large.stroke().weightOfVertex(i) != 0.0f) {
                ++outsideWithWeight;
            }
        }
        r.check("vertices_outside_the_radius_have_no_weight", outsideWithWeight == 0);

        // A drag moves the captured set and nothing else.
        large.updateStroke(kCentreX + 150.0f, kCentreY);
        int movedOutside = 0;
        for (uint32_t i = 0; i < large.mesh().vertexCount(); ++i) {
            if (large.stroke().weightOfVertex(i) != 0.0f) {
                continue;
            }
            const Vec3 now = large.mesh().vertexPosition(i);
            const MeshVertex& before = source.vertices[i];
            if (now.x != before.position[0] || now.y != before.position[1] ||
                now.z != before.position[2]) {
                ++movedOutside;
            }
        }
        r.check("drag_leaves_vertices_outside_the_radius_untouched", movedOutside == 0);
        r.check("drag_left_no_nonfinite_position", allPositionsFinite(large.mesh()));
        r.check("drag_preserves_indices", sameIndices(large.mesh().indices(), source.indices));
        r.check("drag_preserves_vertex_count",
                large.mesh().vertexCount() == kSphereVertexCount);
        r.check("drag_advanced_the_sculpt_revision", large.mesh().revision() > 1);
        r.check("drag_left_construction_source_unchanged",
                sameVertices(object.generateMesh().vertices, source.vertices));
        r.check("drag_left_construction_parameters_unchanged",
                object.spec().sphere() != nullptr && object.spec().sphere()->diameter == 2.0);
    }

    // -----------------------------------------------------------------------
    // Grab: the displacement itself
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();

        SculptSession session;
        session.freezeToSculpt(source, object.objectId());
        session.setStrength(1.0f);
        session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);
        const float scale = session.stroke().worldPerPixel();
        const float dx = 120.0f;
        const float dy = -80.0f;
        r.check("stroke_moved_something", session.updateStroke(kCentreX + dx, kCentreY + dy));

        // With the identity transform, local space IS world space, so the
        // displacement must be exactly the camera-plane vector for that travel.
        const Vec3 expected = expectedWorldDelta(camera, dx, dy, scale);
        r.check("displacement_follows_the_camera_plane",
                nearlyVec(session.stroke().lastLocalDisplacement(), expected));

        // Screen right is camera right, and screen DOWN is camera -up. Checking
        // both signs separately is what catches a flipped Y.
        SculptSession rightOnly;
        rightOnly.freezeToSculpt(source, object.objectId());
        rightOnly.setStrength(1.0f);
        rightOnly.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                              identity);
        rightOnly.updateStroke(kCentreX + 100.0f, kCentreY);
        const Vec3 right{camera.view.m[0], camera.view.m[4], camera.view.m[8]};
        r.check("dragging_right_moves_along_camera_right",
                vec3Dot(rightOnly.stroke().lastLocalDisplacement(), right) > 0.0f);

        SculptSession downOnly;
        downOnly.freezeToSculpt(source, object.objectId());
        downOnly.setStrength(1.0f);
        downOnly.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                             identity);
        downOnly.updateStroke(kCentreX, kCentreY + 100.0f);
        const Vec3 up{camera.view.m[1], camera.view.m[5], camera.view.m[9]};
        r.check("dragging_down_moves_against_camera_up",
                vec3Dot(downOnly.stroke().lastLocalDisplacement(), up) < 0.0f);

        // Every captured vertex sits at base + displacement * its own weight.
        bool allPlaced = true;
        for (int i = 0; i < session.stroke().affectedVertexCount(); ++i) {
            const SculptStrokeVertex& v = session.stroke().affectedVertex(i);
            const Vec3 want = vec3Add(v.basePosition, vec3Scale(expected, v.weight));
            if (!nearlyVec(session.mesh().vertexPosition(v.index), want)) {
                allPlaced = false;
            }
        }
        r.check("each_vertex_moved_by_displacement_times_its_weight", allPlaced);

        // The affected set is captured at stroke start and does not change while
        // the finger moves, however far it goes.
        const int capturedBefore = session.stroke().affectedVertexCount();
        session.updateStroke(kCentreX + 900.0f, kCentreY + 900.0f);
        r.check("affected_set_is_fixed_for_the_whole_stroke",
                session.stroke().affectedVertexCount() == capturedBefore);
        r.check("long_drag_left_no_nonfinite_position", allPositionsFinite(session.mesh()));
    }

    // -----------------------------------------------------------------------
    // Grab: strength scales the displacement
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();

        SculptSession full;
        full.freezeToSculpt(source, object.objectId());
        full.setStrength(1.0f);
        full.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                         identity);
        full.updateStroke(kCentreX + 200.0f, kCentreY);

        SculptSession half;
        half.freezeToSculpt(source, object.objectId());
        half.setStrength(0.5f);
        half.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                         identity);
        half.updateStroke(kCentreX + 200.0f, kCentreY);

        r.check("half_strength_halves_the_displacement",
                nearlyVec(half.stroke().lastLocalDisplacement(),
                          vec3Scale(full.stroke().lastLocalDisplacement(), 0.5f)));
        r.check("strength_does_not_change_the_affected_set",
                half.stroke().affectedVertexCount() == full.stroke().affectedVertexCount());
        r.check("strength_is_clamped_into_the_documented_range",
                (half.setStrength(99.0f), half.strength() == kMaxBrushStrength));
    }

    // -----------------------------------------------------------------------
    // Grab: the object transform
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        TransformValues placement;
        placement.positionX = 0.5;
        placement.positionY = -0.25;
        placement.rotationZ = 90.0;
        placement_.setValues(placement);
        const Mat4 model = placement_.modelMatrix();
        const Mat4 inverseModel = placement_.inverseModelMatrix();
        const ConstructionMesh source = object.generateMesh();

        SculptSession session;
        session.freezeToSculpt(source, object.objectId());
        session.setStrength(1.0f);
        const bool started = session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth,
                                                 kViewportHeight, model, inverseModel);
        r.check("grab_starts_on_a_transformed_object", started);

        const float scale = session.stroke().worldPerPixel();
        session.updateStroke(kCentreX + 130.0f, kCentreY - 70.0f);
        const Vec3 world = expectedWorldDelta(camera, 130.0f, -70.0f, scale);
        const Vec3 wantLocal = mat4TransformDirection(inverseModel, world);
        r.check("displacement_is_converted_into_local_space",
                nearlyVec(session.stroke().lastLocalDisplacement(), wantLocal));

        // A rigid transform preserves length, so the local displacement is the
        // same size as the world one it came from — and, under a 90 degree Z
        // rotation, is genuinely a different vector.
        const float worldLength = std::sqrt(vec3Dot(world, world));
        const Vec3 got = session.stroke().lastLocalDisplacement();
        r.check("local_displacement_keeps_its_length",
                nearly(std::sqrt(vec3Dot(got, got)), worldLength));
        r.check("local_displacement_differs_from_the_world_one_under_rotation",
                !nearlyVec(got, world));
        r.check("transformed_stroke_left_no_nonfinite_position", allPositionsFinite(session.mesh()));
        r.check("transformed_stroke_left_construction_source_unchanged",
                sameVertices(object.generateMesh().vertices, source.vertices));
        r.check("transformed_stroke_left_transform_unchanged",
                placement_.values().positionX == 0.5 &&
                    placement_.values().rotationZ == 90.0);
    }

    // -----------------------------------------------------------------------
    // Stroke end, cancel, and the absence of stale state
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();
        SculptSession session;
        session.freezeToSculpt(source, object.objectId());
        session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);
        session.updateStroke(kCentreX + 90.0f, kCentreY);
        const uint32_t movedIndex = session.stroke().affectedVertex(0).index;
        const Vec3 deformed = session.mesh().vertexPosition(movedIndex);
        r.check("the_stroke_actually_moved_that_vertex",
                !nearlyVec(deformed, Vec3{source.vertices[movedIndex].position[0],
                                          source.vertices[movedIndex].position[1],
                                          source.vertices[movedIndex].position[2]}));

        session.cancelStroke();
        r.check("cancel_clears_the_stroke", !session.stroke().active());
        r.check("cancel_releases_the_captured_set",
                session.stroke().affectedVertexCount() == 0);
        r.check("cancel_clears_the_last_displacement",
                nearlyVec(session.stroke().lastLocalDisplacement(), Vec3{0.0f, 0.0f, 0.0f}));
        const SculptRevision afterCancel = session.mesh().revision();
        r.check("update_after_cancel_does_nothing",
                !session.updateStroke(kCentreX + 400.0f, kCentreY));
        r.check("update_after_cancel_publishes_no_revision",
                session.mesh().revision() == afterCancel);
        // A cancelled stroke is a stroke that STOPPED, not one that is undone:
        // there is no undo in this stage, and inventing a partial one here would
        // be worse than keeping what the finger already did.
        r.check("cancel_keeps_what_the_stroke_already_did",
                nearlyVec(session.mesh().vertexPosition(movedIndex), deformed));
        r.check("cancel_left_no_nonfinite_position", allPositionsFinite(session.mesh()));

        // A fresh stroke after a cancel starts cleanly.
        r.check("a_new_stroke_starts_after_a_cancel",
                session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                    identity, identity));
        session.endStroke();
        r.check("update_after_end_does_nothing",
                !session.updateStroke(kCentreX + 400.0f, kCentreY));

        // Entering Construction mid-stroke drops the stroke, not the geometry.
        session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);
        session.enterConstruction();
        r.check("switching_to_construction_clears_a_live_stroke", !session.stroke().active());
        r.check("switching_to_construction_keeps_the_sculpt_mesh", session.hasSculptMesh());
        r.check("no_construction_source_damage_after_all_strokes",
                sameVertices(object.generateMesh().vertices, source.vertices));
    }

    // =======================================================================
    // The tool set
    // =======================================================================
    {
        r.check("tool_count_is_four", kSculptToolCount == 4);
        SculptTool decoded = SculptTool::Smooth;
        r.check("tool_index_0_is_grab",
                sculptToolFromIndex(0, &decoded) && decoded == SculptTool::Grab);
        r.check("tool_index_1_is_clay",
                sculptToolFromIndex(1, &decoded) && decoded == SculptTool::Clay);
        r.check("tool_index_2_is_smooth",
                sculptToolFromIndex(2, &decoded) && decoded == SculptTool::Smooth);
        r.check("tool_index_3_is_inflate",
                sculptToolFromIndex(3, &decoded) && decoded == SculptTool::Inflate);
        // Refused, not clamped: an unknown tool is a caller bug, and repairing
        // it into a neighbouring tool would silently sculpt with something the
        // user did not choose.
        r.check("tool_index_negative_is_refused", !sculptToolFromIndex(-1, &decoded));
        r.check("tool_index_past_end_is_refused", !sculptToolFromIndex(4, &decoded));
        r.check("tool_index_round_trips",
                sculptToolIndex(SculptTool::Inflate) == 3 &&
                    sculptToolIndex(SculptTool::Grab) == 0);
        r.check("only_clay_and_inflate_use_normals",
                sculptToolUsesNormals(SculptTool::Clay) &&
                    sculptToolUsesNormals(SculptTool::Inflate) &&
                    !sculptToolUsesNormals(SculptTool::Grab) &&
                    !sculptToolUsesNormals(SculptTool::Smooth));

        SculptSession session;
        r.check("default_tool_is_grab", session.tool() == kDefaultSculptTool &&
                                            session.tool() == SculptTool::Grab);
        session.setTool(SculptTool::Inflate);
        r.check("tool_is_native_state", session.tool() == SculptTool::Inflate);
    }

    // =======================================================================
    // Fixed-topology adjacency
    // =======================================================================
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        SculptSession session;
        session.freezeToSculpt(object.generateMesh(), object.objectId());
        const SculptMesh& mesh = session.mesh();
        const SculptTopology& topology = mesh.topology();

        r.check("adjacency_is_built_by_freeze", topology.built());
        r.check("adjacency_covers_every_vertex", topology.vertexCount() == mesh.vertexCount());
        r.check("adjacency_built_exactly_once_per_freeze", topology.buildCount() == 1);

        bool everyNeighborInRange = true;
        bool noSelfNeighbor = true;
        bool noDuplicateNeighbor = true;
        bool everyVertexHasNeighbors = true;
        bool symmetric = true;
        for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
            uint32_t count = 0;
            const uint32_t* list = topology.neighbors(v, &count);
            if (count == 0) {
                everyVertexHasNeighbors = false;
                continue;
            }
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t n = list[i];
                if (n >= mesh.vertexCount()) everyNeighborInRange = false;
                if (n == v) noSelfNeighbor = false;
                if (i > 0 && list[i] <= list[i - 1]) noDuplicateNeighbor = false;
                // Every edge is shared by two triangles, so seeing it from one
                // end must mean seeing it from the other.
                uint32_t backCount = 0;
                const uint32_t* back = topology.neighbors(n, &backCount);
                bool found = false;
                for (uint32_t j = 0; j < backCount; ++j) {
                    if (back[j] == v) { found = true; break; }
                }
                if (!found) symmetric = false;
            }
        }
        r.check("no_neighbor_index_is_out_of_range", everyNeighborInRange);
        r.check("no_vertex_is_its_own_neighbor", noSelfNeighbor);
        r.check("neighbor_lists_are_sorted_and_deduplicated", noDuplicateNeighbor);
        r.check("every_sphere_vertex_has_neighbors", everyVertexHasNeighbors);
        r.check("adjacency_is_symmetric", symmetric);

        // Every directed edge of the index buffer must be present in the
        // adjacency, which is what makes "correct on a known mesh" a statement
        // about the actual data rather than about counts.
        bool everyEdgePresent = true;
        for (size_t t = 0; t + 2 < mesh.indices().size(); t += 3) {
            const uint32_t corner[3] = {mesh.indices()[t], mesh.indices()[t + 1],
                                        mesh.indices()[t + 2]};
            for (int e = 0; e < 3; ++e) {
                const uint32_t a = corner[e];
                const uint32_t b = corner[(e + 1) % 3];
                uint32_t count = 0;
                const uint32_t* list = topology.neighbors(a, &count);
                bool found = false;
                for (uint32_t i = 0; i < count; ++i) {
                    if (list[i] == b) { found = true; break; }
                }
                if (!found) everyEdgePresent = false;
            }
        }
        r.check("every_index_buffer_edge_is_in_the_adjacency", everyEdgePresent);

        // The known mesh: the sphere's poles are one vertex fanned to 32
        // meridians, so each pole has exactly 32 neighbours and 32 incident
        // triangles. A collapsed-quad pole would not.
        const uint32_t northPole = kSphereVertexCount - 2;
        const uint32_t southPole = kSphereVertexCount - 1;
        r.check("north_pole_has_one_ring_of_32", topology.neighborCount(northPole) == 32);
        r.check("south_pole_has_one_ring_of_32", topology.neighborCount(southPole) == 32);
        r.check("north_pole_has_32_incident_triangles",
                topology.incidentTriangleCount(northPole) == 32);
        r.check("south_pole_has_32_incident_triangles",
                topology.incidentTriangleCount(southPole) == 32);

        // Every corner of every triangle is accounted for exactly once.
        uint32_t incidentTotal = 0;
        bool incidentInRange = true;
        const auto triangleCount = static_cast<uint32_t>(mesh.indexCount() / 3);
        for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
            uint32_t count = 0;
            const uint32_t* list = topology.incidentTriangles(v, &count);
            incidentTotal += count;
            for (uint32_t i = 0; i < count; ++i) {
                if (list[i] >= triangleCount) incidentInRange = false;
            }
        }
        r.check("incident_triangles_account_for_every_corner",
                incidentTotal == mesh.indexCount());
        r.check("no_incident_triangle_is_out_of_range", incidentInRange);

        // Out-of-range queries are answered, not trusted.
        uint32_t count = 0;
        r.check("neighbors_of_an_out_of_range_vertex_is_null",
                topology.neighbors(mesh.vertexCount(), &count) == nullptr && count == 0);
        r.check("incident_triangles_of_an_out_of_range_vertex_is_null",
                topology.incidentTriangles(mesh.vertexCount(), &count) == nullptr && count == 0);

        // The adjacency is NOT rebuilt by sculpting: topology is fixed for the
        // life of a frozen mesh, and rebuilding it per move is exactly the cost
        // this structure exists to avoid.
        session.setTool(SculptTool::Clay);
        session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);
        driveTravel(session, kCentreX, kCentreY, 6, 40.0f);
        session.endStroke();
        r.check("adjacency_is_not_rebuilt_by_a_stroke", topology.buildCount() == 1);
        r.check("adjacency_survives_a_stroke_intact",
                topology.neighborCount(northPole) == 32 &&
                    topology.vertexCount() == mesh.vertexCount());

        // A second Freeze rebuilds it — because that is a different mesh.
        session.freezeToSculpt(object.generateMesh(), object.objectId());
        r.check("a_second_freeze_rebuilds_the_adjacency",
                session.mesh().topology().buildCount() == 2);
    }

    // =======================================================================
    // Vertex normals
    // =======================================================================
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        SculptSession session;
        session.freezeToSculpt(object.generateMesh(), object.objectId());
        const SculptMesh& mesh = session.mesh();

        const std::vector<Vec3>& normals = mesh.vertexNormals();
        r.check("one_normal_per_vertex", normals.size() == mesh.vertices().size());

        bool allFinite = true;
        bool allUnit = true;
        bool allOutward = true;
        for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
            const Vec3 n = normals[v];
            if (!vec3Finite(n)) allFinite = false;
            if (!nearly(lengthOf(n), 1.0f)) allUnit = false;
            // A sphere centred on the local origin is the one mesh whose
            // normals are known independently: each is the normalized position.
            const Vec3 radial = vec3Normalize(mesh.vertexPosition(v));
            if (vec3Dot(n, radial) < 0.99f) allOutward = false;
        }
        r.check("every_normal_is_finite", allFinite);
        r.check("every_sphere_normal_is_unit_length", allUnit);
        r.check("every_sphere_normal_points_radially_outward", allOutward);

        // The recomputation rule: normals follow the positions they were derived
        // from, and are recomputed at most once per batch of writes.
        const uint64_t afterFirstRead = mesh.normalRecomputeCount();
        mesh.vertexNormals();
        mesh.vertexNormals();
        r.check("normals_are_not_recomputed_when_nothing_moved",
                mesh.normalRecomputeCount() == afterFirstRead);
        session.mesh().setVertexPosition(0, vec3Scale(mesh.vertexPosition(0), 1.5f));
        session.mesh().setVertexPosition(1, vec3Scale(mesh.vertexPosition(1), 1.5f));
        mesh.vertexNormals();
        mesh.vertexNormals();
        r.check("a_batch_of_writes_costs_exactly_one_recompute",
                mesh.normalRecomputeCount() == afterFirstRead + 1);

        // Degenerate input must be safe rather than merely unlikely.
        std::vector<MeshVertex> degenerate(3);
        for (int i = 0; i < 3; ++i) {
            degenerate[i].position[0] = 1.0f;
            degenerate[i].position[1] = 2.0f;
            degenerate[i].position[2] = 3.0f;
        }
        const std::vector<uint32_t> degenerateIndices{0, 1, 2};
        std::vector<Vec3> degenerateNormals;
        computeVertexNormals(degenerate, degenerateIndices, &degenerateNormals);
        bool degenerateSafe = degenerateNormals.size() == 3;
        for (const Vec3& n : degenerateNormals) {
            if (!vec3Finite(n)) degenerateSafe = false;
            if (n.x != 0.0f || n.y != 0.0f || n.z != 0.0f) degenerateSafe = false;
        }
        r.check("a_zero_area_triangle_yields_a_finite_zero_normal", degenerateSafe);

        // A vertex no triangle references has no defined direction, and gets
        // zero rather than an invented axis.
        std::vector<MeshVertex> withOrphan(4);
        withOrphan[0].position[0] = 0.0f; withOrphan[0].position[1] = 0.0f; withOrphan[0].position[2] = 0.0f;
        withOrphan[1].position[0] = 1.0f; withOrphan[1].position[1] = 0.0f; withOrphan[1].position[2] = 0.0f;
        withOrphan[2].position[0] = 0.0f; withOrphan[2].position[1] = 1.0f; withOrphan[2].position[2] = 0.0f;
        withOrphan[3].position[0] = 5.0f; withOrphan[3].position[1] = 5.0f; withOrphan[3].position[2] = 5.0f;
        std::vector<Vec3> orphanNormals;
        computeVertexNormals(withOrphan, degenerateIndices, &orphanNormals);
        r.check("an_unreferenced_vertex_gets_a_zero_normal",
                orphanNormals.size() == 4 && orphanNormals[3].x == 0.0f &&
                    orphanNormals[3].y == 0.0f && orphanNormals[3].z == 0.0f);
        r.check("a_real_triangle_still_produces_a_unit_normal",
                nearly(lengthOf(orphanNormals[0]), 1.0f));
        // Out-of-range indices are skipped rather than dereferenced.
        const std::vector<uint32_t> badIndices{0, 1, 99};
        std::vector<Vec3> badNormals;
        computeVertexNormals(withOrphan, badIndices, &badNormals);
        bool badSafe = badNormals.size() == 4;
        for (const Vec3& n : badNormals) {
            if (!vec3Finite(n)) badSafe = false;
        }
        r.check("an_out_of_range_index_cannot_poison_normals", badSafe);
    }

    // =======================================================================
    // The shared kernel: one stroke lifecycle for all four tools
    // =======================================================================
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();
        const SculptTool tools[kSculptToolCount] = {SculptTool::Grab, SculptTool::Clay,
                                                    SculptTool::Smooth, SculptTool::Inflate};

        bool everyToolBegins = true;
        bool everyToolMissesOffMesh = true;
        bool everyToolRefusesOutsideSculptMode = true;
        bool everyToolHoldsItsTool = true;
        bool everyToolKeepsAStableAffectedSet = true;
        bool everyToolEndsClean = true;
        bool everyToolStaysFinite = true;
        bool everyToolLeavesConstructionAlone = true;
        bool everyToolUsesTheSameFalloff = true;

        for (int t = 0; t < kSculptToolCount; ++t) {
            SculptSession session;
            session.setTool(tools[t]);
            // Before any Freeze there is nothing to sculpt, whatever the tool.
            if (session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                    identity, identity)) {
                everyToolRefusesOutsideSculptMode = false;
            }
            prepareSession(&session, object, tools[t], 150.0f, 0.8f);

            // A miss starts NO stroke — the same rule for every tool.
            if (session.beginStroke(camera, 20.0f, 40.0f, kViewportWidth, kViewportHeight, identity,
                                    identity)) {
                everyToolMissesOffMesh = false;
            }
            if (session.stroke().active()) {
                everyToolMissesOffMesh = false;
            }

            if (!session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                     identity, identity)) {
                everyToolBegins = false;
                continue;
            }
            if (session.stroke().tool() != tools[t]) {
                everyToolHoldsItsTool = false;
            }

            // The affected set and its weights are captured once and do not
            // change while the finger moves — for every tool.
            const int capturedCount = session.stroke().affectedVertexCount();
            std::vector<uint32_t> capturedIndices;
            std::vector<float> capturedWeights;
            for (int i = 0; i < capturedCount; ++i) {
                capturedIndices.push_back(session.stroke().affectedVertex(i).index);
                capturedWeights.push_back(session.stroke().affectedVertex(i).weight);
            }

            // Every tool's weights come from the one shared falloff, evaluated
            // at the captured distance from the stroke's own centre.
            const Vec3 centre = session.stroke().localCenter();
            const float radius = session.stroke().worldRadius();
            for (int i = 0; i < capturedCount; ++i) {
                const Vec3 d = vec3Sub(session.stroke().affectedVertex(i).basePosition, centre);
                if (!nearly(capturedWeights[i], sculptFalloff(lengthOf(d), radius))) {
                    everyToolUsesTheSameFalloff = false;
                }
            }

            driveTravel(session, kCentreX, kCentreY, 6, 45.0f);

            if (session.stroke().affectedVertexCount() != capturedCount) {
                everyToolKeepsAStableAffectedSet = false;
            } else {
                for (int i = 0; i < capturedCount; ++i) {
                    if (session.stroke().affectedVertex(i).index != capturedIndices[i] ||
                        session.stroke().affectedVertex(i).weight != capturedWeights[i]) {
                        everyToolKeepsAStableAffectedSet = false;
                    }
                }
            }

            if (!allPositionsFinite(session.mesh())) {
                everyToolStaysFinite = false;
            }

            session.endStroke();
            if (session.stroke().active() || session.stroke().affectedVertexCount() != 0) {
                everyToolEndsClean = false;
            }
            if (session.updateStroke(kCentreX + 300.0f, kCentreY)) {
                everyToolEndsClean = false;
            }

            // Topology is never touched, whatever the tool did.
            if (session.mesh().vertexCount() != kSphereVertexCount ||
                session.mesh().indexCount() != kSphereIndexCount ||
                !sameIndices(session.mesh().indices(), source.indices)) {
                everyToolStaysFinite = false;
            }

            // The Construction Source is untouched — the invariant that matters
            // most, checked once per tool rather than once per stage.
            if (!sameVertices(object.generateMesh().vertices, source.vertices)) {
                everyToolLeavesConstructionAlone = false;
            }
        }

        r.check("every_tool_starts_a_stroke_on_a_hit", everyToolBegins);
        r.check("every_tool_starts_no_stroke_on_a_miss", everyToolMissesOffMesh);
        r.check("no_tool_can_stroke_outside_sculpt_mode", everyToolRefusesOutsideSculptMode);
        r.check("a_stroke_holds_the_tool_it_began_with", everyToolHoldsItsTool);
        r.check("every_tool_keeps_a_stable_affected_set", everyToolKeepsAStableAffectedSet);
        r.check("every_tool_uses_the_shared_falloff", everyToolUsesTheSameFalloff);
        r.check("every_tool_ends_and_leaves_no_stale_stroke", everyToolEndsClean);
        r.check("every_tool_stays_finite_and_preserves_topology", everyToolStaysFinite);
        r.check("no_tool_touches_the_construction_source", everyToolLeavesConstructionAlone);
    }

    // -----------------------------------------------------------------------
    // Changing the tool cannot change a stroke already running
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        SculptSession session;
        prepareSession(&session, object, SculptTool::Clay, 150.0f, 0.8f);
        session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);
        session.setTool(SculptTool::Grab);
        r.check("the_session_tool_changed", session.tool() == SculptTool::Grab);
        r.check("the_live_stroke_kept_its_own_tool",
                session.stroke().tool() == SculptTool::Clay);
        session.endStroke();
        r.check("the_next_stroke_uses_the_new_tool",
                session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                    identity, identity) &&
                    session.stroke().tool() == SculptTool::Grab);
        session.endStroke();
    }

    // -----------------------------------------------------------------------
    // The arbitration probe: asking "would a stroke start here?" mutates nothing
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        SculptSession session;
        prepareSession(&session, object, SculptTool::Inflate, 150.0f, 1.0f);
        const std::vector<MeshVertex> before = session.mesh().vertices();
        const SculptRevision revisionBefore = session.mesh().revision();
        const uint64_t strokesBefore = session.strokeCount();

        r.check("the_probe_reports_a_hit_on_the_mesh",
                session.hitsSculptMesh(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                       identity));
        r.check("the_probe_reports_a_miss_off_the_mesh",
                !session.hitsSculptMesh(camera, 20.0f, 40.0f, kViewportWidth, kViewportHeight,
                                        identity));
        for (int i = 0; i < 20; ++i) {
            session.hitsSculptMesh(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                   identity);
        }
        // This is the native half of the multitouch guarantee: a gesture that
        // turns out to be navigation only ever reaches this probe, and the probe
        // provably cannot move a vertex, mint a revision or start a stroke.
        r.check("the_probe_moves_no_vertex", sameVertices(session.mesh().vertices(), before));
        r.check("the_probe_mints_no_sculpt_revision",
                session.mesh().revision() == revisionBefore);
        r.check("the_probe_starts_no_stroke", !session.stroke().active());
        r.check("the_probe_advances_no_stroke_count", session.strokeCount() == strokesBefore);

        session.enterConstruction();
        r.check("the_probe_reports_a_miss_in_construction_mode",
                !session.hitsSculptMesh(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                        identity));
        SculptSession unfrozen;
        r.check("the_probe_reports_a_miss_with_nothing_frozen",
                !unfrozen.hitsSculptMesh(camera, kCentreX, kCentreY, kViewportWidth,
                                         kViewportHeight, identity));
    }

    // =======================================================================
    // Clay
    // =======================================================================
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();
        SculptSession session;
        prepareSession(&session, object, SculptTool::Clay, 150.0f, 0.8f);
        session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);

        const int centreSlot = slotOfHighestWeight(session.stroke());
        const int edgeSlot = slotOfLowestWeight(session.stroke());
        const SculptStrokeVertex centreVertex = session.stroke().affectedVertex(centreSlot);
        const SculptStrokeVertex edgeVertex = session.stroke().affectedVertex(edgeSlot);
        const int count = session.stroke().affectedVertexCount();
        r.check("clay_captured_a_meaningful_set", count > 4);
        r.check("clay_captured_a_real_weight_spread", centreVertex.weight > edgeVertex.weight);
        r.check("clay_captured_a_unit_base_normal",
                nearly(lengthOf(centreVertex.baseNormal), 1.0f));

        const int applied = driveTravel(session, kCentreX, kCentreY, 6, 50.0f);
        r.check("clay_applied_every_move_with_travel", applied == 6);

        const Vec3 centreAfter = session.mesh().vertexPosition(centreVertex.index);
        const Vec3 edgeAfter = session.mesh().vertexPosition(edgeVertex.index);
        const Vec3 centreDisplacement = vec3Sub(centreAfter, centreVertex.basePosition);
        const Vec3 edgeDisplacement = vec3Sub(edgeAfter, edgeVertex.basePosition);

        r.check("clay_moved_the_centre", lengthOf(centreDisplacement) > 0.0f);
        r.check("clay_moves_the_centre_more_than_the_edge",
                lengthOf(centreDisplacement) > lengthOf(edgeDisplacement));
        // Deposition is ADDITIVE and OUTWARD along the normal, not toward it.
        r.check("clay_deposits_along_the_positive_normal",
                vec3Dot(centreDisplacement, centreVertex.baseNormal) > 0.0f);

        // The defining property: every clayed vertex's TOTAL displacement is
        // exactly parallel to the normal that vertex started with, however many
        // moves it took. This is what Inflate does NOT do.
        bool everyDisplacementIsParallel = true;
        bool everyDisplacementIsOutward = true;
        for (int i = 0; i < count; ++i) {
            const SculptStrokeVertex& v = session.stroke().affectedVertex(i);
            const Vec3 d = vec3Sub(session.mesh().vertexPosition(v.index), v.basePosition);
            if (offAxisDistance(d, v.baseNormal) > 1e-4f) everyDisplacementIsParallel = false;
            if (vec3Dot(d, v.baseNormal) <= 0.0f) everyDisplacementIsOutward = false;
        }
        r.check("every_clay_displacement_is_parallel_to_its_base_normal",
                everyDisplacementIsParallel);
        r.check("every_clay_displacement_is_outward", everyDisplacementIsOutward);

        // Sanity on the threshold above: at these sizes the accumulated float
        // rounding of adding a step to a coordinate of magnitude ~1 is a few
        // times 1e-7 per move, so 1e-4 is roughly two orders of magnitude of
        // headroom over the noise floor and far below any real axis error,
        // which would be a fraction of the displacement itself.
        // Repeated application builds outward: more travel, more material.
        const float builtOnce = lengthOf(centreDisplacement);
        driveTravel(session, kCentreX, kCentreY, 6, 50.0f);
        const float builtTwice =
            lengthOf(vec3Sub(session.mesh().vertexPosition(centreVertex.index),
                             centreVertex.basePosition));
        r.check("repeated_clay_builds_further_outward", builtTwice > builtOnce);
        r.check("clay_is_bounded_and_finite", allPositionsFinite(session.mesh()));

        // Path length, not event count: the same travel split into twice as many
        // moves deposits the same amount.
        SculptSession coarse;
        SculptSession fine;
        prepareSession(&coarse, object, SculptTool::Clay, 150.0f, 0.8f);
        prepareSession(&fine, object, SculptTool::Clay, 150.0f, 0.8f);
        coarse.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                           identity);
        fine.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                         identity);
        const uint32_t probe = coarse.stroke().affectedVertex(slotOfHighestWeight(coarse.stroke()))
                                   .index;
        driveTravel(coarse, kCentreX, kCentreY, 4, 60.0f);
        driveTravel(fine, kCentreX, kCentreY, 8, 30.0f);
        r.check("clay_depends_on_path_length_not_event_count",
                nearlyVec(coarse.mesh().vertexPosition(probe), fine.mesh().vertexPosition(probe)));

        // A stationary finger deposits nothing at all.
        const SculptRevision still = fine.mesh().revision();
        r.check("clay_deposits_nothing_without_travel",
                !fine.updateStroke(kCentreX, kCentreY) || fine.mesh().revision() == still);

        // Vertices outside the brush are bit-identical to the frozen source.
        // Checked while the stroke is still live, because ending it releases the
        // captured set this needs to know about.
        std::vector<bool> captured(session.mesh().vertexCount(), false);
        for (int i = 0; i < count; ++i) {
            captured[session.stroke().affectedVertex(i).index] = true;
        }
        bool outsideUntouched = true;
        for (uint32_t v = 0; v < session.mesh().vertexCount(); ++v) {
            if (captured[v]) continue;
            const Vec3 now = session.mesh().vertexPosition(v);
            const MeshVertex& was = source.vertices[v];
            if (now.x != was.position[0] || now.y != was.position[1] || now.z != was.position[2]) {
                outsideUntouched = false;
            }
        }
        r.check("clay_moved_nothing_outside_the_brush", outsideUntouched);

        session.endStroke();
        r.check("clay_left_the_construction_source_untouched",
                sameVertices(object.generateMesh().vertices, source.vertices));
        r.check("clay_changed_no_index", sameIndices(session.mesh().indices(), source.indices));
        r.check("clay_changed_no_vertex_count",
                session.mesh().vertexCount() == kSphereVertexCount);
    }

    // =======================================================================
    // Smooth
    // =======================================================================
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();
        SculptSession session;
        prepareSession(&session, object, SculptTool::Smooth, 150.0f, 0.9f);

        // Find the vertex under the brush centre, then build a real spike there
        // by hand — so what Smooth is asked to fix is a known deviation from a
        // known neighbour mean, not something a previous brush produced.
        session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);
        const uint32_t spike =
            session.stroke().affectedVertex(slotOfHighestWeight(session.stroke())).index;
        session.cancelStroke();

        // A modest spike on purpose. A violent one would move the silhouette
        // enough to change where the next ray hits, so the brush would be
        // capturing a different neighbourhood and the test would be measuring
        // the picker rather than the relaxation.
        const Vec3 restPosition = session.mesh().vertexPosition(spike);
        session.mesh().setVertexPosition(spike, vec3Scale(restPosition, 1.12f));
        session.mesh().advanceRevision();

        Vec3 meanBefore{0.0f, 0.0f, 0.0f};
        r.check("the_spike_vertex_has_a_neighbour_mean",
                neighborMean(session.mesh(), spike, &meanBefore));
        const Vec3 spikeBefore = session.mesh().vertexPosition(spike);
        const float deviationBefore = lengthOf(vec3Sub(spikeBefore, meanBefore));
        r.check("the_spike_really_deviates", deviationBefore > 0.02f);

        session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);
        const int count = session.stroke().affectedVertexCount();
        std::vector<bool> captured(session.mesh().vertexCount(), false);
        for (int i = 0; i < count; ++i) {
            captured[session.stroke().affectedVertex(i).index] = true;
        }
        r.check("the_smooth_brush_captured_the_spike", captured[spike]);

        // A vertex the brush did NOT capture, recorded to prove Smooth modifies
        // nothing outside its affected set even though it READS neighbours that
        // may lie outside it.
        uint32_t farVertex = session.mesh().vertexCount();
        for (uint32_t v = 0; v < session.mesh().vertexCount(); ++v) {
            if (!captured[v]) { farVertex = v; break; }
        }
        r.check("an_uncaptured_vertex_exists", farVertex < session.mesh().vertexCount());
        const Vec3 farBefore = session.mesh().vertexPosition(farVertex);

        driveTravel(session, kCentreX, kCentreY, 3, 50.0f);

        const Vec3 spikeAfter = session.mesh().vertexPosition(spike);
        Vec3 meanAfter{0.0f, 0.0f, 0.0f};
        neighborMean(session.mesh(), spike, &meanAfter);
        const float deviationAfter = lengthOf(vec3Sub(spikeAfter, meanAfter));

        r.check("smooth_reduces_the_deviation_from_the_neighbour_mean",
                deviationAfter < deviationBefore);
        // The direction is the defining one: toward the mean, never away.
        r.check("smooth_moves_the_spike_toward_its_neighbour_mean",
                vec3Dot(vec3Sub(spikeAfter, spikeBefore), vec3Sub(meanBefore, spikeBefore)) > 0.0f);
        // Bounded interpolation, never extrapolation: the vertex cannot overshoot
        // past the mean it is moving toward.
        r.check("smooth_never_overshoots_the_mean",
                vec3Dot(vec3Sub(meanBefore, spikeAfter), vec3Sub(meanBefore, spikeBefore)) > 0.0f);

        const float deviationOnce = deviationAfter;
        driveTravel(session, kCentreX, kCentreY, 3, 50.0f);
        Vec3 meanTwice{0.0f, 0.0f, 0.0f};
        neighborMean(session.mesh(), spike, &meanTwice);
        r.check("repeated_smoothing_reduces_it_further",
                lengthOf(vec3Sub(session.mesh().vertexPosition(spike), meanTwice)) < deviationOnce);
        r.check("smooth_is_bounded_and_finite", allPositionsFinite(session.mesh()));

        const Vec3 farAfter = session.mesh().vertexPosition(farVertex);
        r.check("smooth_modified_nothing_outside_its_affected_set",
                farAfter.x == farBefore.x && farAfter.y == farBefore.y &&
                    farAfter.z == farBefore.z);

        // A finger that stops relaxes nothing: like Clay and Inflate, Smooth is
        // driven by pointer path, not by the passage of events.
        session.updateStroke(kCentreX + 77.0f, kCentreY);
        r.check("smooth_relaxes_nothing_without_travel",
                !session.updateStroke(kCentreX + 77.0f, kCentreY));

        session.endStroke();
        r.check("smooth_left_the_construction_source_untouched",
                sameVertices(object.generateMesh().vertices, source.vertices));
        r.check("smooth_changed_no_index", sameIndices(session.mesh().indices(), source.indices));
    }

    // =======================================================================
    // Inflate, and how it differs from Clay
    // =======================================================================
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();
        SculptSession session;
        prepareSession(&session, object, SculptTool::Inflate, 150.0f, 0.8f);
        session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);

        const int count = session.stroke().affectedVertexCount();
        const int centreSlot = slotOfHighestWeight(session.stroke());
        const int edgeSlot = slotOfLowestWeight(session.stroke());
        const SculptStrokeVertex centreVertex = session.stroke().affectedVertex(centreSlot);
        const SculptStrokeVertex edgeVertex = session.stroke().affectedVertex(edgeSlot);

        driveTravel(session, kCentreX, kCentreY, 6, 50.0f);

        const Vec3 centreDisplacement =
            vec3Sub(session.mesh().vertexPosition(centreVertex.index), centreVertex.basePosition);
        const Vec3 edgeDisplacement =
            vec3Sub(session.mesh().vertexPosition(edgeVertex.index), edgeVertex.basePosition);

        r.check("inflate_moved_the_centre", lengthOf(centreDisplacement) > 0.0f);
        r.check("inflate_moves_the_centre_more_than_the_edge",
                lengthOf(centreDisplacement) > lengthOf(edgeDisplacement));
        r.check("inflate_expands_outward_along_the_normal",
                vec3Dot(centreDisplacement, centreVertex.baseNormal) > 0.0f);

        bool everyVertexExpanded = true;
        for (int i = 0; i < count; ++i) {
            const SculptStrokeVertex& v = session.stroke().affectedVertex(i);
            const Vec3 d = vec3Sub(session.mesh().vertexPosition(v.index), v.basePosition);
            if (vec3Dot(d, v.baseNormal) <= 0.0f) everyVertexExpanded = false;
        }
        r.check("every_inflated_vertex_moved_outward", everyVertexExpanded);
        r.check("inflate_is_finite_after_repeated_application",
                allPositionsFinite(session.mesh()));

        driveTravel(session, kCentreX, kCentreY, 20, 90.0f);
        r.check("inflate_stays_finite_under_heavy_repetition",
                allPositionsFinite(session.mesh()));
        r.check("inflate_changed_no_index", sameIndices(session.mesh().indices(), source.indices));
        session.endStroke();
        r.check("inflate_left_the_construction_source_untouched",
                sameVertices(object.generateMesh().vertices, source.vertices));
    }

    // -----------------------------------------------------------------------
    // Clay is not an alias of Inflate
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        SculptSession clay;
        SculptSession inflate;
        prepareSession(&clay, object, SculptTool::Clay, 150.0f, 1.0f);
        prepareSession(&inflate, object, SculptTool::Inflate, 150.0f, 1.0f);

        clay.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                         identity);
        inflate.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                            identity);

        // The two strokes captured the same geometry, so anything that differs
        // afterwards is the formula, not the setup.
        r.check("clay_and_inflate_capture_the_same_set",
                clay.stroke().affectedVertexCount() == inflate.stroke().affectedVertexCount());

        const int count = clay.stroke().affectedVertexCount();
        std::vector<SculptStrokeVertex> captured;
        for (int i = 0; i < count; ++i) {
            captured.push_back(clay.stroke().affectedVertex(i));
        }

        driveTravel(clay, kCentreX, kCentreY, 8, 60.0f);
        driveTravel(inflate, kCentreX, kCentreY, 8, 60.0f);

        // The first move is identical by construction — the surface has not yet
        // changed, so the live normals ARE the base normals. The difference is
        // only what the following moves do, which is exactly the point.
        float maxSeparation = 0.0f;
        float clayMaxOffAxis = 0.0f;
        float inflateMaxOffAxis = 0.0f;
        for (int i = 0; i < count; ++i) {
            const uint32_t index = captured[i].index;
            const Vec3 c = clay.mesh().vertexPosition(index);
            const Vec3 f = inflate.mesh().vertexPosition(index);
            maxSeparation = std::max(maxSeparation, lengthOf(vec3Sub(c, f)));
            clayMaxOffAxis = std::max(
                clayMaxOffAxis,
                offAxisDistance(vec3Sub(c, captured[i].basePosition), captured[i].baseNormal));
            inflateMaxOffAxis = std::max(
                inflateMaxOffAxis,
                offAxisDistance(vec3Sub(f, captured[i].basePosition), captured[i].baseNormal));
        }

        r.check("clay_and_inflate_produce_different_geometry", maxSeparation > 1e-3f);
        // The formula difference stated as a measurement: Clay follows the
        // normals the surface HAD, Inflate follows the normals it HAS. The two
        // thresholds are three orders of magnitude apart, so this is not a
        // borderline distinction being read as a real one.
        r.check("clay_never_leaves_its_base_normal_axis", clayMaxOffAxis < 1e-4f);
        r.check("inflate_leaves_its_base_normal_axis", inflateMaxOffAxis > 1e-2f);
        r.check("both_stay_finite", allPositionsFinite(clay.mesh()) &&
                                       allPositionsFinite(inflate.mesh()));

        clay.endStroke();
        inflate.endStroke();
    }

    // -----------------------------------------------------------------------
    // The shared Radius and Strength contract, across the non-Grab tools
    // -----------------------------------------------------------------------
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const SculptTool tools[3] = {SculptTool::Clay, SculptTool::Smooth, SculptTool::Inflate};
        bool radiusWidensEveryTool = true;
        bool strengthScalesEveryTool = true;

        for (int t = 0; t < 3; ++t) {
            SculptSession narrow;
            SculptSession wide;
            prepareSession(&narrow, object, tools[t], 60.0f, 0.8f);
            prepareSession(&wide, object, tools[t], 400.0f, 0.8f);
            narrow.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                               identity, identity);
            wide.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                             identity);
            if (!(wide.stroke().affectedVertexCount() > narrow.stroke().affectedVertexCount()) ||
                !(wide.stroke().worldRadius() > narrow.stroke().worldRadius())) {
                radiusWidensEveryTool = false;
            }

            // Strength: same brush, same path, more strength, more movement.
            // Smooth is included, where "more" means further toward the mean.
            SculptSession weak;
            SculptSession strong;
            prepareSession(&weak, object, tools[t], 150.0f, 0.1f);
            prepareSession(&strong, object, tools[t], 150.0f, 1.0f);
            if (tools[t] == SculptTool::Smooth) {
                // Smooth needs something to smooth.
                weak.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                 identity, identity);
                const uint32_t spike =
                    weak.stroke().affectedVertex(slotOfHighestWeight(weak.stroke())).index;
                weak.cancelStroke();
                weak.mesh().setVertexPosition(spike,
                                              vec3Scale(weak.mesh().vertexPosition(spike), 1.6f));
                strong.mesh().setVertexPosition(
                    spike, vec3Scale(strong.mesh().vertexPosition(spike), 1.6f));
            }
            weak.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight, identity,
                             identity);
            strong.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                               identity, identity);
            const uint32_t probe =
                weak.stroke().affectedVertex(slotOfHighestWeight(weak.stroke())).index;
            const Vec3 base = weak.mesh().vertexPosition(probe);
            driveTravel(weak, kCentreX, kCentreY, 4, 50.0f);
            driveTravel(strong, kCentreX, kCentreY, 4, 50.0f);
            const float weakMoved = lengthOf(vec3Sub(weak.mesh().vertexPosition(probe), base));
            const float strongMoved = lengthOf(vec3Sub(strong.mesh().vertexPosition(probe), base));
            if (!(strongMoved > weakMoved)) {
                strengthScalesEveryTool = false;
            }
        }

        r.check("radius_widens_the_affected_set_for_every_non_grab_tool", radiusWidensEveryTool);
        r.check("strength_scales_the_deformation_for_every_non_grab_tool",
                strengthScalesEveryTool);
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-11 — sculpt works in BOTH projections.
    //
    // Part of the CAMPROJ series that begins in the camera suite; this member
    // lives here because the code it exercises is the stroke kernel's
    // pixel-to-world resolution.
    //
    // Three things have to hold in Orthographic, and only the first is obvious:
    // the ray must hit the right surface, the brush radius must be right, and
    // the radius must be INDEPENDENT OF DEPTH — because a parallel view does not
    // open with distance, so there is nothing for a depth-scaled brush to be
    // proportional to.
    // -----------------------------------------------------------------------
    {
        CameraController orthoCam;
        orthoCam.setViewport(kViewportWidth, kViewportHeight);
        orthoCam.setProjectionMode(ProjectionMode::Orthographic);
        const CameraSnapshot ortho = orthoCam.snapshot();

        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();

        // A stroke must start in Orthographic at all, on the same pixel and the
        // same object that works in Perspective.
        SculptMesh perspectiveMesh;
        SculptMesh orthoMesh;
        perspectiveMesh.freezeFrom(source, object.objectId());
        orthoMesh.freezeFrom(source, object.objectId());

        SculptStroke perspectiveStroke;
        SculptStroke orthoStroke;
        const bool pBegan =
            perspectiveStroke.begin(SculptTool::Grab, perspectiveMesh, camera, kCentreX, kCentreY,
                                    kViewportWidth, kViewportHeight, identity, identity, 120.0f);
        const bool oBegan =
            orthoStroke.begin(SculptTool::Grab, orthoMesh, ortho, kCentreX, kCentreY,
                              kViewportWidth, kViewportHeight, identity, identity, 120.0f);

        r.check("camproj11_stroke_begins_in_both_projections", pBegan && oBegan);
        r.check("camproj11_ortho_captures_vertices",
                oBegan && orthoStroke.affectedVertexCount() > 0);

        // The hit is on the real surface: a 2.0 m diameter sphere means the hit
        // point is 1.0 m from its centre. Faceting pulls a hit slightly inside
        // the exact radius, which is why this is a band rather than an equality.
        const float orthoHitRadius = lengthOf(orthoStroke.localCenter());
        r.check("camproj11_ortho_hit_is_on_the_sphere_surface",
                oBegan && orthoHitRadius > 0.97f && orthoHitRadius <= 1.0f + kEpsilon);
        r.check("camproj11_both_projections_hit_the_same_point",
                pBegan && oBegan &&
                    lengthOf(vec3Sub(orthoStroke.localCenter(),
                                     perspectiveStroke.localCenter())) < 1e-3f);

        // The orthographic pixel-to-world scale is the ortho span over the
        // viewport height, with no depth term anywhere in it.
        const float expectedScale = (2.0f * ortho.orthoHalfHeightMeters) /
                                    static_cast<float>(kViewportHeight);
        r.check("camproj11_ortho_world_per_pixel_is_the_span_over_the_viewport",
                oBegan && std::fabs(orthoStroke.worldPerPixel() - expectedScale) < 1e-6f);
        r.check("camproj11_ortho_radius_follows_that_scale",
                oBegan && std::fabs(orthoStroke.worldRadius() - 120.0f * expectedScale) < 1e-4f);
        r.check("camproj11_ortho_radius_finite_and_positive",
                oBegan && std::isfinite(orthoStroke.worldRadius()) &&
                    orthoStroke.worldRadius() > 0.0f &&
                    std::isfinite(orthoStroke.worldPerPixel()));

        // Depth independence, measured rather than argued: move the object far
        // away along the view axis and the brush must cover the SAME amount of
        // surface. The perspective brush, on the same move, must not.
        {
            const Vec3 dir{ortho.view.m[2], ortho.view.m[6], ortho.view.m[10]};  // backward
            // +/- 3 m along the view axis. Far enough that the perspective
            // brush must visibly differ, close enough that the near case still
            // frames the sphere and captures vertices in both projections.
            const Mat4 pushedIn = mat4Translation(vec3Scale(dir, -3.0f));   // farther away
            const Mat4 pulledOut = mat4Translation(vec3Scale(dir, 3.0f));   // nearer
            const Mat4 inInverse = mat4Translation(vec3Scale(dir, 3.0f));
            const Mat4 outInverse = mat4Translation(vec3Scale(dir, -3.0f));

            SculptMesh farMesh, nearMesh;
            farMesh.freezeFrom(source, object.objectId());
            nearMesh.freezeFrom(source, object.objectId());
            SculptStroke farStroke, nearStroke;
            const bool farOk =
                farStroke.begin(SculptTool::Grab, farMesh, ortho, kCentreX, kCentreY,
                                kViewportWidth, kViewportHeight, pushedIn, inInverse, 120.0f);
            const bool nearOk =
                nearStroke.begin(SculptTool::Grab, nearMesh, ortho, kCentreX, kCentreY,
                                 kViewportWidth, kViewportHeight, pulledOut, outInverse, 120.0f);
            r.check("camproj11_ortho_brush_radius_is_depth_independent",
                    farOk && nearOk &&
                        std::fabs(farStroke.worldRadius() - nearStroke.worldRadius()) < 1e-5f);

            SculptMesh pFar, pNear;
            pFar.freezeFrom(source, object.objectId());
            pNear.freezeFrom(source, object.objectId());
            SculptStroke pFarStroke, pNearStroke;
            const bool pFarOk =
                pFarStroke.begin(SculptTool::Grab, pFar, camera, kCentreX, kCentreY,
                                 kViewportWidth, kViewportHeight, pushedIn, inInverse, 120.0f);
            const bool pNearOk =
                pNearStroke.begin(SculptTool::Grab, pNear, camera, kCentreX, kCentreY,
                                  kViewportWidth, kViewportHeight, pulledOut, outInverse, 120.0f);
            // The counterpart check: without it, a brush that had simply frozen
            // to a constant would pass the depth-independence check above.
            r.check("camproj11_perspective_brush_radius_does_depend_on_depth",
                    pFarOk && pNearOk &&
                        pFarStroke.worldRadius() > pNearStroke.worldRadius() + 1e-3f);
        }

        // A real Grab in Orthographic deforms the mesh, keeps the topology
        // fixed, produces no NaN, and leaves the Construction Source untouched.
        {
            const uint32_t vertexCountBefore = orthoMesh.vertexCount();
            const SculptRevision revisionBefore = orthoMesh.revision();
            const uint32_t probe =
                orthoStroke.affectedVertex(slotOfHighestWeight(orthoStroke)).index;
            const Vec3 base = orthoMesh.vertexPosition(probe);

            bool everyMoveApplied = true;
            for (int i = 1; i <= 6; ++i) {
                if (!orthoStroke.update(orthoMesh, kCentreX + static_cast<float>(i) * 12.0f,
                                        kCentreY, 1.0f)) {
                    everyMoveApplied = false;
                }
                orthoMesh.advanceRevision();
            }
            orthoStroke.end();

            const Vec3 moved = vec3Sub(orthoMesh.vertexPosition(probe), base);
            r.check("camproj11_ortho_grab_applies_every_move", everyMoveApplied);
            r.check("camproj11_ortho_grab_actually_moved_a_vertex", lengthOf(moved) > 1e-4f);

            // The stroke's own displacement is the unweighted camera-plane
            // vector for the total travel from the anchor; what an individual
            // vertex receives is that vector scaled by its falloff weight, so
            // the two are compared separately.
            const Vec3 expected =
                expectedWorldDelta(ortho, 72.0f, 0.0f, orthoStroke.worldPerPixel());
            r.check("camproj11_ortho_grab_direction_matches_the_camera_plane",
                    lengthOf(vec3Sub(orthoStroke.lastLocalDisplacement(), expected)) < 1e-4f);
            // The vertex moved along that same axis, at no more than the full
            // displacement — which is what a falloff weight in [0, 1] means.
            r.check("camproj11_ortho_grab_vertex_follows_that_axis_within_its_weight",
                    lengthOf(moved) <= lengthOf(expected) + 1e-4f &&
                        vec3Dot(moved, expected) > 0.0f &&
                        lengthOf(vec3Sub(vec3Scale(vec3Normalize(moved), lengthOf(expected)),
                                         expected)) < 1e-3f);
            r.check("camproj11_ortho_grab_keeps_topology_fixed",
                    orthoMesh.vertexCount() == vertexCountBefore &&
                        sameIndices(orthoMesh.indices(), source.indices));
            r.check("camproj11_ortho_grab_produces_no_nan", allPositionsFinite(orthoMesh));
            r.check("camproj11_ortho_grab_minted_revisions",
                    orthoMesh.revision() > revisionBefore);
            // The Construction Source is never written by sculpting, in either
            // projection. Changing the camera cannot change that.
            r.check("camproj11_ortho_grab_leaves_construction_source_untouched",
                    sameVertices(object.generateMesh().vertices, source.vertices));
        }

        // A pixel clear of the object starts NO stroke in Orthographic, the same
        // rule Perspective follows.
        {
            SculptMesh missMesh;
            missMesh.freezeFrom(source, object.objectId());
            SculptStroke missStroke;
            r.check("camproj11_ortho_miss_starts_no_stroke",
                    !missStroke.begin(SculptTool::Grab, missMesh, ortho, 6.0f, 6.0f,
                                      kViewportWidth, kViewportHeight, identity, identity,
                                      40.0f) &&
                        !missStroke.active());
        }
    }

    // -----------------------------------------------------------------------
    // SIDE-01..09 -- sidedness belongs to the ACTIVE PUBLISHED REPRESENTATION
    // -----------------------------------------------------------------------
    //
    // One fact, three consumers. Render, selection picking and the Sculpt
    // hit-test must all get the same answer for the mesh that is active right
    // now, and none of them may re-derive it from the Construction Source's
    // current PrimitiveKind -- a Frozen Sculpt Mesh outlives the Source it was
    // frozen from, so the Source can be a Plane while the frozen geometry is a
    // solid, and vice versa.
    runSidednessSelfTests(r);

    // -----------------------------------------------------------------------
    // Stylus pressure is CARRIED, never CONSUMED
    // -----------------------------------------------------------------------
    //
    // The teeth behind "the pointer boundary adds infrastructure, not
    // behaviour". Two strokes trace the SAME pixels with the same tool, radius
    // and strength; one is driven by pointers reporting the lightest pressure
    // the contract allows, the other by pointers reporting the heaviest, held
    // almost flat. If any brush arithmetic had started reading pressure or tilt,
    // the two meshes would differ.
    //
    // Bit-exact, not approximate: "pressure changed nothing" is a statement
    // about identical data, and a tolerance would hide a small modulation --
    // which is precisely the bug this check exists to catch. Run for all four
    // tools, because one kernel does not mean one code path through it.
    {
        ConstructionObject object = makeSphereObject();
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here.
        ConstructionTransform placement_;
        const ConstructionMesh source = object.generateMesh();

        // The geometric path, authored once so the two runs cannot drift apart.
        const float pathX[] = {kCentreX + 20.0f, kCentreX + 60.0f, kCentreX + 110.0f,
                               kCentreX + 140.0f};
        const float pathY[] = {kCentreY - 10.0f, kCentreY - 25.0f, kCentreY + 15.0f,
                               kCentreY + 60.0f};
        constexpr int kSampleCount = 4;

        // Two pressure/tilt profiles over that one path. The pointers are built
        // explicitly so this reads as what it is -- a stylus stroke -- even
        // though the brush is handed only the coordinates, which is the whole
        // point.
        TouchPointer light[kSampleCount];
        TouchPointer heavy[kSampleCount];
        for (int i = 0; i < kSampleCount; ++i) {
            light[i] = TouchPointer{1, pathX[i], pathY[i], PointerToolType::Stylus,
                                    kPointerPressureMin, kPointerTiltNoneRadians, 0.0f};
            heavy[i] = TouchPointer{1, pathX[i], pathY[i], PointerToolType::Stylus,
                                    kPointerPressureMax, kPointerTiltMaxRadians, 2.5f};
        }
        r.check("pressure_profiles_actually_differ",
                light[0].pressure != heavy[0].pressure &&
                    light[2].tiltRadians != heavy[2].tiltRadians);
        r.check("pressure_profiles_trace_the_same_pixels", [&] {
            for (int i = 0; i < kSampleCount; ++i) {
                if (light[i].x != heavy[i].x || light[i].y != heavy[i].y) {
                    return false;
                }
            }
            return true;
        }());

        // One name per tool per assertion, so a failure says which tool broke.
        static const char* const kVertexNames[kSculptToolCount] = {
            "grab_result_is_pressure_independent", "clay_result_is_pressure_independent",
            "smooth_result_is_pressure_independent", "inflate_result_is_pressure_independent"};
        static const char* const kRevisionNames[kSculptToolCount] = {
            "grab_revision_is_pressure_independent", "clay_revision_is_pressure_independent",
            "smooth_revision_is_pressure_independent", "inflate_revision_is_pressure_independent"};
        static const char* const kAffectedNames[kSculptToolCount] = {
            "grab_affected_set_is_pressure_independent",
            "clay_affected_set_is_pressure_independent",
            "smooth_affected_set_is_pressure_independent",
            "inflate_affected_set_is_pressure_independent"};

        for (int toolIndex = 0; toolIndex < kSculptToolCount; ++toolIndex) {
            SculptTool tool = SculptTool::Grab;
            sculptToolFromIndex(toolIndex, &tool);

            SculptSession lightRun;
            lightRun.freezeToSculpt(source, object.objectId());
            lightRun.setTool(tool);
            lightRun.setRadiusPixels(180.0f);
            lightRun.setStrength(0.8f);
            lightRun.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                 identity, identity);

            SculptSession heavyRun;
            heavyRun.freezeToSculpt(source, object.objectId());
            heavyRun.setTool(tool);
            heavyRun.setRadiusPixels(180.0f);
            heavyRun.setStrength(0.8f);
            heavyRun.beginStroke(camera, kCentreX, kCentreY, kViewportWidth, kViewportHeight,
                                 identity, identity);

            for (int i = 0; i < kSampleCount; ++i) {
                lightRun.updateStroke(light[i].x, light[i].y);
                heavyRun.updateStroke(heavy[i].x, heavy[i].y);
            }
            // Read the affected set while the strokes are still live.
            const int lightAffected = lightRun.stroke().affectedVertexCount();
            const int heavyAffected = heavyRun.stroke().affectedVertexCount();
            lightRun.endStroke();
            heavyRun.endStroke();

            r.check(kVertexNames[toolIndex],
                    sameVertices(lightRun.mesh().vertices(), heavyRun.mesh().vertices()));
            r.check(kRevisionNames[toolIndex],
                    lightRun.mesh().revision() == heavyRun.mesh().revision() &&
                        lightRun.mesh().revision() > kFrozenSculptRevision);
            r.check(kAffectedNames[toolIndex],
                    lightAffected == heavyAffected && lightAffected > 0);
        }
    }

    // -----------------------------------------------------------------------
    // S020R3-01..12 — the brush measures in the WORLD/display metric
    // -----------------------------------------------------------------------
    //
    // A Construction Scale is a multiplier on a derived matrix, so the Frozen
    // Sculpt Mesh stays exactly the local geometry it was frozen from. That is
    // the ownership rule, and it is what made the brush wrong: a brush that
    // compared LOCAL offsets against a radius resolved in WORLD meters is round
    // only while S = (1,1,1). On a body scaled (3,1,1) it selected a footprint
    // three times narrower along the stretched axis — an oval on screen.
    //
    // The fix is one shared metric, brushWorldDistance(), and one shared
    // normal conversion, brushLocalStepAlongNormal(). Nothing is baked into a
    // vertex and no transform is touched, which is what the lifecycle checks at
    // the end of this section prove numerically rather than by argument.
    {
        const Mat4 identity = mat4Identity();
        const CameraSnapshot camera = defaultCamera();

        // A matrix for an arbitrary placement, built the one way the product
        // builds one: through the authoritative nine values.
        struct Placement {
            Mat4 model;
            Mat4 inverseModel;
        };
        auto placementOf = [](double sx, double sy, double sz, double rx, double ry,
                              double rz) -> Placement {
            ConstructionTransform t;
            TransformValues v{};
            v.rotationX = rx;
            v.rotationY = ry;
            v.rotationZ = rz;
            v.scaleX = sx;
            v.scaleY = sy;
            v.scaleZ = sz;
            t.setValues(v);
            return Placement{t.modelMatrix(), t.inverseModelMatrix()};
        };

        // --- S020R3-01: the metric helper itself ---------------------------
        {
            const Vec3 dx{1.0f, 0.0f, 0.0f};
            const Vec3 dy{0.0f, 1.0f, 0.0f};
            const Vec3 dz{0.0f, 0.0f, 1.0f};
            const Vec3 mixed{0.3f, -0.4f, 0.5f};

            r.check("s020r3_01_identity_metric_is_the_local_length",
                    nearly(brushWorldDistance(identity, mixed), lengthOf(mixed)));

            const Placement uniform = placementOf(2.0, 2.0, 2.0, 0.0, 0.0, 0.0);
            r.check("s020r3_01_uniform_scale_multiplies_the_length",
                    nearly(brushWorldDistance(uniform.model, mixed), 2.0f * lengthOf(mixed)));

            // (3,1,1): three world meters along the body's own X, one along the
            // other two. This is the exact case the stage names.
            const Placement stretched = placementOf(3.0, 1.0, 1.0, 0.0, 0.0, 0.0);
            r.check("s020r3_01_stretched_axis_measures_three",
                    nearly(brushWorldDistance(stretched.model, dx), 3.0f));
            r.check("s020r3_01_unstretched_axes_measure_one",
                    nearly(brushWorldDistance(stretched.model, dy), 1.0f) &&
                        nearly(brushWorldDistance(stretched.model, dz), 1.0f));

            // |S * d| for a mixed offset, computed independently of the helper.
            const Placement odd = placementOf(0.5, 2.5, 1.5, 0.0, 0.0, 0.0);
            const Vec3 scaled{mixed.x * 0.5f, mixed.y * 2.5f, mixed.z * 1.5f};
            r.check("s020r3_01_nonuniform_metric_is_the_scaled_length",
                    nearly(brushWorldDistance(odd.model, mixed), lengthOf(scaled)));

            // A translation is not a distance: the offset is a DIRECTION.
            const Mat4 moved = mat4Translation(Vec3{7.0f, -3.0f, 11.0f});
            r.check("s020r3_01_translation_does_not_change_the_metric",
                    nearly(brushWorldDistance(moved, mixed), lengthOf(mixed)));

            // The old, wrong answer, stated so the test cannot pass by
            // accident: on (3,1,1) the local length and the displayed one
            // genuinely differ, and by the factor the scale names.
            r.check("s020r3_01_local_length_is_not_the_displayed_one",
                    std::fabs(brushWorldDistance(stretched.model, dx) - lengthOf(dx)) > 1.0f);
        }

        // --- S020R3-02: the influence set is ROUND in world space ----------
        //
        // Proved twice. First without any vertex sampling at all: the rim of
        // the brush is the set of local offsets whose world distance is R, so
        // every world DIRECTION must reach that rim at the same world distance.
        // Then on the real affected set of a real stroke.
        {
            const double kScales[2][3] = {{3.0, 1.0, 1.0}, {0.5, 2.5, 1.5}};
            static const char* const kRimNames[2] = {
                "s020r3_02_rim_is_a_world_sphere_at_3_1_1",
                "s020r3_02_rim_is_a_world_sphere_at_0.5_2.5_1.5"};
            static const char* const kSetNames[2] = {
                "s020r3_02_affected_set_is_the_world_ball_at_3_1_1",
                "s020r3_02_affected_set_is_the_world_ball_at_0.5_2.5_1.5"};
            static const char* const kWeightNames[2] = {
                "s020r3_02_weights_follow_the_world_distance_at_3_1_1",
                "s020r3_02_weights_follow_the_world_distance_at_0.5_2.5_1.5"};
            static const char* const kOvalNames[2] = {
                "s020r3_02_local_metric_would_have_differed_at_3_1_1",
                "s020r3_02_local_metric_would_have_differed_at_0.5_2.5_1.5"};

            for (int c = 0; c < 2; ++c) {
                const Placement p =
                    placementOf(kScales[c][0], kScales[c][1], kScales[c][2], 0.0, 0.0, 0.0);
                const float kRadius = 0.7f;

                // 128 directions over the sphere. Each is carried into local
                // space by the inverse model and scaled to sit exactly on the
                // rim; the metric must report R for every one of them, which is
                // the definition of a round footprint on screen.
                bool rimIsRound = true;
                for (int i = 0; i < 128; ++i) {
                    const float u = (static_cast<float>(i) + 0.5f) / 128.0f;
                    const float theta = u * 6.2831853f * 7.0f;  // co-prime turns
                    const float z = 2.0f * u - 1.0f;
                    const float rho = std::sqrt(std::max(0.0f, 1.0f - z * z));
                    const Vec3 worldDir{rho * std::cos(theta), rho * std::sin(theta), z};
                    const Vec3 localDir = mat4TransformDirection(p.inverseModel, worldDir);
                    const float unit = brushWorldDistance(p.model, localDir);
                    if (!std::isfinite(unit) || unit <= 0.0f) {
                        rimIsRound = false;
                        break;
                    }
                    const Vec3 rim = vec3Scale(localDir, kRadius / unit);
                    if (std::fabs(brushWorldDistance(p.model, rim) - kRadius) > 1e-4f) {
                        rimIsRound = false;
                        break;
                    }
                }
                r.check(kRimNames[c], rimIsRound);

                // And on a real stroke: the affected set is exactly the set of
                // vertices inside the world ball, and each weight is the
                // falloff of its WORLD distance.
                ConstructionObject object = makeSphereObject();
                // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
                // Source. This suite drives a standalone source, so it pairs one here.
                ConstructionTransform placement_;
                SculptMesh mesh;
                mesh.freezeFrom(object.generateMesh(), object.objectId());
                SculptStroke stroke;
                const bool began =
                    stroke.begin(SculptTool::Clay, mesh, camera, kCentreX, kCentreY,
                                 kViewportWidth, kViewportHeight, p.model, p.inverseModel, 120.0f);

                bool setMatches = began;
                bool weightsMatch = began;
                bool ovalWouldDiffer = false;
                if (began) {
                    const float radius = stroke.worldRadius();
                    for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
                        const Vec3 d = vec3Sub(mesh.vertexPosition(i), stroke.localCenter());
                        const float world = brushWorldDistance(p.model, d);
                        const float local = lengthOf(d);
                        const bool inWorldBall = world < radius;
                        const bool inLocalBall = local < radius;
                        if (inWorldBall != (stroke.weightOfVertex(i) > 0.0f)) {
                            setMatches = false;
                        }
                        if (inWorldBall &&
                            !nearly(stroke.weightOfVertex(i), sculptFalloff(world, radius))) {
                            weightsMatch = false;
                        }
                        if (inWorldBall != inLocalBall) {
                            ovalWouldDiffer = true;  // the old code chose differently
                        }
                    }
                }
                r.check(kSetNames[c], setMatches && stroke.affectedVertexCount() > 0);
                r.check(kWeightNames[c], weightsMatch);
                r.check(kOvalNames[c], ovalWouldDiffer);
            }
        }

        // --- S020R3-03: a rotation does not change the metric --------------
        {
            const Placement plain = placementOf(0.5, 2.5, 1.5, 0.0, 0.0, 0.0);
            const Placement turned = placementOf(0.5, 2.5, 1.5, 37.0, -21.0, 64.0);

            bool metricUnchanged = true;
            for (int i = 0; i < 64; ++i) {
                const float a = static_cast<float>(i) * 0.37f;
                const Vec3 d{std::cos(a) * 0.6f, std::sin(a * 1.7f) * 0.4f,
                             std::sin(a) * 0.5f};
                if (std::fabs(brushWorldDistance(plain.model, d) -
                              brushWorldDistance(turned.model, d)) > 1e-5f) {
                    metricUnchanged = false;
                }
            }
            r.check("s020r3_03_rotation_is_length_preserving_in_the_metric", metricUnchanged);

            // And a stroke on the turned, stretched body still selects the
            // world ball — the mixed case the stage names.
            ConstructionObject object = makeSphereObject();
            // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
            // Source. This suite drives a standalone source, so it pairs one here.
            ConstructionTransform placement_;
            SculptMesh mesh;
            mesh.freezeFrom(object.generateMesh(), object.objectId());
            SculptStroke stroke;
            const bool began = stroke.begin(SculptTool::Grab, mesh, camera, kCentreX, kCentreY,
                                            kViewportWidth, kViewportHeight, turned.model,
                                            turned.inverseModel, 120.0f);
            bool roundUnderRotation = began;
            if (began) {
                for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
                    const float world = brushWorldDistance(
                        turned.model, vec3Sub(mesh.vertexPosition(i), stroke.localCenter()));
                    if ((world < stroke.worldRadius()) != (stroke.weightOfVertex(i) > 0.0f)) {
                        roundUnderRotation = false;
                    }
                }
            }
            r.check("s020r3_03_mixed_rotation_and_scale_still_selects_the_world_ball",
                    roundUnderRotation && stroke.affectedVertexCount() > 0);
        }

        // --- S020R3-04: the screen-pixel radius contract still holds -------
        //
        // Orthographic, because there the pixel-to-world scale does not depend
        // on depth — which is what lets "the same nominal px at the same hit
        // depth" be stated exactly rather than approximately, even though
        // scaling a body necessarily moves its silhouette.
        {
            CameraController orthoCam;
            orthoCam.setViewport(kViewportWidth, kViewportHeight);
            orthoCam.setProjectionMode(ProjectionMode::Orthographic);
            const CameraSnapshot ortho = orthoCam.snapshot();

            ConstructionObject object = makeSphereObject();
            // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
            // Source. This suite drives a standalone source, so it pairs one here.
            ConstructionTransform placement_;
            const ConstructionMesh source = object.generateMesh();

            const Placement unscaled = placementOf(1.0, 1.0, 1.0, 0.0, 0.0, 0.0);
            const Placement stretched = placementOf(3.0, 1.0, 1.0, 0.0, 0.0, 0.0);

            SculptMesh a, b;
            a.freezeFrom(source, object.objectId());
            b.freezeFrom(source, object.objectId());
            SculptStroke plainStroke, scaledStroke;
            const bool aOk = plainStroke.begin(SculptTool::Clay, a, ortho, kCentreX, kCentreY,
                                               kViewportWidth, kViewportHeight, unscaled.model,
                                               unscaled.inverseModel, 120.0f);
            const bool bOk = scaledStroke.begin(SculptTool::Clay, b, ortho, kCentreX, kCentreY,
                                                kViewportWidth, kViewportHeight, stretched.model,
                                                stretched.inverseModel, 120.0f);

            r.check("s020r3_04_a_stroke_starts_on_a_scaled_body", aOk && bOk);
            r.check("s020r3_04_world_per_pixel_is_unchanged_by_body_scale",
                    aOk && bOk &&
                        std::fabs(plainStroke.worldPerPixel() - scaledStroke.worldPerPixel()) <
                            1e-6f);
            // The displayed radius of the brush is the same length in meters
            // before and after the body was scaled: Scale sizes the BODY, never
            // the instrument.
            r.check("s020r3_04_displayed_brush_radius_is_unchanged_by_body_scale",
                    aOk && bOk &&
                        std::fabs(plainStroke.worldRadius() - scaledStroke.worldRadius()) < 1e-5f);
            r.check("s020r3_04_radius_still_follows_the_authored_pixels",
                    bOk && std::fabs(scaledStroke.worldRadius() -
                                     120.0f * scaledStroke.worldPerPixel()) < 1e-4f);
        }

        // --- S020R3-05..08: every shipped brush uses the shared metric -----
        //
        // Parameterized, because the affected set and its weights are captured
        // by ONE shared path: four independent corrections is exactly what this
        // stage refused to write, so the test is written the same way.
        {
            static const char* const kSetNames[kSculptToolCount] = {
                "s020r3_05_grab_affected_set_is_scale_correct",
                "s020r3_06_clay_affected_set_is_scale_correct",
                "s020r3_07_smooth_affected_set_is_scale_correct",
                "s020r3_08_inflate_affected_set_is_scale_correct"};
            static const char* const kWeightNames[kSculptToolCount] = {
                "s020r3_05_grab_weights_follow_the_world_distance",
                "s020r3_06_clay_weights_follow_the_world_distance",
                "s020r3_07_smooth_weights_follow_the_world_distance",
                "s020r3_08_inflate_weights_follow_the_world_distance"};
            static const char* const kMoveNames[kSculptToolCount] = {
                "s020r3_05_grab_still_deforms_a_scaled_body",
                "s020r3_06_clay_still_deforms_a_scaled_body",
                "s020r3_07_smooth_still_deforms_a_scaled_body",
                "s020r3_08_inflate_still_deforms_a_scaled_body"};

            const Placement p = placementOf(3.0, 1.0, 1.0, 18.0, 42.0, -9.0);

            for (int toolIndex = 0; toolIndex < kSculptToolCount; ++toolIndex) {
                SculptTool tool = SculptTool::Grab;
                sculptToolFromIndex(toolIndex, &tool);

                ConstructionObject object = makeSphereObject();
                // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
                // Source. This suite drives a standalone source, so it pairs one here.
                ConstructionTransform placement_;
                SculptMesh mesh;
                mesh.freezeFrom(object.generateMesh(), object.objectId());
                SculptStroke stroke;
                const bool began =
                    stroke.begin(tool, mesh, camera, kCentreX, kCentreY, kViewportWidth,
                                 kViewportHeight, p.model, p.inverseModel, 160.0f);

                bool setMatches = began;
                bool weightsMatch = began;
                if (began) {
                    const float radius = stroke.worldRadius();
                    for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
                        const float world = brushWorldDistance(
                            p.model, vec3Sub(mesh.vertexPosition(i), stroke.localCenter()));
                        const bool inside = world < radius;
                        if (inside != (stroke.weightOfVertex(i) > 0.0f)) {
                            setMatches = false;
                        }
                        if (inside &&
                            !nearly(stroke.weightOfVertex(i), sculptFalloff(world, radius))) {
                            weightsMatch = false;
                        }
                    }
                }
                r.check(kSetNames[toolIndex], setMatches && stroke.affectedVertexCount() > 0);
                r.check(kWeightNames[toolIndex], weightsMatch);

                // The brush still does its job, and produces nothing that is
                // not a number, on a body it has never been scaled onto before.
                bool moved = false;
                for (int step = 1; step <= 8; ++step) {
                    if (stroke.update(mesh, kCentreX + static_cast<float>(step) * 9.0f,
                                      kCentreY + static_cast<float>(step) * 4.0f, 0.9f)) {
                        moved = true;
                    }
                }
                r.check(kMoveNames[toolIndex],
                        moved && allPositionsFinite(mesh) &&
                            mesh.vertexCount() == object.generateMesh().vertices.size());
                stroke.end();
            }
        }

        // --- S020R3-05/06/08: the DISPLACEMENT is a world length too -------
        //
        // Grab already carried its camera-plane delta through the inverse
        // model, so it was correct before this stage and must stay so; Clay and
        // Inflate deposit along a normal, and that is the conversion this stage
        // added. Both are one statement: what the vertex does IN THE WORLD is
        // what the brush asked for.
        {
            const Placement p = placementOf(3.0, 1.0, 1.0, 0.0, 0.0, 0.0);

            // Grab: the world displacement of the centre vertex is exactly the
            // camera-plane delta the pointer travelled, times its weight.
            {
                ConstructionObject object = makeSphereObject();
                // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
                // Source. This suite drives a standalone source, so it pairs one here.
                ConstructionTransform placement_;
                SculptMesh mesh;
                mesh.freezeFrom(object.generateMesh(), object.objectId());
                SculptStroke stroke;
                const bool began =
                    stroke.begin(SculptTool::Grab, mesh, camera, kCentreX, kCentreY,
                                 kViewportWidth, kViewportHeight, p.model, p.inverseModel, 120.0f);
                bool ok = began;
                if (began) {
                    const int slot = slotOfHighestWeight(stroke);
                    const SculptStrokeVertex& v = stroke.affectedVertex(slot);
                    const Vec3 before = mat4TransformPoint(p.model, v.basePosition);
                    const float kDx = 40.0f;
                    const float kDy = -25.0f;
                    ok = stroke.update(mesh, kCentreX + kDx, kCentreY + kDy, 1.0f);
                    const Vec3 after = mat4TransformPoint(p.model, mesh.vertexPosition(v.index));
                    const Vec3 expected = vec3Scale(
                        expectedWorldDelta(camera, kDx, kDy, stroke.worldPerPixel()), v.weight);
                    ok = ok && lengthOf(vec3Sub(vec3Sub(after, before), expected)) < 1e-4f;
                }
                r.check("s020r3_05_grab_world_displacement_is_the_pointer_delta", ok);
            }

            // Clay and Inflate: the world displacement of the centre vertex has
            // the world LENGTH the amount named, and points along the direction
            // the surface faces ON SCREEN — the inverse-transpose normal, not
            // the raw local one, which under (3,1,1) is a different direction.
            static const SculptTool kNormalTools[2] = {SculptTool::Clay, SculptTool::Inflate};
            static const char* const kLengthNames[2] = {
                "s020r3_06_clay_world_step_has_the_amount_as_its_length",
                "s020r3_08_inflate_world_step_has_the_amount_as_its_length"};
            static const char* const kDirNames[2] = {
                "s020r3_06_clay_steps_along_the_displayed_normal",
                "s020r3_08_inflate_steps_along_the_displayed_normal"};

            for (int i = 0; i < 2; ++i) {
                ConstructionObject object = makeSphereObject();
                // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
                // Source. This suite drives a standalone source, so it pairs one here.
                ConstructionTransform placement_;
                SculptMesh mesh;
                mesh.freezeFrom(object.generateMesh(), object.objectId());
                SculptStroke stroke;
                const bool began =
                    stroke.begin(kNormalTools[i], mesh, camera, kCentreX, kCentreY,
                                 kViewportWidth, kViewportHeight, p.model, p.inverseModel, 120.0f);
                bool lengthOk = began;
                bool directionOk = began;
                if (began) {
                    const int slot = slotOfHighestWeight(stroke);
                    const SculptStrokeVertex& v = stroke.affectedVertex(slot);
                    const Vec3 normalBefore = mesh.vertexNormals()[v.index];
                    const Vec3 before = mat4TransformPoint(p.model, mesh.vertexPosition(v.index));
                    const bool applied = stroke.update(mesh, kCentreX + 30.0f, kCentreY, 0.8f);
                    const Vec3 after = mat4TransformPoint(p.model, mesh.vertexPosition(v.index));
                    const Vec3 worldStep = vec3Sub(after, before);

                    const float expectedLength = stroke.lastAmount() * v.weight;
                    lengthOk = applied && expectedLength > 0.0f &&
                               std::fabs(lengthOf(worldStep) - expectedLength) < 1e-4f;

                    // The displayed normal: R * S^-1 * n, normalised.
                    const Vec3 raw = (kNormalTools[i] == SculptTool::Clay) ? v.baseNormal
                                                                          : normalBefore;
                    const Vec3 displayed = vec3Normalize(Vec3{
                        p.inverseModel.m[0] * raw.x + p.inverseModel.m[1] * raw.y +
                            p.inverseModel.m[2] * raw.z,
                        p.inverseModel.m[4] * raw.x + p.inverseModel.m[5] * raw.y +
                            p.inverseModel.m[6] * raw.z,
                        p.inverseModel.m[8] * raw.x + p.inverseModel.m[9] * raw.y +
                            p.inverseModel.m[10] * raw.z});
                    directionOk = applied && offAxisDistance(worldStep, displayed) < 1e-4f;
                    // ... and that this is a real statement: on (3,1,1) the raw
                    // local normal points somewhere else entirely.
                    directionOk = directionOk &&
                                  offAxisDistance(vec3Normalize(raw), displayed) > 1e-3f;
                }
                r.check(kLengthNames[i], lengthOk);
                r.check(kDirNames[i], directionOk);
            }
        }

        // --- S020R3-09: Start Sculpting bakes nothing ----------------------
        {
            ConstructionObject object = makeSphereObject();
            // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
            // Source. This suite drives a standalone source, so it pairs one here.
            ConstructionTransform placement_;
            TransformValues v{};
            v.positionX = 1.25;
            v.positionY = -0.5;
            v.positionZ = 3.0;
            v.rotationX = 18.0;
            v.rotationY = 42.0;
            v.rotationZ = -9.0;
            v.scaleX = 3.0;
            v.scaleY = 1.0;
            v.scaleZ = 1.0;
            placement_.setValues(v);
            const TransformValues before = placement_.values();
            const ConstructionMesh source = object.generateMesh();

            SculptSession session;
            const bool froze = session.freezeToSculpt(source, object.objectId());
            const TransformValues after = placement_.values();

            r.check("s020r3_09_freeze_succeeds_on_a_scaled_body", froze);
            // Bit-exact on all nine: Start Sculpting is not a transform edit.
            r.check("s020r3_09_placement_is_bit_identical_after_start_sculpting",
                    before.positionX == after.positionX && before.positionY == after.positionY &&
                        before.positionZ == after.positionZ &&
                        before.rotationX == after.rotationX &&
                        before.rotationY == after.rotationY &&
                        before.rotationZ == after.rotationZ && before.scaleX == after.scaleX &&
                        before.scaleY == after.scaleY && before.scaleZ == after.scaleZ);
            r.check("s020r3_09_scale_is_still_non_uniform",
                    after.scaleX == 3.0 && after.scaleY == 1.0 && after.scaleZ == 1.0);
            // The decisive one: the frozen vertices are the LOCAL ones, byte
            // for byte. A baked scale would have multiplied every x by three.
            r.check("s020r3_09_no_scale_is_baked_into_the_sculpt_mesh",
                    froze && sameVertices(session.mesh().vertices(), source.vertices));
            r.check("s020r3_09_construction_source_is_untouched",
                    sameVertices(object.generateMesh().vertices, source.vertices));
        }

        // --- S020R3-10: Back to Construction and Resume Sculpt retain ------
        {
            ConstructionObject object = makeSphereObject();
            // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
            // Source. This suite drives a standalone source, so it pairs one here.
            ConstructionTransform placement_;
            TransformValues v{};
            v.scaleX = 0.5;
            v.scaleY = 2.5;
            v.scaleZ = 1.5;
            placement_.setValues(v);
            const Placement p{placement_.modelMatrix(),
                              placement_.inverseModelMatrix()};

            SculptSession session;
            session.freezeToSculpt(object.generateMesh(), object.objectId());
            session.setTool(SculptTool::Clay);
            session.setRadiusPixels(150.0f);
            session.setStrength(0.9f);
            const bool began = session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth,
                                                   kViewportHeight, p.model, p.inverseModel);
            driveTravel(session, kCentreX, kCentreY, 6, 14.0f);
            session.endStroke();

            const SculptRevision revision = session.mesh().revision();
            const ObjectId id = session.mesh().objectId();
            const std::vector<MeshVertex> sculpted = session.mesh().vertices();
            const uint64_t freezes = session.freezeCount();

            session.enterConstruction();
            const bool inConstruction = session.mode() == ProductMode::Construction;
            const TransformValues mid = placement_.values();
            const bool resumed = session.enterSculpt();

            r.check("s020r3_10_a_stroke_ran_on_the_scaled_body",
                    began && revision > kFrozenSculptRevision);
            r.check("s020r3_10_back_to_construction_then_resume",
                    inConstruction && resumed && session.mode() == ProductMode::Sculpt);
            r.check("s020r3_10_resume_does_not_refreeze",
                    session.freezeCount() == freezes &&
                        session.mesh().revision() == revision);
            r.check("s020r3_10_sculpt_mesh_survives_bit_exactly",
                    sameVertices(session.mesh().vertices(), sculpted));
            r.check("s020r3_10_object_id_survives", session.mesh().objectId() == id);
            r.check("s020r3_10_non_uniform_scale_survives_the_round_trip",
                    mid.scaleX == 0.5 && mid.scaleY == 2.5 && mid.scaleZ == 1.5 &&
                        placement_.scaleXFactor() == 0.5 &&
                        placement_.scaleYFactor() == 2.5 &&
                        placement_.scaleZFactor() == 1.5);
        }

        // --- S020R3-11: none of this is a Construction history step --------
        //
        // A sculpt stroke is a sculpt mutation. It is not a user act in the
        // Construction sense, it opens no edit, and neither does entering or
        // leaving Sculpt — so both stacks must be exactly where they were.
        {
            ConstructionScene scene;
            ConstructionHistory history(scene);
            SceneObject& body = scene.activeBody();
            body.construction().setPrimitive(PrimitiveSpec::forSphere(2.0));
            TransformValues v{};
            v.scaleX = 3.0;
            v.scaleY = 1.0;
            v.scaleZ = 1.0;
            body.transform().setValues(v);

            // One real Construction act, so the stacks are not trivially empty
            // and a spurious extra step would be visible as a change.
            history.beginEdit();
            TransformValues moved = body.transform().values();
            moved.positionY = 0.75;
            body.transform().setValues(moved);
            history.commitEdit();

            const size_t undoBefore = history.undoDepth();
            const size_t redoBefore = history.redoDepth();

            SculptSession session;
            session.bindTarget(&body.frozenSculpt());
            session.freezeToSculpt(body.construction().generateMesh(), body.objectId());
            session.setTool(SculptTool::Inflate);
            const Mat4 model = body.transform().modelMatrix();
            const Mat4 inverseModel = body.transform().inverseModelMatrix();
            const bool began = session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth,
                                                   kViewportHeight, model, inverseModel);
            const int applied = driveTravel(session, kCentreX, kCentreY, 6, 16.0f);
            session.endStroke();
            session.enterConstruction();
            session.enterSculpt();

            r.check("s020r3_11_the_stroke_actually_ran", began && applied > 0 &&
                                                            session.mesh().revision() >
                                                                kFrozenSculptRevision);
            r.check("s020r3_11_no_edit_is_left_open", !history.editInProgress());
            r.check("s020r3_11_undo_depth_is_unchanged", history.undoDepth() == undoBefore);
            r.check("s020r3_11_redo_depth_is_unchanged", history.redoDepth() == redoBefore);
            r.check("s020r3_11_the_one_construction_act_is_still_there",
                    undoBefore == 1 && redoBefore == 0);
        }

        // --- S020R3-12: an unscaled body behaves exactly as it always did --
        //
        // The regression guard for the correction itself. On S = (1,1,1) the
        // world metric IS the local one, and the shared normal conversion
        // reduces to the captured normal times the amount — so the affected
        // set, the weights and the deposition direction must reproduce the
        // pre-correction rule exactly rather than merely closely. Every other
        // sculpt check in this suite runs unscaled and is the rest of the guard.
        {
            static const char* const kSetNames[kSculptToolCount] = {
                "s020r3_12_grab_unscaled_set_is_the_plain_local_ball",
                "s020r3_12_clay_unscaled_set_is_the_plain_local_ball",
                "s020r3_12_smooth_unscaled_set_is_the_plain_local_ball",
                "s020r3_12_inflate_unscaled_set_is_the_plain_local_ball"};
            static const char* const kRunNames[kSculptToolCount] = {
                "s020r3_12_grab_unscaled_stroke_still_deforms",
                "s020r3_12_clay_unscaled_stroke_still_deforms",
                "s020r3_12_smooth_unscaled_stroke_still_deforms",
                "s020r3_12_inflate_unscaled_stroke_still_deforms"};

            for (int toolIndex = 0; toolIndex < kSculptToolCount; ++toolIndex) {
                SculptTool tool = SculptTool::Grab;
                sculptToolFromIndex(toolIndex, &tool);

                ConstructionObject object = makeSphereObject();
                // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
                // Source. This suite drives a standalone source, so it pairs one here.
                ConstructionTransform placement_;
                SculptSession session;
                prepareSession(&session, object, tool, 150.0f, 0.8f);
                const bool began =
                    session.beginStroke(camera, kCentreX, kCentreY, kViewportWidth,
                                        kViewportHeight, identity, identity);

                bool plainRule = began;
                if (began) {
                    const SculptStroke& stroke = session.stroke();
                    const float radius = stroke.worldRadius();
                    plainRule = plainRule &&
                                std::fabs(radius - 150.0f * stroke.worldPerPixel()) < 1e-4f;
                    for (uint32_t i = 0; i < session.mesh().vertexCount(); ++i) {
                        const float local =
                            lengthOf(vec3Sub(session.mesh().vertexPosition(i),
                                             stroke.localCenter()));
                        const float expected = sculptFalloff(local, radius);
                        if (!nearly(stroke.weightOfVertex(i), expected)) {
                            plainRule = false;
                        }
                    }
                }
                r.check(kSetNames[toolIndex], plainRule);

                // Clay's total displacement stays on the axis of the normal the
                // vertex started with — the unscaled statement the pre-existing
                // Clay contract makes, restated through the shared conversion.
                Vec3 baseNormal{0.0f, 0.0f, 0.0f};
                Vec3 basePosition{0.0f, 0.0f, 0.0f};
                uint32_t probe = 0;
                if (began) {
                    const int slot = slotOfHighestWeight(session.stroke());
                    baseNormal = session.stroke().affectedVertex(slot).baseNormal;
                    basePosition = session.stroke().affectedVertex(slot).basePosition;
                    probe = session.stroke().affectedVertex(slot).index;
                }
                const int applied = driveTravel(session, kCentreX, kCentreY, 5, 18.0f);
                const Vec3 total = vec3Sub(session.mesh().vertexPosition(probe), basePosition);
                session.endStroke();

                const bool clayStaysOnAxis = tool != SculptTool::Clay ||
                                             offAxisDistance(total, baseNormal) < 1e-5f;
                r.check(kRunNames[toolIndex],
                        began && applied > 0 && clayStaysOnAxis &&
                            allPositionsFinite(session.mesh()) &&
                            session.mesh().revision() > kFrozenSculptRevision);
            }
        }
    }

    return r.n;
}

}  // namespace forgeshape
