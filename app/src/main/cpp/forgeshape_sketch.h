// The 2D sketch domain: entities, validation, closed-profile extraction and
// triangulation.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer, no
// camera and no pixel. A sketch is authored in METRES on a workplane
// (forgeshape_workplane.h); where the finger was on the screen when a point
// was placed is the sketch session's business and is never stored here.
//
// What is truth, and what is derived
// ----------------------------------
// TRUTH is the entity list: a line's two endpoints, a polyline's vertices, a
// rectangle's centre and its two sizes, a circle's centre and its radius --
// each with a stable, per-sketch integer identity. That is what the project
// file stores and what a numeric edit changes.
//
// DERIVED is everything this file computes from it: which entities close a
// profile, the polygon a profile becomes, the triangles that polygon splits
// into. None of it is stored, all of it is regenerated, and no rule anywhere
// reads a sketch parameter back out of a polygon or a triangle.
//
// The rectangle is parametric on purpose
// --------------------------------------
// It could have been four lines. It is not, because "make it 40 mm wide" is an
// edit a user makes to a RECTANGLE, and a rectangle stored as four unrelated
// lines has no width to edit -- only four endpoints that have to be moved in
// concert by code that would then be guessing which lines belong together.
// Its four edges are generated. R0's rectangle is axis-aligned in sketch
// space; a rotated rectangle is a parameter this stage does not add.
//
// Fail closed, and say why
// ------------------------
// Every refusal is a named `CadStatus`. Nothing is repaired: an open chain is
// not closed for the user, a self-intersecting loop is not untangled, a
// zero-length edge is not dropped. A profile either IS closed, simple and of
// non-zero area, or it is refused with the reason.
#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_workplane.h"

namespace forgeshape {

// ---------------------------------------------------------------------------
// The one status vocabulary for the whole CAD domain
// ---------------------------------------------------------------------------
//
// Sketch validation, profile extraction, triangulation and extrusion all refuse
// with a value from this ONE closed enum, so a log line, a test and the shell
// can name the exact reason without a second table mapping between three
// enums. Every value but Ok is a refusal, and a refusal changes nothing.
enum class CadStatus : uint8_t {
    Ok,
    // A coordinate, a size or a depth is NaN or infinite.
    NonFinite,
    // A coordinate is outside the bounded sketch range.
    OutOfRange,
    // A line's two endpoints coincide.
    ZeroLengthLine,
    // Two consecutive polyline vertices coincide.
    DuplicateEdge,
    // A polyline has fewer than two vertices, or a closed one fewer than three.
    TooFewVertices,
    // A rectangle's width or height is not a usable length.
    ZeroSizeRectangle,
    // A circle's radius is not a usable length.
    InvalidCircleRadius,
    // More entities, or more polyline vertices, than the sketch may hold.
    TooManyEntities,
    // An entity id the sketch does not carry, or one that is not unique.
    UnknownEntity,
    // The workplane code is not one of the three.
    InvalidWorkplane,
    // A chain of lines that does not close.
    OpenProfile,
    // A loop whose non-adjacent edges cross.
    SelfIntersectingProfile,
    // A loop whose polygon has no area.
    ZeroAreaProfile,
    // A line whose endpoint meets more than one other line: the chain forks
    // and no single loop can be read from it.
    BranchingChain,
    // The sketch closes no profile at all.
    NoClosedProfile,
    // The sketch closes more than one profile and none was chosen.
    AmbiguousProfile,
    // The chosen profile identity names no closed profile.
    ProfileNotFound,
    // A closed profile lies inside another. Holes are not R0.
    NestedProfileUnsupported,
    // The extrusion depth is not a usable length.
    InvalidExtrudeDepth,
    // The extrusion direction code is not one of the two.
    InvalidExtrudeDirection,
    // The polygon could not be triangulated within the bounded pass.
    TriangulationFailed,
    // Validated parameters still produced a mesh the runtime refused.
    RegenerationFailed,
    // The act is refused while no sketch session is open, or while the session
    // is in a state that cannot accept it.
    NotSketching,
    // A body that is not a CAD body was asked for a CAD edit.
    NotCadBody,
    // A sketch edit was refused while a Construction edit is open.
    RefusedEditInProgress,
};

constexpr int kCadStatusCount = 27;

const char* cadStatusName(CadStatus status);
int cadStatusCode(CadStatus status);
bool cadStatusFromCode(int code, CadStatus* out);

// ---------------------------------------------------------------------------
// Bounds
// ---------------------------------------------------------------------------

// Sketch coordinates are bounded so every size computation and every float
// conversion downstream is provably finite. Not a product limit on how large
// a part may be: it is far beyond anything the editor can frame.
constexpr double kMaxSketchCoordinateMeters = 1.0e5;

// Below this, two sketch points are the SAME point: a line this short has no
// direction, an edge this short is a duplicate vertex, and two chain endpoints
// this close are joined. One micrometre, three orders of magnitude below the
// finest length a user can type in millimetres to three places.
constexpr double kSketchCoincidenceMeters = 1.0e-6;

// A profile whose absolute area is below this is refused as zero-area. One
// square micrometre: the coincidence tolerance squared.
constexpr double kMinProfileAreaSquareMeters = 1.0e-12;

// How many entities one sketch may carry, and how many vertices one polyline
// may. Both are what makes a history snapshot of a CAD body BOUNDED, and both
// are far above what a finger draws in one sketch.
constexpr uint32_t kMaxSketchEntities = 256;
constexpr uint32_t kMaxPolylineVertices = 256;

// How many segments a circle profile is tessellated into. The SAME count every
// round Construction primitive uses, so a circle extruded here has exactly the
// cylinder's silhouette -- and, like there, divisible by four so the four
// cardinal points land exactly on the radius. Fixed and not a parameter: the
// circle's truth is its radius, and the polygon is regenerated from it.
constexpr uint32_t kSketchCircleSegments = kPrimitiveRadialSegments;

// The largest polygon a profile may become: a polyline at its vertex cap, or a
// chain of lines at the entity cap. Bounds the triangulation pass.
constexpr uint32_t kMaxProfileVertices = kMaxSketchEntities;

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------

// Per-sketch identity. Minted by the sketch, monotonic, never reused within a
// sketch -- so a profile selection that names an entity still names the same
// entity after others are deleted. Zero is "no entity". It is deliberately
// NOT an ObjectId: a sketch element is not a scene body and must not be
// listable, selectable or deletable as one.
using SketchEntityId = uint32_t;
constexpr SketchEntityId kNoSketchEntity = 0;

enum class SketchEntityKind : uint8_t {
    Line,
    Polyline,
    Rectangle,
    Circle,
};

const char* sketchEntityKindName(SketchEntityKind kind);

struct SketchLine {
    SketchPoint start;
    SketchPoint end;
};

struct SketchPolyline {
    std::vector<SketchPoint> vertices;
    // Explicitly closed: the last vertex joins back to the first. A polyline
    // whose last vertex merely coincides with its first is ALSO read as closed
    // by the profile engine, but only this flag is truth.
    bool closed = false;
};

// Axis-aligned in sketch space, described by its centre and its two sizes.
struct SketchRectangle {
    SketchPoint center;
    Meters width = 0.0;
    Meters height = 0.0;
};

struct SketchCircle {
    SketchPoint center;
    Meters radius = 0.0;
};

// One entity: an identity plus exactly one geometry. A real tagged union
// rather than a struct carrying every payload at once, for the same reason
// `PrimitiveSpec` is one: an entity that IS a circle physically carries no
// rectangle width for anything to misread.
class SketchEntity {
public:
    using Payload = std::variant<SketchLine, SketchPolyline, SketchRectangle, SketchCircle>;

    SketchEntity() = default;
    SketchEntity(SketchEntityId id, Payload payload) : id_(id), payload_(std::move(payload)) {}

    SketchEntityId id() const { return id_; }
    SketchEntityKind kind() const { return static_cast<SketchEntityKind>(payload_.index()); }

    const SketchLine* line() const { return std::get_if<SketchLine>(&payload_); }
    const SketchPolyline* polyline() const { return std::get_if<SketchPolyline>(&payload_); }
    const SketchRectangle* rectangle() const { return std::get_if<SketchRectangle>(&payload_); }
    const SketchCircle* circle() const { return std::get_if<SketchCircle>(&payload_); }

    const Payload& payload() const { return payload_; }
    Payload& payload() { return payload_; }

private:
    SketchEntityId id_ = kNoSketchEntity;
    Payload payload_{SketchLine{}};
};

// Bit-exact equality, for the history and the codec: a coordinate one ULP
// away is a different sketch.
bool sameSketchEntity(const SketchEntity& a, const SketchEntity& b);

// Validates one entity's own geometry against the bounds above. Says nothing
// about whether it closes anything.
CadStatus validateSketchEntity(const SketchEntity& entity);

// ---------------------------------------------------------------------------
// The sketch
// ---------------------------------------------------------------------------

// A workplane, an entity list and the id allocator's high-water mark. Plain
// data, copyable, comparable: it is what a history step and a `.forge` record
// hold for a CAD body, so it carries nothing derived.
struct CadSketch {
    Workplane plane = Workplane::XY;
    std::vector<SketchEntity> entities;
    // The id the NEXT entity will be given. Stored, because a reopened sketch
    // must not mint an id one of its own entities is wearing.
    SketchEntityId nextEntityId = 1;
};

bool sameCadSketch(const CadSketch& a, const CadSketch& b);

// Every per-entity rule, plus the sketch-level ones: the workplane is one of
// the three, no id is zero, none repeats, every id is below the high-water
// mark, and the counts are within bounds. Says nothing about profiles.
CadStatus validateCadSketch(const CadSketch& sketch);

// Appends an entity, minting its id. Refuses (returning the reason, appending
// nothing, minting nothing) when the entity or the resulting sketch would be
// invalid. `outId` receives the minted id on success.
CadStatus addSketchEntity(CadSketch* sketch, SketchEntity::Payload payload,
                          SketchEntityId* outId = nullptr);

// Replaces one entity's geometry, keeping its id. Refuses on an unknown id or
// invalid geometry, changing nothing.
CadStatus replaceSketchEntity(CadSketch* sketch, SketchEntityId id, SketchEntity::Payload payload);

// Removes one entity. Refuses an unknown id, changing nothing. Never touches
// the id allocator: a deleted id is never reused.
CadStatus removeSketchEntity(CadSketch* sketch, SketchEntityId id);

const SketchEntity* findSketchEntity(const CadSketch& sketch, SketchEntityId id);

// ---------------------------------------------------------------------------
// Closed profiles
// ---------------------------------------------------------------------------

// One closed, simple, counter-clockwise polygon read out of the sketch, and
// the entity that IDENTIFIES it.
//
// `anchorEntityId` is how a profile is named across edits and across a file:
// a rectangle or a circle is its own anchor, a closed polyline is its own, and
// a loop of lines is anchored by the smallest id among them. An index into the
// profile list would move when an unrelated entity was deleted; an entity id
// does not.
struct ClosedProfile {
    SketchEntityId anchorEntityId = kNoSketchEntity;
    // Counter-clockwise in (u, v), with no repeated closing vertex.
    std::vector<SketchPoint> polygon;
    // Positive, in square metres.
    double area = 0.0;
    // True when the polygon is a circle's tessellation. Presentation only: the
    // extruder treats every profile as a polygon.
    bool fromCircle = false;
    // Every entity that takes part, ascending. One for a rectangle, a circle
    // or a polyline; several for a chain of lines.
    std::vector<SketchEntityId> memberEntityIds;
};

// Why one candidate loop was NOT a profile, by the entity that anchors it.
struct ProfileRejection {
    SketchEntityId anchorEntityId = kNoSketchEntity;
    CadStatus why = CadStatus::Ok;
};

struct ProfileExtraction {
    // Ascending by anchor id: the deterministic order every caller sees.
    std::vector<ClosedProfile> profiles;
    std::vector<ProfileRejection> rejections;
};

// Reads every closed profile out of a VALID sketch.
//
// A rectangle and a circle each close one profile by construction. A polyline
// closes one when it is flagged closed or its last vertex coincides with its
// first; an open polyline is reported as `OpenProfile`. Lines are chained by
// coincident endpoints: a component in which every endpoint meets exactly one
// other line is one loop, a component with a free end is `OpenProfile`, and
// one with a fork is `BranchingChain`.
//
// Every loop is then held to the same rules: at least three vertices, no
// duplicate consecutive edge, non-zero area, and no crossing between
// non-adjacent edges. A loop that fails is listed under `rejections` with the
// reason, never repaired. A profile that CONTAINS another profile is rejected
// as `NestedProfileUnsupported` -- a hole this stage will not fill silently --
// while the inner one stays extrudable on its own.
//
// Bounded: at most kMaxSketchEntities loops, each at most kMaxProfileVertices
// long, and the crossing test is a plain O(n^2) over a bounded n.
ProfileExtraction extractClosedProfiles(const CadSketch& sketch);

const ClosedProfile* findClosedProfile(const ProfileExtraction& extraction,
                                       SketchEntityId anchorEntityId);

// ---------------------------------------------------------------------------
// Triangulation
// ---------------------------------------------------------------------------

// Ear clipping over a simple counter-clockwise polygon.
//
// Deterministic: the same polygon always yields the same triangles, in the
// same order. Bounded: at most n - 2 ears are clipped and each search is over
// at most n candidates, so the whole pass is O(n^2) with n <= kMaxProfileVertices
// and cannot loop. A polygon it cannot finish -- which a simple polygon never
// is, but a numerically degenerate one may be -- is refused as
// `TriangulationFailed` with nothing written. Triangles keep the polygon's
// counter-clockwise orientation.
CadStatus triangulateSimplePolygon(const std::vector<SketchPoint>& polygon,
                                   std::vector<uint32_t>* outIndices);

// Twice the signed area: positive for counter-clockwise.
double polygonSignedAreaTwice(const std::vector<SketchPoint>& polygon);

// Whether two closed segments cross or touch. Exposed for the self-tests.
bool sketchSegmentsIntersect(const SketchPoint& a0, const SketchPoint& a1,
                             const SketchPoint& b0, const SketchPoint& b1);

// The tessellated polygon for a circle, counter-clockwise, starting at +U.
// The four cardinal vertices are exact.
std::vector<SketchPoint> circleProfilePolygon(const SketchCircle& circle);

// The four corners of a rectangle, counter-clockwise from the (-w/2, -h/2)
// corner.
std::vector<SketchPoint> rectangleProfilePolygon(const SketchRectangle& rectangle);

}  // namespace forgeshape
