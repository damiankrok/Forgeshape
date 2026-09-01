#include "forgeshape_imported_mesh.h"

#include <cmath>

namespace forgeshape {
namespace {

// How far from unit length a supplied normal may be and still be accepted.
//
// Generous enough for float32 accumulated through an inverse transpose and a
// normalize, tight enough that an un-normalized direction is caught rather than
// stored as truth.
constexpr float kNormalLengthTolerance = 1e-3f;

bool isFinite3(const float* v) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

}  // namespace

const char* importedMeshValidationName(ImportedMeshValidation why) {
    switch (why) {
        case ImportedMeshValidation::Ok: return "Ok";
        case ImportedMeshValidation::EmptyVertices: return "EmptyVertices";
        case ImportedMeshValidation::EmptyIndices: return "EmptyIndices";
        case ImportedMeshValidation::TooLarge: return "TooLarge";
        case ImportedMeshValidation::IndexCountNotTriangles: return "IndexCountNotTriangles";
        case ImportedMeshValidation::IndexOutOfRange: return "IndexOutOfRange";
        case ImportedMeshValidation::NonFinitePosition: return "NonFinitePosition";
        case ImportedMeshValidation::NonFiniteNormal: return "NonFiniteNormal";
        case ImportedMeshValidation::CountMismatch: return "CountMismatch";
        case ImportedMeshValidation::NoBatches: return "NoBatches";
        case ImportedMeshValidation::BatchesDoNotTile: return "BatchesDoNotTile";
        case ImportedMeshValidation::NotDrawable: return "NotDrawable";
    }
    return "unknown";
}

ImportedMeshValidation validateImportedMeshData(const std::vector<float>& positions,
                                                const std::vector<float>& normals,
                                                const std::vector<uint32_t>& indices,
                                                const std::vector<ImportedMeshBatch>& batches) {
    if (positions.empty() || (positions.size() % 3u) != 0u) {
        return ImportedMeshValidation::EmptyVertices;
    }
    if (indices.empty()) {
        return ImportedMeshValidation::EmptyIndices;
    }
    if ((indices.size() % 3u) != 0u) {
        return ImportedMeshValidation::IndexCountNotTriangles;
    }

    const uint64_t vertexCount = positions.size() / 3u;
    // Checked before anything is scanned, so an impossible count in a corrupt
    // file cannot make this loop run for a very long time.
    if (vertexCount > kMaxImportedMeshVertices || indices.size() > kMaxImportedMeshIndices
        || batches.size() > kMaxImportedMeshBatches) {
        return ImportedMeshValidation::TooLarge;
    }
    if (normals.size() != positions.size()) {
        // One normal per position, or the representation cannot be shaded or
        // re-exported coherently. The importer generates them when a file
        // states none, so "none at all" never reaches here legitimately.
        return ImportedMeshValidation::CountMismatch;
    }
    if (batches.empty()) {
        return ImportedMeshValidation::NoBatches;
    }

    for (size_t i = 0; i < positions.size(); i += 3) {
        if (!isFinite3(&positions[i])) {
            return ImportedMeshValidation::NonFinitePosition;
        }
    }
    for (size_t i = 0; i < normals.size(); i += 3) {
        if (!isFinite3(&normals[i])) {
            return ImportedMeshValidation::NonFiniteNormal;
        }
        const float length = std::sqrt(normals[i] * normals[i] + normals[i + 1] * normals[i + 1]
                                       + normals[i + 2] * normals[i + 2]);
        if (std::fabs(length - 1.0f) > kNormalLengthTolerance) {
            return ImportedMeshValidation::NonFiniteNormal;
        }
    }
    for (uint32_t index : indices) {
        if (index >= vertexCount) {
            return ImportedMeshValidation::IndexOutOfRange;
        }
    }

    // The batches must tile the index array exactly, in order. A gap would be
    // triangles nothing draws and an overlap would be triangles drawn twice;
    // either means the record does not describe the geometry beside it.
    uint64_t covered = 0;
    for (const ImportedMeshBatch& batch : batches) {
        if (batch.firstIndex != covered || batch.indexCount == 0
            || (batch.indexCount % 3u) != 0u) {
            return ImportedMeshValidation::BatchesDoNotTile;
        }
        covered += batch.indexCount;
        if (covered > indices.size()) {
            return ImportedMeshValidation::BatchesDoNotTile;
        }
    }
    if (covered != indices.size()) {
        return ImportedMeshValidation::BatchesDoNotTile;
    }
    return ImportedMeshValidation::Ok;
}

ImportedMesh ImportedMesh::build(std::vector<float> positions, std::vector<float> normals,
                                 std::vector<uint32_t> indices,
                                 std::vector<ImportedMeshBatch> batches,
                                 ImportedMeshValidation* outWhy) {
    const ImportedMeshValidation why =
            validateImportedMeshData(positions, normals, indices, batches);
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != ImportedMeshValidation::Ok) {
        return ImportedMesh{};
    }

    ImportedMesh mesh;
    mesh.positions_ = std::move(positions);
    mesh.normals_ = std::move(normals);
    mesh.indices_ = std::move(indices);
    mesh.batches_ = std::move(batches);
    return mesh;
}

bool ImportedMesh::localBounds(float* out) const {
    if (out == nullptr || positions_.empty()) {
        return false;
    }
    for (int c = 0; c < 3; ++c) {
        out[c] = positions_[c];
        out[3 + c] = positions_[c];
    }
    for (size_t i = 3; i < positions_.size(); i += 3) {
        for (int c = 0; c < 3; ++c) {
            const float value = positions_[i + c];
            if (value < out[c]) out[c] = value;
            if (value > out[3 + c]) out[3 + c] = value;
        }
    }
    return true;
}

bool ImportedMesh::buildDrawData(std::vector<MeshVertex>* outVertices,
                                 std::vector<uint32_t>* outIndices) const {
    if (outVertices == nullptr || outIndices == nullptr || !valid()) {
        return false;
    }

    std::vector<MeshVertex> vertices(vertexCount());
    for (uint32_t v = 0; v < vertexCount(); ++v) {
        const size_t at = static_cast<size_t>(v) * 3u;
        vertices[v].position[0] = positions_[at];
        vertices[v].position[1] = positions_[at + 1];
        vertices[v].position[2] = positions_[at + 2];
        vertices[v].color[0] = kImportedMeshVertexColor[0];
        vertices[v].color[1] = kImportedMeshVertexColor[1];
        vertices[v].color[2] = kImportedMeshVertexColor[2];
    }

    // Double-sided submeshes contribute their triangles twice, the second time
    // reversed. Doing it HERE rather than through the published mesh's
    // `renderBothSides` is what keeps the answer per submesh: that flag is one
    // answer for a whole mesh, and an imported object may legitimately need a
    // different one for each of its parts.
    uint64_t total = indices_.size();
    for (const ImportedMeshBatch& batch : batches_) {
        if (batch.doubleSided) {
            total += batch.indexCount;
        }
    }
    if (total > kMaxImportedMeshIndices) {
        return false;
    }

    std::vector<uint32_t> drawIndices;
    drawIndices.reserve(static_cast<size_t>(total));
    for (const ImportedMeshBatch& batch : batches_) {
        const size_t first = batch.firstIndex;
        const size_t end = first + batch.indexCount;
        for (size_t i = first; i < end; ++i) {
            drawIndices.push_back(indices_[i]);
        }
        if (!batch.doubleSided) {
            continue;
        }
        for (size_t t = first; t + 2 < end; t += 3) {
            drawIndices.push_back(indices_[t]);
            drawIndices.push_back(indices_[t + 2]);
            drawIndices.push_back(indices_[t + 1]);
        }
    }

    if (validateMeshData(vertices.data(), static_cast<uint32_t>(vertices.size()),
                         drawIndices.data(), static_cast<uint32_t>(drawIndices.size()))
        != MeshValidation::Ok) {
        return false;
    }

    *outVertices = std::move(vertices);
    *outIndices = std::move(drawIndices);
    return true;
}

std::string sanitizeImportedMeshName(const std::string& raw) {
    // Control characters out first, and any byte that is not part of a
    // well-formed UTF-8 sequence dropped entirely: a name from another tool is
    // arbitrary bytes, and it reaches a label, a status line, a Java `String`
    // and a `.forge` file. Half a character is a defect in all four.
    std::string cleaned;
    cleaned.reserve(raw.size());
    for (size_t i = 0; i < raw.size();) {
        const unsigned char lead = static_cast<unsigned char>(raw[i]);
        if (lead < 0x20u || lead == 0x7Fu) {
            cleaned.push_back(' ');  // a control character is a separator, not text
            ++i;
            continue;
        }
        if (lead < 0x80u) {
            cleaned.push_back(raw[i]);
            ++i;
            continue;
        }
        // A multi-byte sequence: its length, its shortest-form floor and its
        // upper bound all come from the lead byte, and every continuation must
        // actually be one. Surrogates and anything past U+10FFFF are not
        // characters and are dropped with the rest.
        size_t extra = 0;
        uint32_t code = 0;
        if ((lead & 0xE0u) == 0xC0u) {
            extra = 1;
            code = lead & 0x1Fu;
        } else if ((lead & 0xF0u) == 0xE0u) {
            extra = 2;
            code = lead & 0x0Fu;
        } else if ((lead & 0xF8u) == 0xF0u) {
            extra = 3;
            code = lead & 0x07u;
        } else {
            ++i;  // a continuation byte or an invalid lead: not text
            continue;
        }
        if (i + extra >= raw.size()) {
            break;  // the sequence runs past the end of the string
        }
        bool wellFormed = true;
        for (size_t c = 1; c <= extra; ++c) {
            const unsigned char cont = static_cast<unsigned char>(raw[i + c]);
            if ((cont & 0xC0u) != 0x80u) {
                wellFormed = false;
                break;
            }
            code = (code << 6) | (cont & 0x3Fu);
        }
        const uint32_t floors[4] = {0u, 0x80u, 0x800u, 0x10000u};
        if (!wellFormed || code < floors[extra] || code > 0x10FFFFu
            || (code >= 0xD800u && code <= 0xDFFFu)) {
            ++i;  // drop only the lead byte; the rest is re-judged on its own
            continue;
        }
        cleaned.append(raw, i, extra + 1);
        i += extra + 1;
    }

    size_t begin = cleaned.find_first_not_of(' ');
    if (begin == std::string::npos) {
        return {};
    }
    size_t end = cleaned.find_last_not_of(' ');
    std::string trimmed = cleaned.substr(begin, end - begin + 1);

    if (trimmed.size() > kMaxImportedMeshNameBytes) {
        // Cut on a UTF-8 boundary: a continuation byte is 10xxxxxx, so back up
        // past any of them rather than storing half a character.
        size_t cut = kMaxImportedMeshNameBytes;
        while (cut > 0 && (static_cast<unsigned char>(trimmed[cut]) & 0xC0u) == 0x80u) {
            --cut;
        }
        trimmed.resize(cut);
        // Trailing space from the cut is not part of a name.
        const size_t last = trimmed.find_last_not_of(' ');
        if (last == std::string::npos) {
            return {};
        }
        trimmed.resize(last + 1);
    }
    return trimmed;
}

bool importedMeshNameIsStorable(const std::string& name) {
    // Stated as "this is what the sanitizer would produce for it" rather than
    // as a second list of rules, so the `.forge` decoder and the importer can
    // never disagree about what a name may be.
    return !name.empty() && sanitizeImportedMeshName(name) == name;
}

std::string fallbackImportedMeshName(uint32_t ordinal) {
    // Built by hand rather than with snprintf so the result is exactly the
    // same bytes on every platform and has no locale in it.
    std::string digits;
    uint32_t value = ordinal;
    if (value == 0) {
        digits = "0";
    } else {
        while (value > 0) {
            digits.insert(digits.begin(), static_cast<char>('0' + (value % 10u)));
            value /= 10u;
        }
    }
    return "Imported " + digits;
}

}  // namespace forgeshape
