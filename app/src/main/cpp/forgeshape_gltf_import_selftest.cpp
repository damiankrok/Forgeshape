#include "forgeshape_gltf_import_selftest.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_construction.h"
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

            // World = translation * local, which is the whole transform R0
            // supports and the identity the roundtrip rests on.
            const Vec3 world = mesh.worldPosition(0);
            r.check("GLBIR0_05_world_position_is_local_plus_translation",
                    std::fabs(world.x - (mesh.positions[0] + 1.5f)) < 1e-5f
                            && std::fabs(world.y - (mesh.positions[1] - 0.5f)) < 1e-5f
                            && std::fabs(world.z - (mesh.positions[2] + 2.25f)) < 1e-5f);
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
            {"GLBIR0_06_a_node_matrix_is_refused", "\"translation\":",
             "\"matrix\":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],\"unused\":",
             GlbImportStatus::NodeMatrix},
            {"GLBIR0_06_a_non_identity_node_rotation_is_refused", "\"translation\":",
             "\"rotation\":[0,0.7,0,0.7],\"translation\":", GlbImportStatus::NodeRotation},
            {"GLBIR0_06_a_non_identity_node_scale_is_refused", "\"translation\":",
             "\"scale\":[2,2,2],\"translation\":", GlbImportStatus::NodeScale},
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
            {"GLBIR0_07_a_missing_NORMAL_is_refused", "\"NORMAL\"", "\"NORMAL_X\"",
             GlbImportStatus::MissingAttribute},
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

        // The node transform is the file's translation and nothing else.
        const SceneDrawItem& item = preview.snapshot()[0];
        r.check("GLBIR0_08_the_draw_item_carries_the_files_translation",
                item.model.m[12] == 1.0f && item.model.m[13] == 2.0f && item.model.m[14] == 3.0f);
        r.check("GLBIR0_08_and_an_identity_normal_matrix_because_a_translation_turns_nothing",
                item.normalModel.m[0] == 1.0f && item.normalModel.m[5] == 1.0f
                        && item.normalModel.m[10] == 1.0f && item.normalModel.m[1] == 0.0f);

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
                        && std::string(glbImportStatusName(GlbImportStatus::NodeMatrix))
                                == "NodeMatrix"
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
