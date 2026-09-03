#include "forgeshape_project_document.h"

#include <cmath>
#include <cstring>

#include "forgeshape_mesh.h"
#include "forgeshape_project_bytes.h"

namespace forgeshape {
namespace {

// Bit-exact scalar comparison.
//
// `==` is the wrong operator for this file's purpose twice over: it says two
// NaNs differ, and it says -0.0 and +0.0 are the same. A roundtrip claim is
// "the bytes came back", so the bytes are what is compared.
bool sameBits(double a, double b) {
    uint64_t left = 0;
    uint64_t right = 0;
    std::memcpy(&left, &a, sizeof(left));
    std::memcpy(&right, &b, sizeof(right));
    return left == right;
}

bool sameBits(float a, float b) {
    uint32_t left = 0;
    uint32_t right = 0;
    std::memcpy(&left, &a, sizeof(left));
    std::memcpy(&right, &b, sizeof(right));
    return left == right;
}

bool sameTransform(const TransformValues& a, const TransformValues& b) {
    return sameBits(a.positionX, b.positionX) && sameBits(a.positionY, b.positionY)
           && sameBits(a.positionZ, b.positionZ) && sameBits(a.rotationX, b.rotationX)
           && sameBits(a.rotationY, b.rotationY) && sameBits(a.rotationZ, b.rotationZ)
           && sameBits(a.scaleX, b.scaleX) && sameBits(a.scaleY, b.scaleY)
           && sameBits(a.scaleZ, b.scaleZ);
}

bool sameShape(const ConstructionObjectState& a, const ConstructionObjectState& b) {
    return a.kind == b.kind && sameBits(a.box.width, b.box.width)
           && sameBits(a.box.height, b.box.height) && sameBits(a.box.depth, b.box.depth)
           && sameBits(a.cylinder.diameter, b.cylinder.diameter)
           && sameBits(a.cylinder.height, b.cylinder.height)
           && sameBits(a.sphere.diameter, b.sphere.diameter)
           && sameBits(a.cone.bottomDiameter, b.cone.bottomDiameter)
           && sameBits(a.cone.height, b.cone.height)
           && sameBits(a.capsule.diameter, b.capsule.diameter)
           && sameBits(a.capsule.totalHeight, b.capsule.totalHeight)
           && sameBits(a.plane.width, b.plane.width) && sameBits(a.plane.depth, b.plane.depth);
}

// The six remembered parameter sets, in the ONE canonical file order, written
// and read by the same list so the two can never drift apart.
void writeShapeParameters(ByteWriter& out, const ConstructionObjectState& shape) {
    out.f64(shape.box.width);
    out.f64(shape.box.height);
    out.f64(shape.box.depth);
    out.f64(shape.cylinder.diameter);
    out.f64(shape.cylinder.height);
    out.f64(shape.sphere.diameter);
    out.f64(shape.cone.bottomDiameter);
    out.f64(shape.cone.height);
    out.f64(shape.capsule.diameter);
    out.f64(shape.capsule.totalHeight);
    out.f64(shape.plane.width);
    out.f64(shape.plane.depth);
}

bool readShapeParameters(ByteReader& in, ConstructionObjectState* shape) {
    return in.f64(&shape->box.width) && in.f64(&shape->box.height) && in.f64(&shape->box.depth)
           && in.f64(&shape->cylinder.diameter) && in.f64(&shape->cylinder.height)
           && in.f64(&shape->sphere.diameter) && in.f64(&shape->cone.bottomDiameter)
           && in.f64(&shape->cone.height) && in.f64(&shape->capsule.diameter)
           && in.f64(&shape->capsule.totalHeight) && in.f64(&shape->plane.width)
           && in.f64(&shape->plane.depth);
}

void writeTransform(ByteWriter& out, const TransformValues& values) {
    out.f64(values.positionX);
    out.f64(values.positionY);
    out.f64(values.positionZ);
    out.f64(values.rotationX);
    out.f64(values.rotationY);
    out.f64(values.rotationZ);
    out.f64(values.scaleX);
    out.f64(values.scaleY);
    out.f64(values.scaleZ);
}

bool readTransform(ByteReader& in, TransformValues* values) {
    return in.f64(&values->positionX) && in.f64(&values->positionY) && in.f64(&values->positionZ)
           && in.f64(&values->rotationX) && in.f64(&values->rotationY)
           && in.f64(&values->rotationZ) && in.f64(&values->scaleX) && in.f64(&values->scaleY)
           && in.f64(&values->scaleZ);
}

// One section, header and all, appended to the file being built.
//
// The CRC is taken over the payload bytes ONLY, which is what lets a reader
// validate a section it does not understand and skip it safely.
void appendSection(std::vector<uint8_t>& file, const char tag[4], uint16_t sectionVersion,
                   bool required, const std::vector<uint8_t>& payload) {
    ByteWriter out(file);
    out.bytes(tag, 4);
    out.u16(sectionVersion);
    out.u16(required ? kSectionFlagRequired : 0u);
    out.u64(static_cast<uint64_t>(payload.size()));
    out.u32(crc32IsoHdlc(payload.empty() ? nullptr : payload.data(), payload.size()));
    out.u32(0u);  // reserved, and checked to be zero on read
    if (!payload.empty()) {
        out.bytes(payload.data(), payload.size());
    }
}

bool tagIs(const char actual[4], const char expected[4]) {
    return std::memcmp(actual, expected, 4) == 0;
}

// Every semantic length rule the live editor applies, for one body's six
// remembered parameter sets. The capsule goes through its RELATION check rather
// than two independent length checks, because "0.5 m is not a length" and
// "0.5 m is too short to be this capsule's total height" are different defects
// and the domain already knows the difference.
bool shapeParametersValid(const ConstructionObjectState& shape) {
    const DimensionValidation checks[] = {
            validateDimensionMeters(shape.box.width),
            validateDimensionMeters(shape.box.height),
            validateDimensionMeters(shape.box.depth),
            validateDimensionMeters(shape.cylinder.diameter),
            validateDimensionMeters(shape.cylinder.height),
            validateDimensionMeters(shape.sphere.diameter),
            validateDimensionMeters(shape.cone.bottomDiameter),
            validateDimensionMeters(shape.cone.height),
            validateCapsuleMeters(shape.capsule.diameter, shape.capsule.totalHeight),
            validateDimensionMeters(shape.plane.width),
            validateDimensionMeters(shape.plane.depth),
    };
    for (DimensionValidation why : checks) {
        if (why != DimensionValidation::Ok) {
            return false;
        }
    }
    return true;
}

bool transformValid(const TransformValues& values) {
    const double free[] = {values.positionX, values.positionY, values.positionZ,
                           values.rotationX, values.rotationY, values.rotationZ};
    for (double value : free) {
        if (validateTransformValue(value) != TransformValidation::Ok) {
            return false;
        }
    }
    const double scales[] = {values.scaleX, values.scaleY, values.scaleZ};
    for (double value : scales) {
        if (validateScaleValue(value) != TransformValidation::Ok) {
            return false;
        }
    }
    return true;
}

}  // namespace

const char* projectKindName(ProjectKind kind) {
    switch (kind) {
        case ProjectKind::Construction: return "Construction";
        case ProjectKind::Sculpt: return "Sculpt";
    }
    return "unknown";
}

const char* projectCodecStatusName(ProjectCodecStatus status) {
    switch (status) {
        case ProjectCodecStatus::Ok: return "Ok";
        case ProjectCodecStatus::NotForgeFile: return "NotForgeFile";
        case ProjectCodecStatus::UnsupportedMajor: return "UnsupportedMajor";
        case ProjectCodecStatus::UnsupportedSectionVersion: return "UnsupportedSectionVersion";
        case ProjectCodecStatus::BadHeader: return "BadHeader";
        case ProjectCodecStatus::Truncated: return "Truncated";
        case ProjectCodecStatus::BadSectionHeader: return "BadSectionHeader";
        case ProjectCodecStatus::ChecksumMismatch: return "ChecksumMismatch";
        case ProjectCodecStatus::UnknownRequiredSection: return "UnknownRequiredSection";
        case ProjectCodecStatus::DuplicateSection: return "DuplicateSection";
        case ProjectCodecStatus::MissingRequiredSection: return "MissingRequiredSection";
        case ProjectCodecStatus::BadPayload: return "BadPayload";
        case ProjectCodecStatus::ImpossibleCount: return "ImpossibleCount";
        case ProjectCodecStatus::InvalidSemanticValue: return "InvalidSemanticValue";
        case ProjectCodecStatus::UnresolvedReference: return "UnresolvedReference";
        case ProjectCodecStatus::RefusedEditInProgress: return "RefusedEditInProgress";
    }
    return "unknown";
}

uint8_t workplaneFileCode(Workplane plane) {
    switch (plane) {
        case Workplane::XY: return 1;
        case Workplane::XZ: return 2;
        case Workplane::YZ: return 3;
    }
    return 0;
}

bool workplaneFromFileCode(uint8_t code, Workplane* out) {
    if (out == nullptr) return false;
    switch (code) {
        case 1: *out = Workplane::XY; return true;
        case 2: *out = Workplane::XZ; return true;
        case 3: *out = Workplane::YZ; return true;
        default: return false;
    }
}

uint8_t extrudeDirectionFileCode(ExtrudeDirection direction) {
    switch (direction) {
        case ExtrudeDirection::AlongNormal: return 1;
        case ExtrudeDirection::AgainstNormal: return 2;
    }
    return 0;
}

bool extrudeDirectionFromFileCode(uint8_t code, ExtrudeDirection* out) {
    if (out == nullptr) return false;
    switch (code) {
        case 1: *out = ExtrudeDirection::AlongNormal; return true;
        case 2: *out = ExtrudeDirection::AgainstNormal; return true;
        default: return false;
    }
}

uint8_t sketchEntityKindFileCode(SketchEntityKind kind) {
    switch (kind) {
        case SketchEntityKind::Line: return 1;
        case SketchEntityKind::Polyline: return 2;
        case SketchEntityKind::Rectangle: return 3;
        case SketchEntityKind::Circle: return 4;
    }
    return 0;
}

bool sketchEntityKindFromFileCode(uint8_t code, SketchEntityKind* out) {
    if (out == nullptr) return false;
    switch (code) {
        case 1: *out = SketchEntityKind::Line; return true;
        case 2: *out = SketchEntityKind::Polyline; return true;
        case 3: *out = SketchEntityKind::Rectangle; return true;
        case 4: *out = SketchEntityKind::Circle; return true;
        default: return false;
    }
}

uint8_t primitiveFileCode(PrimitiveKind kind) {
    switch (kind) {
        case PrimitiveKind::Box: return 1;
        case PrimitiveKind::Cylinder: return 2;
        case PrimitiveKind::Sphere: return 3;
        case PrimitiveKind::Cone: return 4;
        case PrimitiveKind::Capsule: return 5;
        case PrimitiveKind::Plane: return 6;
    }
    return 0;
}

bool primitiveKindFromFileCode(uint8_t code, PrimitiveKind* out) {
    if (out == nullptr) {
        return false;
    }
    switch (code) {
        case 1: *out = PrimitiveKind::Box; return true;
        case 2: *out = PrimitiveKind::Cylinder; return true;
        case 3: *out = PrimitiveKind::Sphere; return true;
        case 4: *out = PrimitiveKind::Cone; return true;
        case 5: *out = PrimitiveKind::Capsule; return true;
        case 6: *out = PrimitiveKind::Plane; return true;
        default: return false;
    }
}

bool sameProjectDocument(const ProjectDocument& a, const ProjectDocument& b) {
    if (a.kind != b.kind || a.hasConstruction != b.hasConstruction
        || a.hasSculpt != b.hasSculpt || a.hasImported != b.hasImported
        || a.hasCad != b.hasCad) {
        return false;
    }
    if (a.hasCad) {
        if (a.cad.bodies.size() != b.cad.bodies.size()) {
            return false;
        }
        for (size_t i = 0; i < a.cad.bodies.size(); ++i) {
            if (a.cad.bodies[i].objectId != b.cad.bodies[i].objectId
                || !sameCadBodyState(a.cad.bodies[i].state, b.cad.bodies[i].state)) {
                return false;
            }
        }
    }
    if (a.scene.nextObjectId != b.scene.nextObjectId
        || a.scene.activeObjectId != b.scene.activeObjectId
        || a.scene.bodies.size() != b.scene.bodies.size()) {
        return false;
    }
    for (size_t i = 0; i < a.scene.bodies.size(); ++i) {
        if (a.scene.bodies[i].objectId != b.scene.bodies[i].objectId
            || !sameTransform(a.scene.bodies[i].transform, b.scene.bodies[i].transform)) {
            return false;
        }
    }
    if (a.hasConstruction) {
        if (a.construction.bodies.size() != b.construction.bodies.size()) {
            return false;
        }
        for (size_t i = 0; i < a.construction.bodies.size(); ++i) {
            const ProjectConstructionBody& left = a.construction.bodies[i];
            const ProjectConstructionBody& right = b.construction.bodies[i];
            if (left.objectId != right.objectId || !sameShape(left.shape, right.shape)
                || left.features.size() != right.features.size()) {
                return false;
            }
            for (size_t f = 0; f < left.features.size(); ++f) {
                if (left.features[f].localFeatureId != right.features[f].localFeatureId
                    || left.features[f].kindCode != right.features[f].kindCode) {
                    return false;
                }
            }
        }
    }
    if (a.hasSculpt) {
        if (a.sculpt.bodies.size() != b.sculpt.bodies.size()) {
            return false;
        }
        for (size_t i = 0; i < a.sculpt.bodies.size(); ++i) {
            const ProjectSculptBody& left = a.sculpt.bodies[i];
            const ProjectSculptBody& right = b.sculpt.bodies[i];
            if (left.objectId != right.objectId || left.renderBothSides != right.renderBothSides
                || left.sourceStale != right.sourceStale || left.hasEdits != right.hasEdits
                || left.positions.size() != right.positions.size()
                || left.indices != right.indices) {
                return false;
            }
            for (size_t v = 0; v < left.positions.size(); ++v) {
                if (!sameBits(left.positions[v], right.positions[v])) {
                    return false;
                }
            }
        }
    }
    if (a.hasImported) {
        if (a.imported.bodies.size() != b.imported.bodies.size()) {
            return false;
        }
        for (size_t i = 0; i < a.imported.bodies.size(); ++i) {
            const ProjectImportedBody& left = a.imported.bodies[i];
            const ProjectImportedBody& right = b.imported.bodies[i];
            if (left.objectId != right.objectId || left.name != right.name
                || left.positions.size() != right.positions.size()
                || left.normals.size() != right.normals.size()
                || left.indices != right.indices
                || left.batches.size() != right.batches.size()) {
                return false;
            }
            // Positions AND normals bit for bit, for the same reason the sculpt
            // branch compares positions that way: an imported mesh is the only
            // copy of itself, and a value that came back one ULP away is a
            // roundtrip that lost something.
            for (size_t v = 0; v < left.positions.size(); ++v) {
                if (!sameBits(left.positions[v], right.positions[v])) {
                    return false;
                }
            }
            for (size_t v = 0; v < left.normals.size(); ++v) {
                if (!sameBits(left.normals[v], right.normals[v])) {
                    return false;
                }
            }
            for (size_t bIndex = 0; bIndex < left.batches.size(); ++bIndex) {
                if (left.batches[bIndex].firstIndex != right.batches[bIndex].firstIndex
                    || left.batches[bIndex].indexCount != right.batches[bIndex].indexCount
                    || left.batches[bIndex].doubleSided != right.batches[bIndex].doubleSided) {
                    return false;
                }
            }
        }
    }
    return true;
}

ProjectCodecStatus validateProjectDocument(const ProjectDocument& document) {
    const std::vector<ProjectBodyPlacement>& bodies = document.scene.bodies;
    if (bodies.empty() || bodies.size() > kMaxProjectBodies) {
        return ProjectCodecStatus::ImpossibleCount;
    }

    // Identity first: every later check names a body by id, so a duplicated or
    // reserved id would make the rest of this function ambiguous rather than
    // merely wrong.
    ObjectId highest = kNoObject;
    for (size_t i = 0; i < bodies.size(); ++i) {
        const ObjectId id = bodies[i].objectId;
        if (id == kNoObject) {
            return ProjectCodecStatus::InvalidSemanticValue;
        }
        for (size_t j = 0; j < i; ++j) {
            if (bodies[j].objectId == id) {
                return ProjectCodecStatus::InvalidSemanticValue;
            }
        }
        if (id > highest) {
            highest = id;
        }
        if (!transformValid(bodies[i].transform)) {
            return ProjectCodecStatus::InvalidSemanticValue;
        }
    }

    // The allocator can never mint an id a loaded body is already wearing. This
    // is the file's half of the never-reuse rule; forgeshape_project_state.cpp
    // enforces the other half by only ever pushing the live allocator forward.
    if (document.scene.nextObjectId <= highest) {
        return ProjectCodecStatus::InvalidSemanticValue;
    }

    bool activeFound = false;
    for (const ProjectBodyPlacement& body : bodies) {
        if (body.objectId == document.scene.activeObjectId) {
            activeFound = true;
            break;
        }
    }
    if (!activeFound) {
        return ProjectCodecStatus::UnresolvedReference;
    }

    if (document.kind == ProjectKind::Sculpt && !document.hasSculpt) {
        return ProjectCodecStatus::MissingRequiredSection;
    }

    // Which representation each SCNE body's geometry comes from, filled in by
    // the two branches below. A body must be named by EXACTLY ONE of them:
    // neither leaves a body with no geometry at all, and both would be two
    // answers to what the object is.
    std::vector<uint8_t> covered(bodies.size(), 0);

    if (document.hasConstruction) {
        // A SUBSEQUENCE of the scene, in ascending scene order — every body
        // that has a Construction Source and no other. It read as "one per
        // body" until `IMPORT-01A` only because every body had one.
        if (document.construction.bodies.empty()
            || document.construction.bodies.size() > bodies.size()) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        size_t sceneCursor = 0;
        for (size_t i = 0; i < document.construction.bodies.size(); ++i) {
            const ProjectConstructionBody& body = document.construction.bodies[i];
            size_t found = bodies.size();
            for (size_t s = sceneCursor; s < bodies.size(); ++s) {
                if (bodies[s].objectId == body.objectId) {
                    found = s;
                    break;
                }
            }
            if (found == bodies.size()) {
                return ProjectCodecStatus::UnresolvedReference;
            }
            // Strictly ascending, which makes a duplicate impossible without a
            // second pass and keeps the writer's output canonical.
            sceneCursor = found + 1;
            covered[found] = 1;
            if (primitiveFileCode(body.shape.kind) == 0) {
                return ProjectCodecStatus::InvalidSemanticValue;
            }
            if (!shapeParametersValid(body.shape)) {
                return ProjectCodecStatus::InvalidSemanticValue;
            }
            // The v1 feature graph is exactly one PrimitiveSource per body,
            // identified (ObjectId, LocalFeatureId=1). A file that carries more,
            // fewer or a different kind is describing a CAD project this
            // version cannot evaluate, and guessing at it would be worse than
            // refusing it.
            if (body.features.size() != 1
                || body.features[0].localFeatureId != kPrimitiveSourceFeatureId
                || body.features[0].kindCode != kFeatureKindPrimitiveSource) {
                return ProjectCodecStatus::InvalidSemanticValue;
            }
        }
    }

    if (document.hasImported) {
        // Same subsequence rule as CONS, and for the same reason: the file's
        // order is the scene's order, and a reader must be able to say which
        // body an entry belongs to without searching backwards.
        if (document.imported.bodies.empty()
            || document.imported.bodies.size() > bodies.size()) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        size_t sceneCursor = 0;
        for (const ProjectImportedBody& body : document.imported.bodies) {
            size_t found = bodies.size();
            for (size_t s = sceneCursor; s < bodies.size(); ++s) {
                if (bodies[s].objectId == body.objectId) {
                    found = s;
                    break;
                }
            }
            if (found == bodies.size()) {
                return ProjectCodecStatus::UnresolvedReference;
            }
            sceneCursor = found + 1;
            if (covered[found] != 0) {
                // Already claimed by CONS. A body cannot be both a primitive
                // and a mesh read from a file: that is two answers to what the
                // object IS, and every later edit would have to pick one.
                return ProjectCodecStatus::UnresolvedReference;
            }
            covered[found] = 2;

            // The name is checked against the DOMAIN's own rule rather than a
            // restatement of it, so a file cannot carry a name the importer
            // could not have produced — a control character, malformed UTF-8,
            // untrimmed padding or more than the stored maximum.
            if (!importedMeshNameIsStorable(body.name)) {
                return ProjectCodecStatus::InvalidSemanticValue;
            }
            if (body.positions.size() != static_cast<size_t>(body.vertexCount()) * 3u) {
                return ProjectCodecStatus::BadPayload;
            }
            // And the geometry against the domain's own validator, which is the
            // same function `ImportedMesh::build` runs. There is one rule for
            // what an imported object may be, and the file is held to it.
            const ImportedMeshValidation why = validateImportedMeshData(
                    body.positions, body.normals, body.indices, body.batches);
            switch (why) {
                case ImportedMeshValidation::Ok:
                    break;
                case ImportedMeshValidation::EmptyVertices:
                case ImportedMeshValidation::EmptyIndices:
                case ImportedMeshValidation::TooLarge:
                case ImportedMeshValidation::IndexCountNotTriangles:
                case ImportedMeshValidation::NoBatches:
                    return ProjectCodecStatus::ImpossibleCount;
                case ImportedMeshValidation::CountMismatch:
                case ImportedMeshValidation::BatchesDoNotTile:
                    return ProjectCodecStatus::BadPayload;
                default:
                    // An index outside the vertex array, a non-finite position
                    // or a normal that is not a direction: values the live
                    // model refuses.
                    return ProjectCodecStatus::InvalidSemanticValue;
            }
        }
    }

    if (document.hasCad) {
        // The third geometry source, on IMPT's terms: a subsequence of the
        // scene in strictly ascending order, claiming each body exactly once
        // and never one CONS or IMPT already claimed.
        if (document.cad.bodies.empty() || document.cad.bodies.size() > bodies.size()) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        size_t sceneCursor = 0;
        for (const ProjectCadBody& body : document.cad.bodies) {
            size_t found = bodies.size();
            for (size_t s = sceneCursor; s < bodies.size(); ++s) {
                if (bodies[s].objectId == body.objectId) {
                    found = s;
                    break;
                }
            }
            if (found == bodies.size()) {
                return ProjectCodecStatus::UnresolvedReference;
            }
            sceneCursor = found + 1;
            if (covered[found] != 0) {
                return ProjectCodecStatus::UnresolvedReference;
            }
            covered[found] = 3;
            if (body.state.sketch.entities.size() > kMaxSketchEntities) {
                return ProjectCodecStatus::ImpossibleCount;
            }
            // The DOMAIN's own rule, in full: every entity, the depth, the
            // direction, and that the sketch closes the profile the extrusion
            // names. A file cannot carry a CAD body this build could not
            // regenerate, and a sketch with no closed profile is refused here
            // rather than opening as an object with nothing to draw.
            if (validateCadBodyState(body.state) != CadStatus::Ok) {
                return ProjectCodecStatus::InvalidSemanticValue;
            }
        }
    }

    // A body named by NEITHER branch is deliberately not refused here.
    //
    // It is a legal document: an optional section at a version this reader
    // cannot read is skipped by contract, and a Sculpt project's retained CONS
    // companion is exactly such a section. What that leaves is a document this
    // BUILD cannot evaluate, which is a different sentence and is said in
    // forgeshape_project_state.cpp, where the reason is about the runtime
    // rather than about the file.

    if (document.hasSculpt) {
        // "Carries a sculpt branch" and "carries an EMPTY sculpt branch" are
        // different claims, and only the first is a project. A document that
        // announced SCUL and then held nothing would encode a section the
        // decoder is right to refuse, so it is refused here instead -- on the
        // side that can still say why.
        if (document.sculpt.bodies.empty()) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        // A Sculpt project reopens showing the active body's Frozen Sculpt
        // Mesh, so that body must actually have one. Without this rule the mode
        // in the header and what the product can present would disagree, and
        // the load would have to invent an answer at the last moment.
        if (document.kind == ProjectKind::Sculpt) {
            bool activeIsSculpted = false;
            for (const ProjectSculptBody& body : document.sculpt.bodies) {
                if (body.objectId == document.scene.activeObjectId) {
                    activeIsSculpted = true;
                    break;
                }
            }
            if (!activeIsSculpted) {
                return ProjectCodecStatus::UnresolvedReference;
            }
        }
        size_t sceneCursor = 0;
        for (const ProjectSculptBody& body : document.sculpt.bodies) {
            // Ascending scene order, which makes duplicates impossible without
            // a second pass and keeps the writer's output canonical.
            size_t found = bodies.size();
            for (size_t i = sceneCursor; i < bodies.size(); ++i) {
                if (bodies[i].objectId == body.objectId) {
                    found = i;
                    break;
                }
            }
            if (found == bodies.size()) {
                return ProjectCodecStatus::UnresolvedReference;
            }
            sceneCursor = found + 1;
            // A `SCUL` entry over a `CADB` body is REFUSED: `CAD-R0-A1A2`
            // leaves CAD -> Sculpt out, so a file claiming a sculpt mesh for
            // a CAD Body describes something this build cannot evaluate. The
            // same fail-closed shape the pre-`IMPORT-01B` reader gave an
            // `IMPT`+`SCUL` file.
            if (covered[found] == 3) {
                return ProjectCodecStatus::UnresolvedReference;
            }
            // A `SCUL` entry over an `IMPT` body is VALID since `IMPORT-01B`.
            //
            // It was refused while an imported object could not be sculpted at
            // all, and that refusal was right then: a file claiming a sculpt
            // mesh for one described something the build could not evaluate,
            // and dropping either half would have lost it. What the entry means
            // now is exactly what it means over a `CONS` body -- a second
            // representation of the same body, frozen from whatever its source
            // is -- so there is nothing left for the pair to disagree about.
            //
            // The exactly-one-of rule is untouched: it is about `CONS` and
            // `IMPT`, the two things a body's geometry can COME from, and
            // `SCUL` is not one of them. Which source it was frozen from is not
            // stored, because nothing reads it back: a Frozen Sculpt Mesh is
            // its own positions and its own topology whatever produced it.
            const uint32_t vertexCount = body.vertexCount();
            const uint32_t indexCount = body.indexCount();
            if (body.positions.size() != static_cast<size_t>(vertexCount) * 3u) {
                return ProjectCodecStatus::BadPayload;
            }
            if (vertexCount == 0 || vertexCount > kMaxMeshVertices || indexCount == 0
                || indexCount > kMaxMeshIndices || (indexCount % 3u) != 0u) {
                return ProjectCodecStatus::ImpossibleCount;
            }
            for (float value : body.positions) {
                // The same rule RuntimeMesh applies: a non-finite position is
                // not a place, and one would poison every normal that touches it.
                if (!std::isfinite(value)) {
                    return ProjectCodecStatus::InvalidSemanticValue;
                }
            }
            for (uint32_t index : body.indices) {
                if (index >= vertexCount) {
                    return ProjectCodecStatus::InvalidSemanticValue;
                }
            }
        }
    }

    return ProjectCodecStatus::Ok;
}

std::vector<uint8_t> encodeProjectV1(const ProjectDocument& document,
                                     ProjectCodecStatus* outWhy) {
    const ProjectCodecStatus why = validateProjectDocument(document);
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != ProjectCodecStatus::Ok) {
        return {};
    }

    std::vector<uint8_t> scenePayload;
    {
        ByteWriter out(scenePayload);
        out.u32(static_cast<uint32_t>(document.scene.bodies.size()));
        out.u64(document.scene.nextObjectId);
        out.u64(document.scene.activeObjectId);
        for (const ProjectBodyPlacement& body : document.scene.bodies) {
            out.u64(body.objectId);
            writeTransform(out, body.transform);
        }
    }

    std::vector<uint8_t> constructionPayload;
    if (document.hasConstruction) {
        ByteWriter out(constructionPayload);
        out.u32(static_cast<uint32_t>(document.construction.bodies.size()));
        for (const ProjectConstructionBody& body : document.construction.bodies) {
            out.u64(body.objectId);
            out.u8(primitiveFileCode(body.shape.kind));
            writeShapeParameters(out, body.shape);
            out.u32(static_cast<uint32_t>(body.features.size()));
            for (const ProjectFeatureRecord& feature : body.features) {
                out.u32(feature.localFeatureId);
                out.u8(feature.kindCode);
            }
        }
    }

    std::vector<uint8_t> sculptPayload;
    if (document.hasSculpt) {
        ByteWriter out(sculptPayload);
        out.u32(static_cast<uint32_t>(document.sculpt.bodies.size()));
        for (const ProjectSculptBody& body : document.sculpt.bodies) {
            out.u64(body.objectId);
            out.u8(static_cast<uint8_t>((body.renderBothSides ? 0x01u : 0x00u)
                                        | (body.sourceStale ? 0x02u : 0x00u)
                                        | (body.hasEdits ? 0x04u : 0x00u)));
            out.u32(body.vertexCount());
            out.u32(body.indexCount());
            for (float value : body.positions) {
                out.f32(value);
            }
            for (uint32_t index : body.indices) {
                out.u32(index);
            }
        }
    }

    std::vector<uint8_t> importedPayload;
    if (document.hasImported) {
        ByteWriter out(importedPayload);
        out.u32(static_cast<uint32_t>(document.imported.bodies.size()));
        for (const ProjectImportedBody& body : document.imported.bodies) {
            out.u64(body.objectId);
            // The name's length as a u16 before its bytes: no terminator, and
            // no fixed-width padding, so the same name is always the same
            // bytes. Every field after it is read byte-wise little-endian, so
            // a variable-length field in the middle costs nothing in alignment.
            out.u16(static_cast<uint16_t>(body.name.size()));
            if (!body.name.empty()) {
                out.bytes(body.name.data(), body.name.size());
            }
            out.u32(body.vertexCount());
            out.u32(body.indexCount());
            out.u32(body.batchCount());
            for (float value : body.positions) {
                out.f32(value);
            }
            for (float value : body.normals) {
                out.f32(value);
            }
            for (uint32_t index : body.indices) {
                out.u32(index);
            }
            for (const ImportedMeshBatch& batch : body.batches) {
                out.u32(batch.firstIndex);
                out.u32(batch.indexCount);
                out.u8(batch.doubleSided ? 0x01u : 0x00u);
            }
        }
    }

    std::vector<uint8_t> cadPayload;
    if (document.hasCad) {
        ByteWriter out(cadPayload);
        out.u32(static_cast<uint32_t>(document.cad.bodies.size()));
        for (const ProjectCadBody& body : document.cad.bodies) {
            const CadBodyState& state = body.state;
            out.u64(body.objectId);
            out.u8(workplaneFileCode(state.sketch.plane));
            out.u32(state.sketch.nextEntityId);
            out.u32(state.extrude.profileEntityId);
            out.u8(extrudeDirectionFileCode(state.extrude.direction));
            out.f64(state.extrude.depth);
            out.u32(static_cast<uint32_t>(state.sketch.entities.size()));
            for (const SketchEntity& entity : state.sketch.entities) {
                out.u32(entity.id());
                out.u8(sketchEntityKindFileCode(entity.kind()));
                if (const SketchLine* line = entity.line()) {
                    out.f64(line->start.u);
                    out.f64(line->start.v);
                    out.f64(line->end.u);
                    out.f64(line->end.v);
                } else if (const SketchPolyline* polyline = entity.polyline()) {
                    out.u8(polyline->closed ? 0x01u : 0x00u);
                    out.u32(static_cast<uint32_t>(polyline->vertices.size()));
                    for (const SketchPoint& p : polyline->vertices) {
                        out.f64(p.u);
                        out.f64(p.v);
                    }
                } else if (const SketchRectangle* rectangle = entity.rectangle()) {
                    out.f64(rectangle->center.u);
                    out.f64(rectangle->center.v);
                    out.f64(rectangle->width);
                    out.f64(rectangle->height);
                } else if (const SketchCircle* circle = entity.circle()) {
                    out.f64(circle->center.u);
                    out.f64(circle->center.v);
                    out.f64(circle->radius);
                }
            }
        }
    }

    uint32_t sectionCount = 1;
    if (document.hasConstruction) ++sectionCount;
    if (document.hasSculpt) ++sectionCount;
    if (document.hasImported) ++sectionCount;
    if (document.hasCad) ++sectionCount;

    uint64_t fileBytes = kForgeHeaderBytes;
    fileBytes += kForgeSectionHeaderBytes + scenePayload.size();
    if (document.hasConstruction) {
        fileBytes += kForgeSectionHeaderBytes + constructionPayload.size();
    }
    if (document.hasSculpt) {
        fileBytes += kForgeSectionHeaderBytes + sculptPayload.size();
    }
    if (document.hasImported) {
        fileBytes += kForgeSectionHeaderBytes + importedPayload.size();
    }
    if (document.hasCad) {
        fileBytes += kForgeSectionHeaderBytes + cadPayload.size();
    }

    std::vector<uint8_t> file;
    file.reserve(static_cast<size_t>(fileBytes));
    {
        ByteWriter out(file);
        out.bytes(kForgeMagic, sizeof(kForgeMagic));
        out.u16(kForgeVersionMajor);
        out.u16(kForgeVersionMinor);
        out.u16(kForgeHeaderBytes);
        out.u8(static_cast<uint8_t>(document.kind));
        out.u8(static_cast<uint8_t>((document.hasConstruction ? kHeaderFlagHasConstruction : 0u)
                                    | (document.hasSculpt ? kHeaderFlagHasSculpt : 0u)
                                    | (document.hasImported ? kHeaderFlagHasImported : 0u)
                                    | (document.hasCad ? kHeaderFlagHasCad : 0u)));
        out.u32(sectionCount);
        out.u64(fileBytes);
    }

    // Canonical order, and it is part of the format rather than an accident of
    // this function: SCNE is what both project kinds share, CONS is the
    // Construction branch, SCUL the sculpt one.
    appendSection(file, kSectionTagScene, kSceneSectionVersion, /*required=*/true, scenePayload);
    if (document.hasConstruction) {
        appendSection(file, kSectionTagConstruction, kConstructionSectionVersion,
                      /*required=*/document.kind == ProjectKind::Construction, constructionPayload);
    }
    if (document.hasSculpt) {
        appendSection(file, kSectionTagSculpt, kSculptSectionVersion,
                      /*required=*/document.kind == ProjectKind::Sculpt, sculptPayload);
    }
    if (document.hasImported) {
        // ALWAYS required, in either project kind, unlike CONS and SCUL whose
        // required bit follows the header's ProjectKind. Those two are branches
        // of data a reader can legitimately skip because the other branch still
        // describes the same bodies. An Imported Mesh has no other branch: it
        // is the only copy of its own geometry, and a reader that skipped it
        // would open the project with objects silently missing.
        appendSection(file, kSectionTagImported, kImportedSectionVersion, /*required=*/true,
                      importedPayload);
    }
    if (document.hasCad) {
        // ALWAYS required, on IMPT's terms: a CAD Body has no other branch
        // describing it, and a reader that skipped this would open the
        // project with objects silently missing.
        appendSection(file, kSectionTagCad, kCadSectionVersion, /*required=*/true, cadPayload);
    }
    return file;
}

namespace {

ProjectCodecStatus decodeScenePayload(ByteReader& in, ProjectSceneRecord* scene) {
    uint32_t bodyCount = 0;
    if (!in.u32(&bodyCount)) {
        return ProjectCodecStatus::Truncated;
    }
    // Refused BEFORE the reserve: an impossible count in a corrupt file must
    // never reach an allocator.
    if (bodyCount == 0 || bodyCount > kMaxProjectBodies) {
        return ProjectCodecStatus::ImpossibleCount;
    }
    if (!in.u64(&scene->nextObjectId) || !in.u64(&scene->activeObjectId)) {
        return ProjectCodecStatus::Truncated;
    }
    // 8 bytes of identity plus nine binary64 placement values per body.
    const uint64_t needed = static_cast<uint64_t>(bodyCount) * (8ull + 9ull * 8ull);
    if (needed > in.remaining()) {
        return ProjectCodecStatus::Truncated;
    }
    scene->bodies.resize(bodyCount);
    for (uint32_t i = 0; i < bodyCount; ++i) {
        if (!in.u64(&scene->bodies[i].objectId)
            || !readTransform(in, &scene->bodies[i].transform)) {
            return ProjectCodecStatus::Truncated;
        }
    }
    if (!in.atEnd()) {
        return ProjectCodecStatus::BadPayload;
    }
    return ProjectCodecStatus::Ok;
}

ProjectCodecStatus decodeConstructionPayload(ByteReader& in, ProjectConstructionRecord* record) {
    uint32_t bodyCount = 0;
    if (!in.u32(&bodyCount)) {
        return ProjectCodecStatus::Truncated;
    }
    if (bodyCount == 0 || bodyCount > kMaxProjectBodies) {
        return ProjectCodecStatus::ImpossibleCount;
    }
    // Smallest possible Construction body record: identity, active kind code, the
    // six remembered parameter sets, and a feature count. Checked before the
    // reserve for the same reason SCNE is.
    if (static_cast<uint64_t>(bodyCount) * (8ull + 1ull + 96ull + 4ull) > in.remaining()) {
        return ProjectCodecStatus::Truncated;
    }
    record->bodies.resize(bodyCount);
    for (uint32_t i = 0; i < bodyCount; ++i) {
        ProjectConstructionBody& body = record->bodies[i];
        uint8_t kindCode = 0;
        if (!in.u64(&body.objectId) || !in.u8(&kindCode)) {
            return ProjectCodecStatus::Truncated;
        }
        if (!primitiveKindFromFileCode(kindCode, &body.shape.kind)) {
            return ProjectCodecStatus::InvalidSemanticValue;
        }
        if (!readShapeParameters(in, &body.shape)) {
            return ProjectCodecStatus::Truncated;
        }
        uint32_t featureCount = 0;
        if (!in.u32(&featureCount)) {
            return ProjectCodecStatus::Truncated;
        }
        // Five bytes per feature record, checked against what is actually left.
        if (static_cast<uint64_t>(featureCount) * 5ull > in.remaining()) {
            return ProjectCodecStatus::Truncated;
        }
        body.features.resize(featureCount);
        for (uint32_t f = 0; f < featureCount; ++f) {
            if (!in.u32(&body.features[f].localFeatureId) || !in.u8(&body.features[f].kindCode)) {
                return ProjectCodecStatus::Truncated;
            }
        }
    }
    if (!in.atEnd()) {
        return ProjectCodecStatus::BadPayload;
    }
    return ProjectCodecStatus::Ok;
}

ProjectCodecStatus decodeSculptPayload(ByteReader& in, ProjectSculptRecord* record) {
    uint32_t bodyCount = 0;
    if (!in.u32(&bodyCount)) {
        return ProjectCodecStatus::Truncated;
    }
    if (bodyCount == 0 || bodyCount > kMaxProjectBodies) {
        return ProjectCodecStatus::ImpossibleCount;
    }
    // Smallest possible SCUL entry: identity, flags and the two counts.
    if (static_cast<uint64_t>(bodyCount) * (8ull + 1ull + 4ull + 4ull) > in.remaining()) {
        return ProjectCodecStatus::Truncated;
    }
    record->bodies.resize(bodyCount);
    for (uint32_t i = 0; i < bodyCount; ++i) {
        ProjectSculptBody& body = record->bodies[i];
        uint8_t flags = 0;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        if (!in.u64(&body.objectId) || !in.u8(&flags) || !in.u32(&vertexCount)
            || !in.u32(&indexCount)) {
            return ProjectCodecStatus::Truncated;
        }
        if ((flags & ~0x07u) != 0u) {
            return ProjectCodecStatus::BadPayload;
        }
        body.renderBothSides = (flags & 0x01u) != 0u;
        body.sourceStale = (flags & 0x02u) != 0u;
        body.hasEdits = (flags & 0x04u) != 0u;
        if (vertexCount == 0 || vertexCount > kMaxMeshVertices || indexCount == 0
            || indexCount > kMaxMeshIndices || (indexCount % 3u) != 0u) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        // Widened to 64 bits before multiplying, and compared against what the
        // section window actually still holds: this is the check that stops a
        // fabricated count from reserving gigabytes, and it happens before the
        // resize rather than after it.
        const uint64_t needed =
                static_cast<uint64_t>(vertexCount) * 12ull + static_cast<uint64_t>(indexCount) * 4ull;
        if (needed > in.remaining()) {
            return ProjectCodecStatus::Truncated;
        }
        body.positions.resize(static_cast<size_t>(vertexCount) * 3u);
        for (float& value : body.positions) {
            if (!in.f32(&value)) {
                return ProjectCodecStatus::Truncated;
            }
        }
        body.indices.resize(indexCount);
        for (uint32_t& index : body.indices) {
            if (!in.u32(&index)) {
                return ProjectCodecStatus::Truncated;
            }
        }
    }
    if (!in.atEnd()) {
        return ProjectCodecStatus::BadPayload;
    }
    return ProjectCodecStatus::Ok;
}

ProjectCodecStatus decodeImportedPayload(ByteReader& in, ProjectImportedRecord* record) {
    uint32_t bodyCount = 0;
    if (!in.u32(&bodyCount)) {
        return ProjectCodecStatus::Truncated;
    }
    if (bodyCount == 0 || bodyCount > kMaxProjectBodies) {
        return ProjectCodecStatus::ImpossibleCount;
    }
    // Smallest possible IMPT entry: identity, an empty name's length, and the
    // three counts. Checked against what is actually left before the resize,
    // for the same reason every other section checks it there.
    if (static_cast<uint64_t>(bodyCount) * (8ull + 2ull + 4ull + 4ull + 4ull) > in.remaining()) {
        return ProjectCodecStatus::Truncated;
    }
    record->bodies.resize(bodyCount);
    for (uint32_t i = 0; i < bodyCount; ++i) {
        ProjectImportedBody& body = record->bodies[i];
        uint16_t nameBytes = 0;
        if (!in.u64(&body.objectId) || !in.u16(&nameBytes)) {
            return ProjectCodecStatus::Truncated;
        }
        // Bounded before it is read, so a fabricated length cannot make this
        // allocate. The ceiling is the domain's own, not a second number.
        if (nameBytes > kMaxImportedMeshNameBytes) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        if (static_cast<uint64_t>(nameBytes) > in.remaining()) {
            return ProjectCodecStatus::Truncated;
        }
        body.name.resize(nameBytes);
        if (nameBytes > 0 && !in.raw(&body.name[0], nameBytes)) {
            return ProjectCodecStatus::Truncated;
        }

        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t batchCount = 0;
        if (!in.u32(&vertexCount) || !in.u32(&indexCount) || !in.u32(&batchCount)) {
            return ProjectCodecStatus::Truncated;
        }
        if (vertexCount == 0 || vertexCount > kMaxImportedMeshVertices || indexCount == 0
            || indexCount > kMaxImportedMeshIndices || (indexCount % 3u) != 0u || batchCount == 0
            || batchCount > kMaxImportedMeshBatches) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        // Widened to 64 bits before multiplying and compared against the
        // section window: this is what stops a fabricated count from reserving
        // gigabytes, and it happens before the resize rather than after it.
        // Positions and normals are three float32 each, indices one u32, and a
        // batch is two u32 and a flags byte.
        const uint64_t needed = static_cast<uint64_t>(vertexCount) * 24ull
                                + static_cast<uint64_t>(indexCount) * 4ull
                                + static_cast<uint64_t>(batchCount) * 9ull;
        if (needed > in.remaining()) {
            return ProjectCodecStatus::Truncated;
        }

        body.positions.resize(static_cast<size_t>(vertexCount) * 3u);
        for (float& value : body.positions) {
            if (!in.f32(&value)) {
                return ProjectCodecStatus::Truncated;
            }
        }
        body.normals.resize(static_cast<size_t>(vertexCount) * 3u);
        for (float& value : body.normals) {
            if (!in.f32(&value)) {
                return ProjectCodecStatus::Truncated;
            }
        }
        body.indices.resize(indexCount);
        for (uint32_t& index : body.indices) {
            if (!in.u32(&index)) {
                return ProjectCodecStatus::Truncated;
            }
        }
        body.batches.resize(batchCount);
        for (ImportedMeshBatch& batch : body.batches) {
            uint8_t flags = 0;
            if (!in.u32(&batch.firstIndex) || !in.u32(&batch.indexCount) || !in.u8(&flags)) {
                return ProjectCodecStatus::Truncated;
            }
            if ((flags & ~0x01u) != 0u) {
                return ProjectCodecStatus::BadPayload;  // a reserved batch-flag bit
            }
            batch.doubleSided = (flags & 0x01u) != 0u;
        }
    }
    if (!in.atEnd()) {
        return ProjectCodecStatus::BadPayload;
    }
    return ProjectCodecStatus::Ok;
}

ProjectCodecStatus decodeCadPayload(ByteReader& in, ProjectCadRecord* record) {
    uint32_t bodyCount = 0;
    if (!in.u32(&bodyCount)) {
        return ProjectCodecStatus::Truncated;
    }
    if (bodyCount == 0 || bodyCount > kMaxProjectBodies) {
        return ProjectCodecStatus::ImpossibleCount;
    }
    // Smallest possible CAD body record: identity, plane, the two ids, the
    // direction, the depth and an entity count.
    if (static_cast<uint64_t>(bodyCount) * (8ull + 1ull + 4ull + 4ull + 1ull + 8ull + 4ull)
        > in.remaining()) {
        return ProjectCodecStatus::Truncated;
    }
    record->bodies.resize(bodyCount);
    for (uint32_t i = 0; i < bodyCount; ++i) {
        ProjectCadBody& body = record->bodies[i];
        CadBodyState& state = body.state;
        uint8_t planeCode = 0;
        uint8_t directionCode = 0;
        uint32_t entityCount = 0;
        if (!in.u64(&body.objectId) || !in.u8(&planeCode) || !in.u32(&state.sketch.nextEntityId)
            || !in.u32(&state.extrude.profileEntityId) || !in.u8(&directionCode)
            || !in.f64(&state.extrude.depth) || !in.u32(&entityCount)) {
            return ProjectCodecStatus::Truncated;
        }
        if (!workplaneFromFileCode(planeCode, &state.sketch.plane)
            || !extrudeDirectionFromFileCode(directionCode, &state.extrude.direction)) {
            return ProjectCodecStatus::InvalidSemanticValue;
        }
        if (entityCount == 0 || entityCount > kMaxSketchEntities) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        // Smallest entity: an id, a kind, and a circle's three values.
        if (static_cast<uint64_t>(entityCount) * (4ull + 1ull + 24ull) > in.remaining()) {
            return ProjectCodecStatus::Truncated;
        }
        state.sketch.entities.reserve(entityCount);
        for (uint32_t e = 0; e < entityCount; ++e) {
            uint32_t id = 0;
            uint8_t kindCode = 0;
            if (!in.u32(&id) || !in.u8(&kindCode)) {
                return ProjectCodecStatus::Truncated;
            }
            SketchEntityKind kind;
            if (!sketchEntityKindFromFileCode(kindCode, &kind)) {
                return ProjectCodecStatus::InvalidSemanticValue;
            }
            switch (kind) {
                case SketchEntityKind::Line: {
                    SketchLine line;
                    if (!in.f64(&line.start.u) || !in.f64(&line.start.v) || !in.f64(&line.end.u)
                        || !in.f64(&line.end.v)) {
                        return ProjectCodecStatus::Truncated;
                    }
                    state.sketch.entities.emplace_back(id, line);
                    break;
                }
                case SketchEntityKind::Polyline: {
                    SketchPolyline polyline;
                    uint8_t flags = 0;
                    uint32_t vertexCount = 0;
                    if (!in.u8(&flags) || !in.u32(&vertexCount)) {
                        return ProjectCodecStatus::Truncated;
                    }
                    if ((flags & ~0x01u) != 0u) {
                        return ProjectCodecStatus::BadPayload;
                    }
                    if (vertexCount == 0 || vertexCount > kMaxPolylineVertices) {
                        return ProjectCodecStatus::ImpossibleCount;
                    }
                    if (static_cast<uint64_t>(vertexCount) * 16ull > in.remaining()) {
                        return ProjectCodecStatus::Truncated;
                    }
                    polyline.closed = (flags & 0x01u) != 0u;
                    polyline.vertices.resize(vertexCount);
                    for (SketchPoint& p : polyline.vertices) {
                        if (!in.f64(&p.u) || !in.f64(&p.v)) {
                            return ProjectCodecStatus::Truncated;
                        }
                    }
                    state.sketch.entities.emplace_back(id, std::move(polyline));
                    break;
                }
                case SketchEntityKind::Rectangle: {
                    SketchRectangle rectangle;
                    if (!in.f64(&rectangle.center.u) || !in.f64(&rectangle.center.v)
                        || !in.f64(&rectangle.width) || !in.f64(&rectangle.height)) {
                        return ProjectCodecStatus::Truncated;
                    }
                    state.sketch.entities.emplace_back(id, rectangle);
                    break;
                }
                case SketchEntityKind::Circle: {
                    SketchCircle circle;
                    if (!in.f64(&circle.center.u) || !in.f64(&circle.center.v)
                        || !in.f64(&circle.radius)) {
                        return ProjectCodecStatus::Truncated;
                    }
                    state.sketch.entities.emplace_back(id, circle);
                    break;
                }
            }
        }
    }
    if (!in.atEnd()) {
        return ProjectCodecStatus::BadPayload;
    }
    return ProjectCodecStatus::Ok;
}

ProjectCodecStatus decodeProjectV1(ByteReader& file, ProjectKind kind, uint8_t headerFlags,
                                   uint32_t sectionCount, ProjectDocument* out,
                                   uint32_t* outSkipped) {
    ProjectDocument document;
    document.kind = kind;
    // Which section TAGS the file carried, as opposed to which ones this reader
    // could understand. The header's flags are a claim about the former, and the
    // singleton rule is about the former too.
    bool sawSceneTag = false;
    bool sawCadTag = false;
    bool sawConstructionTag = false;
    bool sawSculptTag = false;
    bool sawImportedTag = false;

    for (uint32_t i = 0; i < sectionCount; ++i) {
        char tag[4] = {0, 0, 0, 0};
        uint16_t sectionVersion = 0;
        uint16_t flags = 0;
        uint64_t payloadBytes = 0;
        uint32_t crc = 0;
        uint32_t reserved = 0;
        if (!file.raw(tag, 4) || !file.u16(&sectionVersion) || !file.u16(&flags)
            || !file.u64(&payloadBytes) || !file.u32(&crc) || !file.u32(&reserved)) {
            return ProjectCodecStatus::Truncated;
        }
        if ((flags & ~kSectionFlagRequired) != 0u || reserved != 0u) {
            return ProjectCodecStatus::BadSectionHeader;
        }
        if (payloadBytes > file.remaining()) {
            return ProjectCodecStatus::Truncated;
        }
        ByteReader payload(nullptr, 0);
        if (!file.window(static_cast<size_t>(payloadBytes), &payload)) {
            return ProjectCodecStatus::Truncated;
        }
        // Integrity BEFORE meaning, and before the section is even identified:
        // a section that does not checksum is not evidence of anything, not
        // even of its own tag.
        if (crc32IsoHdlc(payloadBytes == 0 ? nullptr : payload.cursor(),
                         static_cast<size_t>(payloadBytes))
            != crc) {
            return ProjectCodecStatus::ChecksumMismatch;
        }

        const bool required = (flags & kSectionFlagRequired) != 0u;
        const bool isScene = tagIs(tag, kSectionTagScene);
        const bool isConstruction = tagIs(tag, kSectionTagConstruction);
        const bool isSculpt = tagIs(tag, kSectionTagSculpt);
        const bool isImported = tagIs(tag, kSectionTagImported);
        const bool isCad = tagIs(tag, kSectionTagCad);

        if (!isScene && !isConstruction && !isSculpt && !isImported && !isCad) {
            if (required) {
                return ProjectCodecStatus::UnknownRequiredSection;
            }
            if (outSkipped != nullptr) {
                ++(*outSkipped);
            }
            continue;  // skipped after its VALIDATED length, never guessed past
        }

        // Duplicate detection is on the TAG, before the version is judged: two
        // CONS sections at two different versions are still two CONS sections,
        // and letting the second one through because the reader happened not to
        // understand the first would be a hole in the singleton rule.
        if ((isScene && sawSceneTag) || (isConstruction && sawConstructionTag)
            || (isSculpt && sawSculptTag) || (isImported && sawImportedTag)
            || (isCad && sawCadTag)) {
            return ProjectCodecStatus::DuplicateSection;
        }
        sawSceneTag = sawSceneTag || isScene;
        sawConstructionTag = sawConstructionTag || isConstruction;
        sawSculptTag = sawSculptTag || isSculpt;
        sawImportedTag = sawImportedTag || isImported;
        sawCadTag = sawCadTag || isCad;

        const uint16_t known =
                isScene ? kSceneSectionVersion
                        : (isConstruction ? kConstructionSectionVersion
                                          : (isSculpt ? kSculptSectionVersion
                                                      : (isImported ? kImportedSectionVersion
                                                                    : kCadSectionVersion)));
        if (sectionVersion != known) {
            if (required) {
                return ProjectCodecStatus::UnsupportedSectionVersion;
            }
            // An OPTIONAL section this reader cannot read is skipped like any
            // other unknown one. A Sculpt project whose retained Construction
            // companion is a version newer than this build still opens, and
            // still opens sculpting the right body — which is the whole point
            // of the companion being optional.
            if (outSkipped != nullptr) {
                ++(*outSkipped);
            }
            continue;
        }

        ProjectCodecStatus status = ProjectCodecStatus::Ok;
        if (isScene) {
            status = decodeScenePayload(payload, &document.scene);
        } else if (isConstruction) {
            status = decodeConstructionPayload(payload, &document.construction);
            document.hasConstruction = (status == ProjectCodecStatus::Ok);
        } else if (isSculpt) {
            status = decodeSculptPayload(payload, &document.sculpt);
            document.hasSculpt = (status == ProjectCodecStatus::Ok);
        } else if (isImported) {
            status = decodeImportedPayload(payload, &document.imported);
            document.hasImported = (status == ProjectCodecStatus::Ok);
        } else {
            status = decodeCadPayload(payload, &document.cad);
            document.hasCad = (status == ProjectCodecStatus::Ok);
        }
        if (status != ProjectCodecStatus::Ok) {
            return status;
        }
    }

    if (!file.atEnd()) {
        return ProjectCodecStatus::BadHeader;  // sectionCount did not describe the whole file
    }
    if (!sawSceneTag) {
        return ProjectCodecStatus::MissingRequiredSection;
    }
    // The header's own claim about what it carries has to be true, or a reader
    // that trusted the flags without parsing would be told something false.
    const uint8_t expectedFlags =
            static_cast<uint8_t>((sawConstructionTag ? kHeaderFlagHasConstruction : 0u)
                                 | (sawSculptTag ? kHeaderFlagHasSculpt : 0u)
                                 | (sawImportedTag ? kHeaderFlagHasImported : 0u)
                                 | (sawCadTag ? kHeaderFlagHasCad : 0u));
    if (headerFlags != expectedFlags) {
        return ProjectCodecStatus::BadHeader;
    }

    const ProjectCodecStatus semantic = validateProjectDocument(document);
    if (semantic != ProjectCodecStatus::Ok) {
        return semantic;
    }
    *out = std::move(document);
    return ProjectCodecStatus::Ok;
}

}  // namespace

ProjectCodecStatus decodeProject(const uint8_t* data, size_t size, ProjectDocument* out,
                                 uint32_t* outSkippedOptionalSections) {
    if (outSkippedOptionalSections != nullptr) {
        *outSkippedOptionalSections = 0;
    }
    if (out == nullptr) {
        return ProjectCodecStatus::BadHeader;
    }
    if (data == nullptr || size < kForgeHeaderBytes) {
        return ProjectCodecStatus::Truncated;
    }

    ByteReader file(data, size);
    char magic[8] = {0};
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t headerBytes = 0;
    uint8_t kindCode = 0;
    uint8_t headerFlags = 0;
    uint32_t sectionCount = 0;
    uint64_t fileBytes = 0;
    if (!file.raw(magic, 8) || !file.u16(&major) || !file.u16(&minor) || !file.u16(&headerBytes)
        || !file.u8(&kindCode) || !file.u8(&headerFlags) || !file.u32(&sectionCount)
        || !file.u64(&fileBytes)) {
        return ProjectCodecStatus::Truncated;
    }
    if (std::memcmp(magic, kForgeMagic, sizeof(kForgeMagic)) != 0) {
        return ProjectCodecStatus::NotForgeFile;
    }
    // THE version dispatch seam. Only the major decides which decoder runs;
    // there is exactly one today, and no fabricated predecessor.
    if (major != kForgeVersionMajor) {
        return ProjectCodecStatus::UnsupportedMajor;
    }
    // A newer MINOR of the same major is readable by contract: additive changes
    // arrive as new optional sections or new section versions, and the
    // per-section rules below decide whether this reader can honour them.
    (void)minor;
    if (headerBytes != kForgeHeaderBytes) {
        return ProjectCodecStatus::BadHeader;
    }
    if (kindCode != static_cast<uint8_t>(ProjectKind::Construction)
        && kindCode != static_cast<uint8_t>(ProjectKind::Sculpt)) {
        return ProjectCodecStatus::BadHeader;
    }
    if ((headerFlags
         & ~(kHeaderFlagHasConstruction | kHeaderFlagHasSculpt | kHeaderFlagHasImported
             | kHeaderFlagHasCad))
        != 0u) {
        return ProjectCodecStatus::BadHeader;
    }
    if (fileBytes != static_cast<uint64_t>(size)) {
        return ProjectCodecStatus::Truncated;
    }
    if (sectionCount == 0 || static_cast<uint64_t>(sectionCount) * kForgeSectionHeaderBytes
                                     > file.remaining()) {
        return ProjectCodecStatus::BadHeader;
    }

    return decodeProjectV1(file, static_cast<ProjectKind>(kindCode), headerFlags, sectionCount,
                           out, outSkippedOptionalSections);
}

}  // namespace forgeshape
