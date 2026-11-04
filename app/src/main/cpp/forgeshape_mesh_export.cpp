#include "forgeshape_mesh_export.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

#include "forgeshape_imported_mesh.h"

namespace forgeshape {
namespace {

// STL and 3MF are written in millimetres (see the header); the snapshot is in
// metres. Applied in double and narrowed once, so a value is scaled exactly
// one way in every file that carries it.
constexpr double kMillimetresPerMetre = 1000.0;

// ---------------------------------------------------------------------------
// Numbers as text, independent of the C locale
// ---------------------------------------------------------------------------
//
// `%.9g` is the shortest precision that round-trips every float32, which is
// why the GLB writer uses it. But `printf` spells the decimal point the way the
// current C locale does, and a shell that calls `setlocale(LC_ALL, "")` on a
// Polish or German desktop would then write "1,5" into a file whose grammar
// says "1.5". Every byte `%g` can produce for a finite value is a digit, a
// sign, an exponent letter or the locale's decimal separator, so the separator
// -- however many bytes it is -- is rewritten as '.' after the fact. Negative
// zero is written as zero, so one position has one spelling.
void appendNumber(std::string& out, float value) {
    if (value == 0.0f) {
        value = 0.0f;
    }
    char text[48];
    std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(value));
    bool inSeparator = false;
    for (const char* c = text; *c != '\0'; ++c) {
        const char ch = *c;
        const bool numeric = (ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == 'e'
                             || ch == 'E';
        if (numeric) {
            out += ch;
            inSeparator = false;
        } else if (!inSeparator) {
            out += '.';
            inSeparator = true;
        }
    }
}

void appendUnsigned(std::string& out, uint64_t value) {
    char text[24];
    int length = 0;
    do {
        text[length++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);
    while (length > 0) {
        out += text[--length];
    }
}

void appendU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
}

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
    }
}

void appendF32(std::vector<uint8_t>& out, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    appendU32(out, bits);
}

std::string technicalName(ObjectId objectId) {
    std::string name = "Body_";
    appendUnsigned(name, static_cast<uint64_t>(objectId));
    return name;
}

// ---------------------------------------------------------------------------
// The printable triangle set
// ---------------------------------------------------------------------------

// A float's exact bits, with negative zero folded into zero. Exact on purpose:
// a tolerance would merge vertices the user placed apart, and the render
// mesh's hard-edge split produces bit-identical copies.
uint32_t exactBits(float value) {
    if (value == 0.0f) {
        value = 0.0f;
    }
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// For every xyz triple, the smallest index whose triple is bit-identical to
// its own. Found by SORTING rather than hashing, so it costs two words per
// vertex and no node allocation -- a multi-million-vertex sculpt mesh exports
// on a phone -- and its answer depends on no hash function.
bool sameTriple(const std::vector<float>& xyz, uint32_t a, uint32_t b) {
    for (int c = 0; c < 3; ++c) {
        if (exactBits(xyz[static_cast<size_t>(a) * 3u + c])
            != exactBits(xyz[static_cast<size_t>(b) * 3u + c])) {
            return false;
        }
    }
    return true;
}

std::vector<uint32_t> firstIdenticalTriple(const std::vector<float>& xyz) {
    const uint32_t count = static_cast<uint32_t>(xyz.size() / 3u);
    std::vector<uint32_t> order(count);
    for (uint32_t i = 0; i < count; ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&xyz](uint32_t a, uint32_t b) {
        for (int c = 0; c < 3; ++c) {
            const uint32_t ka = exactBits(xyz[static_cast<size_t>(a) * 3u + c]);
            const uint32_t kb = exactBits(xyz[static_cast<size_t>(b) * 3u + c]);
            if (ka != kb) {
                return ka < kb;
            }
        }
        return a < b;
    });
    // Sorted by (bits, index): each run of identical triples starts with its
    // smallest index, which every member of the run maps to.
    std::vector<uint32_t> first(count);
    for (uint32_t i = 0; i < count;) {
        const uint32_t leader = order[i];
        uint32_t j = i;
        while (j < count && sameTriple(xyz, leader, order[j])) {
            first[order[j]] = leader;
            ++j;
        }
        i = j;
    }
    return first;
}

// One body reduced to what the non-GLB formats write: the distinct positions
// its kept triangles use (first-use order, world metres) and, per kept corner,
// which welded position and which snapshot vertex it came from.
struct PrintableBody {
    std::vector<float> positions;
    std::vector<uint32_t> cornerPosition;
    std::vector<uint32_t> cornerVertex;

    size_t triangleCount() const { return cornerPosition.size() / 3u; }
};

bool allFinite(const std::vector<float>& values) {
    for (float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

// One triangle's three welded vertices as an UNORDERED set, plus where it
// stands in the body, so sorting groups every statement of one facet.
struct FacetKey {
    uint32_t ids[3] = {0, 0, 0};
    uint32_t triangle = 0;
};

bool facetKeyLess(const FacetKey& a, const FacetKey& b) {
    for (int k = 0; k < 3; ++k) {
        if (a.ids[k] != b.ids[k]) {
            return a.ids[k] < b.ids[k];
        }
    }
    return a.triangle < b.triangle;
}

bool sameFacet(const FacetKey& a, const FacetKey& b) {
    return a.ids[0] == b.ids[0] && a.ids[1] == b.ids[1] && a.ids[2] == b.ids[2];
}

// Validates one object of a (possibly hand-built) snapshot and reduces it to
// the printable triangle set. Ok, or the refusal the encoder returns.
//
// The rule, stated once: a body writes the FIRST triangle on each distinct set
// of three distinct welded positions, in its own order and winding. Three
// corners on two positions have no area; a later triangle on the same three
// positions is either the same facet stated again or its reversed twin -- a
// triangle on three points has only two windings -- and neither is a second
// facet any of these formats can carry.
GlbExportStatus buildPrintable(const MeshExportObject& object, PrintableBody* out) {
    if (object.positions.empty() || (object.positions.size() % 3u) != 0u
        || object.normals.size() != object.positions.size() || object.indices.empty()
        || (object.indices.size() % 3u) != 0u) {
        return GlbExportStatus::InvalidMesh;
    }
    if (!allFinite(object.positions) || !allFinite(object.normals)) {
        return GlbExportStatus::NonFiniteValue;
    }
    const size_t vertexCount = object.positions.size() / 3u;
    for (uint32_t index : object.indices) {
        if (index >= vertexCount) {
            return GlbExportStatus::InvalidMesh;
        }
    }

    // Welded identity: the smallest vertex index at the same exact position.
    const std::vector<uint32_t> welded = firstIdenticalTriple(object.positions);
    const uint32_t triangleCount = static_cast<uint32_t>(object.indices.size() / 3u);
    std::vector<FacetKey> facets;
    facets.reserve(triangleCount);
    for (uint32_t t = 0; t < triangleCount; ++t) {
        uint32_t a = welded[object.indices[t * 3u]];
        uint32_t b = welded[object.indices[t * 3u + 1]];
        uint32_t c = welded[object.indices[t * 3u + 2]];
        if (a == b || b == c || a == c) {
            continue;  // no area: not a facet any of these formats can carry
        }
        if (a > b) std::swap(a, b);
        if (b > c) std::swap(b, c);
        if (a > b) std::swap(a, b);
        FacetKey key;
        key.ids[0] = a;
        key.ids[1] = b;
        key.ids[2] = c;
        key.triangle = t;
        facets.push_back(key);
    }
    std::sort(facets.begin(), facets.end(), facetKeyLess);
    std::vector<uint8_t> keep(triangleCount, 0);
    for (size_t i = 0; i < facets.size(); ++i) {
        if (i == 0 || !sameFacet(facets[i - 1], facets[i])) {
            keep[facets[i].triangle] = 1;
        }
    }
    facets.clear();
    facets.shrink_to_fit();

    PrintableBody body;
    std::vector<uint32_t> positionId(vertexCount, std::numeric_limits<uint32_t>::max());
    for (uint32_t t = 0; t < triangleCount; ++t) {
        if (keep[t] == 0) {
            continue;
        }
        for (uint32_t k = 0; k < 3; ++k) {
            const uint32_t vertex = object.indices[t * 3u + k];
            const uint32_t representative = welded[vertex];
            if (positionId[representative] == std::numeric_limits<uint32_t>::max()) {
                positionId[representative] = static_cast<uint32_t>(body.positions.size() / 3u);
                for (int c = 0; c < 3; ++c) {
                    const float value = object.positions[static_cast<size_t>(representative) * 3u + c];
                    body.positions.push_back(value == 0.0f ? 0.0f : value);
                }
            }
            body.cornerPosition.push_back(positionId[representative]);
            body.cornerVertex.push_back(vertex);
        }
    }
    if (body.cornerPosition.empty()) {
        return GlbExportStatus::InvalidMesh;
    }
    *out = std::move(body);
    return GlbExportStatus::Ok;
}

GlbExportStatus buildAllPrintable(const MeshExportSnapshot& snapshot,
                                  std::vector<PrintableBody>* out) {
    if (snapshot.objects.empty()) {
        return GlbExportStatus::NothingToExport;
    }
    std::vector<PrintableBody> bodies(snapshot.objects.size());
    for (size_t i = 0; i < snapshot.objects.size(); ++i) {
        const GlbExportStatus status = buildPrintable(snapshot.objects[i], &bodies[i]);
        if (status != GlbExportStatus::Ok) {
            return status;
        }
    }
    *out = std::move(bodies);
    return GlbExportStatus::Ok;
}

std::vector<uint8_t> refuse(GlbExportStatus* outWhy, GlbExportStatus status) {
    if (outWhy != nullptr) {
        *outWhy = status;
    }
    return std::vector<uint8_t>();
}

void accept(GlbExportStatus* outWhy) {
    if (outWhy != nullptr) {
        *outWhy = GlbExportStatus::Ok;
    }
}

// A world metre value as millimetres, or false when it does not fit a float.
bool toMillimetres(float metres, float* out) {
    const float mm = static_cast<float>(static_cast<double>(metres) * kMillimetresPerMetre);
    if (!std::isfinite(mm)) {
        return false;
    }
    *out = mm == 0.0f ? 0.0f : mm;
    return true;
}

// ---------------------------------------------------------------------------
// Names, narrowed per format
// ---------------------------------------------------------------------------

// For OBJ: printable ASCII on one line. A multi-byte UTF-8 character becomes
// ONE '_' (its lead byte does, its continuation bytes are dropped), and the
// characters an OBJ reader may treat as syntax inside a line -- '#' (a
// trailing comment to some readers) and '\' (a line continuation) -- are
// replaced too. Empty after all that: the technical name.
std::string asciiName(const std::string& raw, ObjectId objectId) {
    std::string name;
    for (unsigned char c : raw) {
        if (c >= 0x80u && c < 0xC0u) {
            continue;
        }
        if (c >= 0xC0u || c == '#' || c == '\\') {
            name += '_';
        } else if (c >= 0x20u && c < 0x7Fu) {
            name += static_cast<char>(c);
        }
    }
    const size_t first = name.find_first_not_of(' ');
    if (first == std::string::npos) {
        return technicalName(objectId);
    }
    const size_t last = name.find_last_not_of(' ');
    return name.substr(first, last - first + 1);
}

// For 3MF: XML is UTF-8, so the domain's own name travels whole -- re-run
// through the domain's sanitizer (idempotent on a stored name) so nothing
// malformed or control can reach the document -- and is escaped for an
// attribute value.
void appendXmlAttributeText(std::string& out, const std::string& raw, ObjectId objectId) {
    std::string name = sanitizeImportedMeshName(raw);
    if (name.empty()) {
        name = technicalName(objectId);
    }
    for (size_t i = 0; i < name.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        // U+FFFE and U+FFFF are not XML characters even though they are valid
        // UTF-8 (EF BF BE / EF BF BF).
        if (c == 0xEFu && i + 2 < name.size() && static_cast<unsigned char>(name[i + 1]) == 0xBFu
            && (static_cast<unsigned char>(name[i + 2]) == 0xBEu
                || static_cast<unsigned char>(name[i + 2]) == 0xBFu)) {
            out += '_';
            i += 2;
            continue;
        }
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:
                if (c >= 0x20u) {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// A STORED ZIP, for the 3MF package
// ---------------------------------------------------------------------------
//
// Written here rather than borrowed: the product has no ZIP library, and a
// STORED archive is three fixed records per entry. Deterministic by
// construction -- every entry carries the same fixed DOS time stamp (1980-01-01
// 00:00:00, the format's epoch), no extra fields, no comment, and the entries
// are in the order given.
constexpr uint16_t kZipVersion = 20;       // 2.0: the baseline every reader takes
constexpr uint16_t kZipDosTime = 0x0000;   // 00:00:00
constexpr uint16_t kZipDosDate = 0x0021;   // 1980-01-01: (0 << 9) | (1 << 5) | 1

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

struct ZipEntry {
    std::string name;
    std::string data;
};

bool writeStoredZip(const std::vector<ZipEntry>& entries, std::vector<uint8_t>* out) {
    uint64_t total = 22;
    for (const ZipEntry& entry : entries) {
        total += 30u + 46u + 2ull * entry.name.size() + entry.data.size();
    }
    if (total > kMaxGlbBytes || entries.size() > 0xFFFFu) {
        return false;
    }
    std::vector<uint8_t> zip;
    zip.reserve(static_cast<size_t>(total));
    std::vector<uint32_t> offsets;
    std::vector<uint32_t> crcs;
    for (const ZipEntry& entry : entries) {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(entry.data.data());
        const uint32_t crc = crc32Of(data, entry.data.size());
        offsets.push_back(static_cast<uint32_t>(zip.size()));
        crcs.push_back(crc);
        appendU32(zip, 0x04034B50u);
        appendU16(zip, kZipVersion);
        appendU16(zip, 0);  // flags: no data descriptor, no encryption
        appendU16(zip, 0);  // method: STORED
        appendU16(zip, kZipDosTime);
        appendU16(zip, kZipDosDate);
        appendU32(zip, crc);
        appendU32(zip, static_cast<uint32_t>(entry.data.size()));
        appendU32(zip, static_cast<uint32_t>(entry.data.size()));
        appendU16(zip, static_cast<uint16_t>(entry.name.size()));
        appendU16(zip, 0);  // no extra field
        zip.insert(zip.end(), entry.name.begin(), entry.name.end());
        zip.insert(zip.end(), data, data + entry.data.size());
    }
    const uint32_t directoryOffset = static_cast<uint32_t>(zip.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        const ZipEntry& entry = entries[i];
        appendU32(zip, 0x02014B50u);
        appendU16(zip, kZipVersion);  // made by: 2.0, MS-DOS attributes
        appendU16(zip, kZipVersion);  // needed to extract
        appendU16(zip, 0);
        appendU16(zip, 0);
        appendU16(zip, kZipDosTime);
        appendU16(zip, kZipDosDate);
        appendU32(zip, crcs[i]);
        appendU32(zip, static_cast<uint32_t>(entry.data.size()));
        appendU32(zip, static_cast<uint32_t>(entry.data.size()));
        appendU16(zip, static_cast<uint16_t>(entry.name.size()));
        appendU16(zip, 0);  // extra
        appendU16(zip, 0);  // comment
        appendU16(zip, 0);  // disk number start
        appendU16(zip, 0);  // internal attributes
        appendU32(zip, 0);  // external attributes
        appendU32(zip, offsets[i]);
        zip.insert(zip.end(), entry.name.begin(), entry.name.end());
    }
    const uint32_t directorySize = static_cast<uint32_t>(zip.size()) - directoryOffset;
    appendU32(zip, 0x06054B50u);
    appendU16(zip, 0);  // this disk
    appendU16(zip, 0);  // disk with the directory
    appendU16(zip, static_cast<uint16_t>(entries.size()));
    appendU16(zip, static_cast<uint16_t>(entries.size()));
    appendU32(zip, directorySize);
    appendU32(zip, directoryOffset);
    appendU16(zip, 0);  // no comment
    *out = std::move(zip);
    return true;
}

}  // namespace

const char* meshExportFormatName(MeshExportFormat format) {
    switch (format) {
        case MeshExportFormat::Glb: return "glb";
        case MeshExportFormat::Stl: return "stl";
        case MeshExportFormat::Obj: return "obj";
        case MeshExportFormat::Ply: return "ply";
        case MeshExportFormat::ThreeMf: return "3mf";
    }
    return "unknown";
}

const char* meshExportFileExtension(MeshExportFormat format) {
    switch (format) {
        case MeshExportFormat::Glb: return ".glb";
        case MeshExportFormat::Stl: return ".stl";
        case MeshExportFormat::Obj: return ".obj";
        case MeshExportFormat::Ply: return ".ply";
        case MeshExportFormat::ThreeMf: return ".3mf";
    }
    return "";
}

const char* meshExportMimeType(MeshExportFormat format) {
    switch (format) {
        case MeshExportFormat::Glb: return "model/gltf-binary";
        case MeshExportFormat::Stl: return "model/stl";
        case MeshExportFormat::Obj: return "model/obj";
        // PLY has no registered media type; this is the one its readers use.
        case MeshExportFormat::Ply: return "application/x-ply";
        case MeshExportFormat::ThreeMf: return "model/3mf";
    }
    return "application/octet-stream";
}

GlbExportStatus captureMeshExportSnapshot(const ConstructionScene& scene, ProjectKind kind,
                                          MeshExportSnapshot* out) {
    if (out == nullptr) {
        return GlbExportStatus::NothingToExport;
    }
    GlbExportScene captured;
    const GlbExportStatus status = captureGlbExportScene(scene, kind, &captured);
    if (status != GlbExportStatus::Ok) {
        return status;
    }
    MeshExportSnapshot snapshot;
    snapshot.objects.reserve(captured.bodies.size());
    for (const GlbExportBody& body : captured.bodies) {
        MeshExportObject object;
        object.objectId = body.objectId;
        const SceneObject* source = scene.findBody(body.objectId);
        object.name = (source != nullptr && !source->name().empty())
                              ? source->name()
                              : technicalName(body.objectId);
        // World = baked local + translation, in float, which is exactly what a
        // glTF consumer computes from the GLB of the same project.
        object.positions.reserve(body.render.vertices.size() * 3u);
        object.normals.reserve(body.render.vertices.size() * 3u);
        for (const RenderVertex& vertex : body.render.vertices) {
            for (int c = 0; c < 3; ++c) {
                const float world = vertex.position[c] + body.translation[c];
                if (!std::isfinite(world)) {
                    return GlbExportStatus::NonFiniteValue;
                }
                object.positions.push_back(world);
                object.normals.push_back(vertex.normal[c]);
            }
        }
        object.indices = body.render.indices;
        snapshot.objects.push_back(std::move(object));
    }
    *out = std::move(snapshot);
    return GlbExportStatus::Ok;
}

std::vector<uint8_t> encodeMeshExportStl(const MeshExportSnapshot& snapshot,
                                         GlbExportStatus* outWhy) {
    std::vector<PrintableBody> bodies;
    const GlbExportStatus status = buildAllPrintable(snapshot, &bodies);
    if (status != GlbExportStatus::Ok) {
        return refuse(outWhy, status);
    }
    uint64_t triangles = 0;
    for (const PrintableBody& body : bodies) {
        triangles += body.triangleCount();
    }
    const uint64_t total = 84ull + 50ull * triangles;
    if (triangles > 0xFFFFFFFFull || total > kMaxGlbBytes) {
        return refuse(outWhy, GlbExportStatus::TooLarge);
    }

    std::vector<uint8_t> stl;
    stl.reserve(static_cast<size_t>(total));
    // The 80-byte header names the product and is zero-padded. It must NOT
    // begin with "solid": that is how a reader tells an ASCII STL apart.
    static const char kHeader[] = "ForgeShape";
    stl.insert(stl.end(), kHeader, kHeader + sizeof(kHeader) - 1);
    stl.resize(80, 0);
    appendU32(stl, static_cast<uint32_t>(triangles));

    for (const PrintableBody& body : bodies) {
        for (size_t t = 0; t < body.triangleCount(); ++t) {
            float corner[3][3];
            for (int k = 0; k < 3; ++k) {
                const float* metres = &body.positions[static_cast<size_t>(
                                                              body.cornerPosition[t * 3 + k])
                                                      * 3u];
                for (int c = 0; c < 3; ++c) {
                    if (!toMillimetres(metres[c], &corner[k][c])) {
                        return refuse(outWhy, GlbExportStatus::NonFiniteValue);
                    }
                }
            }
            // The facet normal, from the corners as written. A reader that
            // trusts it and one that recomputes it then agree; a sliver whose
            // cross product has no length writes the zero vector the format
            // reserves for "compute it yourself".
            const double e1[3] = {double(corner[1][0]) - corner[0][0],
                                  double(corner[1][1]) - corner[0][1],
                                  double(corner[1][2]) - corner[0][2]};
            const double e2[3] = {double(corner[2][0]) - corner[0][0],
                                  double(corner[2][1]) - corner[0][1],
                                  double(corner[2][2]) - corner[0][2]};
            double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                           e1[0] * e2[1] - e1[1] * e2[0]};
            const double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            for (int c = 0; c < 3; ++c) {
                const double unit = (length > 0.0 && std::isfinite(length)) ? n[c] / length : 0.0;
                appendF32(stl, static_cast<float>(unit));
            }
            for (int k = 0; k < 3; ++k) {
                for (int c = 0; c < 3; ++c) {
                    appendF32(stl, corner[k][c]);
                }
            }
            appendU16(stl, 0);  // attribute byte count: unused
        }
    }
    accept(outWhy);
    return stl;
}

std::vector<uint8_t> encodeMeshExportObj(const MeshExportSnapshot& snapshot,
                                         GlbExportStatus* outWhy) {
    std::vector<PrintableBody> bodies;
    const GlbExportStatus status = buildAllPrintable(snapshot, &bodies);
    if (status != GlbExportStatus::Ok) {
        return refuse(outWhy, status);
    }
    std::string text;
    text += "# ForgeShape\n";
    text += "# units: metres; right-handed; +Y up\n";
    uint64_t positionBase = 0;
    uint64_t normalBase = 0;
    for (size_t b = 0; b < bodies.size(); ++b) {
        const PrintableBody& body = bodies[b];
        const MeshExportObject& object = snapshot.objects[b];
        text += "o ";
        text += asciiName(object.name, object.objectId);
        text += '\n';
        for (size_t p = 0; p + 2 < body.positions.size(); p += 3) {
            text += "v ";
            appendNumber(text, body.positions[p]);
            text += ' ';
            appendNumber(text, body.positions[p + 1]);
            text += ' ';
            appendNumber(text, body.positions[p + 2]);
            text += '\n';
        }
        // Normals are per CORNER (the render mesh's crease split), deduplicated
        // by exact value in first-use order, so a box writes its six face
        // normals once and its eight corners once.
        const std::vector<uint32_t> sameNormal = firstIdenticalTriple(object.normals);
        std::vector<uint32_t> normalId(sameNormal.size(), std::numeric_limits<uint32_t>::max());
        std::vector<uint32_t> cornerNormal(body.cornerVertex.size());
        uint32_t normalCount = 0;
        for (size_t k = 0; k < body.cornerVertex.size(); ++k) {
            const uint32_t representative = sameNormal[body.cornerVertex[k]];
            if (normalId[representative] == std::numeric_limits<uint32_t>::max()) {
                normalId[representative] = normalCount++;
                const float* n = &object.normals[static_cast<size_t>(representative) * 3u];
                text += "vn ";
                appendNumber(text, n[0]);
                text += ' ';
                appendNumber(text, n[1]);
                text += ' ';
                appendNumber(text, n[2]);
                text += '\n';
            }
            cornerNormal[k] = normalId[representative];
            if (text.size() > kMaxGlbBytes) {
                return refuse(outWhy, GlbExportStatus::TooLarge);
            }
        }
        for (size_t t = 0; t < body.triangleCount(); ++t) {
            text += 'f';
            for (int k = 0; k < 3; ++k) {
                text += ' ';
                appendUnsigned(text, positionBase + body.cornerPosition[t * 3 + k] + 1u);
                text += "//";
                appendUnsigned(text, normalBase + cornerNormal[t * 3 + k] + 1u);
            }
            text += '\n';
            if (text.size() > kMaxGlbBytes) {
                return refuse(outWhy, GlbExportStatus::TooLarge);
            }
        }
        positionBase += body.positions.size() / 3u;
        normalBase += normalCount;
    }
    if (text.size() > kMaxGlbBytes) {
        return refuse(outWhy, GlbExportStatus::TooLarge);
    }
    accept(outWhy);
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::vector<uint8_t> encodeMeshExportPly(const MeshExportSnapshot& snapshot,
                                         GlbExportStatus* outWhy) {
    std::vector<PrintableBody> bodies;
    const GlbExportStatus status = buildAllPrintable(snapshot, &bodies);
    if (status != GlbExportStatus::Ok) {
        return refuse(outWhy, status);
    }
    // PLY's vertex carries ONE normal, so vertices stay the render mesh's own
    // (split along hard edges), compacted to the ones a kept triangle uses, in
    // first-use order, and every body is merged into one vertex list.
    std::vector<std::vector<uint32_t>> remap(bodies.size());
    std::vector<std::vector<uint32_t>> used(bodies.size());
    uint64_t vertexTotal = 0;
    uint64_t faceTotal = 0;
    for (size_t b = 0; b < bodies.size(); ++b) {
        const size_t vertexCount = snapshot.objects[b].positions.size() / 3u;
        remap[b].assign(vertexCount, std::numeric_limits<uint32_t>::max());
        for (uint32_t vertex : bodies[b].cornerVertex) {
            if (remap[b][vertex] == std::numeric_limits<uint32_t>::max()) {
                remap[b][vertex] = static_cast<uint32_t>(vertexTotal + used[b].size());
                used[b].push_back(vertex);
            }
        }
        vertexTotal += used[b].size();
        faceTotal += bodies[b].triangleCount();
    }
    std::string header;
    header += "ply\nformat binary_little_endian 1.0\ncomment ForgeShape\n";
    header += "comment units: metres; right-handed; +Y up\n";
    header += "element vertex ";
    appendUnsigned(header, vertexTotal);
    header += "\nproperty float x\nproperty float y\nproperty float z\n"
              "property float nx\nproperty float ny\nproperty float nz\n";
    header += "element face ";
    appendUnsigned(header, faceTotal);
    header += "\nproperty list uchar int vertex_indices\nend_header\n";
    const uint64_t total = header.size() + vertexTotal * 24ull + faceTotal * 13ull;
    if (vertexTotal > 0x7FFFFFFFull || total > kMaxGlbBytes) {
        return refuse(outWhy, GlbExportStatus::TooLarge);
    }
    std::vector<uint8_t> ply;
    ply.reserve(static_cast<size_t>(total));
    ply.insert(ply.end(), header.begin(), header.end());
    for (size_t b = 0; b < bodies.size(); ++b) {
        const MeshExportObject& object = snapshot.objects[b];
        for (uint32_t vertex : used[b]) {
            const size_t at = static_cast<size_t>(vertex) * 3u;
            for (int c = 0; c < 3; ++c) {
                const float value = object.positions[at + c];
                appendF32(ply, value == 0.0f ? 0.0f : value);
            }
            for (int c = 0; c < 3; ++c) {
                appendF32(ply, object.normals[at + c]);
            }
        }
    }
    for (size_t b = 0; b < bodies.size(); ++b) {
        const PrintableBody& body = bodies[b];
        for (size_t t = 0; t < body.triangleCount(); ++t) {
            ply.push_back(3);
            for (int k = 0; k < 3; ++k) {
                appendU32(ply, remap[b][body.cornerVertex[t * 3 + k]]);
            }
        }
    }
    accept(outWhy);
    return ply;
}

std::vector<uint8_t> encodeMeshExportThreeMf(const MeshExportSnapshot& snapshot,
                                             GlbExportStatus* outWhy) {
    std::vector<PrintableBody> bodies;
    const GlbExportStatus status = buildAllPrintable(snapshot, &bodies);
    if (status != GlbExportStatus::Ok) {
        return refuse(outWhy, status);
    }

    // The model part (3MF Core 1.2). Vertices are the WELDED positions --
    // slicers report a mesh whose hard edges are split as non-manifold -- in
    // millimetres, world space, one object and one build item per body in
    // scene order. No item transform: the placement is already in the
    // vertices, exactly as it is in the other formats.
    std::string model;
    model += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    model += "<model unit=\"millimeter\" xml:lang=\"en-US\" "
             "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n";
    model += " <metadata name=\"Application\">ForgeShape</metadata>\n";
    model += " <resources>\n";
    for (size_t b = 0; b < bodies.size(); ++b) {
        const PrintableBody& body = bodies[b];
        const MeshExportObject& object = snapshot.objects[b];
        model += "  <object id=\"";
        appendUnsigned(model, b + 1u);
        model += "\" type=\"model\" name=\"";
        appendXmlAttributeText(model, object.name, object.objectId);
        model += "\">\n   <mesh>\n    <vertices>\n";
        for (size_t p = 0; p + 2 < body.positions.size(); p += 3) {
            float mm[3];
            for (int c = 0; c < 3; ++c) {
                if (!toMillimetres(body.positions[p + c], &mm[c])) {
                    return refuse(outWhy, GlbExportStatus::NonFiniteValue);
                }
            }
            model += "     <vertex x=\"";
            appendNumber(model, mm[0]);
            model += "\" y=\"";
            appendNumber(model, mm[1]);
            model += "\" z=\"";
            appendNumber(model, mm[2]);
            model += "\"/>\n";
            if (model.size() > kMaxGlbBytes) {
                return refuse(outWhy, GlbExportStatus::TooLarge);
            }
        }
        model += "    </vertices>\n    <triangles>\n";
        for (size_t t = 0; t < body.triangleCount(); ++t) {
            model += "     <triangle v1=\"";
            appendUnsigned(model, body.cornerPosition[t * 3]);
            model += "\" v2=\"";
            appendUnsigned(model, body.cornerPosition[t * 3 + 1]);
            model += "\" v3=\"";
            appendUnsigned(model, body.cornerPosition[t * 3 + 2]);
            model += "\"/>\n";
            if (model.size() > kMaxGlbBytes) {
                return refuse(outWhy, GlbExportStatus::TooLarge);
            }
        }
        model += "    </triangles>\n   </mesh>\n  </object>\n";
    }
    model += " </resources>\n <build>\n";
    for (size_t b = 0; b < bodies.size(); ++b) {
        model += "  <item objectid=\"";
        appendUnsigned(model, b + 1u);
        model += "\"/>\n";
    }
    model += " </build>\n</model>\n";

    std::vector<ZipEntry> entries(3);
    entries[0].name = "[Content_Types].xml";
    entries[0].data =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            "<Default Extension=\"rels\" "
            "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
            "<Default Extension=\"model\" "
            "ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>"
            "</Types>\n";
    entries[1].name = "_rels/.rels";
    entries[1].data =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
            "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>"
            "</Relationships>\n";
    entries[2].name = "3D/3dmodel.model";
    entries[2].data = std::move(model);

    std::vector<uint8_t> zip;
    if (!writeStoredZip(entries, &zip)) {
        return refuse(outWhy, GlbExportStatus::TooLarge);
    }
    accept(outWhy);
    return zip;
}

std::vector<uint8_t> exportSceneAs(MeshExportFormat format, const ConstructionScene& scene,
                                   ProjectKind kind, GlbExportStatus* outWhy) {
    if (format == MeshExportFormat::Glb) {
        return exportSceneAsGlb(scene, kind, outWhy);
    }
    if (format != MeshExportFormat::Stl && format != MeshExportFormat::Obj
        && format != MeshExportFormat::Ply && format != MeshExportFormat::ThreeMf) {
        return refuse(outWhy, GlbExportStatus::NothingToExport);
    }
    MeshExportSnapshot snapshot;
    const GlbExportStatus captured = captureMeshExportSnapshot(scene, kind, &snapshot);
    if (captured != GlbExportStatus::Ok) {
        return refuse(outWhy, captured);
    }
    switch (format) {
        case MeshExportFormat::Stl: return encodeMeshExportStl(snapshot, outWhy);
        case MeshExportFormat::Obj: return encodeMeshExportObj(snapshot, outWhy);
        case MeshExportFormat::Ply: return encodeMeshExportPly(snapshot, outWhy);
        case MeshExportFormat::ThreeMf: return encodeMeshExportThreeMf(snapshot, outWhy);
        case MeshExportFormat::Glb: break;
    }
    return refuse(outWhy, GlbExportStatus::NothingToExport);
}

}  // namespace forgeshape
