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

// The character position of the nth (0-based) occurrence of `needle`.
//
// `readNumberArray` and `readIntField` take a character offset to search FROM,
// not an occurrence index, which is an easy thing to get wrong by one whole
// meaning. This turns "the second byteOffset" into the position those two want.
size_t occurrenceAt(const std::string& text, const char* needle, size_t nth) {
    size_t at = text.find(needle);
    for (size_t i = 0; i < nth && at != std::string::npos; ++i) {
        at = text.find(needle, at + 1);
    }
    return at == std::string::npos ? text.size() : at;
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
        // scene lists exactly those, and an unplaced body's node translation is
        // zero — a root rotation or an offset would show up here.
        std::vector<double> sceneNodes;
        readNumberArray(parsed.json, "\"nodes\":", 0, &sceneNodes);
        r.check("FSR1C_04_the_scene_lists_exactly_the_body_nodes",
                sceneNodes.size() == 1 && sceneNodes[0] == 0.0);
        r.check("FSR1C_04_there_is_no_extra_conversion_node",
                countOccurrences(parsed.json, "\"translation\":") == 1);
        // ARCH-OWNER-07: the baked policy writes NO node matrix at all, so a
        // "matrix" anywhere in the document is either the old policy returning
        // or a conversion node arriving.
        r.check("FSR1C_04_no_node_carries_a_matrix",
                countOccurrences(parsed.json, "\"matrix\":") == 0);

        std::vector<double> translation;
        readNumberArray(parsed.json, "\"translation\":", 0, &translation);
        r.check("FSR1C_04_an_unplaced_body_exports_a_zero_translation",
                translation.size() == 3 && translation[0] == 0.0 && translation[1] == 0.0
                        && translation[2] == 0.0);
    }

    // -----------------------------------------------------------------------
    // FSR1C-05 / FSR1C-C1: the node carries T, and only T
    // -----------------------------------------------------------------------
    //
    // ARCH-OWNER-07 split Model = T * L. This case is the T half: the node's
    // translation is the body's authored position, and rotation and scale are
    // absent so a consumer reads identity for both. The L half is FSR1C-C1
    // below.
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        // Asymmetric on purpose: a rotation on all three axes with different
        // magnitudes, a translation with three different signs and magnitudes,
        // and a non-uniform scale.
        const TransformValues values =
                placement(1.5, -0.5, 2.25, 370.0, 30.0, 12.25, 1.25, 2.0, 0.5);
        f.place(f.first(), values);

        const Mat4 expected = f.first().transform().modelMatrix();
        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsed = parseGlb(glb);
        std::vector<double> translation;
        readNumberArray(parsed.json, "\"translation\":", 0, &translation);

        // `%.9g` round-trips float32 exactly, so these are EQUALITY tests and
        // not tolerances: the exported text must name the very float the
        // renderer's model matrix holds in its last column.
        r.check("FSR1C_05_the_node_translation_is_the_model_matrix_last_column",
                translation.size() == 3 && static_cast<float>(translation[0]) == expected.m[12]
                        && static_cast<float>(translation[1]) == expected.m[13]
                        && static_cast<float>(translation[2]) == expected.m[14]);
        r.check("FSR1C_05_and_is_the_authored_position_in_metres",
                translation.size() == 3 && static_cast<float>(translation[0]) == 1.5f
                        && static_cast<float>(translation[1]) == -0.5f
                        && static_cast<float>(translation[2]) == 2.25f);
        // NOT multiplied by the scale: 1.5 * 1.25 would read 1.875.
        r.check("FSR1C_05_translation_is_not_multiplied_by_the_scale",
                translation.size() == 3 && static_cast<float>(translation[0]) == 1.5f);
        r.check("FSR1C_05_the_node_writes_no_rotation",
                countOccurrences(parsed.json, "\"rotation\":") == 0);
        r.check("FSR1C_05_the_node_writes_no_scale",
                countOccurrences(parsed.json, "\"scale\":") == 0);
        r.check("FSR1C_05_the_node_writes_no_matrix",
                countOccurrences(parsed.json, "\"matrix\":") == 0);
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
        // The TRANSLATION travels on the node. This stays true under
        // ARCH-OWNER-07: it is rotation and scale that are baked, never T.
        std::vector<double> translation;
        readNumberArray(parsedMoved.json, "\"translation\":", 0, &translation);
        r.check("FSR1C_06_the_placement_travels_on_the_node",
                translation.size() == 3 && static_cast<float>(translation[0]) == 37.0f
                        && static_cast<float>(translation[1]) == -12.5f
                        && static_cast<float>(translation[2]) == 4.25f);
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
                countOccurrences(parsed.json, "\"translation\":") == 3
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
        captured.bodies[0].translation[0] = std::nanf("");
        const std::vector<uint8_t> corrupt = encodeGlb(captured, &why);
        r.check("FSR1C_12_a_non_finite_placement_is_refused_by_the_writer",
                corrupt.empty() && why == GlbExportStatus::NonFiniteValue);

        // The same for the matrix that was baked, which the writer re-checks
        // even though it no longer writes it: a snapshot whose bake matrix is
        // garbage is a snapshot whose VERTICES are garbage.
        GlbExportScene nanBake;
        captureGlbExportScene(f.scene, ProjectKind::Construction, &nanBake);
        nanBake.bodies[0].local.m[0] = std::nanf("");
        const std::vector<uint8_t> nanBakeBytes = encodeGlb(nanBake, &why);
        r.check("FSR1C_12_a_non_finite_bake_matrix_is_refused_by_the_writer",
                nanBakeBytes.empty() && why == GlbExportStatus::NonFiniteValue);

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
                                == "NonFiniteValue"
                        && std::string(glbExportStatusName(GlbExportStatus::SingularTransform))
                                == "SingularTransform"
                        && std::string(glbExportStatusName(GlbExportStatus::MirroredTransform))
                                == "MirroredTransform");
    }

    // -----------------------------------------------------------------------
    // FSR1C-C1: the bake — rotation and scale live in the vertices
    // -----------------------------------------------------------------------
    //
    // ARCH-OWNER-07. The node half is FSR1C-04/05/06 above; everything here is
    // the L half: Model = T * L, L is baked, and the numbers in the file are
    // the body's final rotated and scaled shape about its own origin.

    // The identity the whole design rests on: modelMatrix() really is
    // T(position) * localMatrix(). If this ever stopped holding, the split
    // would be exporting a different placement from the one drawn.
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        f.place(f.first(), placement(1.5, -0.5, 2.25, 370.0, 30.0, 12.25, 1.25, 2.0, 0.5));
        const ConstructionTransform& transform = f.first().transform();
        const Mat4 model = transform.modelMatrix();
        const Mat4 local = transform.localMatrix();
        const Mat4 rebuilt =
                mat4Multiply(mat4Translation(Vec3{model.m[12], model.m[13], model.m[14]}), local);
        bool same = true;
        for (int i = 0; i < 16 && same; ++i) {
            same = std::fabs(rebuilt.m[i] - model.m[i]) < 1e-5f;
        }
        r.check("FSR1C_C1_04_model_is_exactly_translation_times_local", same);
        // And localMatrix() carries no translation of its own, which is what
        // makes "bake L about the local origin" mean what it says.
        r.check("FSR1C_C1_05_the_local_matrix_has_no_translation",
                local.m[12] == 0.0f && local.m[13] == 0.0f && local.m[14] == 0.0f);
    }

    // A vertex baked by hand, compared with the vertex in the file.
    {
        Fixture f;
        // A CONE, and the choice matters. A box, a plane and a sphere are all
        // centrally symmetric about their local origin, and rotating a
        // centrally symmetric point set leaves its bounding box symmetric too —
        // so in any of those, a recentre-on-bounds would be invisible. A cone
        // has its apex at +Y and its base disc at -Y, so once it is turned its
        // bounds are genuinely lopsided about the origin and a recentre would
        // show.
        applyPrimitive(f.first().construction(), f.first().meshStore(),
                       PrimitiveSpec::forCone(2.0, 3.0));
        f.place(f.first(), placement(4.0, -2.0, 0.5, 25.0, -40.0, 65.0, 1.5, 1.0, 0.25));

        const Mat4 local = f.first().transform().localMatrix();
        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsed = parseGlb(glb);

        // Read the written POSITION accessor straight out of the BIN chunk and
        // compare every vertex with L * p computed here.
        long long positionCount = 0;
        long long viewOffset = 0;
        long long viewLength = 0;
        readIntField(parsed.json, "\"count\":", 0, &positionCount);
        readIntField(parsed.json, "\"byteOffset\":", 0, &viewOffset);
        readIntField(parsed.json, "\"byteLength\":", 0, &viewLength);

        // The unbaked source, regenerated the same way the exporter does.
        const ConstructionMesh source = f.first().construction().generateMesh();
        RenderMeshData expectedMesh;
        const bool built = buildRenderMesh(source.vertices.data(),
                                           static_cast<uint32_t>(source.vertices.size()),
                                           source.indices.data(),
                                           static_cast<uint32_t>(source.indices.size()),
                                           SurfaceShading::Smooth, &expectedMesh,
                                           /*renderBothSides=*/false);
        r.check("FSR1C_C1_04_the_comparison_mesh_builds", built);

        bool bakedMatches = built && positionCount > 0
                && static_cast<size_t>(positionCount) == expectedMesh.vertices.size()
                && viewLength == positionCount * 12;
        for (long long v = 0; v < positionCount && bakedMatches; ++v) {
            const float* p = expectedMesh.vertices[static_cast<size_t>(v)].position;
            const Vec3 expected = mat4TransformPoint(local, Vec3{p[0], p[1], p[2]});
            const size_t at = parsed.binStart + static_cast<size_t>(viewOffset)
                    + static_cast<size_t>(v) * 12u;
            bakedMatches = std::fabs(readF32(glb, at) - expected.x) < 1e-5f
                    && std::fabs(readF32(glb, at + 4) - expected.y) < 1e-5f
                    && std::fabs(readF32(glb, at + 8) - expected.z) < 1e-5f;
        }
        r.check("FSR1C_C1_04_every_written_position_is_L_times_the_source_position",
                bakedMatches);

        // No recentre: the baked bounds are the baked geometry's own bounds and
        // are NOT symmetric about zero, because a turned 3 x 1 plane is not.
        std::vector<double> minValues;
        std::vector<double> maxValues;
        readNumberArray(parsed.json, "\"min\":", 0, &minValues);
        readNumberArray(parsed.json, "\"max\":", 0, &maxValues);
        // A recentre-on-bounds forces min == -max on EVERY axis, so the
        // falsifying observation is one axis where it does not.
        bool anyAxisIsLopsided = false;
        if (minValues.size() == 3 && maxValues.size() == 3) {
            for (int c = 0; c < 3; ++c) {
                if (std::fabs(minValues[c] + maxValues[c]) > 1e-4) {
                    anyAxisIsLopsided = true;
                }
            }
        }
        r.check("FSR1C_C1_05_the_baked_bounds_are_not_recentred_on_the_geometry",
                anyAxisIsLopsided);

        // The pivot is still the local origin, and the node still positions it.
        std::vector<double> translation;
        readNumberArray(parsed.json, "\"translation\":", 0, &translation);
        r.check("FSR1C_C1_05_the_pivot_is_the_local_origin_positioned_by_the_node",
                translation.size() == 3 && static_cast<float>(translation[0]) == 4.0f
                        && static_cast<float>(translation[1]) == -2.0f
                        && static_cast<float>(translation[2]) == 0.5f);

        // POSITION min/max are recomputed from the BAKED vertices, not carried
        // over from the source. An unbaked cone of bottom diameter 2 spans
        // exactly +-1 on X; a baked one under this rotation and 1.5/1/0.25
        // scale does not.
        bool boundsAreBaked = minValues.size() == 3
                && (std::fabs(minValues[0] + 1.0) > 1e-3 || std::fabs(maxValues[0] - 1.0) > 1e-3);
        r.check("FSR1C_C1_07_position_bounds_are_recomputed_from_the_baked_vertices",
                boundsAreBaked);
        // ...and they really do bound the written data.
        bool boundsHold = minValues.size() == 3 && maxValues.size() == 3;
        for (long long v = 0; v < positionCount && boundsHold; ++v) {
            const size_t at = parsed.binStart + static_cast<size_t>(viewOffset)
                    + static_cast<size_t>(v) * 12u;
            for (int c = 0; c < 3 && boundsHold; ++c) {
                const float value = readF32(glb, at + static_cast<size_t>(c) * 4u);
                boundsHold = value >= static_cast<float>(minValues[c]) - 1e-6f
                        && value <= static_cast<float>(maxValues[c]) + 1e-6f;
            }
        }
        r.check("FSR1C_C1_07_and_the_recomputed_bounds_hold", boundsHold);
    }

    // Normals under a non-uniform scale ride the inverse transpose, not L.
    {
        Fixture f;
        // A SPHERE, and the choice matters as much as the cone's did. A box's
        // face normals all lie along its local axes, and for a normal that is
        // parallel to a scale axis, L and inverse-transpose(L) produce the same
        // DIRECTION and differ only in length — which normalising then hides.
        // So a box cannot tell the two apart at all. A sphere's normals point
        // everywhere, and on every oblique one the two matrices disagree.
        applyPrimitive(f.first().construction(), f.first().meshStore(),
                       PrimitiveSpec::forSphere(1.0));
        // 4 : 1 : 1 is violent enough that the disagreement is wide.
        f.place(f.first(), placement(0.0, 0.0, 0.0, 0.0, 45.0, 0.0, 4.0, 1.0, 1.0));

        const Mat4 local = f.first().transform().localMatrix();
        const Mat4 normalMatrix = f.first().transform().normalMatrix();

        // normalMatrix() is R * S^-1, derived analytically. That it EQUALS
        // transpose(inverse(L)) is the claim ARCH-OWNER-07 relies on, so it is
        // checked numerically here rather than assumed: inverse(L) is built
        // from the same authoritative values through inverseModelMatrix(),
        // whose linear part is S^-1 * R^-1.
        const Mat4 inverseModel = f.first().transform().inverseModelMatrix();
        bool inverseTransposeMatches = true;
        for (int row = 0; row < 3 && inverseTransposeMatches; ++row) {
            for (int col = 0; col < 3 && inverseTransposeMatches; ++col) {
                // transpose(inverse(L))[col][row] == inverse(L)[row][col]
                const float transposed = inverseModel.m[row * 4 + col];
                inverseTransposeMatches =
                        std::fabs(normalMatrix.m[col * 4 + row] - transposed) < 1e-4f;
            }
        }
        r.check("FSR1C_C1_06_the_normal_matrix_is_the_inverse_transpose_of_L",
                inverseTransposeMatches);

        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const ParsedGlb parsed = parseGlb(glb);
        long long count = 0;
        readIntField(parsed.json, "\"count\":", 0, &count);
        // bufferView 1 is the NORMAL accessor's; its byteOffset is the second
        // "byteOffset" in the document.
        long long normalOffset = 0;
        readIntField(parsed.json, "\"byteOffset\":",
                     occurrenceAt(parsed.json, "\"byteOffset\":", 1), &normalOffset);

        // Every written normal is unit length, and none of them equals what
        // carrying the normal by L would have produced — which is the whole
        // point: under 4:1:1 those are different directions.
        bool unit = count > 0;
        bool differsFromNaiveL = false;
        const ConstructionMesh source = f.first().construction().generateMesh();
        RenderMeshData sourceMesh;
        buildRenderMesh(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                        source.indices.data(), static_cast<uint32_t>(source.indices.size()),
                        SurfaceShading::Smooth, &sourceMesh, /*renderBothSides=*/false);
        for (long long v = 0; v < count && unit; ++v) {
            const size_t at = parsed.binStart + static_cast<size_t>(normalOffset)
                    + static_cast<size_t>(v) * 12u;
            const Vec3 written{readF32(glb, at), readF32(glb, at + 4), readF32(glb, at + 8)};
            const float length = std::sqrt(written.x * written.x + written.y * written.y
                                           + written.z * written.z);
            unit = std::fabs(length - 1.0f) < 1e-3f;

            if (static_cast<size_t>(v) < sourceMesh.vertices.size()) {
                const float* n = sourceMesh.vertices[static_cast<size_t>(v)].normal;
                const Vec3 naive =
                        vec3Normalize(mat4TransformDirection(local, Vec3{n[0], n[1], n[2]}));
                if (std::fabs(written.x - naive.x) > 1e-2f
                    || std::fabs(written.y - naive.y) > 1e-2f
                    || std::fabs(written.z - naive.z) > 1e-2f) {
                    differsFromNaiveL = true;
                }
            }
        }
        r.check("FSR1C_C1_06_every_baked_normal_is_unit_length", unit);
        r.check("FSR1C_C1_06_and_is_not_what_carrying_it_by_L_would_give", differsFromNaiveL);
    }

    // The determinant rule, and what happens when it is violated.
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        f.place(f.first(), placement(0.0, 0.0, 0.0, 20.0, -35.0, 50.0, 2.0, 0.5, 3.0));
        GlbExportScene captured;
        GlbExportStatus why = GlbExportStatus::Ok;
        r.check("FSR1C_C1_08_a_positive_scale_captures",
                captureGlbExportScene(f.scene, ProjectKind::Construction, &captured) ==
                        GlbExportStatus::Ok);

        // det(R * S) = sx * sy * sz for the product's domain, so it is exactly
        // the scale product and it is positive. Winding therefore survives the
        // bake untouched, which is why no triangle is ever reversed.
        const Mat4 local = f.first().transform().localMatrix();
        const float a = local.m[0], b = local.m[4], c = local.m[8];
        const float d = local.m[1], e = local.m[5], g = local.m[9];
        const float h = local.m[2], i = local.m[6], j = local.m[10];
        const float determinant =
                a * (e * j - g * i) - b * (d * j - g * h) + c * (d * i - e * h);
        r.check("FSR1C_C1_08_the_determinant_is_the_scale_product_and_positive",
                determinant > 0.0f && std::fabs(determinant - 3.0f) < 1e-4f);

        // A mirror cannot be authored — the transform domain refuses a negative
        // scale — so it is injected into a snapshot to prove the WRITER refuses
        // it rather than silently reversing every triangle.
        GlbExportScene mirrored = captured;
        for (int m = 0; m < 3; ++m) {
            mirrored.bodies[0].local.m[m] = -mirrored.bodies[0].local.m[m];
        }
        const std::vector<uint8_t> mirroredBytes = encodeGlb(mirrored, &why);
        r.check("FSR1C_C1_08_a_mirrored_bake_is_refused_and_never_compensated",
                mirroredBytes.empty() && why == GlbExportStatus::MirroredTransform);

        GlbExportScene singular = captured;
        for (int m = 0; m < 3; ++m) {
            singular.bodies[0].local.m[m] = 0.0f;
        }
        const std::vector<uint8_t> singularBytes = encodeGlb(singular, &why);
        r.check("FSR1C_C1_08_a_singular_bake_is_refused",
                singularBytes.empty() && why == GlbExportStatus::SingularTransform);

        // And the transform domain still refuses both at the source, so neither
        // status is reachable from an authored placement. Removing the writer's
        // guard is not removing this one.
        TransformValidation validation = TransformValidation::Ok;
        ConstructionTransform probe;
        r.check("FSR1C_C1_08_the_domain_still_refuses_a_negative_scale",
                probe.setValues(placement(0, 0, 0, 0, 0, 0, -1.0, 1.0, 1.0), &validation)
                        == TransformUpdateStatus::Rejected);
        r.check("FSR1C_C1_08_and_a_zero_scale",
                probe.setValues(placement(0, 0, 0, 0, 0, 0, 1.0, 0.0, 1.0), &validation)
                        == TransformUpdateStatus::Rejected);
    }

    // Multi-body: each bakes its OWN L, and nothing is merged.
    {
        Fixture f;
        f.setBox(f.first(), 1.0, 1.0, 1.0);
        f.place(f.first(), placement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
        SceneObject& second = f.scene.addBody();
        r.check("FSR1C_C1_09_a_second_body_is_added", f.scene.bodyCount() == 2);
        {
            f.setBox(second, 1.0, 1.0, 1.0);
            // Same 1 m cube, a very different L.
            f.place(second, placement(5.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 7.0, 0.25));

            const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Construction);
            const ParsedGlb parsed = parseGlb(glb);
            r.check("FSR1C_C1_09_two_bodies_are_two_nodes",
                    countOccurrences(parsed.json, "\"translation\":") == 2
                            && countOccurrences(parsed.json, "\"primitives\":") == 2);

            // Body 1's cube stays 1 m; body 2's is 3 x 7 x 0.25 m. One shared
            // bake, or a merge, could not produce both.
            std::vector<double> firstMin, firstMax, secondMin, secondMax;
            readNumberArray(parsed.json, "\"min\":", 0, &firstMin);
            readNumberArray(parsed.json, "\"max\":", 0, &firstMax);
            readNumberArray(parsed.json, "\"min\":",
                            occurrenceAt(parsed.json, "\"min\":", 1), &secondMin);
            readNumberArray(parsed.json, "\"max\":",
                            occurrenceAt(parsed.json, "\"max\":", 1), &secondMax);
            const bool firstUnbaked = firstMin.size() == 3
                    && std::fabs((firstMax[0] - firstMin[0]) - 1.0) < 1e-5
                    && std::fabs((firstMax[1] - firstMin[1]) - 1.0) < 1e-5;
            const bool secondBaked = secondMin.size() == 3
                    && std::fabs((secondMax[0] - secondMin[0]) - 3.0) < 1e-5
                    && std::fabs((secondMax[1] - secondMin[1]) - 7.0) < 1e-5
                    && std::fabs((secondMax[2] - secondMin[2]) - 0.25) < 1e-5;
            r.check("FSR1C_C1_09_the_unscaled_body_is_unchanged", firstUnbaked);
            r.check("FSR1C_C1_09_and_the_scaled_body_baked_only_its_own_scale", secondBaked);
        }
    }

    // A Sculpt body still exports its sculpt mesh, now baked.
    {
        Fixture f;
        applyPrimitive(f.first().construction(), f.first().meshStore(),
                       PrimitiveSpec::forSphere(1.0));
        const ConstructionMesh sphere = f.first().construction().generateMesh();
        MeshValidation freezeWhy = MeshValidation::Ok;
        const bool frozen = f.first().frozenSculpt().mesh.freezeFrom(sphere, f.first().objectId(),
                                                                    &freezeWhy);
        r.check("FSR1C_C1_10_the_fixture_freezes", frozen);
        f.place(f.first(), placement(2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 4.0, 1.0));

        GlbExportScene captured;
        r.check("FSR1C_C1_10_a_sculpt_project_captures",
                captureGlbExportScene(f.scene, ProjectKind::Sculpt, &captured)
                        == GlbExportStatus::Ok);
        r.check("FSR1C_C1_10_and_reports_the_sculpt_source",
                !captured.bodies.empty() && captured.bodies[0].fromSculpt);

        const std::vector<uint8_t> glb = exportSceneAsGlb(f.scene, ProjectKind::Sculpt);
        const ParsedGlb parsed = parseGlb(glb);
        std::vector<double> minValues, maxValues, translation;
        readNumberArray(parsed.json, "\"min\":", 0, &minValues);
        readNumberArray(parsed.json, "\"max\":", 0, &maxValues);
        readNumberArray(parsed.json, "\"translation\":", 0, &translation);

        // The 1 m sphere is 4x taller after the bake, and 1 m wide still.
        r.check("FSR1C_C1_10_the_sculpt_geometry_carries_the_bodys_own_scale",
                minValues.size() == 3 && std::fabs((maxValues[1] - minValues[1]) - 4.0) < 1e-3
                        && std::fabs((maxValues[0] - minValues[0]) - 1.0) < 1e-3);
        r.check("FSR1C_C1_10_and_the_node_still_carries_only_the_translation",
                translation.size() == 3 && static_cast<float>(translation[0]) == 2.0f
                        && countOccurrences(parsed.json, "\"matrix\":") == 0
                        && countOccurrences(parsed.json, "\"rotation\":") == 0
                        && countOccurrences(parsed.json, "\"scale\":") == 0);
    }

    // Determinism survives the bake.
    {
        Fixture f;
        f.setBox(f.first(), 2.0, 1.0, 0.5);
        f.place(f.first(), placement(1.5, -0.5, 2.25, 370.0, 30.0, 12.25, 1.25, 2.0, 0.5));
        const std::vector<uint8_t> once = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        const std::vector<uint8_t> twice = exportSceneAsGlb(f.scene, ProjectKind::Construction);
        r.check("FSR1C_C1_11_the_same_baked_snapshot_exports_byte_identically", once == twice);
        r.check("FSR1C_C1_11_and_is_not_empty", !once.empty());
    }

    return r.n;
}

}  // namespace forgeshape
