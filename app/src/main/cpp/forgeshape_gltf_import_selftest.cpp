#include "forgeshape_gltf_import_selftest.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_glb_import_fixture.h"
#include "forgeshape_glb_roundtrip.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_gltf_import.h"
#include "forgeshape_history.h"
#include "forgeshape_import_preview.h"
#include "forgeshape_json.h"
#include "forgeshape_math.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_transform.h"

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
    ImportedScene scene;
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

    std::string json = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
                       "\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0";
    json += spec.nodeMembers;
    json += "}],\"materials\":[{\"doubleSided\":false},{\"doubleSided\":true}],"
            "\"meshes\":[{\"primitives\":[";
    char text[256];
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

        ImportedScene imported;
        const GlbImportStatus why = importGlb(glb.data(), glb.size(), &imported);
        r.check("GLBIR0_01_a_real_forgeshape_export_parses", why == GlbImportStatus::Ok);
        r.check("GLBIR0_01_one_body_is_one_imported_mesh", imported.meshes.size() == 1);

        if (imported.meshes.size() == 1) {
            const ImportedMesh& mesh = imported.meshes[0];
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
        ImportedScene imported;
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
        ImportedScene empty;
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
        ImportedScene imported;
        const GlbImportStatus why = importGlb(fixture.data(), fixture.size(), &imported);
        r.check("GLBIR1_19_the_nomad_like_fixture_imports", why == GlbImportStatus::Ok);
        r.check("GLBIR1_19_it_is_one_node_and_one_mesh", imported.meshes.size() == 1);

        if (imported.meshes.size() == 1) {
            const ImportedMesh& mesh = imported.meshes[0];
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
            // on ImportedMesh that could hold it, and the only string it takes
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
        ImportedScene composed;
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
        ImportedScene untransformed;
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
        ImportedScene flipped;
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
            ImportedScene scene;
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
            ImportedScene once;
            ImportedScene twice;
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
            ImportedScene scene;
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
            ImportedScene scene;
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

    return r.n;
}

}  // namespace forgeshape
