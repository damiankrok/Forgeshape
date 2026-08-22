// ForgeShape runtime mesh ownership.
//
// This is the CPU-side boundary between "who produces geometry" and "who draws
// it". It is deliberately the SMALLEST representation that lets a mesh change
// at runtime; it is NOT the Geometry Core and NOT the Construction mesh format.
//
// No JNI, no Android, no Vulkan types appear here. The renderer describes this
// data to Vulkan; Vulkan does not live in it.
//
// Contract
// --------
//   * A RuntimeMesh is IMMUTABLE once published. Nothing hands out a mutable
//     pointer into it, so a consumer that holds a snapshot can read it without
//     any lock and without the data changing underneath.
//   * Revisions are monotonically increasing. A revision that is not newer than
//     the current one can never replace it.
//   * The stable ObjectId lives on the mesh (and on the store), not on any GPU
//     resource, so reallocating buffers cannot change what is selected.
//   * Invalid input FAILS CLOSED: the store keeps the previous revision and
//     reports the reason. Nothing is ever partially published.
//
// Consumers observe the latest revision; if several revisions are published
// between two observations, only the newest is seen. That coalescing is
// deliberate and is what keeps the GPU upload path bounded.
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "forgeshape_object_id.h"
#include "forgeshape_picking.h"

namespace forgeshape {

// Interleaved position + colour: exactly the layout the cube pipeline's vertex
// input description expects, so a published mesh can be memcpy'd into staging
// memory without any repacking.
struct MeshVertex {
    float position[3];
    float color[3];
};

using MeshRevision = uint64_t;

// Reserved: "no mesh has ever been published".
constexpr MeshRevision kNoMeshRevision = 0;

// Hard caps. They exist so that every byte-size computation in this module and
// in the renderer's capacity policy is provably free of overflow, not because
// any real mesh is expected to approach them.
constexpr uint32_t kMaxMeshVertices = 4u * 1000u * 1000u;
constexpr uint32_t kMaxMeshIndices = 24u * 1000u * 1000u;
constexpr uint64_t kMaxMeshBytes = 512ull * 1024ull * 1024ull;

enum class MeshValidation {
    Ok,
    NullData,
    EmptyVertices,
    EmptyIndices,
    IndexCountNotTriangles,
    IndexOutOfRange,
    NonFinitePosition,
    TooLarge,
};

const char* meshValidationName(MeshValidation why);

// Fails closed on: null pointers, zero counts, an index count that is not a
// multiple of three, any index >= vertexCount, any non-finite position
// component, and anything past the hard caps.
MeshValidation validateMeshData(const MeshVertex* vertices, uint32_t vertexCount,
                                const uint32_t* indices, uint32_t indexCount);

class RuntimeMesh;
using RuntimeMeshPtr = std::shared_ptr<const RuntimeMesh>;

// Validates, then copies. Returns nullptr (and sets *outWhy) if the data is not
// a usable triangle mesh; nothing partial is ever produced. This is the ONLY
// way to obtain a RuntimeMesh, so every mesh that exists has been validated.
//
// `renderBothSides` is a GEOMETRIC fact about this mesh, not a display setting:
// it is true only for a flat, open, single-sided sheet (today, exactly a
// Construction Plane) that has no "inside" a two-sided render or pick could
// wrongly reach, unlike a closed solid. It travels with the mesh so the
// render-only derived-normals layer and CPU picking can each make their own
// bounded, presentation/pick-appropriate use of it without either one needing
// to know PrimitiveKind.
RuntimeMeshPtr createRuntimeMesh(ObjectId objectId, MeshRevision revision,
                                 const MeshVertex* vertices, uint32_t vertexCount,
                                 const uint32_t* indices, uint32_t indexCount,
                                 MeshValidation* outWhy = nullptr,
                                 bool renderBothSides = false);

// One immutable published mesh revision.
class RuntimeMesh {
public:
    ObjectId objectId() const { return objectId_; }
    MeshRevision revision() const { return revision_; }

    const MeshVertex* vertices() const { return vertices_.data(); }
    uint32_t vertexCount() const { return static_cast<uint32_t>(vertices_.size()); }
    uint64_t vertexBytes() const { return vertices_.size() * sizeof(MeshVertex); }

    const uint32_t* indices() const { return indices_.data(); }
    uint32_t indexCount() const { return static_cast<uint32_t>(indices_.size()); }
    uint64_t indexBytes() const { return indices_.size() * sizeof(uint32_t); }

    uint32_t triangleCount() const { return indexCount() / 3; }

    // See createRuntimeMesh's doc comment: a geometric fact about this mesh
    // (flat, open, one canonical front), not a display setting.
    bool renderBothSides() const { return renderBothSides_; }

    // Non-owning triangle view for CPU picking. Valid for as long as the caller
    // holds the shared_ptr this mesh came from.
    TriangleMeshView triangleView() const;

private:
    // Private, so validation cannot be bypassed.
    friend RuntimeMeshPtr createRuntimeMesh(ObjectId, MeshRevision, const MeshVertex*, uint32_t,
                                            const uint32_t*, uint32_t, MeshValidation*, bool);
    RuntimeMesh(ObjectId objectId, MeshRevision revision, const MeshVertex* vertices,
                uint32_t vertexCount, const uint32_t* indices, uint32_t indexCount,
                bool renderBothSides);

    const ObjectId objectId_;
    const MeshRevision revision_;
    const std::vector<MeshVertex> vertices_;
    const std::vector<uint32_t> indices_;
    const bool renderBothSides_;
};

// The single publication point for runtime geometry.
//
// Producers publish; consumers (the renderer on its own thread, CPU picking on
// the UI thread) observe. The mutex is held only long enough to swap a
// shared_ptr, never across a GPU copy.
class MeshStore {
public:
    explicit MeshStore(ObjectId objectId = kDemoCubeObjectId) : objectId_(objectId) {}

    // The identity of the object this store publishes revisions for. Constant
    // for the life of the store, and completely independent of any GPU
    // resource, which is what makes selection survive reallocation.
    ObjectId objectId() const { return objectId_; }

    // Mints the next revision and publishes it. Returns kNoMeshRevision and
    // leaves the current revision untouched when the data is invalid.
    // `renderBothSides`: see createRuntimeMesh's doc comment.
    MeshRevision publish(const MeshVertex* vertices, uint32_t vertexCount,
                         const uint32_t* indices, uint32_t indexCount,
                         MeshValidation* outWhy = nullptr, bool renderBothSides = false);

    // Publishes an already-built revision. Rejects null, a foreign ObjectId and
    // any revision that is not strictly newer than the current one, so a stale
    // snapshot can never overwrite a newer one.
    bool publishSnapshot(const RuntimeMeshPtr& mesh);

    // The latest published revision, or nullptr before the first publish.
    RuntimeMeshPtr current() const;
    MeshRevision currentRevision() const;

    uint64_t publishedCount() const;
    uint64_t rejectedCount() const;

private:
    const ObjectId objectId_;
    mutable std::mutex mutex_;
    RuntimeMeshPtr current_;
    MeshRevision nextRevision_ = 1;
    uint64_t published_ = 0;
    uint64_t rejected_ = 0;
};

// Process-scoped store. Like the camera and the selection, it outlives every
// Surface: the current mesh revision survives home/resume and swapchain
// recreation and is reset only when the process dies.
MeshStore& meshStore();

// ---------------------------------------------------------------------------
// GPU capacity policy (pure arithmetic, so it is testable without Vulkan)
// ---------------------------------------------------------------------------
//
// Reuse when the existing capacity is already sufficient — including for a
// SMALLER mesh, which must never trigger a shrink or a reallocation. Otherwise
// grow by 1.5x, but never to less than what is needed. `current` is always a
// value this function previously produced, so it is bounded by kMaxMeshBytes
// and `current + current / 2` cannot overflow.
//
// Returns false (leaving *out untouched) when `needed` exceeds the hard cap.
bool growCapacityBytes(uint64_t currentCapacity, uint64_t neededBytes, uint64_t* out);

// True when the existing capacity is sufficient and must be reused as-is.
inline bool capacityIsSufficient(uint64_t currentCapacity, uint64_t neededBytes) {
    return neededBytes <= currentCapacity;
}

// ---------------------------------------------------------------------------
// Bounded upload diagnostics
// ---------------------------------------------------------------------------
//
// Plain counters, no Vulkan types. The renderer is the only writer; the debug
// command path reads them to report what the GPU side actually did. This is a
// diagnostics sink, not a second owner of GPU state.
struct MeshGpuStats {
    uint64_t uploadedRevision = 0;
    // What the GPU holds. Since shading normals were added these are the
    // DERIVED RENDER counts, which differ from the authoritative ones whenever
    // a hard edge forced a corner to split into several render vertices.
    uint64_t uploadedVertexCount = 0;
    uint64_t uploadedIndexCount = 0;
    // What the authoritative RuntimeMesh that render data was derived from
    // holds. These are the counts every other subsystem means — picking,
    // sculpt topology, capacity reasoning about the source — and they are
    // reported separately precisely so the two can never be confused.
    uint64_t sourceVertexCount = 0;
    uint64_t sourceIndexCount = 0;
    uint64_t vertexCapacityBytes = 0;
    uint64_t indexCapacityBytes = 0;
    uint64_t stagingCapacityBytes = 0;
    uint64_t bufferGrowCount = 0;    // device-local buffer (re)creations
    uint64_t stagingGrowCount = 0;
    uint64_t uploadCount = 0;
    uint64_t reuseUploadCount = 0;   // uploads that reused existing capacity
    uint64_t liveBufferObjects = 0;  // live VkBuffers owned by the mesh path
    uint64_t failedUploadCount = 0;
};

class MeshUploadDiagnostics {
public:
    MeshGpuStats snapshot() const;

    void recordUpload(MeshRevision revision, uint32_t vertexCount, uint32_t indexCount,
                      uint64_t vertexCapacity, uint64_t indexCapacity, bool reusedCapacity);
    // Records the authoritative counts the last upload's render data was
    // derived from. Separate from recordUpload because they describe a
    // different mesh, not a different aspect of the same one.
    void recordSourceCounts(uint32_t vertexCount, uint32_t indexCount) {
        sourceVertexCount_.store(vertexCount);
        sourceIndexCount_.store(indexCount);
    }
    void recordBufferGrow() { bufferGrowCount_.fetch_add(1); }
    void recordStagingGrow(uint64_t capacity) {
        stagingGrowCount_.fetch_add(1);
        stagingCapacityBytes_.store(capacity);
    }
    void recordFailure() { failedUploadCount_.fetch_add(1); }
    void setLiveBufferObjects(uint64_t n) { liveBufferObjects_.store(n); }

private:
    std::atomic<uint64_t> uploadedRevision_{0};
    std::atomic<uint64_t> uploadedVertexCount_{0};
    std::atomic<uint64_t> uploadedIndexCount_{0};
    std::atomic<uint64_t> sourceVertexCount_{0};
    std::atomic<uint64_t> sourceIndexCount_{0};
    std::atomic<uint64_t> vertexCapacityBytes_{0};
    std::atomic<uint64_t> indexCapacityBytes_{0};
    std::atomic<uint64_t> stagingCapacityBytes_{0};
    std::atomic<uint64_t> bufferGrowCount_{0};
    std::atomic<uint64_t> stagingGrowCount_{0};
    std::atomic<uint64_t> uploadCount_{0};
    std::atomic<uint64_t> reuseUploadCount_{0};
    std::atomic<uint64_t> liveBufferObjects_{0};
    std::atomic<uint64_t> failedUploadCount_{0};
};

MeshUploadDiagnostics& meshUploadDiagnostics();

}  // namespace forgeshape
