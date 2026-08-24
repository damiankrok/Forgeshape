#include "forgeshape_mesh.h"

#include <cmath>

namespace forgeshape {

const char* meshValidationName(MeshValidation why) {
    switch (why) {
        case MeshValidation::Ok: return "ok";
        case MeshValidation::NullData: return "null_data";
        case MeshValidation::EmptyVertices: return "empty_vertices";
        case MeshValidation::EmptyIndices: return "empty_indices";
        case MeshValidation::IndexCountNotTriangles: return "index_count_not_triangles";
        case MeshValidation::IndexOutOfRange: return "index_out_of_range";
        case MeshValidation::NonFinitePosition: return "non_finite_position";
        case MeshValidation::TooLarge: return "too_large";
    }
    return "unknown";
}

MeshValidation validateMeshData(const MeshVertex* vertices, uint32_t vertexCount,
                                const uint32_t* indices, uint32_t indexCount) {
    if (vertices == nullptr || indices == nullptr) {
        return MeshValidation::NullData;
    }
    if (vertexCount == 0) {
        return MeshValidation::EmptyVertices;
    }
    if (indexCount == 0) {
        return MeshValidation::EmptyIndices;
    }
    if ((indexCount % 3u) != 0u) {
        return MeshValidation::IndexCountNotTriangles;
    }
    if (vertexCount > kMaxMeshVertices || indexCount > kMaxMeshIndices) {
        return MeshValidation::TooLarge;
    }
    // Bounded by the caps above, so neither product can overflow uint64.
    const uint64_t vertexBytes = static_cast<uint64_t>(vertexCount) * sizeof(MeshVertex);
    const uint64_t indexBytes = static_cast<uint64_t>(indexCount) * sizeof(uint32_t);
    if (vertexBytes > kMaxMeshBytes || indexBytes > kMaxMeshBytes) {
        return MeshValidation::TooLarge;
    }

    for (uint32_t i = 0; i < vertexCount; ++i) {
        const float* p = vertices[i].position;
        if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2])) {
            return MeshValidation::NonFinitePosition;
        }
    }
    for (uint32_t i = 0; i < indexCount; ++i) {
        if (indices[i] >= vertexCount) {
            return MeshValidation::IndexOutOfRange;
        }
    }
    return MeshValidation::Ok;
}

RuntimeMesh::RuntimeMesh(ObjectId objectId, MeshRevision revision, const MeshVertex* vertices,
                         uint32_t vertexCount, const uint32_t* indices, uint32_t indexCount,
                         bool renderBothSides)
    : objectId_(objectId),
      revision_(revision),
      vertices_(vertices, vertices + vertexCount),
      indices_(indices, indices + indexCount),
      renderBothSides_(renderBothSides) {}

TriangleMeshView RuntimeMesh::triangleView() const {
    TriangleMeshView view{};
    view.positions = vertices_.empty() ? nullptr : vertices_[0].position;
    view.positionStride = sizeof(MeshVertex);
    view.vertexCount = vertexCount();
    view.indices = indices_.empty() ? nullptr : indices_.data();
    view.indexCount = indexCount();
    return view;
}

RuntimeMeshPtr createRuntimeMesh(ObjectId objectId, MeshRevision revision,
                                 const MeshVertex* vertices, uint32_t vertexCount,
                                 const uint32_t* indices, uint32_t indexCount,
                                 MeshValidation* outWhy, bool renderBothSides) {
    const MeshValidation why = validateMeshData(vertices, vertexCount, indices, indexCount);
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != MeshValidation::Ok) {
        return nullptr;  // fail closed: no partial mesh is ever produced
    }
    if (revision == kNoMeshRevision) {
        return nullptr;  // revision 0 is reserved for "nothing published"
    }
    return RuntimeMeshPtr(new RuntimeMesh(objectId, revision, vertices, vertexCount, indices,
                                          indexCount, renderBothSides));
}

// ---------------------------------------------------------------------------
// MeshStore
// ---------------------------------------------------------------------------

MeshRevision MeshStore::publish(const MeshVertex* vertices, uint32_t vertexCount,
                                const uint32_t* indices, uint32_t indexCount,
                                MeshValidation* outWhy, bool renderBothSides) {
    // Validate before the lock: rejecting bad data must never disturb readers.
    const MeshValidation why = validateMeshData(vertices, vertexCount, indices, indexCount);
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != MeshValidation::Ok) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++rejected_;
        return kNoMeshRevision;  // the previous revision stays current
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const MeshRevision revision = nextRevision_++;
    RuntimeMeshPtr mesh = createRuntimeMesh(objectId_, revision, vertices, vertexCount, indices,
                                            indexCount, nullptr, renderBothSides);
    if (!mesh) {
        ++rejected_;
        return kNoMeshRevision;
    }
    current_ = std::move(mesh);
    ++published_;
    return revision;
}

bool MeshStore::publishSnapshot(const RuntimeMeshPtr& mesh) {
    if (!mesh) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++rejected_;
        return false;
    }
    if (mesh->objectId() != objectId_) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++rejected_;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const MeshRevision currentRev = current_ ? current_->revision() : kNoMeshRevision;
    if (mesh->revision() <= currentRev) {
        ++rejected_;  // a stale snapshot can never replace a newer one
        return false;
    }
    current_ = mesh;
    if (mesh->revision() >= nextRevision_) {
        nextRevision_ = mesh->revision() + 1;
    }
    ++published_;
    return true;
}

RuntimeMeshPtr MeshStore::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;  // a shared_ptr copy; the lock is never held across a copy
}

MeshRevision MeshStore::currentRevision() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_ ? current_->revision() : kNoMeshRevision;
}

uint64_t MeshStore::publishedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return published_;
}

uint64_t MeshStore::rejectedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rejected_;
}

// meshStore() is now "the ACTIVE body's store" and is defined in
// forgeshape_scene.cpp; see the note there for why it moved.

// ---------------------------------------------------------------------------
// Capacity policy
// ---------------------------------------------------------------------------

bool growCapacityBytes(uint64_t currentCapacity, uint64_t neededBytes, uint64_t* out) {
    if (out == nullptr) {
        return false;
    }
    if (neededBytes > kMaxMeshBytes || currentCapacity > kMaxMeshBytes) {
        return false;
    }
    if (neededBytes <= currentCapacity) {
        *out = currentCapacity;  // reuse; a smaller mesh never shrinks capacity
        return true;
    }
    // currentCapacity <= kMaxMeshBytes (512 MB), so this cannot overflow.
    uint64_t grown = currentCapacity + (currentCapacity / 2u);
    if (grown < neededBytes) {
        grown = neededBytes;
    }
    if (grown > kMaxMeshBytes) {
        grown = kMaxMeshBytes;
    }
    if (grown < neededBytes) {
        return false;
    }
    *out = grown;
    return true;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

MeshGpuStats MeshUploadDiagnostics::snapshot() const {
    MeshGpuStats s{};
    s.uploadedRevision = uploadedRevision_.load();
    s.uploadedVertexCount = uploadedVertexCount_.load();
    s.uploadedIndexCount = uploadedIndexCount_.load();
    s.sourceVertexCount = sourceVertexCount_.load();
    s.sourceIndexCount = sourceIndexCount_.load();
    s.vertexCapacityBytes = vertexCapacityBytes_.load();
    s.indexCapacityBytes = indexCapacityBytes_.load();
    s.stagingCapacityBytes = stagingCapacityBytes_.load();
    s.bufferGrowCount = bufferGrowCount_.load();
    s.stagingGrowCount = stagingGrowCount_.load();
    s.uploadCount = uploadCount_.load();
    s.reuseUploadCount = reuseUploadCount_.load();
    s.liveBufferObjects = liveBufferObjects_.load();
    s.failedUploadCount = failedUploadCount_.load();
    return s;
}

void MeshUploadDiagnostics::recordUpload(MeshRevision revision, uint32_t vertexCount,
                                         uint32_t indexCount, uint64_t vertexCapacity,
                                         uint64_t indexCapacity, bool reusedCapacity) {
    uploadedVertexCount_.store(vertexCount);
    uploadedIndexCount_.store(indexCount);
    vertexCapacityBytes_.store(vertexCapacity);
    indexCapacityBytes_.store(indexCapacity);
    uploadCount_.fetch_add(1);
    if (reusedCapacity) {
        reuseUploadCount_.fetch_add(1);
    }
    // Published last, so a reader that sees the revision also sees the rest.
    uploadedRevision_.store(revision);
}

MeshUploadDiagnostics& meshUploadDiagnostics() {
    static MeshUploadDiagnostics diagnostics;
    return diagnostics;
}

}  // namespace forgeshape
