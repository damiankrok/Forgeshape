#include "forgeshape_mirror_selftest.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "forgeshape_body_commands.h"
#include "forgeshape_body_mirror.h"
#include "forgeshape_cad_body.h"
#include "forgeshape_construction.h"
#include "forgeshape_history.h"
#include "forgeshape_imported_mesh.h"
#include "forgeshape_mesh.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

struct Recorder {
    MirrorSelfTestResult* out;
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

// The tolerance every geometric assertion in this suite is stated at.
//
// The reflected orientation is carried through `rotationMatrixFromEuler` and
// back through `eulerFromRotationMatrix`, both of which work in the FLOAT Mat4
// the renderer and the picker consume. A world point recomposed from the
// re-derived matrix therefore agrees to about a part in 10^6 of its magnitude,
// not to a double ULP -- "exact within the current floating-point tolerance",
// which is what this stage asks for and what an honest assertion says.
constexpr double kWorldEpsilon = 1e-4;

bool near(double a, double b, double epsilon) { return std::fabs(a - b) <= epsilon; }

// Where a LOCAL point sits in the world under one placement.
//
// Written here from the stated convention `Model = T * Rz * Ry * Rx * S` rather
// than borrowed from the code under test, so the equivalence cases are checked
// against the contract instead of against the implementation.
void worldPointOf(const TransformValues& v, double lx, double ly, double lz, double* out) {
    const Mat4 rotation = rotationMatrixFromEuler(eulerOf(v));
    const double sx = lx * v.scaleX;
    const double sy = ly * v.scaleY;
    const double sz = lz * v.scaleZ;
    for (int row = 0; row < 3; ++row) {
        out[row] = static_cast<double>(rotation.m[0 * 4 + row]) * sx
                 + static_cast<double>(rotation.m[1 * 4 + row]) * sy
                 + static_cast<double>(rotation.m[2 * 4 + row]) * sz;
    }
    out[0] += v.positionX;
    out[1] += v.positionY;
    out[2] += v.positionZ;
}

// det of the upper-left 3x3, in double, read off the float matrix.
double determinant3(const Mat4& m) {
    const double a = m.m[0], b = m.m[4], c = m.m[8];
    const double d = m.m[1], e = m.m[5], f = m.m[9];
    const double g = m.m[2], h = m.m[6], i = m.m[10];
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

TransformValues placementOf(double px, double py, double pz, double rx, double ry, double rz,
                            double sx, double sy, double sz) {
    TransformValues v;
    v.positionX = px;
    v.positionY = py;
    v.positionZ = pz;
    v.rotationX = rx;
    v.rotationY = ry;
    v.rotationZ = rz;
    v.scaleX = sx;
    v.scaleY = sy;
    v.scaleZ = sz;
    return v;
}

double positionAxis(const TransformValues& v, int axis) {
    return axis == 0 ? v.positionX : (axis == 1 ? v.positionY : v.positionZ);
}

bool sameScale(const TransformValues& a, const TransformValues& b) {
    return a.scaleX == b.scaleX && a.scaleY == b.scaleY && a.scaleZ == b.scaleZ;
}

bool sameValues(const TransformValues& a, const TransformValues& b) {
    return a.positionX == b.positionX && a.positionY == b.positionY && a.positionZ == b.positionZ
        && a.rotationX == b.rotationX && a.rotationY == b.rotationY && a.rotationZ == b.rotationZ
        && sameScale(a, b);
}

// The suite's own scene and history.
struct Fixture {
    ConstructionScene scene;
    ConstructionHistory history{scene};

    Fixture() {
        publishConstructionObject(scene.activeBody().construction(),
                                  scene.activeBody().meshStore());
    }

    SceneObject& body() { return scene.activeBody(); }
    ObjectId id() { return scene.activeBody().objectId(); }

    void setShape(const PrimitiveSpec& spec) {
        ScopedConstructionEdit edit(history);
        applyPrimitive(body().construction(), body().meshStore(), spec);
    }

    void setPlacement(const TransformValues& values) {
        ScopedConstructionEdit edit(history);
        applyTransformValues(body().transform(), values);
    }
};

// The three world planes and the axis each one reflects.
struct PlaneCase {
    MirrorPlane plane;
    int axis;
};
const PlaneCase kPlanes[3] = {
    {MirrorPlane::Xy, 2},
    {MirrorPlane::Xz, 1},
    {MirrorPlane::Yz, 0},
};

// Every placement the geometric cases are run over: at the origin, moved off
// it, turned about each axis alone and about all three at once, non-uniformly
// scaled, standing exactly ON the plane, crossing it, at the extremes of a
// valid positive scale, and past a whole turn. Section 12's whole list in one
// table, so a new case is a row and never a new copy of the assertions.
const TransformValues kPlacements[] = {
    placementOf(0, 0, 0, 0, 0, 0, 1, 1, 1),
    placementOf(3.5, -2.25, 1.75, 0, 0, 0, 1, 1, 1),
    placementOf(1.0, 2.0, -3.0, 37.0, 0, 0, 1, 1, 1),
    placementOf(1.0, 2.0, -3.0, 0, -64.5, 0, 1, 1, 1),
    placementOf(1.0, 2.0, -3.0, 0, 0, 118.25, 1, 1, 1),
    placementOf(2.5, -1.5, 0.75, 31.0, -47.5, 118.25, 1.5, 2.5, 0.5),
    placementOf(0.0, 0.0, 0.0, 17.0, -29.0, 63.0, 2.0, 0.5, 3.0),
    placementOf(0.25, -0.125, 0.0625, 12.0, 84.0, -175.0, 4.0, 4.0, 4.0),
    placementOf(0.5, 0.5, 0.5, 45.0, 45.0, 45.0, 1e-4, 1.0, 1e4),
    placementOf(-1.0, 0.5, 2.0, 400.0, -390.0, 725.0, 1.0, 1.0, 1.0),
};
constexpr int kPlacementCount = static_cast<int>(sizeof(kPlacements) / sizeof(kPlacements[0]));

// The six primitives, deliberately non-cubic so a wrong axis is visible in the
// numbers themselves.
std::vector<PrimitiveSpec> everyPrimitive() {
    return {
        PrimitiveSpec::forBox(2.0, 1.0, 0.5),
        PrimitiveSpec::forCylinder(1.5, 4.0),
        PrimitiveSpec::forSphere(2.5),
        PrimitiveSpec::forCone(3.0, 2.0),
        PrimitiveSpec::forCapsule(1.0, 3.0),
        PrimitiveSpec::forPlane(4.0, 2.0),
    };
}

// Whether every point of `mesh` reflected through the body's own local X axis
// is itself a point of `mesh`. THE symmetry the positive-scale contract rests
// on -- see `forgeshape_body_mirror.h`.
bool localGeometryIsXSymmetric(const ConstructionMesh& mesh) {
    for (const MeshVertex& v : mesh.vertices) {
        bool found = false;
        for (const MeshVertex& w : mesh.vertices) {
            if (near(w.position[0], -static_cast<double>(v.position[0]), 1e-5)
                && near(w.position[1], v.position[1], 1e-5)
                && near(w.position[2], v.position[2], 1e-5)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

// Whether the mirrored body's WORLD point set is the reflection of the
// source's. Set equality, because the two bodies generate the same vertices in
// a different order -- which is the only honest statement to make about them.
bool worldGeometryIsReflection(const ConstructionMesh& mesh, const TransformValues& source,
                               const TransformValues& mirrored, int axis) {
    const size_t count = mesh.vertices.size();
    std::vector<double> reflectedSource(count * 3);
    std::vector<double> mirroredWorld(count * 3);
    for (size_t i = 0; i < count; ++i) {
        const MeshVertex& v = mesh.vertices[i];
        worldPointOf(source, v.position[0], v.position[1], v.position[2],
                     reflectedSource.data() + i * 3);
        reflectedSource[i * 3 + axis] = -reflectedSource[i * 3 + axis];
        worldPointOf(mirrored, v.position[0], v.position[1], v.position[2],
                     mirroredWorld.data() + i * 3);
    }
    for (size_t i = 0; i < count; ++i) {
        const double magnitude = 1.0 + std::fabs(mirroredWorld[i * 3 + 0])
                               + std::fabs(mirroredWorld[i * 3 + 1])
                               + std::fabs(mirroredWorld[i * 3 + 2]);
        bool found = false;
        for (size_t j = 0; j < count && !found; ++j) {
            found = near(mirroredWorld[i * 3 + 0], reflectedSource[j * 3 + 0],
                         kWorldEpsilon * magnitude)
                 && near(mirroredWorld[i * 3 + 1], reflectedSource[j * 3 + 1],
                         kWorldEpsilon * magnitude)
                 && near(mirroredWorld[i * 3 + 2], reflectedSource[j * 3 + 2],
                         kWorldEpsilon * magnitude);
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

ImportedMesh triangleMesh() {
    const std::vector<float> positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    const std::vector<float> normals{0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    const std::vector<uint32_t> indices{0, 1, 2};
    const std::vector<ImportedMeshBatch> batches{ImportedMeshBatch{0, 3, false}};
    return ImportedMesh::build(positions, normals, indices, batches);
}

CadBodyState rectCadBody() {
    CadBodyState state;
    cadBaseSketch(state).plane = Workplane::XY;
    SketchRectangle rect;
    rect.center = SketchPoint{0.0, 0.0};
    rect.width = 2.0;
    rect.height = 1.0;
    addSketchEntity(&cadBaseSketch(state), rect);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = 1.5;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

}  // namespace

int runMirrorSelfTests(MirrorSelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};

    // -----------------------------------------------------------------------
    // MIRROR01-01..03  each plane reflects exactly its own world axis
    // -----------------------------------------------------------------------
    //
    // Asserted EXACTLY, not within a tolerance: a position reflection is one
    // sign flip and nothing touches the other two components.
    {
        const TransformValues source = placementOf(3.5, -2.25, 1.75, 0, 0, 0, 1, 1, 1);
        TransformValues xy{};
        TransformValues xz{};
        TransformValues yz{};
        r.check("MIRROR01-01 XY solves",
                mirrorPlacement(source, MirrorPlane::Xy, &xy) == MirrorStatus::Ok);
        r.check("MIRROR01-01 XY reflects Z and leaves X and Y alone",
                xy.positionZ == -1.75 && xy.positionX == 3.5 && xy.positionY == -2.25);
        r.check("MIRROR01-02 XZ solves",
                mirrorPlacement(source, MirrorPlane::Xz, &xz) == MirrorStatus::Ok);
        r.check("MIRROR01-02 XZ reflects Y and leaves X and Z alone",
                xz.positionY == 2.25 && xz.positionX == 3.5 && xz.positionZ == 1.75);
        r.check("MIRROR01-03 YZ solves",
                mirrorPlacement(source, MirrorPlane::Yz, &yz) == MirrorStatus::Ok);
        r.check("MIRROR01-03 YZ reflects X and leaves Y and Z alone",
                yz.positionX == -3.5 && yz.positionY == -2.25 && yz.positionZ == 1.75);

        TransformValues onPlane{};
        r.check("MIRROR01-01 a body standing on the plane solves",
                mirrorPlacement(TransformValues{}, MirrorPlane::Xy, &onPlane)
                    == MirrorStatus::Ok);
        r.check("MIRROR01-01 a zero coordinate reflects to +0.0, never -0.0",
                onPlane.positionZ == 0.0 && !std::signbit(onPlane.positionZ));

        r.check("MIRROR01-01 the plane/axis mapping is XY->Z, XZ->Y, YZ->X",
                mirrorReflectedAxis(MirrorPlane::Xy) == 2
                    && mirrorReflectedAxis(MirrorPlane::Xz) == 1
                    && mirrorReflectedAxis(MirrorPlane::Yz) == 0);
        MirrorPlane fromIndex = MirrorPlane::Xy;
        r.check("MIRROR01-01 the transport index round-trips and refuses out of range",
                mirrorPlaneFromIndex(1, &fromIndex) && fromIndex == MirrorPlane::Xz
                    && mirrorPlaneIndex(MirrorPlane::Yz) == 2
                    && !mirrorPlaneFromIndex(3, &fromIndex)
                    && !mirrorPlaneFromIndex(-1, &fromIndex));
    }

    // -----------------------------------------------------------------------
    // MIRROR01-04  the mirrored rotation is PROPER for every plane and pose
    // MIRROR01-05  Absolute Scale is carried across unchanged and positive
    // -----------------------------------------------------------------------
    {
        bool allProper = true;
        bool allScaleKept = true;
        bool allPositionReflected = true;
        for (int p = 0; p < 3; ++p) {
            for (int i = 0; i < kPlacementCount; ++i) {
                const TransformValues& source = kPlacements[i];
                TransformValues mirrored{};
                if (mirrorPlacement(source, kPlanes[p].plane, &mirrored) != MirrorStatus::Ok) {
                    allProper = false;
                    continue;
                }
                if (!near(determinant3(rotationMatrixFromEuler(eulerOf(mirrored))), 1.0, 1e-4)) {
                    allProper = false;
                }
                if (!sameScale(source, mirrored) || mirrored.scaleX <= 0.0
                    || mirrored.scaleY <= 0.0 || mirrored.scaleZ <= 0.0) {
                    allScaleKept = false;
                }
                const int axis = kPlanes[p].axis;
                for (int a = 0; a < 3; ++a) {
                    const double expected = a == axis
                        ? (positionAxis(source, a) == 0.0 ? 0.0 : -positionAxis(source, a))
                        : positionAxis(source, a);
                    if (positionAxis(mirrored, a) != expected) {
                        allPositionReflected = false;
                    }
                }
            }
        }
        r.check("MIRROR01-04 det(R') is +1 for every plane and every pose", allProper);
        r.check("MIRROR01-05 Absolute Scale is unchanged and strictly positive throughout",
                allScaleKept);
        r.check("MIRROR01-05 the position reflects exactly, on every plane and pose",
                allPositionReflected);
    }

    // -----------------------------------------------------------------------
    // MIRROR01-06  the algebraic identity, with no symmetry assumed
    // -----------------------------------------------------------------------
    //
    // `Model_mirror(q) == F * Model_source(Qx * q)` for ARBITRARY local points,
    // including ones no primitive generates. This is the claim the whole
    // contract rests on, and it holds for any q at all -- so a failure here is
    // the arithmetic being wrong rather than a primitive being asymmetric.
    {
        const double probes[6][3] = {
            {0.0, 0.0, 0.0},    {1.0, 0.0, 0.0},     {0.0, 1.0, 0.0},
            {0.0, 0.0, 1.0},    {0.37, -2.5, 4.25},  {-8.75, 0.125, -0.5},
        };
        bool identityHolds = true;
        for (int p = 0; p < 3; ++p) {
            const int axis = kPlanes[p].axis;
            for (int i = 0; i < kPlacementCount; ++i) {
                const TransformValues& source = kPlacements[i];
                TransformValues mirrored{};
                if (mirrorPlacement(source, kPlanes[p].plane, &mirrored) != MirrorStatus::Ok) {
                    identityHolds = false;
                    continue;
                }
                for (const auto& q : probes) {
                    double lhs[3];
                    double rhs[3];
                    worldPointOf(mirrored, q[0], q[1], q[2], lhs);
                    // Qx negates the LOCAL x; F then negates the world axis.
                    worldPointOf(source, -q[0], q[1], q[2], rhs);
                    rhs[axis] = -rhs[axis];
                    const double magnitude =
                        1.0 + std::fabs(rhs[0]) + std::fabs(rhs[1]) + std::fabs(rhs[2]);
                    for (int a = 0; a < 3; ++a) {
                        if (!near(lhs[a], rhs[a], kWorldEpsilon * magnitude)) {
                            identityHolds = false;
                        }
                    }
                }
            }
        }
        r.check("MIRROR01-06 Model_mirror(q) == F * Model_source(Qx*q) for every plane, pose and "
                "probe",
                identityHolds);
    }

    // -----------------------------------------------------------------------
    // MIRROR01-07  a BOX's world geometry is the reflection of the original's
    // MIRROR01-08  and so is every other current Construction primitive
    // -----------------------------------------------------------------------
    //
    // Parameterized over the six primitives rather than one suite each, which
    // is what this stage asks for -- and it asserts the LOCAL symmetry too,
    // because that is the property which makes the world equivalence true
    // rather than a coincidence of the poses chosen.
    {
        const std::vector<PrimitiveSpec> specs = everyPrimitive();
        bool boxLocalSymmetric = true;
        bool boxWorldEquivalent = true;
        bool allLocalSymmetric = true;
        bool allWorldEquivalent = true;
        for (size_t s = 0; s < specs.size(); ++s) {
            Fixture f;
            f.setShape(specs[s]);
            const ConstructionMesh mesh = f.body().construction().generateMesh();
            const bool symmetric = localGeometryIsXSymmetric(mesh);
            if (s == 0) {
                boxLocalSymmetric = symmetric;
            }
            if (!symmetric) {
                allLocalSymmetric = false;
            }
            for (int p = 0; p < 3; ++p) {
                for (int i = 0; i < kPlacementCount; ++i) {
                    const TransformValues& source = kPlacements[i];
                    TransformValues mirrored{};
                    const bool equivalent =
                        mirrorPlacement(source, kPlanes[p].plane, &mirrored) == MirrorStatus::Ok
                        && worldGeometryIsReflection(mesh, source, mirrored, kPlanes[p].axis);
                    if (s == 0 && !equivalent) {
                        boxWorldEquivalent = false;
                    }
                    if (!equivalent) {
                        allWorldEquivalent = false;
                    }
                }
            }
        }
        r.check("MIRROR01-07 a box's local geometry is symmetric under Qx", boxLocalSymmetric);
        r.check("MIRROR01-07 a box's mirrored world geometry IS the reflection of the source's",
                boxWorldEquivalent);
        r.check("MIRROR01-08 every current Construction primitive is symmetric under Qx",
                allLocalSymmetric);
        r.check("MIRROR01-08 every primitive's mirrored world geometry IS the reflection, on "
                "every plane and pose",
                allWorldEquivalent);
    }

    // -----------------------------------------------------------------------
    // MIRROR01-09  a fresh ObjectId, an exact source clone, an untouched source
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forCylinder(1.5, 4.0));
        const TransformValues start =
            placementOf(2.5, -1.5, 0.75, 31.0, -47.5, 118.25, 1.5, 2.5, 0.5);
        f.setPlacement(start);
        {
            ScopedConstructionEdit edit(f.history);
            f.body().setName("Bracket");
        }
        const ObjectId sourceId = f.id();
        const ConstructionObjectState sourceShape = f.body().construction().captureState();
        const MeshRevision sourceRevision = f.body().meshStore().currentRevision();

        MirrorBodyReport report;
        const BodyCommandStatus status =
            mirrorSceneBody(sourceId, MirrorPlane::Yz, f.scene, f.history, &report);
        r.check("MIRROR01-09 the mirror is accepted", status == BodyCommandStatus::Ok);
        r.check("MIRROR01-09 a second body exists and is the active one",
                f.scene.bodyCount() == 2 && f.scene.activeBody().objectId() == report.newBodyId);
        r.check("MIRROR01-09 the new ObjectId is fresh",
                report.newBodyId != kNoObject && report.newBodyId != sourceId);
        r.check("MIRROR01-09 the reflection is appended at the end of scene order",
                report.newIndex == 1 && report.bodyCount == 2);

        const SceneObject* source = f.scene.findBody(sourceId);
        const SceneObject* reflection = f.scene.findBody(report.newBodyId);
        r.check("MIRROR01-09 both bodies resolve", source != nullptr && reflection != nullptr);
        if (source != nullptr && reflection != nullptr) {
            r.check("MIRROR01-09 the SOURCE is unchanged in placement, shape and revision",
                    sameValues(source->transform().values(), start)
                        && sameConstructionShape(source->construction().captureState(),
                                                 sourceShape)
                        && source->meshStore().currentRevision() == sourceRevision);
            r.check("MIRROR01-09 the reflection carries the source's shape exactly",
                    sameConstructionShape(reflection->construction().captureState(), sourceShape));
            r.check("MIRROR01-09 the reflection is a Construction Body with its own published "
                    "mesh",
                    reflection->hasConstructionSource()
                        && reflection->meshStore().currentRevision() != kNoMeshRevision);
            TransformValues expected{};
            mirrorPlacement(start, MirrorPlane::Yz, &expected);
            r.check("MIRROR01-09 the reflection wears exactly the solved placement",
                    sameValues(reflection->transform().values(), expected));
            r.check("MIRROR01-09 the reflection is named through the shared derived-name rule",
                    reflection->name() == "Bracket Mirror");
            r.check("MIRROR01-09 the reflection carries no Frozen Sculpt Mesh",
                    !reflection->frozenSculpt().mesh.frozen());
        }

        // Visibility and lock follow Stage 018A's Duplicate policy, and the
        // source keeps both.
        {
            ScopedConstructionEdit edit(f.history);
            f.scene.findBody(sourceId)->setVisible(false);
            f.scene.findBody(sourceId)->setLocked(true);
        }
        MirrorBodyReport second;
        mirrorSceneBody(sourceId, MirrorPlane::Xz, f.scene, f.history, &second);
        const SceneObject* inherited = f.scene.findBody(second.newBodyId);
        r.check("MIRROR01-09 a hidden and locked source produces a hidden and locked reflection",
                inherited != nullptr && !inherited->visible() && inherited->locked());
        r.check("MIRROR01-09 the source keeps its own visibility and lock",
                !f.scene.findBody(sourceId)->visible() && f.scene.findBody(sourceId)->locked());
        r.check("MIRROR01-09 a second reflection gets its own disambiguated name",
                inherited != nullptr && inherited->name() == "Bracket Mirror 2");
        r.check("MIRROR01-09 Duplicate's own name is unchanged by the shared rule",
                duplicateBodyName("Bracket", f.scene) == "Bracket copy");
    }

    // -----------------------------------------------------------------------
    // MIRROR01-10  one history step; Undo removes only the mirror; Redo
    //              restores the SAME ObjectId and the same everything
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        f.setPlacement(placementOf(1.0, 2.0, -3.0, 15.0, 25.0, 35.0, 1.0, 2.0, 3.0));
        const ObjectId sourceId = f.id();
        const size_t depthBefore = f.history.undoDepth();

        MirrorBodyReport report;
        mirrorSceneBody(sourceId, MirrorPlane::Xy, f.scene, f.history, &report);
        r.check("MIRROR01-10 one Mirror records exactly one step",
                f.history.undoDepth() == depthBefore + 1);

        const SceneObject* created = f.scene.findBody(report.newBodyId);
        const TransformValues mirroredPlacement =
            created != nullptr ? created->transform().values() : TransformValues{};
        const std::string mirroredName = created != nullptr ? created->name() : std::string();

        r.check("MIRROR01-10 undo is available", f.history.canUndo());
        f.history.undo();
        r.check("MIRROR01-10 undo removes ONLY the mirror",
                f.scene.bodyCount() == 1 && f.scene.findBody(report.newBodyId) == nullptr
                    && f.scene.findBody(sourceId) != nullptr);
        r.check("MIRROR01-10 undo restores the previous active body",
                f.scene.activeBody().objectId() == sourceId);

        r.check("MIRROR01-10 redo is available", f.history.canRedo());
        f.history.redo();
        const SceneObject* restored = f.scene.findBody(report.newBodyId);
        r.check("MIRROR01-10 redo restores the SAME mirrored ObjectId",
                restored != nullptr && f.scene.bodyCount() == 2);
        if (restored != nullptr) {
            r.check("MIRROR01-10 redo restores the same placement, name and source",
                    sameValues(restored->transform().values(), mirroredPlacement)
                        && restored->name() == mirroredName
                        && restored->hasConstructionSource());
            r.check("MIRROR01-10 the mirrored body is active again after redo",
                    f.scene.activeBody().objectId() == report.newBodyId);
        }

        // The allocator is never rolled back: a body created after an undone
        // mirror may not be handed the mirror's id.
        f.history.undo();
        SceneObject& later = f.scene.addBody();
        r.check("MIRROR01-10 an undone mirror's ObjectId is never reused",
                later.objectId() != report.newBodyId);
    }

    // -----------------------------------------------------------------------
    // MIRROR01-11  every ineligible case refuses by name and changes nothing
    // -----------------------------------------------------------------------
    {
        Fixture f;
        f.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        const size_t depthBefore = f.history.undoDepth();
        const size_t countBefore = f.scene.bodyCount();

        SceneObject* imported = f.scene.addImportedBody(triangleMesh(), "head_low");
        r.check("MIRROR01-11 the imported fixture body was created", imported != nullptr);
        MirrorBodyReport importedReport;
        const BodyCommandStatus importedStatus =
            imported == nullptr
                ? BodyCommandStatus::UnknownBody
                : mirrorSceneBody(imported->objectId(), MirrorPlane::Yz, f.scene, f.history,
                                  &importedReport);
        r.check("MIRROR01-11 an Imported Mesh is refused by name",
                importedStatus == BodyCommandStatus::RefusedNotMirrorable
                    && importedReport.eligibility == MirrorEligibility::NotConstruction);

        SceneObject* cad = f.scene.addCadBody(rectCadBody());
        r.check("MIRROR01-11 the CAD fixture body was created", cad != nullptr);
        MirrorBodyReport cadReport;
        const BodyCommandStatus cadStatus =
            cad == nullptr
                ? BodyCommandStatus::UnknownBody
                : mirrorSceneBody(cad->objectId(), MirrorPlane::Xy, f.scene, f.history,
                                  &cadReport);
        r.check("MIRROR01-11 a CAD Body is refused by name",
                cadStatus == BodyCommandStatus::RefusedNotMirrorable
                    && cadReport.eligibility == MirrorEligibility::NotCad);

        const ObjectId sculptedId = f.scene.bodyAt(0).objectId();
        MeshValidation why = MeshValidation::Ok;
        const bool froze = f.scene.bodyAt(0).frozenSculpt().mesh.freezeFrom(
            f.scene.bodyAt(0).construction().generateMesh(), sculptedId, &why);
        r.check("MIRROR01-11 the sculpt fixture froze", froze);
        MirrorBodyReport sculptReport;
        const BodyCommandStatus sculptStatus =
            mirrorSceneBody(sculptedId, MirrorPlane::Xz, f.scene, f.history, &sculptReport);
        r.check("MIRROR01-11 a body carrying sculpt truth is refused by name",
                sculptStatus == BodyCommandStatus::RefusedNotMirrorable
                    && sculptReport.eligibility == MirrorEligibility::HasSculptTruth);

        MirrorBodyReport unknownReport;
        r.check("MIRROR01-11 an unknown body is refused by name",
                mirrorSceneBody(kNoObject, MirrorPlane::Xy, f.scene, f.history, &unknownReport)
                    == BodyCommandStatus::UnknownBody);
        {
            ScopedConstructionEdit edit(f.history);
            MirrorBodyReport busyReport;
            r.check("MIRROR01-11 an open edit is refused before anything else is asked",
                    mirrorSceneBody(sculptedId, MirrorPlane::Xy, f.scene, f.history, &busyReport)
                        == BodyCommandStatus::RefusedEditInProgress);
        }

        // Not one refusal created a body or a step. The two fixture bodies were
        // added straight to the scene, outside the history, so the count moved
        // by exactly two and the depth not at all.
        r.check("MIRROR01-11 no refusal minted an ObjectId or grew the scene",
                f.scene.bodyCount() == countBefore + 2);
        r.check("MIRROR01-11 no refusal recorded a history step",
                f.history.undoDepth() == depthBefore);

        TransformValues broken = placementOf(1.0, 2.0, 3.0, 0, 0, 0, 1, 1, 1);
        broken.rotationY = std::numeric_limits<double>::quiet_NaN();
        TransformValues untouched = placementOf(9.0, 9.0, 9.0, 9, 9, 9, 9, 9, 9);
        const TransformValues before = untouched;
        r.check("MIRROR01-11 a non-finite placement is refused by name",
                mirrorPlacement(broken, MirrorPlane::Xy, &untouched) == MirrorStatus::NotFinite);
        r.check("MIRROR01-11 a refused placement writes nothing at all",
                sameValues(untouched, before));
    }

    // -----------------------------------------------------------------------
    // MIRROR01-12  the mirrored body round-trips through the ORDINARY codec,
    //              with no new field, section or version
    // -----------------------------------------------------------------------
    //
    // Two scenes reach the same two-body project two ways: one by mirroring,
    // one by placing a second body at the solved numbers the product already
    // knew how to write. Byte-identical files prove Mirror added no schema.
    {
        Fixture viaMirror;
        viaMirror.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        const TransformValues start =
            placementOf(2.5, -1.5, 0.75, 31.0, -47.5, 118.25, 1.5, 2.5, 0.5);
        viaMirror.setPlacement(start);
        MirrorBodyReport report;
        mirrorSceneBody(viaMirror.id(), MirrorPlane::Yz, viaMirror.scene, viaMirror.history,
                        &report);
        const SceneObject* created = viaMirror.scene.findBody(report.newBodyId);
        const TransformValues solved =
            created != nullptr ? created->transform().values() : TransformValues{};

        Fixture viaHand;
        viaHand.setShape(PrimitiveSpec::forBox(2.0, 1.0, 0.5));
        viaHand.setPlacement(start);
        {
            ScopedConstructionEdit edit(viaHand.history);
            const ConstructionObjectState shape =
                viaHand.scene.bodyAt(0).construction().captureState();
            SceneObject& second = viaHand.scene.addBody();
            second.construction().restoreState(shape);
            second.transform().setValues(solved);
            second.setName(created != nullptr ? created->name() : std::string());
            publishSceneObject(second);
        }

        ProjectCodecStatus whyA = ProjectCodecStatus::Ok;
        ProjectCodecStatus whyB = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> a = encodeProjectV1(
            captureProjectDocument(viaMirror.scene, ProjectKind::Construction), &whyA);
        const std::vector<uint8_t> b = encodeProjectV1(
            captureProjectDocument(viaHand.scene, ProjectKind::Construction), &whyB);
        r.check("MIRROR01-12 both projects encode",
                whyA == ProjectCodecStatus::Ok && whyB == ProjectCodecStatus::Ok && !a.empty());
        r.check("MIRROR01-12 a mirrored project encodes exactly the bytes a hand-placed one does",
                a == b);

        ProjectDocument decoded;
        r.check("MIRROR01-12 the file decodes through the ordinary decoder",
                decodeProject(a.data(), a.size(), &decoded) == ProjectCodecStatus::Ok);
        r.check("MIRROR01-12 it is an ordinary Construction project with no CAD and no import",
                decoded.hasConstruction && !decoded.hasCad && !decoded.hasImported);

        Fixture reopened;
        SculptSession reopenedSession;
        const ProjectCodecStatus loaded = loadProjectDocument(
            decoded, reopened.scene, reopenedSession, reopened.history);
        r.check("MIRROR01-12 the mirrored project loads", loaded == ProjectCodecStatus::Ok);
        r.check("MIRROR01-12 both bodies come back", reopened.scene.bodyCount() == 2);
        if (reopened.scene.bodyCount() == 2) {
            r.check("MIRROR01-12 the reopened reflection wears the mirrored placement exactly",
                    sameValues(reopened.scene.bodyAt(1).transform().values(), solved));
            r.check("MIRROR01-12 a loaded project starts with a fresh, empty history",
                    !reopened.history.canUndo() && !reopened.history.canRedo());
        }
    }

    return r.n;
}

}  // namespace forgeshape
