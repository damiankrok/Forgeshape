#include "forgeshape_mesh_import.h"

#include <cmath>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "forgeshape_math.h"

namespace forgeshape {
namespace {

// What one parsed mesh may hold, beyond the vertex bound the GLB reader shares.
// The triangle bound is the Imported Mesh's own index ceiling
// (`kMaxImportedMeshIndices`, 24 million) over three, so a mesh this reader
// accepts is never refused one step later for its size; the total bounds what
// a whole file may make the reader allocate.
constexpr uint32_t kMaxTrianglesPerMesh = 8u * 1000u * 1000u;
constexpr uint64_t kMaxTrianglesTotal = 16ull * 1000ull * 1000ull;

// STL is millimetres by the convention of every slicer that writes one.
constexpr double kMetresPerMillimetre = 0.001;

// How far one 3MF component may nest inside another. Far beyond any assembly a
// slicer writes; its job is to make a pathological file stop rather than
// recurse.
constexpr int kMaxComponentDepth = 16;

constexpr uint32_t kNoIndex = std::numeric_limits<uint32_t>::max();

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

char lowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

bool equalsIgnoreCase(const char* begin, const char* end, const char* word) {
    const size_t length = std::strlen(word);
    if (static_cast<size_t>(end - begin) != length) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (lowerAscii(begin[i]) != lowerAscii(word[i])) {
            return false;
        }
    }
    return true;
}

bool equals(const char* begin, const char* end, const char* word) {
    const size_t length = std::strlen(word);
    return static_cast<size_t>(end - begin) == length && std::memcmp(begin, word, length) == 0;
}

bool equalsIgnoreCase(const std::string& a, const std::string& b) {
    return equalsIgnoreCase(a.data(), a.data() + a.size(), b.c_str());
}

// ---------------------------------------------------------------------------
// Numbers from text, independent of the C locale
// ---------------------------------------------------------------------------
//
// `strtod` reads the decimal point the current C locale spells, so a desktop
// shell running under a decimal-comma locale would read "1.5" as 1. This
// parser reads the one grammar every one of these formats uses --
// [+-] digits [. digits] [(e|E) [+-] digits], or inf / infinity / nan, which
// are accepted so they can be REFUSED by name (NonFiniteValue) rather than as
// a syntax error. Up to 19 significant digits are kept exactly and scaled by
// exact powers of ten, so the shortest round-trip text of a float32 -- what
// every ForgeShape writer emits -- comes back as the same float.
bool parseNumber(const char* begin, const char* end, double* out) {
    const char* p = begin;
    if (p == end) {
        return false;
    }
    bool negative = false;
    if (*p == '+' || *p == '-') {
        negative = *p == '-';
        ++p;
    }
    if (equalsIgnoreCase(p, end, "inf") || equalsIgnoreCase(p, end, "infinity")) {
        *out = negative ? -std::numeric_limits<double>::infinity()
                        : std::numeric_limits<double>::infinity();
        return true;
    }
    if (equalsIgnoreCase(p, end, "nan")) {
        *out = std::numeric_limits<double>::quiet_NaN();
        return true;
    }
    uint64_t mantissa = 0;
    int significant = 0;
    long exponent10 = 0;
    bool anyDigit = false;
    while (p < end && *p >= '0' && *p <= '9') {
        anyDigit = true;
        const int digit = *p - '0';
        if (mantissa == 0 && digit == 0) {
            // a leading zero carries nothing
        } else if (significant < 19) {
            mantissa = mantissa * 10u + static_cast<uint64_t>(digit);
            ++significant;
        } else {
            ++exponent10;
        }
        ++p;
    }
    if (p < end && *p == '.') {
        ++p;
        while (p < end && *p >= '0' && *p <= '9') {
            anyDigit = true;
            const int digit = *p - '0';
            if (mantissa == 0 && digit == 0) {
                --exponent10;
            } else if (significant < 19) {
                mantissa = mantissa * 10u + static_cast<uint64_t>(digit);
                ++significant;
                --exponent10;
            }
            ++p;
        }
    }
    if (!anyDigit) {
        return false;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        ++p;
        bool exponentNegative = false;
        if (p < end && (*p == '+' || *p == '-')) {
            exponentNegative = *p == '-';
            ++p;
        }
        if (p == end || *p < '0' || *p > '9') {
            return false;
        }
        long stated = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            if (stated < 100000) {
                stated = stated * 10 + (*p - '0');
            }
            ++p;
        }
        exponent10 += exponentNegative ? -stated : stated;
    }
    if (p != end) {
        return false;
    }
    static const double kPow10[23] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                      1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                      1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
    double value = static_cast<double>(mantissa);
    if (mantissa != 0) {
        while (exponent10 > 22 && std::isfinite(value)) {
            value *= 1e22;
            exponent10 -= 22;
        }
        while (exponent10 < -22 && value != 0.0) {
            value /= 1e22;
            exponent10 += 22;
        }
        if (exponent10 > 0 && exponent10 <= 22) {
            value *= kPow10[exponent10];
        } else if (exponent10 < 0 && exponent10 >= -22) {
            value /= kPow10[-exponent10];
        }
    }
    *out = negative ? -value : value;
    return true;
}

// A number that must be a whole value in [0, limit].
bool asCount(double value, uint64_t limit, uint64_t* out) {
    if (!std::isfinite(value) || value < 0.0 || value != std::floor(value)
        || value > static_cast<double>(limit)) {
        return false;
    }
    *out = static_cast<uint64_t>(value);
    return true;
}

// ---------------------------------------------------------------------------
// One mesh, from raw positions and triangles to the parsed shape
// ---------------------------------------------------------------------------

struct RawMesh {
    std::string name;
    std::vector<float> positions;   // xyz triples, world metres
    std::vector<uint32_t> indices;  // triangles over `positions`
};

// Exact-position welding, for formats whose triangles share nothing (STL).
struct PositionKey {
    uint32_t bits[3] = {0, 0, 0};
    bool operator==(const PositionKey& other) const {
        return bits[0] == other.bits[0] && bits[1] == other.bits[1] && bits[2] == other.bits[2];
    }
};

struct PositionKeyHash {
    size_t operator()(const PositionKey& key) const {
        uint64_t hash = 0xCBF29CE484222325ull;
        for (uint32_t word : key.bits) {
            hash = (hash ^ word) * 0x100000001B3ull;
        }
        return static_cast<size_t>(hash ^ (hash >> 32));
    }
};

class PositionWelder {
public:
    // The index of this position in `mesh`, appending it the first time it is
    // seen. False when the mesh would pass the vertex bound.
    bool add(RawMesh& mesh, float x, float y, float z, uint32_t* outIndex) {
        const float xyz[3] = {x == 0.0f ? 0.0f : x, y == 0.0f ? 0.0f : y, z == 0.0f ? 0.0f : z};
        PositionKey key;
        std::memcpy(key.bits, xyz, sizeof(key.bits));
        const auto found = ids_.find(key);
        if (found != ids_.end()) {
            *outIndex = found->second;
            return true;
        }
        const uint32_t index = static_cast<uint32_t>(mesh.positions.size() / 3u);
        if (index >= kMaxImportedVerticesPerMesh) {
            return false;
        }
        mesh.positions.insert(mesh.positions.end(), xyz, xyz + 3);
        ids_.emplace(key, index);
        *outIndex = index;
        return true;
    }

private:
    std::unordered_map<PositionKey, uint32_t, PositionKeyHash> ids_;
};

Vec3 rawPosition(const RawMesh& mesh, uint32_t vertex) {
    const size_t at = static_cast<size_t>(vertex) * 3u;
    return Vec3{mesh.positions[at], mesh.positions[at + 1], mesh.positions[at + 2]};
}

// Drops the triangles with no area, keeps only the vertices the rest use (in
// first-use order), and generates the area-weighted normals -- the GLB
// reader's rule for a mesh that states none. `NothingToImport` when no
// triangle has area, which a caller treats as "this mesh is not an object".
GlbImportStatus finishMesh(RawMesh&& raw, ParsedGlbMesh* out) {
    const uint32_t rawVertexCount = static_cast<uint32_t>(raw.positions.size() / 3u);
    std::vector<uint32_t> kept;
    kept.reserve(raw.indices.size());
    for (size_t t = 0; t + 2 < raw.indices.size(); t += 3) {
        const uint32_t a = raw.indices[t];
        const uint32_t b = raw.indices[t + 1];
        const uint32_t c = raw.indices[t + 2];
        if (a >= rawVertexCount || b >= rawVertexCount || c >= rawVertexCount) {
            return GlbImportStatus::IndexOutOfRange;
        }
        if (a == b || b == c || a == c) {
            continue;
        }
        const Vec3 p0 = rawPosition(raw, a);
        const Vec3 face = vec3Cross(vec3Sub(rawPosition(raw, b), p0), vec3Sub(rawPosition(raw, c), p0));
        if (!vec3Finite(face)) {
            return GlbImportStatus::CannotGenerateNormals;
        }
        if (face.x == 0.0f && face.y == 0.0f && face.z == 0.0f) {
            continue;
        }
        kept.push_back(a);
        kept.push_back(b);
        kept.push_back(c);
    }
    if (kept.empty()) {
        return GlbImportStatus::NothingToImport;
    }
    if (kept.size() / 3u > kMaxTrianglesPerMesh) {
        return GlbImportStatus::TooLarge;
    }

    ParsedGlbMesh mesh;
    mesh.name = std::move(raw.name);
    mesh.nodeTransform = mat4Identity();
    mesh.normalTransform = mat4Identity();
    mesh.normalsGenerated = true;
    std::vector<uint32_t> remap(rawVertexCount, kNoIndex);
    mesh.indices.reserve(kept.size());
    for (uint32_t vertex : kept) {
        if (remap[vertex] == kNoIndex) {
            remap[vertex] = static_cast<uint32_t>(mesh.positions.size() / 3u);
            const size_t at = static_cast<size_t>(vertex) * 3u;
            mesh.positions.insert(mesh.positions.end(), raw.positions.begin() + at,
                                  raw.positions.begin() + at + 3);
        }
        mesh.indices.push_back(remap[vertex]);
    }
    if (mesh.vertexCount() > kMaxImportedVerticesPerMesh) {
        return GlbImportStatus::TooLarge;
    }

    std::vector<Vec3> accumulated(mesh.vertexCount(), Vec3{0.0f, 0.0f, 0.0f});
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const uint32_t i0 = mesh.indices[t];
        const uint32_t i1 = mesh.indices[t + 1];
        const uint32_t i2 = mesh.indices[t + 2];
        const Vec3 p0 = mesh.worldPosition(i0);
        const Vec3 face = vec3Cross(vec3Sub(mesh.worldPosition(i1), p0),
                                    vec3Sub(mesh.worldPosition(i2), p0));
        accumulated[i0] = vec3Add(accumulated[i0], face);
        accumulated[i1] = vec3Add(accumulated[i1], face);
        accumulated[i2] = vec3Add(accumulated[i2], face);
    }
    mesh.normals.resize(mesh.positions.size());
    for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
        const Vec3 sum = accumulated[v];
        const float lengthSquared = vec3Dot(sum, sum);
        if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f) {
            // The faces around this vertex cancel exactly (a sheet stated in
            // both windings, welded together). Refused by name rather than
            // given a direction the file never stated.
            return GlbImportStatus::CannotGenerateNormals;
        }
        const Vec3 unit = vec3Normalize(sum);
        const size_t at = static_cast<size_t>(v) * 3u;
        mesh.normals[at] = unit.x;
        mesh.normals[at + 1] = unit.y;
        mesh.normals[at + 2] = unit.z;
    }

    ParsedGlbBatch batch;
    batch.firstIndex = 0;
    batch.indexCount = static_cast<uint32_t>(mesh.indices.size());
    batch.doubleSided = false;
    mesh.batches.push_back(batch);
    *out = std::move(mesh);
    return GlbImportStatus::Ok;
}

// Finishes every raw mesh into a scene. A mesh with no area left is not an
// object and is skipped; a file left with none is NothingToImport.
GlbImportStatus finishScene(std::vector<RawMesh>&& raws, ParsedGlbScene* out) {
    ParsedGlbScene scene;
    uint64_t triangles = 0;
    for (RawMesh& raw : raws) {
        triangles += raw.indices.size() / 3u;
        if (triangles > kMaxTrianglesTotal) {
            return GlbImportStatus::TooLarge;
        }
        ParsedGlbMesh mesh;
        const GlbImportStatus status = finishMesh(std::move(raw), &mesh);
        if (status == GlbImportStatus::NothingToImport) {
            continue;
        }
        if (status != GlbImportStatus::Ok) {
            return status;
        }
        if (scene.meshes.size() >= kMaxImportedMeshes) {
            return GlbImportStatus::TooLarge;
        }
        scene.meshes.push_back(std::move(mesh));
    }
    if (scene.meshes.empty()) {
        return GlbImportStatus::NothingToImport;
    }
    *out = std::move(scene);
    return GlbImportStatus::Ok;
}

// ---------------------------------------------------------------------------
// Text scanning shared by the ASCII readers
// ---------------------------------------------------------------------------

struct TokenCursor {
    const char* p = nullptr;
    const char* end = nullptr;

    bool next(const char** begin, const char** stop) {
        while (p < end && isSpace(*p)) {
            ++p;
        }
        if (p >= end) {
            return false;
        }
        *begin = p;
        while (p < end && !isSpace(*p)) {
            ++p;
        }
        *stop = p;
        return true;
    }

    bool nextIs(const char* word) {
        const char* b = nullptr;
        const char* e = nullptr;
        return next(&b, &e) && equalsIgnoreCase(b, e, word);
    }

    bool nextNumber(double* out) {
        const char* b = nullptr;
        const char* e = nullptr;
        return next(&b, &e) && parseNumber(b, e, out);
    }

    // The rest of the current line, trimmed, consuming its line end.
    std::string restOfLine() {
        const char* b = p;
        while (p < end && *p != '\n') {
            ++p;
        }
        const char* e = p;
        if (p < end) {
            ++p;
        }
        while (b < e && isSpace(*b)) {
            ++b;
        }
        while (e > b && isSpace(e[-1])) {
            --e;
        }
        return std::string(b, e);
    }

    bool onlySpaceLeft() const {
        for (const char* q = p; q < end; ++q) {
            if (!isSpace(*q)) {
                return false;
            }
        }
        return true;
    }
};

// Whether the bytes are plain text: no NUL and no control byte other than the
// whitespace a text format uses.
bool looksLikeText(const uint8_t* bytes, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        const uint8_t c = bytes[i];
        if (c < 0x20u && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v') {
            return false;
        }
    }
    return true;
}

bool containsWord(const uint8_t* bytes, size_t length, const char* word) {
    const size_t n = std::strlen(word);
    for (size_t i = 0; i + n <= length; ++i) {
        bool match = true;
        for (size_t k = 0; k < n && match; ++k) {
            match = lowerAscii(static_cast<char>(bytes[i + k])) == word[k];
        }
        if (match) {
            return true;
        }
    }
    return false;
}

// "solid" (any case) after optional leading whitespace, followed by whitespace
// or the end of the file.
bool startsWithSolid(const uint8_t* bytes, size_t length) {
    size_t i = 0;
    while (i < length && isSpace(static_cast<char>(bytes[i]))) {
        ++i;
    }
    if (length - i < 5) {
        return false;
    }
    const char* p = reinterpret_cast<const char*>(bytes + i);
    if (!equalsIgnoreCase(p, p + 5, "solid")) {
        return false;
    }
    return i + 5 == length || isSpace(static_cast<char>(bytes[i + 5]));
}

uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
           | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

float readF32LE(const uint8_t* p) {
    const uint32_t bits = readU32LE(p);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool binaryStlSizeHolds(const uint8_t* bytes, size_t length) {
    if (length < 84) {
        return false;
    }
    const uint64_t count = readU32LE(bytes + 80);
    return 84ull + 50ull * count == static_cast<uint64_t>(length);
}

// ---------------------------------------------------------------------------
// STL
// ---------------------------------------------------------------------------

bool millimetresToMetres(double mm, float* out) {
    const float metres = static_cast<float>(mm * kMetresPerMillimetre);
    if (!std::isfinite(mm) || !std::isfinite(metres)) {
        return false;
    }
    *out = metres;
    return true;
}

GlbImportStatus readBinaryStl(const uint8_t* bytes, size_t length, ParsedGlbScene* out) {
    const uint32_t count = readU32LE(bytes + 80);
    if (count == 0) {
        return GlbImportStatus::NothingToImport;
    }
    if (count > kMaxTrianglesPerMesh) {
        return GlbImportStatus::TooLarge;
    }
    RawMesh mesh;  // a binary STL's header is free-form bytes, not a name
    mesh.indices.reserve(static_cast<size_t>(count) * 3u);
    PositionWelder welder;
    for (uint32_t t = 0; t < count; ++t) {
        // 12 bytes of facet normal (not read: normals are regenerated), nine
        // floats of corners, and a two-byte attribute word (not read).
        const uint8_t* facet = bytes + 84 + static_cast<size_t>(t) * 50u;
        for (int k = 0; k < 3; ++k) {
            float metres[3];
            for (int c = 0; c < 3; ++c) {
                const float mm = readF32LE(facet + 12 + k * 12 + c * 4);
                if (!millimetresToMetres(static_cast<double>(mm), &metres[c])) {
                    return GlbImportStatus::NonFiniteValue;
                }
            }
            uint32_t index = 0;
            if (!welder.add(mesh, metres[0], metres[1], metres[2], &index)) {
                return GlbImportStatus::TooLarge;
            }
            mesh.indices.push_back(index);
        }
    }
    (void)length;
    std::vector<RawMesh> raws;
    raws.push_back(std::move(mesh));
    return finishScene(std::move(raws), out);
}

GlbImportStatus readAsciiStl(const uint8_t* bytes, size_t length, ParsedGlbScene* out) {
    TokenCursor cursor;
    cursor.p = reinterpret_cast<const char*>(bytes);
    cursor.end = cursor.p + length;
    std::vector<RawMesh> raws;
    uint64_t triangles = 0;
    while (!cursor.onlySpaceLeft()) {
        if (!cursor.nextIs("solid")) {
            return GlbImportStatus::MalformedText;
        }
        if (raws.size() >= kMaxImportedMeshes) {
            return GlbImportStatus::TooLarge;
        }
        RawMesh mesh;
        mesh.name = cursor.restOfLine();
        PositionWelder welder;
        for (;;) {
            const char* b = nullptr;
            const char* e = nullptr;
            if (!cursor.next(&b, &e)) {
                return GlbImportStatus::MalformedText;  // no endsolid
            }
            if (equalsIgnoreCase(b, e, "endsolid")) {
                cursor.restOfLine();
                break;
            }
            if (!equalsIgnoreCase(b, e, "facet") || !cursor.nextIs("normal")) {
                return GlbImportStatus::MalformedText;
            }
            double ignored = 0.0;
            for (int c = 0; c < 3; ++c) {
                if (!cursor.nextNumber(&ignored)) {
                    return GlbImportStatus::MalformedText;
                }
            }
            if (!cursor.nextIs("outer") || !cursor.nextIs("loop")) {
                return GlbImportStatus::MalformedText;
            }
            for (int k = 0; k < 3; ++k) {
                double mm[3];
                if (!cursor.nextIs("vertex") || !cursor.nextNumber(&mm[0])
                    || !cursor.nextNumber(&mm[1]) || !cursor.nextNumber(&mm[2])) {
                    return GlbImportStatus::MalformedText;
                }
                float metres[3];
                for (int c = 0; c < 3; ++c) {
                    if (!millimetresToMetres(mm[c], &metres[c])) {
                        return GlbImportStatus::NonFiniteValue;
                    }
                }
                uint32_t index = 0;
                if (!welder.add(mesh, metres[0], metres[1], metres[2], &index)) {
                    return GlbImportStatus::TooLarge;
                }
                mesh.indices.push_back(index);
            }
            if (!cursor.nextIs("endloop") || !cursor.nextIs("endfacet")) {
                return GlbImportStatus::MalformedText;
            }
            if (++triangles > kMaxTrianglesTotal || mesh.indices.size() / 3u > kMaxTrianglesPerMesh) {
                return GlbImportStatus::TooLarge;
            }
        }
        raws.push_back(std::move(mesh));
    }
    if (raws.empty()) {
        return GlbImportStatus::NothingToImport;
    }
    return finishScene(std::move(raws), out);
}

GlbImportStatus readStl(const uint8_t* bytes, size_t length, ParsedGlbScene* out) {
    const bool solid = startsWithSolid(bytes, length);
    if (binaryStlSizeHolds(bytes, length)) {
        // A binary header may begin with "solid" too (several exporters write
        // exactly that), so the length rule wins unless the WHOLE file is text
        // that reads like ASCII STL -- binary corners are full of NUL bytes.
        if (!(solid && looksLikeText(bytes, length) && containsWord(bytes, length, "facet"))) {
            return readBinaryStl(bytes, length, out);
        }
    }
    if (solid) {
        if (!looksLikeText(bytes, length)) {
            return GlbImportStatus::MalformedText;
        }
        return readAsciiStl(bytes, length, out);
    }
    if (length < 84) {
        return GlbImportStatus::TruncatedFile;
    }
    const uint64_t declared = 84ull + 50ull * readU32LE(bytes + 80);
    return declared > length ? GlbImportStatus::TruncatedFile : GlbImportStatus::MalformedBinary;
}

// ---------------------------------------------------------------------------
// OBJ
// ---------------------------------------------------------------------------

// One logical OBJ line at a time: '\r' stripped, and a trailing '\' joining the
// next physical line, as the format allows.
struct LineReader {
    const char* p = nullptr;
    const char* end = nullptr;
    std::string joined;

    bool next(const char** begin, const char** stop) {
        if (p >= end) {
            return false;
        }
        const char* b = nullptr;
        const char* e = nullptr;
        physical(&b, &e);
        if (e == b || e[-1] != '\\') {
            *begin = b;
            *stop = e;
            return true;
        }
        joined.assign(b, e - 1);
        while (p < end) {
            joined += ' ';
            physical(&b, &e);
            if (e > b && e[-1] == '\\') {
                joined.append(b, e - 1);
                continue;
            }
            joined.append(b, e);
            break;
        }
        *begin = joined.data();
        *stop = joined.data() + joined.size();
        return true;
    }

private:
    void physical(const char** begin, const char** stop) {
        const char* b = p;
        const void* found = std::memchr(p, '\n', static_cast<size_t>(end - p));
        const char* lineEnd = found != nullptr ? static_cast<const char*>(found) : end;
        p = found != nullptr ? lineEnd + 1 : end;
        const char* e = lineEnd;
        if (e > b && e[-1] == '\r') {
            --e;
        }
        *begin = b;
        *stop = e;
    }
};

enum class ObjStatement { Comment, Vertex, TexCoord, Normal, Face, Object, Group, Ignored,
                          FreeForm, Unknown };

ObjStatement classifyObjKeyword(const char* b, const char* e) {
    if (b < e && *b == '#') return ObjStatement::Comment;
    if (equals(b, e, "v")) return ObjStatement::Vertex;
    if (equals(b, e, "vt")) return ObjStatement::TexCoord;
    if (equals(b, e, "vn")) return ObjStatement::Normal;
    if (equals(b, e, "f") || equals(b, e, "fo")) return ObjStatement::Face;
    if (equals(b, e, "o")) return ObjStatement::Object;
    if (equals(b, e, "g")) return ObjStatement::Group;
    static const char* const kIgnored[] = {"usemtl", "mtllib", "s", "l", "p", "lod", "maplib",
                                           "usemap", "shadow_obj", "trace_obj", "ctech",
                                           "stech", "bevel", "c_interp", "d_interp", "mg"};
    for (const char* word : kIgnored) {
        if (equals(b, e, word)) return ObjStatement::Ignored;
    }
    static const char* const kFreeForm[] = {"vp", "cstype", "deg", "bmat", "step", "curv",
                                            "curv2", "surf", "parm", "trim", "hole", "scrv",
                                            "sp", "end", "con", "res"};
    for (const char* word : kFreeForm) {
        if (equals(b, e, word)) return ObjStatement::FreeForm;
    }
    return ObjStatement::Unknown;
}

// The first whitespace-delimited token of [b, e), and where the rest begins.
bool firstToken(const char* b, const char* e, const char** tb, const char** te) {
    while (b < e && isSpace(*b)) ++b;
    if (b >= e) return false;
    *tb = b;
    while (b < e && !isSpace(*b)) ++b;
    *te = b;
    return true;
}

std::string trimmed(const char* b, const char* e) {
    while (b < e && isSpace(*b)) ++b;
    while (e > b && isSpace(e[-1])) --e;
    return std::string(b, e);
}

bool lineHasControlBytes(const char* b, const char* e) {
    for (const char* q = b; q < e; ++q) {
        const unsigned char c = static_cast<unsigned char>(*q);
        if (c < 0x20u && c != '\t' && c != '\f' && c != '\v') {
            return true;
        }
    }
    return false;
}

// An OBJ index field: a non-zero signed integer.
bool parseObjIndex(const char* b, const char* e, long long* out) {
    if (b == e) return false;
    bool negative = false;
    if (*b == '-' || *b == '+') {
        negative = *b == '-';
        ++b;
    }
    if (b == e) return false;
    long long value = 0;
    for (; b < e; ++b) {
        if (*b < '0' || *b > '9') return false;
        if (value < (1ll << 40)) value = value * 10 + (*b - '0');
    }
    *out = negative ? -value : value;
    return true;
}

// Resolves one OBJ reference against the count defined so far. A positive
// index is kept as stated (0-based) and range-checked once the whole file is
// read, since nothing in the format orders a face after its vertices; a
// negative one is relative to what precedes it and is resolved now.
bool resolveObjReference(long long stated, uint64_t definedSoFar, uint64_t* out) {
    if (stated > 0) {
        *out = static_cast<uint64_t>(stated - 1);
        return true;
    }
    if (stated < 0 && static_cast<uint64_t>(-stated) <= definedSoFar) {
        *out = definedSoFar - static_cast<uint64_t>(-stated);
        return true;
    }
    return false;  // 0, or a relative index before the first definition
}

struct ObjMesh {
    std::string name;
    std::vector<uint32_t> positionIndices;  // triangles, 0-based, over the file's `v`s
};

GlbImportStatus readObj(const uint8_t* bytes, size_t length, ParsedGlbScene* out) {
    const char* text = reinterpret_cast<const char*>(bytes);

    // One splitting rule per file: `o` when the file has any, `g` otherwise.
    bool hasObjects = false;
    {
        LineReader lines{text, text + length, {}};
        const char* b = nullptr;
        const char* e = nullptr;
        while (lines.next(&b, &e) && !hasObjects) {
            const char* tb = nullptr;
            const char* te = nullptr;
            hasObjects = firstToken(b, e, &tb, &te) && equals(tb, te, "o");
        }
    }

    std::vector<float> positions;
    uint64_t texCoordCount = 0;
    uint64_t normalCount = 0;
    uint64_t highestTexCoord = 0;  // the largest positive 1-based reference
    uint64_t highestNormal = 0;
    uint64_t triangles = 0;
    std::vector<ObjMesh> meshes(1);

    const auto startMesh = [&meshes](std::string name) {
        if (!meshes.back().positionIndices.empty()) {
            meshes.emplace_back();
        }
        meshes.back().name = std::move(name);
    };

    LineReader lines{text, text + length, {}};
    const char* b = nullptr;
    const char* e = nullptr;
    std::vector<uint32_t> corners;
    while (lines.next(&b, &e)) {
        if (lineHasControlBytes(b, e)) {
            return GlbImportStatus::MalformedText;
        }
        const char* tb = nullptr;
        const char* te = nullptr;
        if (!firstToken(b, e, &tb, &te)) {
            continue;
        }
        const ObjStatement statement = classifyObjKeyword(tb, te);
        TokenCursor rest;
        rest.p = te;
        rest.end = e;
        if (statement == ObjStatement::Vertex || statement == ObjStatement::Face) {
            // A trailing "# ..." on a numeric line is a comment some writers
            // add; it is not data.
            const void* hash = std::memchr(te, '#', static_cast<size_t>(e - te));
            if (hash != nullptr) {
                rest.end = static_cast<const char*>(hash);
            }
        }
        switch (statement) {
            case ObjStatement::Comment:
            case ObjStatement::Ignored:
                break;
            case ObjStatement::FreeForm:
            case ObjStatement::Unknown:
                return GlbImportStatus::UnsupportedObjFeature;
            case ObjStatement::Vertex: {
                // x y z, then an optional w or an r g b colour: read, not kept.
                double values[7];
                int count = 0;
                const char* nb = nullptr;
                const char* ne = nullptr;
                while (rest.next(&nb, &ne)) {
                    if (count == 7 || !parseNumber(nb, ne, &values[count])) {
                        return GlbImportStatus::MalformedText;
                    }
                    ++count;
                }
                if (count < 3) {
                    return GlbImportStatus::MalformedText;
                }
                if (positions.size() / 3u >= kMaxImportedVerticesPerMesh) {
                    return GlbImportStatus::TooLarge;
                }
                for (int c = 0; c < 3; ++c) {
                    const float value = static_cast<float>(values[c]);
                    if (!std::isfinite(values[c]) || !std::isfinite(value)) {
                        return GlbImportStatus::NonFiniteValue;
                    }
                    positions.push_back(value);
                }
                break;
            }
            case ObjStatement::TexCoord:
                ++texCoordCount;
                break;
            case ObjStatement::Normal:
                ++normalCount;
                break;
            case ObjStatement::Object:
                startMesh(trimmed(te, e));
                break;
            case ObjStatement::Group:
                if (!hasObjects) {
                    startMesh(trimmed(te, e));
                }
                break;
            case ObjStatement::Face: {
                corners.clear();
                const char* cb = nullptr;
                const char* ce = nullptr;
                const uint64_t definedPositions = positions.size() / 3u;
                while (rest.next(&cb, &ce)) {
                    // v | v/vt | v//vn | v/vt/vn
                    const char* fields[3][2] = {{cb, cb}, {nullptr, nullptr}, {nullptr, nullptr}};
                    int fieldCount = 1;
                    const char* q = cb;
                    while (q < ce && *q != '/') ++q;
                    fields[0][1] = q;
                    while (q < ce && fieldCount < 3) {
                        ++q;  // past '/'
                        fields[fieldCount][0] = q;
                        while (q < ce && *q != '/') ++q;
                        fields[fieldCount][1] = q;
                        ++fieldCount;
                    }
                    if (q < ce) {
                        return GlbImportStatus::MalformedText;  // a fourth field
                    }
                    long long stated = 0;
                    uint64_t resolved = 0;
                    if (!parseObjIndex(fields[0][0], fields[0][1], &stated)) {
                        return GlbImportStatus::MalformedText;
                    }
                    if (!resolveObjReference(stated, definedPositions, &resolved)
                        || resolved >= kMaxImportedVerticesPerMesh) {
                        return GlbImportStatus::IndexOutOfRange;
                    }
                    corners.push_back(static_cast<uint32_t>(resolved));
                    for (int f = 1; f < fieldCount; ++f) {
                        if (fields[f][0] == fields[f][1]) {
                            if (f == 1 && fieldCount == 3) {
                                continue;  // "v//vn": no texture coordinate
                            }
                            return GlbImportStatus::MalformedText;
                        }
                        if (!parseObjIndex(fields[f][0], fields[f][1], &stated)) {
                            return GlbImportStatus::MalformedText;
                        }
                        const uint64_t defined = f == 1 ? texCoordCount : normalCount;
                        if (!resolveObjReference(stated, defined, &resolved)) {
                            return GlbImportStatus::IndexOutOfRange;
                        }
                        uint64_t& highest = f == 1 ? highestTexCoord : highestNormal;
                        if (stated > 0 && resolved + 1 > highest) {
                            highest = resolved + 1;
                        }
                    }
                }
                if (corners.size() < 3) {
                    return GlbImportStatus::MalformedText;
                }
                ObjMesh& mesh = meshes.back();
                for (size_t k = 1; k + 1 < corners.size(); ++k) {
                    if (++triangles > kMaxTrianglesTotal
                        || mesh.positionIndices.size() / 3u >= kMaxTrianglesPerMesh) {
                        return GlbImportStatus::TooLarge;
                    }
                    mesh.positionIndices.push_back(corners[0]);
                    mesh.positionIndices.push_back(corners[k]);
                    mesh.positionIndices.push_back(corners[k + 1]);
                }
                break;
            }
        }
    }

    if (highestTexCoord > texCoordCount || highestNormal > normalCount) {
        return GlbImportStatus::IndexOutOfRange;
    }
    const uint64_t positionCount = positions.size() / 3u;
    std::vector<RawMesh> raws;
    std::vector<uint32_t> local(positionCount, kNoIndex);
    std::vector<uint32_t> touched;
    for (ObjMesh& objMesh : meshes) {
        if (objMesh.positionIndices.empty()) {
            continue;
        }
        if (raws.size() >= kMaxImportedMeshes) {
            return GlbImportStatus::TooLarge;
        }
        RawMesh raw;
        raw.name = std::move(objMesh.name);
        raw.indices.reserve(objMesh.positionIndices.size());
        touched.clear();
        for (uint32_t global : objMesh.positionIndices) {
            if (global >= positionCount) {
                return GlbImportStatus::IndexOutOfRange;
            }
            if (local[global] == kNoIndex) {
                local[global] = static_cast<uint32_t>(raw.positions.size() / 3u);
                touched.push_back(global);
                const size_t at = static_cast<size_t>(global) * 3u;
                raw.positions.insert(raw.positions.end(), positions.begin() + at,
                                     positions.begin() + at + 3);
            }
            raw.indices.push_back(local[global]);
        }
        for (uint32_t global : touched) {
            local[global] = kNoIndex;  // every mesh re-indexes from scratch
        }
        raws.push_back(std::move(raw));
    }
    if (raws.empty()) {
        return GlbImportStatus::NothingToImport;
    }
    return finishScene(std::move(raws), out);
}

// The OBJ line-grammar sniff: every non-blank, non-comment line in the first
// 64 KB opens with a statement OBJ defines, and at least one is a `v`.
bool sniffObj(const uint8_t* bytes, size_t length) {
    const size_t window = length < 65536u ? length : 65536u;
    if (!looksLikeText(bytes, window)) {
        return false;
    }
    const char* text = reinterpret_cast<const char*>(bytes);
    const char* stop = text + window;
    const char* p = text;
    bool sawVertex = false;
    while (p < stop) {
        const void* found = std::memchr(p, '\n', static_cast<size_t>(stop - p));
        if (found == nullptr && window < length) {
            break;  // a line cut by the window is not judged
        }
        const char* lineEnd = found != nullptr ? static_cast<const char*>(found) : stop;
        const char* tb = nullptr;
        const char* te = nullptr;
        if (firstToken(p, lineEnd, &tb, &te)) {
            const ObjStatement statement = classifyObjKeyword(tb, te);
            if (statement == ObjStatement::Unknown) {
                return false;
            }
            sawVertex = sawVertex || statement == ObjStatement::Vertex;
        }
        if (found == nullptr) {
            break;
        }
        p = lineEnd + 1;
    }
    return sawVertex;
}

// ---------------------------------------------------------------------------
// PLY
// ---------------------------------------------------------------------------

enum class PlyEncoding { Ascii, BinaryLittle, BinaryBig };

enum class PlyType { Int8, UInt8, Int16, UInt16, Int32, UInt32, Float32, Float64 };

bool plyTypeFromName(const char* b, const char* e, PlyType* out) {
    struct Name { const char* name; PlyType type; };
    static const Name kNames[] = {
            {"char", PlyType::Int8},     {"int8", PlyType::Int8},
            {"uchar", PlyType::UInt8},   {"uint8", PlyType::UInt8},
            {"short", PlyType::Int16},   {"int16", PlyType::Int16},
            {"ushort", PlyType::UInt16}, {"uint16", PlyType::UInt16},
            {"int", PlyType::Int32},     {"int32", PlyType::Int32},
            {"uint", PlyType::UInt32},   {"uint32", PlyType::UInt32},
            {"float", PlyType::Float32}, {"float32", PlyType::Float32},
            {"double", PlyType::Float64}, {"float64", PlyType::Float64}};
    for (const Name& name : kNames) {
        if (equals(b, e, name.name)) {
            *out = name.type;
            return true;
        }
    }
    return false;
}

size_t plyTypeSize(PlyType type) {
    switch (type) {
        case PlyType::Int8:
        case PlyType::UInt8: return 1;
        case PlyType::Int16:
        case PlyType::UInt16: return 2;
        case PlyType::Int32:
        case PlyType::UInt32:
        case PlyType::Float32: return 4;
        case PlyType::Float64: return 8;
    }
    return 0;
}

bool plyTypeIsInteger(PlyType type) {
    return type != PlyType::Float32 && type != PlyType::Float64;
}

struct PlyProperty {
    std::string name;
    bool isList = false;
    PlyType type = PlyType::Float32;       // the scalar, or the list's items
    PlyType countType = PlyType::UInt8;    // lists only
};

struct PlyElement {
    std::string name;
    uint64_t count = 0;
    std::vector<PlyProperty> properties;
};

// Reads one scalar of a declared type from the data section, in either
// encoding. TruncatedFile when the data runs out.
struct PlyValueReader {
    PlyEncoding encoding = PlyEncoding::Ascii;
    const uint8_t* p = nullptr;
    const uint8_t* end = nullptr;

    GlbImportStatus read(PlyType type, double* out) {
        if (encoding == PlyEncoding::Ascii) {
            TokenCursor cursor;
            cursor.p = reinterpret_cast<const char*>(p);
            cursor.end = reinterpret_cast<const char*>(end);
            const char* b = nullptr;
            const char* e = nullptr;
            if (!cursor.next(&b, &e)) {
                return GlbImportStatus::TruncatedFile;
            }
            p = reinterpret_cast<const uint8_t*>(cursor.p);
            if (!parseNumber(b, e, out)) {
                return GlbImportStatus::MalformedText;
            }
            if (plyTypeIsInteger(type) && std::isfinite(*out) && *out != std::floor(*out)) {
                return GlbImportStatus::MalformedText;
            }
            return GlbImportStatus::Ok;
        }
        const size_t size = plyTypeSize(type);
        if (static_cast<size_t>(end - p) < size) {
            return GlbImportStatus::TruncatedFile;
        }
        uint8_t raw[8];
        for (size_t i = 0; i < size; ++i) {
            raw[i] = encoding == PlyEncoding::BinaryLittle ? p[i] : p[size - 1 - i];
        }
        p += size;
        switch (type) {
            case PlyType::Int8: *out = static_cast<int8_t>(raw[0]); break;
            case PlyType::UInt8: *out = raw[0]; break;
            case PlyType::Int16: *out = static_cast<int16_t>(readU16LE(raw)); break;
            case PlyType::UInt16: *out = readU16LE(raw); break;
            case PlyType::Int32: *out = static_cast<int32_t>(readU32LE(raw)); break;
            case PlyType::UInt32: *out = readU32LE(raw); break;
            case PlyType::Float32: *out = readF32LE(raw); break;
            case PlyType::Float64: {
                const uint64_t bits = static_cast<uint64_t>(readU32LE(raw))
                                      | (static_cast<uint64_t>(readU32LE(raw + 4)) << 32);
                double value = 0.0;
                std::memcpy(&value, &bits, sizeof(value));
                *out = value;
                break;
            }
        }
        return GlbImportStatus::Ok;
    }

    // Skips `count` values of one type without decoding them where it can.
    GlbImportStatus skip(PlyType type, uint64_t count) {
        if (encoding != PlyEncoding::Ascii) {
            const uint64_t bytes = count * plyTypeSize(type);
            if (count > static_cast<uint64_t>(end - p) || bytes > static_cast<uint64_t>(end - p)) {
                return GlbImportStatus::TruncatedFile;
            }
            p += bytes;
            return GlbImportStatus::Ok;
        }
        double ignored = 0.0;
        for (uint64_t i = 0; i < count; ++i) {
            const GlbImportStatus status = read(type, &ignored);
            if (status != GlbImportStatus::Ok) {
                return status;
            }
        }
        return GlbImportStatus::Ok;
    }
};

GlbImportStatus readPly(const uint8_t* bytes, size_t length, ParsedGlbScene* out) {
    const char* text = reinterpret_cast<const char*>(bytes);
    const char* end = text + length;
    const char* p = text;

    // --- header ------------------------------------------------------------
    const auto nextLine = [&p, end](const char** b, const char** e) {
        if (p >= end) return false;
        const void* found = std::memchr(p, '\n', static_cast<size_t>(end - p));
        if (found == nullptr) return false;  // a header line must end
        *b = p;
        const char* lineEnd = static_cast<const char*>(found);
        p = lineEnd + 1;
        *e = (lineEnd > *b && lineEnd[-1] == '\r') ? lineEnd - 1 : lineEnd;
        return true;
    };
    const char* lb = nullptr;
    const char* le = nullptr;
    if (!nextLine(&lb, &le) || !equals(lb, le, "ply")) {
        return GlbImportStatus::MalformedText;
    }
    bool haveFormat = false;
    PlyEncoding encoding = PlyEncoding::Ascii;
    std::vector<PlyElement> elements;
    bool ended = false;
    while (!ended) {
        if (!nextLine(&lb, &le)) {
            return GlbImportStatus::TruncatedFile;
        }
        if (lineHasControlBytes(lb, le)) {
            return GlbImportStatus::MalformedText;
        }
        TokenCursor tokens;
        tokens.p = lb;
        tokens.end = le;
        const char* tb = nullptr;
        const char* te = nullptr;
        if (!tokens.next(&tb, &te)) {
            continue;
        }
        if (equals(tb, te, "comment") || equals(tb, te, "obj_info")) {
            continue;
        }
        if (equals(tb, te, "end_header")) {
            ended = true;
            break;
        }
        if (equals(tb, te, "format")) {
            const char* kb = nullptr;
            const char* ke = nullptr;
            const char* vb = nullptr;
            const char* ve = nullptr;
            const char* xb = nullptr;
            const char* xe = nullptr;
            if (haveFormat || !elements.empty() || !tokens.next(&kb, &ke)
                || !tokens.next(&vb, &ve) || tokens.next(&xb, &xe)) {
                return GlbImportStatus::MalformedText;
            }
            if (equals(kb, ke, "ascii")) {
                encoding = PlyEncoding::Ascii;
            } else if (equals(kb, ke, "binary_little_endian")) {
                encoding = PlyEncoding::BinaryLittle;
            } else if (equals(kb, ke, "binary_big_endian")) {
                encoding = PlyEncoding::BinaryBig;
            } else {
                return GlbImportStatus::UnsupportedPlyFormat;
            }
            if (!equals(vb, ve, "1.0")) {
                return GlbImportStatus::UnsupportedPlyFormat;
            }
            haveFormat = true;
            continue;
        }
        if (!haveFormat) {
            return GlbImportStatus::MalformedText;
        }
        if (equals(tb, te, "element")) {
            const char* nb = nullptr;
            const char* ne = nullptr;
            const char* cb = nullptr;
            const char* ce = nullptr;
            double countValue = 0.0;
            PlyElement element;
            if (!tokens.next(&nb, &ne) || !tokens.next(&cb, &ce) || !parseNumber(cb, ce, &countValue)
                || !asCount(countValue, 0xFFFFFFFFull, &element.count) || !tokens.onlySpaceLeft()) {
                return GlbImportStatus::MalformedText;
            }
            element.name.assign(nb, ne);
            for (const PlyElement& existing : elements) {
                if (existing.name == element.name) {
                    return GlbImportStatus::MalformedText;
                }
            }
            if (elements.size() >= 256u) {
                return GlbImportStatus::TooLarge;
            }
            elements.push_back(std::move(element));
            continue;
        }
        if (equals(tb, te, "property")) {
            if (elements.empty() || elements.back().properties.size() >= 256u) {
                return GlbImportStatus::MalformedText;
            }
            PlyProperty property;
            const char* xb = nullptr;
            const char* xe = nullptr;
            if (!tokens.next(&xb, &xe)) {
                return GlbImportStatus::MalformedText;
            }
            if (equals(xb, xe, "list")) {
                property.isList = true;
                const char* cb = nullptr;
                const char* ce = nullptr;
                if (!tokens.next(&cb, &ce) || !tokens.next(&xb, &xe)) {
                    return GlbImportStatus::MalformedText;
                }
                if (!plyTypeFromName(cb, ce, &property.countType)
                    || !plyTypeFromName(xb, xe, &property.type)) {
                    return GlbImportStatus::UnsupportedPlyFormat;
                }
                if (!plyTypeIsInteger(property.countType)) {
                    return GlbImportStatus::UnsupportedPlyFormat;
                }
            } else if (!plyTypeFromName(xb, xe, &property.type)) {
                return GlbImportStatus::UnsupportedPlyFormat;
            }
            const char* nb = nullptr;
            const char* ne = nullptr;
            if (!tokens.next(&nb, &ne) || !tokens.onlySpaceLeft()) {
                return GlbImportStatus::MalformedText;
            }
            property.name.assign(nb, ne);
            elements.back().properties.push_back(std::move(property));
            continue;
        }
        return GlbImportStatus::MalformedText;
    }

    // --- the plan: which properties carry the geometry ---------------------
    int vertexElement = -1;
    int faceElement = -1;
    int axis[3] = {-1, -1, -1};
    int faceList = -1;
    for (size_t i = 0; i < elements.size(); ++i) {
        const PlyElement& element = elements[i];
        if (element.name == "vertex") {
            vertexElement = static_cast<int>(i);
            for (size_t k = 0; k < element.properties.size(); ++k) {
                const PlyProperty& property = element.properties[k];
                const int which = property.name == "x" ? 0 : property.name == "y" ? 1
                                  : property.name == "z" ? 2 : -1;
                if (which >= 0) {
                    if (property.isList || axis[which] >= 0) {
                        return GlbImportStatus::MalformedText;
                    }
                    axis[which] = static_cast<int>(k);
                }
            }
        } else if (element.name == "face") {
            faceElement = static_cast<int>(i);
            for (size_t k = 0; k < element.properties.size(); ++k) {
                const PlyProperty& property = element.properties[k];
                if (property.name == "vertex_indices" || property.name == "vertex_index") {
                    if (!property.isList || faceList >= 0) {
                        return GlbImportStatus::MalformedText;
                    }
                    if (!plyTypeIsInteger(property.type)) {
                        return GlbImportStatus::UnsupportedPlyFormat;
                    }
                    faceList = static_cast<int>(k);
                }
            }
        }
    }
    if (vertexElement < 0 || faceElement < 0 || elements[vertexElement].count == 0
        || elements[faceElement].count == 0) {
        return GlbImportStatus::NothingToImport;  // a point cloud draws no surface
    }
    if (axis[0] < 0 || axis[1] < 0 || axis[2] < 0 || faceList < 0) {
        return GlbImportStatus::MissingAttribute;
    }
    if (elements[vertexElement].count > kMaxImportedVerticesPerMesh) {
        return GlbImportStatus::TooLarge;
    }

    // --- data --------------------------------------------------------------
    PlyValueReader reader;
    reader.encoding = encoding;
    reader.p = reinterpret_cast<const uint8_t*>(p);
    reader.end = bytes + length;
    RawMesh mesh;
    std::vector<uint32_t> polygon;
    for (size_t i = 0; i < elements.size(); ++i) {
        const PlyElement& element = elements[i];
        if (element.properties.empty()) {
            continue;  // carries no data in either encoding
        }
        const bool isVertex = static_cast<int>(i) == vertexElement;
        const bool isFace = static_cast<int>(i) == faceElement;
        if (isVertex) {
            // Each vertex takes at least one byte, so the bytes left bound what
            // a declared count may make this reserve.
            const uint64_t left = static_cast<uint64_t>(reader.end - reader.p);
            mesh.positions.reserve(
                    static_cast<size_t>(element.count < left ? element.count : left) * 3u);
        }
        for (uint64_t n = 0; n < element.count; ++n) {
            double xyz[3] = {0.0, 0.0, 0.0};
            for (size_t k = 0; k < element.properties.size(); ++k) {
                const PlyProperty& property = element.properties[k];
                GlbImportStatus status = GlbImportStatus::Ok;
                if (!property.isList) {
                    double value = 0.0;
                    status = reader.read(property.type, &value);
                    if (status != GlbImportStatus::Ok) return status;
                    if (isVertex) {
                        for (int c = 0; c < 3; ++c) {
                            if (axis[c] == static_cast<int>(k)) xyz[c] = value;
                        }
                    }
                    continue;
                }
                double countValue = 0.0;
                status = reader.read(property.countType, &countValue);
                if (status != GlbImportStatus::Ok) return status;
                uint64_t count = 0;
                if (!asCount(countValue, 0xFFFFFFFFull, &count)) {
                    return encoding == PlyEncoding::Ascii ? GlbImportStatus::MalformedText
                                                          : GlbImportStatus::MalformedBinary;
                }
                if (!(isFace && static_cast<int>(k) == faceList)) {
                    status = reader.skip(property.type, count);
                    if (status != GlbImportStatus::Ok) return status;
                    continue;
                }
                polygon.clear();
                for (uint64_t c = 0; c < count; ++c) {
                    double indexValue = 0.0;
                    status = reader.read(property.type, &indexValue);
                    if (status != GlbImportStatus::Ok) return status;
                    uint64_t index = 0;
                    if (!asCount(indexValue, kMaxImportedVerticesPerMesh, &index)) {
                        return GlbImportStatus::IndexOutOfRange;
                    }
                    polygon.push_back(static_cast<uint32_t>(index));
                }
                // A list of fewer than three corners is an edge or a point and
                // carries no surface, like an OBJ `l`.
                for (size_t c = 1; c + 1 < polygon.size(); ++c) {
                    if (mesh.indices.size() / 3u >= kMaxTrianglesPerMesh) {
                        return GlbImportStatus::TooLarge;
                    }
                    mesh.indices.push_back(polygon[0]);
                    mesh.indices.push_back(polygon[c]);
                    mesh.indices.push_back(polygon[c + 1]);
                }
            }
            if (isVertex) {
                for (int c = 0; c < 3; ++c) {
                    const float value = static_cast<float>(xyz[c]);
                    if (!std::isfinite(xyz[c]) || !std::isfinite(value)) {
                        return GlbImportStatus::NonFiniteValue;
                    }
                    mesh.positions.push_back(value);
                }
            }
        }
    }
    if (encoding == PlyEncoding::Ascii) {
        for (const uint8_t* q = reader.p; q < reader.end; ++q) {
            if (!isSpace(static_cast<char>(*q))) {
                return GlbImportStatus::MalformedText;
            }
        }
    } else if (reader.p != reader.end) {
        return GlbImportStatus::MalformedBinary;
    }
    std::vector<RawMesh> raws;
    raws.push_back(std::move(mesh));
    return finishScene(std::move(raws), out);
}

// ---------------------------------------------------------------------------
// ZIP and DEFLATE, for the 3MF package
// ---------------------------------------------------------------------------

uint32_t crc32Of(const uint8_t* data, size_t length) {
    static const auto table = [] {
        std::vector<uint32_t> entries(256);
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) != 0u ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
            }
            entries[n] = c;
        }
        return entries;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// A bounded RFC 1951 inflater: stored, fixed-Huffman and dynamic-Huffman
// blocks. Every read is checked against the input, every write against the
// size the archive declared, and any code the RFC does not allow -- an
// over-subscribed or incomplete code table, a length or distance symbol past
// its table, a distance reaching before the output -- stops it.
class Inflater {
public:
    Inflater(const uint8_t* input, size_t inputLength, size_t expected, std::string* output)
        : in_(input), inLength_(inputLength), expected_(expected), out_(output) {}

    bool run() {
        out_->clear();
        out_->reserve(expected_);
        bool last = false;
        do {
            uint32_t header = 0;
            if (!bits(3, &header)) return false;
            last = (header & 1u) != 0u;
            const uint32_t type = header >> 1;
            bool ok = false;
            if (type == 0) {
                ok = stored();
            } else if (type == 1) {
                ok = fixed();
            } else if (type == 2) {
                ok = dynamic();
            }
            if (!ok) return false;  // type 3 is reserved
        } while (!last);
        return out_->size() == expected_;
    }

private:
    struct Huffman {
        uint16_t count[16] = {};
        uint16_t symbol[320] = {};
    };

    bool bits(int need, uint32_t* value) {
        while (bitCount_ < need) {
            if (pos_ >= inLength_) return false;
            bitBuffer_ |= static_cast<uint32_t>(in_[pos_++]) << bitCount_;
            bitCount_ += 8;
        }
        *value = bitBuffer_ & ((1u << need) - 1u);
        bitBuffer_ >>= need;
        bitCount_ -= need;
        return true;
    }

    bool put(uint8_t byte) {
        if (out_->size() >= expected_) return false;
        out_->push_back(static_cast<char>(byte));
        return true;
    }

    bool stored() {
        bitBuffer_ = 0;  // the rest of the current byte is padding
        bitCount_ = 0;
        if (inLength_ - pos_ < 4) return false;
        const uint32_t length = readU16LE(in_ + pos_);
        const uint32_t complement = readU16LE(in_ + pos_ + 2);
        pos_ += 4;
        if (length != (~complement & 0xFFFFu)) return false;
        if (inLength_ - pos_ < length || expected_ - out_->size() < length) return false;
        out_->append(reinterpret_cast<const char*>(in_ + pos_), length);
        pos_ += length;
        return true;
    }

    // Builds a canonical code from its lengths. Returns the number of unused
    // code values: negative when over-subscribed, positive when incomplete.
    static int build(Huffman* h, const uint8_t* lengths, int n) {
        for (int len = 0; len < 16; ++len) h->count[len] = 0;
        for (int s = 0; s < n; ++s) h->count[lengths[s]]++;
        if (h->count[0] == n) return 0;
        int left = 1;
        for (int len = 1; len < 16; ++len) {
            left <<= 1;
            left -= h->count[len];
            if (left < 0) return left;
        }
        uint16_t offsets[16];
        offsets[1] = 0;
        for (int len = 1; len < 15; ++len) offsets[len + 1] = offsets[len] + h->count[len];
        for (int s = 0; s < n; ++s) {
            if (lengths[s] != 0) h->symbol[offsets[lengths[s]]++] = static_cast<uint16_t>(s);
        }
        return left;
    }

    bool decode(const Huffman& h, int* symbol) {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len < 16; ++len) {
            uint32_t bit = 0;
            if (!bits(1, &bit)) return false;
            code |= static_cast<int>(bit);
            const int count = h.count[len];
            if (code - count < first) {
                *symbol = h.symbol[index + (code - first)];
                return true;
            }
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return false;
    }

    bool codes(const Huffman& lengthCode, const Huffman& distanceCode) {
        static const uint16_t kLengthBase[29] = {3,  4,  5,  6,  7,  8,  9,  10,  11,  13,
                                                 15, 17, 19, 23, 27, 31, 35, 43,  51,  59,
                                                 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const uint8_t kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const uint16_t kDistanceBase[30] = {
                1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const uint8_t kDistanceExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                   6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            int symbol = 0;
            if (!decode(lengthCode, &symbol)) return false;
            if (symbol < 256) {
                if (!put(static_cast<uint8_t>(symbol))) return false;
                continue;
            }
            if (symbol == 256) return true;
            symbol -= 257;
            if (symbol >= 29) return false;
            uint32_t extra = 0;
            if (!bits(kLengthExtra[symbol], &extra)) return false;
            const uint32_t length = kLengthBase[symbol] + extra;
            int distanceSymbol = 0;
            if (!decode(distanceCode, &distanceSymbol) || distanceSymbol >= 30) return false;
            if (!bits(kDistanceExtra[distanceSymbol], &extra)) return false;
            const size_t distance = kDistanceBase[distanceSymbol] + extra;
            if (distance > out_->size() || expected_ - out_->size() < length) return false;
            for (uint32_t i = 0; i < length; ++i) {
                out_->push_back((*out_)[out_->size() - distance]);
            }
        }
    }

    bool fixed() {
        uint8_t lengths[288];
        int s = 0;
        for (; s < 144; ++s) lengths[s] = 8;
        for (; s < 256; ++s) lengths[s] = 9;
        for (; s < 280; ++s) lengths[s] = 7;
        for (; s < 288; ++s) lengths[s] = 8;
        Huffman lengthCode;
        build(&lengthCode, lengths, 288);
        uint8_t distances[30];
        for (int d = 0; d < 30; ++d) distances[d] = 5;
        Huffman distanceCode;
        build(&distanceCode, distances, 30);  // incomplete by design (RFC 1951 3.2.6)
        return codes(lengthCode, distanceCode);
    }

    bool dynamic() {
        uint32_t value = 0;
        if (!bits(5, &value)) return false;
        const int lengthCount = static_cast<int>(value) + 257;
        if (!bits(5, &value)) return false;
        const int distanceCount = static_cast<int>(value) + 1;
        if (!bits(4, &value)) return false;
        const int codeCount = static_cast<int>(value) + 4;
        if (lengthCount > 286 || distanceCount > 30) return false;

        static const uint8_t kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                           11, 4, 12, 3, 13, 2, 14, 1, 15};
        uint8_t lengths[320] = {};
        for (int i = 0; i < codeCount; ++i) {
            if (!bits(3, &value)) return false;
            lengths[kOrder[i]] = static_cast<uint8_t>(value);
        }
        Huffman lengthCode;
        if (build(&lengthCode, lengths, 19) != 0) return false;  // must be complete

        int index = 0;
        while (index < lengthCount + distanceCount) {
            int symbol = 0;
            if (!decode(lengthCode, &symbol)) return false;
            if (symbol < 16) {
                lengths[index++] = static_cast<uint8_t>(symbol);
                continue;
            }
            uint8_t repeated = 0;
            uint32_t repeat = 0;
            if (symbol == 16) {
                if (index == 0) return false;
                repeated = lengths[index - 1];
                if (!bits(2, &repeat)) return false;
                repeat += 3;
            } else if (symbol == 17) {
                if (!bits(3, &repeat)) return false;
                repeat += 3;
            } else {
                if (!bits(7, &repeat)) return false;
                repeat += 11;
            }
            if (index + static_cast<int>(repeat) > lengthCount + distanceCount) return false;
            while (repeat-- > 0) lengths[index++] = repeated;
        }
        if (lengths[256] == 0) return false;  // no end-of-block code

        // An incomplete code is allowed only when it is a single code of one
        // bit (RFC 1951 3.2.7, as zlib reads it); an over-subscribed one never.
        Huffman literalCode;
        int left = build(&literalCode, lengths, lengthCount);
        if (left < 0
            || (left > 0 && lengthCount != literalCode.count[0] + literalCode.count[1])) {
            return false;
        }
        Huffman distanceCode;
        left = build(&distanceCode, lengths + lengthCount, distanceCount);
        if (left < 0
            || (left > 0 && distanceCount != distanceCode.count[0] + distanceCode.count[1])) {
            return false;
        }
        return codes(literalCode, distanceCode);
    }

    const uint8_t* in_;
    size_t inLength_;
    size_t pos_ = 0;
    uint32_t bitBuffer_ = 0;
    int bitCount_ = 0;
    size_t expected_;
    std::string* out_;
};

struct ZipEntry {
    std::string name;
    uint16_t flags = 0;
    uint16_t method = 0;
    uint32_t crc = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint32_t localHeaderOffset = 0;
};

struct ZipDirectory {
    std::vector<ZipEntry> entries;
    uint32_t directoryOffset = 0;
};

GlbImportStatus readZipDirectory(const uint8_t* bytes, size_t length, ZipDirectory* out) {
    if (length < 22) {
        return GlbImportStatus::MalformedZip;
    }
    // The end-of-central-directory record is the last 22 bytes plus a comment
    // of at most 65535; the one whose comment length reaches exactly to the
    // end of the file is it.
    size_t eocd = std::numeric_limits<size_t>::max();
    const size_t lowest = length - 22 > 65535u ? length - 22 - 65535u : 0;
    for (size_t at = length - 22 + 1; at-- > lowest;) {
        if (readU32LE(bytes + at) == 0x06054B50u
            && at + 22u + readU16LE(bytes + at + 20) == length) {
            eocd = at;
            break;
        }
    }
    if (eocd == std::numeric_limits<size_t>::max()) {
        return GlbImportStatus::MalformedZip;
    }
    if (eocd >= 20 && readU32LE(bytes + eocd - 20) == 0x07064B50u) {
        return GlbImportStatus::UnsupportedZipFeature;  // ZIP64
    }
    const uint16_t disk = readU16LE(bytes + eocd + 4);
    const uint16_t directoryDisk = readU16LE(bytes + eocd + 6);
    const uint16_t entriesOnDisk = readU16LE(bytes + eocd + 8);
    const uint16_t entriesTotal = readU16LE(bytes + eocd + 10);
    const uint32_t directorySize = readU32LE(bytes + eocd + 12);
    const uint32_t directoryOffset = readU32LE(bytes + eocd + 16);
    if (entriesTotal == 0xFFFFu || directorySize == 0xFFFFFFFFu
        || directoryOffset == 0xFFFFFFFFu) {
        return GlbImportStatus::UnsupportedZipFeature;  // ZIP64 markers
    }
    if (disk != 0 || directoryDisk != 0 || entriesOnDisk != entriesTotal) {
        return GlbImportStatus::UnsupportedZipFeature;  // a spanned archive
    }
    if (static_cast<uint64_t>(directoryOffset) + directorySize > eocd) {
        return GlbImportStatus::MalformedZip;
    }
    ZipDirectory directory;
    directory.directoryOffset = directoryOffset;
    size_t at = directoryOffset;
    const size_t stop = static_cast<size_t>(directoryOffset) + directorySize;
    for (uint32_t i = 0; i < entriesTotal; ++i) {
        if (stop - at < 46 || readU32LE(bytes + at) != 0x02014B50u) {
            return GlbImportStatus::MalformedZip;
        }
        ZipEntry entry;
        entry.flags = readU16LE(bytes + at + 8);
        entry.method = readU16LE(bytes + at + 10);
        entry.crc = readU32LE(bytes + at + 16);
        entry.compressedSize = readU32LE(bytes + at + 20);
        entry.uncompressedSize = readU32LE(bytes + at + 24);
        const uint16_t nameLength = readU16LE(bytes + at + 28);
        const uint16_t extraLength = readU16LE(bytes + at + 30);
        const uint16_t commentLength = readU16LE(bytes + at + 32);
        entry.localHeaderOffset = readU32LE(bytes + at + 42);
        const size_t recordLength = 46u + nameLength + extraLength + commentLength;
        if (stop - at < recordLength) {
            return GlbImportStatus::MalformedZip;
        }
        entry.name.assign(reinterpret_cast<const char*>(bytes + at + 46), nameLength);
        directory.entries.push_back(std::move(entry));
        at += recordLength;
    }
    if (at != stop) {
        return GlbImportStatus::MalformedZip;
    }
    *out = std::move(directory);
    return GlbImportStatus::Ok;
}

GlbImportStatus extractZipEntry(const uint8_t* bytes, size_t length, const ZipDirectory& directory,
                                const ZipEntry& entry, std::string* out) {
    if ((entry.flags & 0x0001u) != 0u || (entry.flags & 0x0040u) != 0u
        || (entry.flags & 0x2000u) != 0u) {
        return GlbImportStatus::UnsupportedZipFeature;  // encrypted
    }
    if (entry.method != 0 && entry.method != 8) {
        return GlbImportStatus::UnsupportedZipFeature;
    }
    if (entry.compressedSize == 0xFFFFFFFFu || entry.uncompressedSize == 0xFFFFFFFFu
        || entry.localHeaderOffset == 0xFFFFFFFFu) {
        return GlbImportStatus::UnsupportedZipFeature;  // ZIP64
    }
    if (entry.uncompressedSize > kMaxImportedGlbBytes) {
        return GlbImportStatus::TooLarge;
    }
    const size_t header = entry.localHeaderOffset;
    if (header > length || length - header < 30 || readU32LE(bytes + header) != 0x04034B50u) {
        return GlbImportStatus::MalformedZip;
    }
    const uint16_t localMethod = readU16LE(bytes + header + 8);
    const uint16_t nameLength = readU16LE(bytes + header + 26);
    const uint16_t extraLength = readU16LE(bytes + header + 28);
    const uint64_t dataStart = static_cast<uint64_t>(header) + 30u + nameLength + extraLength;
    // The data must end before the central directory begins, which also
    // proves the local name lies inside the file.
    if (localMethod != entry.method
        || dataStart + entry.compressedSize > directory.directoryOffset) {
        return GlbImportStatus::MalformedZip;
    }
    if (std::string(reinterpret_cast<const char*>(bytes + header + 30), nameLength)
        != entry.name) {
        return GlbImportStatus::MalformedZip;
    }
    const uint8_t* data = bytes + dataStart;
    std::string content;
    if (entry.method == 0) {
        if (entry.compressedSize != entry.uncompressedSize) {
            return GlbImportStatus::MalformedZip;
        }
        content.assign(reinterpret_cast<const char*>(data), entry.compressedSize);
    } else {
        Inflater inflater(data, entry.compressedSize, entry.uncompressedSize, &content);
        if (!inflater.run()) {
            return GlbImportStatus::MalformedZip;
        }
    }
    if (crc32Of(reinterpret_cast<const uint8_t*>(content.data()), content.size()) != entry.crc) {
        return GlbImportStatus::ZipChecksumMismatch;
    }
    *out = std::move(content);
    return GlbImportStatus::Ok;
}

const ZipEntry* findZipEntry(const ZipDirectory& directory, const std::string& name) {
    for (const ZipEntry& entry : directory.entries) {
        if (equalsIgnoreCase(entry.name, name)) {  // OPC part names ignore ASCII case
            return &entry;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// A small, bounded XML reader
// ---------------------------------------------------------------------------
//
// Enough of XML 1.0 for an OPC package's parts: elements, attributes, the five
// predefined entities and character references, comments, processing
// instructions and CDATA, and namespace prefixes resolved per element. A DTD
// is REFUSED (MalformedXml), which is what makes entity expansion impossible
// rather than merely bounded. Text content is skipped: no 3MF geometry lives
// in it.

constexpr int kMaxXmlDepth = 256;
constexpr size_t kMaxXmlAttributes = 256;

struct XmlAttribute {
    std::string name;  // qualified, as written
    std::string value; // entity-decoded
};

struct XmlElement {
    bool isEnd = false;
    bool selfClosing = false;
    std::string qualifiedName;
    std::string localName;
    std::string namespaceUri;
    std::vector<XmlAttribute> attributes;
    size_t attributeCount = 0;

    // An UNPREFIXED attribute by name; null when absent. (A prefixed one --
    // `p:UUID`, `xml:lang` -- belongs to another vocabulary.)
    const std::string* attribute(const char* name) const {
        for (size_t i = 0; i < attributeCount; ++i) {
            if (attributes[i].name == name) {
                return &attributes[i].value;
            }
        }
        return nullptr;
    }
};

class XmlReader {
public:
    XmlReader(const char* text, size_t length) : p_(text), end_(text + length) {
        if (length >= 3 && static_cast<unsigned char>(text[0]) == 0xEFu
            && static_cast<unsigned char>(text[1]) == 0xBBu
            && static_cast<unsigned char>(text[2]) == 0xBFu) {
            p_ += 3;
        }
    }

    // The next start or end tag. False at the end of the document (`*why` is
    // Ok) or on a refusal (`*why` says which).
    bool next(XmlElement* element, GlbImportStatus* why) {
        *why = GlbImportStatus::Ok;
        if (pendingEnd_) {
            // The end of a self-closing tag, reported as its own event so a
            // consumer sees every element open and close the same way.
            pendingEnd_ = false;
            element->isEnd = true;
            element->selfClosing = false;
            element->attributeCount = 0;
            element->qualifiedName = pendingQualified_;
            element->localName = pendingLocal_;
            element->namespaceUri = pendingNamespace_;
            popScope();
            return true;
        }
        for (;;) {
            const char* text = p_;
            while (p_ < end_ && *p_ != '<') ++p_;
            if (stack_.empty()) {
                for (const char* q = text; q < p_; ++q) {
                    if (!isSpace(*q)) return fail(why);  // text outside the root
                }
            }
            if (p_ >= end_) {
                if (!stack_.empty() || !sawRoot_) return fail(why);
                return false;
            }
            if (startsWith("<?")) {
                if (!skipPast("?>")) return fail(why);
                continue;
            }
            if (startsWith("<!--")) {
                if (!skipPast("-->")) return fail(why);
                continue;
            }
            if (startsWith("<![CDATA[")) {
                if (stack_.empty() || !skipPast("]]>")) return fail(why);
                continue;
            }
            if (startsWith("<!")) {
                return fail(why);  // a DTD, which a 3MF part must not carry
            }
            if (startsWith("</")) {
                p_ += 2;
                const char* nb = p_;
                while (p_ < end_ && !isSpace(*p_) && *p_ != '>') ++p_;
                const std::string name(nb, p_);
                while (p_ < end_ && isSpace(*p_)) ++p_;
                if (p_ >= end_ || *p_ != '>' || stack_.empty() || stack_.back() != name) {
                    return fail(why);
                }
                ++p_;
                element->isEnd = true;
                element->selfClosing = false;
                element->qualifiedName = name;
                resolve(element);
                popScope();
                return true;
            }
            return startTag(element, why);
        }
    }

private:
    struct Binding {
        std::string prefix;
        std::string uri;
        size_t depth = 0;
    };

    bool fail(GlbImportStatus* why) {
        *why = GlbImportStatus::MalformedXml;
        return false;
    }

    bool startsWith(const char* word) const {
        const size_t n = std::strlen(word);
        return static_cast<size_t>(end_ - p_) >= n && std::memcmp(p_, word, n) == 0;
    }

    bool skipPast(const char* word) {
        const size_t n = std::strlen(word);
        for (const char* q = p_; static_cast<size_t>(end_ - q) >= n; ++q) {
            if (std::memcmp(q, word, n) == 0) {
                p_ = q + n;
                return true;
            }
        }
        return false;
    }

    static bool nameChar(char c) {
        return !isSpace(c) && c != '/' && c != '>' && c != '=' && c != '<' && c != '"'
               && c != '\'';
    }

    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp < 0x80u) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800u) {
            out += static_cast<char>(0xC0u | (cp >> 6));
            out += static_cast<char>(0x80u | (cp & 0x3Fu));
        } else if (cp < 0x10000u) {
            out += static_cast<char>(0xE0u | (cp >> 12));
            out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            out += static_cast<char>(0x80u | (cp & 0x3Fu));
        } else {
            out += static_cast<char>(0xF0u | (cp >> 18));
            out += static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
            out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            out += static_cast<char>(0x80u | (cp & 0x3Fu));
        }
    }

    static bool decodeValue(const char* b, const char* e, std::string* out) {
        out->clear();
        while (b < e) {
            if (*b == '<') return false;
            if (*b != '&') {
                *out += *b++;
                continue;
            }
            const char* semicolon = b;
            while (semicolon < e && *semicolon != ';' && semicolon - b < 12) ++semicolon;
            if (semicolon >= e || *semicolon != ';') return false;
            const char* name = b + 1;
            if (equals(name, semicolon, "lt")) *out += '<';
            else if (equals(name, semicolon, "gt")) *out += '>';
            else if (equals(name, semicolon, "amp")) *out += '&';
            else if (equals(name, semicolon, "quot")) *out += '"';
            else if (equals(name, semicolon, "apos")) *out += '\'';
            else if (name < semicolon && *name == '#') {
                const bool hex = name + 1 < semicolon && name[1] == 'x';
                const char* digit = name + (hex ? 2 : 1);
                if (digit == semicolon) return false;
                uint32_t cp = 0;
                for (; digit < semicolon; ++digit) {
                    int v = -1;
                    if (*digit >= '0' && *digit <= '9') v = *digit - '0';
                    else if (hex && *digit >= 'a' && *digit <= 'f') v = *digit - 'a' + 10;
                    else if (hex && *digit >= 'A' && *digit <= 'F') v = *digit - 'A' + 10;
                    if (v < 0) return false;
                    cp = cp * (hex ? 16u : 10u) + static_cast<uint32_t>(v);
                    if (cp > 0x10FFFFu) return false;
                }
                const bool legal = cp == 0x9u || cp == 0xAu || cp == 0xDu
                                   || (cp >= 0x20u && cp <= 0xD7FFu)
                                   || (cp >= 0xE000u && cp <= 0xFFFDu) || cp >= 0x10000u;
                if (!legal) return false;
                appendUtf8(*out, cp);
            } else {
                return false;  // no DTD, so no other entity exists
            }
            b = semicolon + 1;
        }
        return true;
    }

    bool startTag(XmlElement* element, GlbImportStatus* why) {
        if (stack_.empty() && sawRoot_) return fail(why);  // a second root
        ++p_;  // '<'
        const char* nb = p_;
        while (p_ < end_ && nameChar(*p_)) ++p_;
        if (p_ == nb) return fail(why);
        element->isEnd = false;
        element->selfClosing = false;
        element->qualifiedName.assign(nb, p_);
        element->attributeCount = 0;
        const size_t depth = stack_.size() + 1;
        for (;;) {
            const char* before = p_;
            while (p_ < end_ && isSpace(*p_)) ++p_;
            if (p_ >= end_) return fail(why);
            if (*p_ == '>') {
                ++p_;
                break;
            }
            if (*p_ == '/') {
                if (p_ + 1 >= end_ || p_[1] != '>') return fail(why);
                p_ += 2;
                element->selfClosing = true;
                break;
            }
            if (p_ == before) return fail(why);  // attributes must be separated
            const char* ab = p_;
            while (p_ < end_ && nameChar(*p_)) ++p_;
            if (p_ == ab) return fail(why);
            const char* ae = p_;
            while (p_ < end_ && isSpace(*p_)) ++p_;
            if (p_ >= end_ || *p_ != '=') return fail(why);
            ++p_;
            while (p_ < end_ && isSpace(*p_)) ++p_;
            if (p_ >= end_ || (*p_ != '"' && *p_ != '\'')) return fail(why);
            const char quote = *p_++;
            const char* vb = p_;
            while (p_ < end_ && *p_ != quote) ++p_;
            if (p_ >= end_) return fail(why);
            const char* ve = p_++;
            if (element->attributeCount >= kMaxXmlAttributes) return fail(why);
            if (element->attributes.size() <= element->attributeCount) {
                element->attributes.emplace_back();
            }
            XmlAttribute& attribute = element->attributes[element->attributeCount];
            attribute.name.assign(ab, ae);
            for (size_t i = 0; i < element->attributeCount; ++i) {
                if (element->attributes[i].name == attribute.name) return fail(why);
            }
            if (!decodeValue(vb, ve, &attribute.value)) return fail(why);
            ++element->attributeCount;
            if (attribute.name == "xmlns") {
                bindings_.push_back(Binding{std::string(), attribute.value, depth});
            } else if (attribute.name.compare(0, 6, "xmlns:") == 0) {
                bindings_.push_back(Binding{attribute.name.substr(6), attribute.value, depth});
            }
        }
        if (static_cast<int>(depth) > kMaxXmlDepth) return fail(why);
        stack_.push_back(element->qualifiedName);
        sawRoot_ = true;
        if (!resolve(element)) {
            *why = GlbImportStatus::MalformedXml;
            return false;
        }
        if (element->selfClosing) {
            pendingEnd_ = true;
            pendingQualified_ = element->qualifiedName;
            pendingLocal_ = element->localName;
            pendingNamespace_ = element->namespaceUri;
        }
        return true;
    }

    // Splits the qualified name and finds its namespace among the bindings in
    // scope. An unbound prefix is not well-formed.
    bool resolve(XmlElement* element) {
        const std::string& qualified = element->qualifiedName;
        const size_t colon = qualified.find(':');
        const std::string prefix = colon == std::string::npos ? std::string()
                                                              : qualified.substr(0, colon);
        element->localName = colon == std::string::npos ? qualified : qualified.substr(colon + 1);
        for (size_t i = bindings_.size(); i-- > 0;) {
            if (bindings_[i].prefix == prefix) {
                element->namespaceUri = bindings_[i].uri;
                return true;
            }
        }
        element->namespaceUri.clear();
        return prefix.empty();
    }

    void popScope() {
        const size_t depth = stack_.size();
        while (!bindings_.empty() && bindings_.back().depth >= depth) {
            bindings_.pop_back();
        }
        if (!stack_.empty()) {
            stack_.pop_back();
        }
    }

public:
    // The namespace a prefix is bound to at the current element; empty when
    // unbound.
    std::string namespaceOf(const std::string& prefix) const {
        for (size_t i = bindings_.size(); i-- > 0;) {
            if (bindings_[i].prefix == prefix) {
                return bindings_[i].uri;
            }
        }
        return std::string();
    }

private:
    const char* p_;
    const char* end_;
    std::vector<std::string> stack_;
    std::vector<Binding> bindings_;
    bool sawRoot_ = false;
    bool pendingEnd_ = false;
    std::string pendingQualified_;
    std::string pendingLocal_;
    std::string pendingNamespace_;
};

// ---------------------------------------------------------------------------
// 3MF
// ---------------------------------------------------------------------------

constexpr const char* kThreeMfCoreNamespace =
        "http://schemas.microsoft.com/3dmanufacturing/core/2015/02";
constexpr const char* kThreeMfMaterialNamespace =
        "http://schemas.microsoft.com/3dmanufacturing/material/2015/02";
constexpr const char* kThreeMfProductionNamespace =
        "http://schemas.microsoft.com/3dmanufacturing/production/2015/06";
constexpr const char* kThreeMfModelRelationship =
        "http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel";
constexpr const char* kOpcRelationshipsNamespace =
        "http://schemas.openxmlformats.org/package/2006/relationships";

// A 3MF transform: row vectors, `p' = [x y z 1] * M` with M's last column
// fixed at (0, 0, 0, 1), stated as "m00 m01 m02 m10 m11 m12 m20 m21 m22 m30
// m31 m32" (3MF Core, 3.3). Held in that order.
struct ThreeMfTransform {
    double m[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};

    void apply(const double p[3], double out[3]) const {
        for (int c = 0; c < 3; ++c) {
            out[c] = p[0] * m[c] + p[1] * m[3 + c] + p[2] * m[6 + c] + m[9 + c];
        }
    }

    double determinant() const {
        return m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6])
               + m[2] * (m[3] * m[7] - m[4] * m[6]);
    }

    double magnitude() const {
        double largest = 0.0;
        for (int i = 0; i < 9; ++i) largest = std::fmax(largest, std::fabs(m[i]));
        return largest;
    }
};

// `first` then `second`: p * first * second.
ThreeMfTransform compose(const ThreeMfTransform& first, const ThreeMfTransform& second) {
    ThreeMfTransform out;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) {
            double sum = r == 3 ? second.m[9 + c] : 0.0;
            for (int k = 0; k < 3; ++k) {
                sum += first.m[r * 3 + k] * second.m[k * 3 + c];
            }
            out.m[r * 3 + c] = sum;
        }
    }
    return out;
}

GlbImportStatus parseTransform(const std::string& text, ThreeMfTransform* out) {
    TokenCursor tokens;
    tokens.p = text.data();
    tokens.end = text.data() + text.size();
    ThreeMfTransform transform;
    for (int i = 0; i < 12; ++i) {
        if (!tokens.nextNumber(&transform.m[i])) {
            return GlbImportStatus::MalformedThreeMf;
        }
        if (!std::isfinite(transform.m[i])) {
            return GlbImportStatus::NonFiniteValue;
        }
    }
    if (!tokens.onlySpaceLeft()) {
        return GlbImportStatus::MalformedThreeMf;
    }
    *out = transform;
    return GlbImportStatus::Ok;
}

bool parseAttributeNumber(const std::string* text, double* out) {
    return text != nullptr && parseNumber(text->data(), text->data() + text->size(), out);
}

bool parseAttributeIndex(const std::string* text, uint64_t limit, uint64_t* out) {
    double value = 0.0;
    return parseAttributeNumber(text, &value) && asCount(value, limit, out);
}

struct ThreeMfComponent {
    uint64_t objectId = 0;
    ThreeMfTransform transform;
};

struct ThreeMfObject {
    uint64_t id = 0;
    std::string name;
    bool hasMesh = false;
    bool hasComponents = false;
    std::vector<float> vertices;    // model units, xyz
    std::vector<uint32_t> triangles;
    std::vector<ThreeMfComponent> components;
};

struct ThreeMfItem {
    uint64_t objectId = 0;
    ThreeMfTransform transform;
};

GlbImportStatus parseThreeMfModel(const std::string& xml, double* outUnitScale,
                                  std::vector<ThreeMfObject>* outObjects,
                                  std::vector<ThreeMfItem>* outItems) {
    XmlReader reader(xml.data(), xml.size());
    XmlElement element;
    GlbImportStatus why = GlbImportStatus::Ok;
    std::vector<std::string> path;  // local names of the open CORE elements
    int foreignDepth = 0;           // > 0 while inside an element of another vocabulary
    std::vector<ThreeMfObject> objects;
    std::vector<ThreeMfItem> items;
    double unitScale = 0.001;
    bool sawModel = false;
    uint64_t triangleTotal = 0;
    ThreeMfObject* current = nullptr;

    const auto inPath = [&path](std::initializer_list<const char*> names) {
        if (path.size() != names.size()) return false;
        size_t i = 0;
        for (const char* name : names) {
            if (path[i++] != name) return false;
        }
        return true;
    };

    while (reader.next(&element, &why)) {
        if (element.isEnd) {
            if (foreignDepth > 0) {
                --foreignDepth;
                continue;
            }
            if (!path.empty() && path.back() == "object") {
                current = nullptr;
            }
            if (!path.empty()) path.pop_back();
            continue;
        }
        if (foreignDepth > 0 || element.namespaceUri != kThreeMfCoreNamespace) {
            if (!sawModel) {
                return GlbImportStatus::MalformedThreeMf;  // the root is not a 3MF model
            }
            ++foreignDepth;  // another vocabulary: skipped with all it holds
            continue;
        }
        const std::string& name = element.localName;
        if (!sawModel) {
            if (name != "model") return GlbImportStatus::MalformedThreeMf;
            sawModel = true;
            const std::string* unit = element.attribute("unit");
            if (unit != nullptr) {
                if (*unit == "micron") unitScale = 0.000001;
                else if (*unit == "millimeter") unitScale = 0.001;
                else if (*unit == "centimeter") unitScale = 0.01;
                else if (*unit == "inch") unitScale = 0.0254;
                else if (*unit == "foot") unitScale = 0.3048;
                else if (*unit == "meter") unitScale = 1.0;
                else return GlbImportStatus::UnsupportedThreeMfFeature;
            }
            // A REQUIRED extension changes what the document means, so an
            // extension this reader does not implement is refused. Materials
            // (appearance, which no ForgeShape import keeps) and Production
            // (identity; its one geometric construct, a component in another
            // part, is refused where it appears) are the two it can honour.
            if (const std::string* required = element.attribute("requiredextensions")) {
                TokenCursor tokens;
                tokens.p = required->data();
                tokens.end = required->data() + required->size();
                const char* pb = nullptr;
                const char* pe = nullptr;
                while (tokens.next(&pb, &pe)) {
                    const std::string uri = reader.namespaceOf(std::string(pb, pe));
                    if (uri != kThreeMfMaterialNamespace && uri != kThreeMfProductionNamespace) {
                        return GlbImportStatus::UnsupportedThreeMfFeature;
                    }
                }
            }
        } else if (inPath({"model", "resources"}) && name == "object") {
            uint64_t id = 0;
            if (!parseAttributeIndex(element.attribute("id"), 0x7FFFFFFFull, &id) || id == 0) {
                return GlbImportStatus::MalformedThreeMf;
            }
            for (const ThreeMfObject& existing : objects) {
                if (existing.id == id) return GlbImportStatus::MalformedThreeMf;
            }
            if (objects.size() >= 65536u) return GlbImportStatus::TooLarge;
            objects.emplace_back();
            current = &objects.back();
            current->id = id;
            if (const std::string* objectName = element.attribute("name")) {
                current->name = *objectName;
            }
        } else if (inPath({"model", "resources", "object"}) && name == "mesh") {
            if (current == nullptr || current->hasMesh || current->hasComponents) {
                return GlbImportStatus::MalformedThreeMf;
            }
            current->hasMesh = true;
        } else if (inPath({"model", "resources", "object", "mesh", "vertices"})
                   && name == "vertex") {
            double xyz[3];
            if (!parseAttributeNumber(element.attribute("x"), &xyz[0])
                || !parseAttributeNumber(element.attribute("y"), &xyz[1])
                || !parseAttributeNumber(element.attribute("z"), &xyz[2])) {
                return GlbImportStatus::MalformedThreeMf;
            }
            if (current->vertices.size() / 3u >= kMaxImportedVerticesPerMesh) {
                return GlbImportStatus::TooLarge;
            }
            for (double value : xyz) {
                const float narrowed = static_cast<float>(value);
                if (!std::isfinite(value) || !std::isfinite(narrowed)) {
                    return GlbImportStatus::NonFiniteValue;
                }
                current->vertices.push_back(narrowed);
            }
        } else if (inPath({"model", "resources", "object", "mesh", "triangles"})
                   && name == "triangle") {
            uint64_t v[3];
            if (!parseAttributeIndex(element.attribute("v1"), 0xFFFFFFFEull, &v[0])
                || !parseAttributeIndex(element.attribute("v2"), 0xFFFFFFFEull, &v[1])
                || !parseAttributeIndex(element.attribute("v3"), 0xFFFFFFFEull, &v[2])) {
                return GlbImportStatus::MalformedThreeMf;
            }
            if (++triangleTotal > kMaxTrianglesTotal) return GlbImportStatus::TooLarge;
            for (uint64_t index : v) {
                current->triangles.push_back(static_cast<uint32_t>(index));
            }
        } else if (inPath({"model", "resources", "object"}) && name == "components") {
            if (current == nullptr || current->hasMesh || current->hasComponents) {
                return GlbImportStatus::MalformedThreeMf;
            }
            current->hasComponents = true;
        } else if (inPath({"model", "resources", "object", "components"})
                   && name == "component") {
            ThreeMfComponent component;
            if (!parseAttributeIndex(element.attribute("objectid"), 0x7FFFFFFFull,
                                     &component.objectId)) {
                return GlbImportStatus::MalformedThreeMf;
            }
            for (size_t i = 0; i < element.attributeCount; ++i) {
                const std::string& attribute = element.attributes[i].name;
                const size_t colon = attribute.find(':');
                if (colon != std::string::npos && attribute.compare(colon + 1, 4, "path") == 0
                    && attribute.size() == colon + 5
                    && reader.namespaceOf(attribute.substr(0, colon))
                               == kThreeMfProductionNamespace) {
                    return GlbImportStatus::UnsupportedThreeMfFeature;  // another part
                }
            }
            if (const std::string* transform = element.attribute("transform")) {
                const GlbImportStatus status = parseTransform(*transform, &component.transform);
                if (status != GlbImportStatus::Ok) return status;
            }
            if (current->components.size() >= 65536u) return GlbImportStatus::TooLarge;
            current->components.push_back(component);
        } else if (inPath({"model", "build"}) && name == "item") {
            ThreeMfItem item;
            if (!parseAttributeIndex(element.attribute("objectid"), 0x7FFFFFFFull,
                                     &item.objectId)) {
                return GlbImportStatus::MalformedThreeMf;
            }
            for (size_t i = 0; i < element.attributeCount; ++i) {
                const std::string& attribute = element.attributes[i].name;
                const size_t colon = attribute.find(':');
                if (colon != std::string::npos && attribute.compare(colon + 1, 4, "path") == 0
                    && attribute.size() == colon + 5
                    && reader.namespaceOf(attribute.substr(0, colon))
                               == kThreeMfProductionNamespace) {
                    return GlbImportStatus::UnsupportedThreeMfFeature;
                }
            }
            if (const std::string* transform = element.attribute("transform")) {
                const GlbImportStatus status = parseTransform(*transform, &item.transform);
                if (status != GlbImportStatus::Ok) return status;
            }
            if (items.size() >= kMaxImportedMeshes) return GlbImportStatus::TooLarge;
            items.push_back(item);
        }
        path.push_back(name);  // a self-closing tag's end follows as its own event
    }
    if (why != GlbImportStatus::Ok) {
        return why;
    }
    if (!sawModel) {
        return GlbImportStatus::MalformedThreeMf;
    }
    *outUnitScale = unitScale;
    *outObjects = std::move(objects);
    *outItems = std::move(items);
    return GlbImportStatus::Ok;
}

const ThreeMfObject* findObject(const std::vector<ThreeMfObject>& objects, uint64_t id) {
    for (const ThreeMfObject& object : objects) {
        if (object.id == id) return &object;
    }
    return nullptr;
}

// Appends one object, placed by `transform` and scaled to metres, to `mesh`,
// following components recursively. `visiting` holds the objects on the
// current path, so a component cycle is caught.
GlbImportStatus appendThreeMfObject(const std::vector<ThreeMfObject>& objects, uint64_t id,
                                    const ThreeMfTransform& transform, double unitScale,
                                    int depth, std::vector<uint64_t>& visiting, RawMesh* mesh) {
    const ThreeMfObject* object = findObject(objects, id);
    if (object == nullptr || (!object->hasMesh && !object->hasComponents)) {
        return GlbImportStatus::MalformedThreeMf;
    }
    for (uint64_t open : visiting) {
        if (open == id) return GlbImportStatus::MalformedThreeMf;  // a cycle
    }
    if (depth > kMaxComponentDepth) {
        return GlbImportStatus::UnsupportedThreeMfFeature;
    }
    if (object->hasComponents) {
        visiting.push_back(id);
        for (const ThreeMfComponent& component : object->components) {
            const GlbImportStatus status =
                    appendThreeMfObject(objects, component.objectId,
                                        compose(component.transform, transform), unitScale,
                                        depth + 1, visiting, mesh);
            if (status != GlbImportStatus::Ok) return status;
        }
        visiting.pop_back();
        return GlbImportStatus::Ok;
    }

    const double determinant = transform.determinant();
    const double magnitude = transform.magnitude();
    if (!std::isfinite(determinant)
        || std::fabs(determinant) <= 1e-12 * (magnitude * magnitude * magnitude + 1.0)) {
        return GlbImportStatus::SingularNodeTransform;
    }
    // A mirroring transform turns every front face into a back face; the
    // winding is corrected so the outside stays the outside, as the GLB
    // reader's node bake does.
    const bool mirrored = determinant < 0.0;

    const uint64_t vertexCount = object->vertices.size() / 3u;
    const uint64_t base = mesh->positions.size() / 3u;
    if (base + vertexCount > kMaxImportedVerticesPerMesh
        || mesh->indices.size() / 3u + object->triangles.size() / 3u > kMaxTrianglesPerMesh) {
        return GlbImportStatus::TooLarge;
    }
    for (uint64_t v = 0; v < vertexCount; ++v) {
        const double local[3] = {object->vertices[v * 3], object->vertices[v * 3 + 1],
                                 object->vertices[v * 3 + 2]};
        double placed[3];
        transform.apply(local, placed);
        for (int c = 0; c < 3; ++c) {
            const float metres = static_cast<float>(placed[c] * unitScale);
            if (!std::isfinite(metres)) return GlbImportStatus::NonFiniteValue;
            mesh->positions.push_back(metres);
        }
    }
    for (size_t t = 0; t + 2 < object->triangles.size(); t += 3) {
        const uint32_t a = object->triangles[t];
        const uint32_t b = object->triangles[t + 1];
        const uint32_t c = object->triangles[t + 2];
        if (a >= vertexCount || b >= vertexCount || c >= vertexCount) {
            return GlbImportStatus::IndexOutOfRange;
        }
        mesh->indices.push_back(static_cast<uint32_t>(base + a));
        mesh->indices.push_back(static_cast<uint32_t>(base + (mirrored ? c : b)));
        mesh->indices.push_back(static_cast<uint32_t>(base + (mirrored ? b : c)));
    }
    return GlbImportStatus::Ok;
}

// The model part named by the package's root relationships, or the
// conventional name when there is no relationship to it.
GlbImportStatus findModelPartName(const uint8_t* bytes, size_t length,
                                  const ZipDirectory& directory, std::string* out) {
    *out = "3D/3dmodel.model";
    const ZipEntry* rels = findZipEntry(directory, "_rels/.rels");
    if (rels == nullptr) {
        return GlbImportStatus::Ok;
    }
    std::string xml;
    GlbImportStatus status = extractZipEntry(bytes, length, directory, *rels, &xml);
    if (status != GlbImportStatus::Ok) {
        return status;
    }
    XmlReader reader(xml.data(), xml.size());
    XmlElement element;
    while (reader.next(&element, &status)) {
        if (element.isEnd || element.localName != "Relationship"
            || element.namespaceUri != kOpcRelationshipsNamespace) {
            continue;
        }
        const std::string* type = element.attribute("Type");
        const std::string* target = element.attribute("Target");
        if (type != nullptr && target != nullptr && *type == kThreeMfModelRelationship) {
            *out = (!target->empty() && (*target)[0] == '/') ? target->substr(1) : *target;
            // Keep reading: the rest of the part must still be well-formed.
        }
    }
    return status;
}

GlbImportStatus readThreeMf(const uint8_t* bytes, size_t length, ParsedGlbScene* out) {
    ZipDirectory directory;
    GlbImportStatus status = readZipDirectory(bytes, length, &directory);
    if (status != GlbImportStatus::Ok) {
        return status;
    }
    std::string partName;
    status = findModelPartName(bytes, length, directory, &partName);
    if (status != GlbImportStatus::Ok) {
        return status;
    }
    const ZipEntry* part = findZipEntry(directory, partName);
    if (part == nullptr) {
        return GlbImportStatus::MissingModelPart;
    }
    std::string xml;
    status = extractZipEntry(bytes, length, directory, *part, &xml);
    if (status != GlbImportStatus::Ok) {
        return status;
    }
    double unitScale = 0.001;
    std::vector<ThreeMfObject> objects;
    std::vector<ThreeMfItem> items;
    status = parseThreeMfModel(xml, &unitScale, &objects, &items);
    if (status != GlbImportStatus::Ok) {
        return status;
    }
    xml.clear();
    xml.shrink_to_fit();

    std::vector<RawMesh> raws;
    for (const ThreeMfItem& item : items) {
        const ThreeMfObject* object = findObject(objects, item.objectId);
        if (object == nullptr) {
            return GlbImportStatus::MalformedThreeMf;
        }
        RawMesh mesh;
        mesh.name = object->name;
        std::vector<uint64_t> visiting;
        status = appendThreeMfObject(objects, item.objectId, item.transform, unitScale, 0,
                                     visiting, &mesh);
        if (status != GlbImportStatus::Ok) {
            return status;
        }
        raws.push_back(std::move(mesh));
    }
    if (raws.empty()) {
        return GlbImportStatus::NothingToImport;
    }
    return finishScene(std::move(raws), out);
}

MeshImportFormat formatFromExtension(const char* hint) {
    if (hint == nullptr || hint[0] == '\0') {
        return MeshImportFormat::Unknown;
    }
    const char* name = hint[0] == '.' ? hint + 1 : hint;
    const char* stop = name + std::strlen(name);
    if (equalsIgnoreCase(name, stop, "glb")) return MeshImportFormat::Glb;
    if (equalsIgnoreCase(name, stop, "stl")) return MeshImportFormat::Stl;
    if (equalsIgnoreCase(name, stop, "obj")) return MeshImportFormat::Obj;
    if (equalsIgnoreCase(name, stop, "ply")) return MeshImportFormat::Ply;
    if (equalsIgnoreCase(name, stop, "3mf")) return MeshImportFormat::ThreeMf;
    return MeshImportFormat::Unknown;
}

}  // namespace

const char* meshImportFormatName(MeshImportFormat format) {
    switch (format) {
        case MeshImportFormat::Unknown: return "unknown";
        case MeshImportFormat::Glb: return "glb";
        case MeshImportFormat::Stl: return "stl";
        case MeshImportFormat::Obj: return "obj";
        case MeshImportFormat::Ply: return "ply";
        case MeshImportFormat::ThreeMf: return "3mf";
    }
    return "unknown";
}

MeshImportFormat detectMeshImportFormat(const uint8_t* bytes, size_t length,
                                        const char* extensionHint) {
    const MeshImportFormat hinted = formatFromExtension(extensionHint);
    if (bytes == nullptr || length == 0) {
        return hinted;
    }
    // In priority order: the magic numbers first, then the structural rules.
    MeshImportFormat matches[5];
    int matchCount = 0;
    if (length >= 4 && std::memcmp(bytes, "glTF", 4) == 0) {
        matches[matchCount++] = MeshImportFormat::Glb;
    }
    if (length >= 4 && bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 3 && bytes[3] == 4) {
        matches[matchCount++] = MeshImportFormat::ThreeMf;
    }
    if (length >= 4 && std::memcmp(bytes, "ply", 3) == 0
        && (bytes[3] == '\n' || (bytes[3] == '\r' && length >= 5 && bytes[4] == '\n'))) {
        matches[matchCount++] = MeshImportFormat::Ply;
    }
    const size_t window = length < 65536u ? length : 65536u;
    if (binaryStlSizeHolds(bytes, length)
        || (startsWithSolid(bytes, length) && containsWord(bytes, window, "facet"))) {
        matches[matchCount++] = MeshImportFormat::Stl;
    }
    if (sniffObj(bytes, length)) {
        matches[matchCount++] = MeshImportFormat::Obj;
    }
    if (matchCount == 0) {
        return hinted;
    }
    for (int i = 0; i < matchCount; ++i) {
        if (matches[i] == hinted) {
            return hinted;
        }
    }
    return matches[0];
}

GlbImportStatus importMeshFile(const uint8_t* bytes, size_t length, MeshImportFormat format,
                               ParsedGlbScene* out) {
    if (out == nullptr || bytes == nullptr || length == 0) {
        return GlbImportStatus::NoData;
    }
    if (static_cast<uint64_t>(length) > kMaxImportedGlbBytes) {
        return GlbImportStatus::TooLarge;
    }
    ParsedGlbScene scene;
    GlbImportStatus status = GlbImportStatus::UnknownFormat;
    switch (format) {
        case MeshImportFormat::Glb: return importGlb(bytes, length, out);
        case MeshImportFormat::Stl: status = readStl(bytes, length, &scene); break;
        case MeshImportFormat::Obj: status = readObj(bytes, length, &scene); break;
        case MeshImportFormat::Ply: status = readPly(bytes, length, &scene); break;
        case MeshImportFormat::ThreeMf: status = readThreeMf(bytes, length, &scene); break;
        case MeshImportFormat::Unknown: status = GlbImportStatus::UnknownFormat; break;
    }
    if (status != GlbImportStatus::Ok) {
        return status;
    }
    *out = std::move(scene);
    return GlbImportStatus::Ok;
}

}  // namespace forgeshape
