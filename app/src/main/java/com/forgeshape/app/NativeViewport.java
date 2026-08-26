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
    // DEGREES, and both live in native code. A transform edit moves and rotates
    // the box without touching its mesh: no revision is published and nothing
    // is uploaded.
    // ---------------------------------------------------------------------

    /**
     * Reads the authoritative Construction transform.
     *
     * @param outPositionRotation caller-allocated array of at least 6 doubles,
     *                            filled with position X/Y/Z in meters followed by
     *                            rotation X/Y/Z in degrees
     */
    static native void boxTransform(double[] outPositionRotation);

    /**
     * Submits all six Construction transform values as one atomic request.
     *
     * <p>Native code validates all six before writing any of them. Zero and
     * negative are valid for every one of them: a coordinate is a place, not a
     * size. The result reuses the {@code APPLY_*} constants;
     * {@link #APPLY_REJECTED_NOT_POSITIVE} cannot occur here.
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyBoxTransform(double positionXMeters, double positionYMeters,
                                        double positionZMeters, double rotationXDegrees,
                                        double rotationYDegrees, double rotationZDegrees);

    // ---------------------------------------------------------------------
    // Product mode and the Frozen Sculpt Mesh.
    //
    // The object has two representations: the Construction Source (its exact
    // primitive, that primitive's parameters and its placement) and the Frozen
    // Sculpt Mesh (a copy of the Construction local mesh taken at the moment of
    // Freeze, with its own SculptRevision). Native code owns which one is
    // active. Java may request a mode; it holds none, holds no vertex and holds
    // no brush setting.
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
    // Construction history
    // -----------------------------------------------------------------------
    //
    // This layer holds NO history. It asks whether an undo is available, asks
    // for one, and re-reads native state afterwards. There is deliberately no
    // Java-side depth counter and no list of parameter snapshots: a second copy
    // of the history would be a second answer to "what does undo do next", and
    // the enabled state of a control would eventually disagree with the model.
    //
    // Both acts are refused below JNI while sculpting. The workspace withdraws
    // the controls there as well, because Construction Undo is not Sculpt Undo
    // and a control that could be read as one would be a lie; the native guard
    // stays regardless.

    /** The step was performed. */
    static final int HISTORY_OK = 0;
    /** There was nothing to undo or redo; nothing changed. */
    static final int HISTORY_NOTHING_TO_DO = 1;
    /** Refused: Construction history is not touched while sculpting. */
    static final int HISTORY_REFUSED_IN_SCULPT = 2;

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
    // The Construction Move / Rotate gizmo
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

    /** Direct manipulation with the axis handles. */
    static final int GIZMO_MODE_MOVE = 0;
    /** Direct manipulation with the axis rings. */
    static final int GIZMO_MODE_ROTATE = 1;

    /** Size of the array {@link #gizmoState(double[])} fills. */
    static final int GIZMO_STATE_SIZE = 10;
    /** 1 when the workspace is offering direct transform at all. */
    static final int GIZMO_ACTIVE = 0;
    /** One of the {@code GIZMO_MODE_*} constants. */
    static final int GIZMO_MODE = 1;
    /** 1 while a drag holds a pointer. */
    static final int GIZMO_CAPTURING = 2;
    /** The captured pointer id, or -1. */
    static final int GIZMO_POINTER_ID = 3;
    /** The captured axis: 0 none, 1 X, 2 Y, 3 Z. */
    static final int GIZMO_AXIS = 4;
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

    /** No handle is under that pixel. */
    static final int GIZMO_AXIS_NONE = 0;
    static final int GIZMO_AXIS_X = 1;
    static final int GIZMO_AXIS_Y = 2;
    static final int GIZMO_AXIS_Z = 3;

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
     * Chooses Move or Rotate.
     *
     * <p>Presentation state: no mesh revision, no geometry publication, no
     * history step. Returns false for an unknown index and while a drag holds a
     * pointer — a mode must not change under a moving finger.
     */
    static native boolean setGizmoMode(int mode);

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
     * @return one of the {@code GIZMO_AXIS_*} constants
     */
    static native int gizmoHitTest(float x, float y);

    /**
     * Where an axis handle can be grabbed, in view-local pixels.
     *
     * <p>Derived from the same projection the hit test uses, so a synthetic
     * pointer sent here reaches the same handle a finger would. Returns false,
     * writing nothing, when that handle is not on screen.
     *
     * @param axis one of the {@code GIZMO_AXIS_*} constants. {@code
     *             GIZMO_AXIS_NONE} asks for the PIVOT rather than a handle —
     *             not something that can be grabbed, but the point a caller
     *             needs in order to know which way along the screen an axis
     *             actually runs
     * @param out  two floats: x, y
     */
    static native boolean gizmoHandlePoint(int axis, float[] out);

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

    /**
     * Opens one Construction edit, so that every mutation until the matching
     * commit becomes a single history step.
     *
     * <p>For a user act made of more than one native mutation — choosing a
     * shape from Add Primitive is the one that exists today, and a dragged
     * handle is the one that is coming. Between begin and commit the ordinary
     * entry points still publish, so the model follows the edit live; they
     * simply stop being steps of their own.
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
}
