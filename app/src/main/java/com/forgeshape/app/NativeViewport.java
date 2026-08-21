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
     * <p>Nothing here is interpreted on the Java side: {@code action} is the raw
     * {@link android.view.MotionEvent} masked action, and the arrays carry stable
     * pointer ids with view-local pixel coordinates. Native code decides what the
     * gesture means.
     *
     * @param actionPointerId the id of the pointer that is lifting on an up-style
     *                        action, or {@code -1} when the action has no such pointer
     */
    static native void touchEvent(int action, int actionPointerId, int pointerCount,
                                  int[] ids, float[] xs, float[] ys,
                                  int viewWidth, int viewHeight);

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

    /** Length of the array {@link #constructionPrimitive} fills. */
    static final int PRIMITIVE_STATE_SIZE = 11;

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
    static final int SCULPT_STATE_SIZE = 11;

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
    /** How many Grab strokes have been started. */
    static final int SCULPT_STROKE_COUNT = 8;
    /** The object id — the same one the Construction object carries. */
    static final int SCULPT_OBJECT_ID = 9;
    /** The active tool, one of the {@code TOOL_*} constants. */
    static final int SCULPT_TOOL = 10;

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
