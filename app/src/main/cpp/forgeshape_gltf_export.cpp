#include "forgeshape_gltf_export.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_sculpt.h"

namespace forgeshape {
namespace {

// --- GLB envelope constants (glTF 2.0, section 4.4) -------------------------
constexpr uint32_t kGlbMagic = 0x46546C67u;  // "glTF", little-endian
constexpr uint32_t kGlbVersion = 2u;
constexpr uint32_t kChunkTypeJson = 0x4E4F534Au;  // "JSON"
constexpr uint32_t kChunkTypeBin = 0x004E4942u;   // "BIN\0"
constexpr uint32_t kGlbHeaderBytes = 12u;
constexpr uint32_t kChunkHeaderBytes = 8u;

// glTF component and target codes, named so no magic number appears below.
constexpr int kComponentFloat = 5126;
constexpr int kComponentUnsignedInt = 5125;
constexpr int kTargetArrayBuffer = 34962;
constexpr int kTargetElementArrayBuffer = 34963;
constexpr int kModeTriangles = 4;

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
    }
}

void appendFloatBytes(std::vector<uint8_t>& out, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    appendU32(out, bits);
}

// A float as JSON text, deterministically.
//
// `%.9g` is the shortest precision that round-trips every float32 exactly, so
// two exports of the same geometry produce the same characters and an importer
// reads back the value ForgeShape held. Non-finite values never reach here —
// they are refused during capture — because JSON has no way to spell them and
// an importer would be handed a syntax error or, worse, a silent NaN.
void appendJsonFloat(std::string& out, float value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(value));
    out += text;
}

void appendJsonInt(std::string& out, long long value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%lld", value);
    out += text;
}

// A body's technical name. Deterministic, derived from identity, and carrying
// no user text: Stage 018 naming does not exist yet, and inventing a display
// name here would be inventing product vocabulary in an exporter.
std::string bodyName(ObjectId objectId) {
    std::string name = "Body_";
    appendJsonInt(name, static_cast<long long>(objectId));
    return name;
}

bool matrixFinite(const Mat4& m) { return mat4Finite(m); }

// The determinant of a matrix's upper-left 3x3.
//
// Computed from the sixteen floats rather than as sx*sy*sz, deliberately. The
// shortcut is only true while `L` really is R*S with R orthonormal; this is a
// check on the matrix that will actually be baked, so it must not assume the
// thing it is checking for.
float linearDeterminant(const Mat4& m) {
    const float a = m.m[0], b = m.m[4], c = m.m[8];
    const float d = m.m[1], e = m.m[5], f = m.m[9];
    const float g = m.m[2], h = m.m[6], i = m.m[10];
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

// Applies the baked linear transform to one body's surfaces, in place.
//
// Positions ride `local` directly. Normals ride `normalMatrix`, which is
// `transpose(inverse(local))` — NOT `local` — because under a non-uniform scale
// those differ and using the wrong one tilts every normal off its surface. It
// is normalised here because a file states unit normals, where the renderer
// could leave that to the fragment stage.
//
// Degenerate normals stay zero rather than becoming a made-up direction: a zero
// normal is a defect a validator names, and a plausible-looking wrong one is a
// defect nothing names.
void bakeLinearTransform(RenderMeshData& mesh, const Mat4& local, const Mat4& normalMatrix) {
    for (RenderVertex& vertex : mesh.vertices) {
        const Vec3 position{vertex.position[0], vertex.position[1], vertex.position[2]};
        const Vec3 baked = mat4TransformPoint(local, position);
        vertex.position[0] = baked.x;
        vertex.position[1] = baked.y;
        vertex.position[2] = baked.z;

        const Vec3 normal{vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        const Vec3 carried = mat4TransformDirection(normalMatrix, normal);
        const float length =
                std::sqrt(carried.x * carried.x + carried.y * carried.y + carried.z * carried.z);
        if (length > 0.0f) {
            vertex.normal[0] = carried.x / length;
            vertex.normal[1] = carried.y / length;
            vertex.normal[2] = carried.z / length;
        } else {
            vertex.normal[0] = 0.0f;
            vertex.normal[1] = 0.0f;
            vertex.normal[2] = 0.0f;
        }
    }
}

// Rounds a byte count up to the next multiple of four.
//
// glTF requires every chunk to be 4-byte aligned, and requires an accessor's
// byteOffset to be a multiple of its component size. Everything written here is
// 4 bytes wide, so keeping every bufferView on a 4-byte boundary satisfies both
// rules at once and leaves no case where one is met and the other is not.
uint64_t alignUp4(uint64_t value) { return (value + 3ull) & ~3ull; }

struct AccessorPlan {
    int componentType = kComponentFloat;
    const char* type = "VEC3";
    uint32_t count = 0;
    uint64_t byteOffset = 0;
    uint64_t byteLength = 0;
    int target = kTargetArrayBuffer;
    bool hasBounds = false;
    float minValue[3] = {0.0f, 0.0f, 0.0f};
    float maxValue[3] = {0.0f, 0.0f, 0.0f};
};

}  // namespace

const char* glbExportStatusName(GlbExportStatus status) {
    switch (status) {
        case GlbExportStatus::Ok: return "Ok";
        case GlbExportStatus::NothingToExport: return "NothingToExport";
        case GlbExportStatus::InvalidMesh: return "InvalidMesh";
        case GlbExportStatus::NonFiniteValue: return "NonFiniteValue";
        case GlbExportStatus::TooLarge: return "TooLarge";
        case GlbExportStatus::SingularTransform: return "SingularTransform";
        case GlbExportStatus::MirroredTransform: return "MirroredTransform";
    }
    return "unknown";
}

GlbExportStatus captureGlbExportScene(const ConstructionScene& scene, ProjectKind kind,
                                      GlbExportScene* out) {
    if (out == nullptr) {
        return GlbExportStatus::NothingToExport;
    }
    GlbExportScene captured;
    const size_t bodyCount = scene.bodyCount();
    if (bodyCount == 0) {
        return GlbExportStatus::NothingToExport;
    }
    captured.bodies.reserve(bodyCount);

    for (size_t i = 0; i < bodyCount; ++i) {
        const SceneObject& body = scene.bodyAt(i);
        GlbExportBody exported;
        exported.objectId = body.objectId();

        // Split the placement: Model = T * L. Both halves come from the ONE
        // composition order in ConstructionTransform, and the translation is
        // read out of the model matrix rather than re-narrowed from the
        // authored doubles, so the exported position is bit-for-bit the
        // position the renderer draws at.
        const Mat4 model = body.transform().modelMatrix();
        exported.local = body.transform().localMatrix();
        const Mat4 normalMatrix = body.transform().normalMatrix();
        if (!matrixFinite(model) || !matrixFinite(exported.local) ||
            !matrixFinite(normalMatrix)) {
            return GlbExportStatus::NonFiniteValue;
        }
        exported.translation[0] = model.m[12];
        exported.translation[1] = model.m[13];
        exported.translation[2] = model.m[14];

        // Winding survives baking only while the linear part preserves
        // orientation. The domain guarantees it — scale is strictly positive —
        // so this is a guard against something being wrong upstream, not a case
        // the product can produce.
        const float determinant = linearDeterminant(exported.local);
        if (!std::isfinite(determinant) || determinant == 0.0f) {
            return GlbExportStatus::SingularTransform;
        }
        if (determinant < 0.0f) {
            return GlbExportStatus::MirroredTransform;
        }

        // WHICH representation. A Sculpt project exports the sculpted body's own
        // mesh; anything else re-evaluates the Construction source. The rule is
        // stated once, here, and is the same rule the header describes.
        const FrozenSculpt& frozen = body.frozenSculpt();
        const bool useSculpt = (kind == ProjectKind::Sculpt) && frozen.mesh.frozen();
        exported.fromSculpt = useSculpt;

        bool built = false;
        if (useSculpt) {
            exported.doubleSided = frozen.mesh.renderBothSides();
            built = buildRenderMesh(frozen.mesh.vertices().data(), frozen.mesh.vertexCount(),
                                    frozen.mesh.indices().data(), frozen.mesh.indexCount(),
                                    SurfaceShading::Smooth, &exported.render,
                                    /*renderBothSides=*/false);
        } else if (const ImportedMesh* imported = body.importedOrNull()) {
            // An Imported Mesh has no parameters to re-evaluate: the geometry
            // IS the truth, and it is exported from the same CPU arrays the
            // body owns — never from a GPU buffer and never from a `.forge`
            // file. Skipping it instead would write a file quietly missing an
            // object the user can see, select and move, which is the one thing
            // this exporter refuses to do anywhere else.
            //
            // `buildDrawData` is what already resolves per-submesh
            // `doubleSided` into geometry, by emitting a reversed copy of a
            // two-sided submesh's triangles. glTF states `doubleSided` per
            // material, and this product exports one material for a whole
            // body, so carrying it in the geometry is the only encoding that
            // keeps a mixed mesh's answer per submesh.
            std::vector<MeshVertex> vertices;
            std::vector<uint32_t> indices;
            exported.doubleSided = false;
            if (imported->buildDrawData(&vertices, &indices)) {
                built = buildRenderMesh(vertices.data(), static_cast<uint32_t>(vertices.size()),
                                        indices.data(), static_cast<uint32_t>(indices.size()),
                                        SurfaceShading::Smooth, &exported.render,
                                        /*renderBothSides=*/false);
            }
        } else if (const CadBody* cad = body.cadOrNull()) {
            // Regenerated NOW from the sketch and the extrusion through the one
            // CAD path, on exactly the terms a primitive is regenerated from
            // its parameters: never a GPU buffer, never a `.forge` file, so an
            // edited dimension is what the file carries.
            ConstructionMesh source;
            exported.doubleSided = false;
            if (cad->generateMesh(&source) == CadStatus::Ok) {
                built = buildRenderMesh(source.vertices.data(),
                                        static_cast<uint32_t>(source.vertices.size()),
                                        source.indices.data(),
                                        static_cast<uint32_t>(source.indices.size()),
                                        SurfaceShading::Smooth, &exported.render,
                                        /*renderBothSides=*/false);
            }
        } else if (body.constructionOrNull() == nullptr) {
            // A representation this exporter does not know. Refused rather
            // than skipped, for the reason stated below.
            return GlbExportStatus::InvalidMesh;
        } else {
            // Generated NOW from the canonical parameters, through the same
            // generator the product publishes from. Nothing is read out of the
            // mesh store, so a body whose GPU copy is stale still exports what
            // it currently IS.
            const ConstructionMesh source = body.construction().generateMesh();
            exported.doubleSided = source.renderBothSides;
            built = buildRenderMesh(source.vertices.data(),
                                    static_cast<uint32_t>(source.vertices.size()),
                                    source.indices.data(),
                                    static_cast<uint32_t>(source.indices.size()),
                                    SurfaceShading::Smooth, &exported.render,
                                    /*renderBothSides=*/false);
        }
        if (!built) {
            // Refused rather than skipped: a file quietly missing a body is
            // worse than no file, because the user would not notice.
            return GlbExportStatus::InvalidMesh;
        }
        if (exported.render.vertices.empty() || exported.render.indices.empty()) {
            return GlbExportStatus::InvalidMesh;
        }

        // Bake LAST, after the surfaces exist and before anything is checked
        // for finiteness, so what is validated is what will be written.
        bakeLinearTransform(exported.render, exported.local, normalMatrix);
        if (!renderMeshIsFinite(exported.render)) {
            return GlbExportStatus::NonFiniteValue;
        }
        captured.bodies.push_back(std::move(exported));
    }

    *out = std::move(captured);
    return GlbExportStatus::Ok;
}

std::vector<uint8_t> encodeGlb(const GlbExportScene& scene, GlbExportStatus* outWhy) {
    const auto fail = [outWhy](GlbExportStatus status) {
        if (outWhy != nullptr) {
            *outWhy = status;
        }
        return std::vector<uint8_t>();
    };
    if (scene.bodies.empty()) {
        return fail(GlbExportStatus::NothingToExport);
    }

    // -----------------------------------------------------------------------
    // Validate the snapshot again, here, at the boundary that writes bytes.
    // -----------------------------------------------------------------------
    //
    // `captureGlbExportScene` already checks all of this, and that is not a
    // reason to skip it. This function is a public entry point: anything that
    // builds a snapshot by hand reaches the file through here, and a writer that
    // trusted its input would emit `"matrix":[nan,...]` — a file that parses as
    // JSON, passes a length check, and puts a NaN into a user's model. A refused
    // export is a bad afternoon; a corrupt file that looks valid is a bad week.
    for (const GlbExportBody& body : scene.bodies) {
        if (!mat4Finite(body.local)) {
            return fail(GlbExportStatus::NonFiniteValue);
        }
        for (float component : body.translation) {
            if (!std::isfinite(component)) {
                return fail(GlbExportStatus::NonFiniteValue);
            }
        }
        // The determinant is re-checked here for the same reason everything
        // else is: this is a public entry point, and a hand-built snapshot
        // reaches the bytes through it. A mirrored bake would produce a file
        // whose every triangle faces inward.
        const float determinant = linearDeterminant(body.local);
        if (!std::isfinite(determinant) || determinant == 0.0f) {
            return fail(GlbExportStatus::SingularTransform);
        }
        if (determinant < 0.0f) {
            return fail(GlbExportStatus::MirroredTransform);
        }
        if (body.render.vertices.empty() || body.render.indices.empty()) {
            return fail(GlbExportStatus::InvalidMesh);
        }
        if ((body.render.indices.size() % 3u) != 0u) {
            return fail(GlbExportStatus::InvalidMesh);
        }
        if (!renderMeshIsFinite(body.render)) {
            return fail(GlbExportStatus::NonFiniteValue);
        }
        const uint32_t vertexCount = body.render.vertexCount();
        for (uint32_t index : body.render.indices) {
            // An out-of-range index would be an accessor pointing outside the
            // vertex data it names — the one structural error a validator
            // catches and a naive importer does not.
            if (index >= vertexCount) {
                return fail(GlbExportStatus::InvalidMesh);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Plan the binary chunk first, so every accessor knows its offset before a
    // single character of JSON is written.
    // -----------------------------------------------------------------------
    std::vector<AccessorPlan> accessors;
    accessors.reserve(scene.bodies.size() * 3);
    uint64_t binBytes = 0;

    for (const GlbExportBody& body : scene.bodies) {
        const uint32_t vertexCount = body.render.vertexCount();
        const uint32_t indexCount = body.render.indexCount();

        AccessorPlan position;
        position.componentType = kComponentFloat;
        position.type = "VEC3";
        position.count = vertexCount;
        position.byteOffset = binBytes;
        position.byteLength = static_cast<uint64_t>(vertexCount) * 12ull;
        position.target = kTargetArrayBuffer;
        position.hasBounds = true;
        // glTF REQUIRES min/max on POSITION, and requires them to bound the
        // data. Computed from the exact floats that will be written, so the
        // bound can never be one ULP short of the geometry it describes.
        for (uint32_t v = 0; v < vertexCount; ++v) {
            const float* p = body.render.vertices[v].position;
            for (int c = 0; c < 3; ++c) {
                if (v == 0 || p[c] < position.minValue[c]) position.minValue[c] = p[c];
                if (v == 0 || p[c] > position.maxValue[c]) position.maxValue[c] = p[c];
            }
        }
        binBytes = alignUp4(binBytes + position.byteLength);
        accessors.push_back(position);

        AccessorPlan normal;
        normal.componentType = kComponentFloat;
        normal.type = "VEC3";
        normal.count = vertexCount;
        normal.byteOffset = binBytes;
        normal.byteLength = static_cast<uint64_t>(vertexCount) * 12ull;
        normal.target = kTargetArrayBuffer;
        binBytes = alignUp4(binBytes + normal.byteLength);
        accessors.push_back(normal);

        AccessorPlan index;
        // uint32 for every mesh, deliberately. The smallest safe type would be
        // uint16 for most bodies, but then the component type would depend on a
        // vertex count, and two projects that differ only in tessellation would
        // produce structurally different files. One type is simpler to validate,
        // always 4-byte aligned, and never one edit away from overflowing.
        index.componentType = kComponentUnsignedInt;
        index.type = "SCALAR";
        index.count = indexCount;
        index.byteOffset = binBytes;
        index.byteLength = static_cast<uint64_t>(indexCount) * 4ull;
        index.target = kTargetElementArrayBuffer;
        binBytes = alignUp4(binBytes + index.byteLength);
        accessors.push_back(index);

        if (binBytes > kMaxGlbBytes) {
            return fail(GlbExportStatus::TooLarge);
        }
    }

    // Materials, created on demand in a fixed order so the file is
    // deterministic: index 0 is the ordinary one-sided surface, and the
    // two-sided one exists only when some body actually needs it.
    bool needsDoubleSided = false;
    for (const GlbExportBody& body : scene.bodies) {
        needsDoubleSided = needsDoubleSided || body.doubleSided;
    }
    const int doubleSidedMaterialIndex = needsDoubleSided ? 1 : -1;

    // -----------------------------------------------------------------------
    // JSON, written by hand in one fixed key order.
    // -----------------------------------------------------------------------
    std::string json;
    json.reserve(1024 + scene.bodies.size() * 512);
    json += "{\"asset\":{\"version\":\"2.0\",\"generator\":\"";
    json += kGlbGenerator;
    json += "\"},\"scene\":0,\"scenes\":[{\"nodes\":[";
    for (size_t i = 0; i < scene.bodies.size(); ++i) {
        if (i > 0) json += ',';
        appendJsonInt(json, static_cast<long long>(i));
    }
    json += "]}],\"nodes\":[";
    for (size_t i = 0; i < scene.bodies.size(); ++i) {
        if (i > 0) json += ',';
        json += "{\"name\":\"";
        json += bodyName(scene.bodies[i].objectId);
        json += "\",\"mesh\":";
        appendJsonInt(json, static_cast<long long>(i));
        // TRANSLATION ONLY. No "rotation", no "scale" and no "matrix": both are
        // baked into the mesh, so a consumer reads identity for each, which is
        // the whole point of ARCH-OWNER-07. Writing an identity quaternion and
        // a [1,1,1] scale would say the same thing in more bytes; omitting them
        // is what glTF means by a default.
        json += ",\"translation\":[";
        for (int c = 0; c < 3; ++c) {
            if (c > 0) json += ',';
            appendJsonFloat(json, scene.bodies[i].translation[c]);
        }
        json += "]}";
    }
    json += "],\"meshes\":[";
    for (size_t i = 0; i < scene.bodies.size(); ++i) {
        if (i > 0) json += ',';
        json += "{\"name\":\"";
        json += bodyName(scene.bodies[i].objectId);
        json += "\",\"primitives\":[{\"attributes\":{\"POSITION\":";
        appendJsonInt(json, static_cast<long long>(i * 3 + 0));
        json += ",\"NORMAL\":";
        appendJsonInt(json, static_cast<long long>(i * 3 + 1));
        json += "},\"indices\":";
        appendJsonInt(json, static_cast<long long>(i * 3 + 2));
        json += ",\"material\":";
        appendJsonInt(json, scene.bodies[i].doubleSided ? doubleSidedMaterialIndex : 0);
        json += ",\"mode\":";
        appendJsonInt(json, kModeTriangles);
        json += "}]}";
    }

    // One neutral material, and a two-sided twin only when something needs it.
    //
    // A material exists at all because glTF expresses sidedness on the material
    // and because several importers treat a primitive with no material as
    // untextured-and-unlit rather than as a default surface. It authors no
    // colour choice, no texture and no PBR intent: base colour is white, metallic
    // is zero and roughness is one, which is the neutral matte every viewer
    // agrees on.
    json += "],\"materials\":[";
    json += "{\"name\":\"ForgeShape Surface\",\"pbrMetallicRoughness\":{\"baseColorFactor\":"
            "[1,1,1,1],\"metallicFactor\":0,\"roughnessFactor\":1},\"doubleSided\":false}";
    if (needsDoubleSided) {
        json += ",{\"name\":\"ForgeShape Surface Two Sided\",\"pbrMetallicRoughness\":"
                "{\"baseColorFactor\":[1,1,1,1],\"metallicFactor\":0,\"roughnessFactor\":1},"
                "\"doubleSided\":true}";
    }

    json += "],\"bufferViews\":[";
    for (size_t a = 0; a < accessors.size(); ++a) {
        if (a > 0) json += ',';
        json += "{\"buffer\":0,\"byteOffset\":";
        appendJsonInt(json, static_cast<long long>(accessors[a].byteOffset));
        json += ",\"byteLength\":";
        appendJsonInt(json, static_cast<long long>(accessors[a].byteLength));
        json += ",\"target\":";
        appendJsonInt(json, accessors[a].target);
        json += '}';
    }
    json += "],\"accessors\":[";
    for (size_t a = 0; a < accessors.size(); ++a) {
        const AccessorPlan& plan = accessors[a];
        if (a > 0) json += ',';
        json += "{\"bufferView\":";
        appendJsonInt(json, static_cast<long long>(a));
        json += ",\"componentType\":";
        appendJsonInt(json, plan.componentType);
        json += ",\"count\":";
        appendJsonInt(json, plan.count);
        json += ",\"type\":\"";
        json += plan.type;
        json += '"';
        if (plan.hasBounds) {
            json += ",\"min\":[";
            for (int c = 0; c < 3; ++c) {
                if (c > 0) json += ',';
                appendJsonFloat(json, plan.minValue[c]);
            }
            json += "],\"max\":[";
            for (int c = 0; c < 3; ++c) {
                if (c > 0) json += ',';
                appendJsonFloat(json, plan.maxValue[c]);
            }
            json += ']';
        }
        json += '}';
    }
    json += "],\"buffers\":[{\"byteLength\":";
    appendJsonInt(json, static_cast<long long>(binBytes));
    json += "}]}";

    // -----------------------------------------------------------------------
    // Assemble. Both chunks are padded to four bytes — JSON with SPACES and BIN
    // with ZEROS, which is what the specification requires rather than a
    // stylistic choice: a parser is allowed to read the JSON chunk as text.
    // -----------------------------------------------------------------------
    const uint64_t jsonPadded = alignUp4(json.size());
    const uint64_t binPadded = alignUp4(binBytes);
    const uint64_t total = kGlbHeaderBytes + kChunkHeaderBytes + jsonPadded
                           + kChunkHeaderBytes + binPadded;
    if (total > kMaxGlbBytes) {
        return fail(GlbExportStatus::TooLarge);
    }

    std::vector<uint8_t> glb;
    glb.reserve(static_cast<size_t>(total));
    appendU32(glb, kGlbMagic);
    appendU32(glb, kGlbVersion);
    appendU32(glb, static_cast<uint32_t>(total));

    appendU32(glb, static_cast<uint32_t>(jsonPadded));
    appendU32(glb, kChunkTypeJson);
    glb.insert(glb.end(), json.begin(), json.end());
    for (uint64_t pad = json.size(); pad < jsonPadded; ++pad) {
        glb.push_back(0x20);  // space
    }

    appendU32(glb, static_cast<uint32_t>(binPadded));
    appendU32(glb, kChunkTypeBin);
    const size_t binStart = glb.size();
    glb.resize(binStart + static_cast<size_t>(binPadded), 0);

    size_t accessorIndex = 0;
    for (const GlbExportBody& body : scene.bodies) {
        std::vector<uint8_t> scratch;

        // POSITION
        scratch.clear();
        scratch.reserve(body.render.vertices.size() * 12);
        for (const RenderVertex& vertex : body.render.vertices) {
            appendFloatBytes(scratch, vertex.position[0]);
            appendFloatBytes(scratch, vertex.position[1]);
            appendFloatBytes(scratch, vertex.position[2]);
        }
        std::memcpy(glb.data() + binStart + accessors[accessorIndex].byteOffset, scratch.data(),
                    scratch.size());
        ++accessorIndex;

        // NORMAL
        scratch.clear();
        for (const RenderVertex& vertex : body.render.vertices) {
            appendFloatBytes(scratch, vertex.normal[0]);
            appendFloatBytes(scratch, vertex.normal[1]);
            appendFloatBytes(scratch, vertex.normal[2]);
        }
        std::memcpy(glb.data() + binStart + accessors[accessorIndex].byteOffset, scratch.data(),
                    scratch.size());
        ++accessorIndex;

        // INDICES — copied unreversed. ForgeShape's winding is already glTF's.
        scratch.clear();
        scratch.reserve(body.render.indices.size() * 4);
        for (uint32_t index : body.render.indices) {
            appendU32(scratch, index);
        }
        std::memcpy(glb.data() + binStart + accessors[accessorIndex].byteOffset, scratch.data(),
                    scratch.size());
        ++accessorIndex;
    }

    if (outWhy != nullptr) {
        *outWhy = GlbExportStatus::Ok;
    }
    return glb;
}

std::vector<uint8_t> exportSceneAsGlb(const ConstructionScene& scene, ProjectKind kind,
                                      GlbExportStatus* outWhy) {
    GlbExportScene captured;
    const GlbExportStatus status = captureGlbExportScene(scene, kind, &captured);
    if (status != GlbExportStatus::Ok) {
        if (outWhy != nullptr) {
            *outWhy = status;
        }
        return {};
    }
    return encodeGlb(captured, outWhy);
}

}  // namespace forgeshape
