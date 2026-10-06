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
    /**
     * Refused: the body is LOCKED (Stage 018A).
     *
     * <p>Its own code because a lock is not a statement about a value — every
     * number passed may be perfectly good — so a status line that blamed a
     * coordinate would be describing the wrong problem. The transform controls
     * are withdrawn over a locked body as well; this is the guard that stays
     * regardless, because removing a control is not removing a guard.
     */
    static final int APPLY_REJECTED_LOCKED = 8;

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

    /**
     * Refused: the axis has no thickness to resize (Stage 020M).
     *
     * <p>A plane's local Y extent is exactly zero, and no scale gives it a
     * size. Its own code because the number the user typed may be perfectly
     * good — what cannot be done is the resize, and no thickness is fabricated
     * to make it possible.
     */
    static final int APPLY_REJECTED_DEGENERATE_AXIS = 9;

    /**
     * Refused: this body cannot be measured or resized here (Stage 020M).
     *
     * <p>An Imported Mesh, a CAD Body, a hidden body, Sculpt, no project at
     * all, or an edit already in progress. The controls are absent in every one
     * of those states; this is the guard that stays regardless.
     */
    static final int APPLY_REJECTED_UNAVAILABLE = 10;

    // ---------------------------------------------------------------------
    // Body Dimensions and Relative Scale (Stage 020M, `UI-OWNER-33B`)
    //
    // Construction-only, and NOT new project truth. A body's overall dimension
    // on one axis is DERIVED — its own unscaled local extent times its stored
    // Absolute Scale — so it is read on demand and never held here. Editing one
    // writes the Scale (and, for a one-sided anchor, the Position) the transform
    // already had, which is why this stage changes no `.forge` byte.
    //
    // RELATIVE Scale is a temporary multiplier that opens at (1, 1, 1) EVERY
    // time. Nothing stores one — not this class, not the domain and not the
    // file — so there is deliberately no way to read one back, and reopening
    // the interaction starts from the identity because there is nothing else it
    // could start from.
    // ---------------------------------------------------------------------

    /** Slots in {@link #bodyDimensionsState(double[])}. */
    static final int BODY_DIM_SIZE = 14;
    /** 1 when the Dimensions and Relative Scale controls may be drawn at all. */
    static final int BODY_DIM_SUPPORTED = 0;
    /** 1 while Dimensions mode is open. */
    static final int BODY_DIM_MODE_ACTIVE = 1;
    /** 0, 1 or 2 for the axis being read, or {@link #BODY_DIM_AXIS_NONE}. */
    static final int BODY_DIM_ACTIVE_AXIS = 2;
    /** One of the {@code DIMENSION_ANCHOR_*} constants. */
    static final int BODY_DIM_ANCHOR = 3;
    /** The overall X dimension in METRES; Y and Z follow. */
    static final int BODY_DIM_X = 4;
    /** The unscaled local X extent in METRES; Y and Z follow. */
    static final int BODY_DIM_EXTENT_X = 7;
    /** The stored Absolute Scale X; Y and Z follow. */
    static final int BODY_DIM_SCALE_X = 10;
    /** 1 when the dimension values above are meaningful. */
    static final int BODY_DIM_MEASURABLE = 13;

    /** No axis is being read or edited. */
    static final int BODY_DIM_AXIS_NONE = -1;

    /** The body's negative side on that axis stays where it is in the world. */
    static final int DIMENSION_ANCHOR_NEGATIVE = 0;
    /** The Position does not move; only the Scale changes. */
    static final int DIMENSION_ANCHOR_CENTER = 1;
    /** The body's positive side on that axis stays where it is in the world. */
    static final int DIMENSION_ANCHOR_POSITIVE = 2;

    /**
     * The whole Dimensions state, in the {@code BODY_DIM_*} slots.
     *
     * <p>A read: it opens no edit, records nothing and publishes nothing.
     */
    static native void bodyDimensionsState(double[] out);

    /**
     * Opens or closes Dimensions mode, and returns what the mode IS afterwards.
     *
     * <p>Opening withdraws the transform gizmo below JNI as well as above it:
     * the leaders and the handles are two instruments for the same placement.
     * Opening over a body this stage cannot measure — locked, hidden, imported,
     * CAD, sculpted, or none at all — is refused and leaves the mode closed.
     */
    static native boolean setBodyDimensionsMode(boolean on);

    /** Which axis's value is being read, or {@link #BODY_DIM_AXIS_NONE}. */
    static native boolean setBodyDimensionAxis(int axis);

    /** Which side of the body a resize holds still. */
    static native boolean setBodyDimensionAnchor(int anchor);

    /**
     * Sets one axis to an exact overall dimension, in METRES, with the mode's
     * current anchor. One call is one Construction history transaction.
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyBodyDimension(int axis, double targetMeters);

    /**
     * Commits a Relative Scale multiplier into the stored Absolute Scale, as
     * one Construction history transaction.
     *
     * <p>The multiplier itself is not persisted anywhere: what survives is the
     * product, and the next interaction opens at (1, 1, 1).
     *
     * @return one of the {@code APPLY_*} constants
     */
    static native int applyBodyRelativeScale(double multiplierX, double multiplierY,
                                             double multiplierZ);

    /**
     * Where one axis's numeric label belongs, in view-local pixels.
     *
     * <p>Derived from the midpoint of that axis's real dimension line through
     * the same projection the viewport uses — never from a guessed offset off a
     * world bounding box. False when the mode is closed or the anchor does not
     * project, and a label with nowhere honest to stand is not drawn.
     */
    static native boolean bodyDimensionLabelPoint(int axis, float[] out);

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
    /**
     * The active body is hidden (Stage027 GUARD-2): Start and Resume Sculpt are
     * refused, changing nothing. The controls are withdrawn over a hidden body;
     * this is the guard behind them.
     */
    static final int SCULPT_REFUSED_HIDDEN_BODY = 4;

    // -----------------------------------------------------------------------
    // The seven sculpt tools.
    //
    // One brush kernel carries all seven: they share the stroke lifecycle, the
    // hit test, the affected set, the falloff and the Radius/Strength contract,
    // and differ only in what they write for the vertices they captured — six
    // write a position and Mask writes a weight. Native code owns which one is
    // active; Java may request one and is told which is actually active.
    //
    // The last three were APPENDED by `SCULPT-FCM-R1`, so the four before them
    // keep the indices they always had. The RAIL presents them in the product's
    // reading order, which is a separate decision — see
    // WorkspaceTrailingHostView.
    // -----------------------------------------------------------------------

    /** Drag the surface with the finger, in the camera plane. */
    static final int TOOL_GRAB = 0;
    /** Deposit material along the normals the surface had at stroke start. */
    static final int TOOL_CLAY = 1;
    /** Relax each vertex toward its 1-ring neighbour average. */
    static final int TOOL_SMOOTH = 2;
    /** Expand along the normals the surface has right now. */
    static final int TOOL_INFLATE = 3;
    /** Draw the surface toward one plane fitted to the brush footprint. */
    static final int TOOL_FLATTEN = 4;
    /** Cut a narrow groove: inward along the normal, pinched radially. */
    static final int TOOL_CREASE = 5;
    /** Paint the weight that holds the other six off a vertex. */
    static final int TOOL_MASK = 6;

    /** Length of the array {@link #sculptState} fills. */
    static final int SCULPT_STATE_SIZE = 13;

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
    /**
     * How many vertices carry a non-zero Sculpt Mask (`SCULPT-FCM-R1`).
     *
     * <p><b>Runtime state, never project truth.</b> It reaches no {@code .forge}
     * byte, moves no project fingerprint and comes back zero after a reopen.
     * This layer holds no mask of its own: what it uses this for is deciding
     * whether Clear Mask is drawn at all, and native code answers that through
     * {@link #sculptCanClearMask()} anyway.
     */
    static final int SCULPT_MASKED_VERTEX_COUNT = 12;

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
    // The object commands: Rename, Show/Hide, Lock/Unlock, Duplicate
    // (Stage 018A, UI-OWNER-40) and Mirror (MIRROR-01)
    // -----------------------------------------------------------------------
    //
    // Status codes in step with the native kObjCmd* constants. They are a JNI
    // transport detail; the domain's own vocabulary is BodyCommandStatus.
    //
    // Each is exactly one history transaction, and each refuses while sculpting
    // and while sketching on the same terms Delete and body switching already
    // do. A refusal changes nothing at all. The first four are
    // representation-neutral; Mirror deliberately is not, and reflects a
    // Construction primitive alone.

    /** The command ran, as exactly one history transaction. */
    static final int OBJCMD_OK = 0;
    /** No body in the scene carries that id. */
    static final int OBJCMD_UNKNOWN_BODY = 1;
    /** A Construction edit is open; its captured pre-state names this body. */
    static final int OBJCMD_REFUSED_EDIT_IN_PROGRESS = 2;
    /**
     * The requested name is empty, or empty once sanitized.
     *
     * <p>Refused rather than replaced by the ObjectId fallback label: the
     * fallback is what a body with NO name gets, and a user must not be able to
     * reach it by typing.
     */
    static final int OBJCMD_REFUSED_INVALID_NAME = 3;
    /**
     * Duplicate only: the body is a face-supported CAD Body.
     *
     * <p>Its world placement is derived from its producer's face frame, so a
     * copy would stand exactly where the original stands with no way to move it
     * apart. Refused by name rather than created; nothing is retargeted and no
     * dependency is rewritten.
     */
    static final int OBJCMD_REFUSED_FACE_SUPPORTED_CAD = 4;
    /** Duplicate only: the copy did not build or did not regenerate. */
    static final int OBJCMD_REFUSED_NOT_DUPLICABLE = 5;
    /** The product is in Sculpt mode, or a sketch is open. */
    static final int OBJCMD_REFUSED_IN_SCULPT = 6;
    /**
     * Mirror only: the body is not one Mirror can reflect.
     *
     * <p>An Imported Mesh, a CAD Body, or a body carrying a sculpt mesh.
     * {@code MIRROR-01} reflects a Construction primitive, and it carries the
     * reflection in the body's ORIENTATION rather than in a negative scale —
     * only a Construction primitive's own geometry is symmetric enough for that
     * to be exact.
     */
    static final int OBJCMD_REFUSED_NOT_MIRRORABLE = 7;
    /**
     * Mirror only: the reflected placement is not representable.
     *
     * <p>A non-finite source transform, or a value the transform itself would
     * reject. Refused by name rather than applied and then rejected one layer
     * down.
     */
    static final int OBJCMD_REFUSED_NOT_REPRESENTABLE = 8;

    // The three principal world mirror planes, as a transport index. A JNI
    // transport detail, never an enum ABI value and never anything the `.forge`
    // file stores — a mirrored body is an ordinary body wearing an ordinary
    // transform, so nothing persists which plane made it.
    /** Reflects world Z. */
    static final int MIRROR_PLANE_XY = 0;
    /** Reflects world Y. */
    static final int MIRROR_PLANE_XZ = 1;
    /** Reflects world X. */
    static final int MIRROR_PLANE_YZ = 2;

    /**
     * Renames one body, as exactly one history transaction.
     *
     * <p>The string is put through the domain's own name rule — the same one an
     * imported object's name goes through — so there is one bound, one
     * sanitizer and one storability predicate rather than a second policy for
     * Rename. Unicode survives exactly: the boundary reads UTF-16 units and
     * encodes real UTF-8, never JNI's modified UTF-8, so an emoji typed here
     * comes back byte for byte.
     *
     * <p>Publishes nothing and mints no revision: a name is truth about
     * identity, not about geometry.
     *
     * @return one of the {@code OBJCMD_*} constants
     */
    static native int sceneRenameBody(long objectId, String name);

    /** Whether this body is drawn and pickable. Unknown bodies answer true. */
    static native boolean sceneBodyVisible(long objectId);

    /**
     * Shows or hides one body, as exactly one history transaction.
     *
     * <p>Hidden means not drawn AND not pickable, which is one fact rather than
     * two: both read the same native scene snapshot, and a hidden body is
     * simply not in it. It is not deleted — the row stays, the body stays
     * selectable from it, and it stays saved in the project.
     *
     * @return one of the {@code OBJCMD_*} constants
     */
    static native int sceneSetBodyVisible(long objectId, boolean visible);

    /** Whether this body refuses to be moved. */
    static native boolean sceneBodyLocked(long objectId);

    /**
     * Whether the body the editors currently act on is locked.
     *
     * <p>Asked so the workspace can withdraw the transform controls. The guards
     * below JNI stay regardless: removing a control is not removing a guard.
     */
    static native boolean sceneActiveBodyIsLocked();

    /**
     * Locks or unlocks one body, as exactly one history transaction.
     *
     * <p>A locked body stays visible and stays pickable; what it refuses is
     * being moved. Rename, Show/Hide, Duplicate and Unlock all remain
     * available, and Delete is deliberately unchanged by lock.
     *
     * @return one of the {@code OBJCMD_*} constants
     */
    static native int sceneSetBodyLocked(long objectId, boolean locked);

    /**
     * Duplicates one body, as exactly one history transaction.
     *
     * <p>The copy gets a fresh ObjectId, the source's own representation truth,
     * its placement, its visibility, its lock, a deterministic copy name and —
     * when the source has one — a clone of its sculpt mesh. It does NOT get the
     * source's sculpt Undo stack, which describes strokes made on the original.
     * The copy becomes the active body.
     *
     * @return one of the {@code OBJCMD_*} constants
     */
    static native int sceneDuplicateBody(long objectId);

    /**
     * Whether this body is one Mirror could actually reflect right now.
     *
     * <p>Asked per row so the control is ABSENT for an Imported Mesh, a CAD
     * Body and a body carrying a sculpt mesh, rather than shown and then
     * refused. Answers false for an unknown body and while sculpting or
     * sketching. The guard below JNI stays regardless.
     */
    static native boolean sceneBodyCanMirror(long objectId);

    /**
     * Reflects one Construction Body across one principal world plane, as
     * exactly one history transaction, creating one new body.
     *
     * <p>The source is not touched: this is a discrete creation act and not a
     * live symmetry modifier, so nothing links the two bodies afterwards. The
     * reflection gets a fresh ObjectId, the source's own Construction
     * parameters, the mirrored placement, the source's visibility and lock, and
     * a deterministic {@code Mirror} name. It becomes the active body.
     *
     * <p>The reflection is carried by the body's rotation, never by a negative
     * scale — scale in this product is strictly positive, and a negative factor
     * would invert winding and every normal with it.
     *
     * @param planeIndex one of the {@code MIRROR_PLANE_*} constants
     * @return one of the {@code OBJCMD_*} constants
     */
    static native int sceneMirrorBody(long objectId, int planeIndex);

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
    /**
     * A History navigator jump named a state that is not on the retained
     * branch ({@code SCULPT-H1}).
     *
     * <p>Only {@link #sculptJumpToHistoryCursor} returns it, and it means the
     * branch moved between the read that drew the rows and the tap: a stroke
     * truncated the future, or an eviction dropped the oldest state. Nothing
     * was applied. The correct handling is to re-read the model and redraw,
     * never to retry with a different ordinal — native code deliberately does
     * not clamp, because landing the user on a state they did not tap is worse
     * than a refusal they can see.
     */
    static final int HISTORY_OUT_OF_RANGE = 6;
    /**
     * Clear Mask on a mask whose single history entry would exceed the
     * per-entry byte cap ({@code SCULPT-FCM-R1}).
     *
     * <p>Only {@link #sculptClearMask} returns it, and it means <b>nothing was
     * cleared</b>. It is deliberately not the brush's own "applied but not
     * retained" policy: a brush stroke's deformation is what the user is doing
     * and lands whatever the history can hold, while Clear Mask's whole value
     * is that it can be taken back.
     */
    static final int HISTORY_ENTRY_TOO_LARGE = 7;

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
     * The project refused an import because the product is in Sculpt
     * ({@code ImportCommitStatus::RefusedInSculpt}, appended as ordinal 5).
     *
     * <p>Native code decides it, from its own mode, under the same lock as the
     * commit: an import would make its first body active and record a
     * Construction step, and in Sculpt the active body is the sculpt target
     * and Construction history is refused. Nothing changes on this answer.
     */
    static final int IMPORT_REFUSED_IN_SCULPT = IMPORT_COMMIT_BASE + 5;

    /**
     * Reads GLB bytes and creates durable Imported Mesh bodies from them.
     *
     * <p>Atomic: every object is built and validated before any of them reaches
     * the scene, so a refusal creates no body, mints no ObjectId, records no
     * history step and does not move the project fingerprint. On success the
     * whole import is exactly one Undo, and the first object it created is
     * selected. Refused in Sculpt with {@link #IMPORT_REFUSED_IN_SCULPT}.
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
     * How many frames the renderer has presented in this process.
     *
     * <p>A process-lifetime count: a render thread restart continues it rather
     * than starting it again from zero, so a later read is never below an
     * earlier one.
     *
     * <p>Introspection for evidence captures: a screenshot taken after a state
     * change should show that state, and on a software rasteriser a frame can
     * take hundreds of milliseconds while the swapchain holds several more in
     * flight. Waiting for presented frames is a measurement where a fixed delay
     * is a guess. Carries a count and nothing else.
     */
    static native long debugRendererFramesPresented();

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

    /**
     * Which kind of selection the Ready sketch makes (`CAD-V6-S2`): 0 loop
     * regions, 1 planar faces, -1 when no sketch is Ready. In planar-faces mode
     * {@link #sketchProfiles} lists atomic faces by TRANSIENT row handle.
     */
    static native int sketchSelectionKind();

    /**
     * Which kind of selection feature {@code index} (0 = base) of a CAD body
     * stores: 0 loop regions, 1 planar faces, -1 when there is none.
     */
    static native int cadFeatureSelectionKind(long bodyId, int index);

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

    // -----------------------------------------------------------------------
    // The Sculpt Mask (SCULPT-FCM-R1)
    // -----------------------------------------------------------------------
    //
    // Runtime-local per-vertex state that holds the six geometry brushes off a
    // vertex. It lives on the body's Frozen Sculpt Mesh, survives Back and
    // Resume, reaches no {@code .forge} byte, moves no project fingerprint and
    // comes back empty after a reopen. This layer holds none of it.

    /**
     * Clears the active body's Sculpt Mask, as ONE Sculpt history entry.
     *
     * <p>One Undo puts the whole mask back. It mints no {@code SCULPT_REVISION}
     * and sets no {@code SCULPT_HAS_EDITS}, because a mask is not geometry, so
     * clearing one earns no autosave checkpoint.
     *
     * @return one of the {@code HISTORY_*} constants;
     *     {@link #HISTORY_NOTHING_TO_DO} when there is no mask,
     *     {@link #HISTORY_ENTRY_TOO_LARGE} when the entry would not fit — and
     *     in both of those, and every other refusal, nothing was cleared
     */
    static native int sculptClearMask();

    /**
     * @return whether Clear Mask could succeed right now — in Sculpt, with a
     *     frozen mesh, no live stroke, and a mask actually painted
     */
    static native boolean sculptCanClearMask();

    /** @return one of the {@code HISTORY_*} constants */
    static native int sculptUndo();

    /** @return one of the {@code HISTORY_*} constants */
    static native int sculptRedo();

    // -----------------------------------------------------------------------
    // The History navigator (SCULPT-H1)
    // -----------------------------------------------------------------------
    //
    // The compact list of the active body's retained sculpt states, and the one
    // act that moves between them.
    //
    // <b>It is a VIEW of the history that already existed.</b> There is no
    // second stack, no new capacity, no new budget and nothing new that is
    // stored: the retained branch was always a line and the cursor was always a
    // position on it, so the navigator only names them. This layer holds
    // neither — the model is re-read on every refresh, exactly as the two
    // control's enabled states are, because a cached copy would be the second
    // answer the one-history rule exists to prevent.
    //
    // A jump is repeated Undo and repeated Redo below JNI. It mints no history
    // entry, records no Construction step, writes no {@code .forge} byte and
    // reaches no checkpoint of its own.

    /** Length of the array {@link #sculptHistoryState} fills. */
    static final int SCULPT_HISTORY_STATE_SIZE = 5;

    /**
     * 1 when the navigator has a branch to offer: in Sculpt, no stroke in
     * flight, and the active body has a sculpt mesh.
     *
     * <p>Deliberately true for an EMPTY history too — there is still one state,
     * the one on screen — so the control does not vanish exactly when a new
     * user first looks for it.
     */
    static final int SCULPT_HISTORY_AVAILABLE = 0;
    /** Which state the mesh currently shows, as an ordinal on the branch. */
    static final int SCULPT_HISTORY_CURSOR = 1;
    /** How many states lie behind the cursor. Equals {@link #sculptUndoDepth}. */
    static final int SCULPT_HISTORY_UNDO_COUNT = 2;
    /** How many lie ahead of it. Equals {@link #sculptRedoDepth}. */
    static final int SCULPT_HISTORY_REDO_COUNT = 3;
    /**
     * How many rows the navigator draws: undo + redo + 1.
     *
     * <p>Never zero. A body with no strokes at all still has one state, and
     * ordinal 0 is the oldest state still RETAINED rather than necessarily the
     * Freeze — eviction drops from the oldest end, and what it dropped cannot
     * be jumped to.
     */
    static final int SCULPT_HISTORY_STATE_COUNT = 4;

    /**
     * Reads the navigator's whole model in one locked native read.
     *
     * <p>One call rather than five, for the reason {@link #sculptState} is one
     * call: the five values must describe ONE body's branch at ONE instant, and
     * reading them separately would let a body switch or a stroke land between
     * two of them.
     *
     * @param out at least {@link #SCULPT_HISTORY_STATE_SIZE} doubles, indexed
     *     by the {@code SCULPT_HISTORY_*} constants
     */
    static native void sculptHistoryState(double[] out);

    /**
     * Moves the active body's sculpt mesh to one state on its retained branch.
     *
     * <p>Backward is repeated Undo and forward is repeated Redo — not merely
     * equivalent to them, but implemented as them — so one jump lands on
     * bit-exactly the geometry the taps would have produced. A jump to the
     * state already shown applies nothing and answers
     * {@link #HISTORY_NOTHING_TO_DO}; an ordinal off the branch is refused with
     * {@link #HISTORY_OUT_OF_RANGE} and moves nothing at all.
     *
     * <p>The abandoned future is NOT dropped here. Jumping backward leaves the
     * states ahead walkable, exactly as Undo does; they go by the existing rule
     * when the next stroke makes them describe a future that no longer follows.
     *
     * @param targetCursor the state's ordinal, {@code 0} to
     *     {@code stateCount - 1}
     * @return one of the {@code HISTORY_*} constants
     */
    static native int sculptJumpToHistoryCursor(int targetCursor);

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
     * Sculpt Isolate (Stage027): restricts the viewport to the body being
     * sculpted, or lifts the restriction. A native-owned, session-only VIEW
     * decision -- it reaches no {@code .forge} byte, fingerprint, checkpoint or
     * history step and writes no visibility. Refused outside Sculpt.
     *
     * @return true when the request was applied
     */
    static native boolean setSculptIsolate(boolean on);

    /** Whether the viewport is currently isolated; always false outside Sculpt. */
    static native boolean sculptIsolated();

    /**
     * Read-only verification seam: the ObjectIds of the list the viewport draws
     * and picks against right now, in scene order. Mutates nothing.
     *
     * @return how many ids were written into {@code outIds}
     */
    static native int debugViewSceneBodyIds(long[] outIds);

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

    /** The lightest DARK ground. */
    static final int VIEWPORT_BACKGROUND_LIGHT_CHARCOAL = 2;

    /** A warm, cream-biased LIGHT ground (`UI-PREF-R1`, UI-OWNER-42). */
    static final int VIEWPORT_BACKGROUND_WARM_LIGHT = 3;

    /** A cool, steel-biased LIGHT ground. */
    static final int VIEWPORT_BACKGROUND_COOL_LIGHT = 4;

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

    // --- gizmo appearance preferences (`UI-PREF-R1` E/F) ---------------------
    //
    // Both are PRESENTATION on the display settings' terms: no mesh is
    // published, no revision minted, no history step recorded, no vertex moved,
    // and nothing about which handle a touch grabs or how far a drag moves a
    // body changes. The Android layer persists them in AppPreferences and pushes
    // them down on every launch, exactly as it pushes the viewport appearance.

    /** The smallest visual size the domain accepts, as a multiplier. */
    static final float GIZMO_VISUAL_SCALE_MIN = 0.9f;
    /** The accepted gizmo, exactly. */
    static final float GIZMO_VISUAL_SCALE_DEFAULT = 1.0f;
    /** The largest visual size the domain accepts. */
    static final float GIZMO_VISUAL_SCALE_MAX = 1.5f;

    /**
     * Requests how large the gizmo is drawn and placed. Refused — leaving the
     * current size standing — outside
     * [{@link #GIZMO_VISUAL_SCALE_MIN}, {@link #GIZMO_VISUAL_SCALE_MAX}] or
     * non-finite; the hit corridors and every drag amount are unchanged at any
     * accepted value.
     */
    static native boolean setGizmoVisualScale(float scale);

    /** @return the visual size multiplier in effect */
    static native float gizmoVisualScale();

    /** The Regular bundle at half the spread. */
    static final int GIZMO_STROKE_THIN = 0;
    /** The accepted gizmo, byte-identical to the pre-preference geometry. */
    static final int GIZMO_STROKE_REGULAR = 1;
    /** A wider, filled bundle. */
    static final int GIZMO_STROKE_BOLD = 2;

    /**
     * Requests how heavily the gizmo's strokes are drawn. An unknown index is
     * refused and the current weight stands.
     *
     * @return the {@code GIZMO_STROKE_*} constant in effect afterwards
     */
    static native int setGizmoStrokeWeight(int weight);

    /** @return the active {@code GIZMO_STROKE_*} constant */
    static native int gizmoStrokeWeight();

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
     * Shows or hides the <b>selection outline</b>: the persistent silhouette
     * the renderer draws around the selected body ({@code SEL-OUT-R1},
     * UI-OWNER-10 / UI-OWNER-11).
     *
     * <p>A true geometry-derived outline, not an approximation. Native code
     * rasterises the selected body's own device-local vertex and index buffers
     * — the same ones its shaded draw binds — into a coverage mask with a depth
     * attachment, then extracts the edge in screen space. So it is correct for
     * every representation without asking which one a body is, it follows the
     * exact model transform the body is drawn with, and it obeys depth: the
     * part of a selected body hidden behind another body is not in the mask and
     * therefore grows no outline.
     *
     * <p><b>Presentation only.</b> Toggling this publishes no mesh, mints no
     * {@code MeshRevision}, rebuilds no render mesh, re-uploads no vertex or
     * index buffer, regenerates no CAD body, records no Construction or Sculpt
     * history step, dirties no project and reaches no {@code .forge} byte. With
     * it off the renderer records neither the mask pass nor the composite draw,
     * so "off" costs one boolean per frame.
     *
     * <p>Its lifecycle is the <b>grid's</b>, deliberately: native-owned,
     * process-scoped and session-only, so the choice survives rotation, an
     * Activity recreation and a HOME/resume with no save/restore code up here,
     * and a process kill returns it to the default, which is <b>on</b>. It is
     * <em>not</em> an {@link AppPreferences} field — the persistent preferences
     * are the ones the Settings page owns, and a transient viewport overlay
     * belongs with the other transient viewport overlays.
     *
     * @return whether the outline is drawn afterwards
     */
    static native boolean setSelectionOutlineVisible(boolean visible);

    /** @return whether the selected body's outline is currently drawn */
    static native boolean selectionOutlineVisible();

    /** Slots in the array {@link #selectionOutlineStats} fills. */
    static final int OUTLINE_STATS_SIZE = 7;
    /** Mask + depth image allocations for the life of the process. */
    static final int OUTLINE_STAT_MASK_ALLOCATIONS = 0;
    /** Frames in which the mask pass was recorded. */
    static final int OUTLINE_STAT_MASK_PASS_FRAMES = 1;
    /** Composite draws recorded. */
    static final int OUTLINE_STAT_COMPOSITE_DRAWS = 2;
    /** Mask width in pixels. */
    static final int OUTLINE_STAT_MASK_WIDTH = 3;
    /** Mask height in pixels. */
    static final int OUTLINE_STAT_MASK_HEIGHT = 4;
    /** The band's half-width in screen pixels the last composite used. */
    static final int OUTLINE_STAT_WIDTH_PIXELS = 5;
    /** 1 when the outline is currently enabled. */
    static final int OUTLINE_STAT_ENABLED = 6;

    /**
     * Reads the renderer's bounded selection-outline counters.
     *
     * <p>A diagnostic seam and nothing else, on {@link
     * #debugRendererDeviceRebuilds}'s terms: it carries a count, an extent and
     * a width, and no {@code ObjectId}, geometry, dimension or {@code .forge}
     * byte. It exists so the two performance promises can be <em>asserted</em>
     * rather than inferred — that repeated selection switches allocate no GPU
     * resource, and that turning the outline off stops the work.
     *
     * <p>Read without the render thread's cooperation, so a momentarily stale
     * count is possible and is corrected on the next read.
     */
    static native void selectionOutlineStats(double[] out);

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
    static final int CAD_INVALID_EXTRUDE_EXTENT = 30;
    // `CAD-VERTICAL-SLICE-R1`: regions, the feature chain and the operations,
    // APPENDED in the native enum's order so no code above moves.
    static final int CAD_PROFILE_REGION_MISMATCH = 31;
    static final int CAD_OVERLAPPING_REGIONS = 32;
    static final int CAD_OVERLAPPING_HOLES = 33;
    static final int CAD_TOO_MANY_REGIONS = 34;
    static final int CAD_INVALID_FEATURE_OPERATION = 35;
    static final int CAD_TOO_MANY_FEATURES = 36;
    static final int CAD_FEATURE_SUPPORT_INVALID = 37;
    static final int CAD_SUPPORT_FACE_LOST = 38;
    static final int CAD_OPERATION_NEEDS_TARGET = 39;
    static final int CAD_ADD_DISJOINT = 40;
    static final int CAD_ADD_NO_EFFECT = 41;
    static final int CAD_CUT_NO_INTERSECTION = 42;
    static final int CAD_CUT_REMOVES_BODY = 43;
    static final int CAD_KERNEL_FAILED = 44;
    /**
     * `CAD-V6-S2-CORRECTION-FILL-HUD-R1`: the planar arrangement's refusals
     * Finish can now return for a sketch whose curves cross, instead of handing
     * it to the loop model.
     */
    static final int CAD_PLANAR_FACE_AMBIGUOUS_OVERLAP = 57;
    static final int CAD_PLANAR_FACE_CAP_EXCEEDED = 58;
    static final int CAD_PLANAR_FACE_DEGENERATE = 59;
    /** Two chosen areas meet only at a point: their union cannot be extruded. */
    static final int CAD_PLANAR_FACES_TOUCH_AT_POINT = 61;
    /**
     * `CAD-V6-REVOLVE-NEWBODY-E2E-R1`: the Revolve feature's refusals, appended
     * below JNI so every earlier code keeps its number.
     */
    static final int CAD_INVALID_FEATURE_KIND = 62;
    static final int CAD_REVOLVE_AXIS_UNRESOLVED = 63;
    static final int CAD_REVOLVE_AXIS_NOT_STRAIGHT = 64;
    static final int CAD_REVOLVE_AXIS_DEGENERATE = 65;
    static final int CAD_REVOLVE_ANGLE_INVALID = 66;
    static final int CAD_REVOLVE_DIRECTION_INVALID = 67;
    static final int CAD_REVOLVE_PROFILE_CROSSES_AXIS = 68;
    static final int CAD_REVOLVE_ZERO_RADIUS = 69;
    static final int CAD_REVOLVE_COMPONENTS_OVERLAP = 70;
    static final int CAD_REVOLVE_PAYLOAD_MISMATCH = 71;
    static final int CAD_REVOLVE_LATER_FEATURE_UNSUPPORTED = 72;
    static final int CAD_REVOLVE_NEEDS_AXIS = 73;
    // `CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`, appended in the native order.
    static final int CAD_SKETCH_DIMENSION_INVALID = 74;
    static final int CAD_SKETCH_DIMENSION_CONFLICT = 75;
    static final int CAD_SKETCH_DIMENSION_DEPENDENCY = 76;
    static final int CAD_SKETCH_DIMENSION_LOCKED = 77;
    static final int CAD_SKETCH_DIMENSION_READ_ONLY = 78;
    static final int CAD_SKETCH_DIMENSION_VALUE_INVALID = 79;
    static final int CAD_TRIM_SPLINE_UNSUPPORTED = 80;
    static final int CAD_DRAFTING_NO_TARGET = 81;
    static final int CAD_EXTEND_UNSUPPORTED = 82;
    static final int CAD_EXTEND_NO_TARGET = 83;
    static final int CAD_EXTEND_AMBIGUOUS = 84;
    static final int CAD_OFFSET_SPLINE_UNSUPPORTED = 85;
    static final int CAD_OFFSET_INVALID = 86;
    static final int CAD_OFFSET_MITER_LIMIT = 87;
    static final int CAD_OFFSET_SELF_INTERSECTING = 88;
    static final int CAD_MIRROR_AXIS_INVALID = 89;
    static final int CAD_MIRROR_NOTHING_SELECTED = 90;

    /**
     * What an extrusion does to material (`CAD-VERTICAL-SLICE-R1`), in the
     * native enum's order: New Body makes a new body; Add and Cut change the
     * body the sketch stands on, in place.
     */
    static final int OPERATION_NEW_BODY = 0;
    static final int OPERATION_ADD = 1;
    static final int OPERATION_CUT = 2;
    /** Bits of {@link #CAD_EXTRUDE_OPERATIONS_AVAILABLE}. */
    static final int OPERATION_BIT_NEW_BODY = 1;
    static final int OPERATION_BIT_ADD = 2;
    static final int OPERATION_BIT_CUT = 4;

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

    // -----------------------------------------------------------------------
    // Sketch drafting (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`)
    // -----------------------------------------------------------------------
    //
    // The selection set, the Construction role, the modify modes and the
    // retained dimensions all live in the ONE native session. Java reads them on
    // every refresh and holds none of them.

    /** What the last snap landed on: slot {@link #SKETCH_LAST_SNAP}. */
    static final int SNAP_NONE = 0;
    static final int SNAP_GRID = 1;
    static final int SNAP_ENDPOINT = 2;
    static final int SNAP_INTERSECTION = 3;
    static final int SNAP_MIDPOINT = 4;
    static final int SNAP_CENTER = 5;
    static final int SNAP_ORIGIN = 6;
    static final int SNAP_HORIZONTAL_GUIDE = 7;
    static final int SNAP_VERTICAL_GUIDE = 8;

    /** The modify modes, as the native SketchModifyMode index. */
    static final int MODIFY_NONE = 0;
    static final int MODIFY_DIMENSION = 1;
    static final int MODIFY_TRIM = 2;
    static final int MODIFY_EXTEND = 3;
    static final int MODIFY_OFFSET = 4;
    static final int MODIFY_MIRROR = 5;

    /** Dimension kinds, as the native SketchDimensionKind index. */
    static final int DIM_LINE_LENGTH = 0;
    static final int DIM_LINE_ANGLE = 1;
    static final int DIM_LINE_HORIZONTAL = 2;
    static final int DIM_LINE_VERTICAL = 3;
    static final int DIM_RECTANGLE_WIDTH = 4;
    static final int DIM_RECTANGLE_HEIGHT = 5;
    static final int DIM_CIRCLE_RADIUS = 6;
    static final int DIM_CIRCLE_DIAMETER = 7;
    static final int DIM_EDGE_LENGTH = 8;
    static final int DIM_ARC_RADIUS = 9;
    static final int DIM_ARC_SWEEP = 10;
    static final int DIM_EDGE_ANGLE = 11;
    static final int DIM_KIND_COUNT = 12;
    /** Dimension modes as {@link #sketchAddDimension} takes them. */
    static final int DIM_MODE_DRIVING = 0;
    static final int DIM_MODE_REFERENCE = 1;

    /** Dimension visibility, as the native SketchDimensionVisibility index. */
    static final int DIM_VISIBILITY_SELECTED = 0;
    static final int DIM_VISIBILITY_ALL = 1;
    static final int DIM_VISIBILITY_OFF = 2;

    /** Slots of {@link #sketchDraftingState}. */
    static final int SKETCH_DRAFT_SIZE = 25;
    static final int SKETCH_DRAFT_MODE = 0;
    static final int SKETCH_DRAFT_SELECTION_COUNT = 1;
    static final int SKETCH_DRAFT_MULTI_SELECT = 2;
    static final int SKETCH_DRAFT_CONSTRUCTION_TARGET = 3;
    static final int SKETCH_DRAFT_DIM_TARGET_ENTITY = 4;
    static final int SKETCH_DRAFT_DIM_TARGET_EDGE = 5;
    static final int SKETCH_DRAFT_DIM_AWAIT_SECOND = 6;
    static final int SKETCH_DRAFT_OFFSET_SOURCE = 7;
    static final int SKETCH_DRAFT_OFFSET_DISTANCE = 8;
    static final int SKETCH_DRAFT_OFFSET_STATUS = 9;
    static final int SKETCH_DRAFT_MIRROR_AXIS_SET = 10;
    static final int SKETCH_DRAFT_MIRROR_AXIS_ENTITY = 11;
    static final int SKETCH_DRAFT_MIRROR_AXIS_EDGE = 12;
    static final int SKETCH_DRAFT_DIM_VISIBILITY = 13;
    static final int SKETCH_DRAFT_DIM_COUNT = 14;
    static final int SKETCH_DRAFT_LAST_DELETED_DIMS = 15;
    static final int SKETCH_DRAFT_TRIM_CONVERTED_RECTANGLE = 16;
    static final int SKETCH_DRAFT_LAST_SNAP = 17;
    static final int SKETCH_DRAFT_GUIDE_H = 18;
    static final int SKETCH_DRAFT_GUIDE_V = 19;
    static final int SKETCH_DRAFT_CONSTRUCTION_COUNT = 20;
    static final int SKETCH_DRAFT_SINGLE_ROLE = 21;
    /** Advances on every Trim or Extend that landed, so each is reported once. */
    static final int SKETCH_DRAFT_TAP_ACT_SERIAL = 22;
    /** The single selection's SKETCH_ENTITY_KIND_*, or -1. */
    static final int SKETCH_DRAFT_SINGLE_KIND = 23;
    /** Which tap act last landed: {@link #TAP_ACT_TRIM} or {@link #TAP_ACT_EXTEND}. */
    static final int SKETCH_DRAFT_TAP_ACT = 24;
    static final int TAP_ACT_NONE = 0;
    static final int TAP_ACT_TRIM = 1;
    static final int TAP_ACT_EXTEND = 2;

    /** Stride of {@link #sketchDimensionLabels}. */
    static final int SKETCH_LABEL_STRIDE = 10;
    static final int SKETCH_LABEL_ID = 0;
    static final int SKETCH_LABEL_KIND = 1;
    static final int SKETCH_LABEL_MODE = 2;
    static final int SKETCH_LABEL_VALUE = 3;
    static final int SKETCH_LABEL_X = 4;
    static final int SKETCH_LABEL_Y = 5;
    static final int SKETCH_LABEL_PROJECTS = 6;
    static final int SKETCH_LABEL_ENTITY = 7;
    /** The point on the measured geometry the label stands off from (screen px). */
    static final int SKETCH_LABEL_ATTACH_X = 8;
    static final int SKETCH_LABEL_ATTACH_Y = 9;
    /** The most labels a refresh reads; the domain caps dimensions at 512. */
    static final int SKETCH_LABEL_MAX = 512;

    /** Stride of {@link #sketchDimensions}. */
    static final int SKETCH_DIM_STRIDE = 8;
    static final int SKETCH_DIM_ID = 0;
    static final int SKETCH_DIM_KIND = 1;
    static final int SKETCH_DIM_MODE = 2;
    static final int SKETCH_DIM_FIRST_ENTITY = 3;
    static final int SKETCH_DIM_FIRST_EDGE = 4;
    static final int SKETCH_DIM_SECOND_ENTITY = 5;
    static final int SKETCH_DIM_SECOND_EDGE = 6;
    static final int SKETCH_DIM_VALUE = 7;

    /** Size of {@link #sketchEntityValues}: the entity slots plus role and a point. */
    static final int SKETCH_ENTITY_VALUES_SIZE = 9;
    static final int SKETCH_ENTITY_ROLE = 6;
    static final int SKETCH_ENTITY_EXTRA_U = 7;
    static final int SKETCH_ENTITY_EXTRA_V = 8;

    /** The selection, in the order it was made. Returns the count. */
    static native int sketchSelection(long[] out);

    /** Whether a Select tap adds to and removes from the selection. */
    static native void sketchSetMultiSelect(boolean on);

    /** Adds the entity to the selection or removes it. */
    static native boolean sketchToggleSelectEntity(long entityId);

    /** Make Construction / Make Regular for the whole selection, as one act. */
    static native int sketchToggleConstruction();

    /** Enters or leaves a modify mode ({@code MODIFY_*}). */
    static native int sketchSetModifyMode(int mode);

    /** The drafting state; {@link #SKETCH_DRAFT_SIZE} slots. */
    static native void sketchDraftingState(double[] out);

    /** The dimension kinds the Dimension mode's target supports. */
    static native int sketchDimensionTargetKinds(int[] out);

    /** Chooses the Dimension mode's target edge directly (the panel's path). */
    static native int sketchSetDimensionTarget(long entityId, int edge);

    /** The next straight-edge tap becomes an angle's second edge. */
    static native int sketchBeginDimensionAngle();

    /** Adds a dimension of {@code kind} ({@code DIM_*}) and {@code mode} on the target. */
    static native int sketchAddDimension(int kind, int mode);

    /** Removes a dimension; the geometry is untouched. */
    static native int sketchRemoveDimension(long dimensionId);

    /** THE Driving edit: metres for a length, degrees for an angle. */
    static native int sketchApplyDimensionValue(long dimensionId, double value);

    /** Selected / All / Off ({@code DIM_VISIBILITY_*}). */
    static native void sketchSetDimensionVisibility(int visibility);

    /** The shown labels, stride {@link #SKETCH_LABEL_STRIDE}. Returns the count. */
    static native int sketchDimensionLabels(double[] out);

    /** Every retained dimension, stride {@link #SKETCH_DIM_STRIDE}. Returns the count. */
    static native int sketchDimensions(double[] out);

    /** Every entity id in stored order. Returns the count. */
    static native int sketchEntityIds(long[] out);

    /** One entity by id; {@link #SKETCH_ENTITY_VALUES_SIZE} slots. */
    static native boolean sketchEntityValues(long entityId, double[] out);

    /** The Offset mode's signed distance, metres, as typed. */
    static native int sketchSetOffsetDistance(double meters);

    /** Creates the offset previewed now; the mode ends. */
    static native int sketchConfirmOffset();

    /** Creates the mirror previewed now; the mode ends. */
    static native int sketchConfirmMirror();

    /** Slots of {@link #sketchLineDimension}. */
    static final int SKETCH_DIMENSION_SIZE = 4;
    static final int SKETCH_DIMENSION_ENTITY = 0;
    static final int SKETCH_DIMENSION_LENGTH = 1;
    static final int SKETCH_DIMENSION_ANCHOR_U = 2;
    static final int SKETCH_DIMENSION_ANCHOR_V = 3;

    /**
     * Slots of {@link #cadExtrudeToolState} (`CAD-UX-S1`).
     *
     * <p>Read as ONE call because the arrow, the value beside it and the badge
     * have to describe one instant; reading them separately would let a drag
     * land between two of them. Every slot is DERIVED below JNI on every read —
     * the shell stores no depth, no direction and no anchor.
     */
    static final int CAD_EXTRUDE_SIZE = 59;
    /** 1 when the canvas manipulator is live; 0 is the whole reason it is absent. */
    static final int CAD_EXTRUDE_ACTIVE = 0;
    /** The PRIMARY side's distance: the whole depth of a One Side extrusion. */
    static final int CAD_EXTRUDE_DEPTH = 1;
    static final int CAD_EXTRUDE_DIRECTION = 2;
    static final int CAD_EXTRUDE_PROFILE = 3;
    static final int CAD_EXTRUDE_DRAGGING = 4;
    /** 1 when the anchor projects on screen. 0 means HIDE, never guess a spot. */
    static final int CAD_EXTRUDE_ON_SCREEN = 5;
    static final int CAD_EXTRUDE_LABEL_X = 6;
    static final int CAD_EXTRUDE_LABEL_Y = 7;
    static final int CAD_EXTRUDE_TIP_X = 8;
    static final int CAD_EXTRUDE_TIP_Y = 9;
    /**
     * The camera-attached VISUAL multiplier (0.40..1.60) the glyphs and the
     * value text are drawn at. Never a hit area.
     */
    static final int CAD_EXTRUDE_SCALE = 10;
    /** 0 unclamped, 1 clamped at the minimum, 2 at the maximum. */
    static final int CAD_EXTRUDE_CLAMP = 11;
    static final int CAD_EXTRUDE_CLAMP_NONE = 0;
    static final int CAD_EXTRUDE_CLAMP_LOW = 1;
    static final int CAD_EXTRUDE_CLAMP_HIGH = 2;

    // `CAD-EXT-R1`. The slots above describe the PRIMARY side — the only one a
    // One Side extrusion has — so nothing that read them before had to change.
    /** The extent mode: {@link #EXTENT_ONE_SIDE}, {@code _SYMMETRIC}, {@code _TWO_SIDES}. */
    static final int CAD_EXTRUDE_EXTENT = 12;
    /** The +N distance, in metres. */
    static final int CAD_EXTRUDE_POSITIVE = 13;
    /** The -N distance, in metres. */
    static final int CAD_EXTRUDE_NEGATIVE = 14;
    /** 1 when the SECOND side exists and its anchor projects. 0 means HIDE. */
    static final int CAD_EXTRUDE_SECOND_ON_SCREEN = 15;
    static final int CAD_EXTRUDE_SECOND_LABEL_X = 16;
    static final int CAD_EXTRUDE_SECOND_LABEL_Y = 17;
    static final int CAD_EXTRUDE_SECOND_TIP_X = 18;
    static final int CAD_EXTRUDE_SECOND_TIP_Y = 19;
    /** Which side a live drag captured: 0 none, 1 the +N side, 2 the -N side. */
    static final int CAD_EXTRUDE_DRAG_SIDE = 20;

    // `CAD-VERTICAL-SLICE-R1`. Valid whenever a session is open -- including in
    // Ready before any region is chosen, when CAD_EXTRUDE_ACTIVE is 0.
    /** 1 when the session is in Ready (the extrusion stage). */
    static final int CAD_EXTRUDE_READY = 21;
    /** The operation: {@link #OPERATION_NEW_BODY}, {@code _ADD}, {@code _CUT}. */
    static final int CAD_EXTRUDE_OPERATION = 22;
    /** Which operations can be chosen now, as {@code OPERATION_BIT_*}. */
    static final int CAD_EXTRUDE_OPERATIONS_AVAILABLE = 23;
    /**
     * The candidate's CAD status: {@link #CAD_OK} when the preview IS what the
     * commit will make, otherwise the named reason the commit would refuse.
     */
    static final int CAD_EXTRUDE_CANDIDATE_STATUS = 24;
    /** The later feature that refused, or 0. */
    static final int CAD_EXTRUDE_FAILED_FEATURE = 25;
    /** The body an Add/Cut changes or an edit rewrites; 0 for a New Body. */
    static final int CAD_EXTRUDE_TARGET_BODY = 26;
    /** The last candidate regeneration, in microseconds. */
    static final int CAD_EXTRUDE_PREVIEW_MICROS = 27;
    static final int CAD_EXTRUDE_REGION_COUNT = 28;
    static final int CAD_EXTRUDE_SELECTED_REGIONS = 29;
    /** The feature an edit session edits, or 0 for a new one. */
    static final int CAD_EXTRUDE_EDITING_FEATURE = 30;
    static final int CAD_EXTRUDE_CANDIDATE_REVISION = 31;

    // `CAD-FOUNDATION-C1`. The technical-drawing leader each value stands
    // beside: the dimension line the frame draws next to the shaft, projected
    // below JNI. Start is beside the base, end beside the tip.
    /** 1 when the PRIMARY leader projects; 0 means HIDE, never guess a spot. */
    static final int CAD_EXTRUDE_LEADER_ON_SCREEN = 32;
    static final int CAD_EXTRUDE_LEADER_START_X = 33;
    static final int CAD_EXTRUDE_LEADER_START_Y = 34;
    static final int CAD_EXTRUDE_LEADER_END_X = 35;
    static final int CAD_EXTRUDE_LEADER_END_Y = 36;
    /** 1 when the SECOND side's leader projects. */
    static final int CAD_EXTRUDE_SECOND_LEADER_ON_SCREEN = 37;
    static final int CAD_EXTRUDE_SECOND_LEADER_START_X = 38;
    static final int CAD_EXTRUDE_SECOND_LEADER_START_Y = 39;
    static final int CAD_EXTRUDE_SECOND_LEADER_END_X = 40;
    static final int CAD_EXTRUDE_SECOND_LEADER_END_Y = 41;
    // `CAD-FOUNDATION-C2`. The PRIMARY arrow's drawn point -- the far end of
    // its head, from the same native function the drawing and the hit test
    // end the head at. The action panel is anchored just past it.
    /** 1 when the primary arrow exists and its point projects; 0 means HIDE the panel. */
    static final int CAD_EXTRUDE_HEAD_ON_SCREEN = 42;
    static final int CAD_EXTRUDE_HEAD_X = 43;
    static final int CAD_EXTRUDE_HEAD_Y = 44;
    // `CAD-V6-S2-OWNER-CORRECTION-E2E-R1`. The action DOCK: a world rectangle
    // on the axis past that point, projected below JNI, whole or not at all.
    /** 1 when the dock is drawn; 0 means it is ABSENT (46..56 meaningless). */
    static final int CAD_EXTRUDE_DOCK_VISIBLE = 45;
    /** The near-axis fade, (0, 1]. */
    static final int CAD_EXTRUDE_DOCK_ALPHA = 46;
    /** Top-left x; then y, top-right x/y, bottom-right x/y, bottom-left x/y (47..54). */
    static final int CAD_EXTRUDE_DOCK_TL_X = 47;
    static final int CAD_EXTRUDE_DOCK_CENTRE_X = 55;
    static final int CAD_EXTRUDE_DOCK_CENTRE_Y = 56;
    /** |sin| of the view against the axis at the arrow point. Diagnostic. */
    static final int CAD_EXTRUDE_DOCK_AXIS_SINE = 57;
    /** Why the dock is hidden: 0 shown, 1 no frame, 2 near the axis, 3 behind the eye, 4 off the viewport. */
    static final int CAD_EXTRUDE_DOCK_HIDDEN = 58;
    static final int DOCK_SHOWN = 0;
    static final int DOCK_HIDDEN_NO_FRAME = 1;
    static final int DOCK_HIDDEN_NEAR_AXIS = 2;
    static final int DOCK_HIDDEN_BEHIND_EYE = 3;
    static final int DOCK_HIDDEN_OFF_VIEWPORT = 4;

    /** Extent modes, in the native enum's own order. */
    static final int EXTENT_ONE_SIDE = 0;
    static final int EXTENT_SYMMETRIC = 1;
    static final int EXTENT_TWO_SIDES = 2;

    /** Side selectors for {@link #sketchSetExtrudeSide}. */
    static final int EXTRUDE_SIDE_POSITIVE = 1;
    static final int EXTRUDE_SIDE_NEGATIVE = 2;

    /**
     * Slots of {@link #sketchProfileInfo}. Since `CAD-VERTICAL-SLICE-R1` a
     * "profile" is a REGION (an outer loop minus its holes); a 3-slot array
     * still reads the first three, a {@link #SKETCH_REGION_INFO_SIZE} one the
     * rest.
     */
    static final int SKETCH_PROFILE_INFO_SIZE = 3;
    static final int SKETCH_REGION_INFO_SIZE = 11;
    static final int SKETCH_REGION_KIND = 0;
    static final int SKETCH_REGION_VERTICES = 1;
    static final int SKETCH_REGION_AREA = 2;
    static final int SKETCH_REGION_HOLES = 3;
    static final int SKETCH_REGION_SELECTED = 4;
    static final int SKETCH_REGION_SELECTABLE = 5;
    static final int SKETCH_REGION_STATUS = 6;
    static final int SKETCH_REGION_ON_SCREEN = 7;
    static final int SKETCH_REGION_SCREEN_X = 8;
    static final int SKETCH_REGION_SCREEN_Y = 9;
    static final int SKETCH_REGION_DEPTH = 10;
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

    /**
     * The ObjectId the viewport's tap selection currently holds, 0 for none.
     * Read-only verification seam on {@link #debugProjectWorld}'s terms: it lets
     * a test observe what a viewport tap did to the selection, and mutates
     * nothing.
     */
    static native long debugViewportSelection();

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
     * Adds the region to the selection, or removes it when it is selected.
     * Adding replaces any selected region it would overlap or share a loop
     * with. Returns a {@code CAD_*} code.
     */
    static native int sketchToggleRegion(long outerAnchorEntityId);

    /** Chooses New Body, Add or Cut. Returns a {@code CAD_*} code. */
    static native int sketchSetOperation(int operation);

    /**
     * Where a sketch point is on screen, in view-local pixels. Verification
     * infrastructure on the gizmo's terms: the one honest source of a pixel
     * to send a synthetic touch to.
     */
    static native boolean sketchScreenPoint(double u, double v, float[] out);

    /** The current adaptive sketch grid step in metres (`CAD-A3`): what a grid
     *  snap rounds to at the zoom the last drag started under. */
    static native double sketchGridStep();

    // --- the canvas extrude manipulator (`CAD-UX-S1`) --------------------
    //
    // The arrow, the exact length beside it and the Flip that reverses it are
    // three views of the ONE extrusion the session already owned. Java holds
    // none of it: it reads the state below on every refresh and sends acts back.

    /** The whole manipulator state, in the CAD_EXTRUDE_* slots, in one read. */
    static native void cadExtrudeToolState(double[] out);

    /**
     * Reverses which side of the sketch plane the solid grows on, keeping the
     * exact depth and the same profile. A DIRECTION change and never a negative
     * depth. Refused by name outside the sketch Ready state.
     */
    static native int sketchFlipExtrudeDirection();

    // --- Revolve (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`) -------------------------
    //
    // The axis, the angle and the direction live below JNI until the one
    // commit; Java submits acts and re-reads the REVOLVE_* slots.

    /** Ready (Extrude) -> Ready (Revolve). Returns a {@code CAD_*} code. */
    static native int sketchBeginRevolve();

    /** Ready (Revolve) -> Ready (Extrude), keeping the selection. */
    static native int sketchEndRevolve();

    /** Re-enters the axis pick: the next tap on a straight edge sets the axis. */
    static native int sketchBeginRevolveAxisPick();

    /** Sets the axis to exactly this edge, or refuses it by name. */
    static native int sketchSetRevolveAxis(long entityId, int edgeIndex);

    /** A typed angle in degrees, refused (never clamped) outside (0, 360]. */
    static native int sketchSetRevolveAngle(double degrees);

    /** Reverses the revolve direction, keeping the exact angle. */
    static native int sketchFlipRevolve();

    /** The Revolve chrome's state, in the REVOLVE_* slots, in one read. */
    static native void cadRevolveToolState(double[] out);

    static final int REVOLVE_STATE_SIZE = 24;
    static final int REVOLVE_ACTIVE = 0;
    static final int REVOLVE_AXIS_PICKING = 1;
    static final int REVOLVE_AXIS_ENTITY = 2;
    static final int REVOLVE_AXIS_EDGE = 3;
    static final int REVOLVE_ANGLE = 4;
    static final int REVOLVE_DIRECTION = 5;
    static final int REVOLVE_CANDIDATE_STATUS = 6;
    static final int REVOLVE_LABEL_VISIBLE = 7;
    static final int REVOLVE_LABEL_X = 8;
    static final int REVOLVE_LABEL_Y = 9;
    static final int REVOLVE_HANDLE_VISIBLE = 10;
    static final int REVOLVE_HANDLE_X = 11;
    static final int REVOLVE_HANDLE_Y = 12;
    static final int REVOLVE_AXIS_VISIBLE = 13;
    static final int REVOLVE_AXIS_START_X = 14;
    static final int REVOLVE_AXIS_START_Y = 15;
    static final int REVOLVE_AXIS_END_X = 16;
    static final int REVOLVE_AXIS_END_Y = 17;
    static final int REVOLVE_DRAGGING = 18;
    static final int REVOLVE_AVAILABLE = 19;
    static final int REVOLVE_FULL_TURN = 20;
    static final int REVOLVE_SELECTED_AREAS = 21;
    static final int REVOLVE_EDITING_REVOLVE_BODY = 22;
    static final int REVOLVE_CANDIDATE_REVISION = 23;

    /**
     * Changes the extent mode (`CAD-EXT-R1`): {@link #EXTENT_ONE_SIDE},
     * {@link #EXTENT_SYMMETRIC} or {@link #EXTENT_TWO_SIDES}.
     *
     * <p>The distances carry across by the deterministic transition policy
     * below JNI; the shell holds no draft extent and no draft distance, and a
     * refusal changes nothing at all.
     */
    static native int sketchSetExtrudeExtent(int mode);

    /**
     * Writes ONE side's distance in metres — {@link #EXTRUDE_SIDE_POSITIVE}
     * along the support normal, {@link #EXTRUDE_SIDE_NEGATIVE} against it.
     *
     * <p>The same door an arrow drag lands through below JNI, so a typed value
     * and a dragged one pass exactly the same validation. In Symmetric either
     * side writes the one shared distance; in One Side a write to the empty
     * side is refused by name.
     */
    static native int sketchSetExtrudeSide(int side, double meters);

    /**
     * Where a committed CAD Body's retained sketch is on screen — {@code out[0]}
     * and {@code out[1]} in view-local pixels, {@code out[2]} the same
     * camera-attached multiplier the manipulator cluster is drawn at
     * (`CAD-UX-S1`).
     *
     * <p>A read: no session is begun, nothing is regenerated and no revision is
     * minted. False — so the control is ABSENT rather than shown and refused —
     * for a body that is not a CAD Body, one that is hidden, and one whose
     * anchor does not project on screen.
     */
    static native boolean cadBodySketchAnchor(long bodyId, float[] out);

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

    /**
     * Opens a staged edit of ONE feature of a CAD body's chain: feature 1 is the
     * body's first sketch and extrusion, a later id an Add or a Cut. With
     * {@code startReady} the session opens on the extrusion. Returns a
     * {@code CAD_*} code.
     */
    static native int sketchBeginEditFeature(long bodyId, long featureId, boolean startReady);

    /** The feature an open edit session edits, or 0. */
    static native long sketchEditingFeatureId();

    /** How many features a CAD body's chain carries, the first included. */
    static native int cadFeatureCount(long bodyId);

    /** Slots of {@link #cadFeatureInfo}. */
    static final int CAD_FEATURE_INFO_SIZE = 12;
    static final int CAD_FEATURE_ID = 0;
    static final int CAD_FEATURE_OPERATION = 1;
    static final int CAD_FEATURE_EXTENT = 2;
    static final int CAD_FEATURE_POSITIVE = 3;
    static final int CAD_FEATURE_NEGATIVE = 4;
    static final int CAD_FEATURE_REGIONS = 5;
    static final int CAD_FEATURE_HOLES = 6;
    static final int CAD_FEATURE_SUPPORT = 7;
    static final int CAD_FEATURE_ENTITIES = 8;
    /** {@link #FEATURE_KIND_EXTRUDE} or {@link #FEATURE_KIND_REVOLVE}. */
    static final int CAD_FEATURE_KIND = 9;
    /** A Revolve's angle in degrees; 0 for an Extrude. */
    static final int CAD_FEATURE_ANGLE = 10;
    /** A Revolve's direction: {@link #REVOLVE_POSITIVE} or {@link #REVOLVE_NEGATIVE}. */
    static final int CAD_FEATURE_REVOLVE_DIRECTION = 11;

    /** A feature's kind (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`), in the native enum's order. */
    static final int FEATURE_KIND_EXTRUDE = 0;
    static final int FEATURE_KIND_REVOLVE = 1;
    static final int REVOLVE_POSITIVE = 0;
    static final int REVOLVE_NEGATIVE = 1;

    /** One feature of a CAD body's chain, by chain position. False past the end. */
    static native boolean cadFeatureInfo(long bodyId, int index, double[] out);

    /** Slots of {@link #cadBodyMeasure}. */
    static final int CAD_MEASURE_SIZE = 9;
    static final int CAD_MEASURE_VOLUME = 0;
    static final int CAD_MEASURE_COMPONENTS = 1;
    static final int CAD_MEASURE_TRIANGLES = 2;
    static final int CAD_MEASURE_MIN_X = 3;
    static final int CAD_MEASURE_MAX_X = 6;

    /**
     * A CAD body's regenerated solid, measured in its own local space: volume,
     * shells, triangles and bounds. What a test asserts an Add or a Cut
     * changed, by geometry. False for a body that is not a CAD body.
     */
    static native boolean cadBodyMeasure(long bodyId, double[] out);

    /** Slots of {@link #sketchCandidateMeasure}. */
    static final int CANDIDATE_MEASURE_SIZE = 11;
    static final int CANDIDATE_MEASURE_STATUS = 0;
    static final int CANDIDATE_MEASURE_VOLUME = 1;
    static final int CANDIDATE_MEASURE_COMPONENTS = 2;
    static final int CANDIDATE_MEASURE_TRIANGLES = 3;
    static final int CANDIDATE_MEASURE_MIN_X = 4;
    static final int CANDIDATE_MEASURE_MAX_X = 7;
    static final int CANDIDATE_MEASURE_REVISION = 10;

    /**
     * The staged candidate — the evaluation the preview draws and a commit
     * would apply — measured like {@link #cadBodyMeasure}. Verification
     * infrastructure; false unless a sketch is Ready.
     */
    static native boolean sketchCandidateMeasure(double[] out);

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
    static final int CAD_STATE_SIZE = 11;
    static final int CAD_PLANE = 0;
    /** The PRIMARY authored distance: One Side's depth, Symmetric's per side. */
    static final int CAD_DEPTH = 1;
    static final int CAD_DIRECTION = 2;
    static final int CAD_PROFILE_KIND = 3;
    /** A rectangle's width, or a circle's radius. */
    static final int CAD_PRIMARY_SIZE = 4;
    /** A rectangle's height. */
    static final int CAD_SECONDARY_SIZE = 5;
    static final int CAD_ENTITY_COUNT = 6;
    static final int CAD_PROFILE_VERTICES = 7;
    /** The body's extent mode (`CAD-EXT-R1`), one of the EXTENT_* values. */
    static final int CAD_STATE_EXTENT = 8;
    /** The base feature's kind: {@link #FEATURE_KIND_EXTRUDE} or {@link #FEATURE_KIND_REVOLVE}. */
    static final int CAD_STATE_KIND = 9;
    /** A Revolve body's angle in degrees. */
    static final int CAD_STATE_REVOLVE_ANGLE = 10;
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

    // --- Parametric History (`MODELING-FOUNDATIONS-R1` A) -------------------

    /** Slots of {@link #cadTimeline}'s header. */
    static final int TIMELINE_HEADER_SIZE = 5;
    /** The regeneration verdict, a {@code CAD_*} code (Surface: a SURFACE_* code). */
    static final int TIMELINE_STATUS = 0;
    /** The first failing feature's id, or 0. */
    static final int TIMELINE_FAILED_FEATURE = 1;
    /** The feature a staged edit changes, or 0. */
    static final int TIMELINE_EDITING_FEATURE = 2;
    /** 1 when the verdict is a real evaluation, 0 while a staged sketch is drawn. */
    static final int TIMELINE_EVALUATED = 3;
    static final int TIMELINE_ROW_COUNT = 4;

    /** Slots of one {@link #cadTimeline} row. */
    static final int TIMELINE_ROW_SIZE = 27;
    /** {@link #TIMELINE_KIND_SKETCH} or {@link #TIMELINE_KIND_FEATURE}. */
    static final int TIMELINE_ROW_KIND = 0;
    /** The durable id: a sketch id or a feature id. Never a position. */
    static final int TIMELINE_ROW_ID = 1;
    static final int TIMELINE_ROW_SKETCH = 2;
    /** The feature a tap on the row edits; 0 for a sketch no feature consumes. */
    static final int TIMELINE_ROW_EDIT_FEATURE = 3;
    static final int TIMELINE_ROW_ORDINAL = 4;
    /** One of the TIMELINE_STATE_* values. */
    static final int TIMELINE_ROW_STATE = 5;
    /** A Failed row's reason, a {@code CAD_*} code. */
    static final int TIMELINE_ROW_STATUS = 6;
    static final int TIMELINE_ROW_EDITING = 7;
    static final int TIMELINE_ROW_ENTITIES = 8;
    static final int TIMELINE_ROW_DIMENSIONS = 9;
    static final int TIMELINE_ROW_PLANE = 10;
    static final int TIMELINE_ROW_ON_BODY_FACE = 11;
    static final int TIMELINE_ROW_ON_FEATURE_FACE = 12;
    static final int TIMELINE_ROW_SUPPORT_FEATURE = 13;
    static final int TIMELINE_ROW_FEATURE_KIND = 14;
    static final int TIMELINE_ROW_OPERATION = 15;
    static final int TIMELINE_ROW_PLANAR_FACES = 16;
    static final int TIMELINE_ROW_REGIONS = 17;
    static final int TIMELINE_ROW_HOLES = 18;
    static final int TIMELINE_ROW_EXTENT = 19;
    static final int TIMELINE_ROW_SIDE = 20;
    static final int TIMELINE_ROW_POSITIVE = 21;
    static final int TIMELINE_ROW_NEGATIVE = 22;
    static final int TIMELINE_ROW_ANGLE = 23;
    static final int TIMELINE_ROW_REVOLVE_DIRECTION = 24;
    static final int TIMELINE_ROW_AXIS_ENTITY = 25;
    static final int TIMELINE_ROW_AXIS_EDGE = 26;

    static final int TIMELINE_KIND_SKETCH = 0;
    static final int TIMELINE_KIND_FEATURE = 1;

    static final int TIMELINE_STATE_OK = 0;
    static final int TIMELINE_STATE_FAILED = 1;
    static final int TIMELINE_STATE_NOT_REGENERATED = 2;
    static final int TIMELINE_STATE_PENDING = 3;
    static final int TIMELINE_STATE_UNUSED = 4;

    /** The most rows a CAD timeline can have: one per feature and one per sketch. */
    static final int TIMELINE_MAX_ROWS = 32;

    /**
     * A CAD body's feature chain as a TIMELINE of sketch and feature rows in
     * construction order, derived on every call and stored nowhere. With
     * {@code staged} and an open edit session over the body, the chain the edit
     * stages and its own evaluation: the failing downstream row is the one a
     * commit would be refused by. Returns the row count, or -1 for a body that
     * is not a CAD body.
     */
    static native int cadTimeline(long bodyId, boolean staged, double[] header, double[] rows);
}
