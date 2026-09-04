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
#include "forgeshape_object_id.h"
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
    // An arc's three authored points do not describe an arc: two coincide, or
    // all three are collinear and no finite circle passes through them.
    InvalidArc,
    // A spline's authored points do not describe a curve: fewer than two, or
    // two consecutive ones coincide.
    InvalidSpline,
    // The support plane of a sketch that already carries geometry may not be
    // changed, because the authored (u, v) values would silently come to mean
    // somewhere else. Refused by name rather than remapped (`CADUXR1-15`).
    SketchNotEmpty,
    // A sketch edit would remove a planar face another body's sketch is
    // standing on. Refused on exactly the terms deleting such a producer is
    // (`RefusedHasDependents`): the dependency is never cascaded, never
    // retargeted to the nearest surviving face, and never silently broken.
    DependentFaceLost,
};

constexpr int kCadStatusCount = 31;

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

// ---------------------------------------------------------------------------
// Curves (`SKETCH-UX-R1`)
// ---------------------------------------------------------------------------
//
// An Arc and a Spline are AUTHORED as a bounded set of points and DERIVED into
// a polyline when a profile needs one. The tessellation is a pure function of
// the authored points and these constants -- never of the camera, the zoom or
// the window -- because a profile that changed shape when the user pinched
// would make the extruded solid depend on how the sketch was looked at.

// How many points a spline may carry. Bounded for the same reason a polyline's
// vertices are: it is what keeps a history step and a `.forge` record finite.
constexpr uint32_t kMaxSplinePoints = 32;

// A spline span (between two consecutive authored points) becomes this many
// straight segments. Fixed, so the same authored points always yield the same
// polygon.
constexpr uint32_t kSplineSegmentsPerSpan = 8;

// An arc's segment count is derived from its swept angle at the SAME angular
// density a full circle gets (kSketchCircleSegments over 2*pi), then clamped,
// so a quarter arc and a quarter of a circle are tessellated alike.
constexpr uint32_t kMinArcSegments = 4;
constexpr uint32_t kMaxArcSegments = 64;

// The largest polygon a profile may become. A chain of curves contributes many
// polygon vertices per entity, so this is no longer the entity cap: it is the
// bound the triangulation pass and every O(n^2) check here are held to.
constexpr uint32_t kMaxProfileVertices = 1024;

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

// ---------------------------------------------------------------------------
// Semantic CAD topology (`CAD-A3`, `ARCH-OWNER-13`)
// ---------------------------------------------------------------------------
//
// A sketch may be supported by a world plane, as in `CAD-R0`, OR by a PLANAR
// FACE of another CAD body. The identity of that face is SEMANTIC — feature
// lineage, never a render-triangle index — so that it survives save/open, a
// parent parameter edit, undo/redo and regeneration.

// Which planar face of an extrusion. The two caps are always eligible; a side
// face is eligible for a rectangle, a closed polyline and a line chain (one
// planar face per profile edge) but NOT for a circle, whose side is cylindrical.
enum class CadFaceKind : uint8_t {
    // The cap lying ON the producing sketch's support plane (offset 0).
    CapPlane,
    // The cap at the far end of the extrusion (offset +depth from the plane).
    CapFar,
    // One planar side face, identified by the profile edge that produced it.
    Side,
};

const char* cadFaceKindName(CadFaceKind kind);

// A compact, stable identity of one face within a producer feature's topology.
//
// For a Side, `edgeEntityId` is the sketch entity that owns the profile edge
// and `edgeLocalIndex` is which of that entity's edges it is (0..3 for a
// rectangle, 0..n-1 for a polyline, 0 for a chained line). For a cap both are
// zero. It is NEVER a triangle index.
struct CadFaceToken {
    CadFaceKind kind = CadFaceKind::CapPlane;
    SketchEntityId edgeEntityId = kNoSketchEntity;
    uint32_t edgeLocalIndex = 0;
};

bool sameCadFaceToken(const CadFaceToken& a, const CadFaceToken& b);
// A stable u64 encoding, used to build a lineage signature and to compare.
uint64_t cadFaceTokenCode(const CadFaceToken& token);

// The v1 CAD feature id every CAD body's single Sketch+Extrude feature has,
// mirrored from the codec's `kPrimitiveSourceFeatureId` so this header does not
// depend on the codec.
constexpr uint32_t kCadFeatureId = 1;

// A persistent reference to a producer feature's face (`ARCH-OWNER-13`).
//
// `lineageToken` is a deterministic signature of the producer's face topology
// at the time the reference was made. A supported parameter edit (a rectangle's
// size, a circle's radius, a depth, a direction) does not change the set of
// faces and so keeps the token valid; a change that removed the referenced face
// would change the signature and the reference fails closed rather than
// silently retargeting to the nearest face.
struct TopoRef {
    ObjectId producerObjectId = kNoObject;
    uint32_t producerLocalFeatureId = kCadFeatureId;
    CadFaceToken face{};
    uint64_t lineageToken = 0;
};

bool sameTopoRef(const TopoRef& a, const TopoRef& b);

enum class SketchEntityKind : uint8_t {
    Line,
    Polyline,
    Rectangle,
    Circle,
    Arc,
    Spline,
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

// A three-point arc: it starts at `start`, passes THROUGH `mid` and ends at
// `end` (`SKETCH-UX-R1` D1).
//
// Three points on the curve, rather than a centre, a radius and two angles,
// because that is what the gesture produces and because it has no ambiguity to
// resolve: there is exactly one circle through three non-collinear points and
// exactly one of its two arcs contains the middle point. A centre/angle form
// would have to store a sweep direction and a "major or minor" flag, and every
// edit would have to keep them consistent with the endpoints.
//
// The centre, the radius and the sweep are DERIVED (`arcGeometry`), and the
// endpoints are EXACTLY `start` and `end` -- never a tessellated approximation
// of them -- so a chain closes on the authored values and not on rounding.
struct SketchArc {
    SketchPoint start;
    SketchPoint mid;
    SketchPoint end;
};

// A bounded interpolating spline through its authored points
// (`SKETCH-UX-R1` D2).
//
// TRUTH is the point list. The curve is a Catmull-Rom interpolation converted
// to a cubic Bezier span by span (`splineTessellation`), so the curve PASSES
// THROUGH every authored point: moving one point moves the curve there, which
// is the edit a user expects. Its endpoints are exactly `points.front()` and
// `points.back()`.
//
// Deliberately not a NURBS, not a fitted stroke and not a knot vector: an
// interpolating spline through a bounded point list is deterministic, editable
// point by point, and small enough to store and to snapshot.
struct SketchSpline {
    std::vector<SketchPoint> points;
};

// One entity: an identity plus exactly one geometry. A real tagged union
// rather than a struct carrying every payload at once, for the same reason
// `PrimitiveSpec` is one: an entity that IS a circle physically carries no
// rectangle width for anything to misread.
class SketchEntity {
public:
    // The variant's alternative ORDER is `SketchEntityKind`'s order: `kind()`
    // is the variant index. New kinds are APPENDED, never inserted, so a kind
    // never changes number under code that already switched on it. The `.forge`
    // codes are separate and file-owned regardless.
    using Payload = std::variant<SketchLine, SketchPolyline, SketchRectangle, SketchCircle,
                                 SketchArc, SketchSpline>;

    SketchEntity() = default;
    SketchEntity(SketchEntityId id, Payload payload) : id_(id), payload_(std::move(payload)) {}

    SketchEntityId id() const { return id_; }
    SketchEntityKind kind() const { return static_cast<SketchEntityKind>(payload_.index()); }

    const SketchLine* line() const { return std::get_if<SketchLine>(&payload_); }
    const SketchPolyline* polyline() const { return std::get_if<SketchPolyline>(&payload_); }
    const SketchRectangle* rectangle() const { return std::get_if<SketchRectangle>(&payload_); }
    const SketchCircle* circle() const { return std::get_if<SketchCircle>(&payload_); }
    const SketchArc* arc() const { return std::get_if<SketchArc>(&payload_); }
    const SketchSpline* spline() const { return std::get_if<SketchSpline>(&payload_); }

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
// Chainable entities and their derived polylines
// ---------------------------------------------------------------------------
//
// A Line, an Arc and a Spline are OPEN entities with two ends, and the profile
// engine chains all three by coincident endpoints in exactly one walker. A
// Rectangle, a Circle and a closed Polyline close a profile on their own and
// are not chainable.

// True, and fills the two endpoints, for a chainable entity. The endpoints are
// AUTHORED values -- a line's own two points, an arc's own start and end, a
// spline's own first and last -- never sampled off a tessellation, so a chain
// joins on the numbers the user's snap produced.
bool sketchEntityEndpoints(const SketchEntity& entity, SketchPoint* outStart, SketchPoint* outEnd);

// True when the entity's derived polyline is an approximation of a CURVE rather
// than the exact edge. It decides whether the extruded side face is planar and
// therefore eligible to carry a sketch (`forgeshape_cad_face.h`), and nothing
// else: a curved side is reported so a tap on it resolves, never as a support.
bool sketchEntityIsCurved(const SketchEntity& entity);

// The derived polyline of a chainable entity, from its start to its end,
// INCLUSIVE of both, with at least two points and no consecutive duplicates.
//
// Deterministic and bounded: a Line gives exactly its two endpoints; an Arc
// gives kMinArcSegments..kMaxArcSegments segments chosen from its own swept
// angle; a Spline gives kSplineSegmentsPerSpan per authored span. None of it
// depends on a camera, a zoom or a window, because the extruded solid must not.
CadStatus tessellateSketchCurve(const SketchEntity& entity, std::vector<SketchPoint>* out);

// The circle an arc lies on. Refuses `InvalidArc` when the three points are
// collinear or two of them coincide. `outSweep` is the SIGNED swept angle from
// start to end through mid, in radians, in (-2*pi, 2*pi) and never zero.
CadStatus arcGeometry(const SketchArc& arc, SketchPoint* outCenter, double* outRadius,
                      double* outStartAngle, double* outSweep);

// ---------------------------------------------------------------------------
// The sketch
// ---------------------------------------------------------------------------

// A workplane, an entity list and the id allocator's high-water mark. Plain
// data, copyable, comparable: it is what a history step and a `.forge` record
// hold for a CAD body, so it carries nothing derived.
struct CadSketch {
    // The authoring basis. For a WORLD-plane sketch this is the chosen principal
    // plane; for a FACE-supported sketch it is always XY -- the sketch authors on
    // its own canonical local XY and the support FRAME (see forgeshape_cad_face.h)
    // places that local space onto the producer's face, so `generateCadMesh` is
    // unchanged either way.
    Workplane plane = Workplane::XY;
    std::vector<SketchEntity> entities;
    // The id the NEXT entity will be given. Stored, because a reopened sketch
    // must not mint an id one of its own entities is wearing.
    SketchEntityId nextEntityId = 1;
    // CAD-A3: where the sketch is supported. When false the support is the world
    // plane above and the body has an INDEPENDENT placement (as in CAD-R0). When
    // true the body is FACE-SUPPORTED: its placement is DERIVED from the producer
    // named by `faceSupport`, `plane` is the canonical XY, and the body may not
    // be moved independently. A CAD-R0 body has this false and is byte-identical
    // in the v1 file.
    bool hasFaceSupport = false;
    TopoRef faceSupport{};
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
    // Per polygon EDGE (edge k connects polygon[k] -> polygon[(k+1) % n]): which
    // sketch entity owns it, and which of that entity's edges it is
    // (`CAD-A3`). Parallel to `polygon`, same length. For a rectangle, a circle
    // or a polyline every entry names the anchor with the edge's own index; for
    // a line chain it names the member Line and index 0. This is what gives an
    // extruded SIDE face a stable semantic identity that is not a triangle
    // index.
    std::vector<SketchEntityId> edgeEntityId;
    std::vector<uint32_t> edgeLocalIndex;
    // Per polygon EDGE, parallel to the two above: whether that edge is one
    // segment of a CURVE's approximation rather than an exact straight edge of
    // the authored sketch (`SKETCH-UX-R1` D3). The extruded side face of such
    // an edge is a facet of a curved surface and is therefore not eligible to
    // support a sketch -- the same answer a circle's cylindrical side already
    // gets, now decided per edge because one profile may mix lines and curves.
    std::vector<uint8_t> edgeCurved;
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
