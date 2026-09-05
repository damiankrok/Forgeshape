package com.forgeshape.app;

import android.view.Surface;

/**
 * Minimal JNI boundary between the Android shell and the ForgeShape native renderer.
 *
 * This class owns no geometry, no matrices and no render state. It only forwards
 * Android surface lifecycle events into native code.
 */
final class NativeViewport {

    static {
        System.loadLibrary("forgeshape_native");
    }

    private NativeViewport() {
    }

    /** Starts the native render thread and creates the Vulkan instance. */
    static native void start();

    /** Hands a live Android Surface to the native renderer. */
    static native void surfaceCreated(Surface surface);

    /** Propagates the surface size to the native renderer. */
    static native void surfaceChanged(int width, int height);

    /**
     * Blocks until the native renderer has stopped presenting and released every
     * resource that references the ANativeWindow.
     */
    static native void surfaceDestroyed();

    /**
     * Forwards one complete Android touch event to the native camera owner.
     *
     * <p>Nothing here is interpreted on the Java side beyond mapping the Android
     * tool type onto ForgeShape's own wire codes: {@code action} is the raw
     * {@link android.view.MotionEvent} masked action, and the arrays carry stable
     * pointer ids with view-local pixel coordinates. Native code decides what the
     * gesture means.
     *
     * <p>The stylus arrays are <b>carried, not consumed</b>. No brush, camera or
     * selection rule reads them, so a stylus and a finger tracing the same pixels
     * produce identical geometry. Their ranges and their fallback rules belong to
     * native ForgeShape ({@code forgeshape_input.h}), which is why the values are
     * passed through exactly as Android reported them.
     *
     * <p>Every array is indexed the same way, by pointer index, so slot {@code i}
     * of each describes one pointer. All are at least {@code pointerCount} long
     * and native code reads no further than {@code pointerCount} entries, itself
     * bounded by the native pointer limit.
     *
     * @param actionPointerId the id of the pointer that is lifting on an up-style
     *                        action, or {@code -1} when the action has no such pointer
     * @param toolTypes       one {@code PointerSemantics.TOOL_*} wire code per pointer
     * @param pressures       raw Android pressure per pointer; native clamps to [0, 1]
     *                        and substitutes full pressure for anything non-finite
     * @param tilts           raw {@code AXIS_TILT} per pointer, radians from
     *                        perpendicular; native clamps to [0, pi/2]
     * @param tiltOrientations raw {@code getOrientation} per pointer, radians in the
     *                        screen plane; native wraps to (-pi, pi] and zeroes it
     *                        when there is no tilt
     */
    static native void touchEvent(int action, int actionPointerId, int pointerCount,
                                  int[] ids, float[] xs, float[] ys,
                                  int[] toolTypes, float[] pressures,
                                  float[] tilts, float[] tiltOrientations,
                                  int viewWidth, int viewHeight);

    // -----------------------------------------------------------------------
    // Pointer semantics observation -- DEBUG/TEST ONLY
    // -----------------------------------------------------------------------
    //
    // Test infrastructure, not product functionality. There is no UI for it,
    // no exported component, and native code compiles it to a no-op in a
    // release build. It only READS a bounded snapshot of the last touch event
    // native code received; it holds no model truth, drives nothing, and the
    // Java side never becomes an owner of pointer data because of it.

    /** Stride of one pointer's record inside the debug snapshot. */
    static final int POINTER_SAMPLE_STRIDE = 7;

    /** Largest pointer count native code will accept from one event. */
    static final int POINTER_SAMPLE_MAX = 6;

    /**
     * Length of the array {@link #debugLastPointerEvent} fills: one leading
     * pointer count, then {@link #POINTER_SAMPLE_STRIDE} floats per pointer.
     */
    static final int POINTER_EVENT_STATE_SIZE = 1 + POINTER_SAMPLE_MAX * POINTER_SAMPLE_STRIDE;

    /** Slot 0: how many pointers the last event carried into native code. */
    static final int POINTER_EVENT_COUNT = 0;

    /** Offset of pointer {@code i}'s first slot. */
    static int pointerSampleBase(int index) {
        return 1 + index * POINTER_SAMPLE_STRIDE;
    }

    /** Stable pointer id, relative to {@link #pointerSampleBase}. */
    static final int POINTER_SAMPLE_ID = 0;

    /** View-local x in pixels. */
    static final int POINTER_SAMPLE_X = 1;

    /** View-local y in pixels. */
    static final int POINTER_SAMPLE_Y = 2;

    /** The neutral tool type, one of the {@code PointerSemantics.TOOL_*} codes. */
    static final int POINTER_SAMPLE_TOOL_TYPE = 3;

    /** Sanitized pressure in [0, 1]. */
    static final int POINTER_SAMPLE_PRESSURE = 4;

    /** Sanitized tilt in radians, [0, pi/2], measured from perpendicular. */
    static final int POINTER_SAMPLE_TILT = 5;

    /** Sanitized tilt orientation in radians, (-pi, pi]; 0 when there is no tilt. */
    static final int POINTER_SAMPLE_TILT_ORIENTATION = 6;

    /**
     * DEBUG-ONLY test hook: reads back the platform-neutral pointer data the
     * last {@link #touchEvent} call produced, exactly as native consumers saw
     * it.
     *
     * @param outState caller-allocated array of at least
     *                 {@link #POINTER_EVENT_STATE_SIZE} floats
     * @return the pointer count written, or {@code -1} in a release build or
     *         when the array is too small
     */
    static native int debugLastPointerEvent(float[] outState);

    /** Stops the native render thread and tears down Vulkan. */
    static native void stop();

    // ---------------------------------------------------------------------
    // Construction dimensions.
    //
    // Native code owns the authoritative width/height/depth as double meters,
    // owns their validation, and owns the decision to publish a mesh revision.
    // Java holds no dimension of its own: it reads them to display them, and
    // submits a complete replacement to be judged.
    // ---------------------------------------------------------------------

    /** The dimensions changed and exactly one new mesh revision was published. */
    static final int APPLY_APPLIED = 0;
    /** Valid, but identical to the current dimensions: nothing was published. */
    static final int APPLY_UNCHANGED = 1;
    /** Refused: a value was NaN or infinite. */
    static final int APPLY_REJECTED_NOT_FINITE = 2;
    /** Refused: a value was zero or negative. */
    static final int APPLY_REJECTED_NOT_POSITIVE = 3;
    /** Refused: a value cannot survive as the mesh's float half-extent. */
    static final int APPLY_REJECTED_NOT_REPRESENTABLE = 4;
    /** Applied to the parameters, but the generated mesh was refused downstream. */
    static final int APPLY_PUBLISH_FAILED = 5;
    /**
     * Refused: every value is a usable length, but the primitive's own rule
     * relating two of them is broken. Today the only such rule is a capsule's
     * total height, which cannot be less than its diameter.
     */
    static final int APPLY_REJECTED_RELATION = 6;
    /**
     * Refused by a CAD rule that is not a plain length problem — the sketch
     * would no longer close the extruded profile, for instance. The exact
     * reason is {@link #cadLastStatus()} and is in the log.
     */
    static final int APPLY_REJECTED_CAD = 7;

    /** The active primitive is a box. */
    static final int PRIMITIVE_BOX = 0;
    /** The active primitive is a cylinder. */
    static final int PRIMITIVE_CYLINDER = 1;
    /** The active primitive is a sphere. */
    static final int PRIMITIVE_SPHERE = 2;
    /** The active primitive is a cone. */
    static final int PRIMITIVE_CONE = 3;
    /** The active primitive is a capsule. */
    static final int PRIMITIVE_CAPSULE = 4;
    /** The active primitive is a plane. */
    static final int PRIMITIVE_PLANE = 5;

    /** Index of the ACTIVE primitive kind within that array: a {@code
     *  PRIMITIVE_*} constant carried as a double. */
    static final int PRIMITIVE_KIND = 0;

    /** Length of the array {@link #constructionPrimitive} fills. */
    static final int PRIMITIVE_STATE_SIZE = 13;

    /** Index of the box width within that array; height and depth follow it. */
    static final int PRIMITIVE_BOX_WIDTH = 1;
    /** Index of the cylinder diameter within that array; its height follows. */
    static final int PRIMITIVE_CYLINDER_DIAMETER = 4;
    /** Index of the sphere diameter within that array. */
    static final int PRIMITIVE_SPHERE_DIAMETER = 6;
    /** Index of the cone bottom diameter within that array; its height follows. */
    static final int PRIMITIVE_CONE_BOTTOM_DIAMETER = 7;
    /** Index of the capsule diameter within that array; its total height follows. */
    static final int PRIMITIVE_CAPSULE_DIAMETER = 9;
    /** Index of the plane width within that array; its depth follows. */
    static final int PRIMITIVE_PLANE_WIDTH = 11;

    /**
     * Reads the authoritative Construction primitive state.
     *
     * <p>Every primitive's parameters are reported, not only the active one, so
     * the panel can show an inactive primitive's remembered values without
     * inventing defaults of its own. Each slot has one fixed meaning whatever
     * the active kind is, so nothing here is positional-by-kind.
     *
     * @param outState caller-allocated array of at least
     *                 {@link #PRIMITIVE_STATE_SIZE} doubles, filled with
     *                 <ol start="0">
     *                   <li>the active kind, {@link #PRIMITIVE_BOX},
     *                       {@link #PRIMITIVE_CYLINDER} or
     *                       {@link #PRIMITIVE_SPHERE}, {@link #PRIMITIVE_CONE}
     *                       or {@link #PRIMITIVE_CAPSULE}</li>
     *                   <li>box width, height, depth in meters (indices 1-3)</li>
     *                   <li>cylinder diameter, height in meters (indices 4-5)</li>
     *                   <li>sphere diameter in meters (index 6)</li>
     *                   <li>cone bottom diameter, height in meters (indices 7-8)</li>
     *                   <li>capsule diameter, total height in meters (indices 9-10)</li>
     *                   <li>plane width, depth in meters (indices 11-12)</li>
     *                 </ol>
     */
    static native void constructionPrimitive(double[] outState);

    // -----------------------------------------------------------------------
    // Shape submission: one method per primitive.
    //
    // There is deliberately no generic "kind plus three doubles" entry point.
    // Each method's parameter list IS that primitive's parameter list, so a
    // caller cannot send a cylinder's height where a box's depth belongs, and a
    // sphere request has no second or third number to get wrong.
    //
    // All three share one contract: native code validates before writing
    // anything, changes the kind and those parameters only if every value is
    // usable, and publishes exactly one new mesh revision only if something
    // actually changed. Java never splits this into a set step and a publish
    // step, and never changes the kind without the parameters that go with it.
    // The transform is not touched, in any outcome.
    // -----------------------------------------------------------------------

    /**
     * Makes the object a box of exactly these dimensions.
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyConstructionBox(double widthMeters, double heightMeters,
                                           double depthMeters);

    /**
     * Makes the object a cylinder of exactly this diameter and height.
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyConstructionCylinder(double diameterMeters, double heightMeters);

    /**
     * Makes the object a sphere of exactly this diameter.
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyConstructionSphere(double diameterMeters);

    /**
     * Makes the object a cone of exactly this bottom diameter and height.
     *
     * <p>The apex radius is zero by definition and is not a parameter: there is
     * no top diameter and no frustum here.
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyConstructionCone(double bottomDiameterMeters, double heightMeters);

    /**
     * Makes the object a capsule of exactly this diameter and total height.
     *
     * <p>{@code totalHeightMeters} is the <b>whole</b> height, hemispherical ends
     * included — not the length of the cylindrical middle, which is derived from
     * the two and never stored. It may not be less than the diameter, because
     * the two ends alone are already that tall; that case is refused with
     * {@link #APPLY_REJECTED_RELATION}. Equality is valid and describes a
     * capsule with no middle at all.
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyConstructionCapsule(double diameterMeters, double totalHeightMeters);

    /**
     * Makes the object a flat, zero-thickness plane of exactly this width and
     * depth, centred at the local origin with its canonical front along local
     * +Y.
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyConstructionPlane(double widthMeters, double depthMeters);

    // ---------------------------------------------------------------------
    // Construction transform.
    //
    // Position is authoritative double METERS, rotation is authoritative double
    // DEGREES, scale is an authoritative unitless double multiplier, and all
    // three live in native code. A transform edit moves, rotates and stretches
    // the body without touching its mesh: no revision is published and nothing
    // is uploaded, because the mesh is the body LOCAL geometry and only a
    // derived matrix changes.
    // ---------------------------------------------------------------------

    /** How many doubles {@link #boxTransform} fills: position, rotation, scale. */
    static final int TRANSFORM_SIZE = 9;
    /** Index of position X in the {@link #boxTransform} array; Y and Z follow. */
    static final int TRANSFORM_POSITION = 0;
    /** Index of rotation X; Y and Z follow. */
    static final int TRANSFORM_ROTATION = 3;
    /** Index of scale X; Y and Z follow. */
    static final int TRANSFORM_SCALE = 6;

    /**
     * Reads the authoritative Construction transform.
     *
     * @param outPlacement caller-allocated array of at least
     *                     {@link #TRANSFORM_SIZE} doubles, filled with position
     *                     X/Y/Z in meters, rotation X/Y/Z in degrees, then
     *                     scale X/Y/Z as unitless multipliers
     */
    static native void boxTransform(double[] outPlacement);

    /**
     * Submits all nine Construction transform values as one atomic request.
     *
     * <p>Native code validates all nine before writing any of them, so a
     * refused scale cannot leave a position half applied. Zero and negative are
     * valid for every position and every rotation — a coordinate is a place and
     * an angle is a direction, neither is a size — but a SCALE must be strictly
     * positive: zero would make the transform singular and negative would be a
     * Mirror, which this product does not have. That case returns
     * {@link #APPLY_REJECTED_NOT_POSITIVE}.
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyBoxTransform(double positionXMeters, double positionYMeters,
                                        double positionZMeters, double rotationXDegrees,
                                        double rotationYDegrees, double rotationZDegrees,
                                        double scaleX, double scaleY, double scaleZ);

    // ---------------------------------------------------------------------
    // Product mode and the Frozen Sculpt Mesh.
    //
    // A body has a SOURCE representation (a Construction Source, an Imported
    // Mesh or a CAD Body) and may also own a Frozen Sculpt Mesh: a copy of its
    // local source mesh taken at the moment of Freeze, with its own
    // SculptRevision and its own per-body stroke history. Native code owns
    // which one is active. Java may request a mode; it holds none, holds no
    // vertex and holds no brush setting.
    // ---------------------------------------------------------------------

    /** Editing the exact primitive, its parameters and its placement. */
    static final int MODE_CONSTRUCTION = 0;
    /** Editing the Frozen Sculpt Mesh with the Grab brush. */
    static final int MODE_SCULPT = 1;

    /** The requested mode change happened. */
    static final int SCULPT_OK = 0;
    /** The Construction mesh could not be frozen; the mode did not change. */
    static final int SCULPT_FAILED_FREEZE = 1;
    /** Nothing has ever been frozen, so there is no sculpt mesh to return to. */
    static final int SCULPT_NOTHING_FROZEN = 2;
    /** The active body is a CAD Body, which this stage does not sculpt. */
    static final int SCULPT_REFUSED_CAD_BODY = 3;

    // -----------------------------------------------------------------------
    // The four sculpt tools.
    //
    // One brush kernel carries all four: they share the stroke lifecycle, the
    // hit test, the affected set, the falloff and the Radius/Strength contract,
    // and differ only in how they displace the vertices they captured. Native
    // code owns which one is active; Java may request one and is told which is
    // actually active.
    // -----------------------------------------------------------------------

    /** Drag the surface with the finger, in the camera plane. */
    static final int TOOL_GRAB = 0;
    /** Deposit material along the normals the surface had at stroke start. */
    static final int TOOL_CLAY = 1;
    /** Relax each vertex toward its 1-ring neighbour average. */
    static final int TOOL_SMOOTH = 2;
    /** Expand along the normals the surface has right now. */
    static final int TOOL_INFLATE = 3;

    /** Length of the array {@link #sculptState} fills. */
    static final int SCULPT_STATE_SIZE = 12;

    /** {@link #MODE_CONSTRUCTION} or {@link #MODE_SCULPT}. */
    static final int SCULPT_MODE = 0;
    /** 1 when a Frozen Sculpt Mesh exists. */
    static final int SCULPT_HAS_MESH = 1;
    /** The mesh's own revision, which is not a mesh-store revision. */
    static final int SCULPT_REVISION = 2;
    /** Frozen vertex count. */
    static final int SCULPT_VERTEX_COUNT = 3;
    /** Frozen index count. */
    static final int SCULPT_INDEX_COUNT = 4;
    /** Brush radius in screen pixels. */
    static final int SCULPT_RADIUS_PIXELS = 5;
    /** Brush strength. */
    static final int SCULPT_STRENGTH = 6;
    /** 1 when the Construction Source changed after the Freeze. */
    static final int SCULPT_SOURCE_STALE = 7;
    /**
     * How many strokes have been started for the life of the SESSION.
     *
     * <p>Diagnostic only. This counts strokes on frozen meshes that no longer
     * exist, so it cannot answer whether re-freezing would destroy anything the
     * user still has — use {@link #SCULPT_HAS_EDITS} for that.
     */
    static final int SCULPT_STROKE_COUNT = 8;
    /** The object id — the same one the Construction object carries. */
    static final int SCULPT_OBJECT_ID = 9;
    /** The active tool, one of the {@code TOOL_*} constants. */
    static final int SCULPT_TOOL = 10;
    /**
     * 1 when the <b>current</b> Frozen Sculpt Mesh has user sculpt edits.
     *
     * <p>Native code owns what counts as an edit; this layer only reads the
     * answer. Reset by every Freeze, and unaffected by a gesture that began and
     * was abandoned to navigation without moving a vertex.
     */
    static final int SCULPT_HAS_EDITS = 11;

    // -----------------------------------------------------------------------
    // The scene: several Construction Bodies
    // -----------------------------------------------------------------------
    //
    // This layer holds NO body list and NO model selection of its own. It asks
    // native code how many bodies exist, what their ids are in scene order and
    // which one is active, every time it needs to know. That is what keeps the
    // Objects list, the Property Inspector and the viewport from disagreeing.

    /** @return how many Construction Bodies the scene currently holds */
    static native int sceneBodyCount();

    /**
     * Fills {@code outIds} with every body's ObjectId in scene (insertion)
     * order.
     *
     * @return how many ids were written, never more than the array length
     */
    static native int sceneBodyIds(long[] outIds);

    /**
     * The ObjectId meaning "no object". Zero, matching kNoObject below JNI, and
     * declared once here so nothing above it writes a bare literal.
     */
    static final long NO_OBJECT = 0L;

    /** @return the ObjectId of the body the Construction editors act on */
    static native long sceneActiveBodyId();

    /**
     * The body's stored name, or {@code ""} when it has none.
     *
     * <p>Only an Imported Mesh carries one — it arrives named by the file it
     * came from. A Construction Body returns the empty string, which is the
     * signal to label it {@code Body #id} exactly as before, not a name.
     */
    static native String sceneBodyName(long objectId);

    /**
     * Whether a body's geometry came from a file rather than from parameters.
     *
     * <p>Asked rather than inferred from an empty name or an absent dimension:
     * the representation is a domain fact, and the controls an imported body
     * has no answer for are withdrawn on this and nothing else.
     */
    static native boolean sceneBodyIsImported(long objectId);

    /** @return whether the body the editors act on is an Imported Mesh */
    static native boolean sceneActiveBodyIsImported();

    /**
     * Makes an existing body the edit target. Selection only: publishes
     * nothing, mints no revision and changes no ObjectId.
     *
     * @return {@link #SCULPT_OK}, or a non-OK status when the id is unknown or
     *         the product is in Sculpt mode (where the target is fixed)
     */
    static native int sceneSelectBody(long objectId);

    /**
     * Adds a Construction Body with the startup defaults, appends it to the
     * scene and makes it active.
     *
     * @return the new body's ObjectId, or 0 when the add was refused
     */
    static native long sceneAddBody();

    // -----------------------------------------------------------------------
    // Delete (UI-OWNER-45)
    // -----------------------------------------------------------------------
    //
    // Status codes in step with the native kDelete* constants. They are a JNI
    // transport detail; the domain's own vocabulary is DeleteBodyStatus.

    /** The body was removed, as exactly one history transaction. */
    static final int DELETE_OK = 0;
    /** No body in the scene carries that id. */
    static final int DELETE_UNKNOWN_BODY = 1;
    /**
     * The scene holds exactly one body.
     *
     * <p>A project is never empty: an empty scene is Home, reached only by
     * closing the project, and a {@code .forge} file with zero bodies is
     * refused. So the last body is refused by name rather than removed and
     * replaced with a primitive nobody asked for.
     */
    static final int DELETE_REFUSED_LAST_BODY = 2;
    /** A Construction edit is open; its captured pre-state names this body. */
    static final int DELETE_REFUSED_EDIT_IN_PROGRESS = 3;
    /**
     * The product is in Sculpt mode.
     *
     * <p>The same rule body switching and Undo/Redo already follow: the Sculpt
     * target is fixed for the duration of the mode, and a delete there could
     * not be undone until the user left it.
     */
    static final int DELETE_REFUSED_IN_SCULPT = 4;

    /**
     * The body is a producer with face-supported CAD dependents (`CAD-A3`):
     * refused rather than cascaded. Delete the dependents first.
     */
    static final int DELETE_REFUSED_HAS_DEPENDENTS = 5;

    /**
     * Removes one body from the project, as exactly one history transaction.
     *
     * <p>Representation-neutral: a Construction Body, an Imported Mesh and
     * either of them carrying a retained sculpt mesh all leave the same way and
     * come back the same way. Undo restores the SAME object — its identity, its
     * geometry and its sculpt state — because the history holds it rather than
     * destroying it.
     *
     * <p>A refusal changes nothing at all.
     *
     * @return one of the {@code DELETE_*} constants
     */
    static native int sceneDeleteBody(long objectId);

    // -----------------------------------------------------------------------
    // Construction history
    // -----------------------------------------------------------------------
    //
    // This layer holds NO history. It asks whether an undo is available, asks
    // for one, and re-reads native state afterwards. There is deliberately no
    // Java-side depth counter and no list of parameter snapshots: a second copy
    // of the history would be a second answer to "what does undo do next".
    //
    // The construction* pair is refused while sculpting: Construction Undo is
    // not Sculpt Undo. The chrome calls the {@code history*} family instead,
    // which dispatches on the product mode in native code where the mode lives.

    /** The step was performed. */
    static final int HISTORY_OK = 0;
    /** There was nothing to undo or redo; nothing changed. */
    static final int HISTORY_NOTHING_TO_DO = 1;
    /**
     * Refused: the Construction history is not touched while sculpting.
     *
     * <p>Only {@link #constructionUndo} and {@link #constructionRedo} return
     * this. {@link #historyUndo} never does — in Sculpt it means the Sculpt
     * history, so there is nothing there to refuse.
     */
    static final int HISTORY_REFUSED_IN_SCULPT = 2;
    /**
     * Refused for now: a sculpt stroke is in progress. Self-clearing — the
     * finger lifts and the step is available.
     */
    static final int HISTORY_STROKE_ACTIVE = 3;
    /**
     * Asked for a Sculpt step where there is no Sculpt history to step: not in
     * Sculpt mode, or the active body has no sculpt mesh.
     */
    static final int HISTORY_UNAVAILABLE = 4;
    /**
     * Never returned by a step. It names the one condition a step cannot
     * report because it happened earlier: a stroke too large for the history's
     * byte budget applied but was not retained, so it cannot be taken back.
     * Read through {@link #sculptHistoryNotRetainedCount}.
     */
    static final int HISTORY_ENTRY_NOT_RETAINED = 5;

    /** @return whether a Construction step can be undone right now */
    static native boolean constructionUndoAvailable();

    /** @return whether a Construction step can be redone right now */
    static native boolean constructionRedoAvailable();

    /**
     * The active body's current mesh revision.
     *
     * <p>Read-back only, like {@link #constructionPrimitive}: nothing on this
     * side keeps it or derives anything from it. It exists so verification can
     * state how many publications one product act cost.
     */
    static native long constructionMeshRevision();

    // -----------------------------------------------------------------------
    // The Construction transform gizmo
    // -----------------------------------------------------------------------
    //
    // This side owns WHEN there is a gizmo — which product mode, which Tool Rail
    // context, whether a body exists. It owns nothing about WHERE the handles
    // are, how large they are, which one a touch landed on, what a drag means in
    // world space, or when a transaction opens and closes: every one of those
    // needs the camera, the projection and the Construction placement, and each
    // has exactly one owner below JNI.
    //
    // There is deliberately no Java transform, no parallel pivot and no second
    // solver here. A drag writes the authoritative placement directly, so the
    // exact-value editors read the same numbers mid-drag that they read at rest.

    /** Direct manipulation with the arrow handles and the plane squares. */
    static final int GIZMO_MODE_MOVE = 0;
    /** Direct manipulation with the axis rings. */
    static final int GIZMO_MODE_ROTATE = 1;
    /** Direct manipulation with the cube handles. Local space only. */
    static final int GIZMO_MODE_SCALE = 2;

    /** Handles are constrained to the world axes. */
    static final int GIZMO_SPACE_WORLD = 0;
    /** Handles are constrained to the body own axes. */
    static final int GIZMO_SPACE_LOCAL = 1;

    /** Size of the array {@link #gizmoState(double[])} fills. */
    static final int GIZMO_STATE_SIZE = 12;
    /** 1 when the workspace is offering direct transform at all. */
    static final int GIZMO_ACTIVE = 0;
    /** One of the {@code GIZMO_MODE_*} constants. */
    static final int GIZMO_MODE = 1;
    /** 1 while a drag holds a pointer. */
    static final int GIZMO_CAPTURING = 2;
    /** The captured pointer id, or -1. */
    static final int GIZMO_POINTER_ID = 3;
    /** The captured handle: one of the {@code GIZMO_HANDLE_*} constants. */
    static final int GIZMO_HANDLE = 4;
    /** The ObjectId the drag is bound to, or 0. */
    static final int GIZMO_OBJECT_ID = 5;
    /** How many updates the current or last drag applied. Diagnostic. */
    static final int GIZMO_DRAG_UPDATES = 6;
    /** 1 when handles are actually on screen for the active body. */
    static final int GIZMO_VISIBLE = 7;
    /** World length of one reference unit at the pivot, or 0. */
    static final int GIZMO_WORLD_PER_UNIT = 8;
    /** How many drags have committed a history step this session. Monotone. */
    static final int GIZMO_COMMITTED_DRAGS = 9;
    /** One of the {@code GIZMO_SPACE_*} constants. */
    static final int GIZMO_SPACE = 10;
    /**
     * 1 when the space is the user choice rather than a consequence of the mode.
     *
     * <p>The workspace draws the space selector exactly when this is 1, which is
     * everywhere except Scale — a world-axis scale of a rotated body is a shear,
     * so the choice is withdrawn rather than shown and refused.
     */
    static final int GIZMO_SPACE_SELECTABLE = 11;

    /** No handle is under that pixel. */
    static final int GIZMO_HANDLE_NONE = 0;
    static final int GIZMO_HANDLE_AXIS_X = 1;
    static final int GIZMO_HANDLE_AXIS_Y = 2;
    static final int GIZMO_HANDLE_AXIS_Z = 3;
    static final int GIZMO_HANDLE_PLANE_XY = 4;
    static final int GIZMO_HANDLE_PLANE_XZ = 5;
    static final int GIZMO_HANDLE_PLANE_YZ = 6;
    /** The centre cube: one factor on all three axes. Scale only. */
    static final int GIZMO_HANDLE_UNIFORM = 7;

    /**
     * Whether the workspace is currently offering direct transform.
     *
     * <p>Refused with no effect while sculpting — a guard, not a UI decision.
     * The workspace also withdraws the controls there, because a Construction
     * transform is not a sculpt edit and a control that could be read as one
     * would be a lie; removing a control is not removing a guard.
     *
     * <p>Turning it off cancels any drag in progress: a captured handle whose
     * gizmo has gone cannot be released by the user.
     */
    static native void setGizmoActive(boolean active);

    /**
     * Chooses Move, Rotate or Scale.
     *
     * <p>Presentation state: no mesh revision, no geometry publication, no
     * history step. Returns false for an unknown index and while a drag holds a
     * pointer — a mode must not change under a moving finger.
     *
     * <p>Entering Scale forces {@link #GIZMO_SPACE_LOCAL} and remembers the
     * space that was in use; leaving Scale puts that space back, so a round trip
     * through Scale does not quietly change what a Move handle means.
     */
    static native boolean setGizmoMode(int mode);

    /**
     * Chooses the world or the body own axes.
     *
     * <p>Presentation state on the same terms as the mode. Returns false for an
     * unknown index, while a drag holds a pointer, and for
     * {@link #GIZMO_SPACE_WORLD} while the mode is Scale — a world-axis scale of
     * a rotated body is a shear and cannot be stored in the transform at all.
     * The workspace withdraws the selector there as well; removing a control is
     * not removing a guard.
     */
    static native boolean setGizmoSpace(int space);

    /**
     * How many physical pixels one reference unit is on this display.
     *
     * <p>The one number this side owns about gizmo size. The sizes themselves —
     * how long a shaft is, how wide its hit corridor is — are the domain's, so
     * the 48 dp interactive floor is a property of the product rather than of a
     * layout file. Refused, changing nothing, for a non-finite or absurd scale.
     */
    static native boolean setGizmoPixelScale(float scale);

    /** Fills {@code out} (length {@link #GIZMO_STATE_SIZE}) with gizmo state. */
    static native void gizmoState(double[] out);

    /**
     * Which handle a pixel would grab, without grabbing it.
     *
     * <p>Pure: it starts nothing and mutates nothing. Verification uses it to
     * ask where a handle is rather than encoding a coordinate that would be true
     * for one window and one camera only.
     *
     * @return one of the {@code GIZMO_HANDLE_*} constants
     */
    static native int gizmoHitTest(float x, float y);

    /**
     * Where a handle can be grabbed, in view-local pixels.
     *
     * <p>Derived from the same projection the hit test uses, so a synthetic
     * pointer sent here reaches the same handle a finger would. Returns false,
     * writing nothing, when that handle is not on screen.
     *
     * @param handle one of the {@code GIZMO_HANDLE_*} constants. {@code
     *               GIZMO_HANDLE_NONE} asks for the PIVOT rather than a handle
     *               — in Move and Rotate not something that can be grabbed, but
     *               the point a caller needs in order to know which way along
     *               the screen a handle actually runs
     * @param out    two floats: x, y
     */
    static native boolean gizmoHandlePoint(int handle, float[] out);

    /** Length of the array {@link #debugCameraPose} fills. */
    static final int CAMERA_POSE_SIZE = 3;
    /** Yaw in radians. */
    static final int CAMERA_POSE_YAW = 0;
    /** Pitch in radians. */
    static final int CAMERA_POSE_PITCH = 1;
    /** Orbit distance in meters. */
    static final int CAMERA_POSE_DISTANCE = 2;

    /**
     * DEBUG-ONLY: reads the camera's orbit pose.
     *
     * <p>Verification infrastructure, not product functionality — no UI reaches
     * it and it is a no-op in a release build. It exists so a case can assert
     * that a captured gizmo handle did <b>not</b> orbit the camera, which is
     * otherwise unobservable from this side.
     */
    static native void debugCameraPose(float[] out);

    /**
     * DEBUG-ONLY: places the camera's orbit pose, clamped exactly as a gesture
     * would clamp it.
     *
     * <p>So that a case can say "from a viewpoint where this axis is nearly
     * edge-on to the viewer" without first synthesising an orbit gesture of
     * exactly the right pixel length — which would make the case a test of
     * gesture arithmetic rather than of the solver it is about. A no-op
     * returning false in a release build.
     */
    static native boolean debugSetCameraPose(float yaw, float pitch, float distance);

    // -----------------------------------------------------------------------
    // Project persistence
    // -----------------------------------------------------------------------
    //
    // Two calls, and between them everything Java knows about a ForgeShape
    // project file: bytes out, bytes in. The FORMAT is native and
    // platform-neutral; the Java layer owns only where those bytes are kept, in
    // {@link ProjectSlot}. Nothing about presentation crosses — the camera, the
    // open panels, the display unit, the theme, the held tool and the brush are
    // session state, not project truth, and `.forge` v1 carries none of them.

    /** The project was saved, or loaded and is now live. */
    static final int PROJECT_OK = 0;

    /** There was nothing to read: no saved project, or an empty file. */
    static final int PROJECT_NO_DATA = 1;

    /** The bytes are not a ForgeShape project at all. */
    static final int PROJECT_NOT_A_PROJECT = 2;

    /** A ForgeShape project written by a newer, incompatible version. */
    static final int PROJECT_UNSUPPORTED_VERSION = 3;

    /** A ForgeShape project that is damaged: truncated, or it fails its checksum. */
    static final int PROJECT_DAMAGED = 4;

    /** Structurally readable, but it describes something the model refuses. */
    static final int PROJECT_INVALID = 5;

    /** Refused because a Construction edit is still open. */
    static final int PROJECT_BUSY = 6;

    /**
     * Encodes the running project to portable {@code .forge} v1 bytes.
     *
     * <p>Reads only: it publishes no mesh, mints no revision and cannot change
     * the mode, the scene or the active body. The file's project kind is taken
     * from the mode the user is in right now, so a project saved while sculpting
     * reopens showing the sculpt mesh.
     *
     * @return the complete file, or null when native code could not encode the
     *         project — which is reported rather than written out as a file
     *         nothing could open
     */
    static native byte[] encodeProject();

    /**
     * Exports the running project as GLB 2.0 bytes.
     *
     * <p>Reads only: it publishes no mesh, mints no revision, changes no mode
     * and cannot touch the scene, the history or either { .forge} slot.
     * Geometry is evaluated fresh from the Construction sources — or taken from
     * the Frozen Sculpt Meshes while sculpting — so what is exported is what the
     * project currently IS, never a decoded file and never a GPU buffer.
     *
     * <p>ForgeShape and glTF already agree on metres, on +Y up, on right-handed
     * space, on column-major matrices and on counter-clockwise winding, so the
     * export applies no conversion of any kind. See
     * { forgeshape_gltf_export.h} for where each of those facts is defined.
     *
     * @return the complete { .glb}, or null when nothing could be exported
     */
    static native byte[] exportGlb();

    // ---------------------------------------------------------------------
    // IMPORT-01A — durable import
    //
    // The product path. It reads a `.glb` with the same parser the diagnostic
    // preview uses and turns what it describes into REAL bodies: one per
    // supported top-level mesh node, each with an ObjectId from the scene's own
    // allocator, a row in the Objects list, the ordinary Move/Rotate/Scale
    // gizmo, one Undo step for the whole import, and a place in every `.forge`
    // file the project is saved into afterwards.
    //
    // An imported body is NON-PARAMETRIC: it has no Construction Source and
    // nothing may invent one, so `Shape` is withdrawn for one and refused below
    // JNI. It CAN be sculpted (`IMPORT-01B`), seeded from its own geometry,
    // with the imported arrays immutable throughout.
    // ---------------------------------------------------------------------

    /**
     * Where commit refusals begin, so the two vocabularies stay apart.
     *
     * <p>A status below this is a {@code GlbImportStatus} — something about the
     * FILE. A status at or above it is {@code IMPORT_COMMIT_BASE} plus an
     * {@code ImportCommitStatus} — something about the PROJECT. Must stay in
     * step with {@code kImportCommitStatusBase} in forgeshape_jni.cpp.
     */
    static final int IMPORT_COMMIT_BASE = 1000;

    /**
     * Reads GLB bytes and creates durable Imported Mesh bodies from them.
     *
     * <p>Atomic: every object is built and validated before any of them reaches
     * the scene, so a refusal creates no body, mints no ObjectId, records no
     * history step and does not move the project fingerprint. On success the
     * whole import is exactly one Undo, and the first object it created is
     * selected.
     *
     * @return {@link #IMPORT_OK}, a {@code GlbImportStatus} ordinal when the
     *         file was refused, or {@link #IMPORT_COMMIT_BASE} plus an
     *         {@code ImportCommitStatus} ordinal when the project refused it
     */
    static native int importGlbDurable(byte[] bytes);

    /** @return the stable refusal token for a commit status, e.g. {@code TooManyObjects} */
    static native String glbCommitStatusToken(int status);

    /** @return one of the {@code IMPORT_CATEGORY_*} values for a commit status */
    static native int glbCommitStatusCategory(int status);

    // ---------------------------------------------------------------------
    // GLB-IMPORT-R0/R1 — the diagnostic imported mesh preview
    //
    // A DIAGNOSTIC, not production import. It reads a `.glb` — one ForgeShape
    // wrote, or an ordinary static mesh another tool wrote — with a parser that
    // shares nothing with the writer, and shows the result in the viewport so
    // the geometry in the FILE can be compared with the geometry in the SCENE.
    //
    // Nothing here is project truth. The preview has no ObjectId, no
    // Construction Source, no sculpt representation and no history; it is never
    // saved, autosaved, checkpointed or re-exported; and it is gone when the
    // process is. Since `IMPORT-01A` it has no user-facing control either: the
    // product's Import GLB… is the durable path above, and these entries are
    // reached only by the verification suites.
    // ---------------------------------------------------------------------

    /** Import succeeded. Any other value is a {@code GlbImportStatus} ordinal. */
    static final int IMPORT_OK = 0;

    /**
     * Parses GLB bytes and loads them as the session's imported preview.
     *
     * <p>Fails closed: on anything but {@link #IMPORT_OK} no preview is
     * created, an existing preview is untouched, and the project is untouched.
     * Loading does not show the preview — {@link #setGlbPreviewVisible} does,
     * so a successful import cannot yank the viewport out from under the user.
     *
     * @return {@link #IMPORT_OK}, or the ordinal of the refusal reason
     */
    static native int importGlbPreview(byte[] bytes);

    /**
     * The bounded category of a refusal, for the one sentence the user is
     * shown. The detailed reason stays a token in the log.
     */
    static final int IMPORT_CATEGORY_UNREADABLE = 0;
    static final int IMPORT_CATEGORY_UNSUPPORTED = 1;
    static final int IMPORT_CATEGORY_INCONSISTENT = 2;

    /** @return the stable refusal token for a status ordinal, e.g. {@code HasSkin} */
    static native String glbImportStatusToken(int status);

    /** @return one of the {@code IMPORT_CATEGORY_*} values for a status ordinal */
    static native int glbImportStatusCategory(int status);

    /** Forgets the preview and releases its meshes. Safe with none loaded. */
    static native void clearGlbPreview();

    /** @return whether a preview is currently held */
    static native boolean glbPreviewLoaded();

    /**
     * Shows the preview INSTEAD of the project, or stops. Ignored when nothing
     * is loaded, so "visible" can never be true with nothing to show.
     *
     * <p>The project is not affected in either direction: it is still there,
     * still selected and still editable, and switching back restores exactly
     * what was on the screen.
     */
    static native void setGlbPreviewVisible(boolean visible);

    /** @return whether the viewport is currently drawing the preview */
    static native boolean glbPreviewVisible();

    /**
     * Fills {@code out} with the file's mesh, vertex and triangle counts and,
     * when {@code out} is at least four long, the draw-batch count. Zeros when
     * empty.
     *
     * <p>A batch is one TRIANGLES primitive. One mesh becomes several batches
     * when the file states several primitives, and several primitives may share
     * one POSITION accessor, so the batch count is a renderer fact and the
     * other three are the file's.
     */
    static native void glbPreviewCounts(int[] out);

    /**
     * Fills {@code out} with the preview's world bounds, min xyz then max xyz.
     *
     * <p>World, because the node transform is baked into the vertices — so
     * these are the numbers that say a node matrix reached the geometry, and
     * reached it the right way round.
     *
     * @return false when nothing is loaded, leaving {@code out} untouched
     */
    static native boolean glbPreviewBounds(float[] out);

    /**
     * DEBUG/TEST ONLY: whether every loaded preview batch renders both sides.
     *
     * <p>Verification infrastructure. The seam that decides culling is the
     * published mesh's own two-sided flag, and a case has to be able to read
     * it — a screenshot of a flat grey surface cannot tell a culled back face
     * from a drawn one. Reads only; false with nothing loaded.
     */
    static native boolean debugPreviewRendersBothSides();

    /**
     * The deterministic Nomad-like compatibility fixture, as GLB bytes.
     *
     * <p>A TEST SEAM, not a product path — nothing in the app calls it. The
     * owner's own low-poly character is external to this repository, so
     * GLB-IMPORT-R1 is proven against a synthetic file carrying the same
     * structural features: a node matrix, seven TRIANGLES primitives over one
     * shared POSITION accessor, no NORMAL, ignored colour and UV attributes,
     * and a double-sided material.
     */
    static native byte[] nomadLikeGlbFixture();

    /**
     * The roundtrip diagnostic: export the live scene, read the bytes back with
     * the independent parser, compare both against domain truth.
     *
     * <p>Reads only. The report is {@code key=value} lines and begins with
     * {@code verdict=ROUNDTRIP_EQUIVALENT} or {@code ROUNDTRIP_MISMATCH}.
     */
    static native String glbRoundtripReport();

    /** The same comparison against bytes the caller already has. Reads only. */
    static native String glbCompareReport(byte[] bytes);

    /**
     * Replaces the running project with the one these bytes describe, or
     * changes nothing at all.
     *
     * <p>Fail-closed: the bytes are decoded, checksummed and completely
     * validated into temporary native state before anything live is touched. A
     * refusal leaves the current scene, every Frozen Sculpt Mesh, the active
     * mode, the active body and the session history exactly as they were.
     *
     * <p>A successful load clears the Construction Undo/Redo history, because
     * the loaded document starts a fresh session and a step recorded before it
     * would describe a scene that no longer exists.
     *
     * @return one of the {@code PROJECT_*} codes above
     */
    static native int loadProject(byte[] bytes);

    /**
     * Decodes and validates bytes as a project <b>without applying them</b>.
     *
     * <p>The recovery flow has to know whether a candidate is worth offering
     * before it may put anything in front of the user, and the only honest way
     * to know is to run the real decoder. This runs it and throws the result
     * away: no mesh is published, no scene replaced, no mode changed and no
     * history cleared. A file that passes here is still not loaded.
     *
     * @return one of the {@code PROJECT_*} codes; {@code PROJECT_OK} means
     *         {@link #loadProject} would accept it
     */
    static native int validateProject(byte[] bytes);

    /**
     * A cheap fingerprint of everything a {@code .forge} document would contain
     * right now.
     *
     * <p>Autosave's whole economy rests on this. It answers "would encoding
     * produce a different file than the last checkpoint did?" for the cost of a
     * hash over the semantic values, so the question can be asked after every
     * edit and after every gesture without serializing the project to find out.
     *
     * <p>It hashes the <b>values</b>, not the domain's update counters, and that
     * is what makes it correct: an undo deliberately does not advance
     * {@code updateCount}, so a counter-based answer would call an undone
     * project unchanged and quietly stop protecting it.
     *
     * <p>A change <b>detector</b>, never an identity. Equal fingerprints mean
     * "no checkpoint needed"; nothing may treat one as proof that two projects
     * are the same file.
     */
    static native long projectFingerprint();

    // -----------------------------------------------------------------------
    // Home and the project lifecycle (APP-H1)
    // -----------------------------------------------------------------------
    //
    // A project is open exactly when the native scene holds a body. Home is
    // what the shell shows when none is; it is derived from this answer on
    // every refresh and is never remembered in Java, so a rotation, a
    // recreation and a resume all land where native truth says.

    /** Whether a project is open. False on a cold launch, at Home and during
     *  the CAD bootstrap before its first commit. */
    static native boolean projectOpen();

    /**
     * Closes the project: the way to Home.
     *
     * <p>Every body is destroyed, the Construction history is dropped, any
     * sketch or support selection is cancelled with its borrowed view given
     * back, and the session leaves Sculpt. <b>Writes nothing.</b> Whether the
     * work was saved first is the shell's question, answered before this is
     * called; the manual slot and the recovery checkpoint are not touched by
     * this.
     */
    static native void closeProject();

    /**
     * DEBUG introspection: how many times native code was asked for the active
     * body while no project was open. Zero in every product flow; the device
     * suite asserts it across Home, the bootstrap and the first commit.
     */
    static native long debugActiveBodyMisuseCount();

    // -----------------------------------------------------------------------
    // Renderer lifecycle
    // -----------------------------------------------------------------------
    //
    // GPU resources are not project truth. The scene, every published mesh and
    // every Frozen Sculpt Mesh live in CPU domain code that a lost device
    // cannot reach, so losing the device costs the GPU's copy of derived data
    // and nothing else. These constants exist so the product can tell the user
    // the truth about the viewport while saying, accurately, that the work is
    // safe.

    /** Presenting, or ready to. */
    static final int RENDERER_HEALTHY = 0;

    /** The device was lost; a rebuild is under way. The project is intact. */
    static final int RENDERER_RECOVERING = 1;

    /**
     * The renderer has stopped and will not start again in this process.
     *
     * <p>The project is still intact and has been checkpointed. What the user
     * needs is a restart, and saying so is better than presenting a black
     * viewport forever or drawing through a corrupt device.
     */
    static final int RENDERER_RESTART_REQUIRED = 2;

    /** One of the {@code RENDERER_*} codes. Never blocks on the render thread. */
    static native int rendererLifecycle();

    /**
     * DEBUG-ONLY: makes the next frame behave exactly as though the GPU device
     * had been lost. Returns false in a release build, where it does nothing.
     *
     * <p>Not a product path and unreachable from any UI. Provoking a real
     * {@code VK_ERROR_DEVICE_LOST} would mean destabilising the GPU of the
     * authoritative emulator, which the repository forbids, and would make the
     * test depend on driver behaviour rather than on ForgeShape's. Everything
     * after the injection point is the real recovery path.
     */
    static native boolean debugInjectDeviceLoss();

    /**
     * How many GPU device rebuilds have completed in this process.
     *
     * <p>Introspection, and the difference between a real claim and a weak one:
     * "the renderer is healthy" is also what a renderer that never noticed the
     * loss would report, while a rebuild count that went up says the device
     * really was torn down and built again.
     */
    static native int debugRendererDeviceRebuilds();

    /**
     * Opens the production session-initialization boundary.
     *
     * <p>Seeding a session is not something the user did. Answering the start
     * question with Sculpt drives the real Construction entry points to get
     * there — deliberately, so nothing about Freeze is duplicated — and that
     * shapes the body into a sphere, which is a genuine Construction change.
     * Recorded, it would be the user's first Undo, and undoing it would rewind
     * a decision they never made.
     *
     * <p>Between this and {@link #endSessionInitialization()} the mutations
     * still run and the model on screen still follows them; only the RECORDING
     * is suppressed. The end establishes the postcondition: no undo step, no
     * redo step.
     *
     * <p>Reachable from exactly one place — the start answer — and from nowhere
     * else. Back to Construction, a rotation and a resume are outside it, and
     * none of them may start using it to tidy a history away.
     */
    static native void beginSessionInitialization();

    /** Closes the boundary; afterwards the Construction history is empty. */
    static native void endSessionInitialization();

    /**
     * DEBUG-ONLY: forgets the Construction history without moving the scene.
     *
     * <p>Not a product act and reachable from no UI. It is the observation seam
     * the instrumented suite uses to reach "a session that has done nothing",
     * which is otherwise unreachable in a process that has already run other
     * cases. A no-op in a release build.
     */
    static native void debugResetConstructionHistory();

    /** @return how many steps the undo stack holds. Diagnostic and test use. */
    static native int constructionUndoDepth();

    /** @return how many steps the redo stack holds. Diagnostic and test use. */
    static native int constructionRedoDepth();

    /** @return one of the {@code HISTORY_*} constants */
    static native int constructionUndo();

    /** @return one of the {@code HISTORY_*} constants */
    static native int constructionRedo();

    // -----------------------------------------------------------------------
    // Sculpt history (ARCH-OWNER-12)
    // -----------------------------------------------------------------------
    //
    // The ACTIVE BODY's own Undo/Redo over completed sculpt strokes. A separate
    // history from the Construction one, with separate stacks, separate bounds
    // and a separate lifetime: it is runtime-only, it is never written to a
    // {@code .forge} document or a recovery checkpoint, and reopening a project
    // starts an empty one over the geometry the file restored.
    //
    // As with the Construction history, this layer holds none of it.

    /** @return whether a completed sculpt stroke can be undone right now */
    static native boolean sculptUndoAvailable();

    /** @return whether an undone sculpt stroke can be redone right now */
    static native boolean sculptRedoAvailable();

    /** @return how many strokes the active body's undo stack holds */
    static native int sculptUndoDepth();

    /** @return how many strokes the active body's redo stack holds */
    static native int sculptRedoDepth();

    /**
     * @return what the active body's retained sculpt history costs, in bytes,
     *     by the same conservative measure the cap is enforced with.
     *     Diagnostic: nothing here derives anything from it.
     */
    static native long sculptHistoryBytes();

    /**
     * @return how many strokes on the active body were too large to retain.
     *     A stroke counted here applied normally but cannot be taken back.
     */
    static native long sculptHistoryNotRetainedCount();

    /**
     * @return how many entries the active body's history has evicted to stay
     *     inside its step and byte caps.
     */
    static native long sculptHistoryEvictedCount();

    /** @return one of the {@code HISTORY_*} constants */
    static native int sculptUndo();

    /** @return one of the {@code HISTORY_*} constants */
    static native int sculptRedo();

    // -----------------------------------------------------------------------
    // What the two chrome controls mean
    // -----------------------------------------------------------------------
    //
    // These four dispatch on the product mode, in native code: in Sculpt they
    // are the active body's stroke history, and everywhere else they are the
    // Construction history with behaviour identical to what it was before
    // Sculpt Undo existed. The workspace calls only these — it never picks a
    // history itself, because it does not own the mode that decides.

    /** @return whether the control pair's Undo can act right now */
    static native boolean historyUndoAvailable();

    /** @return whether the control pair's Redo can act right now */
    static native boolean historyRedoAvailable();

    /** @return one of the {@code HISTORY_*} constants */
    static native int historyUndo();

    /** @return one of the {@code HISTORY_*} constants */
    static native int historyRedo();

    /**
     * Opens one Construction edit, so that every mutation until the matching
     * commit becomes a single history step.
     *
     * <p>For a user act made of more than one native mutation, such as
     * choosing a shape from Add Primitive (add, then apply). Between begin and
     * commit the ordinary entry points still publish, so the model follows the
     * edit live; they simply stop being steps of their own. A gizmo drag opens
     * its own edit below JNI.
     *
     * @return true when this call opened the edit, false when one was already
     *         open (in which case the caller must NOT commit it)
     */
    static native boolean beginConstructionEdit();

    /**
     * Closes an open edit, recording at most one step.
     *
     * @return true when a step was actually recorded — which happens only when
     *         the Construction state genuinely changed
     */
    static native boolean commitConstructionEdit();

    /**
     * Closes an open edit by restoring the state captured when it opened, and
     * records nothing.
     */
    static native void cancelConstructionEdit();

    /** @return {@link #MODE_CONSTRUCTION} or {@link #MODE_SCULPT} */
    static native int productMode();

    /**
     * Copies the Construction object's current local mesh into a new Frozen
     * Sculpt Mesh and enters Sculpt mode.
     *
     * <p>The Construction Source is only read: its primitive, its parameters and
     * its transform are untouched, and it stays available in full. Freezing
     * again replaces the sculpt mesh, discarding the sculpted vertices — which
     * is why it is only ever done by an explicit user act.
     *
     * @return one of the {@code SCULPT_*} status constants
     */
    static native int freezeToSculpt();

    /**
     * Returns to Construction mode. The original, unsculpted object comes back
     * on screen; the Frozen Sculpt Mesh is kept exactly as it was, and nothing
     * is copied back into the Construction Source.
     *
     * @return one of the {@code SCULPT_*} status constants
     */
    static native int enterConstructionMode();

    /**
     * Returns to Sculpt mode without re-freezing, so prior deformation comes
     * back. Refused with {@link #SCULPT_NOTHING_FROZEN} when nothing has ever
     * been frozen.
     *
     * @return one of the {@code SCULPT_*} status constants
     */
    static native int enterSculptMode();

    /**
     * Reads the authoritative sculpt state.
     *
     * @param outState caller-allocated array of at least
     *                 {@link #SCULPT_STATE_SIZE} doubles, indexed by the
     *                 {@code SCULPT_*} slot constants above
     */
    static native void sculptState(double[] outState);

    /**
     * Sets the brush, for every tool at once. Both values are clamped into their
     * documented native ranges rather than refused, so a slider cannot produce a
     * brush that does nothing or a brush without bound.
     *
     * <p>There is deliberately no per-tool radius or strength: switching tools
     * never changes how big or how strong the brush is.
     *
     * @param radiusPixels brush radius in screen pixels, resolved to object
     *                     space at the depth of each stroke's hit point
     * @param strength     how much each tool does — for Grab, how much of the
     *                     finger's travel the grabbed centre follows
     */
    static native void setSculptBrush(double radiusPixels, double strength);

    /**
     * Requests an active tool and returns the tool that is actually active.
     *
     * <p>An unknown index is refused and the current tool stands. This changes
     * no geometry, publishes no revision and uploads nothing, and a stroke
     * already in progress keeps the tool it captured when the finger landed.
     *
     * @param tool one of the {@code TOOL_*} constants
     * @return the {@code TOOL_*} constant that is active after the call
     */
    static native int setSculptTool(int tool);

    /** @return the active {@code TOOL_*} constant */
    static native int sculptTool();

    // -----------------------------------------------------------------------
    // Viewport display settings
    // -----------------------------------------------------------------------
    //
    // PRESENTATION ONLY. None of the four calls below publishes a mesh, mints a
    // MeshRevision or a SculptRevision, moves a vertex, changes a Construction
    // parameter or affects picking. They decide how the object is DRAWN and
    // nothing about what it is.
    //
    // Native owns the values, exactly as it owns the product mode and the active
    // tool, so they survive HOME/resume for free: the Android layer reads them
    // back on resume rather than saving and restoring them.

    /** Neutral clay studio lighting: the modelling default. */
    static final int SHADING_STUDIO = 0;

    /** The ForgeShape-owned MatCap: the high-readability form/sculpt view. */
    static final int SHADING_MATCAP = 1;

    /**
     * DEBUG-ONLY: the source mesh's own per-vertex colours, which is what the
     * viewport looked like before shading existed. Reachable only from a
     * debuggable build, and never the default.
     */
    static final int SHADING_DEBUG_SOURCE_COLOR = 2;

    /** Normals averaged across crease-bounded face groups. */
    static final int SURFACE_SMOOTH = 0;

    /** One flat normal per triangle; intentionally exposes triangle structure. */
    static final int SURFACE_FACETED = 1;

    /**
     * Requests a shading model.
     *
     * <p>An unknown index is refused and the current model stands. This is a
     * fragment-stage uniform: it rebuilds no geometry and re-uploads nothing.
     *
     * @param model one of the {@code SHADING_*} constants
     * @return the {@code SHADING_*} constant that is active after the call
     */
    static native int setShadingModel(int model);

    /** @return the active {@code SHADING_*} constant */
    static native int shadingModel();

    /**
     * Requests smooth or faceted surface shading.
     *
     * <p>An unknown index is refused and the current choice stands. This is the
     * one display setting that causes any recomputation, and it is confined to
     * the render-only derived mesh: the authoritative RuntimeMesh, its revision
     * and picking are all untouched.
     *
     * @param shading one of the {@code SURFACE_*} constants
     * @return the {@code SURFACE_*} constant that is active after the call
     */
    static native int setSurfaceShading(int shading);

    /** @return the active {@code SURFACE_*} constant */
    static native int surfaceShading();

    /** The warm dark studio ground: the product default. */
    static final int VIEWPORT_BACKGROUND_WARM_GRAPHITE = 0;

    /** The cooler steel-grey ground. */
    static final int VIEWPORT_BACKGROUND_NEUTRAL_CHARCOAL = 1;

    /** The lightest ground in the set, and still a dark one. */
    static final int VIEWPORT_BACKGROUND_LIGHT_CHARCOAL = 2;

    /**
     * Requests what the viewport is CLEARED to, behind everything.
     *
     * <p>This is the one value the Android theme system hands to native code,
     * and it crosses as a closed viewport <b>appearance</b> — never a theme,
     * never an Android type, never an RGB authored up here. Native code owns
     * what each appearance looks like, so the geometry domain does not learn
     * that themes exist.
     *
     * <p>The cheapest presentation change in the product: the clear value is
     * written into the render pass every frame anyway, so this publishes no
     * mesh, mints no {@code MeshRevision} or {@code SculptRevision}, moves no
     * vertex, rebuilds no derived geometry, re-uploads nothing and cannot be
     * observed by picking. An unknown index is refused and the current
     * appearance stands.
     *
     * @param background one of the {@code VIEWPORT_BACKGROUND_*} constants
     * @return the {@code VIEWPORT_BACKGROUND_*} constant in effect afterwards
     */
    static native int setViewportBackground(int background);

    /** @return the active {@code VIEWPORT_BACKGROUND_*} constant */
    static native int viewportBackground();

    /**
     * Shows or hides the world reference grid: the 1 m floor on the world XZ
     * plane the viewport draws behind the model.
     *
     * <p>It is a viewport <b>reference</b>, not geometry, and the distinction is
     * absolute. The grid has no {@code ObjectId}, never enters the scene or a
     * scene snapshot, is not a {@code RuntimeMesh}, is invisible to picking,
     * takes no part in Freeze or in a sculpt stroke, is not exported and is not
     * a snap target. A future Sketch grid — the one that snaps, drawn on a
     * sketch plane — is a different feature with its own approval and is not
     * this.
     *
     * <p>Structurally the cheapest change the renderer has: the grid's vertices
     * are generated and uploaded exactly once with the Vulkan device and never
     * again, so this decides only whether one already-built draw call is
     * recorded. No body's render mesh is rebuilt, nothing is re-uploaded and no
     * revision is minted.
     *
     * <p>Native-owned and process-scoped like the rest of the display settings,
     * which is why the choice survives rotation, an Activity recreation and a
     * HOME/resume with no save/restore code up here. A process kill returns it
     * to the default, which is <b>on</b>.
     *
     * @return whether the grid is drawn afterwards
     */
    static native boolean setGridVisible(boolean visible);

    /** @return whether the world reference grid is currently drawn */
    static native boolean gridVisible();

    /**
     * Tells the viewport whether the user has asked the system for reduced
     * motion.
     *
     * <p>This is the whole of the accessibility seam, and it carries one bool
     * on purpose. Reading {@code Settings.Global.ANIMATOR_DURATION_SCALE} and
     * deciding what it means is Android's job and stays here; native code is
     * handed the answer, exactly as it is handed a viewport appearance rather
     * than an {@link AppTheme}. No Android type crosses.
     *
     * <p>Presentation only, and only timing at that: it publishes no mesh,
     * mints no revision, moves no vertex and re-uploads nothing. The single
     * observable difference is whether a newly selected body reaches its
     * resting tint over a fifth of a second or in one frame.
     */
    static native void setReducedMotion(boolean reduced);

    /** @return whether the viewport was last told to reduce motion */
    static native boolean reducedMotion();

    /**
     * Standard pinhole projection: nearer parts of a solid are drawn larger and
     * parallel edges converge. The product default.
     */
    static final int PROJECTION_PERSPECTIVE = 0;

    /**
     * True parallel projection: equal lengths parallel to the screen are drawn
     * at equal size at every depth, so exact Construction geometry can be judged
     * without anything being enlarged merely for being closer.
     */
    static final int PROJECTION_ORTHOGRAPHIC = 1;

    /**
     * Requests a camera projection.
     *
     * <p>An unknown index is refused and the current projection stands. This is
     * camera/presentation state, not geometry: it publishes no mesh, mints no
     * {@code MeshRevision} or {@code SculptRevision}, moves no vertex, changes
     * no Construction parameter or transform, and re-uploads nothing. The
     * framing at the target plane is preserved across the switch, so the frame
     * does not jump.
     *
     * <p>Native owns the value, exactly as it owns the camera pose, so the
     * chosen projection and its framing survive HOME/resume and a Surface
     * recreation for free.
     *
     * @param mode one of the {@code PROJECTION_*} constants
     * @return the {@code PROJECTION_*} constant that is active after the call
     */
    static native int setProjectionMode(int mode);

    /** @return the active {@code PROJECTION_*} constant */
    static native int projectionMode();

    /**
     * DEBUG-ONLY test hook: publishes a native debug mesh fixture, or drives the
     * native Construction box's authoritative dimensions.
     *
     * <p>This is test infrastructure, not product functionality. There is no UI
     * for it and no exported component; native code compiles it to a no-op in a
     * release build, where it always returns {@code false}. It only mutates CPU
     * state and publishes CPU mesh revisions — the native render thread still
     * owns every Vulkan operation.
     *
     * @param command 1 = baseline fixture, 2 = same-topology fixture,
     *                3 = larger replacement fixture, 4 = repeated-update stress
     *                run, 5 = dump mesh/GPU/Construction diagnostics,
     *                6/7/8 = Construction box dimension state A/B/C,
     *                9 = invalid dimension update (must be rejected)
     * @return whether the command was accepted
     */
    static native boolean debugMeshCommand(int command);

    // -----------------------------------------------------------------------
    // The sketch session and the CAD Body (CAD-R0-A1A2)
    // -----------------------------------------------------------------------
    //
    // The Java layer holds NO sketch. It asks the one native session what state
    // it is in, which tool is held, what is selected and what it would build,
    // and every answer comes from here. Pointer samples reach the session
    // through the ordinary touchEvent path; these are the chrome's acts.
    //
    // Every status is a CAD_* code — the native CadStatus enum's own order —
    // and cadStatusToken turns one back into its name for a log line.

    /** The one status vocabulary of the CAD domain. */
    static final int CAD_OK = 0;
    static final int CAD_NON_FINITE = 1;
    static final int CAD_OUT_OF_RANGE = 2;
    static final int CAD_ZERO_LENGTH_LINE = 3;
    static final int CAD_DUPLICATE_EDGE = 4;
    static final int CAD_TOO_FEW_VERTICES = 5;
    static final int CAD_ZERO_SIZE_RECTANGLE = 6;
    static final int CAD_INVALID_CIRCLE_RADIUS = 7;
    static final int CAD_TOO_MANY_ENTITIES = 8;
    static final int CAD_UNKNOWN_ENTITY = 9;
    static final int CAD_INVALID_WORKPLANE = 10;
    static final int CAD_OPEN_PROFILE = 11;
    static final int CAD_SELF_INTERSECTING_PROFILE = 12;
    static final int CAD_ZERO_AREA_PROFILE = 13;
    static final int CAD_BRANCHING_CHAIN = 14;
    static final int CAD_NO_CLOSED_PROFILE = 15;
    static final int CAD_AMBIGUOUS_PROFILE = 16;
    static final int CAD_PROFILE_NOT_FOUND = 17;
    static final int CAD_NESTED_PROFILE_UNSUPPORTED = 18;
    static final int CAD_INVALID_EXTRUDE_DEPTH = 19;
    static final int CAD_INVALID_EXTRUDE_DIRECTION = 20;
    static final int CAD_TRIANGULATION_FAILED = 21;
    static final int CAD_REGENERATION_FAILED = 22;
    static final int CAD_NOT_SKETCHING = 23;
    static final int CAD_NOT_CAD_BODY = 24;
    static final int CAD_REFUSED_EDIT_IN_PROGRESS = 25;
    static final int CAD_INVALID_ARC = 26;
    static final int CAD_INVALID_SPLINE = 27;
    static final int CAD_SKETCH_NOT_EMPTY = 28;
    static final int CAD_DEPENDENT_FACE_LOST = 29;

    /** The three principal workplanes, as the native Workplane index. */
    static final int WORKPLANE_XY = 0;
    static final int WORKPLANE_XZ = 1;
    static final int WORKPLANE_YZ = 2;

    /** The sketch session's state. */
    static final int SKETCH_INACTIVE = 0;
    static final int SKETCH_EDITING = 1;
    static final int SKETCH_READY = 2;

    /** The seven sketch tools, as the native SketchTool index. */
    static final int SKETCH_TOOL_SELECT = 0;
    static final int SKETCH_TOOL_LINE = 1;
    static final int SKETCH_TOOL_POLYLINE = 2;
    static final int SKETCH_TOOL_RECTANGLE = 3;
    static final int SKETCH_TOOL_CIRCLE = 4;
    static final int SKETCH_TOOL_ARC = 5;
    static final int SKETCH_TOOL_SPLINE = 6;

    /** Which way an extrusion grows, as the native ExtrudeDirection index. */
    static final int EXTRUDE_ALONG_NORMAL = 0;
    static final int EXTRUDE_AGAINST_NORMAL = 1;

    /** Slots of {@link #sketchState}. */
    static final int SKETCH_STATE_SIZE = 13;
    static final int SKETCH_STATE = 0;
    static final int SKETCH_PLANE = 1;
    static final int SKETCH_TOOL = 2;
    static final int SKETCH_ENTITY_COUNT = 3;
    static final int SKETCH_SELECTED_ENTITY = 4;
    static final int SKETCH_PROFILE_COUNT = 5;
    static final int SKETCH_CHOSEN_PROFILE = 6;
    static final int SKETCH_EXTRUDE_DEPTH = 7;
    static final int SKETCH_EXTRUDE_DIRECTION = 8;
    static final int SKETCH_LAST_STATUS = 9;
    static final int SKETCH_POLYLINE_IN_PROGRESS = 10;
    static final int SKETCH_ENTITIES_PLACED = 11;
    static final int SKETCH_LAST_SNAP = 12;

    /** Slots of {@link #sketchSelectedEntity}. */
    static final int SKETCH_ENTITY_SIZE = 6;
    static final int SKETCH_ENTITY_ID = 0;
    static final int SKETCH_ENTITY_KIND = 1;
    /** The first of the entity's own values; see the native comment. */
    static final int SKETCH_ENTITY_VALUES = 2;
    static final int SKETCH_ENTITY_KIND_LINE = 0;
    static final int SKETCH_ENTITY_KIND_POLYLINE = 1;
    static final int SKETCH_ENTITY_KIND_RECTANGLE = 2;
    static final int SKETCH_ENTITY_KIND_CIRCLE = 3;
    static final int SKETCH_ENTITY_KIND_ARC = 4;
    static final int SKETCH_ENTITY_KIND_SPLINE = 5;

    /** Slots of {@link #sketchViewState} — the orientation navigator's state. */
    static final int SKETCH_VIEW_SIZE = 6;
    static final int SKETCH_VIEW_ACTIVE = 0;
    static final int SKETCH_VIEW_PLANE = 1;
    static final int SKETCH_VIEW_FLIPPED = 2;
    static final int SKETCH_VIEW_QUARTER_TURNS = 3;
    static final int SKETCH_VIEW_FACE_SUPPORTED = 4;
    static final int SKETCH_VIEW_PLANE_SWITCHABLE = 5;

    /** Slots of {@link #sketchLineDimension}. */
    static final int SKETCH_DIMENSION_SIZE = 4;
    static final int SKETCH_DIMENSION_ENTITY = 0;
    static final int SKETCH_DIMENSION_LENGTH = 1;
    static final int SKETCH_DIMENSION_ANCHOR_U = 2;
    static final int SKETCH_DIMENSION_ANCHOR_V = 3;

    /** Slots of {@link #sketchProfileInfo}. */
    static final int SKETCH_PROFILE_INFO_SIZE = 3;
    static final int SKETCH_PROFILE_KIND_RECTANGLE = 1;
    static final int SKETCH_PROFILE_KIND_CIRCLE = 2;
    static final int SKETCH_PROFILE_KIND_POLYGON = 3;

    // --- spatial support chooser (CAD-A3) --------------------------------

    /** A support-chooser pick that hit nothing. */
    static final int SUPPORT_KIND_NONE = -1;
    /** A support-chooser pick that hit a planar CAD face. */
    static final int SUPPORT_KIND_FACE = 3;

    /**
     * Enters spatial "Choose Sketch Support": the three world planes are drawn
     * as touchable targets, and (when {@code allowFaces}) the planar faces of
     * CAD bodies are eligible too. Returns false if refused (sculpting or
     * mid-edit).
     */
    static native boolean supportChooserBegin(boolean allowFaces);

    /** Stylus hover: highlights the target under the point, never selects. */
    static native int supportChooserHover(float x, float y);

    /** A tap: selects the target under the point. Returns a SUPPORT_KIND_*. */
    static native int supportChooserSelect(float x, float y);

    /** The current selection's kind, or SUPPORT_KIND_NONE. */
    static native int supportChooserSelectedKind();

    /** Begins the sketch on the selected support, framing the camera on it. */
    static native int supportChooserConfirm();

    /** Leaves support selection without starting a sketch. */
    static native void supportChooserCancel();

    /** Whether spatial support selection is active. */
    static native boolean supportChooserActive();

    /**
     * Projects a world point to a screen pixel through the current camera, into
     * {@code out[0..1]}. Read-only verification seam so a test can tap the exact
     * pixel a plane or face target projects to. Returns false if off screen.
     */
    static native boolean debugProjectWorld(double x, double y, double z, float[] out);

    /** Begins a sketch on a workplane. Returns a CAD_* code. */
    static native int sketchBegin(int workplane);

    /** Drops the sketch. Never a project mutation. */
    static native void sketchCancel();

    static native boolean sketchSetTool(int tool);

    static native int sketchTool();

    static native void sketchState(double[] out);

    /** Editing to Ready, or a named refusal that leaves the sketch editable. */
    static native int sketchFinish();

    static native void sketchBackToEditing();

    static native int sketchSelectProfile(long anchorEntityId);

    static native int sketchSetExtrude(double depthMeters, int direction);

    /**
     * THE commit: the sketch becomes one new CAD Body as one history step.
     *
     * @return the new body's ObjectId, or {@link #NO_OBJECT} with the reason in
     *         {@link #sketchLastStatus()}
     */
    static native long sketchCommit();

    static native int sketchLastStatus();

    static native int sketchDeleteSelected();

    /** Selects an entity by id, or clears the selection for 0. */
    static native boolean sketchSelectEntity(long entityId);

    static native boolean sketchSelectedEntity(double[] out);

    static native int sketchApplyRectangle(long entityId, double widthMeters,
                                           double heightMeters);

    static native int sketchApplyCircle(long entityId, double radiusMeters);

    static native int sketchApplyLine(long entityId, double x0, double y0, double x1,
                                      double y1);

    /** The closed profiles' anchor ids, in the domain's order. */
    static native int sketchProfiles(long[] out);

    static native boolean sketchProfileInfo(long anchorEntityId, double[] out);

    /**
     * Where a sketch point is on screen, in view-local pixels. Verification
     * infrastructure on the gizmo's terms: the one honest source of a pixel
     * to send a synthetic touch to.
     */
    static native boolean sketchScreenPoint(double u, double v, float[] out);

    /** The current adaptive sketch grid step in metres (`CAD-A3`): what a grid
     *  snap rounds to at the zoom the last drag started under. */
    static native double sketchGridStep();

    // --- the orientation navigator (`SKETCH-UX-R1` C) --------------------
    //
    // The navigator holds NO state: it reads what the session says on every
    // refresh and sends acts back. Nothing here is persisted, and none of the
    // three acts can move an authored coordinate.

    /** The navigator's state, in the SKETCH_VIEW_* slots. */
    static native void sketchViewState(double[] out);

    /**
     * Chooses the world support plane. Refused by name — {@code
     * CAD_SKETCH_NOT_EMPTY} once the sketch carries geometry, {@code
     * CAD_INVALID_WORKPLANE} for a face-supported sketch — and a refusal
     * changes nothing, the camera included.
     */
    static native int sketchSetSupportPlane(int workplane);

    /** Looks at the plane's positive or negative normal. Presentation only. */
    static native int sketchSetViewFlipped(boolean flipped);

    /** Rolls the view a quarter turn about the sketch normal (+1 or -1). */
    static native int sketchRotateView(int quarterTurns);

    // --- the selected line's dimension (`SKETCH-UX-R1` E) ----------------

    /**
     * The selected straight Line's dimension, in the SKETCH_DIMENSION_* slots.
     * False, writing nothing, when the selection is not a straight Line — which
     * is exactly the condition for the annotation not being drawn.
     */
    static native boolean sketchLineDimension(double[] out);

    /**
     * Sets a straight Line's length EXACTLY: the first endpoint stays put, the
     * direction is unchanged, and the second endpoint moves along it. No
     * solver, no neighbour moved, nothing re-snapped.
     */
    static native int sketchApplyLineLength(long entityId, double lengthMeters);

    // --- Edit Sketch (`SKETCH-UX-R1` F) ----------------------------------

    /**
     * Opens a STAGED edit of a committed CAD body's sketch. The project keeps
     * its own truth until {@link #sketchCommitEdit}; {@link #sketchCancel}
     * costs it nothing.
     */
    static native int sketchBeginEdit(long bodyId);

    /** Which body the open session edits, or 0 when it authors a new one. */
    static native long sketchEditingBodyId();

    /** Finishes a staged sketch edit: one transaction, one Undo. */
    static native int sketchCommitEdit();

    static native String cadStatusToken(int code);

    /** 1 Construction, 2 Imported, 3 CAD; 0 for an unknown id. */
    static native int sceneBodyRepresentation(long objectId);

    static final int REPRESENTATION_CONSTRUCTION = 1;
    static final int REPRESENTATION_IMPORTED = 2;
    static final int REPRESENTATION_CAD = 3;

    static native boolean sceneActiveBodyIsCad();

    /** Whether the active body is a face-supported CAD body (`CAD-A3`). */
    static native boolean sceneActiveBodyIsFaceSupportedCad();

    /** Slots of {@link #cadState}. */
    static final int CAD_STATE_SIZE = 8;
    static final int CAD_PLANE = 0;
    static final int CAD_DEPTH = 1;
    static final int CAD_DIRECTION = 2;
    static final int CAD_PROFILE_KIND = 3;
    /** A rectangle's width, or a circle's radius. */
    static final int CAD_PRIMARY_SIZE = 4;
    /** A rectangle's height. */
    static final int CAD_SECONDARY_SIZE = 5;
    static final int CAD_ENTITY_COUNT = 6;
    static final int CAD_PROFILE_VERTICES = 7;
    static final int CAD_PROFILE_NONE = 0;
    static final int CAD_PROFILE_RECTANGLE = 1;
    static final int CAD_PROFILE_CIRCLE = 2;
    static final int CAD_PROFILE_POLYGON = 3;

    /** The active CAD Body's authored values; false when it is not one. */
    static native boolean cadState(double[] out);

    /** One Apply, one history step, one regeneration. Returns an APPLY_* code. */
    static native int cadApplyExtrude(double depthMeters, int direction);

    static native int cadApplyRectangle(double widthMeters, double heightMeters,
                                        double depthMeters, int direction);

    static native int cadApplyCircle(double radiusMeters, double depthMeters, int direction);

    /** The exact CAD reason behind the last CAD Apply, as a CAD_* code. */
    static native int cadLastStatus();
}
