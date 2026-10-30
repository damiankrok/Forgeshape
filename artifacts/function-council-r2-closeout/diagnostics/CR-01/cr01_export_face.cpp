// CR-01 (FUNCTION-COUNCIL-R2-CLOSEOUT-R1) -- audit-only host diagnostic.
// Claim under test: GLB export places a FACE-SUPPORTED CAD Body at its stored
// (identity) transform instead of at its derived world placement
// (ConstructionScene::resolveWorldModel), so the exported file disagrees with
// what is rendered/picked.
//
// For each case the program builds a scene, exports it through the PRODUCT
// exporter (captureGlbExportScene + exportSceneAsGlb -> encodeGlb), writes the
// .glb bytes to argv[1]/<case>.glb, and prints per body:
//   * the snapshot model translation (what the renderer draws / CPU picking casts against),
//   * resolveWorldModel translation, body.transform() translation,
//   * the exporter's captured translation,
//   * EXPECTED world AABB = resolveWorldModel * (unbaked source mesh vertices),
//   * the AABB the exporter will write = (baked local positions + translation).
// The .glb is parsed independently by parse_glb.py (node translation + POSITION
// floats read back from the BIN chunk), so the written bytes are checked too.
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "forgeshape_cad_face.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_history.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"

using namespace forgeshape;

static std::string gOut = ".";

static CadBodyState rectBody(double w, double h, double d) {
    CadBodyState s;
    cadBaseSketch(s).plane = Workplane::XY;
    SketchRectangle r;
    r.center = SketchPoint{0, 0};
    r.width = w;
    r.height = h;
    addSketchEntity(&cadBaseSketch(s), r);
    s.extrude.profileEntityId = 1;
    s.extrude.depth = d;
    s.extrude.direction = ExtrudeDirection::AlongNormal;
    return s;
}

// A dependent rectangle body whose sketch stands on `kind` (first match) of `producer`.
static SceneObject* addDependent(ConstructionScene& scene, SceneObject* producer, CadFaceKind kind,
                                 double w, double h, double d) {
    const CadBodyState& ps = producer->cadOrNull()->state();
    std::vector<CadFace> faces;
    enumerateCadFaces(ps, &faces);
    CadFaceToken tok{};
    bool found = false;
    for (auto& f : faces) {
        if (f.token.kind == kind && f.eligible) { tok = f.token; found = true; break; }
    }
    if (!found) { std::printf("  no face of kind %d\n", (int)kind); return nullptr; }
    CadBodyState c = rectBody(w, h, d);
    cadBaseSketch(c).hasFaceSupport = true;
    auto& fs = cadBaseSketch(c).faceSupport;
    fs.producerObjectId = producer->objectId();
    fs.producerLocalFeatureId = kCadFeatureId;
    fs.face = tok;
    fs.lineageToken = cadTopologySignature(ps);
    CadStatus why = CadStatus::Ok;
    SceneObject* dep = scene.addCadBody(c, &why);
    if (!dep) { std::printf("  dependent refused %d\n", (int)why); return nullptr; }
    publishSceneObject(*dep);
    return dep;
}

static bool sourceVertices(const SceneObject& body, std::vector<Vec3>* out) {
    out->clear();
    if (const CadBody* cad = body.cadOrNull()) {
        ConstructionMesh m;
        if (cad->generateMesh(&m) != CadStatus::Ok) return false;
        for (auto& v : m.vertices) out->push_back(Vec3{v.position[0], v.position[1], v.position[2]});
        return true;
    }
    if (body.constructionOrNull()) {
        const ConstructionMesh m = body.construction().generateMesh();
        for (auto& v : m.vertices) out->push_back(Vec3{v.position[0], v.position[1], v.position[2]});
        return true;
    }
    return false;
}

static void report(const char* name, ConstructionScene& scene, ProjectKind kind) {
    std::printf("== CASE %s\n", name);
    SceneSnapshot snap = scene.snapshot();
    GlbExportScene ex;
    const GlbExportStatus st = captureGlbExportScene(scene, kind, &ex);
    std::printf("  capture=%s bodies=%zu\n", glbExportStatusName(st), ex.bodies.size());
    for (const GlbExportBody& e : ex.bodies) {
        const SceneObject* body = scene.findBody(e.objectId);
        Mat4 world{};
        const bool ok = scene.resolveWorldModel(e.objectId, &world);
        Mat4 snapModel{};
        bool inSnap = false;
        for (auto& it : snap) if (it.objectId == e.objectId) { snapModel = it.model; inSnap = true; }
        const Mat4 own = body->transform().modelMatrix();
        std::vector<Vec3> src;
        sourceVertices(*body, &src);
        float xmn[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, xmx[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (auto& p : src) {
            const Vec3 w = mat4TransformPoint(world, p);
            const float c[3] = {w.x, w.y, w.z};
            for (int k = 0; k < 3; ++k) { xmn[k] = std::fmin(xmn[k], c[k]); xmx[k] = std::fmax(xmx[k], c[k]); }
        }
        float gmn[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, gmx[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (auto& v : e.render.vertices)
            for (int k = 0; k < 3; ++k) {
                const float x = v.position[k] + e.translation[k];
                gmn[k] = std::fmin(gmn[k], x); gmx[k] = std::fmax(gmx[k], x);
            }
        float dmax = 0.0f;
        for (int k = 0; k < 3; ++k) dmax = std::fmax(dmax, std::fmax(std::fabs(gmn[k] - xmn[k]), std::fabs(gmx[k] - xmx[k])));
        std::printf("  body %llu kind=%s faceSupported=%d resolved=%d inSnapshot=%d\n",
                    (unsigned long long)e.objectId,
                    body->cadOrNull() ? "CAD" : (body->constructionOrNull() ? "Construction" : "other"),
                    body->isFaceSupportedCad() ? 1 : 0, ok ? 1 : 0, inSnap ? 1 : 0);
        std::printf("    snapshot.T=(%.4f,%.4f,%.4f) resolveWorldModel.T=(%.4f,%.4f,%.4f) transform().T=(%.4f,%.4f,%.4f) export.T=(%.4f,%.4f,%.4f)\n",
                    snapModel.m[12], snapModel.m[13], snapModel.m[14], world.m[12], world.m[13], world.m[14],
                    own.m[12], own.m[13], own.m[14], e.translation[0], e.translation[1], e.translation[2]);
        std::printf("    resolveWorldModel.col0=(%.3f,%.3f,%.3f) col2=(%.3f,%.3f,%.3f)  export.local.col0=(%.3f,%.3f,%.3f) col2=(%.3f,%.3f,%.3f)\n",
                    world.m[0], world.m[1], world.m[2], world.m[8], world.m[9], world.m[10],
                    e.local.m[0], e.local.m[1], e.local.m[2], e.local.m[8], e.local.m[9], e.local.m[10]);
        if (!src.empty()) std::printf("    expected_world_aabb=[%.4f,%.4f,%.4f]..[%.4f,%.4f,%.4f]\n", xmn[0], xmn[1], xmn[2], xmx[0], xmx[1], xmx[2]);
        std::printf("    exported_world_aabb=[%.4f,%.4f,%.4f]..[%.4f,%.4f,%.4f]\n", gmn[0], gmn[1], gmn[2], gmx[0], gmx[1], gmx[2]);
        if (src.empty()) {
            // Imported Mesh etc.: this diagnostic re-derives no source for it; only the
            // translation columns above are compared.
            std::printf("    max_abs_aabb_delta=n/a verdict=NO_SOURCE_REDERIVED (translation equal=%d)\n",
                        (e.translation[0] == world.m[12] && e.translation[1] == world.m[13] && e.translation[2] == world.m[14]) ? 1 : 0);
        } else {
            std::printf("    max_abs_aabb_delta=%.4f verdict=%s\n", dmax, dmax <= 1e-4f ? "MATCH" : "MISMATCH");
        }
    }
    GlbExportStatus why = GlbExportStatus::Ok;
    std::vector<uint8_t> glb = exportSceneAsGlb(scene, kind, &why);
    const std::string path = gOut + "/" + name + ".glb";
    std::ofstream o(path, std::ios::binary);
    o.write(reinterpret_cast<const char*>(glb.data()), static_cast<std::streamsize>(glb.size()));
    std::printf("  glb=%s status=%s bytes=%zu\n", path.c_str(), glbExportStatusName(why), glb.size());
}

static void setT(SceneObject* b, double px, double py, double pz, double rx, double ry, double rz,
                 double sx = 1, double sy = 1, double sz = 1) {
    TransformValues v;
    v.positionX = px; v.positionY = py; v.positionZ = pz;
    v.rotationX = rx; v.rotationY = ry; v.rotationZ = rz;
    v.scaleX = sx; v.scaleY = sy; v.scaleZ = sz;
    TransformValidation w{};
    const auto s = b->transform().setValues(v, &w);
    std::printf("  setValues(id=%llu) status=%d\n", (unsigned long long)b->objectId(), (int)s);
}

int main(int argc, char** argv) {
    if (argc > 1) gOut = argv[1];
    // S1: dependent on the FAR cap of an unmoved producer.
    {
        ConstructionScene scene{NoProjectTag{}};
        CadStatus why;
        SceneObject* p = scene.addCadBody(rectBody(2, 1, 3), &why); publishSceneObject(*p);
        addDependent(scene, p, CadFaceKind::CapFar, 0.5, 0.5, 0.5);
        report("S1_cap", scene, ProjectKind::Construction);
    }
    // S2: dependent on a SIDE face of an unmoved producer.
    {
        ConstructionScene scene{NoProjectTag{}};
        CadStatus why;
        SceneObject* p = scene.addCadBody(rectBody(2, 1, 3), &why); publishSceneObject(*p);
        addDependent(scene, p, CadFaceKind::Side, 0.5, 0.5, 0.5);
        report("S2_side", scene, ProjectKind::Construction);
    }
    // S3: a chain A -> B (far cap of A) -> C (far cap of B).
    {
        ConstructionScene scene{NoProjectTag{}};
        CadStatus why;
        SceneObject* a = scene.addCadBody(rectBody(2, 2, 1), &why); publishSceneObject(*a);
        SceneObject* b = addDependent(scene, a, CadFaceKind::CapFar, 1, 1, 0.5);
        addDependent(scene, b, CadFaceKind::CapFar, 0.5, 0.5, 0.25);
        report("S3_chain", scene, ProjectKind::Construction);
    }
    // S4: producer MOVED and ROTATED, dependent on its far cap and on a side.
    {
        ConstructionScene scene{NoProjectTag{}};
        CadStatus why;
        SceneObject* p = scene.addCadBody(rectBody(2, 1, 3), &why); publishSceneObject(*p);
        setT(p, 1.0, 2.0, 3.0, 0.0, 30.0, 45.0);
        addDependent(scene, p, CadFaceKind::CapFar, 0.5, 0.5, 0.5);
        addDependent(scene, p, CadFaceKind::Side, 0.4, 0.4, 0.2);
        report("S4_producer_moved_rotated", scene, ProjectKind::Construction);
    }
    // C1 (control): world-plane CAD body, translated + rotated + non-uniform scale.
    {
        ConstructionScene scene{NoProjectTag{}};
        CadStatus why;
        SceneObject* p = scene.addCadBody(rectBody(2, 1, 3), &why); publishSceneObject(*p);
        setT(p, -1.5, 0.75, 2.0, 10.0, 20.0, 30.0, 1.0, 2.0, 0.5);
        report("C1_world_cad_trs", scene, ProjectKind::Construction);
    }
    // C2 (control): Construction primitive (default startup body), translated + rotated + scaled.
    {
        ConstructionScene scene{NoProjectTag{}};
        SceneObject& b = scene.addBody(); publishSceneObject(b);
        setT(&b, 0.5, -1.0, 4.0, 45.0, 0.0, 15.0, 1.5, 1.0, 0.75);
        report("C2_construction_trs", scene, ProjectKind::Construction);
    }
    // F*: committed corpus fixtures as read-only INPUT, loaded through the product path.
    for (int a = 2; a < argc; ++a) {
        std::ifstream in(argv[a], std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ProjectDocument d;
        const ProjectCodecStatus ds = decodeProject(bytes.data(), bytes.size(), &d);
        ConstructionScene scene{NoProjectTag{}};
        SculptSession s;
        ConstructionHistory h(scene);
        const ProjectCodecStatus ls = loadProjectDocument(d, scene, s, h);
        std::string n = argv[a];
        n = n.substr(n.find_last_of('/') + 1);
        n = "F_" + n.substr(0, n.find('.'));
        std::printf("-- fixture %s decode=%s load=%s\n", argv[a], projectCodecStatusName(ds), projectCodecStatusName(ls));
        report(n.c_str(), scene, d.kind);
    }
    return 0;
}
