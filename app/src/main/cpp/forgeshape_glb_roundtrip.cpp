#include "forgeshape_glb_roundtrip.h"


#include <cmath>
#include <cstdio>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_freeform_subdivision.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

// One body's world-space geometry, re-derived from the domain.
//
// Nothing here comes from the exporter. The mesh is regenerated from the
// Construction parameters (or read from the Frozen Sculpt Mesh), carried
// through the product's own crease-policy render-mesh derivation, and placed
// with `modelMatrix()` — the same matrix the RENDERER consumes. That is what
// makes this side an independent expectation rather than a second copy of what
// was written.
struct SourceGeometry {
    bool ok = false;
    bool fromSculpt = false;
    std::vector<Vec3> worldPositions;
    std::vector<Vec3> worldNormals;
    std::vector<uint32_t> indices;
};

SourceGeometry evaluateSourceGeometry(const SceneObject& body, ProjectKind kind) {
    SourceGeometry out;

    const FrozenSculpt& frozen = body.frozenSculpt();
    const bool useSculpt = (kind == ProjectKind::Sculpt) && frozen.mesh.frozen();
    out.fromSculpt = useSculpt;

    RenderMeshData surfaces;
    bool built = false;
    if (useSculpt) {
        built = buildRenderMesh(frozen.mesh.vertices().data(), frozen.mesh.vertexCount(),
                                frozen.mesh.indices().data(), frozen.mesh.indexCount(),
                                SurfaceShading::Smooth, &surfaces, /*renderBothSides=*/false);
    } else if (const ImportedMesh* imported = body.importedOrNull()) {
        // The same arrays the exporter reads, through the same one call that
        // resolves per-submesh `doubleSided` into geometry — so this diagnostic
        // compares the file against what the SCENE holds rather than against a
        // second idea of what an imported body is.
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
        if (imported->buildDrawData(&vertices, &indices)) {
            built = buildRenderMesh(vertices.data(), static_cast<uint32_t>(vertices.size()),
                                    indices.data(), static_cast<uint32_t>(indices.size()),
                                    SurfaceShading::Smooth, &surfaces,
                                    /*renderBothSides=*/false);
        }
    } else if (const ConstructionObject* source = body.constructionOrNull()) {
        const ConstructionMesh mesh = source->generateMesh();
        built = buildRenderMesh(mesh.vertices.data(),
                                static_cast<uint32_t>(mesh.vertices.size()),
                                mesh.indices.data(),
                                static_cast<uint32_t>(mesh.indices.size()),
                                SurfaceShading::Smooth, &surfaces, /*renderBothSides=*/false);
    } else if (const CadBody* cad = body.cadOrNull()) {
        ConstructionMesh mesh;
        if (cad->generateMesh(&mesh) == CadStatus::Ok) {
            built = buildRenderMesh(mesh.vertices.data(),
                                    static_cast<uint32_t>(mesh.vertices.size()),
                                    mesh.indices.data(),
                                    static_cast<uint32_t>(mesh.indices.size()),
                                    SurfaceShading::Smooth, &surfaces,
                                    /*renderBothSides=*/false);
        }    } else if (const FreeformBody* freeform = body.freeformOrNull()) {
        std::shared_ptr<const FreeformMesh> mesh;
        if (freeform->derived(&mesh) == FreeformStatus::Ok && mesh != nullptr) {
            built = buildRenderMesh(mesh->render.vertices.data(),
                                    static_cast<uint32_t>(mesh->render.vertices.size()),
                                    mesh->render.indices.data(),
                                    static_cast<uint32_t>(mesh->render.indices.size()),
                                    SurfaceShading::Smooth, &surfaces,
                                    /*renderBothSides=*/false);
        }
    }
    if (!built || surfaces.vertices.empty() || surfaces.indices.empty()) {
        return out;
    }

    // T * Rz * Ry * Rx * S, and the matrix a normal is carried by. Both taken
    // from ConstructionTransform, which is the one composition authority.
    const Mat4 model = body.transform().modelMatrix();
    const Mat4 normalMatrix = body.transform().normalMatrix();

    out.worldPositions.reserve(surfaces.vertices.size());
    out.worldNormals.reserve(surfaces.vertices.size());
    for (const RenderVertex& vertex : surfaces.vertices) {
        out.worldPositions.push_back(mat4TransformPoint(
                model, Vec3{vertex.position[0], vertex.position[1], vertex.position[2]}));
        const Vec3 carried = mat4TransformDirection(
                normalMatrix, Vec3{vertex.normal[0], vertex.normal[1], vertex.normal[2]});
        const float length = std::sqrt(carried.x * carried.x + carried.y * carried.y
                                       + carried.z * carried.z);
        out.worldNormals.push_back(length > 0.0f
                                           ? Vec3{carried.x / length, carried.y / length,
                                                  carried.z / length}
                                           : Vec3{0.0f, 0.0f, 0.0f});
    }
    out.indices = surfaces.indices;
    out.ok = true;
    return out;
}

double toleranceFor(const Vec3& a, const Vec3& b) {
    const double magnitude = std::fmax(
            std::fmax(std::fabs(static_cast<double>(a.x)), std::fabs(static_cast<double>(a.y))),
            std::fmax(std::fabs(static_cast<double>(a.z)),
                      std::fmax(std::fmax(std::fabs(static_cast<double>(b.x)),
                                          std::fabs(static_cast<double>(b.y))),
                                std::fabs(static_cast<double>(b.z)))));
    return std::fmax(kRoundtripAbsoluteToleranceMeters,
                     magnitude * kRoundtripRelativeTolerance);
}

double distance(const Vec3& a, const Vec3& b) {
    const double dx = static_cast<double>(a.x) - b.x;
    const double dy = static_cast<double>(a.y) - b.y;
    const double dz = static_cast<double>(a.z) - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double angleDegrees(const Vec3& a, const Vec3& b) {
    const double la = std::sqrt(static_cast<double>(a.x) * a.x + static_cast<double>(a.y) * a.y
                                + static_cast<double>(a.z) * a.z);
    const double lb = std::sqrt(static_cast<double>(b.x) * b.x + static_cast<double>(b.y) * b.y
                                + static_cast<double>(b.z) * b.z);
    if (la <= 0.0 || lb <= 0.0) {
        // A zero normal on either side is not an angle. Reported as a full
        // disagreement rather than as zero, so it cannot pass by accident.
        return (la <= 0.0 && lb <= 0.0) ? 0.0 : 180.0;
    }
    double dot = (static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y
                  + static_cast<double>(a.z) * b.z)
            / (la * lb);
    if (dot > 1.0) dot = 1.0;
    if (dot < -1.0) dot = -1.0;
    return std::acos(dot) * 180.0 / 3.14159265358979323846;
}

void accumulateBounds(const Vec3& p, double* min, double* max, bool first) {
    const double v[3] = {p.x, p.y, p.z};
    for (int c = 0; c < 3; ++c) {
        if (first || v[c] < min[c]) min[c] = v[c];
        if (first || v[c] > max[c]) max[c] = v[c];
    }
}

RoundtripResult notComparable(const char* reason, const char* exportStatus,
                              const char* importStatus) {
    RoundtripResult result;
    result.verdict = RoundtripVerdict::NotComparable;
    result.reason = reason;
    result.exportStatus = exportStatus == nullptr ? "" : exportStatus;
    result.importStatus = importStatus == nullptr ? "" : importStatus;
    return result;
}

}  // namespace

const char* roundtripVerdictName(RoundtripVerdict verdict) {
    switch (verdict) {
        case RoundtripVerdict::Equivalent: return "ROUNDTRIP_EQUIVALENT";
        case RoundtripVerdict::Mismatch: return "ROUNDTRIP_MISMATCH";
        case RoundtripVerdict::NotComparable: return "ROUNDTRIP_NOT_COMPARABLE";
    }
    return "ROUNDTRIP_UNKNOWN";
}

RoundtripResult compareSceneToGlb(const ConstructionScene& scene, ProjectKind kind,
                                  const uint8_t* bytes, size_t length) {
    ParsedGlbScene imported;
    const GlbImportStatus importStatus = importGlb(bytes, length, &imported);
    if (importStatus != GlbImportStatus::Ok) {
        return notComparable("the file could not be imported", "Ok",
                             glbImportStatusName(importStatus));
    }

    RoundtripResult result;
    result.exportStatus = "Ok";
    result.importStatus = glbImportStatusName(importStatus);
    result.sourceBodyCount = static_cast<uint32_t>(scene.bodyCount());
    result.importedMeshCount = static_cast<uint32_t>(imported.meshes.size());

    if (result.sourceBodyCount != result.importedMeshCount) {
        result.verdict = RoundtripVerdict::Mismatch;
        result.reason = "the file describes a different number of objects than the scene holds";
        return result;
    }

    bool allEquivalent = true;
    for (size_t i = 0; i < scene.bodyCount(); ++i) {
        const SceneObject& body = scene.bodyAt(i);
        const ParsedGlbMesh& mesh = imported.meshes[i];

        RoundtripBodyResult row;
        row.objectId = body.objectId();
        row.importedName = mesh.name;
        row.importedVertexCount = mesh.vertexCount();
        row.importedTriangleCount = mesh.triangleCount();

        const SourceGeometry source = evaluateSourceGeometry(body, kind);
        row.fromSculpt = source.fromSculpt;
        if (!source.ok) {
            row.countsMatch = false;
            allEquivalent = false;
            result.bodies.push_back(row);
            continue;
        }
        row.sourceVertexCount = static_cast<uint32_t>(source.worldPositions.size());
        row.sourceTriangleCount = static_cast<uint32_t>(source.indices.size() / 3u);
        row.countsMatch = row.sourceVertexCount == row.importedVertexCount
                && row.sourceTriangleCount == row.importedTriangleCount;

        // Bounds are computed for BOTH sides whatever the counts say, so a
        // report is still informative when the comparison itself cannot run.
        for (size_t v = 0; v < source.worldPositions.size(); ++v) {
            accumulateBounds(source.worldPositions[v], row.sourceMin, row.sourceMax, v == 0);
        }
        for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
            accumulateBounds(mesh.worldPosition(v), row.importedMin, row.importedMax, v == 0);
        }

        if (!row.countsMatch) {
            allEquivalent = false;
            result.bodies.push_back(row);
            continue;
        }

        // The current exporter's vertex order is deterministic and is the
        // render mesh's own order, so this compares PER INDEX. That is the
        // strongest available comparison: it would catch a reordering that a
        // bounds or nearest-point comparison would hide.
        row.topologyMatches = source.indices.size() == mesh.indices.size();
        for (size_t t = 0; t < source.indices.size() && row.topologyMatches; ++t) {
            row.topologyMatches = source.indices[t] == mesh.indices[t];
        }

        row.withinTolerance = true;
        for (uint32_t v = 0; v < row.sourceVertexCount; ++v) {
            const Vec3 expected = source.worldPositions[v];
            const Vec3 actual = mesh.worldPosition(v);
            const double delta = distance(expected, actual);
            const double tolerance = toleranceFor(expected, actual);
            if (delta > row.maxPositionDelta) {
                row.maxPositionDelta = delta;
                row.maxPositionDeltaVertex = v;
                row.toleranceAtMaxDelta = tolerance;
            }
            if (delta > tolerance) {
                row.withinTolerance = false;
            }
            const double degrees = angleDegrees(source.worldNormals[v], mesh.worldNormal(v));
            if (degrees > row.maxNormalDegrees) {
                row.maxNormalDegrees = degrees;
                row.maxNormalDegreesVertex = v;
            }
        }
        if (row.maxNormalDegrees > kRoundtripNormalToleranceDegrees) {
            row.withinTolerance = false;
        }
        if (!row.topologyMatches) {
            row.withinTolerance = false;
        }

        if (row.maxPositionDelta > result.maxPositionDelta) {
            result.maxPositionDelta = row.maxPositionDelta;
            result.maxPositionDeltaBody = row.objectId;
            result.maxPositionDeltaVertex = row.maxPositionDeltaVertex;
        }
        if (row.maxNormalDegrees > result.maxNormalDegrees) {
            result.maxNormalDegrees = row.maxNormalDegrees;
            result.maxNormalDegreesBody = row.objectId;
        }
        allEquivalent = allEquivalent && row.withinTolerance;
        result.bodies.push_back(row);
    }

    result.verdict = allEquivalent ? RoundtripVerdict::Equivalent : RoundtripVerdict::Mismatch;
    if (!allEquivalent) {
        result.reason = "at least one body disagrees beyond float32 quantization";
    }
    return result;
}

RoundtripResult runGlbRoundtripDiagnostic(const ConstructionScene& scene, ProjectKind kind) {
    GlbExportStatus why = GlbExportStatus::Ok;
    const std::vector<uint8_t> glb = exportSceneAsGlb(scene, kind, &why);
    if (why != GlbExportStatus::Ok || glb.empty()) {
        return notComparable("the scene could not be exported", glbExportStatusName(why), "");
    }
    return compareSceneToGlb(scene, kind, glb.data(), glb.size());
}

std::string formatRoundtripReport(const RoundtripResult& result) {
    char line[512];
    std::string out;

    std::snprintf(line, sizeof(line), "verdict=%s\n", roundtripVerdictName(result.verdict));
    out += line;
    std::snprintf(line, sizeof(line), "export_status=%s\nimport_status=%s\n",
                  result.exportStatus.c_str(), result.importStatus.c_str());
    out += line;
    if (!result.reason.empty()) {
        std::snprintf(line, sizeof(line), "reason=%s\n", result.reason.c_str());
        out += line;
    }
    std::snprintf(line, sizeof(line), "source_bodies=%u\nimported_meshes=%u\n",
                  result.sourceBodyCount, result.importedMeshCount);
    out += line;
    std::snprintf(line, sizeof(line),
                  "max_position_delta_m=%.9g\nmax_position_delta_body=%llu\n"
                  "max_position_delta_vertex=%u\n",
                  result.maxPositionDelta,
                  static_cast<unsigned long long>(result.maxPositionDeltaBody),
                  result.maxPositionDeltaVertex);
    out += line;
    std::snprintf(line, sizeof(line), "max_normal_delta_deg=%.9g\nmax_normal_delta_body=%llu\n",
                  result.maxNormalDegrees,
                  static_cast<unsigned long long>(result.maxNormalDegreesBody));
    out += line;
    std::snprintf(line, sizeof(line), "position_tolerance=max(%.9g m, %.9g x magnitude)\n",
                  kRoundtripAbsoluteToleranceMeters, kRoundtripRelativeTolerance);
    out += line;
    std::snprintf(line, sizeof(line), "normal_tolerance_deg=%.9g\n",
                  kRoundtripNormalToleranceDegrees);
    out += line;

    for (const RoundtripBodyResult& body : result.bodies) {
        std::snprintf(line, sizeof(line),
                      "body=%llu name=%s source=%s verts=%u/%u tris=%u/%u counts=%s"
                      " topology=%s within=%s\n",
                      static_cast<unsigned long long>(body.objectId), body.importedName.c_str(),
                      body.fromSculpt ? "sculpt" : "construction", body.sourceVertexCount,
                      body.importedVertexCount, body.sourceTriangleCount,
                      body.importedTriangleCount, body.countsMatch ? "match" : "DIFFER",
                      body.topologyMatches ? "match" : "DIFFER",
                      body.withinTolerance ? "yes" : "NO");
        out += line;
        std::snprintf(line, sizeof(line),
                      "  source_aabb=[%.9g %.9g %.9g]..[%.9g %.9g %.9g]\n",
                      body.sourceMin[0], body.sourceMin[1], body.sourceMin[2], body.sourceMax[0],
                      body.sourceMax[1], body.sourceMax[2]);
        out += line;
        std::snprintf(line, sizeof(line),
                      "  import_aabb=[%.9g %.9g %.9g]..[%.9g %.9g %.9g]\n",
                      body.importedMin[0], body.importedMin[1], body.importedMin[2],
                      body.importedMax[0], body.importedMax[1], body.importedMax[2]);
        out += line;
        std::snprintf(line, sizeof(line),
                      "  max_position_delta_m=%.9g at_vertex=%u tolerance=%.9g"
                      " max_normal_deg=%.9g at_vertex=%u\n",
                      body.maxPositionDelta, body.maxPositionDeltaVertex,
                      body.toleranceAtMaxDelta, body.maxNormalDegrees,
                      body.maxNormalDegreesVertex);
        out += line;
    }
    return out;
}

}  // namespace forgeshape
