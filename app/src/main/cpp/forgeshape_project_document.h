// The `.forge` project document: ForgeShape's own portable, versioned project
// file, and the only place its binary layout is expressed as code.
//
// Platform-neutral C++17. No Android, no JNI, no Vulkan, no renderer, no
// filesystem and no `ConstructionScene`: this layer knows what a project MEANS
// and what its bytes look like, and nothing about where those bytes live or
// which live objects they came from. Binding it to the running scene is
// forgeshape_project_state.{h,cpp}'s job, and that separation is what lets the
// whole format be exercised without a device.
//
// What a project IS
// -----------------
// A declarative semantic document -- a feature graph -- never a render-mesh
// snapshot, never a UI event log and never the Undo/Redo stack. Today the graph
// is degenerate: each Construction Body carries exactly one real feature, a
// PrimitiveSource, plus the body's placement. Future CAD features extend the
// graph; nothing here assumes there will only ever be one.
//
// Truth vs. derived, stated once
// ------------------------------
// SERIALIZED, because it cannot be recomputed:
//   * body identity, scene order, which body is active, and the id allocator's
//     high-water mark;
//   * the active primitive kind and ALL SIX remembered primitive parameter sets
//     (a Box -> Sphere -> Box round trip must come back to the box the user
//     typed, so the five inactive sets are user data, not spare state);
//   * placement -- position in metres, rotation in degrees stored EXACTLY as
//     given (370 stays 370; see ConstructionTransform), scale as a positive
//     unitless multiplier;
//   * each Frozen Sculpt Mesh's local float32 vertex POSITIONS and its index
//     topology, because after a stroke they cannot be recreated from the
//     Construction Source.
//
// NOT serialized, because the receiving installation regenerates it exactly:
//   * every Construction RuntimeMesh (generated from the parameters above);
//   * vertex normals, sculpt adjacency, render-mesh vertex duplication;
//   * `SculptRevision`, `MeshRevision`, update counters and GPU state;
//   * vertex COLOURS, which feed only the debug-only source-colour shading mode
//     and are reconstructed as one neutral value on load;
//   * the Construction Undo/Redo stack, which is session history.
//
// Portability
// -----------
// Every field is a file-owned fixed-width little-endian encoding written by
// forgeshape_project_bytes.h. Nothing here emits a struct image, an enum's ABI
// value, a pointer, a `size_t`, a padding byte, a device path, an Android
// object id or a GPU handle, so a file written on one supported ForgeShape
// installation decodes identically on another.
//
// The exact byte layout is owned by DATA_PACKAGE_SPEC.md; this header and its
// .cpp are its implementation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_object_id.h"
#include "forgeshape_transform.h"

namespace forgeshape {

// ---------------------------------------------------------------------------
// Format constants
// ---------------------------------------------------------------------------

// ASCII, 8 bytes, no terminator. The trailing '1' is a generation marker and is
// NOT the version: the version is the u16 pair that follows it.
constexpr char kForgeMagic[8] = {'F', 'O', 'R', 'G', 'E', 'S', 'H', '1'};

constexpr uint16_t kForgeVersionMajor = 1;
constexpr uint16_t kForgeVersionMinor = 0;
constexpr uint16_t kForgeHeaderBytes = 28;
constexpr uint16_t kForgeSectionHeaderBytes = 24;

// Section flags. bit0 says the reader must understand this section or refuse
// the file; every other bit is reserved and must be zero in v1.
constexpr uint16_t kSectionFlagRequired = 0x0001u;

// Header flags. They state which optional branches the file carries, so a
// reader knows what it is holding before it has parsed a single section.
constexpr uint8_t kHeaderFlagHasConstruction = 0x01u;
constexpr uint8_t kHeaderFlagHasSculpt = 0x02u;

// FourCCs as four ASCII bytes in file order. Compared byte by byte rather than
// packed into an integer, so nothing about the comparison depends on the host's
// byte order.
constexpr char kSectionTagScene[4] = {'S', 'C', 'N', 'E'};
constexpr char kSectionTagConstruction[4] = {'C', 'O', 'N', 'S'};
constexpr char kSectionTagSculpt[4] = {'S', 'C', 'U', 'L'};

constexpr uint16_t kSceneSectionVersion = 1;
constexpr uint16_t kConstructionSectionVersion = 1;
constexpr uint16_t kSculptSectionVersion = 1;

// v1 feature-graph codes. FILE-owned and independent of any C++ enum's ABI.
constexpr uint8_t kFeatureKindPrimitiveSource = 1;

// The one real feature every current body has. Identity is the pair
// (ObjectId, LocalFeatureId), and a body's PrimitiveSource is always 1 -- the
// numbering is per body, so a future feature graph can grow without renumbering
// anything that already exists.
constexpr uint32_t kPrimitiveSourceFeatureId = 1;

// A ceiling that makes every size computation here provably free of overflow.
// Not a product limit on how large a project may be: it is far above anything
// the editor can produce, and its job is to refuse an impossible count in a
// corrupt file BEFORE a single byte is allocated for it.
constexpr uint32_t kMaxProjectBodies = 4096;

// Which representation the file reopens in. The PRIMARY mode, not a hint: it
// decides what the user sees when the project comes back.
enum class ProjectKind : uint8_t {
    Construction = 1,
    Sculpt = 2,
};

const char* projectKindName(ProjectKind kind);

// File-owned primitive codes, deliberately 1-based and deliberately NOT
// `static_cast<uint8_t>(PrimitiveKind)`: the C++ enumerator order is an
// internal decision, and a file must not move underneath a project if it ever
// changes.
uint8_t primitiveFileCode(PrimitiveKind kind);
bool primitiveKindFromFileCode(uint8_t code, PrimitiveKind* out);

// ---------------------------------------------------------------------------
// Why a file was refused
// ---------------------------------------------------------------------------
//
// Every one of these is a REFUSAL, and a refusal changes nothing: decoding
// happens entirely into the DTOs below, and no live project state is touched
// until a complete document has passed every check.
enum class ProjectCodecStatus {
    Ok,
    NotForgeFile,               // the magic is not FORGESH1
    UnsupportedMajor,           // a newer incompatible generation of the format
    UnsupportedSectionVersion,  // same major, a required section we cannot read
    BadHeader,                  // headerBytes, kind, flags, reserved bits, fileBytes
    Truncated,                  // the file ends inside a header or a payload
    BadSectionHeader,           // flags/reserved bits, or a length past the file
    ChecksumMismatch,
    UnknownRequiredSection,
    DuplicateSection,
    MissingRequiredSection,
    BadPayload,                 // a payload's own structure does not add up
    ImpossibleCount,            // a count no project can have, refused before allocation
    InvalidSemanticValue,       // refused by the SAME contracts the live model uses
    UnresolvedReference,        // a CONS/SCUL body SCNE does not carry
    // Not a property of the file. A load is refused outright while a
    // Construction edit is open, because that half-finished user act has a
    // pre-state the history is holding, and replacing the scene under it would
    // leave that state describing bodies that no longer exist.
    RefusedEditInProgress,
};

const char* projectCodecStatusName(ProjectCodecStatus status);

// ---------------------------------------------------------------------------
// The document
// ---------------------------------------------------------------------------

// One entry of the v1 feature graph. Today the only kind is PrimitiveSource and
// every body has exactly one; it is a record because the GRAPH is the thing
// that grows, and a future feature must be able to arrive without the file
// format changing shape.
struct ProjectFeatureRecord {
    uint32_t localFeatureId = kPrimitiveSourceFeatureId;
    uint8_t kindCode = kFeatureKindPrimitiveSource;
};

// SCNE: project-neutral identity and placement, true for both project kinds.
struct ProjectBodyPlacement {
    ObjectId objectId = kNoObject;
    TransformValues transform{};
};

struct ProjectSceneRecord {
    std::vector<ProjectBodyPlacement> bodies;  // scene order, and it is the file's order
    // The id allocator's high-water mark. Stored so that a reopened project
    // cannot mint an id some already-loaded body is wearing -- the one thing
    // that would let a stale ObjectId resolve to the wrong body.
    ObjectId nextObjectId = kNoObject;
    ObjectId activeObjectId = kNoObject;
};

// CONS, per body. `shape` carries the active kind and all six remembered
// parameter sets; its `transform` member is NOT this record's truth and is not
// encoded here -- placement belongs to SCNE, which both project kinds carry.
// ConstructionObjectState is reused rather than re-declared so that adding a
// seventh primitive is one change in the domain and one in the codec, not
// three.
struct ProjectConstructionBody {
    ObjectId objectId = kNoObject;
    ConstructionObjectState shape{};
    std::vector<ProjectFeatureRecord> features;
};

struct ProjectConstructionRecord {
    std::vector<ProjectConstructionBody> bodies;  // one per scene body, in scene order
};

// SCUL, per body that HAS a Frozen Sculpt Mesh. Positions are local-space
// float32 triples in vertex order; indices are the exact index buffer. Normals,
// adjacency and revisions are absent on purpose -- all three are rebuilt from
// exactly this data on load.
struct ProjectSculptBody {
    ObjectId objectId = kNoObject;
    bool renderBothSides = false;
    bool sourceStale = false;
    // Whether this mesh had been SCULPTED when it was saved.
    //
    // A boolean, never the SculptRevision it is derived from: the number is
    // derived state and is not file truth. The FACT is, because it is what the
    // destructive Reset-Sculpt-from-Shape guard asks. Without it a reopened
    // project would report an unedited mesh, and the guard would let a reset
    // discard every stroke in the file without a word.
    bool hasEdits = false;
    std::vector<float> positions;  // 3 per vertex
    std::vector<uint32_t> indices;

    uint32_t vertexCount() const { return static_cast<uint32_t>(positions.size() / 3); }
    uint32_t indexCount() const { return static_cast<uint32_t>(indices.size()); }
};

struct ProjectSculptRecord {
    std::vector<ProjectSculptBody> bodies;  // scene order; only bodies that have one
};

// A complete project, decoded or about to be encoded. Plain data with no
// identity of its own: two documents that compare equal produce byte-identical
// files, which is the deterministic-writer rule stated as a property.
struct ProjectDocument {
    ProjectKind kind = ProjectKind::Construction;
    ProjectSceneRecord scene;
    bool hasConstruction = false;
    ProjectConstructionRecord construction;
    bool hasSculpt = false;
    ProjectSculptRecord sculpt;
};

// True when the two documents carry the same project semantics, field for
// field -- float BITS included, so a sculpted vertex that came back a single
// ULP away is a failure and not a rounding difference.
bool sameProjectDocument(const ProjectDocument& a, const ProjectDocument& b);

// ---------------------------------------------------------------------------
// Validation, encoding, decoding
// ---------------------------------------------------------------------------

// Every semantic rule the live model enforces, applied to a document.
//
// It calls the DOMAIN's own validators -- validateDimensionMeters,
// validateCapsuleMeters, validateTransformValue, validateScaleValue -- rather
// than restating them, so a file can never carry a value the editor would have
// refused, and a change to a domain rule cannot leave the codec behind.
ProjectCodecStatus validateProjectDocument(const ProjectDocument& document);

// Encodes to v1 bytes, or returns empty and reports why.
//
// Validates FIRST: an invalid document produces no file at all rather than a
// file that cannot be opened. The writer is deterministic -- the same document
// always produces the same bytes, in canonical section order (SCNE, then CONS,
// then SCUL), bodies in scene order, features by ascending LocalFeatureId.
std::vector<uint8_t> encodeProjectV1(const ProjectDocument& document,
                                     ProjectCodecStatus* outWhy = nullptr);

// Reads any supported version into `out`, writing nothing on failure.
//
// The version dispatch seam: it reads the header's major and hands a v1 file to
// the v1 decoder. There has never been a production format before v1, so there
// is deliberately NO v0 branch and no migration to claim -- the first real
// schema bump adds a branch here, keeps the v1 fixtures, and brings a real
// compatibility test with it.
//
// Compatibility rules, in one place:
//   * newer MAJOR                -> UnsupportedMajor;
//   * same major, newer minor    -> accepted only if every REQUIRED section
//                                   version is understood;
//   * unknown OPTIONAL section   -> skipped after its validated length;
//   * unknown REQUIRED section   -> UnknownRequiredSection;
//   * duplicate singleton section, bad flags/reserved bits, a length past the
//     end, an inconsistent fileBytes, a CRC mismatch, truncation -> refused.
//
// `outSkippedOptionalSections`, when given, receives how many sections were
// skipped because they were optional and unknown: "was skipped" and "was never
// there" are different outcomes, and a test must be able to tell them apart.
ProjectCodecStatus decodeProject(const uint8_t* data, size_t size, ProjectDocument* out,
                                 uint32_t* outSkippedOptionalSections = nullptr);

}  // namespace forgeshape
