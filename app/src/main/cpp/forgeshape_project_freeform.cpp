#include "forgeshape_project_freeform.h"

namespace forgeshape {

namespace {

// The fixed part of one body record: id, level, symmetry, reserved, the three
// marks and the three counts.
constexpr uint64_t kFreeformBodyHeaderBytes = 8u + 1u + 1u + 2u + 12u + 12u;
constexpr uint64_t kFreeformVertexBytes = 4u + 3u * 8u;
constexpr uint64_t kFreeformEdgeBytes = 4u + 4u + 4u + 8u;
constexpr uint64_t kFreeformFaceBytes = 4u + 4u * 4u;

}  // namespace

void writeFreeformPayload(ByteWriter& out, const ProjectFreeformRecord& record) {
    out.u32(static_cast<uint32_t>(record.bodies.size()));
    for (const ProjectFreeformBody& body : record.bodies) {
        const FreeformCage& cage = body.cage;
        out.u64(body.objectId);
        out.u8(cage.subdivisionLevel);
        out.u8(cage.symmetry);
        out.u16(0u);  // reserved
        out.u32(cage.nextVertexId);
        out.u32(cage.nextEdgeId);
        out.u32(cage.nextFaceId);
        out.u32(static_cast<uint32_t>(cage.vertices.size()));
        out.u32(static_cast<uint32_t>(cage.edges.size()));
        out.u32(static_cast<uint32_t>(cage.faces.size()));
        for (const FreeformVertex& v : cage.vertices) {
            out.u32(idOf(v.id));
            out.f64(v.position.x);
            out.f64(v.position.y);
            out.f64(v.position.z);
        }
        for (const FreeformEdge& e : cage.edges) {
            out.u32(idOf(e.id));
            out.u32(idOf(e.v0));
            out.u32(idOf(e.v1));
            out.f64(e.crease);
        }
        for (const FreeformFace& f : cage.faces) {
            out.u32(idOf(f.id));
            for (FreeformVertexId v : f.loop) out.u32(idOf(v));
        }
    }
}

ProjectCodecStatus decodeFreeformPayload(ByteReader& in, ProjectFreeformRecord* record) {
    uint32_t bodyCount = 0;
    if (!in.u32(&bodyCount)) {
        return ProjectCodecStatus::Truncated;
    }
    if (bodyCount == 0 || bodyCount > kMaxProjectBodies) {
        return ProjectCodecStatus::ImpossibleCount;
    }
    if (static_cast<uint64_t>(bodyCount) * kFreeformBodyHeaderBytes > in.remaining()) {
        return ProjectCodecStatus::Truncated;
    }
    record->bodies.resize(bodyCount);
    for (ProjectFreeformBody& body : record->bodies) {
        FreeformCage& cage = body.cage;
        uint16_t reserved = 0;
        uint32_t vertexCount = 0;
        uint32_t edgeCount = 0;
        uint32_t faceCount = 0;
        if (!in.u64(&body.objectId) || !in.u8(&cage.subdivisionLevel) || !in.u8(&cage.symmetry)
            || !in.u16(&reserved) || !in.u32(&cage.nextVertexId) || !in.u32(&cage.nextEdgeId)
            || !in.u32(&cage.nextFaceId) || !in.u32(&vertexCount) || !in.u32(&edgeCount)
            || !in.u32(&faceCount)) {
            return ProjectCodecStatus::Truncated;
        }
        // A reserved field is refused when set, never masked: a future meaning
        // cannot be silently dropped by this reader.
        if (reserved != 0u) {
            return ProjectCodecStatus::BadPayload;
        }
        if (vertexCount == 0 || vertexCount > kMaxFreeformVertices || edgeCount == 0
            || edgeCount > kMaxFreeformEdges || faceCount == 0 || faceCount > kMaxFreeformFaces) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        const uint64_t needed = vertexCount * kFreeformVertexBytes + edgeCount * kFreeformEdgeBytes
                                + faceCount * kFreeformFaceBytes;
        if (needed > in.remaining()) {
            return ProjectCodecStatus::Truncated;
        }
        cage.vertices.resize(vertexCount);
        for (FreeformVertex& v : cage.vertices) {
            uint32_t id = 0;
            if (!in.u32(&id) || !in.f64(&v.position.x) || !in.f64(&v.position.y)
                || !in.f64(&v.position.z)) {
                return ProjectCodecStatus::Truncated;
            }
            v.id = FreeformVertexId{id};
        }
        cage.edges.resize(edgeCount);
        for (FreeformEdge& e : cage.edges) {
            uint32_t id = 0;
            uint32_t v0 = 0;
            uint32_t v1 = 0;
            if (!in.u32(&id) || !in.u32(&v0) || !in.u32(&v1) || !in.f64(&e.crease)) {
                return ProjectCodecStatus::Truncated;
            }
            e.id = FreeformEdgeId{id};
            e.v0 = FreeformVertexId{v0};
            e.v1 = FreeformVertexId{v1};
        }
        cage.faces.resize(faceCount);
        for (FreeformFace& f : cage.faces) {
            uint32_t id = 0;
            if (!in.u32(&id)) {
                return ProjectCodecStatus::Truncated;
            }
            f.id = FreeformFaceId{id};
            for (FreeformVertexId& v : f.loop) {
                uint32_t vertex = 0;
                if (!in.u32(&vertex)) {
                    return ProjectCodecStatus::Truncated;
                }
                v = FreeformVertexId{vertex};
            }
        }
    }
    if (!in.atEnd()) {
        return ProjectCodecStatus::BadPayload;
    }
    return ProjectCodecStatus::Ok;
}

bool sameProjectFreeformRecord(const ProjectFreeformRecord& a, const ProjectFreeformRecord& b) {
    if (a.bodies.size() != b.bodies.size()) {
        return false;
    }
    for (size_t i = 0; i < a.bodies.size(); ++i) {
        if (a.bodies[i].objectId != b.bodies[i].objectId
            || !sameFreeformCage(a.bodies[i].cage, b.bodies[i].cage)) {
            return false;
        }
    }
    return true;
}

}  // namespace forgeshape
