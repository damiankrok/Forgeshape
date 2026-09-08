// The JNI boundary for the ForgeShape native viewport: the ONE place Android
// and JNI types appear.
//
// Responsibilities, and nothing more:
//   - own the render thread and the Surface handshake (create/resize/destroy),
//     and guarantee the renderer never touches a destroyed ANativeWindow;
//   - translate Android MotionEvent data into platform-neutral touch samples
//     and arbitrate one gesture between the support chooser, the sketch, the
//     gizmo, the sculpt brush, the camera and selection -- in that order;
//   - expose the process-scoped domain (scene, histories, sketch session,
//     sculpt session, display settings, project codec, GLB import/export) to
//     `NativeViewport.java` as flat primitive calls, under `g_stateMutex`;
//   - hand the renderer one scene snapshot, gizmo snapshot and overlay per
//     frame.
//
// It owns no domain rule: camera, picking, selection, construction, sculpt,
// CAD, history and the codec each live in their own platform-neutral module.

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "forgeshape_body_commands.h"
#include "forgeshape_body_delete.h"
#include "forgeshape_body_dimension_overlay.h"
#include "forgeshape_body_dimensions.h"
#include "forgeshape_camera.h"
#include "forgeshape_body_dimensions_selftest.h"
#include "forgeshape_body_mirror.h"
#include "forgeshape_camera_selftest.h"
#include "forgeshape_construction.h"
#include "forgeshape_construction_selftest.h"
#include "forgeshape_display.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_gizmo_selftest.h"
#include "forgeshape_glb_import_fixture.h"
#include "forgeshape_glb_roundtrip.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_gltf_import.h"
#include "forgeshape_import_commit.h"
#include "forgeshape_import_preview.h"
#include "forgeshape_gltf_export_selftest.h"
#include "forgeshape_gltf_import_selftest.h"
#include "forgeshape_cad_body.h"
#include "forgeshape_cad_a3_selftest.h"
#include "forgeshape_sketch_ux_selftest.h"
#include "forgeshape_cad_selftest.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_support_chooser.h"
#include "forgeshape_history.h"
#include "forgeshape_history_selftest.h"
#include "forgeshape_input.h"
#include "forgeshape_mesh.h"
#include "forgeshape_mesh_fixtures.h"
#include "forgeshape_mesh_selftest.h"
#include "forgeshape_mirror_selftest.h"
#include "forgeshape_picking_selftest.h"
#include "forgeshape_primitive_selftest.h"
#include "forgeshape_project_bootstrap.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_render_mesh_selftest.h"
#include "forgeshape_render_recovery.h"
#include "forgeshape_render_recovery_selftest.h"
#include "forgeshape_renderer.h"
#include "forgeshape_scene.h"
#include "forgeshape_scene_selftest.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sculpt_selftest.h"
#include "forgeshape_selection.h"
#include "forgeshape_cone_capsule_selftest.h"
#include "forgeshape_sphere_selftest.h"
#include "forgeshape_transform.h"
#include "forgeshape_transform_selftest.h"

#define FS_TAG "ForgeShape"
#define FS_LOGI(...) __android_log_print(ANDROID_LOG_INFO, FS_TAG, __VA_ARGS__)
#define FS_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, FS_TAG, __VA_ARGS__)

namespace {

// The ONE way a domain string reaches a Java `String`.
//
// `NewStringUTF` takes MODIFIED UTF-8, in which a 4-byte sequence is illegal
// and CheckJNI aborts a debuggable process on one. A sanitized imported-mesh
// name may legally carry one (an emoji from another tool), so every string that
// can contain user or file text is converted to UTF-16 first and handed to
// `NewString`. ASCII-only tokens elsewhere still use `NewStringUTF`.
jstring newJavaString(JNIEnv* env, const std::string& utf8) {
    const std::vector<uint16_t> units = forgeshape::utf8ToUtf16(utf8);
    if (units.empty()) {
        return env->NewStringUTF("");
    }
    return env->NewString(reinterpret_cast<const jchar*>(units.data()),
                          static_cast<jsize>(units.size()));
}

// The ONE way a Java `String` reaches a domain string, and the exact mirror of
// the function above.
//
// `GetStringUTFChars` is deliberately NOT used: it returns MODIFIED UTF-8, in
// which a supplementary character (an emoji a user can type into Rename) is two
// 3-byte surrogate encodings rather than one 4-byte sequence. That is not
// well-formed UTF-8, so `sanitizeImportedMeshName` would drop it as malformed
// and the character would silently vanish. Reading the UTF-16 units and
// encoding them ourselves is what makes the round trip exact.
std::string readJavaString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return std::string();
    }
    const jsize length = env->GetStringLength(value);
    if (length <= 0) {
        return std::string();
    }
    const jchar* units = env->GetStringCritical(value, nullptr);
    if (units == nullptr) {
        return std::string();
    }
    std::string utf8 = forgeshape::utf16ToUtf8(reinterpret_cast<const uint16_t*>(units),
                                               static_cast<size_t>(length));
    env->ReleaseStringCritical(value, units);
    return utf8;
}

using forgeshape::Renderer;

struct ViewportThread {
    std::thread thread;
    std::mutex mutex;
    std::condition_variable toRender;   // signalled when the render thread has work
    std::condition_variable toCaller;   // signalled when the render thread acked

    bool running = false;
    bool quitRequested = false;

    ANativeWindow* pendingWindow = nullptr;  // owned reference awaiting attach
    bool attachRequested = false;
    bool detachRequested = false;
    bool detachAcked = false;
    bool resizeRequested = false;
    int pendingWidth = 0;
    int pendingHeight = 0;

    bool rendererFailed = false;

    // DEBUG-only device-loss injection, carried across the thread boundary like
    // every other request: the UI thread raises it, the render thread consumes
    // it on its next pass. See debugInjectDeviceLoss.
    bool injectDeviceLossRequested = false;
};

ViewportThread g_viewport;

// Where the renderer stands, mirrored out of the render thread so the UI thread
// can read it without waiting on a thread that may be mid-rebuild.
//
// 0 Healthy, 1 Recovering, 2 RestartRequired -- the wire form of
// forgeshape::RendererLifecycle, kept as an integer because that is what
// crosses JNI. Written only by the render thread, read by anyone.
std::atomic<int> g_rendererLifecycle{0};

// How many device rebuilds have COMPLETED, mirrored out of the render thread
// beside the lifecycle. Debug introspection: it is what lets a test say "the
// device was rebuilt" rather than only "the renderer is healthy again", which a
// renderer that never noticed the loss would also report.
std::atomic<int> g_rendererDeviceRebuilds{0};

// The selection outline's bounded diagnostics (`SEL-OUT-R1` §12/§13), mirrored
// out of the render thread beside the two above and for the same reason.
//
// They are what lets a test ASSERT the two performance promises rather than
// infer them: that a selection switch allocates no GPU resource
// (g_outlineMaskAllocations does not move) and that turning the outline off
// costs nothing at all (g_outlineCompositeDraws stops rising). They carry no
// ObjectId, no geometry and no dimension — a count, an extent and a width.
std::atomic<long long> g_outlineMaskAllocations{0};
std::atomic<long long> g_outlineMaskPassFrames{0};
std::atomic<long long> g_outlineCompositeDraws{0};
std::atomic<int> g_outlineMaskWidth{0};
std::atomic<int> g_outlineMaskHeight{0};
// Scaled by 100 and carried as an integer so the whole mirror is lock-free
// integers; the JNI accessor divides it back. A band is never wider than
// kSelectionOutlineMaxPixels, so this cannot overflow.
std::atomic<int> g_outlineWidthPixelsCentis{0};

// ---------------------------------------------------------------------------
// Camera and selection ownership.
//
// Both controllers outlive every Surface: they are only reset when the process
// dies, so the camera pose AND the selected object id survive home/resume and
// swapchain recreation. Gesture tracking (camera anchors, tap candidacy), by
// contrast, is dropped whenever the Surface goes away.
//
// One mutex guards both, because a tap resolves a pick against the camera
// snapshot and then updates the selection: those two steps must see a
// consistent state, and the render thread must not observe a torn one.
// ---------------------------------------------------------------------------
forgeshape::CameraController g_camera;
forgeshape::SelectionController g_selection;
std::mutex g_stateMutex;

// One-shot log tokens, so a live gesture never spams logcat.
bool g_loggedOrbit = false;
bool g_loggedPan = false;
bool g_loggedZoom = false;

// -------------------------------------------------------------------------
// Sculpt gesture arbitration
// -------------------------------------------------------------------------
//
// A one-finger Down on the Frozen Sculpt Mesh is ambiguous when it arrives: the
// start of a brush stroke, or the first of two fingers of a pan/pinch. Starting
// the stroke on Down would let a two-finger gesture commit a stroke, so the
// rule is PENDING-then-promote, decided entirely in native code:
//
//   Down, one finger, hits the mesh   -> PENDING. Swallowed: nothing deformed,
//                                        no stroke exists, and neither the
//                                        camera nor the selection sees it.
//   Move, still one finger, travelled
//   at least kStrokeArmPixels         -> PROMOTE. The stroke begins at the
//                                        ORIGINAL down point, so the anchor,
//                                        the hit and the affected set are what
//                                        the Down would have captured.
//   Anything else (a second finger,
//   an Up, a Cancel, a multi-pointer
//   event)                            -> ABANDON. No stroke ever existed, so
//                                        there is nothing to end or undo, and
//                                        the gesture is ordinary navigation
//                                        from that event onward.
//
// The camera re-anchors on any pointer-set change, so handing it a gesture
// mid-flight produces no jump; the selection never saw a Down, so an abandoned
// pending gesture cannot resolve a tap either.
//
// Both flags are guarded by g_stateMutex, like the camera and the selection,
// because one touch event decides between brushing and navigating and the two
// must not disagree. They are gesture routing only -- mode, tool and mesh live
// in the SculptSession -- and are dropped whenever the Surface goes away.

// How far a pending finger must travel before it is committed to a stroke.
// Deliberately well below the 24 px tap slop (so a stroke still starts long
// before the gesture would stop being a tap) and well above touch jitter (so a
// finger resting on the mesh while the second one lands does not commit).
constexpr float kStrokeArmPixels = 8.0f;

bool g_grabbing = false;        // a stroke owns the gesture
bool g_strokePending = false;   // one finger is down ON the mesh, undecided
float g_pendingDownX = 0.0f;    // where it went down; the stroke will anchor here
float g_pendingDownY = 0.0f;

// android.view.MotionEvent action constants, kept here so no Android input
// semantics leak into the camera module.
constexpr jint kActionDown = 0;
constexpr jint kActionUp = 1;
constexpr jint kActionMove = 2;
constexpr jint kActionCancel = 3;
constexpr jint kActionPointerDown = 5;
constexpr jint kActionPointerUp = 6;

// The view a sketch borrows, and how it is given back (`CAD-R0-A1A2`).
//
// A sketch is drawn through an orthographic view straight down its plane's
// normal. That view is PRESENTATION: the sketch stores no camera, and the
// user's own viewpoint -- pose, projection and orthographic span -- is kept
// here for exactly as long as the sketch is open and restored the moment it
// ends, whether by commit or by cancel. Guarded by g_stateMutex like the
// camera it describes.
forgeshape::CameraController::Pose g_sketchSavedPose;
bool g_sketchPoseSaved = false;

// Spatial support-chooser gesture state: a single-finger TAP selects the target
// under it, a single-finger DRAG orbits, two fingers navigate.
int32_t g_chooserPointerId = -1;
float g_chooserDownX = 0.0f;
float g_chooserDownY = 0.0f;
bool g_chooserTravelled = false;
constexpr float kChooserTapSlopPixels = 24.0f;

// Frames the camera EXACTLY along the active sketch's authoring frame -- no
// pitch-clamp approximation, and correct for a face frame at any orientation
// (`CAD-A3` B3). Reads the frame the session already installed, so a world-plane
// and a face sketch take the same path.
void beginSketchView() {
    if (!g_sketchPoseSaved) {
        g_sketchSavedPose = g_camera.capturePose();
        g_sketchPoseSaved = true;
    }
    const forgeshape::SketchFrame& f = forgeshape::sketchSession().frame();
    g_camera.frameSketchView(f.origin, f.u, f.v, f.n);
}

void endSketchView() {
    if (g_sketchPoseSaved) {
        g_camera.restorePose(g_sketchSavedPose);
        g_sketchPoseSaved = false;
    }
}

// Leaves the exact sketch view for the one the staged extrusion can be
// adjusted through (`CAD-UX-S1-C1`).
//
// The sketch's view and the extrusion's are two presentations of one authored
// truth, and Finish Sketch is where the second begins: the first is aimed
// EXACTLY along the support normal, which is the extrusion axis, so from it
// the arrow has no screen extent and no axial drag can be resolved
// (`OQ-CAD-UX-01`). The policy itself is platform-neutral arithmetic in
// `cadFeatureViewPose`; this is the adapter that reads the camera, hands it
// over and installs the answer.
//
// `g_sketchSavedPose` is deliberately NOT consumed here. It is the view the
// user had BEFORE the sketch, and it stays the view a cancel or a commit
// gives back; the preview is a view the sketch borrows on top, exactly as the
// aligned one was. Nothing installed here is project truth: no `CadBodyState`,
// no `.forge` byte, no checkpoint, no fingerprint and no history step moves.
// The caller holds g_stateMutex.
forgeshape::CadFeatureViewSource beginExtrudeFeatureView() {
    forgeshape::CadExtrudeAnchors anchors;
    if (!forgeshape::sketchSession().extrudeAnchors(&anchors)) {
        return forgeshape::CadFeatureViewSource::Unavailable;
    }
    forgeshape::CameraController::Pose preview;
    const forgeshape::CadFeatureViewSource source = forgeshape::cadFeatureViewPose(
        g_camera.capturePose(), g_sketchPoseSaved ? &g_sketchSavedPose : nullptr,
        forgeshape::sketchSession().frame(), anchors, &preview);
    if (source != forgeshape::CadFeatureViewSource::Unavailable) {
        // restorePose is the one clamped installer, and it leaves the sketch
        // view: from here the camera is an ordinary 3D one and orbit, pan and
        // pinch mean what they mean everywhere else.
        g_camera.restorePose(preview);
    }
    return source;
}

// The last touch event's platform-neutral pointer data, kept for the DEBUG-ONLY
// read-back hook further down. It is a diagnostic mirror and never a source of
// truth: nothing in the product reads it, and it does not exist at all in a
// release build. Guarded by g_stateMutex, like everything else the touch path
// writes.
#ifndef NDEBUG
forgeshape::TouchPointer g_lastPointers[forgeshape::kMaxTrackedPointers];
int g_lastPointerCount = -1;
#endif

bool translateAction(jint androidAction, forgeshape::TouchAction* out) {
    switch (androidAction) {
        case kActionDown:        *out = forgeshape::TouchAction::Down; return true;
        case kActionUp:          *out = forgeshape::TouchAction::Up; return true;
        case kActionMove:        *out = forgeshape::TouchAction::Move; return true;
        case kActionCancel:      *out = forgeshape::TouchAction::Cancel; return true;
        case kActionPointerDown: *out = forgeshape::TouchAction::PointerDown; return true;
        case kActionPointerUp:   *out = forgeshape::TouchAction::PointerUp; return true;
        default: return false;  // hover/scroll/button events are not navigation
    }
}

// Copies at most `count` entries out of an OPTIONAL Java array.
//
// Optional is the whole point: a caller with nothing to say about tilt passes
// null, and the pointer keeps its documented default rather than the event being
// refused. A null, short or unreadable array is a missing value, not an error --
// a stylus angle is never worth dropping a gesture over.
bool readOptionalFloatRegion(JNIEnv* env, jfloatArray array, int count, jfloat* out) {
    if (array == nullptr || count <= 0) {
        return false;
    }
    if (env->GetArrayLength(array) < count) {
        return false;
    }
    env->GetFloatArrayRegion(array, 0, count, out);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    return true;
}

bool readOptionalIntRegion(JNIEnv* env, jintArray array, int count, jint* out) {
    if (array == nullptr || count <= 0) {
        return false;
    }
    if (env->GetArrayLength(array) < count) {
        return false;
    }
    env->GetIntArrayRegion(array, 0, count, out);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    return true;
}

void runCameraSelfTestsAndLog() {
#ifndef NDEBUG
    forgeshape::CameraSelfTestResult results[128];
    const int count = forgeshape::runCameraSelfTests(results, 128);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_CAMERA_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("camera selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_CAMERA_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_CAMERA_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runPickingSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxPickingChecks = 256;
    static forgeshape::PickingSelfTestResult results[kMaxPickingChecks];
    const int count = forgeshape::runPickingSelfTests(results, kMaxPickingChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_PICKING_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("picking selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_PICKING_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_PICKING_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runSceneSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxSceneChecks = 256;
    static forgeshape::SceneSelfTestResult results[kMaxSceneChecks];
    const int count = forgeshape::runSceneSelfTests(results, kMaxSceneChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_SCENE_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("scene selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_SCENE_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_SCENE_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runHistorySelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxHistoryChecks = 256;
    static forgeshape::HistorySelfTestResult results[kMaxHistoryChecks];
    const int count = forgeshape::runHistorySelfTests(results, kMaxHistoryChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("construction history selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_FAIL (%d of %d checks failed)", failed,
                count);
    }
#endif
}



void runBodyDimensionsSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxBodyDimensionChecks = 256;
    static forgeshape::BodyDimensionsSelfTestResult results[kMaxBodyDimensionChecks];
    const int count = forgeshape::runBodyDimensionsSelfTests(results, kMaxBodyDimensionChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_BODY_DIMENSIONS_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("body dimensions selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_BODY_DIMENSIONS_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_BODY_DIMENSIONS_SELFTEST_FAIL (%d of %d checks failed)", failed,
                count);
    }
#endif
}

void runMirrorSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxMirrorChecks = 128;
    static forgeshape::MirrorSelfTestResult results[kMaxMirrorChecks];
    const int count = forgeshape::runMirrorSelfTests(results, kMaxMirrorChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_MIRROR_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("mirror selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_MIRROR_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_MIRROR_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runGltfExportSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxGltfChecks = 128;
    static forgeshape::GltfExportSelfTestResult results[kMaxGltfChecks];
    const int count = forgeshape::runGltfExportSelfTests(results, kMaxGltfChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_GLTF_EXPORT_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("gltf export selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_GLTF_EXPORT_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_GLTF_EXPORT_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runCadSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxCadChecks = 256;
    static forgeshape::CadSelfTestResult results[kMaxCadChecks];
    const int count = forgeshape::runCadSelfTests(results, kMaxCadChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_CAD_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("cad selftest pass: %s", results[i].name);
        }
    }
    // The bounded measurements, printed whether the suite passed or not, so
    // a regression in extraction, triangulation or regeneration time is a
    // number in the log rather than a feeling.
    FS_LOGI("FORGESHAPE_CAD_PERFORMANCE %s", forgeshape::cadPerformanceReport());
    // The six `CADB` v4 golden digests as this build encodes them
    // (`CAD-EXT-R1`), on the project suite's own terms: drift from the
    // committed corpus is then a value a human can READ out of logcat and
    // reconcile with DATA_PACKAGE_SPEC.md, rather than only a failed assertion.
    FS_LOGI("FORGESHAPE_CAD_GOLDEN_SHA256_V4 cad_symmetric=%s cad_two_sides=%s "
            "cad_face_extent=%s mixed_cad_extent=%s cad_bad_extent=%s cad_bad_two_sides=%s",
            forgeshape::cadSymmetricFixtureSha256(), forgeshape::cadTwoSidesFixtureSha256(),
            forgeshape::cadFaceExtentFixtureSha256(), forgeshape::cadMixedExtentFixtureSha256(),
            forgeshape::cadBadExtentFixtureSha256(),
            forgeshape::cadBadTwoSidesFixtureSha256());
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_CAD_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_CAD_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }

    // CAD-A3: semantic faces, TopoRef, the dependency graph, CADB v2, the exact
    // sketch camera, the adaptive grid and the spatial support picking. Its own
    // suite, building its own scenes/histories/camera.
    constexpr int kMaxA3Checks = 128;
    static forgeshape::CadA3SelfTestResult a3[kMaxA3Checks];
    const int a3Count = forgeshape::runCadA3SelfTests(a3, kMaxA3Checks);
    int a3Failed = 0;
    for (int i = 0; i < a3Count; ++i) {
        if (!a3[i].passed) {
            ++a3Failed;
            FS_LOGE("FORGESHAPE_CAD_A3_SELFTEST_CASE_FAIL:%s", a3[i].name);
        }
    }
    FS_LOGI("FORGESHAPE_CAD_A3_PERFORMANCE %s", forgeshape::cadA3PerformanceReport());
    if (a3Failed == 0) {
        FS_LOGI("FORGESHAPE_CAD_A3_SELFTEST_OK (%d checks)", a3Count);
    } else {
        FS_LOGE("FORGESHAPE_CAD_A3_SELFTEST_FAIL (%d of %d checks failed)", a3Failed, a3Count);
    }

    // SKETCH-UX-R1: the curve domain, the exact line-length edit, the
    // orientation navigator's presentation state, support-plane switching, the
    // staged Edit Sketch session and the CADB v3 round trip. Its own suite,
    // building its own sketches, scenes, histories and documents.
    // CAD-UX-S1 widened this suite past 128. The recorder silently DROPS checks
    // past its ceiling, which would read as a smaller passing suite rather than
    // as a failure, so this stays well ahead of it.
    constexpr int kMaxSketchUxChecks = 256;
    static forgeshape::SketchUxSelfTestResult ux[kMaxSketchUxChecks];
    const int uxCount = forgeshape::runSketchUxSelfTests(ux, kMaxSketchUxChecks);
    int uxFailed = 0;
    for (int i = 0; i < uxCount; ++i) {
        if (!ux[i].passed) {
            ++uxFailed;
            FS_LOGE("FORGESHAPE_SKETCH_UX_SELFTEST_CASE_FAIL:%s", ux[i].name);
        }
    }
    FS_LOGI("FORGESHAPE_SKETCH_UX_PERFORMANCE %s", forgeshape::sketchUxPerformanceReport());
    if (uxFailed == 0) {
        FS_LOGI("FORGESHAPE_SKETCH_UX_SELFTEST_OK (%d checks)", uxCount);
    } else {
        FS_LOGE("FORGESHAPE_SKETCH_UX_SELFTEST_FAIL (%d of %d checks failed)", uxFailed, uxCount);
    }
#endif
}

void runGltfImportSelfTestsAndLog() {
#ifndef NDEBUG
    // R1 added the external-GLB subset, so the suite outgrew 128. The recorder
    // silently drops checks past its ceiling, which would look like a smaller
    // passing suite rather than a failure, so this stays well ahead of it.
    constexpr int kMaxImportChecks = 256;
    static forgeshape::GltfImportSelfTestResult results[kMaxImportChecks];
    const int count = forgeshape::runGltfImportSelfTests(results, kMaxImportChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_GLTF_IMPORT_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("gltf import selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_GLTF_IMPORT_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_GLTF_IMPORT_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runRenderRecoverySelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxRecoveryChecks = 128;
    static forgeshape::RenderRecoverySelfTestResult results[kMaxRecoveryChecks];
    const int count = forgeshape::runRenderRecoverySelfTests(results, kMaxRecoveryChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_RENDER_RECOVERY_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("render recovery selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_RENDER_RECOVERY_SELFTEST_FAIL (%d of %d checks failed)", failed,
                count);
    }
#endif
}

void runProjectSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxProjectChecks = 256;
    static forgeshape::ProjectSelfTestResult results[kMaxProjectChecks];
    const int count = forgeshape::runProjectSelfTests(results, kMaxProjectChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_PROJECT_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("project selftest pass: %s", results[i].name);
        }
    }
    // The digests the golden-corpus case compared against, printed whether it
    // passed or failed: a drift between the committed fixtures and the encoder
    // is then a value a human can read out of logcat and reconcile with
    // DATA_PACKAGE_SPEC.md, rather than only a failed assertion.
    FS_LOGI("FORGESHAPE_PROJECT_GOLDEN_SHA256 construction=%s sculpt=%s",
            forgeshape::canonicalConstructionFixtureSha256(),
            forgeshape::canonicalSculptFixtureSha256());
    FS_LOGI("FORGESHAPE_PROJECT_GOLDEN_SHA256_IMPORTED imported_only=%s construction_imported=%s "
            "mixed_imported=%s",
            forgeshape::canonicalImportedOnlyFixtureSha256(),
            forgeshape::canonicalConstructionImportedFixtureSha256(),
            forgeshape::canonicalMixedImportedFixtureSha256());
    FS_LOGI("FORGESHAPE_PROJECT_GOLDEN_SHA256_IMPORTED_SCULPT imported_sculpt=%s "
            "mixed_imported_sculpt=%s",
            forgeshape::canonicalImportedSculptFixtureSha256(),
            forgeshape::canonicalMixedImportedSculptFixtureSha256());
    FS_LOGI("FORGESHAPE_PROJECT_GOLDEN_SHA256_CAD cad_rectangle=%s cad_circle=%s mixed_cad=%s "
            "cad_bad_plane=%s",
            forgeshape::canonicalCadRectangleFixtureSha256(),
            forgeshape::canonicalCadCircleFixtureSha256(),
            forgeshape::canonicalMixedCadFixtureSha256(),
            forgeshape::canonicalCadBadPlaneFixtureSha256());
    FS_LOGI("FORGESHAPE_PROJECT_GOLDEN_SHA256_CAD_V2 cad_face_sketch_cap=%s "
            "cad_face_sketch_side=%s cad_face_chain=%s mixed_cad_face=%s cad_bad_face_ref=%s "
            "cad_dependency_cycle=%s",
            forgeshape::canonicalCadFaceSketchCapFixtureSha256(),
            forgeshape::canonicalCadFaceSketchSideFixtureSha256(),
            forgeshape::canonicalCadFaceChainFixtureSha256(),
            forgeshape::canonicalMixedCadFaceFixtureSha256(),
            forgeshape::canonicalCadBadFaceRefFixtureSha256(),
            forgeshape::canonicalCadDependencyCycleFixtureSha256());
    // Stage 018A: the two SCNE v2 fixtures, printed on the same terms as every
    // other golden digest -- so drift from the committed corpus is a value that
    // can be READ rather than only an assertion that failed.
    FS_LOGI("FORGESHAPE_PROJECT_GOLDEN_SHA256_OBJECT_STATE object_state=%s "
            "object_state_bad_flags=%s",
            forgeshape::canonicalObjectStateFixtureSha256(),
            forgeshape::canonicalObjectStateBadFlagsFixtureSha256());
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_PROJECT_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_PROJECT_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runGizmoSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxGizmoChecks = 256;
    static forgeshape::GizmoSelfTestResult results[kMaxGizmoChecks];
    const int count = forgeshape::runGizmoSelfTests(results, kMaxGizmoChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_GIZMO_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("gizmo selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_GIZMO_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_GIZMO_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}
void runMeshSelfTestsAndLog() {
#ifndef NDEBUG
    forgeshape::MeshSelfTestResult results[128];
    const int count = forgeshape::runMeshSelfTests(results, 128);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_DYNAMIC_MESH_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("dynamic mesh selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_DYNAMIC_MESH_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runConstructionSelfTestsAndLog() {
#ifndef NDEBUG
    forgeshape::ConstructionSelfTestResult results[128];
    const int count = forgeshape::runConstructionSelfTests(results, 128);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("construction box selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_FAIL (%d of %d checks failed)", failed,
                count);
    }
#endif
}

void runPrimitiveSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxPrimitiveChecks = 256;
    static forgeshape::PrimitiveSelfTestResult results[kMaxPrimitiveChecks];
    const int count = forgeshape::runPrimitiveSelfTests(results, kMaxPrimitiveChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("construction primitive selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_FAIL (%d of %d checks failed)", failed,
                count);
    }
#endif
}

void runSphereSelfTestsAndLog() {
#ifndef NDEBUG
    forgeshape::SphereSelfTestResult results[192];
    const int count = forgeshape::runSphereSelfTests(results, 192);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("construction sphere selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_FAIL (%d of %d checks failed)", failed,
                count);
    }
#endif
}

void runConeCapsuleSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxConeCapsuleChecks = 256;
    static forgeshape::ConeCapsuleSelfTestResult results[kMaxConeCapsuleChecks];
    const int count = forgeshape::runConeCapsuleSelfTests(results, kMaxConeCapsuleChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_CONE_CAPSULE_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("cone capsule selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_CONE_CAPSULE_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_CONE_CAPSULE_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runSculptSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxSculptChecks = 768;
    static forgeshape::SculptSelfTestResult results[kMaxSculptChecks];
    const int count = forgeshape::runSculptSelfTests(results, kMaxSculptChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("sculpt brush kernel selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_FAIL (%d of %d checks failed)", failed,
                count);
    }
#endif
}

void runRenderMeshSelfTestsAndLog() {
#ifndef NDEBUG
    constexpr int kMaxRenderMeshChecks = 512;
    static forgeshape::RenderMeshSelfTestResult results[kMaxRenderMeshChecks];
    const int count = forgeshape::runRenderMeshSelfTests(results, kMaxRenderMeshChecks);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_RENDER_SHADING_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("render shading selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_RENDER_SHADING_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_RENDER_SHADING_SELFTEST_FAIL (%d of %d checks failed)", failed, count);
    }
#endif
}

void runTransformSelfTestsAndLog() {
#ifndef NDEBUG
    forgeshape::TransformSelfTestResult results[192];
    const int count = forgeshape::runTransformSelfTests(results, 192);
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            FS_LOGE("FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_CASE_FAIL:%s", results[i].name);
        } else {
            FS_LOGI("construction transform selftest pass: %s", results[i].name);
        }
    }
    if (failed == 0) {
        FS_LOGI("FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK (%d checks)", count);
    } else {
        FS_LOGE("FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_FAIL (%d of %d checks failed)", failed,
                count);
    }
#endif
}

// ---------------------------------------------------------------------------
// Runtime mesh publication.
//
// Publishing is a CPU-side act: it never touches Vulkan. The render thread
// notices the new revision on its next frame and does the GPU work there.
// ---------------------------------------------------------------------------

// Renders a primitive's authoritative parameters into a log fragment. Each
// primitive is named by its OWN parameters, never by a generic triple, so a log
// line can never be misread as the wrong shape. The spec carries only the active
// primitive's payload, so there is nothing here to pick the wrong branch of.
void describeSpec(const forgeshape::PrimitiveSpec& spec, char* out, size_t size) {
    if (const forgeshape::BoxDimensionsMeters* box = spec.box()) {
        snprintf(out, size, "box w=%.6fm h=%.6fm d=%.6fm", box->width, box->height, box->depth);
    } else if (const forgeshape::CylinderDimensionsMeters* cylinder = spec.cylinder()) {
        snprintf(out, size, "cylinder dia=%.6fm h=%.6fm", cylinder->diameter, cylinder->height);
    } else if (const forgeshape::SphereDimensionsMeters* sphere = spec.sphere()) {
        snprintf(out, size, "sphere dia=%.6fm", sphere->diameter);
    } else if (const forgeshape::ConeDimensionsMeters* cone = spec.cone()) {
        snprintf(out, size, "cone bottomDia=%.6fm h=%.6fm", cone->bottomDiameter, cone->height);
    } else if (const forgeshape::CapsuleDimensionsMeters* capsule = spec.capsule()) {
        snprintf(out, size, "capsule dia=%.6fm totalH=%.6fm", capsule->diameter,
                 capsule->totalHeight);
    } else if (const forgeshape::PlaneDimensionsMeters* plane = spec.plane()) {
        snprintf(out, size, "plane w=%.6fm d=%.6fm", plane->width, plane->depth);
    } else {
        snprintf(out, size, "unknown");
    }
}

// Product geometry publication.
//
// The active Construction object's authoritative double-meter parameters are the
// source; the RuntimeMesh is generated from them in LOCAL space and published
// through the same MeshStore path everything else uses. Parameters are never
// read back out of the mesh, so this direction is the only one that exists.
forgeshape::MeshRevision publishConstructionObject(const char* reason) {
    if (!forgeshape::constructionScene().hasProject()) {
        // No project is open (Home, or the CAD bootstrap before its first
        // commit): there is no body to publish and nothing on screen to
        // refresh. Not a failure -- an empty scene draws as empty.
        return forgeshape::kNoMeshRevision;
    }
    forgeshape::ConstructionObject* source = forgeshape::activeConstructionOrNull();
    if (source == nullptr) {
        // The active body is an Imported Mesh: it has no parameters to
        // regenerate from, and its geometry is already published. Dispatched
        // through the one representation-aware entry point rather than given a
        // second publish path here.
        forgeshape::MeshValidation why = forgeshape::MeshValidation::Ok;
        const forgeshape::MeshRevision revision = forgeshape::publishSceneObject(
            forgeshape::constructionScene().activeBody(), &why);
        if (revision == forgeshape::kNoMeshRevision) {
            FS_LOGE("FORGESHAPE_CONSTRUCTION_PUBLISH_FAIL:%s:%s", reason,
                    forgeshape::meshValidationName(why));
        }
        return revision;
    }
    forgeshape::ConstructionObject& object = *source;
    forgeshape::MeshValidation why = forgeshape::MeshValidation::Ok;
    const forgeshape::MeshRevision revision =
        forgeshape::publishConstructionObject(object, forgeshape::meshStore(), &why);
    if (revision == forgeshape::kNoMeshRevision) {
        FS_LOGE("FORGESHAPE_CONSTRUCTION_PUBLISH_FAIL:%s:%s", reason,
                forgeshape::meshValidationName(why));
        return revision;
    }
    // The counts come from what was just published rather than from a second
    // generateMesh() run for the log line: the store holds exactly the vertices
    // and indices the generator produced, one tessellation ago.
    const forgeshape::RuntimeMeshPtr published = forgeshape::meshStore().current();
    const uint32_t vertexCount = published ? published->vertexCount() : 0u;
    const uint32_t indexCount = published ? published->indexCount() : 0u;
    char described[128];
    describeSpec(object.spec(), described, sizeof(described));
    FS_LOGI("FORGESHAPE_CONSTRUCTION_PUBLISHED:%llu:%u:%u kind=%s %s objectId=%llu reason=%s",
            (unsigned long long)revision, vertexCount, indexCount,
            forgeshape::primitiveKindName(object.kind()), described,
            (unsigned long long)object.objectId(), reason);
    return revision;
}

// Publishes the CURRENT Frozen Sculpt Mesh through the same MeshStore path
// everything else uses.
//
// This is the whole of the sculpt render/pick integration: the renderer already
// draws the store's current revision and picking already reads it, so making the
// sculpt mesh the active representation is a publication, not a renderer change.
// The renderer therefore still owns no sculpt truth.
//
// Takes the session rather than fetching it: `sculptSession()` rebinds the
// borrowed target and is only called under g_stateMutex, which some callers
// hold across this call and others release first.
forgeshape::MeshRevision publishSculptRepresentation(forgeshape::SculptSession& session,
                                                     const char* reason) {
    forgeshape::MeshValidation why = forgeshape::MeshValidation::Ok;
    const forgeshape::MeshRevision revision =
        forgeshape::publishSculptMesh(session.mesh(), forgeshape::meshStore(), &why);
    if (revision == forgeshape::kNoMeshRevision) {
        FS_LOGE("FORGESHAPE_SCULPT_PUBLISH_FAIL:%s:%s", reason,
                forgeshape::meshValidationName(why));
    }
    return revision;
}

// Publishes whichever representation the product mode says is active.
//
// Construction mode publishes the generated Construction mesh; Sculpt mode
// publishes the Frozen Sculpt Mesh. There is exactly one place that makes this
// choice, so the renderer and the picker can never end up looking at different
// representations.
forgeshape::MeshRevision publishActiveRepresentation(const char* reason) {
    forgeshape::SculptSession* session = nullptr;
    bool sculpting = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        session = &forgeshape::sculptSession();
        sculpting = session->inSculptMode();
    }
    if (sculpting) {
        return publishSculptRepresentation(*session, reason);
    }
    return publishConstructionObject(reason);
}

// Applies an authoritative primitive through the one Construction entry point
// and reports what happened.
//
// The update-then-publish rule is NOT restated here: forgeshape::applyPrimitive
// owns it, and this function only turns the result into log lines. Every
// caller — the product UI for both Box and Cylinder, and the debug driver — goes
// through it, so there is exactly one place where a shape change becomes a mesh
// revision.
forgeshape::PrimitiveApplyResult applyPrimitive(const char* label,
                                                const forgeshape::PrimitiveSpec& requested) {
    forgeshape::PrimitiveApplyResult result;
    // Same lock as applyBoxTransform, and for the same reason it has a second
    // reader: the Construction parameters, the remembered sets and the
    // stale-source flag written below are what the autosave worker reads —
    // under this lock — for `projectFingerprint()` and `encodeProject()`. A
    // shape Apply that wrote them unlocked could hand a checkpoint half of one
    // primitive and half of another. Held across the mesh generation exactly as
    // runHistoryStep and loadProject already hold it across theirs; the domain
    // never takes this mutex, so nothing below can re-enter it.
    std::lock_guard<std::mutex> lock(g_stateMutex);
    {
        // One user Apply is ONE history step, whatever it changes underneath —
        // a kind, several parameters, or both. And it is NO step at all when the
        // request is refused or identical, because the commit compares the
        // Construction state on either side rather than trusting that a call was
        // made. Where a composite act (creation) has already opened an edit,
        // this scope joins it instead of opening a second one.
        forgeshape::ScopedConstructionEdit edit(forgeshape::constructionHistory());
        result = forgeshape::applyConstructionPrimitive(requested);
    }
    const forgeshape::ConstructionObject* source = forgeshape::activeConstructionOrNull();
    if (source == nullptr) {
        // The active body is an Imported Mesh -- or no project is open at all
        // -- so the entry point above refused and there is no Construction
        // Source to report counts from. Logged by name rather than left silent:
        // a refusal nobody can see is how a missing UI guard stays invisible.
        FS_LOGE("FORGESHAPE_CONSTRUCTION_PRIMITIVE_REJECTED:%s:NoConstructionSource "
                "representation=%s",
                label,
                forgeshape::constructionScene().hasProject()
                    ? forgeshape::bodyRepresentationName(
                          forgeshape::constructionScene().activeBody().representation())
                    : "NoProject");
        return result;
    }
    const forgeshape::ConstructionObject& object = *source;

    char described[128];
    describeSpec(result.spec, described, sizeof(described));
    char requestedText[128];
    describeSpec(requested, requestedText, sizeof(requestedText));

    switch (result.status) {
        case forgeshape::PrimitiveUpdateStatus::Applied:
            FS_LOGI("FORGESHAPE_CONSTRUCTION_PRIMITIVE:%s kind=%s %s updates=%llu",
                    label, forgeshape::primitiveKindName(result.spec.kind()), described,
                    (unsigned long long)object.updateCount());
            if (result.published) {
                FS_LOGI("FORGESHAPE_CONSTRUCTION_PUBLISHED:%llu:%u:%u kind=%s %s objectId=%llu "
                        "reason=%s",
                        (unsigned long long)result.revision, result.vertexCount, result.indexCount,
                        forgeshape::primitiveKindName(result.spec.kind()), described,
                        (unsigned long long)object.objectId(), label);
            } else {
                FS_LOGE("FORGESHAPE_CONSTRUCTION_PUBLISH_FAIL:%s:%s", label,
                        forgeshape::meshValidationName(result.meshValidation));
            }
            // Stale-source policy. A Construction change while a Frozen Sculpt
            // Mesh exists NEVER re-derives or replaces that mesh: it is marked
            // as having been frozen from an older source, and adopting the new
            // source stays an explicit user act (another Freeze). There is no
            // automatic sculpt-edit transfer.
            if (forgeshape::sculptSession().hasSculptMesh()) {
                forgeshape::sculptSession().markSourceStale();
                FS_LOGI("FORGESHAPE_SCULPT_SOURCE_STALE:%s sculptRev=%llu", label,
                        (unsigned long long)forgeshape::sculptSession().mesh().revision());
            }
            // In Sculpt mode the sculpt mesh is the active representation, so
            // the revision the Construction apply just published must not be
            // left on screen. This restores the sculpt geometry immediately;
            // the Construction Source keeps its new parameters regardless.
            if (forgeshape::sculptSession().inSculptMode()) {
                publishSculptRepresentation(forgeshape::sculptSession(),
                                            "construction_changed_in_sculpt_mode");
            }
            break;
        case forgeshape::PrimitiveUpdateStatus::Unchanged:
            // Deliberately publishes nothing: an identical shape must not cost a
            // mesh revision or a GPU upload.
            FS_LOGI("FORGESHAPE_CONSTRUCTION_PRIMITIVE_UNCHANGED:%s kind=%s %s rev=%llu",
                    label, forgeshape::primitiveKindName(result.spec.kind()), described,
                    (unsigned long long)result.revision);
            break;
        case forgeshape::PrimitiveUpdateStatus::Rejected:
            // Fails closed: previous kind, parameters, transform and mesh
            // revision are all untouched.
            FS_LOGE("FORGESHAPE_CONSTRUCTION_PRIMITIVE_REJECTED:%s:%s requested=(%s) retained "
                    "kind=%s %s rev=%llu rejects=%llu",
                    label, forgeshape::dimensionValidationName(result.validation), requestedText,
                    forgeshape::primitiveKindName(result.spec.kind()), described,
                    (unsigned long long)result.revision,
                    (unsigned long long)object.rejectedUpdateCount());
            break;
    }
    return result;
}

// Status codes handed back to the Android UI. They are a JNI transport detail:
// the domain's own vocabulary is PrimitiveUpdateStatus + DimensionValidation, and
// these constants must stay in step with NativeViewport's APPLY_* fields.
constexpr jint kApplyApplied = 0;
constexpr jint kApplyUnchanged = 1;
constexpr jint kApplyRejectedNotFinite = 2;
constexpr jint kApplyRejectedNotPositive = 3;
constexpr jint kApplyRejectedNotRepresentable = 4;
constexpr jint kApplyPublishFailed = 5;
// Every value is a length, but the primitive's own rule relating two of them is
// broken — today only a capsule whose total height is under its diameter.
constexpr jint kApplyRejectedRelation = 6;
// Stage 018A: the body is LOCKED. Its own code because a lock is not a
// statement about a value -- every number the caller passed may be good -- and a
// status line that blamed a coordinate would be describing the wrong problem.
constexpr jint kApplyRejectedLocked = 8;

// Applies an authoritative transform through the one Construction transform
// entry point and reports what happened.
//
// It publishes nothing and uploads nothing, by construction: the entry point has
// no access to the mesh store. The box's RuntimeMesh is its LOCAL geometry, so
// moving or rotating it changes only the derived matrix the renderer and the
// picker read.
forgeshape::TransformApplyResult applyBoxTransform(const char* label,
                                                   const forgeshape::TransformValues& requested,
                                                   bool* outRefusedLocked = nullptr) {
    forgeshape::TransformApplyResult result;
    if (outRefusedLocked != nullptr) {
        *outRefusedLocked = false;
    }
    {
        // Same lock as the camera and the selection: the render thread reads the
        // derived model matrix under it once per frame, and a tap resolves its
        // pick against it, so neither can observe a torn transform.
        std::lock_guard<std::mutex> lock(g_stateMutex);
        // One Apply of the six values is one history step, and the six move
        // together: an undo can never put X back without Y and Z, because the
        // step is the placement rather than a field. Declared after the lock so
        // the commit — which reads the scene — happens before the lock is
        // released.
        if (!forgeshape::constructionScene().hasProject()) {
            // No project, no placement to write. The unbound identity the
            // global answers with belongs to no body and must not be edited.
            FS_LOGE("FORGESHAPE_CONSTRUCTION_TRANSFORM_REJECTED:%s:NoProject", label);
            result.status = forgeshape::TransformUpdateStatus::Rejected;
            result.values = forgeshape::constructionTransform().values();
            return result;
        }
        if (forgeshape::constructionScene().activeBody().locked()) {
            // Stage 018A. The FIRST of lock's two guards, and the one that
            // covers every exact-value route into a placement: the precision
            // surface, the unit chips and anything else that types a number.
            // The second is `setGizmoActive`, which covers direct manipulation.
            //
            // Rejected rather than clamped or ignored: the body keeps the
            // placement it has, nothing is published and no history step is
            // opened, so a refused write costs the project nothing. The control
            // is withdrawn above JNI as well -- removing a control is not
            // removing a guard.
            FS_LOGE("FORGESHAPE_CONSTRUCTION_TRANSFORM_REJECTED:%s:BodyLocked", label);
            // Reported through its own out-parameter rather than through
            // `TransformValidation`, because a lock is not a statement about a
            // VALUE: every number the caller passed may be perfectly good. The
            // JNI wrapper turns this into its own APPLY_REJECTED_LOCKED code,
            // so the status line can say what actually happened instead of
            // blaming a coordinate.
            if (outRefusedLocked != nullptr) {
                *outRefusedLocked = true;
            }
            result.status = forgeshape::TransformUpdateStatus::Rejected;
            result.values = forgeshape::constructionTransform().values();
            return result;
        }
        forgeshape::ScopedConstructionEdit edit(forgeshape::constructionHistory());
        result = forgeshape::applyConstructionTransform(requested);
    }
    const forgeshape::TransformValues& v = result.values;
    const forgeshape::ConstructionTransform& transform = forgeshape::constructionTransform();
    switch (result.status) {
        case forgeshape::TransformUpdateStatus::Applied:
            FS_LOGI("FORGESHAPE_CONSTRUCTION_TRANSFORM:%s pos=(%.6f,%.6f,%.6f)m "
                    "rot=(%.6f,%.6f,%.6f)deg scale=(%.6f,%.6f,%.6f) updates=%llu rev=%llu",
                    label, v.positionX, v.positionY, v.positionZ, v.rotationX, v.rotationY,
                    v.rotationZ, v.scaleX, v.scaleY, v.scaleZ,
                    (unsigned long long)transform.updateCount(),
                    (unsigned long long)forgeshape::meshStore().currentRevision());
            break;
        case forgeshape::TransformUpdateStatus::Unchanged:
            FS_LOGI("FORGESHAPE_CONSTRUCTION_TRANSFORM_UNCHANGED:%s pos=(%.6f,%.6f,%.6f)m "
                    "rot=(%.6f,%.6f,%.6f)deg scale=(%.6f,%.6f,%.6f) rev=%llu",
                    label, v.positionX, v.positionY, v.positionZ, v.rotationX, v.rotationY,
                    v.rotationZ, v.scaleX, v.scaleY, v.scaleZ,
                    (unsigned long long)forgeshape::meshStore().currentRevision());
            break;
        case forgeshape::TransformUpdateStatus::Rejected:
            FS_LOGE("FORGESHAPE_CONSTRUCTION_TRANSFORM_REJECTED:%s:%s "
                    "requested=(%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f) "
                    "retained pos=(%.6f,%.6f,%.6f)m rot=(%.6f,%.6f,%.6f)deg "
                    "scale=(%.6f,%.6f,%.6f) rev=%llu rejects=%llu",
                    label, forgeshape::transformValidationName(result.validation),
                    requested.positionX, requested.positionY, requested.positionZ,
                    requested.rotationX, requested.rotationY, requested.rotationZ,
                    requested.scaleX, requested.scaleY, requested.scaleZ, v.positionX,
                    v.positionY, v.positionZ, v.rotationX, v.rotationY, v.rotationZ, v.scaleX,
                    v.scaleY, v.scaleZ,
                    (unsigned long long)forgeshape::meshStore().currentRevision(),
                    (unsigned long long)transform.rejectedUpdateCount());
            break;
    }
    return result;
}

jint applyResultToJni(const forgeshape::PrimitiveApplyResult& result) {
    switch (result.status) {
        case forgeshape::PrimitiveUpdateStatus::Applied:
            return result.published ? kApplyApplied : kApplyPublishFailed;
        case forgeshape::PrimitiveUpdateStatus::Unchanged:
            return kApplyUnchanged;
        case forgeshape::PrimitiveUpdateStatus::Rejected:
            break;
    }
    switch (result.validation) {
        case forgeshape::DimensionValidation::NotFinite: return kApplyRejectedNotFinite;
        case forgeshape::DimensionValidation::NotPositive: return kApplyRejectedNotPositive;
        case forgeshape::DimensionValidation::NotRepresentable:
            return kApplyRejectedNotRepresentable;
        case forgeshape::DimensionValidation::RelationInvalid: return kApplyRejectedRelation;
        case forgeshape::DimensionValidation::Ok: break;
    }
    // Rejected with no dimension to blame. Since `IMPORT-01A` there is one way
    // to reach this: a shape Apply against a body that has no Construction
    // Source. The Android layer cannot get here — the control is withdrawn for
    // an imported body — and the honest reason is already in the log as
    // `FORGESHAPE_CONSTRUCTION_PRIMITIVE_REJECTED:...:NoConstructionSource`, so
    // this stays the generic refusal rather than growing a transport code for a
    // case no user-facing path can produce.
    return kApplyRejectedNotFinite;
}

// The transform reuses the same status vocabulary; "not positive" simply cannot
// occur, because zero and negative are ordinary coordinates and angles.
jint transformResultToJni(const forgeshape::TransformApplyResult& result) {
    switch (result.status) {
        case forgeshape::TransformUpdateStatus::Applied: return kApplyApplied;
        case forgeshape::TransformUpdateStatus::Unchanged: return kApplyUnchanged;
        case forgeshape::TransformUpdateStatus::Rejected: break;
    }
    switch (result.validation) {
        case forgeshape::TransformValidation::NotRepresentable:
            return kApplyRejectedNotRepresentable;
        // A scale at or below the floor. It reuses the dimension code because it
        // means the same thing to a reader — a size that is not a size — even
        // though a coordinate and an angle on this same transform are perfectly
        // free to be zero or negative.
        case forgeshape::TransformValidation::NotPositive:
            return kApplyRejectedNotPositive;
        case forgeshape::TransformValidation::NotFinite:
        case forgeshape::TransformValidation::Ok: break;
    }
    return kApplyRejectedNotFinite;
}

forgeshape::MeshRevision publishFixture(const char* label, const forgeshape::FixtureMesh& mesh,
                                        bool log) {
    forgeshape::MeshValidation why = forgeshape::MeshValidation::Ok;
    const auto vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    const auto indexCount = static_cast<uint32_t>(mesh.indices.size());
    const forgeshape::MeshRevision revision = forgeshape::meshStore().publish(
        mesh.vertices.data(), vertexCount, mesh.indices.data(), indexCount, &why);
    if (revision == forgeshape::kNoMeshRevision) {
        FS_LOGE("FORGESHAPE_MESH_PUBLISH_FAIL:%s:%s", label, forgeshape::meshValidationName(why));
    } else if (log) {
        FS_LOGI("FORGESHAPE_MESH_REVISION_PUBLISHED:%llu:%u:%u fixture=%s",
                (unsigned long long)revision, vertexCount, indexCount, label);
    }
    return revision;
}

// Reports the product mode and the Frozen Sculpt Mesh side by side with the
// Construction state, so one dump shows both representations and it is visible
// that the SculptRevision and the MeshStore revision are different counters.
// Requires g_stateMutex: `sculptSession()` rebinds the borrowed target.
void logSculptStateLocked(const char* reason) {
    const forgeshape::SculptSession& session = forgeshape::sculptSession();
    const forgeshape::SculptMesh& mesh = session.mesh();
    FS_LOGI("FORGESHAPE_SCULPT_STATE:%s mode=%s tool=%s frozen=%d sculptRev=%llu v=%u i=%u "
            "objectId=%llu radiusPx=%.1f strength=%.3f strokes=%llu freezes=%llu stale=%d "
            "storeRev=%llu adjacency=%u normalRecomputes=%llu",
            reason, forgeshape::productModeName(session.mode()),
            forgeshape::sculptToolName(session.tool()), session.hasSculptMesh() ? 1 : 0,
            (unsigned long long)mesh.revision(), mesh.vertexCount(), mesh.indexCount(),
            (unsigned long long)mesh.objectId(), session.radiusPixels(), session.strength(),
            (unsigned long long)session.strokeCount(), (unsigned long long)session.freezeCount(),
            session.sourceStale() ? 1 : 0,
            (unsigned long long)forgeshape::meshStore().currentRevision(),
            mesh.topology().totalNeighborEntries(),
            (unsigned long long)mesh.normalRecomputeCount());
}

void logSculptState(const char* reason) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    logSculptStateLocked(reason);
}

void logMeshDiagnostics(const char* reason) {
    // The GPU counters are atomics and are read before the lock; everything
    // below reads scene state and the sculpt target, so one bounded lock covers
    // the whole dump. Callers never hold g_stateMutex here.
    const forgeshape::MeshGpuStats s = forgeshape::meshUploadDiagnostics().snapshot();
    std::lock_guard<std::mutex> lock(g_stateMutex);
    forgeshape::MeshStore& store = forgeshape::meshStore();
    FS_LOGI("FORGESHAPE_MESH_DIAG:%s currentRev=%llu uploadedRev=%llu v=%llu i=%llu "
            "vcap=%llu icap=%llu scap=%llu grows=%llu sgrows=%llu uploads=%llu reuse=%llu "
            "live=%llu failed=%llu published=%llu rejected=%llu objectId=%llu",
            reason, (unsigned long long)store.currentRevision(),
            (unsigned long long)s.uploadedRevision, (unsigned long long)s.uploadedVertexCount,
            (unsigned long long)s.uploadedIndexCount, (unsigned long long)s.vertexCapacityBytes,
            (unsigned long long)s.indexCapacityBytes, (unsigned long long)s.stagingCapacityBytes,
            (unsigned long long)s.bufferGrowCount, (unsigned long long)s.stagingGrowCount,
            (unsigned long long)s.uploadCount, (unsigned long long)s.reuseUploadCount,
            (unsigned long long)s.liveBufferObjects, (unsigned long long)s.failedUploadCount,
            (unsigned long long)store.publishedCount(), (unsigned long long)store.rejectedCount(),
            (unsigned long long)store.objectId());
    // The authoritative Construction parameters are logged next to the derived
    // mesh/GPU numbers, so evidence can be read from one place without ever
    // inferring dimensions from vertices. An Imported Mesh has none, and the
    // line says WHICH representation rather than going quiet.
    if (const forgeshape::ConstructionObject* object = forgeshape::activeConstructionOrNull()) {
        char described[128];
        describeSpec(object->spec(), described, sizeof(described));
        FS_LOGI("FORGESHAPE_CONSTRUCTION_STATE:%s kind=%s %s objectId=%llu updates=%llu "
                "rejects=%llu",
                reason, forgeshape::primitiveKindName(object->kind()), described,
                (unsigned long long)object->objectId(), (unsigned long long)object->updateCount(),
                (unsigned long long)object->rejectedUpdateCount());
    } else if (!forgeshape::constructionScene().hasProject()) {
        FS_LOGI("FORGESHAPE_NO_PROJECT_STATE:%s bodies=0", reason);
    } else {
        const forgeshape::SceneObject& body = forgeshape::constructionScene().activeBody();
        const forgeshape::ImportedMesh* imported = body.importedOrNull();
        FS_LOGI("FORGESHAPE_IMPORTED_STATE:%s objectId=%llu vertices=%u triangles=%u batches=%u",
                reason, (unsigned long long)body.objectId(),
                imported != nullptr ? imported->vertexCount() : 0u,
                imported != nullptr ? imported->triangleCount() : 0u,
                imported != nullptr ? imported->batchCount() : 0u);
    }
    // Placement is reported next to the dimensions and the mesh numbers, so one
    // line pair shows both truths and it is obvious that the transform's update
    // count is independent of the mesh revision.
    const forgeshape::TransformValues t = forgeshape::constructionTransform().values();
    const forgeshape::ConstructionTransform& transform = forgeshape::constructionTransform();
    FS_LOGI("FORGESHAPE_CONSTRUCTION_TRANSFORM_STATE:%s pos=(%.6f,%.6f,%.6f)m "
            "rot=(%.6f,%.6f,%.6f)deg scale=(%.6f,%.6f,%.6f) identity=%d unscaled=%d "
            "updates=%llu rejects=%llu",
            reason, t.positionX, t.positionY, t.positionZ, t.rotationX, t.rotationY, t.rotationZ,
            t.scaleX, t.scaleY, t.scaleZ, transform.isIdentity() ? 1 : 0,
            transform.isUnscaled() ? 1 : 0, (unsigned long long)transform.updateCount(),
            (unsigned long long)transform.rejectedUpdateCount());
    logSculptStateLocked(reason);
}

// Bounded repeated-update test. One short-lived thread, not a task system: it
// publishes a fixed number of revisions and exits. Publication is paced so the
// render thread has a chance to consume each one; whatever it does coalesce is
// reported rather than hidden.
constexpr uint32_t kStressRevisions = 60;
std::atomic<bool> g_stressRunning{false};

void runMeshStress() {
    forgeshape::MeshStore& store = forgeshape::meshStore();
    forgeshape::MeshUploadDiagnostics& diagnostics = forgeshape::meshUploadDiagnostics();

    const forgeshape::MeshGpuStats before = diagnostics.snapshot();
    const auto startedAt = std::chrono::steady_clock::now();

    uint32_t published = 0;
    forgeshape::MeshRevision finalRevision = forgeshape::kNoMeshRevision;
    for (uint32_t step = 0; step < kStressRevisions; ++step) {
        const forgeshape::FixtureMesh mesh = forgeshape::buildStressStep(step);
        const forgeshape::MeshRevision revision =
            publishFixture("stress", mesh, /*log=*/false);
        if (revision != forgeshape::kNoMeshRevision) {
            ++published;
            finalRevision = revision;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Give the render thread a bounded chance to reach the final revision.
    for (int i = 0; i < 200 && diagnostics.snapshot().uploadedRevision < finalRevision; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const forgeshape::MeshGpuStats after = diagnostics.snapshot();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - startedAt)
                               .count();
    const uint64_t uploaded = after.uploadCount - before.uploadCount;
    const uint64_t coalesced = (published > uploaded) ? (published - uploaded) : 0;
    const uint64_t growsDuringStress = after.bufferGrowCount - before.bufferGrowCount;
    const uint64_t stagingGrowsDuringStress = after.stagingGrowCount - before.stagingGrowCount;

    FS_LOGI("FORGESHAPE_MESH_STRESS_OK:%u:%llu:%llu finalRev=%llu uploadedRev=%llu "
            "grows=%llu sgrows=%llu live=%llu vcap=%llu icap=%llu scap=%llu ms=%lld",
            published, (unsigned long long)uploaded, (unsigned long long)coalesced,
            (unsigned long long)finalRevision, (unsigned long long)after.uploadedRevision,
            (unsigned long long)growsDuringStress, (unsigned long long)stagingGrowsDuringStress,
            (unsigned long long)after.liveBufferObjects,
            (unsigned long long)after.vertexCapacityBytes,
            (unsigned long long)after.indexCapacityBytes,
            (unsigned long long)after.stagingCapacityBytes, (long long)elapsedMs);
    logMeshDiagnostics("after_stress");
    g_stressRunning.store(false);
}

// Whether the ACTIVE body can be measured at all, and its bounds. Called under
// the state lock by everything below.
bool activeBodyDimensions(forgeshape::LocalBounds* outBounds,
                          forgeshape::BodyDimensions* outDimensions) {
    const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
    if (!scene.hasProject()) {
        return false;
    }
    return forgeshape::sceneBodyDimensions(scene.activeBodyId(), scene, outBounds, outDimensions);
}

// Whether the ACTIVE body is one Dimensions mode may stand on, right now.
//
// The ONE predicate, named once and asked from both threads: the render thread
// closes the session with it when what it measures stops being measurable, and
// every chrome read below asks it before answering. Two copies of this rule is
// how the shell came to draw three numbers over a body that had been hidden,
// or over a sculpt session -- native had already shut the mode on a frame the
// shell never looked at again.
//
// Called under g_stateMutex, like everything else that reads the scene.
bool activeBodyDimensionsEditable(forgeshape::LocalBounds* outBounds,
                                  forgeshape::BodyDimensions* outDimensions) {
    const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
    return activeBodyDimensions(outBounds, outDimensions) && scene.hasProject()
        && forgeshape::bodySizeEditable(scene.activeBody())
        && !forgeshape::sculptSession().inSculptMode();
}

// The Dimensions session, having first shut itself if its body left.
//
// The mode CLOSES itself the moment what it measures stops being measurable --
// another body selected, this one locked, hidden or deleted, Sculpt entered,
// the project closed. That rule was the render thread's alone, so it was true
// only once a frame had been drawn and the shell's own refresh, which happens
// at the instant of the transition, saw a mode still open (UI3D-F-004). It is
// asked HERE too, so every observation of the session agrees with every other.
forgeshape::BodyDimensionSession& settledBodyDimensionSession() {
    forgeshape::BodyDimensionSession& session = forgeshape::bodyDimensionSession();
    if (session.active() && !activeBodyDimensionsEditable(nullptr, nullptr)) {
        session.close();
    }
    return session;
}

// Where the three numeric labels belong in WORLD space, for this instant.
//
// Derived from the active body's CURRENT bounds, placement and camera scale
// rather than read out of whatever the render thread last cached. The cache is
// a frame behind by construction, which showed as no labels at all on the frame
// the mode opened (UI3D-F-003) and as the PREVIOUS body's anchors after a switch
// (UI3D-F-007) -- the same race with its two outcomes.
//
// Called under g_stateMutex.
bool activeBodyDimensionAnchors(forgeshape::BodyDimensionLabelAnchors* out) {
    forgeshape::LocalBounds bounds;
    if (!settledBodyDimensionSession().active() || !activeBodyDimensionsEditable(&bounds, nullptr)) {
        return false;
    }
    // The same camera-derived world-per-unit the frame builds the leaders with,
    // so the label stands at the midpoint of the line actually drawn.
    float worldPerUnit = 0.0f;
    forgeshape::gizmoWorldScale(g_camera.snapshot(), forgeshape::Vec3{0.0f, 0.0f, 0.0f},
                                g_camera.viewportHeight(), &worldPerUnit);
    const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
    return forgeshape::bodyDimensionLabelAnchors(
        bounds, scene.activeBody().transform().values(), worldPerUnit, out);
}

void renderThreadMain() {
    Renderer renderer;
    if (!renderer.createInstance()) {
        std::lock_guard<std::mutex> lock(g_viewport.mutex);
        g_viewport.rendererFailed = true;
        g_viewport.toCaller.notify_all();
        return;
    }

    for (;;) {
        ANativeWindow* windowToAttach = nullptr;
        bool doDetach = false;
        bool doResize = false;

        {
            std::unique_lock<std::mutex> lock(g_viewport.mutex);
            // Sleep only when there is nothing to do and nothing to present.
            g_viewport.toRender.wait(lock, [] {
                return g_viewport.quitRequested || g_viewport.attachRequested ||
                       g_viewport.detachRequested || g_viewport.resizeRequested ||
                       g_viewport.pendingWindow != nullptr || g_viewport.running;
            });

            if (g_viewport.quitRequested) {
                break;
            }
            if (g_viewport.detachRequested) {
                doDetach = true;
            } else if (g_viewport.attachRequested) {
                windowToAttach = g_viewport.pendingWindow;
                g_viewport.pendingWindow = nullptr;
                g_viewport.attachRequested = false;
            } else if (g_viewport.resizeRequested) {
                g_viewport.resizeRequested = false;
                doResize = true;
            }
        }

        if (doDetach) {
            renderer.detachSurface();
            std::lock_guard<std::mutex> lock(g_viewport.mutex);
            g_viewport.detachRequested = false;
            g_viewport.detachAcked = true;
            g_viewport.running = false;
            g_viewport.toCaller.notify_all();
            continue;
        }

        if (windowToAttach != nullptr) {
            if (renderer.attachSurface(windowToAttach)) {
                std::lock_guard<std::mutex> lock(g_viewport.mutex);
                g_viewport.running = true;
            } else {
                FS_LOGE("Renderer failed to attach surface");
                std::lock_guard<std::mutex> lock(g_viewport.mutex);
                g_viewport.rendererFailed = true;
                g_viewport.running = false;
            }
            continue;
        }

        if (doResize) {
            renderer.requestResize();
        }

        if (renderer.hasSurface()) {
            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                renderer.setCamera(g_camera.snapshot());
                // One immutable snapshot of every renderable body: its own
                // published mesh, its own derived model matrix and its own
                // selection flag. Taken under the state mutex; the geometry
                // work the renderer then does with it happens after the mutex
                // is released, because the snapshot holds shared_ptrs rather
                // than borrowing anything the scene could change underneath.
                // WHICH scene is drawn. The imported preview is a diagnostic
                // VIEW, so it replaces what the renderer is handed for the
                // frame and changes nothing about the project: the scene it is
                // standing in front of is untouched, still selected, still
                // editable, and one call to setPreviewVisible(false) brings it
                // straight back. The preview never merges with the project
                // snapshot, because a mixed list is a list somebody would
                // eventually pick, save or export from.
                if (forgeshape::importedMeshPreview().visible()) {
                    renderer.setScene(forgeshape::importedMeshPreview().snapshot());
                } else {
                    renderer.setScene(forgeshape::constructionScene().snapshot());
                }
                // Taken under the SAME mutex and from the same instant as the
                // camera and the scene, so the pivot the handles are drawn
                // around is the placement this frame's body is drawn at. Taking
                // it separately is exactly how a handle would lag a drag by a
                // frame.
                //
                // Handles are withdrawn while the preview is shown, for the
                // same reason the editing controls are: a gizmo drawn over an
                // imported mesh would be pointing at a body that is not on the
                // screen, and dragging it would move something invisible.
                //
                // They are withdrawn in Dimensions mode for a related reason
                // (Stage 020M): the leaders and the handles are two instruments
                // for the same placement, and drawing both would invite a drag
                // in a mode whose whole point is an exact typed value. The
                // session already turned the gizmo off when it opened; this is
                // the frame's own answer, so a mode entered mid-drag cannot
                // leave one standing.
                if (forgeshape::importedMeshPreview().visible()
                    || forgeshape::bodyDimensionSession().active()) {
                    renderer.setGizmo(forgeshape::GizmoSnapshot{});
                } else {
                    renderer.setGizmo(forgeshape::gizmoSession().snapshot(
                        g_camera.snapshot(), g_camera.viewportWidth(),
                        g_camera.viewportHeight()));
                }
                // The sketch overlay, from the same instant. Sized by the same
                // camera-derived world-per-unit the gizmo uses, so the snap
                // marker reads at the same size the handles do. Empty -- and
                // therefore free -- whenever no sketch is in progress.
                {
                    float worldPerUnit = 0.0f;
                    forgeshape::gizmoWorldScale(g_camera.snapshot(),
                                                forgeshape::Vec3{0.0f, 0.0f, 0.0f},
                                                g_camera.viewportHeight(), &worldPerUnit);
                    // The spatial support chooser and the sketch never both run:
                    // the chooser draws the plane/face targets, then the sketch
                    // it starts draws the entities. When neither is active the
                    // overlay is empty and free.
                    //
                    // Dimensions mode is the third producer for the same one
                    // slot, and it borrows the whole path rather than adding a
                    // second: a body's dimension leaders and a sketch's
                    // dimension annotation are the same kind of drawing, so the
                    // renderer needed no change for either. The three are
                    // mutually exclusive by construction -- a sketch cannot be
                    // open over a body being measured, because the mode refuses
                    // to open in Sculpt and creation is refused in a sketch.
                    if (forgeshape::supportChooser().active()) {
                        renderer.setSketchOverlay(forgeshape::supportChooser().overlay());
                    } else if (forgeshape::bodyDimensionSession().active()) {
                        forgeshape::LocalBounds bounds;
                        const forgeshape::ConstructionScene& scene =
                            forgeshape::constructionScene();
                        // The ONE predicate, asked here and by every chrome
                        // read, so a frame and a refresh can never disagree
                        // about whether the mode still has a body.
                        const bool measurable = activeBodyDimensionsEditable(&bounds, nullptr);
                        if (measurable) {
                            renderer.setSketchOverlay(forgeshape::bodyDimensionSession().overlay(
                                bounds, scene.activeBody().transform().values(), worldPerUnit));
                        } else {
                            // The mode CLOSES itself the moment what it measures
                            // stops being measurable -- another body selected,
                            // this one locked, hidden or deleted, Sculpt entered,
                            // the project closed. One place decides that, every
                            // frame, so no caller has to remember to unwind it
                            // and the shell's next refresh simply finds the mode
                            // shut.
                            forgeshape::bodyDimensionSession().close();
                            renderer.setSketchOverlay(forgeshape::SketchOverlayPtr{});
                        }
                    } else {
                        renderer.setSketchOverlay(
                            forgeshape::sketchSession().overlay(worldPerUnit));
                    }
                }
            }
            // Presentation only, and deliberately OUTSIDE the state mutex: the
            // display settings are plain atomics that no domain invariant
            // depends on, so a frame must never wait on the geometry lock to
            // find out which shading model to draw with.
            renderer.setDisplaySettings(forgeshape::displaySettings().snapshot());
#ifndef NDEBUG
            {
                bool inject = false;
                {
                    std::lock_guard<std::mutex> lock(g_viewport.mutex);
                    inject = g_viewport.injectDeviceLossRequested;
                    g_viewport.injectDeviceLossRequested = false;
                }
                if (inject) {
                    renderer.injectDeviceLossForTest();
                }
            }
#endif
            const bool drew = renderer.drawFrame();
            // Mirrored every frame rather than only on a change: it is one
            // relaxed store, and it means the UI thread's answer is never stale
            // for longer than a frame no matter which path the renderer took.
            g_rendererLifecycle.store(static_cast<int>(renderer.lifecycle()),
                                      std::memory_order_relaxed);
            g_rendererDeviceRebuilds.store(renderer.deviceRebuildsCompleted(),
                                           std::memory_order_relaxed);
            // The outline counters, mirrored on the same terms: relaxed stores
            // every frame, so the UI thread's answer is never more than one
            // frame stale whichever path the renderer took.
            {
                const forgeshape::Renderer::SelectionOutlineStats outline =
                    renderer.selectionOutlineStats();
                g_outlineMaskAllocations.store(static_cast<long long>(outline.maskAllocations),
                                               std::memory_order_relaxed);
                g_outlineMaskPassFrames.store(static_cast<long long>(outline.maskPassFrames),
                                              std::memory_order_relaxed);
                g_outlineCompositeDraws.store(static_cast<long long>(outline.compositeDraws),
                                              std::memory_order_relaxed);
                g_outlineMaskWidth.store(static_cast<int>(outline.maskWidth),
                                         std::memory_order_relaxed);
                g_outlineMaskHeight.store(static_cast<int>(outline.maskHeight),
                                          std::memory_order_relaxed);
                g_outlineWidthPixelsCentis.store(
                    static_cast<int>(outline.widthPixels * 100.0f + 0.5f),
                    std::memory_order_relaxed);
            }
            if (!drew) {
                // The renderer has stopped for good. CPU project truth is
                // untouched by any of this — the scene, every published mesh and
                // every Frozen Sculpt Mesh live here, not on the device — so the
                // right thing to do is say so loudly and let the Android layer
                // checkpoint the project and tell the user a restart is needed.
                FS_LOGE("FORGESHAPE_%s:render_loop_stopped bodies=%d",
                        forgeshape::kRenderRestartRequiredToken,
                        (int)forgeshape::constructionScene().bodyCount());
                renderer.detachSurface();
                g_rendererLifecycle.store(
                    static_cast<int>(forgeshape::RendererLifecycle::RestartRequired),
                    std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(g_viewport.mutex);
                g_viewport.running = false;
                g_viewport.rendererFailed = true;
            }
        }
    }

    renderer.detachSurface();
    renderer.destroyInstance();

    std::lock_guard<std::mutex> lock(g_viewport.mutex);
    g_viewport.running = false;
    g_viewport.detachAcked = true;
    g_viewport.toCaller.notify_all();
    FS_LOGI("Render thread exited");
}

}  // namespace

extern "C" {

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_start(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_viewport.mutex);
    if (g_viewport.thread.joinable()) {
        return;
    }
    g_viewport.quitRequested = false;
    g_viewport.rendererFailed = false;
    g_viewport.thread = std::thread(renderThreadMain);
    FS_LOGI("ForgeShape native viewport started (render thread launched)");
    // The process starts with NO project (`APP-H1`): the scene is empty, Home
    // is what the shell shows, and the first body arrives when the user asks
    // for one -- through the CAD bootstrap's first commit, the Sculpt seed or
    // a load. Nothing is published here; an empty scene draws as empty.
    FS_LOGI("FORGESHAPE_STARTUP_NO_PROJECT bodies=%d",
            (int)forgeshape::constructionScene().bodyCount());
    runCameraSelfTestsAndLog();
    runPickingSelfTestsAndLog();
    runMeshSelfTestsAndLog();
    runConstructionSelfTestsAndLog();
    runTransformSelfTestsAndLog();
    runPrimitiveSelfTestsAndLog();
    runSphereSelfTestsAndLog();
    runConeCapsuleSelfTestsAndLog();
    runSculptSelfTestsAndLog();
    runRenderMeshSelfTestsAndLog();
    runSceneSelfTestsAndLog();
    runHistorySelfTestsAndLog();
    runGizmoSelfTestsAndLog();
    runProjectSelfTestsAndLog();
    runRenderRecoverySelfTestsAndLog();
    runGltfExportSelfTestsAndLog();
    runGltfImportSelfTestsAndLog();
    runCadSelfTestsAndLog();
    runBodyDimensionsSelfTestsAndLog();
    runMirrorSelfTestsAndLog();
    // The mesh and construction self-tests publish revisions of their own into
    // the store, so republish the ACTIVE representation: the app must always
    // come up showing what the current product mode says it is showing. At a
    // cold start no project is open, so this is a no-op and the store the
    // suites wrote into is the unbound one nothing draws from.
    publishActiveRepresentation("startup_after_selftests");
}

#ifndef NDEBUG
// DEBUG-ONLY heavy-mesh density harness (Gate P1).
//
// Not a Construction primitive, not a product feature, not reachable from any
// UI: it exists so the render/upload/picking/Sculpt path can be measured at
// vertex counts no real primitive produces. Keeps the last generated tier so
// a separate command can freeze exactly what was just published/measured into
// Sculpt, instead of silently regenerating a second, distinct instance.
forgeshape::FixtureMesh g_lastStressMesh;
const char* g_lastStressLabel = "none";

void publishStressTier(const char* label, uint32_t targetVertexCount) {
    const auto genStart = std::chrono::steady_clock::now();
    forgeshape::FixtureMesh mesh = forgeshape::buildStressMesh(targetVertexCount);
    const double genMs = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - genStart)
                             .count();
    const auto publishStart = std::chrono::steady_clock::now();
    const forgeshape::MeshRevision revision = publishFixture(label, mesh, /*log=*/true);
    const double publishMs = std::chrono::duration<double, std::milli>(
                                 std::chrono::steady_clock::now() - publishStart)
                                 .count();
    FS_LOGI("FORGESHAPE_STRESS_MESH_TIER:%s target=%u v=%zu i=%zu genMs=%.3f publishMs=%.3f "
            "rev=%llu",
            label, targetVertexCount, mesh.vertices.size(), mesh.indices.size(), genMs, publishMs,
            (unsigned long long)revision);
    g_lastStressMesh = std::move(mesh);
    g_lastStressLabel = label;
}

// Freezes the mesh most recently built by publishStressTier directly into
// Sculpt, bypassing Construction entirely -- the same freezeToSculpt entry
// point the real product path uses, just fed a debug-only source. Exercises
// adjacency build and normal computation at stress density without inventing
// a second Sculpt path.
void freezeStressMeshToSculpt() {
    if (g_lastStressMesh.vertices.empty()) {
        FS_LOGE("FORGESHAPE_STRESS_SCULPT_FREEZE_FAIL:no_stress_mesh_generated_yet");
        return;
    }
    forgeshape::ConstructionMesh source;
    source.vertices = g_lastStressMesh.vertices;
    source.indices = g_lastStressMesh.indices;
    forgeshape::MeshValidation why = forgeshape::MeshValidation::Ok;
    bool froze = false;
    forgeshape::SculptSession* session = nullptr;
    const auto start = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_grabbing = false;
        g_strokePending = false;
        session = &forgeshape::sculptSession();
        froze = session->freezeToSculpt(source, forgeshape::kConstructionBoxObjectId, &why);
    }
    const double freezeMs = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - start)
                                .count();
    if (!froze) {
        FS_LOGE("FORGESHAPE_STRESS_SCULPT_FREEZE_FAIL:%s:%s", g_lastStressLabel,
                forgeshape::meshValidationName(why));
        return;
    }
    const forgeshape::MeshRevision revision =
        publishSculptRepresentation(*session, "stress_freeze");
    FS_LOGI("FORGESHAPE_STRESS_SCULPT_FROZEN:%s freezeMs=%.3f meshRev=%llu", g_lastStressLabel,
            freezeMs, (unsigned long long)revision);
    logSculptState("after_stress_freeze");
}
#endif  // NDEBUG

// DEBUG-ONLY read-back of the platform-neutral pointer data the last touch
// event produced.
//
// Test infrastructure. It exists so an instrumented test can prove that a tool
// type, a pressure and a tilt survive MotionEvent -> SurfaceView -> JNI ->
// native intact and stay attached to the right pointer, without a debug overlay
// and without the Java layer ever owning pointer data. It READS a bounded
// snapshot and changes nothing: no mesh, no revision, no camera, no selection.
//
// Layout, matching NativeViewport's slot constants: slot 0 is the pointer
// count, then seven floats per pointer -- id, x, y, tool-type wire code,
// pressure, tilt, tilt orientation.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_debugLastPointerEvent(JNIEnv* env, jclass,
                                                             jfloatArray outState) {
#ifdef NDEBUG
    (void)env;
    (void)outState;
    return -1;
#else
    constexpr int kStride = 7;
    constexpr int kSize = 1 + forgeshape::kMaxTrackedPointers * kStride;
    if (outState == nullptr || env->GetArrayLength(outState) < kSize) {
        return -1;
    }

    jfloat buffer[kSize];
    for (int i = 0; i < kSize; ++i) {
        buffer[i] = 0.0f;
    }

    int count = 0;
    bool sawAnyEvent = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        sawAnyEvent = g_lastPointerCount >= 0;
        count = sawAnyEvent ? g_lastPointerCount : 0;
        buffer[0] = static_cast<jfloat>(count);
        for (int i = 0; i < count && i < forgeshape::kMaxTrackedPointers; ++i) {
            const forgeshape::TouchPointer& p = g_lastPointers[i];
            jfloat* slot = buffer + 1 + i * kStride;
            slot[0] = static_cast<jfloat>(p.id);
            slot[1] = p.x;
            slot[2] = p.y;
            slot[3] = static_cast<jfloat>(forgeshape::pointerToolTypeCode(p.toolType));
            slot[4] = p.pressure;
            slot[5] = p.tiltRadians;
            slot[6] = p.tiltOrientationRadians;
        }
    }

    env->SetFloatArrayRegion(outState, 0, kSize, buffer);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return -1;
    }
    // -1 also means "no touch event has reached native code yet", which is
    // distinct from an event that legitimately carried zero pointers.
    return static_cast<jint>(sawAnyEvent ? count : -1);
#endif
}

// DEBUG-ONLY mesh fixture trigger.
//
// Compiled to a no-op in release, so no product surface, no exported component
// and no console exists in a shipping build. It only publishes CPU mesh
// revisions; the render thread still owns every Vulkan operation.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_debugMeshCommand(JNIEnv*, jclass, jint command) {
#ifdef NDEBUG
    (void)command;
    return JNI_FALSE;
#else
    switch (command) {
        case 1:
            publishFixture("A_baseline", forgeshape::buildFixtureBaseline(), /*log=*/true);
            return JNI_TRUE;
        case 2:
            publishFixture("B_same_topology", forgeshape::buildFixtureDeformed(), /*log=*/true);
            return JNI_TRUE;
        case 3:
            publishFixture("C_larger", forgeshape::buildFixtureLarge(), /*log=*/true);
            return JNI_TRUE;
        case 4: {
            bool expected = false;
            if (!g_stressRunning.compare_exchange_strong(expected, true)) {
                FS_LOGI("FORGESHAPE_MESH_STRESS_BUSY");
                return JNI_FALSE;
            }
            FS_LOGI("FORGESHAPE_MESH_STRESS_START:%u", kStressRevisions);
            std::thread(runMeshStress).detach();
            return JNI_TRUE;
        }
        case 5:
            logMeshDiagnostics("on_request");
            return JNI_TRUE;
        // Where the gizmo's pivot and its three handles are on screen RIGHT NOW.
        //
        // The one thing a shell-driven walkthrough cannot work out for itself: a
        // handle's pixel is a live function of the camera, the window and the
        // body's placement, so a script that wrote one down would be recording
        // something true for exactly one run. This publishes the same answer the
        // hit test uses, from the same projection, so evidence can drive a real
        // drag against a real handle without ever encoding a coordinate.
        case 23: {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            const forgeshape::CameraSnapshot camera = g_camera.snapshot();
            const int width = g_camera.viewportWidth();
            const int height = g_camera.viewportHeight();
            const forgeshape::GizmoSnapshot state =
                forgeshape::gizmoSession().snapshot(camera, width, height);
            if (!state.visible) {
                FS_LOGI("FORGESHAPE_GIZMO_HANDLES:absent");
                return JNI_FALSE;
            }
            float px = 0.0f, py = 0.0f;
            forgeshape::projectWorldToScreen(camera, state.pivot, width, height, &px, &py);
            char line[512];
            int written = snprintf(line, sizeof(line),
                                   "FORGESHAPE_GIZMO_HANDLES:%s/%s pivot=%.1f,%.1f",
                                   forgeshape::gizmoModeName(state.mode),
                                   forgeshape::gizmoSpaceName(state.space), px, py);
            // Every handle this MODE actually offers, from the one definition of
            // where a handle is. A list that hard-coded three axes would go
            // silent about the plane and uniform handles the moment they existed.
            forgeshape::GizmoHandle handles[forgeshape::kGizmoMaxHandles];
            const int handleCount = forgeshape::gizmoHandlesForMode(
                state.mode, handles, forgeshape::kGizmoMaxHandles);
            for (int i = 0; i < handleCount && written > 0 && written < (int)sizeof(line); ++i) {
                forgeshape::Vec3 world{};
                float hx = 0.0f, hy = 0.0f;
                const bool on =
                    forgeshape::gizmoHandleGrabPoint(state, handles[i], &world) &&
                    forgeshape::projectWorldToScreen(camera, world, width, height, &hx, &hy);
                written += snprintf(line + written, sizeof(line) - written, " %s=%.1f,%.1f%s",
                                    forgeshape::gizmoHandleName(handles[i]), hx, hy,
                                    on ? "" : "(offscreen)");
            }
            FS_LOGI("%s", line);
            return JNI_TRUE;
        }
        // Construction primitive states. These drive the AUTHORITATIVE
        // double-meter parameters through the same entry point the UI uses; the
        // mesh is regenerated and republished from them, never edited directly.
        case 6:
            applyPrimitive("state_A", forgeshape::PrimitiveSpec::forBox(
                                          forgeshape::kDefaultBoxWidthMeters,
                                          forgeshape::kDefaultBoxHeightMeters,
                                          forgeshape::kDefaultBoxDepthMeters));
            return JNI_TRUE;
        case 7:
            applyPrimitive("state_B", forgeshape::PrimitiveSpec::forBox(1.25, 2.5, 0.75));
            return JNI_TRUE;
        case 8:
            applyPrimitive("state_C", forgeshape::PrimitiveSpec::forBox(3.333, 0.42, 1.125));
            return JNI_TRUE;
        case 9:
            // Must fail closed and change nothing.
            applyPrimitive("invalid_negative_width",
                           forgeshape::PrimitiveSpec::forBox(-1.0, 1.0, 0.5));
            return JNI_TRUE;
        // Gate P1 heavy-mesh density ladder. Publishes a closed, deterministic
        // stress mesh at approximately the named vertex count (see
        // buildStressMesh's doc comment for how "approximately" resolves) so
        // render/upload/picking can be measured at that density; command 22
        // then freezes whichever tier was published most recently into Sculpt.
        case 17:
            publishStressTier("stress_10k", 10000);
            return JNI_TRUE;
        case 18:
            publishStressTier("stress_50k", 50000);
            return JNI_TRUE;
        case 19:
            publishStressTier("stress_100k", 100000);
            return JNI_TRUE;
        case 20:
            publishStressTier("stress_250k", 250000);
            return JNI_TRUE;
        case 21:
            publishStressTier("stress_500k", 500000);
            return JNI_TRUE;
        case 22:
            freezeStressMeshToSculpt();
            return JNI_TRUE;
        default:
            FS_LOGI("FORGESHAPE_MESH_DEBUG_UNKNOWN_COMMAND:%d", (int)command);
            return JNI_FALSE;
    }
#endif
}

// Reads the AUTHORITATIVE Construction primitive state for display.
//
// This is the only direction that exists: the UI asks native code what the
// object is, formats it, and never keeps a parameter of its own. Nothing is
// measured from the mesh or from GPU data.
//
// Every primitive's parameters are reported, not only the active one, so the
// panel can show an inactive primitive's remembered values without inventing
// defaults of its own. Each slot has ONE fixed meaning regardless of the kind,
// so nothing here is positional-by-kind:
//
//   [0] active kind (0 = box, 1 = cylinder, 2 = sphere, 3 = cone, 4 = capsule,
//                    5 = plane)
//   [1..3]  box width / height / depth, meters
//   [4..5]  cylinder diameter / height, meters
//   [6]     sphere diameter, meters
//   [7..8]  cone bottom diameter / height, meters
//   [9..10] capsule diameter / TOTAL height, meters
//   [11..12] plane width / depth, meters
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_constructionPrimitive(JNIEnv* env, jclass,
                                                              jdoubleArray outState) {
    constexpr jsize kSlots = 13;
    if (outState == nullptr || env->GetArrayLength(outState) < kSlots) {
        return;
    }
    // Scene reads happen under the state lock; the JNI write happens after it.
    jdouble values[kSlots];
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::ConstructionObject* source = forgeshape::activeConstructionOrNull();
        if (source == nullptr) {
            // An Imported Mesh has no primitive and no dimensions, and there is
            // no honest value to write here. The array is left EXACTLY as the
            // caller supplied it rather than filled with zeroes or a default
            // Box: a zero width is a length the editor refuses, and a default
            // Box is project data this body does not have. The Android layer
            // asks `sceneActiveBodyIsImported()` before it offers Shape at all,
            // so this is the second line of defence and not the first.
            return;
        }
        const forgeshape::ConstructionObject& object = *source;
        const forgeshape::BoxDimensionsMeters box = object.box().dimensionsMeters();
        const forgeshape::CylinderDimensionsMeters cylinder =
            object.cylinder().dimensionsMeters();
        const forgeshape::SphereDimensionsMeters sphere = object.sphere().dimensionsMeters();
        const forgeshape::ConeDimensionsMeters cone = object.cone().dimensionsMeters();
        const forgeshape::CapsuleDimensionsMeters capsule = object.capsule().dimensionsMeters();
        const forgeshape::PlaneDimensionsMeters plane = object.plane().dimensionsMeters();
        const jdouble read[kSlots] = {
            static_cast<jdouble>(static_cast<int>(object.kind())),
            box.width, box.height, box.depth,
            cylinder.diameter, cylinder.height,
            sphere.diameter,
            cone.bottomDiameter, cone.height,
            capsule.diameter, capsule.totalHeight,
            plane.width, plane.depth,
        };
        std::copy(read, read + kSlots, values);
    }
    env->SetDoubleArrayRegion(outState, 0, kSlots, values);
}

// The product shape edit path: ONE native method per primitive.
//
// There is deliberately no generic "kind plus three doubles" entry point. Each
// method's signature is the primitive's own parameter list, so a caller cannot
// send a cylinder's height where a box's depth belongs, and a sphere request
// physically cannot carry a second or third number. Each adds logging and a
// status code and nothing else: they decide no validity, decide nothing about
// publishing, and never touch the mesh store or the transform.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyConstructionBox(JNIEnv*, jclass, jdouble widthMeters,
                                                             jdouble heightMeters,
                                                             jdouble depthMeters) {
    return applyResultToJni(applyPrimitive(
        "ui", forgeshape::PrimitiveSpec::forBox(widthMeters, heightMeters, depthMeters)));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyConstructionCylinder(JNIEnv*, jclass,
                                                                  jdouble diameterMeters,
                                                                  jdouble heightMeters) {
    return applyResultToJni(applyPrimitive(
        "ui", forgeshape::PrimitiveSpec::forCylinder(diameterMeters, heightMeters)));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyConstructionSphere(JNIEnv*, jclass,
                                                                jdouble diameterMeters) {
    return applyResultToJni(
        applyPrimitive("ui", forgeshape::PrimitiveSpec::forSphere(diameterMeters)));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyConstructionCone(JNIEnv*, jclass,
                                                              jdouble bottomDiameterMeters,
                                                              jdouble heightMeters) {
    return applyResultToJni(applyPrimitive(
        "ui", forgeshape::PrimitiveSpec::forCone(bottomDiameterMeters, heightMeters)));
}

// The capsule's second parameter is its TOTAL height, ends included. It is named
// so at every level for the same reason the cylinder's parameter is a diameter
// rather than a radius: it is the number the user measures, and the middle
// length is derived from it and never stored.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyConstructionCapsule(JNIEnv*, jclass,
                                                                 jdouble diameterMeters,
                                                                 jdouble totalHeightMeters) {
    return applyResultToJni(applyPrimitive(
        "ui", forgeshape::PrimitiveSpec::forCapsule(diameterMeters, totalHeightMeters)));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyConstructionPlane(JNIEnv*, jclass,
                                                               jdouble widthMeters,
                                                               jdouble depthMeters) {
    return applyResultToJni(
        applyPrimitive("ui", forgeshape::PrimitiveSpec::forPlane(widthMeters, depthMeters)));
}

// Reads the AUTHORITATIVE Construction transform for display: position in
// meters, rotation in degrees, scale unitless. Nothing is recovered from the
// model matrix.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_boxTransform(JNIEnv* env, jclass,
                                                    jdoubleArray outPlacement) {
    if (outPlacement == nullptr || env->GetArrayLength(outPlacement) < 9) {
        return;
    }
    forgeshape::TransformValues v;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        v = forgeshape::constructionTransform().values();
    }
    const jdouble values[9] = {v.positionX, v.positionY, v.positionZ,
                               v.rotationX, v.rotationY, v.rotationZ,
                               v.scaleX,    v.scaleY,    v.scaleZ};
    env->SetDoubleArrayRegion(outPlacement, 0, 9, values);
}

// The product transform edit path. One call carries all nine values — position
// in meters, rotation in degrees, scale unitless — into the one native
// Construction transform entry point, as ONE atomic request: a bad scale leaves
// the position untouched, and one Apply is one history step. It publishes no
// mesh revision and triggers no upload.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyBoxTransform(JNIEnv*, jclass, jdouble positionX,
                                                          jdouble positionY, jdouble positionZ,
                                                          jdouble rotationX, jdouble rotationY,
                                                          jdouble rotationZ, jdouble scaleX,
                                                          jdouble scaleY, jdouble scaleZ) {
    forgeshape::TransformValues requested;
    requested.positionX = positionX;
    requested.positionY = positionY;
    requested.positionZ = positionZ;
    requested.rotationX = rotationX;
    requested.rotationY = rotationY;
    requested.rotationZ = rotationZ;
    requested.scaleX = scaleX;
    requested.scaleY = scaleY;
    requested.scaleZ = scaleZ;
    bool refusedLocked = false;
    const jint status = transformResultToJni(applyBoxTransform("ui", requested, &refusedLocked));
    return refusedLocked ? kApplyRejectedLocked : status;
}

// ---------------------------------------------------------------------------
// Body Dimensions and Relative Scale (Stage 020M, `UI-OWNER-33B`)
// ---------------------------------------------------------------------------
//
// Two acts and one interaction mode, all Construction-only. Nothing here is new
// project truth: a dimension is DERIVED from the body's own local bounds and
// its stored Absolute Scale, and editing one writes the transform the product
// already had. Relative Scale is a temporary multiplier that never crosses this
// boundary in the other direction -- there is nothing to read back, because
// nothing stores one.

// Two more refusal codes, in step with NativeViewport's APPLY_* fields. Both
// exist because the honest reason for a refusal is not a bad number: an axis
// with no thickness cannot be given one, and a body this stage does not measure
// is not a body whose value was wrong.
constexpr jint kApplyRejectedDegenerateAxis = 9;
constexpr jint kApplyRejectedUnavailable = 10;

jint bodySizeResultToJni(const forgeshape::BodySizeResult& result) {
    switch (result.status) {
        case forgeshape::BodySizeStatus::Ok: return kApplyApplied;
        case forgeshape::BodySizeStatus::Unchanged: return kApplyUnchanged;
        case forgeshape::BodySizeStatus::RefusedLocked: return kApplyRejectedLocked;
        case forgeshape::BodySizeStatus::UnknownBody:
        case forgeshape::BodySizeStatus::RefusedRepresentation:
        case forgeshape::BodySizeStatus::RefusedHidden:
        case forgeshape::BodySizeStatus::RefusedEditInProgress:
            return kApplyRejectedUnavailable;
        case forgeshape::BodySizeStatus::RefusedGeometry: break;
    }
    switch (result.resize) {
        case forgeshape::ResizeStatus::NotPositive: return kApplyRejectedNotPositive;
        case forgeshape::ResizeStatus::NotRepresentable: return kApplyRejectedNotRepresentable;
        case forgeshape::ResizeStatus::DegenerateAxis: return kApplyRejectedDegenerateAxis;
        case forgeshape::ResizeStatus::InvalidAxis: return kApplyRejectedUnavailable;
        case forgeshape::ResizeStatus::NotFinite:
        case forgeshape::ResizeStatus::Ok: break;
    }
    return kApplyRejectedNotFinite;
}

// The whole Dimensions state, in NativeViewport's BODY_DIM_* slots.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_bodyDimensionsState(JNIEnv* env, jclass, jdoubleArray out) {
    constexpr jsize kSize = 14;
    if (out == nullptr || env->GetArrayLength(out) < kSize) {
        return;
    }
    jdouble values[kSize] = {0, 0, -1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::LocalBounds bounds;
        forgeshape::BodyDimensions dimensions;
        const bool measurable = activeBodyDimensions(&bounds, &dimensions);
        const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
        // "Supported" is what decides whether the CONTROLS are drawn, and it is
        // the domain's own predicate rather than a second rule written here.
        const bool editable = activeBodyDimensionsEditable(nullptr, nullptr);
        // Asked through the settling accessor, so a session whose body has been
        // hidden, locked, deleted or handed to Sculpt reads as CLOSED here even
        // if no frame has been drawn since. The shell refreshes at the instant
        // of such a transition, and it must not be told the mode is still open.
        const forgeshape::BodyDimensionSession& session = settledBodyDimensionSession();
        values[0] = editable ? 1.0 : 0.0;
        values[1] = session.active() ? 1.0 : 0.0;
        values[2] = static_cast<double>(session.activeAxis());
        values[3] = static_cast<double>(forgeshape::resizeAnchorIndex(session.anchor()));
        if (measurable) {
            values[4] = dimensions.x;
            values[5] = dimensions.y;
            values[6] = dimensions.z;
            values[7] = bounds.extent(0);
            values[8] = bounds.extent(1);
            values[9] = bounds.extent(2);
            const forgeshape::TransformValues t = scene.activeBody().transform().values();
            values[10] = t.scaleX;
            values[11] = t.scaleY;
            values[12] = t.scaleZ;
        }
        values[13] = measurable ? 1.0 : 0.0;
    }
    env->SetDoubleArrayRegion(out, 0, kSize, values);
}

// Opens or closes Dimensions mode. Returns what the mode IS afterwards, so the
// shell never has to assume a request succeeded.
//
// Opening WITHDRAWS the transform gizmo below JNI, not only above it: the two
// are alternative ways to change the same placement and a handle left standing
// under the leaders would be a second instrument for one act. Closing does not
// put it back -- the shell decides what tool is held, exactly as it does today.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_setBodyDimensionsMode(JNIEnv*, jclass, jboolean on) {
    bool nowActive = false;
    bool refused = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::BodyDimensionSession& session = forgeshape::bodyDimensionSession();
        if (on == JNI_TRUE) {
            // The same one predicate every other reader asks, so what may OPEN
            // the mode and what keeps it open cannot come apart.
            const bool editable = activeBodyDimensionsEditable(nullptr, nullptr);
            if (!editable) {
                // A locked, hidden, imported, CAD or sculpted body, or no
                // project at all. Refused by name; the mode is left CLOSED
                // rather than opened over something it cannot measure.
                refused = true;
                session.close();
            } else {
                session.open();
                forgeshape::gizmoSession().setActive(false);
            }
        } else {
            session.close();
        }
        nowActive = session.active();
    }
    if (refused) {
        FS_LOGI("FORGESHAPE_BODY_DIMENSIONS_REFUSED:not_measurable");
    } else {
        FS_LOGI("FORGESHAPE_BODY_DIMENSIONS_MODE:%d", nowActive ? 1 : 0);
    }
    return nowActive ? JNI_TRUE : JNI_FALSE;
}

// Which axis's value is being read or edited. -1 clears it.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_setBodyDimensionAxis(JNIEnv*, jclass, jint axis) {
    bool accepted = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        accepted = forgeshape::bodyDimensionSession().setActiveAxis(static_cast<int>(axis));
    }
    return accepted ? JNI_TRUE : JNI_FALSE;
}

// Which side of the body a resize holds still.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_setBodyDimensionAnchor(JNIEnv*, jclass, jint anchorIndex) {
    forgeshape::ResizeAnchor anchor;
    if (!forgeshape::resizeAnchorFromIndex(static_cast<int>(anchorIndex), &anchor)) {
        FS_LOGE("FORGESHAPE_BODY_DIMENSION_ANCHOR_REJECTED:%d", (int)anchorIndex);
        return JNI_FALSE;
    }
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::bodyDimensionSession().setAnchor(anchor);
    }
    FS_LOGI("FORGESHAPE_BODY_DIMENSION_ANCHOR:%s", forgeshape::resizeAnchorName(anchor));
    return JNI_TRUE;
}

// One exact overall dimension, in METRES, on one axis, with the session's
// current anchor. One call is one history transaction.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyBodyDimension(JNIEnv*, jclass, jint axis,
                                                          jdouble targetMeters) {
    forgeshape::BodySizeResult result;
    forgeshape::ResizeAnchor anchor = forgeshape::ResizeAnchor::Center;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (!forgeshape::constructionScene().hasProject()) {
            FS_LOGE("FORGESHAPE_BODY_DIMENSION_REJECTED:NoProject");
            return kApplyRejectedUnavailable;
        }
        if (forgeshape::sculptSession().inSculptMode()) {
            // Stage 020M is Construction-only and `SCULPT-DIM-01` is still
            // blocked by OQ-02. Refused below JNI as well as withdrawn above
            // it: removing a control is not removing a guard.
            FS_LOGE("FORGESHAPE_BODY_DIMENSION_REJECTED:InSculpt");
            return kApplyRejectedUnavailable;
        }
        anchor = forgeshape::bodyDimensionSession().anchor();
        result = forgeshape::applyBodyDimension(
            forgeshape::constructionScene().activeBodyId(), static_cast<int>(axis), targetMeters,
            anchor, forgeshape::constructionScene(), forgeshape::constructionHistory());
    }
    if (result.status == forgeshape::BodySizeStatus::Ok) {
        FS_LOGI("FORGESHAPE_BODY_DIMENSION:axis=%d target=%.6fm anchor=%s "
                "pos=(%.6f,%.6f,%.6f)m scale=(%.6f,%.6f,%.6f)",
                (int)axis, (double)targetMeters, forgeshape::resizeAnchorName(anchor),
                result.values.positionX, result.values.positionY, result.values.positionZ,
                result.values.scaleX, result.values.scaleY, result.values.scaleZ);
    } else {
        FS_LOGE("FORGESHAPE_BODY_DIMENSION_REJECTED:%s:%s axis=%d target=%.6f",
                forgeshape::bodySizeStatusName(result.status),
                forgeshape::resizeStatusName(result.resize), (int)axis, (double)targetMeters);
    }
    return bodySizeResultToJni(result);
}

// One Relative Scale Apply: the three multipliers, committed into the stored
// Absolute Scale as one history transaction. The multipliers themselves are not
// stored anywhere, which is why the next interaction opens at (1, 1, 1).
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyBodyRelativeScale(JNIEnv*, jclass, jdouble x,
                                                              jdouble y, jdouble z) {
    forgeshape::BodySizeResult result;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (!forgeshape::constructionScene().hasProject()) {
            FS_LOGE("FORGESHAPE_BODY_RELATIVE_SCALE_REJECTED:NoProject");
            return kApplyRejectedUnavailable;
        }
        if (forgeshape::sculptSession().inSculptMode()) {
            FS_LOGE("FORGESHAPE_BODY_RELATIVE_SCALE_REJECTED:InSculpt");
            return kApplyRejectedUnavailable;
        }
        result = forgeshape::applyBodyRelativeScale(
            forgeshape::constructionScene().activeBodyId(), x, y, z,
            forgeshape::constructionScene(), forgeshape::constructionHistory());
    }
    if (result.status == forgeshape::BodySizeStatus::Ok) {
        FS_LOGI("FORGESHAPE_BODY_RELATIVE_SCALE:multiplier=(%.6f,%.6f,%.6f) "
                "absolute=(%.6f,%.6f,%.6f)",
                (double)x, (double)y, (double)z, result.values.scaleX, result.values.scaleY,
                result.values.scaleZ);
    } else {
        FS_LOGE("FORGESHAPE_BODY_RELATIVE_SCALE_REJECTED:%s:%s multiplier=(%.6f,%.6f,%.6f)",
                forgeshape::bodySizeStatusName(result.status),
                forgeshape::resizeStatusName(result.resize), (double)x, (double)y, (double)z);
    }
    return bodySizeResultToJni(result);
}

// Where one axis's numeric label belongs, in view-local pixels.
//
// Derived from the MIDPOINT of that axis's real dimension line, through the
// same projection everything else in the viewport uses -- never from a guessed
// offset off a world bounding box. False when the mode is closed, the overlay
// has not been built or the anchor does not project, and a label with nowhere
// honest to stand is not drawn.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_bodyDimensionLabelPoint(JNIEnv* env, jclass, jint axis,
                                                               jfloatArray out) {
    if (out == nullptr || env->GetArrayLength(out) < 2) {
        return JNI_FALSE;
    }
    const int index = static_cast<int>(axis);
    if (index < 0 || index >= forgeshape::kBodyAxisCount) {
        return JNI_FALSE;
    }
    float point[2] = {0.0f, 0.0f};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        // Derived for THIS instant rather than read out of the last frame's
        // cache: the anchors belong to whichever body the mode measures now, and
        // they exist before a frame has ever drawn the leaders.
        forgeshape::BodyDimensionLabelAnchors anchors;
        if (activeBodyDimensionAnchors(&anchors)) {
            found = forgeshape::projectWorldToScreen(g_camera.snapshot(), anchors.axis[index],
                                                     g_camera.viewportWidth(),
                                                     g_camera.viewportHeight(), &point[0],
                                                     &point[1]);
        }
    }
    if (!found) {
        return JNI_FALSE;
    }
    env->SetFloatArrayRegion(out, 0, 2, point);
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// Product mode and the Frozen Sculpt Mesh.
//
// The Android UI may REQUEST a mode; native code owns it. Every one of these
// methods returns what native state actually is afterwards, so the panel never
// has to assume a request succeeded.
// ---------------------------------------------------------------------------

// Sculpt status codes handed back to the Android UI. Like the APPLY_* codes,
// they are a JNI transport detail and must stay in step with NativeViewport's
// SCULPT_* fields.
constexpr jint kSculptOk = 0;
constexpr jint kSculptFailedFreeze = 1;
constexpr jint kSculptNothingFrozen = 2;
// The active body is a CAD Body, which `CAD-R0-A1A2` does not sculpt.
constexpr jint kSculptRefusedCadBody = 3;

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_productMode(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::sculptSession().inSculptMode() ? 1 : 0;
}

// Start Sculpting.
//
// One explicit user act performs the whole thing: take a coherent snapshot of
// the ACTIVE BODY's current local mesh, make it the Frozen Sculpt Mesh with the
// same ObjectId, enter Sculpt mode, and publish it as the active
// representation.
//
// Representation-neutral since `IMPORT-01B`, and it is `buildSculptSourceMesh`
// that makes it so: a Construction Body regenerates from its parameters, an
// Imported Mesh hands over the arrays it owns, and this function does not ask
// which. Either source is only READ -- no primitive parameter, no placement and
// no imported vertex is written by a freeze, then or ever.
//
// The source is built under the state mutex, exactly as applyPrimitive,
// runHistoryStep and loadProject already generate under it: what is being
// generated FROM is what another thread could otherwise read half-written.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_freezeToSculpt(JNIEnv*, jclass) {
    forgeshape::MeshValidation why = forgeshape::MeshValidation::Ok;
    forgeshape::ObjectId objectId = forgeshape::kNoObject;
    bool haveSource = false;
    bool froze = false;
    bool cadBody = false;
    bool sketching = false;
    forgeshape::SculptSession* session = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (!forgeshape::constructionScene().hasProject()) {
            FS_LOGE("FORGESHAPE_SCULPT_FREEZE_FAIL:NoProject");
            return kSculptFailedFreeze;
        }
        forgeshape::SceneObject& body = forgeshape::constructionScene().activeBody();
        objectId = body.objectId();
        cadBody = body.cadOrNull() != nullptr;
        sketching = forgeshape::sketchSession().active();
        forgeshape::ConstructionMesh source;
        haveSource = !cadBody && !sketching && forgeshape::buildSculptSourceMesh(body, &source);
        if (haveSource) {
            g_grabbing = false;
            g_strokePending = false;
            session = &forgeshape::sculptSession();
            froze = session->freezeToSculpt(source, objectId, &why);
        }
    }
    if (cadBody) {
        // `CAD-R0-A1A2` leaves CAD -> Sculpt out, by name. The control is
        // absent for a CAD Body; this is the guard behind it.
        FS_LOGE("FORGESHAPE_SCULPT_FREEZE_FAIL:CadBodyNotSculptable objectId=%llu",
                (unsigned long long)objectId);
        return kSculptRefusedCadBody;
    }
    if (sketching) {
        FS_LOGE("FORGESHAPE_SCULPT_FREEZE_FAIL:in_sketch");
        return kSculptFailedFreeze;
    }
    if (!haveSource) {
        // A body with no geometry at all. Not reachable from the product -- both
        // representations always have some -- and refused by name rather than
        // left to fail as an empty mesh.
        FS_LOGE("FORGESHAPE_SCULPT_FREEZE_FAIL:NoGeometrySource");
        return kSculptFailedFreeze;
    }
    if (!froze) {
        FS_LOGE("FORGESHAPE_SCULPT_FREEZE_FAIL:%s", forgeshape::meshValidationName(why));
        return kSculptFailedFreeze;
    }

    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SculptMesh& mesh = session->mesh();
        // The log names WHAT was frozen from, which is the one thing that
        // differs between the two representations and the one thing evidence
        // needs.
        char described[128];
        const forgeshape::SceneObject& body = forgeshape::constructionScene().activeBody();
        if (const forgeshape::ConstructionObject* source = body.constructionOrNull()) {
            describeSpec(source->spec(), described, sizeof(described));
            FS_LOGI("FORGESHAPE_SCULPT_FROZEN:%u:%u sculptRev=%llu objectId=%llu from kind=%s "
                    "%s",
                    mesh.vertexCount(), mesh.indexCount(), (unsigned long long)mesh.revision(),
                    (unsigned long long)mesh.objectId(),
                    forgeshape::primitiveKindName(source->kind()), described);
        } else {
            const forgeshape::ImportedMesh* imported = body.importedOrNull();
            FS_LOGI("FORGESHAPE_SCULPT_FROZEN:%u:%u sculptRev=%llu objectId=%llu from "
                    "representation=Imported sourceVertices=%u sourceTriangles=%u batches=%u",
                    mesh.vertexCount(), mesh.indexCount(), (unsigned long long)mesh.revision(),
                    (unsigned long long)mesh.objectId(),
                    imported != nullptr ? imported->vertexCount() : 0u,
                    imported != nullptr ? imported->triangleCount() : 0u,
                    imported != nullptr ? imported->batchCount() : 0u);
        }
    }
    const forgeshape::MeshRevision revision = publishSculptRepresentation(*session, "freeze");
    FS_LOGI("FORGESHAPE_SCULPT_MODE:sculpt meshRev=%llu", (unsigned long long)revision);
    logSculptState("freeze");
    return kSculptOk;
}
// Leaves Sculpt mode and republishes the active body's SOURCE representation,
// so what is on screen is the original object, unsculpted. The Frozen Sculpt
// Mesh is kept, untouched.
//
// Which source that is comes from `publishConstructionObject`, which has
// dispatched per representation since `IMPORT-01A`: a Construction Body
// regenerates its primitive, an Imported Mesh republishes the geometry it owns.
// The user reads this as Back to Construction or Back to Imported Mesh; the act
// below is the same one either way.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_enterConstructionMode(JNIEnv*, jclass) {
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::sculptSession().enterConstruction();
        g_grabbing = false;
        g_strokePending = false;
    }
    const forgeshape::MeshRevision revision = publishConstructionObject("mode_construction");
    FS_LOGI("FORGESHAPE_SCULPT_MODE:construction meshRev=%llu", (unsigned long long)revision);
    logSculptState("mode_construction");
    return kSculptOk;
}

// Returns to Sculpt mode WITHOUT re-freezing, so prior edits come back exactly
// as they were. Refuses when nothing has ever been frozen.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_enterSculptMode(JNIEnv*, jclass) {
    bool entered = false;
    forgeshape::SculptSession* session = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        session = &forgeshape::sculptSession();
        entered = session->enterSculpt();
        g_grabbing = false;
        g_strokePending = false;
    }
    if (!entered) {
        FS_LOGI("FORGESHAPE_SCULPT_MODE_REFUSED:nothing_frozen");
        return kSculptNothingFrozen;
    }
    const forgeshape::MeshRevision revision =
        publishSculptRepresentation(*session, "mode_sculpt");
    FS_LOGI("FORGESHAPE_SCULPT_MODE:sculpt meshRev=%llu", (unsigned long long)revision);
    logSculptState("mode_sculpt");
    return kSculptOk;
}

// ---------------------------------------------------------------------------
// The scene: several Construction Bodies
// ---------------------------------------------------------------------------
//
// The Java layer holds NO model selection and no body list of its own. It asks
// how many bodies there are, what their ids are in scene order, and which one
// is active; every answer comes from here. That is what keeps the Objects list,
// the Property Inspector and the viewport from ever disagreeing.

JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_sceneBodyCount(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jint>(forgeshape::constructionScene().bodyCount());
}

// Fills `outIds` with every body's ObjectId in SCENE ORDER (insertion order),
// and returns how many were written. Order is the contract: the Objects list
// must not reshuffle when something is edited or selected.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sceneBodyIds(JNIEnv* env, jclass, jlongArray outIds) {
    if (outIds == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_stateMutex);
    forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
    const jsize capacity = env->GetArrayLength(outIds);
    const jsize count =
        std::min<jsize>(capacity, static_cast<jsize>(scene.bodyCount()));
    for (jsize i = 0; i < count; ++i) {
        const jlong id = static_cast<jlong>(scene.bodyAt(static_cast<size_t>(i)).objectId());
        env->SetLongArrayRegion(outIds, i, 1, &id);
    }
    return static_cast<jint>(count);
}

JNIEXPORT jlong JNICALL Java_com_forgeshape_app_NativeViewport_sceneActiveBodyId(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jlong>(forgeshape::constructionScene().activeBodyId());
}

// The body's stored name, or the empty string when it has none.
//
// Only an Imported Mesh carries one: it arrives named by the file it came from,
// and a list of anonymous rows would have lost something the user could see. A
// Construction Body is still labelled from its ObjectId by the UI, exactly as
// before, and this product still has no Rename. The empty string is the Android
// layer's signal to use that label — not a name of its own.
JNIEXPORT jstring JNICALL
Java_com_forgeshape_app_NativeViewport_sceneBodyName(JNIEnv* env, jclass, jlong objectId) {
    std::string name;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SceneObject* body = forgeshape::constructionScene().findBody(
            static_cast<forgeshape::ObjectId>(objectId));
        if (body != nullptr) {
            name = body->name();
        }
    }
    return newJavaString(env, name);
}

// Whether a body's geometry came from a file rather than from parameters.
//
// The Android layer asks so it can withdraw the controls an imported body has
// no answer for — Shape, and Start Sculpting in `IMPORT-01A`. It is asked, not
// inferred from an empty name or a missing dimension: the representation is a
// domain fact and the UI reads it rather than guessing at it.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sceneBodyIsImported(JNIEnv*, jclass, jlong objectId) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const forgeshape::SceneObject* body =
        forgeshape::constructionScene().findBody(static_cast<forgeshape::ObjectId>(objectId));
    return (body != nullptr && body->isImported()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sceneActiveBodyIsImported(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
    return scene.hasProject() && scene.activeBody().isImported() ? JNI_TRUE : JNI_FALSE;
}

// Makes an existing body the edit target. Selection ONLY: it publishes nothing,
// mints no revision, uploads nothing and cannot change any ObjectId.
//
// Refused while the active body is in Sculpt mode. Stage 017's deliberate
// policy is that the Sculpt target is fixed for the duration of Sculpt mode and
// the user returns to Construction to change bodies; allowing a switch mid-mode
// would mean deciding what happens to a half-finished stroke, which is a
// question this stage does not need to answer.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sceneSelectBody(JNIEnv*, jclass, jlong objectId) {
    bool selected = false;
    bool refusedInSculpt = false;
    bool refusedInSketch = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (forgeshape::sculptSession().inSculptMode()) {
            refusedInSculpt = true;
        } else if (forgeshape::sketchSession().active()) {
            // The scene holds still while a sketch is open, on the same terms
            // Sculpt fixes its target: the sketch commits as a NEW body, and
            // the body it lands beside is decided when it lands.
            refusedInSketch = true;
        } else {
            selected = forgeshape::constructionScene().setActiveBody(
                static_cast<forgeshape::ObjectId>(objectId));
        }
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_SCENE_SELECT_REFUSED:in_sculpt_mode");
        return kSculptFailedFreeze;
    }
    if (refusedInSketch) {
        FS_LOGI("FORGESHAPE_SCENE_SELECT_REFUSED:in_sketch");
        return kSculptFailedFreeze;
    }
    if (!selected) {
        FS_LOGI("FORGESHAPE_SCENE_SELECT_REFUSED:unknown_body:%lld", (long long)objectId);
        return kSculptNothingFrozen;
    }
    // Deliberately no publish here: the newly active body already has its own
    // current revision in its own store, so switching the edit target costs no
    // geometry work at all.
    FS_LOGI("FORGESHAPE_SCENE_ACTIVE_BODY:%lld", (long long)objectId);
    return kSculptOk;
}

// Adds a Construction Body with the same defaults as the startup body, appends
// it, and makes it active. Its Construction mesh is published immediately, so
// it is visible and pickable straight away.
JNIEXPORT jlong JNICALL Java_com_forgeshape_app_NativeViewport_sceneAddBody(JNIEnv*, jclass) {
    forgeshape::ObjectId created = forgeshape::kNoObject;
    bool refusedInSculpt = false;
    bool refusedInSketch = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (forgeshape::sculptSession().inSculptMode()) {
            refusedInSculpt = true;
        } else if (forgeshape::sketchSession().active()) {
            refusedInSketch = true;
        } else {
            // A bare append is its own step. The product's creation flow opens
            // an edit around this call AND the primitive apply that follows it,
            // so there the two become one step and an undo of "add a Sphere"
            // cannot leave the default Box the append produced on its own.
            forgeshape::ScopedConstructionEdit edit(forgeshape::constructionHistory());
            created = forgeshape::constructionScene().addBody().objectId();
        }
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_SCENE_ADD_REFUSED:in_sculpt_mode");
        return static_cast<jlong>(forgeshape::kNoObject);
    }
    if (refusedInSketch) {
        // A sketch is a creation in progress; a second one beside it would be
        // two acts sharing one moment. The control is withdrawn while
        // sketching; this is the guard behind it.
        FS_LOGI("FORGESHAPE_SCENE_ADD_REFUSED:in_sketch");
        return static_cast<jlong>(forgeshape::kNoObject);
    }
    // Outside the lock: the new body is already in the scene and selected, its
    // parameters are the untouched defaults, and the only thing publication
    // changes is its own MeshStore, which has a mutex of its own. That is a
    // fact about THIS call, not a rule about generation: applyPrimitive,
    // runHistoryStep and loadProject all generate under g_stateMutex, because
    // there the parameters being generated FROM are what another thread could
    // otherwise read half-written.
    const forgeshape::MeshRevision revision = publishConstructionObject("body_added");
    FS_LOGI("FORGESHAPE_SCENE_BODY_ADDED:%llu meshRev=%llu bodies=%d",
            (unsigned long long)created, (unsigned long long)revision,
            (int)forgeshape::constructionScene().bodyCount());
    return static_cast<jlong>(created);
}

// ---------------------------------------------------------------------------

// Delete status codes handed back to the Android UI. A JNI transport detail, in
// step with NativeViewport's DELETE_* fields; the domain's own vocabulary is
// DeleteBodyStatus.
constexpr jint kDeleteOk = 0;
constexpr jint kDeleteUnknownBody = 1;
constexpr jint kDeleteRefusedLastBody = 2;
constexpr jint kDeleteRefusedEditInProgress = 3;
constexpr jint kDeleteRefusedInSculpt = 4;
constexpr jint kDeleteRefusedHasDependents = 5;

// Removes one body from the project (`UI-OWNER-45`).
//
// The whole decision -- the transaction, holding the removed body so an undo
// can put THAT object back, the replacement selection and the last-body refusal
// -- is `deleteSceneBody`'s, in platform-neutral code. What is here is the lock,
// the mode guard and the log line.
//
// REFUSED WHILE SCULPTING, on the same terms as body switching and Undo/Redo:
// the Sculpt target is fixed for the duration of Sculpt mode, and a delete
// there would remove the very mesh a stroke may be running on AND could not be
// undone until the user left the mode, because Undo is refused there too. The
// control is withdrawn in Sculpt as well; removing a control is not removing a
// guard.
//
// A refusal changes nothing at all: no body leaves the scene, no step is
// recorded, no ObjectId moves and the project fingerprint does not shift.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sceneDeleteBody(JNIEnv*, jclass, jlong objectId) {
    forgeshape::DeleteBodyStatus status = forgeshape::DeleteBodyStatus::Ok;
    forgeshape::DeleteBodyReport report;
    bool refusedInSculpt = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (forgeshape::sculptSession().inSculptMode() || forgeshape::sketchSession().active()) {
            // Refused while sketching on the same terms as while sculpting:
            // the scene holds still until the sketch commits or is cancelled,
            // and the control is withdrawn there too.
            refusedInSculpt = true;
        } else {
            status = forgeshape::deleteSceneBody(static_cast<forgeshape::ObjectId>(objectId),
                                                 forgeshape::constructionScene(),
                                                 forgeshape::constructionHistory(), &report);
        }
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_SCENE_DELETE_REFUSED:in_sculpt_mode:%lld", (long long)objectId);
        return kDeleteRefusedInSculpt;
    }
    if (status != forgeshape::DeleteBodyStatus::Ok) {
        FS_LOGI("FORGESHAPE_SCENE_DELETE_REFUSED:%s:%lld",
                forgeshape::deleteBodyStatusName(status), (long long)objectId);
        switch (status) {
            case forgeshape::DeleteBodyStatus::UnknownBody: return kDeleteUnknownBody;
            case forgeshape::DeleteBodyStatus::RefusedLastBody: return kDeleteRefusedLastBody;
            case forgeshape::DeleteBodyStatus::RefusedEditInProgress:
                return kDeleteRefusedEditInProgress;
            case forgeshape::DeleteBodyStatus::RefusedHasDependents:
                return kDeleteRefusedHasDependents;
            case forgeshape::DeleteBodyStatus::Ok: break;
        }
        return kDeleteUnknownBody;
    }
    // Nothing is published: the bodies that remain already hold their own
    // current revisions in their own stores, and the one that left is simply
    // absent from the next snapshot. A delete costs no geometry work at all.
    FS_LOGI("FORGESHAPE_SCENE_BODY_DELETED:%llu index=%d bodies=%d active=%lld",
            (unsigned long long)report.removedBodyId, (int)report.removedIndex,
            (int)report.bodyCount, (long long)report.activeBodyId);
    return kDeleteOk;
}

// ---------------------------------------------------------------------------
// The object commands: Rename, Show/Hide, Lock/Unlock, Duplicate (Stage 018A)
// and Mirror (`MIRROR-01`, below)
// ---------------------------------------------------------------------------
//
// The whole decision for each -- the transaction, the sanitizer, the refusals,
// what a duplicate copies -- is `forgeshape_body_commands.{h,cpp}`'s, in
// platform-neutral code. What is here is the lock, the mode guard, the string
// boundary and the log line, exactly as `sceneDeleteBody` is.
//
// REFUSED WHILE SCULPTING AND WHILE SKETCHING, on the same terms body
// switching, Delete and Undo/Redo already are: the Sculpt target is fixed for
// the duration of the mode and the scene holds still while a sketch is open. As
// everywhere else in this product the control is withdrawn there too, and
// removing a control is not removing a guard.
//
// A refusal changes nothing at all: no name moves, no flag moves, no ObjectId
// is minted, no step is recorded and the project fingerprint does not shift.

// Status codes handed back to the Android UI, in step with NativeViewport's
// OBJCMD_* fields. A JNI transport detail, never a domain enum's ABI value.
constexpr jint kObjCmdOk = 0;
constexpr jint kObjCmdUnknownBody = 1;
constexpr jint kObjCmdRefusedEditInProgress = 2;
constexpr jint kObjCmdRefusedInvalidName = 3;
constexpr jint kObjCmdRefusedFaceSupportedCad = 4;
constexpr jint kObjCmdRefusedNotDuplicable = 5;
constexpr jint kObjCmdRefusedInSculpt = 6;
constexpr jint kObjCmdRefusedNotMirrorable = 7;
constexpr jint kObjCmdRefusedNotRepresentable = 8;

jint objCmdStatusToJni(forgeshape::BodyCommandStatus status) {
    switch (status) {
        case forgeshape::BodyCommandStatus::Ok: return kObjCmdOk;
        case forgeshape::BodyCommandStatus::UnknownBody: return kObjCmdUnknownBody;
        case forgeshape::BodyCommandStatus::RefusedEditInProgress:
            return kObjCmdRefusedEditInProgress;
        case forgeshape::BodyCommandStatus::RefusedInvalidName:
            return kObjCmdRefusedInvalidName;
        case forgeshape::BodyCommandStatus::RefusedFaceSupportedCad:
            return kObjCmdRefusedFaceSupportedCad;
        case forgeshape::BodyCommandStatus::RefusedNotDuplicable:
            return kObjCmdRefusedNotDuplicable;
        case forgeshape::BodyCommandStatus::RefusedNotMirrorable:
            return kObjCmdRefusedNotMirrorable;
        case forgeshape::BodyCommandStatus::RefusedNotRepresentable:
            return kObjCmdRefusedNotRepresentable;
    }
    return kObjCmdUnknownBody;
}

// Whether an object command may run at all right now. One answer for all of
// them, because they all hold the scene still for the same reason.
bool objectCommandsBlockedByMode() {
    return forgeshape::sculptSession().inSculptMode() || forgeshape::sketchSession().active();
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sceneRenameBody(JNIEnv* env, jclass, jlong objectId,
                                                       jstring name) {
    if (name == nullptr) {
        return kObjCmdRefusedInvalidName;
    }
    // The string is copied out of the JVM BEFORE the state mutex is taken: a
    // JNI call that can allocate must never run while the render thread is
    // waiting on that lock.
    const std::string requested = readJavaString(env, name);
    forgeshape::BodyCommandStatus status = forgeshape::BodyCommandStatus::Ok;
    bool refusedInSculpt = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (objectCommandsBlockedByMode()) {
            refusedInSculpt = true;
        } else {
            status = forgeshape::renameSceneBody(static_cast<forgeshape::ObjectId>(objectId),
                                                 requested, forgeshape::constructionScene(),
                                                 forgeshape::constructionHistory());
        }
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_SCENE_RENAME_REFUSED:in_sculpt_mode:%lld", (long long)objectId);
        return kObjCmdRefusedInSculpt;
    }
    if (status != forgeshape::BodyCommandStatus::Ok) {
        FS_LOGI("FORGESHAPE_SCENE_RENAME_REFUSED:%s:%lld",
                forgeshape::bodyCommandStatusName(status), (long long)objectId);
        return objCmdStatusToJni(status);
    }
    // Deliberately no publish: a name is truth about IDENTITY, and nothing
    // about the geometry, the mesh revision or the GPU changed.
    FS_LOGI("FORGESHAPE_SCENE_BODY_RENAMED:%lld", (long long)objectId);
    return kObjCmdOk;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sceneBodyVisible(JNIEnv*, jclass, jlong objectId) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const forgeshape::SceneObject* body =
        forgeshape::constructionScene().findBody(static_cast<forgeshape::ObjectId>(objectId));
    // An unknown body answers "visible": the Objects list only ever asks about
    // rows it just read from this same scene, and the visible answer is the one
    // that cannot make a row draw a Show control for something that is not
    // there.
    return (body == nullptr || body->visible()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sceneSetBodyVisible(JNIEnv*, jclass, jlong objectId,
                                                           jboolean visible) {
    forgeshape::BodyCommandStatus status = forgeshape::BodyCommandStatus::Ok;
    bool refusedInSculpt = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (objectCommandsBlockedByMode()) {
            refusedInSculpt = true;
        } else {
            status = forgeshape::setSceneBodyVisible(
                static_cast<forgeshape::ObjectId>(objectId), visible == JNI_TRUE,
                forgeshape::constructionScene(), forgeshape::constructionHistory());
        }
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_SCENE_VISIBILITY_REFUSED:in_sculpt_mode:%lld", (long long)objectId);
        return kObjCmdRefusedInSculpt;
    }
    if (status != forgeshape::BodyCommandStatus::Ok) {
        FS_LOGI("FORGESHAPE_SCENE_VISIBILITY_REFUSED:%s:%lld",
                forgeshape::bodyCommandStatusName(status), (long long)objectId);
        return objCmdStatusToJni(status);
    }
    // Nothing is published and nothing is uploaded. A hidden body simply is not
    // in the next snapshot, which is the one list the renderer and CPU picking
    // both read.
    FS_LOGI("FORGESHAPE_SCENE_BODY_VISIBILITY:%lld visible=%d", (long long)objectId,
            visible == JNI_TRUE ? 1 : 0);
    return kObjCmdOk;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sceneBodyLocked(JNIEnv*, jclass, jlong objectId) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const forgeshape::SceneObject* body =
        forgeshape::constructionScene().findBody(static_cast<forgeshape::ObjectId>(objectId));
    return (body != nullptr && body->locked()) ? JNI_TRUE : JNI_FALSE;
}

// Whether the body the editors currently act on is locked.
//
// Asked by the workspace so it can withdraw the transform controls, and asked
// by the two guards below so a locked body cannot be moved even if a control
// were somehow reached. Answers false while no project is open, which is the
// same shape every other active-body accessor uses at Home.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sceneActiveBodyIsLocked(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
    return scene.hasProject() && scene.activeBody().locked() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sceneSetBodyLocked(JNIEnv*, jclass, jlong objectId,
                                                          jboolean locked) {
    forgeshape::BodyCommandStatus status = forgeshape::BodyCommandStatus::Ok;
    bool refusedInSculpt = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (objectCommandsBlockedByMode()) {
            refusedInSculpt = true;
        } else {
            status = forgeshape::setSceneBodyLocked(
                static_cast<forgeshape::ObjectId>(objectId), locked == JNI_TRUE,
                forgeshape::constructionScene(), forgeshape::constructionHistory());
            const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
            if (status == forgeshape::BodyCommandStatus::Ok && scene.hasProject()
                && scene.activeBody().locked()) {
                // A gizmo standing over a body that has just been locked would
                // be a control that cannot succeed. Taken down here rather than
                // left to the next refresh, because `setActive(false)` also
                // cancels a captured handle -- and a finger already on one must
                // not keep dragging a body the user just locked.
                forgeshape::gizmoSession().setActive(false);
            }
        }
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_SCENE_LOCK_REFUSED:in_sculpt_mode:%lld", (long long)objectId);
        return kObjCmdRefusedInSculpt;
    }
    if (status != forgeshape::BodyCommandStatus::Ok) {
        FS_LOGI("FORGESHAPE_SCENE_LOCK_REFUSED:%s:%lld",
                forgeshape::bodyCommandStatusName(status), (long long)objectId);
        return objCmdStatusToJni(status);
    }
    FS_LOGI("FORGESHAPE_SCENE_BODY_LOCK:%lld locked=%d", (long long)objectId,
            locked == JNI_TRUE ? 1 : 0);
    return kObjCmdOk;
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sceneDuplicateBody(JNIEnv*, jclass, jlong objectId) {
    forgeshape::BodyCommandStatus status = forgeshape::BodyCommandStatus::Ok;
    forgeshape::DuplicateBodyReport report;
    bool refusedInSculpt = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (objectCommandsBlockedByMode()) {
            refusedInSculpt = true;
        } else {
            status = forgeshape::duplicateSceneBody(static_cast<forgeshape::ObjectId>(objectId),
                                                    forgeshape::constructionScene(),
                                                    forgeshape::constructionHistory(), &report);
        }
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_SCENE_DUPLICATE_REFUSED:in_sculpt_mode:%lld", (long long)objectId);
        return kObjCmdRefusedInSculpt;
    }
    if (status != forgeshape::BodyCommandStatus::Ok) {
        FS_LOGI("FORGESHAPE_SCENE_DUPLICATE_REFUSED:%s:%lld",
                forgeshape::bodyCommandStatusName(status), (long long)objectId);
        return objCmdStatusToJni(status);
    }
    FS_LOGI("FORGESHAPE_SCENE_BODY_DUPLICATED:%llu from=%llu index=%d bodies=%d sculpt=%d",
            (unsigned long long)report.newBodyId, (unsigned long long)report.sourceBodyId,
            (int)report.newIndex, (int)report.bodyCount, report.clonedSculptMesh ? 1 : 0);
    return kObjCmdOk;
}

// ---------------------------------------------------------------------------
// Mirror (`MIRROR-01`)
// ---------------------------------------------------------------------------
//
// The whole decision -- the eligibility, the reflection arithmetic, the
// transaction and what the new body carries -- belongs to
// `forgeshape_body_mirror.{h,cpp}` and `mirrorSceneBody`. What is here is the
// lock, the mode guard, the plane transport and the log line, exactly as every
// other object command is.
//
// REFUSED WHILE SCULPTING AND WHILE SKETCHING on the same terms as the other
// four, and the UI withdraws the control there too -- removing a control is not
// removing a guard.

// Whether this body is one Mirror could actually reflect right now.
//
// Asked per row so the control is ABSENT for an Imported Mesh, a CAD Body and a
// body carrying a sculpt mesh, rather than shown and then refused. The domain
// guard below stays regardless. Answers false for an unknown body and while the
// mode holds the scene still, which is the same shape every other row query
// uses.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sceneBodyCanMirror(JNIEnv*, jclass, jlong objectId) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (objectCommandsBlockedByMode()) {
        return JNI_FALSE;
    }
    const forgeshape::SceneObject* body =
        forgeshape::constructionScene().findBody(static_cast<forgeshape::ObjectId>(objectId));
    if (body == nullptr) {
        return JNI_FALSE;
    }
    return forgeshape::mirrorEligibilityOf(*body) == forgeshape::MirrorEligibility::Eligible
               ? JNI_TRUE
               : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sceneMirrorBody(JNIEnv*, jclass, jlong objectId,
                                                       jint planeIndex) {
    forgeshape::MirrorPlane plane = forgeshape::MirrorPlane::Xy;
    if (!forgeshape::mirrorPlaneFromIndex(static_cast<int>(planeIndex), &plane)) {
        // A transport value the UI never sends. Refused as unrepresentable
        // rather than defaulted to a plane the user did not choose.
        FS_LOGI("FORGESHAPE_SCENE_MIRROR_REFUSED:invalid_plane:%d", (int)planeIndex);
        return kObjCmdRefusedNotRepresentable;
    }
    forgeshape::BodyCommandStatus status = forgeshape::BodyCommandStatus::Ok;
    forgeshape::MirrorBodyReport report;
    bool refusedInSculpt = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (objectCommandsBlockedByMode()) {
            refusedInSculpt = true;
        } else {
            status = forgeshape::mirrorSceneBody(static_cast<forgeshape::ObjectId>(objectId),
                                                 plane, forgeshape::constructionScene(),
                                                 forgeshape::constructionHistory(), &report);
        }
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_SCENE_MIRROR_REFUSED:in_sculpt_mode:%lld", (long long)objectId);
        return kObjCmdRefusedInSculpt;
    }
    if (status != forgeshape::BodyCommandStatus::Ok) {
        // The eligibility is named beside the status, because one status covers
        // three different reasons a body cannot be reflected.
        FS_LOGI("FORGESHAPE_SCENE_MIRROR_REFUSED:%s:%s:%lld",
                forgeshape::bodyCommandStatusName(status),
                forgeshape::mirrorEligibilityName(report.eligibility), (long long)objectId);
        return objCmdStatusToJni(status);
    }
    FS_LOGI("FORGESHAPE_SCENE_BODY_MIRRORED:%llu from=%llu plane=%s index=%d bodies=%d",
            (unsigned long long)report.newBodyId, (unsigned long long)report.sourceBodyId,
            forgeshape::mirrorPlaneName(plane), (int)report.newIndex, (int)report.bodyCount);
    return kObjCmdOk;
}

// ---------------------------------------------------------------------------
// Construction history
// ---------------------------------------------------------------------------
//
// The Java layer holds NO history. It asks whether an undo is available, asks
// for one, and re-reads. There is no Java-side depth counter, no mirror scene
// and no list of parameter snapshots: every answer comes from the one native
// owner, which is what keeps the enabled state of a control and what the model
// actually is from ever disagreeing.
//
// TWO HISTORIES, ONE PAIR OF CONTROLS (`ARCH-OWNER-12`)
// -----------------------------------------------------
// There are two entirely separate histories below this boundary — the
// Construction history for project/object truth, and each body's own Sculpt
// history for stroke geometry — and there are three families of entry point
// here rather than one, so nothing has to guess which is meant:
//
//   constructionUndo* / constructionRedo*  ALWAYS the Construction history, and
//                                          still REFUSED in Sculpt. The guard
//                                          did not move: a project act that
//                                          could fire while sculpting is still
//                                          a defect, and this is where that is
//                                          stated.
//   sculptUndo* / sculptRedo*              ALWAYS the active body's Sculpt
//                                          history, and only in Sculpt mode.
//   historyUndo* / historyRedo*            The CHROME's entry points, which
//                                          dispatch on the product mode.
//
// The dispatch is native because the mode is native. Letting Java choose which
// history a button means would put the one decision that must never be wrong
// in the layer that holds no state to decide it with.

// History status codes handed back to the Android UI. A JNI transport detail,
// in step with NativeViewport's HISTORY_* fields.
constexpr jint kHistoryOk = 0;
constexpr jint kHistoryNothingToDo = 1;
// Returned by the Construction entry points in Sculpt mode. The dispatching
// entry points never return it: in Sculpt they mean the Sculpt history, so
// there is nothing to refuse.
constexpr jint kHistoryRefusedInSculpt = 2;
// A stroke is in progress. Temporary and self-clearing: the finger lifts and
// the step is available.
constexpr jint kHistoryStrokeActive = 3;
// Asked for a Sculpt step where there is no Sculpt history to step — not in
// Sculpt mode, or the active body has no Frozen Sculpt Mesh.
constexpr jint kHistoryUnavailable = 4;
// Not returned by a step. It names the one condition a step cannot report
// because it happened earlier: a stroke too large for
// `kMaxSculptHistoryEntryBytes` applied but was not retained, so it cannot be
// taken back. Surfaced through sculptHistoryNotRetainedCount().
constexpr jint kHistoryEntryNotRetained = 5;
// A History navigator jump named a state that is not on the retained branch
// (`SCULPT-H1`). Only the jump entry point returns it.
constexpr jint kHistoryOutOfRange = 6;
// Clear Mask on a mask whose single entry would exceed the per-entry byte cap
// (`SCULPT-FCM-R1`). REFUSED: nothing was cleared. Only sculptClearMask
// returns it.
constexpr jint kHistoryEntryTooLarge = 7;

// Turns a domain refusal into the transport code for it. One mapping, so a new
// domain status cannot quietly arrive as a generic failure.
static jint sculptHistoryStatusCode(forgeshape::SculptSession::SculptHistoryStatus status) {
    using Status = forgeshape::SculptSession::SculptHistoryStatus;
    switch (status) {
        case Status::Ok:
            return kHistoryOk;
        case Status::NothingToDo:
            return kHistoryNothingToDo;
        case Status::StrokeActive:
            return kHistoryStrokeActive;
        case Status::OutOfRange:
            return kHistoryOutOfRange;
        case Status::EntryTooLarge:
            return kHistoryEntryTooLarge;
        case Status::NotSculpting:
        case Status::NoSculptMesh:
            return kHistoryUnavailable;
    }
    return kHistoryUnavailable;
}

// Runs one Sculpt history step and republishes what it produced.
//
// Under `g_stateMutex` for exactly the reasons a stroke's own position writes
// are: this writes sculpt vertices and then publishes them, and the render
// thread takes its whole-scene snapshot under the same lock. A step is one
// discrete user act, so the cost is one publication.
static jint runSculptHistoryStep(const char* label, bool forward) {
    forgeshape::SculptSession::SculptHistoryStatus status;
    forgeshape::SculptRevision revision = 0;
    forgeshape::MeshRevision meshRevision = forgeshape::kNoMeshRevision;
    size_t undoDepth = 0;
    size_t redoDepth = 0;
    bool edits = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SculptSession& session = forgeshape::sculptSession();
        status = forward ? session.redoStroke() : session.undoStroke();
        if (status == forgeshape::SculptSession::SculptHistoryStatus::Ok) {
            // The ordinary sculpt publication path, unchanged. A history step
            // reaches the renderer exactly the way a stroke does, so there is
            // no second way for sculpt geometry to become a frame.
            meshRevision = publishSculptRepresentation(session, label);
            revision = session.mesh().revision();
            edits = session.mesh().hasEdits();
        }
        undoDepth = session.history().undoDepth();
        redoDepth = session.history().redoDepth();
    }
    if (status != forgeshape::SculptSession::SculptHistoryStatus::Ok) {
        FS_LOGI("FORGESHAPE_SCULPT_HISTORY_REFUSED:%s:%s", label,
                forgeshape::SculptSession::sculptHistoryStatusName(status));
        return sculptHistoryStatusCode(status);
    }
    FS_LOGI("FORGESHAPE_SCULPT_HISTORY:%s sculptRevision=%llu meshRevision=%llu edits=%d "
            "undo=%d redo=%d",
            label, (unsigned long long)revision, (unsigned long long)meshRevision, edits ? 1 : 0,
            (int)undoDepth, (int)redoDepth);
    return kHistoryOk;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_constructionUndoAvailable(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::constructionHistory().canUndo() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_constructionRedoAvailable(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::constructionHistory().canRedo() ? JNI_TRUE : JNI_FALSE;
}

// The active body's CURRENT mesh revision.
//
// A read-back diagnostic, in the same spirit as `constructionPrimitive`: the
// Java layer keeps no revision and derives nothing from it. It exists so that
// verification can state plainly how many publications one product act cost —
// which is the only way "wrapping an Apply in a transaction did not add a second
// rebuild" is a checkable claim rather than an assertion.
JNIEXPORT jlong JNICALL
Java_com_forgeshape_app_NativeViewport_constructionMeshRevision(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jlong>(forgeshape::meshStore().currentRevision());
}

// ---------------------------------------------------------------------------
// The Construction Move / Rotate gizmo
// ---------------------------------------------------------------------------
//
// Java owns WHEN there is a gizmo — which product mode, which Tool Rail
// context, whether a body exists — because those are workspace facts. It owns
// nothing about WHERE the handles are, how large they are on screen, which one a
// touch landed on, what a drag means in world space, or when a transaction opens
// and closes: every one of those needs the camera, the projection and the
// Construction placement, and there is exactly one owner of each.
//
// There is deliberately no Java-side transform, no parallel pivot and no second
// solver. A drag writes the authoritative ConstructionTransform directly, so the
// renderer, the picker and the exact-value editors read the same numbers
// mid-drag that they read at rest.

// Whether the workspace is currently offering direct transform. Refused with no
// effect while sculpting, which is a guard and not a UI decision: the workspace
// also withdraws the controls there, and removing a control is not removing a
// guard.
JNIEXPORT void JNICALL Java_com_forgeshape_app_NativeViewport_setGizmoActive(JNIEnv*, jclass,
                                                                            jboolean active) {
    bool refusedInSculpt = false;
    bool refusedLocked = false;
    bool nowActive = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
        const bool activeIsLocked = scene.hasProject() && scene.activeBody().locked();
        if (active == JNI_TRUE && forgeshape::sculptSession().inSculptMode()) {
            refusedInSculpt = true;
            // Still turned OFF, so entering Sculpt with a gizmo up cannot leave
            // one standing — and a captured handle is cancelled by setActive.
            forgeshape::gizmoSession().setActive(false);
        } else if (active == JNI_TRUE && activeIsLocked) {
            // Stage 018A. The SECOND of lock's two guards, covering direct
            // manipulation the way the transform entry point covers typed
            // values. Turned OFF rather than merely not turned on, on exactly
            // the Sculpt branch's terms: a gizmo left standing over a locked
            // body would be a control that cannot succeed, and `setActive(false)`
            // also cancels a captured handle so a finger already on one stops.
            refusedLocked = true;
            forgeshape::gizmoSession().setActive(false);
        } else {
            forgeshape::gizmoSession().setActive(active == JNI_TRUE);
        }
        nowActive = forgeshape::gizmoSession().active();
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_GIZMO_REFUSED:in_sculpt_mode");
        return;
    }
    if (refusedLocked) {
        FS_LOGI("FORGESHAPE_GIZMO_REFUSED:body_locked");
        return;
    }
    FS_LOGI("FORGESHAPE_GIZMO_ACTIVE:%d", nowActive ? 1 : 0);
}

// Move or Rotate. Presentation state: no revision, no publication, no history.
// Returns false for an unknown index and while a drag is captured — a mode must
// not change under a moving finger.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_setGizmoMode(JNIEnv*, jclass, jint modeIndex) {
    forgeshape::GizmoMode mode;
    if (!forgeshape::gizmoModeFromIndex(static_cast<int>(modeIndex), &mode)) {
        FS_LOGE("FORGESHAPE_GIZMO_MODE_REJECTED:%d", (int)modeIndex);
        return JNI_FALSE;
    }
    bool accepted = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        accepted = forgeshape::gizmoSession().setMode(mode);
    }
    FS_LOGI("FORGESHAPE_GIZMO_MODE:%s accepted=%d", forgeshape::gizmoModeName(mode),
            accepted ? 1 : 0);
    return accepted ? JNI_TRUE : JNI_FALSE;
}

// How many physical pixels one reference unit is on this display. The one number
// the platform adapter owns about gizmo size; the sizes themselves are the
// domain's, in forgeshape_gizmo.h, so the 48-unit hit floor is a property of the
// product and not of a layout file.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_setGizmoPixelScale(JNIEnv*, jclass, jfloat scale) {
    const bool accepted = forgeshape::setGizmoPixelsPerReferenceUnit(scale);
    if (!accepted) {
        FS_LOGE("FORGESHAPE_GIZMO_SCALE_REJECTED:%.4f", (double)scale);
    }
    return accepted ? JNI_TRUE : JNI_FALSE;
}

// The VISUAL SIZE preference (UI-PREF-R1 E): how large the instrument is drawn
// and placed, as a bounded multiplier the Android layer persists. Presentation
// on the mode's terms — no revision, no publication, no history, no change to
// what a drag does — and refused outside the domain's bounds rather than
// clamped, leaving the current size standing. Taken under the state lock like
// the mode, because the session is.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_setGizmoVisualScale(JNIEnv*, jclass, jfloat scale) {
    bool accepted = false;
    float inEffect = forgeshape::kGizmoDefaultVisualScale;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        accepted = forgeshape::gizmoSession().setVisualScale(static_cast<float>(scale));
        inEffect = forgeshape::gizmoSession().visualScale();
    }
    FS_LOGI("FORGESHAPE_GIZMO_VISUAL_SCALE:%.3f requested=%.3f accepted=%d", (double)inEffect,
            (double)scale, accepted ? 1 : 0);
    return accepted ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloat JNICALL
Java_com_forgeshape_app_NativeViewport_gizmoVisualScale(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jfloat>(forgeshape::gizmoSession().visualScale());
}

// The STROKE WEIGHT preference (UI-PREF-R1 F), on the display store's terms:
// no lock, a closed index that is refused if unrecognised, and the answer is
// what is actually in effect afterwards. Its one consequence is that the
// renderer re-uploads the gizmo's canonical vertex list before its next frame.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_setGizmoStrokeWeight(JNIEnv*, jclass, jint weightIndex) {
    forgeshape::DisplaySettingsStore& settings = forgeshape::displaySettings();
    forgeshape::GizmoStrokeWeight requested = settings.gizmoStrokeWeight();
    const bool known =
        forgeshape::gizmoStrokeWeightFromIndex(static_cast<int>(weightIndex), &requested);
    bool changed = false;
    if (known) {
        changed = settings.setGizmoStrokeWeight(requested);
    }
    FS_LOGI("FORGESHAPE_GIZMO_STROKE_WEIGHT:%s requested=%d known=%d changed=%d",
            forgeshape::gizmoStrokeWeightName(settings.gizmoStrokeWeight()),
            static_cast<int>(weightIndex), known ? 1 : 0, changed ? 1 : 0);
    return static_cast<jint>(forgeshape::gizmoStrokeWeightIndex(settings.gizmoStrokeWeight()));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_gizmoStrokeWeight(JNIEnv*, jclass) {
    return static_cast<jint>(
        forgeshape::gizmoStrokeWeightIndex(forgeshape::displaySettings().gizmoStrokeWeight()));
}

// World or Local. Presentation state on the same terms as the mode: no
// revision, no publication, no history. Returns false for an unknown index,
// while a drag is captured, and for World while the mode is Scale — a
// world-axis scale of a rotated body is a shear, so the workspace withdraws the
// selector there and this guard stays regardless.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_setGizmoSpace(JNIEnv*, jclass, jint spaceIndex) {
    forgeshape::GizmoSpace space;
    if (!forgeshape::gizmoSpaceFromIndex(static_cast<int>(spaceIndex), &space)) {
        FS_LOGE("FORGESHAPE_GIZMO_SPACE_REJECTED:%d", (int)spaceIndex);
        return JNI_FALSE;
    }
    bool accepted = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        accepted = forgeshape::gizmoSession().setSpace(space);
    }
    FS_LOGI("FORGESHAPE_GIZMO_SPACE:%s accepted=%d", forgeshape::gizmoSpaceName(space),
            accepted ? 1 : 0);
    return accepted ? JNI_TRUE : JNI_FALSE;
}

// Reads the authoritative gizmo state for display and verification. Nothing here
// is a second truth: every value is read back from the one session.
//
//   [0] 1 when the gizmo is offered at all
//   [1] mode index (0 move, 1 rotate, 2 scale)
//   [2] 1 when a drag is capturing a pointer
//   [3] captured pointer id, or -1
//   [4] captured handle code (0 none, 1..3 axis X/Y/Z, 4..6 plane XY/XZ/YZ,
//       7 uniform)
//   [5] captured ObjectId, or 0
//   [6] how many updates the current or last drag applied — diagnostic, and how
//       "a drag of any length is one step" is checked rather than asserted
//   [7] 1 when the gizmo is currently visible for the active body
//   [8] the world length of one reference unit at the pivot, or 0
//   [9] how many drags have committed a history step this session — monotone,
//       so the shell can tell "the model moved under the finger" from "the
//       camera orbited" with one comparison
//   [10] space index (0 world, 1 local)
//   [11] 1 when the space is the user choice rather than a consequence of the
//        mode — the shell draws the space selector exactly when this is 1
JNIEXPORT void JNICALL Java_com_forgeshape_app_NativeViewport_gizmoState(JNIEnv* env, jclass,
                                                                        jdoubleArray out) {
    constexpr jsize kGizmoStateSize = 12;
    if (out == nullptr || env->GetArrayLength(out) < kGizmoStateSize) {
        return;
    }
    jdouble values[kGizmoStateSize];
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::GizmoSession& gizmo = forgeshape::gizmoSession();
        const forgeshape::GizmoSnapshot state = gizmo.snapshot(
            g_camera.snapshot(), g_camera.viewportWidth(), g_camera.viewportHeight());
        values[0] = gizmo.active() ? 1.0 : 0.0;
        values[1] = static_cast<double>(forgeshape::gizmoModeIndex(gizmo.mode()));
        values[2] = gizmo.capturing() ? 1.0 : 0.0;
        values[3] = static_cast<double>(gizmo.capturedPointerId());
        values[4] = static_cast<double>(forgeshape::gizmoHandleCode(gizmo.capturedHandle()));
        values[5] = static_cast<double>(gizmo.capturedObjectId());
        values[6] = static_cast<double>(gizmo.dragUpdateCount());
        values[7] = state.visible ? 1.0 : 0.0;
        values[8] = static_cast<double>(state.worldPerReferenceUnit);
        values[9] = static_cast<double>(gizmo.committedDragCount());
        values[10] = static_cast<double>(forgeshape::gizmoSpaceIndex(gizmo.space()));
        values[11] = gizmo.spaceIsSelectable() ? 1.0 : 0.0;
    }
    env->SetDoubleArrayRegion(out, 0, kGizmoStateSize, values);
}

// Which handle a pixel would grab, without grabbing it, as a GizmoHandle code.
//
// Pure, and that is the point — the workspace own verification can ask where a
// handle IS without starting a transaction, which is what lets an instrumented
// case drive a real drag through the real gesture path instead of fabricating
// coordinates.
JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_gizmoHitTest(JNIEnv*, jclass,
                                                                          jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jint>(forgeshape::gizmoHandleCode(forgeshape::gizmoSession().hitTest(
        g_camera.snapshot(), x, y, g_camera.viewportWidth(), g_camera.viewportHeight())));
}

// Where a handle grabbable point is, in view-local pixels: the middle of the
// shaft grab span, a point on a ring away from the crossings, the centre of a
// plane square, or the pivot for the uniform handle.
//
// Verification needs SOME pixel to send a synthetic pointer to, and the only
// honest source of one is the same projection the hit test uses. Deriving it
// here rather than in a test is what keeps a case from encoding a coordinate
// that is true for one window and one camera — the rule the workspace chrome
// already follows with semantic ids.
//
// Returns false, writing nothing, when there is no such handle on screen.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_gizmoHandlePoint(JNIEnv* env, jclass, jint handleCode,
                                                        jfloatArray out) {
    if (out == nullptr || env->GetArrayLength(out) < 2) {
        return JNI_FALSE;
    }
    forgeshape::GizmoHandle handle = forgeshape::GizmoHandle::None;
    if (!forgeshape::gizmoHandleFromCode(static_cast<int>(handleCode), &handle)) {
        return JNI_FALSE;
    }
    float point[2] = {0.0f, 0.0f};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::CameraSnapshot camera = g_camera.snapshot();
        const int width = g_camera.viewportWidth();
        const int height = g_camera.viewportHeight();
        const forgeshape::GizmoSnapshot state =
            forgeshape::gizmoSession().snapshot(camera, width, height);
        if (state.visible) {
            forgeshape::Vec3 world{};
            // Code 0 is the PIVOT itself, which is not a handle and cannot be
            // grabbed. It is answerable because a caller driving a synthetic
            // drag needs to know which way along the SCREEN a handle actually
            // runs, and the honest answer is the projected direction from the
            // pivot to the handle — not a screen direction guessed from a name.
            const bool haveWorld =
                (handle == forgeshape::GizmoHandle::None)
                    ? (world = state.pivot, true)
                    : forgeshape::gizmoHandleGrabPoint(state, handle, &world);
            found = haveWorld && forgeshape::projectWorldToScreen(camera, world, width, height,
                                                                 &point[0], &point[1]);
        }
    }
    if (!found) {
        return JNI_FALSE;
    }
    env->SetFloatArrayRegion(out, 0, 2, point);
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// The sketch session and the CAD Body (`CAD-R0-A1A2`)
// ---------------------------------------------------------------------------
//
// The Java layer holds NO sketch. It asks the one native session what state
// it is in, which tool is held, what is selected and what it would build, and
// every answer comes from here. Pointer samples reach the session through the
// ordinary touchEvent path; these entry points are the chrome's acts -- begin,
// cancel, tool, finish, profile, depth, commit -- and the exact-value edits.
//
// Every status crosses as a CadStatus code, and `cadStatusToken` turns one
// back into its name for a log line or a message. The codes are the enum's
// own order, stated in NativeViewport's CAD_* fields and checked by the
// instrumentation, so a new status cannot arrive as a silent zero.

static jint cadCode(forgeshape::CadStatus status) {
    return static_cast<jint>(forgeshape::cadStatusCode(status));
}

// Begins a sketch on one of the three planes. Refused while sculpting, while
// a Construction edit is open, and while one is already open. On success the
// gizmo is withdrawn, any tap candidate is dropped and the camera is framed
// on the plane; the user's own view is kept for the way back.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchBegin(JNIEnv*, jclass, jint planeIndex) {
    forgeshape::Workplane plane;
    if (!forgeshape::workplaneFromIndex(static_cast<int>(planeIndex), &plane)) {
        FS_LOGE("FORGESHAPE_SKETCH_BEGIN_REFUSED:InvalidWorkplane:%d", (int)planeIndex);
        return cadCode(forgeshape::CadStatus::InvalidWorkplane);
    }
    forgeshape::CadStatus status = forgeshape::CadStatus::Ok;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (forgeshape::sculptSession().inSculptMode()) {
            status = forgeshape::CadStatus::NotSketching;
        } else if (forgeshape::constructionHistory().editInProgress()) {
            status = forgeshape::CadStatus::RefusedEditInProgress;
        } else {
            status = forgeshape::sketchSession().begin(plane);
        }
        if (status == forgeshape::CadStatus::Ok) {
            // A by-name plane chosen while the spatial chooser was still
            // open supersedes it: the sketch owns the gesture from here, and
            // a chooser left active would take every touch first.
            forgeshape::supportChooser().cancel();
            forgeshape::gizmoSession().setActive(false);
            g_selection.resetGesture();
            g_camera.resetGesture();
            beginSketchView();
        }
    }
    if (status != forgeshape::CadStatus::Ok) {
        FS_LOGE("FORGESHAPE_SKETCH_BEGIN_REFUSED:%s", forgeshape::cadStatusName(status));
        return cadCode(status);
    }
    FS_LOGI("FORGESHAPE_SKETCH_BEGIN plane=%s tool=%s", forgeshape::workplaneName(plane),
            forgeshape::sketchToolName(forgeshape::sketchSession().tool()));
    return cadCode(status);
}

// ---------------------------------------------------------------------------
// Spatial "Choose Sketch Support" (CAD-A3)
// ---------------------------------------------------------------------------

namespace {
// Begins the sketch on the chooser's current selection and frames the camera
// exactly on it. The caller holds g_stateMutex. Shared by the JNI confirm and
// the confirm-on-reselect tap. Returns the CadStatus.
forgeshape::CadStatus confirmChosenSupportLocked() {
    const forgeshape::ChosenSupport& c = forgeshape::supportChooser().selected();
    forgeshape::CadStatus status = forgeshape::CadStatus::NotSketching;
    if (c.kind == forgeshape::ChosenSupport::Kind::WorldPlane) {
        status = forgeshape::sketchSession().begin(c.plane);
    } else if (c.kind == forgeshape::ChosenSupport::Kind::Face) {
        status = forgeshape::sketchSession().beginOnFace(c.worldFrame, c.faceRef);
    } else {
        return status;
    }
    if (status == forgeshape::CadStatus::Ok) {
        forgeshape::supportChooser().cancel();
        forgeshape::gizmoSession().setActive(false);
        g_selection.resetGesture();
        g_camera.resetGesture();
        beginSketchView();
    }
    return status;
}

// A ChosenSupport kind as a JNI code: -1 none, 0/1/2 world plane XY/XZ/YZ, 3 face.
jint supportKindCode(const forgeshape::ChosenSupport& c) {
    switch (c.kind) {
        case forgeshape::ChosenSupport::Kind::WorldPlane:
            return static_cast<jint>(forgeshape::workplaneIndex(c.plane));
        case forgeshape::ChosenSupport::Kind::Face:
            return 3;
        case forgeshape::ChosenSupport::Kind::None:
        default:
            return -1;
    }
}
}  // namespace

// Enters spatial support selection. allowFaces true for New Sketch inside a CAD
// project. Refused while sculpting or mid-edit (returns false).
JNIEXPORT jboolean JNICALL Java_com_forgeshape_app_NativeViewport_supportChooserBegin(
    JNIEnv*, jclass, jboolean allowFaces) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (forgeshape::sculptSession().inSculptMode() || forgeshape::sketchSession().active()
        || forgeshape::constructionHistory().editInProgress()) {
        return JNI_FALSE;
    }
    forgeshape::supportChooser().begin(allowFaces == JNI_TRUE);
    FS_LOGI("FORGESHAPE_SUPPORT_CHOOSER_BEGIN allowFaces=%d", allowFaces == JNI_TRUE ? 1 : 0);
    return JNI_TRUE;
}

// Stylus hover: highlights the target under the point, never selects it.
JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_supportChooserHover(
    JNIEnv*, jclass, jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!forgeshape::supportChooser().active()) return -1;
    const forgeshape::ChosenSupport at = forgeshape::supportChooser().hover(
        g_camera.snapshot(), x, y, g_camera.viewportWidth(), g_camera.viewportHeight(),
        forgeshape::constructionScene());
    return supportKindCode(at);
}

// A tap: selects the target under the point. Returns its kind code.
JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_supportChooserSelect(
    JNIEnv*, jclass, jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!forgeshape::supportChooser().active()) return -1;
    const forgeshape::ChosenSupport at = forgeshape::supportChooser().select(
        g_camera.snapshot(), x, y, g_camera.viewportWidth(), g_camera.viewportHeight(),
        forgeshape::constructionScene());
    FS_LOGI("FORGESHAPE_SUPPORT_CHOOSER_SELECT kind=%d", supportKindCode(at));
    return supportKindCode(at);
}

// The current selection's kind, or -1.
JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_supportChooserSelectedKind(
    JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return supportKindCode(forgeshape::supportChooser().selected());
}

// Confirms the current selection: begins the sketch on it and frames the camera
// EXACTLY normal to it. Returns the CadStatus code (Ok on success).
JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_supportChooserConfirm(
    JNIEnv*, jclass) {
    forgeshape::CadStatus status = forgeshape::CadStatus::NotSketching;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        status = confirmChosenSupportLocked();
    }
    if (status == forgeshape::CadStatus::Ok) {
        FS_LOGI("FORGESHAPE_SUPPORT_CHOOSER_CONFIRM ok");
    }
    return cadCode(status);
}

// Leaves support selection without starting a sketch. No project mutation.
JNIEXPORT void JNICALL Java_com_forgeshape_app_NativeViewport_supportChooserCancel(
    JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    forgeshape::supportChooser().cancel();
}

JNIEXPORT jboolean JNICALL Java_com_forgeshape_app_NativeViewport_supportChooserActive(
    JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::supportChooser().active() ? JNI_TRUE : JNI_FALSE;
}

// Read-only projection of a world point to screen through the current camera.
// A verification seam so a test can tap the exact pixel a plane or face target
// projects to; it mutates nothing.
JNIEXPORT jboolean JNICALL Java_com_forgeshape_app_NativeViewport_debugProjectWorld(
    JNIEnv* env, jclass, jdouble x, jdouble y, jdouble z, jfloatArray out) {
    if (out == nullptr || env->GetArrayLength(out) < 2) {
        return JNI_FALSE;
    }
    float sx = 0.0f;
    float sy = 0.0f;
    bool ok = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        ok = forgeshape::projectWorldToScreen(
            g_camera.snapshot(), forgeshape::Vec3{static_cast<float>(x), static_cast<float>(y),
                                                  static_cast<float>(z)},
            g_camera.viewportWidth(), g_camera.viewportHeight(), &sx, &sy);
    }
    if (!ok) {
        return JNI_FALSE;
    }
    jfloat vals[2] = {sx, sy};
    env->SetFloatArrayRegion(out, 0, 2, vals);
    return JNI_TRUE;
}

// Drops the sketch. Never a project mutation, and gives the view back.
JNIEXPORT void JNICALL Java_com_forgeshape_app_NativeViewport_sketchCancel(JNIEnv*, jclass) {
    bool wasActive = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        wasActive = forgeshape::sketchSession().active();
        forgeshape::sketchSession().cancel();
        endSketchView();
    }
    if (wasActive) {
        FS_LOGI("FORGESHAPE_SKETCH_CANCEL undo=%d bodies=%d",
                (int)forgeshape::constructionHistory().undoDepth(),
                (int)forgeshape::constructionScene().bodyCount());
    }
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sketchSetTool(JNIEnv*, jclass, jint toolIndex) {
    forgeshape::SketchTool tool;
    if (!forgeshape::sketchToolFromIndex(static_cast<int>(toolIndex), &tool)) {
        return JNI_FALSE;
    }
    bool accepted = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        accepted = forgeshape::sketchSession().setTool(tool);
    }
    FS_LOGI("FORGESHAPE_SKETCH_TOOL:%s accepted=%d", forgeshape::sketchToolName(tool),
            accepted ? 1 : 0);
    return accepted ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_sketchTool(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jint>(forgeshape::sketchToolIndex(forgeshape::sketchSession().tool()));
}

// The session, read back for display and verification. Layout matches
// NativeViewport's SKETCH_* slots:
//   [0] state (0 inactive, 1 editing, 2 ready)   [1] plane index
//   [2] tool index                                [3] entity count
//   [4] selected entity id                        [5] closed profile count
//   [6] chosen profile anchor id                  [7] extrude depth, metres
//   [8] extrude direction index                   [9] last status code
//   [10] 1 while a polyline is being placed       [11] entities placed by touch
//   [12] last snap kind (0 none, 1 grid, 2 endpoint)
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_sketchState(JNIEnv* env, jclass, jdoubleArray out) {
    constexpr jsize kSize = 13;
    if (out == nullptr || env->GetArrayLength(out) < kSize) {
        return;
    }
    jdouble values[kSize];
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SketchSession& s = forgeshape::sketchSession();
        values[0] = static_cast<double>(static_cast<int>(s.state()));
        values[1] = static_cast<double>(forgeshape::workplaneIndex(s.plane()));
        values[2] = static_cast<double>(forgeshape::sketchToolIndex(s.tool()));
        values[3] = static_cast<double>(s.sketch().entities.size());
        values[4] = static_cast<double>(s.selectedEntityId());
        values[5] = static_cast<double>(s.profiles().profiles.size());
        values[6] = static_cast<double>(s.selectedProfileId());
        values[7] = s.extrude().depth;
        values[8] = static_cast<double>(forgeshape::extrudeDirectionIndex(s.extrude().direction));
        values[9] = static_cast<double>(forgeshape::cadStatusCode(s.lastStatus()));
        values[10] = s.polylineInProgress() ? 1.0 : 0.0;
        values[11] = static_cast<double>(s.entitiesPlaced());
        values[12] = static_cast<double>(static_cast<int>(s.lastSnapKind()));
    }
    env->SetDoubleArrayRegion(out, 0, kSize, values);
}

// Editing -> Ready, or a named refusal that leaves the sketch editable.
JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_sketchFinish(JNIEnv*, jclass) {
    forgeshape::CadStatus status;
    size_t profiles = 0;
    forgeshape::SketchEntityId chosen = forgeshape::kNoSketchEntity;
    forgeshape::CadFeatureViewSource view = forgeshape::CadFeatureViewSource::Unavailable;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        status = forgeshape::sketchSession().finish();
        profiles = forgeshape::sketchSession().profiles().profiles.size();
        chosen = forgeshape::sketchSession().selectedProfileId();
        if (status == forgeshape::CadStatus::Ok) {
            // Inside the same lock as the transition it belongs to: the
            // session reaches Ready and the view it is adjusted through are
            // one moment, and a frame taken between the two would draw an
            // arrow nobody could grab.
            view = beginExtrudeFeatureView();
        }
    }
    if (status != forgeshape::CadStatus::Ok) {
        FS_LOGI("FORGESHAPE_SKETCH_FINISH_REFUSED:%s", forgeshape::cadStatusName(status));
    } else {
        FS_LOGI("FORGESHAPE_SKETCH_FINISH profiles=%d chosen=%u", (int)profiles, chosen);
    }
    if (status == forgeshape::CadStatus::Ok) {
        FS_LOGI("FORGESHAPE_SKETCH_FEATURE_VIEW:%s", forgeshape::cadFeatureViewSourceName(view));
    }
    return cadCode(status);
}

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_sketchBackToEditing(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    forgeshape::sketchSession().backToEditing();
    // Back to the authored sketch is back to the EXACT support-normal view
    // (`CAD-UX-S1-C1`). The feature preview belongs to the staged
    // extrusion; the drawing is authored through the aligned one, where a
    // sketch length on screen is the length it is.
    beginSketchView();
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchSelectProfile(JNIEnv*, jclass, jlong anchorId) {
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        status = forgeshape::sketchSession().selectProfile(
            static_cast<forgeshape::SketchEntityId>(anchorId));
    }
    FS_LOGI("FORGESHAPE_SKETCH_PROFILE:%lld %s", (long long)anchorId,
            forgeshape::cadStatusName(status));
    return cadCode(status);
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchSetExtrude(JNIEnv*, jclass, jdouble depthMeters,
                                                        jint directionIndex) {
    forgeshape::ExtrudeDirection direction;
    if (!forgeshape::extrudeDirectionFromIndex(static_cast<int>(directionIndex), &direction)) {
        return cadCode(forgeshape::CadStatus::InvalidExtrudeDirection);
    }
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        status = forgeshape::sketchSession().setExtrude(depthMeters, direction);
    }
    FS_LOGI("FORGESHAPE_SKETCH_EXTRUDE depth=%.6f direction=%s %s", (double)depthMeters,
            forgeshape::extrudeDirectionName(direction), forgeshape::cadStatusName(status));
    return cadCode(status);
}

// THE commit. Returns the new body's ObjectId, or 0 with the reason in
// sketchLastStatus(). One transaction, one Undo; the view is given back and
// the new body is active, published and on screen.
JNIEXPORT jlong JNICALL Java_com_forgeshape_app_NativeViewport_sketchCommit(JNIEnv*, jclass) {
    forgeshape::CadStatus status;
    forgeshape::ObjectId created = forgeshape::kNoObject;
    size_t undoDepth = 0;
    bool firstProject = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
        if (!scene.hasProject()) {
            // The CAD bootstrap (`APP-H1`): no project is open, so this commit
            // CREATES the first one rather than adding a body to it. Decided
            // here, from native truth, so the shell has one Extrude and no
            // second code path; see forgeshape_project_bootstrap.h.
            firstProject = true;
            forgeshape::FirstProjectReport report;
            status = forgeshape::commitFirstCadProject(
                forgeshape::sketchSession(), scene, forgeshape::sculptSession(),
                forgeshape::constructionHistory(), &report);
            created = report.bodyId;
            if (status == forgeshape::CadStatus::Ok) {
                // A first project starts from the product's own opening view,
                // not from the pose the sketch borrowed before there was
                // anything to look at.
                forgeshape::supportChooser().cancel();
                endSketchView();
            }
        } else {
            status = forgeshape::sketchSession().commit(scene, forgeshape::constructionHistory(),
                                                        &created);
            if (status == forgeshape::CadStatus::Ok) {
                endSketchView();
            }
        }
        undoDepth = forgeshape::constructionHistory().undoDepth();
    }
    if (status != forgeshape::CadStatus::Ok) {
        FS_LOGI("FORGESHAPE_SKETCH_COMMIT_REFUSED:%s%s", forgeshape::cadStatusName(status),
                firstProject ? " first_project" : "");
        return static_cast<jlong>(forgeshape::kNoObject);
    }
    if (firstProject) {
        FS_LOGI("FORGESHAPE_FIRST_PROJECT_CREATED objectId=%llu bodies=%d undo=%d",
                (unsigned long long)created, (int)forgeshape::constructionScene().bodyCount(),
                (int)undoDepth);
    }
    const forgeshape::SceneObject* body = forgeshape::constructionScene().findBody(created);
    const forgeshape::RuntimeMeshPtr published = body ? body->meshStore().current() : nullptr;
    FS_LOGI("FORGESHAPE_SKETCH_COMMIT objectId=%llu meshRev=%llu vertices=%u indices=%u undo=%d "
            "bodies=%d",
            (unsigned long long)created,
            (unsigned long long)(published ? published->revision() : 0ull),
            published ? published->vertexCount() : 0u, published ? published->indexCount() : 0u,
            (int)undoDepth, (int)forgeshape::constructionScene().bodyCount());
    return static_cast<jlong>(created);
}

JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_sketchLastStatus(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return cadCode(forgeshape::sketchSession().lastStatus());
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchDeleteSelected(JNIEnv*, jclass) {
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        status = forgeshape::sketchSession().deleteSelected();
    }
    FS_LOGI("FORGESHAPE_SKETCH_DELETE %s entities=%d", forgeshape::cadStatusName(status),
            (int)forgeshape::sketchSession().sketch().entities.size());
    return cadCode(status);
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sketchSelectEntity(JNIEnv*, jclass, jlong entityId) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (entityId == 0) {
        forgeshape::sketchSession().clearSelection();
        return JNI_TRUE;
    }
    return forgeshape::sketchSession().select(static_cast<forgeshape::SketchEntityId>(entityId))
               ? JNI_TRUE
               : JNI_FALSE;
}

// The selected entity's own values, in NativeViewport's SKETCH_ENTITY_* slots:
//   [0] id  [1] kind (0 line, 1 polyline, 2 rectangle, 3 circle, 4 arc,
//                     5 spline)
//   line:      [2] x0 [3] y0 [4] x1 [5] y1
//   rectangle: [2] cu [3] cv [4] width [5] height
//   circle:    [2] cu [3] cv [4] radius
//   polyline:  [2] vertex count [3] 1 when closed
//   arc:       [2] start u [3] start v [4] end u [5] end v
//   spline:    [2] point count [3] 0
// A curve reports what the shell needs to NAME it and to draw its selection;
// its authored points are edited in the sketch, not through a numeric field,
// so nothing here has to carry them all.
// Returns false, writing nothing, when nothing is selected.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sketchSelectedEntity(JNIEnv* env, jclass,
                                                            jdoubleArray out) {
    constexpr jsize kSize = 6;
    if (out == nullptr || env->GetArrayLength(out) < kSize) {
        return JNI_FALSE;
    }
    jdouble values[kSize] = {0, 0, 0, 0, 0, 0};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SketchSession& s = forgeshape::sketchSession();
        const forgeshape::SketchEntity* entity =
            forgeshape::findSketchEntity(s.sketch(), s.selectedEntityId());
        if (entity != nullptr) {
            found = true;
            values[0] = static_cast<double>(entity->id());
            values[1] = static_cast<double>(static_cast<int>(entity->kind()));
            if (const forgeshape::SketchLine* line = entity->line()) {
                values[2] = line->start.u;
                values[3] = line->start.v;
                values[4] = line->end.u;
                values[5] = line->end.v;
            } else if (const forgeshape::SketchRectangle* rectangle = entity->rectangle()) {
                values[2] = rectangle->center.u;
                values[3] = rectangle->center.v;
                values[4] = rectangle->width;
                values[5] = rectangle->height;
            } else if (const forgeshape::SketchCircle* circle = entity->circle()) {
                values[2] = circle->center.u;
                values[3] = circle->center.v;
                values[4] = circle->radius;
            } else if (const forgeshape::SketchPolyline* polyline = entity->polyline()) {
                values[2] = static_cast<double>(polyline->vertices.size());
                values[3] = polyline->closed ? 1.0 : 0.0;
            } else if (const forgeshape::SketchArc* arc = entity->arc()) {
                values[2] = arc->start.u;
                values[3] = arc->start.v;
                values[4] = arc->end.u;
                values[5] = arc->end.v;
            } else if (const forgeshape::SketchSpline* spline = entity->spline()) {
                values[2] = static_cast<double>(spline->points.size());
            }
        }
    }
    if (!found) {
        return JNI_FALSE;
    }
    env->SetDoubleArrayRegion(out, 0, kSize, values);
    return JNI_TRUE;
}

// Exact typed values for the selected entity, never snapped. Each keeps the
// entity's identity and what is not typed -- a rectangle's centre, a circle's
// centre -- and replaces the rest.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchApplyRectangle(JNIEnv*, jclass, jlong entityId,
                                                            jdouble widthMeters,
                                                            jdouble heightMeters) {
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SketchSession& s = forgeshape::sketchSession();
        const forgeshape::SketchEntity* entity = forgeshape::findSketchEntity(
            s.sketch(), static_cast<forgeshape::SketchEntityId>(entityId));
        if (entity == nullptr || entity->rectangle() == nullptr) {
            status = forgeshape::CadStatus::UnknownEntity;
        } else {
            forgeshape::SketchRectangle rectangle = *entity->rectangle();
            rectangle.width = widthMeters;
            rectangle.height = heightMeters;
            status = s.replaceEntity(entity->id(), rectangle);
        }
    }
    FS_LOGI("FORGESHAPE_SKETCH_APPLY rectangle id=%lld w=%.6f h=%.6f %s", (long long)entityId,
            (double)widthMeters, (double)heightMeters, forgeshape::cadStatusName(status));
    return cadCode(status);
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchApplyCircle(JNIEnv*, jclass, jlong entityId,
                                                         jdouble radiusMeters) {
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SketchSession& s = forgeshape::sketchSession();
        const forgeshape::SketchEntity* entity = forgeshape::findSketchEntity(
            s.sketch(), static_cast<forgeshape::SketchEntityId>(entityId));
        if (entity == nullptr || entity->circle() == nullptr) {
            status = forgeshape::CadStatus::UnknownEntity;
        } else {
            forgeshape::SketchCircle circle = *entity->circle();
            circle.radius = radiusMeters;
            status = s.replaceEntity(entity->id(), circle);
        }
    }
    FS_LOGI("FORGESHAPE_SKETCH_APPLY circle id=%lld r=%.6f %s", (long long)entityId,
            (double)radiusMeters, forgeshape::cadStatusName(status));
    return cadCode(status);
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchApplyLine(JNIEnv*, jclass, jlong entityId,
                                                       jdouble x0, jdouble y0, jdouble x1,
                                                       jdouble y1) {
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SketchSession& s = forgeshape::sketchSession();
        const forgeshape::SketchEntity* entity = forgeshape::findSketchEntity(
            s.sketch(), static_cast<forgeshape::SketchEntityId>(entityId));
        if (entity == nullptr || entity->line() == nullptr) {
            status = forgeshape::CadStatus::UnknownEntity;
        } else {
            status = s.replaceEntity(entity->id(),
                                     forgeshape::SketchLine{forgeshape::SketchPoint{x0, y0},
                                                            forgeshape::SketchPoint{x1, y1}});
        }
    }
    FS_LOGI("FORGESHAPE_SKETCH_APPLY line id=%lld %s", (long long)entityId,
            forgeshape::cadStatusName(status));
    return cadCode(status);
}

// The closed profiles' anchor ids, in the deterministic order the domain
// extracted them. Returns how many there are; writes at most the array's
// length.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchProfiles(JNIEnv* env, jclass, jlongArray out) {
    std::vector<jlong> ids;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        for (const forgeshape::ClosedProfile& profile :
             forgeshape::sketchSession().profiles().profiles) {
            ids.push_back(static_cast<jlong>(profile.anchorEntityId));
        }
    }
    if (out != nullptr && !ids.empty()) {
        const jsize capacity = env->GetArrayLength(out);
        const jsize written = std::min(capacity, static_cast<jsize>(ids.size()));
        if (written > 0) {
            env->SetLongArrayRegion(out, 0, written, ids.data());
        }
    }
    return static_cast<jint>(ids.size());
}

// One profile, for the chooser: [0] kind (1 rectangle, 2 circle, 3 polygon),
// [1] vertex count, [2] area in square metres. False for an unknown anchor.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sketchProfileInfo(JNIEnv* env, jclass, jlong anchorId,
                                                         jdoubleArray out) {
    if (out == nullptr || env->GetArrayLength(out) < 3) {
        return JNI_FALSE;
    }
    jdouble values[3] = {0, 0, 0};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SketchSession& s = forgeshape::sketchSession();
        const forgeshape::ClosedProfile* profile = forgeshape::findClosedProfile(
            s.profiles(), static_cast<forgeshape::SketchEntityId>(anchorId));
        if (profile != nullptr) {
            found = true;
            const forgeshape::SketchEntity* anchor =
                forgeshape::findSketchEntity(s.sketch(), profile->anchorEntityId);
            int kind = 3;
            if (anchor != nullptr && anchor->rectangle() != nullptr) kind = 1;
            if (anchor != nullptr && anchor->circle() != nullptr) kind = 2;
            values[0] = kind;
            values[1] = static_cast<double>(profile->polygon.size());
            values[2] = profile->area;
        }
    }
    if (!found) {
        return JNI_FALSE;
    }
    env->SetDoubleArrayRegion(out, 0, 3, values);
    return JNI_TRUE;
}

// Where a sketch point IS on screen, in view-local pixels, through the
// current camera. Verification infrastructure on the gizmo's terms: a case
// that drives a real touch needs some pixel to send it to, and the only
// honest source is the same projection the session unprojects with.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sketchScreenPoint(JNIEnv* env, jclass, jdouble u,
                                                         jdouble v, jfloatArray out) {
    if (out == nullptr || env->GetArrayLength(out) < 2) {
        return JNI_FALSE;
    }
    float point[2] = {0.0f, 0.0f};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        found = forgeshape::sketchSession().sketchToScreen(
            g_camera.snapshot(), forgeshape::SketchPoint{u, v}, g_camera.viewportWidth(),
            g_camera.viewportHeight(), &point[0], &point[1]);
    }
    if (!found) {
        return JNI_FALSE;
    }
    env->SetFloatArrayRegion(out, 0, 2, point);
    return JNI_TRUE;
}

// The current adaptive sketch grid step, in metres (`CAD-A3`): what a grid snap
// rounds to at the zoom the last drag started under. A verification seam so a
// test can assert a snapped value lands on the grid without hardcoding a step.
JNIEXPORT jdouble JNICALL
Java_com_forgeshape_app_NativeViewport_sketchGridStep(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::sketchSession().gridStep();
}

// ---------------------------------------------------------------------------
// The canvas extrude manipulator (`CAD-UX-S1`)
// ---------------------------------------------------------------------------
//
// Three entry points, and none of them carries a semantic the Android layer
// could hold instead: the state read is DERIVED on every call from the session
// own extrusion and the current camera, the flip is a native act, and the
// retained-sketch anchor is a projection of a committed body own sketch. The
// shell learns two pixel coordinates and a multiplier, which is presentation,
// and nothing else.

namespace {

// The WORLD authoring frame of a committed CAD body sketch.
//
// A face-supported body frame is its producer resolved face frame composed
// with the producer world model, which is exactly what `resolveWorldModel`
// already computes; a world-plane body frame is the plane at its own placement
// origin. Factored out of `sketchBeginEdit` so the canvas anchor and the edit
// session cannot disagree about where a sketch lives.
bool resolveCadSketchWorldFrame(forgeshape::ConstructionScene& scene, forgeshape::ObjectId id,
                                const forgeshape::CadBody& body, forgeshape::SketchFrame* out) {
    if (out == nullptr) {
        return false;
    }
    const forgeshape::WorkplaneFrame wf = forgeshape::workplaneFrame(body.sketch().plane);
    forgeshape::Mat4 model;
    if (scene.resolveWorldModel(id, &model)) {
        out->origin = forgeshape::mat4TransformPoint(model, forgeshape::Vec3{0, 0, 0});
        out->u = forgeshape::vec3Normalize(forgeshape::mat4TransformDirection(model, wf.uAxis));
        out->v = forgeshape::vec3Normalize(forgeshape::mat4TransformDirection(model, wf.vAxis));
        out->n = forgeshape::vec3Normalize(forgeshape::mat4TransformDirection(model, wf.normal));
        return true;
    }
    *out = forgeshape::SketchFrame{forgeshape::Vec3{0, 0, 0}, wf.uAxis, wf.vAxis, wf.normal};
    return true;
}

}  // namespace

// The canvas manipulator whole state, in one locked read, into
// NativeViewport CAD_EXTRUDE_* slots. One call because the arrow, the value
// beside it and the badge must describe ONE instant: reading them separately
// would let a drag land between two of them.
//
//   [0]  1 when the manipulator is live (Ready, one profile chosen, anchors
//        resolvable), 0 otherwise -- and 0 is the whole condition for the
//        cluster being absent rather than disabled
//   [1]  the exact depth, in metres
//   [2]  the direction code (0 along the normal, 1 against)
//   [3]  the chosen profile entity id
//   [4]  1 while a drag is captured
//   [5]  1 when the label anchor projects on screen; slots 6..9 are meaningless
//        otherwise, and the shell HIDES rather than guessing a position
//   [6]  label anchor x, in view-local pixels
//   [7]  label anchor y
//   [8]  arrow tip x
//   [9]  arrow tip y
//   [10] the camera-attached visual scale multiplier (see CadExtrudeControlScale)
//   [11] 0 unclamped, 1 clamped at the minimum, 2 clamped at the maximum --
//        diagnostics, so a test can assert the clamp engaged rather than
//        inferring it from a rounded number
//
// `CAD-EXT-R1` added the extent, and with it a SECOND side. Slots 5..9 describe
// the PRIMARY side -- the one the mode's primary distance is on, which for a One
// Side extrusion is the only one there is -- so every reader written before this
// stage still reads what it always read.
//
//   [12] the extent mode (0 One Side, 1 Symmetric, 2 Two Sides)
//   [13] the +N distance, in metres
//   [14] the -N distance, in metres
//   [15] 1 when the SECOND side exists and its label anchor projects on screen;
//        slots 16..19 are meaningless otherwise and the shell HIDES
//   [16] second-side label anchor x   [17] y
//   [18] second-side arrow tip x      [19] y
//   [20] which side a live drag captured: 0 none, 1 the +N side, 2 the -N side
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_cadExtrudeToolState(JNIEnv* env, jclass,
                                                           jdoubleArray out) {
    constexpr jsize kSlots = 21;
    if (out == nullptr || env->GetArrayLength(out) < kSlots) {
        return;
    }
    double values[kSlots] = {0};
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SketchSession& session = forgeshape::sketchSession();
        forgeshape::CadExtrudeAnchors anchors;
        if (session.extrudeAnchors(&anchors)) {
            const forgeshape::ExtrudeFeature& extrude = session.extrude();
            values[0] = 1.0;
            values[1] = anchors.depth;
            values[2] = forgeshape::extrudeDirectionIndex(extrude.direction);
            values[3] = static_cast<double>(extrude.profileEntityId);
            values[4] = session.extrudeManipulator().capturing() ? 1.0 : 0.0;
            values[12] = forgeshape::extrudeExtentModeIndex(extrude.extent);
            values[13] = forgeshape::extrudePositiveDistance(extrude);
            values[14] = forgeshape::extrudeNegativeDistance(extrude);
            values[20] = session.extrudeManipulator().capturing()
                                 ? (session.extrudeManipulator().capturedPositiveSide() ? 1.0 : 2.0)
                                 : 0.0;
            const int w = g_camera.viewportWidth();
            const int h = g_camera.viewportHeight();
            forgeshape::CadExtrudeControlScale scale;
            if (forgeshape::cadExtrudeControlScale(g_camera.snapshot(), anchors.base, h, &scale)) {
                values[10] = scale.scale;
                values[11] = scale.clampedLow ? 1.0 : (scale.clampedHigh ? 2.0 : 0.0);
            }
            // The two sides, projected through the same camera and by the same
            // rule, so neither cluster can be placed by a different arithmetic
            // than the other.
            const forgeshape::CadExtrudeSideAnchor* sides[2] = {
                &anchors.side(anchors.primaryIsPositive),
                &anchors.side(!anchors.primaryIsPositive)};
            const int base[2] = {5, 15};
            for (int s = 0; s < 2; ++s) {
                // Deliberately NOT gated on `present`. A Two Sides side may
                // legitimately be zero, and its value is then still authored
                // truth the user must be able to read and type back up; its
                // label simply coincides with the base. `present` gates the
                // ARROW -- what is drawn and what can be grabbed -- and a
                // number with nowhere to grab is not a number with nowhere to
                // stand.
                float lx = 0.0f;
                float ly = 0.0f;
                float tx = 0.0f;
                float ty = 0.0f;
                if (forgeshape::projectWorldToScreen(g_camera.snapshot(), sides[s]->label, w, h,
                                                     &lx, &ly)
                    && forgeshape::projectWorldToScreen(g_camera.snapshot(), sides[s]->tip, w, h,
                                                        &tx, &ty)) {
                    values[base[s] + 0] = 1.0;
                    values[base[s] + 1] = lx;
                    values[base[s] + 2] = ly;
                    values[base[s] + 3] = tx;
                    values[base[s] + 4] = ty;
                }
            }
        }
    }
    env->SetDoubleArrayRegion(out, 0, kSlots, values);
}

// The extent mode: One Side, Symmetric or Two Sides. One door, taking the
// deterministic transition policy with it, so a mode change can never lose or
// invent a distance.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchSetExtrudeExtent(JNIEnv*, jclass, jint modeIndex) {
    forgeshape::ExtrudeExtentMode mode;
    if (!forgeshape::extrudeExtentModeFromIndex(static_cast<int>(modeIndex), &mode)) {
        return cadCode(forgeshape::CadStatus::InvalidExtrudeExtent);
    }
    forgeshape::CadStatus status;
    double positive = 0.0;
    double negative = 0.0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SketchSession& session = forgeshape::sketchSession();
        status = session.setExtrudeExtent(mode);
        positive = forgeshape::extrudePositiveDistance(session.extrude());
        negative = forgeshape::extrudeNegativeDistance(session.extrude());
    }
    FS_LOGI("FORGESHAPE_EXTRUDE_EXTENT mode=%s positive=%.6f negative=%.6f %s",
            forgeshape::extrudeExtentModeName(mode), positive, negative,
            forgeshape::cadStatusName(status));
    return cadCode(status);
}

// ONE side's distance, in metres. What a typed A or B lands through, and what
// the arrow drag already lands through below JNI, so the two pass exactly the
// same validation. `side` is 1 for the `+N` side and 2 for `-N`.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchSetExtrudeSide(JNIEnv*, jclass, jint side,
                                                            jdouble meters) {
    if (side != 1 && side != 2) {
        return cadCode(forgeshape::CadStatus::InvalidExtrudeExtent);
    }
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        status = forgeshape::sketchSession().setExtrudeSide(side == 1, meters);
    }
    FS_LOGI("FORGESHAPE_EXTRUDE_SIDE side=%s meters=%.6f %s", side == 1 ? "positive" : "negative",
            (double)meters, forgeshape::cadStatusName(status));
    return cadCode(status);
}

// Reverses which side of the sketch plane the solid grows on, keeping the exact
// depth and the same profile. A DIRECTION change and never a negative depth.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchFlipExtrudeDirection(JNIEnv*, jclass) {
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        status = forgeshape::sketchSession().flipExtrudeDirection();
    }
    if (status != forgeshape::CadStatus::Ok) {
        FS_LOGI("FORGESHAPE_EXTRUDE_FLIP_REFUSED:%s", forgeshape::cadStatusName(status));
    } else {
        FS_LOGI("FORGESHAPE_EXTRUDE_FLIP_OK");
    }
    return cadCode(status);
}

// Where a COMMITTED CAD body retained sketch is, on screen (`CAD-UX-S1` 4.7).
//
// The whole point of the retained-sketch access: a body made from a sketch
// still HAS that sketch, and this is the anchor the canvas chip that reopens it
// stands on. It is a read -- no session is begun, nothing is regenerated, no
// revision is minted -- and it refuses for every body that is not a CAD body,
// so the control cannot be drawn where it could not succeed.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_cadBodySketchAnchor(JNIEnv* env, jclass, jlong bodyId,
                                                           jfloatArray out) {
    if (out == nullptr || env->GetArrayLength(out) < 3) {
        return JNI_FALSE;
    }
    // x, y and the same camera-attached multiplier the manipulator cluster is
    // drawn at, so the chip that reopens a sketch belongs to the work in
    // exactly the way the arrow that made it did.
    float point[3] = {0.0f, 0.0f, 1.0f};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
        // Exactly the conditions `sketchBeginEdit` refuses, asked HERE so the
        // chip is absent where it could not succeed rather than shown and then
        // refused. The guard below JNI stays regardless: removing a control is
        // not removing a guard.
        const bool available = scene.hasProject()
                               && !forgeshape::sketchSession().active()
                               && !forgeshape::sculptSession().inSculptMode()
                               && !forgeshape::constructionHistory().editInProgress();
        if (available) {
            const forgeshape::ObjectId id = static_cast<forgeshape::ObjectId>(bodyId);
            forgeshape::SceneObject* object = scene.findBody(id);
            const forgeshape::CadBody* body =
                object != nullptr ? object->cadOrNull() : nullptr;
            if (body != nullptr && object->visible()) {
                forgeshape::SketchFrame frame;
                forgeshape::ProfileExtraction profiles =
                    forgeshape::extractClosedProfiles(body->sketch());
                const forgeshape::ClosedProfile* chosen =
                    forgeshape::findClosedProfile(profiles, body->extrude().profileEntityId);
                forgeshape::CadExtrudeAnchors anchors;
                if (chosen != nullptr
                    && resolveCadSketchWorldFrame(scene, id, *body, &frame)
                    && forgeshape::cadExtrudeAnchors(frame, *chosen, body->extrude(), &anchors)) {
                    // The BASE, not the label: the chip belongs on the sketch
                    // the body was made from, which is the cap lying on the
                    // support plane, rather than halfway up the solid.
                    found = forgeshape::projectWorldToScreen(g_camera.snapshot(), anchors.base,
                                                             g_camera.viewportWidth(),
                                                             g_camera.viewportHeight(),
                                                             &point[0], &point[1]);
                    forgeshape::CadExtrudeControlScale scale;
                    if (found
                        && forgeshape::cadExtrudeControlScale(g_camera.snapshot(), anchors.base,
                                                              g_camera.viewportHeight(), &scale)) {
                        point[2] = scale.scale;
                    }
                }
            }
        }
    }
    if (!found) {
        return JNI_FALSE;
    }
    env->SetFloatArrayRegion(out, 0, 3, point);
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// The orientation navigator (`SKETCH-UX-R1` C)
// ---------------------------------------------------------------------------
//
// Three acts, and each of them re-frames the camera on the session's own VIEW
// frame. The authoring frame is untouched by all three: nothing here can move
// an authored coordinate, and the shell holds no orientation of its own.

// The navigator's current state, in NativeViewport's SKETCH_VIEW_* slots:
//   [0] 1 when a sketch is active
//   [1] the support plane index (a face sketch reports its canonical XY)
//   [2] 1 when the view is flipped to the plane's negative normal
//   [3] the quarter turns, 0..3
//   [4] 1 when the support is a FACE and so cannot be switched to a world plane
//   [5] 1 when the support plane may be switched right now (the sketch is empty)
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_sketchViewState(JNIEnv* env, jclass, jdoubleArray out) {
    constexpr jsize kSize = 6;
    if (out == nullptr || env->GetArrayLength(out) < kSize) {
        return;
    }
    jdouble values[kSize] = {0, 0, 0, 0, 0, 0};
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SketchSession& s = forgeshape::sketchSession();
        values[0] = s.active() ? 1.0 : 0.0;
        values[1] = static_cast<double>(forgeshape::workplaneIndex(s.plane()));
        values[2] = s.viewFlipped() ? 1.0 : 0.0;
        values[3] = static_cast<double>(s.viewQuarterTurns());
        values[4] = s.sketch().hasFaceSupport ? 1.0 : 0.0;
        values[5] = (s.active() && !s.sketch().hasFaceSupport && s.sketch().entities.empty())
                            ? 1.0
                            : 0.0;
    }
    env->SetDoubleArrayRegion(out, 0, kSize, values);
}

// Chooses the world support plane. Refused by name -- SketchNotEmpty once the
// sketch carries geometry, InvalidWorkplane for a face-supported sketch -- and
// a refusal changes nothing, including the camera.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchSetSupportPlane(JNIEnv*, jclass, jint planeIndex) {
    forgeshape::Workplane plane;
    if (!forgeshape::workplaneFromIndex(static_cast<int>(planeIndex), &plane)) {
        return cadCode(forgeshape::CadStatus::InvalidWorkplane);
    }
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        status = forgeshape::sketchSession().setSupportPlane(plane);
        if (status == forgeshape::CadStatus::Ok) {
            beginSketchView();
        }
    }
    FS_LOGI("FORGESHAPE_SKETCH_SUPPORT_PLANE plane=%s %s", forgeshape::workplaneName(plane),
            forgeshape::cadStatusName(status));
    return cadCode(status);
}

// Looks at the plane's positive or negative normal. Presentation only.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchSetViewFlipped(JNIEnv*, jclass, jboolean flipped) {
    forgeshape::CadStatus status = forgeshape::CadStatus::Ok;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SketchSession& s = forgeshape::sketchSession();
        if (!s.active()) {
            status = forgeshape::CadStatus::NotSketching;
        } else {
            s.setViewFlipped(flipped == JNI_TRUE);
            beginSketchView();
        }
    }
    return cadCode(status);
}

// Rolls the view a quarter turn about the sketch normal. Presentation only:
// no authored coordinate moves, and nothing is mirrored.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchRotateView(JNIEnv*, jclass, jint quarterTurns) {
    forgeshape::CadStatus status = forgeshape::CadStatus::Ok;
    int turns = 0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SketchSession& s = forgeshape::sketchSession();
        if (!s.active()) {
            status = forgeshape::CadStatus::NotSketching;
        } else {
            s.rotateView(static_cast<int>(quarterTurns));
            turns = s.viewQuarterTurns();
            beginSketchView();
        }
    }
    if (status == forgeshape::CadStatus::Ok) {
        FS_LOGI("FORGESHAPE_SKETCH_VIEW_ROTATED quarters=%d", turns);
    }
    return cadCode(status);
}

// ---------------------------------------------------------------------------
// The selected line's dimension (`SKETCH-UX-R1` E)
// ---------------------------------------------------------------------------

// The dimension of the selected straight Line, in NativeViewport's
// SKETCH_DIMENSION_* slots:
//   [0] the entity id  [1] the length in metres
//   [2] the label anchor's u  [3] its v
// Returns false, writing nothing, when the selection is not a straight Line --
// which is exactly the condition for the annotation not being drawn.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sketchLineDimension(JNIEnv* env, jclass, jdoubleArray out) {
    constexpr jsize kSize = 4;
    if (out == nullptr || env->GetArrayLength(out) < kSize) {
        return JNI_FALSE;
    }
    jdouble values[kSize] = {0, 0, 0, 0};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SketchSession& s = forgeshape::sketchSession();
        forgeshape::Meters length = 0.0;
        forgeshape::SketchPoint anchor;
        // The SAME camera-derived scale the overlay is built with, so the label
        // sits exactly on the annotation the renderer drew.
        float perPixel = 0.0f;
        double worldPerUnit = 0.0;
        if (forgeshape::worldMetersPerPixel(g_camera.snapshot(), s.frame().origin,
                                            g_camera.viewportHeight(), &perPixel)) {
            worldPerUnit = static_cast<double>(perPixel) * forgeshape::gizmoPixelsPerReferenceUnit();
        }
        if (s.selectedLineLength(&length)
            && s.selectedLineDimensionAnchor(worldPerUnit, &anchor)) {
            found = true;
            values[0] = static_cast<double>(s.selectedEntityId());
            values[1] = length;
            values[2] = anchor.u;
            values[3] = anchor.v;
        }
    }
    if (!found) {
        return JNI_FALSE;
    }
    env->SetDoubleArrayRegion(out, 0, kSize, values);
    return JNI_TRUE;
}

// Sets a straight Line's length EXACTLY: P0 fixed, direction preserved. No
// solver, no neighbour moved, nothing re-snapped.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchApplyLineLength(JNIEnv*, jclass, jlong entityId,
                                                             jdouble lengthMeters) {
    forgeshape::CadStatus status;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        status = forgeshape::sketchSession().applyLineLength(
            static_cast<forgeshape::SketchEntityId>(entityId), lengthMeters);
    }
    FS_LOGI("FORGESHAPE_SKETCH_LINE_LENGTH id=%lld length=%.6f %s", (long long)entityId,
            (double)lengthMeters, forgeshape::cadStatusName(status));
    return cadCode(status);
}

// ---------------------------------------------------------------------------
// Edit Sketch (`SKETCH-UX-R1` F)
// ---------------------------------------------------------------------------

// Opens a staged edit of a committed CAD body's sketch. Refused while
// sculpting, while a Construction edit is open, while a session is already
// open, and for a body that is not a CAD Body. On success the camera frames the
// body's own sketch support exactly as a new sketch's does.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchBeginEdit(JNIEnv*, jclass, jlong bodyId) {
    forgeshape::CadStatus status = forgeshape::CadStatus::Ok;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
        if (!scene.hasProject()) {
            status = forgeshape::CadStatus::NotSketching;
        } else if (forgeshape::sculptSession().inSculptMode()) {
            status = forgeshape::CadStatus::NotSketching;
        } else if (forgeshape::constructionHistory().editInProgress()) {
            status = forgeshape::CadStatus::RefusedEditInProgress;
        } else {
            const forgeshape::ObjectId id = static_cast<forgeshape::ObjectId>(bodyId);
            forgeshape::SceneObject* object = scene.findBody(id);
            const forgeshape::CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
            if (body == nullptr) {
                status = forgeshape::CadStatus::NotCadBody;
            } else {
                // Where the body's sketch is authored in WORLD space. A
                // face-supported body's frame is its producer's resolved face
                // frame composed with the producer's world model, which is
                // exactly what `resolveWorldModel` already computes; a
                // world-plane body's is the plane at its own placement origin.
                forgeshape::SketchFrame frame;
                forgeshape::Mat4 model;
                const forgeshape::WorkplaneFrame wf =
                    forgeshape::workplaneFrame(body->sketch().plane);
                if (scene.resolveWorldModel(id, &model)) {
                    frame.origin = forgeshape::mat4TransformPoint(model,
                                                                  forgeshape::Vec3{0, 0, 0});
                    frame.u = forgeshape::vec3Normalize(
                        forgeshape::mat4TransformDirection(model, wf.uAxis));
                    frame.v = forgeshape::vec3Normalize(
                        forgeshape::mat4TransformDirection(model, wf.vAxis));
                    frame.n = forgeshape::vec3Normalize(
                        forgeshape::mat4TransformDirection(model, wf.normal));
                } else {
                    frame = forgeshape::SketchFrame{forgeshape::Vec3{0, 0, 0}, wf.uAxis, wf.vAxis,
                                                    wf.normal};
                }
                status = forgeshape::sketchSession().beginEdit(id, body->state(), frame);
            }
        }
        if (status == forgeshape::CadStatus::Ok) {
            forgeshape::supportChooser().cancel();
            forgeshape::gizmoSession().setActive(false);
            g_selection.resetGesture();
            g_camera.resetGesture();
            beginSketchView();
        }
    }
    if (status != forgeshape::CadStatus::Ok) {
        FS_LOGE("FORGESHAPE_SKETCH_EDIT_REFUSED:%s", forgeshape::cadStatusName(status));
        return cadCode(status);
    }
    FS_LOGI("FORGESHAPE_SKETCH_EDIT_BEGIN objectId=%lld", (long long)bodyId);
    return cadCode(status);
}

// Which body the open session is editing, or 0 when it is authoring a new one.
JNIEXPORT jlong JNICALL
Java_com_forgeshape_app_NativeViewport_sketchEditingBodyId(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jlong>(forgeshape::sketchSession().editingBodyId());
}

// Finishes a staged sketch edit: one transaction, one Undo. A refusal changes
// nothing and leaves the session in Ready with the reason named.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sketchCommitEdit(JNIEnv*, jclass) {
    forgeshape::CadStatus status;
    forgeshape::ObjectId edited = forgeshape::kNoObject;
    size_t undoDepth = 0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        edited = forgeshape::sketchSession().editingBodyId();
        status = forgeshape::sketchSession().commitEdit(forgeshape::constructionScene(),
                                                        forgeshape::constructionHistory());
        if (status == forgeshape::CadStatus::Ok) {
            endSketchView();
        }
        undoDepth = forgeshape::constructionHistory().undoDepth();
    }
    if (status != forgeshape::CadStatus::Ok) {
        FS_LOGI("FORGESHAPE_SKETCH_EDIT_REFUSED:%s", forgeshape::cadStatusName(status));
        return cadCode(status);
    }
    FS_LOGI("FORGESHAPE_SKETCH_EDIT_COMMIT objectId=%lld undo=%d", (long long)edited,
            (int)undoDepth);
    return cadCode(status);
}

JNIEXPORT jstring JNICALL
Java_com_forgeshape_app_NativeViewport_cadStatusToken(JNIEnv* env, jclass, jint code) {
    forgeshape::CadStatus status;
    if (!forgeshape::cadStatusFromCode(static_cast<int>(code), &status)) {
        return env->NewStringUTF("unknown");
    }
    return env->NewStringUTF(forgeshape::cadStatusName(status));
}

// --- the CAD Body ------------------------------------------------------------

// Which representation a body has: 1 Construction, 2 Imported, 3 CAD. Zero for
// an unknown id.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sceneBodyRepresentation(JNIEnv*, jclass, jlong objectId) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const forgeshape::SceneObject* body =
        forgeshape::constructionScene().findBody(static_cast<forgeshape::ObjectId>(objectId));
    return body != nullptr ? static_cast<jint>(body->representation()) : 0;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sceneActiveBodyIsCad(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
    return scene.hasProject() && scene.activeBody().isCad() ? JNI_TRUE : JNI_FALSE;
}

// Whether the active body is a FACE-SUPPORTED CAD body (`CAD-A3`): its placement
// is derived from a producer and it cannot be moved independently. A
// verification seam.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sceneActiveBodyIsFaceSupportedCad(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
    return scene.hasProject() && scene.activeBody().isFaceSupportedCad() ? JNI_TRUE : JNI_FALSE;
}

// The active CAD Body's authored values, in NativeViewport's CAD_* slots:
//   [0] plane index      [1] extrude depth, metres   [2] direction index
//   [3] profile kind (0 none, 1 rectangle, 2 circle, 3 polygon)
//   [4] rectangle width or circle radius   [5] rectangle height
//   [6] sketch entity count                [7] profile vertex count
//   [8] extent mode (0 One Side, 1 Symmetric, 2 Two Sides) -- `CAD-EXT-R1`, so
//       the panel can withdraw Flip where there is no side left to choose
// Returns false, writing nothing, when the active body is not a CAD Body.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_cadState(JNIEnv* env, jclass, jdoubleArray out) {
    constexpr jsize kSize = 9;
    if (out == nullptr || env->GetArrayLength(out) < kSize) {
        return JNI_FALSE;
    }
    jdouble values[kSize] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
        const forgeshape::CadBody* cad =
            scene.hasProject() ? scene.activeBody().cadOrNull() : nullptr;
        if (cad != nullptr) {
            found = true;
            const forgeshape::CadBodyState& state = cad->state();
            values[0] = static_cast<double>(forgeshape::workplaneIndex(state.sketch.plane));
            values[1] = state.extrude.depth;
            values[2] = static_cast<double>(
                forgeshape::extrudeDirectionIndex(state.extrude.direction));
            values[3] = static_cast<double>(static_cast<int>(forgeshape::cadProfileKind(state)));
            const forgeshape::SketchEntity* anchor =
                forgeshape::findSketchEntity(state.sketch, state.extrude.profileEntityId);
            if (anchor != nullptr && anchor->rectangle() != nullptr) {
                values[4] = anchor->rectangle()->width;
                values[5] = anchor->rectangle()->height;
            } else if (anchor != nullptr && anchor->circle() != nullptr) {
                values[4] = anchor->circle()->radius;
            }
            values[6] = static_cast<double>(state.sketch.entities.size());
            values[8] = static_cast<double>(
                forgeshape::extrudeExtentModeIndex(state.extrude.extent));
            forgeshape::ProfileExtraction extraction;
            if (forgeshape::validateCadBodyState(state, &extraction) == forgeshape::CadStatus::Ok) {
                const forgeshape::ClosedProfile* profile =
                    forgeshape::findClosedProfile(extraction, state.extrude.profileEntityId);
                values[7] = profile ? static_cast<double>(profile->polygon.size()) : 0.0;
            }
        }
    }
    if (!found) {
        return JNI_FALSE;
    }
    env->SetDoubleArrayRegion(out, 0, kSize, values);
    return JNI_TRUE;
}

// A CAD Apply status, in the APPLY_* vocabulary the shape editor already
// speaks, so one Apply footer can report both. The exact CadStatus is in
// cadLastStatus() and in the log.
constexpr jint kApplyRejectedCad = 7;
static forgeshape::CadStatus g_lastCadApplyStatus = forgeshape::CadStatus::Ok;

static jint cadApplyCode(forgeshape::CadStatus status, bool changed) {
    switch (status) {
        case forgeshape::CadStatus::Ok:
            return changed ? kApplyApplied : kApplyUnchanged;
        case forgeshape::CadStatus::NonFinite:
            return kApplyRejectedNotFinite;
        case forgeshape::CadStatus::InvalidExtrudeDepth:
        case forgeshape::CadStatus::ZeroSizeRectangle:
        case forgeshape::CadStatus::InvalidCircleRadius:
            return kApplyRejectedNotPositive;
        case forgeshape::CadStatus::RegenerationFailed:
            return kApplyPublishFailed;
        default:
            return kApplyRejectedCad;
    }
}

// ONE Apply of a complete candidate state to the active CAD Body: one history
// step, one regeneration, one publication -- and none of them when the state
// is refused or identical. The candidate is built by the caller from the
// current state and the typed values, so a rectangle's centre and every
// entity the user did not type survive untouched.
static jint applyCadCandidate(const char* label,
                              forgeshape::CadBodyState (*build)(const forgeshape::CadBodyState&,
                                                                const double*, int),
                              const double* values, int direction) {
    forgeshape::CadStatus status = forgeshape::CadStatus::Ok;
    bool changed = false;
    forgeshape::MeshRevision revision = forgeshape::kNoMeshRevision;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
        forgeshape::CadBody* cad = nullptr;
        if (scene.hasProject()) {
            cad = scene.activeBody().cadOrNull();
        }
        if (cad == nullptr) {
            status = forgeshape::CadStatus::NotCadBody;
        } else if (forgeshape::sketchSession().active()) {
            status = forgeshape::CadStatus::NotSketching;
        } else {
            // One user Apply is ONE history step, and NO step when refused or
            // identical: the commit compares state rather than trusting the call.
            forgeshape::ScopedConstructionEdit edit(forgeshape::constructionHistory());
            status = cad->applyState(build(cad->state(), values, direction), &changed);
            if (status == forgeshape::CadStatus::Ok && changed) {
                revision = forgeshape::publishSceneObject(scene.activeBody());
            }
        }
        g_lastCadApplyStatus = status;
    }
    if (status != forgeshape::CadStatus::Ok) {
        FS_LOGE("FORGESHAPE_CAD_APPLY_REJECTED:%s:%s", label, forgeshape::cadStatusName(status));
    } else if (changed) {
        FS_LOGI("FORGESHAPE_CAD_APPLY:%s meshRev=%llu undo=%d", label,
                (unsigned long long)revision, (int)forgeshape::constructionHistory().undoDepth());
    } else {
        FS_LOGI("FORGESHAPE_CAD_APPLY_UNCHANGED:%s", label);
    }
    return cadApplyCode(status, changed);
}

static forgeshape::CadBodyState buildExtrudeCandidate(const forgeshape::CadBodyState& current,
                                                      const double* values, int direction) {
    forgeshape::CadBodyState candidate = current;
    forgeshape::ExtrudeDirection dir = current.extrude.direction;
    forgeshape::extrudeDirectionFromIndex(direction, &dir);
    // The PRIMARY distance, and the side only where a side is a choice: the
    // panel edits a Symmetric body's per-side length without turning it into a
    // One Side body, and its Flip is withdrawn there rather than ignored.
    candidate.extrude =
        forgeshape::extrudeFeatureWithPrimary(current.extrude, values[0], dir);
    return candidate;
}

static forgeshape::CadBodyState buildRectangleCandidate(const forgeshape::CadBodyState& current,
                                                        const double* values, int direction) {
    forgeshape::CadBodyState candidate = buildExtrudeCandidate(current, values + 2, direction);
    for (forgeshape::SketchEntity& entity : candidate.sketch.entities) {
        if (entity.id() != candidate.extrude.profileEntityId) continue;
        if (const forgeshape::SketchRectangle* rectangle = entity.rectangle()) {
            forgeshape::SketchRectangle edited = *rectangle;
            edited.width = values[0];
            edited.height = values[1];
            entity = forgeshape::SketchEntity(entity.id(), edited);
        }
    }
    return candidate;
}

static forgeshape::CadBodyState buildCircleCandidate(const forgeshape::CadBodyState& current,
                                                     const double* values, int direction) {
    forgeshape::CadBodyState candidate = buildExtrudeCandidate(current, values + 1, direction);
    for (forgeshape::SketchEntity& entity : candidate.sketch.entities) {
        if (entity.id() != candidate.extrude.profileEntityId) continue;
        if (const forgeshape::SketchCircle* circle = entity.circle()) {
            forgeshape::SketchCircle edited = *circle;
            edited.radius = values[0];
            entity = forgeshape::SketchEntity(entity.id(), edited);
        }
    }
    return candidate;
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_cadApplyExtrude(JNIEnv*, jclass, jdouble depthMeters,
                                                       jint directionIndex) {
    const double values[1] = {depthMeters};
    return applyCadCandidate("extrude", buildExtrudeCandidate, values, directionIndex);
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_cadApplyRectangle(JNIEnv*, jclass, jdouble widthMeters,
                                                         jdouble heightMeters, jdouble depthMeters,
                                                         jint directionIndex) {
    const double values[3] = {widthMeters, heightMeters, depthMeters};
    return applyCadCandidate("rectangle", buildRectangleCandidate, values, directionIndex);
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_cadApplyCircle(JNIEnv*, jclass, jdouble radiusMeters,
                                                      jdouble depthMeters, jint directionIndex) {
    const double values[2] = {radiusMeters, depthMeters};
    return applyCadCandidate("circle", buildCircleCandidate, values, directionIndex);
}

JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_cadLastStatus(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return cadCode(g_lastCadApplyStatus);
}

// DEBUG-ONLY: reads the orbit pose, and places it.
//
// Verification infrastructure, not product functionality: no UI reaches either,
// and both compile to nothing in a release build. The read exists so a case can
// assert that a captured handle did NOT orbit the camera; the write exists so a
// case can say "from a viewpoint where this axis is nearly edge-on" without
// synthesising an orbit gesture of exactly the right pixel length first.
//
//   [0] yaw, radians   [1] pitch, radians   [2] orbit distance, meters
JNIEXPORT void JNICALL Java_com_forgeshape_app_NativeViewport_debugCameraPose(JNIEnv* env, jclass,
                                                                             jfloatArray out) {
#ifndef NDEBUG
    if (out == nullptr || env->GetArrayLength(out) < 3) {
        return;
    }
    jfloat pose[3];
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        pose[0] = g_camera.yaw();
        pose[1] = g_camera.pitch();
        pose[2] = g_camera.distance();
    }
    env->SetFloatArrayRegion(out, 0, 3, pose);
#else
    (void)env;
    (void)out;
#endif
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_debugSetCameraPose(JNIEnv*, jclass, jfloat yaw,
                                                          jfloat pitch, jfloat distance) {
#ifndef NDEBUG
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_camera.setPose(yaw, pitch, distance) ? JNI_TRUE : JNI_FALSE;
#else
    (void)yaw;
    (void)pitch;
    (void)distance;
    return JNI_FALSE;
#endif
}

// The production session-initialization boundary.
//
// The Android shell answers the start question by driving the SAME entry points
// a user would, which is what keeps Freeze from being re-implemented — and which
// means seeding a session in Sculpt performs a real Construction shape change.
// That is a change the user did not make, so it is bracketed rather than
// recorded: see ConstructionHistory's boundary comment. Nothing else in the
// product may call these, and nothing else does — Back to Construction, a
// rotation, a resume and every ordinary edit are outside the bracket.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_beginSessionInitialization(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    forgeshape::constructionHistory().beginSessionInitialization();
    FS_LOGI("FORGESHAPE_SESSION_INIT_BEGIN");
}

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_endSessionInitialization(JNIEnv*, jclass) {
    size_t undoDepth = 0;
    size_t redoDepth = 0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::ConstructionHistory& history = forgeshape::constructionHistory();
        history.endSessionInitialization();
        undoDepth = history.undoDepth();
        redoDepth = history.redoDepth();
    }
    // The postcondition, in the log, so "a new session starts with an empty
    // history" is a thing a captured run states rather than a claim.
    FS_LOGI("FORGESHAPE_SESSION_INIT_END undo=%d redo=%d", (int)undoDepth, (int)redoDepth);
}

// DEBUG-ONLY: forgets the Construction history without moving the scene.
//
// Not a product act and not reachable from any UI. It is the observation seam
// the instrumented suite needs to establish "a session that has done nothing",
// which is otherwise impossible to reach in a process that has already run
// other cases — the history is process-scoped exactly like the scene, and there
// is no New Project act to clear it. Compiled out of a release build.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_debugResetConstructionHistory(JNIEnv*, jclass) {
#ifndef NDEBUG
    std::lock_guard<std::mutex> lock(g_stateMutex);
    forgeshape::constructionHistory().clear();
#endif
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_constructionUndoDepth(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jint>(forgeshape::constructionHistory().undoDepth());
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_constructionRedoDepth(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jint>(forgeshape::constructionHistory().redoDepth());
}

// Runs one history step and reports what it cost.
//
// The state mutex is held across the restore, which is the ONE place in the
// product a lock is held across mesh generation. It has to be: a restore
// rebuilds the scene's body list, and the render thread takes its whole-scene
// snapshot under this same mutex — a frame that observed the list mid-rebuild
// would draw a scene that never existed. An undo is one discrete user act, not
// a per-frame path, so the cost is one publication of the bodies that actually
// changed.
static jint runHistoryStep(const char* label, bool forward) {
    forgeshape::ConstructionRestoreReport report;
    bool moved = false;
    size_t undoDepth = 0;
    size_t redoDepth = 0;
    size_t bodyCount = 0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        // The mode and the sketch guards are read under the same lock as the
        // step they guard, so neither can change between the check and the act.
        if (forgeshape::sculptSession().inSculptMode()) {
            FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY_REFUSED:%s:in_sculpt_mode", label);
            return kHistoryRefusedInSculpt;
        }
        if (forgeshape::sketchSession().active()) {
            // A sketch in progress is not in the history yet, and a Construction
            // step under it would move the scene it is about to land in. The
            // controls are withdrawn while sketching; this is the guard behind
            // them.
            FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY_REFUSED:%s:in_sketch", label);
            return kHistoryRefusedInSculpt;
        }
        forgeshape::ConstructionHistory& history = forgeshape::constructionHistory();
        moved = forward ? history.redo(&report) : history.undo(&report);
        undoDepth = history.undoDepth();
        redoDepth = history.redoDepth();
        bodyCount = forgeshape::constructionScene().bodyCount();
    }
    if (!moved) {
        FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY_EMPTY:%s", label);
        return kHistoryNothingToDo;
    }
    FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY:%s republished=%d placements=%d restored=%d "
            "removed=%d activeChanged=%d undo=%d redo=%d bodies=%d",
            label, report.republishedBodies, report.replacedPlacements, report.restoredBodies,
            report.removedBodies, report.activeBodyChanged ? 1 : 0, (int)undoDepth,
            (int)redoDepth, (int)bodyCount);
    return kHistoryOk;
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_constructionUndo(JNIEnv*, jclass) {
    return runHistoryStep("undo", false);
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_constructionRedo(JNIEnv*, jclass) {
    return runHistoryStep("redo", true);
}

// --- the active body's Sculpt history ---------------------------------------

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sculptUndoAvailable(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::sculptSession().canUndoSculpt() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sculptRedoAvailable(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::sculptSession().canRedoSculpt() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sculptUndoDepth(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jint>(forgeshape::sculptSession().history().undoDepth());
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sculptRedoDepth(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jint>(forgeshape::sculptSession().history().redoDepth());
}

// What the active body's retained Sculpt history costs right now, by the same
// conservative measure the cap is enforced with. A diagnostic: the Java layer
// derives nothing from it, and it exists so "bounded" is a value a test can read
// rather than only an assertion that failed.
JNIEXPORT jlong JNICALL
Java_com_forgeshape_app_NativeViewport_sculptHistoryBytes(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jlong>(forgeshape::sculptSession().history().payloadBytes());
}

// How many strokes on this body were too large to retain, and how many entries
// have been evicted to stay inside the caps. Session-lifetime counters, never
// project truth. The first is what makes `kHistoryEntryNotRetained` visible: a
// stroke that cannot be taken back must say so rather than leave a user tapping
// Undo at geometry that will not move.
JNIEXPORT jlong JNICALL
Java_com_forgeshape_app_NativeViewport_sculptHistoryNotRetainedCount(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jlong>(forgeshape::sculptSession().history().notRetainedStrokes());
}

JNIEXPORT jlong JNICALL
Java_com_forgeshape_app_NativeViewport_sculptHistoryEvictedCount(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jlong>(forgeshape::sculptSession().history().evictedEntries());
}

// --- the History navigator (`SCULPT-H1`) ------------------------------------

// The navigator's whole read-only model, in one locked read.
//
// One call rather than five, for the reason `sculptState` is one call: the five
// values describe ONE body's branch at ONE instant, and reading them separately
// would let a body switch or a stroke land between two of them and hand the
// navigator a row count that does not go with its cursor.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_sculptHistoryState(JNIEnv* env, jclass,
                                                          jdoubleArray out) {
    constexpr jsize kSculptHistoryStateSize = 5;
    if (out == nullptr || env->GetArrayLength(out) < kSculptHistoryStateSize) {
        return;
    }
    jdouble values[kSculptHistoryStateSize];
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SculptSession& session = forgeshape::sculptSession();
        const forgeshape::SculptHistoryCursor at = session.history().cursor();
        const jdouble read[kSculptHistoryStateSize] = {
            session.canNavigateSculptHistory() ? 1.0 : 0.0,
            static_cast<jdouble>(at.cursor),
            static_cast<jdouble>(at.undoCount),
            static_cast<jdouble>(at.redoCount),
            static_cast<jdouble>(at.stateCount),
        };
        std::copy(read, read + kSculptHistoryStateSize, values);
    }
    env->SetDoubleArrayRegion(out, 0, kSculptHistoryStateSize, values);
}

// Moves the active body's sculpt mesh to one state on its retained branch.
//
// Publishes through `publishSculptRepresentation` exactly as a single Undo
// does, and for the same reason: a jump reaches the renderer the way every
// other sculpt geometry change does, so there is no second way for sculpt
// geometry to become a frame. ONE publication for the whole jump, not one per
// step — the intermediate states are arithmetic on the way to the state the
// user tapped, and drawing them would be showing frames nobody asked for.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sculptJumpToHistoryCursor(JNIEnv*, jclass,
                                                                 jint targetCursor) {
    if (targetCursor < 0) {
        // A negative ordinal cannot address a state, and it must not be widened
        // into a huge `size_t` on the way to a range check that would then be
        // asking about the wrong number.
        return kHistoryOutOfRange;
    }
    forgeshape::SculptSession::SculptHistoryStatus status;
    forgeshape::SculptRevision revision = 0;
    forgeshape::MeshRevision meshRevision = forgeshape::kNoMeshRevision;
    size_t undoDepth = 0;
    size_t redoDepth = 0;
    bool edits = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SculptSession& session = forgeshape::sculptSession();
        status = session.jumpToHistoryCursor(static_cast<size_t>(targetCursor));
        if (status == forgeshape::SculptSession::SculptHistoryStatus::Ok) {
            meshRevision = publishSculptRepresentation(session, "history_jump");
            revision = session.mesh().revision();
            edits = session.mesh().hasEdits();
        }
        undoDepth = session.history().undoDepth();
        redoDepth = session.history().redoDepth();
    }
    if (status != forgeshape::SculptSession::SculptHistoryStatus::Ok) {
        FS_LOGI("FORGESHAPE_SCULPT_HISTORY_REFUSED:jump:%s target=%d",
                forgeshape::SculptSession::sculptHistoryStatusName(status), (int)targetCursor);
        return sculptHistoryStatusCode(status);
    }
    FS_LOGI("FORGESHAPE_SCULPT_HISTORY:jump target=%d sculptRevision=%llu meshRevision=%llu "
            "edits=%d undo=%d redo=%d",
            (int)targetCursor, (unsigned long long)revision, (unsigned long long)meshRevision,
            edits ? 1 : 0, (int)undoDepth, (int)redoDepth);
    return kHistoryOk;
}

// Clears the active body's Sculpt Mask, as ONE history entry (`SCULPT-FCM-R1`).
//
// Publishes through `publishSculptRepresentation` exactly as a stroke does,
// because the mask is a per-vertex channel of the same published mesh and this
// is how it reaches a frame. It mints no SculptRevision and sets no edited
// flag, so the project fingerprint does not move and no checkpoint is earned
// for it — a mask is runtime state, not project truth.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sculptClearMask(JNIEnv*, jclass) {
    forgeshape::SculptSession::SculptHistoryStatus status;
    forgeshape::MeshRevision meshRevision = forgeshape::kNoMeshRevision;
    uint32_t maskedAfter = 0;
    size_t undoDepth = 0;
    size_t redoDepth = 0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SculptSession& session = forgeshape::sculptSession();
        status = session.clearMask();
        if (status == forgeshape::SculptSession::SculptHistoryStatus::Ok) {
            meshRevision = publishSculptRepresentation(session, "clear_mask");
        }
        maskedAfter = session.mesh().maskedVertexCount();
        undoDepth = session.history().undoDepth();
        redoDepth = session.history().redoDepth();
    }
    if (status != forgeshape::SculptSession::SculptHistoryStatus::Ok) {
        FS_LOGI("FORGESHAPE_SCULPT_MASK_REFUSED:clear:%s",
                forgeshape::SculptSession::sculptHistoryStatusName(status));
        return sculptHistoryStatusCode(status);
    }
    FS_LOGI("FORGESHAPE_SCULPT_MASK:cleared meshRevision=%llu masked=%u undo=%d redo=%d",
            (unsigned long long)meshRevision, maskedAfter, (int)undoDepth, (int)redoDepth);
    return kHistoryOk;
}

// Whether Clear Mask has anything to do right now, so the control can be ABSENT
// rather than drawn and then refused. Native decides; the Java layer holds no
// copy of the rule.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_sculptCanClearMask(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::sculptSession().canClearMask() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sculptUndo(JNIEnv*, jclass) {
    return runSculptHistoryStep("undo", false);
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sculptRedo(JNIEnv*, jclass) {
    return runSculptHistoryStep("redo", true);
}

// --- what the two chrome controls mean, decided here ------------------------
//
// The mode owns the answer, so the dispatch lives beside the mode. In Sculpt
// the pair means the active body's strokes; everywhere else it means the
// Construction history, with behaviour byte-for-byte what it was before this
// feature existed.

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_historyUndoAvailable(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (forgeshape::sculptSession().inSculptMode()) {
        return forgeshape::sculptSession().canUndoSculpt() ? JNI_TRUE : JNI_FALSE;
    }
    return forgeshape::constructionHistory().canUndo() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_historyRedoAvailable(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (forgeshape::sculptSession().inSculptMode()) {
        return forgeshape::sculptSession().canRedoSculpt() ? JNI_TRUE : JNI_FALSE;
    }
    return forgeshape::constructionHistory().canRedo() ? JNI_TRUE : JNI_FALSE;
}

// The mode is re-read under the step's own lock rather than passed in, so a
// mode change between the availability read and the tap cannot route a step to
// the history the user is no longer in.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_historyUndo(JNIEnv*, jclass) {
    bool sculpting = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        sculpting = forgeshape::sculptSession().inSculptMode();
    }
    return sculpting ? runSculptHistoryStep("undo", false) : runHistoryStep("undo", false);
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_historyRedo(JNIEnv*, jclass) {
    bool sculpting = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        sculpting = forgeshape::sculptSession().inSculptMode();
    }
    return sculpting ? runSculptHistoryStep("redo", true) : runHistoryStep("redo", true);
}

// The transaction boundary, for a user act that is made of more than one
// mutation — creation today, a dragged handle next.
//
// Between begin and commit the ordinary mutation entry points still run and
// still publish, so the model on screen follows the edit live; what changes is
// only that they stop being history steps of their own. A commit that finds
// nothing different records nothing, so a creation that was refused leaves the
// history exactly as it was.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_beginConstructionEdit(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::constructionHistory().beginEdit() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_commitConstructionEdit(JNIEnv*, jclass) {
    bool recorded = false;
    size_t undoDepth = 0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        recorded = forgeshape::constructionHistory().commitEdit();
        undoDepth = forgeshape::constructionHistory().undoDepth();
    }
    FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY_COMMIT:%s undo=%d",
            recorded ? "recorded" : "no_change", (int)undoDepth);
    return recorded ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_cancelConstructionEdit(JNIEnv*, jclass) {
    forgeshape::ConstructionRestoreReport report;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::constructionHistory().cancelEdit(&report);
    }
    FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY_CANCEL: republished=%d placements=%d removed=%d",
            report.republishedBodies, report.replacedPlacements, report.removedBodies);
}

// ---------------------------------------------------------------------------
// Project persistence
// ---------------------------------------------------------------------------
//
// Two calls, and between them the whole of what the Android layer knows about a
// project file: it receives bytes and it hands bytes back. Where those bytes are
// stored, under what name and with what durability is the adapter's business
// (see ProjectSlot.java); what they MEAN is the codec's, and the codec has no
// idea a filesystem exists.
//
// Nothing about presentation crosses here. The camera pose, the open panels, the
// display unit, the theme, the shading model, the held tool and the brush are
// session or presentation state, not project truth, and `.forge` v1 deliberately
// carries none of them.

// Project status codes handed back to the Android UI. A JNI transport detail,
// in step with NativeViewport's PROJECT_* fields. They are deliberately coarser
// than ProjectCodecStatus: the user needs to know whether the file was damaged,
// too new or not a project at all, and the exact refusal is in logcat.
constexpr jint kProjectOk = 0;
constexpr jint kProjectNoData = 1;
constexpr jint kProjectNotAProject = 2;
constexpr jint kProjectUnsupportedVersion = 3;
constexpr jint kProjectDamaged = 4;
constexpr jint kProjectInvalid = 5;
constexpr jint kProjectBusy = 6;

jint projectStatusCode(forgeshape::ProjectCodecStatus status) {
    switch (status) {
        case forgeshape::ProjectCodecStatus::Ok:
            return kProjectOk;
        case forgeshape::ProjectCodecStatus::NotForgeFile:
            return kProjectNotAProject;
        case forgeshape::ProjectCodecStatus::UnsupportedMajor:
        case forgeshape::ProjectCodecStatus::UnsupportedSectionVersion:
            return kProjectUnsupportedVersion;
        case forgeshape::ProjectCodecStatus::BadHeader:
        case forgeshape::ProjectCodecStatus::Truncated:
        case forgeshape::ProjectCodecStatus::BadSectionHeader:
        case forgeshape::ProjectCodecStatus::ChecksumMismatch:
        case forgeshape::ProjectCodecStatus::UnknownRequiredSection:
        case forgeshape::ProjectCodecStatus::DuplicateSection:
        case forgeshape::ProjectCodecStatus::MissingRequiredSection:
        case forgeshape::ProjectCodecStatus::BadPayload:
        case forgeshape::ProjectCodecStatus::ImpossibleCount:
            return kProjectDamaged;
        case forgeshape::ProjectCodecStatus::InvalidSemanticValue:
        case forgeshape::ProjectCodecStatus::UnresolvedReference:
            return kProjectInvalid;
        case forgeshape::ProjectCodecStatus::RefusedEditInProgress:
            return kProjectBusy;
    }
    return kProjectInvalid;
}

// Encodes the running project to portable `.forge` v1 bytes.
//
// Reads only: it publishes nothing, mints no revision and cannot change the
// mode, the scene or the active body. Returns null when the document could not
// be encoded, which for a live project means the scene held a value the codec's
// own domain contracts refuse — a should-not-happen that is reported rather than
// written out as a file nothing could open.
JNIEXPORT jbyteArray JNICALL
Java_com_forgeshape_app_NativeViewport_encodeProject(JNIEnv* env, jclass) {
    std::vector<uint8_t> bytes;
    forgeshape::ProjectCodecStatus why = forgeshape::ProjectCodecStatus::Ok;
    forgeshape::ProjectKind kind = forgeshape::ProjectKind::Construction;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (!forgeshape::constructionScene().hasProject()) {
            // Home is not a project and has no document: nothing to save,
            // nothing to checkpoint. Said by name rather than left to the
            // codec's empty-SCNE refusal, because the shell asks this on every
            // lifecycle edge and the answer is not a fault.
            FS_LOGI("FORGESHAPE_PROJECT_ENCODE_SKIPPED:no_project");
            return nullptr;
        }
        // The mode the user is in IS the mode the file reopens in. Taken here,
        // under the same lock as the capture, so a file cannot say Sculpt and
        // then carry the scene as it was a moment before the mode changed.
        kind = forgeshape::sculptSession().inSculptMode() ? forgeshape::ProjectKind::Sculpt
                                                          : forgeshape::ProjectKind::Construction;
        const forgeshape::ProjectDocument document =
            forgeshape::captureProjectDocument(forgeshape::constructionScene(), kind);
        bytes = forgeshape::encodeProjectV1(document, &why);
    }
    if (bytes.empty()) {
        FS_LOGE("FORGESHAPE_PROJECT_ENCODE_FAIL:%s", forgeshape::projectCodecStatusName(why));
        return nullptr;
    }
    FS_LOGI("FORGESHAPE_PROJECT_ENCODED:%zu kind=%s bodies=%d", bytes.size(),
            forgeshape::projectKindName(kind), (int)forgeshape::constructionScene().bodyCount());
    jbyteArray out = env->NewByteArray(static_cast<jsize>(bytes.size()));
    if (out == nullptr) {
        return nullptr;
    }
    env->SetByteArrayRegion(out, 0, static_cast<jsize>(bytes.size()),
                            reinterpret_cast<const jbyte*>(bytes.data()));
    return out;
}

// Replaces the running project with the one these bytes describe, or changes
// nothing at all.
//
// Fail-closed in three stages, none of which touches the live project until the
// one before it has completely succeeded: decode and checksum into temporary
// document state, validate every id, count, reference and semantic value, then
// commit. Anything short of that returns a refusal and leaves the current scene,
// every Frozen Sculpt Mesh, the active mode, the active body and the session
// history exactly as they were.
//
// The state mutex is held across the whole load, for the same reason an undo
// holds it: the render thread takes its whole-scene snapshot under this mutex,
// and a frame that observed the body list mid-replacement would draw a scene
// that never existed.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_loadProject(JNIEnv* env, jclass, jbyteArray data) {
    if (data == nullptr) {
        FS_LOGE("FORGESHAPE_PROJECT_LOAD_FAIL:no_data");
        return kProjectNoData;
    }
    const jsize size = env->GetArrayLength(data);
    if (size <= 0) {
        FS_LOGE("FORGESHAPE_PROJECT_LOAD_FAIL:empty");
        return kProjectNoData;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    env->GetByteArrayRegion(data, 0, size, reinterpret_cast<jbyte*>(bytes.data()));

    forgeshape::ProjectDocument document;
    forgeshape::ProjectCodecStatus status =
        forgeshape::decodeProject(bytes.data(), bytes.size(), &document);
    if (status != forgeshape::ProjectCodecStatus::Ok) {
        FS_LOGE("FORGESHAPE_PROJECT_LOAD_REJECTED:%s bytes=%d",
                forgeshape::projectCodecStatusName(status), (int)size);
        return projectStatusCode(status);
    }

    forgeshape::ProjectLoadReport report;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        // Any gesture that was mid-flight belongs to a scene that is about to
        // stop existing. Dropping it here rather than letting it land is the
        // same rule a surface teardown follows.
        g_grabbing = false;
        g_strokePending = false;
        // And a sketch in progress belongs to it too. It is volatile by
        // contract and costs the project nothing to drop; the view it borrowed
        // is given back.
        if (forgeshape::sketchSession().active()) {
            forgeshape::sketchSession().cancel();
            endSketchView();
        }
        status = forgeshape::loadProjectDocument(document, forgeshape::constructionScene(),
                                                 forgeshape::sculptSession(),
                                                 forgeshape::constructionHistory(), &report);
    }
    if (status != forgeshape::ProjectCodecStatus::Ok) {
        FS_LOGE("FORGESHAPE_PROJECT_LOAD_REJECTED:%s bytes=%d",
                forgeshape::projectCodecStatusName(status), (int)size);
        return projectStatusCode(status);
    }
    FS_LOGI("FORGESHAPE_PROJECT_LOADED:%d bodies sculptMeshes=%d active=%llu kind=%s meshRev=%llu "
            "undo=0 redo=0",
            report.bodies, report.sculptMeshes, (unsigned long long)report.activeBodyId,
            forgeshape::projectKindName(report.kind), (unsigned long long)report.activeRevision);
    return kProjectOk;
}

// Exports the running project as GLB 2.0 bytes.
//
// Reads only: it publishes nothing, mints no revision, changes no mode and
// cannot touch the scene, the history or any `.forge` slot. Geometry is
// evaluated fresh from the Construction sources — or taken from the Frozen
// Sculpt Meshes when the session is sculpting — so what is exported is what the
// project currently IS, never a decoded file and never a GPU buffer.
//
// Returns null when nothing could be exported; the reason is logged. There is
// deliberately no status code across JNI: the product has exactly one thing to
// say to the user about a failed export, and the codec's own vocabulary belongs
// in the log rather than in five status strings nobody can act on differently.
JNIEXPORT jbyteArray JNICALL
Java_com_forgeshape_app_NativeViewport_exportGlb(JNIEnv* env, jclass) {
    std::vector<uint8_t> bytes;
    forgeshape::GlbExportStatus why = forgeshape::GlbExportStatus::Ok;
    forgeshape::ProjectKind kind = forgeshape::ProjectKind::Construction;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        // The mode the user is in decides which representation each body
        // exports, under the same lock as the capture, so a file cannot show a
        // sculpt mesh for a session that had already left Sculpt.
        kind = forgeshape::sculptSession().inSculptMode() ? forgeshape::ProjectKind::Sculpt
                                                          : forgeshape::ProjectKind::Construction;
        bytes = forgeshape::exportSceneAsGlb(forgeshape::constructionScene(), kind, &why);
    }
    if (bytes.empty()) {
        FS_LOGE("FORGESHAPE_GLB_EXPORT_FAIL:%s", forgeshape::glbExportStatusName(why));
        return nullptr;
    }
    FS_LOGI("FORGESHAPE_GLB_EXPORTED:%zu kind=%s bodies=%d", bytes.size(),
            forgeshape::projectKindName(kind),
            (int)forgeshape::constructionScene().bodyCount());
    jbyteArray out = env->NewByteArray(static_cast<jsize>(bytes.size()));
    if (out == nullptr) {
        return nullptr;
    }
    env->SetByteArrayRegion(out, 0, static_cast<jsize>(bytes.size()),
                            reinterpret_cast<const jbyte*>(bytes.data()));
    return out;
}

// ---------------------------------------------------------------------------
// IMPORT-01A — durable import
// ---------------------------------------------------------------------------
//
// This is the product path: it creates real bodies with real identities that
// are selectable, movable, undoable, saved into `.forge` and reopened. The
// preview below it is still a diagnostic and still creates nothing.

// How a commit refusal is distinguished from a parse refusal over one int.
//
// The two are different vocabularies — `GlbImportStatus` is about the FILE,
// `ImportCommitStatus` is about the PROJECT — and collapsing them into one
// enum would put "this file uses a sparse accessor" beside "this project
// already holds four thousand bodies". Offsetting the second keeps both stable
// tokens intact and keeps the Java side's mapping a single comparison. It must
// stay in step with NativeViewport.IMPORT_COMMIT_BASE.
constexpr jint kImportCommitStatusBase = 1000;

// Reads a `.glb` and creates durable Imported Mesh bodies from it.
//
// @return 0 on success; a GlbImportStatus ordinal when the FILE was refused;
//         kImportCommitStatusBase + an ImportCommitStatus ordinal when the
//         PROJECT refused it. Every non-zero answer leaves the scene, the
//         history and the project fingerprint exactly as they were.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_importGlbDurable(JNIEnv* env, jclass, jbyteArray data) {
    if (data == nullptr) {
        return static_cast<jint>(forgeshape::GlbImportStatus::NoData);
    }
    const jsize length = env->GetArrayLength(data);
    if (length <= 0) {
        return static_cast<jint>(forgeshape::GlbImportStatus::NoData);
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    env->GetByteArrayRegion(data, 0, length, reinterpret_cast<jbyte*>(bytes.data()));

    // Parsed OUTSIDE the lock: it is pure computation over a private buffer and
    // touches nothing shared, and holding the state mutex across a
    // multi-megabyte decode would stall the render thread for it.
    forgeshape::ParsedGlbScene parsed;
    const forgeshape::GlbImportStatus why =
            forgeshape::importGlb(bytes.data(), bytes.size(), &parsed);
    if (why != forgeshape::GlbImportStatus::Ok) {
        FS_LOGE("FORGESHAPE_IMPORT_FAIL:%s bytes=%d", forgeshape::glbImportStatusName(why),
                (int)length);
        return static_cast<jint>(why);
    }

    forgeshape::ImportCommitReport report;
    forgeshape::ImportCommitStatus committed = forgeshape::ImportCommitStatus::Ok;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        committed = forgeshape::commitImportedGlbScene(parsed, forgeshape::constructionScene(),
                                                       forgeshape::constructionHistory(), &report);
    }
    if (committed != forgeshape::ImportCommitStatus::Ok) {
        FS_LOGE("FORGESHAPE_IMPORT_FAIL:%s geometry=%s bytes=%d",
                forgeshape::importCommitStatusName(committed),
                forgeshape::importedMeshValidationName(report.geometryWhy), (int)length);
        return kImportCommitStatusBase + static_cast<jint>(committed);
    }
    FS_LOGI("FORGESHAPE_IMPORTED:%d objects=%d vertices=%u triangles=%u batches=%u "
            "firstObjectId=%llu activeObjectId=%llu",
            (int)length, report.objects, report.vertices, report.triangles, report.batches,
            (unsigned long long)report.firstObjectId,
            (unsigned long long)report.activeBodyId);
    return 0;
}

// The stable refusal token for a commit refusal, for the diagnostics ring and
// the log. Bounded, and never a path, a `Uri` or a byte of the file.
JNIEXPORT jstring JNICALL
Java_com_forgeshape_app_NativeViewport_glbCommitStatusToken(JNIEnv* env, jclass, jint status) {
    const jint ordinal = status - kImportCommitStatusBase;
    if (ordinal < 0
        || ordinal > static_cast<jint>(forgeshape::ImportCommitStatus::RefusedEditInProgress)) {
        return env->NewStringUTF("unknown");
    }
    return env->NewStringUTF(forgeshape::importCommitStatusName(
            static_cast<forgeshape::ImportCommitStatus>(ordinal)));
}

// The bounded category a commit refusal is shown as.
//
// The same three answers a parse refusal uses, because they are the three
// things a person can act on, and the domain decides which is which so the
// Android layer never has to know.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_glbCommitStatusCategory(JNIEnv*, jclass, jint status) {
    const jint ordinal = status - kImportCommitStatusBase;
    switch (static_cast<forgeshape::ImportCommitStatus>(ordinal)) {
        case forgeshape::ImportCommitStatus::TooManyObjects:
            // A valid file this project cannot hold: the same shape of answer
            // as a valid glTF feature this reader does not implement.
            return static_cast<jint>(forgeshape::GlbImportCategory::Unsupported);
        case forgeshape::ImportCommitStatus::RejectedGeometry:
            return static_cast<jint>(forgeshape::GlbImportCategory::Inconsistent);
        default:
            // NothingToImport, RefusedEditInProgress and anything out of range.
            // The last is unreachable from the product — a menu cannot be
            // opened mid-drag — and is categorised rather than given a fourth
            // user-facing sentence nobody would ever read.
            return static_cast<jint>(forgeshape::GlbImportCategory::Unreadable);
    }
}

// ---------------------------------------------------------------------------
// GLB-IMPORT-R0 — the diagnostic imported mesh preview
// ---------------------------------------------------------------------------
//
// Every entry below is a DIAGNOSTIC. None of them creates a body, mints an
// ObjectId, publishes a revision, records a history step or touches either
// `.forge` slot, and the preview they operate on disappears with the process.

// Parses GLB bytes and loads them as the session's imported preview.
//
// @return 0 on success, or the GlbImportStatus ordinal. A refusal leaves any
//         existing preview and the whole project exactly as they were.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_importGlbPreview(JNIEnv* env, jclass, jbyteArray data) {
    if (data == nullptr) {
        return static_cast<jint>(forgeshape::GlbImportStatus::NoData);
    }
    const jsize length = env->GetArrayLength(data);
    if (length <= 0) {
        return static_cast<jint>(forgeshape::GlbImportStatus::NoData);
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    env->GetByteArrayRegion(data, 0, length, reinterpret_cast<jbyte*>(bytes.data()));

    forgeshape::ParsedGlbScene scene;
    const forgeshape::GlbImportStatus why =
            forgeshape::importGlb(bytes.data(), bytes.size(), &scene);
    if (why != forgeshape::GlbImportStatus::Ok) {
        FS_LOGE("FORGESHAPE_GLB_IMPORT_FAIL:%s bytes=%d", forgeshape::glbImportStatusName(why),
                (int)length);
        return static_cast<jint>(why);
    }
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (!forgeshape::importedMeshPreview().load(scene)) {
            FS_LOGE("FORGESHAPE_GLB_IMPORT_FAIL:PreviewRejected bytes=%d", (int)length);
            return static_cast<jint>(forgeshape::GlbImportStatus::NothingToImport);
        }
    }
    FS_LOGI("FORGESHAPE_GLB_IMPORTED:%d meshes=%u vertices=%u triangles=%u", (int)length,
            forgeshape::importedMeshPreview().meshCount(),
            forgeshape::importedMeshPreview().vertexCount(),
            forgeshape::importedMeshPreview().triangleCount());
    return 0;
}

// An ordinal that came back over JNI, turned back into a status it is safe to
// switch on. Anything out of range is reported as NoData rather than cast into
// an enum value the domain does not have.
static forgeshape::GlbImportStatus importStatusFromOrdinal(jint ordinal) {
    if (ordinal < 0
        || ordinal > static_cast<jint>(forgeshape::GlbImportStatus::TooLarge)) {
        return forgeshape::GlbImportStatus::NoData;
    }
    return static_cast<forgeshape::GlbImportStatus>(ordinal);
}

// The stable refusal token, for the diagnostics ring and the log. A bounded
// name and never a path, a `Uri` or a byte of the file.
JNIEXPORT jstring JNICALL
Java_com_forgeshape_app_NativeViewport_glbImportStatusToken(JNIEnv* env, jclass, jint status) {
    return env->NewStringUTF(
            forgeshape::glbImportStatusName(importStatusFromOrdinal(status)));
}

// The bounded category the user is shown. Three answers, chosen in the domain
// so the Android layer never has to know which statuses mean what.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_glbImportStatusCategory(JNIEnv*, jclass, jint status) {
    return static_cast<jint>(
            forgeshape::glbImportStatusCategory(importStatusFromOrdinal(status)));
}

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_clearGlbPreview(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    forgeshape::importedMeshPreview().clear();
    FS_LOGI("FORGESHAPE_GLB_PREVIEW_CLEARED");
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_glbPreviewLoaded(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::importedMeshPreview().loaded() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_setGlbPreviewVisible(JNIEnv*, jclass, jboolean visible) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    forgeshape::importedMeshPreview().setVisible(visible == JNI_TRUE);
    FS_LOGI("FORGESHAPE_GLB_PREVIEW_VISIBLE:%d",
            forgeshape::importedMeshPreview().visible() ? 1 : 0);
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_glbPreviewVisible(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::importedMeshPreview().visible() ? JNI_TRUE : JNI_FALSE;
}

// Fills {meshes, vertices, triangles} and, when the array is long enough, the
// draw-batch count as a fourth entry. Zeros when nothing is loaded.
//
// The first three are the FILE's counts. A batch is one TRIANGLES primitive and
// is a renderer fact, reported separately so a diagnostic can say a
// multi-primitive file did not quietly lose a primitive.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_glbPreviewCounts(JNIEnv* env, jclass, jintArray out) {
    if (out == nullptr) {
        return;
    }
    const jsize length = env->GetArrayLength(out);
    if (length < 3) {
        return;
    }
    jint counts[4] = {0, 0, 0, 0};
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::ImportedMeshPreview& preview = forgeshape::importedMeshPreview();
        counts[0] = static_cast<jint>(preview.meshCount());
        counts[1] = static_cast<jint>(preview.vertexCount());
        counts[2] = static_cast<jint>(preview.triangleCount());
        counts[3] = static_cast<jint>(preview.batchCount());
    }
    env->SetIntArrayRegion(out, 0, length >= 4 ? 4 : 3, counts);
}

// Fills {minX, minY, minZ, maxX, maxY, maxZ} with the preview's WORLD bounds.
//
// World, because the importer bakes the node transform into the vertices —
// which is what makes this the number that says a node matrix was applied, and
// applied the right way round. Leaves `out` untouched with nothing loaded.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_glbPreviewBounds(JNIEnv* env, jclass, jfloatArray out) {
    if (out == nullptr || env->GetArrayLength(out) < 6) {
        return JNI_FALSE;
    }
    float bounds[6] = {0, 0, 0, 0, 0, 0};
    bool any = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        any = forgeshape::importedMeshPreview().worldBounds(bounds);
    }
    if (!any) {
        return JNI_FALSE;
    }
    env->SetFloatArrayRegion(out, 0, 6, bounds);
    return JNI_TRUE;
}

// DEBUG/TEST ONLY: whether every loaded preview batch renders both sides.
//
// The seam that actually decides culling is the published mesh's own two-sided
// flag, and a test has to be able to read it: a screenshot of a flat grey
// surface cannot tell a culled back face from a drawn one reliably. Reads only,
// and false with nothing loaded.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_debugPreviewRendersBothSides(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const forgeshape::SceneSnapshot& snapshot = forgeshape::importedMeshPreview().snapshot();
    if (snapshot.empty()) {
        return JNI_FALSE;
    }
    for (const forgeshape::SceneDrawItem& item : snapshot) {
        if (item.mesh == nullptr || !item.mesh->renderBothSides()) {
            return JNI_FALSE;
        }
    }
    return JNI_TRUE;
}

// The deterministic Nomad-like compatibility fixture, as GLB bytes.
//
// A TEST SEAM, not a product path. The owner's own low-poly character is not in
// this repository, so GLB-IMPORT-R1 is proven against a synthetic file that
// carries the same structural features — a node matrix, seven TRIANGLES
// primitives over one shared POSITION accessor, no NORMAL, ignored colour and
// UV attributes, a double-sided material. Nothing in the product calls this:
// it exists so an instrumented test can drive the REAL parse and the REAL
// preview switch with bytes it did not have to ship as an asset.
JNIEXPORT jbyteArray JNICALL
Java_com_forgeshape_app_NativeViewport_nomadLikeGlbFixture(JNIEnv* env, jclass) {
    const std::vector<uint8_t> bytes = forgeshape::buildNomadLikeGlbFixture();
    jbyteArray out = env->NewByteArray(static_cast<jsize>(bytes.size()));
    if (out == nullptr) {
        return nullptr;
    }
    env->SetByteArrayRegion(out, 0, static_cast<jsize>(bytes.size()),
                            reinterpret_cast<const jbyte*>(bytes.data()));
    return out;
}

// The roundtrip diagnostic, as a report a person and a test can both read.
//
// Exports the live scene through the real writer, reads the bytes back with the
// independent parser and compares both against DOMAIN truth. Reads only.
JNIEXPORT jstring JNICALL
Java_com_forgeshape_app_NativeViewport_glbRoundtripReport(JNIEnv* env, jclass) {
    std::string report;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::ProjectKind kind = forgeshape::sculptSession().inSculptMode()
                ? forgeshape::ProjectKind::Sculpt
                : forgeshape::ProjectKind::Construction;
        report = forgeshape::formatRoundtripReport(
                forgeshape::runGlbRoundtripDiagnostic(forgeshape::constructionScene(), kind));
    }
    return newJavaString(env, report);
}

// The same comparison against bytes the caller supplies, so a file from the
// evidence bundle can be checked against the project it was written from.
JNIEXPORT jstring JNICALL
Java_com_forgeshape_app_NativeViewport_glbCompareReport(JNIEnv* env, jclass, jbyteArray data) {
    if (data == nullptr) {
        return env->NewStringUTF("verdict=ROUNDTRIP_NOT_COMPARABLE\nreason=no bytes\n");
    }
    const jsize length = env->GetArrayLength(data);
    std::vector<uint8_t> bytes(static_cast<size_t>(length > 0 ? length : 0));
    if (length > 0) {
        env->GetByteArrayRegion(data, 0, length, reinterpret_cast<jbyte*>(bytes.data()));
    }
    std::string report;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::ProjectKind kind = forgeshape::sculptSession().inSculptMode()
                ? forgeshape::ProjectKind::Sculpt
                : forgeshape::ProjectKind::Construction;
        report = forgeshape::formatRoundtripReport(forgeshape::compareSceneToGlb(
                forgeshape::constructionScene(), kind, bytes.data(), bytes.size()));
    }
    return newJavaString(env, report);
}

// Validates bytes as a project WITHOUT applying them.
//
// The recovery flow has to answer "is there a candidate worth offering?" before
// it may touch anything the user can see, and the only honest way to answer it
// is to run the real decoder. This is that, and nothing else: it decodes and
// validates into temporary document state and throws it away. It publishes no
// mesh, replaces no scene, changes no mode and clears no history — a file that
// passes here is still not loaded, and a file that fails here has cost the live
// project nothing.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_validateProject(JNIEnv* env, jclass, jbyteArray data) {
    if (data == nullptr) {
        return kProjectNoData;
    }
    const jsize size = env->GetArrayLength(data);
    if (size <= 0) {
        return kProjectNoData;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    env->GetByteArrayRegion(data, 0, size, reinterpret_cast<jbyte*>(bytes.data()));

    forgeshape::ProjectDocument document;
    const forgeshape::ProjectCodecStatus status =
        forgeshape::decodeProject(bytes.data(), bytes.size(), &document);
    if (status != forgeshape::ProjectCodecStatus::Ok) {
        FS_LOGI("FORGESHAPE_PROJECT_VALIDATE_REJECTED:%s bytes=%d",
                forgeshape::projectCodecStatusName(status), (int)size);
        return projectStatusCode(status);
    }
    // The one rule the codec cannot express, checked here so a candidate this
    // build could never load is never offered as one. Asked through the SHARED
    // predicate rather than restated: a second copy of it here is exactly what
    // went stale when `IMPORT-01A` gave a body a second way to have geometry.
    if (!forgeshape::runtimeCanEvaluateProject(document)) {
        FS_LOGI("FORGESHAPE_PROJECT_VALIDATE_REJECTED:MissingRequiredSection bytes=%d", (int)size);
        return kProjectDamaged;
    }
    return kProjectOk;
}

// A cheap fingerprint of what a `.forge` document would contain right now.
//
// Autosave's whole economy rests on this: it is asked often, it costs a hash of
// the semantic values rather than a serialization, and it changes if and only if
// the file would. See projectSemanticFingerprint — in particular why it hashes
// VALUES and not the domain's update counters, which an undo deliberately does
// not advance.
JNIEXPORT jlong JNICALL
Java_com_forgeshape_app_NativeViewport_projectFingerprint(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!forgeshape::constructionScene().hasProject()) {
        return 0;  // no project, no document, no fingerprint
    }
    const forgeshape::ProjectKind kind = forgeshape::sculptSession().inSculptMode()
                                             ? forgeshape::ProjectKind::Sculpt
                                             : forgeshape::ProjectKind::Construction;
    return static_cast<jlong>(
        forgeshape::projectSemanticFingerprint(forgeshape::constructionScene(), kind));
}

// ---------------------------------------------------------------------------
// Home and the project lifecycle (`APP-H1`)
// ---------------------------------------------------------------------------
//
// A project is open exactly when the scene holds a body; there is no second
// flag. The shell derives Home from this, so a rotation, a recreation and a
// resume all land where native truth says rather than where a Java field
// remembers.

JNIEXPORT jboolean JNICALL Java_com_forgeshape_app_NativeViewport_projectOpen(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return forgeshape::constructionScene().hasProject() ? JNI_TRUE : JNI_FALSE;
}

// Closes the project: the way to Home. Every body is destroyed, the history is
// dropped (it described a scene that no longer exists, exactly as after a
// load), any sketch or support selection is cancelled with its borrowed view
// given back, any stroke in flight is dropped, the session leaves Sculpt and
// the gizmo is withdrawn. Writes NOTHING: whether the work was saved first is
// the shell's dirty-guard question, answered before this is called.
JNIEXPORT void JNICALL Java_com_forgeshape_app_NativeViewport_closeProject(JNIEnv*, jclass) {
    int bodies = 0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
        bodies = static_cast<int>(scene.bodyCount());
        g_grabbing = false;
        g_strokePending = false;
        forgeshape::supportChooser().cancel();
        if (forgeshape::sketchSession().active()) {
            forgeshape::sketchSession().cancel();
        }
        endSketchView();
        forgeshape::sculptSession().cancelStroke();
        forgeshape::sculptSession().enterConstruction();
        forgeshape::gizmoSession().setActive(false);
        g_selection.resetGesture();
        forgeshape::constructionHistory().clear();
        scene.closeProject();
    }
    FS_LOGI("FORGESHAPE_PROJECT_CLOSED bodies=%d", bodies);
}

// How many times the active body was read while no project was open. Zero in
// every product flow; the device suite asserts it across Home, the bootstrap
// and the first commit. Read-only.
JNIEXPORT jlong JNICALL
Java_com_forgeshape_app_NativeViewport_debugActiveBodyMisuseCount(JNIEnv*, jclass) {
    return static_cast<jlong>(forgeshape::ConstructionScene::activeBodyMisuseCount());
}

// Where the renderer stands, in step with NativeViewport's RENDERER_* fields.
constexpr jint kRendererHealthy = 0;
constexpr jint kRendererRecovering = 1;
constexpr jint kRendererRestartRequired = 2;

JNIEXPORT jint JNICALL Java_com_forgeshape_app_NativeViewport_rendererLifecycle(JNIEnv*, jclass) {
    // Read without the render thread's cooperation on purpose: this is a
    // diagnostic the UI thread asks for, and blocking the UI on a render thread
    // that may be mid-rebuild is exactly what a status read must not do. The
    // value is a plain enum written by one thread and read by another; a
    // momentarily stale answer is corrected on the next read.
    switch (g_rendererLifecycle.load(std::memory_order_relaxed)) {
        case 1: return kRendererRecovering;
        case 2: return kRendererRestartRequired;
        default: return kRendererHealthy;
    }
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_debugRendererDeviceRebuilds(JNIEnv*, jclass) {
    return g_rendererDeviceRebuilds.load(std::memory_order_relaxed);
}

// The selection outline's renderer diagnostics (`SEL-OUT-R1` §12/§13).
//
// Read without the render thread's cooperation, exactly as the lifecycle above
// is and for the same reason: a diagnostic must not block the UI thread on a
// render thread that may be mid-rebuild, and a momentarily stale count is
// corrected on the next read.
//
//   [0] mask + depth image ALLOCATIONS for the life of the process. Moves on a
//       swapchain extent change or a device rebuild and on NOTHING else — a
//       selection switch that moved it would be the leak this reports on.
//   [1] frames in which the mask pass was recorded
//   [2] composite draws recorded
//   [3] mask width in pixels    [4] mask height in pixels
//   [5] the band's half-width in screen pixels the last composite used
//   [6] 1 when the outline is currently enabled
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_selectionOutlineStats(JNIEnv* env, jclass,
                                                             jdoubleArray out) {
    if (out == nullptr) {
        return;
    }
    const jsize length = env->GetArrayLength(out);
    if (length < 7) {
        return;
    }
    jdouble values[7];
    values[0] = static_cast<jdouble>(g_outlineMaskAllocations.load(std::memory_order_relaxed));
    values[1] = static_cast<jdouble>(g_outlineMaskPassFrames.load(std::memory_order_relaxed));
    values[2] = static_cast<jdouble>(g_outlineCompositeDraws.load(std::memory_order_relaxed));
    values[3] = static_cast<jdouble>(g_outlineMaskWidth.load(std::memory_order_relaxed));
    values[4] = static_cast<jdouble>(g_outlineMaskHeight.load(std::memory_order_relaxed));
    values[5] =
        static_cast<jdouble>(g_outlineWidthPixelsCentis.load(std::memory_order_relaxed)) / 100.0;
    values[6] = forgeshape::displaySettings().selectionOutlineVisible() ? 1.0 : 0.0;
    env->SetDoubleArrayRegion(out, 0, 7, values);
}

#ifndef NDEBUG
// DEBUG-ONLY: makes the next frame behave exactly as though the GPU device had
// been lost.
//
// Not a product path and unreachable from any UI. It exists because the
// alternative — provoking a real `VK_ERROR_DEVICE_LOST` — means destabilising
// the GPU of the authoritative emulator, which the repository forbids, and
// would make the test depend on driver behaviour rather than on ForgeShape's.
// Everything after the injection point is the real recovery path.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_debugInjectDeviceLoss(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_viewport.mutex);
    g_viewport.injectDeviceLossRequested = true;
    g_viewport.toRender.notify_all();
    FS_LOGI("FORGESHAPE_RENDER_DEVICE_LOSS_INJECTED");
    return JNI_TRUE;
}
#else
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_debugInjectDeviceLoss(JNIEnv*, jclass) {
    return JNI_FALSE;
}
#endif

// Reads the authoritative sculpt state for display. Nothing here is measured
// from the mesh: every value is state the session owns.
//
//   [0] mode (0 = construction, 1 = sculpt)
//   [1] 1 when a Frozen Sculpt Mesh exists
//   [2] SculptRevision
//   [3] vertex count      [4] index count
//   [5] brush radius, screen pixels
//   [6] brush strength
//   [7] 1 when the Construction Source changed since the Freeze
//   [8] completed stroke count, for the LIFE OF THE SESSION — diagnostic only.
//       Deliberately not what the destructive re-Freeze guard asks: it counts
//       strokes on frozen meshes that no longer exist, so it can never say
//       whether re-freezing would destroy anything the user still has.
//   [9] ObjectId
//  [10] active tool (0 grab, 1 clay, 2 smooth, 3 inflate, 4 flatten, 5 crease,
//       6 mask)
//  [11] 1 when the CURRENT Frozen Sculpt Mesh has user edits. This is the one
//       the re-Freeze guard asks, so the Java layer decides nothing about what
//       counts as an edit and holds no copy of the rule — see
//       SculptMesh::hasEdits().
//  [12] how many vertices carry a non-zero Sculpt Mask (`SCULPT-FCM-R1`).
//       Runtime state, never project truth: it is what decides whether the
//       Clear Mask control is drawn, and the Java layer holds no mask of its
//       own to derive it from.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_sculptState(JNIEnv* env, jclass, jdoubleArray outState) {
    constexpr jsize kSculptStateSize = 13;
    if (outState == nullptr || env->GetArrayLength(outState) < kSculptStateSize) {
        return;
    }
    // One locked read of the session and its target, so the twelve values
    // describe one body at one instant; the JNI write happens after the lock.
    jdouble values[kSculptStateSize];
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        const forgeshape::SculptSession& session = forgeshape::sculptSession();
        const forgeshape::SculptMesh& mesh = session.mesh();
        const jdouble read[kSculptStateSize] = {
            session.inSculptMode() ? 1.0 : 0.0,
            session.hasSculptMesh() ? 1.0 : 0.0,
            static_cast<jdouble>(mesh.revision()),
            static_cast<jdouble>(mesh.vertexCount()),
            static_cast<jdouble>(mesh.indexCount()),
            static_cast<jdouble>(session.radiusPixels()),
            static_cast<jdouble>(session.strength()),
            session.sourceStale() ? 1.0 : 0.0,
            static_cast<jdouble>(session.strokeCount()),
            static_cast<jdouble>(mesh.objectId()),
            static_cast<jdouble>(forgeshape::sculptToolIndex(session.tool())),
            mesh.hasEdits() ? 1.0 : 0.0,
            static_cast<jdouble>(mesh.maskedVertexCount()),
        };
        std::copy(read, read + kSculptStateSize, values);
    }
    env->SetDoubleArrayRegion(outState, 0, kSculptStateSize, values);
}

// Sets the brush. Both values are CLAMPED into their documented ranges rather
// than refused: a slider cannot produce a meaningless value, and native code
// stays the authority on what the brush actually is. Radius and Strength are
// shared by all four tools — there is deliberately no per-tool copy of either,
// so switching tools never changes how big or how strong the brush is.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_setSculptBrush(JNIEnv*, jclass, jdouble radiusPixels,
                                                       jdouble strength) {
    const char* toolName = "";
    float radiusNow = 0.0f;
    float strengthNow = 0.0f;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SculptSession& session = forgeshape::sculptSession();
        session.setRadiusPixels(static_cast<float>(radiusPixels));
        session.setStrength(static_cast<float>(strength));
        toolName = forgeshape::sculptToolName(session.tool());
        radiusNow = session.radiusPixels();
        strengthNow = session.strength();
    }
    FS_LOGI("FORGESHAPE_SCULPT_BRUSH:%s radiusPx=%.1f strength=%.3f", toolName, radiusNow,
            strengthNow);
}

// Selects the active tool. Java may REQUEST a tool and is told which tool is
// actually active, exactly as it is told what the mode actually is: an unknown
// index is refused and the current tool stands, rather than being clamped into
// some neighbouring tool the user did not ask for.
//
// This changes no geometry, publishes no revision and uploads nothing. A stroke
// already in progress keeps the tool it captured on Down, so a tool cannot
// change under a finger that is already moving.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_setSculptTool(JNIEnv*, jclass, jint toolIndex) {
    forgeshape::SculptTool requested = forgeshape::SculptTool::Grab;
    const bool known = forgeshape::sculptToolFromIndex(static_cast<int>(toolIndex), &requested);
    forgeshape::SculptTool active = requested;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::SculptSession& session = forgeshape::sculptSession();
        if (known) {
            session.setTool(requested);
        }
        active = session.tool();
    }
    FS_LOGI("FORGESHAPE_SCULPT_TOOL:%s requested=%d known=%d", forgeshape::sculptToolName(active),
            static_cast<int>(toolIndex), known ? 1 : 0);
    return static_cast<jint>(forgeshape::sculptToolIndex(active));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sculptTool(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jint>(forgeshape::sculptToolIndex(forgeshape::sculptSession().tool()));
}

// --- viewport display settings (presentation only) --------------------------
//
// These four take no lock. The values are independent atomics that no geometry
// invariant depends on, and holding g_stateMutex here would let a display
// control block on a running stroke for no reason. Each setter returns the
// value that is actually in effect afterwards, so a refused index leaves the UI
// showing the truth rather than its own optimistic guess.
//
// None of these publishes a mesh, mints a revision or touches picking.

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_setShadingModel(JNIEnv*, jclass, jint modelIndex) {
    forgeshape::DisplaySettingsStore& settings = forgeshape::displaySettings();
    forgeshape::ShadingModel requested = settings.shadingModel();
    const bool known = forgeshape::shadingModelFromIndex(static_cast<int>(modelIndex), &requested);
    bool changed = false;
    if (known) {
        changed = settings.setShadingModel(requested);
    }
    FS_LOGI("FORGESHAPE_SHADING_MODEL:%s requested=%d known=%d changed=%d",
            forgeshape::shadingModelName(settings.shadingModel()), static_cast<int>(modelIndex),
            known ? 1 : 0, changed ? 1 : 0);
    return static_cast<jint>(forgeshape::shadingModelIndex(settings.shadingModel()));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_shadingModel(JNIEnv*, jclass) {
    return static_cast<jint>(
        forgeshape::shadingModelIndex(forgeshape::displaySettings().shadingModel()));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_setSurfaceShading(JNIEnv*, jclass, jint shadingIndex) {
    forgeshape::DisplaySettingsStore& settings = forgeshape::displaySettings();
    forgeshape::SurfaceShading requested = settings.surfaceShading();
    const bool known =
        forgeshape::surfaceShadingFromIndex(static_cast<int>(shadingIndex), &requested);
    bool changed = false;
    if (known) {
        changed = settings.setSurfaceShading(requested);
    }
    FS_LOGI("FORGESHAPE_SURFACE_SHADING:%s requested=%d known=%d changed=%d",
            forgeshape::surfaceShadingName(settings.surfaceShading()),
            static_cast<int>(shadingIndex), known ? 1 : 0, changed ? 1 : 0);
    return static_cast<jint>(
        forgeshape::surfaceShadingIndex(settings.surfaceShading()));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_surfaceShading(JNIEnv*, jclass) {
    return static_cast<jint>(
        forgeshape::surfaceShadingIndex(forgeshape::displaySettings().surfaceShading()));
}

// The viewport background: what the render pass clears to, behind everything.
//
// This is the ONE presentation value the Android theme system hands down, and it
// crosses as a closed appearance index rather than as a theme or an RGB. Native
// code owns what each appearance looks like, so the geometry domain never learns
// that an Android theme exists. Refused if unrecognised, exactly like a shading
// model, and the caller is told what is actually in effect afterwards.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_setViewportBackground(JNIEnv*, jclass,
                                                            jint backgroundIndex) {
    forgeshape::DisplaySettingsStore& settings = forgeshape::displaySettings();
    forgeshape::ViewportBackground requested = settings.viewportBackground();
    const bool known = forgeshape::viewportBackgroundFromIndex(
        static_cast<int>(backgroundIndex), &requested);
    bool changed = false;
    if (known) {
        changed = settings.setViewportBackground(requested);
    }
    FS_LOGI("FORGESHAPE_VIEWPORT_BACKGROUND:%s requested=%d known=%d changed=%d",
            forgeshape::viewportBackgroundName(settings.viewportBackground()),
            static_cast<int>(backgroundIndex), known ? 1 : 0, changed ? 1 : 0);
    return static_cast<jint>(
        forgeshape::viewportBackgroundIndex(settings.viewportBackground()));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_viewportBackground(JNIEnv*, jclass) {
    return static_cast<jint>(
        forgeshape::viewportBackgroundIndex(forgeshape::displaySettings().viewportBackground()));
}

// The world reference grid's visibility.
//
// A plain bool rather than an index, because there is no third answer to refuse:
// unlike a shading model or an appearance, "on" and "off" exhaust the values. It
// returns what is actually in effect afterwards for the same reason all of these
// do — the Android control repaints from the answer, never from what was tapped.
//
// This is presentation and only presentation. It publishes no mesh, mints no
// MeshRevision, changes no Construction parameter and moves no sculpt vertex;
// the grid is not a SceneObject, has no ObjectId and is invisible to picking.
// See forgeshape_grid.h for the whole contract.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_setGridVisible(JNIEnv*, jclass, jboolean visible) {
    forgeshape::DisplaySettingsStore& settings = forgeshape::displaySettings();
    const bool wanted = (visible == JNI_TRUE);
    const bool changed = settings.setGridVisible(wanted);
    FS_LOGI("FORGESHAPE_VIEWPORT_GRID:%d changed=%d", settings.gridVisible() ? 1 : 0,
            changed ? 1 : 0);
    return settings.gridVisible() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_gridVisible(JNIEnv*, jclass) {
    return forgeshape::displaySettings().gridVisible() ? JNI_TRUE : JNI_FALSE;
}

// The selected body's persistent outline (`SEL-OUT-R1`, UI-OWNER-11).
//
// The grid's shape exactly, and deliberately so: a plain bool because "on" and
// "off" exhaust the answers, returning what is actually in effect afterwards
// because the Android chip repaints from the answer and never from what was
// tapped, and living in the same process-scoped display store so it survives a
// HOME/resume with no save/restore code above JNI.
//
// Presentation and only presentation. It publishes no mesh, mints no
// MeshRevision, changes no Construction or CAD parameter, moves no sculpt
// vertex, records no history step, dirties no project and reaches no `.forge`
// byte. It cannot even reach a body's GPU buffers: with it on, the renderer
// rasterises the buffers that were already there for that body's own draw.
JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_setSelectionOutlineVisible(JNIEnv*, jclass,
                                                                 jboolean visible) {
    forgeshape::DisplaySettingsStore& settings = forgeshape::displaySettings();
    const bool wanted = (visible == JNI_TRUE);
    const bool changed = settings.setSelectionOutlineVisible(wanted);
    FS_LOGI("FORGESHAPE_VIEWPORT_SELECTION_OUTLINE:%d changed=%d",
            settings.selectionOutlineVisible() ? 1 : 0, changed ? 1 : 0);
    return settings.selectionOutlineVisible() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_selectionOutlineVisible(JNIEnv*, jclass) {
    return forgeshape::displaySettings().selectionOutlineVisible() ? JNI_TRUE : JNI_FALSE;
}

// Reduced motion: the whole of the accessibility seam, and deliberately one
// bool.
//
// The Android layer reads the platform's animator duration scale and decides
// what it means; what crosses here is only the answer. Native code must not
// learn that an Android setting exists, exactly as it must not learn that an
// Android theme does — and the renderer must not learn either, which is why the
// value lands in the display store beside the other presentation state rather
// than in a renderer method of its own.
//
// It changes nothing but timing. No revision is minted, no mesh is published,
// no buffer is touched: the only observable difference is whether a newly
// selected body reaches its resting tint over 220 ms or in one frame.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_setReducedMotion(JNIEnv*, jclass, jboolean reduced) {
    const bool wanted = (reduced == JNI_TRUE);
    if (forgeshape::displaySettings().setReducedMotion(wanted)) {
        FS_LOGI("FORGESHAPE_REDUCED_MOTION:%d", wanted ? 1 : 0);
    }
}

JNIEXPORT jboolean JNICALL
Java_com_forgeshape_app_NativeViewport_reducedMotion(JNIEnv*, jclass) {
    return forgeshape::displaySettings().reducedMotion() ? JNI_TRUE : JNI_FALSE;
}

// The projection mode is CAMERA state, not a display setting, so it is taken
// under the same lock the camera gestures use rather than through the display
// store. It publishes no mesh, mints no revision and touches no geometry: the
// only thing it changes is which pixels the existing geometry lands on.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_setProjectionMode(JNIEnv*, jclass, jint modeIndex) {
    forgeshape::ProjectionMode requested = forgeshape::kDefaultProjectionMode;
    const bool known =
        forgeshape::projectionModeFromIndex(static_cast<int>(modeIndex), &requested);
    bool changed = false;
    forgeshape::ProjectionMode active = forgeshape::kDefaultProjectionMode;
    float span = 0.0f;
    float distance = 0.0f;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (known) {
            changed = g_camera.setProjectionMode(requested);
        }
        active = g_camera.projectionMode();
        span = g_camera.orthoHalfHeightMeters();
        distance = g_camera.distance();
    }
    FS_LOGI("FORGESHAPE_PROJECTION_MODE:%s requested=%d known=%d changed=%d "
            "orthoHalfHeightMeters=%.4f distance=%.4f",
            forgeshape::projectionModeName(active), static_cast<int>(modeIndex), known ? 1 : 0,
            changed ? 1 : 0, span, distance);
    return static_cast<jint>(forgeshape::projectionModeIndex(active));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_projectionMode(JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return static_cast<jint>(forgeshape::projectionModeIndex(g_camera.projectionMode()));
}

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_surfaceCreated(JNIEnv* env, jclass, jobject surface) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, FS_TAG,
                            "FORGESHAPE_NATIVE_VIEWPORT_FAIL:ANativeWindow_fromSurface_null");
        return;
    }
    FS_LOGI("JNI: surfaceCreated -> ANativeWindow %dx%d",
            ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
    {
        // A new Surface means any pointers we were tracking are gone. The
        // camera pose and the selected object are deliberately preserved.
        std::lock_guard<std::mutex> stateLock(g_stateMutex);
        g_camera.setViewport(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
        g_camera.resetGesture();
        g_selection.resetGesture();
        // A Surface swap means the finger that was drawing is gone. The stroke
        // is dropped; the sculpted vertices and the active mode are deliberately
        // preserved, because they are process-scoped, not surface-scoped.
        forgeshape::sculptSession().cancelStroke();
        g_grabbing = false;
        g_strokePending = false;
    }

    std::lock_guard<std::mutex> lock(g_viewport.mutex);
    if (g_viewport.pendingWindow != nullptr) {
        ANativeWindow_release(g_viewport.pendingWindow);
    }
    g_viewport.pendingWindow = window;  // ownership handed to the render thread
    g_viewport.attachRequested = true;
    g_viewport.detachRequested = false;
    g_viewport.toRender.notify_all();
}

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_surfaceChanged(JNIEnv*, jclass, jint width, jint height) {
    std::lock_guard<std::mutex> lock(g_viewport.mutex);
    g_viewport.pendingWidth = width;
    g_viewport.pendingHeight = height;
    g_viewport.resizeRequested = true;
    g_viewport.toRender.notify_all();
    {
        std::lock_guard<std::mutex> cameraLock(g_stateMutex);
        g_camera.setViewport(width, height);
    }
    // Companion to FORGESHAPE_SURFACE_CONFIG: the other half of the orientation
    // chain, the viewport the camera actually projects with. Not per frame.
    FS_LOGI("JNI: surfaceChanged %dx%d", width, height);
    FS_LOGI("FORGESHAPE_CAMERA_VIEWPORT view=%dx%d aspect=%.4f", width, height,
            height > 0 ? static_cast<double>(width) / static_cast<double>(height) : 0.0);
}

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_surfaceDestroyed(JNIEnv*, jclass) {
    {
        // Never let a half-finished gesture survive a Surface swap. Selected
        // identity is untouched: it is process-scoped, not surface-scoped.
        std::lock_guard<std::mutex> stateLock(g_stateMutex);
        g_camera.resetGesture();
        g_selection.resetGesture();
        // A Surface swap means the finger that was drawing is gone. The stroke
        // is dropped; the sculpted vertices and the active mode are deliberately
        // preserved, because they are process-scoped, not surface-scoped.
        forgeshape::sculptSession().cancelStroke();
        g_grabbing = false;
        g_strokePending = false;
        // The same for a sketch gesture: the entity being dragged is dropped
        // and the sketch itself -- process-scoped like the mode -- is kept.
        forgeshape::sketchSession().resetGesture();
    }
    std::unique_lock<std::mutex> lock(g_viewport.mutex);
    if (!g_viewport.thread.joinable()) {
        return;
    }
    g_viewport.detachAcked = false;
    g_viewport.detachRequested = true;
    g_viewport.attachRequested = false;
    if (g_viewport.pendingWindow != nullptr) {
        ANativeWindow_release(g_viewport.pendingWindow);
        g_viewport.pendingWindow = nullptr;
    }
    g_viewport.toRender.notify_all();
    // Block until the renderer no longer references the ANativeWindow.
    g_viewport.toCaller.wait_for(lock, std::chrono::seconds(5), [] { return g_viewport.detachAcked; });
    FS_LOGI("JNI: surfaceDestroyed acked=%d", g_viewport.detachAcked ? 1 : 0);
}

// One compact call carries the complete pointer state of a single MotionEvent.
//
// The Java side computes nothing except the tool-type mapping, which is the one
// thing that HAS to happen there because it is the last place an Android
// constant is allowed to exist. Everything else -- ids, view-local coordinates,
// pressure, tilt -- crosses raw, and the range and fallback rules in
// forgeshape_input.h are applied here, once, so no consumer can be handed a NaN
// or an out-of-range angle.
//
// Every array is indexed by pointer index, so slot i of each describes the same
// pointer, and each is read only up to the bounded pointer count.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_touchEvent(JNIEnv* env, jclass, jint action,
                                                  jint actionPointerId, jint pointerCount,
                                                  jintArray ids, jfloatArray xs, jfloatArray ys,
                                                  jintArray toolTypes, jfloatArray pressures,
                                                  jfloatArray tilts, jfloatArray tiltOrientations,
                                                  jint viewWidth, jint viewHeight) {
    forgeshape::TouchAction translated;
    if (!translateAction(action, &translated)) {
        return;
    }

    int count = pointerCount;
    if (count < 0) {
        count = 0;
    }
    if (count > forgeshape::kMaxTrackedPointers) {
        count = forgeshape::kMaxTrackedPointers;
    }
    if (ids == nullptr || xs == nullptr || ys == nullptr) {
        count = 0;
    }

    // Defaults everywhere: an event that carries no stylus arrays at all still
    // produces full-pressure, untilted fingers, which is exactly the behaviour
    // that existed before the stylus fields did.
    forgeshape::TouchPointer pointers[forgeshape::kMaxTrackedPointers];
    if (count > 0) {
        jint idBuf[forgeshape::kMaxTrackedPointers];
        jfloat xBuf[forgeshape::kMaxTrackedPointers];
        jfloat yBuf[forgeshape::kMaxTrackedPointers];
        // JNI rule: a region read may raise ArrayIndexOutOfBounds, and no JNI
        // call may follow while that exception is pending, so each read is
        // checked before the next. A short required array drops the event, as
        // it always has; the stylus arrays below stay optional.
        env->GetIntArrayRegion(ids, 0, count, idBuf);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
        }
        env->GetFloatArrayRegion(xs, 0, count, xBuf);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
        }
        env->GetFloatArrayRegion(ys, 0, count, yBuf);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return;
        }

        // The stylus arrays are optional, and each is optional on its own: a
        // caller that has no tilt to report simply passes null and every pointer
        // keeps the untilted default. A short or unreadable array is treated the
        // same way rather than aborting the event -- losing a stylus angle must
        // never lose the gesture.
        jint toolBuf[forgeshape::kMaxTrackedPointers];
        jfloat pressureBuf[forgeshape::kMaxTrackedPointers];
        jfloat tiltBuf[forgeshape::kMaxTrackedPointers];
        jfloat orientationBuf[forgeshape::kMaxTrackedPointers];
        const bool haveTools = readOptionalIntRegion(env, toolTypes, count, toolBuf);
        const bool havePressures = readOptionalFloatRegion(env, pressures, count, pressureBuf);
        const bool haveTilts = readOptionalFloatRegion(env, tilts, count, tiltBuf);
        const bool haveOrientations =
            readOptionalFloatRegion(env, tiltOrientations, count, orientationBuf);

        for (int i = 0; i < count; ++i) {
            pointers[i].id = static_cast<int32_t>(idBuf[i]);
            pointers[i].x = xBuf[i];
            pointers[i].y = yBuf[i];
            if (haveTools) {
                pointers[i].toolType =
                    forgeshape::pointerToolTypeFromCode(static_cast<int32_t>(toolBuf[i]));
            }
            if (havePressures) {
                pointers[i].pressure = forgeshape::sanitizePointerPressure(pressureBuf[i]);
            }
            if (haveTilts || haveOrientations) {
                forgeshape::sanitizePointerTilt(
                    haveTilts ? tiltBuf[i] : forgeshape::kPointerTiltNoneRadians,
                    haveOrientations ? orientationBuf[i] : 0.0f,
                    &pointers[i].tiltRadians, &pointers[i].tiltOrientationRadians);
            }
        }
    }

    bool logOrbit = false;
    bool logPan = false;
    bool logZoom = false;
    bool logState = false;
    float yaw = 0.0f, pitch = 0.0f, distance = 0.0f;
    forgeshape::Vec3 target{};

    bool tapResolved = false;
    bool selectionChanged = false;
    forgeshape::SceneHit hit{};
    forgeshape::ObjectId selectedNow = forgeshape::kNoObject;

    // Gizmo reporting, gathered under the lock and logged outside it. `handled`
    // is the arbitration answer: true means this event belonged to a handle and
    // neither the camera nor the selection may see it.
    bool gizmoHandled = false;
    bool gizmoBegan = false;
    bool gizmoMoved = false;
    bool gizmoCommitted = false;
    bool gizmoRecorded = false;
    bool gizmoCancelled = false;
    const char* gizmoAxis = "";
    forgeshape::ObjectId gizmoObjectId = forgeshape::kNoObject;

    // Brush-stroke reporting, gathered under the lock and logged outside it.
    bool grabBegan = false;
    bool grabEnded = false;
    bool grabCancelled = false;
    bool grabPublished = false;
    bool grabPending = false;
    bool grabAbandoned = false;
    const char* grabTool = "";
    int grabVertices = 0;
    float grabWorldRadius = 0.0f;
    // How far the affected set actually reaches, measured both ways. The brush
    // selects in the WORLD metric, so maxWorld must stay inside the radius
    // while maxLocal may exceed it by whatever the body's Scale is — which is
    // the whole difference the world metric makes, readable in one log line
    // without a brush cursor and without a screenshot.
    float grabMaxWorld = 0.0f;
    float grabMaxLocal = 0.0f;
    forgeshape::SculptRevision grabRevision = 0;
    // The sculpt revision as the lock is released, for the two arbitration
    // tokens logged after it; nothing reads the session outside the lock.
    forgeshape::SculptRevision sculptRevisionAfter = 0;
    forgeshape::MeshRevision grabMeshRevision = 0;
    forgeshape::Vec3 grabDisplacement{};
    bool sketchLogPending = false;

    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (viewWidth > 0 && viewHeight > 0) {
            g_camera.setViewport(viewWidth, viewHeight);
        }

#ifndef NDEBUG
        // Diagnostic mirror only, and taken BEFORE any arbitration, so what a
        // test reads back is precisely what the camera, the selection and the
        // sculpt arbitration are about to be handed.
        g_lastPointerCount = count;
        for (int i = 0; i < count; ++i) {
            g_lastPointers[i] = pointers[i];
        }
#endif

        // -------------------------------------------------------------------
        // Sketch arbitration (`CAD-R0-A1A2`)
        // -------------------------------------------------------------------
        //
        // While a sketch is open the viewport is the sketch plane. ONE pointer
        // draws or selects and never orbits -- an orbit would take the aligned
        // view away under the finger placing a point -- and TWO pointers pan
        // and pinch exactly as they always have, because the camera's own
        // two-finger gesture cannot orbit. The session decides what a single
        // pointer means; while the sketch is being DRAWN anything it does not
        // consume with one pointer down is swallowed rather than handed to the
        // camera, and anything with two is navigation. In READY the drawing is
        // done and an unclaimed single pointer navigates -- see below.
        //
        // The gizmo is inactive for the whole sketch (setActive(false) at
        // begin) and the product is in Construction, so the two arbitrations
        // below are already out of the picture; only the camera and the
        // selection have to be told.
        // Spatial support selection (CAD-A3) owns the gesture the same way the
        // sketch does: a single-finger TAP selects the plane or face under it,
        // a single-finger DRAG orbits so the user can look around, and two
        // fingers navigate. It never mutates the project.
        bool chooserOwned = false;
        if (forgeshape::supportChooser().active()) {
            chooserOwned = true;
            forgeshape::SupportChooser& chooser = forgeshape::supportChooser();
            if (count > 1) {
                // Two fingers: navigate, and this is not a tap.
                g_chooserPointerId = -1;
                g_chooserTravelled = true;
                g_camera.onTouch(translated, static_cast<int32_t>(actionPointerId),
                                 count > 0 ? pointers : nullptr, count);
            } else if (translated == forgeshape::TouchAction::Down && count == 1) {
                g_chooserPointerId = pointers[0].id;
                g_chooserDownX = pointers[0].x;
                g_chooserDownY = pointers[0].y;
                g_chooserTravelled = false;
                g_camera.onTouch(translated, static_cast<int32_t>(actionPointerId), pointers, count);
            } else if (translated == forgeshape::TouchAction::Move && count == 1) {
                const float dx = pointers[0].x - g_chooserDownX;
                const float dy = pointers[0].y - g_chooserDownY;
                if (dx * dx + dy * dy > kChooserTapSlopPixels * kChooserTapSlopPixels) {
                    g_chooserTravelled = true;
                }
                g_camera.onTouch(translated, static_cast<int32_t>(actionPointerId), pointers, count);
            } else if (translated == forgeshape::TouchAction::Up) {
                if (!g_chooserTravelled && g_chooserPointerId >= 0) {
                    // Tap to aim, tap the SAME target again to commit: the first
                    // tap selects and highlights, a second tap on the target
                    // already selected begins the sketch on it. No extra chrome.
                    const forgeshape::ChosenSupport before = chooser.selected();
                    const forgeshape::ChosenSupport now =
                        chooser.select(g_camera.snapshot(), g_chooserDownX, g_chooserDownY,
                                       viewWidth, viewHeight, forgeshape::constructionScene());
                    const bool reselected =
                        now.kind != forgeshape::ChosenSupport::Kind::None
                        && before.kind == now.kind
                        && (now.kind == forgeshape::ChosenSupport::Kind::WorldPlane
                                ? before.plane == now.plane
                                : forgeshape::sameTopoRef(before.faceRef, now.faceRef));
                    if (reselected) {
                        confirmChosenSupportLocked();
                        sketchLogPending = true;
                    }
                }
                g_chooserPointerId = -1;
                g_camera.onTouch(translated, static_cast<int32_t>(actionPointerId), pointers, count);
            } else {
                g_camera.onTouch(translated, static_cast<int32_t>(actionPointerId),
                                 count > 0 ? pointers : nullptr, count);
            }
            g_selection.resetGesture();
        }

        bool sketchOwned = false;
        if (!chooserOwned) {
            forgeshape::SketchSession& sketch = forgeshape::sketchSession();
            if (sketch.active()) {
                sketchOwned = true;
                const bool consumed = sketch.onTouch(translated,
                                                     static_cast<int32_t>(actionPointerId),
                                                     pointers, count, g_camera.snapshot(),
                                                     viewWidth, viewHeight);
                // READY is the one state where an unclaimed single finger
                // NAVIGATES (`CAD-UX-S1-C1`). While the sketch is being drawn a
                // single pointer belongs to the drawing whether or not the
                // session took it, because an orbit would take the aligned view
                // away under the finger placing a point -- that rule is
                // unchanged. Once Finish Sketch has been taken there is no
                // aligned view left to protect and no point being placed: the
                // camera is an ordinary 3D one and the user is looking at a
                // staged solid, so a finger that misses the arrow does what a
                // finger does everywhere else in the product.
                const bool navigable =
                    sketch.state() == forgeshape::SketchSessionState::Ready;
                if (consumed || (count <= 1 && !navigable)) {
                    g_camera.resetGesture();
                    g_selection.resetGesture();
                } else {
                    g_camera.onTouch(translated, static_cast<int32_t>(actionPointerId),
                                     count > 0 ? pointers : nullptr, count);
                    g_selection.resetGesture();
                }
                if (sketch.gestureActive() || translated == forgeshape::TouchAction::Up
                    || translated == forgeshape::TouchAction::Cancel) {
                    sketchLogPending = true;
                }
            }
        }

        // -------------------------------------------------------------------
        // Construction gizmo arbitration
        // -------------------------------------------------------------------
        //
        // The rule, stated once: a pointer that goes DOWN on a handle belongs to
        // the gizmo for the whole of its life; a pointer that goes down anywhere
        // else navigates and picks exactly as it always has. Orbit, pan, pinch
        // and tap are untouched everywhere except on the handles themselves,
        // which is what keeps a new tool from taking the camera away.
        //
        // Whether the touch landed on a handle is decided ONCE, on Down, against
        // the same projected geometry the renderer drew — so a drag cannot turn
        // into an orbit half way through as the finger leaves the shaft.
        //
        // The gizmo follows ONE pointer, by stable id and never by index. A
        // second finger does not get to steer it and does not get to orbit
        // around it either: it CANCELS the drag, which puts the placement back
        // exactly where the first finger found it and records nothing. Trying to
        // do both at once is how a transform ends up half applied.
        forgeshape::GizmoSession& gizmo = forgeshape::gizmoSession();
        if (gizmo.capturing() && !gizmo.active()) {
            // Defensive: setActive already cancels, so this cannot normally
            // happen. If it ever does, a captured handle with no gizmo behind it
            // is an open transaction the user cannot close.
            gizmo.cancelDrag();
            gizmoCancelled = true;
        }
        if (gizmo.capturing()) {
            const int32_t captured = gizmo.capturedPointerId();
            int capturedIndex = -1;
            for (int i = 0; i < count; ++i) {
                if (pointers[i].id == captured) {
                    capturedIndex = i;
                    break;
                }
            }
            switch (translated) {
                case forgeshape::TouchAction::Move:
                    if (count == 1 && capturedIndex >= 0) {
                        if (gizmo.updateDrag(captured, g_camera.snapshot(),
                                             pointers[capturedIndex].x,
                                             pointers[capturedIndex].y, viewWidth, viewHeight)) {
                            gizmoMoved = true;
                        }
                        gizmoHandled = true;
                    } else {
                        // More than one pointer is down. Cancel and hand the
                        // gesture on; the camera re-anchors on any pointer-set
                        // change, so there is no jump.
                        gizmo.cancelDrag();
                        gizmoCancelled = true;
                    }
                    break;
                case forgeshape::TouchAction::Up:
                    gizmoRecorded = gizmo.commitDrag();
                    gizmoCommitted = true;
                    gizmoHandled = true;
                    break;
                case forgeshape::TouchAction::Cancel:
                    gizmo.cancelDrag();
                    gizmoCancelled = true;
                    gizmoHandled = true;
                    break;
                case forgeshape::TouchAction::PointerUp:
                    if (capturedIndex >= 0 && actionPointerId == captured) {
                        // The captured finger is the one leaving while others
                        // remain. That is still the end of ITS drag.
                        gizmoRecorded = gizmo.commitDrag();
                        gizmoCommitted = true;
                        gizmoHandled = true;
                    } else {
                        gizmo.cancelDrag();
                        gizmoCancelled = true;
                    }
                    break;
                default:
                    // PointerDown: a second finger arrived. Cancel, restore, and
                    // let the event go on to the camera.
                    gizmo.cancelDrag();
                    gizmoCancelled = true;
                    break;
            }
            if (!gizmoHandled) {
                // Whatever ended the drag is navigation from here on, and it
                // must not be able to resolve a tap either.
                g_selection.resetGesture();
            }
        } else if (gizmo.active() && translated == forgeshape::TouchAction::Down && count == 1 &&
                   viewWidth > 0 && viewHeight > 0) {
            if (gizmo.beginDrag(pointers[0].id, g_camera.snapshot(), pointers[0].x, pointers[0].y,
                                viewWidth, viewHeight)) {
                gizmoBegan = true;
                gizmoAxis = forgeshape::gizmoHandleName(gizmo.capturedHandle());
                gizmoObjectId = gizmo.capturedObjectId();
                gizmoHandled = true;
                // Swallowed: neither the camera nor the selection sees a Down
                // that belongs to a handle, so this gesture can neither orbit
                // nor re-select while it drags.
                g_camera.resetGesture();
                g_selection.resetGesture();
            }
            // A miss captures nothing, opens no transaction and is deliberately
            // NOT swallowed: the gesture falls through and navigates.
        }

        // -------------------------------------------------------------------
        // Sculpt-mode gesture rule
        // -------------------------------------------------------------------
        // One finger that goes DOWN on the Frozen Sculpt Mesh is a brush stroke
        // and owns the whole gesture; one finger that goes down anywhere else
        // navigates exactly as it always has. Two-finger pan and pinch are
        // untouched and can never sculpt.
        //
        // Whether the finger landed on the mesh is decided ONCE, on Down, and
        // never revisited, so a stroke cannot turn into an orbit half way
        // through a drag as the finger crosses the silhouette. What is deferred
        // is only WHETHER THIS GESTURE IS A STROKE AT ALL — see the
        // pending-then-promote rule where g_strokePending is declared.
        bool grabHandled = false;
        forgeshape::SculptSession& sculpt = forgeshape::sculptSession();
        if (sculpt.inSculptMode()) {
            if (!g_grabbing && !g_strokePending && translated == forgeshape::TouchAction::Down &&
                count == 1 && viewWidth > 0 && viewHeight > 0) {
                const forgeshape::ConstructionTransform& placement =
                    forgeshape::constructionTransform();
                // Probe only. This asks "would a stroke start here?" without
                // starting one and without touching a single vertex, which is
                // exactly what makes a gesture that turns out to be navigation
                // structurally incapable of having mutated the mesh.
                if (sculpt.hitsSculptMesh(g_camera.snapshot(), pointers[0].x, pointers[0].y,
                                          viewWidth, viewHeight,
                                          placement.inverseModelMatrix())) {
                    g_strokePending = true;
                    g_pendingDownX = pointers[0].x;
                    g_pendingDownY = pointers[0].y;
                    grabPending = true;
                    // Swallowed: neither the camera nor the selection sees a
                    // Down that may yet become a stroke. If it is abandoned they
                    // pick the gesture up from the event that abandoned it, and
                    // the camera re-anchors on any pointer-set change anyway.
                    grabHandled = true;
                    g_camera.resetGesture();
                    g_selection.resetGesture();
                }
                // A miss starts no stroke and is deliberately NOT swallowed: the
                // gesture falls through and orbits, which is the documented rule.
            } else if (g_strokePending) {
                bool travelled = false;
                if (count == 1) {
                    const float dx = pointers[0].x - g_pendingDownX;
                    const float dy = pointers[0].y - g_pendingDownY;
                    travelled = (dx * dx + dy * dy) >= (kStrokeArmPixels * kStrokeArmPixels);
                }
                if (translated == forgeshape::TouchAction::Move && count == 1) {
                    grabHandled = true;  // still undecided, still swallowed
                    if (travelled) {
                        const forgeshape::ConstructionTransform& placement =
                            forgeshape::constructionTransform();
                        // Promote. The stroke anchors at the ORIGINAL down
                        // point, so the hit, the affected set and the falloff
                        // weights are exactly what they would have been had the
                        // stroke begun on Down — the deferral costs nothing.
                        if (sculpt.beginStroke(g_camera.snapshot(), g_pendingDownX, g_pendingDownY,
                                               viewWidth, viewHeight, placement.modelMatrix(),
                                               placement.inverseModelMatrix())) {
                            g_strokePending = false;
                            g_grabbing = true;
                            grabBegan = true;
                            grabTool = forgeshape::sculptToolName(sculpt.stroke().tool());
                            grabVertices = sculpt.stroke().affectedVertexCount();
                            grabWorldRadius = sculpt.stroke().worldRadius();
                            {
                                const forgeshape::SculptStroke& s = sculpt.stroke();
                                const forgeshape::Mat4 model = placement.modelMatrix();
                                for (int i = 0; i < s.affectedVertexCount(); ++i) {
                                    const forgeshape::Vec3 d =
                                        forgeshape::vec3Sub(s.affectedVertex(i).basePosition,
                                                            s.localCenter());
                                    const float world = forgeshape::brushWorldDistance(model, d);
                                    const float local =
                                        std::sqrt(forgeshape::vec3Dot(d, d));
                                    if (world > grabMaxWorld) {
                                        grabMaxWorld = world;
                                    }
                                    if (local > grabMaxLocal) {
                                        grabMaxLocal = local;
                                    }
                                }
                            }
                            // This same event is the stroke's first move, so no
                            // pointer travel is lost to the deferral.
                            if (sculpt.updateStroke(pointers[0].x, pointers[0].y)) {
                                grabRevision = sculpt.mesh().revision();
                                grabDisplacement = sculpt.stroke().lastLocalDisplacement();
                                grabMeshRevision = publishSculptRepresentation(
                                    sculpt, forgeshape::sculptToolName(sculpt.stroke().tool()));
                                grabPublished =
                                    grabMeshRevision != forgeshape::kNoMeshRevision;
                            }
                        } else {
                            // The probe hit but the stroke could not start (no
                            // vertex inside the brush). Abandon and navigate.
                            g_strokePending = false;
                            grabHandled = false;
                            grabAbandoned = true;
                        }
                    }
                } else {
                    // A second finger, an Up, a Cancel, or a multi-pointer
                    // event. No stroke was ever created, so there is nothing to
                    // end, nothing to publish and nothing to undo: the mesh and
                    // the SculptRevision are bit-identical to what they were
                    // when the finger landed.
                    g_strokePending = false;
                    grabAbandoned = true;
                    // The selection never saw the Down, so it holds no tap
                    // candidate and this gesture cannot select or clear.
                    g_selection.resetGesture();
                }
            } else if (g_grabbing) {
                switch (translated) {
                    case forgeshape::TouchAction::Move:
                        if (count == 1) {
                            grabTool = forgeshape::sculptToolName(sculpt.stroke().tool());
                            if (sculpt.updateStroke(pointers[0].x, pointers[0].y)) {
                                grabRevision = sculpt.mesh().revision();
                                grabDisplacement = sculpt.stroke().lastLocalDisplacement();
                                grabMeshRevision = publishSculptRepresentation(sculpt, grabTool);
                                grabPublished = grabMeshRevision != forgeshape::kNoMeshRevision;
                            }
                            grabHandled = true;
                        }
                        break;
                    case forgeshape::TouchAction::Up:
                        sculpt.endStroke();
                        g_grabbing = false;
                        grabEnded = true;
                        grabHandled = true;
                        grabRevision = sculpt.mesh().revision();
                        break;
                    case forgeshape::TouchAction::Cancel:
                        sculpt.cancelStroke();
                        g_grabbing = false;
                        grabCancelled = true;
                        grabHandled = true;
                        grabRevision = sculpt.mesh().revision();
                        break;
                    default:
                        // A second finger arrived, or the pointer set changed in
                        // some other way. The stroke ends cleanly and the event
                        // goes on to the camera, which re-anchors on any pointer
                        // set change and so produces no jump.
                        sculpt.endStroke();
                        g_grabbing = false;
                        grabEnded = true;
                        grabRevision = sculpt.mesh().revision();
                        break;
                }
                if (!grabHandled) {
                    // Whatever ended the stroke is navigation from here on, and
                    // it must not be able to resolve a tap either.
                    g_selection.resetGesture();
                }
            }
        } else if (g_grabbing || g_strokePending) {
            // Leaving Sculpt mode with a finger still down.
            sculpt.cancelStroke();
            g_grabbing = false;
            g_strokePending = false;
        }

        // A gesture the gizmo owns never reaches the camera or the selection.
        // The two conditions are separate because they are separate rules: one
        // is Sculpt's brush arbitration, the other is Construction's handle
        // arbitration, and they are mutually exclusive by product mode.
        if (!grabHandled && !gizmoHandled && !sketchOwned && !chooserOwned) {
            g_camera.onTouch(translated, static_cast<int32_t>(actionPointerId),
                             count > 0 ? pointers : nullptr, count);

            // The selection owner sees exactly the same event and decides on its
            // own whether this gesture is still a tap. Navigation never consults
            // it and it never moves the camera.
            float tapX = 0.0f;
            float tapY = 0.0f;
            tapResolved = g_selection.onTouch(translated, static_cast<int32_t>(actionPointerId),
                                              count > 0 ? pointers : nullptr, count, &tapX, &tapY);
            // A tap while the imported preview is on the screen selects
            // nothing. The preview is not in the scene and is not pickable, and
            // picking the project underneath it would change the selection over
            // a body the user cannot currently see. Navigation is unaffected:
            // orbit, pan and zoom all still work, which is the point of looking
            // at the preview at all.
            if (forgeshape::importedMeshPreview().visible()) {
                tapResolved = false;
            }
            if (tapResolved && viewWidth > 0 && viewHeight > 0) {
                // Picking uses the camera snapshot as it stands at release, so a
                // tap after any amount of navigation resolves against what is on
                // screen right now — and against the ACTIVE representation,
                // because it reads the store's current revision.
                hit = forgeshape::pickScene(g_camera.snapshot(), tapX, tapY, viewWidth, viewHeight);
                selectionChanged = g_selection.applyPick(hit);
                // Picking a body also makes it the one the Construction editors
                // act on, so the Property Inspector and the Objects list can
                // never disagree with what the viewport says is selected.
                // A miss clears the selection but deliberately leaves the edit
                // target alone: there would be nothing to put in its place, and
                // an editor bound to no body would have nothing to show.
                if (hit.hit) {
                    forgeshape::constructionScene().setActiveBody(hit.objectId);
                }
            } else {
                tapResolved = false;
            }

            // Camera state is logged only when a gesture ends, never per move,
            // and never for a gesture the brush owned.
            logState = (translated == forgeshape::TouchAction::Up ||
                        translated == forgeshape::TouchAction::Cancel);
        }
        selectedNow = g_selection.selected();
        if (grabPending || grabAbandoned) {
            sculptRevisionAfter = forgeshape::sculptSession().mesh().revision();
        }

        if (!g_loggedOrbit && g_camera.orbitCount() > 0) { g_loggedOrbit = true; logOrbit = true; }
        if (!g_loggedPan && g_camera.panCount() > 0) { g_loggedPan = true; logPan = true; }
        if (!g_loggedZoom && g_camera.zoomCount() > 0) { g_loggedZoom = true; logZoom = true; }

        yaw = g_camera.yaw();
        pitch = g_camera.pitch();
        distance = g_camera.distance();
        target = g_camera.target();
    }

    // Stroke tokens. Begin and end are one line each; a Move logs only when it
    // actually changed geometry, so a stationary finger costs nothing.
    //
    // PENDING and ABANDONED are the arbitration made visible: a gesture that
    // logs PENDING then ABANDONED never became a stroke at all, which is exactly
    // what makes "multi-touch navigation cannot mutate the sculpt mesh" a thing
    // that can be checked from a log rather than asserted.
    // Gizmo tokens. Begin, commit and cancel are one line each; a Move logs
    // nothing at all, because a drag produces hundreds of them and the count is
    // read back from gizmoState instead.
    //
    // COMMIT carries how many updates the drag applied AND whether a step was
    // recorded, which is exactly what makes "a drag of any length is one history
    // step, and a tap is none" a thing a captured run states rather than a claim.
    if (gizmoBegan) {
        FS_LOGI("FORGESHAPE_GIZMO_DRAG_BEGIN:%s axis=%s objectId=%llu",
                forgeshape::gizmoModeName(forgeshape::gizmoSession().mode()), gizmoAxis,
                (unsigned long long)gizmoObjectId);
    }
    if (gizmoCommitted) {
        FS_LOGI("FORGESHAPE_GIZMO_DRAG_COMMIT:%s updates=%llu solve=%s undo=%d",
                gizmoRecorded ? "recorded" : "no_change",
                (unsigned long long)forgeshape::gizmoSession().dragUpdateCount(),
                forgeshape::axisSolveStatusName(forgeshape::gizmoSession().lastSolveStatus()),
                (int)forgeshape::constructionHistory().undoDepth());
    }
    if (gizmoCancelled) {
        FS_LOGI("FORGESHAPE_GIZMO_DRAG_CANCEL updates=%llu undo=%d",
                (unsigned long long)forgeshape::gizmoSession().dragUpdateCount(),
                (int)forgeshape::constructionHistory().undoDepth());
    }
    (void)gizmoMoved;

    if (grabPending) {
        FS_LOGI("FORGESHAPE_SCULPT_STROKE_PENDING sculptRev=%llu",
                (unsigned long long)sculptRevisionAfter);
    }
    if (grabAbandoned) {
        FS_LOGI("FORGESHAPE_SCULPT_STROKE_ABANDONED:navigation sculptRev=%llu",
                (unsigned long long)sculptRevisionAfter);
    }
    if (grabBegan) {
        FS_LOGI("FORGESHAPE_SCULPT_STROKE_BEGIN:%s:%d radiusWorld=%.4f maxWorld=%.4f "
                "maxLocal=%.4f",
                grabTool, grabVertices, grabWorldRadius, grabMaxWorld, grabMaxLocal);
    }
    if (grabPublished) {
        FS_LOGI("FORGESHAPE_SCULPT_STROKE_MOVE:%s sculptRev=%llu meshRev=%llu "
                "local=(%.4f,%.4f,%.4f)",
                grabTool, (unsigned long long)grabRevision, (unsigned long long)grabMeshRevision,
                grabDisplacement.x, grabDisplacement.y, grabDisplacement.z);
    }
    if (grabEnded) {
        FS_LOGI("FORGESHAPE_SCULPT_STROKE_END sculptRev=%llu", (unsigned long long)grabRevision);
    }
    if (grabCancelled) {
        FS_LOGI("FORGESHAPE_SCULPT_STROKE_CANCEL sculptRev=%llu", (unsigned long long)grabRevision);
    }

    if (sketchLogPending && (translated == forgeshape::TouchAction::Up
                             || translated == forgeshape::TouchAction::Cancel)) {
        // One line per gesture end, never per move: what the sketch now holds
        // and how the last point was snapped, which is what a device case
        // reads to prove an endpoint snap against a grid snap.
        const forgeshape::SketchSession& sketch = forgeshape::sketchSession();
        FS_LOGI("FORGESHAPE_SKETCH_GESTURE_END tool=%s entities=%d placed=%u snap=%d status=%s "
                "selected=%u polyline=%d",
                forgeshape::sketchToolName(sketch.tool()), (int)sketch.sketch().entities.size(),
                sketch.entitiesPlaced(), (int)sketch.lastSnapKind(),
                forgeshape::cadStatusName(sketch.lastStatus()), sketch.selectedEntityId(),
                sketch.polylineInProgress() ? 1 : 0);
    }

    if (tapResolved) {
        if (hit.hit) {
            FS_LOGI("FORGESHAPE_PICK_HIT:%llu:%d dist=%.4f at=(%.4f,%.4f,%.4f)",
                    (unsigned long long)hit.objectId, hit.triangleIndex, hit.distance,
                    hit.position.x, hit.position.y, hit.position.z);
        } else {
            FS_LOGI("FORGESHAPE_PICK_MISS");
        }
    }
    if (selectionChanged) {
        FS_LOGI("FORGESHAPE_SELECTION_CHANGED:%llu", (unsigned long long)selectedNow);
    }

    if (logOrbit) FS_LOGI("FORGESHAPE_CAMERA_ORBIT_OK");
    if (logPan) FS_LOGI("FORGESHAPE_CAMERA_PAN_OK");
    if (logZoom) FS_LOGI("FORGESHAPE_CAMERA_ZOOM_OK");
    if (logState) {
        FS_LOGI("FORGESHAPE_CAMERA_STATE yaw=%.4f pitch=%.4f distance=%.4f target=(%.4f,%.4f,%.4f)",
                yaw, pitch, distance, target.x, target.y, target.z);
        FS_LOGI("FORGESHAPE_SELECTION_STATE:%llu", (unsigned long long)selectedNow);
    }
}

JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_stop(JNIEnv*, jclass) {
    std::thread worker;
    {
        std::lock_guard<std::mutex> lock(g_viewport.mutex);
        if (!g_viewport.thread.joinable()) {
            return;
        }
        g_viewport.quitRequested = true;
        g_viewport.toRender.notify_all();
        worker = std::move(g_viewport.thread);
    }
    worker.join();
    FS_LOGI("ForgeShape native viewport stopped");
}

}  // extern "C"
