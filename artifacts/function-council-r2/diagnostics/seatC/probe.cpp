// FC2 seat C scratch diagnostic. NOT part of the build. Links against the
// host self-test objects of the audited tree.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_sketch_inventory.h"
#include "forgeshape_camera.h"
#include "forgeshape_history.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"
using namespace forgeshape;

static SketchRectangle rect(double cu, double cv, double w, double h) {
    SketchRectangle r; r.center = SketchPoint{cu, cv}; r.width = w; r.height = h; return r; }
static SketchLine lineOf(double a, double b, double c, double d) {
    SketchLine l; l.start = SketchPoint{a, b}; l.end = SketchPoint{c, d}; return l; }
static SketchEntityId add(CadSketch* s, SketchEntity::Payload p) {
    SketchEntityId id = kNoSketchEntity; addSketchEntity(s, std::move(p), &id); return id; }
static ExtrudeFeature oneSide(SketchEntityId p, double d, ExtrudeDirection dir = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature e; e.profileEntityId = p; e.depth = d; e.direction = dir; return e; }
static CadFeatureSupport capFar(const CadBodyState& s, uint32_t fid) {
    CadFeatureSupport sup; sup.featureId = fid; sup.face.kind = CadFaceKind::CapFar;
    sup.lineageToken = cadFeatureTopologySignature(s, fid); return sup; }

int main() {
    // P1: navigator flip / roll vs the camera (FUNCTION-COUNCIL-R1 D2 re-check).
    {
        SketchSession s;
        s.begin(Workplane::XY);
        CameraController cam; cam.setViewport(1000, 1000);
        auto frameFrom = [&](const SketchFrame& f) { cam.frameSketchView(f.origin, f.u, f.v, f.n); return cam.snapshot(); };
        // What JNI beginSketchView() does: reads frame(), the authoring frame.
        CameraSnapshot a = frameFrom(s.frame());
        s.setViewFlipped(true);
        CameraSnapshot b = frameFrom(s.frame());
        s.rotateView(1);
        CameraSnapshot c = frameFrom(s.frame());
        CameraSnapshot d = frameFrom(s.viewFrame());
        bool ab = std::memcmp(&a.view, &b.view, sizeof(a.view)) == 0;
        bool ac = std::memcmp(&a.view, &c.view, sizeof(a.view)) == 0;
        bool ad = std::memcmp(&a.view, &d.view, sizeof(a.view)) == 0;
        std::printf("P1 flipped=%d quarters=%d frame()_unchanged_view_after_flip=%d after_flip_and_roll=%d viewFrame()_differs=%d\n",
                    s.viewFlipped(), s.viewQuarterTurns(), ab, ac, !ad);
        s.cancel();
    }
    // P2: Revolve angle domain edges.
    {
        CadSketch sk; add(&sk, rect(1.5, 0.5, 1.0, 1.0)); add(&sk, lineOf(0.0, -1.0, 0.0, 2.0));
        double angles[] = {360.0, 359.9999999, 359.99, 0.001, 0.000999, 360.0000001, NAN, -90.0};
        for (double ang : angles) {
            RevolveFeature r; r.profileEntityId = 1; r.axis = CadSketchEdgeRef{2, 0}; r.angleDegrees = ang;
            CadBodyState st = makeCadRevolveBodyState(sk, r);
            CadBodyMesh mesh; uint32_t failed = 0;
            CadStatus v = validateCadBodyState(st);
            CadStatus g = regenerateCadBody(st, &mesh);
            std::printf("P2 angle=%.10g validate=%s regenerate=%s volume=%.9g tris=%zu\n", ang, cadStatusName(v), cadStatusName(g),
                        g == CadStatus::Ok ? mesh.volume : -1.0, g == CadStatus::Ok ? mesh.mesh.indices.size() / 3 : 0);
            (void)failed;
        }
    }
    // P3: the shop body: codec round trip bytes, determinism, lineage under a size edit.
    {
        CadSketch base; SketchEntityId br = add(&base, rect(0, 0, 2, 2));
        CadBodyState st = makeCadBodyState(base, oneSide(br, 1.0));
        CadSketch boss; SketchEntityId bo = add(&boss, rect(-0.5, 0.5, 0.6, 0.6));
        appendCadLaterFeatureWithSketch(&st, CadFeatureOperation::Add, capFar(st, kCadFeatureId), std::move(boss), oneSide(bo, 0.5));
        CadSketch pocket; SketchEntityId pr = add(&pocket, rect(0.5, -0.5, 0.6, 0.6));
        appendCadLaterFeatureWithSketch(&st, CadFeatureOperation::Cut, capFar(st, kCadFeatureId), std::move(pocket), oneSide(pr, 0.4, ExtrudeDirection::AgainstNormal));
        CadBodyMesh m1, m2; regenerateCadBody(st, &m1); regenerateCadBody(st, &m2);
        bool det = m1.mesh.vertices.size() == m2.mesh.vertices.size() &&
                   std::memcmp(m1.mesh.vertices.data(), m2.mesh.vertices.data(), m1.mesh.vertices.size() * sizeof(m1.mesh.vertices[0])) == 0 &&
                   m1.mesh.indices == m2.mesh.indices && m1.triangleFace == m2.triangleFace;
        std::printf("P3 volume=%.9g expected=4.036 deterministic_regen=%d\n", m1.volume, det);
        ConstructionScene scene{NoProjectTag{}};
        ConstructionHistory hist{scene};
        SceneObject* body = scene.addCadBody(st);
        publishSceneObject(*body);
        std::vector<uint8_t> e1 = encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction));
        ProjectDocument doc; ProjectCodecStatus ds = decodeProject(e1.data(), e1.size(), &doc);
        std::vector<uint8_t> e2 = encodeProjectV1(doc);
        std::printf("P3 encode_bytes=%zu decode=%s reencode_identical=%d\n", e1.size(), projectCodecStatusName(ds), e1 == e2);
        // lineage of feature 1 under a depth edit, a symmetric edit, and a structural edit (add hole).
        uint64_t sig0 = cadFeatureTopologySignature(st, kCadFeatureId);
        CadBodyState deeper = st; deeper.extrude.depth = 1.5;
        CadBodyState sym = st; sym.extrude = extrudeFeatureWithExtent(st.extrude, ExtrudeExtentMode::Symmetric, ExtrudeDirection::AlongNormal);
        std::printf("P3 sig_base=%016llx depth_edit_same=%d symmetric_same=%d validate_deeper=%s validate_sym=%s\n",
                    (unsigned long long)sig0, cadFeatureTopologySignature(deeper, kCadFeatureId) == sig0,
                    cadFeatureTopologySignature(sym, kCadFeatureId) == sig0,
                    cadStatusName(validateCadBodyState(deeper)), cadStatusName(validateCadBodyState(sym)));
        // Retained sketch deletion refusals.
        CadBodyState out;
        std::printf("P3 delete_root=%s delete_used=%s\n",
                    cadStatusName(removeUnusedCadSketch(st, st.baseSketchId, &out)),
                    cadStatusName(removeUnusedCadSketch(st, 2, &out)));
        // Cut that swallows the body; Add disjoint.
        CadBodyState swallow = st;
        CadSketch big; SketchEntityId bg = add(&big, rect(0, 0, 10, 10));
        appendCadLaterFeatureWithSketch(&swallow, CadFeatureOperation::Cut, capFar(swallow, kCadFeatureId), std::move(big), oneSide(bg, 5.0, ExtrudeDirection::AgainstNormal));
        CadRegenerationReport rep1; CadBodyMesh mm;
        CadStatus sw = regenerateCadBody(swallow, &mm, &rep1); uint32_t ff = rep1.failedFeatureId;
        CadBodyState disj = st;
        CadSketch far; SketchEntityId fr = add(&far, rect(0, 0, 0.2, 0.2));
        appendCadLaterFeatureWithSketch(&disj, CadFeatureOperation::Add, capFar(disj, kCadFeatureId), std::move(far), oneSide(fr, 0.5, ExtrudeDirection::AlongNormal));
        // move it off: offset normal? use plane offset not available; instead Add pointing into body fully inside -> AddNoEffect
        CadBodyState inside = st;
        CadSketch in; SketchEntityId ir = add(&in, rect(-0.5, -0.5, 0.2, 0.2));
        appendCadLaterFeatureWithSketch(&inside, CadFeatureOperation::Add, capFar(inside, kCadFeatureId), std::move(in), oneSide(ir, 0.3, ExtrudeDirection::AgainstNormal));
        CadRegenerationReport rep2; CadStatus is = regenerateCadBody(inside, &mm, &rep2); uint32_t f2 = rep2.failedFeatureId;
        std::printf("P3 cut_swallow=%s failed_feature=%u add_inside=%s failed_feature=%u\n", cadStatusName(sw), ff, cadStatusName(is), f2);
        // feature cap
        CadBodyState many = st; CadStatus last = CadStatus::Ok; int n = 0;
        for (int i = 0; i < 20; ++i) {
            CadSketch k; SketchEntityId kr = add(&k, rect(-0.8 + 0.1 * (i % 10), 0.8 - 0.1 * (i / 10), 0.04, 0.04));
            CadBodyState next = many;
            uint32_t fid = appendCadLaterFeatureWithSketch(&next, CadFeatureOperation::Add, capFar(next, kCadFeatureId), std::move(k), oneSide(kr, 0.05));
            if (fid == 0) { std::printf("P3 append_refused_at_count=%u (kMaxCadFeatures=%u)\n", cadFeatureCount(next), (unsigned)kMaxCadFeatures); last = CadStatus::TooManyFeatures; break; }
            last = validateCadBodyState(next);
            if (last != CadStatus::Ok) break;
            many = next; ++n;
        }
        std::printf("P3 later_features_appended=%d then=%s total_features=%u\n", n, cadStatusName(last), cadFeatureCount(many));
    }
    // P5: a hand-built over-cap state (17 features) must be refused by validate, not only by the builder.
    {
        CadSketch base; SketchEntityId br = add(&base, rect(0, 0, 2, 2));
        CadBodyState st = makeCadBodyState(base, oneSide(br, 1.0));
        for (int i = 0; i < 15; ++i) {
            CadSketch k; SketchEntityId kr = add(&k, rect(-0.8 + 0.1 * i, 0.0, 0.04, 0.04));
            appendCadLaterFeatureWithSketch(&st, CadFeatureOperation::Add, capFar(st, kCadFeatureId), std::move(k), oneSide(kr, 0.05));
        }
        CadBodyState over = st;
        CadFeature extra = over.laterFeatures.back(); extra.featureId = over.nextFeatureId; over.nextFeatureId += 1;
        over.laterFeatures.push_back(extra);
        std::printf("P5 at_cap=%u validate=%s over_cap=%u validate=%s\n", cadFeatureCount(st), cadStatusName(validateCadBodyState(st)),
                    cadFeatureCount(over), cadStatusName(validateCadBodyState(over)));
    }
    // P6: full sketch TABLE (16 records) with only ONE feature; then the new-Add candidate as
    // SketchSession::candidateState builds it (forgeshape_sketch_session.cpp:2880-2903).
    {
        CadSketch base; SketchEntityId br = add(&base, rect(0, 0, 2, 2));
        CadBodyState st = makeCadBodyState(base, oneSide(br, 1.0));
        int kept = 0;
        for (int i = 0; i < 20; ++i) {
            CadSketch k; add(&k, rect(-0.8 + 0.1 * i, 0.0, 0.04, 0.04));
            CadFeatureSupport sup = capFar(st, kCadFeatureId);
            CadBodyState next = st;
            if (addCadSketchRecord(&next, std::move(k), &sup) == kNoCadSketch) break;
            if (validateCadBodyState(next) != CadStatus::Ok) { std::printf("P6 keep invalid %s\n", cadStatusName(validateCadBodyState(next))); break; }
            st = next; ++kept;
        }
        std::printf("P6 kept_sketches=%d table=%zu features=%u add_offered_by_feature_count_rule=%d\n", kept, st.sketches.size(),
                    cadFeatureCount(st), cadFeatureCount(st) < kMaxCadFeatures);
        CadBodyState cand = st;
        CadSketch k; SketchEntityId kr = add(&k, rect(0.5, 0.5, 0.2, 0.2));
        if (appendCadLaterFeatureWithSketch(&cand, CadFeatureOperation::Add, capFar(cand, kCadFeatureId), std::move(k), oneSide(kr, 0.2)) == 0) {
            CadFeature refused; refused.featureId = cand.nextFeatureId; refused.operation = CadFeatureOperation::Add; refused.extrude = oneSide(kr, 0.2);
            cand.laterFeatures.push_back(refused);
            CadBodyMesh mm; CadRegenerationReport rep;
            std::printf("P6 candidate_validate=%s regenerate=%s (refused feature sketchId=%u)\n", cadStatusName(validateCadBodyState(cand)),
                        cadStatusName(regenerateCadBody(cand, &mm, &rep)), (unsigned)refused.sketchId);
        }
    }
    // P7: ring + its own disk (CAD-FOUNDATION-C1 union) vs ring alone vs a stale hole list.
    {
        CadSketch sk; SketchEntityId r = add(&sk, rect(0, 0, 2, 2));
        SketchCircle c; c.center = SketchPoint{0, 0}; c.radius = 0.5; SketchEntityId ci = add(&sk, c);
        ExtrudeFeature ring = oneSide(r, 1.0); setExtrudeRegions(&ring, {ProfileRegionRef{r, {ci}}});
        ExtrudeFeature both = oneSide(r, 1.0); setExtrudeRegions(&both, {ProfileRegionRef{r, {ci}}, ProfileRegionRef{ci, {}}});
        ExtrudeFeature stale = oneSide(r, 1.0); setExtrudeRegions(&stale, {ProfileRegionRef{r, {}}});
        CadBodyMesh m; CadStatus a = regenerateCadBody(makeCadBodyState(sk, ring), &m); double va = m.volume;
        CadStatus b = regenerateCadBody(makeCadBodyState(sk, both), &m); double vb = m.volume; uint32_t comps = m.components;
        CadStatus d = regenerateCadBody(makeCadBodyState(sk, stale), &m);
        std::printf("P7 ring=%s vol=%.6f (4-pi/4*~=%.6f) ring+disk=%s vol=%.6f components=%u stale_holes=%s\n", cadStatusName(a), va, 4.0 - M_PI * 0.25,
                    cadStatusName(b), vb, comps, cadStatusName(d));
    }
    // P4: line length typed edit refusals and P0-fixed rule.
    {
        SketchSession s; s.begin(Workplane::XY);
        CameraController cam; cam.setViewport(1000, 1000);
        const SketchFrame& f = s.frame(); cam.frameSketchView(f.origin, f.u, f.v, f.n);
        SketchEntity::Payload p = lineOf(0, 0, 0.3, 0.4);
        // place via touch then replace
        s.setTool(SketchTool::Line);
        float x0, y0, x1, y1;
        s.sketchToScreen(cam.snapshot(), SketchPoint{0.1, 0.1}, 1000, 1000, &x0, &y0);
        s.sketchToScreen(cam.snapshot(), SketchPoint{0.5, 0.6}, 1000, 1000, &x1, &y1);
        TouchPointer tp{7, x0, y0};
        s.onTouch(TouchAction::Down, -1, &tp, 1, cam.snapshot(), 1000, 1000);
        tp = TouchPointer{7, x1, y1}; s.onTouch(TouchAction::Move, -1, &tp, 1, cam.snapshot(), 1000, 1000);
        s.onTouch(TouchAction::Up, 7, &tp, 1, cam.snapshot(), 1000, 1000);
        SketchEntityId id = s.sketch().entities.empty() ? kNoSketchEntity : s.sketch().entities.back().id();
        CadStatus rs = s.replaceEntity(id, p);
        double vals[] = {0.0, -1.0, NAN, INFINITY, 1e9, 1.0};
        for (double v : vals) {
            CadStatus st = s.applyLineLength(id, Meters{v});
            const SketchLine& l = *s.sketch().entities.back().line();
            std::printf("P4 placed=%zu replace=%s len=%g -> %s start=(%g,%g) end=(%.9g,%.9g)\n", s.sketch().entities.size(), cadStatusName(rs), v, cadStatusName(st),
                        l.start.u, l.start.v, l.end.u, l.end.v);
        }
        s.cancel();
    }
    return 0;
}
