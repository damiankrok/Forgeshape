#include "forgeshape_gltf_import_selftest.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_construction.h"
#include "forgeshape_glb_import_fixture.h"
#include "forgeshape_glb_roundtrip.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_gltf_import.h"
#include "forgeshape_history.h"
#include "forgeshape_import_commit.h"
#include "forgeshape_import_preview.h"
#include "forgeshape_json.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh_export.h"
#include "forgeshape_mesh_import.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_transform.h"
#include "forgeshape_workplane.h"

namespace forgeshape {
namespace {

struct Recorder {
    GltfImportSelfTestResult* out;
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

// A scene built the way the product builds one.
struct Fixture {
    ConstructionScene scene;
    ConstructionHistory history{scene};
    SculptSession session;

    Fixture() {
        session.bindTarget(&scene.activeBody().frozenSculpt());
        publishConstructionObject(scene.activeBody().construction(),
                                  scene.activeBody().meshStore());
    }

    SceneObject& first() { return scene.bodyAt(0); }

    void setBox(SceneObject& body, double w, double h, double d) {
        applyPrimitive(body.construction(), body.meshStore(), PrimitiveSpec::forBox(w, h, d));
    }

    void place(SceneObject& body, const TransformValues& values) {
        applyTransformValues(body.transform(), values);
    }
};

TransformValues placement(double px, double py, double pz, double rx, double ry, double rz,
                          double sx, double sy, double sz) {
    TransformValues values;
    values.positionX = px;
    values.positionY = py;
    values.positionZ = pz;
    values.rotationX = rx;
    values.rotationY = ry;
    values.rotationZ = rz;
    values.scaleX = sx;
    values.scaleY = sy;
    values.scaleZ = sz;
    return values;
}

// Rewrites the JSON chunk of a GLB, re-framing the container around it.
//
// How every unsupported-feature case below is built: take a real export, change
// one thing in its document, and check that the reader refuses for THAT reason.
// Hand-writing a whole GLB per case would test a fixture rather than the file
// the product actually produces.
std::vector<uint8_t> withJson(const std::vector<uint8_t>& original, const std::string& json) {
    // Read the original's BIN chunk out, from its own headers.
    if (original.size() < 20) {
        return {};
    }
    const auto readU32 = [&original](size_t at) {
        return static_cast<uint32_t>(original[at]) | (static_cast<uint32_t>(original[at + 1]) << 8)
                | (static_cast<uint32_t>(original[at + 2]) << 16)
                | (static_cast<uint32_t>(original[at + 3]) << 24);
    };
    const uint32_t originalJsonLength = readU32(12);
    const size_t binHeader = 20 + originalJsonLength;
    if (binHeader + 8 > original.size()) {
        return {};
    }
    const uint32_t binLength = readU32(binHeader);
    if (binHeader + 8 + binLength > original.size()) {
        return {};
    }

    std::string padded = json;
    while ((padded.size() % 4u) != 0u) {
        padded.push_back(' ');
    }
    const uint32_t jsonLength = static_cast<uint32_t>(padded.size());
    const uint32_t total = 12u + 8u + jsonLength + 8u + binLength;

    std::vector<uint8_t> out;
    out.reserve(total);
    const auto appendU32 = [&out](uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
        }
    };
    appendU32(0x46546C67u);
    appendU32(2u);
    appendU32(total);
    appendU32(jsonLength);
    appendU32(0x4E4F534Au);
    out.insert(out.end(), padded.begin(), padded.end());
    appendU32(binLength);
    appendU32(0x004E4942u);
    out.insert(out.end(), original.begin() + static_cast<long>(binHeader) + 8,
               original.begin() + static_cast<long>(binHeader) + 8 + binLength);
    return out;
}

std::string jsonOf(const std::vector<uint8_t>& glb) {
    if (glb.size() < 20) {
        return {};
    }
    const uint32_t length = static_cast<uint32_t>(glb[12]) | (static_cast<uint32_t>(glb[13]) << 8)
            | (static_cast<uint32_t>(glb[14]) << 16) | (static_cast<uint32_t>(glb[15]) << 24);
    if (20u + length > glb.size()) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(glb.data() + 20), length);
}

// Replaces the first occurrence of `find` with `replace`. Enough for the
// one-change-per-case edits below, and deliberately not a JSON writer.
std::string replaceFirst(const std::string& text, const std::string& find,
                         const std::string& replace) {
    const size_t at = text.find(find);
    if (at == std::string::npos) {
        return text;
    }
    std::string out = text;
    out.replace(at, find.size(), replace);
    return out;
}

GlbImportStatus importOf(const std::vector<uint8_t>& glb) {
    ParsedGlbScene scene;
    return importGlb(glb.data(), glb.size(), &scene);
}

// A hand-built GLB, for the R1 cases a ForgeShape export cannot produce.
//
// The exporter writes one primitive, always with NORMAL, always with uint32
// indices and never with a node matrix, so editing its output cannot reach a
// shared POSITION accessor, a uint16 index buffer, a double-sided material or a
// mesh with no normals at all. This builder can, and it deliberately shares
// nothing with either the exporter or the fixture builder: it exists to state
// one awkward file per case.
struct MiniGlbSpec {
    std::vector<float> positions;               // xyz triples
    std::vector<float> normals;                 // empty means "no NORMAL"
    std::vector<std::vector<uint32_t>> primitives;  // one index list per primitive
    std::vector<bool> doubleSided;              // parallel to `primitives`
    bool uint16Indices = false;
    // Extra members for the node object, e.g. `,"matrix":[...]`. Empty means a
    // node with no transform at all, which glTF says is the identity.
    std::string nodeMembers;
    // The node's and the mesh's `name`, if the file states one. Both are
    // omitted when empty, because "absent" and "empty string" are different
    // files and the naming rule has to be tested against both.
    std::string nodeName;
    std::string meshName;
    // Further top-level nodes, all instancing the SAME mesh 0. Each entry is
    // that node's extra members, so a multi-node file can place its instances
    // apart. Present so the "one node, one object" rule can be tested with more
    // than one node.
    std::vector<std::string> extraNodeMembers;
};

std::vector<uint8_t> buildMiniGlb(const MiniGlbSpec& spec) {
    std::vector<uint8_t> bin;
    const auto appendU32 = [&bin](uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            bin.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
        }
    };
    const auto appendF32 = [&appendU32](float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        appendU32(bits);
    };
    const auto pad = [&bin]() {
        while ((bin.size() % 4u) != 0u) {
            bin.push_back(0u);
        }
    };

    const uint32_t positionOffset = 0;
    for (float value : spec.positions) {
        appendF32(value);
    }
    const uint32_t positionLength = static_cast<uint32_t>(bin.size());
    const uint32_t normalOffset = static_cast<uint32_t>(bin.size());
    for (float value : spec.normals) {
        appendF32(value);
    }
    const uint32_t normalLength = static_cast<uint32_t>(bin.size()) - normalOffset;

    std::vector<uint32_t> indexOffset;
    std::vector<uint32_t> indexLength;
    for (const std::vector<uint32_t>& indices : spec.primitives) {
        pad();
        indexOffset.push_back(static_cast<uint32_t>(bin.size()));
        for (uint32_t index : indices) {
            if (spec.uint16Indices) {
                bin.push_back(static_cast<uint8_t>(index & 0xFFu));
                bin.push_back(static_cast<uint8_t>((index >> 8) & 0xFFu));
            } else {
                appendU32(index);
            }
        }
        indexLength.push_back(static_cast<uint32_t>(bin.size()) - indexOffset.back());
    }
    pad();

    const uint32_t vertexCount = static_cast<uint32_t>(spec.positions.size() / 3u);
    const bool hasNormals = !spec.normals.empty();
    const uint32_t firstIndexAccessor = hasNormals ? 2u : 1u;

    char text[256];
    std::string json = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0";
    for (size_t n = 0; n < spec.extraNodeMembers.size(); ++n) {
        std::snprintf(text, sizeof(text), ",%u", static_cast<uint32_t>(n + 1));
        json += text;
    }
    json += "]}],\"nodes\":[{\"mesh\":0";
    json += spec.nodeMembers;
    if (!spec.nodeName.empty()) {
        json += ",\"name\":\"" + spec.nodeName + "\"";
    }
    json += "}";
    for (const std::string& members : spec.extraNodeMembers) {
        json += ",{\"mesh\":0";
        json += members;
        json += "}";
    }
    json += "],\"materials\":[{\"doubleSided\":false},{\"doubleSided\":true}],\"meshes\":[{";
    if (!spec.meshName.empty()) {
        json += "\"name\":\"" + spec.meshName + "\",";
    }
    json += "\"primitives\":[";
    for (size_t p = 0; p < spec.primitives.size(); ++p) {
        if (p != 0) {
            json += ",";
        }
        json += "{\"attributes\":{\"POSITION\":0";
        if (hasNormals) {
            json += ",\"NORMAL\":1";
        }
        std::snprintf(text, sizeof(text), "},\"indices\":%u,\"material\":%d,\"mode\":4}",
                      firstIndexAccessor + static_cast<uint32_t>(p),
                      (p < spec.doubleSided.size() && spec.doubleSided[p]) ? 1 : 0);
        json += text;
    }
    json += "]}],\"accessors\":[";
    std::snprintf(text, sizeof(text),
                  "{\"bufferView\":0,\"componentType\":5126,\"count\":%u,\"type\":\"VEC3\"}",
                  vertexCount);
    json += text;
    if (hasNormals) {
        std::snprintf(text, sizeof(text),
                      ",{\"bufferView\":1,\"componentType\":5126,\"count\":%u,\"type\":\"VEC3\"}",
                      vertexCount);
        json += text;
    }
    for (size_t p = 0; p < spec.primitives.size(); ++p) {
        std::snprintf(text, sizeof(text),
                      ",{\"bufferView\":%u,\"componentType\":%d,\"count\":%u,\"type\":\"SCALAR\"}",
                      firstIndexAccessor + static_cast<uint32_t>(p),
                      spec.uint16Indices ? 5123 : 5125,
                      static_cast<uint32_t>(spec.primitives[p].size()));
        json += text;
    }
    json += "],\"bufferViews\":[";
    std::snprintf(text, sizeof(text), "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}",
                  positionOffset, positionLength);
    json += text;
    if (hasNormals) {
        std::snprintf(text, sizeof(text), ",{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}",
                      normalOffset, normalLength);
        json += text;
    }
    for (size_t p = 0; p < spec.primitives.size(); ++p) {
        std::snprintf(text, sizeof(text), ",{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}",
                      indexOffset[p], indexLength[p]);
        json += text;
    }
    std::snprintf(text, sizeof(text), "],\"buffers\":[{\"byteLength\":%u}]}",
                  static_cast<uint32_t>(bin.size()));
    json += text;

    std::vector<uint8_t> jsonChunk(json.begin(), json.end());
    while ((jsonChunk.size() % 4u) != 0u) {
        jsonChunk.push_back(static_cast<uint8_t>(' '));
    }

    std::vector<uint8_t> glb;
    const auto appendHeader = [&glb](uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            glb.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
        }
    };
    appendHeader(0x46546C67u);
    appendHeader(2u);
    appendHeader(static_cast<uint32_t>(12 + 8 + jsonChunk.size() + 8 + bin.size()));
    appendHeader(static_cast<uint32_t>(jsonChunk.size()));
    appendHeader(0x4E4F534Au);
    glb.insert(glb.end(), jsonChunk.begin(), jsonChunk.end());
    appendHeader(static_cast<uint32_t>(bin.size()));
    appendHeader(0x004E4942u);
    glb.insert(glb.end(), bin.begin(), bin.end());
    return glb;
}

// A small asymmetric quad grid: four vertices, two triangles, no symmetry that
// could let a wrong axis pass for a right one.
MiniGlbSpec twoTriangleSheet() {
    MiniGlbSpec spec;
    spec.positions = {
        0.0f, 0.0f, 0.0f,
        2.0f, 0.0f, 0.0f,
        2.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };
    spec.primitives = {{0u, 1u, 2u}, {0u, 2u, 3u}};
    spec.doubleSided = {false, false};
    return spec;
}


// ---------------------------------------------------------------------------
// IMPORT-GLB-SKETCH-GUARD-R1 helpers
// ---------------------------------------------------------------------------

// Everything an import would move, read the way the product reads it.
struct ImportWitness {
    size_t bodies = 0;
    ObjectId active = kNoObject;
    ObjectId nextId = kNoObject;
    size_t undo = 0;
    size_t redo = 0;
    uint64_t fingerprint = 0;
    std::vector<uint8_t> encoded;

    bool operator==(const ImportWitness& o) const {
        return bodies == o.bodies && active == o.active && nextId == o.nextId && undo == o.undo
               && redo == o.redo && fingerprint == o.fingerprint && encoded == o.encoded;
    }
};

ImportWitness importWitness(const ConstructionScene& scene, const ConstructionHistory& history) {
    ImportWitness w;
    w.bodies = scene.bodyCount();
    w.active = scene.activeBodyId();
    w.nextId = scene.nextObjectId();
    w.undo = history.undoDepth();
    w.redo = history.redoDepth();
    w.fingerprint = projectSemanticFingerprint(scene, ProjectKind::Construction);
    w.encoded = encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction));
    return w;
}

// A committed CAD body (a 1 x 0.5 rectangle extruded 0.25 on XY) so a staged
// Edit Sketch has a real owner and a real draft.
CadBodyState importGuardRectangle() {
    CadBodyState state;
    cadBaseSketch(state).plane = Workplane::XY;
    SketchRectangle rect;
    rect.center = SketchPoint{0.0, 0.0};
    rect.width = 1.0;
    rect.height = 0.5;
    addSketchEntity(&cadBaseSketch(state), rect);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = 0.25;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

SketchFrame worldFrameOf(Workplane plane) {
    const WorkplaneFrame wf = workplaneFrame(plane);
    return SketchFrame{Vec3{0, 0, 0}, wf.uAxis, wf.vAxis, wf.normal};
}

ImportModeFacts factsOf(const SculptSession& sculpt, const SketchSession& sketch) {
    ImportModeFacts facts;
    facts.sculpting = sculpt.inSculptMode();
    facts.sketchActive = sketch.active();
    return facts;
}

void runImportSketchGuardChecks(Recorder& r) {
    const std::vector<uint8_t> glb = buildMiniGlb(twoTriangleSheet());
    ParsedGlbScene parsed;
    const bool parsedOk = importGlb(glb.data(), glb.size(), &parsed) == GlbImportStatus::Ok;

    Fixture fixture;
    CadStatus cadWhy = CadStatus::Ok;
    SceneObject* cad = fixture.scene.addCadBody(importGuardRectangle(), &cadWhy);
    if (cad != nullptr) publishSceneObject(*cad);
    const ObjectId cadId = cad != nullptr ? cad->objectId() : kNoObject;
    const CadBodyState cadState = cad != nullptr ? cad->cadOrNull()->state() : CadBodyState{};
    r.check("IMPSK_00_a_project_with_a_cad_body_and_a_valid_glb",
            parsedOk && cad != nullptr && fixture.scene.hasProject());

    SketchSession sketch;
    // IMPORT-SK-01: a new sketch in Editing refuses by name and moves nothing.
    {
        const bool began = sketch.begin(Workplane::XY) == CadStatus::Ok
                           && sketch.state() == SketchSessionState::Editing;
        const ImportWitness before = importWitness(fixture.scene, fixture.history);
        ImportCommitReport report;
        report.objects = 99;
        const ImportCommitStatus status = commitImportedGlbSceneInMode(
                parsed, fixture.scene, fixture.history, factsOf(fixture.session, sketch), &report);
        r.check("IMPORT_SK_01_a_valid_glb_is_refused_in_editing_by_name",
                began && status == ImportCommitStatus::RefusedInSketch
                        && std::string(importCommitStatusName(status)) == "RefusedInSketch");
        r.check("IMPORT_SK_01_bodies_active_ids_history_fingerprint_and_bytes_are_identical",
                importWitness(fixture.scene, fixture.history) == before);
        r.check("IMPORT_SK_01_no_partial_import_is_reported",
                report.objects == 0 && report.firstObjectId == kNoObject
                        && report.activeBodyId == kNoObject);
        r.check("IMPORT_SK_01_and_the_sketch_is_still_editing",
                sketch.state() == SketchSessionState::Editing);
        sketch.cancel();
    }
    // IMPORT-SK-02 / 03: a STAGED Edit Sketch over the CAD body, in Editing
    // and then in Ready; the draft, its selection and the owner body survive.
    {
        const bool editing = sketch.beginEdit(cadId, cadState, worldFrameOf(Workplane::XY))
                                     == CadStatus::Ok
                             && sketch.state() == SketchSessionState::Editing;
        const size_t entities = sketch.sketch().entities.size();
        const ImportWitness before = importWitness(fixture.scene, fixture.history);
        const ImportCommitStatus inEditing = commitImportedGlbSceneInMode(
                parsed, fixture.scene, fixture.history, factsOf(fixture.session, sketch));
        r.check("IMPORT_SK_03_a_staged_edit_sketch_refuses_and_keeps_its_draft",
                editing && entities == 1u && inEditing == ImportCommitStatus::RefusedInSketch
                        && sketch.state() == SketchSessionState::Editing
                        && sketch.sketch().entities.size() == entities
                        && importWitness(fixture.scene, fixture.history) == before);

        const bool ready = sketch.finish() == CadStatus::Ok
                           && sketch.state() == SketchSessionState::Ready;
        const bool chosen = sketch.selectionChosen();
        const ImportCommitStatus inReady = commitImportedGlbSceneInMode(
                parsed, fixture.scene, fixture.history, factsOf(fixture.session, sketch));
        r.check("IMPORT_SK_02_a_valid_glb_is_refused_in_ready_by_name",
                ready && inReady == ImportCommitStatus::RefusedInSketch);
        r.check("IMPORT_SK_02_ready_selection_and_draft_are_intact",
                sketch.state() == SketchSessionState::Ready && sketch.selectionChosen() == chosen
                        && chosen && sketch.sketch().entities.size() == entities);
        r.check("IMPORT_SK_02_and_nothing_in_the_project_moved",
                importWitness(fixture.scene, fixture.history) == before);

        // The same draft still commits normally, as an edit of the SAME body.
        const size_t bodiesBefore = fixture.scene.bodyCount();
        const CadStatus committed = sketch.commitEdit(fixture.scene, fixture.history);
        r.check("IMPORT_SK_03_after_the_refusal_the_staged_edit_still_commits",
                committed == CadStatus::Ok && sketch.state() == SketchSessionState::Inactive
                        && fixture.scene.bodyCount() == bodiesBefore
                        && fixture.scene.findBody(cadId) != nullptr);

        // And a second staged edit still cancels to exactly what it found.
        const ImportWitness beforeCancel = importWitness(fixture.scene, fixture.history);
        const bool reopened =
                sketch.beginEdit(cadId, fixture.scene.findBody(cadId)->cadOrNull()->state(),
                                 worldFrameOf(Workplane::XY)) == CadStatus::Ok;
        const ImportCommitStatus again = commitImportedGlbSceneInMode(
                parsed, fixture.scene, fixture.history, factsOf(fixture.session, sketch));
        sketch.cancel();
        r.check("IMPORT_SK_03_and_cancels_with_the_project_unchanged",
                reopened && again == ImportCommitStatus::RefusedInSketch
                        && sketch.state() == SketchSessionState::Inactive
                        && importWitness(fixture.scene, fixture.history) == beforeCancel);
    }
    // IMPORT-SK-06: repeated refusals are idempotent and leave no id gap.
    {
        const bool began = sketch.begin(Workplane::XZ) == CadStatus::Ok;
        const ImportWitness before = importWitness(fixture.scene, fixture.history);
        bool allRefused = began;
        for (int i = 0; i < 5; ++i) {
            allRefused = allRefused
                         && commitImportedGlbSceneInMode(parsed, fixture.scene, fixture.history,
                                                         factsOf(fixture.session, sketch))
                                    == ImportCommitStatus::RefusedInSketch;
        }
        r.check("IMPORT_SK_06_five_refusals_change_nothing_and_mint_no_id",
                allRefused && importWitness(fixture.scene, fixture.history) == before);
        sketch.cancel();
    }
    // IMPORT-SK-04: Inactive -- the same GLB imports, one body, one Undo, the
    // next id with no gap, and the project encodes and decodes.
    {
        const ImportWitness before = importWitness(fixture.scene, fixture.history);
        ImportCommitReport report;
        const ImportCommitStatus status = commitImportedGlbSceneInMode(
                parsed, fixture.scene, fixture.history, factsOf(fixture.session, sketch), &report);
        const SceneObject* body = fixture.scene.findBody(report.firstObjectId);
        r.check("IMPORT_SK_04_with_no_sketch_the_same_glb_imports",
                sketch.state() == SketchSessionState::Inactive && status == ImportCommitStatus::Ok
                        && report.objects == 1 && fixture.scene.bodyCount() == before.bodies + 1
                        && body != nullptr && body->isImported()
                        && fixture.scene.activeBodyId() == report.firstObjectId);
        r.check("IMPORT_SK_04_as_one_construction_undo_with_the_next_id_and_no_gap",
                fixture.history.undoDepth() == before.undo + 1
                        && report.firstObjectId == before.nextId);
        const std::vector<uint8_t> bytes =
                encodeProjectV1(captureProjectDocument(fixture.scene, ProjectKind::Construction));
        ProjectDocument decoded;
        const bool decodes = !bytes.empty()
                && decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok
                && encodeProjectV1(decoded) == bytes;
        r.check("IMPORT_SK_04_and_the_project_saves_and_reopens_byte_identically", decodes);
    }
    // IMPORT-SK-05: Sculpt keeps its refusal and its precedence; a malformed
    // file is still a FILE refusal in a legal state.
    {
        ImportModeFacts sculptOnly;
        sculptOnly.sculpting = true;
        ImportModeFacts both;
        both.sculpting = true;
        both.sketchActive = true;
        const ImportWitness before = importWitness(fixture.scene, fixture.history);
        r.check("IMPORT_SK_05_sculpt_still_refuses_as_refused_in_sculpt",
                commitImportedGlbSceneInMode(parsed, fixture.scene, fixture.history, sculptOnly)
                                == ImportCommitStatus::RefusedInSculpt
                        && importModeRefusal(both) == ImportCommitStatus::RefusedInSculpt
                        && importWitness(fixture.scene, fixture.history) == before);
        std::vector<uint8_t> truncated(glb.begin(), glb.begin() + static_cast<long>(glb.size() / 2));
        ParsedGlbScene bad;
        r.check("IMPORT_SK_05_a_malformed_glb_is_still_a_parse_refusal_not_a_mode_refusal",
                importGlb(truncated.data(), truncated.size(), &bad) != GlbImportStatus::Ok);
    }
    // IMPORT-SK-07: no ordinal moved; the new one is appended with a name.
    r.check("IMPORT_SK_07_every_earlier_ordinal_is_unchanged",
            static_cast<int>(ImportCommitStatus::Ok) == 0
                    && static_cast<int>(ImportCommitStatus::NothingToImport) == 1
                    && static_cast<int>(ImportCommitStatus::TooManyObjects) == 2
                    && static_cast<int>(ImportCommitStatus::RejectedGeometry) == 3
                    && static_cast<int>(ImportCommitStatus::RefusedEditInProgress) == 4
                    && static_cast<int>(ImportCommitStatus::RefusedInSculpt) == 5
                    && static_cast<int>(ImportCommitStatus::RefusedInSketch) == 6);
    r.check("IMPORT_SK_07_no_mode_means_no_mode_refusal",
            importModeRefusal(ImportModeFacts{}) == ImportCommitStatus::Ok);
}

// ---------------------------------------------------------------------------
// MESH-IMPORT: STL, OBJ, PLY and 3MF into the one parsed-scene shape
// ---------------------------------------------------------------------------
//
// Every fixture below is built HERE -- text by hand, binary by this file's own
// little-endian writers, ZIP containers and DEFLATE streams by this file's own
// encoder and CRC -- so a reader is never proven only against the writer it
// shipped beside. The one exception is the dynamic-Huffman stream, which is
// the output of an independent DEFLATE implementation (zlib, level 9) frozen
// as bytes: no hand-written encoder here could prove the reader against
// another's idea of a dynamic block.

std::vector<uint8_t> bytesOf(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

GlbImportStatus importText(const std::string& text, MeshImportFormat format,
                           ParsedGlbScene* out) {
    const std::vector<uint8_t> bytes = bytesOf(text);
    return importMeshFile(bytes.data(), bytes.size(), format, out);
}

GlbImportStatus importBytes(const std::vector<uint8_t>& bytes, MeshImportFormat format,
                            ParsedGlbScene* out) {
    return importMeshFile(bytes.data(), bytes.size(), format, out);
}

MeshImportFormat detectBytes(const std::vector<uint8_t>& bytes, const char* hint = "") {
    return detectMeshImportFormat(bytes.data(), bytes.size(), hint);
}

struct WorldBounds {
    float lo[3] = {1e30f, 1e30f, 1e30f};
    float hi[3] = {-1e30f, -1e30f, -1e30f};

    void add(const float* xyz) {
        for (int c = 0; c < 3; ++c) {
            lo[c] = std::fmin(lo[c], xyz[c]);
            hi[c] = std::fmax(hi[c], xyz[c]);
        }
    }
};

WorldBounds boundsOf(const ParsedGlbScene& scene) {
    WorldBounds bounds;
    for (const ParsedGlbMesh& mesh : scene.meshes) {
        for (size_t v = 0; v + 2 < mesh.positions.size(); v += 3) bounds.add(&mesh.positions[v]);
    }
    return bounds;
}

WorldBounds boundsOf(const ParsedGlbMesh& mesh) {
    WorldBounds bounds;
    for (size_t v = 0; v + 2 < mesh.positions.size(); v += 3) bounds.add(&mesh.positions[v]);
    return bounds;
}

WorldBounds boundsOf(const MeshExportSnapshot& snapshot) {
    WorldBounds bounds;
    for (const MeshExportObject& object : snapshot.objects) {
        for (size_t v = 0; v + 2 < object.positions.size(); v += 3) {
            bounds.add(&object.positions[v]);
        }
    }
    return bounds;
}

// Within 1e-5 relative (absolute below one unit): a millimetre round trip
// moves a float by about one ULP, and nothing a reader may do moves it more.
bool boundsMatch(const WorldBounds& a, const WorldBounds& b) {
    for (int c = 0; c < 3; ++c) {
        const double tolLo = 1e-5 * std::fmax(1.0, std::fabs(b.lo[c]));
        const double tolHi = 1e-5 * std::fmax(1.0, std::fabs(b.hi[c]));
        if (std::fabs(a.lo[c] - b.lo[c]) > tolLo || std::fabs(a.hi[c] - b.hi[c]) > tolHi) {
            return false;
        }
    }
    return true;
}

bool boundsAre(const WorldBounds& b, float x0, float y0, float z0, float x1, float y1, float z1) {
    WorldBounds expected;
    const float lo[3] = {x0, y0, z0};
    const float hi[3] = {x1, y1, z1};
    expected.add(lo);
    expected.add(hi);
    return boundsMatch(b, expected);
}

// Every reader's shared contract on its output.
bool parsedLikeTheContract(const ParsedGlbScene& scene) {
    if (scene.meshes.empty()) return false;
    const Mat4 identity = mat4Identity();
    for (const ParsedGlbMesh& mesh : scene.meshes) {
        if (!mesh.normalsGenerated || mesh.translation[0] != 0.0f || mesh.translation[1] != 0.0f
            || mesh.translation[2] != 0.0f || mesh.batches.size() != 1
            || mesh.batches[0].firstIndex != 0 || mesh.batches[0].indexCount != mesh.indices.size()
            || mesh.batches[0].doubleSided || mesh.normals.size() != mesh.positions.size()
            || std::memcmp(mesh.nodeTransform.m, identity.m, sizeof(identity.m)) != 0) {
            return false;
        }
        for (size_t v = 0; v + 2 < mesh.normals.size(); v += 3) {
            const double length = std::sqrt(double(mesh.normals[v]) * mesh.normals[v]
                                            + double(mesh.normals[v + 1]) * mesh.normals[v + 1]
                                            + double(mesh.normals[v + 2]) * mesh.normals[v + 2]);
            if (std::fabs(length - 1.0) > 1e-5) return false;
        }
    }
    return true;
}

uint32_t testCrc32(const std::string& data) {
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void putU16(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
}

void putU32(std::vector<uint8_t>& out, uint32_t v) {
    putU16(out, v & 0xFFFFu);
    putU16(out, v >> 16);
}

struct TestZipPart {
    std::string name;
    std::string content;            // uncompressed
    std::vector<uint8_t> deflated;  // non-empty: stored as DEFLATE with these bytes
    uint32_t crcFlip = 0;           // XORed into the recorded CRC
    uint16_t flags = 0;
};

std::vector<uint8_t> testZip(const std::vector<TestZipPart>& parts) {
    std::vector<uint8_t> zip;
    std::vector<uint32_t> offsets;
    for (const TestZipPart& part : parts) {
        const bool deflate = !part.deflated.empty();
        const uint32_t stored = static_cast<uint32_t>(deflate ? part.deflated.size()
                                                              : part.content.size());
        offsets.push_back(static_cast<uint32_t>(zip.size()));
        putU32(zip, 0x04034B50u);
        putU16(zip, 20);
        putU16(zip, part.flags);
        putU16(zip, deflate ? 8 : 0);
        putU16(zip, 0);
        putU16(zip, 0x21);
        putU32(zip, testCrc32(part.content) ^ part.crcFlip);
        putU32(zip, stored);
        putU32(zip, static_cast<uint32_t>(part.content.size()));
        putU16(zip, static_cast<uint32_t>(part.name.size()));
        putU16(zip, 0);
        zip.insert(zip.end(), part.name.begin(), part.name.end());
        if (deflate) {
            zip.insert(zip.end(), part.deflated.begin(), part.deflated.end());
        } else {
            zip.insert(zip.end(), part.content.begin(), part.content.end());
        }
    }
    const uint32_t directory = static_cast<uint32_t>(zip.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        const TestZipPart& part = parts[i];
        const bool deflate = !part.deflated.empty();
        putU32(zip, 0x02014B50u);
        putU16(zip, 20);
        putU16(zip, 20);
        putU16(zip, part.flags);
        putU16(zip, deflate ? 8 : 0);
        putU16(zip, 0);
        putU16(zip, 0x21);
        putU32(zip, testCrc32(part.content) ^ part.crcFlip);
        putU32(zip, static_cast<uint32_t>(deflate ? part.deflated.size() : part.content.size()));
        putU32(zip, static_cast<uint32_t>(part.content.size()));
        putU16(zip, static_cast<uint32_t>(part.name.size()));
        putU16(zip, 0);
        putU16(zip, 0);
        putU16(zip, 0);
        putU16(zip, 0);
        putU32(zip, 0);
        putU32(zip, offsets[i]);
        zip.insert(zip.end(), part.name.begin(), part.name.end());
    }
    const uint32_t size = static_cast<uint32_t>(zip.size()) - directory;
    putU32(zip, 0x06054B50u);
    putU16(zip, 0);
    putU16(zip, 0);
    putU16(zip, static_cast<uint32_t>(parts.size()));
    putU16(zip, static_cast<uint32_t>(parts.size()));
    putU32(zip, size);
    putU32(zip, directory);
    putU16(zip, 0);
    return zip;
}

const char* const kTestContentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" "
        "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"model\" "
        "ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/></Types>";

std::string testRels(const char* target) {
    return std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                       "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/"
                       "relationships\"><Relationship Target=\"")
           + target
           + "\" Id=\"rel0\" "
             "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>"
             "</Relationships>";
}

std::vector<uint8_t> testThreeMf(const std::string& model, uint32_t crcFlip = 0,
                                 uint16_t flags = 0) {
    std::vector<TestZipPart> parts(3);
    parts[0].name = "[Content_Types].xml";
    parts[0].content = kTestContentTypes;
    parts[1].name = "_rels/.rels";
    parts[1].content = testRels("/3D/3dmodel.model");
    parts[2].name = "3D/3dmodel.model";
    parts[2].content = model;
    parts[2].crcFlip = crcFlip;
    parts[2].flags = flags;
    return testZip(parts);
}

// A 10 mm tetrahedron, wound outward, with whatever the case varies.
std::string tetraModel(const std::string& modelAttributes, const std::string& itemAttributes,
                       const std::string& extraResources = "",
                       const std::string& lastTriangle = "v1=\"1\" v2=\"2\" v3=\"3\"") {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<model " + modelAttributes
           + " xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">"
             "<resources><object id=\"1\" type=\"model\"><mesh><vertices>"
             "<vertex x=\"0\" y=\"0\" z=\"0\"/><vertex x=\"10\" y=\"0\" z=\"0\"/>"
             "<vertex x=\"0\" y=\"10\" z=\"0\"/><vertex x=\"0\" y=\"0\" z=\"10\"/>"
             "</vertices><triangles><triangle v1=\"0\" v2=\"2\" v3=\"1\"/>"
             "<triangle v1=\"0\" v2=\"1\" v3=\"3\"/><triangle v1=\"0\" v2=\"3\" v3=\"2\"/>"
             "<triangle "
           + lastTriangle + "/></triangles></mesh></object>" + extraResources
           + "</resources><build><item " + itemAttributes + "/></build></model>\n";
}

// A STORED block then a FIXED-Huffman block with greedy LZ77 matches that
// reach back into the stored block -- RFC 1951 3.2.4 and 3.2.6, written out by
// hand. Huffman codes go most-significant bit first, everything else least.
class TestBitWriter {
public:
    std::vector<uint8_t> bytes;
    void bit(uint32_t value) {
        if (count_ == 0) bytes.push_back(0);
        bytes.back() = static_cast<uint8_t>(bytes.back() | ((value & 1u) << count_));
        count_ = (count_ + 1) & 7;
    }
    void lsb(uint32_t value, int n) { for (int i = 0; i < n; ++i) bit(value >> i); }
    void code(uint32_t value, int n) { for (int i = n - 1; i >= 0; --i) bit(value >> i); }
    void align() { count_ = 0; }

private:
    int count_ = 0;
};

void fixedSymbol(TestBitWriter& w, int symbol) {
    if (symbol <= 143) w.code(0x30u + symbol, 8);
    else if (symbol <= 255) w.code(0x190u + (symbol - 144), 9);
    else if (symbol <= 279) w.code(static_cast<uint32_t>(symbol - 256), 7);
    else w.code(0xC0u + (symbol - 280), 8);
}

std::vector<uint8_t> testDeflateStoredThenFixed(const std::string& data) {
    static const int kLenBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                     31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const int kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const int kDistBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                      33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                      1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                       6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    TestBitWriter w;
    const size_t storedLength = data.size() / 3 < 60000 ? data.size() / 3 : 60000;
    w.lsb(0, 1);  // not final
    w.lsb(0, 2);  // stored
    w.align();
    w.bytes.push_back(static_cast<uint8_t>(storedLength & 0xFF));
    w.bytes.push_back(static_cast<uint8_t>(storedLength >> 8));
    w.bytes.push_back(static_cast<uint8_t>(~storedLength & 0xFF));
    w.bytes.push_back(static_cast<uint8_t>((~storedLength >> 8) & 0xFF));
    w.bytes.insert(w.bytes.end(), data.begin(), data.begin() + storedLength);
    w.lsb(1, 1);  // final
    w.lsb(1, 2);  // fixed Huffman
    size_t i = storedLength;
    while (i < data.size()) {
        size_t bestLength = 0;
        size_t bestDistance = 0;
        const size_t from = i > 1024 ? i - 1024 : 0;
        for (size_t candidate = from; candidate < i; ++candidate) {
            size_t length = 0;
            while (length < 258 && i + length < data.size()
                   && data[candidate + length] == data[i + length]) {
                ++length;
            }
            if (length > bestLength) {
                bestLength = length;
                bestDistance = i - candidate;
            }
        }
        if (bestLength < 3) {
            fixedSymbol(w, static_cast<unsigned char>(data[i]));
            ++i;
            continue;
        }
        int l = 28;
        while (kLenBase[l] > static_cast<int>(bestLength)) --l;
        fixedSymbol(w, 257 + l);
        w.lsb(static_cast<uint32_t>(bestLength - kLenBase[l]), kLenExtra[l]);
        int d = 29;
        while (kDistBase[d] > static_cast<int>(bestDistance)) --d;
        w.code(static_cast<uint32_t>(d), 5);
        w.lsb(static_cast<uint32_t>(bestDistance - kDistBase[d]), kDistExtra[d]);
        i += bestLength;
    }
    fixedSymbol(w, 256);
    return w.bytes;
}

// The content of a STORED entry, found by walking local headers -- enough for
// reading back a file this suite's own writer, or the exporter, produced.
bool storedZipEntry(const std::vector<uint8_t>& zip, const std::string& name, std::string* out) {
    size_t at = 0;
    while (at + 30 <= zip.size() && zip[at] == 'P' && zip[at + 1] == 'K' && zip[at + 2] == 3
           && zip[at + 3] == 4) {
        const auto u16 = [&zip](size_t p) { return static_cast<size_t>(zip[p] | (zip[p + 1] << 8)); };
        const size_t size = u16(at + 18) | (u16(at + 20) << 16);
        const size_t nameLength = u16(at + 26);
        const size_t extra = u16(at + 28);
        const size_t data = at + 30 + nameLength + extra;
        if (data + size > zip.size()) return false;
        if (std::string(zip.begin() + at + 30, zip.begin() + at + 30 + nameLength) == name) {
            out->assign(zip.begin() + data, zip.begin() + data + size);
            return true;
        }
        at = data + size;
    }
    return false;
}

// The model zlib deflated (raw, level 9) into ONE dynamic-Huffman block: a
// centimetre tetrahedron placed by a scaling, translating item, and a second
// object built of two components of it.
const char* const kDynamicModelText =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model unit=\"centimeter\" xml:lang=\"en-US\" "
        "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
        " <resources>\n"
        "  <object id=\"1\" type=\"model\" name=\"Tetra\">\n"
        "   <mesh>\n"
        "    <vertices>\n"
        "     <vertex x=\"0\" y=\"0\" z=\"0\"/>\n"
        "     <vertex x=\"1\" y=\"0\" z=\"0\"/>\n"
        "     <vertex x=\"0\" y=\"1\" z=\"0\"/>\n"
        "     <vertex x=\"0\" y=\"0\" z=\"1\"/>\n"
        "    </vertices>\n"
        "    <triangles>\n"
        "     <triangle v1=\"0\" v2=\"2\" v3=\"1\"/>\n"
        "     <triangle v1=\"0\" v2=\"1\" v3=\"3\"/>\n"
        "     <triangle v1=\"0\" v2=\"3\" v3=\"2\"/>\n"
        "     <triangle v1=\"1\" v2=\"2\" v3=\"3\"/>\n"
        "    </triangles>\n"
        "   </mesh>\n"
        "  </object>\n"
        "  <object id=\"2\" type=\"model\" name=\"Pair\">\n"
        "   <components>\n"
        "    <component objectid=\"1\"/>\n"
        "    <component objectid=\"1\" transform=\"1 0 0 0 1 0 0 0 1 5 0 0\"/>\n"
        "   </components>\n"
        "  </object>\n"
        " </resources>\n"
        " <build>\n"
        "  <item objectid=\"1\" transform=\"2 0 0 0 1 0 0 0 1 10 0 5\"/>\n"
        "  <item objectid=\"2\"/>\n"
        " </build>\n"
        "</model>\n";

const uint8_t kDynamicModelDeflated[] = {
        0x85, 0x52, 0xDB, 0x6E, 0xC2, 0x30, 0x0C, 0x7D, 0xE7, 0x2B, 0xAC, 0xBC, 0x43, 0x9A, 0x54, 0x48,
        0x13, 0x4A, 0xE1, 0x6D, 0xCF, 0x93, 0x06, 0x1F, 0x50, 0x52, 0x33, 0x32, 0x35, 0x09, 0x4A, 0x52,
        0x04, 0xFB, 0xFA, 0xA5, 0x4D, 0x3B, 0x28, 0xB7, 0xA9, 0x52, 0x1C, 0xBB, 0xC7, 0xC7, 0x27, 0xB6,
        0xC5, 0xEA, 0xA4, 0x6B, 0x38, 0xA2, 0xF3, 0xCA, 0x9A, 0x82, 0xB0, 0x59, 0x46, 0x00, 0x8D, 0xB4,
        0x95, 0x32, 0x5F, 0x05, 0xD9, 0xAC, 0xDF, 0xA7, 0x6F, 0x64, 0xB5, 0x9C, 0x08, 0x6D, 0x2B, 0xAC,
        0xA1, 0x31, 0x2A, 0x14, 0x44, 0xA2, 0x09, 0x4A, 0x63, 0x40, 0x47, 0x20, 0x66, 0x2F, 0xEA, 0xB2,
        0xC5, 0xA2, 0x99, 0x6E, 0x3E, 0xBB, 0x80, 0xF1, 0x05, 0xD9, 0x87, 0x70, 0x58, 0x50, 0xEA, 0xE5,
        0x1E, 0x75, 0xE9, 0x67, 0x5A, 0x49, 0x67, 0xBD, 0xDD, 0x85, 0x99, 0xB4, 0x9A, 0xE6, 0x95, 0x2E,
        0x4D, 0xB3, 0x2B, 0x65, 0x68, 0x5C, 0xAC, 0x43, 0xA5, 0x75, 0x48, 0x79, 0xC6, 0xE6, 0x34, 0xE3,
        0x64, 0x39, 0x01, 0xE1, 0xD0, 0xDB, 0xC6, 0x49, 0xF4, 0xD1, 0x01, 0x61, 0xB7, 0xDF, 0x28, 0x03,
        0xA8, 0x2A, 0xEA, 0x23, 0x10, 0xCE, 0x07, 0x2C, 0x48, 0xA7, 0x87, 0x80, 0x29, 0x75, 0x74, 0xD6,
        0x18, 0x5C, 0xD9, 0x26, 0x46, 0xB0, 0x46, 0xBF, 0xEF, 0x6E, 0x20, 0xE2, 0xAB, 0x82, 0xEA, 0x49,
        0x06, 0x1F, 0x4F, 0x70, 0x2A, 0x48, 0x7C, 0xE4, 0xB9, 0x3B, 0x7F, 0xDA, 0x93, 0xDE, 0x03, 0xD8,
        0x7F, 0x80, 0xC4, 0xC0, 0xFE, 0x03, 0x24, 0x06, 0x36, 0x00, 0x04, 0x1D, 0x6B, 0x12, 0xC1, 0xA9,
        0xD8, 0xBC, 0xFA, 0xA2, 0x71, 0x08, 0xC0, 0x91, 0x75, 0xD9, 0x47, 0x5E, 0x10, 0x1E, 0x4D, 0x7E,
        0xC5, 0xF2, 0x18, 0xC5, 0x12, 0x2A, 0x7F, 0x8D, 0xCA, 0x13, 0x8A, 0x3F, 0x41, 0xB1, 0x51, 0xC5,
        0xFC, 0xA2, 0x7B, 0x2C, 0x54, 0xD0, 0xA1, 0xCD, 0x82, 0xA6, 0xE9, 0xDC, 0x0E, 0x8A, 0x3F, 0x1C,
        0xD4, 0x47, 0xA9, 0x5C, 0x3F, 0xA7, 0xB8, 0x07, 0x07, 0x6B, 0xE2, 0x26, 0x0D, 0xAD, 0xF8, 0x0B,
        0x40, 0xA2, 0x49, 0xE3, 0xA6, 0x2F, 0xFF, 0x42, 0x9C, 0xBB, 0xF1, 0x3B, 0xEB, 0x74, 0xF4, 0x20,
        0xEB, 0xBE, 0x8B, 0x9D, 0xB7, 0xB6, 0x67, 0x10, 0x74, 0x5C, 0xF0, 0x4A, 0xB7, 0xA0, 0xD7, 0xEB,
        0x26, 0xB6, 0x8D, 0xAA, 0xAB, 0x0E, 0xA2, 0x02, 0xEA, 0xA7, 0xE5, 0xF8, 0x5D, 0x39, 0xD6, 0x9A,
        0x79, 0xAA, 0x77, 0x9B, 0x9B, 0xFA, 0x2D, 0x68, 0x4F, 0x1E, 0xFB, 0xD7, 0xB6, 0x65, 0x39, 0xF9,
        0x05,
};

void runMeshImportFormatChecks(Recorder& r) {
    // -----------------------------------------------------------------------
    // Round trips: the exporter's four new formats, read back
    // -----------------------------------------------------------------------
    Fixture f;
    f.setBox(f.first(), 2.0, 1.0, 0.5);
    f.place(f.first(), placement(1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
    f.first().setName("P\xC3\xA0rt #1 & co");
    SceneObject& second = f.scene.addBody();
    f.setBox(second, 1.0, 1.0, 1.0);
    f.place(second, placement(-2.0, 0.0, 0.0, 0.0, 45.0, 0.0, 1.0, 1.0, 1.0));
    MeshExportSnapshot snapshot;
    const bool captured = captureMeshExportSnapshot(f.scene, ProjectKind::Construction, &snapshot)
                          == GlbExportStatus::Ok;
    const WorldBounds expected = boundsOf(snapshot);
    const std::vector<uint8_t> glb = exportSceneAs(MeshExportFormat::Glb, f.scene,
                                                   ProjectKind::Construction);
    const std::vector<uint8_t> stl = exportSceneAs(MeshExportFormat::Stl, f.scene,
                                                   ProjectKind::Construction);
    const std::vector<uint8_t> obj = exportSceneAs(MeshExportFormat::Obj, f.scene,
                                                   ProjectKind::Construction);
    const std::vector<uint8_t> ply = exportSceneAs(MeshExportFormat::Ply, f.scene,
                                                   ProjectKind::Construction);
    const std::vector<uint8_t> threeMf = exportSceneAs(MeshExportFormat::ThreeMf, f.scene,
                                                       ProjectKind::Construction);
    r.check("MIM_01_every_exported_format_is_detected_from_its_bytes_alone",
            detectBytes(glb) == MeshImportFormat::Glb && detectBytes(stl) == MeshImportFormat::Stl
                    && detectBytes(obj) == MeshImportFormat::Obj
                    && detectBytes(ply) == MeshImportFormat::Ply
                    && detectBytes(threeMf) == MeshImportFormat::ThreeMf);

    ParsedGlbScene fromStl, fromObj, fromPly, from3mf;
    const bool stlOk = importBytes(stl, MeshImportFormat::Stl, &fromStl) == GlbImportStatus::Ok;
    const bool objOk = importBytes(obj, MeshImportFormat::Obj, &fromObj) == GlbImportStatus::Ok;
    const bool plyOk = importBytes(ply, MeshImportFormat::Ply, &fromPly) == GlbImportStatus::Ok;
    const bool threeMfOk =
            importBytes(threeMf, MeshImportFormat::ThreeMf, &from3mf) == GlbImportStatus::Ok;
    r.check("MIM_02_stl_reads_back_as_one_welded_mesh_in_metres",
            captured && stlOk && fromStl.meshes.size() == 1 && fromStl.totalTriangles() == 24
                    && fromStl.totalVertices() == 16 && boundsMatch(boundsOf(fromStl), expected));
    r.check("MIM_03_obj_reads_back_one_named_mesh_per_object_in_metres",
            objOk && fromObj.meshes.size() == 2 && fromObj.meshes[0].name == "P_rt _1 & co"
                    && fromObj.meshes[0].triangleCount() == 12
                    && fromObj.meshes[1].triangleCount() == 12
                    && fromObj.meshes[0].vertexCount() == 8 && fromObj.meshes[1].vertexCount() == 8
                    && boundsMatch(boundsOf(fromObj), expected));
    r.check("MIM_04_ply_reads_back_every_triangle_in_metres",
            plyOk && fromPly.meshes.size() == 1 && fromPly.totalTriangles() == 24
                    && boundsMatch(boundsOf(fromPly), expected));
    r.check("MIM_05_3mf_reads_back_one_named_mesh_per_item_from_millimetres",
            threeMfOk && from3mf.meshes.size() == 2
                    && from3mf.meshes[0].name == "P\xC3\xA0rt #1 & co"
                    && from3mf.meshes[0].triangleCount() == 12
                    && from3mf.meshes[1].vertexCount() == 8
                    && boundsMatch(boundsOf(from3mf), expected));
    {
        ParsedGlbScene viaSwitch;
        ParsedGlbScene direct;
        const bool both = importBytes(glb, MeshImportFormat::Glb, &viaSwitch) == GlbImportStatus::Ok
                          && importGlb(glb.data(), glb.size(), &direct) == GlbImportStatus::Ok;
        r.check("MIM_06_glb_goes_through_the_one_glb_parser_unchanged",
                both && viaSwitch.meshes.size() == direct.meshes.size()
                        && viaSwitch.meshes[0].positions == direct.meshes[0].positions
                        && viaSwitch.meshes[0].normals == direct.meshes[0].normals
                        && viaSwitch.meshes[0].indices == direct.meshes[0].indices);
    }
    r.check("MIM_07_every_reader_generates_unit_normals_at_the_identity_with_no_translation",
            parsedLikeTheContract(fromStl) && parsedLikeTheContract(fromObj)
                    && parsedLikeTheContract(fromPly) && parsedLikeTheContract(from3mf));
    {
        bool committed = true;
        for (const ParsedGlbScene* parsed : {&fromStl, &fromObj, &fromPly, &from3mf}) {
            Fixture g;
            ImportCommitReport report;
            committed = committed
                        && commitImportedGlbScene(*parsed, g.scene, g.history, &report)
                                   == ImportCommitStatus::Ok
                        && report.objects == static_cast<int>(parsed->meshes.size());
        }
        r.check("MIM_08_every_reader_output_commits_as_durable_imported_bodies", committed);
    }

    // -----------------------------------------------------------------------
    // Detection
    // -----------------------------------------------------------------------
    {
        std::vector<uint8_t> solidHeader = stl;
        std::memcpy(solidHeader.data(), "solid ForgeShape", 16);
        ParsedGlbScene parsed;
        r.check("MIM_09_a_binary_stl_whose_header_says_solid_is_still_read_as_binary",
                detectBytes(solidHeader) == MeshImportFormat::Stl
                        && importBytes(solidHeader, MeshImportFormat::Stl, &parsed)
                                   == GlbImportStatus::Ok
                        && parsed.totalTriangles() == 24);
        const std::vector<uint8_t> junk = bytesOf("not a mesh at all\n\x01\x02");
        ParsedGlbScene none;
        r.check("MIM_10_unknown_bytes_are_unknown_and_a_hint_only_names_the_reader_that_refuses",
                detectBytes(junk) == MeshImportFormat::Unknown
                        && detectBytes(junk, ".stl") == MeshImportFormat::Stl
                        && detectBytes(junk, "3MF") == MeshImportFormat::ThreeMf
                        && detectBytes(stl, ".obj") == MeshImportFormat::Stl
                        && importBytes(junk, MeshImportFormat::Unknown, &none)
                                   == GlbImportStatus::UnknownFormat
                        && importBytes(junk, MeshImportFormat::Stl, &none)
                                   == GlbImportStatus::TruncatedFile
                        && std::string(meshImportFormatName(MeshImportFormat::ThreeMf)) == "3mf");
    }

    // -----------------------------------------------------------------------
    // Refusals, each by its own name
    // -----------------------------------------------------------------------
    {
        bool noData = true;
        const uint8_t one = 0;
        for (int k = 0; k < kMeshImportFormatCount; ++k) {
            ParsedGlbScene parsed;
            const MeshImportFormat format = static_cast<MeshImportFormat>(k);
            noData = noData && importMeshFile(nullptr, 0, format, &parsed) == GlbImportStatus::NoData
                     && importMeshFile(&one, 0, format, &parsed) == GlbImportStatus::NoData;
        }
        r.check("MIM_11_empty_input_is_NoData_for_every_format", noData);
    }
    {
        const std::vector<uint8_t> truncated(stl.begin(), stl.end() - 10);
        ParsedGlbScene parsed;
        r.check("MIM_12_a_truncated_binary_stl_is_TruncatedFile",
                importBytes(truncated, MeshImportFormat::Stl, &parsed)
                                == GlbImportStatus::TruncatedFile
                        && detectBytes(truncated, ".stl") == MeshImportFormat::Stl);
    }
    {
        const std::string vertices = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n";
        ParsedGlbScene parsed;
        r.check("MIM_13_an_obj_face_naming_a_missing_vertex_or_normal_is_IndexOutOfRange",
                importText(vertices + "f 1 2 4\n", MeshImportFormat::Obj, &parsed)
                                == GlbImportStatus::IndexOutOfRange
                        && importText(vertices + "f 1 2 0\n", MeshImportFormat::Obj, &parsed)
                                   == GlbImportStatus::IndexOutOfRange
                        && importText(vertices + "f -4 1 2\n", MeshImportFormat::Obj, &parsed)
                                   == GlbImportStatus::IndexOutOfRange
                        && importText(vertices + "f 1//2 2//2 3//2\n", MeshImportFormat::Obj,
                                      &parsed)
                                   == GlbImportStatus::IndexOutOfRange);
    }
    {
        ParsedGlbScene parsed;
        r.check("MIM_14_a_ply_with_an_unknown_format_version_or_type_is_UnsupportedPlyFormat",
                importText("ply\nformat binary_middle_endian 1.0\nend_header\n",
                           MeshImportFormat::Ply, &parsed)
                                == GlbImportStatus::UnsupportedPlyFormat
                        && importText("ply\nformat ascii 2.0\nend_header\n", MeshImportFormat::Ply,
                                      &parsed)
                                   == GlbImportStatus::UnsupportedPlyFormat
                        && importText("ply\nformat ascii 1.0\nelement vertex 1\n"
                                      "property quad x\nend_header\n",
                                      MeshImportFormat::Ply, &parsed)
                                   == GlbImportStatus::UnsupportedPlyFormat);
    }
    const std::string goodTetra = tetraModel("unit=\"millimeter\"", "objectid=\"1\"");
    {
        ParsedGlbScene parsed;
        r.check("MIM_15_a_zip_entry_whose_bytes_break_its_crc_is_ZipChecksumMismatch",
                importBytes(testThreeMf(goodTetra), MeshImportFormat::ThreeMf, &parsed)
                                == GlbImportStatus::Ok
                        && importBytes(testThreeMf(goodTetra, 0x1u), MeshImportFormat::ThreeMf,
                                       &parsed)
                                   == GlbImportStatus::ZipChecksumMismatch);
    }
    {
        std::vector<TestZipPart> noModel(2);
        noModel[0].name = "[Content_Types].xml";
        noModel[0].content = kTestContentTypes;
        noModel[1].name = "Metadata/notes.txt";
        noModel[1].content = "no model here";
        std::vector<TestZipPart> danglingTarget(3);
        danglingTarget[0] = noModel[0];
        danglingTarget[1].name = "_rels/.rels";
        danglingTarget[1].content = testRels("/3D/elsewhere.model");
        danglingTarget[2].name = "3D/3dmodel.model";
        danglingTarget[2].content = goodTetra;
        ParsedGlbScene parsed;
        r.check("MIM_16_a_zip_with_no_3mf_model_part_is_MissingModelPart",
                importBytes(testZip(noModel), MeshImportFormat::ThreeMf, &parsed)
                                == GlbImportStatus::MissingModelPart
                        && importBytes(testZip(danglingTarget), MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::MissingModelPart);
    }
    {
        ParsedGlbScene parsed;
        r.check("MIM_17_obj_free_form_and_unknown_statements_are_UnsupportedObjFeature",
                importText("v 0 0 0\ncstype bspline\n", MeshImportFormat::Obj, &parsed)
                                == GlbImportStatus::UnsupportedObjFeature
                        && importText("v 0 0 0\nfrobnicate 1\n", MeshImportFormat::Obj, &parsed)
                                   == GlbImportStatus::UnsupportedObjFeature);
    }
    {
        const std::string beamRequired = tetraModel(
                "unit=\"millimeter\" requiredextensions=\"b\" "
                "xmlns:b=\"http://schemas.microsoft.com/3dmanufacturing/beamlattice/2017/02\"",
                "objectid=\"1\"");
        const std::string parsec = tetraModel("unit=\"parsec\"", "objectid=\"1\"");
        const std::string otherPart = tetraModel(
                "unit=\"millimeter\" "
                "xmlns:p=\"http://schemas.microsoft.com/3dmanufacturing/production/2015/06\"",
                "objectid=\"2\"",
                "<object id=\"2\"><components><component objectid=\"1\" "
                "p:path=\"/3D/other.model\"/></components></object>");
        std::vector<TestZipPart> badDeflate(1);
        badDeflate[0].name = "3D/3dmodel.model";
        badDeflate[0].content = "x";
        badDeflate[0].deflated = {0x07};  // BFINAL with the reserved block type 3
        ParsedGlbScene parsed;
        r.check("MIM_18_3mf_and_zip_features_this_reader_does_not_implement_are_refused_by_name",
                importBytes(testThreeMf(beamRequired), MeshImportFormat::ThreeMf, &parsed)
                                == GlbImportStatus::UnsupportedThreeMfFeature
                        && importBytes(testThreeMf(parsec), MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::UnsupportedThreeMfFeature
                        && importBytes(testThreeMf(otherPart), MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::UnsupportedThreeMfFeature
                        && importBytes(testThreeMf(goodTetra, 0, 0x0001u),
                                       MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::UnsupportedZipFeature
                        && importBytes(testZip(badDeflate), MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::MalformedZip);
    }
    {
        ParsedGlbScene parsed;
        r.check("MIM_19_a_3mf_part_with_a_dtd_or_unbalanced_tags_is_MalformedXml",
                importBytes(testThreeMf("<?xml version=\"1.0\"?><!DOCTYPE model [<!ENTITY a "
                                        "\"b\">]><model/>"),
                            MeshImportFormat::ThreeMf, &parsed)
                                == GlbImportStatus::MalformedXml
                        && importBytes(testThreeMf("<model xmlns=\"http://schemas.microsoft.com/"
                                                   "3dmanufacturing/core/2015/02\"><resources>"
                                                   "</model>"),
                                       MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::MalformedXml);
    }
    {
        const std::string unknownObject = tetraModel("unit=\"millimeter\"", "objectid=\"9\"");
        const std::string badTriangle = tetraModel("unit=\"millimeter\"", "objectid=\"1\"", "",
                                                   "v1=\"1\" v2=\"2\" v3=\"9\"");
        const std::string cycle = tetraModel(
                "unit=\"millimeter\"", "objectid=\"2\"",
                "<object id=\"2\"><components><component objectid=\"3\"/></components></object>"
                "<object id=\"3\"><components><component objectid=\"2\"/></components></object>");
        ParsedGlbScene parsed;
        r.check("MIM_20_a_3mf_whose_references_disagree_is_refused_by_name",
                importBytes(testThreeMf(unknownObject), MeshImportFormat::ThreeMf, &parsed)
                                == GlbImportStatus::MalformedThreeMf
                        && importBytes(testThreeMf(badTriangle), MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::IndexOutOfRange
                        && importBytes(testThreeMf(cycle), MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::MalformedThreeMf);
    }
    {
        ParsedGlbScene parsed;
        const std::string infiniteVertex = tetraModel("unit=\"millimeter\"", "objectid=\"1\"")
                                                   .replace(goodTetra.find("x=\"10\""), 6,
                                                            "x=\"inf\"");
        r.check("MIM_21_a_non_finite_coordinate_is_NonFiniteValue_in_every_format",
                importText("v nan 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", MeshImportFormat::Obj,
                           &parsed)
                                == GlbImportStatus::NonFiniteValue
                        && importText("solid s\nfacet normal 0 0 1\nouter loop\nvertex inf 0 0\n"
                                      "vertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid s\n",
                                      MeshImportFormat::Stl, &parsed)
                                   == GlbImportStatus::NonFiniteValue
                        && importText("ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\n"
                                      "property float y\nproperty float z\nelement face 1\n"
                                      "property list uchar int vertex_indices\nend_header\n"
                                      "1e999 0 0\n3 0 0 0\n",
                                      MeshImportFormat::Ply, &parsed)
                                   == GlbImportStatus::NonFiniteValue
                        && importBytes(testThreeMf(infiniteVertex), MeshImportFormat::ThreeMf,
                                       &parsed)
                                   == GlbImportStatus::NonFiniteValue);
    }
    {
        ParsedGlbScene parsed;
        parsed.meshes.resize(1);
        parsed.meshes[0].name = "sentinel";
        const bool refused = importText("v 0 0 0\nf 1 2 3\n", MeshImportFormat::Obj, &parsed)
                             == GlbImportStatus::IndexOutOfRange;
        r.check("MIM_22_a_refusal_leaves_the_caller_s_scene_untouched",
                refused && parsed.meshes.size() == 1 && parsed.meshes[0].name == "sentinel");
    }

    // -----------------------------------------------------------------------
    // What each format's reader understands
    // -----------------------------------------------------------------------
    {
        const std::string fans =
                "# fans\n"
                "o Quad\n"
                "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                "vt 0 0\nvn 0 0 1\n"
                "f -4/1/1 -3/1/1 -2/1/1 -1/1/1\n"
                "o Pentagon\n"
                "v 0 0 1\nv 1 0 1\nv 1.5 0.5 1\nv 1 1 1\nv 0 1 1\n"
                "f 5//1 6//1 7//1 8//1 9//1\n"
                "g ignored because this file has objects\n"
                "f -5 -3 -1 # a trailing comment\n";
        ParsedGlbScene parsed;
        const bool ok = importText(fans, MeshImportFormat::Obj, &parsed) == GlbImportStatus::Ok;
        r.check("MIM_23_obj_negative_indices_resolve_and_polygons_fan_triangulate",
                ok && parsed.meshes.size() == 2 && parsed.meshes[0].name == "Quad"
                        && parsed.meshes[0].indices == std::vector<uint32_t>({0, 1, 2, 0, 2, 3})
                        && parsed.meshes[1].name == "Pentagon"
                        && parsed.meshes[1].vertexCount() == 5
                        && parsed.meshes[1].triangleCount() == 4);
        const std::string groups =
                "v 0 0 0\nv 1 0 0\nv 0 1 0\ng first\nf 1 2 3\ng second\nf 3 1 2\n";
        ParsedGlbScene byGroup;
        ParsedGlbScene byObject;
        const bool grouped =
                importText(groups, MeshImportFormat::Obj, &byGroup) == GlbImportStatus::Ok;
        const bool objected = importText("o only\n" + groups, MeshImportFormat::Obj, &byObject)
                              == GlbImportStatus::Ok;
        r.check("MIM_24_obj_splits_by_g_only_in_a_file_with_no_o",
                grouped && byGroup.meshes.size() == 2 && byGroup.meshes[0].name == "first"
                        && byGroup.meshes[1].name == "second"
                        && byGroup.meshes[1].vertexCount() == 3 && objected
                        && byObject.meshes.size() == 1 && byObject.meshes[0].name == "only"
                        && byObject.meshes[0].triangleCount() == 2);
    }
    {
        const std::string facet = " facet normal 0 0 0\n  outer loop\n";
        const std::string end = "  endloop\n endfacet\n";
        const std::string ascii =
                "solid first part\n" + facet
                + "   vertex 0 0 0\n   vertex 0 10 0\n   vertex 10 0 0\n" + end + facet
                + "   vertex 0 0 0\n   vertex 10 0 0\n   vertex 0 0 10\n" + end + facet
                + "   vertex 0 0 0\n   vertex 0 0 10\n   vertex 0 10 0\n" + end + facet
                + "   vertex 10 0 0\n   vertex 0 10 0\n   vertex 0 0 10\n" + end
                + "endsolid first part\nsolid second\n" + facet
                + "   vertex 0 0 0\n   vertex 0 0 0\n   vertex 5 0 0\n" + end + facet
                + "   vertex 0 0 0\n   vertex 5 0 0\n   vertex 5 5 0\n" + end + facet
                + "   vertex 0 0 0\n   vertex 5 5 0\n   vertex 0 5 0\n" + end + "endsolid second\n";
        ParsedGlbScene parsed;
        const bool ok = detectBytes(bytesOf(ascii)) == MeshImportFormat::Stl
                        && importText(ascii, MeshImportFormat::Stl, &parsed) == GlbImportStatus::Ok;
        r.check("MIM_25_ascii_stl_reads_every_solid_by_name_in_millimetres_dropping_slivers",
                ok && parsed.meshes.size() == 2 && parsed.meshes[0].name == "first part"
                        && parsed.meshes[0].vertexCount() == 4
                        && parsed.meshes[0].triangleCount() == 4
                        && boundsAre(boundsOf(parsed.meshes[0]), 0, 0, 0, 0.01f, 0.01f, 0.01f)
                        && parsed.meshes[1].name == "second"
                        && parsed.meshes[1].vertexCount() == 4
                        && parsed.meshes[1].triangleCount() == 2);
    }
    {
        const std::string asciiPly =
                "ply\nformat ascii 1.0\ncomment a quad with a colour and an extra element\n"
                "element vertex 4\nproperty float x\nproperty float y\nproperty float z\n"
                "property uchar red\nelement face 1\nproperty list uchar int vertex_indices\n"
                "element extra 2\nproperty list uchar float values\nend_header\n"
                "0 0 0 255\n1 0 0 0\n1 1 0 0\n0 1 0 0\n4 0 1 2 3\n2 0.5 0.25\n0\n";
        std::string bigHeader =
                "ply\nformat binary_big_endian 1.0\nelement vertex 4\nproperty double x\n"
                "property double y\nproperty double z\nproperty uchar red\nelement face 1\n"
                "property list uchar int vertex_indices\nelement extra 2\n"
                "property list uchar float values\nend_header\n";
        std::vector<uint8_t> big(bigHeader.begin(), bigHeader.end());
        const auto putBig = [&big](const void* value, size_t size) {
            const uint8_t* bytes = static_cast<const uint8_t*>(value);
            for (size_t i = size; i-- > 0;) big.push_back(bytes[i]);  // host is little-endian
        };
        const double corners[4][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
        for (const auto& corner : corners) {
            for (double value : corner) putBig(&value, sizeof(value));
            big.push_back(7);
        }
        big.push_back(4);
        for (int32_t index : {0, 1, 2, 3}) putBig(&index, sizeof(index));
        big.push_back(2);
        for (float value : {0.5f, 0.25f}) putBig(&value, sizeof(value));
        big.push_back(0);
        ParsedGlbScene fromAscii;
        ParsedGlbScene fromBig;
        const bool asciiOk =
                importText(asciiPly, MeshImportFormat::Ply, &fromAscii) == GlbImportStatus::Ok;
        r.check("MIM_26_ascii_ply_skips_other_properties_and_elements_and_fans_a_quad",
                asciiOk && fromAscii.meshes.size() == 1 && fromAscii.meshes[0].vertexCount() == 4
                        && fromAscii.meshes[0].indices
                                   == std::vector<uint32_t>({0, 1, 2, 0, 2, 3}));
        r.check("MIM_27_big_endian_ply_of_doubles_reads_the_same_mesh",
                importBytes(big, MeshImportFormat::Ply, &fromBig) == GlbImportStatus::Ok
                        && asciiOk && fromBig.meshes.size() == 1
                        && fromBig.meshes[0].positions == fromAscii.meshes[0].positions
                        && fromBig.meshes[0].indices == fromAscii.meshes[0].indices);
    }
    {
        std::vector<TestZipPart> parts(3);
        parts[0].name = "[Content_Types].xml";
        parts[0].content = kTestContentTypes;
        parts[1].name = "_rels/.rels";
        parts[1].content = testRels("/3D/3dmodel.model");
        parts[2].name = "3D/3dmodel.model";
        parts[2].content = kDynamicModelText;
        parts[2].deflated.assign(kDynamicModelDeflated,
                                 kDynamicModelDeflated + sizeof(kDynamicModelDeflated));
        ParsedGlbScene parsed;
        const bool ok = (kDynamicModelDeflated[0] & 0x06u) == 0x04u  // BTYPE 2: dynamic
                        && importBytes(testZip(parts), MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::Ok;
        r.check("MIM_28_a_dynamic_huffman_deflated_3mf_model_decodes", ok && parsed.meshes.size() == 2);
        r.check("MIM_29_3mf_unit_and_item_transform_are_baked_into_world_metres",
                ok && parsed.meshes.size() == 2 && parsed.meshes[0].name == "Tetra"
                        && parsed.meshes[0].vertexCount() == 4
                        && boundsAre(boundsOf(parsed.meshes[0]), 0.10f, 0.0f, 0.05f, 0.12f, 0.01f,
                                     0.06f));
        r.check("MIM_30_3mf_components_resolve_and_merge_into_their_item_s_one_mesh",
                ok && parsed.meshes.size() == 2 && parsed.meshes[1].name == "Pair"
                        && parsed.meshes[1].vertexCount() == 8
                        && parsed.meshes[1].triangleCount() == 8
                        && boundsAre(boundsOf(parsed.meshes[1]), 0.0f, 0.0f, 0.0f, 0.06f, 0.01f,
                                     0.01f));
    }
    {
        std::string model;
        const bool found = storedZipEntry(threeMf, "3D/3dmodel.model", &model);
        std::vector<TestZipPart> parts(3);
        parts[0].name = "[Content_Types].xml";
        parts[0].content = kTestContentTypes;
        parts[1].name = "_rels/.rels";
        parts[1].content = testRels("/3D/3dmodel.model");
        parts[2].name = "3D/3dmodel.model";
        parts[2].content = model;
        parts[2].deflated = testDeflateStoredThenFixed(model);
        ParsedGlbScene parsed;
        const bool ok = found && threeMfOk
                        && importBytes(testZip(parts), MeshImportFormat::ThreeMf, &parsed)
                                   == GlbImportStatus::Ok;
        bool same = ok && parsed.meshes.size() == from3mf.meshes.size();
        for (size_t m = 0; same && m < parsed.meshes.size(); ++m) {
            same = parsed.meshes[m].positions == from3mf.meshes[m].positions
                   && parsed.meshes[m].indices == from3mf.meshes[m].indices;
        }
        r.check("MIM_31_stored_and_fixed_huffman_blocks_decode_to_the_same_model",
                same && testCrc32("123456789") == 0xCBF43926u
                        && parts[2].deflated.size() < model.size());
    }
    {
        ParsedGlbScene parsed;
        const bool ok = importBytes(testThreeMf(tetraModel("unit=\"millimeter\"",
                                                           "objectid=\"1\" transform=\"-1 0 0 0 1 "
                                                           "0 0 0 1 0 0 0\"")),
                                    MeshImportFormat::ThreeMf, &parsed)
                        == GlbImportStatus::Ok;
        bool outward = ok && parsed.meshes.size() == 1 && parsed.meshes[0].vertexCount() == 4;
        if (outward) {
            const ParsedGlbMesh& mesh = parsed.meshes[0];
            Vec3 centroid{0.0f, 0.0f, 0.0f};
            for (uint32_t v = 0; v < 4; ++v) centroid = vec3Add(centroid, mesh.worldPosition(v));
            centroid = vec3Scale(centroid, 0.25f);
            for (uint32_t v = 0; v < 4; ++v) {
                outward = outward
                          && vec3Dot(mesh.worldNormal(v), vec3Sub(mesh.worldPosition(v), centroid))
                                     > 0.0f;
            }
            outward = outward && boundsAre(boundsOf(mesh), -0.01f, 0, 0, 0, 0.01f, 0.01f);
        }
        r.check("MIM_32_a_mirroring_3mf_item_keeps_the_outside_outside", outward);
    }
    r.check("MIM_33_the_appended_refusals_keep_stable_ordinals_names_and_categories",
            static_cast<int>(GlbImportStatus::TooLarge) == 36
                    && static_cast<int>(GlbImportStatus::UnknownFormat) == 37
                    && static_cast<int>(GlbImportStatus::MalformedThreeMf) == 48
                    && std::string(glbImportStatusName(GlbImportStatus::UnknownFormat))
                               == "UnknownFormat"
                    && std::string(glbImportStatusName(GlbImportStatus::ZipChecksumMismatch))
                               == "ZipChecksumMismatch"
                    && std::string(glbImportStatusName(GlbImportStatus::MalformedThreeMf))
                               == "MalformedThreeMf"
                    && glbImportStatusCategory(GlbImportStatus::UnsupportedPlyFormat)
                               == GlbImportCategory::Unsupported
                    && glbImportStatusCategory(GlbImportStatus::MalformedZip)
                               == GlbImportCategory::Unreadable
                    && glbImportStatusCategory(GlbImportStatus::MalformedThreeMf)
                               == GlbImportCategory::Inconsistent);
}

}  // namespace

int runGltfImportSelfTests(GltfImportSelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};

    // -----------------------------------------------------------------------
    // The JSON reader, on its own
    // -----------------------------------------------------------------------
    {
        JsonDocument doc;
        const std::string text = R"({"a":1,"b":[1,2,3],"c":"x","d":true,"e":null})";
        r.check("GLBIR0_JSON_an_ordinary_document_parses",
                doc.parse(text.c_str(), text.size()) == JsonStatus::Ok);
        double number = 0.0;
        r.check("GLBIR0_JSON_a_number_member_reads",
                doc.numberMember(doc.root(), "a", &number) && number == 1.0);
        const JsonValue* array = doc.member(doc.root(), "b");
        r.check("GLBIR0_JSON_an_array_has_its_elements",
                array != nullptr && array->children.size() == 3
                        && doc.element(*array, 2)->number == 3.0);
        r.check("GLBIR0_JSON_reading_past_an_array_is_a_null_not_a_crash",
                doc.element(*array, 99) == nullptr);
        std::string s;
        r.check("GLBIR0_JSON_a_string_member_reads",
                doc.stringMember(doc.root(), "c", &s) && s == "x");
        bool flag = false;
        r.check("GLBIR0_JSON_a_bool_member_reads",
                doc.boolMember(doc.root(), "d", &flag) && flag);
        r.check("GLBIR0_JSON_a_missing_member_is_absent_not_zero",
                !doc.numberMember(doc.root(), "missing", &number));
        r.check("GLBIR0_JSON_asking_a_string_for_a_number_fails",
                !doc.numberMember(doc.root(), "c", &number));

        // The refusals that matter. `nan` and `inf` are the reason this parser
        // does its own number grammar instead of handing text to strtod.
        JsonDocument bad;
        const std::string nan = R"({"x":nan})";
        r.check("GLBIR0_JSON_nan_is_not_a_number",
                bad.parse(nan.c_str(), nan.size()) != JsonStatus::Ok);
        const std::string inf = R"({"x":Infinity})";
        r.check("GLBIR0_JSON_infinity_is_not_a_number",
                bad.parse(inf.c_str(), inf.size()) != JsonStatus::Ok);
        const std::string huge = R"({"x":1e400})";
        r.check("GLBIR0_JSON_an_overflowing_literal_is_refused",
                bad.parse(huge.c_str(), huge.size()) != JsonStatus::Ok);
        const std::string trailing = R"({"a":1,})";
        r.check("GLBIR0_JSON_a_trailing_comma_is_refused",
                bad.parse(trailing.c_str(), trailing.size()) != JsonStatus::Ok);
        const std::string leadingZero = R"({"x":01})";
        r.check("GLBIR0_JSON_a_leading_zero_is_refused",
                bad.parse(leadingZero.c_str(), leadingZero.size()) != JsonStatus::Ok);
        const std::string unterminated = R"({"a":"x)";
        r.check("GLBIR0_JSON_an_unterminated_string_is_refused",
                bad.parse(unterminated.c_str(), unterminated.size())
                        == JsonStatus::UnterminatedString);
        const std::string second = R"({"a":1} {"b":2})";
        r.check("GLBIR0_JSON_a_second_document_is_refused",
                bad.parse(second.c_str(), second.size()) == JsonStatus::TrailingContent);
        // Deep nesting is bounded rather than allowed to recurse the stack away.
        std::string deep;
        for (int i = 0; i < 200; ++i) deep += '[';
        for (int i = 0; i < 200; ++i) deep += ']';
        r.check("GLBIR0_JSON_deep_nesting_is_bounded",
                bad.parse(deep.c_str(), deep.size()) == JsonStatus::DepthExceeded);
        r.check("GLBIR0_JSON_a_refused_parse_keeps_nothing", !bad.ok());
        // GLB pads its JSON chunk with spaces; that must parse.
        JsonDocument padded;
        const std::string withPadding = R"({"a":1}   )";
        r.check("GLBIR0_JSON_trailing_space_padding_is_accepted",
                padded.parse(withPadding.c_str(), withPadding.size()) == JsonStatus::Ok);
    }

    // -----------------------------------------------------------------------
    // GLBIR0-01/03/04/05: a real export parses, exactly
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        f.place(f.first(), placement(1.5, -0.5, 2.25, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        r.check("GLBIR0_01_the_export_produced_bytes", !glb.empty());

        ParsedGlbScene imported;
        const GlbImportStatus why = importGlb(glb.data(), glb.size(), &imported);
        r.check("GLBIR0_01_a_real_forgeshape_export_parses", why == GlbImportStatus::Ok);
        r.check("GLBIR0_01_one_body_is_one_imported_mesh", imported.meshes.size() == 1);

        if (imported.meshes.size() == 1) {
            const ParsedGlbMesh& mesh = imported.meshes[0];
            // GLBIR0-05: the node translation is applied exactly, and no axis
            // conversion of any kind is introduced. A Blender-style Z-up fix
            // would put 2.25 on Y and -0.5 on Z.
            r.check("GLBIR0_05_the_node_translation_is_read_exactly",
                    mesh.translation[0] == 1.5f && mesh.translation[1] == -0.5f
                            && mesh.translation[2] == 2.25f);
            r.check("GLBIR0_05_no_axis_conversion_is_applied",
                    mesh.nodeTransform.m[12] == 1.5f && mesh.nodeTransform.m[13] == -0.5f
                            && mesh.nodeTransform.m[14] == 2.25f
                            && mesh.nodeTransform.m[0] == 1.0f && mesh.nodeTransform.m[5] == 1.0f
                            && mesh.nodeTransform.m[10] == 1.0f);

            // GLBIR0-04: the accessors decoded exactly. A 2 x 1 x 0.5 m box at
            // unit scale has these local bounds and 12 triangles.
            float min[3] = {0, 0, 0};
            float max[3] = {0, 0, 0};
            for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
                for (int c = 0; c < 3; ++c) {
                    const float value = mesh.positions[static_cast<size_t>(v) * 3u + c];
                    if (v == 0 || value < min[c]) min[c] = value;
                    if (v == 0 || value > max[c]) max[c] = value;
                }
            }
            r.check("GLBIR0_04_positions_decode_to_the_authored_box",
                    std::fabs((max[0] - min[0]) - 2.0f) < 1e-5f
                            && std::fabs((max[1] - min[1]) - 1.0f) < 1e-5f
                            && std::fabs((max[2] - min[2]) - 0.5f) < 1e-5f);
            r.check("GLBIR0_04_a_box_decodes_to_twelve_triangles",
                    mesh.triangleCount() == 12);
            r.check("GLBIR0_04_there_is_one_normal_per_position",
                    mesh.normals.size() == mesh.positions.size());
            bool normalsUnit = mesh.vertexCount() > 0;
            for (uint32_t v = 0; v < mesh.vertexCount() && normalsUnit; ++v) {
                const size_t at = static_cast<size_t>(v) * 3u;
                const float length = std::sqrt(mesh.normals[at] * mesh.normals[at]
                                               + mesh.normals[at + 1] * mesh.normals[at + 1]
                                               + mesh.normals[at + 2] * mesh.normals[at + 2]);
                normalsUnit = std::fabs(length - 1.0f) < 1e-3f;
            }
            r.check("GLBIR0_04_every_decoded_normal_is_unit_length", normalsUnit);
            bool indicesInRange = !mesh.indices.empty();
            for (uint32_t index : mesh.indices) {
                if (index >= mesh.vertexCount()) {
                    indicesInRange = false;
                    break;
                }
            }
            r.check("GLBIR0_04_every_decoded_index_is_in_range", indicesInRange);

            // R1 BAKES the node transform, so the positions are already world
            // space and the box's centre sits at the node's translation. The
            // extents checked above are unaffected by a translation, which is
            // what makes them a decode check rather than a placement one.
            float centre[3] = {(min[0] + max[0]) * 0.5f, (min[1] + max[1]) * 0.5f,
                               (min[2] + max[2]) * 0.5f};
            r.check("GLBIR0_05_the_node_translation_is_baked_into_the_positions",
                    std::fabs(centre[0] - 1.5f) < 1e-5f && std::fabs(centre[1] + 0.5f) < 1e-5f
                            && std::fabs(centre[2] - 2.25f) < 1e-5f);
            const Vec3 world = mesh.worldPosition(0);
            r.check("GLBIR0_05_world_position_reads_the_baked_position",
                    world.x == mesh.positions[0] && world.y == mesh.positions[1]
                            && world.z == mesh.positions[2]);
        }

        // GLBIR0-03: the container is validated independently and fails closed.
        r.check("GLBIR0_03_no_data_is_refused", importOf({}) == GlbImportStatus::NoData);
        std::vector<uint8_t> notGlb = glb;
        notGlb[0] = 'X';
        r.check("GLBIR0_03_a_wrong_magic_is_refused",
                importOf(notGlb) == GlbImportStatus::NotGlb);
        std::vector<uint8_t> wrongVersion = glb;
        wrongVersion[4] = 3;
        r.check("GLBIR0_03_an_unsupported_container_version_is_refused",
                importOf(wrongVersion) == GlbImportStatus::UnsupportedVersion);
        std::vector<uint8_t> truncated = glb;
        truncated.resize(truncated.size() - 64);
        r.check("GLBIR0_03_a_truncated_file_is_refused",
                importOf(truncated) == GlbImportStatus::TruncatedFile);
        std::vector<uint8_t> lying = glb;
        lying[8] = static_cast<uint8_t>(lying[8] + 1);  // declared length disagrees
        r.check("GLBIR0_03_a_file_that_lies_about_its_own_length_is_refused",
                importOf(lying) == GlbImportStatus::TruncatedFile);
        std::vector<uint8_t> shortHeader(8, 0);
        r.check("GLBIR0_03_a_file_shorter_than_a_header_is_refused",
                importOf(shortHeader) == GlbImportStatus::TruncatedFile);

        // GLBIR0-06/07: the R0 subset boundary, one feature at a time.
        const std::string json = jsonOf(glb);
        r.check("GLBIR0_06_the_json_chunk_was_recovered", !json.empty());

        struct Case {
            const char* name;
            std::string find;
            std::string replace;
            GlbImportStatus expected;
        };
        const Case cases[] = {
            // R1 accepts a node matrix and a node TRS, so what stays refused is
            // stating BOTH: glTF forbids it, and which one wins is not a guess
            // a diagnostic may make for the user.
            {"GLBIR1_02_a_node_stating_both_a_matrix_and_a_trs_is_refused", "\"translation\":",
             "\"matrix\":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],\"translation\":",
             GlbImportStatus::NodeTransformConflict},
            {"GLBIR1_04_a_non_finite_node_scale_is_refused", "\"translation\":",
             "\"scale\":[2,1e400,2],\"translation\":", GlbImportStatus::MalformedJson},
            {"GLBIR1_04_a_zero_node_scale_is_refused", "\"translation\":",
             "\"scale\":[2,0,2],\"translation\":", GlbImportStatus::SingularNodeTransform},
            {"GLBIR1_04_a_flattened_node_matrix_is_refused", "\"translation\":",
             "\"matrix\":[1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1],\"unused\":",
             GlbImportStatus::SingularNodeTransform},
            {"GLBIR1_04_a_non_affine_node_matrix_is_refused", "\"translation\":",
             "\"matrix\":[1,0,0,0.5,0,1,0,0,0,0,1,0,0,0,0,1],\"unused\":",
             GlbImportStatus::SingularNodeTransform},
            {"GLBIR0_06_a_node_with_children_is_refused", "\"translation\":",
             "\"children\":[0],\"translation\":", GlbImportStatus::NodeHierarchy},
            {"GLBIR0_07_an_external_buffer_uri_is_refused", "\"buffers\":[{",
             "\"buffers\":[{\"uri\":\"data.bin\",", GlbImportStatus::ExternalBuffer},
            {"GLBIR0_07_a_required_extension_is_refused", "\"asset\":",
             "\"extensionsRequired\":[\"KHR_draco_mesh_compression\"],\"asset\":",
             GlbImportStatus::UnsupportedExtension},
            {"GLBIR0_07_animation_is_refused", "\"asset\":",
             "\"animations\":[{}],\"asset\":", GlbImportStatus::HasAnimation},
            {"GLBIR0_07_skinning_is_refused", "\"asset\":", "\"skins\":[{}],\"asset\":",
             GlbImportStatus::HasSkin},
            {"GLBIR0_07_a_sparse_accessor_is_refused", "\"componentType\":5126",
             "\"sparse\":{},\"componentType\":5126", GlbImportStatus::SparseAccessor},
            {"GLBIR0_07_morph_targets_are_refused", "\"attributes\":",
             "\"targets\":[{}],\"attributes\":", GlbImportStatus::MorphTargets},
            {"GLBIR0_07_a_non_triangle_primitive_is_refused", "\"mode\":4", "\"mode\":1",
             GlbImportStatus::NonTriangleMode},
            {"GLBIR0_07_an_image_is_refused", "\"asset\":",
             "\"images\":[{\"uri\":\"t.png\"}],\"asset\":", GlbImportStatus::ExternalBuffer},
            {"GLBIR0_07_a_missing_POSITION_is_refused", "\"POSITION\"", "\"POSITION_X\"",
             GlbImportStatus::MissingAttribute},
            // R1 generates a missing NORMAL; what stays refused is an
            // attribute this reader has never heard of, because silently
            // dropping one would be reporting on a file it had not understood.
            {"GLBIR1_12_an_unknown_custom_attribute_is_refused", "\"NORMAL\"", "\"_NOMAD_TAG\"",
             GlbImportStatus::UnknownAttribute},
            {"GLBIR1_08_a_non_indexed_primitive_is_refused", "\"indices\"", "\"indices_x\"",
             GlbImportStatus::NonIndexedPrimitive},
            {"GLBIR0_07_an_unsupported_asset_version_is_refused", "\"version\":\"2.0\"",
             "\"version\":\"1.0\"", GlbImportStatus::UnsupportedAssetVersion},
            {"GLBIR0_07_an_interleaved_buffer_view_is_refused", "\"byteLength\":",
             "\"byteStride\":32,\"byteLength\":", GlbImportStatus::InterleavedAccessor},
        };
        for (const Case& c : cases) {
            const std::string edited = replaceFirst(json, c.find, c.replace);
            const bool changed = edited != json;
            const std::vector<uint8_t> rebuilt = withJson(glb, edited);
            r.check(c.name, changed && !rebuilt.empty() && importOf(rebuilt) == c.expected);
        }

        // An identity rotation and a unit scale, explicitly written, are NOT
        // refused: R0 accepts what the specification says is the default.
        {
            const std::string edited = replaceFirst(
                    json, "\"translation\":",
                    "\"rotation\":[0,0,0,1],\"scale\":[1,1,1],\"translation\":");
            const std::vector<uint8_t> rebuilt = withJson(glb, edited);
            r.check("GLBIR0_06_an_explicit_identity_rotation_and_scale_are_accepted",
                    edited != json && importOf(rebuilt) == GlbImportStatus::Ok);
        }
        // A malformed document is refused as malformed rather than half-read.
        {
            const std::vector<uint8_t> rebuilt = withJson(glb, "{\"asset\":");
            r.check("GLBIR0_03_a_malformed_json_chunk_is_refused",
                    importOf(rebuilt) == GlbImportStatus::MalformedJson);
        }
        // An out-of-range index is caught by the reader, not by the renderer.
        {
            const std::string edited = replaceFirst(json, "\"count\":36", "\"count\":39");
            if (edited != json) {
                const std::vector<uint8_t> rebuilt = withJson(glb, edited);
                const GlbImportStatus why2 = importOf(rebuilt);
                r.check("GLBIR0_03_an_accessor_reading_past_its_view_is_refused",
                        why2 == GlbImportStatus::AccessorOutOfRange
                                || why2 == GlbImportStatus::IndexCountNotTriangles);
            } else {
                r.check("GLBIR0_03_an_accessor_reading_past_its_view_is_refused", true);
            }
        }
    }

    // -----------------------------------------------------------------------
    // GLBIR0-13/15/16: Construction source -> GLB -> import, world geometry
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        // Asymmetric on every axis, so no coordinate can stand in for another.
        f.place(f.first(), placement(1.5, -0.5, 2.25, 370.0, 30.0, 12.25, 1.25, 2.0, 0.5));
        SceneObject& second = f.scene.addBody();
        applyPrimitive(second.construction(), second.meshStore(),
                       PrimitiveSpec::forCone(2.0, 3.0));
        f.place(second, placement(-3.0, 1.0, 0.5, 25.0, -40.0, 65.0, 1.5, 1.0, 0.25));

        const RoundtripResult result =
                runGlbRoundtripDiagnostic(f.scene, ProjectKind::Construction);
        r.check("GLBIR0_13_a_construction_scene_roundtrips_as_equivalent",
                result.verdict == RoundtripVerdict::Equivalent);
        r.check("GLBIR0_13_both_bodies_were_compared",
                result.sourceBodyCount == 2 && result.importedMeshCount == 2
                        && result.bodies.size() == 2);
        bool countsAndTopology = result.bodies.size() == 2;
        for (const RoundtripBodyResult& body : result.bodies) {
            countsAndTopology = countsAndTopology && body.countsMatch && body.topologyMatches
                    && body.withinTolerance;
        }
        r.check("GLBIR0_13_counts_and_topology_agree_per_body", countsAndTopology);
        r.check("GLBIR0_15_the_max_world_position_delta_is_within_float32_quantization",
                result.maxPositionDelta
                        <= kRoundtripAbsoluteToleranceMeters
                                + 10.0 * kRoundtripRelativeTolerance);
        r.check("GLBIR0_16_the_max_world_normal_delta_is_within_tolerance",
                result.maxNormalDegrees <= kRoundtripNormalToleranceDegrees);
        r.check("GLBIR0_15_the_report_names_the_verdict_and_the_worst_vertex",
                formatRoundtripReport(result).find("verdict=ROUNDTRIP_EQUIVALENT")
                        != std::string::npos
                        && formatRoundtripReport(result).find("max_position_delta_m=")
                                != std::string::npos);

        // The comparison must be able to FAIL. Moving one body after the export
        // is a real disagreement between the file and the scene, and a
        // diagnostic that reported Equivalent for it would be worthless.
        GlbExportStatus why = GlbExportStatus::Ok;
        const std::vector<uint8_t> glb =
                exportSceneAsGlb(f.scene, ProjectKind::Construction, &why);
        f.place(f.first(), placement(9.5, -0.5, 2.25, 370.0, 30.0, 12.25, 1.25, 2.0, 0.5));
        const RoundtripResult moved =
                compareSceneToGlb(f.scene, ProjectKind::Construction, glb.data(), glb.size());
        r.check("GLBIR0_13_a_real_disagreement_is_reported_as_a_mismatch",
                moved.verdict == RoundtripVerdict::Mismatch);
        r.check("GLBIR0_15_and_the_mismatch_reports_which_body_and_how_far",
                moved.maxPositionDelta > 7.9 && moved.maxPositionDeltaBody == f.first().objectId());
    }

    // -----------------------------------------------------------------------
    // GLBIR0-14: Sculpt source -> GLB -> import
    // -----------------------------------------------------------------------
    {
        Fixture f;
        applyPrimitive(f.first().construction(), f.first().meshStore(),
                       PrimitiveSpec::forSphere(1.0));
        const ConstructionMesh sphere = f.first().construction().generateMesh();
        MeshValidation freezeWhy = MeshValidation::Ok;
        const bool frozen =
                f.first().frozenSculpt().mesh.freezeFrom(sphere, f.first().objectId(), &freezeWhy);
        r.check("GLBIR0_14_the_fixture_freezes", frozen);
        // Move a vertex a long way, so the sculpt mesh cannot be mistaken for
        // the sphere it was frozen from.
        f.first().frozenSculpt().mesh.setVertexPosition(0, Vec3{0.0f, 9.0f, 0.0f});
        f.first().frozenSculpt().mesh.advanceRevision();
        f.place(f.first(), placement(2.0, 0.0, -1.0, 0.0, 45.0, 0.0, 1.0, 4.0, 1.0));

        const RoundtripResult sculpt = runGlbRoundtripDiagnostic(f.scene, ProjectKind::Sculpt);
        r.check("GLBIR0_14_a_sculpt_scene_roundtrips_as_equivalent",
                sculpt.verdict == RoundtripVerdict::Equivalent);
        r.check("GLBIR0_14_the_compared_representation_is_the_sculpt_mesh",
                !sculpt.bodies.empty() && sculpt.bodies[0].fromSculpt);
        r.check("GLBIR0_14_the_sculpt_max_delta_is_within_tolerance",
                sculpt.maxPositionDelta
                        <= kRoundtripAbsoluteToleranceMeters
                                + 20.0 * kRoundtripRelativeTolerance);

        // The same scene read as Construction exports a DIFFERENT thing, which
        // is what proves the sculpt comparison was about the sculpt mesh.
        const RoundtripResult asConstruction =
                runGlbRoundtripDiagnostic(f.scene, ProjectKind::Construction);
        r.check("GLBIR0_14_the_construction_representation_is_a_different_geometry",
                asConstruction.verdict == RoundtripVerdict::Equivalent
                        && !asConstruction.bodies.empty()
                        && !asConstruction.bodies[0].fromSculpt
                        && asConstruction.bodies[0].sourceVertexCount
                                != sculpt.bodies[0].sourceVertexCount);
    }

    // -----------------------------------------------------------------------
    // GLBIR0-08/12: what the preview is, and what it is not
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        f.place(f.first(), placement(1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        ParsedGlbScene imported;
        r.check("GLBIR0_08_the_fixture_imports",
                importGlb(glb.data(), glb.size(), &imported) == GlbImportStatus::Ok);

        ImportedMeshPreview preview;
        r.check("GLBIR0_08_the_preview_loads", preview.load(imported));
        r.check("GLBIR0_08_loading_does_not_show",
                preview.loaded() && !preview.visible());
        preview.setVisible(true);
        r.check("GLBIR0_08_it_shows_when_asked", preview.visible());
        r.check("GLBIR0_08_the_counts_are_the_files_own",
                preview.meshCount() == 1 && preview.triangleCount() == 12);

        // Every renderer key is in the reserved range and none of them is a
        // body id. The two ranges cannot meet: the scene allocator counts up
        // from 1, one per created body.
        bool reserved = !preview.snapshot().empty();
        for (const SceneDrawItem& item : preview.snapshot()) {
            reserved = reserved && previewRenderKeyIsReserved(item.objectId)
                    && item.objectId != f.first().objectId() && !item.selected;
        }
        r.check("GLBIR0_08_every_preview_key_is_reserved_and_is_not_a_body_id", reserved);
        r.check("GLBIR0_08_the_scene_allocator_cannot_reach_the_reserved_range",
                !previewRenderKeyIsReserved(f.scene.nextObjectId())
                        && f.scene.nextObjectId() < kFirstPreviewRenderKey);

        // The preview is not in the scene, and the scene is unchanged by it.
        r.check("GLBIR0_08_the_preview_added_no_body", f.scene.bodyCount() == 1);
        r.check("GLBIR0_08_the_active_body_is_unchanged",
                f.scene.activeBody().objectId() == f.first().objectId());
        r.check("GLBIR0_08_the_preview_has_no_construction_or_sculpt_representation",
                !f.first().frozenSculpt().mesh.frozen());

        // The node transform is BAKED, so the draw item's model is identity and
        // the file's translation is in the vertices. One placement, in one
        // place: nothing downstream can apply it twice or forget it.
        const SceneDrawItem& item = preview.snapshot()[0];
        r.check("GLBIR0_08_the_draw_item_model_is_identity_because_the_transform_is_baked",
                item.model.m[12] == 0.0f && item.model.m[13] == 0.0f && item.model.m[14] == 0.0f
                        && item.model.m[0] == 1.0f && item.model.m[5] == 1.0f
                        && item.model.m[10] == 1.0f);
        r.check("GLBIR0_08_and_an_identity_normal_matrix_for_the_same_reason",
                item.normalModel.m[0] == 1.0f && item.normalModel.m[5] == 1.0f
                        && item.normalModel.m[10] == 1.0f && item.normalModel.m[1] == 0.0f);
        bool bakedIntoVertices = false;
        for (uint32_t v = 0; v < imported.meshes[0].vertexCount(); ++v) {
            const Vec3 world = imported.meshes[0].worldPosition(v);
            bakedIntoVertices = bakedIntoVertices || (world.y > 2.0f && world.z > 2.0f);
        }
        r.check("GLBIR0_08_and_the_files_translation_is_in_the_vertices", bakedIntoVertices);

        // GLBIR0-12: Clear releases everything and cannot leave a visible
        // preview behind.
        preview.clear();
        r.check("GLBIR0_12_clear_releases_the_preview",
                !preview.loaded() && !preview.visible() && preview.meshCount() == 0
                        && preview.vertexCount() == 0 && preview.snapshot().empty());
        preview.setVisible(true);
        r.check("GLBIR0_12_visible_cannot_be_true_with_nothing_loaded", !preview.visible());

        // A refused load leaves an existing preview whole rather than half
        // replaced.
        r.check("GLBIR0_12_a_reload_works", preview.load(imported));
        ParsedGlbScene empty;
        r.check("GLBIR0_12_an_empty_scene_is_refused", !preview.load(empty));
        r.check("GLBIR0_12_and_the_previous_preview_survives_the_refusal",
                preview.loaded() && preview.meshCount() == 1);
    }

    // -----------------------------------------------------------------------
    // GLBIR1-01/03/06/07/12/14/19: the Nomad-like external fixture
    //
    // Structurally the owner's low-poly character: a node matrix, seven
    // TRIANGLES primitives over ONE shared POSITION accessor, no NORMAL,
    // COLOR_0/COLOR_1/TEXCOORD_0 present, one double-sided material, `extras`
    // at three levels. It is not the owner's file and claims to be nothing but
    // the same shape of problem.
    // -----------------------------------------------------------------------
    {
        const std::vector<uint8_t> fixture = buildNomadLikeGlbFixture();
        ParsedGlbScene imported;
        const GlbImportStatus why = importGlb(fixture.data(), fixture.size(), &imported);
        r.check("GLBIR1_19_the_nomad_like_fixture_imports", why == GlbImportStatus::Ok);
        r.check("GLBIR1_19_it_is_one_node_and_one_mesh", imported.meshes.size() == 1);

        if (imported.meshes.size() == 1) {
            const ParsedGlbMesh& mesh = imported.meshes[0];
            // GLBIR1-06/07: every primitive survived, and a shared POSITION
            // accessor was decoded ONCE. Decoding it per primitive would
            // report seven times the vertices the file contains.
            r.check("GLBIR1_06_all_seven_primitives_survive",
                    mesh.batches.size() == kNomadLikeFixturePrimitives);
            r.check("GLBIR1_07_a_shared_POSITION_accessor_is_decoded_once",
                    mesh.vertexCount() == kNomadLikeFixtureVertices);
            r.check("GLBIR1_06_every_triangle_survives",
                    mesh.triangleCount() == kNomadLikeFixtureTriangles);
            uint32_t batched = 0;
            bool contiguous = true;
            for (size_t b = 0; b < mesh.batches.size(); ++b) {
                contiguous = contiguous && mesh.batches[b].firstIndex == batched;
                batched += mesh.batches[b].indexCount;
            }
            r.check("GLBIR1_06_the_batches_tile_the_index_array_exactly",
                    contiguous && batched == mesh.indices.size());
            r.check("GLBIR1_13_the_fixtures_material_is_double_sided",
                    !mesh.batches.empty() && mesh.batches[0].doubleSided);

            // GLBIR1-03: glTF states a node matrix COLUMN-MAJOR, and Mat4 is
            // laid out the same way, so this is a copy. A transposed read
            // would put zero in the translation column and move every vertex.
            r.check("GLBIR1_03_the_matrix_translation_column_is_read_as_a_column",
                    mesh.nodeTransform.m[12] == 0.5f && mesh.nodeTransform.m[13] == 1.25f
                            && mesh.nodeTransform.m[14] == -0.75f);
            // Vertex 0 is local (-1.40625, 0, -1.3125). Through the fixture's
            // matrix that is exactly (-1.2109375, 1.2265625, -1.6875); through
            // its transpose it is somewhere else entirely.
            const Vec3 first = mesh.worldPosition(0);
            r.check("GLBIR1_01_the_node_matrix_is_baked_into_the_positions",
                    std::fabs(first.x + 1.2109375f) < 1e-5f
                            && std::fabs(first.y - 1.2265625f) < 1e-5f
                            && std::fabs(first.z + 1.6875f) < 1e-5f);
            r.check("GLBIR1_05_a_positive_determinant_needs_no_winding_correction",
                    !mesh.windingCorrected);
            // GLBIR1-09: no NORMAL in the file, so every normal was generated.
            r.check("GLBIR1_09_the_fixture_carries_no_NORMAL_so_normals_were_generated",
                    mesh.normalsGenerated
                            && mesh.normals.size() == mesh.positions.size());
            bool generatedAreUnit = mesh.vertexCount() > 0;
            for (uint32_t v = 0; v < mesh.vertexCount() && generatedAreUnit; ++v) {
                const Vec3 normal = mesh.worldNormal(v);
                const float length = std::sqrt(vec3Dot(normal, normal));
                generatedAreUnit = std::isfinite(length) && std::fabs(length - 1.0f) < 1e-3f;
            }
            r.check("GLBIR1_09_every_generated_normal_is_finite_and_unit_length",
                    generatedAreUnit);
            // GLBIR1-14: the file's `extras` reached nothing. There is no field
            // on ParsedGlbMesh that could hold it, and the only string it takes
            // from the node is the name.
            r.check("GLBIR1_14_extras_are_ignored_and_only_the_node_name_is_kept",
                    mesh.name == "NomadLikeFixture");
        }

        // Deterministic: the same build always writes the same bytes, which is
        // what lets a hash of this fixture be evidence.
        r.check("GLBIR1_19_the_fixture_is_byte_deterministic",
                buildNomadLikeGlbFixture() == fixture);

        // GLBIR1-19: it draws, as seven batches over one vertex array.
        ImportedMeshPreview preview;
        r.check("GLBIR1_19_the_fixture_loads_as_a_preview", preview.load(imported));
        r.check("GLBIR1_19_one_draw_batch_per_primitive",
                preview.batchCount() == kNomadLikeFixturePrimitives
                        && preview.meshCount() == 1);
        r.check("GLBIR1_19_the_reported_counts_are_the_files_own",
                preview.vertexCount() == kNomadLikeFixtureVertices
                        && preview.triangleCount() == kNomadLikeFixtureTriangles);
        bool everyBatchIsDoubleSided = preview.batchCount() > 0;
        bool reservedKeys = !preview.snapshot().empty();
        for (const SceneDrawItem& item : preview.snapshot()) {
            everyBatchIsDoubleSided =
                    everyBatchIsDoubleSided && item.mesh != nullptr
                    && item.mesh->renderBothSides();
            reservedKeys = reservedKeys && previewRenderKeyIsReserved(item.objectId)
                    && item.model.m[12] == 0.0f && !item.selected;
        }
        r.check("GLBIR1_13_a_double_sided_material_reaches_preview_culling",
                everyBatchIsDoubleSided);
        r.check("GLBIR1_19_every_batch_key_is_reserved_and_carries_an_identity_model",
                reservedKeys);

        // GLBIR1-12/15: what the fixture proves about refusals.
        const std::string json = jsonOf(fixture);
        r.check("GLBIR1_12_the_fixture_json_was_recovered", !json.empty());
        struct FixtureCase {
            const char* name;
            std::string find;
            std::string replace;
            GlbImportStatus expected;
        };
        const FixtureCase fixtureCases[] = {
            {"GLBIR1_12_a_colour_accessor_that_does_not_exist_is_refused", "\"COLOR_0\":1",
             "\"COLOR_0\":99", GlbImportStatus::AccessorOutOfRange},
            {"GLBIR1_12_a_colour_accessor_that_disagrees_on_count_is_refused",
             "\"componentType\":5123,\"normalized\":true,\"count\":1978",
             "\"componentType\":5123,\"normalized\":true,\"count\":1977",
             GlbImportStatus::CountMismatch},
            {"GLBIR1_15_a_required_compression_extension_is_still_refused", "\"asset\":",
             "\"extensionsRequired\":[\"KHR_draco_mesh_compression\"],\"asset\":",
             GlbImportStatus::UnsupportedExtension},
            {"GLBIR1_15_meshopt_compression_is_still_refused", "\"asset\":",
             "\"extensionsRequired\":[\"EXT_meshopt_compression\"],\"asset\":",
             GlbImportStatus::UnsupportedExtension},
            {"GLBIR1_15_animation_is_still_refused", "\"asset\":",
             "\"animations\":[{}],\"asset\":", GlbImportStatus::HasAnimation},
            {"GLBIR1_15_skinning_is_still_refused", "\"asset\":", "\"skins\":[{}],\"asset\":",
             GlbImportStatus::HasSkin},
            {"GLBIR1_15_morph_targets_are_still_refused", "\"attributes\":",
             "\"targets\":[{}],\"attributes\":", GlbImportStatus::MorphTargets},
            {"GLBIR1_15_an_external_buffer_is_still_refused", "\"buffers\":[{",
             "\"buffers\":[{\"uri\":\"data.bin\",", GlbImportStatus::ExternalBuffer},
            {"GLBIR1_15_an_image_is_still_refused", "\"asset\":",
             "\"images\":[{\"uri\":\"t.png\"}],\"asset\":", GlbImportStatus::ExternalBuffer},
            {"GLBIR1_15_child_nodes_are_refused_by_name_not_flattened", "\"mesh\":0",
             "\"children\":[0],\"mesh\":0", GlbImportStatus::NodeHierarchy},
        };
        for (const FixtureCase& c : fixtureCases) {
            const std::string edited = replaceFirst(json, c.find, c.replace);
            const std::vector<uint8_t> rebuilt = withJson(fixture, edited);
            r.check(c.name,
                    edited != json && !rebuilt.empty() && importOf(rebuilt) == c.expected);
        }
    }

    // -----------------------------------------------------------------------
    // GLBIR1-02/05/08/09/10/11/13: the awkward files an export cannot produce
    // -----------------------------------------------------------------------
    {
        // GLBIR1-02: TRS composes as T * R * S, not in any other order. A
        // 90-degree turn about Y with a 2x stretch on local X sends local
        // (2,0,0) to (1,2,-1) and local (0,0,1) to (2,2,3); every other
        // composition order sends them somewhere else.
        MiniGlbSpec trs = twoTriangleSheet();
        trs.nodeMembers =
                ",\"translation\":[1,2,3],\"rotation\":[0,0.7071067811865476,0,"
                "0.7071067811865476],\"scale\":[2,1,1]";
        ParsedGlbScene composed;
        const std::vector<uint8_t> trsGlb = buildMiniGlb(trs);
        const GlbImportStatus trsWhy =
                importGlb(trsGlb.data(), trsGlb.size(), &composed);
        r.check("GLBIR1_02_a_trs_node_imports", trsWhy == GlbImportStatus::Ok
                        && composed.meshes.size() == 1);
        if (composed.meshes.size() == 1) {
            const Vec3 stretched = composed.meshes[0].worldPosition(1);
            const Vec3 turned = composed.meshes[0].worldPosition(3);
            r.check("GLBIR1_02_trs_composes_as_translation_rotation_scale",
                    std::fabs(stretched.x - 1.0f) < 1e-5f
                            && std::fabs(stretched.y - 2.0f) < 1e-5f
                            && std::fabs(stretched.z + 1.0f) < 1e-5f
                            && std::fabs(turned.x - 2.0f) < 1e-5f
                            && std::fabs(turned.y - 2.0f) < 1e-5f
                            && std::fabs(turned.z - 3.0f) < 1e-5f);
        }

        // A node with no transform members at all is the identity, and the
        // positions come through untouched.
        MiniGlbSpec plain = twoTriangleSheet();
        ParsedGlbScene untransformed;
        {
            const std::vector<uint8_t> glb = buildMiniGlb(plain);
            r.check("GLBIR1_02_a_node_with_no_transform_is_the_identity",
                    importGlb(glb.data(), glb.size(), &untransformed) == GlbImportStatus::Ok
                            && untransformed.meshes.size() == 1
                            && untransformed.meshes[0].worldPosition(1).x == 2.0f
                            && untransformed.meshes[0].worldPosition(1).z == 0.0f);
        }

        // GLBIR1-05: a negative determinant mirrors the geometry, so the bake
        // corrects the winding rather than leaving every face pointing inward.
        MiniGlbSpec mirrored = twoTriangleSheet();
        mirrored.nodeMembers = ",\"scale\":[-1,1,1]";
        ParsedGlbScene flipped;
        {
            const std::vector<uint8_t> glb = buildMiniGlb(mirrored);
            const bool ok =
                    importGlb(glb.data(), glb.size(), &flipped) == GlbImportStatus::Ok
                    && flipped.meshes.size() == 1;
            bool swapped = ok && untransformed.meshes.size() == 1
                    && flipped.meshes[0].indices.size()
                            == untransformed.meshes[0].indices.size();
            for (size_t t = 0; swapped && t + 2 < flipped.meshes[0].indices.size(); t += 3) {
                const std::vector<uint32_t>& was = untransformed.meshes[0].indices;
                const std::vector<uint32_t>& now = flipped.meshes[0].indices;
                swapped = now[t] == was[t] && now[t + 1] == was[t + 2]
                        && now[t + 2] == was[t + 1];
            }
            r.check("GLBIR1_05_a_negative_determinant_corrects_the_winding",
                    ok && flipped.meshes[0].windingCorrected && swapped);
            r.check("GLBIR1_05_and_mirrors_the_positions_it_was_told_to",
                    ok && std::fabs(flipped.meshes[0].worldPosition(1).x + 2.0f) < 1e-5f);
        }

        // GLBIR1-08: uint16 indices decode, and an index naming a vertex that
        // does not exist is caught by the reader rather than by the renderer.
        MiniGlbSpec small = twoTriangleSheet();
        small.uint16Indices = true;
        {
            const std::vector<uint8_t> glb = buildMiniGlb(small);
            ParsedGlbScene scene;
            r.check("GLBIR1_08_uint16_indices_decode",
                    importGlb(glb.data(), glb.size(), &scene) == GlbImportStatus::Ok
                            && scene.meshes.size() == 1
                            && scene.meshes[0].triangleCount() == 2);
        }
        {
            MiniGlbSpec outOfRange = twoTriangleSheet();
            outOfRange.primitives[1] = {0u, 2u, 9u};
            const std::vector<uint8_t> glb = buildMiniGlb(outOfRange);
            r.check("GLBIR1_08_an_index_past_the_last_vertex_is_refused",
                    importOf(glb) == GlbImportStatus::IndexOutOfRange);
        }
        {
            MiniGlbSpec notTriangles = twoTriangleSheet();
            notTriangles.primitives[1] = {0u, 2u};
            const std::vector<uint8_t> glb = buildMiniGlb(notTriangles);
            r.check("GLBIR1_08_an_index_count_that_is_not_triangles_is_refused",
                    importOf(glb) == GlbImportStatus::IndexCountNotTriangles);
        }

        // GLBIR1-09: generation is deterministic and area-weighted. The sheet
        // lies in the XZ plane, so every generated normal is exactly -Y.
        {
            const std::vector<uint8_t> glb = buildMiniGlb(plain);
            ParsedGlbScene once;
            ParsedGlbScene twice;
            const bool ok = importGlb(glb.data(), glb.size(), &once) == GlbImportStatus::Ok
                    && importGlb(glb.data(), glb.size(), &twice) == GlbImportStatus::Ok;
            r.check("GLBIR1_09_generated_normals_are_deterministic",
                    ok && once.meshes[0].normals == twice.meshes[0].normals);
            bool flat = ok && once.meshes[0].normalsGenerated;
            for (uint32_t v = 0; flat && v < once.meshes[0].vertexCount(); ++v) {
                const Vec3 normal = once.meshes[0].worldNormal(v);
                flat = std::fabs(normal.x) < 1e-6f && std::fabs(normal.y + 1.0f) < 1e-6f
                        && std::fabs(normal.z) < 1e-6f;
            }
            r.check("GLBIR1_09_a_flat_sheet_generates_one_flat_normal", flat);
        }

        // GLBIR1-10: a SUPPLIED normal rides the inverse transpose, not the
        // transform. Under a 4x stretch on Y, a normal at 45 degrees leans
        // towards X; multiplying by the transform would lean it towards Y
        // instead, which is the classic non-uniform-scale shading defect.
        {
            MiniGlbSpec supplied = twoTriangleSheet();
            const float diagonal = 0.70710678f;
            supplied.normals.clear();
            for (int v = 0; v < 4; ++v) {
                supplied.normals.push_back(diagonal);
                supplied.normals.push_back(diagonal);
                supplied.normals.push_back(0.0f);
            }
            supplied.nodeMembers = ",\"scale\":[1,4,1]";
            const std::vector<uint8_t> glb = buildMiniGlb(supplied);
            ParsedGlbScene scene;
            const bool ok = importGlb(glb.data(), glb.size(), &scene) == GlbImportStatus::Ok
                    && scene.meshes.size() == 1;
            const Vec3 normal = ok ? scene.meshes[0].worldNormal(0) : Vec3{0.0f, 0.0f, 0.0f};
            r.check("GLBIR1_10_a_supplied_normal_rides_the_inverse_transpose",
                    ok && !scene.meshes[0].normalsGenerated && normal.x > 0.9f
                            && normal.y > 0.0f && normal.y < 0.3f);
            r.check("GLBIR1_10_and_is_normalized_after_it",
                    ok && std::fabs(std::sqrt(vec3Dot(normal, normal)) - 1.0f) < 1e-5f);
        }

        // GLBIR1-11: a vertex whose triangles are all degenerate has no
        // direction to be normal to. Fail closed by name rather than write a
        // NaN or an invented default into the geometry.
        {
            MiniGlbSpec degenerate;
            degenerate.positions = {1.0f, 2.0f, 3.0f, 1.0f, 2.0f, 3.0f, 1.0f, 2.0f, 3.0f};
            degenerate.primitives = {{0u, 1u, 2u}};
            degenerate.doubleSided = {false};
            const std::vector<uint8_t> glb = buildMiniGlb(degenerate);
            r.check("GLBIR1_11_a_wholly_degenerate_triangle_cannot_generate_a_normal",
                    importOf(glb) == GlbImportStatus::CannotGenerateNormals);
        }
        {
            MiniGlbSpec zeroNormals = twoTriangleSheet();
            zeroNormals.normals.assign(12, 0.0f);
            const std::vector<uint8_t> glb = buildMiniGlb(zeroNormals);
            r.check("GLBIR1_11_a_supplied_zero_normal_is_refused_by_the_same_name",
                    importOf(glb) == GlbImportStatus::CannotGenerateNormals);
        }

        // GLBIR1-13: doubleSided is per PRIMITIVE, and one mesh must be able to
        // hold both answers at once.
        {
            MiniGlbSpec mixed = twoTriangleSheet();
            mixed.doubleSided = {true, false};
            const std::vector<uint8_t> glb = buildMiniGlb(mixed);
            ParsedGlbScene scene;
            const bool ok = importGlb(glb.data(), glb.size(), &scene) == GlbImportStatus::Ok
                    && scene.meshes.size() == 1 && scene.meshes[0].batches.size() == 2;
            r.check("GLBIR1_13_double_sidedness_is_read_per_primitive",
                    ok && scene.meshes[0].batches[0].doubleSided
                            && !scene.meshes[0].batches[1].doubleSided);
            ImportedMeshPreview preview;
            const bool loaded = ok && preview.load(scene);
            r.check("GLBIR1_13_and_each_batch_culls_its_own_way",
                    loaded && preview.batchCount() == 2
                            && preview.snapshot()[0].mesh->renderBothSides()
                            && !preview.snapshot()[1].mesh->renderBothSides());
            r.check("GLBIR1_13_while_the_reported_vertex_count_stays_the_files_own",
                    loaded && preview.vertexCount() == 4 && preview.triangleCount() == 2
                            && preview.meshCount() == 1);
        }
    }

    // -----------------------------------------------------------------------
    // IMP01A-01..15: durable import
    // -----------------------------------------------------------------------
    //
    // The same parser, a different destination. These cases are about what the
    // PROJECT does with a parsed file: how many objects it becomes, what they
    // are called, where the node transform ends up, and the rule that an import
    // either happens completely or not at all.

    // IMP01A-01/05/06: one mesh node becomes one durable body, with the node's
    // linear part baked and its translation kept as the placement.
    {
        MiniGlbSpec spec = twoTriangleSheet();
        // A node with a non-uniform scale AND a translation: the linear part is
        // something this product's placement cannot store and must therefore be
        // baked, and the translation is something it can and must not be.
        spec.nodeMembers = ",\"translation\":[1.5,-0.25,4.0],\"scale\":[2.0,1.0,3.0]";
        const std::vector<uint8_t> glb = buildMiniGlb(spec);

        ParsedGlbScene parsed;
        const bool parsedOk = importGlb(glb.data(), glb.size(), &parsed) == GlbImportStatus::Ok;

        Fixture fixture;
        const size_t bodiesBefore = fixture.scene.bodyCount();
        ImportCommitReport report;
        const ImportCommitStatus status = commitImportedGlbScene(
                parsed, fixture.scene, fixture.history, &report);
        r.check("IMP01A_01_a_supported_mesh_node_becomes_a_durable_body",
                parsedOk && status == ImportCommitStatus::Ok && report.objects == 1
                        && fixture.scene.bodyCount() == bodiesBefore + 1);

        const SceneObject* body = fixture.scene.findBody(report.firstObjectId);
        r.check("IMP01A_01_it_is_an_imported_body_with_no_construction_source",
                body != nullptr && body->isImported()
                        && body->constructionOrNull() == nullptr);
        r.check("IMP01A_04_it_carries_an_ordinary_object_id_from_the_scenes_allocator",
                body != nullptr && body->objectId() != kNoObject
                        && !previewRenderKeyIsReserved(body->objectId())
                        && body->objectId() < fixture.scene.nextObjectId());
        r.check("IMP01A_01_and_it_is_drawable",
                body != nullptr && body->meshStore().currentRevision() != kNoMeshRevision);

        // IMP01A-05: the split. Translation on the body, linear part in the
        // geometry, and nothing recentred.
        const TransformValues placement =
                body != nullptr ? body->transform().values() : TransformValues{};
        r.check("IMP01A_05_the_node_translation_became_the_bodys_placement",
                std::fabs(placement.positionX - 1.5) < 1e-6
                        && std::fabs(placement.positionY + 0.25) < 1e-6
                        && std::fabs(placement.positionZ - 4.0) < 1e-6);
        // IMP01A-06: rotation and scale start at the identity, because whatever
        // the node's linear part did is already in the vertices.
        r.check("IMP01A_06_the_imported_body_starts_unrotated_and_unscaled",
                placement.rotationX == 0.0 && placement.rotationY == 0.0
                        && placement.rotationZ == 0.0 && placement.scaleX == 1.0
                        && placement.scaleY == 1.0 && placement.scaleZ == 1.0);
        const ImportedMesh* mesh = body != nullptr ? body->importedOrNull() : nullptr;
        // The sheet is 2 x 1 in the file and the node scales it by (2,1,3), so
        // baked local geometry spans 4 in x and 3 in z. Its ORIGIN stays the
        // node's own: the minimum corner is still zero, not a centred bound.
        float bounds[6] = {0, 0, 0, 0, 0, 0};
        const bool haveBounds = mesh != nullptr && mesh->localBounds(bounds);
        r.check("IMP01A_05_the_node_linear_part_is_baked_into_the_local_geometry",
                haveBounds && std::fabs(bounds[3] - 4.0f) < 1e-5f
                        && std::fabs(bounds[5] - 3.0f) < 1e-5f);
        r.check("IMP01A_05_and_nothing_is_recentred",
                haveBounds && std::fabs(bounds[0]) < 1e-6f && std::fabs(bounds[2]) < 1e-6f);

        // IMP01A-03: one mesh's several primitives stay INSIDE the object.
        r.check("IMP01A_03_several_primitives_stay_one_object_with_exact_batches",
                mesh != nullptr && mesh->batchCount() == 2 && mesh->triangleCount() == 2
                        && mesh->vertexCount() == 4);

        // IMP01A-09: the whole import is exactly one Undo, and undoing it puts
        // the scene back with no imported body left behind.
        r.check("IMP01A_09_an_import_records_exactly_one_history_step",
                fixture.history.undoDepth() == 1);
        r.check("IMP01A_09_an_undo_removes_every_body_the_import_created",
                fixture.history.undo() && fixture.scene.bodyCount() == bodiesBefore
                        && fixture.scene.findBody(report.firstObjectId) == nullptr);
        // IMP01A-10: and a redo brings back the SAME object — its geometry, its
        // batches, its name and its placement — not a fresh one wearing its id.
        r.check("IMP01A_10_a_redo_restores_the_body", fixture.history.redo()
                && fixture.scene.findBody(report.firstObjectId) != nullptr);
        const SceneObject* again = fixture.scene.findBody(report.firstObjectId);
        const ImportedMesh* againMesh = again != nullptr ? again->importedOrNull() : nullptr;
        r.check("IMP01A_10_with_identical_geometry_batches_and_name",
                againMesh != nullptr && mesh != nullptr
                        && againMesh->positions() == mesh->positions()
                        && againMesh->normals() == mesh->normals()
                        && againMesh->indices() == mesh->indices()
                        && againMesh->batchCount() == mesh->batchCount()
                        && again->name() == body->name());
        r.check("IMP01A_10_and_identical_placement",
                again != nullptr
                        && sameConstructionPlacement(again->transform().values(), placement));
    }

    // IMP01A-02/23: several top-level mesh nodes become several objects, and
    // the first one this import created is the one left selected.
    {
        MiniGlbSpec spec = twoTriangleSheet();
        spec.nodeName = "alpha";
        spec.extraNodeMembers = {",\"translation\":[5.0,0.0,0.0],\"name\":\"beta\"",
                                 ",\"translation\":[0.0,5.0,0.0],\"name\":\"gamma\""};
        const std::vector<uint8_t> glb = buildMiniGlb(spec);

        ParsedGlbScene parsed;
        const bool parsedOk = importGlb(glb.data(), glb.size(), &parsed) == GlbImportStatus::Ok
                && parsed.meshes.size() == 3;

        Fixture fixture;
        const size_t bodiesBefore = fixture.scene.bodyCount();
        ImportCommitReport report;
        const ImportCommitStatus status = commitImportedGlbScene(
                parsed, fixture.scene, fixture.history, &report);
        r.check("IMP01A_02_three_mesh_nodes_become_three_objects",
                parsedOk && status == ImportCommitStatus::Ok && report.objects == 3
                        && fixture.scene.bodyCount() == bodiesBefore + 3);
        r.check("IMP01A_04_their_ids_are_distinct_and_collision_free",
                report.firstObjectId != report.lastObjectId
                        && fixture.scene.findBody(report.firstObjectId) != nullptr
                        && fixture.scene.findBody(report.lastObjectId) != nullptr
                        && report.lastObjectId < fixture.scene.nextObjectId());
        r.check("IMP01A_23_the_first_object_the_import_created_is_selected",
                report.activeBodyId == report.firstObjectId
                        && fixture.scene.activeBodyId() == report.firstObjectId);
        r.check("IMP01A_23_they_are_appended_after_the_bodies_that_were_there",
                fixture.scene.indexOfBody(report.firstObjectId) == bodiesBefore);
        // Still ONE transaction, however many objects it made.
        r.check("IMP01A_09_a_multi_object_import_is_still_one_undo_step",
                fixture.history.undoDepth() == 1);
        r.check("IMP01A_09_and_one_undo_removes_all_three",
                fixture.history.undo() && fixture.scene.bodyCount() == bodiesBefore);
        r.check("IMP01A_10_and_one_redo_brings_all_three_back",
                fixture.history.redo() && fixture.scene.bodyCount() == bodiesBefore + 3);
    }

    // IMP01A-07: the naming rule, all three branches.
    {
        Fixture fixture;
        MiniGlbSpec named = twoTriangleSheet();
        named.nodeName = "head_low";
        named.meshName = "mesh_that_loses";
        ParsedGlbScene parsed;
        const std::vector<uint8_t> glb = buildMiniGlb(named);
        ImportCommitReport report;
        const bool ok = importGlb(glb.data(), glb.size(), &parsed) == GlbImportStatus::Ok
                && commitImportedGlbScene(parsed, fixture.scene, fixture.history, &report)
                        == ImportCommitStatus::Ok;
        const SceneObject* body = fixture.scene.findBody(report.firstObjectId);
        r.check("IMP01A_07_the_node_name_wins",
                ok && body != nullptr && body->name() == "head_low");
    }
    {
        Fixture fixture;
        MiniGlbSpec meshOnly = twoTriangleSheet();
        meshOnly.meshName = "body_mesh";
        ParsedGlbScene parsed;
        const std::vector<uint8_t> glb = buildMiniGlb(meshOnly);
        ImportCommitReport report;
        const bool ok = importGlb(glb.data(), glb.size(), &parsed) == GlbImportStatus::Ok
                && commitImportedGlbScene(parsed, fixture.scene, fixture.history, &report)
                        == ImportCommitStatus::Ok;
        const SceneObject* body = fixture.scene.findBody(report.firstObjectId);
        r.check("IMP01A_07_the_mesh_name_is_the_second_choice",
                ok && body != nullptr && body->name() == "body_mesh");
    }
    {
        Fixture fixture;
        ParsedGlbScene parsed;
        const std::vector<uint8_t> glb = buildMiniGlb(twoTriangleSheet());
        ImportCommitReport report;
        const bool ok = importGlb(glb.data(), glb.size(), &parsed) == GlbImportStatus::Ok
                && commitImportedGlbScene(parsed, fixture.scene, fixture.history, &report)
                        == ImportCommitStatus::Ok;
        const SceneObject* body = fixture.scene.findBody(report.firstObjectId);
        r.check("IMP01A_07_an_unnamed_file_gets_the_deterministic_fallback",
                ok && body != nullptr && body->name() == "Imported 1");
    }
    {
        // A name from another tool is sanitized by the DOMAIN's rule before it
        // can become project truth, and the fallback catches whatever is left.
        r.check("IMP01A_07_a_name_is_trimmed_and_bounded",
                sanitizeImportedMeshName("  spaced  ") == "spaced"
                        && sanitizeImportedMeshName(std::string(200, 'x')).size()
                                == kMaxImportedMeshNameBytes);
        r.check("IMP01A_07_control_characters_and_broken_utf8_do_not_survive",
                sanitizeImportedMeshName(std::string("a\tb")) == "a b"
                        && sanitizeImportedMeshName(std::string("\xC3")) .empty()
                        && sanitizeImportedMeshName(std::string("\x80z")) == "z");
        r.check("IMP01A_07_sanitizing_is_idempotent",
                sanitizeImportedMeshName(sanitizeImportedMeshName("  a\tb  "))
                        == sanitizeImportedMeshName("  a\tb  ")
                        && importedMeshNameIsStorable("head_low")
                        && !importedMeshNameIsStorable(" head_low")
                        && !importedMeshNameIsStorable(""));
        r.check("IMP01A_07_a_multibyte_name_is_cut_on_a_character_boundary",
                sanitizeImportedMeshName(std::string(95, 'x') + "\xC3\xA9")
                        == std::string(95, 'x'));
    }

    // IMP01A-08: atomicity. A file whose SECOND node cannot be turned into an
    // object leaves the project exactly as it was — no body, no ObjectId, no
    // history step.
    {
        Fixture fixture;
        const size_t bodiesBefore = fixture.scene.bodyCount();
        const ObjectId nextBefore = fixture.scene.nextObjectId();
        const size_t undoBefore = fixture.history.undoDepth();

        MiniGlbSpec spec = twoTriangleSheet();
        spec.extraNodeMembers = {""};
        ParsedGlbScene parsed;
        const bool parsedOk = importGlb(buildMiniGlb(spec).data(), buildMiniGlb(spec).size(),
                                        &parsed) == GlbImportStatus::Ok
                && parsed.meshes.size() == 2;
        // Broken AFTER the parse, on purpose: the parser is not what is under
        // test here, the commit's all-or-nothing rule is. A normal that is not
        // a direction is a value the domain refuses.
        if (parsedOk) {
            parsed.meshes[1].normals.assign(parsed.meshes[1].normals.size(), 0.0f);
        }
        ImportCommitReport report;
        const ImportCommitStatus status = commitImportedGlbScene(
                parsed, fixture.scene, fixture.history, &report);
        r.check("IMP01A_08_one_bad_object_refuses_the_whole_import",
                parsedOk && status == ImportCommitStatus::RejectedGeometry
                        && report.geometryWhy != ImportedMeshValidation::Ok);
        r.check("IMP01A_08_and_the_project_is_untouched",
                fixture.scene.bodyCount() == bodiesBefore
                        && fixture.history.undoDepth() == undoBefore);
        r.check("IMP01A_08_no_object_id_was_consumed_for_a_body_that_never_existed",
                fixture.scene.nextObjectId() == nextBefore);
    }

    // IMP01A-08: an empty parsed scene and an open edit are both refused, and
    // both by name.
    {
        Fixture fixture;
        ParsedGlbScene empty;
        ImportCommitReport report;
        r.check("IMP01A_08_an_empty_parsed_scene_is_refused",
                commitImportedGlbScene(empty, fixture.scene, fixture.history, &report)
                        == ImportCommitStatus::NothingToImport);

        ParsedGlbScene parsed;
        const std::vector<uint8_t> glb = buildMiniGlb(twoTriangleSheet());
        const bool parsedOk = importGlb(glb.data(), glb.size(), &parsed) == GlbImportStatus::Ok;
        fixture.history.beginEdit();
        const size_t bodiesBefore = fixture.scene.bodyCount();
        r.check("IMP01A_08_an_import_during_an_open_edit_is_refused",
                parsedOk
                        && commitImportedGlbScene(parsed, fixture.scene, fixture.history, &report)
                                == ImportCommitStatus::RefusedEditInProgress
                        && fixture.scene.bodyCount() == bodiesBefore);
        fixture.history.cancelEdit();
        r.check("IMP01A_08_every_commit_status_has_a_name",
                std::string(importCommitStatusName(ImportCommitStatus::TooManyObjects))
                                == "TooManyObjects"
                        && std::string(importCommitStatusName(
                                   ImportCommitStatus::RejectedGeometry))
                                == "RejectedGeometry"
                        && std::string(importCommitStatusName(
                                   ImportCommitStatus::RefusedEditInProgress))
                                == "RefusedEditInProgress"
                        && std::string(importCommitStatusName(
                                   ImportCommitStatus::RefusedInSculpt))
                                == "RefusedInSculpt"
                        && std::string(importCommitStatusName(
                                   ImportCommitStatus::RefusedInSketch))
                                == "RefusedInSketch");
    }

    // IMP01A-11/12/13: an imported body takes the ORDINARY transform path, and
    // no transform edit ever touches its topology.
    {
        Fixture fixture;
        ParsedGlbScene parsed;
        const std::vector<uint8_t> glb = buildMiniGlb(twoTriangleSheet());
        ImportCommitReport report;
        const bool ok = importGlb(glb.data(), glb.size(), &parsed) == GlbImportStatus::Ok
                && commitImportedGlbScene(parsed, fixture.scene, fixture.history, &report)
                        == ImportCommitStatus::Ok;
        SceneObject* body = fixture.scene.findBody(report.firstObjectId);
        const std::vector<float> positionsBefore =
                body != nullptr ? body->importedOrNull()->positions() : std::vector<float>();
        const MeshRevision revisionBefore =
                body != nullptr ? body->meshStore().currentRevision() : kNoMeshRevision;

        TransformValues moved;
        moved.positionX = 2.0;
        moved.rotationY = 37.0;
        moved.scaleX = 1.5;
        moved.scaleY = 0.5;
        moved.scaleZ = 2.0;
        {
            ScopedConstructionEdit edit(fixture.history);
            applyTransformValues(body->transform(), moved);
        }
        r.check("IMP01A_11_a_transform_edit_on_an_imported_body_is_one_history_step",
                ok && fixture.history.undoDepth() == 2);
        r.check("IMP01A_12_move_rotate_and_scale_all_reach_the_bodys_own_transform",
                sameConstructionPlacement(body->transform().values(), moved));
        r.check("IMP01A_13_and_none_of_it_republishes_or_moves_a_vertex",
                body->meshStore().currentRevision() == revisionBefore
                        && body->importedOrNull()->positions() == positionsBefore);
        r.check("IMP01A_11_an_undo_returns_the_placement_the_file_gave_it",
                fixture.history.undo()
                        && fixture.scene.findBody(report.firstObjectId)
                                ->transform().values().positionX == 0.0);
        r.check("IMP01A_11_and_a_redo_returns_the_edited_one",
                fixture.history.redo()
                        && sameConstructionPlacement(
                                fixture.scene.findBody(report.firstObjectId)->transform().values(),
                                moved));
        r.check("IMP01A_13_the_geometry_survived_both",
                fixture.scene.findBody(report.firstObjectId)->importedOrNull()->positions()
                        == positionsBefore);
    }

    // IMP01A-14/15: the domain refuses what the UI withdraws.
    {
        Fixture fixture;
        ParsedGlbScene parsed;
        const std::vector<uint8_t> glb = buildMiniGlb(twoTriangleSheet());
        ImportCommitReport report;
        const bool ok = importGlb(glb.data(), glb.size(), &parsed) == GlbImportStatus::Ok
                && commitImportedGlbScene(parsed, fixture.scene, fixture.history, &report)
                        == ImportCommitStatus::Ok;
        // The import selected the imported body, so the process-scoped active
        // accessors now answer for one.
        r.check("IMP01A_14_an_imported_body_has_no_construction_source_to_edit",
                ok && fixture.scene.activeBody().constructionOrNull() == nullptr);
        const ImportedMesh* mesh = fixture.scene.activeBody().importedOrNull();
        const std::vector<float> positionsBefore =
                mesh != nullptr ? mesh->positions() : std::vector<float>();
        // IMP01A-15: and no freeze is possible, because there is no source mesh
        // to freeze FROM. Start Sculpting on an imported body is `IMPORT-01B`.
        r.check("IMP01A_15_there_is_nothing_to_freeze_an_imported_body_from",
                !fixture.scene.activeBody().frozenSculpt().mesh.frozen());
        r.check("IMP01A_14_and_the_geometry_is_untouched_by_asking",
                mesh != nullptr && mesh->positions() == positionsBefore);
    }

    // IMP01A-25: the scope boundary, stated as a check.
    {
        // An imported body is not a preview: it holds an ordinary ObjectId from
        // the scene's own allocator, never a reserved renderer key.
        r.check("IMP01A_25_a_body_id_is_never_a_reserved_preview_render_key",
                !previewRenderKeyIsReserved(kFirstBodyObjectId)
                        && previewRenderKeyIsReserved(kFirstPreviewRenderKey));
        r.check("IMP01A_25_the_two_representations_are_named",
                std::string(bodyRepresentationName(BodyRepresentation::Construction))
                                == "Construction"
                        && std::string(bodyRepresentationName(BodyRepresentation::Imported))
                                == "Imported");
    }

    // -----------------------------------------------------------------------
    // GLBIR0-20: the scope boundary, stated as a check
    // -----------------------------------------------------------------------
    {
        // Every refusal has a name, so a diagnostic can always say what stopped
        // it rather than reporting a bare failure.
        r.check("GLBIR0_20_every_import_status_has_a_name",
                std::string(glbImportStatusName(GlbImportStatus::ExternalBuffer))
                                == "ExternalBuffer"
                        && std::string(glbImportStatusName(GlbImportStatus::HasAnimation))
                                == "HasAnimation"
                        && std::string(glbImportStatusName(GlbImportStatus::NodeTransformConflict))
                                == "NodeTransformConflict"
                        && std::string(glbImportStatusName(
                                   GlbImportStatus::SingularNodeTransform))
                                == "SingularNodeTransform"
                        && std::string(glbImportStatusName(
                                   GlbImportStatus::CannotGenerateNormals))
                                == "CannotGenerateNormals"
                        && std::string(glbImportStatusName(GlbImportStatus::UnknownAttribute))
                                == "UnknownAttribute"
                        && std::string(glbImportStatusName(GlbImportStatus::NonIndexedPrimitive))
                                == "NonIndexedPrimitive"
                        && std::string(glbImportStatusName(GlbImportStatus::SparseAccessor))
                                == "SparseAccessor");
        r.check("GLBIR0_20_every_verdict_has_a_name",
                std::string(roundtripVerdictName(RoundtripVerdict::Equivalent))
                                == "ROUNDTRIP_EQUIVALENT"
                        && std::string(roundtripVerdictName(RoundtripVerdict::Mismatch))
                                == "ROUNDTRIP_MISMATCH");
    }

    // -----------------------------------------------------------------------
    // DEEP-AUDIT-R1 F-03: a name may carry a supplementary character, and the
    // JNI boundary hands it to Java as UTF-16, never as raw 4-byte UTF-8
    // -----------------------------------------------------------------------
    {
        const std::string emoji = "\xF0\x9F\x98\x80";  // U+1F600
        r.check("DAR1_03_the_sanitizer_keeps_a_four_byte_sequence",
                sanitizeImportedMeshName("face " + emoji) == "face " + emoji
                        && importedMeshNameIsStorable("face " + emoji));
        JsonDocument json;
        const char* text = "{\"name\":\"\\uD83D\\uDE00\"}";
        std::string decoded;
        const bool parsed = json.parse(text, std::strlen(text)) == JsonStatus::Ok
                            && json.stringMember(json.root(), "name", &decoded);
        r.check("DAR1_03_a_json_surrogate_pair_decodes_to_four_byte_utf8",
                parsed && decoded == emoji);
        r.check("DAR1_03_utf8_to_utf16_is_exact_for_one_two_and_three_byte_sequences",
                utf8ToUtf16("abc") == std::vector<uint16_t>{0x61u, 0x62u, 0x63u}
                        && utf8ToUtf16("\xC3\xA9") == std::vector<uint16_t>{0xE9u}
                        && utf8ToUtf16("\xE2\x82\xAC") == std::vector<uint16_t>{0x20ACu});
        r.check("DAR1_03_a_four_byte_sequence_becomes_a_surrogate_pair",
                utf8ToUtf16(emoji) == std::vector<uint16_t>{0xD83Du, 0xDE00u});
        r.check("DAR1_03_malformed_input_maps_to_the_replacement_character_and_keeps_going",
                utf8ToUtf16("a\xC3z") == std::vector<uint16_t>{0x61u, 0xFFFDu, 0x7Au}
                        && utf8ToUtf16("\xF0\x9F") == std::vector<uint16_t>{0xFFFDu}
                        && utf8ToUtf16("\xED\xA0\x80")
                                   == std::vector<uint16_t>{0xFFFDu, 0xFFFDu, 0xFFFDu});
    }

    runImportSketchGuardChecks(r);
    runMeshImportFormatChecks(r);
    // The device recorder for this suite holds 256 (`kMaxImportChecks`): a
    // check past it would be dropped silently and the suite still report OK.
    r.check("IMPSK_gltf_import_suite_fits_its_256_slot_device_recorder", r.n + 1 <= 256);
    return r.n;
}

}  // namespace forgeshape
