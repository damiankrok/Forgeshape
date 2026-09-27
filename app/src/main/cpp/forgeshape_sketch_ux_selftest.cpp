#include "forgeshape_sketch_ux_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_history.h"
#include "forgeshape_math.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {
namespace {

struct Recorder {
    SketchUxSelfTestResult* out;
    int max;
    int n = 0;
    void check(const char* name, bool ok) {
        if (n < max) {
            out[n].name = name;
            out[n].passed = ok;
            ++n;
        }
    }
};

std::string g_performance = "not measured";

bool near2(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }
bool nearPoint(const SketchPoint& p, double u, double v, double tol = 1e-9) {
    return near2(p.u, u, tol) && near2(p.v, v, tol);
}
bool nearVec(const Vec3& a, float x, float y, float z, float tol = 1e-5f) {
    return std::fabs(a.x - x) <= tol && std::fabs(a.y - y) <= tol && std::fabs(a.z - z) <= tol;
}

// A right-handed orthonormal check: u x v == n, all three unit.
bool rightHanded(const SketchFrame& f) {
    const Vec3 cross = vec3Cross(f.u, f.v);
    return nearVec(cross, f.n.x, f.n.y, f.n.z, 1e-4f)
           && std::fabs(vec3Dot(f.u, f.u) - 1.0f) <= 1e-4f
           && std::fabs(vec3Dot(f.v, f.v) - 1.0f) <= 1e-4f
           && std::fabs(vec3Dot(f.n, f.n) - 1.0f) <= 1e-4f;
}

SketchEntity arcEntity(SketchEntityId id, SketchPoint start, SketchPoint mid, SketchPoint end) {
    SketchArc arc;
    arc.start = start;
    arc.mid = mid;
    arc.end = end;
    return SketchEntity(id, arc);
}

SketchEntity splineEntity(SketchEntityId id, std::vector<SketchPoint> points) {
    SketchSpline spline;
    spline.points = std::move(points);
    return SketchEntity(id, spline);
}

// The canonical semicircle used throughout: centre (0,0), radius 1, from
// (1,0) through (0,1) to (-1,0). Counter-clockwise, sweep +pi.
SketchArc unitSemicircle() {
    SketchArc arc;
    arc.start = SketchPoint{1.0, 0.0};
    arc.mid = SketchPoint{0.0, 1.0};
    arc.end = SketchPoint{-1.0, 0.0};
    return arc;
}

// A closed profile made of one semicircular arc and the straight line that
// joins its two ends: a "D" shape. Exactly the mixed line/curve chain the
// stage has to extrude.
CadBodyState arcAndLineBody(double depth = 1.0) {
    CadBodyState state;
    state.sketch.plane = Workplane::XY;
    addSketchEntity(&state.sketch, unitSemicircle());
    addSketchEntity(&state.sketch, SketchLine{SketchPoint{-1.0, 0.0}, SketchPoint{1.0, 0.0}});
    state.extrude.profileEntityId = 1;  // the chain's smallest member id
    state.extrude.depth = depth;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

// A closed profile made of one spline and one line joining its ends.
//
// Every coordinate is an EXACT binary fraction, because this body is also a
// corpus fixture: 0.75 is representable to the last bit and 0.8 is not, and a
// value the two encoders could round differently is a byte-parity argument
// waiting to happen.
CadBodyState splineAndLineBody() {
    CadBodyState state;
    state.sketch.plane = Workplane::XY;
    SketchSpline spline;
    spline.points = {SketchPoint{-1.0, 0.0}, SketchPoint{-0.5, 0.75}, SketchPoint{0.5, 0.75},
                     SketchPoint{1.0, 0.0}};
    addSketchEntity(&state.sketch, spline);
    addSketchEntity(&state.sketch, SketchLine{SketchPoint{1.0, 0.0}, SketchPoint{-1.0, 0.0}});
    state.extrude.profileEntityId = 1;
    state.extrude.depth = 0.5;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

// A plain rectangle body, for the edit-session and dependency cases.
CadBodyState rectBody(double w, double h, double depth) {
    CadBodyState state;
    state.sketch.plane = Workplane::XY;
    SketchRectangle r;
    r.center = SketchPoint{0.0, 0.0};
    r.width = w;
    r.height = h;
    addSketchEntity(&state.sketch, r);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = depth;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

// -------------------------------------------------------------------------
// The curve domain (`CADUXR1-25..31`)
// -------------------------------------------------------------------------

void testArcDomain(Recorder& r) {
    // The derived circle is exact for a semicircle on the unit circle.
    {
        SketchPoint centre;
        double radius = 0.0;
        double startAngle = 0.0;
        double sweep = 0.0;
        const CadStatus why = arcGeometry(unitSemicircle(), &centre, &radius, &startAngle, &sweep);
        r.check("CADUXR1_25_arc_geometry_is_the_circle_through_its_three_points",
                why == CadStatus::Ok && nearPoint(centre, 0.0, 0.0, 1e-12)
                        && near2(radius, 1.0, 1e-12) && near2(startAngle, 0.0, 1e-12)
                        && near2(sweep, 3.14159265358979323846, 1e-12));
    }
    // The other way round the circle: the middle point alone decides, and the
    // sweep comes back negative rather than as the same arc's mirror.
    {
        SketchArc arc;
        arc.start = SketchPoint{1.0, 0.0};
        arc.mid = SketchPoint{0.0, -1.0};
        arc.end = SketchPoint{-1.0, 0.0};
        double sweep = 0.0;
        const CadStatus why = arcGeometry(arc, nullptr, nullptr, nullptr, &sweep);
        r.check("CADUXR1_25_the_middle_point_decides_which_arc_was_drawn",
                why == CadStatus::Ok && sweep < 0.0
                        && near2(sweep, -3.14159265358979323846, 1e-12));
    }
    // Collinear and coincident are refused BY NAME and never straightened.
    {
        const SketchEntity flat = arcEntity(1, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 0.0},
                                            SketchPoint{2.0, 0.0});
        const SketchEntity doubled = arcEntity(1, SketchPoint{0.0, 0.0}, SketchPoint{0.0, 0.0},
                                               SketchPoint{2.0, 1.0});
        r.check("CADUXR1_25_a_collinear_or_degenerate_arc_is_refused_by_name",
                validateSketchEntity(flat) == CadStatus::InvalidArc
                        && validateSketchEntity(doubled) == CadStatus::InvalidArc);
    }
    // A far-from-origin collinear triple is refused too: the test is scaled by
    // the triangle's own extent, not an absolute epsilon.
    {
        const SketchEntity far = arcEntity(1, SketchPoint{1000.0, 1000.0},
                                           SketchPoint{1001.0, 1000.0},
                                           SketchPoint{1002.0, 1000.0});
        r.check("CADUXR1_25_collinearity_is_scaled_not_absolute",
                validateSketchEntity(far) == CadStatus::InvalidArc);
    }
    // The tessellation: exact endpoints, on the circle, bounded, deterministic.
    {
        const SketchEntity arc(1, unitSemicircle());
        std::vector<SketchPoint> a;
        std::vector<SketchPoint> b;
        const bool ok = tessellateSketchCurve(arc, &a) == CadStatus::Ok
                        && tessellateSketchCurve(arc, &b) == CadStatus::Ok;
        bool identical = ok && a.size() == b.size();
        for (size_t i = 0; identical && i < a.size(); ++i) {
            identical = a[i].u == b[i].u && a[i].v == b[i].v;
        }
        bool onCircle = ok;
        for (size_t i = 0; onCircle && i < a.size(); ++i) {
            onCircle = near2(std::sqrt(a[i].u * a[i].u + a[i].v * a[i].v), 1.0, 1e-12);
        }
        r.check("CADUXR1_31_arc_tessellation_is_exact_bounded_and_deterministic",
                ok && identical && onCircle && a.size() >= kMinArcSegments + 1
                        && a.size() <= kMaxArcSegments + 1
                        && nearPoint(a.front(), 1.0, 0.0, 0.0)
                        && nearPoint(a.back(), -1.0, 0.0, 0.0));
    }
    // Half the sweep, about half the segments: the density is the circle's.
    {
        SketchArc quarter;
        quarter.start = SketchPoint{1.0, 0.0};
        quarter.mid = SketchPoint{0.70710678118654752, 0.70710678118654752};
        quarter.end = SketchPoint{0.0, 1.0};
        std::vector<SketchPoint> q;
        std::vector<SketchPoint> h;
        tessellateSketchCurve(SketchEntity(1, quarter), &q);
        tessellateSketchCurve(SketchEntity(1, unitSemicircle()), &h);
        r.check("CADUXR1_31_arc_segment_count_follows_the_swept_angle",
                q.size() < h.size() && q.size() >= kMinArcSegments + 1);
    }
    // The endpoints a chain joins on are the AUTHORED values, bit for bit.
    {
        const SketchEntity arc(1, unitSemicircle());
        SketchPoint start;
        SketchPoint end;
        r.check("CADUXR1_27_an_arc_chains_on_its_authored_endpoints",
                sketchEntityEndpoints(arc, &start, &end) && start.u == 1.0 && start.v == 0.0
                        && end.u == -1.0 && end.v == 0.0 && sketchEntityIsCurved(arc));
    }
}

void testSplineDomain(Recorder& r) {
    // Too few points, a repeated point and a closed run are all refused.
    {
        const SketchEntity one = splineEntity(1, {SketchPoint{0.0, 0.0}});
        const SketchEntity dup = splineEntity(
                1, {SketchPoint{0.0, 0.0}, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}});
        const SketchEntity closed = splineEntity(
                1, {SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}, SketchPoint{0.0, 0.0}});
        r.check("CADUXR1_28_a_spline_needs_two_distinct_points_and_two_ends",
                validateSketchEntity(one) == CadStatus::InvalidSpline
                        && validateSketchEntity(dup) == CadStatus::InvalidSpline
                        && validateSketchEntity(closed) == CadStatus::InvalidSpline);
    }
    // The point cap is a bound, not a suggestion.
    {
        std::vector<SketchPoint> many;
        for (uint32_t i = 0; i <= kMaxSplinePoints; ++i) {
            many.push_back(SketchPoint{static_cast<double>(i), 0.5 * static_cast<double>(i % 3)});
        }
        r.check("CADUXR1_28_a_spline_past_its_point_cap_is_refused",
                validateSketchEntity(splineEntity(1, many)) == CadStatus::TooManyEntities);
    }
    // The curve PASSES THROUGH every authored point, which is the whole reason
    // for choosing an interpolating spline: moving a point moves the curve
    // there. Deterministic and bounded with it.
    {
        const std::vector<SketchPoint> pts = {SketchPoint{0.0, 0.0}, SketchPoint{1.0, 2.0},
                                              SketchPoint{2.0, -1.0}, SketchPoint{3.0, 0.5}};
        const SketchEntity spline = splineEntity(1, pts);
        std::vector<SketchPoint> a;
        std::vector<SketchPoint> b;
        const bool ok = tessellateSketchCurve(spline, &a) == CadStatus::Ok
                        && tessellateSketchCurve(spline, &b) == CadStatus::Ok;
        bool identical = ok && a.size() == b.size();
        for (size_t i = 0; identical && i < a.size(); ++i) {
            identical = a[i].u == b[i].u && a[i].v == b[i].v;
        }
        bool interpolates = ok;
        for (size_t i = 0; interpolates && i < pts.size(); ++i) {
            const size_t at = i * kSplineSegmentsPerSpan;
            interpolates = at < a.size() && a[at].u == pts[i].u && a[at].v == pts[i].v;
        }
        r.check("CADUXR1_29_spline_tessellation_interpolates_and_is_deterministic",
                ok && identical && interpolates
                        && a.size() == (pts.size() - 1) * kSplineSegmentsPerSpan + 1);
    }
    {
        const SketchEntity spline = splineEntity(
                1, {SketchPoint{-1.0, 0.0}, SketchPoint{0.0, 1.0}, SketchPoint{1.0, 0.0}});
        SketchPoint start;
        SketchPoint end;
        r.check("CADUXR1_30_a_spline_chains_on_its_first_and_last_authored_points",
                sketchEntityEndpoints(spline, &start, &end) && start.u == -1.0 && end.u == 1.0
                        && sketchEntityIsCurved(spline));
    }
}

void testCurveProfiles(Recorder& r) {
    // An arc and a line closing one profile, and it extrudes.
    {
        const CadBodyState state = arcAndLineBody();
        const ProfileExtraction extraction = extractClosedProfiles(state.sketch);
        const ClosedProfile* profile = extraction.profiles.empty()
                                           ? nullptr
                                           : &extraction.profiles.front();
        ConstructionMesh mesh;
        const CadStatus generated = generateCadMesh(state, &mesh);
        // Half a unit circle: pi/2, and the closing chord contributes nothing.
        const bool area = profile != nullptr
                          && std::fabs(profile->area - 1.5707963267948966) < 0.02;
        r.check("CADUXR1_27_an_arc_and_a_line_close_one_profile_and_extrude",
                extraction.profiles.size() == 1 && profile != nullptr && area
                        && profile->memberEntityIds.size() == 2 && generated == CadStatus::Ok
                        && mesh.vertices.size() > 0);
    }
    // A spline and a line likewise.
    {
        const CadBodyState state = splineAndLineBody();
        const ProfileExtraction extraction = extractClosedProfiles(state.sketch);
        ConstructionMesh mesh;
        r.check("CADUXR1_30_a_spline_and_a_line_close_one_profile_and_extrude",
                extraction.profiles.size() == 1
                        && extraction.profiles.front().memberEntityIds.size() == 2
                        && generateCadMesh(state, &mesh) == CadStatus::Ok);
    }
    // An arc whose free end meets nothing is an OPEN profile, not a repaired
    // one -- exactly what a lone line already is.
    {
        CadSketch sketch;
        addSketchEntity(&sketch, unitSemicircle());
        const ProfileExtraction extraction = extractClosedProfiles(sketch);
        r.check("CADUXR1_27_a_lone_arc_is_an_open_profile_not_a_closed_one",
                extraction.profiles.empty() && extraction.rejections.size() == 1
                        && extraction.rejections.front().why == CadStatus::OpenProfile);
    }
    // The mixed profile's SIDE faces: the line's is planar and eligible to
    // carry a sketch; every arc facet is reported so a tap resolves, and none
    // of them is eligible.
    {
        const CadBodyState state = arcAndLineBody();
        std::vector<CadFace> faces;
        const CadStatus why = enumerateCadFaces(state, &faces);
        int eligibleSides = 0;
        int curvedSides = 0;
        for (const CadFace& face : faces) {
            if (face.token.kind != CadFaceKind::Side) continue;
            if (face.eligible) {
                ++eligibleSides;
            } else {
                ++curvedSides;
            }
        }
        bool capsEligible = faces.size() >= 2 && faces[0].eligible && faces[1].eligible;
        r.check("CADUXR1_27_a_curved_side_is_never_eligible_and_a_line_side_is",
                why == CadStatus::Ok && capsEligible && eligibleSides == 1 && curvedSides > 1);
    }
    // Determinism at the level that matters: the same authored state produces
    // the same polygon, twice, value for value.
    {
        const CadBodyState state = arcAndLineBody();
        const ProfileExtraction a = extractClosedProfiles(state.sketch);
        const ProfileExtraction b = extractClosedProfiles(state.sketch);
        bool same = a.profiles.size() == 1 && b.profiles.size() == 1
                    && a.profiles[0].polygon.size() == b.profiles[0].polygon.size();
        for (size_t i = 0; same && i < a.profiles[0].polygon.size(); ++i) {
            same = a.profiles[0].polygon[i].u == b.profiles[0].polygon[i].u
                   && a.profiles[0].polygon[i].v == b.profiles[0].polygon[i].v;
        }
        r.check("CADUXR1_31_a_curve_profile_is_bit_identical_between_runs", same);
    }
    // The bound holds: a chain long enough to exceed kMaxProfileVertices is
    // refused rather than triangulated.
    {
        CadSketch sketch;
        // Splines at the point cap, chained end to end. Each contributes
        // (points - 1) * segments edges, so a few of them pass the ceiling.
        double x = 0.0;
        for (int i = 0; i < 8; ++i) {
            std::vector<SketchPoint> pts;
            for (uint32_t k = 0; k < kMaxSplinePoints; ++k) {
                pts.push_back(SketchPoint{x + static_cast<double>(k) * 0.01,
                                          (k % 2 == 0) ? 0.0 : 0.005});
            }
            x = pts.back().u;
            addSketchEntity(&sketch, splineEntity(1, pts).spline() != nullptr
                                             ? SketchEntity::Payload(SketchSpline{pts})
                                             : SketchEntity::Payload(SketchSpline{pts}));
        }
        // Close the run with one line back to the start.
        addSketchEntity(&sketch, SketchLine{SketchPoint{x, 0.0}, SketchPoint{0.0, 0.0}});
        const ProfileExtraction extraction = extractClosedProfiles(sketch);
        bool boundedRefusal = extraction.profiles.empty();
        for (const ClosedProfile& profile : extraction.profiles) {
            boundedRefusal = boundedRefusal || profile.polygon.size() <= kMaxProfileVertices;
        }
        r.check("CADUXR1_31_a_curve_chain_past_the_polygon_bound_is_refused_not_grown",
                boundedRefusal && !extraction.rejections.empty());
    }
}

// -------------------------------------------------------------------------
// The exact line-length edit (`CADUXR1-17..24`)
// -------------------------------------------------------------------------

void testLineDimension(Recorder& r) {
    SketchSession session;
    session.begin(Workplane::XY);
    SketchEntityId lineId = kNoSketchEntity;
    addSketchEntity(const_cast<CadSketch*>(&session.sketch()),
                    SketchLine{SketchPoint{1.0, 1.0}, SketchPoint{4.0, 5.0}}, &lineId);
    session.select(lineId);

    // A 3-4-5 triangle: the reported length is exactly 5.
    {
        Meters length = 0.0;
        r.check("CADUXR1_18_a_selected_line_reports_its_exact_length",
                session.selectedLineLength(&length) && near2(length, 5.0, 1e-12));
    }
    // The edit: P0 fixed, direction preserved, P1 exactly `length` away.
    {
        const CadStatus why = session.applyLineLength(lineId, 10.0);
        const SketchLine* line = findSketchEntity(session.sketch(), lineId)->line();
        const double du = line->end.u - line->start.u;
        const double dv = line->end.v - line->start.v;
        r.check("CADUXR1_20_21_22_a_typed_length_keeps_P0_and_the_direction",
                why == CadStatus::Ok && nearPoint(line->start, 1.0, 1.0, 0.0)
                        && near2(std::sqrt(du * du + dv * dv), 10.0, 1e-12)
                        // 3-4-5 doubled twice over: the direction is unchanged.
                        && near2(du / dv, 3.0 / 4.0, 1e-12) && du > 0.0 && dv > 0.0);
    }
    // Every invalid length is refused and mutates nothing.
    {
        const SketchLine before = *findSketchEntity(session.sketch(), lineId)->line();
        const CadStatus zero = session.applyLineLength(lineId, 0.0);
        const CadStatus negative = session.applyLineLength(lineId, -3.0);
        const CadStatus nan = session.applyLineLength(lineId,
                                                      std::numeric_limits<double>::quiet_NaN());
        const CadStatus infinite = session.applyLineLength(
                lineId, std::numeric_limits<double>::infinity());
        const CadStatus huge = session.applyLineLength(lineId, kMaxSketchCoordinateMeters * 10.0);
        const SketchLine after = *findSketchEntity(session.sketch(), lineId)->line();
        r.check("CADUXR1_23_an_invalid_typed_length_is_refused_and_changes_nothing",
                zero != CadStatus::Ok && negative != CadStatus::Ok && nan == CadStatus::NonFinite
                        && infinite == CadStatus::NonFinite && huge != CadStatus::Ok
                        && after.start.u == before.start.u && after.start.v == before.start.v
                        && after.end.u == before.end.u && after.end.v == before.end.v);
    }
    // Only a straight Line has a length to show.
    {
        SketchSession other;
        other.begin(Workplane::XY);
        SketchEntityId circleId = kNoSketchEntity;
        SketchCircle circle;
        circle.center = SketchPoint{0.0, 0.0};
        circle.radius = 1.0;
        addSketchEntity(const_cast<CadSketch*>(&other.sketch()), circle, &circleId);
        other.select(circleId);
        Meters length = 0.0;
        r.check("CADUXR1_17_only_a_straight_line_carries_a_dimension",
                !other.selectedLineLength(&length)
                        && other.applyLineLength(circleId, 2.0) == CadStatus::UnknownEntity);
    }
    // The label anchor stands OFF the stroke, by the annotation offset.
    {
        SketchPoint anchor;
        const double unit = 0.01;
        const bool ok = session.selectedLineDimensionAnchor(unit, &anchor);
        const SketchLine* line = findSketchEntity(session.sketch(), lineId)->line();
        const SketchPoint mid{(line->start.u + line->end.u) * 0.5,
                              (line->start.v + line->end.v) * 0.5};
        const double away = std::sqrt((anchor.u - mid.u) * (anchor.u - mid.u)
                                      + (anchor.v - mid.v) * (anchor.v - mid.v));
        r.check("CADUXR1_17_the_dimension_label_stands_clear_of_the_stroke",
                ok && near2(away, kSketchDimensionOffsetUnits * unit, 1e-9));
    }
    // A length edit that opens a chain is allowed as an EDIT STATE, and the
    // profile is then refused by name rather than repaired.
    {
        SketchSession chain;
        chain.begin(Workplane::XY);
        CadSketch* sketch = const_cast<CadSketch*>(&chain.sketch());
        SketchEntityId first = kNoSketchEntity;
        addSketchEntity(sketch, SketchLine{SketchPoint{0.0, 0.0}, SketchPoint{2.0, 0.0}}, &first);
        addSketchEntity(sketch, SketchLine{SketchPoint{2.0, 0.0}, SketchPoint{1.0, 2.0}});
        addSketchEntity(sketch, SketchLine{SketchPoint{1.0, 2.0}, SketchPoint{0.0, 0.0}});
        const bool closedBefore = extractClosedProfiles(chain.sketch()).profiles.size() == 1;
        const CadStatus edited = chain.applyLineLength(first, 1.0);
        const CadStatus finished = chain.finish();
        r.check("CADUXR1_24_a_length_edit_may_open_a_profile_and_extrude_then_refuses",
                closedBefore && edited == CadStatus::Ok && finished == CadStatus::OpenProfile
                        && chain.state() == SketchSessionState::Editing);
    }
}

// -------------------------------------------------------------------------
// The orientation navigator (`CADUXR1-08..16`)
// -------------------------------------------------------------------------

void testOrientationNavigator(Recorder& r) {
    SketchSession session;
    session.begin(Workplane::XY);

    // The default: XY seen along +Z, the right way up, and the view frame is
    // the authoring frame untouched.
    {
        const SketchFrame view = session.viewFrame();
        r.check("CADUXR1_06_08_a_new_sketch_is_XY_seen_along_plus_Z",
                session.plane() == Workplane::XY && !session.viewFlipped()
                        && session.viewQuarterTurns() == 0 && nearVec(view.n, 0.0f, 0.0f, 1.0f)
                        && nearVec(view.u, 1.0f, 0.0f, 0.0f) && nearVec(view.v, 0.0f, 1.0f, 0.0f)
                        && rightHanded(view));
    }
    // Flipping looks at the back of the plane. The normal reverses, the basis
    // stays right-handed, and NOTHING is mirrored.
    {
        session.setViewFlipped(true);
        const SketchFrame view = session.viewFrame();
        r.check("CADUXR1_08_flipping_the_normal_stays_right_handed",
                session.viewFlipped() && nearVec(view.n, 0.0f, 0.0f, -1.0f)
                        && nearVec(view.u, 1.0f, 0.0f, 0.0f)
                        && nearVec(view.v, 0.0f, -1.0f, 0.0f) && rightHanded(view));
        session.setViewFlipped(false);
    }
    // A quarter turn: the VIEW turns clockwise, so what was at screen-up ends
    // up at screen-left and the drawing appears to turn counter-clockwise.
    // Exact, and still right-handed.
    {
        session.rotateView(1);
        const SketchFrame plus = session.viewFrame();
        const bool plusOk = session.viewQuarterTurns() == 1
                            && nearVec(plus.u, 0.0f, -1.0f, 0.0f)
                            && nearVec(plus.v, 1.0f, 0.0f, 0.0f) && rightHanded(plus);
        session.rotateView(-2);
        const SketchFrame minus = session.viewFrame();
        const bool minusOk = session.viewQuarterTurns() == 3
                             && nearVec(minus.u, 0.0f, 1.0f, 0.0f)
                             && nearVec(minus.v, -1.0f, 0.0f, 0.0f) && rightHanded(minus);
        r.check("CADUXR1_11_12_plus_and_minus_ninety_are_exact_quarter_turns",
                plusOk && minusOk);
    }
    // Four quarter turns is the identity, and a rotation never touches one
    // authored coordinate.
    {
        SketchSession rolled;
        rolled.begin(Workplane::XY);
        CadSketch* sketch = const_cast<CadSketch*>(&rolled.sketch());
        addSketchEntity(sketch, SketchLine{SketchPoint{1.0, 2.0}, SketchPoint{3.0, 4.0}});
        const CadSketch before = rolled.sketch();
        for (int i = 0; i < 4; ++i) {
            rolled.rotateView(1);
        }
        const SketchFrame view = rolled.viewFrame();
        r.check("CADUXR1_13_a_view_rotation_mutates_no_authored_coordinate",
                rolled.viewQuarterTurns() == 0 && nearVec(view.u, 1.0f, 0.0f, 0.0f)
                        && sameCadSketch(before, rolled.sketch()));
    }
    // Support switching is allowed while the sketch is empty, and it re-bases
    // the authoring frame to the new plane.
    {
        SketchSession empty;
        empty.begin(Workplane::XY);
        const CadStatus toXZ = empty.setSupportPlane(Workplane::XZ);
        const SketchFrame frame = empty.frame();
        const WorkplaneFrame wanted = workplaneFrame(Workplane::XZ);
        r.check("CADUXR1_09_14_support_switching_is_allowed_while_the_sketch_is_empty",
                toXZ == CadStatus::Ok && empty.plane() == Workplane::XZ
                        && nearVec(frame.n, wanted.normal.x, wanted.normal.y, wanted.normal.z)
                        && nearVec(frame.u, wanted.uAxis.x, wanted.uAxis.y, wanted.uAxis.z));
        const CadStatus toYZ = empty.setSupportPlane(Workplane::YZ);
        const WorkplaneFrame yz = workplaneFrame(Workplane::YZ);
        r.check("CADUXR1_10_every_principal_plane_is_reachable_from_the_navigator",
                toYZ == CadStatus::Ok && empty.plane() == Workplane::YZ
                        && nearVec(empty.frame().n, yz.normal.x, yz.normal.y, yz.normal.z));
    }
    // Once there is geometry, the plane is frozen: the authored numbers mean
    // one plane and are never quietly reinterpreted on another.
    {
        SketchSession drawn;
        drawn.begin(Workplane::XY);
        addSketchEntity(const_cast<CadSketch*>(&drawn.sketch()),
                        SketchLine{SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}});
        const CadSketch before = drawn.sketch();
        const CadStatus why = drawn.setSupportPlane(Workplane::XZ);
        r.check("CADUXR1_15_support_switching_after_geometry_is_refused_by_name",
                why == CadStatus::SketchNotEmpty && drawn.plane() == Workplane::XY
                        && sameCadSketch(before, drawn.sketch()));
    }
    // A FACE-supported sketch stays attached to its TopoRef: the navigator can
    // turn the view, but it cannot re-point the support at a world plane.
    {
        SketchSession faceSketch;
        TopoRef ref;
        ref.producerObjectId = 7;
        ref.face.kind = CadFaceKind::CapFar;
        ref.lineageToken = 42;
        SketchFrame worldFrame;
        worldFrame.origin = Vec3{0.0f, 0.0f, 2.0f};
        worldFrame.u = Vec3{1.0f, 0.0f, 0.0f};
        worldFrame.v = Vec3{0.0f, 1.0f, 0.0f};
        worldFrame.n = Vec3{0.0f, 0.0f, 1.0f};
        const CadStatus began = faceSketch.beginOnFace(worldFrame, ref);
        const CadStatus refused = faceSketch.setSupportPlane(Workplane::XZ);
        faceSketch.rotateView(1);
        r.check("CADUXR1_16_a_face_sketch_keeps_its_TopoRef_and_still_rotates",
                began == CadStatus::Ok && refused == CadStatus::InvalidWorkplane
                        && faceSketch.sketch().hasFaceSupport
                        && faceSketch.sketch().faceSupport.producerObjectId == 7
                        && faceSketch.viewQuarterTurns() == 1
                        && rightHanded(faceSketch.viewFrame()));
    }
}

// -------------------------------------------------------------------------
// Edit Sketch (`CADUXR1-32..36`)
// -------------------------------------------------------------------------

SketchFrame planeFrameOf(Workplane plane) {
    const WorkplaneFrame wf = workplaneFrame(plane);
    return SketchFrame{Vec3{0.0f, 0.0f, 0.0f}, wf.uAxis, wf.vAxis, wf.normal};
}

void testEditSketch(Recorder& r) {
    // A one-body project whose only body is a CAD rectangle.
    ConstructionScene scene((NoProjectTag()));
    ConstructionHistory history(scene);
    CadStatus why = CadStatus::Ok;
    SceneObject* object = scene.addCadBody(rectBody(2.0, 1.0, 1.0), &why);
    const bool built = object != nullptr && why == CadStatus::Ok;
    const ObjectId bodyId = built ? object->objectId() : kNoObject;
    if (built) {
        publishSceneObject(*object);
    }

    // Opening the edit stages a COPY. The body is untouched throughout.
    {
        SketchSession session;
        const CadStatus began = session.beginEdit(bodyId, object->cadOrNull()->state(),
                                                  planeFrameOf(Workplane::XY));
        const bool staged = began == CadStatus::Ok && session.editingExistingBody()
                            && session.editingBodyId() == bodyId
                            && session.sketch().entities.size() == 1;
        // Change the staged rectangle. The BODY still says 2.0 x 1.0.
        SketchRectangle wider;
        wider.center = SketchPoint{0.0, 0.0};
        wider.width = 5.0;
        wider.height = 1.0;
        const CadStatus replaced = session.replaceEntity(1, wider);
        const double bodyWidth = object->cadOrNull()->sketch().entities[0].rectangle()->width;
        r.check("CADUXR1_32_edit_sketch_stages_a_copy_and_leaves_the_body_alone",
                built && staged && replaced == CadStatus::Ok && near2(bodyWidth, 2.0)
                        && history.undoDepth() == 0);
        // Cancel: zero durable mutation, no history step.
        session.cancel();
        r.check("CADUXR1_33_edit_sketch_cancel_is_exact_non_mutation",
                !session.active() && !session.editingExistingBody()
                        && near2(object->cadOrNull()->sketch().entities[0].rectangle()->width, 2.0)
                        && history.undoDepth() == 0 && history.redoDepth() == 0);
    }

    // Finish: exactly one history step, and Undo/Redo restore the whole sketch.
    {
        SketchSession session;
        session.beginEdit(bodyId, object->cadOrNull()->state(), planeFrameOf(Workplane::XY));
        SketchRectangle wider;
        wider.center = SketchPoint{0.0, 0.0};
        wider.width = 5.0;
        wider.height = 1.0;
        session.replaceEntity(1, wider);
        const CadStatus finished = session.finish();
        session.selectProfile(1);
        const CadStatus committed = session.commitEdit(scene, history);
        const double afterWidth = object->cadOrNull()->sketch().entities[0].rectangle()->width;
        r.check("CADUXR1_34_finish_edit_is_exactly_one_project_history_step",
                finished == CadStatus::Ok && committed == CadStatus::Ok && near2(afterWidth, 5.0)
                        && history.undoDepth() == 1 && history.redoDepth() == 0
                        && !session.active());
        const bool undone = history.undo();
        const double undoneWidth =
                scene.findBody(bodyId)->cadOrNull()->sketch().entities[0].rectangle()->width;
        const bool redone = history.redo();
        const double redoneWidth =
                scene.findBody(bodyId)->cadOrNull()->sketch().entities[0].rectangle()->width;
        r.check("CADUXR1_35_undo_and_redo_of_a_sketch_edit_are_exact",
                undone && near2(undoneWidth, 2.0) && redone && near2(redoneWidth, 5.0));
        history.undo();  // back to the 2.0-wide body for the cases below
    }

    // A body standing on the producer's far cap follows a SUPPORTED edit --
    // one that changes sizes but not the set of faces.
    {
        CadBodyState child = rectBody(0.5, 0.5, 0.25);
        child.sketch.hasFaceSupport = true;
        child.sketch.faceSupport.producerObjectId = bodyId;
        child.sketch.faceSupport.face.kind = CadFaceKind::CapFar;
        child.sketch.faceSupport.lineageToken =
                cadTopologySignature(object->cadOrNull()->state());
        CadStatus childWhy = CadStatus::Ok;
        SceneObject* childObject = scene.addCadBody(child, &childWhy);
        const bool added = childObject != nullptr && childWhy == CadStatus::Ok;
        if (added) {
            publishSceneObject(*childObject);
        }

        SketchSession session;
        session.beginEdit(bodyId, object->cadOrNull()->state(), planeFrameOf(Workplane::XY));
        SketchRectangle wider;
        wider.center = SketchPoint{0.0, 0.0};
        wider.width = 3.0;
        wider.height = 1.0;
        session.replaceEntity(1, wider);
        session.finish();
        session.selectProfile(1);
        const CadStatus committed = session.commitEdit(scene, history);
        Mat4 childModel;
        const bool resolves = added && scene.resolveWorldModel(childObject->objectId(),
                                                               &childModel);
        r.check("CADUXR1_36_a_dependent_survives_a_supported_sketch_edit",
                added && committed == CadStatus::Ok && resolves);

        // An edit that would take the referenced face away is REFUSED by name
        // and changes nothing, exactly as deleting such a producer is.
        SketchSession breaking;
        breaking.beginEdit(bodyId, object->cadOrNull()->state(), planeFrameOf(Workplane::XY));
        CadSketch* staged = const_cast<CadSketch*>(&breaking.sketch());
        removeSketchEntity(staged, 1);
        SketchCircle circle;
        circle.center = SketchPoint{0.0, 0.0};
        circle.radius = 1.0;
        SketchEntityId circleId = kNoSketchEntity;
        addSketchEntity(staged, circle, &circleId);
        const CadStatus finished = breaking.finish();
        breaking.selectProfile(circleId);
        const CadStatus refused = breaking.commitEdit(scene, history);
        const double stillWide =
                object->cadOrNull()->sketch().entities[0].rectangle()->width;
        r.check("CADUXR1_36_an_edit_that_would_strip_a_supported_face_is_refused",
                finished == CadStatus::Ok && refused == CadStatus::DependentFaceLost
                        && near2(stillWide, 3.0));
    }
}

// -------------------------------------------------------------------------
// The CADB v3 data contract (`CADUXR1-37, 38`)
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// The semantic fingerprint covers every authored curve point (DEEP-AUDIT-R1)
// -------------------------------------------------------------------------
//
// `projectSemanticFingerprint` is what autosave and the dirty guard trust to
// notice that the document would differ. An Arc's or a Spline's authored points
// are document truth, so moving one while the entity keeps its id must move the
// fingerprint exactly as moving a line's endpoint does.
void testFingerprintCoversCurves(Recorder& r) {
    {
        ConstructionScene scene((NoProjectTag()));
        ConstructionHistory history(scene);
        CadStatus why = CadStatus::Ok;
        SceneObject* object = scene.addCadBody(arcAndLineBody(), &why);
        const bool built = object != nullptr && why == CadStatus::Ok;
        if (built) {
            publishSceneObject(*object);
        }
        const uint64_t before = projectSemanticFingerprint(scene, ProjectKind::Construction);
        SketchSession session;
        session.beginEdit(object->objectId(), object->cadOrNull()->state(),
                          planeFrameOf(Workplane::XY));
        SketchArc moved = unitSemicircle();
        moved.mid = SketchPoint{0.0, 0.5};  // same ends, same id, another curve
        const CadStatus replaced = session.replaceEntity(1, moved);
        const CadStatus finished = session.finish();
        session.selectProfile(1);
        const CadStatus committed = session.commitEdit(scene, history);
        const uint64_t after = projectSemanticFingerprint(scene, ProjectKind::Construction);
        r.check("DAR1_01_moving_an_arc_point_in_place_moves_the_fingerprint",
                built && replaced == CadStatus::Ok && finished == CadStatus::Ok
                        && committed == CadStatus::Ok && after != before);
    }
    {
        ConstructionScene scene((NoProjectTag()));
        ConstructionHistory history(scene);
        CadStatus why = CadStatus::Ok;
        SceneObject* object = scene.addCadBody(splineAndLineBody(), &why);
        const bool built = object != nullptr && why == CadStatus::Ok;
        if (built) {
            publishSceneObject(*object);
        }
        const uint64_t before = projectSemanticFingerprint(scene, ProjectKind::Construction);
        SketchSession session;
        session.beginEdit(object->objectId(), object->cadOrNull()->state(),
                          planeFrameOf(Workplane::XY));
        SketchSpline moved;
        moved.points = {SketchPoint{-1.0, 0.0}, SketchPoint{-0.5, 0.5}, SketchPoint{0.5, 0.5},
                        SketchPoint{1.0, 0.0}};
        const CadStatus replaced = session.replaceEntity(1, moved);
        const CadStatus finished = session.finish();
        session.selectProfile(1);
        const CadStatus committed = session.commitEdit(scene, history);
        const uint64_t after = projectSemanticFingerprint(scene, ProjectKind::Construction);
        r.check("DAR1_01_moving_a_spline_point_in_place_moves_the_fingerprint",
                built && replaced == CadStatus::Ok && finished == CadStatus::Ok
                        && committed == CadStatus::Ok && after != before);
    }
}

ProjectDocument documentFor(const CadBodyState& state) {
    ConstructionScene scene((NoProjectTag()));
    CadStatus why = CadStatus::Ok;
    SceneObject* object = scene.addCadBody(state, &why);
    if (object == nullptr) {
        return ProjectDocument{};
    }
    publishSceneObject(*object);
    return captureProjectDocument(scene, ProjectKind::Construction);
}

// The CADB section's declared version, read straight out of the bytes.
bool cadSectionVersion(const std::vector<uint8_t>& bytes, uint16_t* out) {
    for (size_t i = 0; i + 8 <= bytes.size(); ++i) {
        if (bytes[i] == 'C' && bytes[i + 1] == 'A' && bytes[i + 2] == 'D' && bytes[i + 3] == 'B') {
            *out = static_cast<uint16_t>(bytes[i + 4] | (bytes[i + 5] << 8));
            return true;
        }
    }
    return false;
}

// -------------------------------------------------------------------------
// The independent corpus (`CADUXR1-38`)
// -------------------------------------------------------------------------
//
// These six documents are the C++ side of the six `CADB` v3 fixtures
// `scripts/build-forge-corpus.ps1` writes from `DATA_PACKAGE_SPEC.md` §7d. The
// PowerShell builder shares no line with the codec, so a digest agreeing here
// is the format being a SPECIFICATION rather than whatever this encoder happens
// to emit. A change nobody meant to make fails HERE, with the new value beside
// it, rather than silently invalidating the corpus.

TransformValues placementAt(double tx, double ty, double tz) {
    TransformValues t;
    t.positionX = tx;
    t.positionY = ty;
    t.positionZ = tz;
    return t;
}

ProjectBodyPlacement sceneBodyAt(ObjectId id, const TransformValues& transform) {
    ProjectBodyPlacement body;
    body.objectId = id;
    body.transform = transform;
    return body;
}

// The canonical semicircle plus the chord that closes it: a "D" profile that is
// one chain of two entities, one curved and one straight.
ProjectCadBody arcProfileBody(ObjectId id, double depth) {
    ProjectCadBody body;
    body.objectId = id;
    body.state = arcAndLineBody(depth);
    return body;
}

ProjectCadBody splineProfileBody(ObjectId id, double depth) {
    ProjectCadBody body;
    body.objectId = id;
    body.state = splineAndLineBody();
    body.state.extrude.depth = depth;
    return body;
}

ProjectCadBody rectangleProfileBody(ObjectId id, double w, double h, double depth) {
    ProjectCadBody body;
    body.objectId = id;
    body.state = rectBody(w, h, depth);
    return body;
}

ProjectDocument cadArcProfileDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;
    document.scene.bodies.push_back(sceneBodyAt(1, placementAt(0.5, 0.0, 0.0)));
    document.hasCad = true;
    document.cad.bodies.push_back(arcProfileBody(1, 1.5));
    return document;
}

ProjectDocument cadSplineProfileDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;
    document.scene.bodies.push_back(sceneBodyAt(1, TransformValues{}));
    document.hasCad = true;
    document.cad.bodies.push_back(splineProfileBody(1, 0.75));
    return document;
}

// Every entity kind the format has, in one v3 section, with a Construction
// Body beside them so CONS stays sparse.
ProjectDocument cadMixedCurveProfileDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 5;
    document.scene.activeObjectId = 2;
    document.scene.bodies.push_back(sceneBodyAt(1, TransformValues{}));
    document.scene.bodies.push_back(sceneBodyAt(2, placementAt(2.0, 0.0, 0.0)));
    document.scene.bodies.push_back(sceneBodyAt(3, placementAt(-2.0, 0.0, 0.0)));
    document.scene.bodies.push_back(sceneBodyAt(4, placementAt(0.0, 3.0, 0.0)));
    document.hasConstruction = true;
    // The same canonical parameter set every other corpus fixture's
    // Construction Body carries, so this one differs from them in exactly the
    // thing it is here to pin: the v3 `CADB` beside a sparse `CONS`.
    ProjectConstructionBody source;
    source.objectId = 1;
    source.shape = canonicalCorpusShape(PrimitiveKind::Box);
    source.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(source);
    document.hasCad = true;
    document.cad.bodies.push_back(arcProfileBody(2, 1.0));
    document.cad.bodies.push_back(splineProfileBody(3, 0.5));
    document.cad.bodies.push_back(rectangleProfileBody(4, 1.5, 1.5, 1.0));
    return document;
}

// v3 carrying a v2 support block, which is the whole point of a version being a
// superset of the one below it: a CURVE profile standing on a producer's cap.
ProjectDocument cadFaceCurveDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 3;
    document.scene.activeObjectId = 2;
    document.scene.bodies.push_back(sceneBodyAt(1, placementAt(0.0, 0.0, 1.0)));
    document.scene.bodies.push_back(sceneBodyAt(2, TransformValues{}));
    document.hasCad = true;
    const ProjectCadBody producer = rectangleProfileBody(1, 4.0, 4.0, 1.0);
    ProjectCadBody dependent = arcProfileBody(2, 0.5);
    dependent.state.sketch.hasFaceSupport = true;
    dependent.state.sketch.faceSupport.producerObjectId = producer.objectId;
    dependent.state.sketch.faceSupport.producerLocalFeatureId = kCadFeatureId;
    dependent.state.sketch.faceSupport.face = CadFaceToken{CadFaceKind::CapFar, 0, 0};
    dependent.state.sketch.faceSupport.lineageToken = cadTopologySignature(producer.state);
    document.cad.bodies.push_back(producer);
    document.cad.bodies.push_back(dependent);
    return document;
}

// The two CORRUPT v3 fixtures: an arc whose three points are collinear, and a
// spline whose two ends meet. Every length, count and CRC in both is correct,
// so only the SEMANTIC check can refuse them -- which is exactly what makes
// them worth having.
//
// They cannot come out of `encodeProjectV1`, which validates first and would
// refuse the document. So, exactly as the `CAD-R0` bad-plane and `CAD-A3`
// bad-reference fixtures already do here, the valid file is encoded and the bad
// values are written over their own FIXED-WIDTH fields: every length, count and
// offset is unchanged by construction, and only the payload CRC is redone. The
// PowerShell builder reaches the same bytes by constructing them with the bad
// value in place, which is the point -- two routes, one file.

// The byte offset of the first entity's own values inside a v3 `CADB` payload
// carrying ONE body whose first entity is the one being corrupted:
//
//   bodyCount u32 | objectId u64 | plane u8 | supportKind u8 | nextEntityId u32
//   | profileEntityId u32 | direction u8 | depth f64 | entityCount u32
//   | entityId u32 | kind u8
constexpr size_t kV3FirstEntityValueOffset = 4 + 8 + 1 + 1 + 4 + 4 + 1 + 8 + 4 + 4 + 1;

// Overwrites `count` doubles at `valueIndex` within the first entity's values,
// then recomputes the CAD section's CRC. Returns the patched file.
//
// `prefixBytes` is what stands between the kind code and the first double for
// this entity kind: 0 for an Arc, whose six doubles follow immediately, and 4
// for a Spline, whose points follow its `pointCount`.
std::vector<uint8_t> patchFirstCadEntityValues(const std::vector<uint8_t>& file,
                                               size_t prefixBytes, size_t valueIndex,
                                               const double* values, size_t count) {
    std::vector<uint8_t> bytes = file;
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            const size_t payload = offset + kForgeSectionHeaderBytes;
            const size_t at = payload + kV3FirstEntityValueOffset + prefixBytes
                              + valueIndex * sizeof(double);
            if (at + count * sizeof(double) > bytes.size()) {
                return std::vector<uint8_t>();  // refused rather than trusted
            }
            std::memcpy(&bytes[at], values, count * sizeof(double));
            const uint32_t crc = crc32IsoHdlc(&bytes[payload], static_cast<size_t>(payloadBytes));
            std::memcpy(&bytes[offset + 16], &crc, 4);
            return bytes;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
    return std::vector<uint8_t>();
}

// The valid counterparts the two corrupt files are patched from: the same
// documents with a depth of 1.0 and the identity placement, so the only
// difference from the fixture is the values that are then overwritten.
ProjectDocument cadBadArcBaseDocument() {
    ProjectDocument document = cadArcProfileDocument();
    document.scene.bodies[0].transform = TransformValues{};
    document.cad.bodies[0].state.extrude.depth = 1.0;
    return document;
}

ProjectDocument cadBadSplineBaseDocument() {
    ProjectDocument document = cadSplineProfileDocument();
    document.cad.bodies[0].state.extrude.depth = 1.0;
    return document;
}


// -------------------------------------------------------------------------
// The canvas extrude manipulator (`CADUXS1-02..07`, `CADUXS1-09`)
// -------------------------------------------------------------------------

// A camera built here rather than driven through CameraController, so a case is
// a statement about the TOOL and not about how a gesture reached a pose.
CameraSnapshot uxPerspectiveCamera(const Vec3& eye, const Vec3& target, int width, int height) {
    CameraSnapshot camera{};
    Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 direction = vec3Normalize(vec3Sub(target, eye));
    if (std::fabs(vec3Dot(direction, up)) > 0.95f) {
        up = Vec3{0.0f, 0.0f, 1.0f};
    }
    camera.view = mat4LookAt(eye, target, up);
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    camera.proj = mat4Perspective(60.0f * 3.14159265358979323846f / 180.0f, aspect, 0.05f, 400.0f);
    camera.eye = eye;
    camera.target = target;
    camera.projection = ProjectionMode::Perspective;
    camera.orthoHalfHeightMeters = 1.0f;
    return camera;
}

// One profile out of a sketch, so a case can talk about anchors without going
// through a session.
bool firstProfileOf(const CadSketch& sketch, SketchEntityId anchor, ClosedProfile* out) {
    const ProfileExtraction extraction = extractClosedProfiles(sketch);
    const ClosedProfile* found = findClosedProfile(extraction, anchor);
    if (found == nullptr) {
        return false;
    }
    *out = *found;
    return true;
}

// Drives a session all the way to Ready over a rectangle on one world plane.
//
// Seeded through `beginEdit`, which is the public seam for handing a session a
// known sketch; going through the drawing tools instead would make every case
// below a statement about the Rectangle tool as well as about the manipulator.
void readyRectangleSession(SketchSession* session, Workplane plane, double w, double h,
                           double depth, double centreU = 0.0, double centreV = 0.0) {
    CadBodyState state;
    state.sketch.plane = plane;
    SketchRectangle rect;
    rect.center = SketchPoint{centreU, centreV};
    rect.width = w;
    rect.height = h;
    addSketchEntity(&state.sketch, rect);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = depth;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    session->beginEdit(1, state, planeFrameOf(plane));
    session->finish();
    session->selectProfile(1);
    session->setExtrude(depth, ExtrudeDirection::AlongNormal);
}

void testCanvasExtrudeAnchors(Recorder& r) {
    // CADUXS1-02: the arrow axis is the SUPPORT normal on every world plane,
    // and it comes from the frame rather than from any triangle.
    {
        const Workplane planes[3] = {Workplane::XY, Workplane::XZ, Workplane::YZ};
        bool allCorrect = true;
        for (int i = 0; i < 3; ++i) {
            SketchSession session;
            readyRectangleSession(&session, planes[i], 2.0, 1.0, 1.5);
            CadExtrudeAnchors anchors;
            const WorkplaneFrame wf = workplaneFrame(planes[i]);
            allCorrect = allCorrect && session.extrudeAnchors(&anchors) && anchors.valid
                         && nearVec(anchors.normal, wf.normal.x, wf.normal.y, wf.normal.z)
                         && nearVec(anchors.axis, wf.normal.x, wf.normal.y, wf.normal.z);
        }
        r.check("CADUXS1_02_a_arrow_axis_is_the_support_normal_on_all_three_world_planes",
                allCorrect);
    }
    // The base is the profile's AREA centroid on the plane, and the tip is one
    // depth along the axis. A rectangle centred on the origin puts both on the
    // frame origin and its normal, which is checkable exactly.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 1.0, 1.5);
        CadExtrudeAnchors anchors;
        const bool got = session.extrudeAnchors(&anchors);
        r.check("CADUXS1_02_b_base_is_the_profile_centroid_and_tip_is_one_depth_along",
                got && nearVec(anchors.base, 0.0f, 0.0f, 0.0f)
                        && nearVec(anchors.tip, 0.0f, 0.0f, 1.5f)
                        && nearVec(anchors.label, 0.0f, 0.0f, 0.75f)
                        && near2(anchors.depth, 1.5));
    }
    // An off-centre profile moves the anchor with it: the arrow stands on the
    // shape it extrudes, not on the sketch origin.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 1.0, 1.0, 1.0, 3.0, -2.0);
        CadExtrudeAnchors anchors;
        r.check("CADUXS1_02_c_anchor_follows_an_off_centre_profile",
                session.extrudeAnchors(&anchors)
                        && nearVec(anchors.base, 3.0f, -2.0f, 0.0f));
    }
    // A FACE-supported sketch anchors on the face frame it was opened on, and
    // its axis is that face's normal -- semantic, never a render triangle.
    {
        // A face frame standing a metre up the world Y axis, looking outward
        // along +Y -- a plane that is NOT one of the three world planes, so a
        // pass here cannot come from a workplane lookup.
        SketchFrame face;
        face.origin = Vec3{0.0f, 1.0f, 0.0f};
        face.u = Vec3{1.0f, 0.0f, 0.0f};
        face.v = Vec3{0.0f, 0.0f, -1.0f};
        face.n = Vec3{0.0f, 1.0f, 0.0f};
        CadBodyState state;
        state.sketch.plane = Workplane::XY;  // a face sketch canonical basis
        SketchRectangle rect;
        rect.center = SketchPoint{0.0, 0.0};
        rect.width = 1.0;
        rect.height = 1.0;
        addSketchEntity(&state.sketch, rect);
        state.extrude.profileEntityId = 1;
        state.extrude.depth = 2.0;
        state.extrude.direction = ExtrudeDirection::AlongNormal;
        SketchSession session;
        session.beginEdit(1, state, face);
        session.finish();
        session.selectProfile(1);
        CadExtrudeAnchors anchors;
        r.check("CADUXS1_02_d_face_supported_sketch_anchors_on_the_face_frame",
                session.extrudeAnchors(&anchors)
                        && nearVec(anchors.normal, 0.0f, 1.0f, 0.0f)
                        && nearVec(anchors.base, 0.0f, 1.0f, 0.0f)
                        && nearVec(anchors.tip, 0.0f, 3.0f, 0.0f));
    }
    // A2/A1: the anchors carry NO camera. Two very different cameras looking at
    // one sketch produce identical anchors, because no camera reaches them.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 1.0, 1.5);
        CadExtrudeAnchors first;
        CadExtrudeAnchors second;
        const bool a = session.extrudeAnchors(&first);
        // Nothing between the two reads but a camera that the tool never sees.
        const bool b = session.extrudeAnchors(&second);
        r.check("CADUXS1_02_e_anchors_are_camera_free_and_reproducible",
                a && b && nearVec(first.base, second.base.x, second.base.y, second.base.z)
                        && nearVec(first.tip, second.tip.x, second.tip.y, second.tip.z));
    }
    // Not Ready: no anchor, so no arrow. While the sketch is being DRAWN the
    // single finger belongs to the drawing.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 1.0, 1.0, 1.0);
        CadExtrudeAnchors anchors;
        const bool readyHasOne = session.extrudeAnchors(&anchors);
        session.backToEditing();
        const bool editingHasNone = !session.extrudeAnchors(&anchors);
        r.check("CADUXS1_02_f_the_arrow_exists_only_in_ready",
                readyHasOne && editingHasNone);
    }
    // The area centroid, not the vertex mean. A square with one edge densely
    // subdivided would drag a vertex mean toward that edge; the area centroid
    // stays put, which is why it is the rule.
    {
        std::vector<SketchPoint> polygon;
        polygon.push_back(SketchPoint{-1.0, -1.0});
        for (int i = 1; i < 8; ++i) {
            polygon.push_back(SketchPoint{-1.0 + 2.0 * i / 8.0, -1.0});
        }
        polygon.push_back(SketchPoint{1.0, -1.0});
        polygon.push_back(SketchPoint{1.0, 1.0});
        polygon.push_back(SketchPoint{-1.0, 1.0});
        SketchPoint centroid;
        r.check("CADUXS1_02_g_area_centroid_ignores_uneven_vertex_density",
                sketchPolygonCentroid(polygon, &centroid) && nearPoint(centroid, 0.0, 0.0, 1e-9));
    }
}

void testCanvasExtrudeFlip(Recorder& r) {
    // CADUXS1-03: Flip changes the SIDE, keeps the exact positive depth and the
    // same profile reference, and reverses the axis.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 1.0, 1.25);
        CadExtrudeAnchors before;
        session.extrudeAnchors(&before);
        const SketchEntityId profileBefore = session.selectedProfileId();
        const CadStatus flipped = session.flipExtrudeDirection();
        CadExtrudeAnchors after;
        const bool got = session.extrudeAnchors(&after);
        r.check("CADUXS1_03_a_flip_reverses_the_side_and_keeps_the_depth_and_profile",
                flipped == CadStatus::Ok && got
                        && session.extrude().direction == ExtrudeDirection::AgainstNormal
                        && near2(session.extrude().depth, 1.25)
                        && session.selectedProfileId() == profileBefore
                        && nearVec(after.axis, -before.axis.x, -before.axis.y, -before.axis.z)
                        && nearVec(after.tip, 0.0f, 0.0f, -1.25f)
                        && nearVec(after.normal, before.normal.x, before.normal.y,
                                   before.normal.z));
    }
    // Twice is the identity: the direction is two-valued and the depth never
    // acquires a sign.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 1.0, 1.25);
        session.flipExtrudeDirection();
        session.flipExtrudeDirection();
        r.check("CADUXS1_03_b_two_flips_are_the_identity_and_the_depth_is_never_negative",
                session.extrude().direction == ExtrudeDirection::AlongNormal
                        && near2(session.extrude().depth, 1.25)
                        && session.extrude().depth > 0.0);
    }
    // Refused outside Ready, by name, changing nothing.
    {
        SketchSession session;
        const CadStatus inactive = session.flipExtrudeDirection();
        session.begin(Workplane::XY);
        const CadStatus editing = session.flipExtrudeDirection();
        r.check("CADUXS1_03_c_flip_is_refused_by_name_outside_ready",
                inactive == CadStatus::NotSketching && editing == CadStatus::NotSketching
                        && session.extrude().direction == ExtrudeDirection::AlongNormal);
    }
    // A flipped extrusion generates the mirrored solid: one cap still lies on
    // the sketch plane, and the other is on the other side.
    {
        CadBodyState state = rectBody(2.0, 1.0, 1.0);
        state.extrude.direction = ExtrudeDirection::AgainstNormal;
        ConstructionMesh mesh;
        const CadStatus made = generateCadMesh(state, &mesh);
        double minZ = 1.0e9;
        double maxZ = -1.0e9;
        for (const MeshVertex& v : mesh.vertices) {
            if (v.position[2] < minZ) minZ = v.position[2];
            if (v.position[2] > maxZ) maxZ = v.position[2];
        }
        r.check("CADUXS1_03_d_a_flipped_extrusion_grows_on_the_other_side",
                made == CadStatus::Ok && near2(maxZ, 0.0, 1e-6) && near2(minZ, -1.0, 1e-6));
    }
}

void testCanvasExtrudeScale(Recorder& r) {
    // CADUXS1-09: inside the band the control is a rigid WORLD object -- the
    // multiplier is strictly monotonic in the camera distance, which is the
    // whole difference from the gizmo's constant screen size.
    {
        CadExtrudeControlScale near;
        CadExtrudeControlScale far;
        // 0.45 m over 120 px is 0.00375 m/px at scale exactly 1.
        const bool a = cadExtrudeControlScaleFor(0.00375f, &near);
        const bool b = cadExtrudeControlScaleFor(0.00500f, &far);
        r.check("CADUXS1_09_a_scale_is_one_at_the_reference_distance_and_falls_with_it",
                a && b && near.valid && far.valid && near2(near.scale, 1.0, 1e-4)
                        && far.scale < near.scale && !near.clampedLow && !near.clampedHigh);
    }
    // Monotonic across the whole unclamped band, swept rather than sampled at
    // two points: a rule that is right at the ends and wrong in between is the
    // failure a sweep exists to catch.
    {
        bool monotonic = true;
        float previous = 1.0e9f;
        for (int i = 0; i < 40; ++i) {
            const float perPixel = 0.0030f + 0.0000125f * static_cast<float>(i);
            CadExtrudeControlScale s;
            if (!cadExtrudeControlScaleFor(perPixel, &s)) {
                monotonic = false;
                break;
            }
            if (!s.clampedLow && !s.clampedHigh) {
                if (s.scale >= previous) {
                    monotonic = false;
                    break;
                }
                previous = s.scale;
            }
        }
        r.check("CADUXS1_09_b_scale_is_strictly_monotonic_inside_the_band", monotonic);
    }
    // Both clamps engage and are reported by name, and NOTHING leaves the band.
    {
        CadExtrudeControlScale veryFar;
        CadExtrudeControlScale veryNear;
        const bool a = cadExtrudeControlScaleFor(1.0f, &veryFar);        // 0.45 px unclamped
        const bool b = cadExtrudeControlScaleFor(0.00001f, &veryNear);   // 45000 px unclamped
        r.check("CADUXS1_09_c_both_clamps_engage_and_the_band_is_never_left",
                a && b && veryFar.clampedLow && !veryFar.clampedHigh
                        && near2(veryFar.scale, kCadExtrudeControlMinScale, 1e-6)
                        && veryNear.clampedHigh && !veryNear.clampedLow
                        && near2(veryNear.scale, kCadExtrudeControlMaxScale, 1e-6)
                        && veryFar.unclampedScale < kCadExtrudeControlMinScale
                        && veryNear.unclampedScale > kCadExtrudeControlMaxScale);
    }
    // The band is pinned by value. Since `CAD-VERTICAL-SLICE-R1` the HUD scales
    // only its glyphs by it and never its 48 dp hit areas (CadHudPresentationTest
    // asserts that floor at every scale on the JVM), so what is left to pin here
    // is the band itself: 0.80 .. 1.60, a factor of exactly two.
    {
        r.check("CADUXS1_09_d_the_scale_band_is_0_80_to_1_60_a_factor_of_two",
                near2(kCadExtrudeControlMinScale, 0.80, 1e-6)
                        && near2(kCadExtrudeControlMaxScale, 1.60, 1e-6)
                        && near2(kCadExtrudeControlMaxScale / kCadExtrudeControlMinScale, 2.0,
                                 1e-6));
    }
    // A degenerate camera quantity produces nothing rather than a guess.
    {
        CadExtrudeControlScale s;
        const bool zero = cadExtrudeControlScaleFor(0.0f, &s);
        const bool negative = cadExtrudeControlScaleFor(-1.0f, &s);
        const bool nan = cadExtrudeControlScaleFor(
                std::numeric_limits<float>::quiet_NaN(), &s);
        r.check("CADUXS1_09_e_a_degenerate_camera_produces_no_scale_at_all",
                !zero && !negative && !nan);
    }
    // Draw and hit test consume ONE number: the world size is the clamped pixel
    // size carried back through the same metres-per-pixel it came from.
    {
        CadExtrudeControlScale s;
        const bool got = cadExtrudeControlScaleFor(0.00375f, &s);
        r.check("CADUXS1_09_f_drawing_and_hit_testing_share_one_derived_size",
                got && near2(s.pixels, s.scale * kCadExtrudeControlReferencePixels, 1e-4)
                        && near2(s.world, s.pixels * s.metersPerPixel, 1e-6));
    }
    // The same anchor under two cameras at different distances gives different
    // scales, which is the request; the ANCHOR itself does not move.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 1.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        const CameraSnapshot close =
                uxPerspectiveCamera(Vec3{2.0f, 2.0f, 4.0f}, Vec3{0.0f, 0.0f, 0.0f}, 1080, 2000);
        const CameraSnapshot distant =
                uxPerspectiveCamera(Vec3{20.0f, 20.0f, 40.0f}, Vec3{0.0f, 0.0f, 0.0f}, 1080, 2000);
        CadExtrudeControlScale a;
        CadExtrudeControlScale b;
        const bool gotA = cadExtrudeControlScale(close, anchors.base, 2000, &a);
        const bool gotB = cadExtrudeControlScale(distant, anchors.base, 2000, &b);
        r.check("CADUXS1_09_g_a_farther_camera_draws_a_smaller_control",
                gotA && gotB && b.scale < a.scale);
    }
}

void testCanvasExtrudeDrag(Recorder& r) {
    // CADUXS1-04: a drag along the axis changes the depth deterministically,
    // and the SAME world displacement is the same depth change under two very
    // different cameras -- the semantic result does not depend on zoom.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        // A camera looking across the extrusion axis, so the axis solve is well
        // conditioned and a screen drag maps onto it.
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{8.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
        CadExtrudeManipulator drag;
        float startX = 0.0f;
        float startY = 0.0f;
        float endX = 0.0f;
        float endY = 0.0f;
        // Two world points a known distance apart ALONG the axis, projected:
        // dragging from the first pixel to the second is a one-metre move.
        const bool projected =
                projectWorldToScreen(camera, anchors.tip, w, h, &startX, &startY)
                && projectWorldToScreen(camera,
                                        vec3Add(anchors.tip, vec3Scale(anchors.axis, 1.0f)), w, h,
                                        &endX, &endY);
        Meters depth = 0.0;
        const bool began = drag.beginDrag(7, anchors, anchors.primaryIsPositive, camera, startX, startY, w, h);
        const bool moved = drag.updateDrag(7, camera, endX, endY, w, h, &depth);
        r.check("CADUXS1_04_a_a_one_metre_axial_drag_adds_one_metre_of_depth",
                projected && began && moved && near2(depth, 2.0, 1e-3));
    }
    // The same world displacement under a camera five times as far away.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{40.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
        CadExtrudeManipulator drag;
        float startX = 0.0f;
        float startY = 0.0f;
        float endX = 0.0f;
        float endY = 0.0f;
        const bool projected =
                projectWorldToScreen(camera, anchors.tip, w, h, &startX, &startY)
                && projectWorldToScreen(camera,
                                        vec3Add(anchors.tip, vec3Scale(anchors.axis, 1.0f)), w, h,
                                        &endX, &endY);
        Meters depth = 0.0;
        drag.beginDrag(7, anchors, anchors.primaryIsPositive, camera, startX, startY, w, h);
        const bool moved = drag.updateDrag(7, camera, endX, endY, w, h, &depth);
        r.check("CADUXS1_04_b_zoom_does_not_change_what_a_world_displacement_means",
                projected && moved && near2(depth, 2.0, 1e-3));
    }
    // A4: the basis is FROZEN. The preview grows under the finger, and the same
    // pixel delta still means the same depth delta at the end of a long drag as
    // at its start -- the drag is solved against the pointer-down anchor, never
    // against the moved tip.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{8.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
        CadExtrudeManipulator drag;
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float x2 = 0.0f;
        float y2 = 0.0f;
        const bool projected =
                projectWorldToScreen(camera, anchors.tip, w, h, &x0, &y0)
                && projectWorldToScreen(camera,
                                        vec3Add(anchors.tip, vec3Scale(anchors.axis, 1.0f)), w, h,
                                        &x1, &y1)
                && projectWorldToScreen(camera,
                                        vec3Add(anchors.tip, vec3Scale(anchors.axis, 2.0f)), w, h,
                                        &x2, &y2);
        Meters first = 0.0;
        Meters second = 0.0;
        drag.beginDrag(7, anchors, anchors.primaryIsPositive, camera, x0, y0, w, h);
        drag.updateDrag(7, camera, x1, y1, w, h, &first);
        drag.updateDrag(7, camera, x2, y2, w, h, &second);
        r.check("CADUXS1_04_c_the_drag_basis_is_frozen_at_pointer_down",
                projected && near2(first, 2.0, 1e-3) && near2(second, 3.0, 1e-3));
    }
    // A degenerate viewpoint -- looking straight DOWN the extrusion axis -- is
    // named and holds the last good value rather than guessing.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        // Straight down +Z, which is exactly the extrusion axis.
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{0.0f, 0.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, w, h);
        CadExtrudeManipulator drag;
        const bool began = drag.beginDrag(7, anchors, anchors.primaryIsPositive, camera, 540.0f, 1000.0f, w, h);
        Meters depth = 0.0;
        bool held = true;
        if (began) {
            held = !drag.updateDrag(7, camera, 540.0f, 400.0f, w, h, &depth)
                   || drag.lastSolve() != AxisSolveStatus::Unresolvable;
        }
        // Either the drag never starts (nothing to freeze) or every sample is
        // solved by the fallback plane; what must NEVER happen is an
        // Unresolvable that still wrote a depth.
        r.check("CADUXS1_04_d_a_degenerate_viewpoint_never_writes_a_guessed_depth", held);
    }
    // A drag is CLAMPED at the floor rather than producing a zero or negative
    // depth, and the body stays valid throughout. A typed value is refused
    // instead -- the two are deliberately different, and both are asserted.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{8.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
        CadExtrudeManipulator drag;
        float x0 = 0.0f;
        float y0 = 0.0f;
        float xBack = 0.0f;
        float yBack = 0.0f;
        const bool projected =
                projectWorldToScreen(camera, anchors.tip, w, h, &x0, &y0)
                && projectWorldToScreen(camera,
                                        vec3Add(anchors.tip, vec3Scale(anchors.axis, -8.0f)), w, h,
                                        &xBack, &yBack);
        Meters depth = 0.0;
        drag.beginDrag(7, anchors, anchors.primaryIsPositive, camera, x0, y0, w, h);
        const bool moved = drag.updateDrag(7, camera, xBack, yBack, w, h, &depth);
        const CadStatus typedZero = session.setExtrude(0.0, ExtrudeDirection::AlongNormal);
        const CadStatus typedNegative = session.setExtrude(-1.0, ExtrudeDirection::AlongNormal);
        r.check("CADUXS1_04_e_a_drag_is_clamped_at_the_floor_and_a_typed_value_is_refused",
                projected && moved && near2(depth, kCadExtrudeMinDragDepthMeters, 1e-9)
                        && typedZero == CadStatus::InvalidExtrudeDepth
                        && typedNegative == CadStatus::InvalidExtrudeDepth
                        && near2(session.extrude().depth, 1.0));
    }
    // ONE pointer for the life of the drag: a sample from a different id writes
    // nothing at all.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{8.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
        CadExtrudeManipulator drag;
        float x0 = 0.0f;
        float y0 = 0.0f;
        projectWorldToScreen(camera, anchors.tip, w, h, &x0, &y0);
        drag.beginDrag(7, anchors, anchors.primaryIsPositive, camera, x0, y0, w, h);
        Meters depth = 0.0;
        const bool otherPointerIgnored = !drag.updateDrag(9, camera, x0, y0 - 200.0f, w, h, &depth);
        r.check("CADUXS1_04_f_a_second_pointer_id_cannot_steer_the_drag",
                otherPointerIgnored && drag.capturedPointerId() == 7);
    }
    // Cancel reports the depth the gesture started from, which is what the
    // session puts back.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.75);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{8.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
        CadExtrudeManipulator drag;
        float x0 = 0.0f;
        float y0 = 0.0f;
        projectWorldToScreen(camera, anchors.tip, w, h, &x0, &y0);
        drag.beginDrag(11, anchors, anchors.primaryIsPositive, camera, x0, y0, w, h);
        Meters restore = 0.0;
        const bool cancelled = drag.cancelDrag(&restore);
        const bool secondCancelDoesNothing = !drag.cancelDrag(&restore);
        r.check("CADUXS1_06_a_cancel_reports_the_pre_drag_depth_and_releases_the_pointer",
                cancelled && near2(restore, 1.75) && secondCancelDoesNothing
                        && !drag.capturing());
    }
    // The hit test takes the arrow and refuses what is well clear of it, at the
    // same scale the drawing used.
    {
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 2.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{8.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
        CadExtrudeManipulator drag;
        float midX = 0.0f;
        float midY = 0.0f;
        const bool projected =
                projectWorldToScreen(camera, anchors.label, w, h, &midX, &midY);
        const bool onShaft = drag.hitTest(anchors, camera, midX, midY, w, h);
        const bool wellClear = !drag.hitTest(anchors, camera, midX + 600.0f, midY, w, h);
        r.check("CADUXS1_04_g_the_hit_test_takes_the_arrow_and_refuses_what_is_clear_of_it",
                projected && onShaft && wellClear);
    }
}

void testCanvasExtrudeSessionGesture(Recorder& r) {
    // The session routes a Ready-state pointer to the manipulator, writes the
    // depth through its ONE writer, and a Cancel puts the pre-drag depth back.
    // CADUXS1-06 at the session level: nothing here is project truth in the
    // first place, so "changes nothing" is structural.
    SketchSession session;
    readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.0);
    CadExtrudeAnchors anchors;
    session.extrudeAnchors(&anchors);
    const int w = 1080;
    const int h = 2000;
    const CameraSnapshot camera =
            uxPerspectiveCamera(Vec3{8.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    const bool projected =
            projectWorldToScreen(camera, anchors.tip, w, h, &x0, &y0)
            && projectWorldToScreen(camera,
                                    vec3Add(anchors.tip, vec3Scale(anchors.axis, 1.0f)), w, h, &x1,
                                    &y1);
    TouchPointer down{};
    down.id = 3;
    down.x = x0;
    down.y = y0;
    const bool consumedDown =
            session.onTouch(TouchAction::Down, -1, &down, 1, camera, w, h);
    TouchPointer move = down;
    move.x = x1;
    move.y = y1;
    const bool consumedMove =
            session.onTouch(TouchAction::Move, -1, &move, 1, camera, w, h);
    const double dragged = session.extrude().depth;
    session.onTouch(TouchAction::Up, 3, &move, 1, camera, w, h);
    r.check("CADUXS1_04_h_a_ready_state_drag_writes_the_depth_through_the_session",
            projected && consumedDown && consumedMove && near2(dragged, 2.0, 1e-3)
                    && !session.extrudeManipulator().capturing());

    // A pointer that goes down CLEAR of the arrow is not consumed: orbit, pan
    // and tap are untouched everywhere except on the arrow itself.
    {
        SketchSession other;
        readyRectangleSession(&other, Workplane::XY, 2.0, 2.0, 1.0);
        TouchPointer away{};
        away.id = 4;
        away.x = 20.0f;
        away.y = 20.0f;
        r.check("CADUXS1_04_i_a_pointer_clear_of_the_arrow_still_navigates",
                !other.onTouch(TouchAction::Down, -1, &away, 1, camera, w, h));
    }
    // A SECOND pointer cancels the drag and hands the gesture to the camera,
    // putting the depth back exactly where the first finger found it.
    {
        SketchSession two;
        readyRectangleSession(&two, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors a2;
        two.extrudeAnchors(&a2);
        TouchPointer p0{};
        p0.id = 5;
        p0.x = x0;
        p0.y = y0;
        two.onTouch(TouchAction::Down, -1, &p0, 1, camera, w, h);
        TouchPointer m0 = p0;
        m0.x = x1;
        m0.y = y1;
        two.onTouch(TouchAction::Move, -1, &m0, 1, camera, w, h);
        const double moved = two.extrude().depth;
        TouchPointer pair[2];
        pair[0] = m0;
        pair[1] = TouchPointer{};
        pair[1].id = 6;
        pair[1].x = 300.0f;
        pair[1].y = 300.0f;
        const bool handedOn =
                !two.onTouch(TouchAction::PointerDown, 6, pair, 2, camera, w, h);
        r.check("CADUXS1_06_b_a_second_pointer_cancels_the_drag_and_restores_the_depth",
                moved > 1.5 && handedOn && near2(two.extrude().depth, 1.0)
                        && !two.extrudeManipulator().capturing());
    }
    // A Cancel action restores the pre-drag depth too.
    {
        SketchSession three;
        readyRectangleSession(&three, Workplane::XY, 2.0, 2.0, 1.0);
        TouchPointer p0{};
        p0.id = 8;
        p0.x = x0;
        p0.y = y0;
        three.onTouch(TouchAction::Down, -1, &p0, 1, camera, w, h);
        TouchPointer m0 = p0;
        m0.x = x1;
        m0.y = y1;
        three.onTouch(TouchAction::Move, -1, &m0, 1, camera, w, h);
        three.onTouch(TouchAction::Cancel, -1, &m0, 1, camera, w, h);
        r.check("CADUXS1_06_c_a_cancelled_drag_restores_the_pre_drag_depth",
                near2(three.extrude().depth, 1.0));
    }
    // The manipulator is inert while the sketch is being DRAWN: in Editing the
    // single finger belongs to the drawing tool, unchanged.
    {
        SketchSession editing;
        editing.begin(Workplane::XY);
        editing.setTool(SketchTool::Rectangle);
        // A camera that actually LOOKS at the XY plane: the one above solves the
        // extrusion axis well and is edge-on to the sketch, so a drawing tool
        // there would miss the plane for a reason that has nothing to do with
        // the manipulator.
        const CameraSnapshot overPlane =
                uxPerspectiveCamera(Vec3{3.0f, 3.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, w, h);
        TouchPointer p0{};
        p0.id = 12;
        p0.x = 540.0f;
        p0.y = 1000.0f;
        const bool consumed = editing.onTouch(TouchAction::Down, -1, &p0, 1, overPlane, w, h);
        r.check("CADUXS1_04_j_editing_state_still_belongs_to_the_drawing_tool",
                consumed && !editing.extrudeManipulator().capturing()
                        && editing.gestureActive());
    }
}

void testCanvasExtrudeParityAndPurity(Recorder& r) {
    // CADUXS1-05: an exact typed depth and an equivalent drag reach the SAME
    // authored state, byte for byte, and generate the same mesh.
    {
        SketchSession typed;
        readyRectangleSession(&typed, Workplane::XY, 2.0, 2.0, 1.0);
        typed.setExtrude(2.0, ExtrudeDirection::AlongNormal);

        SketchSession dragged;
        readyRectangleSession(&dragged, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        dragged.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{8.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        projectWorldToScreen(camera, anchors.tip, w, h, &x0, &y0);
        projectWorldToScreen(camera, vec3Add(anchors.tip, vec3Scale(anchors.axis, 1.0f)), w, h,
                             &x1, &y1);
        CadExtrudeManipulator drag;
        drag.beginDrag(1, anchors, anchors.primaryIsPositive, camera, x0, y0, w, h);
        Meters depth = 0.0;
        drag.updateDrag(1, camera, x1, y1, w, h, &depth);
        // The drag lands within a pixel of 2.0; the exact value is then typed,
        // which is the workflow: the arrow gets close and the field is exact.
        dragged.setExtrude(2.0, ExtrudeDirection::AlongNormal);

        const CadBodyState a = typed.candidateState();
        const CadBodyState b = dragged.candidateState();
        ConstructionMesh meshA;
        ConstructionMesh meshB;
        const bool madeA = generateCadMesh(a, &meshA) == CadStatus::Ok;
        const bool madeB = generateCadMesh(b, &meshB) == CadStatus::Ok;
        bool sameMesh = madeA && madeB && meshA.vertices.size() == meshB.vertices.size()
                        && meshA.indices.size() == meshB.indices.size();
        if (sameMesh) {
            for (size_t i = 0; i < meshA.vertices.size() && sameMesh; ++i) {
                for (int k = 0; k < 3; ++k) {
                    if (meshA.vertices[i].position[k] != meshB.vertices[i].position[k]) {
                        sameMesh = false;
                    }
                }
            }
        }
        r.check("CADUXS1_05_a_a_typed_depth_and_a_drag_reach_the_same_authored_state",
                sameCadBodyState(a, b) && depth > 1.9 && depth < 2.1 && sameMesh);
    }
    // CADUXS1-11 in the domain: nothing the canvas tool does can reach a
    // `.forge` byte, because none of it is in the state that is encoded. The
    // proof is direct -- a session driven through a drag, a flip and a flip back
    // produces a state bit-identical to one never touched by the manipulator.
    {
        SketchSession untouched;
        readyRectangleSession(&untouched, Workplane::XY, 2.0, 2.0, 1.0);

        SketchSession worked;
        readyRectangleSession(&worked, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        worked.extrudeAnchors(&anchors);
        const int w = 1080;
        const int h = 2000;
        const CameraSnapshot camera =
                uxPerspectiveCamera(Vec3{8.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, w, h);
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        projectWorldToScreen(camera, anchors.tip, w, h, &x0, &y0);
        projectWorldToScreen(camera, vec3Add(anchors.tip, vec3Scale(anchors.axis, 1.0f)), w, h,
                             &x1, &y1);
        TouchPointer p{};
        p.id = 2;
        p.x = x0;
        p.y = y0;
        worked.onTouch(TouchAction::Down, -1, &p, 1, camera, w, h);
        TouchPointer m = p;
        m.x = x1;
        m.y = y1;
        worked.onTouch(TouchAction::Move, -1, &m, 1, camera, w, h);
        worked.onTouch(TouchAction::Cancel, -1, &m, 1, camera, w, h);
        worked.flipExtrudeDirection();
        worked.flipExtrudeDirection();
        r.check("CADUXS1_11_a_a_drag_a_cancel_and_two_flips_leave_the_state_bit_identical",
                sameCadBodyState(untouched.candidateState(), worked.candidateState()));
    }
    // The arrow is DRAWN, and it is drawn into the overlay the renderer already
    // knows how to weight -- no new style, no new range kind, and the shaft is
    // the extrusion while only the head takes the control scale.
    {
        CadExtrudeAnchors anchors;
        anchors.valid = true;
        anchors.base = Vec3{0.0f, 0.0f, 0.0f};
        anchors.normal = Vec3{0.0f, 0.0f, 1.0f};
        anchors.axis = Vec3{0.0f, 0.0f, 1.0f};
        anchors.tip = Vec3{0.0f, 0.0f, 2.0f};
        anchors.label = Vec3{0.0f, 0.0f, 1.0f};
        anchors.depth = 2.0;
        // The `+N` side IS the primary one here, so the two describe one arrow
        // (`CAD-EXT-R1`): a One Side extrusion draws exactly what it always did.
        anchors.primaryIsPositive = true;
        anchors.positive.present = true;
        anchors.positive.axis = anchors.axis;
        anchors.positive.tip = anchors.tip;
        anchors.positive.label = anchors.label;
        anchors.positive.distance = 2.0;
        anchors.negative.axis = Vec3{0.0f, 0.0f, -1.0f};
        std::vector<GizmoVertex> small;
        std::vector<GizmoVertex> large;
        appendCadExtrudeArrow(&small, anchors, 0.10, false);
        appendCadExtrudeArrow(&large, anchors, 0.40, false);
        // The same number of lines at both sizes -- the head is the same shape,
        // drawn larger -- and the shaft still runs base to tip at both.
        const bool sameTopology = !small.empty() && small.size() == large.size();
        const bool shaftIsTheDepth =
                sameTopology && small[0].position[2] == 0.0f && small[1].position[2] == 2.0f
                && large[0].position[2] == 0.0f && large[1].position[2] == 2.0f;
        // The head reaches further past the tip at the larger control size.
        float smallMax = -1.0e9f;
        float largeMax = -1.0e9f;
        for (const GizmoVertex& v : small) smallMax = std::fmax(smallMax, v.position[2]);
        for (const GizmoVertex& v : large) largeMax = std::fmax(largeMax, v.position[2]);
        r.check("CADUXS1_09_h_only_the_head_takes_the_control_scale_and_the_shaft_is_the_depth",
                sameTopology && shaftIsTheDepth && largeMax > smallMax
                        && smallMax > 2.0f);
        // A grabbed arrow carries the emphasis tag the overlay already has.
        std::vector<GizmoVertex> held;
        appendCadExtrudeArrow(&held, anchors, 0.10, true);
        bool allEmphasised = !held.empty();
        for (const GizmoVertex& v : held) {
            if (v.handle != 1.0f) allEmphasised = false;
        }
        r.check("CADUXS1_09_i_a_held_arrow_uses_the_overlay_existing_emphasis_tag",
                allEmphasised);
    }
    // Degenerate inputs draw nothing rather than something wrong.
    {
        std::vector<GizmoVertex> out;
        CadExtrudeAnchors invalid;
        appendCadExtrudeArrow(&out, invalid, 0.1, false);
        CadExtrudeAnchors valid;
        valid.valid = true;
        valid.base = Vec3{0.0f, 0.0f, 0.0f};
        valid.normal = Vec3{0.0f, 0.0f, 1.0f};
        valid.axis = Vec3{0.0f, 0.0f, 1.0f};
        valid.tip = Vec3{0.0f, 0.0f, 1.0f};
        valid.label = Vec3{0.0f, 0.0f, 0.5f};
        valid.depth = 1.0;
        appendCadExtrudeArrow(&out, valid, 0.0, false);
        appendCadExtrudeArrow(&out, valid, -1.0, false);
        r.check("CADUXS1_09_j_a_degenerate_arrow_draws_nothing", out.empty());
    }
    // CADUXS1-10: the operation is New Body because the domain can express no
    // other. The extrusion is two-valued and a third code is refused, so
    // nothing can smuggle a Symmetric or a Two Sides through the same field
    // this stage drives -- which is what makes the absent controls honest
    // rather than merely undrawn.
    {
        ExtrudeDirection parsed = ExtrudeDirection::AlongNormal;
        const bool along = extrudeDirectionFromIndex(0, &parsed)
                           && parsed == ExtrudeDirection::AlongNormal;
        const bool against = extrudeDirectionFromIndex(1, &parsed)
                             && parsed == ExtrudeDirection::AgainstNormal;
        const bool noThird = !extrudeDirectionFromIndex(2, &parsed)
                             && !extrudeDirectionFromIndex(-1, &parsed);
        r.check("CADUXS1_10_a_an_extrusion_is_two_valued_and_a_third_code_is_refused",
                kExtrudeDirectionCount == 2 && along && against && noThird);
    }
    // And a committed body reached through the canvas is an ORDINARY CAD body:
    // one `CadBodyState`, one sketch, one extrusion. No second body was created
    // and nothing about the manipulator is stored in it.
    {
        ConstructionScene scene((NoProjectTag()));
        ConstructionHistory history(scene);
        CadStatus why = CadStatus::Ok;
        const size_t before = scene.bodyCount();
        SceneObject* object = scene.addCadBody(rectBody(2.0, 1.0, 1.5), &why);
        r.check("CADUXS1_07_a_one_extrude_creates_exactly_one_new_body",
                object != nullptr && why == CadStatus::Ok
                        && scene.bodyCount() == before + 1
                        && object->representation() == BodyRepresentation::Cad);
    }
}

void testCanvasExtrudeRetainedSketch(Recorder& r) {
    // CADUXS1-08 in the domain: Extrude CONSUMES NOTHING. A committed body's
    // sketch is bit-identical to the one that was drawn, and the anchor the
    // canvas chip stands on is derived from that retained sketch.
    ConstructionScene scene((NoProjectTag()));
    ConstructionHistory history(scene);
    const CadBodyState authored = rectBody(2.0, 1.0, 1.5);
    CadStatus why = CadStatus::Ok;
    SceneObject* body = nullptr;
    {
        ScopedConstructionEdit edit(history);
        body = scene.addCadBody(authored, &why);
    }
    const CadBody* cad = body != nullptr ? body->cadOrNull() : nullptr;
    r.check("CADUXS1_08_a_extrude_consumes_nothing_the_sketch_is_still_the_body_truth",
            cad != nullptr && sameCadBodyState(cad->state(), authored));

    // And the retained sketch still produces an anchor, so the chip that
    // reopens it has somewhere honest to stand.
    if (cad != nullptr) {
        ClosedProfile profile;
        const bool extracted =
                firstProfileOf(cad->sketch(), cad->extrude().profileEntityId, &profile);
        const WorkplaneFrame wf = workplaneFrame(cad->sketch().plane);
        SketchFrame frame{Vec3{0.0f, 0.0f, 0.0f}, wf.uAxis, wf.vAxis, wf.normal};
        CadExtrudeAnchors anchors;
        r.check("CADUXS1_08_b_a_committed_body_retained_sketch_still_yields_an_anchor",
                extracted && cadExtrudeAnchors(frame, profile, cad->extrude(), &anchors)
                        && anchors.valid && nearVec(anchors.base, 0.0f, 0.0f, 0.0f));
    } else {
        r.check("CADUXS1_08_b_a_committed_body_retained_sketch_still_yields_an_anchor", false);
    }
}

// -------------------------------------------------------------------------
// The feature-preview camera (`CADUXS1C1-02`, `-03`, `-05`, `-09`)
// -------------------------------------------------------------------------

// The pose the sketch itself is looked through: what `frameSketchView` leaves
// behind, captured. Built through a real CameraController rather than by hand,
// so a case is a statement about the product's own camera.
CameraController::Pose sketchPoseOf(const SketchFrame& frame) {
    CameraController camera;
    camera.setViewport(1080, 2000);
    camera.frameSketchView(frame.origin, frame.u, frame.v, frame.n);
    return camera.capturePose();
}

// The pose a general 3D view would have, from its orbit angles.
CameraController::Pose orbitPoseOf(float yaw, float pitch) {
    CameraController camera;
    camera.setViewport(1080, 2000);
    camera.setPose(yaw, pitch, 8.2f);
    return camera.capturePose();
}

// Installs a pose the way the JNI transition does and hands back the snapshot
// the manipulator would actually be given.
CameraSnapshot snapshotOfPose(const CameraController::Pose& pose, int w, int h) {
    CameraController camera;
    camera.setViewport(w, h);
    camera.restorePose(pose);
    return camera.snapshot();
}

// A face-supported frame: an arbitrary world orientation with no world up
// anywhere in its construction, which is what a `CAD-A3` face frame is.
SketchFrame tiltedFaceFrame(float tiltRadians, float aboutRadians) {
    // A normal `tiltRadians` off world +Y, swung `aboutRadians` around it.
    const Vec3 n = vec3Normalize(Vec3{std::sin(tiltRadians) * std::cos(aboutRadians),
                                      std::cos(tiltRadians),
                                      std::sin(tiltRadians) * std::sin(aboutRadians)});
    Vec3 seed{1.0f, 0.0f, 0.0f};
    if (std::fabs(vec3Dot(n, seed)) > 0.9f) {
        seed = Vec3{0.0f, 0.0f, 1.0f};
    }
    const Vec3 u = vec3Normalize(vec3Cross(seed, n));
    const Vec3 v = vec3Normalize(vec3Cross(n, u));
    return SketchFrame{Vec3{0.4f, 1.3f, -0.7f}, u, v, n};
}

void testFeaturePreviewView(Recorder& r) {
    // CADUXS1C1-03 a: the tool's copy of the orbit convention IS the camera's.
    // The direction the policy measures and the direction the camera installs
    // have to be one function; a case rather than a comment, because the
    // camera's own `orbitDirection` is private and could drift under this.
    {
        bool agree = true;
        const float yaws[4] = {0.0f, 0.7f, 2.4f, -1.9f};
        const float pitches[4] = {0.0f, 0.5f, -1.1f, 1.4f};
        for (int i = 0; i < 4 && agree; ++i) {
            CameraController camera;
            camera.setViewport(1080, 2000);
            camera.setPose(yaws[i], pitches[i], 8.2f);
            const CameraSnapshot snap = camera.snapshot();
            // target -> eye, which is what the policy is written in.
            const Vec3 fromCamera = vec3Normalize(vec3Sub(snap.eye, snap.target));
            const Vec3 fromTool = cadFeatureViewDirection(yaws[i], pitches[i]);
            agree = nearVec(fromTool, fromCamera.x, fromCamera.y, fromCamera.z, 1e-4f);
        }
        r.check("CADUXS1C1_03_a_the_tool_orbit_direction_is_the_camera_one", agree);
    }

    // CADUXS1C1-02: the SKETCH view is unchanged, and it is exactly the view a
    // drag cannot be resolved from. Both halves of `OQ-CAD-UX-01` in one case,
    // over all three world planes -- this is the state the fix must not touch.
    {
        bool exact = true;
        bool unusable = true;
        const Workplane planes[3] = {Workplane::XY, Workplane::XZ, Workplane::YZ};
        for (int i = 0; i < 3; ++i) {
            const SketchFrame frame = planeFrameOf(planes[i]);
            CameraController camera;
            camera.setViewport(1080, 2000);
            camera.frameSketchView(frame.origin, frame.u, frame.v, frame.n);
            const CameraSnapshot snap = camera.snapshot();
            const Vec3 direction = vec3Normalize(vec3Sub(snap.eye, snap.target));
            // Exactly along the support normal, and orthographic, as before.
            exact = exact && camera.sketchViewActive()
                    && camera.projectionMode() == ProjectionMode::Orthographic
                    && nearVec(direction, frame.n.x, frame.n.y, frame.n.z, 1e-5f);
            // And therefore the extrusion axis has no screen extent at all.
            unusable = unusable && !cadFeatureViewUsable(direction, frame.n)
                       && cadFeatureViewAxisSine(direction, frame.n) < 1e-4f;
        }
        r.check("CADUXS1C1_02_a_the_sketch_view_is_still_exactly_along_the_support_normal",
                exact);
        r.check("CADUXS1C1_02_b_and_that_is_precisely_why_the_axis_cannot_be_dragged_from_it",
                unusable);
    }

    // CADUXS1C1-03 b: the FALLBACK gives every world plane a usable axis, from
    // the first-project bootstrap where there is no prior view at all.
    {
        bool ok = true;
        const Workplane planes[3] = {Workplane::XY, Workplane::XZ, Workplane::YZ};
        for (int i = 0; i < 3; ++i) {
            const SketchFrame frame = planeFrameOf(planes[i]);
            SketchSession session;
            readyRectangleSession(&session, planes[i], 2.0, 2.0, 1.0);
            CadExtrudeAnchors anchors;
            ok = ok && session.extrudeAnchors(&anchors);
            CameraController::Pose preview;
            // A null `prior` IS the bootstrap: one volatile sketch over an
            // empty scene, opened before any 3D view of a project existed.
            const CadFeatureViewSource source = cadFeatureViewPose(
                sketchPoseOf(frame), nullptr, frame, anchors, &preview);
            ok = ok && source == CadFeatureViewSource::ObliqueFallback;
            const Vec3 installed = cadFeatureViewDirection(preview.yaw, preview.pitch);
            ok = ok && cadFeatureViewUsable(installed, anchors.axis);
            // Inside the orbit's own clamp, so what was measured is what the
            // camera will actually install.
            ok = ok && std::fabs(preview.pitch) <= kPitchLimitRadians;
            // Centred on the work, so the arrow is in frame wherever the
            // sketch was drawn.
            ok = ok && nearVec(preview.target, anchors.base.x, anchors.base.y, anchors.base.z,
                               1e-5f);
        }
        r.check("CADUXS1C1_03_b_the_fallback_makes_the_axis_usable_on_every_world_plane", ok);
    }

    // CADUXS1C1-03 c: XZ is the world-up case. Its support normal is world +Y,
    // which is the gimbal `frameSketchView` exists to bypass, and the fallback
    // has to answer it without one.
    {
        const SketchFrame frame = planeFrameOf(Workplane::XZ);
        const bool normalIsWorldUp = std::fabs(std::fabs(frame.n.y) - 1.0f) < 1e-6f;
        SketchSession session;
        readyRectangleSession(&session, Workplane::XZ, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        CameraController::Pose preview;
        const CadFeatureViewSource source =
            cadFeatureViewPose(sketchPoseOf(frame), nullptr, frame, anchors, &preview);
        const Vec3 installed = cadFeatureViewDirection(preview.yaw, preview.pitch);
        // The tilt is the whole answer: 35.5 degrees off vertical is a pitch of
        // about 54.5, nowhere near the clamp.
        r.check("CADUXS1C1_03_c_a_world_up_support_normal_needs_no_special_case",
                normalIsWorldUp && source == CadFeatureViewSource::ObliqueFallback
                        && cadFeatureViewUsable(installed, anchors.axis)
                        && std::fabs(preview.pitch) < kPitchLimitRadians - 0.2f);
    }

    // CADUXS1C1-03 d: a supported planar FACE, swept right through the band of
    // orientations in which one azimuth's candidate can aim up the meridian and
    // land inside the pitch clamp. The bounded azimuth retry is what carries
    // those, and the proof is that the direction rebuilt from the CLAMPED
    // angles still clears the threshold.
    {
        bool ok = true;
        for (int deg = 0; deg <= 90; deg += 5) {
            const float tilt = static_cast<float>(deg) * 3.14159265358979323846f / 180.0f;
            for (int about = 0; about < 360; about += 45) {
                const float swing =
                    static_cast<float>(about) * 3.14159265358979323846f / 180.0f;
                const SketchFrame frame = tiltedFaceFrame(tilt, swing);
                CadSketch sketch;
                sketch.plane = Workplane::XY;
                SketchRectangle rect;
                rect.center = SketchPoint{0.0, 0.0};
                rect.width = 2.0;
                rect.height = 2.0;
                addSketchEntity(&sketch, rect);
                ClosedProfile profile;
                if (!firstProfileOf(sketch, 1, &profile)) {
                    ok = false;
                    continue;
                }
                ExtrudeFeature extrude;
                extrude.profileEntityId = 1;
                extrude.depth = 1.0;
                extrude.direction = ExtrudeDirection::AlongNormal;
                CadExtrudeAnchors anchors;
                if (!cadExtrudeAnchors(frame, profile, extrude, &anchors)) {
                    ok = false;
                    continue;
                }
                CameraController::Pose preview;
                const CadFeatureViewSource source =
                    cadFeatureViewPose(sketchPoseOf(frame), nullptr, frame, anchors, &preview);
                if (source != CadFeatureViewSource::ObliqueFallback) {
                    ok = false;
                    continue;
                }
                const Vec3 installed = cadFeatureViewDirection(preview.yaw, preview.pitch);
                ok = ok && cadFeatureViewUsable(installed, anchors.axis)
                     && std::fabs(preview.pitch) <= kPitchLimitRadians;
            }
        }
        r.check("CADUXS1C1_03_d_a_face_frame_at_any_orientation_gets_a_usable_view", ok);
    }

    // CADUXS1C1-03 e: the PRIOR view is preferred when it already sees the
    // axis, and REFUSED when it looks down it -- both paths, in one case.
    {
        const SketchFrame frame = planeFrameOf(Workplane::XY);
        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);

        // An ordinary 3D view of the model: the product's own initial pose.
        const CameraController::Pose good = orbitPoseOf(kInitialYaw, kInitialPitch);
        CameraController::Pose fromGood;
        const CadFeatureViewSource keptIt =
            cadFeatureViewPose(sketchPoseOf(frame), &good, frame, anchors, &fromGood);

        // A view already looking straight down the XY normal, which is exactly
        // what the sketch view was: giving it back would give back the problem.
        const CameraController::Pose headOn = orbitPoseOf(0.0f, 0.0f);
        CameraController::Pose fromHeadOn;
        const CadFeatureViewSource refusedIt =
            cadFeatureViewPose(sketchPoseOf(frame), &headOn, frame, anchors, &fromHeadOn);

        const bool keptTheDirection = near2(fromGood.yaw, good.yaw, 1e-6)
                                      && near2(fromGood.pitch, good.pitch, 1e-6)
                                      && near2(fromGood.distance, good.distance, 1e-6);
        const bool movedTheCentre = nearVec(fromGood.target, anchors.base.x, anchors.base.y,
                                            anchors.base.z, 1e-5f);
        const Vec3 fallbackDirection =
            cadFeatureViewDirection(fromHeadOn.yaw, fromHeadOn.pitch);
        r.check("CADUXS1C1_03_e_a_usable_prior_view_is_given_back_re_centred_on_the_work",
                keptIt == CadFeatureViewSource::PriorView && keptTheDirection && movedTheCentre);
        r.check("CADUXS1C1_03_f_a_head_on_prior_view_is_refused_for_the_deterministic_fallback",
                refusedIt == CadFeatureViewSource::ObliqueFallback
                        && cadFeatureViewUsable(fallbackDirection, anchors.axis));
    }

    // CADUXS1C1-03 g: the policy is DETERMINISTIC. One sketch produces one
    // view, every time, because nothing random and no accumulated state enters
    // it.
    {
        const SketchFrame frame = planeFrameOf(Workplane::YZ);
        SketchSession session;
        readyRectangleSession(&session, Workplane::YZ, 3.0, 1.0, 0.5);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        CameraController::Pose a;
        CameraController::Pose b;
        cadFeatureViewPose(sketchPoseOf(frame), nullptr, frame, anchors, &a);
        cadFeatureViewPose(sketchPoseOf(frame), nullptr, frame, anchors, &b);
        r.check("CADUXS1C1_03_g_the_fallback_is_deterministic",
                near2(a.yaw, b.yaw, 0.0) && near2(a.pitch, b.pitch, 0.0)
                        && near2(a.distance, b.distance, 0.0));
    }

    // CADUXS1C1-05 a: the drag WORKS from the view the policy installs, and it
    // means the same thing there as everywhere else -- one metre along the axis
    // is one metre of depth. This is the case `OQ-CAD-UX-01` could not have.
    {
        bool ok = true;
        const Workplane planes[3] = {Workplane::XY, Workplane::XZ, Workplane::YZ};
        for (int i = 0; i < 3; ++i) {
            const SketchFrame frame = planeFrameOf(planes[i]);
            SketchSession session;
            readyRectangleSession(&session, planes[i], 2.0, 2.0, 1.0);
            CadExtrudeAnchors anchors;
            session.extrudeAnchors(&anchors);
            CameraController::Pose preview;
            cadFeatureViewPose(sketchPoseOf(frame), nullptr, frame, anchors, &preview);
            const int w = 1080;
            const int h = 2000;
            const CameraSnapshot camera = snapshotOfPose(preview, w, h);
            // The shaft has real screen extent now: the base and the tip are
            // separated pixels, which is what a hit test needs.
            float bx = 0.0f;
            float by = 0.0f;
            float tx = 0.0f;
            float ty = 0.0f;
            ok = ok && projectWorldToScreen(camera, anchors.base, w, h, &bx, &by)
                 && projectWorldToScreen(camera, anchors.tip, w, h, &tx, &ty);
            ok = ok && std::hypot(tx - bx, ty - by) > 20.0f;
            // The arrow can be taken, and a one-metre axial displacement is one
            // metre of depth.
            CadExtrudeManipulator drag;
            ok = ok && drag.hitTest(anchors, camera, tx, ty, w, h);
            float ex = 0.0f;
            float ey = 0.0f;
            ok = ok && projectWorldToScreen(camera,
                                            vec3Add(anchors.tip, vec3Scale(anchors.axis, 1.0f)),
                                            w, h, &ex, &ey);
            Meters depth = 0.0;
            ok = ok && drag.beginDrag(3, anchors, anchors.primaryIsPositive, camera, tx, ty, w, h)
                 && drag.updateDrag(3, camera, ex, ey, w, h, &depth)
                 && near2(depth, 2.0, 1e-2);
            ok = ok && drag.lastSolve() != AxisSolveStatus::Unresolvable;
        }
        r.check("CADUXS1C1_05_a_the_arrow_is_grabbable_and_a_drag_resolves_from_the_preview",
                ok);
    }

    // CADUXS1C1-05 b: a drag from the preview and a typed value reach the same
    // authored state, and Flip is still a DIRECTION -- the depth stays positive
    // through it.
    {
        const SketchFrame frame = planeFrameOf(Workplane::XY);
        SketchSession dragged;
        readyRectangleSession(&dragged, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        dragged.extrudeAnchors(&anchors);
        CameraController::Pose preview;
        cadFeatureViewPose(sketchPoseOf(frame), nullptr, frame, anchors, &preview);
        const int w = 1080;
        const int h = 2000;
        const CameraSnapshot camera = snapshotOfPose(preview, w, h);
        float tx = 0.0f;
        float ty = 0.0f;
        float ex = 0.0f;
        float ey = 0.0f;
        projectWorldToScreen(camera, anchors.tip, w, h, &tx, &ty);
        projectWorldToScreen(camera, vec3Add(anchors.tip, vec3Scale(anchors.axis, 1.0f)), w, h,
                             &ex, &ey);
        CadExtrudeManipulator drag;
        drag.beginDrag(4, anchors, anchors.primaryIsPositive, camera, tx, ty, w, h);
        Meters depth = 0.0;
        const bool moved = drag.updateDrag(4, camera, ex, ey, w, h, &depth);
        // Through the ONE writer, exactly as the session's own gesture does.
        dragged.setExtrude(depth, dragged.extrude().direction);
        SketchSession typed;
        readyRectangleSession(&typed, Workplane::XY, 2.0, 2.0, 1.0);
        typed.setExtrude(depth, ExtrudeDirection::AlongNormal);
        const bool parity = sameCadBodyState(dragged.candidateState(), typed.candidateState());

        dragged.setExtrude(dragged.extrude().depth, ExtrudeDirection::AgainstNormal);
        const bool flipStillADirection =
            dragged.extrude().direction == ExtrudeDirection::AgainstNormal
            && dragged.extrude().depth > 0.0;
        r.check("CADUXS1C1_05_b_a_preview_drag_and_a_typed_value_are_the_same_authored_state",
                moved && parity && flipStillADirection);
    }

    // CADUXS1C1-09 a: the camera transition is PRESENTATION. The policy writes
    // nothing into the session, and the anchors it was derived from do not
    // depend on it, because no camera enters `cadExtrudeAnchors`.
    {
        const SketchFrame frame = planeFrameOf(Workplane::XY);
        SketchSession untouched;
        readyRectangleSession(&untouched, Workplane::XY, 2.0, 2.0, 1.0);
        const CadBodyState before = untouched.candidateState();

        SketchSession viewed;
        readyRectangleSession(&viewed, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        viewed.extrudeAnchors(&anchors);
        CameraController::Pose preview;
        const CadFeatureViewSource source =
            cadFeatureViewPose(sketchPoseOf(frame), nullptr, frame, anchors, &preview);
        const bool unchanged = sameCadBodyState(viewed.candidateState(), before);

        CadExtrudeAnchors again;
        viewed.extrudeAnchors(&again);
        const bool anchorsStable =
            nearVec(again.base, anchors.base.x, anchors.base.y, anchors.base.z, 0.0f)
            && nearVec(again.tip, anchors.tip.x, anchors.tip.y, anchors.tip.z, 0.0f);
        r.check("CADUXS1C1_09_a_the_camera_transition_writes_no_authored_value",
                source == CadFeatureViewSource::ObliqueFallback && unchanged && anchorsStable);
    }

    // CADUXS1C1-09 b: it FAILS CLOSED. Given nothing it can work from, the
    // policy installs no view and says so rather than guessing one -- the
    // manipulator's own hold-the-last-good-value rule, one level up.
    {
        const SketchFrame frame = planeFrameOf(Workplane::XY);
        CadExtrudeAnchors invalid;  // valid == false
        CameraController::Pose out;
        const CadFeatureViewSource none =
            cadFeatureViewPose(sketchPoseOf(frame), nullptr, frame, invalid, &out);

        SketchSession session;
        readyRectangleSession(&session, Workplane::XY, 2.0, 2.0, 1.0);
        CadExtrudeAnchors anchors;
        session.extrudeAnchors(&anchors);
        SketchFrame degenerate = frame;
        degenerate.n = Vec3{0.0f, 0.0f, 0.0f};
        CameraController::Pose out2;
        const CadFeatureViewSource alsoNone =
            cadFeatureViewPose(sketchPoseOf(frame), nullptr, degenerate, anchors, &out2);
        r.check("CADUXS1C1_09_b_an_unanswerable_view_is_refused_rather_than_guessed",
                none == CadFeatureViewSource::Unavailable
                        && alsoNone == CadFeatureViewSource::Unavailable);
    }
}

void testDataContract(Recorder& r) {
    // A project of straight geometry stays at v1: no existing file moves.
    {
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(rectBody(2.0, 1.0, 1.0)));
        uint16_t version = 0;
        r.check("CADUXR1_37_a_curveless_cad_project_still_writes_CADB_v1",
                !bytes.empty() && cadSectionVersion(bytes, &version)
                        && version == kCadSectionVersion);
    }
    // A body carrying an arc forces v3, and only that body's presence does.
    {
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(arcAndLineBody()));
        uint16_t version = 0;
        r.check("CADUXR1_38_a_body_with_a_curve_writes_CADB_v3",
                !bytes.empty() && cadSectionVersion(bytes, &version)
                        && version == kCadSectionVersionV3);
    }
    // The round trip is bit-exact for both curve kinds, and re-encoding the
    // decoded document reproduces the same bytes: a deterministic writer.
    {
        for (int kind = 0; kind < 2; ++kind) {
            const CadBodyState state = kind == 0 ? arcAndLineBody() : splineAndLineBody();
            const ProjectDocument original = documentFor(state);
            const std::vector<uint8_t> bytes = encodeProjectV1(original);
            ProjectDocument decoded;
            const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &decoded);
            const std::vector<uint8_t> again = encodeProjectV1(decoded);
            r.check(kind == 0 ? "CADUXR1_26_an_arc_round_trips_bit_exactly"
                              : "CADUXR1_29_a_spline_round_trips_bit_exactly",
                    why == ProjectCodecStatus::Ok && sameProjectDocument(original, decoded)
                            && again == bytes);
        }
    }
    // A curve code inside a section that declared itself v1 or v2 is a
    // malformed file, refused rather than read.
    {
        std::vector<uint8_t> bytes = encodeProjectV1(documentFor(arcAndLineBody()));
        bool patched = false;
        for (size_t i = 0; i + 8 <= bytes.size(); ++i) {
            if (bytes[i] == 'C' && bytes[i + 1] == 'A' && bytes[i + 2] == 'D'
                && bytes[i + 3] == 'B') {
                bytes[i + 4] = static_cast<uint8_t>(kCadSectionVersionV2);
                patched = true;
                break;
            }
        }
        ProjectDocument decoded;
        const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &decoded);
        // The CRC guards the section too, so the refusal may name either the
        // checksum or the value; what matters is that it is REFUSED and that
        // nothing was written.
        r.check("CADUXR1_37_a_curve_inside_a_v2_section_is_refused",
                patched && why != ProjectCodecStatus::Ok && !decoded.hasCad);
    }
    // A file whose sketch closes no profile is refused, curve or not.
    {
        CadBodyState open = arcAndLineBody();
        open.sketch.entities.pop_back();  // drop the closing line
        ProjectDocument document = documentFor(rectBody(1.0, 1.0, 1.0));
        if (!document.cad.bodies.empty()) {
            document.cad.bodies[0].state = open;
        }
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(document, &why);
        r.check("CADUXR1_38_a_curve_sketch_that_closes_nothing_is_refused",
                bytes.empty() && why == ProjectCodecStatus::InvalidSemanticValue);
    }

    // ---------------------------------------------------------------------
    // The independent corpus: this encoder against the PowerShell one
    // ---------------------------------------------------------------------
    //
    // The digests below are what `scripts/build-forge-corpus.ps1` wrote into
    // `testdata/forge/v1/` and what `DATA_PACKAGE_SPEC.md` §7d records. The two
    // implementations share no line: one is the production codec, the other is
    // written from the spec text. Agreement is the format being a
    // SPECIFICATION; a change nobody meant to make fails here, with the new
    // value beside it, rather than silently invalidating the corpus.
    {
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> arcBytes = encodeProjectV1(cadArcProfileDocument(), &why);
        r.check("CADUXR1_38_cad_arc_profile_fixture_matches_the_committed_digest",
                why == ProjectCodecStatus::Ok
                        && projectFixtureSha256Hex(arcBytes)
                                   == "580a47dcfa305ddec34c2ed7831ba88b7a05659008e7944285ca2a70fc"
                                      "02b8cd");
        const std::vector<uint8_t> splineBytes = encodeProjectV1(cadSplineProfileDocument(), &why);
        r.check("CADUXR1_38_cad_spline_profile_fixture_matches_the_committed_digest",
                why == ProjectCodecStatus::Ok
                        && projectFixtureSha256Hex(splineBytes)
                                   == "e3ff4f7be2529a30f040bd3d699b46df9792473b88755494144c15697b"
                                      "19261a");
        const std::vector<uint8_t> mixedBytes =
                encodeProjectV1(cadMixedCurveProfileDocument(), &why);
        r.check("CADUXR1_38_cad_mixed_curve_profile_fixture_matches_the_committed_digest",
                why == ProjectCodecStatus::Ok
                        && projectFixtureSha256Hex(mixedBytes)
                                   == "3e2fa16f05e353f1745a36e165aedadbb5d5378db0c039167293aececa"
                                      "d7078d");
        const std::vector<uint8_t> faceCurveBytes = encodeProjectV1(cadFaceCurveDocument(), &why);
        r.check("CADUXR1_38_cad_face_curve_fixture_matches_the_committed_digest",
                why == ProjectCodecStatus::Ok
                        && projectFixtureSha256Hex(faceCurveBytes)
                                   == "0a8218f0aa86cfb7cdcf7781c72864e76e9065e2f7a788d5b2cf4e6fc1"
                                      "250ad0");

        // The two corrupt files, patched from their valid counterparts.
        const double collinear[6] = {-1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
        const std::vector<uint8_t> badArcBytes = patchFirstCadEntityValues(
                encodeProjectV1(cadBadArcBaseDocument(), &why), 0, 0, collinear, 6);
        r.check("CADUXR1_38_cad_bad_arc_fixture_matches_the_committed_digest",
                !badArcBytes.empty()
                        && projectFixtureSha256Hex(badArcBytes)
                                   == "628d74fdbf3cf9082ef4869f9b948acef81c6a14517703b707896602e7"
                                      "c4b418");
        const double meetingEnd[2] = {-1.0, 0.0};
        const std::vector<uint8_t> badSplineBytes = patchFirstCadEntityValues(
                encodeProjectV1(cadBadSplineBaseDocument(), &why), 4, 6, meetingEnd, 2);
        r.check("CADUXR1_38_cad_bad_spline_fixture_matches_the_committed_digest",
                !badSplineBytes.empty()
                        && projectFixtureSha256Hex(badSplineBytes)
                                   == "f5437366d894032e97b2e49d3027726fa2bc739bda747f05f3b1eaf8c9"
                                      "ed714d");

        // And both are REFUSED by the ordinary decoder, by name: the whole
        // point of a corrupt fixture is that only the semantic check catches
        // it, so a reader that opened one would have a real defect.
        ProjectDocument decoded;
        const ProjectCodecStatus arcWhy =
                decodeProject(badArcBytes.data(), badArcBytes.size(), &decoded);
        ProjectDocument decodedSpline;
        const ProjectCodecStatus splineWhy =
                decodeProject(badSplineBytes.data(), badSplineBytes.size(), &decodedSpline);
        r.check("CADUXR1_38_the_corrupt_curve_fixtures_fail_closed_by_name",
                arcWhy == ProjectCodecStatus::InvalidSemanticValue
                        && splineWhy == ProjectCodecStatus::InvalidSemanticValue
                        && !decoded.hasCad && !decodedSpline.hasCad);
    }
}

void measurePerformance() {
    using Clock = std::chrono::steady_clock;
    const auto micros = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
    };
    const SketchEntity arc(1, unitSemicircle());
    std::vector<SketchPoint> points;
    auto t0 = Clock::now();
    for (int i = 0; i < 200; ++i) {
        tessellateSketchCurve(arc, &points);
    }
    auto t1 = Clock::now();
    std::vector<SketchPoint> splinePoints = {SketchPoint{-1.0, 0.0}, SketchPoint{-0.5, 0.8},
                                             SketchPoint{0.5, 0.8}, SketchPoint{1.0, 0.0}};
    const SketchEntity spline(1, SketchSpline{splinePoints});
    for (int i = 0; i < 200; ++i) {
        tessellateSketchCurve(spline, &points);
    }
    auto t2 = Clock::now();
    const CadBodyState mixed = arcAndLineBody();
    for (int i = 0; i < 50; ++i) {
        extractClosedProfiles(mixed.sketch);
    }
    auto t3 = Clock::now();
    ConstructionMesh mesh;
    for (int i = 0; i < 50; ++i) {
        generateCadMesh(mixed, &mesh);
    }
    auto t4 = Clock::now();
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  "arc_tess=%lldus/200 spline_tess=%lldus/200 curve_profile=%lldus/50 "
                  "curve_regen=%lldus/50",
                  static_cast<long long>(micros(t0, t1)), static_cast<long long>(micros(t1, t2)),
                  static_cast<long long>(micros(t2, t3)), static_cast<long long>(micros(t3, t4)));
    g_performance = buffer;
}

}  // namespace

int runSketchUxSelfTests(SketchUxSelfTestResult* out, int maxOut) {
    if (out == nullptr || maxOut <= 0) {
        return 0;
    }
    Recorder r{out, maxOut};
    testArcDomain(r);
    testSplineDomain(r);
    testCurveProfiles(r);
    testLineDimension(r);
    testOrientationNavigator(r);
    testEditSketch(r);
    // CAD-UX-S1: the canvas extrude manipulator. Widened into this suite rather
    // than opened beside it, because the arrow, the exact depth at it and the
    // retained-sketch access are sketch UX in exactly the sense the rest of this
    // file already is.
    testCanvasExtrudeAnchors(r);
    testCanvasExtrudeFlip(r);
    testCanvasExtrudeScale(r);
    testCanvasExtrudeDrag(r);
    testCanvasExtrudeSessionGesture(r);
    testCanvasExtrudeParityAndPurity(r);
    testCanvasExtrudeRetainedSketch(r);
    // CAD-UX-S1-C1: the view the staged extrusion is adjusted through. Beside
    // the manipulator cases rather than in a suite of its own, because the
    // question it answers -- can the arrow be reached -- is theirs.
    testFeaturePreviewView(r);
    testFingerprintCoversCurves(r);
    testDataContract(r);
    measurePerformance();
    return r.n;
}

const char* sketchUxPerformanceReport() { return g_performance.c_str(); }

}  // namespace forgeshape
