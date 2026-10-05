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
// snapshot, never a UI event log and never the Undo/Redo stack. Each body is
// named by exactly ONE geometry section -- `CONS` (a PrimitiveSource), `IMPT`
// (an Imported Mesh) or `CADB` (a sketch and its extrusion, optionally
// supported by another body's face) -- plus its `SCNE` placement and an
// optional `SCUL` Frozen Sculpt Mesh.
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

#include <string>

#include "forgeshape_cad_body.h"
#include "forgeshape_construction.h"
#include "forgeshape_imported_mesh.h"
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
// Set when the file carries Imported Mesh geometry. It is also the COMPATIBILITY
// GATE: every reader before `IMPORT-01A` refuses an unknown header-flag bit
// outright (`BadHeader`), so a build that could not reconstruct an imported body
// cannot open the file at all rather than opening it with the imported objects
// silently missing. The IMPT section's required bit says the same thing a second
// time, for a reader that got past the header.
constexpr uint8_t kHeaderFlagHasImported = 0x04u;
// Set when the file carries a CAD Body (`CAD-R0-A1A2`), on exactly IMPT's
// terms: the bit is the compatibility gate an older reader refuses outright,
// and the CADB section's required bit says it again past the header. A build
// that cannot regenerate a sketch cannot open a file that needs one.
constexpr uint8_t kHeaderFlagHasCad = 0x08u;

// FourCCs as four ASCII bytes in file order. Compared byte by byte rather than
// packed into an integer, so nothing about the comparison depends on the host's
// byte order.
constexpr char kSectionTagScene[4] = {'S', 'C', 'N', 'E'};
constexpr char kSectionTagConstruction[4] = {'C', 'O', 'N', 'S'};
constexpr char kSectionTagSculpt[4] = {'S', 'C', 'U', 'L'};
constexpr char kSectionTagImported[4] = {'I', 'M', 'P', 'T'};
constexpr char kSectionTagCad[4] = {'C', 'A', 'D', 'B'};

constexpr uint16_t kSceneSectionVersion = 1;
// Stage 018A: version 2 adds, per body, a FLAGS byte and a NAME.
//
// Written only when at least one body needs it -- one that is hidden, locked,
// or carries a stored name SCNE is the owner of. A project of visible,
// unlocked, unnamed bodies stays at v1 and byte-identical, which is why the
// whole existing fixture corpus is unchanged; that is the same rule CADB v2 and
// v3 follow, and for the same reason.
//
// SCNE is a REQUIRED section, so an older build refuses a v2 file outright
// (`UnsupportedSectionVersion`) rather than opening a project with every body
// visible and unlocked when the user hid or locked some. Fail-closed is the
// right side to err on: silently ignoring a lock is worse than declining.
//
// A v1 file loads as the default a body has always had -- visible, unlocked,
// and named only where its own section already named it. That is the
// SceneObject's member initializers, not a migration.
constexpr uint16_t kSceneSectionVersionV2 = 2;

// SCNE v2 per-body flag bits. FILE-owned, and every other bit is reserved and
// must be zero -- a set reserved bit is refused as malformed rather than
// ignored, so a future flag cannot be silently dropped by this build.
constexpr uint8_t kSceneBodyFlagHidden = 0x01u;
constexpr uint8_t kSceneBodyFlagLocked = 0x02u;
constexpr uint8_t kSceneBodyFlagMask = 0x03u;
constexpr uint16_t kConstructionSectionVersion = 1;
constexpr uint16_t kSculptSectionVersion = 1;
constexpr uint16_t kImportedSectionVersion = 1;
constexpr uint16_t kCadSectionVersion = 1;
// CAD-A3: version 2 adds a per-body support kind and, for a face-supported
// body, its TopoRef. Written only when a body IS face-supported; a world-only
// CAD project stays byte-identical at v1. An older build refuses v2 (a required
// section at an unknown version) rather than opening half a body.
constexpr uint16_t kCadSectionVersionV2 = 2;
// SKETCH-UX-R1: version 3 adds the two CURVE entity kinds, Arc (file code 5)
// and Spline (file code 6). Written only when a body's sketch actually carries
// one; a project of lines, polylines, rectangles and circles stays at v1 or v2
// and byte-identical. An older build refuses v3 -- a required section at an
// unknown version -- rather than opening a body with a curve silently missing
// or, worse, replaced by the straight edge between its ends.
constexpr uint16_t kCadSectionVersionV3 = 3;
// CAD-EXT-R1: version 4 adds the extrusion's EXTENT -- the mode, and the second
// side's distance. Written only when a body actually reaches both sides of its
// sketch plane; a project whose every extrusion is One Side keeps whichever of
// v1, v2 or v3 it already used and stays byte-identical, because a One Side
// extrusion is exactly `directionCode` + `depth` and always was. An older build
// refuses v4 -- a required section at an unknown version -- rather than opening
// a body with half its extent silently missing.
constexpr uint16_t kCadSectionVersionV4 = 4;
// CAD-VERTICAL-SLICE-R1: version 5 adds the REGION selection (a chosen region's
// holes and any further chosen regions) and the retained FEATURE CHAIN (later
// Add/Cut features standing on the same body's own faces), as a tail after the
// first feature's entities (DATA_PACKAGE_SPEC.md §7f). Written only when a body
// selects anything but one region without holes or carries a later feature; a
// project that does neither keeps v1..v4 and stays byte-identical. An older
// build refuses v5 rather than opening a body with its holes silently filled or
// its Add and Cut features silently missing.
constexpr uint16_t kCadSectionVersionV5 = 5;
// CAD-V6-S1: version 6 is ONE combined migration for two things that both
// change what a feature's input is (DATA_PACKAGE_SPEC.md §7g): a body's
// retained SKETCH TABLE, which features reference by `sketchId` instead of
// carrying a sketch inline -- so two features may extrude one sketch -- with
// the sketch and feature id high-water marks; and an explicit per-feature
// SELECTION KIND, LoopRegions (the v5 REGIONS block) or PlanarFaces (canonical
// `PlanarFaceRef`s). Written only when a body says something v1..v5 cannot
// (`cadBodyStateLegacyRepresentable` false); every other project keeps its
// bytes. An older build refuses v6 rather than opening a body with a shared
// sketch duplicated or a face selection silently read as something else.
constexpr uint16_t kCadSectionVersionV6 = 6;
// CAD-V6-REVOLVE-NEWBODY-E2E-R1: version 7 is v6's layout with an explicit
// FEATURE KIND after every feature id and a payload of that kind's own
// (DATA_PACKAGE_SPEC.md §7h): Extrude (code 1) carries exactly the bytes a v6
// feature carries after its id, Revolve (code 2) its operation, sketch id, axis
// edge ref, angle in degrees and direction, then the same selection block.
// Written only when a body carries a Revolve; every project without one keeps
// the v1..v6 bytes it always had. An older build refuses v7 as a required
// section at an unknown version rather than reading a Revolve as an Extrude.
constexpr uint16_t kCadSectionVersionV7 = 7;

// CADB v1 file codes. FILE-owned, 1-based, and deliberately not a cast of any
// C++ enum, on the same terms as the primitive codes.
uint8_t workplaneFileCode(Workplane plane);
bool workplaneFromFileCode(uint8_t code, Workplane* out);
uint8_t extrudeDirectionFileCode(ExtrudeDirection direction);
bool extrudeDirectionFromFileCode(uint8_t code, ExtrudeDirection* out);
// CADB v4 extent codes (`CAD-EXT-R1`), file-owned and 1-based like the rest.
uint8_t extrudeExtentFileCode(ExtrudeExtentMode mode);
bool extrudeExtentFromFileCode(uint8_t code, ExtrudeExtentMode* out);
uint8_t sketchEntityKindFileCode(SketchEntityKind kind);
bool sketchEntityKindFromFileCode(uint8_t code, SketchEntityKind* out);

// CADB v2 face-kind file codes (`CAD-A3`), file-owned and 1-based like the rest.
uint8_t cadFaceKindFileCode(CadFaceKind kind);
// CADB v5 operation codes: 1 New Body, 2 Add, 3 Cut.
uint8_t cadFeatureOperationFileCode(CadFeatureOperation operation);
bool cadFeatureOperationFromFileCode(uint8_t code, CadFeatureOperation* out);
bool cadFaceKindFromFileCode(uint8_t code, CadFaceKind* out);

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
    // Stage 018A, SCNE v2. Both default to what a body has always been, so a v1
    // file decodes into exactly these values without a migration step.
    bool visible = true;
    bool locked = false;
    // The body's display name, owned HERE for a Construction Body and a CAD
    // Body, and left EMPTY for an Imported Mesh.
    //
    // The name has exactly one owner per representation and is never written
    // twice. `IMPT` has carried an imported object's name since `IMPORT-01A` --
    // it came from the file the geometry came from -- and Rename simply writes
    // that same field, so an imported body needs nothing here and a v2 file
    // that put a name here for one is REFUSED as malformed. A Construction Body
    // and a CAD Body had nowhere to store one, so this is where theirs lives.
    std::string name;
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
    // One entry per scene body that HAS a Construction Source, in scene order.
    //
    // That was "one per scene body" until `IMPORT-01A`, and it read the same
    // way because every body had one. An Imported Mesh has none, and no entry
    // may be fabricated for it: a default Box standing in for geometry the file
    // actually carried would be inventing project data, and a later edit would
    // then reshape a body from parameters nobody authored.
    std::vector<ProjectConstructionBody> bodies;
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

// Which SOURCE a sculpt entry's body has is deliberately not stored, and since
// `IMPORT-01B` a `SCUL` entry may sit over a `CONS` body or over an `IMPT` one.
// A Frozen Sculpt Mesh is its own positions and its own topology whatever it
// was frozen from, and nothing reads the origin back; the exactly-one-of rule
// stays where it belongs, on `CONS` versus `IMPT`.
//
// A build from before `IMPORT-01B` REFUSES an `IMPT`+`SCUL` file
// (`UnresolvedReference`) rather than opening it with half of a body. That is
// the same fail-closed shape `kHeaderFlagHasImported` gives an older reader,
// and it is why this needed no version bump: an older build cannot
// misunderstand the file, only decline it.

// IMPT, per body whose representation is an Imported Mesh.
//
// This is the first section that stores GEOMETRY as project truth rather than
// as something the load regenerates, and the reason is the whole point of the
// representation: a Construction Body's mesh is a product of its parameters and
// `generateMesh()` will make it again, while an imported object IS its
// geometry — no rule exists that could recreate it, and the file it came from
// is not part of the project. Losing these arrays would lose the object.
//
// What is deliberately absent: the source `.glb`'s path, `Uri` or bytes; the
// node transform (its linear part is baked into `positions` and its translation
// is the body's SCNE placement); every material, colour and UV the parser
// validated and then ignored; and the preview that read the file first.
struct ProjectImportedBody {
    ObjectId objectId = kNoObject;
    // The object's display name, already sanitized by the domain's own rule.
    // Stored because it came from the FILE and the file is gone after an
    // import: a project that reopened with anonymous rows would have lost
    // something the user could see.
    std::string name;
    std::vector<float> positions;  // 3 per vertex, LOCAL space, IEEE-754 bits
    // One unit direction per position. Stored rather than regenerated because
    // the file may have STATED them, and re-deriving smooth normals from the
    // triangles would silently replace an artist's hard edges with this
    // build's own guess.
    std::vector<float> normals;
    std::vector<uint32_t> indices;
    // The submesh ranges, in order, tiling `indices` exactly. Each carries the
    // one material fact this product keeps, `doubleSided`, because it decides
    // which triangles are visible rather than how they look.
    std::vector<ImportedMeshBatch> batches;

    uint32_t vertexCount() const { return static_cast<uint32_t>(positions.size() / 3); }
    uint32_t indexCount() const { return static_cast<uint32_t>(indices.size()); }
    uint32_t batchCount() const { return static_cast<uint32_t>(batches.size()); }
};

struct ProjectImportedRecord {
    std::vector<ProjectImportedBody> bodies;  // scene order; only imported bodies
};

// CADB, per body whose representation is a CAD Body (`CAD-R0-A1A2`).
//
// The AUTHORED truth and nothing derived: the workplane, every sketch entity
// with its per-sketch id and the sketch's id allocator, and the extrusion --
// which profile by anchor entity id, how deep, which way. No polygon, no
// triangle and no vertex: all of that is regenerated by `generateCadMesh` on
// load, exactly as a primitive's mesh is regenerated from its parameters. A
// file that stored the mesh and called the feature parametric would have
// stored a product of the truth beside the truth.
//
// v1 of this section is exactly ONE sketch and ONE linear extrusion per body,
// which is what R0 builds. A second feature kind takes a new section version
// rather than a discriminator inside this one.
struct ProjectCadBody {
    ObjectId objectId = kNoObject;
    CadBodyState state;
};

struct ProjectCadRecord {
    std::vector<ProjectCadBody> bodies;  // scene order; only CAD bodies
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
    bool hasImported = false;
    ProjectImportedRecord imported;
    bool hasCad = false;
    ProjectCadRecord cad;
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

// The same deterministic writer WITHOUT the semantic validation: the bytes a
// document says, whether or not a reader will accept them. It exists so a
// refusal fixture can be CONSTRUCTED from a document with its bad value in
// place -- exactly as `scripts/build-forge-corpus.ps1` constructs it -- rather
// than patched afterwards. Never used to save a project.
std::vector<uint8_t> encodeProjectV1Unchecked(const ProjectDocument& document);

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
