// The GLB roundtrip diagnostic (`GLB-IMPORT-R0`).
//
// THE QUESTION: does the `.glb` ForgeShape wrote, decoded by something that is
// not the writer, describe the same WORLD-SPACE geometry as the scene it came
// from? A "no" is an exporter or reader defect; a "yes" means an external tool
// showing something different is presenting the same geometry differently.
//
// The two sides are produced by different code on purpose:
//   * EXPECTED is re-derived from DOMAIN TRUTH -- the effective mesh of each
//     body through `buildRenderMesh`, then `modelMatrix()` -- never from the
//     exporter's `GlbExportScene` or its serialized arrays, so an exporter that
//     captured the wrong geometry still fails here;
//   * ACTUAL is `forgeshape_gltf_import`'s parse of the real bytes, with the
//     node transform applied per glTF semantics.
//
// The identity under test is `modelMatrix() * p_local == nodeTranslation *
// p_baked`, i.e. `T·L·p == T·(L·p)`, which holds only if the bake, the node
// transform and the file agree.
//
// TOLERANCE: the only permitted loss is float32 quantization (`%.9g` round-trips
// float32 exactly), so the tolerance scales with the coordinate's magnitude --
// 1e-4 m is generous at 0.5 m and meaningless at 5000 m. Anything larger is a
// real disagreement and is reported as one.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no filesystem.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "forgeshape_gltf_import.h"
#include "forgeshape_project_document.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// Relative tolerance on a world coordinate, and the floor it never goes below.
//
// float32 carries about 7 significant decimal digits, so 1e-6 relative is
// roughly one part in a million — several ULP of headroom without being loose
// enough to hide a millimetre. The absolute floor keeps a coordinate near zero
// from demanding exact equality it cannot have after two matrix multiplies in
// a different order.
constexpr double kRoundtripRelativeTolerance = 1e-6;
constexpr double kRoundtripAbsoluteToleranceMeters = 1e-5;

// A normal is a direction, so its tolerance is angular. 0.25 degrees is far
// tighter than any shading difference a person could see and far looser than
// float32 noise on a normalized vector.
constexpr double kRoundtripNormalToleranceDegrees = 0.25;

enum class RoundtripVerdict {
    Equivalent,
    Mismatch,
    // The comparison could not be made at all: the export refused, the import
    // refused, or the scene was empty. Distinguished from Mismatch because
    // "the file disagrees with the scene" and "there is no file" are different
    // findings and only one of them is about geometry.
    NotComparable,
};

const char* roundtripVerdictName(RoundtripVerdict verdict);

// One body's row of the comparison.
struct RoundtripBodyResult {
    ObjectId objectId = kNoObject;
    std::string importedName;
    bool fromSculpt = false;

    uint32_t sourceVertexCount = 0;
    uint32_t importedVertexCount = 0;
    uint32_t sourceTriangleCount = 0;
    uint32_t importedTriangleCount = 0;

    // World-space axis-aligned bounds of each side, min then max.
    double sourceMin[3] = {0, 0, 0};
    double sourceMax[3] = {0, 0, 0};
    double importedMin[3] = {0, 0, 0};
    double importedMax[3] = {0, 0, 0};

    // The worst disagreement found, and where.
    double maxPositionDelta = 0.0;
    uint32_t maxPositionDeltaVertex = 0;
    double maxNormalDegrees = 0.0;
    uint32_t maxNormalDegreesVertex = 0;
    // The tolerance that applied at the offending vertex, so a reader can see
    // whether a delta was close to the line or nowhere near it.
    double toleranceAtMaxDelta = 0.0;

    bool countsMatch = false;
    bool topologyMatches = false;  // index arrays identical, element for element
    bool withinTolerance = false;
};

struct RoundtripResult {
    RoundtripVerdict verdict = RoundtripVerdict::NotComparable;
    // Why, when the verdict is NotComparable or Mismatch. Empty on Equivalent.
    std::string reason;
    std::string exportStatus;
    std::string importStatus;

    uint32_t sourceBodyCount = 0;
    uint32_t importedMeshCount = 0;

    double maxPositionDelta = 0.0;
    ObjectId maxPositionDeltaBody = kNoObject;
    uint32_t maxPositionDeltaVertex = 0;
    double maxNormalDegrees = 0.0;
    ObjectId maxNormalDegreesBody = kNoObject;

    std::vector<RoundtripBodyResult> bodies;
};

// Exports the scene through the REAL current writer, reimports the resulting
// bytes through the independent reader, and compares both against domain truth.
//
// Reads only: it publishes nothing, mints no revision, opens no transaction and
// mutates neither the scene nor any body.
RoundtripResult runGlbRoundtripDiagnostic(const ConstructionScene& scene, ProjectKind kind);

// The same comparison against bytes the caller already has — a file on disk, a
// sentinel from the evidence bundle — rather than a fresh export. `scene` and
// `kind` must be the project those bytes were written from.
RoundtripResult compareSceneToGlb(const ConstructionScene& scene, ProjectKind kind,
                                  const uint8_t* bytes, size_t length);

// A compact, stable, machine-readable rendering of a result.
//
// One `key=value` field per line, one `body=` line per body. Deliberately not
// JSON: it is read by a test, by a person and by a shell, it must survive being
// pasted into a report, and it must not tempt anything into parsing a document
// format out of a diagnostic.
std::string formatRoundtripReport(const RoundtripResult& result);

}  // namespace forgeshape
