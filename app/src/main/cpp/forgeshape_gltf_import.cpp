#include "forgeshape_gltf_import.h"

#include <cmath>
#include <cstring>

#include "forgeshape_json.h"

namespace forgeshape {
namespace {

// The GLB container's own constants, read from the specification rather than
// from the writer's header. They are the same numbers because they are the same
// specification; what matters is that a mistake in one file cannot cancel a
// mistake in the other.
constexpr uint32_t kMagic = 0x46546C67u;      // "glTF"
constexpr uint32_t kVersion = 2u;
constexpr uint32_t kChunkJson = 0x4E4F534Au;  // "JSON"
constexpr uint32_t kChunkBin = 0x004E4942u;   // "BIN\0"

// glTF accessor component types.
constexpr int kByte = 5120;
constexpr int kUnsignedByte = 5121;
constexpr int kShort = 5122;
constexpr int kUnsignedShort = 5123;
constexpr int kUnsignedInt = 5125;
constexpr int kFloat = 5126;

constexpr int kModeTriangles = 4;

uint32_t readU32LE(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8)
            | (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

float readF32LE(const uint8_t* bytes) {
    const uint32_t bits = readU32LE(bytes);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// A double from the JSON that must be a whole number in range.
//
// glTF states counts and indices as JSON numbers, which have no integer type,
// so "2.5 accessors" and "1e30 vertices" are both things a file can say. Both
// are refused here rather than truncated, because a truncated count reads a
// different number of bytes than the file described.
bool asIndex(double value, uint32_t limit, uint32_t* out) {
    if (!std::isfinite(value) || value < 0.0 || value != std::floor(value)) {
        return false;
    }
    if (value > static_cast<double>(limit)) {
        return false;
    }
    *out = static_cast<uint32_t>(value);
    return true;
}

// Everything the reader needs to resolve one accessor, gathered and checked
// before a byte is read through it.
struct AccessorView {
    uint32_t count = 0;
    uint32_t components = 0;
    int componentType = 0;
    uint64_t byteOffset = 0;  // absolute, into the BIN chunk
    uint64_t byteLength = 0;
};

uint32_t componentSize(int componentType) {
    switch (componentType) {
        case kByte:
        case kUnsignedByte: return 1;
        case kShort:
        case kUnsignedShort: return 2;
        case kUnsignedInt:
        case kFloat: return 4;
        default: return 0;
    }
}

bool componentsForType(const std::string& type, uint32_t* out) {
    if (type == "SCALAR") { *out = 1; return true; }
    if (type == "VEC2") { *out = 2; return true; }
    if (type == "VEC3") { *out = 3; return true; }
    if (type == "VEC4") { *out = 4; return true; }
    if (type == "MAT2") { *out = 4; return true; }
    if (type == "MAT3") { *out = 9; return true; }
    if (type == "MAT4") { *out = 16; return true; }
    return false;
}

// A three- or four-number array member, when present.
bool readNumbers(const JsonDocument& doc, const JsonValue& object, const char* key, size_t expected,
                 double* out, bool* present) {
    const JsonValue* array = doc.member(object, key);
    *present = array != nullptr;
    if (array == nullptr) {
        return true;
    }
    if (array->type != JsonType::Array || array->children.size() != expected) {
        return false;
    }
    for (size_t i = 0; i < expected; ++i) {
        const JsonValue* element = doc.element(*array, i);
        if (element == nullptr || element->type != JsonType::Number) {
            return false;
        }
        out[i] = element->number;
    }
    return true;
}

}  // namespace

const char* glbImportStatusName(GlbImportStatus status) {
    switch (status) {
        case GlbImportStatus::Ok: return "Ok";
        case GlbImportStatus::NoData: return "NoData";
        case GlbImportStatus::NotGlb: return "NotGlb";
        case GlbImportStatus::UnsupportedVersion: return "UnsupportedVersion";
        case GlbImportStatus::TruncatedFile: return "TruncatedFile";
        case GlbImportStatus::ChunkMisaligned: return "ChunkMisaligned";
        case GlbImportStatus::MissingJsonChunk: return "MissingJsonChunk";
        case GlbImportStatus::MissingBinChunk: return "MissingBinChunk";
        case GlbImportStatus::UnknownChunk: return "UnknownChunk";
        case GlbImportStatus::MalformedJson: return "MalformedJson";
        case GlbImportStatus::UnsupportedAssetVersion: return "UnsupportedAssetVersion";
        case GlbImportStatus::NoScene: return "NoScene";
        case GlbImportStatus::NoMeshes: return "NoMeshes";
        case GlbImportStatus::NothingToImport: return "NothingToImport";
        case GlbImportStatus::ExternalBuffer: return "ExternalBuffer";
        case GlbImportStatus::UnsupportedExtension: return "UnsupportedExtension";
        case GlbImportStatus::HasAnimation: return "HasAnimation";
        case GlbImportStatus::HasSkin: return "HasSkin";
        case GlbImportStatus::SparseAccessor: return "SparseAccessor";
        case GlbImportStatus::MorphTargets: return "MorphTargets";
        case GlbImportStatus::NonTriangleMode: return "NonTriangleMode";
        case GlbImportStatus::NodeHierarchy: return "NodeHierarchy";
        case GlbImportStatus::NodeMatrix: return "NodeMatrix";
        case GlbImportStatus::NodeRotation: return "NodeRotation";
        case GlbImportStatus::NodeScale: return "NodeScale";
        case GlbImportStatus::MissingAttribute: return "MissingAttribute";
        case GlbImportStatus::UnsupportedComponentType: return "UnsupportedComponentType";
        case GlbImportStatus::UnsupportedAccessorType: return "UnsupportedAccessorType";
        case GlbImportStatus::InterleavedAccessor: return "InterleavedAccessor";
        case GlbImportStatus::AccessorOutOfRange: return "AccessorOutOfRange";
        case GlbImportStatus::IndexOutOfRange: return "IndexOutOfRange";
        case GlbImportStatus::IndexCountNotTriangles: return "IndexCountNotTriangles";
        case GlbImportStatus::CountMismatch: return "CountMismatch";
        case GlbImportStatus::NonFiniteValue: return "NonFiniteValue";
        case GlbImportStatus::TooLarge: return "TooLarge";
    }
    return "unknown";
}

Vec3 ImportedMesh::worldPosition(uint32_t vertex) const {
    const size_t at = static_cast<size_t>(vertex) * 3u;
    if (at + 3 > positions.size()) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    return mat4TransformPoint(nodeTransform,
                              Vec3{positions[at], positions[at + 1], positions[at + 2]});
}

Vec3 ImportedMesh::worldNormal(uint32_t vertex) const {
    const size_t at = static_cast<size_t>(vertex) * 3u;
    if (at + 3 > normals.size()) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    return mat4TransformDirection(nodeTransform,
                                  Vec3{normals[at], normals[at + 1], normals[at + 2]});
}

uint32_t ImportedScene::totalVertices() const {
    uint32_t total = 0;
    for (const ImportedMesh& mesh : meshes) {
        total += mesh.vertexCount();
    }
    return total;
}

uint32_t ImportedScene::totalTriangles() const {
    uint32_t total = 0;
    for (const ImportedMesh& mesh : meshes) {
        total += mesh.triangleCount();
    }
    return total;
}

GlbImportStatus importGlb(const uint8_t* bytes, size_t length, ImportedScene* out) {
    if (out == nullptr || bytes == nullptr || length == 0) {
        return GlbImportStatus::NoData;
    }
    if (length > kMaxImportedGlbBytes) {
        return GlbImportStatus::TooLarge;
    }
    if (length < 12) {
        return GlbImportStatus::TruncatedFile;
    }

    // -----------------------------------------------------------------------
    // The container, walked from its own headers
    // -----------------------------------------------------------------------
    if (readU32LE(bytes) != kMagic) {
        return GlbImportStatus::NotGlb;
    }
    if (readU32LE(bytes + 4) != kVersion) {
        return GlbImportStatus::UnsupportedVersion;
    }
    const uint32_t declared = readU32LE(bytes + 8);
    if (declared != length) {
        // Not merely "at most": a GLB states its own total, and a file that
        // disagrees with itself is one this diagnostic must not guess about.
        return GlbImportStatus::TruncatedFile;
    }

    const uint8_t* json = nullptr;
    uint32_t jsonLength = 0;
    const uint8_t* bin = nullptr;
    uint32_t binLength = 0;

    size_t offset = 12;
    while (offset + 8 <= length) {
        const uint32_t chunkLength = readU32LE(bytes + offset);
        const uint32_t chunkType = readU32LE(bytes + offset + 4);
        if ((chunkLength % 4u) != 0u) {
            return GlbImportStatus::ChunkMisaligned;
        }
        if (static_cast<uint64_t>(offset) + 8ull + chunkLength > length) {
            return GlbImportStatus::TruncatedFile;
        }
        const uint8_t* payload = bytes + offset + 8;
        if (chunkType == kChunkJson) {
            if (json != nullptr) {
                return GlbImportStatus::UnknownChunk;  // a second JSON chunk
            }
            json = payload;
            jsonLength = chunkLength;
        } else if (chunkType == kChunkBin) {
            if (bin != nullptr) {
                return GlbImportStatus::UnknownChunk;
            }
            bin = payload;
            binLength = chunkLength;
        } else {
            // The specification says an unknown chunk may be skipped. R0 does
            // not: this reader exists to say exactly what a file contains, and
            // silently skipping a chunk is how it would fail to.
            return GlbImportStatus::UnknownChunk;
        }
        offset += 8u + chunkLength;
    }
    if (offset != length) {
        return GlbImportStatus::TruncatedFile;
    }
    if (json == nullptr) {
        return GlbImportStatus::MissingJsonChunk;
    }
    if (bin == nullptr || binLength == 0) {
        return GlbImportStatus::MissingBinChunk;
    }

    JsonDocument doc;
    if (doc.parse(reinterpret_cast<const char*>(json), jsonLength) != JsonStatus::Ok) {
        return GlbImportStatus::MalformedJson;
    }
    const JsonValue& root = doc.root();
    if (root.type != JsonType::Object) {
        return GlbImportStatus::MalformedJson;
    }

    // -----------------------------------------------------------------------
    // The R0 subset boundary, checked before anything is decoded
    // -----------------------------------------------------------------------
    {
        const JsonValue* asset = doc.member(root, "asset");
        std::string version;
        if (asset == nullptr || !doc.stringMember(*asset, "version", &version)
            || version.rfind("2.", 0) != 0) {
            return GlbImportStatus::UnsupportedAssetVersion;
        }
    }
    if (doc.member(root, "animations") != nullptr) {
        return GlbImportStatus::HasAnimation;
    }
    if (doc.member(root, "skins") != nullptr) {
        return GlbImportStatus::HasSkin;
    }
    if (const JsonValue* required = doc.member(root, "extensionsRequired")) {
        // Any required extension at all. R0 implements none, and an extension
        // is required precisely because a reader that ignores it reads the file
        // wrong.
        if (required->type != JsonType::Array || !required->children.empty()) {
            return GlbImportStatus::UnsupportedExtension;
        }
    }

    {
        const JsonValue* buffers = doc.member(root, "buffers");
        if (buffers == nullptr || buffers->type != JsonType::Array
            || buffers->children.empty()) {
            return GlbImportStatus::MissingBinChunk;
        }
        for (size_t i = 0; i < buffers->children.size(); ++i) {
            const JsonValue* buffer = doc.element(*buffers, i);
            if (buffer == nullptr || buffer->type != JsonType::Object) {
                return GlbImportStatus::MalformedJson;
            }
            if (doc.member(*buffer, "uri") != nullptr) {
                return GlbImportStatus::ExternalBuffer;
            }
        }
        if (buffers->children.size() != 1) {
            // A GLB's embedded buffer is buffer 0; a second buffer with no uri
            // has nowhere to live.
            return GlbImportStatus::ExternalBuffer;
        }
        double byteLength = 0.0;
        const JsonValue* buffer = doc.element(*buffers, 0);
        if (!doc.numberMember(*buffer, "byteLength", &byteLength)) {
            return GlbImportStatus::MalformedJson;
        }
        uint32_t declaredBufferBytes = 0;
        if (!asIndex(byteLength, binLength, &declaredBufferBytes)) {
            // Larger than the BIN chunk actually carries.
            return GlbImportStatus::AccessorOutOfRange;
        }
    }
    if (const JsonValue* images = doc.member(root, "images")) {
        if (images->type == JsonType::Array && !images->children.empty()) {
            return GlbImportStatus::ExternalBuffer;
        }
    }

    const JsonValue* bufferViews = doc.member(root, "bufferViews");
    const JsonValue* accessors = doc.member(root, "accessors");
    const JsonValue* meshes = doc.member(root, "meshes");
    const JsonValue* nodes = doc.member(root, "nodes");
    const JsonValue* scenes = doc.member(root, "scenes");
    if (meshes == nullptr || meshes->type != JsonType::Array || meshes->children.empty()) {
        return GlbImportStatus::NoMeshes;
    }
    if (bufferViews == nullptr || accessors == nullptr || nodes == nullptr
        || bufferViews->type != JsonType::Array || accessors->type != JsonType::Array
        || nodes->type != JsonType::Array) {
        return GlbImportStatus::MalformedJson;
    }
    if (scenes == nullptr || scenes->type != JsonType::Array || scenes->children.empty()) {
        return GlbImportStatus::NoScene;
    }
    double sceneIndexValue = 0.0;
    uint32_t sceneIndex = 0;
    if (!doc.numberMember(root, "scene", &sceneIndexValue)
        || !asIndex(sceneIndexValue, static_cast<uint32_t>(scenes->children.size() - 1),
                    &sceneIndex)) {
        return GlbImportStatus::NoScene;
    }
    const JsonValue* scene = doc.element(*scenes, sceneIndex);
    const JsonValue* sceneNodes = scene == nullptr ? nullptr : doc.member(*scene, "nodes");
    if (sceneNodes == nullptr || sceneNodes->type != JsonType::Array
        || sceneNodes->children.empty()) {
        return GlbImportStatus::NoScene;
    }
    if (sceneNodes->children.size() > kMaxImportedMeshes) {
        return GlbImportStatus::TooLarge;
    }

    // -----------------------------------------------------------------------
    // Accessor resolution, every bound re-derived
    // -----------------------------------------------------------------------
    const auto resolveAccessor = [&](uint32_t accessorIndex, AccessorView* view,
                                     GlbImportStatus* why) -> bool {
        const JsonValue* accessor = doc.element(*accessors, accessorIndex);
        if (accessor == nullptr || accessor->type != JsonType::Object) {
            *why = GlbImportStatus::AccessorOutOfRange;
            return false;
        }
        if (doc.member(*accessor, "sparse") != nullptr) {
            *why = GlbImportStatus::SparseAccessor;
            return false;
        }
        double countValue = 0.0;
        double componentTypeValue = 0.0;
        std::string type;
        if (!doc.numberMember(*accessor, "count", &countValue)
            || !doc.numberMember(*accessor, "componentType", &componentTypeValue)
            || !doc.stringMember(*accessor, "type", &type)) {
            *why = GlbImportStatus::MalformedJson;
            return false;
        }
        if (!asIndex(countValue, kMaxImportedVerticesPerMesh * 3u, &view->count)) {
            *why = GlbImportStatus::TooLarge;
            return false;
        }
        if (!componentsForType(type, &view->components)) {
            *why = GlbImportStatus::UnsupportedAccessorType;
            return false;
        }
        view->componentType = static_cast<int>(componentTypeValue);
        const uint32_t element = componentSize(view->componentType);
        if (element == 0) {
            *why = GlbImportStatus::UnsupportedComponentType;
            return false;
        }

        double viewIndexValue = 0.0;
        uint32_t viewIndex = 0;
        if (!doc.numberMember(*accessor, "bufferView", &viewIndexValue)
            || !asIndex(viewIndexValue, static_cast<uint32_t>(bufferViews->children.size() - 1),
                        &viewIndex)) {
            *why = GlbImportStatus::AccessorOutOfRange;
            return false;
        }
        const JsonValue* bufferView = doc.element(*bufferViews, viewIndex);
        if (bufferView == nullptr || bufferView->type != JsonType::Object) {
            *why = GlbImportStatus::AccessorOutOfRange;
            return false;
        }
        double bufferIndexValue = 0.0;
        if (doc.numberMember(*bufferView, "buffer", &bufferIndexValue)
            && bufferIndexValue != 0.0) {
            *why = GlbImportStatus::ExternalBuffer;
            return false;
        }
        double stride = 0.0;
        if (doc.numberMember(*bufferView, "byteStride", &stride)) {
            // R0 reads tightly packed data only. An interleaved buffer is a
            // valid file this reader would silently mis-stride, so it is
            // refused by name rather than read wrong.
            const double tight = static_cast<double>(element) * view->components;
            if (stride != 0.0 && stride != tight) {
                *why = GlbImportStatus::InterleavedAccessor;
                return false;
            }
        }
        double viewOffsetValue = 0.0;
        double viewLengthValue = 0.0;
        doc.numberMember(*bufferView, "byteOffset", &viewOffsetValue);
        if (!doc.numberMember(*bufferView, "byteLength", &viewLengthValue)) {
            *why = GlbImportStatus::MalformedJson;
            return false;
        }
        uint32_t viewOffset = 0;
        uint32_t viewLength = 0;
        if (!asIndex(viewOffsetValue, binLength, &viewOffset)
            || !asIndex(viewLengthValue, binLength, &viewLength)) {
            *why = GlbImportStatus::AccessorOutOfRange;
            return false;
        }
        double accessorOffsetValue = 0.0;
        doc.numberMember(*accessor, "byteOffset", &accessorOffsetValue);
        uint32_t accessorOffset = 0;
        if (!asIndex(accessorOffsetValue, binLength, &accessorOffset)) {
            *why = GlbImportStatus::AccessorOutOfRange;
            return false;
        }

        const uint64_t need = static_cast<uint64_t>(view->count) * view->components * element;
        const uint64_t start = static_cast<uint64_t>(viewOffset) + accessorOffset;
        if (start + need > static_cast<uint64_t>(viewOffset) + viewLength
            || start + need > binLength) {
            *why = GlbImportStatus::AccessorOutOfRange;
            return false;
        }
        view->byteOffset = start;
        view->byteLength = need;
        return true;
    };

    const auto readVec3Floats = [&](const AccessorView& view, std::vector<float>* dest,
                                    GlbImportStatus* why) -> bool {
        if (view.components != 3) {
            *why = GlbImportStatus::UnsupportedAccessorType;
            return false;
        }
        if (view.componentType != kFloat) {
            *why = GlbImportStatus::UnsupportedComponentType;
            return false;
        }
        dest->resize(static_cast<size_t>(view.count) * 3u);
        for (uint32_t i = 0; i < view.count * 3u; ++i) {
            const float value = readF32LE(bin + view.byteOffset + static_cast<size_t>(i) * 4u);
            if (!std::isfinite(value)) {
                *why = GlbImportStatus::NonFiniteValue;
                return false;
            }
            (*dest)[i] = value;
        }
        return true;
    };

    const auto readIndices = [&](const AccessorView& view, std::vector<uint32_t>* dest,
                                 GlbImportStatus* why) -> bool {
        if (view.components != 1) {
            *why = GlbImportStatus::UnsupportedAccessorType;
            return false;
        }
        const uint32_t element = componentSize(view.componentType);
        if (view.componentType != kUnsignedByte && view.componentType != kUnsignedShort
            && view.componentType != kUnsignedInt) {
            *why = GlbImportStatus::UnsupportedComponentType;
            return false;
        }
        dest->resize(view.count);
        for (uint32_t i = 0; i < view.count; ++i) {
            const uint8_t* at = bin + view.byteOffset + static_cast<size_t>(i) * element;
            switch (view.componentType) {
                case kUnsignedByte: (*dest)[i] = at[0]; break;
                case kUnsignedShort:
                    (*dest)[i] = static_cast<uint32_t>(at[0])
                            | (static_cast<uint32_t>(at[1]) << 8);
                    break;
                default: (*dest)[i] = readU32LE(at); break;
            }
        }
        return true;
    };

    // -----------------------------------------------------------------------
    // The scene, node by node
    // -----------------------------------------------------------------------
    ImportedScene imported;
    imported.meshes.reserve(sceneNodes->children.size());

    for (size_t i = 0; i < sceneNodes->children.size(); ++i) {
        const JsonValue* nodeIndexValue = doc.element(*sceneNodes, i);
        if (nodeIndexValue == nullptr || nodeIndexValue->type != JsonType::Number) {
            return GlbImportStatus::MalformedJson;
        }
        uint32_t nodeIndex = 0;
        if (!asIndex(nodeIndexValue->number, static_cast<uint32_t>(nodes->children.size() - 1),
                     &nodeIndex)) {
            return GlbImportStatus::NoScene;
        }
        const JsonValue* node = doc.element(*nodes, nodeIndex);
        if (node == nullptr || node->type != JsonType::Object) {
            return GlbImportStatus::MalformedJson;
        }
        if (doc.member(*node, "children") != nullptr) {
            return GlbImportStatus::NodeHierarchy;
        }
        if (doc.member(*node, "matrix") != nullptr) {
            return GlbImportStatus::NodeMatrix;
        }

        ImportedMesh mesh;
        doc.stringMember(*node, "name", &mesh.name);

        double rotation[4] = {0.0, 0.0, 0.0, 1.0};
        bool hasRotation = false;
        if (!readNumbers(doc, *node, "rotation", 4, rotation, &hasRotation)) {
            return GlbImportStatus::MalformedJson;
        }
        if (hasRotation
            && !(rotation[0] == 0.0 && rotation[1] == 0.0 && rotation[2] == 0.0
                 && rotation[3] == 1.0)) {
            // Stated and not identity. R0 does not compose a quaternion, and
            // ignoring one would move every vertex of the preview.
            return GlbImportStatus::NodeRotation;
        }
        double scale[3] = {1.0, 1.0, 1.0};
        bool hasScale = false;
        if (!readNumbers(doc, *node, "scale", 3, scale, &hasScale)) {
            return GlbImportStatus::MalformedJson;
        }
        if (hasScale && !(scale[0] == 1.0 && scale[1] == 1.0 && scale[2] == 1.0)) {
            return GlbImportStatus::NodeScale;
        }
        double translation[3] = {0.0, 0.0, 0.0};
        bool hasTranslation = false;
        if (!readNumbers(doc, *node, "translation", 3, translation, &hasTranslation)) {
            return GlbImportStatus::MalformedJson;
        }
        for (int c = 0; c < 3; ++c) {
            if (!std::isfinite(translation[c])) {
                return GlbImportStatus::NonFiniteValue;
            }
            mesh.translation[c] = static_cast<float>(translation[c]);
        }
        mesh.nodeTransform = mat4Translation(
                Vec3{mesh.translation[0], mesh.translation[1], mesh.translation[2]});

        double meshIndexValue = 0.0;
        uint32_t meshIndex = 0;
        if (!doc.numberMember(*node, "mesh", &meshIndexValue)
            || !asIndex(meshIndexValue, static_cast<uint32_t>(meshes->children.size() - 1),
                        &meshIndex)) {
            // A node with no mesh draws nothing. R0 refuses rather than skips:
            // a preview quietly missing an object is the exact failure this
            // diagnostic is supposed to detect, not commit.
            return GlbImportStatus::NothingToImport;
        }
        const JsonValue* meshObject = doc.element(*meshes, meshIndex);
        const JsonValue* primitives =
                meshObject == nullptr ? nullptr : doc.member(*meshObject, "primitives");
        if (primitives == nullptr || primitives->type != JsonType::Array
            || primitives->children.empty()) {
            return GlbImportStatus::NoMeshes;
        }

        // Every primitive of the mesh is appended into one preview mesh, with
        // its indices rebased. R0 files carry one primitive each; handling more
        // costs three lines and removes a whole class of "it drew only part of
        // it" confusion from the diagnostic.
        for (size_t p = 0; p < primitives->children.size(); ++p) {
            const JsonValue* primitive = doc.element(*primitives, p);
            if (primitive == nullptr || primitive->type != JsonType::Object) {
                return GlbImportStatus::MalformedJson;
            }
            double mode = kModeTriangles;
            doc.numberMember(*primitive, "mode", &mode);
            if (mode != kModeTriangles) {
                return GlbImportStatus::NonTriangleMode;
            }
            if (doc.member(*primitive, "targets") != nullptr) {
                return GlbImportStatus::MorphTargets;
            }
            const JsonValue* attributes = doc.member(*primitive, "attributes");
            if (attributes == nullptr || attributes->type != JsonType::Object) {
                return GlbImportStatus::MissingAttribute;
            }
            double positionIndex = 0.0;
            double normalIndex = 0.0;
            if (!doc.numberMember(*attributes, "POSITION", &positionIndex)
                || !doc.numberMember(*attributes, "NORMAL", &normalIndex)) {
                return GlbImportStatus::MissingAttribute;
            }
            double indexAccessorValue = 0.0;
            if (!doc.numberMember(*primitive, "indices", &indexAccessorValue)) {
                // R0 reads indexed triangles only. A non-indexed primitive is a
                // different decode path and is not what ForgeShape writes.
                return GlbImportStatus::MissingAttribute;
            }

            const uint32_t accessorLimit = static_cast<uint32_t>(accessors->children.size() - 1);
            uint32_t positionAccessor = 0;
            uint32_t normalAccessor = 0;
            uint32_t indexAccessor = 0;
            if (!asIndex(positionIndex, accessorLimit, &positionAccessor)
                || !asIndex(normalIndex, accessorLimit, &normalAccessor)
                || !asIndex(indexAccessorValue, accessorLimit, &indexAccessor)) {
                return GlbImportStatus::AccessorOutOfRange;
            }

            GlbImportStatus why = GlbImportStatus::Ok;
            AccessorView positionView;
            AccessorView normalView;
            AccessorView indexView;
            if (!resolveAccessor(positionAccessor, &positionView, &why)
                || !resolveAccessor(normalAccessor, &normalView, &why)
                || !resolveAccessor(indexAccessor, &indexView, &why)) {
                return why;
            }
            if (positionView.count != normalView.count) {
                return GlbImportStatus::CountMismatch;
            }
            if ((indexView.count % 3u) != 0u) {
                return GlbImportStatus::IndexCountNotTriangles;
            }
            if (positionView.count == 0 || indexView.count == 0) {
                return GlbImportStatus::NothingToImport;
            }

            std::vector<float> positions;
            std::vector<float> normals;
            std::vector<uint32_t> indices;
            if (!readVec3Floats(positionView, &positions, &why)
                || !readVec3Floats(normalView, &normals, &why)
                || !readIndices(indexView, &indices, &why)) {
                return why;
            }
            for (uint32_t index : indices) {
                if (index >= positionView.count) {
                    return GlbImportStatus::IndexOutOfRange;
                }
            }

            const uint32_t base = mesh.vertexCount();
            if (static_cast<uint64_t>(base) + positionView.count > kMaxImportedVerticesPerMesh) {
                return GlbImportStatus::TooLarge;
            }
            mesh.positions.insert(mesh.positions.end(), positions.begin(), positions.end());
            mesh.normals.insert(mesh.normals.end(), normals.begin(), normals.end());
            for (uint32_t index : indices) {
                mesh.indices.push_back(base + index);
            }
        }

        if (mesh.positions.empty() || mesh.indices.empty()) {
            return GlbImportStatus::NothingToImport;
        }
        imported.meshes.push_back(std::move(mesh));
    }

    if (imported.meshes.empty()) {
        return GlbImportStatus::NothingToImport;
    }
    *out = std::move(imported);
    return GlbImportStatus::Ok;
}

}  // namespace forgeshape
