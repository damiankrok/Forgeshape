#include "forgeshape_gltf_export_selftest.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_history.h"
#include "forgeshape_math.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

struct Recorder {
    GltfExportSelfTestResult* out;
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

// ---------------------------------------------------------------------------
// An INDEPENDENT reader
// ---------------------------------------------------------------------------
//
// Deliberately not the writer's helpers. Reading a file back through the code
// that wrote it proves the two halves agree with each other and nothing about
// whether either agrees with glTF, so every byte below is decoded here from
// first principles: little-endian by hand, offsets computed from the header the
// file actually carries rather than from what the writer intended.
//
// The fuller check is the Java validator in the instrumentation, which parses
// the JSON with a real parser in another language entirely.

uint32_t readU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) {
        return 0;
    }
    uint32_t value = 0;
    for (int i = 3; i >= 0; --i) {
        value = (value << 8) | bytes[offset + static_cast<size_t>(i)];
    }
    return value;
}

float readF32(const std::vector<uint8_t>& bytes, size_t offset) {
    const uint32_t bits = readU32(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// The GLB envelope, as parsed from the bytes rather than as remembered.
struct ParsedGlb {
    bool ok = false;
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t declaredLength = 0;
    uint32_t jsonLength = 0;
    uint32_t jsonType = 0;
    size_t jsonStart = 0;
    uint32_t binLength = 0;
    uint32_t binType = 0;
    size_t binStart = 0;
    std::string json;
};

ParsedGlb parseGlb(const std::vector<uint8_t>& bytes) {
    ParsedGlb parsed;
    if (bytes.size() < 12 + 8) {
        return parsed;
    }
    parsed.magic = readU32(bytes, 0);
    parsed.version = readU32(bytes, 4);
    parsed.declaredLength = readU32(bytes, 8);
    parsed.jsonLength = readU32(bytes, 12);
    parsed.jsonType = readU32(bytes, 16);
    parsed.jsonStart = 20;
    if (parsed.jsonStart + parsed.jsonLength + 8 > bytes.size()) {
        return parsed;
    }
    parsed.json.assign(reinterpret_cast<const char*>(bytes.data() + parsed.jsonStart),
                       parsed.jsonLength);
    const size_t binHeader = parsed.jsonStart + parsed.jsonLength;
    parsed.binLength = readU32(bytes, binHeader);
    parsed.binType = readU32(bytes, binHeader + 4);
    parsed.binStart = binHeader + 8;
    if (parsed.binStart + parsed.binLength > bytes.size()) {
        return parsed;
    }
    parsed.ok = true;
    return parsed;
}

// Finds the numbers of the first JSON array that follows `key`.
//
// A targeted scan rather than a parser: the native side needs a handful of
// arrays, and a second full JSON implementation here would be a second thing to
// get wrong. The instrumentation's validator does the real parse.
bool readNumberArray(const std::string& json, const char* key, size_t from,
                     std::vector<double>* out, size_t* endOut = nullptr) {
    const size_t at = json.find(key, from);
    if (at == std::string::npos) {
        return false;
    }
    const size_t open = json.find('[', at);
    if (open == std::string::npos) {
        return false;
    }
    const size_t close = json.find(']', open);
    if (close == std::string::npos) {
        return false;
    }
    out->clear();
    const std::string body = json.substr(open + 1, close - open - 1);
    const char* cursor = body.c_str();
    while (*cursor != '\0') {
        char* next = nullptr;
        const double value = std::strtod(cursor, &next);
        if (next == cursor) {
            ++cursor;
            continue;
        }
        out->push_back(value);
        cursor = next;
        while (*cursor == ',' || *cursor == ' ') {
            ++cursor;
        }
    }
    if (endOut != nullptr) {
        *endOut = close;
    }
    return true;
}

bool readIntField(const std::string& json, const char* key, size_t from, long long* out) {
    const size_t at = json.find(key, from);
    if (at == std::string::npos) {
        return false;
    }
    const size_t colon = json.find(':', at);
    if (colon == std::string::npos) {
        return false;
    }
    *out = std::strtoll(json.c_str() + colon + 1, nullptr, 10);
    return true;
}

size_t countOccurrences(const std::string& text, const char* needle) {
    size_t count = 0;
    size_t at = text.find(needle);
    while (at != std::string::npos) {
        ++count;
        at = text.find(needle, at + 1);
    }
    return count;
}

// A scene built the way the product builds one, so an export under test is an
// export of a real project rather than of a fixture the exporter was shaped
// around.
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

}  // namespace

int runGltfExportSelfTests(GltfExportSelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};

    // -----------------------------------------------------------------------
    // FSR1C-01: the GLB envelope
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        GlbExportStatus why = GlbExportStatus::Ok;
        const std::vector<uint8_t> glb =
                exportSceneAsGlb(f.scene, ProjectKind::Construction, &why);
        r.check("FSR1C_01_an_ordinary_project_exports",
                why == GlbExportStatus::Ok && !glb.empty());

        const ParsedGlb parsed = parseGlb(glb);
        r.check("FSR1C_01_the_envelope_parses", parsed.ok);
        r.check("FSR1C_01_magic_is_glTF", parsed.magic == 0x46546C67u);
        r.check("FSR1C_01_version_is_2", parsed.version == 2u);
        r.check("FSR1C_01_declared_length_is_the_actual_length",
                parsed.declaredLength == glb.size());
        r.check("FSR1C_01_first_chunk_is_JSON", parsed.jsonType == 0x4E4F534Au);
        r.check("FSR1C_01_second_chunk_is_BIN", parsed.binType == 0x004E4942u);
        r.check("FSR1C_01_both_chunks_are_four_byte_aligned",
                (parsed.jsonLength % 4u) == 0u && (parsed.binLength % 4u) == 0u);
        r.check("FSR1C_01_the_chunks_exactly_fill_the_file",
                12u + 8u + parsed.jsonLength + 8u + parsed.binLength == glb.size());
        // The JSON chunk is padded with SPACES, which the specification
        // requires so a reader may treat it as text.
        bool jsonPaddingIsSpaces = true;
        for (size_t i = parsed.json.size(); i > 0; --i) {
            const char c = parsed.json[i - 1];
            if (c == ' ') continue;
            jsonPaddingIsSpaces = (c == '}');
            break;
        }
        r.check("FSR1C_01_json_is_padded_with_spaces_and_ends_in_an_object",
                jsonPaddingIsSpaces);
        r.check("FSR1C_01_the_asset_declares_gltf_2_0",
                parsed.json.find("\"version\":\"2.0\"") != std::string::npos);
        r.check("FSR1C_01_the_generator_names_forgeshape_and_nothing_else",
                parsed.json.find("\"generator\":\"ForgeShape\"") != std::string::npos);
    }

    // -----------------------------------------------------------------------
    // FSR1C-02: determinism
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 3.25, 1.5, 0.75);
        f.place(f.first(), placement(1.5, -0.5, 2.25, 370.0, 30.0, 12.25, 1.0, 2.0, 0.5));
        const std::vector<uint8_t> once = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const std::vector<uint8_t> twice = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        r.check("FSR1C_02_the_same_project_exports_byte_identically",
                !once.empty() && once == twice);

        // And an unrelated act between the two exports must not change the file.
        f.scene.setActiveBody(f.first().objectId());
        const std::vector<uint8_t> thrice = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        r.check("FSR1C_02_reselecting_the_same_body_changes_no_byte", once == thrice);
    }

    // -----------------------------------------------------------------------
    // FSR1C-03: metres, with no hidden scale factor
    // -----------------------------------------------------------------------
    {
        Fixture f;
        // One metre on every axis. ForgeShape primitives are centred on their
        // local origin, so this must export as exactly -0.5 .. +0.5.
        f.setBox(f.first(), 1.0, 1.0, 1.0);
        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsed = parseGlb(glb);

        std::vector<double> min;
        std::vector<double> max;
        size_t end = 0;
        const bool haveMin = readNumberArray(parsed.json, "\"min\":", 0, &min, &end);
        const bool haveMax = readNumberArray(parsed.json, "\"max\":", 0, &max);
        r.check("FSR1C_03_position_bounds_are_present", haveMin && haveMax
                && min.size() == 3 && max.size() == 3);
        if (min.size() == 3 && max.size() == 3) {
            r.check("FSR1C_03_a_one_metre_box_is_one_unit_across",
                    std::fabs((max[0] - min[0]) - 1.0) < 1e-6
                            && std::fabs((max[1] - min[1]) - 1.0) < 1e-6
                            && std::fabs((max[2] - min[2]) - 1.0) < 1e-6);
            // The specific failure a hidden unit conversion would produce.
            r.check("FSR1C_03_there_is_no_hidden_millimetre_conversion",
                    std::fabs(max[0]) < 10.0 && std::fabs(max[0] - 500.0) > 1.0
                            && std::fabs(max[0] - 0.0005) > 1e-6);
            r.check("FSR1C_03_the_box_is_centred_on_its_local_origin",
                    std::fabs(min[0] + 0.5) < 1e-6 && std::fabs(max[0] - 0.5) < 1e-6);
        }
    }

    // -----------------------------------------------------------------------
    // FSR1C-04: right-handed +Y-up, and the zero-conversion path is explicit
    // -----------------------------------------------------------------------
    {
        Fixture f;
        // A box that is unmistakable on every axis: 1 wide, 2 tall, 4 deep. If
        // any axis were swapped, these three numbers could not come back in
        // this order.
        f.setBox(f.first(), 1.0, 2.0, 4.0);
        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsed = parseGlb(glb);

        std::vector<double> min;
        std::vector<double> max;
        readNumberArray(parsed.json, "\"min\":", 0, &min);
        readNumberArray(parsed.json, "\"max\":", 0, &max);
        if (min.size() == 3 && max.size() == 3) {
            r.check("FSR1C_04_width_stays_on_X", std::fabs((max[0] - min[0]) - 1.0) < 1e-6);
            // HEIGHT lands on Y. This is the +Y-up assertion: in a Z-up world
            // the 2 would be on Z and the 4 on Y.
            r.check("FSR1C_04_height_stays_on_Y_which_is_up",
                    std::fabs((max[1] - min[1]) - 2.0) < 1e-6);
            r.check("FSR1C_04_depth_stays_on_Z", std::fabs((max[2] - min[2]) - 4.0) < 1e-6);
        }

        // No conversion node exists. There is exactly one node per body, the
        // scene lists exactly those, and an unplaced body carries the identity
        // matrix — a root rotation would show up as a non-identity here.
        std::vector<double> sceneNodes;
        readNumberArray(parsed.json, "\"nodes\":", 0, &sceneNodes);
        r.check("FSR1C_04_the_scene_lists_exactly_the_body_nodes",
                sceneNodes.size() == 1 && sceneNodes[0] == 0.0);
        r.check("FSR1C_04_there_is_no_extra_conversion_node",
                countOccurrences(parsed.json, "\"matrix\":") == 1);

        std::vector<double> matrix;
        readNumberArray(parsed.json, "\"matrix\":", 0, &matrix);
        bool isIdentity = matrix.size() == 16;
        for (size_t i = 0; i < matrix.size() && isIdentity; ++i) {
            const double expected = (i % 5 == 0) ? 1.0 : 0.0;
            isIdentity = std::fabs(matrix[i] - expected) < 1e-9;
        }
        r.check("FSR1C_04_an_unplaced_body_exports_the_identity_matrix", isIdentity);
    }

    // -----------------------------------------------------------------------
    // FSR1C-05: the node matrix IS the renderer's model matrix
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        // Asymmetric on purpose: a rotation on all three axes with different
        // magnitudes, a translation with three different signs and magnitudes,
        // and a non-uniform scale. A swapped axis, a transposed matrix, a
        // reversed composition order or a mishandled scale all break this.
        const TransformValues values =
                placement(1.5, -0.5, 2.25, 370.0, 30.0, 12.25, 1.25, 2.0, 0.5);
        f.place(f.first(), values);

        const Mat4 expected = f.first().transform().modelMatrix();
        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsed = parseGlb(glb);
        std::vector<double> matrix;
        readNumberArray(parsed.json, "\"matrix\":", 0, &matrix);

        bool matches = matrix.size() == 16;
        for (size_t i = 0; i < matrix.size() && matches; ++i) {
            // `%.9g` round-trips float32 exactly, so this is an EQUALITY test
            // and not a tolerance: the exported text must name the very float
            // the renderer holds.
            matches = static_cast<float>(matrix[i]) == expected.m[i];
        }
        r.check("FSR1C_05_the_node_matrix_is_the_renderer_model_matrix_element_for_element",
                matches);

        // The translation must sit in the last COLUMN of a column-major matrix,
        // which is m[12..14]. A transposed writer would put it at 3, 7, 11.
        r.check("FSR1C_05_translation_is_in_the_last_column_not_the_last_row",
                matrix.size() == 16 && static_cast<float>(matrix[12]) == 1.5f
                        && static_cast<float>(matrix[13]) == -0.5f
                        && static_cast<float>(matrix[14]) == 2.25f
                        && matrix[3] == 0.0 && matrix[7] == 0.0 && matrix[11] == 0.0
                        && matrix[15] == 1.0);
        // And it is NOT scaled: a translation multiplied by the scale would
        // read 1.875 rather than 1.5 on X.
        r.check("FSR1C_05_translation_is_not_multiplied_by_the_scale",
                matrix.size() == 16 && static_cast<float>(matrix[12]) == 1.5f);
    }

    // -----------------------------------------------------------------------
    // FSR1C-06: the pivot is the body's own origin, and nothing recentres it
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        const std::vector<uint8_t> atOrigin =
                exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsedAtOrigin = parseGlb(atOrigin);
        std::vector<double> minAtOrigin;
        std::vector<double> maxAtOrigin;
        readNumberArray(parsedAtOrigin.json, "\"min\":", 0, &minAtOrigin);
        readNumberArray(parsedAtOrigin.json, "\"max\":", 0, &maxAtOrigin);

        // Move it a long way. The VERTICES must not move at all: the placement
        // belongs to the node, and baking it into the mesh — or recentring the
        // mesh on its own bounds — would change these numbers.
        f.place(f.first(), placement(37.0, -12.5, 4.25, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
        const std::vector<uint8_t> moved = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsedMoved = parseGlb(moved);
        std::vector<double> minMoved;
        std::vector<double> maxMoved;
        readNumberArray(parsedMoved.json, "\"min\":", 0, &minMoved);
        readNumberArray(parsedMoved.json, "\"max\":", 0, &maxMoved);

        r.check("FSR1C_06_moving_a_body_does_not_move_its_vertices",
                minAtOrigin == minMoved && maxAtOrigin == maxMoved);
        r.check("FSR1C_06_local_geometry_still_straddles_the_local_origin",
                minMoved.size() == 3 && minMoved[0] < 0.0 && maxMoved[0] > 0.0);
        // The placement went into the node instead.
        std::vector<double> matrix;
        readNumberArray(parsedMoved.json, "\"matrix\":", 0, &matrix);
        r.check("FSR1C_06_the_placement_travels_on_the_node",
                matrix.size() == 16 && static_cast<float>(matrix[12]) == 37.0f
                        && static_cast<float>(matrix[13]) == -12.5f
                        && static_cast<float>(matrix[14]) == 4.25f);
    }

    // -----------------------------------------------------------------------
    // FSR1C-07: multi-body order and identity
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        const ObjectId firstId = f.first().objectId();
        SceneObject& second = f.scene.addBody();
        applyPrimitive(second.construction(), second.meshStore(), PrimitiveSpec::forSphere(1.5));
        const ObjectId secondId = second.objectId();
        SceneObject& third = f.scene.addBody();
        applyPrimitive(third.construction(), third.meshStore(),
                       PrimitiveSpec::forCylinder(1.0, 3.0));
        const ObjectId thirdId = third.objectId();

        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsed = parseGlb(glb);

        r.check("FSR1C_07_every_body_becomes_one_node_and_one_mesh",
                countOccurrences(parsed.json, "\"matrix\":") == 3
                        && countOccurrences(parsed.json, "\"primitives\":") == 3);
        std::vector<double> sceneNodes;
        readNumberArray(parsed.json, "\"nodes\":", 0, &sceneNodes);
        r.check("FSR1C_07_the_scene_lists_them_in_scene_order",
                sceneNodes.size() == 3 && sceneNodes[0] == 0.0 && sceneNodes[1] == 1.0
                        && sceneNodes[2] == 2.0);

        // Names are derived from the ObjectId, so identity survives into the
        // file without inventing product vocabulary that does not exist yet.
        std::string expectedFirst = "\"name\":\"Body_" + std::to_string(firstId) + "\"";
        std::string expectedSecond = "\"name\":\"Body_" + std::to_string(secondId) + "\"";
        std::string expectedThird = "\"name\":\"Body_" + std::to_string(thirdId) + "\"";
        const size_t atFirst = parsed.json.find(expectedFirst);
        const size_t atSecond = parsed.json.find(expectedSecond);
        const size_t atThird = parsed.json.find(expectedThird);
        r.check("FSR1C_07_node_names_are_derived_from_the_object_id",
                atFirst != std::string::npos && atSecond != std::string::npos
                        && atThird != std::string::npos);
        r.check("FSR1C_07_and_appear_in_scene_order",
                atFirst < atSecond && atSecond < atThird);
        r.check("FSR1C_07_there_are_three_accessors_per_body",
                countOccurrences(parsed.json, "\"componentType\":") == 9);
    }

    // -----------------------------------------------------------------------
    // FSR1C-08: Construction exports what the source IS, evaluated now
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 1.0, 1.0, 1.0);
        const std::vector<uint8_t> before =
                exportSceneAsGlb(f.scene, ProjectKind::Construction);

        // Change the SOURCE without publishing anything. The mesh store still
        // holds the old revision; an exporter reading the store — or a decoded
        // `.forge` — would export the old box.
        f.first().construction().setPrimitive(PrimitiveSpec::forBox(4.0, 1.0, 1.0));
        const std::vector<uint8_t> after =
                exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsed = parseGlb(after);
        std::vector<double> min;
        std::vector<double> max;
        readNumberArray(parsed.json, "\"min\":", 0, &min);
        readNumberArray(parsed.json, "\"max\":", 0, &max);

        r.check("FSR1C_08_a_source_change_changes_the_export", before != after);
        r.check("FSR1C_08_the_export_is_evaluated_from_the_current_parameters",
                min.size() == 3 && std::fabs((max[0] - min[0]) - 4.0) < 1e-6);
    }

    // -----------------------------------------------------------------------
    // FSR1C-09: a Sculpt project exports the sculpt mesh, never a fallback
    // -----------------------------------------------------------------------
    {
        Fixture f;
        applyPrimitive(f.first().construction(), f.first().meshStore(),
                       PrimitiveSpec::forSphere(1.0));
        const ConstructionMesh source = f.first().construction().generateMesh();
        MeshValidation why = MeshValidation::Ok;
        const bool frozen =
                f.first().frozenSculpt().mesh.freezeFrom(source, f.first().objectId(), &why);
        r.check("FSR1C_09_the_fixture_freezes", frozen);

        // Move one vertex a long way, so the sculpt mesh cannot be mistaken for
        // the sphere it was frozen from.
        const Vec3 moved{0.0f, 9.0f, 0.0f};
        f.first().frozenSculpt().mesh.setVertexPosition(0, moved);
        f.first().frozenSculpt().mesh.advanceRevision();

        const std::vector<uint8_t> asConstruction =
                exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const std::vector<uint8_t> asSculpt = exportSceneAsGlb(f.scene, ProjectKind::Sculpt);
        r.check("FSR1C_09_the_two_representations_export_differently",
                !asConstruction.empty() && !asSculpt.empty()
                        && asConstruction != asSculpt);

        const ParsedGlb sculptParsed = parseGlb(asSculpt);
        std::vector<double> sculptMax;
        readNumberArray(sculptParsed.json, "\"max\":", 0, &sculptMax);
        r.check("FSR1C_09_the_sculpt_export_carries_the_edited_vertex",
                sculptMax.size() == 3 && std::fabs(sculptMax[1] - 9.0) < 1e-5);

        const ParsedGlb constructionParsed = parseGlb(asConstruction);
        std::vector<double> constructionMax;
        readNumberArray(constructionParsed.json, "\"max\":", 0, &constructionMax);
        r.check("FSR1C_09_a_construction_project_still_exports_the_source_shape",
                constructionMax.size() == 3 && constructionMax[1] < 1.0);

        // And the snapshot says which representation it took, so a test never
        // has to infer it from geometry alone.
        GlbExportScene captured;
        captureGlbExportScene(f.scene, ProjectKind::Sculpt, &captured);
        r.check("FSR1C_09_the_snapshot_reports_the_sculpt_source",
                captured.bodies.size() == 1 && captured.bodies[0].fromSculpt);
        captureGlbExportScene(f.scene, ProjectKind::Construction, &captured);
        r.check("FSR1C_09_and_reports_the_construction_source",
                captured.bodies.size() == 1 && !captured.bodies[0].fromSculpt);
    }

    // -----------------------------------------------------------------------
    // FSR1C-10 / FSR1C-11: geometry, normals, winding, accessor bounds
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsed = parseGlb(glb);

        GlbExportScene captured;
        captureGlbExportScene(f.scene, ProjectKind::Construction, &captured);
        const RenderMeshData& mesh = captured.bodies[0].render;

        // Every accessor's range lies inside the BIN chunk, is 4-byte aligned,
        // and its count matches its byte length.
        long long positionCount = 0;
        readIntField(parsed.json, "\"count\":", 0, &positionCount);
        r.check("FSR1C_11_the_position_count_is_the_vertex_count",
                positionCount == static_cast<long long>(mesh.vertexCount()));

        bool viewsInBounds = true;
        bool viewsAligned = true;
        size_t cursor = parsed.json.find("\"bufferViews\":");
        for (int i = 0; i < 3 && cursor != std::string::npos; ++i) {
            long long offset = 0;
            long long length = 0;
            const size_t atOffset = parsed.json.find("\"byteOffset\":", cursor);
            const size_t atLength = parsed.json.find("\"byteLength\":", cursor);
            if (atOffset == std::string::npos || atLength == std::string::npos) break;
            readIntField(parsed.json, "\"byteOffset\":", cursor, &offset);
            readIntField(parsed.json, "\"byteLength\":", cursor, &length);
            viewsInBounds = viewsInBounds && offset >= 0 && length > 0
                            && static_cast<uint64_t>(offset + length) <= parsed.binLength;
            viewsAligned = viewsAligned && (offset % 4) == 0;
            cursor = atLength + 1;
        }
        r.check("FSR1C_11_every_buffer_view_lies_inside_the_bin_chunk", viewsInBounds);
        r.check("FSR1C_11_every_buffer_view_is_four_byte_aligned", viewsAligned);

        // The declared min/max really bound the positions in the BIN chunk.
        std::vector<double> min;
        std::vector<double> max;
        readNumberArray(parsed.json, "\"min\":", 0, &min);
        readNumberArray(parsed.json, "\"max\":", 0, &max);
        bool boundsHold = min.size() == 3 && max.size() == 3;
        bool positionsFinite = true;
        for (uint32_t v = 0; v < mesh.vertexCount() && boundsHold; ++v) {
            for (int c = 0; c < 3; ++c) {
                const float value = readF32(glb, parsed.binStart + v * 12u
                                                    + static_cast<size_t>(c) * 4u);
                positionsFinite = positionsFinite && std::isfinite(value);
                if (value < min[c] - 1e-6 || value > max[c] + 1e-6) {
                    boundsHold = false;
                    break;
                }
            }
        }
        r.check("FSR1C_11_the_declared_bounds_hold_for_the_written_positions", boundsHold);
        r.check("FSR1C_11_no_position_is_non_finite", positionsFinite);

        // NORMALS: unit length (or exactly zero, the honest answer for a corner
        // no non-degenerate triangle touches) and finite.
        bool normalsUnit = true;
        const size_t normalStart = parsed.binStart + (mesh.vertexCount() * 12u);
        for (uint32_t v = 0; v < mesh.vertexCount() && normalsUnit; ++v) {
            const float nx = readF32(glb, normalStart + v * 12u + 0u);
            const float ny = readF32(glb, normalStart + v * 12u + 4u);
            const float nz = readF32(glb, normalStart + v * 12u + 8u);
            if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) {
                normalsUnit = false;
                break;
            }
            const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
            normalsUnit = std::fabs(length - 1.0f) < 1e-3f || length == 0.0f;
        }
        r.check("FSR1C_10_every_written_normal_is_unit_length_or_exactly_zero", normalsUnit);

        // INDICES: in range, a multiple of three, and unreversed. The winding
        // check is the real one — a box's every triangle must face outward,
        // which for a body centred on its local origin means the geometric
        // normal points away from that origin.
        const size_t indexStart = normalStart + (mesh.vertexCount() * 12u);
        bool indicesInRange = (mesh.indexCount() % 3u) == 0u;
        bool windingOutward = indicesInRange;
        for (uint32_t i = 0; i + 2 < mesh.indexCount() && indicesInRange; i += 3) {
            const uint32_t a = readU32(glb, indexStart + (i + 0) * 4u);
            const uint32_t b = readU32(glb, indexStart + (i + 1) * 4u);
            const uint32_t c = readU32(glb, indexStart + (i + 2) * 4u);
            if (a >= mesh.vertexCount() || b >= mesh.vertexCount() || c >= mesh.vertexCount()) {
                indicesInRange = false;
                break;
            }
            const Vec3 v0{readF32(glb, parsed.binStart + a * 12u + 0u),
                          readF32(glb, parsed.binStart + a * 12u + 4u),
                          readF32(glb, parsed.binStart + a * 12u + 8u)};
            const Vec3 v1{readF32(glb, parsed.binStart + b * 12u + 0u),
                          readF32(glb, parsed.binStart + b * 12u + 4u),
                          readF32(glb, parsed.binStart + b * 12u + 8u)};
            const Vec3 v2{readF32(glb, parsed.binStart + c * 12u + 0u),
                          readF32(glb, parsed.binStart + c * 12u + 4u),
                          readF32(glb, parsed.binStart + c * 12u + 8u)};
            const Vec3 e1 = vec3Sub(v1, v0);
            const Vec3 e2 = vec3Sub(v2, v0);
            const Vec3 faceNormal{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z,
                                  e1.x * e2.y - e1.y * e2.x};
            // The centroid doubles as an outward direction for a solid centred
            // on its local origin.
            const float centroidX = (v0.x + v1.x + v2.x) / 3.0f;
            const float centroidY = (v0.y + v1.y + v2.y) / 3.0f;
            const float centroidZ = (v0.z + v1.z + v2.z) / 3.0f;
            const float outward = faceNormal.x * centroidX + faceNormal.y * centroidY
                                  + faceNormal.z * centroidZ;
            windingOutward = windingOutward && outward > 0.0f;
        }
        r.check("FSR1C_11_every_index_is_inside_the_vertex_range", indicesInRange);
        r.check("FSR1C_10_every_triangle_winds_counter_clockwise_seen_from_outside",
                windingOutward);
        r.check("FSR1C_10_the_primitive_mode_is_triangles",
                parsed.json.find("\"mode\":4") != std::string::npos);
        r.check("FSR1C_10_indices_are_unsigned_int",
                parsed.json.find("\"componentType\":5125") != std::string::npos);
    }

    // -----------------------------------------------------------------------
    // FSR1C-12: failing closed
    // -----------------------------------------------------------------------
    {
        GlbExportScene empty;
        GlbExportStatus why = GlbExportStatus::Ok;
        const std::vector<uint8_t> nothing = encodeGlb(empty, &why);
        r.check("FSR1C_12_an_empty_scene_exports_nothing_and_says_why",
                nothing.empty() && why == GlbExportStatus::NothingToExport);

        // A non-finite placement is refused rather than written: a NaN in an
        // exported file is a corrupt file that looks valid.
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        GlbExportScene captured;
        r.check("FSR1C_12_a_good_scene_captures",
                captureGlbExportScene(f.scene, ProjectKind::Construction, &captured)
                        == GlbExportStatus::Ok);
        // A hand-built snapshot with a NaN placement. The WRITER must refuse it,
        // not merely the capture path: anything that builds a snapshot by hand
        // reaches the file through encodeGlb, and a file carrying `nan` would
        // parse as JSON and pass a length check while putting a NaN into a
        // user's model.
        captured.bodies[0].model.m[0] = std::nanf("");
        const std::vector<uint8_t> corrupt = encodeGlb(captured, &why);
        r.check("FSR1C_12_a_non_finite_placement_is_refused_by_the_writer",
                corrupt.empty() && why == GlbExportStatus::NonFiniteValue);

        // An index pointing outside its own vertex data is the one structural
        // error a validator catches and a naive importer does not.
        GlbExportScene outOfRange;
        captureGlbExportScene(f.scene, ProjectKind::Construction, &outOfRange);
        outOfRange.bodies[0].render.indices[0] = outOfRange.bodies[0].render.vertexCount();
        const std::vector<uint8_t> dangling = encodeGlb(outOfRange, &why);
        r.check("FSR1C_12_an_out_of_range_index_is_refused",
                dangling.empty() && why == GlbExportStatus::InvalidMesh);

        // A triangle list that is not a multiple of three.
        GlbExportScene ragged;
        captureGlbExportScene(f.scene, ProjectKind::Construction, &ragged);
        ragged.bodies[0].render.indices.pop_back();
        const std::vector<uint8_t> raggedBytes = encodeGlb(ragged, &why);
        r.check("FSR1C_12_an_index_count_that_is_not_triangles_is_refused",
                raggedBytes.empty() && why == GlbExportStatus::InvalidMesh);

        // And a mesh with no geometry at all.
        GlbExportScene emptyMesh;
        captureGlbExportScene(f.scene, ProjectKind::Construction, &emptyMesh);
        emptyMesh.bodies[0].render.vertices.clear();
        const std::vector<uint8_t> emptyBytes = encodeGlb(emptyMesh, &why);
        r.check("FSR1C_12_an_empty_mesh_is_refused",
                emptyBytes.empty() && why == GlbExportStatus::InvalidMesh);

        r.check("FSR1C_12_every_status_has_a_name",
                std::string(glbExportStatusName(GlbExportStatus::NothingToExport))
                                == "NothingToExport"
                        && std::string(glbExportStatusName(GlbExportStatus::InvalidMesh))
                                == "InvalidMesh"
                        && std::string(glbExportStatusName(GlbExportStatus::NonFiniteValue))
                                == "NonFiniteValue");
    }

    return r.n;
}

}  // namespace forgeshape
