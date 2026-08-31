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

// "this primitive named no accessor here", distinct from accessor 0.
constexpr uint32_t kNoAccessor = 0xFFFFFFFFu;

// Below this the node transform has no volume, so it has no inverse and no
// normal matrix. Scaled to the linear part's own magnitude by the caller, so a
// legitimately tiny model is not mistaken for a degenerate one.
constexpr double kSingularDeterminantEpsilon = 1e-12;

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

// The attributes this reader knows it may leave unread.
//
// Each one is still STRUCTURALLY validated — the accessor resolves, its range
// is inside the buffer, its count agrees with POSITION — because a file whose
// colour accessor runs off the end of the buffer is a broken file whether or
// not this reader wanted the colours. What is skipped is only the decode.
//
// Anything not on this list is refused by name. A custom attribute could carry
// anything, and a diagnostic that silently dropped one would be reporting on a
// file it had not actually understood.
bool isIgnorableAttribute(const std::string& name) {
    return name == "COLOR_0" || name == "COLOR_1" || name == "TEXCOORD_0"
            || name == "TEXCOORD_1";
}

// A three-, four- or sixteen-number array member, when present.
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

// glTF's quaternion order is [x, y, z, w].
Mat4 rotationFromQuaternion(double x, double y, double z, double w) {
    Mat4 r = mat4Identity();
    r.m[0] = static_cast<float>(1.0 - 2.0 * (y * y + z * z));
    r.m[1] = static_cast<float>(2.0 * (x * y + z * w));
    r.m[2] = static_cast<float>(2.0 * (x * z - y * w));
    r.m[4] = static_cast<float>(2.0 * (x * y - z * w));
    r.m[5] = static_cast<float>(1.0 - 2.0 * (x * x + z * z));
    r.m[6] = static_cast<float>(2.0 * (y * z + x * w));
    r.m[8] = static_cast<float>(2.0 * (x * z + y * w));
    r.m[9] = static_cast<float>(2.0 * (y * z - x * w));
    r.m[10] = static_cast<float>(1.0 - 2.0 * (x * x + y * y));
    return r;
}

// The determinant of a column-major matrix's upper-left 3x3.
double linearDeterminant(const Mat4& m) {
    const double a = m.m[0], b = m.m[4], c = m.m[8];
    const double d = m.m[1], e = m.m[5], f = m.m[9];
    const double g = m.m[2], h = m.m[6], i = m.m[10];
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

// The largest absolute entry of the upper-left 3x3, used to scale the singular
// test: a determinant is a volume, so its natural threshold is cubic in the
// matrix's own scale rather than an absolute number.
double linearMagnitude(const Mat4& m) {
    double largest = 0.0;
    const int entries[9] = {0, 1, 2, 4, 5, 6, 8, 9, 10};
    for (int index : entries) {
        const double value = std::fabs(static_cast<double>(m.m[index]));
        if (value > largest) {
            largest = value;
        }
    }
    return largest;
}

// The inverse transpose of the upper-left 3x3, which is the cofactor matrix
// divided by the determinant. Caller has already refused a singular transform.
Mat4 inverseTransposeOfLinear(const Mat4& m, double determinant) {
    const double a = m.m[0], b = m.m[4], c = m.m[8];
    const double d = m.m[1], e = m.m[5], f = m.m[9];
    const double g = m.m[2], h = m.m[6], i = m.m[10];
    const double inv = 1.0 / determinant;

    const double n00 = (e * i - f * h) * inv;
    const double n01 = -(d * i - f * g) * inv;
    const double n02 = (d * h - e * g) * inv;
    const double n10 = -(b * i - c * h) * inv;
    const double n11 = (a * i - c * g) * inv;
    const double n12 = -(a * h - b * g) * inv;
    const double n20 = (b * f - c * e) * inv;
    const double n21 = -(a * f - c * d) * inv;
    const double n22 = (a * e - b * d) * inv;

    Mat4 out = mat4Identity();
    out.m[0] = static_cast<float>(n00);
    out.m[1] = static_cast<float>(n10);
    out.m[2] = static_cast<float>(n20);
    out.m[4] = static_cast<float>(n01);
    out.m[5] = static_cast<float>(n11);
    out.m[6] = static_cast<float>(n21);
    out.m[8] = static_cast<float>(n02);
    out.m[9] = static_cast<float>(n12);
    out.m[10] = static_cast<float>(n22);
    return out;
}

// One decoded vertex block: the POSITION (and optional NORMAL) accessor pair a
// primitive names, decoded once.
//
// A low-poly character exported with several materials states several
// primitives over ONE POSITION accessor. Decoding that accessor once per
// primitive would multiply the vertex count by the number of primitives and
// report a vertex total the file does not contain, so blocks are keyed by the
// accessor pair and reused.
struct VertexBlock {
    uint32_t positionAccessor = kNoAccessor;
    uint32_t normalAccessor = kNoAccessor;
    uint32_t base = 0;   // first vertex of this block in the mesh's arrays
    uint32_t count = 0;
    bool hasNormals = false;
};

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
        case GlbImportStatus::NodeTransformConflict: return "NodeTransformConflict";
        case GlbImportStatus::SingularNodeTransform: return "SingularNodeTransform";
        case GlbImportStatus::MissingAttribute: return "MissingAttribute";
        case GlbImportStatus::NonIndexedPrimitive: return "NonIndexedPrimitive";
        case GlbImportStatus::UnknownAttribute: return "UnknownAttribute";
        case GlbImportStatus::UnsupportedComponentType: return "UnsupportedComponentType";
        case GlbImportStatus::UnsupportedAccessorType: return "UnsupportedAccessorType";
        case GlbImportStatus::InterleavedAccessor: return "InterleavedAccessor";
        case GlbImportStatus::AccessorOutOfRange: return "AccessorOutOfRange";
        case GlbImportStatus::IndexOutOfRange: return "IndexOutOfRange";
        case GlbImportStatus::IndexCountNotTriangles: return "IndexCountNotTriangles";
        case GlbImportStatus::CountMismatch: return "CountMismatch";
        case GlbImportStatus::CannotGenerateNormals: return "CannotGenerateNormals";
        case GlbImportStatus::NonFiniteValue: return "NonFiniteValue";
        case GlbImportStatus::TooLarge: return "TooLarge";
    }
    return "unknown";
}

GlbImportCategory glbImportStatusCategory(GlbImportStatus status) {
    switch (status) {
        case GlbImportStatus::Ok:
        case GlbImportStatus::NoData:
        case GlbImportStatus::NotGlb:
        case GlbImportStatus::UnsupportedVersion:
        case GlbImportStatus::TruncatedFile:
        case GlbImportStatus::ChunkMisaligned:
        case GlbImportStatus::MissingJsonChunk:
        case GlbImportStatus::MissingBinChunk:
        case GlbImportStatus::UnknownChunk:
        case GlbImportStatus::MalformedJson:
        case GlbImportStatus::UnsupportedAssetVersion:
        case GlbImportStatus::NoScene:
        case GlbImportStatus::NoMeshes:
        case GlbImportStatus::NothingToImport:
        case GlbImportStatus::TooLarge:
            return GlbImportCategory::Unreadable;

        case GlbImportStatus::ExternalBuffer:
        case GlbImportStatus::UnsupportedExtension:
        case GlbImportStatus::HasAnimation:
        case GlbImportStatus::HasSkin:
        case GlbImportStatus::SparseAccessor:
        case GlbImportStatus::MorphTargets:
        case GlbImportStatus::NonTriangleMode:
        case GlbImportStatus::NodeHierarchy:
        case GlbImportStatus::NodeTransformConflict:
        case GlbImportStatus::MissingAttribute:
        case GlbImportStatus::NonIndexedPrimitive:
        case GlbImportStatus::UnknownAttribute:
        case GlbImportStatus::UnsupportedComponentType:
        case GlbImportStatus::UnsupportedAccessorType:
        case GlbImportStatus::InterleavedAccessor:
            return GlbImportCategory::Unsupported;

        case GlbImportStatus::SingularNodeTransform:
        case GlbImportStatus::AccessorOutOfRange:
        case GlbImportStatus::IndexOutOfRange:
        case GlbImportStatus::IndexCountNotTriangles:
        case GlbImportStatus::CountMismatch:
        case GlbImportStatus::CannotGenerateNormals:
        case GlbImportStatus::NonFiniteValue:
            return GlbImportCategory::Inconsistent;
    }
    return GlbImportCategory::Unreadable;
}

Vec3 ImportedMesh::worldPosition(uint32_t vertex) const {
    const size_t at = static_cast<size_t>(vertex) * 3u;
    if (at + 3 > positions.size()) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    return Vec3{positions[at], positions[at + 1], positions[at + 2]};
}

Vec3 ImportedMesh::worldNormal(uint32_t vertex) const {
    const size_t at = static_cast<size_t>(vertex) * 3u;
    if (at + 3 > normals.size()) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    return Vec3{normals[at], normals[at + 1], normals[at + 2]};
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

uint32_t ImportedScene::totalBatches() const {
    uint32_t total = 0;
    for (const ImportedMesh& mesh : meshes) {
        total += static_cast<uint32_t>(mesh.batches.size());
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
            // The specification says an unknown chunk may be skipped. This
            // reader does not: it exists to say exactly what a file contains,
            // and silently skipping a chunk is how it would fail to.
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
    // The subset boundary, checked before anything is decoded
    // -----------------------------------------------------------------------
    {
        const JsonValue* asset = doc.member(root, "asset");
        std::string version;
        if (asset == nullptr || !doc.stringMember(*asset, "version", &version)
            || version.rfind("2.", 0) != 0) {
            return GlbImportStatus::UnsupportedAssetVersion;
        }
        // `asset.generator` and every `extras` in the document are read by
        // nothing. They are metadata about who wrote the file, not geometry,
        // and a diagnostic that acted on them would be treating a foreign
        // tool's private data as project truth.
    }
    if (doc.member(root, "animations") != nullptr) {
        return GlbImportStatus::HasAnimation;
    }
    if (doc.member(root, "skins") != nullptr) {
        return GlbImportStatus::HasSkin;
    }
    if (const JsonValue* required = doc.member(root, "extensionsRequired")) {
        // Any required extension at all — Draco and meshopt included. This
        // reader implements none, and an extension is required precisely
        // because a reader that ignores it reads the file wrong.
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
    const JsonValue* materials = doc.member(root, "materials");
    if (meshes == nullptr || meshes->type != JsonType::Array || meshes->children.empty()) {
        return GlbImportStatus::NoMeshes;
    }
    if (bufferViews == nullptr || accessors == nullptr || nodes == nullptr
        || bufferViews->type != JsonType::Array || accessors->type != JsonType::Array
        || nodes->type != JsonType::Array) {
        return GlbImportStatus::MalformedJson;
    }
    if (materials != nullptr && materials->type != JsonType::Array) {
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
            // Tightly packed data only. An interleaved buffer is a valid file
            // this reader would silently mis-stride, so it is refused by name
            // rather than read wrong.
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

    // Whether a material is double-sided. A primitive with no material takes
    // glTF's default material, which is single-sided.
    const auto materialIsDoubleSided = [&](const JsonValue& primitive, bool* doubleSided,
                                           GlbImportStatus* why) -> bool {
        *doubleSided = false;
        double materialIndexValue = 0.0;
        if (!doc.numberMember(primitive, "material", &materialIndexValue)) {
            return true;
        }
        if (materials == nullptr || materials->children.empty()) {
            *why = GlbImportStatus::MalformedJson;
            return false;
        }
        uint32_t materialIndex = 0;
        if (!asIndex(materialIndexValue, static_cast<uint32_t>(materials->children.size() - 1),
                     &materialIndex)) {
            *why = GlbImportStatus::MalformedJson;
            return false;
        }
        const JsonValue* material = doc.element(*materials, materialIndex);
        if (material == nullptr || material->type != JsonType::Object) {
            *why = GlbImportStatus::MalformedJson;
            return false;
        }
        // Only this one member is read. Base colour, roughness, metallic,
        // every texture and every alpha mode are deliberately unread: this is
        // a geometry diagnostic, and reading them would be the first half of a
        // material pipeline nobody has asked for.
        bool value = false;
        if (doc.boolMember(*material, "doubleSided", &value)) {
            *doubleSided = value;
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
            // Child nodes are refused by name rather than flattened. A
            // flattened hierarchy is a different scene from the one the file
            // describes, and this reader never silently changes the answer.
            // Editable hierarchy belongs to production import (`IMPORT-01`).
            return GlbImportStatus::NodeHierarchy;
        }

        ImportedMesh mesh;
        doc.stringMember(*node, "name", &mesh.name);

        // -------------------------------------------------------------------
        // The node transform: a `matrix`, or TRS, never both
        // -------------------------------------------------------------------
        double matrix[16] = {0};
        bool hasMatrix = false;
        if (!readNumbers(doc, *node, "matrix", 16, matrix, &hasMatrix)) {
            return GlbImportStatus::MalformedJson;
        }
        double rotation[4] = {0.0, 0.0, 0.0, 1.0};
        bool hasRotation = false;
        if (!readNumbers(doc, *node, "rotation", 4, rotation, &hasRotation)) {
            return GlbImportStatus::MalformedJson;
        }
        double scale[3] = {1.0, 1.0, 1.0};
        bool hasScale = false;
        if (!readNumbers(doc, *node, "scale", 3, scale, &hasScale)) {
            return GlbImportStatus::MalformedJson;
        }
        double translation[3] = {0.0, 0.0, 0.0};
        bool hasTranslation = false;
        if (!readNumbers(doc, *node, "translation", 3, translation, &hasTranslation)) {
            return GlbImportStatus::MalformedJson;
        }
        if (hasMatrix && (hasRotation || hasScale || hasTranslation)) {
            // glTF forbids stating both forms. Which one wins is not a guess a
            // diagnostic may make on the user's behalf.
            return GlbImportStatus::NodeTransformConflict;
        }

        if (hasMatrix) {
            // glTF states a node matrix in COLUMN-MAJOR order, which is
            // exactly the layout of Mat4, so this is a copy and not a
            // transpose. Getting that backwards is the classic import defect,
            // which is why an asymmetric matrix is a self-test of its own.
            for (int c = 0; c < 16; ++c) {
                if (!std::isfinite(matrix[c])) {
                    return GlbImportStatus::NonFiniteValue;
                }
                mesh.nodeTransform.m[c] = static_cast<float>(matrix[c]);
            }
            // An affine transform only: the bottom row must be [0,0,0,1]. A
            // projective node would move a vertex somewhere no bake could
            // reproduce.
            if (matrix[3] != 0.0 || matrix[7] != 0.0 || matrix[11] != 0.0
                || matrix[15] != 1.0) {
                return GlbImportStatus::SingularNodeTransform;
            }
        } else {
            for (int c = 0; c < 3; ++c) {
                if (!std::isfinite(translation[c]) || !std::isfinite(scale[c])) {
                    return GlbImportStatus::NonFiniteValue;
                }
            }
            double quaternionLength = 0.0;
            for (int c = 0; c < 4; ++c) {
                if (!std::isfinite(rotation[c])) {
                    return GlbImportStatus::NonFiniteValue;
                }
                quaternionLength += rotation[c] * rotation[c];
            }
            quaternionLength = std::sqrt(quaternionLength);
            if (hasRotation) {
                if (quaternionLength <= 0.0) {
                    return GlbImportStatus::SingularNodeTransform;
                }
                // Normalized rather than required to be unit: writers emit
                // quaternions a few ULP off unit length all the time, and
                // refusing those would refuse ordinary files.
                for (int c = 0; c < 4; ++c) {
                    rotation[c] /= quaternionLength;
                }
            }
            Mat4 scaleMatrix = mat4Identity();
            scaleMatrix.m[0] = static_cast<float>(scale[0]);
            scaleMatrix.m[5] = static_cast<float>(scale[1]);
            scaleMatrix.m[10] = static_cast<float>(scale[2]);
            const Mat4 rotationMatrix =
                    rotationFromQuaternion(rotation[0], rotation[1], rotation[2], rotation[3]);
            const Mat4 translationMatrix = mat4Translation(Vec3{static_cast<float>(translation[0]),
                                                                static_cast<float>(translation[1]),
                                                                static_cast<float>(translation[2])});
            // glTF composes a node's TRS as T * R * S.
            mesh.nodeTransform = mat4Multiply(mat4Multiply(translationMatrix, rotationMatrix),
                                              scaleMatrix);
        }
        if (!mat4Finite(mesh.nodeTransform)) {
            return GlbImportStatus::NonFiniteValue;
        }
        mesh.translation[0] = mesh.nodeTransform.m[12];
        mesh.translation[1] = mesh.nodeTransform.m[13];
        mesh.translation[2] = mesh.nodeTransform.m[14];

        const double determinant = linearDeterminant(mesh.nodeTransform);
        const double magnitude = linearMagnitude(mesh.nodeTransform);
        // The threshold is cubic in the matrix's own scale because a
        // determinant is a volume: a model authored in millimetres is not
        // degenerate merely because its numbers are small.
        const double singularFloor =
                kSingularDeterminantEpsilon * (magnitude * magnitude * magnitude + 1.0);
        if (!std::isfinite(determinant) || std::fabs(determinant) <= singularFloor) {
            return GlbImportStatus::SingularNodeTransform;
        }
        mesh.normalTransform = inverseTransposeOfLinear(mesh.nodeTransform, determinant);
        if (!mat4Finite(mesh.normalTransform)) {
            return GlbImportStatus::SingularNodeTransform;
        }
        // A negative determinant mirrors the geometry, which turns every front
        // face into a back face. The bake corrects the winding rather than the
        // shape: the file's geometry is what the owner asked to look at, and a
        // preview drawn inside-out would look like a defect the file does not
        // have. This is a PREVIEW decision — the product still has no Mirror,
        // and the exporter still refuses to write one.
        mesh.windingCorrected = determinant < 0.0;

        double meshIndexValue = 0.0;
        uint32_t meshIndex = 0;
        if (!doc.numberMember(*node, "mesh", &meshIndexValue)
            || !asIndex(meshIndexValue, static_cast<uint32_t>(meshes->children.size() - 1),
                        &meshIndex)) {
            // A node with no mesh draws nothing. Refused rather than skipped:
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
        if (primitives->children.size() > kMaxImportedPrimitivesPerMesh) {
            return GlbImportStatus::TooLarge;
        }

        // Each primitive becomes one draw batch over a shared vertex array.
        // Primitives naming the same POSITION/NORMAL accessors share one
        // decoded block, so a seven-material character stays the vertex count
        // the file states rather than seven copies of it.
        std::vector<VertexBlock> blocks;
        std::vector<uint32_t> batchBlock;  // parallel to mesh.batches

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
            if (!doc.numberMember(*attributes, "POSITION", &positionIndex)) {
                return GlbImportStatus::MissingAttribute;
            }
            double indexAccessorValue = 0.0;
            if (!doc.numberMember(*primitive, "indices", &indexAccessorValue)) {
                // Indexed triangles only. A non-indexed primitive is a
                // different decode path and is named rather than guessed at.
                return GlbImportStatus::NonIndexedPrimitive;
            }

            const uint32_t accessorLimit = static_cast<uint32_t>(accessors->children.size() - 1);
            uint32_t positionAccessor = 0;
            uint32_t indexAccessor = 0;
            if (!asIndex(positionIndex, accessorLimit, &positionAccessor)
                || !asIndex(indexAccessorValue, accessorLimit, &indexAccessor)) {
                return GlbImportStatus::AccessorOutOfRange;
            }
            uint32_t normalAccessor = kNoAccessor;
            double normalIndex = 0.0;
            const bool hasNormal = doc.numberMember(*attributes, "NORMAL", &normalIndex);
            if (hasNormal && !asIndex(normalIndex, accessorLimit, &normalAccessor)) {
                return GlbImportStatus::AccessorOutOfRange;
            }

            GlbImportStatus why = GlbImportStatus::Ok;
            AccessorView positionView;
            AccessorView indexView;
            if (!resolveAccessor(positionAccessor, &positionView, &why)
                || !resolveAccessor(indexAccessor, &indexView, &why)) {
                return why;
            }
            if ((indexView.count % 3u) != 0u) {
                return GlbImportStatus::IndexCountNotTriangles;
            }
            if (positionView.count == 0 || indexView.count == 0) {
                return GlbImportStatus::NothingToImport;
            }

            // Every other attribute is either a known ignorable — structurally
            // validated and not decoded — or a refusal by name.
            for (size_t a = 0; a < attributes->keys.size(); ++a) {
                const std::string& key = attributes->keys[a];
                if (key == "POSITION" || key == "NORMAL") {
                    continue;
                }
                if (!isIgnorableAttribute(key)) {
                    return GlbImportStatus::UnknownAttribute;
                }
                double ignoredIndex = 0.0;
                uint32_t ignoredAccessor = 0;
                if (!doc.numberMember(*attributes, key.c_str(), &ignoredIndex)
                    || !asIndex(ignoredIndex, accessorLimit, &ignoredAccessor)) {
                    return GlbImportStatus::AccessorOutOfRange;
                }
                AccessorView ignoredView;
                if (!resolveAccessor(ignoredAccessor, &ignoredView, &why)) {
                    return why;
                }
                if (ignoredView.count != positionView.count) {
                    return GlbImportStatus::CountMismatch;
                }
                // Deliberately no decode and no allocation: the bytes are
                // proven to be inside the buffer and then left alone. The
                // preview draws one flat neutral, and saying otherwise would
                // claim an appearance it does not render.
            }

            // Find or decode the vertex block this primitive reads from.
            uint32_t blockIndex = static_cast<uint32_t>(blocks.size());
            for (size_t b = 0; b < blocks.size(); ++b) {
                if (blocks[b].positionAccessor == positionAccessor
                    && blocks[b].normalAccessor == normalAccessor) {
                    blockIndex = static_cast<uint32_t>(b);
                    break;
                }
            }
            if (blockIndex == blocks.size()) {
                std::vector<float> positions;
                if (!readVec3Floats(positionView, &positions, &why)) {
                    return why;
                }
                std::vector<float> normals;
                if (normalAccessor != kNoAccessor) {
                    AccessorView normalView;
                    if (!resolveAccessor(normalAccessor, &normalView, &why)) {
                        return why;
                    }
                    if (normalView.count != positionView.count) {
                        return GlbImportStatus::CountMismatch;
                    }
                    if (!readVec3Floats(normalView, &normals, &why)) {
                        return why;
                    }
                } else {
                    // Space for the generated normals, filled after the bake.
                    normals.assign(positions.size(), 0.0f);
                }

                VertexBlock block;
                block.positionAccessor = positionAccessor;
                block.normalAccessor = normalAccessor;
                block.base = mesh.vertexCount();
                block.count = positionView.count;
                block.hasNormals = normalAccessor != kNoAccessor;
                if (static_cast<uint64_t>(block.base) + block.count
                    > kMaxImportedVerticesPerMesh) {
                    return GlbImportStatus::TooLarge;
                }
                mesh.positions.insert(mesh.positions.end(), positions.begin(), positions.end());
                mesh.normals.insert(mesh.normals.end(), normals.begin(), normals.end());
                blocks.push_back(block);
            }
            const VertexBlock& block = blocks[blockIndex];

            std::vector<uint32_t> indices;
            if (!readIndices(indexView, &indices, &why)) {
                return why;
            }
            for (uint32_t index : indices) {
                if (index >= block.count) {
                    return GlbImportStatus::IndexOutOfRange;
                }
            }

            ImportedPrimitiveBatch batch;
            batch.firstIndex = static_cast<uint32_t>(mesh.indices.size());
            batch.indexCount = indexView.count;
            if (!materialIsDoubleSided(*primitive, &batch.doubleSided, &why)) {
                return why;
            }
            mesh.indices.reserve(mesh.indices.size() + indices.size());
            for (size_t t = 0; t + 2 < indices.size(); t += 3) {
                // Rebased onto the shared block, and reordered when the node
                // transform mirrors, so the triangle a viewer sees from the
                // front is the one the file called the front.
                mesh.indices.push_back(block.base + indices[t]);
                if (mesh.windingCorrected) {
                    mesh.indices.push_back(block.base + indices[t + 2]);
                    mesh.indices.push_back(block.base + indices[t + 1]);
                } else {
                    mesh.indices.push_back(block.base + indices[t + 1]);
                    mesh.indices.push_back(block.base + indices[t + 2]);
                }
            }
            mesh.batches.push_back(batch);
            batchBlock.push_back(blockIndex);
        }

        if (mesh.positions.empty() || mesh.indices.empty()) {
            return GlbImportStatus::NothingToImport;
        }

        // -------------------------------------------------------------------
        // The bake: positions first, then normals, because generated normals
        // are accumulated from the BAKED positions and so already carry the
        // node's non-uniform scale.
        // -------------------------------------------------------------------
        for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
            const size_t at = static_cast<size_t>(v) * 3u;
            const Vec3 world = mat4TransformPoint(
                    mesh.nodeTransform,
                    Vec3{mesh.positions[at], mesh.positions[at + 1], mesh.positions[at + 2]});
            if (!vec3Finite(world)) {
                return GlbImportStatus::NonFiniteValue;
            }
            mesh.positions[at] = world.x;
            mesh.positions[at + 1] = world.y;
            mesh.positions[at + 2] = world.z;
        }

        for (size_t b = 0; b < blocks.size(); ++b) {
            const VertexBlock& block = blocks[b];
            if (block.hasNormals) {
                for (uint32_t v = 0; v < block.count; ++v) {
                    const size_t at = (static_cast<size_t>(block.base) + v) * 3u;
                    const Vec3 world = mat4TransformDirection(
                            mesh.normalTransform,
                            Vec3{mesh.normals[at], mesh.normals[at + 1], mesh.normals[at + 2]});
                    const float lengthSquared = vec3Dot(world, world);
                    if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f) {
                        return GlbImportStatus::CannotGenerateNormals;
                    }
                    const Vec3 unit = vec3Normalize(world);
                    mesh.normals[at] = unit.x;
                    mesh.normals[at + 1] = unit.y;
                    mesh.normals[at + 2] = unit.z;
                }
                continue;
            }

            // A deterministic area-weighted smooth vertex normal, for the
            // preview only. Each triangle contributes its UNNORMALIZED face
            // normal, whose magnitude is twice its area, so a large face
            // counts for more than a sliver; a degenerate triangle contributes
            // the zero vector and so contributes nothing. This is not the
            // future production import shading contract, which is
            // `IMPORT-01`'s to decide.
            mesh.normalsGenerated = true;
            std::vector<Vec3> accumulated(block.count, Vec3{0.0f, 0.0f, 0.0f});
            std::vector<uint8_t> referenced(block.count, 0);
            for (size_t batchIndex = 0; batchIndex < mesh.batches.size(); ++batchIndex) {
                if (batchBlock[batchIndex] != b) {
                    continue;
                }
                const ImportedPrimitiveBatch& batch = mesh.batches[batchIndex];
                for (uint32_t t = 0; t + 2 < batch.indexCount; t += 3) {
                    const uint32_t i0 = mesh.indices[batch.firstIndex + t] - block.base;
                    const uint32_t i1 = mesh.indices[batch.firstIndex + t + 1] - block.base;
                    const uint32_t i2 = mesh.indices[batch.firstIndex + t + 2] - block.base;
                    const Vec3 p0 = mesh.worldPosition(block.base + i0);
                    const Vec3 p1 = mesh.worldPosition(block.base + i1);
                    const Vec3 p2 = mesh.worldPosition(block.base + i2);
                    const Vec3 face = vec3Cross(vec3Sub(p1, p0), vec3Sub(p2, p0));
                    referenced[i0] = 1;
                    referenced[i1] = 1;
                    referenced[i2] = 1;
                    if (!vec3Finite(face)) {
                        return GlbImportStatus::CannotGenerateNormals;
                    }
                    accumulated[i0] = vec3Add(accumulated[i0], face);
                    accumulated[i1] = vec3Add(accumulated[i1], face);
                    accumulated[i2] = vec3Add(accumulated[i2], face);
                }
            }
            for (uint32_t v = 0; v < block.count; ++v) {
                const size_t at = (static_cast<size_t>(block.base) + v) * 3u;
                if (referenced[v] == 0) {
                    // A vertex no triangle names has no surface to be normal
                    // to. Recorded as the zero direction rather than an
                    // invented one, and it reaches no shading: the renderer
                    // derives its own normals from the positions.
                    mesh.normals[at] = 0.0f;
                    mesh.normals[at + 1] = 0.0f;
                    mesh.normals[at + 2] = 0.0f;
                    continue;
                }
                const Vec3 sum = accumulated[v];
                const float lengthSquared = vec3Dot(sum, sum);
                if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f) {
                    // Every triangle touching this vertex was degenerate or
                    // they cancelled exactly. Fail closed by name rather than
                    // write a NaN or an invented default into the geometry.
                    return GlbImportStatus::CannotGenerateNormals;
                }
                const Vec3 unit = vec3Normalize(sum);
                mesh.normals[at] = unit.x;
                mesh.normals[at + 1] = unit.y;
                mesh.normals[at + 2] = unit.z;
            }
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
