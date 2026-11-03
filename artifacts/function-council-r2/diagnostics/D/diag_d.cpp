// FUNCTION-COUNCIL-R2 seat D diagnostic. Scratch only; never added to the build.
// Links against the host self-test objects of the audited tree (product .o files).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "forgeshape_freeform.h"
#include "forgeshape_freeform_session.h"
#include "forgeshape_history.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_surface.h"
#include "forgeshape_surface_authoring.h"
#include "forgeshape_sketch.h"

using namespace forgeshape;

static int g_fail = 0;
static void out(const char* id, bool ok, const char* what) {
    std::printf("%s %s %s\n", ok ? "OBS_OK  " : "OBS_DIFF", id, what);
    if (!ok) ++g_fail;
}
static const double kNaN = std::numeric_limits<double>::quiet_NaN();
static const double kInf = std::numeric_limits<double>::infinity();

// ---------------- Freeform ----------------
static void freeform() {
    const FreeformCage box = makeFreeformBox();
    std::printf("INFO box vertices=%zu edges=%zu faces=%zu level=%u valid=%s\n", box.vertices.size(),
                box.edges.size(), box.faces.size(), box.subdivisionLevel,
                freeformStatusName(validateFreeformCage(box)));
    FreeformCage o;
    const uint32_t face = idOf(box.faces[0].id);
    const uint32_t edge = idOf(box.edges[0].id);
    const double dists[] = {kNaN, kInf, 0.0, 1e-7, -0.25, 2e5};
    for (double d : dists) {
        FreeformStatus s = freeformPushPull(box, {face}, d, &o);
        std::printf("INFO pushPull(%g) -> %s\n", d, freeformStatusName(s));
        FreeformStatus e = freeformExtrudeFaces(box, {face}, d, &o);
        std::printf("INFO extrude(%g) -> %s\n", d, freeformStatusName(e));
    }
    const double ratios[] = {kNaN, 0.0, 1.0, -0.1, 1e-12, 0.5};
    for (double r : ratios) {
        std::printf("INFO insertLoop(%g) -> %s\n", r,
                    freeformStatusName(freeformInsertEdgeLoop(box, edge, r, &o)));
    }
    const double creases[] = {kNaN, -0.01, 1.01, 0.0, 1.0, 0.5};
    for (double c : creases) {
        std::printf("INFO crease(%g) -> %s\n", c, freeformStatusName(freeformSetCrease(box, {edge}, c, &o)));
    }
    const int levels[] = {-1, 5, 255, 0, 4};
    for (int l : levels) {
        std::printf("INFO level(%d) -> %s\n", l, freeformStatusName(freeformSetSubdivisionLevel(box, l, &o)));
    }
    std::vector<uint32_t> all;
    for (const FreeformFace& f : box.faces) all.push_back(idOf(f.id));
    out("FFD_delete_all_refused", freeformDeleteFaces(box, all, &o) == FreeformStatus::DeleteWouldEmpty,
        freeformStatusName(freeformDeleteFaces(box, all, &o)));
    FreeformAffine zero;
    zero.linear[0] = 0.0;
    out("FFD_zero_scale_refused",
        freeformTransform(box, FreeformElement::Face, {face}, zero, &o) == FreeformStatus::TransformDegenerate,
        freeformStatusName(freeformTransform(box, FreeformElement::Face, {face}, zero, &o)));
    FreeformAffine nanMove;
    nanMove.translation = DVec3{kNaN, 0, 0};
    std::printf("INFO transform(NaN translation) -> %s\n",
                freeformStatusName(freeformTransform(box, FreeformElement::Vertex, {1}, nanMove, &o)));

    // Symmetry X exactness under an awkward translation.
    FreeformCage sym;
    const FreeformStatus symS = freeformSetSymmetry(box, kFreeformSymmetryX, &sym);
    out("FFD_symmetry_x_on_box", symS == FreeformStatus::Ok, freeformStatusName(symS));
    const FreeformVertex& v1 = sym.vertices[0];
    FreeformAffine move;
    move.translation = DVec3{0.1234567891234, 0.0987654321, -0.333333333333};
    FreeformCage moved;
    const FreeformStatus mS = freeformTransform(sym, FreeformElement::Vertex, {idOf(v1.id)}, move, &moved);
    bool exact = mS == FreeformStatus::Ok;
    const FreeformVertex* a = findFreeformVertex(moved, v1.id);
    const FreeformVertex* partner = nullptr;
    for (const FreeformVertex& v : moved.vertices) {
        if (a != nullptr && v.id != a->id && v.position.x == -a->position.x && v.position.y == a->position.y
            && v.position.z == a->position.z)
            partner = &v;
    }
    out("FFD_symmetry_partner_bitwise_mirror", exact && partner != nullptr,
        partner ? "partner found bit-exact" : freeformStatusName(mS));
    out("FFD_symmetry_result_validates",
        validateFreeformCage(moved) == FreeformStatus::Ok, freeformStatusName(validateFreeformCage(moved)));

    // Insert a loop at 0.5 across an X-crossing ring under symmetry; move the on-plane vertex in x.
    // Find an edge parallel to X.
    uint32_t xEdge = 0;
    for (const FreeformEdge& e : sym.edges) {
        const FreeformVertex* p = findFreeformVertex(sym, e.v0);
        const FreeformVertex* q = findFreeformVertex(sym, e.v1);
        if (p && q && p->position.y == q->position.y && p->position.z == q->position.z) {
            xEdge = idOf(e.id);
            break;
        }
    }
    FreeformCage looped;
    const FreeformStatus lS = freeformInsertEdgeLoop(sym, xEdge, 0.5, &looped);
    FreeformCage looped3;
    const FreeformStatus l3 = freeformInsertEdgeLoop(sym, xEdge, 0.3, &looped3);
    std::printf("INFO symmetric insertLoop(0.5) -> %s ; (0.3) -> %s\n", freeformStatusName(lS), freeformStatusName(l3));
    if (lS == FreeformStatus::Ok) {
        uint32_t onPlane = 0;
        for (const FreeformVertex& v : looped.vertices)
            if (v.position.x == 0.0) { onPlane = idOf(v.id); break; }
        FreeformAffine push;
        push.translation = DVec3{0.2, 0.1, 0.0};
        FreeformCage pushed;
        const FreeformStatus pS = freeformTransform(looped, FreeformElement::Vertex, {onPlane}, push, &pushed);
        const FreeformVertex* pv = findFreeformVertex(pushed, FreeformVertexId{onPlane});
        char buf[160];
        std::snprintf(buf, sizeof buf, "status=%s x=%.17g y=%.17g", freeformStatusName(pS), pv ? pv->position.x : kNaN,
                      pv ? pv->position.y : kNaN);
        out("FFD_on_plane_vertex_stays_on_plane", pS == FreeformStatus::Ok && pv && pv->position.x == 0.0, buf);
    }

    // Scene + history: create a Freeform body, push one face through the session, Undo is exact.
    ConstructionScene scene;
    ConstructionHistory history{scene};
    ObjectId id = kNoObject;
    const FreeformStatus c = createFreeformBody(scene, history, 0, &id);
    out("FFD_create_body_one_step", c == FreeformStatus::Ok && history.undoDepth() == 1u, freeformStatusName(c));
    SceneObject* body = scene.findBody(id);
    const FreeformCage before = body->freeformOrNull()->cage();
    FreeformEditSession session(scene, history);
    out("FFD_session_begin", session.begin(id) == FreeformStatus::Ok, "");
    session.setElement(FreeformElement::Face);
    session.select({face});
    const FreeformStatus pp = session.pushPull(0.3);
    out("FFD_session_pushpull_one_step", pp == FreeformStatus::Ok && history.undoDepth() == 2u,
        freeformStatusName(pp));
    history.undo();
    out("FFD_undo_restores_exact_cage", sameFreeformCage(scene.findBody(id)->freeformOrNull()->cage(), before), "");
    // Undo removed nothing else; the session still names a live face.
    session.reconcile();
    out("FFD_session_survives_undo", session.active() && session.selection().size() == 1u, "");
    // Lock the body: the session must end and tools must refuse.
    scene.findBody(id)->setLocked(true);
    const FreeformStatus lockedPush = session.pushPull(0.1);
    out("FFD_locked_body_refuses_tool", lockedPush != FreeformStatus::Ok, freeformStatusName(lockedPush));
    // Sculpt state untouched: a Freeform body cannot be a sculpt source.
    ConstructionMesh src;
    out("FFD_not_a_sculpt_source", !buildSculptSourceMesh(*scene.findBody(id), &src), "");
}

// ---------------- Surface ----------------
static SketchEntityId addE(CadSketch* s, SketchEntity::Payload p) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(s, std::move(p), &id);
    return id;
}
static SketchLine L(double a, double b, double c, double d) {
    SketchLine l;
    l.start = SketchPoint{a, b};
    l.end = SketchPoint{c, d};
    return l;
}
static SketchRectangle R(double w, double h) {
    SketchRectangle r;
    r.center = SketchPoint{0, 0};
    r.width = w;
    r.height = h;
    return r;
}

static SurfaceBodyState extrudeState(double dist, SketchEntity::Payload curve) {
    SurfaceBodyState s;
    CadSketch sk;
    sk.plane = Workplane::XY;
    const SketchEntityId c = addE(&sk, std::move(curve));
    const uint32_t id = appendSurfaceSketch(&s, sk, 0.0);
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::ExtrudedSurface;
    f.section.sketchId = id;
    f.section.curves = {c};
    f.distance = dist;
    appendSurfaceFeature(&s, f);
    return s;
}

static void surface() {
    const double dists[] = {kNaN, kInf, -0.5, 0.0, 1e-9, 0.5, 1e7};
    for (double d : dists) {
        std::printf("INFO surface extrude distance(%g) -> %s\n", d,
                    surfaceStatusName(regenerateSurfaceBody(extrudeState(d, L(0, 0, 1, 0)), nullptr)));
    }
    // Revolve angle edges.
    const double angles[] = {kNaN, -10.0, 0.0, 0.0005, 0.001, 359.9999, 360.0, 360.0000001, 720.0};
    for (double a : angles) {
        SurfaceBodyState s;
        CadSketch sk;
        sk.plane = Workplane::XY;
        const SketchEntityId p = addE(&sk, L(1, 0, 1, 1));
        const SketchEntityId ax = addE(&sk, L(0, -1, 0, 2));
        const uint32_t id = appendSurfaceSketch(&s, sk, 0.0);
        SurfaceFeature f;
        f.kind = SurfaceFeatureKind::RevolvedSurface;
        f.section.sketchId = id;
        f.section.curves = {p};
        f.axis = CadSketchEdgeRef{ax, 0};
        f.angleDegrees = a;
        appendSurfaceFeature(&s, f);
        std::printf("INFO surface revolve angle(%.10g) -> %s\n", a, surfaceStatusName(regenerateSurfaceBody(s, nullptr)));
    }
    // Thicken of planar patch with thickness edge values; thicken of a spline extrude.
    const double thick[] = {kNaN, 0.0, -0.2, 0.2, 1e9};
    for (double t : thick) {
        SurfaceBodyState s;
        CadSketch sk;
        sk.plane = Workplane::XY;
        addE(&sk, R(2, 2));
        const uint32_t id = appendSurfaceSketch(&s, sk, 0.0);
        SurfaceFeature patch;
        patch.kind = SurfaceFeatureKind::PlanarPatch;
        patch.section.sketchId = id;
        patch.regions = surfaceDefaultPatchRegions(sk);
        appendSurfaceFeature(&s, patch);
        SurfaceFeature th;
        th.kind = SurfaceFeatureKind::Thicken;
        th.source = SurfaceFeatureId{1};
        th.thickness = t;
        appendSurfaceFeature(&s, th);
        SurfaceBodyMesh m;
        const SurfaceStatus st = regenerateSurfaceBody(s, &m);
        std::printf("INFO surface thicken patch(%g) -> %s solidTris=%u open=%d\n", t, surfaceStatusName(st),
                    st == SurfaceStatus::Ok ? m.solid.triangleCount() : 0u, st == SurfaceStatus::Ok ? (int)m.open : -1);
    }
    {
        SketchSpline sp;
        sp.points = {{0.0, 0.0}, {0.5, 0.4}, {1.0, 0.0}, {1.5, -0.3}};
        SurfaceBodyState s = extrudeState(1.0, sp);
        SurfaceFeature th;
        th.kind = SurfaceFeatureKind::Thicken;
        th.source = SurfaceFeatureId{1};
        th.thickness = 0.1;
        appendSurfaceFeature(&s, th);
        const SurfaceStatus st = regenerateSurfaceBody(s, nullptr);
        out("SFD_thicken_spline_extrude_refused_by_name", st == SurfaceStatus::ThickenUnsupportedForSurfaceType,
            surfaceStatusName(st));
    }
    // Stitch tolerance edges: patch 2x2 + tube of width w extruded 1.
    const double widths[] = {2.0, 2.0 + 1.5e-6, 2.0 + 1.9e-6, 2.0 + 2.2e-6, 2.0 + 3e-6};
    for (double w : widths) {
        SurfaceBodyState s;
        CadSketch a;
        a.plane = Workplane::XY;
        addE(&a, R(2, 2));
        const uint32_t ia = appendSurfaceSketch(&s, a, 0.0);
        SurfaceFeature patch;
        patch.kind = SurfaceFeatureKind::PlanarPatch;
        patch.section.sketchId = ia;
        patch.regions = surfaceDefaultPatchRegions(a);
        appendSurfaceFeature(&s, patch);
        CadSketch b;
        b.plane = Workplane::XY;
        const SketchEntityId t = addE(&b, R(w, 2));
        const uint32_t ib = appendSurfaceSketch(&s, b, 0.0);
        SurfaceFeature ex;
        ex.kind = SurfaceFeatureKind::ExtrudedSurface;
        ex.section.sketchId = ib;
        ex.section.curves = {t};
        ex.distance = 1.0;
        appendSurfaceFeature(&s, ex);
        SurfaceBodyMesh pre;
        regenerateSurfaceBody(s, &pre);
        SurfaceFeature st;
        st.kind = SurfaceFeatureKind::Stitch;
        st.stitchFeatures = {SurfaceFeatureId{1}, SurfaceFeatureId{2}};
        appendSurfaceFeature(&s, st);
        SurfaceBodyMesh m;
        const SurfaceStatus r = regenerateSurfaceBody(s, &m);
        // "nothing averaged": positions of the tube patch unchanged by the stitch.
        bool unchanged = r == SurfaceStatus::Ok && pre.patches.size() == m.patches.size();
        for (size_t p = 0; unchanged && p < m.patches.size(); ++p) {
            unchanged = pre.patches[p].positions.size() == m.patches[p].positions.size();
            for (size_t i = 0; unchanged && i < m.patches[p].positions.size(); ++i) {
                unchanged = pre.patches[p].positions[i].x == m.patches[p].positions[i].x
                            && pre.patches[p].positions[i].y == m.patches[p].positions[i].y
                            && pre.patches[p].positions[i].z == m.patches[p].positions[i].z;
            }
        }
        std::printf("INFO stitch halfgap=%.3g -> %s stitches=%zu positionsUnchanged=%d\n", (w - 2.0) / 2.0,
                    surfaceStatusName(r), r == SurfaceStatus::Ok ? m.stitches.size() : 0u, (int)unchanged);
    }
    // A Surface body is never a sculpt source; and never a CAD face support (chooser reads cadOrNull only).
    ConstructionScene scene;
    SurfaceStatus why = SurfaceStatus::Ok;
    SurfaceBodyState s = extrudeState(0.5, L(0, 0, 1, 0));
    SceneObject* body = scene.addSurfaceBody(s, &why);
    ConstructionMesh src;
    out("SFD_not_a_sculpt_source", body != nullptr && !buildSculptSourceMesh(*body, &src), surfaceStatusName(why));
}

// ---------------- Sculpt: Clear Mask over the per-entry cap ----------------
static void sculptClearMaskCap(uint32_t side) {
    ConstructionMesh grid;
    for (uint32_t j = 0; j < side; ++j)
        for (uint32_t i = 0; i < side; ++i) {
            MeshVertex v{};
            v.position[0] = static_cast<float>(i) * 0.01f;
            v.position[1] = static_cast<float>(j) * 0.01f;
            v.position[2] = 0.0f;
            v.color[0] = v.color[1] = v.color[2] = 0.7f;
            grid.vertices.push_back(v);
        }
    for (uint32_t j = 0; j + 1 < side; ++j)
        for (uint32_t i = 0; i + 1 < side; ++i) {
            const uint32_t a = j * side + i, b = a + 1, c = a + side, d = c + 1;
            grid.indices.insert(grid.indices.end(), {a, b, d, a, d, c});
        }
    FrozenSculpt frozen;
    SculptSession session;
    session.bindTarget(&frozen);
    MeshValidation why = MeshValidation::Ok;
    const bool froze = session.freezeToSculpt(grid, ObjectId{7}, &why);
    const uint32_t n = frozen.mesh.vertexCount();
    for (uint32_t v = 0; v < n; ++v) frozen.mesh.setMaskWeight(v, 1.0f);
    const bool can = session.canClearMask();
    const auto st = session.clearMask();
    char buf[200];
    std::snprintf(buf, sizeof buf, "froze=%d vertices=%u masked=%u canClearMask=%d clearMask=%s maskedAfter=%u undo=%zu",
                  (int)froze, n, frozen.mesh.maskedVertexCount(), (int)can,
                  SculptSession::sculptHistoryStatusName(st), frozen.mesh.maskedVertexCount(),
                  session.history().undoDepth());
    std::printf("INFO sculpt_clear_mask side=%u %s\n", side, buf);
}

int main() {
    freeform();
    surface();
    sculptClearMaskCap(280);  // 78400 vertices: 940,800 B + overhead < 1 MiB
    sculptClearMaskCap(300);  // 90000 vertices: 1,080,000 B > 1 MiB
    std::printf("DIAG_D_DONE diffs=%d\n", g_fail);
    return 0;
}
